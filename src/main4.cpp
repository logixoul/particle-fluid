#include "precompiled.h"
#include "util.h"
#include "stuff.h"
#include "shade.h"
#include "gpgpu.h"
#include "gpuBlur2_5.h"
#include "stefanfw.h"
#include "Fbo.h"



typedef Array2D<float> Image;
int wsx = 800, wsy = 800 * (800.0f / 1280.0f);
int scale = 2;
int sx = wsx / ::scale;
int sy = wsy / ::scale;
ivec2 sz(sx, sy);
Image img(sx, sy);
float surfTensionThres;
int bigVelocityCount;
Array2D<float> bigVelocityImg(sx, sy, 0.0f);

bool pause = true;

void updateConfig() {
}

gl::VboMeshRef vboMesh;

template<class T, class FetchFunc>
void aaPoint3(Array2D<T>& dst, vec2 p, T value)
{
	aaPoint3<T, FetchFunc>(dst, p.x, p.y, value);
}
template<class T, class FetchFunc>
void aaPoint3(Array2D<T>& dst, float x, float y, T value)
{
	int ix = x, iy = y;
	float fx = ix, fy = iy;
	if (x < 0.0f && fx != x) { fx--; ix--; }
	if (y < 0.0f && fy != y) { fy--; iy--; }
	float fractx = x - fx;
	float fracty = y - fy;
	float fractx1 = 1.0 - fractx;
	float fracty1 = 1.0 - fracty;
	FetchFunc::fetch(dst, ix, iy) += (fractx1 * fracty1) * value;
	FetchFunc::fetch(dst, ix, iy + 1) += (fractx1 * fracty) * value;
	FetchFunc::fetch(dst, ix + 1, iy) += (fractx * fracty1) * value;
	FetchFunc::fetch(dst, ix + 1, iy + 1) += (fractx * fracty) * value;
}

struct SApp : App {
	void setup()
	{
		enableDenormalFlushToZero();

		createConsole();

		disableGLReadClamp();
		stefanfw::eventHandler.subscribeToEvents(*this);
		setWindowSize(wsx, wsy);

		tmpEnergy = Array2D<vec2>(sx, sy);

		vector<vec2> poss(sx*sy);
		for (int i = 0; i < poss.size(); i++) {
			poss[i] = vec2(i%sx, i / sx);
		}
		gl::VboRef convectionVbo = gl::Vbo::create(GL_ARRAY_BUFFER, poss, GL_STATIC_DRAW);
		geom::BufferLayout posLayout;
		posLayout.append(geom::POSITION, 2, sizeof(decltype(poss[0])), 0);
		vboMesh = gl::VboMesh::create(poss.size(), GL_POINTS,
			{ std::make_pair(posLayout, convectionVbo) }
		);
	}
	void update()
	{
		stefanfw::beginFrame();
		stefanUpdate();
		stefanDraw();
		stefanfw::endFrame();
		cout << "bigVelocityCount=" << bigVelocityCount << endl;
	}
	void keyDown(KeyEvent e)
	{
		if (keys[' ']) {
			doFluidStep();
		}
		if (keys['r'])
		{
			std::fill(img.begin(), img.end(), 0.0f);
			std::fill(tmpEnergy.begin(), tmpEnergy.end(), vec2());
		}
		if (keys['p'] || keys['2'])
		{
			pause = !pause;
		}
	}
	vec2 direction;
	vec2 lastm;
	void mouseDrag(MouseEvent e)
	{
		mm();
	}
	void mouseMove(MouseEvent e)
	{
		mm();
	}
	void mm()
	{
		direction = vec2(getMousePos()) - lastm;
		lastm = getMousePos();
	}
	Array2D<float> img3;
	Array2D<vec2> tmpEnergy3;
	Array2D<vec2> tmpEnergy;
	void stefanDraw()
	{
		gl::clear(Color(0, 0, 0));

		gl::setMatricesWindow(getWindowSize(), false);
		float limit = 65;
		auto img5 = img.clone();
		/*if (keys['9']) {
			forxy(img5) {
				img5(p) = length(tmpEnergy(p));
			}
		}
		if (keys['0']) {
			forxy(img5) {
				img5(p) = length(tmpEnergy(p)) / img5(p);
			}
		}*/
		if (keys['0']) {
			img5 = bigVelocityImg.clone();
		}
		float maxElement = *std::max_element(img5.begin(), img5.end());
		//forxy(img5)img5(p) /= maxElement;
		cout << "maxElement " << maxElement << endl;

		auto tex = gtex(img5);

		static auto envMap = gl::Texture::create(ci::loadImage("envmap2.png"));
		globaldict["surfTensionThres"] = surfTensionThres;

		auto laplacetex = get_laplace_tex(tex);

		laplacetex = shade2(laplacetex,
			"float laplace = max(fetch1(tex), 0.0);"
			"_out = vec3(laplace);"
		);
		auto laplacetexB = gpuBlur2_5::run_longtail(laplacetex, 4, 1.0f);

		auto laplaceBGradientmapped = shade2(laplacetexB,
			"float c = fetch1();"
			"c *= 8.0;"
			"c /= c + 1.0;"
			// this is taken from https://www.shadertoy.com/view/Mld3Rn
			"_out = vec3(min(c*1.5, 1.), pow(c, 2.5), pow(c, 12.)).zyx;"
			, ShadeOpts().ifmt(GL_RGBA16F)
		);

		auto grads = get_gradients_tex(tex);

		auto tex2 = shade2(tex, grads, envMap, laplaceBGradientmapped,
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

			"vec3 laplaceShadedB = fetch3(tex4);"
			"c += laplaceShadedB;"

			"_out = c;"
			, ShadeOpts().ifmt(GL_RGB16F).scale(4.0f),
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
			"if(c.r<0.0||c.g<0.0||c.b<0.0) { _out = vec3(1.0, 0.0, 0.0); }" // eases debugging
			"c = pow(c, vec3(1.0/2.2));"
			"_out = c;"
		);

		if (0)tex2 = shade2(tex,
			"float c = fetch1();"
			"_out.r = c;");
		//tex2->setMagFilter(GL_NEAREST);
		gl::draw(tex2, getWindowBounds());
	}
	void stefanUpdate()
	{
		if (!pause)
		{
			doFluidStep();

		} // if ! pause
		if (mouseDown_[0])
		{
			vec2 scaledm = vec2(mouseX * (float)sx, mouseY * (float)sy);
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
					img.wr(x, y) += 1.f * w *10.0;
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
					if (img.wr(x, y) != 0.0f)
						tmpEnergy.wr(x, y) += w * img.wr(x, y) * 4.0f * direction / (float)::scale;
				}
			}
		}

#if 0
		auto tex6 = shade(list_of(gtex(img))(gtex(colormap)), "void shade() { /*float y=fetch1();*/"
			//"_out=fetch3(tex2,vec2(0.0, y));"
			"vec3 c=vec3(0.0);"
			"c.g = mix(0.1, 0.9, tc.y);"
			"float f=fetch1();"
			"c.r += c.g * (f/(f+1.0));"
			"c = pow(c, vec3(1.0/2.2));"
			"_out=c;"
			"}");
#endif

		if (pause)
			Sleep(50);
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

	void doFluidStep() {
		bigVelocityCount = 0;
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

		forxy(tmpEnergy)
		{
			tmpEnergy(p) += vec2(0.0f, gravity) * img(p);
		}

		img = gauss3(img);
		tmpEnergy = gauss3(tmpEnergy);

		auto img_b = img.clone();
		img_b = gaussianBlur<float, WrapModes::GetClamped>(img_b, 3 * 2 + 1);
		//auto kernel = ::getGaussianKernel(13, ::sigmaFromKsize(13));
		//img_b = ::separableConvolve<float, WrapModes::GetClamped>(img_b, kernel);
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
		bigVelocityImg = zeros_like(bigVelocityImg);

		img3 = Array2D<float>(sx, sy);
		tmpEnergy3 = Array2D<vec2>(sx, sy, vec2());
		forxy(img)
		{
			if (img(p) == 0.0f)
				continue;

			vec2 vec = (tmpEnergy(p) / img(p));
			if (length(vec) > img.w / 2) {
				bigVelocityCount++;
				bigVelocityImg(p) = 1.0f;
			}
			vec2 dst = vec2(p) + vec;

			vec2 newEnergy = tmpEnergy(p);
			for (int dim = 0; dim <= 1; dim++) {
				if (dst[dim] >= sz[dim]) {
					newEnergy[dim] *= -1.0f;
					dst[dim] = sz[dim] - (dst[dim] - sz[dim]);
				}
			}
			aaPoint3<float, WrapModes::GetClamped>(img3, dst, img(p));
			aaPoint3<vec2, WrapModes::GetClamped>(tmpEnergy3, dst, newEnergy);
		}
		img = img3;
		tmpEnergy = tmpEnergy3;
	}
};

CINDER_APP(SApp, RendererGl)