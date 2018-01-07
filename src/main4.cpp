#include "precompiled.h"
#include "util.h"
#include "stuff.h"
#include "shade.h"
#include "stefanfw.h"

int wsx=800, wsy = 800 * (800.0f / 1280.0f);
int sx = wsx;
int sy = wsy;
Array2D<float> img(sx, sy);

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
		stefanfw::endFrame();
	}
	void stefanUpdate()
	{
		gl::clear(Color(0, 0, 0));

		static string vshader =
			"#version 450\n"
			"in vec2 ciPosition;"
			"void main() {"
			"	gl_Position.xy = ciPosition.xy;"
			"	gl_Position.z = 0.0;"
			//"	gl_Position.w = 1.0;"
			"}"
			;
		static string fshader =
			"#version 450\n"
			"layout(location = 0) out vec4 img;"
			"void main() {"
			"	img = vec4(1, 1, 0, 1);"
			"}";

		gl::GlslProgRef prog = gl::GlslProg::create(
			gl::GlslProg::Format().vertex(vshader).fragment(fshader).attribLocation("ciPosition", 0).preprocess(false));

		gl::ScopedGlslProg sgp(prog);
					
		glPointSize(10);
		gl::draw(vboMesh);
		gl::checkError();
	}
};		

CINDER_APP(SApp, RendererGl)