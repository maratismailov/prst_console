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

	- Create data streams (loading)
	- Run the Binder (event declaration and processing)
	- Draw the screen (helicorders, magnitude graph, map, icons)
	- Handle the GUI

*******************************************************************************/

#include <sstream>
#include <iomanip>
#include <fstream>
#include <set>
#include <algorithm>
#include <functional>
#ifdef WIN32
    #include "dirent_win32.h"
#else
    #include <dirent.h>
#endif

#include "gui.h"

#include "broker.h"
#include "state.h"
#include "geometry.h"
#include "graphics2d.h"
#include "config.h"
#include "map.h"
#include "heli.h"
#include "binder.h"
#include "loading_bar.h"
#include "rtloc.h"
#include "rtmag.h"
#include "pgx.h"
#include "target.h"
#include "sound.h"
#include "version.h"

using namespace std;

const unsigned	NUM_SAMPLES			=	2*60*200;

/*******************************************************************************

	Local variables / functions

*******************************************************************************/

namespace
{
// Tweakable parameters:

const int	HELIS_NX = 1;

float		HELIS_W  = 0.55f;
const float	HELIS_H  = (SCRY - icon_t::FONTSIZE*3) * .99f;

float		MAP_W	= 1 - HELIS_W;

const float	HELIS_X0 = (1.0f - (HELIS_W + MAP_W)) / 2;
const float	HELIS_Y0 = icon_t::FONTSIZE * 2; //(SCRY - HELIS_H) / 2;

float		MAP_X	=	HELIS_W;
const float	MAP_Y	=	HELIS_Y0;

const secs_t HELI_ZOOM_TIME	=	0.5f;

stringstream ss;
float user_scale_y = 1.0f;

secs_t mytime;

// selected helicorder:
int heli_i;				// index
secs_t heli_i_t0;		// starting time of the last animation
float heli_i_amount;	// animation amount (0..1)
bool heli_i_zoom_in;	// it's zooming in

// Textures:

texptr_t Tex_Frame()
{
	static texptr_t tex("alarmframe.png");
	return tex;
}
texptr_t Tex_Alarm()
{
	static texptr_t tex("alarm.png");
	return tex;
}
texptr_t Tex_Heartbeat()
{
	static texptr_t tex("heartbeat.png");
	return tex;
}
texptr_t Tex_Display()
{
	static texptr_t tex("display.png");
	return tex;
}

// Icons

vector<icon_t> icons;
int icon_accel_i = -1, icon_pause_i = -1, icon_restart_i = -1;

// Misc stuff

bool Use_Accel()
{
	return icons[icon_accel_i].IsActive();
}

// rtloc    variable: contains the actual P and S grids (stations and targets).
// network  variable: all stations in the network, as defined in stations.txt, i.e. a subset of the grids read by rtloc (usually all of the stations, no targets).
//                    These are the stations shown on the map.
// stations variable: all data streams used, according to seedlink.txt (real-time mode) or the available SAC files (simulation mode), i.e. a subset of network.
//                    Note that stations is a list of pointers to the actual station_t's stored in network.
set<station_t> network;

// Helicorders to display on-screen (a subset of the stations variable):
vector<station_t *> helis;

/*******************************************************************************

	Init / End functions

*******************************************************************************/

float icons_end_x;

bool helicorders_loaded = false;

struct cmp_stations_t : public binary_function<station_t*, station_t*, bool>
{
	bool operator() (const station_t *lhs, const station_t *rhs)
	{
		// lhs < rhs
		return lhs->lat > rhs->lat;	// north to south
	}
};

void PreLoad_Helicorders()
{
	if (helicorders_loaded)
		return;

	const float K = 100.0f/10;
	LoadingBar_Start();

	// Textures

	LoadingBar_SetNextPercent( 1*K );
	Tex_Frame();
	Tex_Alarm();
	Tex_Heartbeat();
	Tex_Display();

	// Magnitude Table

	LoadingBar_SetNextPercent( 2*K );
	rtmag.Init( 0.0f, param_magnitude_max_value, 0.01f, net_dir + "rtmag.txt" );

	// PGA and PGV Tables

	LoadingBar_SetNextPercent( 3*K );
	pga.Init( net_dir + "pga.txt" );
	pgv.Init( net_dir + "pgv.txt" );

	// Read RTLoc stations and travel time grids

	LoadingBar_SetNextPercent( 6*K );
	rtloc.Init(net_dir + "rtloc.txt");

	{
		float min_lon, min_lat, min_dep, max_lon, max_lat, max_dep, dx, dy, dz;
		rtloc.GetGridArea(&min_lon, &min_lat, &min_dep, &max_lon, &max_lat, &max_dep, &dx, &dy, &dz);

		cout << endl;
		cout << "==================================================================================================" << endl;
		cout << "    RTLoc Grids (" << net_dir << "time/)" << endl;
		cout << "==================================================================================================" << endl;
		cout << " Lon    (deg): " << min_lon << " .. " << max_lon << endl;
		cout << " Lat    (deg): " << min_lat << " .. " << max_lat << endl;
		cout << " Dep     (km): " << min_dep << " .. " << max_dep << endl;
		cout << " Spacing (km): " << dx << ", " << dy << ", " << dz << endl;
		cout << " Dep, Vp, Vs (km, km/s): *Note: the velocity model below is a rough 1-d approximation extracted from travel-time grids*" << endl;

		rtloc.LogVelocityModel();

		cout << "==================================================================================================" << endl;

		// Map area is a bit larger than the location grid

		float larger = 0.2f;

		float size_lon = max_lon - min_lon;
		float size_lat = max_lat - min_lat;
		float size_dep = max_dep - min_dep;

		min_lon -= size_lon / 2 * larger;
		max_lon += size_lon / 2 * larger;
		min_lat -= size_lat / 2 * larger;
		max_lat += size_lat / 2 * larger;

		max_dep += size_dep / 2 * larger;

		themap.Init(	min_lon,         min_lat,         min_dep,
						max_lon-min_lon, max_lat-min_lat, max_dep-min_dep	);

		// Map size in km

		float w_km = rtloc.LonLatDep_Distance_km(min_lon,min_lat,0, max_lon,min_lat,0);
		float h_km = rtloc.LonLatDep_Distance_km(min_lon,min_lat,0, min_lon,max_lat,0);

		// Map size in px

		float w = w_km * 10;
		float h = w * h_km / w_km;

		// Clamp to the max Google Static Map size (640x640 px)

		const float maxsize = 640;

		if (w > maxsize || h > maxsize)
		{
			if (w >= h)
			{
				w = maxsize;
				h = w * h_km / w_km;
			}
			else
			{
				h = maxsize;
				w = h * w_km / h_km;
			}
		}

		// Google Maps URLs

		cout << endl;
		cout << "==================================================================================================" << endl;
		cout << "    Map Area" << endl;
		cout << endl;

		for (int i = 0; i <= 4; i++)
		{
			float lon0, lat0, lon1, lat1;

			if (i == 0)
			{

				cout << "    Google Maps (Mid Res): " << endl;
				cout << endl;

				lon0 = min_lon;
				lat0 = min_lat;
				lon1 = max_lon;
				lat1 = max_lat;
			}
			else
			{
				if (i == 1)
				{
					cout << "    Google Maps (4 x Mid Res): " << endl;
					cout << endl;
				}

				lon0 = ((i-1) & 1) ? min_lon + (max_lon-min_lon)/2 : min_lon;
				lat0 = ((i-1) < 2) ? min_lat + (max_lat-min_lat)/2 : min_lat;
				lon1 = lon0 + (max_lon-min_lon)/2;
				lat1 = lat0 + (max_lat-min_lat)/2;
			}

			cout <<	"        http://maps.google.com/maps/api/staticmap?sensor=false"
					"&center=" << (lat0 + lat1) / 2 << "," << (lon0 + lon1) / 2 <<
					"&scale=2" <<
					"&maptype=roadmap" <<	// roadmap, satellite, hybrid, terrain
					"&size=" << RoundToInt(w) << "x" << RoundToInt(h) <<
					"&path=weight:1|color:0x00000040" <<
							"|" << lat0 << "," << lon0 <<
							"|" << lat1 << "," << lon0 <<
							"|" << lat1 << "," << lon1 <<
							"|" << lat0 << "," << lon1 <<
							"|" << lat0 << "," << lon0 <<
					"&visible=" << lat0 << "," << lon0 << "|" << lat1 << "," << lon1 << endl;

			if (i == 0)
				cout << endl;
		}

		cout << "==================================================================================================" << endl;
		cout << "Lon (deg): " << min_lon << " .. " << max_lon << endl;
		cout << "Lat (deg): " << min_lat << " .. " << max_lat << endl;
		cout << "==================================================================================================" << endl;
	}

	// Real earthquake location and magnitude

	if (!realtime)
		themap.LoadRealQuake(sacs_dir + event_name + "_real.txt");

	// Targets / Broker

	LoadingBar_SetNextPercent( 7*K );
	targets.Load( net_dir + "targets.txt" );
	broker.Load( net_dir + "broker.txt" );

	// Read Stations

	LoadingBar_SetNextPercent( 8*K );
	{
		string filename = net_dir + "stations.txt";
		ifstream f(filename.c_str());
		if (!f)
			Fatal_Error("Couldn't open station file \"" + filename + "\"");

		const std::streamsize w1 = 5, w2 = 7, w3 = 7, w4 = 6, w5 = 4, w6 = 12, w7 = 12, w8 = 12, w9 = 15, w10 = 3, w11 = 3, w12 = 3, w13 = 3;

		cout << endl;
		cout << "==================================================================================================" << endl;
		cout << "    Stations (" << filename << ")" << endl;
		cout << endl;
		cout << "    Accel. in m/s^2 (or Vel. in m/s) = counts * logger / sensor" << endl;
		cout << endl;
		cout <<	setw(w1)	<<	"Name"			<<	" | "	<<
				setw(w2)	<<	"Lon"			<<	" | "	<<
				setw(w3)	<<	"Lat"			<<	" | "	<<
				setw(w4)	<<	"Elev"			<<	" | "	<<
				setw(w5)	<<	"Type"			<<	" | "	<<
				setw(w6)	<<	"Clip"			<<	" | "	<<
				setw(w7)	<<	"Logger"		<<	" | "	<<
				setw(w8)	<<	"Sensor"		<<	" | "	<<
				setw(w9)	<<	"IP address"	<<	" | "	<<
				setw(w10)	<<	"Net"			<<	" | "	<<
				setw(w11)	<<	"ChZ"			<<	" | "	<<
				setw(w12)	<<	"ChN"			<<	" | "	<<
				setw(w13)	<<	"ChE"			<<	endl;
		cout << "==================================================================================================" << endl;

		for(;;)
		{
			string name, type, str_clip, str_logger, str_sensor;
			float lon, lat, dep, clip, logger, sensor;
			string ipaddress, net, channel_z, channel_n, channel_e;

			SkipComments(f);

			f >> name >> type >> str_clip >> str_logger >> str_sensor >> ipaddress >> net >> channel_z >> channel_n >> channel_e;

			const string FORMAT = "Parsing station \"" + name + "\" in file \"" + filename + "\".\nUse this format: name type clip logger sensor IPaddress net channelZ channelN channelE\n";

			if (f.fail())
			{
				if (name.empty() && f.eof())
					break;

				Fatal_Error(FORMAT);
			}

			if (type != "ACC" && type != "VEL")
				Fatal_Error(FORMAT + "Invalid type \"" + type + "\". Must be ACC or VEL.");

			char *str_end;
			clip = (float)strtod(str_clip.c_str(), &str_end);
			if (!clip && *str_end)
				Fatal_Error(FORMAT + "Invalid clipping value \"" + str_clip + "\".");

			logger = (float)atof(str_logger.c_str());
			if (!logger)
				Fatal_Error(FORMAT + "Invalid logger value \"" + str_logger + "\".");

			sensor = (float)atof(str_sensor.c_str());
			if (!sensor)
				Fatal_Error(FORMAT + "Invalid sensor value \"" + str_sensor + "\".");

			// No duplicate station name
			if ( find_if(network.begin(), network.end(), bind2nd(StationName(), name)) != network.end() )
				Fatal_Error(FORMAT + "Duplicate station \"" + name + "\".");

			// Get station coordinates from RTLoc grids
			rtloc.GetStationLonLatDep(name, &lon, &lat, &dep);

			cout <<	setw(w1)	<<	name		<<	" | "	<<
					setw(w2)	<<	lon			<<	" | "	<<
					setw(w3)	<<	lat			<<	" | "	<<
					setw(w4)	<<	-dep*1000	<<	" | "	<<
					setw(w5)	<<	type		<<	" | "	<<
					setw(w6)	<<	clip		<<	" | "	<<
					setw(w7)	<<	logger		<<	" | "	<<
					setw(w8)	<<	sensor		<<	" | "	<<
					setw(w9)	<<	ipaddress	<<	" | "	<<
					setw(w10)	<<	net			<<	" | "	<<
					setw(w11)	<<	channel_z	<<	" | "	<<
					setw(w12)	<<	channel_n	<<	" | "	<<
					setw(w13)	<<	channel_e	<<	endl;

			network.insert( station_t(name, lon, lat, dep, type == "ACC", clip, logger / sensor, ipaddress, net, channel_z, channel_n, channel_e) );
		}

		cout << "==================================================================================================" << endl;

		if (!network.size())
			Fatal_Error( "No stations found in file \"" + filename + "\"");
	}

	// Data streams

	LoadingBar_SetNextPercent( 10*K );
	if (realtime)
	{
		// Seedlink streams, loaded in seedlink.txt order

		string sl_filename = net_dir + "seedlink.txt";
		ifstream f(sl_filename.c_str());
		if (!f)
			Fatal_Error("Couldn't open seedlink file \"" + sl_filename + "\"");

		cout << endl;
		cout << "==================================================================================================" << endl;
		cout << "    SeedLink Stations (" << sl_filename << ")" << endl;
		cout << "==================================================================================================" << endl;

		for(;;)
		{
			SkipComments(f);

			string station;

			f >> station;

			if (!f)
				break;

			cout << station << endl;

			set<station_t>::iterator s = find_if(network.begin(), network.end(), bind2nd(StationName(), station));
			if (s == network.end())
				Fatal_Error("Unknown station \"" + station + "\"");

			// FIXME: circumvent the fact we aren't allowed to change set elements
			station_t *sp = const_cast<station_t *>(&(*s));
			stations.push_back( sp );

			if (sp->channel_z != "-")	sp->z = new slink_t;
			if (sp->channel_n != "-")	sp->n = new slink_t;
			if (sp->channel_e != "-")	sp->e = new slink_t;

			if (sp->z)	sp->z->Init(sp->ipaddress+"/"+sp->net+"_"+station+":"+sp->channel_z, NUM_SAMPLES, sp);
			if (sp->n)	sp->n->Init(sp->ipaddress+"/"+sp->net+"_"+station+":"+sp->channel_n, NUM_SAMPLES, sp);
			if (sp->e)	sp->e->Init(sp->ipaddress+"/"+sp->net+"_"+station+":"+sp->channel_e, NUM_SAMPLES, sp);
		}

		cout << "==================================================================================================" << endl;

		if (stations.empty())
			Fatal_Error("Empty seedlink file \"" + sl_filename + "\"");
	}
	else
	{
		// SAC streams, loaded in

		secs_t sacs_seconds_t0 = numeric_limits<secs_t>::max();
		float sac_lon = sac_header_t::UNDEF, sac_lat = sac_header_t::UNDEF, sac_dep = sac_header_t::UNDEF;

		DIR *dir = opendir(sacs_dir.c_str());
		if (!dir)
			Fatal_Error("Can't open \"" + sacs_dir + "\" dir");

		cout << endl;
		cout << "==================================================================================================" << endl;
		cout << "    SAC Files (" << sacs_dir << ")" << endl;
		cout << "==================================================================================================" << endl;

		for(;;)
		{
			// Read all files ending with .SAC, undefined order

			struct dirent *ent = readdir(dir);
			if (!ent)
				break;

			string filename = string(ent->d_name);

			const string sac_ext = ".SAC";
			string::size_type pos = ToUpper(filename).find(sac_ext);
			if ( pos == string::npos || pos != filename.size() - sac_ext.length() )
				continue;

			sac_t *heli = new sac_t;
			heli->Init(sacs_dir + ent->d_name, NUM_SAMPLES, NULL);

			// SAC Station

			string station = heli->GetSACStation();

			set<station_t>::iterator s = find_if(network.begin(), network.end(), bind2nd(StationName(), station));
			if (s == network.end())
			{
				delete heli;
//				Fatal_Error("Unknown station \"" + station + "\"");
				continue;
			}

			// FIXME: circumvent the fact we aren't allowed to change set elements
			station_t *sp = const_cast<station_t *>(&(*s));
			heli->station = sp;

			// SAC Component

			char cmp = heli->GetSACComponent();

			char duplicate = 0;
			switch (cmp)
			{
				case 'Z':	if (sp->z == NULL)	sp->z = heli;	else	duplicate = cmp;	break;
				case 'N':	if (sp->n == NULL)	sp->n = heli;	else	duplicate = cmp;	break;
				case 'E':	if (sp->e == NULL)	sp->e = heli;	else	duplicate = cmp;	break;
				default:
				{
					delete heli;
//					Fatal_Error("Unknown component in SAC file \"" + string(ent->d_name) + "\" (station: \"" + station + "\")");
					continue;
				}
			}

			if (duplicate)
			{
				delete heli;
				Fatal_Error("Duplicate component \"" + string(1,duplicate) + "\" in SAC file \"" + string(ent->d_name) + "\" (station: \"" + station + "\")");
			}

			// SAC Origin

			if ( param_locate_force_sac && (float(param_locate_force_lon) == sac_header_t::UNDEF || float(param_locate_force_lat) == sac_header_t::UNDEF || float(param_locate_force_dep) == sac_header_t::UNDEF ) )
			{
				float lon, lat, dep, mag;
				if ( heli->GetSACEvent(&lon, &lat, &dep, &mag) )
				{
					if (	((sac_lon != sac_header_t::UNDEF) || (sac_lat != sac_header_t::UNDEF) || (sac_dep != sac_header_t::UNDEF)) &&
							((sac_lon != lon) || (sac_lat != lat) || (sac_dep != dep))	)
					{
						delete heli;
						Fatal_Error(	"Different event location in SAC file \"" + string(ent->d_name) + "\": " +
										ToString(lon)     + "," + ToString(lat)     + "," + ToString(dep)     + " (was " +
										ToString(sac_lon) + "," + ToString(sac_lat) + "," + ToString(sac_dep) + ")"
						);
					}
					sac_lon = lon;
					sac_lat = lat;
					sac_dep = dep;
				}
			}

			// SAC Begin Time

			if (heli->Secs_T0() < sacs_seconds_t0)
				sacs_seconds_t0 = heli->Secs_T0();

			if ( find(stations.begin(), stations.end(), sp) == stations.end() )
				stations.push_back( sp );

			cout << filename << endl;
		}
		closedir(dir);

		cout << "==================================================================================================" << endl;

		// Remove stations with no vertical component

		vector<station_t *>::iterator s = stations.begin();
		while (s != stations.end())
		{
			if ((*s)->z == NULL)
				s = stations.erase(s);
			else
				++s;
		}

		// Check that the stations set is not empty

		if (stations.empty())
			Fatal_Error("No known station has Z-component SAC files in \"" + sacs_dir + "\" dir");

		// Shuffle stations from SAC files so that they're not in alphabetical SAC filename order. This way param_heli_max_num gets a random selection

		random_shuffle( stations.begin(), stations.end() );

		// Update simulated time start instant

		simutime_t :: SetT0(sacs_seconds_t0);

		// Check that any forced location lies within the grid

		if (param_locate_force_sac)
		{
			if (float(param_locate_force_lon) == sac_header_t::UNDEF)
				param_locate_force_lon = double(sac_lon);

			if (float(param_locate_force_lat) == sac_header_t::UNDEF)
				param_locate_force_lat = double(sac_lat);

			if (float(param_locate_force_dep) == sac_header_t::UNDEF)
				param_locate_force_dep = double(sac_dep);
		}

		if ( float(param_locate_force_lon) != sac_header_t::UNDEF || float(param_locate_force_lat) != sac_header_t::UNDEF || float(param_locate_force_dep) != sac_header_t::UNDEF )
		{
			cout << endl;
			cout << "==================================================================================================" << endl;
			cout << "    Forced Location" << endl;
			cout << "==================================================================================================" << endl;

			if (float(param_locate_force_lon) != sac_header_t::UNDEF)
			{
				cout << "Lon: " << param_locate_force_lon << endl;
				if ( !rtloc.IsPointInGrid(float(param_locate_force_lon), stations[0]->lat, stations[0]->dep) )
					Fatal_Error("RTLoc: Forced longitude (" + ToString(param_locate_force_lon) + ") lies outside the grid");
			}

			if (float(param_locate_force_lat) != sac_header_t::UNDEF)
			{
				cout << "Lat: " << param_locate_force_lat << endl;
				if ( !rtloc.IsPointInGrid(stations[0]->lon, float(param_locate_force_lat), stations[0]->dep) )
					Fatal_Error("RTLoc: Forced latitude (" + ToString(param_locate_force_lat) + ") lies outside the grid");
			}

			if (float(param_locate_force_dep) != sac_header_t::UNDEF)
			{
				cout << "Dep: " << param_locate_force_dep << endl;
				if ( !rtloc.IsPointInGrid(stations[0]->lon, stations[0]->lat, float(param_locate_force_dep)) )
					Fatal_Error("RTLoc: Forced depth (" + ToString(param_locate_force_dep) + ") lies outside the grid");
			}

			cout << "==================================================================================================" << endl;
		}
	}

	// Check if any target lies outside the grid

	for (targets_t::iterator t = targets.begin(); t != targets.end(); t++)
	{
		if ( !rtloc.IsPointInGrid(t->lon, t->lat, t->dep) )
			Fatal_Error("RTLoc: Target \"" + t->name + "\" lies outside the grid");
	}

	// The vector of helicorders to display (helis) is potentially shorter than the full stations vector, as it's limited by display_heli_max_num
	// i.e. only the first n stations are displayed
	helis.insert(helis.end(), stations.begin(), stations.begin() + min(stations.size(), size_t(param_display_heli_max_num)));

	// sort by decreasing latitude
	sort(stations.begin(), stations.end(), cmp_stations_t());
	sort(helis.begin(),    helis.end(),    cmp_stations_t());

	// Binder (quake_id)
	// this must run after the simulated time start instant is defined (i.e. after SACs loading)

	binder.Init();
	binder.magheli.Init("", NUM_SAMPLES, stations[0], true);

	LoadingBar_End();

	helicorders_loaded = true;
}

void Restart_Helis()
{
	// Stop
	for (vector<station_t *>::iterator s = stations.begin(); s != stations.end() ; s++)
	{
		if ((*s)->z)		(*s)->z->Stop();
		if ((*s)->n)		(*s)->n->Stop();
		if ((*s)->e)		(*s)->e->Stop();
	}
	binder.magheli.Stop();

	// Start
	for (vector<station_t *>::iterator s = stations.begin(); s != stations.end() ; s++)
	{
		if ((*s)->z)		(*s)->z->Start();
		if ((*s)->n)		(*s)->n->Start();
		if ((*s)->e)		(*s)->e->Start();
	}
	binder.magheli.Start();
}

void ResetAll()
{
	mytime = 0;
	simutime_t :: Reset();
	SetPaused(true);
	AllSounds_Stop();

	heli_i = -1;

	// Icons
	icons.clear();

	float s = icon_t::FONTSIZE * 0.4f, x = s, y = icon_t::FONTSIZE/2*1.2f - icon_t::FONTSIZE/2;

	icons.push_back(icon_t("m/s^n",	x,y));					x += icons.back().GetW() + s;	icon_accel_i	=	int(icons.size()-1);

	// Add Pause and Restart icons in simulation mode

	if (!realtime)
	{
		x += s;
		icons.push_back(icon_t("Pause",	x,y,
			false, color_t(0,1,0,1), color_t(0,.4f,0,1)));	x += icons.back().GetW() + s;	icon_pause_i	=	int(icons.size()-1);

		x += s;
		icons.push_back(icon_t("Restart", x,y,
			true, color_t(1,0,0,1), color_t(.4f,0,0,1)));	x += icons.back().GetW() + s;	icon_restart_i	=	int(icons.size()-1);
		x += s;
	}

	icons_end_x = x;

	binder.Reset();

	Restart_Helis();

	if ((realtime || param_alarm_during_simulation) && !broker.Hostname().empty())
		broker.Start();

	SetPaused(false);
}

void Init_GUI()
{
	HELIS_W	=	(float)param_display_heli_width;
	MAP_W	=	1 - HELIS_W;
	MAP_X	=	HELIS_W;

	PreLoad_GUI();

	ResetAll();

	cout << endl << SecsToString(SecsNow()) << ": STARTING " << (realtime ? "REALTIME" : "SIMULATION") << endl << endl;
}

void End_Heli()
{
	if (!helicorders_loaded)
		return;

	binder.magheli.Stop();
	broker.Stop();

	network.clear();

	helicorders_loaded = false;
}

/*******************************************************************************

	Update functions

*******************************************************************************/

struct rect_t {
	float x,y,w,h;
};

void Rect_Heli(int num, rect_t &r)
{
	if (num == helis.size())
	{
		// Map
		r.x = MAP_X;
		r.y = MAP_Y;
		r.w = MAP_W;
		r.h = MAP_W;
		return;
	}
	else if (num == helis.size()+1)
	{
		// Magnitude
		r.x = MAP_X;
		r.h = (HELIS_H / 4);
		r.y = HELIS_Y0 + HELIS_H - r.h;
		r.w = MAP_W;
		return;
	}

	r.w = HELIS_W / 1;
	r.h = HELIS_H / helis.size();

	int total = int(helis.size());

	int partial_cols = (total%HELIS_NX);
	int rows = total/HELIS_NX + (partial_cols?1:0);
	int row  = num/HELIS_NX;

	int cols = (((row == rows-1) && partial_cols) ? partial_cols : HELIS_NX);
	int col = num - (num/HELIS_NX)*HELIS_NX;

	float x0 = HELIS_X0 + HELIS_W/2 - (r.w) * (cols/2) - (r.w/2) * (cols - (cols/2)*2);
	float y0 = HELIS_Y0 + HELIS_H/2 - (r.h) * (rows/2) - (r.h/2) * (rows - (rows/2)*2);

	r.x = x0 + r.w * col;
	r.y = y0 + r.h * row;
}

void Click_Heli(float mx, float my)
{
	if (heli_i >= 0)
	{
		if (heli_i_zoom_in && heli_i_amount >= 1.0f)
		{
			heli_i_t0 = mytime;
			heli_i_zoom_in = false;
		}
	}
	else
	{
		for (int i = 0; i < int(helis.size()) + 2; i++)
		{
			if (i != heli_i)
			{
				rect_t rect;
				Rect_Heli(i,rect);

				if ( (mx >= rect.x) && (mx <= rect.x+rect.w) && (my >= rect.y) && (my <= rect.y+rect.h) )
				{
					heli_i = i;
					heli_i_t0 = mytime;
					heli_i_zoom_in = true;
				}
			}
		}
	}
}

void Update_GUI(win_t &)
{
	mytime += DELTA_T;

	if (!GetPaused())
		binder.Run( stations );
}

/*******************************************************************************

	Draw functions

*******************************************************************************/

void Draw_Text()
{
	// Pause icon must always reflect the "paused" state

	if (!realtime)
	{
		icons[icon_pause_i].SetActive( GetPaused() );
	}

	vector<icon_t>::iterator icon = icons.begin();
	for (; icon != icons.end(); icon++)
	{
		icon->Draw(1.0f);
	}

	float x = icons_end_x;
	float y = icon_t::FONTSIZE/2 + icon_t::FONTSIZE/2*1.2f;
	float h = icon_t::FONTSIZE*1.2f;

	// Current time

	secs_t now = SecsNow();
//	now = 1275381059.945;	// test rounding
	// Round to 1 decimal digit
	now = now - 0.005 + 0.05;
	string now_s = SecsToString(now);
	SmallFont().Print(now_s.substr(0,11),  x,     y, h*.7f, h*.7f, FONT_Y_IS_CENTER);
	SmallFont().Print(now_s.substr(11,10), x+5*h, y, h,     h,     FONT_Y_IS_CENTER);
	x += 12 * h;

	// Simulation speed

	if (!realtime && param_simulation_speed != 1)
		SmallFont().Print("(x" + ToString(param_simulation_speed) + ")", x,y, h*.8f,h*.8f, FONT_Y_IS_CENTER);
	x += 4 * h;

	// Command line arguments

	{
		string args = net_name + " " + (realtime ? "Real-Time" : event_name);
		SmallFont().Print(args, x,y, h,h, FONT_Y_IS_CENTER);
		x = SmallFont().CurrX()/* + h*/;
	}

	// Special flags

	string flags;

	if ( float(param_locate_force_lon) != sac_header_t::UNDEF || float(param_locate_force_lat) != sac_header_t::UNDEF || float(param_locate_force_dep) != sac_header_t::UNDEF )
		flags += " fixloc";

	if (param_debug_gaps_period && param_debug_gaps_duration)
		flags += " gaps";

	SmallFont().Print(flags, x,y, h,h, FONT_Y_IS_CENTER, colors_t(1,1,0,1));

	// Version
	string s1 = APP_NAME, s2 = "v" + APP_VERSION;
	float sh = h*.7f;
	float sw = max(s1.length(),s2.length()) * sh;
	SmallFont().Print(s1, 1.0f-SCRY*0.06f-sw/2,y-sh, sh,sh, FONT_X_IS_CENTER);
	SmallFont().Print(s2, 1.0f-SCRY*0.06f-sw/2,y,    sh,sh, FONT_X_IS_CENTER);
}

void Draw_Heli(const win_t & win, int draw_i)
{
	rect_t rect;
	Rect_Heli(draw_i,rect);

	float space = 0.005f;

	double amount = (mytime - heli_i_t0) / HELI_ZOOM_TIME;
	Clamp(amount, 0.0, 1.0);
	heli_i_amount = float(amount);	// cast to float allowed since we're in the 0..1 range

	if ((heli_i_amount == 1.0f) && (!heli_i_zoom_in))
		heli_i = -1;

	if (!heli_i_zoom_in)
		heli_i_amount = 1.0f - heli_i_amount;

	float alpha = 1.0f;

	if ((heli_i >= 0) && (draw_i != heli_i))
		alpha = Interp(1.0f,0.8f,heli_i_amount);

	if (draw_i == heli_i)
	{
		float sxz,syz, swz,shz, spacez;

		if (draw_i == helis.size())
		{
			swz = 0.65f;
			shz = 0;

			sxz = (1    - swz) / 2;
			syz = MAP_Y;

			spacez = 0.010f;
		}
		else
		{
			swz = HELIS_W * 1.02f;
			shz = HELIS_H / 2;

			sxz = HELIS_X0 + (HELIS_W - swz) / 2;
			syz = HELIS_Y0 + (HELIS_H - shz) / 2;

			spacez = 0.010f;
		}

		rect.x = Interp( rect.x, sxz, heli_i_amount );
		rect.y = Interp( rect.y, syz, heli_i_amount );
		rect.w = Interp( rect.w, swz, heli_i_amount );
		rect.h = Interp( rect.h, shz, heli_i_amount );

		space = Interp( space, spacez, heli_i_amount );
	}

//	alpha = 1;

	if (draw_i == helis.size())
	{
		themap.Draw(
			network,
			binder.Quakes(),
			binder.Picks(),
			win,
			rect.x, rect.y, rect.w
		);
	}
	else if (draw_i == helis.size()+1)
	{
		const float duration = float(param_binder_quakes_life);

		binder.magheli.Draw(win,rect.x,rect.y, rect.w,rect.h, alpha, "Mag", 0/*realtime ? SecsNow() - duration : simutime_t :: Get() - duration*/, duration, true);
	}
	else
	{
		rect.x += space / 2;
		rect.y += space / 2;
		rect.w -= space;
		rect.h -= space;

		float dx = space/2;
		float dy = dx;

		// SAC shadow
		DrawQuad(NULL,rect.x+dx,rect.y+dy,rect.w,rect.h, colors_t(0,0,0,.5f));

		// SAC
		const float duration = float(param_display_heli_secs);

		heli_t *heli = NULL;
		string cmp = "";

		if (heli == NULL)						heli = helis[draw_i]->z;
		if (helis[draw_i]->z == NULL)			cmp += "-";
		else if (helis[draw_i]->z == heli)		cmp += "[Z]";
		else									cmp += "Z";

		cmp += " ";

		if (heli == NULL)						heli = helis[draw_i]->n;
		if (helis[draw_i]->n == NULL)			cmp += "-";
		else if (helis[draw_i]->n == heli)		cmp += "[N]";
		else									cmp += "N";

		cmp += " ";

		if (heli == NULL)						heli = helis[draw_i]->e;
		if (helis[draw_i]->e == NULL)			cmp += "-";
		else if (helis[draw_i]->e == heli)		cmp += "[E]";
		else									cmp += "E";

		heli->Draw(win,rect.x,rect.y, rect.w,rect.h, alpha, helis[draw_i]->name + " " + cmp, realtime ? SecsNow() - duration : simutime_t :: Get() - duration, duration, !Use_Accel());
	}
}


// Draw an alarm icon (tex) in the bottom right corner:
// - index  = 0..+inf, position (0 is rightmost)
// - amount = 0..duration is the highlight amount (0 = ghosted, duration/2 = opaque, duration = ghosted again)
// - duration = highlight duration in seconds
void DrawAlarmIcon(texptr_t tex, int index, secs_t amount, secs_t duration)
{
	if (amount == -1)
		amount = 0;
	else
		amount /= duration;

	float y = HELIS_Y0 + HELIS_H;
	float h = SCRY - y;
	float scale, alpha;

	const float scale_min = 0.8f;
	const float scale_max = 1.2f;

	const float alpha_min = 0.2f;
	const float alpha_max = 1.0f;

	const double amount_max = 0.3;

	Clamp(amount, 0.0, 1.0);

	if (amount < amount_max)
		amount = amount / amount_max;
	else
		amount = 1 - (amount - amount_max) / (1 - amount_max);

	scale = Interp(scale_min, scale_max, float(amount));
	alpha = Interp(alpha_min, alpha_max, float(amount));

	float x = 1 - h/2 - index*h;

	DrawQuad(Tex_Frame(), x-h/2,      y,               h,      h,       colors_t(1,1,1,alpha) );
	DrawQuad(tex,         x-h*scale/2,y+h/2-h*scale/2, h*scale,h*scale, colors_t(1,1,1,alpha) );
}


void Draw_GUI(win_t & win)
{
	// Enter 2d mode
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(
		 0,1,	// left-right
		 0,1,	// bottom-top
		-1,1	// near far
	);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glDisable( GL_CULL_FACE );
	glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );

	// Background

	float r = 0.30f, g = 0.54f, b = 0.72f;

	secs_t quake_age = binder.SecsFromLastQuake();
	if ( quake_age >= 0 )
	{
		const secs_t secs_fade = 2.0;

		if ( quake_age <= secs_fade )									// fade-in to alarm color
			quake_age = quake_age / secs_fade;
		else if ( quake_age <= param_binder_quakes_life - secs_fade )	// alarm color
			quake_age = 1.0;
		else if ( quake_age <= param_binder_quakes_life )				// fade out to normal color
			quake_age = 1.0 - (quake_age - (param_binder_quakes_life - secs_fade)) / secs_fade;
		else
			quake_age = 0.0;											// normal color

		Clamp(quake_age, 0.0, 1.0);
		float quake_amount = float(quake_age);	// cast to float allowed since we're in the 0..1 range

		float quake_r = 0.95f, quake_g = 0.4f, quake_b = 0.0f;

		r = Interp(r, quake_r, quake_amount);
		g = Interp(g, quake_g, quake_amount);
		b = Interp(b, quake_b, quake_amount);
	}
	float r1 = r/2, g1 = g/2, b1 = b/2;

	DrawQuad(NULL, 0,0,				1,HELIS_Y0,			colors_t(r,g,b,1, r1,g1,b1,1)	);
	DrawQuad(NULL, 0,HELIS_Y0,		1,SCRY-HELIS_Y0*2,	colors_t(r1,g1,b1,1)			);
	DrawQuad(NULL, 0,SCRY-HELIS_Y0,	1,HELIS_Y0, 		colors_t(r1,g1,b1,1, r,g,b,1)	);

	int curr_heli_i = heli_i;

	for (int i = 0; i < int(helis.size()) + 2; i++)
	{
		if (i != curr_heli_i)
			Draw_Heli(win, i);
	}

	if (curr_heli_i >= 0)
		Draw_Heli(win, curr_heli_i);

	// Alarms
	int index = 0;	// rightmost icon
	if (!broker.Hostname().empty())
	{
		secs_t amount = binder.SecsFromBrokerConnection();

		const secs_t duration = 5.0;
		Clamp(amount, 0.0, duration/2);
		DrawAlarmIcon(	Tex_Display(),		index++,	amount,							duration);
	}
	DrawAlarmIcon(	Tex_Heartbeat(),	index++,	binder.SecsFromLastHeartbeat(),	2.0	);
	DrawAlarmIcon(	Tex_Alarm(),		index++,	binder.SecsFromLastAlarm(),		1.0	);


	binder.Draw();


	DrawCompanyLogo(TOP_RIGHT, mytime);

	Draw_Text();

	// Back to the default state
	glEnable( GL_CULL_FACE );
	glEnable( GL_DEPTH_TEST );
	glDepthMask( GL_TRUE );
}


/*******************************************************************************

	Events Handling

*******************************************************************************/

void Keyboard_GUI(SDL_KeyboardEvent &event, win_t &)
{
	bool pressed = (event.state == SDL_PRESSED);

	switch(event.keysym.sym)
	{
		case SDLK_r:		if (pressed)	ResetAll();	break;

		default: break;
	}
}

void Mouse_GUI(SDL_Event &event, win_t &)
{
	switch (event.type)
	{
		case SDL_MOUSEBUTTONDOWN:
		{
			switch( event.button.button )
			{
				case SDL_BUTTON_LEFT:

				float mx = float(event.button.x) / screen_w;
				float my = float(event.button.y) / screen_w;

				if ( (my >= HELIS_Y0) && (my < HELIS_Y0+HELIS_H) )
				{
					Click_Heli(mx,my);
				}
				else
				{
					for (int icon_i = 0; icon_i != icons.size(); icon_i++)
					{
						if ( icons[icon_i].Click(userinput.mousex,userinput.mousey) )
						{
							if (icon_i == icon_pause_i)
							{
								SetPaused( !GetPaused() );
							}
							else if (icon_i == icon_restart_i)
							{
								icons[icon_i].SetActive(true);
								ResetAll();
							}
						}
					}
				}
				break;
			}
		}
		break;
	}
}

}	// namespace


/*******************************************************************************

	Global variables / functions

*******************************************************************************/

vector<station_t *> stations;

void heli_t :: Draw(const win_t & win, float x, float y, float w, float h, float alpha, const string & title, secs_t time0, float duration, bool use_counts)
{
	float fonth;

	if ((x > 1.0f) || (x+w < 0.0f) || (y > SCRY) || (y+h < 0.0f))
		return;

	Lock();

	glPushAttrib( GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT );

	// Enter 2D mode
	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();
	glOrtho(
		0,1,		// left-right
		SCRY,0,		// bottom-top
		-1,1			// near far
	);

	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();

	glDisable( GL_CULL_FACE );
	glDisable( GL_LIGHTING );
	glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );
	glDisable ( GL_TEXTURE_2D );
	glEnable ( GL_BLEND );
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_LINE_SMOOTH);
	glLineWidth(1.0f);

	color_t bg_color = IsLaggingOrFuture() ? color_t(1,1,.8f,alpha) : color_t(1,1,1,alpha);

	float border	=	.05f * h * 2;

	// vertical span of the data drawing part of the helicorder (in screen coordinates)
	float min_iy	=	y + border;
	float max_iy	=	y + h - border;

	// Show latencies (only on data streams, not graphs)
	float latency_w = w / 15;
	if (/*!realtime || */isGraph)
		latency_w = 0;

	// horizontal span of the data drawing part of the helicorder (in screen coordinates)
	float min_ix	=	x + border + latency_w;
	float max_ix	=	x + w - border;

	// Latency

	if (latency_w)
	{
		colors_t colors_ok  (0.85f,1.0f,0.85f,alpha);
		colors_t colors_warn(1.0f,1.0f,0.8f,alpha);
		colors_t colors_err (1.0f,0.6f,0.6f,alpha);

		colors_t colors_text(0.2f,0.2f,0.8f,alpha);

		fonth = latency_w / (11-3) * 1.9f;	// Ld 10m20.5s
		Clamp(fonth, 0.0001f, h/2);

		// Feed Latency
		secs_t Lf = latency_feed;
		colors_t Lf_colors;

		if (Lf < -1.0f || Lf > 10.0f)	Lf_colors = colors_err;
		else if (Lf > 5.0f)				Lf_colors = colors_warn;
		else							Lf_colors = colors_ok;

		DrawQuad(NULL,x,y+h/2,latency_w,h/2, Lf_colors);
		ArialFont().Print("Lf "+IntervalToString(Lf),x+fonth/4,y+h/4*3,fonth,fonth, FONT_Y_IS_CENTER, colors_text);

		// Data Latency
		secs_t Ld = ((Lf > 15.0f) ? latency_feed : latency_data);	// use feed latency instead of data latency if packets are not being received (since data latency would be meaningless)
		colors_t Ld_colors;

		if (Ld < -1.0f || Ld > 8.0f)		Ld_colors = colors_err;
		else if (Ld > 4.0f)				Ld_colors = colors_warn;
		else							Ld_colors = colors_ok;

		// Ignore data latency if packets are not being received
		if (Lf > 15.0f)
		{
			Ld = 0;
			Ld_colors = colors_err;
		}

		DrawQuad(NULL,x,y,latency_w,h/2, Ld_colors);
		ArialFont().Print("Ld "+IntervalToString(Ld),x+fonth/4,y+h/4*1,fonth,fonth, FONT_Y_IS_CENTER, colors_text);
	}

	// Helicorder

	DrawQuad(NULL,x+latency_w,y,w-latency_w,h, bg_color);

	if (end_time != -1)
	{
		int numpixels = RoundToInt( (max_ix - min_ix) * win.GetW() );
		float tpixel =  duration / numpixels; // seconds per pixel
		int pixel;

		// show latest data if time0 is not a real instant in time
		if (time0 == 0)
			time0 = end_time - duration;

		// convert time0 to a float offset into the samples
		float tbegin	=	SecsToOffset( time0 );

		tbegin = RoundToInt(tbegin/tpixel) * tpixel;

		float scale_y = h / 2 - border;

		scale_y *= user_scale_y;

		float base_iy = y+h/2;

		float dx		=	(max_ix - min_ix) / NonZero(numpixels);	// dx per pixel (in screen coordinates, i.e. screen width equals 1.0)

		if (isGraph)
		{
			depmin -= 0.5f;
			depmax += 0.5f;

			Clamp(depmin, 0.0f, 10.0f);
			Clamp(depmax, 0.0f, 10.0f);

			dmean = (depmin + depmax)/2;
		}

		float range = max(abs(depmin-float(dmean)),abs(depmax-float(dmean)));
		scale_y /= NonZero(max( range, isGraph ? 0 : float(station->isAccel ? param_display_heli_min_accel : param_display_heli_min_vel) / station->factor ));
		base_iy -= -float(dmean * scale_y);

		colors_t fontcolor;
		float tick;
		stringstream ss;


		if (isGraph)
		{
			// Horizontal ticks

			fonth = min(border * .9f, SCRY/80);
			fontcolor = colors_t(0,0,1,alpha*.7f);
			tick = border * .3f;
			stringstream ss;

			for (float ty = floorf(depmin/0.1f)*0.1f; ty < depmax; ty+=0.1f)
			{
				float iy0 = base_iy - float( ty * scale_y );

				ss.str("");	ss << /*scientific << */setprecision(2) << ty;

				if ((iy0 < min_iy) || (iy0 > max_iy))
					continue;

				bool full_line = abs(ty - RoundToInt(ty/0.5f)*0.5f) < .01f;

				glColor4f(0,0,1,alpha/2);

				glBegin(GL_LINES);
					glVertex2f(max_ix, iy0);
					glVertex2f(full_line ? min_ix : (max_ix-tick), iy0);
				glEnd();

				if (full_line)
					ArialFont().Print(
						ss.str(),
						max_ix,iy0,fonth,fonth,
						FONT_Y_IS_CENTER, fontcolor);
			}
		}
		else
		{
			fonth = min(border * .8f, SCRY/40);
			float  curr_factor	=	use_counts ? 1.0f : station->factor;
			string curr_units	=	use_counts ? "" : " " + string(station->isAccel ? "m/s^2" : "m/s");

			fontcolor = colors_t(0.0f,0.5f,0.5f,alpha);

			// Max
			if (depmax != -numeric_limits<float>::max())
			{
				ss.str("");	ss << /*scientific << */setprecision(2) << (depmax/* - dmean*/) * curr_factor;
				ArialFont().Print(ss.str() + curr_units,	x + latency_w,	y+border/2,		fonth,fonth,	FONT_Y_IS_CENTER,	fontcolor);
			}

			// Mean
			if (isGraph)
			{
				ss.str("");	ss << /*scientific << */setprecision(2) << (dmean/* - dmean*/) * curr_factor;
				ArialFont().Print(ss.str() + curr_units,	x + latency_w,	y+h/2,			fonth,fonth,	FONT_Y_IS_CENTER,	fontcolor);
			}

			// Min
			if (depmin != +numeric_limits<float>::max())
			{
				ss.str("");	ss << /*scientific << */setprecision(2) << (depmin/* - dmean*/) * curr_factor;
				ArialFont().Print(ss.str() + curr_units,	x + latency_w,	y+h-border/2,	fonth,fonth,	FONT_Y_IS_CENTER,	fontcolor);
			}
		}


		// Samples

		dmean = 0;
		depmin = +numeric_limits<float>::max();
		depmax = -numeric_limits<float>::max();
		unsigned dmean_count = 0;

		glScissor(	win.GetX() + RoundToInt(min_ix * win.GetW()), win.GetY() + RoundToInt((SCRY-max_iy) * win.GetW()),
					RoundToInt(win.GetW() * (max_ix-min_ix)), RoundToInt(win.GetW() * (max_iy-min_iy)));
		glEnable(GL_SCISSOR_TEST);

		glPushMatrix();
		glTranslatef(0,base_iy,0);
		glScalef(1,-scale_y,1);

		glColor4f(0,0,0,alpha);
		if (isGraph/*false*/)
			glBegin(GL_LINE_STRIP);
		else
			glBegin(GL_LINES);

		float ix0 = min_ix;
		float t0 = tbegin;
		for (pixel = 0; pixel < numpixels; pixel++, ix0+=dx, t0+=tpixel)
		{
			float iy0, iy1;
			GetSampleBar(t0, t0+tpixel, &iy0, &iy1);

			if (!iy0 && !iy1)
				continue;

			dmean += double(iy0 + iy1);	// omit /2
			++dmean_count;
			if (iy0 < depmin)	depmin = iy0;
			if (iy1 > depmax)	depmax = iy1;

			iy1 += 1.0f/screen_h/scale_y;	// avoid zero len line

			// For helicorders, draw a vertical for every two points (GL_LINES).
			// For graphs, each bar is linked to the next one (GL_LINE_STRIP),
			// and graph points are "cross" shaped cluster of bars (+) so that gaps between them
			// are linked like this: +--+.
			// Yes, it's an ugly hack to reuse this code (for helicorders) to draw sparse graphs,
			// because it causes artifacts when the graph time span is long and many values from
			// the graph are averaged per pixel.

//			glVertex2f(ix0,(iy0+iy1)/2);
			glVertex2f(ix0,iy0);
			glVertex2f(ix0,iy1);
//			glVertex2f(ix0,(iy0+iy1)/2);
		}
		glEnd();

		glPopMatrix();
		glDisable(GL_SCISSOR_TEST);

		dmean /= NonZero(dmean_count * 2);	// 2 points per pixel

		// Vertical ticks

		fonth = min(border * .9f, SCRY/80);
		fontcolor = colors_t(0,0,1,alpha*.7f);
		tick = border * .3f;

		for (secs_t t = (long int)(time0/1)*1; t < time0+duration; t+=1)
		{
			pixel = RoundToInt(float(t-time0)/tpixel);
			float ix0 = min_ix + pixel * dx;

			string s = SecsToString_HHMMSS(t);
//			float s_xsize = fonth * s.length();

			if (ix0 < min_ix || ix0 > max_ix)
				continue;

			bool full_line = (t - (long int)(t/5)*5) == 0;

			glColor4f(0,0,1,alpha/2);

			glBegin(GL_LINES);
				glVertex2f(ix0,max_iy);
				glVertex2f(ix0, full_line ? min_iy : (max_iy-tick));
			glEnd();

			// HH:MM (top)
			if (full_line /*&& (ix0 > x + s_xsize/2) && (ix0 < x+w - s_xsize/2)*/)
			{
				ArialFont().Print(
					s.substr(0,5),
					ix0,y+border,fonth,fonth,
					FONT_X_IS_CENTER | FONT_Y_IS_MAX, fontcolor);
			}

			// SS (bottom)
			if (full_line /*&& (ix0 > x + fonth) && (ix0 < x+w - fonth)*/)
			{
				ArialFont().Print(
					s.substr(6,2),
					ix0,y+h-border,fonth,fonth,
					FONT_X_IS_CENTER, fontcolor);
			}
		}

		// Picks

		fonth = border;
		fontcolor = colors_t(1,0,0,alpha*.8f);
		glColor4f(fontcolor.r,fontcolor.g,fontcolor.b,fontcolor.a);

		picks_set_t::const_iterator p = picks.begin();
		for (int p_index = 0; p != picks.end(); p++, p_index++)
		{
			float ix0, ix1;

			// Pick
			ix0 = min_ix + float(p->t - p->dt - time0) / tpixel * dx;
			ix1 = min_ix + float(p->t + p->dt - time0) / tpixel * dx;
			if ((ix0 >= min_ix) && (ix0 <= max_ix))
				DrawQuad(NULL, ix0,min_iy, max(ix1-ix0,dx),max_iy-min_iy,fontcolor);

			// highlight picks linked to an event
			if (p->quake_id != pick_t::NO_QUAKE)
			{
				// Linked quake
#if 0
				ArialFont().Print(
					"q" + ToString(p->quake_id) /*+ "[" +
					MAGLABEL[MAG_S]       + " " + MagToString(p->quake_mag_s      ) + "," +
					MAGLABEL[MAG_P_SHORT] + " " + MagToString(p->quake_mag_p_short) + "," +
					MAGLABEL[MAG_P_LONG]  + " " + MagToString(p->quake_mag_p_long ) + "]"*/,
					ix0,y,fonth,fonth,
					FONT_X_IS_CENTER, fontcolor);
#endif
				// P-waves (short)
				if (param_magnitude_p_secs_short > 0)
				{
					colors_t p_colors(0,0,0,alpha*.1f);

					if (p->quake_mag[MAG_P_SHORT] != -1)
						p_colors = colors_t(1,1,0,alpha*.4f);

					ix0 = min_ix + float(p->t - time0) / tpixel * dx;
					ix1 = min_ix + float(p->t + param_magnitude_p_secs_short - time0) / tpixel * dx;

					Clamp(ix0, min_ix, max_ix);
					Clamp(ix1, min_ix, max_ix);

					if ((ix0 > min_ix) || (ix1 < max_ix))
						DrawQuad(NULL, ix0,min_iy, ix1-ix0,max_iy-min_iy, p_colors);
				}

				// P-waves (long)
				if (param_magnitude_p_secs_long > 0)
				{
					colors_t p_colors(0,0,0,alpha*.1f);

					if (p->quake_mag[MAG_P_LONG] != -1)
						p_colors = colors_t(1,1,0,alpha*.4f);

					// do not overlap with the other P window
					ix0 = min_ix + float(p->t + param_magnitude_p_secs_short - time0) / tpixel * dx;
					ix1 = min_ix + float(p->t + param_magnitude_p_secs_long - time0) / tpixel * dx;

					Clamp(ix0, min_ix, max_ix);
					Clamp(ix1, min_ix, max_ix);

					if ((ix0 > min_ix) || (ix1 < max_ix))
						DrawQuad(NULL, ix0,min_iy, ix1-ix0,max_iy-min_iy, p_colors);
				}

				// S-waves
				if (param_magnitude_s_secs > 0)
				{
					colors_t s_colors(0,0,0,alpha*.1f);

					if (p->quake_mag[MAG_S] != -1)
						s_colors = colors_t(1,0,0,alpha*.4f);

					float s_delay = station->CalcSDelay( binder.Quake(p->quake_id).origin );

					ix0 = min_ix + float(p->t + s_delay - time0) / tpixel * dx;
					ix1 = min_ix + float(p->t + s_delay + param_magnitude_s_secs - time0) / tpixel * dx;

					Clamp(ix0, min_ix, max_ix);
					Clamp(ix1, min_ix, max_ix);

					if ((ix0 > min_ix) || (ix1 < max_ix))
						DrawQuad(NULL, ix0,min_iy, ix1-ix0,max_iy-min_iy, s_colors);
				}
			}
		}

		// Clipping

		if (param_waveform_clipping_secs > 0)
		{
			for (timespans_t::const_iterator c = clipspans.begin(); c != clipspans.end(); c++)
			{
				float ix0 = min_ix + float(c->GetT0() - time0) / tpixel * dx;
				float ix1 = min_ix + float(c->GetT1() - time0) / tpixel * dx;
				Clamp(ix0, min_ix, max_ix);
				Clamp(ix1, min_ix, max_ix);
				if (ix1 > ix0)
					DrawQuad(NULL, ix0,min_iy, max(ix1-ix0,dx),max_iy-min_iy, colors_t(0,0,0,alpha*.1f));
			}
		}

		// Magnitudes of the last earthquake

		if ( param_display_heli_show_mag )
		{
			for (picks_set_t::const_reverse_iterator p = picks.rbegin(); p != picks.rend(); p++)
			{
				if (p->quake_id != pick_t::NO_QUAKE)
				{
					const quake_t & q = binder.Quake(p->quake_id);
					if ( SecsNow() - q.secs_creation <= param_binder_quakes_life + 30 )
					{
						fonth = (h-2*border) * 0.4f;
						Clamp(fonth, .009f, .02f);

							ArialFont().Print(
								// ToString(p->quake_rms) + " " +
								( param_magnitude_p_secs_short ? rtmag.GetLabel(MAG_P_SHORT) + "=" + MagToString(p->quake_mag[MAG_P_SHORT]) + " " : "" ) +
								( param_magnitude_p_secs_long  ? rtmag.GetLabel(MAG_P_LONG)  + "=" + MagToString(p->quake_mag[MAG_P_LONG] ) + " " : "" ) +
								( param_magnitude_s_secs       ? rtmag.GetLabel(MAG_S)       + "=" + MagToString(p->quake_mag[MAG_S]      ) + " " : "" ) +
								"km=" + ToString(RoundToInt(station->Distance(q.origin))) + " ",
								min_ix, max_iy, fonth,fonth,
								FONT_Y_IS_MAX, fontcolor);
					}

					break;
				}
			}
		}

	} // (end_time != -1)

	// Frame

	glColor4f(0,0,0,alpha);
	glBegin(GL_LINE_LOOP);
		glVertex2f(x+latency_w,y);
		glVertex2f(x+w,y);
		glVertex2f(x+w,y+h);
		glVertex2f(x+latency_w,y+h);
	glEnd();
	glBegin(GL_LINE_LOOP);
		glVertex2f(min_ix,min_iy);
		glVertex2f(min_ix,max_iy);
		glVertex2f(max_ix,max_iy);
		glVertex2f(max_ix,min_iy);
	glEnd();

	// Restore the rendering state
	glMatrixMode( GL_PROJECTION );
	glPopMatrix();

	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();

	glPopAttrib();

	// Title

	fonth = h * 0.15f;
	Clamp(fonth, 0.01f, 0.03f);
	ArialFont().Print(title, min_ix+(max_ix-min_ix)/2,min_iy+fonth/2, fonth,fonth, FONT_CENTER,colors_t(0.0f,0.5f,0.0f,alpha));

	Unlock();
}


void binder_t :: Draw()
{
	// Display every quake as a line of text on screen (only the last one in real-time mode)

	vector<quake_t> :: const_iterator quakes_begin;
	if (realtime && !quakes.empty())
		quakes_begin = quakes.end()-1;
	else
		quakes_begin = quakes.begin();

	for (vector<quake_t>::const_iterator q = quakes_begin; q != quakes.end(); q++)
	{
		stringstream ss;
		ss.str("");

		ss << "QUAKE " << *q;

		debugtext.Add(ss.str());
	}
}


// Preload media files
void PreLoad_GUI()
{
	LoadingBar_Start();
		const float K = 100.0f/3;
		LoadingBar_SetNextPercent( 1*K );	PreLoad_Helicorders();
		LoadingBar_SetNextPercent( 2*K );	PreLoad_Map();
		LoadingBar_SetNextPercent( 3*K );	PreLoad_Binder();
	LoadingBar_End();
}

void End_GUI()
{
	End_Heli();
}

// Enter this state
void State_Add_GUI()
{
	state.Add(win_t(0,SCRY/2.0f,1/2.0f,SCRY/2.0f, 0,0,1.0f,SCRY),Init_GUI,Keyboard_GUI,Mouse_GUI,Update_GUI,Draw_GUI,End_GUI);
}
