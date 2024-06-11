#pragma once
#include "precompiled.h"

struct GetOpt
{
	static void init();
	template<class T>
	static T getOpt(string const& name, string const& opts, T defaultValue);
	static ci::params::InterfaceGl* params;
	static void render();
};

#define GETFLOAT(name, opts, defaultValue) float name = GetOpt::getOpt<float>(#name, opts, defaultValue);
#define GETINT(name, opts, defaultValue) int name = GetOpt::getOpt<int>(#name, opts, defaultValue);
#define GETBOOL(name, opts, defaultValue) bool name = GetOpt::getOpt<bool>(#name, opts, defaultValue);
