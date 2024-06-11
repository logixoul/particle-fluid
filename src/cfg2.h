#pragma once
#include "precompiled.h"

struct cfg2
{
	static void init();
	template<class T>
	static T getOpt(string const& name, string const& opts, T defaultValue);
	static ci::params::InterfaceGlRef params;
	static void render();
};

#define GETFLOAT(name, opts, defaultValue) float name = cfg2::cfg2<float>(#name, opts, defaultValue);
#define GETINT(name, opts, defaultValue) int name = cfg2::cfg2<int>(#name, opts, defaultValue);
#define GETBOOL(name, opts, defaultValue) bool name = cfg2::cfg2<bool>(#name, opts, defaultValue);
