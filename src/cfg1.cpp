#include "precompiled.h"
#include "using_namespace.h"
#include "cfg1.h"

std::map<string,cfg1::Opt> cfg1::opts;

float cfg1::getOpt(string name, float defaultVal, std::function<bool()> shouldUpdate, std::function<float()> getVal) {
	if (opts.find(name) == opts.end()) {
		Opt opt1;
		opt1.name = name;
		opt1.shouldUpdate = shouldUpdate;
		opt1.getVal = getVal;
		opt1.val = defaultVal;
		opts[name] = opt1;
	}
	auto& opt = opts[name];
	if (opt.shouldUpdate()) {
		opt.val = opt.getVal();
	}
	return opt.val;
}

void cfg1::print() {
	cout << "============ CFG values ============";
	for (auto& pair : opts) {
		auto& key = pair.first;
		auto opt = opts[key];
		cout << opt.name << " = " << opt.val;
	}
}
