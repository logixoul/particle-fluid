#include "precompiled.h"

export module util;

export float randFloat()
{
	return rand() / (float)RAND_MAX;
}

export template<class T> int test() {
	return 5;
}