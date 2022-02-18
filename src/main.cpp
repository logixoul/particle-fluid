#include "precompiled.h"
#include "lxlib/using_namespace.h"
#include "lxlib/stuff.h"
#include "lxlib/shade.h"
#include "lxlib/gpgpu.h"
#include "lxlib/gpuBlur2_5.h"
#include "lxlib/stefanfw.h"
#include "lxlib/Array2D_imageProc.h"
#include "lxlib/cfg1.h"
#include "lxlib/CrossThreadCallQueue.h"
//#include "MyVideoWriter.h"

import util;

typedef Array2D<float> Image;
int wsx = 1280, wsy = 720;
int scale = 4;
int sx = wsx / ::scale;
int sy = wsy / ::scale;
ivec2 sz(sx, sy);
struct Material {
	Array2D<float> img = Array2D<float>(sx, sy);
	Array2D<vec2> tmpEnergy = Array2D<vec2>(sx, sy);
};
Material red, green;
vector<Material*> materials{ &red, &green };
float surfTensionThres;

bool pause = false;


void updateConfig() {
}

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
			std::fill(red.img.begin(), red.img.end(), 0.0f);
			std::fill(red.tmpEnergy.begin(), red.tmpEnergy.end(), vec2());

			std::fill(green.img.begin(), green.img.end(), 0.0f);
			std::fill(green.tmpEnergy.begin(), green.tmpEnergy.end(), vec2());
		}
		if (keys['p'] || keys['2'])
		{
			pause = !pause;
		}
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
		auto img = empty_like(red.img);
		forxy(img) {
			img(p) = red.img(p) + green.img(p);
		}
		auto tex = gtex(img);
		auto redTex = gtex(::red.img);
		auto greenTex = gtex(::green.img);

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
		auto greenTexB = gpuBlur2_5::run_longtail(greenTex, bloomIters, bloomSize);

		redTex = shade2( redTex, redTexB, MULTILINE(
			_out.r = (fetch1() + fetch1(tex2)) * bloomIntensity;
		),
			ShadeOpts().uniform("bloomIntensity", bloomIntensity)
			);
		greenTex = shade2(greenTex, greenTexB, MULTILINE(
			_out.r = (fetch1() + fetch1(tex2)) * bloomIntensity;
		),
			ShadeOpts().uniform("bloomIntensity", bloomIntensity)
			);
		static auto envMap = gl::Texture::create(ci::loadImage(loadAsset("envmap2.png")));
		
		auto grads = get_gradients_tex(tex);

		auto tex2 = shade2(tex, grads, envMap, redTex, greenTex,
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
			"float greenVal = fetch1(tex5);"
			//"redVal /= redVal + 1;"
			//"greenVal /= greenVal + 1;"
			// this is taken from https://www.shadertoy.com/view/Mld3Rn
			"vec3 redColor = vec3(min(redVal * 1.5, 1.), pow(redVal, 2.5), pow(redVal, 12.)); "
			"vec3 greenColor = vec3(min(greenVal * 1.5, 1.), pow(greenVal, 2.5), pow(greenVal, 12.)).zyx; "
			"c += redColor;"
			"c += greenColor;"

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

		if (0)tex2 = shade2(tex,
			"float c = fetch1();"
			"_out.r = c;");
		
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
			vec2 scaledm = vec2(getMousePos()-getWindow()->getPos()) / float(::scale); //vec2(mouseX * (float)sx, mouseY * (float)sy);
			Area a(scaledm, scaledm);
			int r = 80 / pow(2, ::scale);
			a.expand(r, r);
			for (int x = a.x1; x <= a.x2; x++)
			{
				for (int y = a.y1; y <= a.y2; y++)
				{
					vec2 v = vec2(x, y) - scaledm;
					float w = max(0.0f, 1.0f - length(v) / r);
					w = 3 * w * w - 2 * w * w * w;
					material->img.wr(x, y) += 1.f * w *10.0;
				}
			}
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
					float w = max(0.0f, 1.0f - length(v) / r);
					w = 3 * w * w - 2 * w * w * w;
					if (material->img.wr(x, y) != 0.0f)
						material->tmpEnergy.wr(x, y) += w * material->img.wr(x, y) * 4.0f * direction / (float)::scale;
				}
			}
		}

		//if (pause)
		//	Sleep(50);
	}

	template<class T>
	static Array2D<T> gauss3_weightAdjusting(Array2D<T> src) {
		T zero = ::zero<T>();
		Array2D<T> dst1(src.w, src.h);
		Array2D<T> dst2(src.w, src.h);
		forxy(dst1) {
			dst1(p) = .25f * (2.0f * get_clamped(src, p.x, p.y) + get_clamped(src, p.x - 1, p.y) + get_clamped(src, p.x + 1, p.y));
		}
		forxy(dst2) {
			vector<float> weights = { .25, .5, .25 };
			if (p.y == 0) weights[0] = 0.0f;
			if (p.y == src.h - 1) weights[2] = 0.0f;
			float sumW = ::accumulate(weights.begin(), weights.end(), 0.0f);
			for (auto& weight : weights) weight /= sumW;
			dst2(p) = weights[1] * get_clamped(dst1, p.x, p.y) + weights[0] * get_clamped(dst1, p.x, p.y - 1) + weights[2] * get_clamped(dst1, p.x, p.y + 1);
		}
		return dst2;
	}

	void repel(Material& a, Material& b) {
		auto offsets = empty_like(a.tmpEnergy);

		auto guidance = gaussianBlur<float, WrapModes::GetClamped>(b.img, 4 * 2 + 1);
		forxy(a.img) {
			auto g = gradient_i<float, WrapModes::GetClamped>(guidance, p);
			g = -safeNormalized(g) * .2f;

			a.tmpEnergy(p) += g * a.img(p);
			//offsets(p) = g * a.img(p);
		}
		//advect(a, offsets);
	}
	void advect(Material& material, Array2D<vec2> offsets) {
		auto& img = material.img;
		auto& tmpEnergy = material.tmpEnergy;

		auto img3 = Array2D<float>(sx, sy);
		auto tmpEnergy3 = Array2D<vec2>(sx, sy, vec2());
		forxy(img)
		{
			if (img(p) == 0.0f)
				continue;

			vec2 vec = offsets(p);
			vec2 dst = vec2(p) + vec;

			vec2 newEnergy = tmpEnergy(p);
			for (int dim = 0; dim <= 1; dim++) {
				if (dst[dim] >= sz[dim]) {
					newEnergy[dim] *= -1.0f;
					dst[dim] = sz[dim] - (dst[dim] - sz[dim]);
				}
			}
			aaPoint<float, WrapModes::GetClamped>(img3, dst, img(p));
			aaPoint<vec2, WrapModes::GetClamped>(tmpEnergy3, dst, newEnergy);
		}
		img = img3;
		tmpEnergy = tmpEnergy3;
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
		auto incompressibilityCoef = cfg1::getOpt("incompressibilityCoef", 1.0f,
			[&]() { return keys['\\']; },
			[&]() { return expRange(mouseY, .0001f, 40000.0f); });


		//for (int i = 0; i < 4; i++) {
			repel(::red, ::green);
			repel(::green, ::red);
		//}

		for (auto material : ::materials) {
			auto& tmpEnergy = material->tmpEnergy;
			auto& img = material->img;

			forxy(tmpEnergy)
			{
				tmpEnergy(p) += vec2(0.0f, gravity) * img(p);
			}

			img = gauss3(img);
			tmpEnergy = gauss3(tmpEnergy);

			auto img_b = img.clone();
			img_b = gaussianBlur<float, WrapModes::GetClamped>(img_b, 3 * 2 + 1);
			forxy(tmpEnergy)
			{
				//auto& guidance = img;
				auto g = gradient_i<float, WrapModes::GetClamped>(img_b, p);
				if (img_b(p) < surfTensionThres)
				{
					g = safeNormalized(g) * surfTension;
				}
				else
				{
					g *= -incompressibilityCoef;
				}

				tmpEnergy(p) += g * img(p);
			}
			
			auto offsets = empty_like(tmpEnergy);
			forxy(offsets) {
				offsets(p) = tmpEnergy(p) / img(p);
			}
			advect(*material, offsets);
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