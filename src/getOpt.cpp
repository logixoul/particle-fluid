#include "precompiled.h"
#include "getOpt.h"

ci::params::InterfaceGl* GetOpt::params;

void GetOpt::init()
{
	params = new ci::params::InterfaceGl( "App parameters", ci::ivec2( 200, 400 ) );

	params->hide();
}

void GetOpt::render()
{
	if(GetOpt::params->isVisible())
	{
		//glColor4f(1,1,1,1);
		//glPushAttrib(GL_ALL_ATTRIB_BITS);
		glActiveTexture(GL_TEXTURE0);
		glUseProgram(0);
			
		params->draw();
			
		//static Font font("Arial", 16);
		//gl::drawString(Pfl::getText(), vec2(20, 430), ColorA::white()); // freezes the app for some reason
		//glPopAttrib();
	}
}

template<class T>
T GetOpt::getOpt(string const& name, string const& opts, T defaultValue) //static
{
	static map<string, T> m;

	if(!m.count(name))
		m[name] = defaultValue;
	auto value = m[name];
	params->addParam(name, &m[name], opts);
	return value;
}

//template class GetOpt<float>;
//template class GetOpt<int>;
template int GetOpt::getOpt<int>(string const& name, string const& opts, int defaultValue);
template float GetOpt::getOpt<float>(string const& name, string const& opts, float defaultValue);
template bool GetOpt::getOpt<bool>(string const& name, string const& opts, bool defaultValue);
//GetOpt::getOpt<float>;