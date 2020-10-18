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
		if (keys[' ']) {
			doFluidStep();
		}
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

			auto tex = gtex(img5);

			auto tex2 = shade2(tex,
				"float c = fetch1()*.2;"
				"_out.r = c / (c + 1);");
			tex2->setMagFilter(GL_NEAREST);
			gl::draw(tex2, getWindowBounds());
		});
	}
	void stefanUpdate()
	{
		if(!pause)
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

		if(pause)
			Sleep(50);
	}

	void doFluidStep() {
		surfTensionThres = cfg1::getOpt("surfTensionThres", .5f,
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

		forxy(velocity)
		{
			tmpEnergy(p) += vec2(0.0f, gravity) * img(p);
		}

		sw::timeit("img&energy gauss3", [&]() {
			img = gauss3(img);
			tmpEnergy = gauss3(tmpEnergy);
		});

		auto img_b = img.clone();
		sw::timeit("surftension&incompressibility [blurs]", [&]() {
			img_b = gaussianBlur<float, WrapModes::GetClamped>(img_b, 6 * 2 + 1);
		});
		sw::timeit("surftension&incompressibility [the rest]", [&]() {
			forxy(velocity)
			{
				auto& guidance = img_b;
				auto g = gradient_i<float, WrapModes::GetClamped>(guidance, p);
				float mul = guidance(p);
				if (guidance(p) < surfTensionThres)
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
	}
};		

CINDER_APP(SApp, RendererGl)