#include "precompiled.h"
#if 1
#include "ciextra.h"
#include "util.h"
#include "stuff.h"
#include "shade.h"
#include "gpgpu.h"
#include "gpuBlur2_4.h"
#include "cfg1.h"
#include "sw.h"
#include "my_console.h"
#include "hdrwrite.h"
#include <float.h>

typedef Array2D<float> Image;
int wsx=800, wsy = 800 * (800.0f / 1280.0f);
int scale = 4;
int sx = wsx / scale;
int sy = wsy / scale;
Image img(sx, sy);
Array2D<Vec2f> velocity(sx, sy, Vec2f::zero());
bool mouseDown_[3];
bool keys[256];
gl::Texture::Format gtexfmt;

float mouseX, mouseY;
bool pause;
bool keys2[256];

void updateConfig() {
}

struct SApp : AppBasic {
	Rectf area;
		
	void setup()
	{
		//keys2['0']=keys2['1']=keys2['2']=keys2['3']=true;
		//_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);

		_controlfp(_DN_FLUSH, _MCW_DN);

		area = Rectf(0, 0, (float)sx-1, (float)sy-1).inflated(Vec2f::zero());

		createConsole();
		
		glClampColor(GL_CLAMP_FRAGMENT_COLOR, GL_FALSE);
		glClampColor(GL_CLAMP_READ_COLOR, GL_FALSE);
		glClampColor(GL_CLAMP_VERTEX_COLOR, GL_FALSE);
		gtexfmt.setInternalFormat(hdrFormat);
		setWindowSize(wsx, wsy);

		tmpEnergy = Array2D<Vec2f>(sx, sy);
	}
	void keyDown(KeyEvent e)
	{
		keys[e.getChar()] = true;
		if(e.isControlDown()&&e.getCode()!=KeyEvent::KEY_LCTRL)
		{
			keys2[e.getChar()] = !keys2[e.getChar()];
			return;
		}
		if(keys['r'])
		{
			std::fill(img.begin(), img.end(), 0.0f);
			std::fill(velocity.begin(), velocity.end(), Vec2f::zero());
		}
		if(keys['p'] || keys['2'])
		{
			pause = !pause;
		}
	}
	void keyUp(KeyEvent e)
	{
		keys[e.getChar()] = false;
	}
	
	void mouseDown(MouseEvent e)
	{
		mouseDown_[e.isLeft() ? 0 : e.isMiddle() ? 1 : 2] = true;
	}
	void mouseUp(MouseEvent e)
	{
		mouseDown_[e.isLeft() ? 0 : e.isMiddle() ? 1 : 2] = false;
	}
	Vec2f direction;
	Vec2f lastm;
	void mouseDrag(MouseEvent e)
	{
		mm();
	}
	void mouseMove(MouseEvent e)
	{
		mm();
	}
	void mm()
	{
		direction = getMousePos() - lastm;
		lastm = getMousePos();
	}
	Array2D<float> img3;
	Array2D<Vec2f> tmpEnergy3;
	Array2D<Vec2f> tmpEnergy;

	void toTmpEnergy()
	{
		tmpEnergy = Array2D<Vec2f>(sx, sy);
		forxy(tmpEnergy)
		{
			tmpEnergy(p) = velocity(p) * img(p);
		}
	}

	void fromTmpEnergy()
	{
		forxy(tmpEnergy)
		{
			if(img(p) != 0.0f)
				velocity(p) = tmpEnergy(p) / img(p);
		}
	}

	void draw()
	{
		my_console::beginFrame();
		sw::beginFrame();
		static bool first = true;
		if(first) { mouseY = .4f; }
		first = false;

		wsx = getWindowSize().x;
		wsy = getWindowSize().y;

		mouseX = getMousePos().x / (float)wsx;
		auto surfTensionThres=cfg1::getOpt("surfTensionThres", .5f,
			[&]() { return keys['6']; },
			[&]() { return expRange(mouseY, 0.1f, 50000.0f); });
		auto surfTension=cfg1::getOpt("surfTension", 1.0f,
			[&]() { return keys['7']; },
			[&]() { return expRange(mouseY, .0001f, 40000.0f); });
		auto gravity=cfg1::getOpt("gravity", .1f,//0.0f,//.1f,
			[&]() { return keys['8']; },
			[&]() { return expRange(mouseY, .0001f, 40000.0f); });
		mouseY = getMousePos().y / (float)wsy;
		auto incompressibilityCoef=cfg1::getOpt("incompressibilityCoef", 1.0f,
			[&]() { return keys['\\']; },
			[&]() { return expRange(mouseY, .0001f, 40000.0f); });

		gl::clear(Color(0, 0, 0));

		if(!pause)
		{
			forxy(velocity)
			{
				tmpEnergy(p) += Vec2f(0.0f, gravity) * img(p);
			}

			sw::timeit("img&energy gauss3", [&]() {
			img=gauss3(img);
			tmpEnergy=gauss3(tmpEnergy);
			});

			auto img_b = img.clone();
			sw::timeit("surftension&incompressibility [blurs]", [&]() {
				//for(int i = 0; i < 4; i++)
				//	img_b = gauss3(img_b);
				img_b = gaussianBlur(img_b, 6*2+1);
			});
			sw::timeit("surftension&incompressibility [the rest]", [&]() {
				forxy(velocity)
				{
					auto& guidance = img_b;
					auto g = gradient_i(guidance, p);
					//g *= surfTension;

					/*if(guidance(p) < surfTensionThres)
					{
						g *= surfTension * (guidance(p) - surfTensionThres);
					}
					else
					{
						g *= guidance(p);
					}*/
					float mul = guidance(p);
					if(guidance(p) < surfTensionThres)
					{
						mul += -surfTension;
					}
					else
					{
						mul *= incompressibilityCoef;
					}
					g *= mul;
					g *= -1.0f;
					//g += surfTension * -g * (1.0f / (guidance(p) + 1.0f));

					tmpEnergy(p) += g; // 
				}
			});
			
			sw::timeit("processFluid", [&]() {
			int times=1;
			for(int i = 0; i < times; i++) {
				img3=Array2D<float>(sx, sy);
				tmpEnergy3=Array2D<Vec2f>(sx, sy, Vec2f::zero());
				auto area2=Rectf(area);
				area2.x2 -= .01f;
				area2.y2 -= .01f;
				forxy(img)
				{
					if(img(p) == 0.0f)
						continue;
					
					Vec2f vec = (tmpEnergy(p) / img(p)) / float(times);
	
					Vec2f dstOrig = Vec2f(p) + vec;
					Vec2f dst = area2.closestPoint(dstOrig);

					aaPoint2_fast(img3, dst, img(p));
	
					auto transferredEnergy = tmpEnergy(p);
					if(area2.contains(dstOrig))
						aaPoint2_fast(tmpEnergy3, dst, transferredEnergy);
				}
				img = img3;
				tmpEnergy = tmpEnergy3;
			}
			});
			
			if(mouseDown_[0])
			{
				Vec2f scaledm = Vec2f(mouseX * (float)sx, mouseY * (float)sy);
				Area a(scaledm, scaledm);
				int r = 5; //sqrt(800);
				a.expand(r, r);
				for(int x = a.x1; x <= a.x2; x++)
				{
					for(int y = a.y1; y <= a.y2; y++)
					{
						Vec2f v = Vec2f(x, y) - scaledm;
						float w = max(0.0f, 1.0f - v.length() / r);
						w = 3 * w * w - 2 * w * w * w;
						w=max(0.0f,w);
						img.wr(x, y) += 1.f * w /** exp(-5.0f + 10.0f * mouseX)*/*10.0;
					}
				}
			} else if(mouseDown_[2]) {
				mm();
				Vec2f scaledm = Vec2f(mouseX * (float)sx, mouseY * (float)sy);
				Area a(scaledm, scaledm);
				int r = 15; //sqrt(800);
				a.expand(r, r);
				for(int x = a.x1; x <= a.x2; x++)
				{
					for(int y = a.y1; y <= a.y2; y++)
					{
						Vec2f v = Vec2f(x, y) - scaledm;
						float w = max(0.0f, 1.0f - v.length() / r);
						w = 3 * w * w - 2 * w * w * w;
						if(img.wr(x, y) != 0.0f)
							tmpEnergy.wr(x, y) += /*(1.0f / img.wr(x, y)) * */ w * img.wr(x, y) * /*1.2f*/4.0f * direction / (float)scale;
					}
				}
			}

			//fromTmpEnergy();
		} // if ! pause
		sw::timeit("draw", [&]() {
			float limit = 65504 - 1;
			auto img5=img.clone();
			if(keys['9']) {
				forxy(img5) {
					img5(p) = tmpEnergy(p).length();
				}
			}
			if(keys['0']) {
				forxy(img5) {
					img5(p) = velocity(p).length() * sqrt(img(p));
				}
			}

			float currentMax = *std::max_element(img5.begin(), img5.end());
			if(currentMax > limit)
				cout << "clamping " << currentMax << " to " << limit << " (ratio " << (currentMax/limit) << ")" << endl;
			forxy(img5)
			{
				img5(p) = min(img5(p), limit);
			}
		
			auto tex = gtex(img5);

			static auto envMap = gl::Texture(ci::loadImage("envmap2.png"));
			//static auto envMap = gtex(loadRgbeFile("envmap.hdr"));
			globaldict["surfTensionThres"] = surfTensionThres;

			auto laplacetex = get_laplace_tex(tex);

			laplacetex  = shade2(laplacetex,
				"float laplace = max(fetch1(tex), 0.0);"
				"_out = vec3(laplace);"
				);
			//auto tex2b = gpuBlur2_4::run(tex2, 2, 2); // this is the slow part.
			auto laplacetexB = gpuBlur2_4::run_longtail(laplacetex, 5, 1.0f);
			auto laplacetexSum = shade2(laplacetex, laplacetexB,
				"_out = fetch3(tex) + fetch3(tex2);"
				);

			auto laplaceBGradientmapped = shade2(laplacetexSum,
				"float c = fetch1();"
				"c /= c + 1.0;"
				// this is taken from https://www.shadertoy.com/view/Mld3Rn
				"_out = vec3(min(c*1.5, 1.), pow(c, 2.5), pow(c, 12.)).zyx;"
				);
			
			auto grads = get_gradients_tex(tex);

			// WHAT I'M WORKING ON - fresnel. So I'm debugging it with drawing vec3(dot(N, I)).
			auto tex2 = shade2(tex, grads, envMap, laplaceBGradientmapped,
				"vec2 grad = fetch2(tex2);"
				// in the following i put - in the beginning randomly, to fix a bug whether the reflection would show the same
				// side of the sphere as the refraction. now the color'd normalmap is bluish rather than yellowish.
				// but when I draw vec3(dot(N, I)), it turns out that the original (without the '-') is correct (and its result
				// is very consistent with what I expect). Also the original makes fresnel work fine. So I reverted to the original.
				"vec3 N = normalize(vec3(-grad.x, -grad.y, -1.0));"
				"vec3 I=-normalize(vec3(tc.x-.5, tc.y-.5, 1.0));"
				"float eta=1.0/1.3;"
				"vec3 R=refract(I, N, eta);"
				"vec3 c = getEnv(R);"
				"vec3 albedo = 0.0*vec3(0.005, 0.0, 0.0);"
				"c = mix(albedo, c, pow(.9, fetch1(tex) * 50.0));" // tmp
				"R = reflect(I, N);"
				"float fresnelAmount = getFresnel(I, N);"
				"if(fetch1(tex) > surfTensionThres)"
				"	c += getEnv(R) * fresnelAmount * 10.0;" // *10.0 to tmp simulate one side of the envmap being brighter than the other
				
				"vec3 laplaceShadedB = fetch3(tex4);"
				"c += laplaceShadedB;"
				
				"_out = c;"
				, ShadeOpts().ifmt(GL_RGB16F).scale(4.0f),
				"float PI = 3.14159265358979323846264;\n"
				"float getFresnel(vec3 I, vec3 N) {"
				"	float R0 = 0.01;" // maybe is ok. but wikipedia has a way for calculating it.
				"	float dotted = dot(I, N);"
				"	return R0 + (1.0-R0) * pow(1.0-dotted, 5.0);"
				"}"
				"vec2 latlong(vec3 v) {\n"
				"v = v.xzy;\n"
				"v = normalize(v);\n"
				"float theta = acos(-v.z);\n" // +z is up
				"\n"
				"v.y=-v.y;\n"
				"float phi = atan(v.y, v.x) + PI;\n"
				//"return vec2(phi / (2.0*PI), theta / (PI/2.0));\n"
				"return vec2(phi / (2.0*PI), theta / (PI/2.0));\n"
				"}\n"
				"vec3 w = vec3(.22, .71, .07);"
				"vec3 getEnv(vec3 v) {\n"
				"	vec3 c = fetch3(tex3, latlong(v));\n"
				//"	c = 5.0*pow(c, vec3(2.0));"
				"	c = pow(c, vec3(2.2));" // gamma correction
				//"	c=smoothstep(vec3(0.0),vec3(1.0),c);"
				//"	float clum=dot(c, w);"
				//"	c *= pow(clum,1.0);" // make it darker
				//"	c/=vec3(1.0)-c*.99; c*=1.0;"
				"	return c;"
				"}\n"
				);

			// vignetting
			if(0) tex2 = shade2(tex2,
				"vec3 c = fetch3();"
				"vec2 tc2 = tc-vec2(.5);"
				//"c /= 1.0 + pow(length(tc2), 2.0);"
				"float ndist = length(tc2)/sqrt(.5*.5+.5*.5);" //normalized dist in [0,1]
				//"c *= pow(1.0-ndist, 3.0);"
				//"c *= exp(-ndist*ndist*12);"
				"float att = exp(-ndist*ndist*6);"
				"c /= c + vec3(1.0);"
				"c = pow(c, vec3(1.0/att));"
				"c /= vec3(1.0) - c;"
				"_out=c;"
				,ShadeOpts().scale(1.0f/::scale));

			tex2 = shade2(tex2,
				"vec3 c = fetch3(tex);"
				//"c = c / (c + vec3(1.0));"
				"if(c.r<0.0||c.g<0.0||c.b<0.0) { _out = vec3(1.0, 0.0, 0.0); }" // eases debugging
				"c = pow(c, vec3(1.0/2.2));"
				/*
				"c.g = smoothstep(0.0, 1.0, c.g);"
				"c.b = max(0.0, min(1.0, c.b));"
				"c.b =1.0-pow(1.0-c.b,2.0);"
				"c.r = mix(.2, 1.0, c.r);"
				*/
				"_out = c;"
				);
			gl::draw(tex2, getWindowBounds());
			//testGpuBlur();
		});

#if 0
		auto tex6 = shade(list_of(gtex(img))(gtex(colormap)), "void shade() { /*float y=fetch1();*/"
			//"_out=fetch3(tex2,vec2(0.0, y));"
			"vec3 c=vec3(0.0);"
			"c.g = mix(0.1, 0.9, tc.y);"
			"float f=fetch1();"
			"c.r += c.g * (f/(f+1.0));"
			"c = pow(c, vec3(1.0/2.2));"
			"_out=c;"
			"}");
#endif
		
		my_console::clr();
		cout<<"==============="<<endl;
		cout<<"min: " << *std::min_element(img.begin(),img.end()) << ", ";
		cout<<"max: " << *std::max_element(img.begin(),img.end()) << endl;
		auto velBegin=(float*)velocity.begin();
		auto velEnd=(float*)velocity.end();
		cout<<"vmin: " << *std::min_element(velBegin,velEnd) << ", ";
		cout<<"vmax: " << *std::max_element(velBegin,velEnd) << endl;

		Vec2i scaledm2 = Vec2i(mouseX * (float)sx, mouseY * (float)sy);
		auto toPrint = keys['g'] ? gradient_f(img, scaledm2) : gradient_i2(img, scaledm2);
		cout << "scale is " << scale << endl;
		cout << "gradient at p = " << toPrint << endl;
		cout << "surftension thres: " << surfTensionThres << endl;
		cout << "surface tension: " << surfTension << endl;
		cout << "gravity: " << gravity << endl;
		cout << "fps: " << getFrameRate() << endl;
		sw::endFrame();
		cfg1::print();
		my_console::endFrame();

		if(pause)
			Sleep(50);
		//Sleep(5);
	}
	// nullTerminatedLoadFile
	string ntLoadFile(string filename)
	{
		vector<unsigned char> vec;
		loadFile(vec, filename);
		return string(vec.begin(), vec.end());
	}
	void draw(gl::Texture tex, Rectf srcRect, Rectf dstRect)
	{
		glBegin(GL_QUADS);
		Rectf tc_(srcRect.getUpperLeft() / tex.getSize(), srcRect.getLowerRight() / tex.getSize());
		glTexCoord2f(tc_.getX1(), tc_.getY1()); glVertex2f(dstRect.getX1(), dstRect.getY1());
		glTexCoord2f(tc_.getX2(), tc_.getY1()); glVertex2f(dstRect.getX2(), dstRect.getY1());
		glTexCoord2f(tc_.getX2(), tc_.getY2()); glVertex2f(dstRect.getX2(), dstRect.getY2());
		glTexCoord2f(tc_.getX1(), tc_.getY2()); glVertex2f(dstRect.getX1(), dstRect.getY2());
		glEnd();
	}
	template<class T>
	Vec2f gradient_f(Array2D<T> src, Vec2f p)
	{
		Vec2f gradient;
		gradient.x = getBilinear(src, p.x + 1, p.y) - getBilinear(src, p.x - 1, p.y);
		gradient.y = getBilinear(src, p.x, p.y + 1) - getBilinear(src, p.x, p.y - 1);
		return gradient;
	}
};

//#include <lib/define_winmain.h>


//CINDER_APP_BASIC(SApp, RendererGl)
int WINAPI WinMain(HINSTANCE hInstance,HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {	
	try{
		createConsole();
		cinder::app::AppBasic::prepareLaunch();														
		cinder::app::AppBasic *app = new SApp;														
		cinder::app::Renderer *ren = new RendererGl;													
		cinder::app::AppBasic::executeLaunch( app, ren, "SApp" );										
		cinder::app::AppBasic::cleanupLaunch();														
		return 0;																					
	}catch(std::exception const& e) {
		cout << "caught: " << endl << e.what() << endl;
		//cout << "(exception type: " << typeinfo
		my_console::endFrame();
		//int dummy;cin>>dummy;
		system("pause");



	}
}

#endif