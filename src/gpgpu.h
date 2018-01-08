#pragma once
#include "precompiled.h"
#include "shade.h"

inline gl::TextureRef get_gradients_tex(gl::TextureRef src) {
	return shade(list_of(src),
		"void shade(){"
		"	float srcL=fetch1(tex,tc+tsize*vec2(-1.0,0.0));"
		"	float srcR=fetch1(tex,tc+tsize*vec2(1.0,0.0));"
		"	float srcT=fetch1(tex,tc+tsize*vec2(0.0,-1.0));" 
		"	float srcB=fetch1(tex,tc+tsize*vec2(0.0,1.0));"
		"	float dx=(srcR-srcL)/2.0;"
		"	float dy=(srcB-srcT)/2.0;"
		"	_out.xy=vec2(dx,dy);"
		"}");
}
inline gl::TextureRef gradientForwardTex(gl::TextureRef src) {
	return shade(list_of(src),
		"void shade(){"
		"	float srcHere=fetch1(tex,tc);"
		"	float srcR=fetch1(tex,tc+tsize*vec2(1.0,0.0));"
		"	float srcB=fetch1(tex,tc+tsize*vec2(0.0,1.0));"
		"	float dx=(srcR-srcHere)/2.0;"
		"	float dy=(srcB-srcHere)/2.0;"
		"	_out.xy=vec2(dx,dy);"
		"}");
}
inline gl::TextureRef gauss3tex(gl::TextureRef src) {
	auto state = shade(list_of(src), "void shade() {"
		"vec3 sum = vec3(0.0);"
		"sum += fetch3(tex, tc + tsize * vec2(-1.0, -1.0)) / 16.0;"
		"sum += fetch3(tex, tc + tsize * vec2(-1.0, 0.0)) / 8.0;"
		"sum += fetch3(tex, tc + tsize * vec2(-1.0, +1.0)) / 16.0;"

		"sum += fetch3(tex, tc + tsize * vec2(0.0, -1.0)) / 8.0;"
		"sum += fetch3(tex, tc + tsize * vec2(0.0, 0.0)) / 4.0;"
		"sum += fetch3(tex, tc + tsize * vec2(0.0, +1.0)) / 8.0;"

		"sum += fetch3(tex, tc + tsize * vec2(+1.0, -1.0)) / 16.0;"
		"sum += fetch3(tex, tc + tsize * vec2(+1.0, 0.0)) / 8.0;"
		"sum += fetch3(tex, tc + tsize * vec2(+1.0, +1.0)) / 16.0;"
		"_out = sum;"
		"}");
	return state;
}

inline gl::TextureRef get_laplace_tex(gl::TextureRef src) {
	auto state = shade(list_of(src), "void shade() {"
		"vec3 sum = vec3(0.0);"
		"sum += fetch3(tex, tc + tsize * vec2(-1.0, 0.0)) * -1.0;"
		"sum += fetch3(tex, tc + tsize * vec2(0.0, -1.0)) * -1.0;"
		"sum += fetch3(tex, tc + tsize * vec2(0.0, +1.0)) * -1.0;"
		"sum += fetch3(tex, tc + tsize * vec2(+1.0, 0.0)) * -1.0;"
		"sum += fetch3(tex, tc + tsize * vec2(0.0, 0.0)) * 4.0;"
		"_out = sum;"
		"}");
	return state;
}

inline string combine_impl(gl::TextureRef tex, vector<gl::TextureRef>* texv)
{
	auto samplerSuffix = [&](int i) -> string {
		return (i == 0) ? "" : ToString(1 + i);
	};
	auto samplerName = [&](int i) -> string {
		return "tex" + samplerSuffix(i);
	};
	
	int texturesSoFar = texv->size();
	texv->push_back(tex);
	return "fetch3(" + samplerName(texturesSoFar) + ")";
}
inline string combine_impl(string str, vector<gl::TextureRef>* texv)
{
	return str;
}
template<class T, class U>
inline gl::TextureRef combine(T t, string op, U u)
{
	int texCount = 0;
	vector<gl::TextureRef> texv;
	string str0 = combine_impl(t, &texv);
	string str1 = combine_impl(u, &texv);
	return shade(texv, ("void shade() { _out = " + str0 + op + str1 + "; }").c_str());
}

// uses syntax like '+', not '+='
template<class T, class U>
inline void combine_ip(T& t, string op, U u)
{
	t = combine(t, op, u);
}