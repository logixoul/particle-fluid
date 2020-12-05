#pragma once
#include "precompiled.h"

class cfg1
{
public:
	static float getOpt(string name, float defaultVal, std::function<bool()> shouldUpdate, std::function<float()> getVal);
	static void print();
private:
	struct Opt
	{
		string name;
		std::function<float()> getVal;
		float val;
		std::function<bool()> shouldUpdate;
	};
	static std::map<string,Opt> opts;
};