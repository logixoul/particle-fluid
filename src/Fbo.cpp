#include "precompiled.h"
#include "Fbo.h"

Fbo::Fbo(vector<gl::TextureRef> texv) {
	glGenFramebuffersEXT(1, &id);
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, id);
	for (int i = 0; i < texv.size(); i++) {
		glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT + i, GL_TEXTURE_2D, texv[i]->getId(), 0);
	}
}

void Fbo::bind() {
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, id);
}

void Fbo::unbind()
{
	glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
}

Fbo::~Fbo()
{
	glDeleteFramebuffersEXT(1, &id);
}
