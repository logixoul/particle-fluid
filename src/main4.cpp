#include "precompiled.h"
#include "util.h"
#include "stuff.h"
#include "shade.h"
#include "gpgpu.h"
#include "gpuBlur2_5.h"
#include "stefanfw.h"
#include "Fbo.h"

#include "hdrwrite.h"

// with scale = 1:
// 16fps

typedef Array2D<float> Image;
int wsx=800, wsy = 800 * (800.0f / 1280.0f);
int sx = wsx;
int sy = wsy;
Image img(sx, sy);

gl::VboMeshRef vboMesh;

struct SApp : App {
	void setup()
	{
		disableGLReadClamp();
		stefanfw::eventHandler.subscribeToEvents(*this);
		setWindowSize(wsx, wsy);

		vector<vec2> poss(sx*sy);
		for(int i = 0; i < poss.size(); i++) {
			poss[i] = vec2(i%sx, i / sx);
		}
		gl::VboRef vbo = gl::Vbo::create(GL_ARRAY_BUFFER, poss, GL_STATIC_DRAW);
		geom::BufferLayout posLayout;
		posLayout.append(geom::POSITION, 2, sizeof(decltype(poss[0])), 0);
		vboMesh = gl::VboMesh::create(poss.size(), GL_POINTS,
			{ std::make_pair(posLayout, vbo) }
		);
	}
	void update()
	{
		stefanfw::beginFrame();
		stefanUpdate();
		stefanDraw();
		stefanfw::endFrame();
	}
	void stefanDraw()
	{
		gl::clear(Color(0, 0, 0));

		auto tex = gtex(img);
		tex->setTopDown(true);
		gl::draw(tex, getWindowBounds());
	}
	void stefanUpdate()
	{
		auto imgt = gtex(img);
		static string vshader =
			"#version 450\n"
			"in vec2 ciPosition;"
			"void main() {"
			"	gl_Position.xy = ciPosition.xy;"
			"}"
			;
		static string fshader =
			"#version 450\n"
			"layout(location = 0) out float img;"
			"void main() {"
			"	img = 1;"
			"}";

		gl::GlslProgRef prog = gl::GlslProg::create(
			gl::GlslProg::Format().vertex(vshader).fragment(fshader).attribLocation("ciPosition", 0).preprocess(false));

		gl::ScopedGlslProg sgp(prog);
					
		gl::TextureRef imgt2 = maketex(sx, sy, GL_R16F);
		Fbo fbo(list_of(imgt2));
		fbo.bind();
		GLenum myBuffers[] = { GL_COLOR_ATTACHMENT0 };
		glDrawBuffers(1, myBuffers);
		glPointSize(10);
		gl::draw(vboMesh);
		img = gettexdata<float>(imgt2, GL_RED, GL_FLOAT);
		Fbo::unbind();
		gl::checkError();
			
		if(mouseDown_[0])
		{
			vec2 scaledm = vec2(mouseX * (float)sx, mouseY * (float)sy);
			Area a(scaledm, scaledm);
			int r = 80;
			a.expand(r, r);
			for(int x = a.x1; x <= a.x2; x++)
			{
				for(int y = a.y1; y <= a.y2; y++)
				{
					img.wr(x, y) += 1;
				}
			}
		}
	}
};		

CINDER_APP(SApp, RendererGl)