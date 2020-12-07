#include "precompiled.h"

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
