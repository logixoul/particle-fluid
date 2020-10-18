#include "precompiled.h"
#include "util.h"
#include "stuff.h"
#include "shade.h"
#include "gpgpu.h"
#include "gpuBlur2_5.h"
#include "stefanfw.h"
#include "Fbo.h"



typedef Array2D<float> Image;
int wsx=800, wsy = 800 * (800.0f / 1280.0f);
int scale = 2;
int sx = wsx / ::scale;
int sy = wsy / ::scale;
Image img(sx, sy);
Array2D<vec2> velocity(sx, sy, vec2());
float surfTensionThres;

bool pause;

void updateConfig() {
}

gl::VboMeshRef vboMesh;

struct SApp : App {
	Rectf area;
		
	void setup()
	{
		enableDenormalFlushToZero();

		area = Rectf(0, 0, (float)sx-1, (float)sy-1).inflated(vec2());

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
	}
	void keyDown(KeyEvent e)
	{
		if(keys['r'])
		{
			std::fill(img.begin(), img.end(), 0.0f);
			std::fill(velocity.begin(), velocity.end(), vec2());
		}
		if(keys['p'] || keys['2'])
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
		sw::timeit("draw", [&]() {
			gl::clear(Color(0, 0, 0));

			gl::setMatricesWindow(getWindowSize(), false);
			float limit = 65;
			auto img5 = img.clone();
			if (keys['9']) {
				forxy(img5) {
					img5(p) = length(tmpEnergy(p));
				}
			}
			if (keys['0']) {
				forxy(img5) {
					img5(p) = length(tmpEnergy(p)) / img5(p);
				}
			}

			float currentMax = *std::max_element(img5.begin(), img5.end());
			float currentMin = *std::min_element(img5.begin(), img5.end());
			cout << "currentMin=" << currentMin << endl; // prints 0 always
			if (currentMax > limit)
				cout << "clamping " << currentMax << " to " << limit << " (ratio " << (currentMax / limit) << ")" << endl;
			forxy(img5)
			{
				img5(p) = min(img5(p), limit);
			}
			
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
			//tex2 = shade2(tex, "_out.r = fetch1() / (fetch1() + 1);");
			tex2 = shade2(tex,
				"float c = fetch1()*.2;"
				"_out.r = c / (c + 1);");
			gl::draw(tex2, getWindowBounds());
		});
	}
	void stefanUpdate()
	{
		surfTensionThres=cfg1::getOpt("surfTensionThres", .5f,
			[&]() { return keys['6']; },
			[&]() { return expRange(mouseY, 0.1f, 50000.0f); });
		auto surfTension=cfg1::getOpt("surfTension", 1.0f,
			[&]() { return keys['7']; },
			[&]() { return expRange(mouseY, .0001f, 40000.0f); });
		auto gravity=cfg1::getOpt("gravity", .1f,//0.0f,//.1f,
			[&]() { return keys['8']; },
			[&]() { return expRange(mouseY, .0001f, 40000.0f); });
		auto incompressibilityCoef=cfg1::getOpt("incompressibilityCoef", 1.0f,
			[&]() { return keys['\\']; },
			[&]() { return expRange(mouseY, .0001f, 40000.0f); });

		if(!pause)
		{
			forxy(velocity)
			{
				tmpEnergy(p) += vec2(0.0f, gravity) * img(p);
			}

			sw::timeit("img&energy gauss3", [&]() {
				img=gauss3(img);
				tmpEnergy=gauss3(tmpEnergy);
			});

			auto img_b = img.clone();
			sw::timeit("surftension&incompressibility [blurs]", [&]() {
				img_b = gaussianBlur<float, WrapModes::GetClamped>(img_b, 6*2+1);
			});
			sw::timeit("surftension&incompressibility [the rest]", [&]() {
				forxy(velocity)
				{
					auto& guidance = img_b;
					auto g = gradient_i<float, WrapModes::GetClamped>(guidance, p);
					float mul = guidance(p);
					if(guidance(p) < surfTensionThres)
					{
						mul += -surfTension;
					}
					else
					{
						mul *= incompressibilityCoef;
					}
					g *= mul;
					g *= -1.0f;

					tmpEnergy(p) += g; // 
				}
			});
			
			sw::timeit("processFluid", [&]() {
				int times = 1;
				for (int i = 0; i < times; i++) {
					img3 = Array2D<float>(sx, sy);
					tmpEnergy3 = Array2D<vec2>(sx, sy, vec2());
					auto area2 = Rectf(area);
					area2.x2 -= .01f;
					area2.y2 -= .01f;
					forxy(img)
					{
						if (img(p) == 0.0f)
							continue;

						vec2 vec = (tmpEnergy(p) / img(p)) / float(times);

						vec2 dstOrig = vec2(p) + vec;
						vec2 dst = area2.closestPoint(dstOrig);

						aaPoint2_fast(img3, dst, img(p));

						auto transferredEnergy = tmpEnergy(p);
						if (area2.contains(dstOrig))
							aaPoint2_fast(tmpEnergy3, dst, transferredEnergy);
					}
					img = img3;
					tmpEnergy = tmpEnergy3;
				}
			});
			
			if(mouseDown_[0])
			{
				vec2 scaledm = vec2(mouseX * (float)sx, mouseY * (float)sy);
				Area a(scaledm, scaledm);
				int r = 80 / pow(2, ::scale);
				a.expand(r, r);
				for(int x = a.x1; x <= a.x2; x++)
				{
					for(int y = a.y1; y <= a.y2; y++)
					{
						vec2 v = vec2(x, y) - scaledm;
						float w = max(0.0f, 1.0f - length(v) / r);
						w = 3 * w * w - 2 * w * w * w;
						img.wr(x, y) += 1.f * w *10.0;
					}
				}
			} else if(mouseDown_[2]) {
				mm();
				vec2 scaledm = vec2(mouseX * (float)sx, mouseY * (float)sy);
				Area a(scaledm, scaledm);
				int r = 15;
				a.expand(r, r);
				for(int x = a.x1; x <= a.x2; x++)
				{
					for(int y = a.y1; y <= a.y2; y++)
					{
						vec2 v = vec2(x, y) - scaledm;
						float w = max(0.0f, 1.0f - length(v) / r);
						w = 3 * w * w - 2 * w * w * w;
						if(img.wr(x, y) != 0.0f)
							tmpEnergy.wr(x, y) += w * img.wr(x, y) * 4.0f * direction / (float)::scale;
					}
				}
			}
		} // if ! pause


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

		if(pause)
			Sleep(50);
	}
};		

CINDER_APP(SApp, RendererGl)