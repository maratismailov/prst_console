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

#include <iomanip>
#include <limits>

#include "rtloc.h"

#include "gui.h"
#include "config.h"
#include "loading_bar.h"


float tnow; //Current time has to be a global variable
float sigma;
int edt_null;

char *statfilename;
char *logfilename;


rtloc_t rtloc;

rtloc_t :: rtloc_t() :
	station(NULL),
	Pgrid(NULL),
	Sgrid(NULL)
{
}

void rtloc_t :: Init(const string & ctrlfilename)
{
	SetConstants();

	FILE *ctrlfile;
	ctrlfile = ReadCtrlFile (ctrlfilename.c_str(), &params);

	cout << endl;
	cout << "==================================================================================================" << endl;
	cout << "    RTLoc Parameters (" << ctrlfilename << ")" << endl;
	cout << "==================================================================================================" << endl;

	const int w = 25;
	cout << setw(w) << "sigma = "                 << params.sigma << endl;
	cout << setw(w) << "sum = "                   << params.sum << endl;
	cout << setw(w) << "pow = "                   << params.pow << endl;
	cout << setw(w) << "renorm = "                << params.renorm << endl;
	cout << setw(w) << "pdfcut = "                << params.pdfcut << endl;
	cout << setw(w) << "init_num_cells_x = "      << params.octtreeParams.init_num_cells_x << endl;
	cout << setw(w) << "init_num_cells_y = "      << params.octtreeParams.init_num_cells_y << endl;
	cout << setw(w) << "init_num_cells_z = "      << params.octtreeParams.init_num_cells_z << endl;
	cout << setw(w) << "min_node_size = "         << params.octtreeParams.min_node_size << endl;
	cout << setw(w) << "max_num_nodes = "         << params.octtreeParams.max_num_nodes << endl;
	cout << setw(w) << "stop_on_min_node_size = " << params.octtreeParams.stop_on_min_node_size << endl;

	cout << "==================================================================================================" << endl;

	int nsta = params.nsta;
//	int npick = params.npick;

	sigma = params.sigma;

	station = ReadStation (ctrlfile, station, nsta);

//	pick = ReadPick (ctrlfile, pick, npick, station, nsta);

	// AJL 20070111
	// initialize random number generator - usef by OctTree
	SRAND_FUNC(9837);
	// -AJL

	/* Set null value for edt according to the approach chosen */
	if ( params.sum ) edt_null = 0;
	else edt_null = 1;

	Pgrid = (GridDesc *) calloc (nsta, sizeof(GridDesc));
	Sgrid = (GridDesc *) calloc (nsta, sizeof(GridDesc));
	if (Pgrid == NULL || Sgrid == NULL)
		Fatal_Error("RTLoc: out of memory allocating the location grids");

	/* Let's open all the time grid files... */
	LoadingBar_Start();
	for (int n=0; n<nsta; n++) {
		FILE *Phdr, *Shdr, *Pbuf, *Sbuf;

		LoadingBar_SetNextPercent(100.0f * (n+1) / nsta);

		// P
		if ( OpenGrid3dFile(station[n].Pfile, &Pbuf, &Phdr, &Pgrid[n], "time", &station[n].desc, 0) < 0) {
			puterr2 ("ERROR opening grid file: ", station[n].Pfile);
//			exit (EXIT_ERROR_FILEIO);
			Fatal_Error("RTLoc: can't open grid file: " + string(station[n].Pfile));
		}
		// AJL 20070110 - set up standard NLL grid storage and array access
		if ( AllocateGrid(&Pgrid[n]) == NULL )
			Fatal_Error("RTLoc: out of memory reading grid file: " + string(station[n].Pfile));
		if ( CreateGridArray(&Pgrid[n]) == NULL )
			Fatal_Error("RTLoc: out of memory reading grid file: " + string(station[n].Pfile));
		if ( ReadGrid3dBuf(&Pgrid[n], Pbuf) != 0 )
			Fatal_Error("RTLoc: can't read grid file: " + string(station[n].Pfile));
		CloseGrid3dFile(&Pbuf, &Phdr);
		if (Pgrid[n].numx <= 1)
			Fatal_Error("RTLoc: grid file: " + string(station[n].Pfile) + " must be 3D, not 2D (i.e. numx > 1)");
		// S
		if ( OpenGrid3dFile(station[n].Sfile, &Sbuf, &Shdr, &Sgrid[n], "time", NULL, 0) < 0) {
			puterr2 ("ERROR opening grid file: ", station[n].Sfile);
//			exit (EXIT_ERROR_FILEIO);
			Fatal_Error("RTLoc: can't open grid file: " + string(station[n].Sfile));
		}
		// AJL 20070110 - set up standard NLL grid storage and array access
		if ( AllocateGrid(&Sgrid[n]) == NULL )
			Fatal_Error("RTLoc: out of memory reading grid file: " + string(station[n].Sfile));
		if ( CreateGridArray(&Sgrid[n]) == NULL )
			Fatal_Error("RTLoc: out of memory reading grid file: " + string(station[n].Sfile));
		if ( ReadGrid3dBuf(&Sgrid[n], Sbuf) != 0 )
			Fatal_Error("RTLoc: can't read grid file: " + string(station[n].Sfile));
		CloseGrid3dFile(&Sbuf, &Shdr);
		if (Sgrid[n].numx <= 1)
			Fatal_Error("RTLoc: grid file: " + string(station[n].Sfile) + " must be 3D, not 2D (i.e. numx > 1)");

		/*TODO: we have to check all the grids to be the same or
		think at defining a grid which contains them all */
	}
	LoadingBar_End();

	/* If all the grids are the same, we can use one of them
	as a prototype for the location grid */
	initLocGrid(&Pgrid[0], &Grid);
	DestroyGridArray(&Grid);
	FreeGrid(&Grid);
}

rtloc_t :: ~rtloc_t()
{
	for (int n=0; n<params.nsta; n++)
	{
		if (Pgrid != NULL)
		{
			DestroyGridArray(&Pgrid[n]);
			FreeGrid(&Pgrid[n]);
		}

		if (Sgrid != NULL)
		{
			DestroyGridArray(&Sgrid[n]);
			FreeGrid(&Sgrid[n]);
		}
	}

	free(Pgrid);
	free(Sgrid);
}

// FIXME remove
int rtloc_t :: StationNameToId(const string & stname)
{
	const char *name = stname.c_str();
	for (int statid = 0; statid < params.nsta; statid++)
	{
		if ( strcmp(station[statid].name, name) == 0 )
			return statid;
	}
	Fatal_Error("RTLoc: Unknown station " + stname);
	return -1;	// unreachable
}

// x,y in km, centered around the grid center
// z in km. 0 is the ground level

void rtloc_t :: LonLat_To_XY(float lon, float lat, float *x, float *y)
{
	double double_x, double_y;
	latlon2rect(0, double(lat), double(lon), &double_x,&double_y);

	*x = float(double_x);
	*y = float(double_y);
}

void rtloc_t :: XY_To_LonLat(float x, float y, float *lon, float *lat)
{
	double double_lat, double_lon;
	rect2latlon(0, x, y, &double_lat, &double_lon);	// x,y in km, centered around the proj center

	*lon = float(double_lon);
	*lat = float(double_lat);
}

bool rtloc_t :: IsPointInGrid(float lon, float lat, float dep)
{
	float x,y;
	LonLat_To_XY(lon, lat, &x, &y);
	return IsPointInsideGrid(&Pgrid[0],double(x),double(y),double(dep)) ? true : false;
}

void rtloc_t :: GetGridArea(float *min_lon, float *min_lat, float *min_dep, float *max_lon, float *max_lat, float *max_dep, float *dx, float *dy, float *dz)
{
	XY_To_LonLat( float(Pgrid->origx),                               float(Pgrid->origy),                               min_lon, min_lat);

	XY_To_LonLat( float(Pgrid->origx + (Pgrid->numx-1) * Pgrid->dx), float(Pgrid->origy + (Pgrid->numy-1) * Pgrid->dy), max_lon, max_lat);

	*min_dep =    float(Pgrid->origz);
	*max_dep =    float(Pgrid->origz + (Pgrid->numz-1) * Pgrid->dz);

	*dx = float(Pgrid->dx);
	*dy = float(Pgrid->dy);
	*dz = float(Pgrid->dz);
}

float rtloc_t :: TravelTime(const string & stname, char wave, float lon, float lat, float dep)
{
	int statid = StationNameToId( stname );

	GridDesc *grid;

	switch( wave )
	{
		case 'p':
		case 'P':	grid = &Pgrid[statid];	break;

		case 's':
		case 'S':	grid = &Sgrid[statid];	break;

		default:	grid = NULL;
					Fatal_Error("TravelTime called with invalid wave parameter");
	}

	float x,y;
	LonLat_To_XY(lon,lat, &x,&y);
	return ReadAbsInterpGrid3d(/*NULL,*/ grid, double(x), double(y), double(dep));
}

void rtloc_t :: GetStationLonLatDep(const string & stname, float *lon, float *lat, float *dep)
{
	int statid = StationNameToId( stname );

	XY_To_LonLat(float(station[statid].desc.x), float(station[statid].desc.y), lon, lat);
	*dep = float(station[statid].desc.z);
}

float rtloc_t :: LonLatDep_Distance_km(float lon1, float lat1, float dep1, float lon2, float lat2, float dep2)
{
	float x1, y1, x2, y2;

	rtloc.LonLat_To_XY( lon1, lat1, &x1, &y1 );
	rtloc.LonLat_To_XY( lon2, lat2, &x2, &y2 );

	return sqrt( Sqr(x1 - x2) + Sqr(y1 - y2) + Sqr(dep1 - dep2) );
}

void rtloc_t :: DistanceWithError(const string & stname, const origin_t & o, float *distance, float *error)
{
	int statid = StationNameToId( stname );

	float float_x, float_y;
	LonLat_To_XY(o.lon, o.lat, &float_x, &float_y);

	double x,y,z;
	x = double(float_x);
	y = double(float_y);
	z = double(o.dep);

	*distance =	(float) sqrt(
		Sqr(station[statid].desc.x - x) +
		Sqr(station[statid].desc.y - y) +
		Sqr(station[statid].desc.z - z)
	);

	if (param_locate_ignore_error)
		*error = 0;
	else
	{
		float mean_distance =	(float) sqrt(
			Sqr(station[statid].desc.x - o.mean_x) +
			Sqr(station[statid].desc.y - o.mean_y) +
			Sqr(station[statid].desc.z - o.mean_z)
		);

		*error		=	float (
			abs(station[statid].desc.x - o.mean_x) * sqrt(o.cov_xx) +
			abs(station[statid].desc.y - o.mean_y) * sqrt(o.cov_yy) +
			abs(station[statid].desc.z - o.mean_z) * sqrt(o.cov_zz)
		) / mean_distance;
	}
}

void rtloc_t :: Locate( quake_t & q, origin_t *o )
{
	int i, statid;

	int evid = 0;

	// Reset station association with the event. Start by ignoring all stations when locating.
	// We will add triggering stations and non-triggering stations (if in stations.txt) later.

	// station[statid].evid[evid] can be:
	// * -1  =  ignore (not working/lagging station)
	// *  0  =  not associated (no pick)
	// *  1  =  associated (pick)

	for (statid = 0; statid < params.nsta; statid++)
	{
		station[statid].evid[evid] = -1;	// -1 means ignore
	}

	// Convert quake picks to Pick array, and mark the stations with a pick to be used in the location

	secs_t t_first_pick = q.picks.begin()->pick.t;
	secs_t t_last_pick  = q.picks.rbegin()->pick.t;

	int npicks = int(q.picks.size());
	params.npick = npicks;
	Pick *picks = new Pick[npicks];

	binder_picks_set_t::iterator p;
	for (i = 0, p = q.picks.begin(); p != q.picks.end(); p++, i++)
	{
		statid = StationNameToId(p->heli->station->name);

		picks[i].pickid		=	i;
		picks[i].evid		=	evid;
		picks[i].statid		=	statid;
		picks[i].time		=	float(p->pick.t - t_first_pick);	// convert to delta from the first pick

//		cout << "RTPICK " << station[statid].name << " " << picks[i].time << endl;

		if ( i == npicks-1 )
			picks[i].next = NULL;
		else
			picks[i].next = &picks[i+1];

		// Stations with a pick associated to the event are always used in the location
		station[ statid ].evid[evid] = 1;
	}

	// Compute the number of working stations, and the maximum end_time

	int nsta_working = 0;
	secs_t t_end_max = t_last_pick;	// tnow must be after the last pick

	for (statid = 0; statid < params.nsta; statid++)
	{
		const char *stname = station[statid].name;

		// Convert between station id's
		vector<station_t *>::const_iterator s;
		for (s = stations.begin(); s != stations.end(); s++)
		{
			if ( strcmp(stname, (*s)->name.c_str() ) == 0 )
				break;
		}
		if (s == stations.end())	// not in stations.txt
			continue;

		if ((*s)->z == NULL)			// no vertical component
			continue;

		secs_t t_end = (*s)->z->EndTime();

		/*
			Consider non-triggering stations only if they have sent data past the first pick i.e. ignore non working
			or severely lagging stations, but otherwise consider them non-triggering up to tnow (maximum end_time of
			working stations), even if they may have not sent data up to tnow yet, which is a compromise between
			correctness and effectiveness.

			The alternatives would be:

			- to altogether ignore non-triggering stations before the last pick, which is rather drastic, and
			  furthermore causes stations to continuously switch between the "non-triggering" and "ignored" state
			  (chaotic behaviour due to data arriving in 1s packets);
			- to use the minimum end_time of non-triggering stations past the first few picks as tnow, discarding
			  picks and lack of picks after that (which is too drastic).
		*/

		if ( (station[statid].evid[evid] == -1) && (t_end >= t_first_pick) )
			station[statid].evid[evid] = 0;	// 0 means not associated (i.e. non-triggering)

		// If this station is working, update the maximum end_time and number of working stations
		if ( station[statid].evid[evid] != -1 )
		{
			++nsta_working;

			if (t_end > t_end_max)
				t_end_max = t_end;
		}
	}


	// Locate

	// Use the maximum time of the last sample of data over all the working station as tnow for location

	tnow = float(t_end_max - t_first_pick);

	Vect3D mean;
	Vect3D ml_hypo;
	Mtrx3D cov;
	float ml_otime;
	Ellipsoid3D ell;

	/*double prob_max = */SearchEdt(&Grid, picks, station, nsta_working, Pgrid, Sgrid, evid, &params, (!realtime && param_debug_save_rtloc) ? 1 : 0, &mean, &ml_hypo, &cov, &ell, &ml_otime);
	// Calc RMS of this pick based on the most likely quake location (not used, maybe in the future..)
	for (i = 0, p = q.picks.begin(); p != q.picks.end(); p++, i++)
	{
		float rms = GetRms(&ml_hypo, Pgrid, Sgrid, picks, i, &Grid, &params);
		// FIXME: circumvent the fact we aren't allowed to change set elements
		const_cast<pick_t *>(&p->pick)->quake_rms = rms;
	}

/*
	//  Allocate and fill Grid with probability density. Calc mean and most likely hypocenter (mean, ml_hypo).
	OctTreeSearch(&Grid, picks, npicks, station, params.nsta, nsta_working, Pgrid, Sgrid, evid, &params, imean, &nevaluated, &norm, &mean, &prob_max, &ml_hypo);
	mean.x /= norm;
	mean.y /= norm;
	mean.z /= norm;

	// ml_hypo is in km and centered at the origin of the grid (i.e. center for x,y, 2 km below the top of the grid for z).
	// mean is centered at the corner of the grid and in grid units.

	// Calc most likely origin time, covariance matrix of location (ml_otime, cov)
	LocStat(&Grid, Pgrid, Sgrid, norm, &mean, &ml_hypo, station, params.nsta, evid, picks,		&cov, &ml_otime);

	if (param_debug_save_rtloc)
	{
		char suffix[80];
		sprintf (Grid.chr_type, "RTLOC_SUM_%05.2f_%2.2f", tnow, sigma);
		sprintf (suffix, "ev%2.2d.%05.2f", evid, tnow);
		WriteGrid3dBuf(&Grid, NULL, const_cast<char*>((sacs_dir + event_name).c_str()), suffix);
	}
*/
	double double_lat, double_lon, double_dep;
	rect2latlon(0, ml_hypo.x, ml_hypo.y, &double_lat, &double_lon);	// x,y in km, centered around the proj center
	double_dep = double(ml_hypo.z);									// z in km. 0 is the ground level

	// Write result

	o->lon		=	float(double_lon);
	o->lat		=	float(double_lat);
	o->dep		=	float(double_dep);

	o->mean_x	=	float(Grid.origx + mean.x * Grid.dx);
	o->mean_y	=	float(Grid.origy + mean.y * Grid.dy);
	o->mean_z	=	float(Grid.origz + mean.z * Grid.dz);

	if (param_locate_ignore_error)
	{
		o->cov_xx	=	0;
		o->cov_yy	=	0;
		o->cov_zz	=	0;
		o->ell		=	EllipsoidNULL;
	}
	else
	{
		o->cov_xx	=	float(cov.xx);
		o->cov_yy	=	float(cov.yy);
		o->cov_zz	=	float(cov.zz);
		o->ell		=	ell;
	}

	o->time		=	t_first_pick + secs_t( ml_otime );

	// Clean up
	DestroyGridArray(&Grid);
	FreeGrid(&Grid);
	delete [] picks;
}




string rtloc_t :: GroundStationName()
{
	float min_dist = +numeric_limits<float>::max();
	int sta_id = 0;
	for (int i = 0; i < params.nsta; i++)
	{
		float dist = abs(float(station[i].desc.z));
		if (dist < min_dist)
		{
			min_dist = dist;
			sta_id = i;
		}
	}

	return station[sta_id].name;
}

#if 0
string rtloc_t :: ClosestStationName(const origin_t & o)
{
	float qx, qy;
	LonLat_To_XY(o.lon, o.lat, &qx, &qy);

	float min_dist = +numeric_limits<float>::max();
	int sta_id = 0;
	for (int i = 0; i < params.nsta; i++)
	{
		float dist = Sqr(qx - float(station[i].desc.x)) + Sqr(qy - float(station[i].desc.y));
		if (dist < min_dist)
		{
			min_dist = dist;
			sta_id = i;
		}
	}

	return station[sta_id].name;
}
float rtloc_t :: QuakeTargetTravelTime(char wave, const origin_t & o, const origin_t & t)
{
	// Convert to linear coordinates

	float ox, oy;
	LonLat_To_XY(o.lon, o.lat, &ox, &oy);

	float tx, ty;
	LonLat_To_XY(t.lon, t.lat, &tx, &ty);

	// Calc the displacement needed to move the epicenter at the exact same position as the closest station

	string stname = ClosestStationName(o);
	int sta_id = StationNameToId( stname );

	float dx = float(station[sta_id].desc.x) - ox;
	float dy = float(station[sta_id].desc.y) - oy;
//	float dz = station[sta_id].desc.z - o.dep;

	// Apply this offset to the target instead (to compensate the fact that travel-times are relative to stations)

	float lon, lat, dep;
	XY_To_LonLat(tx + dx, ty + dy, &lon, &lat);
//	dep = o.dep + dz;
	dep = o.dep;

	return TravelTime(stname, wave, lon, lat, dep);
}
#endif

// Return the distance traveled by waves after "secs" seconds of propagation from the origin.
// Assumes a 1d velocity model!
float rtloc_t :: QuakeRadiusAfterSecs(char wave, const origin_t & o, float secs)
{
	// Find the closest station to the epicenter

//	string stname = ClosestStationName(o);
	string stname = GroundStationName();
	int sta_id = StationNameToId( stname );

	GridDesc *grid;

	switch( wave )
	{
		case 'p':
		case 'P':	grid = &Pgrid[sta_id];	break;

		case 's':
		case 'S':	grid = &Sgrid[sta_id];	break;

		default:	grid = NULL;
					Fatal_Error("QuakeRadiusAfterSecs called with invalid wave parameter");
	}

	// Find the hypocenter with a travel-time of "secs" seconds to the chosen station.
	// The corresponding epicentral distance then is the radius traveled by the waves on the ground, to be plot on the map.
	// Perform a binary search on grid x: start from the mid-point between station and far border,
	// then move closer or more far from the station as appropriate to match the requested travel-time,
	// halving the search x interval at each iteration.

	double sta_x = station[sta_id].desc.x;
	double sta_y = station[sta_id].desc.y;

	double min_x = grid->origx;
	double max_x = grid->origx + grid->numx * grid->dx;

	double near_x = sta_x;
	double far_x = ((max_x - sta_x) > (sta_x - min_x)) ? max_x : min_x;

	double x = (near_x + far_x)/2;
	double y = sta_y;

	while ( abs(far_x-near_x) > 0.25f && (x > min_x) && (x < max_x) )
	{
		float ttime = ReadAbsInterpGrid3d(/*NULL,*/ grid, x, y, o.dep);

		if (ttime - secs > 0)
		{
			// move closer to the station
			far_x = x;
			x = (x + near_x)/2;
		}
		else
		{
			// move away from the station
			near_x = x;
			x = (x + far_x)/2;
		}
	}

	float r = float(sqrt(Sqr(x - sta_x) + Sqr(y - sta_y)));

	// propagate at constant speed outside of the grid

	float ttime = ReadAbsInterpGrid3d(/*NULL,*/ grid, x, y, o.dep);
	if (secs > ttime)
	{
		float vel = r / ttime;
		r += vel * (secs - ttime);
	}

	return r;
}

// Extract a rough velocity model from a travel times grid and log it (for future reference)
// Note: very low precision results
// FIXME probably obsolete now that NonLinLoc saves a Model grid in time/ too.
void rtloc_t :: LogVelocityModel()
{
	float slon, slat, sdep;

	// Use the first station to scan the grid vertically and measure velocities

	int nsta = 0;

	string sname = string(station[nsta].name);
	GetStationLonLatDep(sname, &slon, &slat, &sdep);

//	cout << sname << " " << slon << " " << slat << " " << sdep << endl;

	GridDesc *pgrid = &Pgrid[nsta], *sgrid = &Sgrid[nsta];

	double vp_prev = -99, dep_prev = -99;

	const double depdelta = 0.05;	// depth sampling granularity

	int numChanges = 0;

	for (double dep = 0.0; dep <= 100.0; dep += depdelta)
	{
		double dep1 = dep + depdelta;

		if ( !(IsPointInGrid(slon, slat, float(dep)) && IsPointInGrid(slon, slat, float(dep1))) )
			continue;

		bool sameside = ((dep < sdep) && (dep1 < sdep)) || ((dep >= sdep) && (dep1 >= sdep));

		float ttp = abs(	ReadAbsInterpGrid3d(/*NULL,*/ pgrid, station[nsta].desc.x, station[nsta].desc.y, dep)		+
							ReadAbsInterpGrid3d(/*NULL,*/ pgrid, station[nsta].desc.x, station[nsta].desc.y, dep1)		* (sameside ? -1 : 1)	);

		float tts = abs(	ReadAbsInterpGrid3d(/*NULL,*/ sgrid, station[nsta].desc.x, station[nsta].desc.y, dep)		+
							ReadAbsInterpGrid3d(/*NULL,*/ sgrid, station[nsta].desc.x, station[nsta].desc.y, dep1)		* (sameside ? -1 : 1)	);

		double vp = RoundToInt( (depdelta / ttp) * 10 ) / 10.0;
		double vs = RoundToInt( (depdelta / tts) * 10 ) / 10.0;

		if ( abs(vp - vp_prev) >= 0.005 )
			++numChanges;

		if (numChanges == 1)
			dep_prev = (dep+dep1)/2;

		if (numChanges >= 2)
		{
			cout << setw(3) << RoundToInt(dep_prev*10)/10.0 << setw(5) << vp << setw(5) << vs << endl;
			vp_prev = vp;
			numChanges = 0;
		}
	}
}
