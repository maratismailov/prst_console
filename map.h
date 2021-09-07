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

	Map drawing (stations, hypocenters, P- and S- wavefronts)

*******************************************************************************/

#ifndef MAP_H_DEF
#define MAP_H_DEF

#include <set>

#include "geometry.h"

#include "global.h"

#include "quake.h"

void PreLoad_Map();

class map_t
{
private:

	vec3_t origin;		// map origin in the original coordinates system (e.g. lon,lat,dep). Bottom-left corner on screen
	vec3_t size;		// map size   ""
	vec3_t v_origin;	// visible map area "" (must lie in the map region). Bottom-left corner on screen
	vec3_t v_size;		// map size   "" (must lie in the map region, with the same aspect ratio as the map region)

	float real_quake_lat;
	float real_quake_lon;
	float real_quake_dep;
	float real_quake_mag;

	float KM_PER_LON, KM_PER_LAT;

	void DrawFrame( float sx, float sy, float sw, float sh, float sh_dep, bool isGrid, bool isDep );	// Draw the frame with ticks and labels around the map

	// Convert between the original coordinates system (lon,lat,dep) and a system where (uv=0..1) spans the visible area (for drawing)
	// Note: v = 0 is at the top of the screen
	void LonLatToUV(float lon, float lat, float *u, float *v)	const;
	void UVToLonLat(float u, float v, float *lon, float *lat)	const;
	void DepToV(float dep, float *v)	const;

	// Set the visible region of the map in the original coordinates system (e.g. lon,lat,dep).
	// The actual visible region will be adjusted in order to show the whole requested while keeping the map aspect ratio
	void SetVisible(	float origin_lon, float origin_lat,
						float size_lon,   float size_lat		);

	void DrawLonLat(const string & s, float x, float y, float size, const colors_t & colors);

public:

	map_t() :
		origin(0,0,0),
		size(1,1,1),
		v_origin(0,0,0),
		v_size(1,1,1),
		real_quake_lat(sac_header_t::UNDEF),
		real_quake_lon(sac_header_t::UNDEF),
		real_quake_dep(sac_header_t::UNDEF),
		real_quake_mag(-1),
		KM_PER_LON(1),
		KM_PER_LAT(1)
	{
	}

	// Set the map origin and size in the original coordinates system (e.g. lon,lat,dep), as well as its screen position and size
	void Init(	float _origin_lon, float _origin_lat, float _origin_dep,
				float _size_lon,   float _size_lat,   float _size_dep		);

	void LoadRealQuake(const string & filename);

	void Draw(
		const set<station_t> & network,
		const vector<quake_t> & quakes,
		const binder_picks_set_t & picks,
		const win_t & win,
		float map_x, float map_y, float map_w
	);
};

extern map_t themap;

#endif
