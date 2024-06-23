#include "precompiled.h"
#include "cfg2.h"

ci::params::InterfaceGlRef cfg2::params;

void cfg2::init()
{
	//ci::app::getWindow()->getContentScale();
	params = ci::params::InterfaceGl::create( "App parameters", ci::ivec2(200, 400));
	
	//TwDefine(" GLOBAL fontsize=3 ");

	//params->hide();
}

void cfg2::render()
{
	if(cfg2::params->isVisible())
	{
		//glColor4f(1,1,1,1);
		//glPushAttrib(GL_ALL_ATTRIB_BITS);
		glActiveTexture(GL_TEXTURE0);
		glUseProgram(0);
			
		gl::pushMatrices();
		//gl::scale(2.0f, 2.0f);
		//glScalef(2.0f, 2.0f, 2.0f);
		params->draw();
		gl::popMatrices();
			
		//static Font font("Arial", 16);
		//gl::drawString(Pfl::getText(), vec2(20, 430), ColorA::white()); // freezes the app for some reason
		//glPopAttrib();
	}
}

template<class T>
T cfg2::getOpt(string const& name, string const& opts, T defaultValue) //static
{
	static map<string, T> m;

	if (!m.count(name)) {
		m[name] = defaultValue;
		params->addParam(name, &m[name], opts);
	}
	auto value = m[name];
	return value;
}

template int cfg2::getOpt<int>(string const& name, string const& opts, int defaultValue);
template float cfg2::getOpt<float>(string const& name, string const& opts, float defaultValue);
template bool cfg2::getOpt<bool>(string const& name, string const& opts, bool defaultValue);
