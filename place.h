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

	place_t - Helper class to store a geographic point and provide distance
	          and travel times using RTLoc functions (for e.g. stations/targets)

*******************************************************************************/

#ifndef PLACE_H_DEF
#define PLACE_H_DEF

#include "global.h"

class place_t
{
public:
	string name;
	float lon, lat, dep;

	place_t();
	place_t(const string & _name, float _lon, float _lat, float _dep);
	virtual ~place_t() {}

	bool operator < (const place_t & rhs) const
	{
		return name < rhs.name;
	}

	float Distance   ( const place_t & p ) const;
	float EpiDistance( const place_t & p ) const;
};

class gridplace_t : public place_t
{
public:

	gridplace_t();
	gridplace_t(const string & _name, float _lon, float _lat, float _dep) : place_t(_name, _lon, _lat, _dep) { }
	virtual ~gridplace_t() {}

	float CalcSDelay ( const place_t & p ) const;
	float CalcTravelTime( const char wave, const place_t & p ) const;
};

#endif
