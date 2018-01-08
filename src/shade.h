#pragma once

#include "precompiled.h"
#include "util.h"
#include "TextureCache.h"

void beginRTT(gl::TextureRef fbotex);
void endRTT();

struct Str {
	string s;
	Str& operator<<(string s2) {
		s += s2 + "\n";
		return *this;
	}
	Str& operator<<(Str s2) {
		s += s2.s + "\n";
		return *this;
	}
	operator std::string() {
		return s;
	}
};

extern std::map<string, float> globaldict;
void globaldict_default(string s, float f);
gl::TextureRef shade(gl::TextureRef const& texv, const char* fshader_constChar);
