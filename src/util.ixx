//#include "precompiled.h"
#include "util.h" // for Array2D
#include <vector>
#include <functional>
using std::vector;
using std::function;

export module util;

export float randFloat()
{
	return rand() / (float)RAND_MAX;
}

export template<class InputIt, class T>
T accumulate(InputIt begin, InputIt end, T base) {
	T sum = base;
	for (auto it = begin; it != end; it++) {
		sum += *it;
	}
	return sum;
}

export template<class T>
Array2D<T> empty_like(Array2D<T> a) {
	return Array2D<T>(a.Size(), nofill());
}

export template<class T>
Array2D<T> ones_like(Array2D<T> a) {
	return Array2D<T>(a.Size(), 1.0f);
}

export template<class T>
Array2D<T> zeros_like(Array2D<T> a) {
	return Array2D<T>(a.Size(), ::zero<T>());
}
export template<class T>
void myRemoveIf(vector<T>& vec, function<bool(T const&)> const& pred) {
	vec.erase(std::remove_if(vec.begin(), vec.end(), pred), vec.end());
}

export const float pi = 3.14159265f;

export void rotate(vec2& p, float angle)
{
	float c = cos(angle), s = sin(angle);
	p = vec2(p.x * c + p.y * (-s), p.x * s + p.y * c);
}

export void trapFP()
{
	// Get the default control word.
	int cw = _controlfp_s(NULL, 0, 0);

	// Set the exception masks OFF, turn exceptions on.
	cw &= ~(EM_OVERFLOW | EM_UNDERFLOW |/*EM_INEXACT|*/EM_ZERODIVIDE | EM_DENORMAL);

	// Set the control word.
	_controlfp_s(NULL, cw, MCW_EM);
}
