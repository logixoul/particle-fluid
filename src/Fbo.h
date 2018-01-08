#pragma once

#include "precompiled.h"

class Fbo {
public:
	Fbo(vector<gl::TextureRef> texv);
	void bind();
	static void unbind();
	~Fbo();

private:
	GLuint id;
};