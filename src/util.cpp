/*
Tonemaster - HDR software
Copyright (C) 2018, 2019, 2020 Stefan Monov <logixoul@gmail.com>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "precompiled.h"
#include "util.h"


/*void createConsole()
{
	AllocConsole();
	std::fstream* fsOut = new std::fstream("CONOUT$");
	std::cout.rdbuf(fsOut->rdbuf());
	std::fstream* fsIn = new std::fstream("CONIN$");
	std::cin.rdbuf(fsIn->rdbuf());
}*/

void copyCvtData(ci::Surface8u const & surface, Array2D<bytevec3> dst)
{
	forxy(dst) {
		ColorAT<uint8_t> inPixel = surface.getPixel(p);
		dst(p) = bytevec3(inPixel.r, inPixel.g, inPixel.b);
	}
}

void copyCvtData(ci::Surface8u const& surface, Array2D<vec3> dst) {
	forxy(dst) {
		ColorAT<uint8_t> inPixel = surface.getPixel(p);
		dst(p) = vec3(inPixel.r, inPixel.g, inPixel.b) / 255.0f;
	}
}
void copyCvtData(ci::SurfaceT<float> const& surface, Array2D<vec3> dst) {
	forxy(dst) {
		ColorAT<float> inPixel = surface.getPixel(p);
		dst(p) = vec3(inPixel.r, inPixel.g, inPixel.b);
	}
}
void copyCvtData(ci::SurfaceT<float> const& surface, Array2D<float> dst) {
	forxy(dst) {
		ColorAT<float> inPixel = surface.getPixel(p);
		dst(p) = inPixel.r;
	}
}
void copyCvtData(ci::ChannelT<float> const& surface, Array2D<float> dst) {
	forxy(dst) {
		float inPixel = surface.getValue(p);
		dst(p) = inPixel;
	}
}

