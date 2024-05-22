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
struct Material {
	Array2D<float> img = Array2D<float>(sx, sy);
	Array2D<vec2> tmpEnergy = Array2D<vec2>(sx, sy);
};
Material red, green;
vector<Material*> materials{ &red, &green };
float surfTensionThres;

bool pause = false;

Array2D<float> bounces_dbg;

template<class T, class FetchFunc>
static Array2D<T> gauss3_forwardMapping(Array2D<T> src) {
	T zero = ::zero<T>();
	Array2D<T> dst1(src.w, src.h);
	Array2D<T> dst2(src.w, src.h);
	forxy(dst1) {
		dst1(p) = .25f * (2.0f * FetchFunc::fetch(src, p.x, p.y) + get_clamped(src, p.x - 1, p.y) + FetchFunc::fetch(src, p.x + 1, p.y));
	}
	forxy(dst1) {
		//vector<float> weights = { .25, .5, .25 };
		//auto one = T(1);
		FetchFunc::fetch(dst2, p.x, p.y - 1) += .25f * dst1(p);
		FetchFunc::fetch(dst2, p.x, p.y) += .5f * dst1(p);
		FetchFunc::fetch(dst2, p.x, p.y + 1) += .25f * dst1(p);
		
	}
	return dst2;
}

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

		reset();

		// focus
		getWindow()->setAlwaysOnTop(true);
		getWindow()->setAlwaysOnTop(false);
	}
	void update()
	{
		bounces_dbg = Array2D<float>(sx, sy, 0);
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
		std::fill(red.img.begin(), red.img.end(), 0.0f);
		std::fill(red.tmpEnergy.begin(), red.tmpEnergy.end(), vec2());

		std::fill(green.img.begin(), green.img.end(), 0.0f);
		std::fill(green.tmpEnergy.begin(), green.tmpEnergy.end(), vec2());

		for (int x = 0; x < sz.x; x++) {
			for (int y = sz.y *.75; y < sz.y; y++) {
				//red.img(x, y) = 1;
			}
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
		//static auto envMap = gl::TextureCubeMap::create(loadImage(loadAsset("envmap_cube.jpg")), gl::TextureCubeMap::Format().mipmap());
		
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

		if (0) {
			//auto toDraw = img.clone();
			auto imgLocal = img.clone();//Array2D<float>(30, 30);
			imgLocal(15, 29) = 10;
			imgLocal(15, 28) = 9;
			imgLocal(15, 27) = 8;
			imgLocal(15, 26) = 7;
			//imgLocal(15, 25) = 6;
			auto toDraw = Array2D<vec3>(imgLocal.Size());
			forxy(toDraw) {
				if (imgLocal(p) < surfTensionThres) {
					toDraw(p).r = getElapsedFrames() % 2;
					continue;
				}
				auto g = gradient_i<float, WrapModes::GetClamped>(imgLocal, p);
				if (g.y < 0) {
					toDraw(p).r = -g.y;
					//toDraw(p) = std::log(toDraw(p) * 10);
				}
				else if (g.y > 0) toDraw(p).g = g.y;
				else toDraw(p).b = 1;
			}
			cout << "bottommost g val = " << toDraw(imgLocal.w / 2, imgLocal.h - 1) << endl;
			cout << "the one above that = " << toDraw(imgLocal.w / 2, imgLocal.h - 2) << endl;
			cout << "the one above that = " << toDraw(imgLocal.w / 2, imgLocal.h - 3) << endl;
			cout << "the one above that = " << toDraw(imgLocal.w / 2, imgLocal.h - 4) << endl;
			//::mm("todraw", toDraw);
			//toDraw = ::to01(toDraw);
			
			tex2 = gtex(toDraw);
			tex2 = shade2(tex2,
				"vec3 c = fetch3()*mouse.x*1000;"
				"_out.rgb = c;", ShadeOpts().ifmt(GL_RGB16F));
			//tex2 = ::get_gradients_tex(tex2, GL_CLAMP_TO_BORDER);
			//tex2 = ::get_laplace_tex(tex2, GL_CLAMP_TO_BORDER);
			tex2->setMagFilter(GL_NEAREST);
		}
		else if(0){
			tex2 = gtex(img);
			tex2 = shade2(tex2,
				"vec3 c = vec3(fetch1())*mouse.x*1;"
				"_out.rgb = c;", ShadeOpts().ifmt(GL_RGB16F));
			tex2->setMagFilter(GL_NEAREST);
			cout << "bottommost img val = " << img(img.w / 2, img.h - 1) << endl;
			cout << "the one above that = " << img(img.w / 2, img.h - 2) << endl;

		}
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
			//vec2 scaledm = vec2(getMousePos()-getWindow()->getPos()) / float(::scale); //vec2(mouseX * (float)sx, mouseY * (float)sy);
			vec2 scaledm = vec2(mouseX * (float)sx, mouseY * (float)sy);
			Area a(scaledm, scaledm);
			int r = 80 / pow(2, ::scale);
			a.expand(r, r);
			for (int x = a.x1; x <= a.x2; x++)
			{
				for (int y = a.y1; y <= a.y2; y++)
				{
					vec2 v = vec2(x, y) - scaledm;
					float w = std::max(0.0f, 1.0f - length(v) / r);
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
					float w = std::max(0.0f, 1.0f - length(v) / r);
					w = 3 * w * w - 2 * w * w * w;
					if (material->img.wr(x, y) != 0.0f)
						material->tmpEnergy.wr(x, y) += w * material->img.wr(x, y) * 4.0f * direction / (float)::scale;
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
		auto incompressibilityCoef = cfg1::getOpt("incompressibilityCoef", 1.0f,
			[&]() { return keys['/']; },
			[&]() { return expRange(mouseY, .0001f, 40000.0f); });


		//for (int i = 0; i < 4; i++) {
			//repel(::red, ::green);
			//repel(::green, ::red);
		//}

		for (auto material : ::materials) {
			auto& tmpEnergy = material->tmpEnergy;
			auto& img = material->img;

			forxy(tmpEnergy)
			{
				tmpEnergy(p) += vec2(0.0f, gravity) * img(p);
			}

			img = gauss3_forwardMapping<float, WrapModes::GetClamped>(img);
			tmpEnergy = gauss3_forwardMapping<vec2, WrapModes::GetClamped>(tmpEnergy);

			auto img_b = img.clone();
			//for(int i < 0
			img_b = gaussianBlur<float, WrapModes::GetClamped>(img_b, 3 * 2 + 1);
			auto& guidance = img_b;
			forxy(tmpEnergy)
			{
				/*if (p.x == tmpEnergy.w - 1)
					continue;
				if (p.y == tmpEnergy.h-1)
					continue;
				if (p.x == 0)
					continue;
				if (p.y == 0)
					continue;*/
				
				//auto g = gradient_i<float, WrapModes::NoWrap>(guidance, p);
				auto g = gradient_i<float, WrapModes::Get_WrapZeros>(guidance, p);
				if (img_b(p) < surfTensionThres)
				{
					// todo: move the  "* img(p)" back outside the if.
					// todo: readd the safeNormalized()
					//g = safeNormalized(g) * surfTension * img(p);
					g = g * surfTension * img(p);
				}
				else
				{
					g *= -incompressibilityCoef;
				}

				tmpEnergy(p) += g;
			}
			
			auto offsets = empty_like(tmpEnergy);
			forxy(offsets) {
				offsets(p) = tmpEnergy(p) / img(p);
			}
			advect(*material, offsets);
		}


	}
	void advect(Material& material, Array2D<vec2> offsets) {
		auto& img = material.img;
		auto& tmpEnergy = material.tmpEnergy;

		auto img3 = Array2D<float>(sx, sy);
		auto tmpEnergy3 = Array2D<vec2>(sx, sy, vec2());
		int count = 0;
		float sumOffsetY = 0; float div = 0;
		forxy(img)
		{
			if (img(p) == 0.0f)
				continue;

			vec2 offset = offsets(p);
			sumOffsetY += abs(offset.y); div++;
			vec2 dst = vec2(p) + offset;

			vec2 newEnergy = tmpEnergy(p);
			bool bounced = false;
			for (int dim = 0; dim <= 1; dim++) {
				float maxVal = sz[dim]-1;
				if (dst[dim] > maxVal) {
					newEnergy[dim] *= -1.0f;
					dst[dim] = maxVal - (dst[dim] - maxVal);
					//if(dim==1)
						//cout << "dst[dim]=" << dst[dim] << endl;
					bounced = true;
				}
				if (dst[dim] < 0) {
					newEnergy[dim] *= -1.0f;
					dst[dim] = -dst[dim];
					bounced = true;
				}
			}
			if (dst.y >= sz.y - 1)
				count++;
			//if(bounced)
			//	aaPoint<float, WrapModes::NoWrap>(bounces_dbg, dst, 1);
			aaPoint<float, WrapModes::GetClamped>(img3, dst, img(p));
			aaPoint<vec2, WrapModes::GetClamped>(tmpEnergy3, dst, newEnergy);
		}
		//cout << "bugged=" << count << endl;
		//cout << "sumOffsetY=" << sumOffsetY/div << endl;
		img = img3;
		tmpEnergy = tmpEnergy3;
	}
};

CrossThreadCallQueue * gMainThreadCallQueue;
CINDER_APP(SApp, ci::app::RendererGl(),
	[&](ci::app::App::Settings *settings)
{
	//bool developer = (bool)ifstream(getAssetPath("developer"));
	settings->setConsoleWindowEnabled(true);
})