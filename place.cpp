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

#include "place.h"

#include "rtloc.h"

/*******************************************************************************

	place_t - Helper class to store a geographic point and provide distances
	          using RTLoc functions

*******************************************************************************/

place_t :: place_t()
{}

place_t :: place_t(const string & _name, float _lon, float _lat, float _dep)
	:	name(_name), lon(_lon), lat(_lat), dep(_dep)
{}

float place_t :: Distance( const place_t & p ) const
{
	return rtloc.LonLatDep_Distance_km( p.lon,p.lat,p.dep, lon,lat,dep );
}

float place_t :: EpiDistance( const place_t & p ) const
{
	return rtloc.LonLatDep_Distance_km( p.lon,p.lat,0, lon,lat,0 );
}

/*******************************************************************************

	gridplace_t - A place_t that additionally provides travel times using RTLoc
	              functions (for e.g. stations/targets with a travel time grid)

*******************************************************************************/

gridplace_t :: gridplace_t()
{}

float gridplace_t :: CalcSDelay( const place_t & p ) const
{
	return	rtloc.TravelTime(name, 'S', p.lon, p.lat, p.dep) - 
			rtloc.TravelTime(name, 'P', p.lon, p.lat, p.dep) ;
}

float gridplace_t :: CalcTravelTime( const char wave, const place_t & p ) const
{
	return rtloc.TravelTime(name, wave, p.lon, p.lat, p.dep);
}
