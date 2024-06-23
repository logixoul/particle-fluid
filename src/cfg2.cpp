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

template<class T> T& getOpt_Base(string const& name, T defaultValue) {
	static map<string, T> m;
	if (!m.count(name)) {
		m[name] = defaultValue;
	}
	return m[name];
}

template<> int cfg2::getOpt<int>(string const& name, string const& opts, int defaultValue) {
	auto& ref = getOpt_Base<int>(name, defaultValue);
	ImGui::DragInt(name.c_str(), &ref);
	return ref;
}
template<> float cfg2::getOpt<float>(string const& name, string const& opts, float defaultValue) {
	auto& ref = getOpt_Base<float>(name, defaultValue);
	ImGui::DragFloat(name.c_str(), &ref);
	return ref;
}
template<> bool cfg2::getOpt<bool>(string const& name, string const& opts, bool defaultValue) {
	auto& ref = getOpt_Base<bool>(name, defaultValue);
	ImGui::Checkbox(name.c_str(), &ref);
	return ref;
}
