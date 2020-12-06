#include "precompiled.h"

export module util;

export float randFloat()
{
	return rand() / (float)RAND_MAX;
}

