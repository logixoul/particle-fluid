#include "precompiled.h"
//#include "using_namespace.h"
#include "stuff.h"
#include "shade.h"
#include "gpgpu.h"
#include "gpuBlur2_5.h"
#include "stefanfw.h"
#include "Array2D_imageProc.h"
#include "cfg1.h"
#include "CrossThreadCallQueue.h"
#include "cfg2.h"
#include "CinderImGui.h"
//#include "MyVideoWriter.h"

#include "util.h"

typedef Array2D<float> Image;
int wsx = 800, wsy = 800;
int scale = 2;
int sx = wsx / ::scale;
int sy = wsy / ::scale;
ivec2 sz(sx, sy);

// https://lucasschuermann.com/writing/implementing-sph-in-2d
struct Particle {
	vec2 pos;
	vec2 velocity;
	vec2 force;
	float rho, p; // density and pressure
};



const static vec2 G(0.f, 10.f);   // external (gravitational) forces
const static float REST_DENS = 300.f;  // rest density
const static float GAS_CONST = 2000.f; // const for equation of state
const static float H = 16.f;           // kernel radius
const static float HSQ = H * H;        // radius^2 for optimization
const static float MASS = 2.5f;        // assume all particles have the same mass
const static float VISC = 200.f;       // viscosity constant
const static float DT = 0.0007f;       // integration timestep

// smoothing kernels defined in Müller and their gradients
// adapted to 2D per "SPH Based Shallow Water Simulation" by Solenthaler et al.
const static float POLY6 = 4.f / (M_PI * pow(H, 8.f));
const static float SPIKY_GRAD = -10.f / (M_PI * pow(H, 5.f));
const static float VISC_LAP = 40.f / (M_PI * pow(H, 5.f));

// simulation parameters
const static float EPS = H; // boundary epsilon
const static float BOUND_DAMPING = -0.5f;

vector<Particle> particles;

void reset() {
	particles.clear();
}

void ComputeDensityPressure(void)
{
	for (auto& pi : particles)
	{
		pi.rho = 0.f;
		for (auto& pj : particles)
		{
			vec2 rij = pj.pos - pi.pos;
			float r2 = dot(rij, rij);

			if (r2 < HSQ)
			{
				// this computation is symmetric
				pi.rho += MASS * POLY6 * pow(HSQ - r2, 3.f);
			}
		}
		pi.p = GAS_CONST * (pi.rho - REST_DENS);
	}
}
	
void ComputeForces(void)
{
	for (auto& pi : particles)
	{
		vec2 fpress(0.f, 0.f);
		vec2 fvisc(0.f, 0.f);
		for (auto& pj : particles)
		{
			if (&pi == &pj)
			{
				continue;
			}

			vec2 rij = pj.pos - pi.pos;
			float r = length(rij);

			if (r < H)
			{
				// compute pressure force contribution
				fpress += -normalize(rij) * MASS * (pi.p + pj.p) / (2.f * pj.rho) * SPIKY_GRAD * pow(H - r, 3.f);
				// compute viscosity force contribution
				fvisc += VISC * MASS * (pj.velocity - pi.velocity) / pj.rho * VISC_LAP * (H - r);
			}
		}
		vec2 fgrav = G * MASS / pi.rho;
		pi.force = fpress + fvisc + fgrav;
	}
}

void Integrate(void)
{
	for (auto& p : particles)
	{
		// forward Euler integration
		p.velocity += DT * p.force / p.rho;
		p.pos += DT * p.velocity;

		// enforce boundary conditions
		if (p.pos.x - EPS < 0.f)
		{
			p.velocity.x *= BOUND_DAMPING;
			p.pos.x = EPS;
		}
		if (p.pos.x + EPS > sx)
		{
			p.velocity.x *= BOUND_DAMPING;
			p.pos.x = sx - EPS;
		}
		if (p.pos.y - EPS < 0.f)
		{
			p.velocity.y *= BOUND_DAMPING;
			p.pos.y = EPS;
		}
		if (p.pos.y + EPS > sy)
		{
			p.velocity.y *= BOUND_DAMPING;
			p.pos.y = sy - EPS;
		}
	}
}

bool pause = false;

struct SApp : ci::app::App {
	//shared_ptr<MyVideoWriter> videoWriter = make_shared<MyVideoWriter>();
	

	void cleanup() {
		//videoWriter.reset();
	}

	void setup()
	{
		cfg2::init();

		enableDenormalFlushToZero();

		disableGLReadClamp();
		stefanfw::eventHandler.subscribeToEvents(*this);
		setWindowSize(wsx, wsy);

		reset();

		// focus
		getWindow()->setAlwaysOnTop(true);
		getWindow()->setAlwaysOnTop(false);
	}
	void update()
	{
		stefanfw::beginFrame();
		stefanUpdate();
		stefanDraw();
		stefanfw::endFrame();
	}
	void keyDown(ci::app::KeyEvent e)
	{
		if (e.getChar() == 'd')
		{
			cfg2::params->isVisible() ? cfg2::params->hide() : cfg2::params->show();
		}

		if (keys[' ']) {
			doFluidStep();
		}
		if (keys['r'])
		{
			reset();
		}
		if (keys['p'] || keys['2'])
		{
			pause = !pause;
		}
	}
	vec2 direction;
	vec2 lastm;
	void mouseDrag(ci::app::MouseEvent e)
	{
		mm();
	}
	void mouseMove(ci::app::MouseEvent e)
	{
		mm();
	}
	void mm()
	{
		direction = vec2(getMousePos()) - lastm;
		lastm = getMousePos();
	}
	void stefanDraw()
	{
		float blurSize = cfg2::getOpt("blurSize", "", 1.0f);
		int blurIters = cfg2::getOpt("blurIters", "", 4.0f);
		float blurMul = cfg2::getOpt("blurMul", "", 0.2f);
		const float bloomSize = 1.0f;
		const int bloomIters = 4.0f;
		const float bloomIntensity = 0.2f;

		gl::clear(Color(0, 0, 0));

		gl::setMatricesWindow(getWindowSize(), false);
		auto img = Array2D<float>(sz);
		for (auto& particle : particles) {
			aaPoint<float, WrapModes::GetClamped>(img, particle.pos, 10 * blurMul);
		}
		
		auto tex = gtex(img);
		tex = gpuBlur2_5::run_longtail(tex, blurIters, blurSize);
		auto redTex = gtex(img);
		
		auto redTexB = gpuBlur2_5::run_longtail(redTex, bloomIters, bloomSize);
		
		redTex = shade2( redTex, redTexB, MULTILINE(
			_out.r = (fetch1() + fetch1(tex2)) * bloomIntensity;
		),
			ShadeOpts().uniform("bloomIntensity", bloomIntensity)
			);
		/*greenTex = shade2(greenTex, greenTexB, MULTILINE(
			_out.r = (fetch1() + fetch1(tex2)) * bloomIntensity;
		),
			ShadeOpts().uniform("bloomIntensity", bloomIntensity)
			);*/
		static auto envMap = gl::Texture::create(ci::loadImage(loadAsset("envmap2.png")));
		//static auto envMap = gl::TextureCubeMap::create(loadImage(loadAsset("envmap_cube.jpg")), gl::TextureCubeMap::Format().mipmap());
		//gl::TextureBaseRef test=envMap;
		auto grads = get_gradients_tex(tex);

		auto tex2 = shade2(tex, grads, envMap, redTex, /*greenTex,*/
			"vec2 grad = fetch2(tex2);"
			"vec3 N = normalize(vec3(-grad.x, -grad.y, -1.0));"
			"vec3 I=-normalize(vec3(tc.x-.5, tc.y-.5, 1.0));"
			"float eta=1.0/1.3;"
			"vec3 R=refract(I, N, eta);"
			"vec3 c = getEnv(R);"
			//"vec3 c = N;"
			"vec3 albedo = 0.0*vec3(0.005, 0.0, 0.0);"
			"c = mix(albedo, c, pow(.9, fetch1(tex) * 50.0));" // tmp
			"R = reflect(I, N);"
			"if(fetch1(tex) > surfTensionThres)"
			"	c += getEnv(R) * 5.0;" // mul to tmp simulate one side of the envmap being brighter than the other

			"float redVal = fetch1(tex4);"
			//"float greenVal = fetch1(tex5);"
			// this is taken from https://www.shadertoy.com/view/Mld3Rn
			"vec3 redColor = vec3(min(redVal * 1.5, 1.), pow(redVal, 2.5), pow(redVal, 12.)); "
			//"vec3 greenColor = vec3(min(greenVal * 1.5, 1.), pow(greenVal, 2.5), pow(greenVal, 12.)).zyx; "
			"c += redColor;"
			//"c += greenColor;"

			"_out.rgb = c;"
			, ShadeOpts().ifmt(GL_RGB16F).scale(4.0f).uniform("surfTensionThres", 0.1f),
			"float PI = 3.14159265358979323846264;\n"
			"vec2 latlong(vec3 v) {\n"
			"v = v.xzy;\n"
			"v = normalize(v);\n"
			"float theta = asin(v.z);\n" // +z is up
			"\n"
			"v.y=-v.y;\n"
			"float phi = atan(v.y, v.x) + PI/2;\n"
			"return vec2(phi / (PI), theta / (PI/2.0));\n"
			"}\n"
			"vec3 w = vec3(.22, .71, .07);"
			"vec3 getEnv(vec3 v) {\n"
			"	vec3 c = fetch3(tex3, latlong(v));\n"
			"	c = pow(c, vec3(2.2));" // gamma correction
			"	return c;"
			"}\n"
		);

		tex2 = shade2(tex2,
			"vec3 c = fetch3(tex);"
			"if(c.r<0.0||c.g<0.0||c.b<0.0) { _out.rgb = vec3(1.0, 0.0, 0.0); }" // eases debugging
			"c = pow(c, vec3(1.0/2.2));"
			"_out.rgb = c;"
		);

		tex2 = tex;
		tex2 = shade2(tex2,
			"float f = fetch1();"
			"float fw = fwidth(f);"
			"f = smoothstep(0.5-fw/2, 0.5+fw/2, f);"
			//"f = dFdx(f)+dFdy(f);"
			"_out.rgb = vec3(f);"
			, ShadeOpts().ifmt(GL_RGB16F));

		//videoWriter->write(tex2);
		
		gl::draw(tex2, getWindowBounds());

		cfg2::render();
	}
	void stefanUpdate()
	{
		if (!pause)
		{
			doFluidStep();

		} // if ! pause
		if (mouseDown_[0])
		{
			vec2 scaledm = vec2(mouseX * (float)sx, mouseY * (float)sy);
			float t = getElapsedFrames();

			Particle part; part.pos = scaledm + vec2(sin(t), cos(t)) * 30.0f;
			particles.push_back(part);
		}
		else if (mouseDown_[2]) {
			mm();
			vec2 scaledm = vec2(mouseX * (float)sx, mouseY * (float)sy);
			Area a(scaledm, scaledm);
			int r = 15;
			a.expand(r, r);
			for (int x = a.x1; x <= a.x2; x++)
			{
				for (int y = a.y1; y <= a.y2; y++)
				{
					vec2 v = vec2(x, y) - scaledm;
					float w = std::max(0.0f, 1.0f - length(v) / r);
					w = 3 * w * w - 2 * w * w * w;
					for (Particle& part : particles) {
						if (distance(part.pos, v) < 10)
							part.velocity += 4.0f * direction / (float)::scale;
					}
				}
			}
		}
	}

	void doFluidStep() {
		ComputeDensityPressure();
		ComputeForces();
		Integrate();
	}
};

CrossThreadCallQueue * gMainThreadCallQueue;
CINDER_APP(SApp, ci::app::RendererGl(),
	[&](ci::app::App::Settings *settings)
{
	//bool developer = (bool)ifstream(getAssetPath("developer"));
	settings->setConsoleWindowEnabled(true);
})