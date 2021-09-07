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

	RTLoc interface for PRESTo

*******************************************************************************/

#ifndef RTLOC_H_DEF
#define RTLOC_H_DEF

#include "global.h"

#include "quake.h"
#include "rtloc/rtloclib.h"

class rtloc_t
{
private:

	Control params;

	struct Station *station;

	GridDesc Grid;		// Location grid

	GridDesc *Pgrid;	// P-Travel time grid for each station
	GridDesc *Sgrid;	// S-Travel time grid for each station

	int StationNameToId(const string & stname);

public:

	rtloc_t();
	~rtloc_t();

	void Init(const string & ctrlfile);
	void Locate( quake_t & q, origin_t *o );
	void DistanceWithError(const string & stname, const origin_t & o, float *distance, float *error);

	// x,y in km. 0 is the grid center
	// z in km. 0 is the ground level
	void LonLat_To_XY(float lon, float lat, float *x, float *y);
	void XY_To_LonLat(float x, float y, float *lon, float *lat);
	bool IsPointInGrid(float lon, float lat, float dep);
	void GetGridArea(float *min_lon, float *min_lat, float *min_dep, float *max_lon, float *max_lat, float *max_dep, float *dx, float *dy, float *dz);

	void GetStationLonLatDep(const string & stname, float *lon, float *lat, float *dep);
	float LonLatDep_Distance_km(float lon1, float lat1, float dep1, float lon2, float lat2, float dep2);

//	string ClosestStationName(const origin_t & o);
	string GroundStationName();
	float TravelTime(const string & stname, char wave, float lon, float lat, float dep);
//	float QuakeTargetTravelTime(char wave, const origin_t & o, const origin_t & t);
	float QuakeRadiusAfterSecs(char wave, const origin_t & o, float secs);
	
	void LogVelocityModel();
};

extern rtloc_t rtloc;

#endif
