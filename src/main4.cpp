#include "precompiled.h"
#include "shade.h"
#include "stuff.h"

int sx = 800, sy = 500;
gl::VboMeshRef vboMesh;

struct SApp : App {
	void setup()
	{
		setWindowSize(sx, sy);
		vector<vec2> poss(sx*sy);
		for (int i = 0; i < poss.size(); i+=4) {
			poss[i] = vec2(i%sx, i / sx);
		}
		gl::VboRef vbo = gl::Vbo::create(GL_ARRAY_BUFFER, poss, GL_STATIC_DRAW);
		geom::BufferLayout posLayout;
		posLayout.append(geom::POSITION, 2, sizeof(decltype(poss[0])), 0);
		vboMesh = gl::VboMesh::create(poss.size(), GL_POINTS,
		{ std::make_pair(posLayout, vbo) }
		);
	}
	void draw()
	{
		gl::clear(Color(0, 0, 0));
		
		static string vshader =
			"#version 450\n"
			"uniform mat4 proj;"
			"in vec2 ciPosition;"
			"void main() {"
			"	gl_Position = proj * vec4(ciPosition, 0.0, 1.0);"
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
		auto proj = gl::context()->getProjectionMatrixStack().back();
		gl::TextureRef imgt2 = maketex(sx, sy, GL_R16F);
		imgt2 = shade(list_of(imgt2), "void shade() { _out = vec3(0); }");
		prog->uniform("proj", proj);
		glPointSize(1);
		beginRTT(imgt2);
		gl::draw(vboMesh);
		endRTT();
		gl::checkError();
		gl::draw(imgt2, getWindowBounds());
	}
};		

CINDER_APP(SApp, RendererGl)