#include "precompiled.h"
#include "shade.h"
#include "util.h"
#include "qdebug.h"
#include "stuff.h"

void beginRTT(gl::TextureRef fbotex)
{
	static unsigned int fboid = 0;
	if(fboid == 0)
	{
		glGenFramebuffersEXT(1, &fboid);
	}
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, fboid);
	glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, fbotex->getId(), 0);
}
void endRTT()
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

static void drawRect() {
	auto ctx = gl::context();

	ctx->getDrawTextureVao()->bind();
	//ctx->getDrawTextureVbo()->bind(); // this seems to be unnecessary

	ctx->drawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

std::map<string, float> globaldict;
void globaldict_default(string s, float f) {
	if(globaldict.find(s) == globaldict.end())
	{
		globaldict[s] = f;
	}
}

auto samplerSuffix = [&](int i) -> string {
	return (i == 0) ? "" : ToString(1 + i);
};

auto samplerName = [&](int i) -> string {
	return "tex" + samplerSuffix(i);
};

std::string getCompleteFshader(std::string const& fshader) {
	string intro =
		Str()
		<< "#version 130"
		<< "vec3 _out = vec3(0.0);"
		<< "out vec4 OUTPUT;";
		string outro =
		Str()
		<< "void main()"
		<< "{"
		<< "	OUTPUT.a = 1.0;"
		<< "	shade();"
		<< "	OUTPUT.rgb = _out;"
		<< "}";
	return intro + fshader + outro;
}

gl::TextureRef shade(gl::TextureRef const& texv, const char* fshader_constChar)
{
	const string fshader(fshader_constChar);

	static std::map<string, gl::GlslProgRef> shaders;
	gl::GlslProgRef shader;
	if(shaders.find(fshader) == shaders.end())
	{
		std::string completeFshader = getCompleteFshader(fshader);
		try{
			auto fmt = gl::GlslProg::Format()
				.vertex(
					Str()
					<< "#version 150"
					<< "in vec4 ciPosition;"
					<< "void main()"
					<< "{"
					<< "	gl_Position = ciPosition * 2 - 1;"
					<< "	gl_Position.y = -gl_Position.y;"
					<< "}")
				.fragment(completeFshader)
				.attribLocation("ciPosition", 0)
				.preprocess(false);
			shader = gl::GlslProg::create(fmt);
			shaders[fshader] = shader;
		} catch(gl::GlslProgCompileExc const& e) {
			cout << "gl::GlslProgCompileExc: " << e.what() << endl;
			cout << "source:" << endl;
			cout << completeFshader << endl;
			throw;
		}
	} else {
		shader = shaders[fshader];
	}
	shader->bind();
	gl::TextureRef result;
	ivec2 resultSize(texv->getWidth(), texv->getHeight());
	GLenum ifmt = texv->getInternalFormat();
	result = maketex(resultSize.x, resultSize.y, ifmt);

	beginRTT(result);
	
	gl::pushMatrices();
	{
		gl::ScopedViewport sv(ivec2(), result->getSize());
		gl::setMatricesWindow(ivec2(1, 1), true);
		::drawRect();
		gl::popMatrices();
	}

	endRTT();

	return result;
}
