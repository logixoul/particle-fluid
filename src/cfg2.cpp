#include "precompiled.h"
#include "cfg2.h"
#include "CinderImGui.h"


void cfg2::init()
{
	ImGui::Initialize();
}

void cfg2::begin()
{
	ImGui::Begin("Parameters");
}

void cfg2::end()
{
	ImGui::End();
}

/*template<class T>
T cfg2::getOpt(string const& name, string const& opts, T defaultValue) //static
{
	static map<string, T> m;

	if (!m.count(name)) {
		m[name] = defaultValue;
		params->addParam(name, &m[name], opts);
	}
	auto value = m[name];
	return value;
}*/

template<> int cfg2::getOpt<int>(string const& name, string const& opts, int defaultValue) {
	int val = defaultValue;
	ImGui::DragInt(name.c_str(), &val);
	return val;
}
template<> float cfg2::getOpt<float>(string const& name, string const& opts, float defaultValue) {
	float val = defaultValue;
	ImGui::DragFloat(name.c_str(), &val);
	return val;
}
template<> bool cfg2::getOpt<bool>(string const& name, string const& opts, bool defaultValue) {
	bool val = defaultValue;
	ImGui::Checkbox(name.c_str(), &val);
	return val;
}
