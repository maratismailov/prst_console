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

#include <fstream>
#include <limits>
#include <map>
#include <iomanip>
#include <algorithm>

#include "map.h"

#include "graphics2d.h"
#include "rtloc.h"
#include "target.h"
#include "pgx.h"
#include "config.h"
#include "loading_bar.h"
#include "SDL_opengl.h"

/*******************************************************************************

	Local variables / functions

*******************************************************************************/

namespace
{

// Tweakable parameters:

const colors_t	colors_station					=	colors_t(0.3f,1.0f,0.3f,1.0f);
const colors_t	colors_station_ghost			=	colors_t(0.6f,0.6f,0.6f,0.8f);
const colors_t	colors_station_loc				=	colors_t(1.0f,0.6f,0.3f,1.0f);
const colors_t	colors_station_pick				=	colors_t(1.0f,1.0f,0.3f,1.0f);

const colors_t	colors_event					=	colors_t(1.0f,0.2f,0.2f,1.0f);
const colors_t	colors_event_real				=	colors_t(1.0f,1.0f,1.0f,0.7f);

// Textures:

texptr_t Tex_Map()
{
	static texptr_t tex(net_dir + "map.png");
	return tex;
}

texptr_t Tex_Station()
{
	static texptr_t tex("station.png");
	return tex;
}

texptr_t Tex_Quake()
{
	static texptr_t tex("quake.png");
	return tex;
}

texptr_t Tex_Circle()
{
	static texptr_t tex("circle.png");
	return tex;
}

texptr_t Tex_Wave()
{
	static texptr_t tex("wave.png");
	return tex;
}

texptr_t Tex_Target()
{
	static texptr_t tex("target.png");
	return tex;
}

}	// namespace

/*******************************************************************************

	Global variables / functions

*******************************************************************************/

// Load real earthquake location from disk (to display on the map)

void map_t :: LoadRealQuake(const string & filename)
{
	const std::streamsize w1 = 15;

	cout << endl;
	cout << "==================================================================================================" << endl;
	cout << "    Real Earthquake Location and Magnitude (" << filename <<  ")" << endl;
	cout << endl;
	cout <<	setw(w1)	<<	"Lat (deg)"			<<	" | "	<<
			setw(w1)	<<	"Lon (deg)"			<<	" | "	<<
			setw(w1)	<<	"Dep (km)"			<<	" | "	<<
			setw(w1)	<<	"Mag"				<<	endl;
	cout << "==================================================================================================" << endl;

	ifstream f(filename.c_str());
	if (f)
	{
		SkipComments(f);

		f >> real_quake_lat >> real_quake_lon >> real_quake_dep >> real_quake_mag;

		if (f.fail())
			Fatal_Error("Parsing real quake location and magnitude in file \"" + filename + "\". Use this format: Lat(deg) Lon(deg) Dep(km) Mag");
	}

	// Check if the real quake location (if specified) lies outside the grid

	if ( (real_quake_lon != sac_header_t::UNDEF) || (real_quake_lat != sac_header_t::UNDEF) || (real_quake_dep != sac_header_t::UNDEF) || (real_quake_mag != -1)	 )
	{
		if ( (real_quake_lon == sac_header_t::UNDEF) || (real_quake_lat == sac_header_t::UNDEF) || (real_quake_dep == sac_header_t::UNDEF) || (real_quake_mag == -1)	 )
			Fatal_Error("Not all real quake parameters specified in file \"" + filename + "\"");

		if ( real_quake_mag <= 0 || real_quake_mag >= 10 )
			Fatal_Error("Invalid real quake magnitude (" +  ToString(real_quake_mag) + ") in file \"" + filename + "\"");

		if ( !rtloc.IsPointInGrid(real_quake_lon, real_quake_lat, real_quake_dep) )
			Fatal_Error("RTLoc: the real quake location in file \"" + filename + "\" lies outside the grid");
	}

	cout << setw(w1)	<<	real_quake_lat	<<	" | "	<<
			setw(w1)	<<	real_quake_lon	<<	" | "	<<
			setw(w1)	<<	real_quake_dep	<<	" | "	<<
			setw(w1)	<<	real_quake_mag	<<	endl;

	cout << "==================================================================================================" << endl;
}

void map_t :: LonLatToUV(float lon, float lat, float *u, float *v)	const
{
	*u = (lon - v_origin.x) / v_size.x;
	*v = 1.0f - (lat - v_origin.y) / v_size.y;
}

void map_t :: DepToV(float dep, float *v)	const
{
	*v = (dep - v_origin.z) / v_size.z;
}

void map_t :: UVToLonLat(float u, float v, float *lon, float *lat)	const
{
	*lon = Interp(v_origin.x, v_origin.x + v_size.x, u);
	*lat = Interp(v_origin.y, v_origin.y + v_size.y, 1-v);
}

void map_t :: Init(	float _origin_lon, float _origin_lat, float _origin_dep,
					float _size_lon,   float _size_lat,   float _size_dep		)
{
	float lon	=	_origin_lon + _size_lon / 2;
	float lat	=	_origin_lat + _size_lat / 2;

	KM_PER_LON	=	rtloc.LonLatDep_Distance_km(lon,lat,0, lon+1,lat,0);
	KM_PER_LAT	=	rtloc.LonLatDep_Distance_km(lon,lat,0, lon,lat+1,0);

	origin.x	=	_origin_lon;
	origin.y	=	_origin_lat;
	origin.z	=	_origin_dep;

	size.x		=	_size_lon;
	size.y		=	_size_lat;
	size.z		=	_size_dep;

	v_origin	=	origin;
	v_size		=	size;
}


void map_t :: SetVisible(	float origin_lon, float origin_lat,
							float size_lon,   float size_lat		)
{
	// Clamp the requested visible area to the map area

	float max_lon = origin_lon + size_lon;
	Clamp(origin_lon, origin.x, origin.x + size.x);
	Clamp(max_lon,    origin.x, origin.x + size.x);
	size_lon = max_lon - origin_lon;

	float max_lat = origin_lat + size_lat;
	Clamp(origin_lat, origin.y, origin.y + size.y);
	Clamp(max_lat,    origin.y, origin.y + size.y);
	size_lat = max_lat - origin_lat;

	if (param_display_map_fixed_size)
	{
		// on-screen height/width ratio

		float ratio =  (size.y * KM_PER_LAT) / (size.x * KM_PER_LON);

		// Fit a rectangle with the same aspect ratio as the on-screen map over the requested lon-lat area

		float w_km = size_lon * KM_PER_LON;
		float h_km = size_lat * KM_PER_LAT;

		if (w_km * ratio < h_km)
		{
			w_km = h_km / ratio;

			float w_lon = w_km / KM_PER_LON;

			if (origin_lon + size_lon/2 - w_lon/2 < origin.x)
				origin_lon = origin.x;
			else if (origin_lon + size_lon/2 + w_lon/2 > origin.x + size.x)
				origin_lon = origin.x + size.x - w_lon;
			else
				origin_lon = origin_lon + size_lon/2 - w_lon/2;

			size_lon = w_lon;
		}
		else if (w_km * ratio > h_km)
		{
			h_km = w_km * ratio;

			float h_lat = h_km / KM_PER_LAT;

			if (origin_lat + size_lat/2 - h_lat/2 < origin.y)
				origin_lat = origin.y;
			else if (origin_lat + size_lat/2 + h_lat/2 > origin.y + size.y)
				origin_lat = origin.y + size.y - h_lat;
			else
				origin_lat = origin_lat + size_lat/2 - h_lat/2;

			size_lat = h_lat;
		}
	}

	v_origin.x	=	origin_lon;
	v_size.x	=	size_lon;

	v_origin.y	=	origin_lat;
	v_size.y	=	size_lat;
}

/*******************************************************************************

	map_t - Draw functions

*******************************************************************************/

void map_t :: DrawLonLat(const string & s, float x, float y, float size, const colors_t & colors)
{
	float w = size*3;
	float h = size;

	DrawQuad(NULL,x-w/2,y-h/2,w,h,colors_t(0,0,0,1));

	w -= .2f * h;
	h -= .2f * h;

	DrawQuad(NULL,x-w/2,y-h/2,w,h,colors_t(1.0f,1.0f,1.0f,1, 0.7f,0.7f,0.9f,1));

	SmallFont().Print(s, x,y, size,size, FONT_CENTER, colors);
}

void map_t :: DrawFrame( float sx, float sy, float sw, float sh, float sh_dep, bool isGrid, bool isDep )
{
	// frame colors
	color_t colorarray[] = {
		color_t(0.30f,0.30f,0.40f, 1),	// "black"
		color_t(0.60f,0.60f,0.80f, 1),	// "white"
		color_t(1.00f,1.00f,0.00f, 1),	// corner
		color_t(0.80f,0.80f,0.80f, 1)	// text
	};

	float lon0,lat0,lon1,lat1;
	float fy, fh;
	const float border = SCRY/200;
	const float FONTSIZE = SCRY/80;

	float dlon = 0.25f, dlat;
	UVToLonLat(0,0, &lon0, &lat0);
	UVToLonLat(1,1, &lon1, &lat1);

	if (isDep)
	{
		fy = sy + sh;
		fh = sh_dep;
		dlat = -10.0f;
		lat0 = origin.z;
		lat1 = origin.z + size.z;
	}
	else
	{
		fy = sy;
		fh = sh;
		dlat = 0.2f;
	}

	glLineWidth( 2.0f );

	float dxOndlon = (sw / (lon1-lon0));
	float dx = dxOndlon * dlon;

	float lon = RoundToInt(lon0/dlon) * dlon - dlon;
	float x = sx - (lon0-lon) * dxOndlon;

	int color = RoundToInt(lon0/dlon) & 1;
	for (; lon < lon1+dlon; lon+=dlon,x+=dx,color=1-color)
	{
		if (isGrid)
		{
			glColor4f(0.0f,0.0f,0.0f,0.1f);
			glBegin( GL_LINES );
				glVertex2f(x,fy);
				glVertex2f(x,fy+fh);
			glEnd();
		}
		else
		{
			DrawQuad(NULL, x,fy,			dx,border, colorarray[color]);
			DrawQuad(NULL, x,fy+fh-border,	dx,border, colorarray[color]);

			DrawLonLat(OneDecimal(lon), x,fy+fh-FONTSIZE/2, FONTSIZE, colorarray[3]);
		}
	}

	float dyOndlat = (fh / abs(lat0-lat1));
	float dy = dyOndlat * abs(dlat);

	float lat = RoundToInt(lat1/dlat) * dlat - dlat;
	float y = fy+fh + abs(lat1-lat) * dyOndlat;

	color = RoundToInt(abs(lat1/dlat)) & 1;
	for (; lat*dlat < (lat0+dlat)*dlat; lat+=dlat,y-=dy,color=1-color)
	{
		if (isGrid)
		{
			glColor4f(0.0f,0.0f,0.0f,0.1f);
			glBegin( GL_LINES );
				glVertex2f(sx,y);
				glVertex2f(sx+sw,y);
			glEnd();
		}
		else
		{
			DrawQuad(NULL, sx,y-dy,				border,dy, colorarray[color]);
			DrawQuad(NULL, sx+sw-border,y-dy,	border,dy, colorarray[color]);

			DrawLonLat(OneDecimal(lat), sx + FONTSIZE*1.5f,y, FONTSIZE, colorarray[3]);
		}
	}

	glLineWidth( 1.0f );

	DrawQuad(NULL, sx,				fy,				border,border, colorarray[2]);
	DrawQuad(NULL, sx+sw-border,		fy,				border,border, colorarray[2]);
	DrawQuad(NULL, sx+sw-border,		fy+fh-border,	border,border, colorarray[2]);
	DrawQuad(NULL, sx,				fy+fh-border,	border,border, colorarray[2]);
}

void map_t :: Draw(
	const set<station_t> & network,
	const vector<quake_t> & quakes,
	const binder_picks_set_t & picks,
	const win_t & win,
	float map_x, float map_y, float map_w
	)
{
	secs_t secs_now = SecsNow();

	set<station_t> :: const_iterator s;
	vector<quake_t> :: const_iterator q;
	binder_picks_set_t :: const_iterator p;

	// Find the lon,lat,dep ranges to show:

	float min_lon = +numeric_limits<float>::max();
	float min_lat = +numeric_limits<float>::max();
	float max_lon = -numeric_limits<float>::max();
	float max_lat = -numeric_limits<float>::max();

	// Enlarge to include every station

	for (s = network.begin(); s != network.end(); s++)
	{
		if (s->lon < min_lon)	min_lon = s->lon;
		if (s->lon > max_lon)	max_lon = s->lon;

		if (s->lat < min_lat)	min_lat = s->lat;
		if (s->lat > max_lat)	max_lat = s->lat;
	}

	// Enlarge to include every quake (only the last one in real-time mode)

	vector<quake_t> :: const_iterator quakes_begin;
	if (realtime && !quakes.empty())
		quakes_begin = quakes.end()-1;
	else
		quakes_begin = quakes.begin();

	for (q = quakes_begin; q != quakes.end(); q++)
	{
		if (q->origin.lon < min_lon)		min_lon = q->origin.lon;
		if (q->origin.lon > max_lon)		max_lon = q->origin.lon;

		if (q->origin.lat < min_lat)		min_lat = q->origin.lat;
		if (q->origin.lat > max_lat)		max_lat = q->origin.lat;
	}

	if ( param_display_real_quake && real_quake_lon != sac_header_t::UNDEF )
	{
		if (real_quake_lon < min_lon)		min_lon = real_quake_lon;
		if (real_quake_lon > max_lon)		max_lon = real_quake_lon;

		if (real_quake_lat < min_lat)		min_lat = real_quake_lat;
		if (real_quake_lat > max_lat)		max_lat = real_quake_lat;
	}

	// Enlarge to include every (visible) target

	for (targets_t::iterator t = targets.begin(); t != targets.end(); t++)
	{
		if (!t->shown)
			continue;

		if (t->lon < min_lon)	min_lon = t->lon;
		if (t->lon > max_lon)	max_lon = t->lon;

		if (t->lat < min_lat)	min_lat = t->lat;
		if (t->lat > max_lat)	max_lat = t->lat;
	}

/*
	// debug
	float clon = 15.4155f, clat = 40.7804f;
	secs_t now = SecsNow();
	float tt = float(now - long int(now / 10) * 10);
	float cwlon = 0.5 + 4/2 * sin(2 * PI * tt / 10) + 4/2;
	float cwlat = cwlon;

	min_lon = clon - cwlon/2;
	max_lon = clon + cwlon/2;
	min_lat = clat - cwlat/2;
	max_lat = clat + cwlat/2;
*/
	// Make sure the extents are not zero (e.g. only one station, no targets)
	const float SIZE_LON_MIN = 10.0f / KM_PER_LON;
	const float SIZE_LAT_MIN = 10.0f / KM_PER_LAT;

	float size_lon = max_lon - min_lon;
	float size_lat = max_lat - min_lat;

	if (size_lon < SIZE_LON_MIN)
		size_lon = SIZE_LON_MIN;
	if (size_lat < SIZE_LAT_MIN)
		size_lat = SIZE_LAT_MIN;

	// Enlarge a bit more
	const float larger = 0.2f;

	min_lon -= size_lon / 2 * larger;
	max_lon += size_lon / 2 * larger;
	min_lat -= size_lat / 2 * larger;
	max_lat += size_lat / 2 * larger;

	SetVisible(min_lon,min_lat, max_lon-min_lon,max_lat-min_lat);

	float map_h		=	map_w * ((v_size.y * KM_PER_LAT) / (v_size.x * KM_PER_LON));
	float map_hdep	=	map_w * (size.z / (v_size.x * KM_PER_LON));

	const float STATION_SIZE_LAT = size_lat / network.size();
	const float MIN_H_SYMBOL = map_w / 30;
	const float MAX_H_SYMBOL = map_w / 12;

	glPushAttrib( GL_SCISSOR_BIT | GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT );

	glScissor(	win.GetX() + RoundToInt(map_x * win.GetW()), win.GetY() + RoundToInt((SCRY-(map_y+map_h)) * win.GetW()),
				RoundToInt(win.GetW() * map_w), RoundToInt(win.GetW() * map_h));

	glEnable(GL_SCISSOR_TEST);

	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();

	// Enter 2D mode
	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();
	glOrtho(
		0,1,	// left-right
		SCRY,0,	// bottom-top
		-1,1		// near far
	);

	glDisable( GL_CULL_FACE );
	glDisable( GL_LIGHTING );
	glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );
	glDisable ( GL_TEXTURE_2D );
	glEnable ( GL_BLEND );
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_LINE_SMOOTH);


	// map

	float x = (v_origin.x - origin.x) / size.x;
	float y = 1 - ((v_origin.y + v_size.y - origin.y) / size.y);
	float w = v_size.x / size.x;
	float h = v_size.y / size.y;

	float map_alpha = 1.0f;

	DrawQuad(Tex_Map(), map_x,map_y, map_w,map_h, colors_t(1,1,1,map_alpha), 0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
		x,y, x+w,y, x+w,y+h, x,y+h
	);

	// grid

	DrawFrame(map_x,map_y, map_w,map_h,map_hdep, true, false);

	// P-waves & S-waves

	LonLatToUV(0,0, &x,&y);
	LonLatToUV(0,STATION_SIZE_LAT, &w,&h);
	float h_station = abs(h-y);
	Clamp(h_station, MIN_H_SYMBOL, MAX_H_SYMBOL);
	h_station *= float(param_display_map_station_scale);

	if (!quakes.empty())
	{
		q = quakes.end()-1;

		float q_x, q_y, q_w, q_h;

		LonLatToUV(q->origin.lon,q->origin.lat, &q_x,&q_y);
		q_x = map_x + q_x * map_w;
		q_y = map_y + q_y * map_h;

		q_h = h_station;
		q_w = q_h * Tex_Quake()->GetW() / Tex_Quake()->GetH();

		float secs_from_otime = float(secs_now - q->origin.time);

		float qr, quake_w;

		// P-Waves
		qr = rtloc.QuakeRadiusAfterSecs('P', q->origin, secs_from_otime);
		quake_w = qr * (map_w / (v_size.x * KM_PER_LON));
		if (quake_w < 1)
		{
			DrawQuad(Tex_Wave(), q_x - quake_w, q_y - quake_w, quake_w*2,quake_w*2, colors_t(1,1,0,0.4f));
			SmallFont().Print(ToString(RoundToInt(qr)),q_x+quake_w,q_y,q_w/2*.8f,q_w/2*.8f,FONT_CENTER);
			SmallFont().Print(ToString(RoundToInt(qr)),q_x-quake_w,q_y,q_w/2*.8f,q_w/2*.8f,FONT_CENTER);
			SmallFont().Print(ToString(RoundToInt(qr)),q_x,q_y+quake_w,q_w/2*.8f,q_w/2*.8f,FONT_CENTER);
			SmallFont().Print(ToString(RoundToInt(qr)),q_x,q_y-quake_w,q_w/2*.8f,q_w/2*.8f,FONT_CENTER);
		}

		// S-Waves
		qr = rtloc.QuakeRadiusAfterSecs('S', q->origin, secs_from_otime);
		quake_w = qr * (map_w / (v_size.x * KM_PER_LON));
		if (quake_w < 1)
		{
			DrawQuad(Tex_Wave(), q_x - quake_w, q_y - quake_w, quake_w*2,quake_w*2, colors_t(1,0,0,0.4f));
			SmallFont().Print(ToString(RoundToInt(qr)),q_x+quake_w,q_y,q_w/2*.8f,q_w/2*.8f,FONT_CENTER);
			SmallFont().Print(ToString(RoundToInt(qr)),q_x-quake_w,q_y,q_w/2*.8f,q_w/2*.8f,FONT_CENTER);
			SmallFont().Print(ToString(RoundToInt(qr)),q_x,q_y+quake_w,q_w/2*.8f,q_w/2*.8f,FONT_CENTER);
			SmallFont().Print(ToString(RoundToInt(qr)),q_x,q_y-quake_w,q_w/2*.8f,q_w/2*.8f,FONT_CENTER);
		}
	}

	// Stations

	colors_t col_stat		=	colors_t(colors_station.r,			colors_station.g,		colors_station.b,		map_alpha);
	colors_t col_stat_ghost	=	colors_t(colors_station_ghost.r,	colors_station_ghost.g,	colors_station_ghost.b,	map_alpha * .5f);
	colors_t col_stat_pick	=	colors_t(colors_station_pick.r,		colors_station_pick.g,	colors_station_pick.b,	map_alpha);
	colors_t col_stat_loc	=	colors_t(colors_station_loc.r,		colors_station_loc.g,	colors_station_loc.b,	map_alpha);

	for (s = network.begin(); s != network.end(); s++)
	{
		colors_t col = col_stat;
		float scale = 1.0f;

		/*
			Station color:
			- ghosted if not used or, in case of earthquake, if lagging behind the first pick;
			- otherwise: according to pick order, from orange (first) to yellow (last)
		*/
		if (!s->z)
		{
			col = col_stat_ghost;
		}
		else if (!quakes.empty() && (secs_now - (quakes.end()-1)->secs_creation < 2*60))
		{
			q = quakes.end()-1;

			if (s->z->EndTime() < q->picks.begin()->pick.t)
			{
				col = col_stat_ghost;
			}
			else
			{
				int pick_i = 0;
				for (p = q->picks.begin(); p != q->picks.end(); p++, pick_i++)
				{
					if (p->heli->station->name != s->name)
						continue;

					float amount = float(pick_i) / NonZero( q->picks.size() - 1 );
					col.r = Interp(col_stat_loc.r, col_stat_pick.r, amount);
					col.g = Interp(col_stat_loc.g, col_stat_pick.g, amount);
					col.b = Interp(col_stat_loc.b, col_stat_pick.b, amount);
					col.a = Interp(col_stat_loc.a, col_stat_pick.a, amount);
					col.r2 = col.r;
					col.g2 = col.g;
					col.b2 = col.b;
					col.a2 = col.a;

					scale = 1.1f;

					break;
				}
			}
		}
		else if (s->z->IsLaggingOrFuture())
		{
			col = col_stat_ghost;
		}

		float s_x, s_y, s_w, s_h;

		LonLatToUV(s->lon,s->lat, &s_x,&s_y);
		s_x = map_x + s_x * map_w;
		s_y = map_y + s_y * map_h;

		s_h = h_station;
		s_w = s_h * Tex_Station()->GetW() / Tex_Station()->GetH();

		DrawQuad(Tex_Station(), s_x - s_w/2*scale,s_y - s_h/2*scale, s_w*scale,s_h*scale, col);
		SmallFont().Print(s->name,s_x,s_y,s_h/3*scale,s_h/3*scale,FONT_CENTER, colors_t(1,1,1,col.a));
	}

	// real quake (epicenter)

	if ( param_display_real_quake && real_quake_lon != sac_header_t::UNDEF )
	{
		float q_x, q_y, q_w, q_h;

		LonLatToUV(real_quake_lon,real_quake_lat, &q_x,&q_y);
		q_x = map_x + q_x * map_w;
		q_y = map_y + q_y * map_h;

		q_h = h_station * 0.7f;
		q_w = q_h * Tex_Quake()->GetW() / Tex_Quake()->GetH();

		DrawQuad(Tex_Quake(), q_x - q_w/2, q_y - q_h/2, q_w,q_h, colors_event_real);
		SmallFont().Print(MagToString(real_quake_mag), q_x,q_y,q_w/2*1.3f,q_w/2*1.3f,FONT_CENTER, colors_t(1,1,1,colors_event_real.a));
	}

	// PRESTo quakes (epicenters)

	for (q = quakes_begin; q != quakes.end(); q++)
	{
		float q_x, q_y, q_w, q_h;

		LonLatToUV(q->origin.lon,q->origin.lat, &q_x,&q_y);
		q_x = map_x + q_x * map_w;
		q_y = map_y + q_y * map_h;

		q_h = h_station;
		q_w = q_h * Tex_Quake()->GetW() / Tex_Quake()->GetH();

		float alpha = (q == (quakes.end()-1)) ? 1.0f : 0.5f;

		DrawQuad(Tex_Quake(), q_x - q_w/2, q_y - q_h/2, q_w,q_h, colors_t(colors_event.r,colors_event.g,colors_event.b,alpha));

		// Draw a error ellipsoid for the most recent quake
		if ( q == (quakes.end()-1) && !param_locate_ignore_error )
		{
			float quake_w = sqrt(q->origin.cov_xx) * 2 * (map_w / (v_size.x * KM_PER_LON));
			float quake_h = sqrt(q->origin.cov_yy) * 2 * (map_w / (v_size.x * KM_PER_LON));

			DrawQuad(Tex_Circle(), q_x - quake_w/2, q_y - quake_h/2, quake_w,quake_h, colors_t(1,1,1,0.3f));
		}

		if (q->mag != -1)
			SmallFont().Print(MagToString(q->mag),q_x,q_y,q_w/2*1.3f,q_w/2*1.3f,FONT_CENTER, colors_t(1,1,1,alpha));
	}

	// Targets

	colors_t tcol;
	bool tfirst = true;
	for (targets_t::iterator t = targets.begin(); t != targets.end(); t++)
	{
		if (!t->shown)
			continue;

		float t_x, t_y, t_w, t_h;

		LonLatToUV(t->lon,t->lat, &t_x,&t_y);
		t_x = map_x + t_x * map_w;
		t_y = map_y + t_y * map_h;

		t_h = h_station;
		t_w = t_h * Tex_Target()->GetW() / Tex_Target()->GetH();

		string tlabel = "";

		// Compute target remaining seconds, PGA and color
		tcol = colors_t(1,1,1,1);
		if (!quakes.empty())
		{
			q = quakes.end()-1;
			float secs_from_otime = float(secs_now - q->origin.time);
			float secs_remaining = t->CalcTravelTime('S', q->origin) - secs_from_otime;

			// Add remaining seconds for up to a few second after the S-waves arrival
			if (secs_remaining > -20)
			{
				tlabel = ToString(RoundToInt(secs_remaining)) + "s";
				if (q->mag != -1)
				{
					float R_epi = rtloc.LonLatDep_Distance_km(q->origin.lon,q->origin.lat,q->origin.dep, t->lon,t->lat,t->dep);
					range_t peak;
					pga.CalcPeak( q->mag, R_epi, q->origin.dep, &peak );
					tlabel += " " + ToString(RoundToInt((peak.val / 100 / 9.81f * 100) * 10)/10.0f) + "%g";
				}
			}

			// First (shown) target is coloured according to S-waves distance:
			// white (no quake) -> yellow (before arrival) -> red (after arrival)
			if (tfirst)
			{
				tfirst = false;

				if (secs_remaining > 0)
					tcol = colors_t(1,1,.5f,1);
				else
					tcol = colors_t(1,.5f,.5f,1);
			}
		}

		// Draw icon
		DrawQuad(Tex_Target(), t_x - t_w/2, t_y - t_h/2, t_w,t_h, colors_t(0.7f,0.7f,1.0f,1.0f));

		// Calc the best horizontal alignment of text to fit in the map
		const float FONTSIZE = t_w*.6f;
		int len = int(max(t->fullname.size(), tlabel.size()));

		int xflags;
		if (t_x + FONTSIZE * len / 2 > map_x + map_w)
			xflags = FONT_X_IS_MAX;
		else if ((t_x - FONTSIZE * len / 2 < map_x) && (t_x + FONTSIZE * len <= map_x + map_w))
			xflags = 0;
		else
			xflags = FONT_X_IS_CENTER;

		// Print text
		if (tlabel.size())
			t_y -= FONTSIZE/2;

		SmallFont().Print(t->fullname, t_x,t_y, FONTSIZE,FONTSIZE, xflags | FONT_Y_IS_CENTER, tcol);

		if (tlabel.size())
			SmallFont().Print(tlabel, t_x,t_y + FONTSIZE, FONTSIZE,FONTSIZE, xflags | FONT_Y_IS_CENTER, tcol);
	}

	DrawFrame(map_x,map_y, map_w,map_h,map_hdep, false, false);



	// Depth

	glScissor(	win.GetX() + RoundToInt(map_x * win.GetW()), win.GetY() + RoundToInt((SCRY-(map_y+map_h+map_hdep)) * win.GetW()),
				RoundToInt(win.GetW() * map_w), RoundToInt(win.GetW() * map_hdep));

	DrawQuad(NULL, map_x,map_y+map_h, map_w,map_hdep, colors_t(0.9f,0.8f,0.7f,map_alpha), 0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// real quake (depth)

	if ( param_display_real_quake && real_quake_lon != sac_header_t::UNDEF )
	{
		float q_x, q_y, q_w, q_h;

		LonLatToUV(real_quake_lon,real_quake_lat, &q_x,&q_y);
		DepToV(real_quake_dep, &q_y);
		q_x = map_x + q_x * map_w;
		q_y = map_y + map_h + q_y * map_hdep;

		q_h = h_station / 2;
		q_w = q_h * Tex_Quake()->GetW() / Tex_Quake()->GetH();

		DrawQuad(Tex_Quake(), q_x - q_w/2, q_y - q_h/2, q_w,q_h, colors_event_real);
		SmallFont().Print(MagToString(real_quake_mag),q_x,q_y,q_w/2*1.3f,q_w/2*1.3f,FONT_CENTER, colors_t(1,1,1,colors_event_real.a));
	}

	// PRESTo quakes (depth)

	for (q = quakes_begin; q != quakes.end(); q++)
	{
		float q_x, q_y, q_w, q_h;

		LonLatToUV(q->origin.lon,q->origin.lat, &q_x,&q_y);
		DepToV(q->origin.dep,&q_y);
		q_x = map_x + q_x * map_w;
		q_y = map_y + map_h + q_y * map_hdep;

		q_h = h_station;
		q_w = q_h * Tex_Quake()->GetW() / Tex_Quake()->GetH();

		float alpha = (q == (quakes.end()-1)) ? 1.0f : 0.5f;

		DrawQuad(Tex_Quake(), q_x - q_w/2, q_y - q_h/2, q_w,q_h, colors_t(colors_event.r,colors_event.g,colors_event.b,alpha));

		// Draw a error ellipsoid for the most recent quake
		if ( q == (quakes.end()-1) && !param_locate_ignore_error )
		{
			float quake_w = sqrt(q->origin.cov_xx) * 2 * (map_w / (v_size.x * KM_PER_LON));
			float quake_h = sqrt(q->origin.cov_zz) * 2 * (map_w / (v_size.x * KM_PER_LON));

			DrawQuad(Tex_Circle(), q_x - quake_w/2, q_y - quake_h/2, quake_w,quake_h, colors_t(1,1,1,0.3f));
		}

		if (q->mag != -1)
			SmallFont().Print(MagToString(q->mag),q_x,q_y,q_w/2*1.3f,q_w/2*1.3f,FONT_CENTER, colors_t(1,1,1,alpha));
	}

	DrawFrame(map_x,map_y, map_w,map_h,map_hdep, true,  true);
	DrawFrame(map_x,map_y, map_w,map_h,map_hdep, false, true);



	// Restore the rendering state
	glMatrixMode( GL_PROJECTION );
	glPopMatrix();

	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();

	glPopAttrib();
}


// Preload media files
void PreLoad_Map()
{
	LoadingBar_Start();
		const float K = 100.0f/10;
		LoadingBar_SetNextPercent( 1*K);	Tex_Station();
		LoadingBar_SetNextPercent( 2*K);	Tex_Quake();
		LoadingBar_SetNextPercent( 3*K);	Tex_Circle();
		LoadingBar_SetNextPercent( 4*K);	Tex_Wave();
		LoadingBar_SetNextPercent( 5*K);	Tex_Target();
		LoadingBar_SetNextPercent( 6*K);	Tex_Map();
	LoadingBar_End();
}

map_t themap;
