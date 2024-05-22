#include "precompiled.h"
//#include "using_namespace.h"
#include "stuff.h"
#include "shade.h"
#include "gpgpu.h"
#include "gpuBlur2_5.h"
#include "stefanfw.h"
#include "Array2D_imageProc.h"
#include "cfg1.h"
#include "CrossThreadCallQueue.h"
//#include "MyVideoWriter.h"

#include "util.h"

typedef Array2D<float> Image;
int wsx = 400, wsy = 400;
int scale = 4;
int sx = wsx / ::scale;
int sy = wsy / ::scale;
ivec2 sz(sx, sy);

float surfTensionThres;

struct Particle {
	vec2 pos;
	vec2 velocity;
};

struct Material {
	//Array2D<float> img = Array2D<float>(sx, sy);
	vector<Particle> particles;

	void reset() {
		particles.clear();
	}
};
Material red, green;
vector<Material*> materials{ &red, &green };

bool pause = false;

struct SApp : ci::app::App {
	//shared_ptr<MyVideoWriter> videoWriter = make_shared<MyVideoWriter>();

	void cleanup() {
		//videoWriter.reset();
	}

	void setup()
	{
		enableDenormalFlushToZero();

		disableGLReadClamp();
		stefanfw::eventHandler.subscribeToEvents(*this);
		setWindowSize(wsx, wsy);

		reset();

		// focus
		getWindow()->setAlwaysOnTop(true);
		getWindow()->setAlwaysOnTop(false);
	}
	void update()
	{
		stefanfw::beginFrame();
		stefanUpdate();
		stefanDraw();
		stefanfw::endFrame();
	}
	void keyDown(ci::app::KeyEvent e)
	{
		if (keys[' ']) {
			doFluidStep();
		}
		if (keys['r'])
		{
			reset();
		}
		if (keys['p'] || keys['2'])
		{
			pause = !pause;
		}
	}
	void reset() {
		red.reset();
		green.reset();
	}
	vec2 direction;
	vec2 lastm;
	void mouseDrag(ci::app::MouseEvent e)
	{
		mm();
	}
	void mouseMove(ci::app::MouseEvent e)
	{
		mm();
	}
	void mm()
	{
		direction = vec2(getMousePos()) - lastm;
		lastm = getMousePos();
	}
	void stefanDraw()
	{
		gl::clear(Color(0, 0, 0));

		gl::setMatricesWindow(getWindowSize(), false);
		auto img = Array2D<float>(sz);
		for (auto* mat : materials) {
			for (auto& particle : mat->particles) {
				aaPoint<float, WrapModes::GetClamped>(img, particle.pos, 10);
			}
		}
		int ksize = 10 * 2 + 1;
		auto kernel = getGaussianKernel(ksize, sigmaFromKsize(ksize)/2);
		img = separableConvolve<float, WrapModes::GetClamped>(
			img, kernel);

		//img = gaussianBlur<float, WrapModes::GetClamped>(img, 10 * 2 + 1);
		forxy(img) {
			img(p) += sqrt(img(p));
			//img(p) = smoothstep(0.0f, 0.1f, img(p));
		}
		auto tex = gtex(img);
		auto redTex = gtex(img);
		//auto redTex = gtex(::red.img);
		//auto greenTex = gtex(::green.img);

		auto bloomSize = cfg1::getOpt("bloomSize", 1.0f,
			[&]() { return keys['b']; },
			[&]() { return mix(0.1, 8.0, mouseY); });
		int bloomIters = cfg1::getOpt("bloomIters", 4.0f,
			[&]() { return keys['B']; },
			[&]() { return mix(1.0, 8.0, mouseY); });
		float bloomIntensity = cfg1::getOpt("bloomIntensity", 0.2f,
			[&]() { return keys['i']; },
			[&]() { return mix(0.1, 8.0, mouseY); });
		auto redTexB = gpuBlur2_5::run_longtail(redTex, bloomIters, bloomSize);
		//auto greenTexB = gpuBlur2_5::run_longtail(greenTex, bloomIters, bloomSize);

		redTex = shade2( redTex, redTexB, MULTILINE(
			_out.r = (fetch1() + fetch1(tex2)) * bloomIntensity;
		),
			ShadeOpts().uniform("bloomIntensity", bloomIntensity)
			);
		/*greenTex = shade2(greenTex, greenTexB, MULTILINE(
			_out.r = (fetch1() + fetch1(tex2)) * bloomIntensity;
		),
			ShadeOpts().uniform("bloomIntensity", bloomIntensity)
			);*/
		static auto envMap = gl::Texture::create(ci::loadImage(loadAsset("envmap2.png")));
		//static auto envMap = gl::TextureCubeMap::create(loadImage(loadAsset("envmap_cube.jpg")), gl::TextureCubeMap::Format().mipmap());
		//gl::TextureBaseRef test=envMap;
		auto grads = get_gradients_tex(tex);

		auto tex2 = shade2(tex, grads, envMap, redTex, /*greenTex,*/
			"vec2 grad = fetch2(tex2);"
			"vec3 N = normalize(vec3(-grad.x, -grad.y, -1.0));"
			"vec3 I=-normalize(vec3(tc.x-.5, tc.y-.5, 1.0));"
			"float eta=1.0/1.3;"
			"vec3 R=refract(I, N, eta);"
			"vec3 c = getEnv(R);"
			"vec3 albedo = 0.0*vec3(0.005, 0.0, 0.0);"
			"c = mix(albedo, c, pow(.9, fetch1(tex) * 50.0));" // tmp
			"R = reflect(I, N);"
			"if(fetch1(tex) > surfTensionThres)"
			"	c += getEnv(R) * 5.0;" // mul to tmp simulate one side of the envmap being brighter than the other

			"float redVal = fetch1(tex4);"
			//"float greenVal = fetch1(tex5);"
			// this is taken from https://www.shadertoy.com/view/Mld3Rn
			"vec3 redColor = vec3(min(redVal * 1.5, 1.), pow(redVal, 2.5), pow(redVal, 12.)); "
			//"vec3 greenColor = vec3(min(greenVal * 1.5, 1.), pow(greenVal, 2.5), pow(greenVal, 12.)).zyx; "
			"c += redColor;"
			//"c += greenColor;"

			"_out.rgb = c;"
			, ShadeOpts().ifmt(GL_RGB16F).scale(4.0f).uniform("surfTensionThres", surfTensionThres),
			"float PI = 3.14159265358979323846264;\n"
			"vec2 latlong(vec3 v) {\n"
			"v = v.xzy;\n"
			"v = normalize(v);\n"
			"float theta = acos(-v.z);\n" // +z is up
			"\n"
			"v.y=-v.y;\n"
			"float phi = atan(v.y, v.x) + PI;\n"
			"return vec2(phi / (2.0*PI), theta / (PI/2.0));\n"
			"}\n"
			"vec3 w = vec3(.22, .71, .07);"
			"vec3 getEnv(vec3 v) {\n"
			"	vec3 c = fetch3(tex3, latlong(v));\n"
			"	c = pow(c, vec3(2.2));" // gamma correction
			"	return c;"
			"}\n"
		);

		tex2 = shade2(tex2,
			"vec3 c = fetch3(tex);"
			"if(c.r<0.0||c.g<0.0||c.b<0.0) { _out.rgb = vec3(1.0, 0.0, 0.0); }" // eases debugging
			"c = pow(c, vec3(1.0/2.2));"
			"_out.rgb = c;"
		);

		//videoWriter->write(tex2);
		
		gl::draw(tex2, getWindowBounds());
	}
	void stefanUpdate()
	{
		if (!pause)
		{
			doFluidStep();

		} // if ! pause
		auto material = keys['g'] ? &green : &red;

		if (mouseDown_[0])
		{
			vec2 scaledm = vec2(mouseX * (float)sx, mouseY * (float)sy);
			Particle part; part.pos = scaledm;
			material->particles.push_back(part);
		}
		else if (mouseDown_[2]) {
			mm();
			vec2 scaledm = vec2(mouseX * (float)sx, mouseY * (float)sy);
			Area a(scaledm, scaledm);
			int r = 15;
			a.expand(r, r);
			for (int x = a.x1; x <= a.x2; x++)
			{
				for (int y = a.y1; y <= a.y2; y++)
				{
					vec2 v = vec2(x, y) - scaledm;
					float w = std::max(0.0f, 1.0f - length(v) / r);
					w = 3 * w * w - 2 * w * w * w;
					for (Particle& part : material->particles) {
						if (distance(part.pos, v) < 10)
							part.velocity += 4.0f * direction / (float)::scale;
					}
				}
			}
		}
	}

	void doFluidStep() {
		surfTensionThres = cfg1::getOpt("surfTensionThres", .04f,
			[&]() { return keys['6']; },
			[&]() { return expRange(mouseY, 0.1f, 50000.0f); });

		auto surfTension = cfg1::getOpt("surfTension", 1.0f,
			[&]() { return keys['7']; },
			[&]() { return expRange(mouseY, .0001f, 40000.0f); });
		auto gravity = cfg1::getOpt("gravity", .1f,//0.0f,//.1f,
			[&]() { return keys['8']; },
			[&]() { return expRange(mouseY, .0001f, 40000.0f); });
		auto incompressibilityCoef = cfg1::getOpt("incompressibilityCoef", 10.0f,
			[&]() { return keys['/']; },
			[&]() { return expRange(mouseY, .0001f, 40000.0f); });


		//for (int i = 0; i < 4; i++) {
			//repel(::red, ::green);
			//repel(::green, ::red);
		//}

		auto imgForAdvect = Array2D<float>(sz);
		for (auto* mat : materials) {
			for (auto& particle : mat->particles) {
				aaPoint<float, WrapModes::GetClamped>(imgForAdvect, particle.pos, 1);
			}
		}
		for (int i = 0; i < 10; i++) {
			imgForAdvect = ::gaussianBlur<float, WrapModes::GetClamped>(imgForAdvect, 3 * 2 + 1);
		}
		//auto grads = get_gradients(imgForAdvect);

		for (auto material : ::materials) {
			auto& particles = material->particles;
			auto img_b = imgForAdvect;
			//for(int i < 0
			//img_b = gaussianBlur<float, WrapModes::GetClamped>(imgForAdvect, 3 * 2 + 1);
			auto& guidance = img_b;
			for (auto& particle : material->particles) {
				particle.velocity += vec2(0.0f, gravity);

				auto g = gradient_i<float, WrapModes::Get_WrapZeros>(guidance, particle.pos);
				if (img_b(particle.pos) < surfTensionThres)
				{
					g = g * surfTension * imgForAdvect(particle.pos);
				}
				else
				{
					g *= -incompressibilityCoef;
				}

				particle.velocity += g;
			}

			advect(*material);
		}


	}
	void advect(Material& material) {
		for (auto& particle : material.particles) {
			vec2 newPos = particle.pos + particle.velocity;
			for (int dim = 0; dim <= 1; dim++) {
				const float damping = 0.9f;
				float maxVal = sz[dim] - 1;
				if (newPos[dim] > maxVal) {
					particle.velocity[dim] *= -1.0f * damping;
					newPos[dim] = maxVal - (newPos[dim] - maxVal);
				}
				if (newPos[dim] < 0) {
					particle.velocity[dim] *= -1.0f * damping;
					newPos[dim] = -newPos[dim];
				}
			}
			particle.pos = newPos;
		}
	}
};

CrossThreadCallQueue * gMainThreadCallQueue;
CINDER_APP(SApp, ci::app::RendererGl(),
	[&](ci::app::App::Settings *settings)
{
	//bool developer = (bool)ifstream(getAssetPath("developer"));
	settings->setConsoleWindowEnabled(true);
})