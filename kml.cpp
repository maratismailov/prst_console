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

	Write a KML animation on disk of the estimates provided by PRESTo for an earthquake
	(each estimate contains time, location, magnitude and triggered stations)

*******************************************************************************/

#include <iomanip>
#include <fstream>

#include "binder.h"

#include "config.h"
#include "rtloc.h"
#include "pgx.h"

/*******************************************************************************

	Local variables / functions

*******************************************************************************/

namespace
{

void KML_Disc(ostream & os, float lon, float lat, float r)
{
	lat = DegsToRads(lat);
	lon = DegsToRads(lon);
	r = r/6378.137f;

	os << "\t\t\t<MultiGeometry>\n";
	os << "\t\t\t<Polygon>\n";
	os << "\t\t\t\t<outerBoundaryIs>\n";
	os << "\t\t\t\t\t<LinearRing>\n";
	os << "\t\t\t\t\t\t<coordinates>\n";
	float lon_rads = 0, lat_rads = 0;
	const int N = 36;
	for (int i = 0; i <= N; i++)
	{
		float rads = 2*FLOAT_PI / N * i;

		lat_rads = asin(sin(lat)*cos(r) + cos(lat)*sin(r)*cos(rads));

		lon_rads = atan2(sin(rads)*sin(r)*cos(lat), cos(r)-sin(lat)*sin(lat_rads));
		lon_rads = fmod((lon+lon_rads + FLOAT_PI), 2*FLOAT_PI) - FLOAT_PI;

		os << "\t\t\t\t\t\t\t" << RadsToDegs(lon_rads) << "," << RadsToDegs(lat_rads) << "," << "0\n";
	}
	os << "\t\t\t\t\t\t</coordinates>\n";
	os << "\t\t\t\t\t</LinearRing>\n";
	os << "\t\t\t\t</outerBoundaryIs>\n";
	os << "\t\t\t</Polygon>\n";
	os << "\t\t\t<Point><altitudeMode>relativeToGround</altitudeMode>\n";
	os << "\t\t\t\t<coordinates>" << RadsToDegs(lon_rads) << "," << RadsToDegs(lat_rads) << "," << "0</coordinates>\n";
	os << "\t\t\t</Point>\n";
	os << "\t\t\t</MultiGeometry>\n";
}

}	// namespace

/*******************************************************************************

	Global variables / functions

*******************************************************************************/

quake_estimate_t :: quake_estimate_t( const binder_picks_set_t & _picks, const origin_t & _origin, float _mag, float _mag_min, float _mag_max )
: secs_estimate(SecsNow()), picks(_picks), origin(_origin), mag(_mag), mag_min(_mag_min), mag_max(_mag_max)
{
}

void binder_t :: SaveQuakeKML( const string & fileprefix, quake_t & q, const vector<station_t *> & stations )
{
	cout << SecsToString(SecsNow()) << ": Save KML" << endl;

	const color_t col_stat			=	color_t(0.3f,1.0f,0.3f,1.0f);
//	const color_t col_stat_ghost	=	color_t(0.6f,0.6f,0.6f,0.8f);
	const color_t col_stat_loc		=	color_t(1.0f,0.6f,0.3f,1.0f);
	const color_t col_stat_pick		=	color_t(1.0f,1.0f,0.3f,1.0f);

	stringstream ss;

	// Begin KML

	ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
	ss << "<kml xmlns=\"http://www.opengis.net/kml/2.2\">\n";

	// Document

	ss << "<Document>\n";
	ss << "\t<name>" << SecsToString(q.estimates.back().origin.time) << " M = " << MagToString(q.estimates.back().mag) << "</name>\n";

	// Station style

	ss << "\t<Style id=\"station\">\n";
	ss << "\t\t<LabelStyle>\n";
	ss << "\t\t\t<scale>0.6</scale>\n";
	ss << "\t\t</LabelStyle>\n";
	ss << "\t\t<IconStyle>\n";
	ss << "\t\t\t<Icon>\n";
	ss << "\t\t\t\t<href>http://maps.google.com/mapfiles/kml/shapes/triangle.png</href>\n";
	ss << "\t\t\t</Icon>\n";
	ss << "\t\t\t<color>" << col_stat << "</color>\n";
	ss << "\t\t</IconStyle>\n";
	ss << "\t</Style>\n";

	// Quake style

	ss << "\t<Style id=\"quake\">\n";
	ss << "\t\t<IconStyle>\n";
	ss << "\t\t\t<Icon>\n";
	ss << "\t\t\t\t<href>http://maps.google.com/mapfiles/kml/shapes/star.png</href>\n";
	ss << "\t\t\t</Icon>\n";
	ss << "\t\t\t<color>ff0000ff</color>\n";
	ss << "\t\t</IconStyle>\n";
	ss << "\t</Style>\n";

	// P-Waves style

	ss << "\t<Style id=\"p-waves\">\n";
	ss << "\t\t<IconStyle>\n";
	ss << "\t\t\t<scale>0</scale>\n";
	ss << "\t\t</IconStyle>\n";
	ss << "\t\t<LineStyle>\n";
	ss << "\t\t\t<color>c000ffff</color>\n";
	ss << "\t\t\t<width>5</width>\n";
	ss << "\t\t</LineStyle>\n";
	ss << "\t\t<PolyStyle>\n";
	ss << "\t\t\t<fill>1</fill>\n";
	ss << "\t\t\t<outline>1</outline>\n";
	ss << "\t\t\t<color>4000ffff</color>\n";
	ss << "\t\t</PolyStyle>\n";
	ss << "\t</Style>\n";

	// S-Waves style

	ss << "\t<Style id=\"s-waves\">\n";
	ss << "\t\t<IconStyle>\n";
	ss << "\t\t\t<scale>0</scale>\n";
	ss << "\t\t</IconStyle>\n";
	ss << "\t\t<LineStyle>\n";
	ss << "\t\t\t<color>c00000ff</color>\n";
	ss << "\t\t\t<width>5</width>\n";
	ss << "\t\t</LineStyle>\n";
	ss << "\t\t<PolyStyle>\n";
	ss << "\t\t\t<fill>1</fill>\n";
	ss << "\t\t\t<outline>1</outline>\n";
	ss << "\t\t\t<color>400000ff</color>\n";
	ss << "\t\t</PolyStyle>\n";
	ss << "\t</Style>\n";

	// Target style

	ss << "\t<Style id=\"target\">\n";
	ss << "\t\t<LabelStyle>\n";
	ss << "\t\t\t<scale>1.2</scale>\n";
	ss << "\t\t</LabelStyle>\n";
	ss << "\t\t<IconStyle>\n";
	ss << "\t\t\t<Icon>\n";
	ss << "\t\t\t\t<href>http://maps.google.com/mapfiles/kml/shapes/target.png</href>\n";
	ss << "\t\t\t</Icon>\n";
	ss << "\t\t\t<color>ffd0ffff</color>\n";
	ss << "\t\t</IconStyle>\n";
	ss << "\t</Style>\n";

	// Info

	ss << "\t<Style id=\"info\">\n";
	ss << "\t\t<LabelStyle>\n";
	ss << "\t\t\t<scale>1.2</scale>\n";
	ss << "\t\t</LabelStyle>\n";
	ss << "\t\t<IconStyle>\n";
	ss << "\t\t\t<Icon>\n";
	ss << "\t\t\t\t<href>http://maps.google.com/mapfiles/kml/shapes/info-i.png</href>\n";
	ss << "\t\t\t</Icon>\n";
	ss << "\t\t</IconStyle>\n";
	ss << "\t</Style>\n";

	// Loop on estimates

//	string stname_near_quake = rtloc.ClosestStationName(q.estimates.back().origin);

	for (secs_t secs_begin = q.estimates.back().origin.time; secs_begin < q.estimates.back().secs_estimate + 15; secs_begin++)
	{
		secs_t secs_end = secs_begin + 1;

		// Folder (TimeSpan)

		ss << "\t<Folder>\n";
		ss << "\t\t<name>" << SecsToString(secs_begin) << "</name>\n";

		string timespan	=	"\t\t<TimeSpan>\n";
		timespan		+=	"\t\t\t<begin>" + SecsToKMLString(secs_begin) + "</begin>\n";
		if (secs_end)
			timespan	+=	"\t\t\t<end>"   + SecsToKMLString(secs_end)   + "</end>\n";
		timespan		+=	"\t\t</TimeSpan>\n";

		ss << timespan;

		// Info

		float secs_from_otime = float(secs_begin-q.estimates.back().origin.time);

		ss << "\t\t<Placemark>\n";
		ss << "\t\t\t<name>Time: " << RoundToInt(secs_from_otime*10.0f)/10.0f << " s</name>\n";
		ss << "\t\t\t<styleUrl>#info</styleUrl>\n";
		ss << "\t\t\t<Point><altitudeMode>relativeToGround</altitudeMode>\n";
		ss << "\t\t\t\t<coordinates>" << q.estimates.back().origin.lon << "," << stations.front()->lat << "</coordinates>\n";
		ss << "\t\t\t</Point>\n";
		ss << "\t\t</Placemark>\n";

		// P and S Waves

		float r2;

		// P-Waves
//		r2 = Sqr(5.0f * secs_from_otime) - Sqr(q.estimates.back().origin.dep);
//		if (r2 > 0)
		r2 = Sqr(rtloc.QuakeRadiusAfterSecs('P', q.estimates.back().origin, secs_from_otime));
		{
			ss << "\t\t<Placemark>\n";
			ss << "\t\t\t<name>P " << RoundToInt(sqrt(r2)) << " km</name>\n";
			ss << "\t\t\t<styleUrl>#p-waves</styleUrl>\n";
			KML_Disc(ss, q.estimates.back().origin.lon, q.estimates.back().origin.lat, sqrt(r2));
			ss << "\t\t</Placemark>\n";
		}

		// S-Waves
//		r2 = Sqr(3.5f * ttime) - Sqr(q.estimates.back().origin.dep);
//		if (r2 > 0)
		r2 = Sqr(rtloc.QuakeRadiusAfterSecs('S', q.estimates.back().origin, secs_from_otime));
		{
			ss << "\t\t<Placemark>\n";
			ss << "\t\t\t<name>S " << RoundToInt(sqrt(r2)) << " km</name>\n";
			ss << "\t\t\t<styleUrl>#s-waves</styleUrl>\n";
			KML_Disc(ss, q.estimates.back().origin.lon, q.estimates.back().origin.lat, sqrt(r2));
			ss << "\t\t</Placemark>\n";
		}

		// Find the relevant estimate for this timespan
		vector<quake_estimate_t> :: const_iterator qe = q.estimates.end();
		for (vector<quake_estimate_t> :: const_iterator e = q.estimates.begin(); e != q.estimates.end(); e++)
		{
			if ( (e->secs_estimate < secs_end) )
				qe = e;

			if ( (e->secs_estimate >= secs_end) )
				break;
		}

		// Stations

		for (vector<station_t *>::const_iterator s = stations.begin(); s != stations.end(); s++)
		{
			float scale = 1.0f;
			color_t col = col_stat;

			// Triggered stations

			if (qe != q.estimates.end())
			{
				int picknum = 0;
				for (binder_picks_set_t :: const_iterator p = qe->picks.begin(); p != qe->picks.end(); p++, picknum++)
				{
					if (p->heli->station->name != (*s)->name)
						continue;

					float amount = float(picknum) / NonZero( qe->picks.size() - 1 );
					col.r = Interp( col_stat_loc.r, col_stat_pick.r, amount);
					col.g = Interp( col_stat_loc.g, col_stat_pick.g, amount);
					col.b = Interp( col_stat_loc.b, col_stat_pick.b, amount);
					col.a = Interp( col_stat_loc.a, col_stat_pick.a, amount);

					break;
				}
			}

			ss << "\t\t<Placemark>\n";
			ss << "\t\t\t<styleUrl>#station</styleUrl>\n";
			ss << "\t\t\t<Style>\n";
			ss << "\t\t\t\t<IconStyle>\n";
			ss << "\t\t\t\t\t<scale>" << scale << "</scale>\n";
			ss << "\t\t\t\t\t<color>" << col << "</color>\n";
			ss << "\t\t\t\t</IconStyle>\n";
			ss << "\t\t\t</Style>\n";
			ss << "\t\t\t<Point><altitudeMode>relativeToGround</altitudeMode>\n";
			ss << "\t\t\t\t<coordinates>" << (*s)->lon << "," << (*s)->lat << "</coordinates>\n";
			ss << "\t\t\t</Point>\n";
			ss << "\t\t</Placemark>\n";
		}

		// Quake estimate

		if (qe != q.estimates.end())
		{
			ss << "\t\t<Placemark>\n";
			ss << "\t\t\t<name>" << MagToString(qe->mag) << "</name>\n";
			ss << "\t\t\t<styleUrl>#quake</styleUrl>\n";
			ss << "\t\t\t<Point><altitudeMode>relativeToGround</altitudeMode>\n";
			ss << "\t\t\t\t<coordinates>" << qe->origin.lon << "," << qe->origin.lat << "</coordinates>\n";
			ss << "\t\t\t</Point>\n";
			ss << "\t\t</Placemark>\n";
		}

		// Targets

		for (targets_t::iterator t = targets.begin(); t != targets.end(); t++)
		{
			if (!t->shown)
				continue;

			string label = t->fullname;

			if ( secs_begin >= q.estimates.front().secs_estimate )
			{
				float secs_remaining = t->CalcTravelTime('S', q.estimates.back().origin) - secs_from_otime;

				if ( secs_remaining > -15 )
				{
					label += ": " + ToString(RoundToInt(secs_remaining)) + "s";
					if (q.estimates.back().mag != -1)
					{
						float R_epi = rtloc.LonLatDep_Distance_km(q.estimates.back().origin.lon,q.estimates.back().origin.lat,q.estimates.back().origin.dep, t->lon,t->lat,t->dep);
						range_t peak;
						pga.CalcPeak( q.estimates.back().mag, R_epi, q.estimates.back().origin.dep, &peak );
						label += " " + ToString(RoundToInt((peak.val / 100 / 9.81f * 100) * 10)/10.0f) + "%g";
					}
				}
			}

			ss << "\t\t<Placemark>\n";
			ss << "\t\t\t<name>" << label << "</name>\n";
			ss << "\t\t\t<styleUrl>#target</styleUrl>\n";
			ss << "\t\t\t<Point><altitudeMode>relativeToGround</altitudeMode>\n";
			ss << "\t\t\t\t<coordinates>" << t->lon << "," << t->lat << "</coordinates>\n";
			ss << "\t\t\t</Point>\n";
			ss << "\t\t</Placemark>\n";
		}

		// End Folder (TimeSpan)

		ss << "\t</Folder>\n";
	}

	// Quake estimate (Final)

	ss << "\t\t\t<Placemark>\n";
	ss << "\t\t\t\t<name>" << MagToString(q.estimates.back().mag) << "</name>\n";
	ss << "\t\t\t\t<styleUrl>#quake</styleUrl>\n";
	ss << "\t\t\t\t<Style>\n";
	ss << "\t\t\t\t\t<IconStyle>\n";
	ss << "\t\t\t\t\t\t<color>ffffffff</color>\n";
	ss << "\t\t\t\t\t</IconStyle>\n";
	ss << "\t\t\t\t</Style>\n";
	ss << "\t\t\t\t<Point><altitudeMode>relativeToGround</altitudeMode>\n";
	ss << "\t\t\t\t\t<coordinates>" << q.estimates.back().origin.lon << "," << q.estimates.back().origin.lat << "</coordinates>\n";
	ss << "\t\t\t\t</Point>\n";
	ss << "\t\t\t</Placemark>\n";

	// End KML

	ss << "</Document>\n";
	ss << "</kml>\n";

	// Write to disk

	ofstream kml( (fileprefix + ".kml").c_str(), ios::out | ios::trunc );
	if (kml)
	{
		kml << ss.str();
		kml.flush();
		kml.close();
	}
}
