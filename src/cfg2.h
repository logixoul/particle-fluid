#pragma once
#include "precompiled.h"
#include "CinderImGui.h"

struct cfg2
{
	static void init();
	//static bool getBool(string const& name, bool defaultValue);
	//static int getInt(string const& name, int min, int max, int defaultValue, ImGuiSliderFlags_ flags = ImGuiSliderFlags_::ImGuiSliderFlags_None);
	//static float getFloat(string const& name, float min, float max, float defaultValue, ImGuiSliderFlags_ flags_ flags = ImGuiSliderFlags_::ImGuiSliderFlags_None);
	static void begin();
	static void end();
};
