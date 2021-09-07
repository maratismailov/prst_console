/*******************************************************************************
 This file is part of PRESTo Early Warning System
 Copyright (C) 2009-2015 Luca Elia

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
*******************************************************************************/

/*******************************************************************************


	RTLoc Origin class - position, uncertainty and time


*******************************************************************************/

#ifndef ORIGIN_H_DEF
#define ORIGIN_H_DEF

// needed!:
#include "SDL_opengl.h"

#include "global.h"

#include "place.h"
#include "rtloc/GridLib.h"

struct origin_t : public place_t
{
	float mean_x, mean_y, mean_z;
	float cov_xx, cov_yy, cov_zz;
	Ellipsoid3D ell;
	secs_t time;

	bool operator != (const origin_t & rhs);

	origin_t(float _lon, float _lat, float _dep)
	: place_t("",_lon,_lat,_dep),mean_x(0),mean_y(0),mean_z(0),cov_xx(0),cov_yy(0),cov_zz(0),ell(EllipsoidNULL),time(0)
	{ }

	~origin_t()
	{ }

	friend ostream& operator<< (ostream& os, const origin_t& o);
};

#endif
