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

	Binder - Core of the EWS. It contains a list of all earthquakes,
	         handles new picks, binds them to declare a new earthquake,
			 processes all earthquakes, sends alarms.

*******************************************************************************/

#include <set>
#include <fstream>

#include "binder.h"

#include "heli.h"
#include "config.h"
#include "rtloc.h"
#include "rtmag.h"
#include "pgx.h"
#include "version.h"
#include "graphics2d.h"
#include "sound.h"
#include "map.h"

using namespace std;

/*******************************************************************************

	Local variables / functions

*******************************************************************************/

namespace
{

// Tweakable parameters:

const float		PICKS_MEMORY_MAX_SECS	=	60*30;		// delete picks more than 30 minutes in the past (or in the future!)
const size_t	PICKS_MEMORY_MIN_SIZE	=	200;		// but keep at least the last 200 picks in memory
														// (in case the system time is way off, or we might delete all incoming picks!)

const float		QUAKES_MEMORY_MAX_SECS	=	3600*1;		// delete quakes more than 1 hour in the past (or in the future!)
const size_t	QUAKES_MEMORY_MIN_SIZE	=	5;			// but keep at least the last 5 quakes in memory
														// (in case the system time is way off, or we might delete all new quakes!)

// Media files

static soundptr_t Sound_Alarm()
{
	static soundptr_t sound("alarm.wav",true);
	return sound;
}

static soundptr_t Sound_Shaking()
{
	static soundptr_t sound("shaking.wav");
	return sound;
}

}	// namespace


/*******************************************************************************


	RTLoc Origin class - position, uncertainty and time


*******************************************************************************/

bool origin_t :: operator != (const origin_t & rhs)
{
	return	(lon    != rhs.lon)    || (lat    != rhs.lat)    || (dep    != rhs.dep)    ||
			(mean_x != rhs.mean_x) || (mean_y != rhs.mean_y) || (mean_z != rhs.mean_z) ||
			(cov_xx != rhs.cov_xx) || (cov_yy != rhs.cov_yy) || (cov_zz != rhs.cov_zz) ||
			(time   != rhs.time);
}

ostream& operator<< (ostream& os, const origin_t& o)
{
	bool force_lon = (float(param_locate_force_lon) != sac_header_t::UNDEF);
	bool force_lat = (float(param_locate_force_lat) != sac_header_t::UNDEF);
	bool force_dep = (float(param_locate_force_dep) != sac_header_t::UNDEF);

	float lon = RoundToInt(o.lon*10000)/10000.0f;
	float lat = RoundToInt(o.lat*10000)/10000.0f;
	float dep = RoundToInt(o.dep*1000)/1000.0f;

	if (force_lon)
		os << "[" << lon << "]";
	else
		os << lon;
	if (param_locate_ignore_error)
		os << ", ";
	else
		os << " dx " << RoundToInt(sqrt(o.cov_xx)*10)/10.0f << " km, ";

	if (force_lat)
		os << "[" << lat << "]";
	else
		os << lat;
	if (param_locate_ignore_error)
		os << ", ";
	else
		os << " dy " << RoundToInt(sqrt(o.cov_yy)*10)/10.0f << " km, ";

	if (force_dep)
		os << "[" << dep << "]";
	else
		os << dep;
	if (param_locate_ignore_error)
		os << ", ";
	else
		os << " dz " << RoundToInt(sqrt(o.cov_zz)*10)/10.0f << " km, ";

	os << SecsToString(o.time);

	return os;
}

/*******************************************************************************

	quake_t - earthquake source parameters and related information:
	          e.g. ID, linked picks, PRESTo estimates and alarms data

*******************************************************************************/

string MagToString(const float mag)
{
	if (mag == -1)
		return "n/a";
	return OneDecimal(mag);
}

ostream& operator<< (ostream& os, const quake_t& q)
{
	os <<	q.id <<
			" " << q.origin <<
			" MS: " << MagToString(q.mag_s  ) <<
			" MP: " << MagToString(q.mag_p  ) <<
			" BM: " << MagToString(q.mag    ) <<
			" ("    << MagToString(q.mag_min) <<
			" - "   << MagToString(q.mag_max) << ")"
	;

	return os;
}


string quake_t :: filename() const
{
	string filename;
	if (realtime)
	{
		// e.g. <NET_DIR>/2013-12-01_19.21.53.99_<ID>
		filename = SecsToString(origin.time);
		filename = Replace(filename, " ", "_");
		filename = Replace(filename, ":", ".");
		filename += "_" + ToString(id);

		filename = net_dir + filename;
	}
	else
	{
		// e.g. <SACS_DIR>/<EVENT_NAME>_<ID>
		filename = sacs_dir + event_name;
		filename += "_" + ToString(id);
	}

	return filename;
}

void quake_t :: LinkPick(const binder_pick_t & new_pick)
{
	pair<binder_picks_set_t::iterator,bool> new_pick_result = picks.insert(new_pick);
	bool new_pick_found = new_pick_result.second;

	// FIXME: circumvent the fact we aren't allowed to change set elements
	const_cast<pick_t *>(&new_pick_result.first->pick)->quake_id = id;
	new_pick_result.first->heli->UpdatePick( new_pick_result.first->pick );

	// Exit on duplicate pick
	if (!new_pick_found)
		return;

	// LINK message. Mail log gets a LINK message too (but without the quake id)

	string link = SecsToString(SecsNow()) + ": LINK " + new_pick.heli->station->name + " " + SecsToString(new_pick.pick.t);

	cout <<	link										<<
			" Q: "		<<	id							<<
			endl;

	mail_log += link + "\n";
}

/*******************************************************************************

	binder_t - I/O

*******************************************************************************/

void binder_t :: LoadQuakeId()
{
	cout << endl;
	cout << "==================================================================================================" << endl;

	if (realtime)
	{
		string filename = net_dir + "quake_id.txt";

		cout << "    Quake ID (" << filename << ")" << endl;

		ifstream f( filename.c_str() );
		if (!f)
			Fatal_Error("Couldn't open quake ID file \"" + filename + "\"");

		f >> quake_id;

		if (f.fail())
			Fatal_Error("Couldn't parse a valid quake ID from file \"" + filename + "\"");

		if (quake_id < 0)
			Fatal_Error("Negative quake ID read from file \"" + filename + "\"");

		f.close();
	}
	else
	{
		cout << "    Quake ID (0)" << endl;
		quake_id = 0;
	}

	cout << "==================================================================================================" << endl;

	cout << quake_id << endl;

	cout << "==================================================================================================" << endl;
}

void binder_t :: SaveQuakeId()
{
	if (!realtime)
		return;

	string filename = net_dir + "quake_id.txt";
	ofstream f( filename.c_str(), ios::out | ios::trunc );
	if (f)
	{
		f << quake_id;
		f.flush();
	}

	if (!f)
		cerr << "*** Unable to save quake ID to \"" << filename << "\"" << endl;

	f.close();
}

void binder_t :: Init()
{
	LoadQuakeId();
}


void binder_t :: LogQuake( quake_t & q, bool logToMail )
{
	stringstream ss;
	ss << SecsToString(SecsNow()) << ": QUAKE " << q << endl;

	cout << ss.str();

	if (logToMail)
		q.mail_log += ss.str();
}

int binder_t :: MailQuake_ThreadFunc(void *quake_copy_ptr)
{
	quake_t *q_copy = (quake_t *)quake_copy_ptr;

	int res = 0;

	if (realtime)
	{
		string cmd =
		#if defined(WIN32)
			"eventpostproc\\eventpostproc.bat";
		#else
			"eventpostproc/eventpostproc.sh";
		#endif

		cmd += " " + q_copy->filename();

		res = system( cmd.c_str() );
	}

	delete q_copy;

	return res;
}

void binder_t :: MailQuake( quake_t & q )
{
	q.mail_sent = true;

	stringstream ss;
	ss << SecsToString(SecsNow()) << ": MAIL " << (realtime ? "(SENT) " : "(NOT SENT) ") << q << endl;

	cout << ss.str();
	q.mail_log += ss.str();

#if 0
	// Log the mail_log too
	string tmp = q.mail_log;
	string indent = "=\t";
	cout << "==================================================================================================" << endl;
	cout << "    Mail Log" << endl;
	cout << "==================================================================================================" << endl;
	cout << indent << endl << indent << Replace(tmp, "\n", "\n" + indent) << endl ;
	cout << "==================================================================================================" << endl;
#endif

	cout.flush();

	// Create a copy of the quake to send as mail, for thread safety:
	// the original quake can be modified if new picks arrive after quake_life has expired (i.e. while sending the mail)
	try	{
		quake_t *q_copy = new quake_t(q);
		SDL_CreateThread( MailQuake_ThreadFunc, ("mailquake" + ToString(q_copy->id)).c_str(), q_copy );	// thread function is responsible for deleting q_copy
	} catch (...) {}
}

/*******************************************************************************

	binder_t

*******************************************************************************/

binder_t binder;

void binder_t :: Reset()
{
	picks.clear();
	quakes.clear();
	if (!realtime)
		quake_id = 0;

	secs_heartbeat_sent = secs_latencies_logged = SecsNow();

	Sound_Alarm()->Stop();
	Sound_Shaking()->Stop();
}

binder_t :: binder_t()
{
	quake_id = 0;
	secs_heartbeat_sent = secs_latencies_logged = 0;
}

/*******************************************************************************

	binder_t - Picks

*******************************************************************************/

void binder_t :: PurgeOldPicks()
{
	if (picks.size() <= PICKS_MEMORY_MIN_SIZE)
		return;

	secs_t now = SecsNow();

	binder_picks_set_t::iterator p = picks.begin();
	while (p != picks.end())
	{
		if (abs(now - p->pick.t) > PICKS_MEMORY_MAX_SECS)
//			p = picks.erase(p);	// requires c++11
			picks.erase(p++);
		else
			++p;
	}
}

void binder_t :: PurgeOldQuakes()
{
	if (quakes.size() <= QUAKES_MEMORY_MIN_SIZE)
		return;

	secs_t now = SecsNow();

	vector<quake_t>::iterator q = quakes.begin();
	while (q != quakes.end())
	{
		if (abs(now - q->picks.begin()->pick.t) > QUAKES_MEMORY_MAX_SECS)
			q = quakes.erase( q );
		else
			++q;
	}
}

// Return true if the apparent velocity measured between this pick and the input pick is within a physically possible range
// (i.e. not much faster or slower than P-waves)
bool binder_pick_t :: CheckApparentVel( const binder_pick_t & bp ) const
{
	float dt = float(bp.pick.t - pick.t);
	if (dt < 0.001f)
		dt = 0.001f;

	float dr = heli->station->Distance( *bp.heli->station );

	// Do not associate picks too far away, even if they would match the highest apparent velocity at the longest time window.
	// Useful for nation-wide networks
	if (dr >= param_binder_apparent_vel_max_distance)
		return false;

	// Relax velocity checks when the distance between the two triggered stations is comparable
	// to the inter-station distance of the network
	if (dr <= param_binder_apparent_vel_stations_spacing)
	{
//		if ( (dt >= -2.0f) && (dt <= 4.0f) )
			return true;
	}

	// The apparent velocity can be slow, yet not as slow as S-waves, or fast,
	// up to "simultaneous" arrival at many stations (deep/external events)
	float vel = dr / dt;
	if ( (vel >= param_binder_apparent_vel_min) && (vel <= param_binder_apparent_vel_max) )
		return true;

	return false;
}

// Return true if the new pick falls within the association window, it has a compatible apparent velocity
// and its station is not already in picks
bool binder_t :: CheckPickAssoc(const binder_picks_set_t & picks, const binder_pick_t & new_pick, secs_t secs) const
{
	secs_t dt = new_pick.pick.t - picks.begin()->pick.t;

	if ( (dt >= 0) && (dt <= secs) && picks.begin()->CheckApparentVel(new_pick) )
	{
		// Check if the station of new_pick is already in picks
		for (binder_picks_set_t :: iterator p = picks.begin(); p != picks.end(); ++p)
		{
			if ( p->heli == new_pick.heli )
				return false;
		}

		return true;
	}

	return false;
}

// Link picks in the association window (of secs duration) of quake q
void binder_t :: LinkAssocPicks(quake_t & q, secs_t secs)
{
	binder_picks_set_t::iterator p = picks.begin();
	while (p != picks.end())
	{
		if ( CheckPickAssoc(q.picks, *p, secs) )
		{
			q.LinkPick(*p);
			// Remove pick from not linked ones
//			p = picks.erase(p);	// requires c++11
			picks.erase(p++);
		}
		else
			++p;
	}
}

// If the quake parameter q is NULL, find out if it's possible to declare a new quake thanks to the arrival of new_pick, i.e.:
// check if there are many picks (>= binder_stations_for_coincidence) in a short time window (<= binder_secs_for coincidence) around
// new_pick. The picks must also satisfy the apparent velocity criterium. Return the "best" set of picks, i.e. larger set / earlier picks.
// If q is an already declared quake, try to add new_pick to the coincidence window of q, eventually moving it back in time.
void binder_t :: FindCoincPicks(const binder_pick_t & new_pick, const quake_t *q, vector<binder_picks_set_t::iterator> & best_picks_iter)
{
	// Find all picks within the coincidence window to the new pick (including the new pick itself).
	// This will span an interval twice the coincidence window centered on the new pick.
	// Allow multiple picks from the same station (we'll be able to choose the best one later)
	secs_t quake_t_min = ((q == NULL) ? 0 : q->picks.rbegin()->pick.t - param_binder_secs_for_association);	// ignore picks too early for q
	vector<binder_picks_set_t::iterator> near_picks_iter;
	for (binder_picks_set_t :: iterator p = picks.begin(); p != picks.end(); ++p)
	{
		if ( p->pick.t >= quake_t_min && abs(new_pick.pick.t - p->pick.t) < param_binder_secs_for_coincidence )
			near_picks_iter.push_back(p);
	}

	// Find the largest set of picks in the coincidence window, with compatible apparent velocity (from the first pick in the set),
	// that contains enough picks to declare a new quake. Include picks from q if not NULL.
	best_picks_iter.clear();
	while (!near_picks_iter.empty())
	{
		// "Good" picks satisfy the apparent velocity and coincidence criteria
		binder_picks_set_t good_picks;
		good_picks.clear();
		good_picks.insert(*near_picks_iter.front());

		vector<binder_picks_set_t::iterator> good_picks_iter;
		good_picks_iter.clear();
		good_picks_iter.push_back(near_picks_iter.front());

		// Check compatibility of all the current quake picks with the first good pick
		if (q != NULL)
		{
			binder_picks_set_t :: iterator qp;
			for (qp = q->picks.begin(); qp != q->picks.end(); ++qp)
			{
				if (CheckPickAssoc(good_picks, *qp, param_binder_secs_for_coincidence))
					// Do not insert into good_picks_iter as the container is different (q->picks instead of picks).
					// Also, we don't want to link the quake picks again.
					good_picks.insert(*qp);
				else if (!CheckPickAssoc(good_picks, *qp, param_binder_secs_for_association))
					break;
			}
			if (qp != q->picks.end())
			{
				near_picks_iter.erase(near_picks_iter.begin());
				continue;
			}
		}

		// Enlarge the set of good picks with compatible not linked picks
		for (vector<binder_picks_set_t::iterator>::iterator np = ++near_picks_iter.begin(); np != near_picks_iter.end(); ++np)
		{
			if ( CheckPickAssoc(good_picks, **np, param_binder_secs_for_coincidence) )
			{
				good_picks.insert(**np);
				good_picks_iter.push_back(*np);
			}
		}

		// The largest set of good picks that satisfies the coincidence criteria is the best one
		// (prefer the previous set in case of a tie: it contains earlier picks)
		if ( (good_picks.size() >= int(param_binder_stations_for_coincidence)) && (good_picks.size() > best_picks_iter.size()) )
			best_picks_iter = good_picks_iter;

		// Try checking apparent velocities using the next pick as first pick
		near_picks_iter.erase(near_picks_iter.begin());
	}
}

/*
	Run binding algorithm on the new pick:
		- Insert the new pick in the list of not linked picks.
		- Link the new pick to the last quake if within the association window,
		  and with compatible apparent velocity.
		- Declare a new quake if enough picks lie in the coincidence window,
		  all with compatible apparent velocity, and no other quake occurred
		  within quake_life seconds from the new one.

	Note: keep in mind picks are not received in time order due to latency/concurrency.
*/
bool binder_t :: AddAndLinkPick(const binder_pick_t & new_pick, int *res_quake_id)
{
	bool quake_found = false;

	// Insert the new pick in the list of not linked picks

	pair<binder_picks_set_t::iterator,bool> new_pick_result = picks.insert(new_pick);
	bool new_pick_found = new_pick_result.second;

	// Exit on duplicate pick
	if (!new_pick_found)
		return quake_found;

	cout << SecsToString(SecsNow()) << ": PICK " << new_pick.heli->station->name << " " << new_pick.pick << endl;

	// Try to link the new pick to the last quake
	vector<binder_picks_set_t::iterator> best_picks_iter;

	if (!quakes.empty())
	{
		quake_t & q = quakes.back();

		// Check the coincidence window from the first linked pick, eventually moving the coincidence window
		// back in time thanks to the new pick (for e.g. earlier picks arriving later due to transmission lag)
		FindCoincPicks(new_pick, &q, best_picks_iter);
		bool coinc = !best_picks_iter.empty();

		// Otherwise check the association window from the first linked pick
		bool assoc = !coinc && CheckPickAssoc(q.picks, new_pick, param_binder_secs_for_association);
		if (assoc)
		{
			q.LinkPick(new_pick);
			// Remove pick from not linked ones
			picks.erase(new_pick_result.first);
		}

		if (assoc || coinc)
		{
			// Link the new pick to the last quake
			quake_found = true;
			*res_quake_id = q.id;

			if (coinc)
			{
				// Link coincidence picks, and remove them from not linked ones
				for (vector<binder_picks_set_t::iterator>::iterator bpi = best_picks_iter.begin(); bpi != best_picks_iter.end(); ++bpi)
				{
					q.LinkPick(**bpi);
					picks.erase(*bpi);
				}
				// Link picks in the association window of the new quake
				// (since later picks may have been received before those in the coincidence window!)
				LinkAssocPicks(q, param_binder_secs_for_association);
			}

			return quake_found;
		}
	}

	// Declare a new quake if enough not linked picks, with compatible apparent velocities, lie in a short time span
	FindCoincPicks(new_pick, NULL, best_picks_iter);
	if ( !best_picks_iter.empty() )
	{
		// Do not declare a new quake it is too close in time to the last declared one (either before or after)
		if ( quakes.empty() || (abs(best_picks_iter.front()->pick.t - quakes.back().picks.begin()->pick.t) > param_binder_quakes_separation) )
		{
			// New quake
			quake_found = true;

			quakes.push_back( quake_t(quake_id) );
			quake_t & q = quakes.back();

			*res_quake_id = q.id;

			// Link coincidence picks, and remove them from not linked ones
			for (vector<binder_picks_set_t::iterator>::iterator bpi = best_picks_iter.begin(); bpi != best_picks_iter.end(); ++bpi)
			{
				q.LinkPick(**bpi);
				picks.erase(*bpi);
			}
			// Link picks in the association window of the new quake
			// (since later picks may have been received before those in the coincidence window!)
			LinkAssocPicks(q, param_binder_secs_for_association);

			++quake_id;
			SaveQuakeId();

			PurgeOldQuakes();
		}
	}

	PurgeOldPicks();

	return quake_found;
}

/*******************************************************************************

	binder_t - SecsFromLast...

*******************************************************************************/

secs_t binder_t :: SecsFromLastQuake()
{
	if ( quakes.empty() )
		return -1;

	return SecsNow() - quakes.back().secs_creation;
}

secs_t binder_t :: SecsFromLastAlarm()
{
	if ( quakes.empty() )
		return -1;

	secs_t secs_alarm_sent = quakes.back().secs_alarm_sent;
	if (!secs_alarm_sent)
		return -1;

	return SecsNow() - secs_alarm_sent;
}

secs_t binder_t :: SecsFromLastHeartbeat()
{
	return SecsNow() - secs_heartbeat_sent;
}

secs_t binder_t :: SecsFromBrokerConnection()
{
	return broker.SecsFromConnection();
}

/*******************************************************************************

	binder_t - CalcQuakeLoc

*******************************************************************************/

bool binder_t :: CalcQuakeLoc( quake_t & q )
{
	q.secs_located = SecsNow();

	origin_t o(0,0,0);

	if ( float(param_locate_force_lon) == sac_header_t::UNDEF || float(param_locate_force_lat) == sac_header_t::UNDEF || float(param_locate_force_dep) == sac_header_t::UNDEF )
		rtloc.Locate( q, &o );

	if ( float(param_locate_force_lon) != sac_header_t::UNDEF || float(param_locate_force_lat) != sac_header_t::UNDEF || float(param_locate_force_dep) != sac_header_t::UNDEF )
	{
		if ( float(param_locate_force_lon) != sac_header_t::UNDEF )
		{
			o.lon = float(param_locate_force_lon);
			o.cov_xx = 0;
		}

		if ( float(param_locate_force_lat) != sac_header_t::UNDEF )
		{
			o.lat = float(param_locate_force_lat);
			o.cov_yy = 0;
		}

		if ( float(param_locate_force_dep) != sac_header_t::UNDEF )
		{
			o.dep = float(param_locate_force_dep);
			o.cov_zz = 0;
		}

		// Average origin time over stations
		o.time = 0;
		binder_picks_set_t::const_iterator bp = q.picks.begin(), end = q.picks.end();
		for (; bp != end; bp++)
			o.time += bp->pick.t - secs_t(rtloc.TravelTime(bp->heli->station->name,'P',o.lon,o.lat,o.dep));
		o.time /= q.picks.size();
	}

	// Round to 4 decimal digits
	o.lon = RoundToInt(o.lon*10000)/10000.0f;
	o.lat = RoundToInt(o.lat*10000)/10000.0f;
	o.dep = RoundToInt(o.dep*10000)/10000.0f;
	// o.time?

	if (o != q.origin)
	{
		q.origin = o;
//		Do not log magnitude yet. Otherwise we log the magnitude of the previous location
//		which is plain wrong if the location has changed much.
//		LogQuake( q, true );
		cout << SecsToString(SecsNow()) << ": LOCATION " << q.id << " " << q.origin << endl;

		return true;
	}

	return false;
}

/*******************************************************************************

	binder_t - CalcQuakeMag

*******************************************************************************/

bool binder_t :: CalcQuakeMag( quake_t & q )
{
	//	Magnitude has to be always updated (new quake location, new picks, new signal)

	// Calc the event magnitude from the displacement at each station

	float mag_p, mag_s, mag, mag_min, mag_max;
	stringstream rtmag_log;

	// Filter used to obatin the displacement:
	// - first use the filter for low mag events on all stations;
	// - if the resulting overall (i.e. bayesian) magnitude is above a threshold, calc again using the high mag filter.
	magfilt_t magfilt = MAGFILT_LOW;

	bool hasNewWindow;
	for (;;)
	{
		// Calc MP_SHORT, MP_LONG and MS at every station.
		// Obtain an average MP and MS, and the overall Bayesian magnitude

		int num_s, num_p;
		num_s = 0, num_p = 0;
		mag_s = 0, mag_p = 0;

		rtmag_log.clear();
		rtmag.ClearPeaks();

		hasNewWindow = false;
		for (binder_picks_set_t::iterator p = q.picks.begin(); p != q.picks.end(); p++)
		{
			float sta_ms = -1, sta_mp = -1;

			// Magnitudes
			// FIXME: circumvent the fact we aren't allowed to change set elements
			if ( CalcAndAddAllPickMagnitudes( magfilt, *const_cast<binder_pick_t *>(&(*p)), q.origin, rtmag_log ) )
				hasNewWindow = true;

			// Store the updated parameters in the heli pick for display
			p->heli->UpdatePick( p->pick );

			// MS
			if (p->pick.quake_mag[MAG_S] != -1)
			{
				sta_ms = p->pick.quake_mag[MAG_S];
				++num_s;
			}

			// FIXME: review
			// MP - Use P_SHORT if it's less than 5 else P_LONG
			if (p->pick.quake_mag[MAG_P_SHORT] != -1 && ((p->pick.quake_mag[MAG_P_SHORT] < 5) || (p->pick.quake_mag[MAG_P_LONG] == -1)))
			{
				sta_mp = p->pick.quake_mag[MAG_P_SHORT];
				++num_p;
			}
			else if (p->pick.quake_mag[MAG_P_LONG] != -1)
			{
				sta_mp = p->pick.quake_mag[MAG_P_LONG];
				++num_p;
			}

			if ( sta_ms != -1 )	mag_s += sta_ms;
			if ( sta_mp != -1 )	mag_p += sta_mp;
		}

		// Average MP and MS magnitudes across the stations

		mag_s = (num_s ? mag_s / num_s : -1);
		mag_p = (num_p ? mag_p / num_p : -1);

		// **************************
		// Bayesian Magnitude
		// **************************

		rtmag.CalcMagnitude(&mag, &mag_min, &mag_max, rtmag_log);

		// **************************

		if ( magfilt == MAGFILT_LOW && mag >= param_magnitude_high_threshold )
			magfilt = MAGFILT_HIGH;
		else
			break;
	}

	// Use one decimal digit for magnitudes

	if (mag_s != -1)
		mag_s = RoundToInt(mag_s*10)/10.0f;

	if (mag_p != -1)
		mag_p = RoundToInt(mag_p*10)/10.0f;

	if (mag != -1)
	{
		mag		=	RoundToInt(mag    *10)/10.0f;
		mag_min	=	RoundToInt(mag_min*10)/10.0f;
		mag_max	=	RoundToInt(mag_max*10)/10.0f;
	}

	q.mag_s		=	mag_s;
	q.mag_p		=	mag_p;

	// Bayesian magnitude change?

	// Use an epsilon close to 0.1 but a bit smaller, to make sure rounding errors are accounted for (e.g epsilon must be higher than 0.001 but smaller than 0.999)
	// IMPORTANT: beware of drifting magnitudes in steps smaller than epsilon that could go completely unnoticed!
	//            In our case, since we round to 1 digit, the minimum possible change is 0.1 so smaller steps than epsilon are not possible and we are safe!
	//            For instance, if smaller steps than epsilon were possible, two successive changes of epsilon/2 would never be detected as a magnitude change!
	const float epsilon = 0.09f;

	// Note that this also works when the previous magnitude is undefined, i.e. -1!
	bool magChanged	=	( abs(q.mag     - mag    )	>=	epsilon ) ||
						( abs(q.mag_min - mag_min)	>=	epsilon ) ||
						( abs(q.mag_max - mag_max)	>=	epsilon );

	if ( magChanged )
	{
		q.mag		=	mag;
		q.mag_min	=	mag_min;
		q.mag_max	=	mag_max;

		if ((mag != -1) && (&q == &quakes.back()) && magChanged)
		{
			magheli.Add(SecsNow(), mag_min, mag, mag_max);
			magheli.SetMarker(q.origin.time);
		}

		cout << rtmag_log.str();

		// Save RTMag distribution on disk (for debug)

		if (param_debug_save_rtmag)
			rtmag.SaveMagDistribution();

		// Log pick magnitudes in the mail

		for (binder_picks_set_t::iterator p = q.picks.begin(); p != q.picks.end(); p++)
		{
			if ( (p->pick.quake_mag[MAG_P_SHORT] != -1) || (p->pick.quake_mag[MAG_P_LONG] != -1) || (p->pick.quake_mag[MAG_S] != -1) )
			{
				q.mail_log += SecsToString(SecsNow()) + ": MAG " + p->heli->station->name;

				if (p->pick.quake_mag[MAG_P_SHORT] != -1)
					q.mail_log += " " +  rtmag.GetLabel(MAG_P_SHORT) + ": " + MagToString(p->pick.quake_mag[MAG_P_SHORT]);

				if (p->pick.quake_mag[MAG_P_LONG] != -1)
					q.mail_log += " " +  rtmag.GetLabel(MAG_P_LONG)  + ": " + MagToString(p->pick.quake_mag[MAG_P_LONG]);

				if (p->pick.quake_mag[MAG_S] != -1)
					q.mail_log += " " +  rtmag.GetLabel(MAG_S)       + ": " + MagToString(p->pick.quake_mag[MAG_S]);

				q.mail_log += "\n";
			}
		}
	}

	return magChanged;
}


inline void logDISP( stringstream & rtmag_log, station_t *station, const string & label, float mag, float disp, secs_t disp_time, secs_t pick_time, float distance, float distance_err, float snr )
{
	rtmag_log	<< SecsToString(SecsNow()) << ": DISP " << label
				<< " km: " << RoundToInt(distance*1000)/1000.0f << " +- " << RoundToInt(distance_err*1000)/1000.0f
				<< " arrival: " << SecsToString(pick_time)
				<< " Pd_time: " << SecsToString( disp_time )
				<< " Pd_counts: " << disp / station->factor
				<< " mag: " << MagToString(mag)
				<< " Pd(cm): " << disp*100
				<< " SNR: " << ((snr != -1) ? RoundToInt(snr*10)/10.0f : -1)
				<< endl;
}

bool binder_t :: CalcAndAddPickMagnitude( magfilt_t magfilt, magtype_t magtype, binder_pick_t & bp, const origin_t & origin, stringstream & rtmag_log )
{
	bool hasNewWindow =	false;

	float duration = rtmag.GetDuration(magtype);
	if (!duration)
		return hasNewWindow;

	// Calculate the peak displacement for this time window
	// (continuously update in case one of the components arrives later than the others)
	float disp = -1;
	secs_t disp_time = 0;

	// Filter used to obatin the displacement:
	string label = bp.heli->station->name + " " + rtmag.GetLabel(magtype);
	float delay = ((magtype == MAG_S) ? bp.heli->station->CalcSDelay( origin ) : 0);
	switch (magfilt)
	{
		case MAGFILT_LOW:
			bp.heli->station->CalcPeakDisplacement( float(param_magnitude_low_fmin),  float(param_magnitude_low_fmax),  rtmag.GetLabel(magtype) + " LOW",  rtmag.GetComponents(magtype), bp.pick.t + delay, duration, &disp,  &disp_time  );
			label += " LOW";
			break;

		case MAGFILT_HIGH:
			bp.heli->station->CalcPeakDisplacement( float(param_magnitude_high_fmin), float(param_magnitude_high_fmax), rtmag.GetLabel(magtype) + " HIGH", rtmag.GetComponents(magtype), bp.pick.t + delay, duration, &disp, &disp_time );
			label += " HIGH";
			break;
	}

	// Convert the displacement to magnitude (reject it if the SNR is low)
	float mag = -1;
	if (disp != -1)
	{
		float distance, distance_err;
		rtloc.DistanceWithError( bp.heli->station->name, origin, &distance, &distance_err);
		mag = rtmag.Mag( magtype, disp, distance );

		float snr = bp.heli->station->CalcPickSNR(rtmag.GetComponents(magtype), bp.pick.t, 10.0f, delay, duration);
		logDISP( rtmag_log, bp.heli->station, label, mag, disp, disp_time, bp.pick.t + delay, distance, distance_err, snr );

		if (snr == -1 || snr >= float(param_waveform_min_snr))
		{
			rtmag.AddPeak( label, magtype, disp, distance, distance_err );
		}
		else
		{
			disp = -1;
			mag  = -1;
		}
	}

	hasNewWindow				=	(bp.pick.disp[magtype] != disp);
	bp.pick.disp[magtype]		=	disp;
	bp.pick.quake_mag[magtype]	=	mag;

	return hasNewWindow;
}

bool binder_t :: CalcAndAddAllPickMagnitudes( magfilt_t magfilt, binder_pick_t & bp, const origin_t & origin, stringstream & rtmag_log )
{
	// Continuously update the magnitudes, in case one of the components arrives later than the others

	bool hasNewS = false, hasNewPShort = false, hasNewPLong = false;

	// S-waves
	if (param_magnitude_s_secs)
		hasNewS = CalcAndAddPickMagnitude( magfilt, MAG_S, bp, origin, rtmag_log );

	// Skip any P time window that overlaps the S time window, unless told to ignore the overlapping

	float s_delay = bp.heli->station->CalcSDelay( origin );

	// P-waves (short)
	if ( param_magnitude_p_secs_short && (!param_magnitude_s_secs || s_delay >= (float)param_magnitude_p_secs_short || param_magnitude_p_can_overlap_s) )
		hasNewPShort = CalcAndAddPickMagnitude( magfilt, MAG_P_SHORT, bp, origin, rtmag_log );

	// P-waves (long)
	if ( param_magnitude_p_secs_long && (!param_magnitude_s_secs || s_delay >= (float)param_magnitude_p_secs_long || param_magnitude_p_can_overlap_s) )
		hasNewPLong = CalcAndAddPickMagnitude( magfilt, MAG_P_LONG, bp, origin, rtmag_log );

	bool hasNewWindow =	(hasNewS || hasNewPShort || hasNewPLong);

	return hasNewWindow;
}

/*******************************************************************************

	binder_t - Send alarms to targets and brokers

*******************************************************************************/

void binder_t :: SendTargetsAlarm( quake_t & q, const vector<station_t *> & stations, targets_t & targets, bool logToLog, bool logToMail )
{
	if (q.mag == -1)
		return;

	stringstream ss;

	// Targets: send UDP alarms and log PGX

	for (targets_t::iterator t = targets.begin(); t != targets.end(); t++)
	{
		float R_epi		=	rtloc.LonLatDep_Distance_km( q.origin.lon, q.origin.lat, t->dep, t->lon,t->lat,t->dep );

		range_t PGA;
		pga.CalcPeak( q.mag, R_epi, q.origin.dep, &PGA );

		range_t PGV;
		pgv.CalcPeak( q.mag, R_epi, q.origin.dep, &PGV );

		float S_ttime	=	t->CalcTravelTime('S', q.origin);
		float remaining	=	float( q.origin.time + secs_t(S_ttime) - SecsNow() );

		// Send target specific alarm (PGX)

		if ((realtime || param_alarm_during_simulation) && t->ipaddress.host != 0)
		{
			float lon	=	RoundToInt(q.origin.lon*10000)/10000.0f;
			float lat	=	RoundToInt(q.origin.lat*10000)/10000.0f;
			float dep	=	RoundToInt(q.origin.dep*1000)/1000.0f;
			float dx	=	RoundToInt(sqrt(q.origin.cov_xx)*10)/10.0f;
			float dy	=	RoundToInt(sqrt(q.origin.cov_yy)*10)/10.0f;
			float dz	=	RoundToInt(sqrt(q.origin.cov_zz)*10)/10.0f;

			ss.str("");
			ss	<<	SecsToString(SecsNow()) <<		": ALARM"				<<
				" DEST:"		<<	t->name.substr(0,10)						<<

				" QID:"			<<	q.id									<<
				" SEQ:"			<<	q.alarm_seq								<<

				" PGA:"			<<	PGA.val									<<
				" PGAmin:"		<<	PGA.min									<<
				" PGAmax:"		<<	PGA.max									<<

				" PGV:"			<<	PGV.val									<<
				" PGVmin:"		<<	PGV.min									<<
				" PGVmax:"		<<	PGV.max									<<

				" SECS:"		<<	RoundToInt(remaining*100)/100.0f		<<

				" M:"			<<	MagToString(q.mag)						<<
				" Mmin:"		<<	MagToString(q.mag_min)					<<
				" Mmax:"		<<	MagToString(q.mag_max)					<<

				" SumPd:"		<<	-numeric_limits<float>::infinity()		<<
				" SumLgPd:"		<<	-numeric_limits<float>::infinity()		<<
				" SumTc:"		<<	-numeric_limits<float>::infinity()		<<
				" SumLgTc:"		<<	-numeric_limits<float>::infinity()		<<
				" STA:"			<<	0										<<

				" Rep:"			<<	RoundToInt(R_epi*1000)/1000.0f			<<
				" LON:"			<<	lon										<<
				" Xer:"			<<	dx										<<
				" LAT:"			<<	lat										<<
				" Yer:"			<<	dy										<<
				" DEP:"			<<	dep										<<
				" Zer:"			<<	dz										<<

				" Ot0:"			<<	SecsToString(q.origin.time)				;

			cout << ss.str() << endl;

			targets.SendAlarm(ss.str(), &t->ipaddress);

			++q.alarm_seq;
		}

		// Log target PGX

		if (logToLog || logToMail)
		{
			ss.str("");
			ss	<<	SecsToString(SecsNow()) << ": PGX " << t->name								<<
					" R: "			<<	RoundToInt(R_epi*1000)/1000.0f							<<
					" M: "			<<	MagToString(q.mag)										<<
					" PGA: "		<<	PGA.val << " (" << PGA.min << ", " << PGA.max << ")"	<<
					" PGV: "		<<	PGV.val << " (" << PGV.min << ", " << PGV.max << ")"	<<
					" S_TT: "		<<	RoundToInt(S_ttime*100)/100.0f							<<
					" Remaining: "	<<	RoundToInt(remaining*100)/100.0f						<<
					endl;

			if (logToLog)
				cout << ss.str();

			if (logToMail)
				q.mail_log += ss.str();
		}
	}


	if (!logToLog && !logToMail)
		return;


	// Stations: just log PGx

	for (vector<station_t *>::const_iterator s = stations.begin(); s != stations.end(); s++)
	{
		float R_epi		=	(*s)->EpiDistance( q.origin );

		range_t PGA;
		pga.CalcPeak( q.mag, R_epi, q.origin.dep, &PGA );

		range_t PGV;
		pgv.CalcPeak( q.mag, R_epi, q.origin.dep, &PGV );

		float S_ttime	=	(*s)->CalcTravelTime( 'S', q.origin );
		float remaining	=	float( q.origin.time + secs_t(S_ttime) - SecsNow() );

		// Log station PGX

		ss.str("");
		ss	<<	SecsToString(SecsNow()) << ": PGX " << (*s)->name			<<
				" R: "			<<	RoundToInt(R_epi*1000)/1000.0f			<<
				" M: "			<<	MagToString(q.mag)						<<
				" PGA: "		<<	PGA.val << " (" << PGA.min << ", " << PGA.max << ")"	<<
				" PGV: "		<<	PGV.val << " (" << PGV.min << ", " << PGV.max << ")"	<<
				" S_TT: "		<<	RoundToInt(S_ttime*100)/100.0f			<<
				" Remaining: "	<<	RoundToInt(remaining*100)/100.0f		<<
				endl;

		if (logToLog)
			cout << ss.str();

		if (logToMail)
			q.mail_log += ss.str();
	}
}

void binder_t :: SendBrokerHeartBeat( broker_t & broker )
{
	if (! ((realtime || param_alarm_during_simulation) && !broker.Hostname().empty()) )
		return;

	string s	=	"<?xml version=\"1.0\" ?>\n" +
					("<hb xmlns=\"http://heartbeat.reakteu.org\" originator=\"" + APP_NAME_VERSION + "\" sender=\"" + APP_NAME_VERSION + "\" timestamp=\"" + SecsToKMLString(SecsNow()) + "\" />");

	broker.SendAlarm(s);
	cout << SecsToString(SecsNow()) << ": QML HEARTBEAT BROKER: " + broker.Hostname() << endl;
}

void binder_t :: SendBrokerAlarm( quake_t & q, const vector<station_t *> & stations, broker_t & broker, bool logToLog, bool logToMail )
{
	if (! ((realtime || param_alarm_during_simulation) && !broker.Hostname().empty()) )
		return;

	const string PUBID = "smi:org.presto";
	const string QUAKEID = (event_name.empty() ? ("realtime_" + SecsToString(q.origin.time).substr(0,4+1+2+1+2)) : event_name) + "_" + ToString(q.id);

	const string ew_id = PUBID + "/ew/" + QUAKEID;
	const string ev_id = PUBID + "/ev/" + QUAKEID;
	const string or_id = PUBID + "/or/" + QUAKEID;
	const string ma_id = PUBID + "/ma/" + QUAKEID;

	stringstream ss;

	// Begin QuakeML

	ss << "<?xml version=\"1.0\" ?>\n";
	ss << "<q:quakeml xmlns='http://quakeml.org/xmlns/bed-rt/1.2' xmlns:q='http://quakeml.org/xmlns/quakeml-rt/1.2'>\n";

	// Event Parameters (open)

	ss << "<eventParameters publicID='" << ew_id << "'>\n";

	// Event

	ss << "<event publicID='"       << ev_id << "'><type>earthquake</type>\n";	// if type is "not existing", the event is canceled
	ss << "<preferredOriginID>"     << or_id << "</preferredOriginID>\n";
	ss << "<preferredMagnitudeID>"  << ma_id << "</preferredMagnitudeID>\n";
	ss << "</event>\n";

	// Origin (open)

	ss << "<origin publicID='" << or_id << "'>\n";

	// Origin Time

	ss << "\t<time><value>" << SecsToKMLString(q.origin.time) << "</value></time>\n";

	// Location

	float KM_PER_LON	=	rtloc.LonLatDep_Distance_km(q.origin.lon,q.origin.lat,0, q.origin.lon+1,q.origin.lat,0);
	float KM_PER_LAT	=	rtloc.LonLatDep_Distance_km(q.origin.lon,q.origin.lat,0, q.origin.lon,q.origin.lat+1,0);

	ss << "\t<longitude><value>"	<< q.origin.lon						<<	"</value>";
	ss << "<uncertainty>"			<< sqrt(q.origin.cov_xx)/KM_PER_LON	<<	"</uncertainty></longitude>\n";

	ss << "\t<latitude><value>"		<< q.origin.lat						<<	"</value>";
	ss << "<uncertainty>"			<< sqrt(q.origin.cov_yy)/KM_PER_LON	<<	"</uncertainty></latitude>\n";

	ss << "\t<depth><value>"		<< q.origin.dep * 1000				<<	"</value>";
	ss << "<uncertainty>"			<< sqrt(q.origin.cov_zz) * 1000		<<	"</uncertainty></depth>\n";

	// Origin (close)

	ss << "</origin>\n";

	// Magnitude
	//FIXME 1 decimal digit

	ss << "<magnitude publicID='" << ma_id << "'><mag>";
	ss << "<value>"				<< q.mag				<<	"</value>";
	ss << "<lowerUncertainty>"	<< q.mag - q.mag_min		<<	"</lowerUncertainty>";
	ss << "<upperUncertainty>"	<< q.mag_max - q.mag		<<	"</upperUncertainty>";
	ss << "</mag></magnitude>\n";

	// Picks

	for (binder_picks_set_t::const_iterator p = q.picks.begin(); p != q.picks.end(); ++p)
	{
		string pi_id = PUBID + "/pi/" + ToString(RoundToInt(p->pick.t)) + "." + p->heli->station->name;
		ss << "<pick publicID='" << pi_id << "'>";
		ss << "<time><value>" << SecsToKMLString(p->pick.t) << "</value></time>";
		ss << "<waveformID networkCode='" << p->heli->station->net << "' stationCode='" << p->heli->station->name << "' />";
		ss << "</pick>\n";
	}

	// Event Parameters (close)

	ss << "</eventParameters>\n";

	// End QuakeML

	ss << "</q:quakeml>";

	// Send and log message

	broker.SendAlarm(ss.str());

	if (logToLog || logToMail)
	{
		string qml = SecsToString(SecsNow()) + ": QML Q: " + ToString(q.id) + " BROKER: " + broker.Hostname();

		if (logToLog)
			cout << qml << ":\n" << ss.str() << endl;

		if (logToMail)
			q.mail_log += qml + ss.str() + "\n";
	}
}

/*******************************************************************************

	binder_t - Run

*******************************************************************************/

void binder_t :: Run(vector<station_t *> & stations)
{
	// Get new picks from all the stations (ordered by pick time)
	binder_picks_set_t bpicks;
	for (vector<station_t *>::const_iterator s = stations.begin(); s != stations.end(); s++)
	{
		heli_t *heli = (*s)->z;

		if (!heli)
			continue;

		picks_set_t hpicks;
		heli->GetNewPicks(hpicks);

		for (picks_set_t :: const_iterator hp = hpicks.begin(); hp != hpicks.end(); hp++)
			bpicks.insert( binder_pick_t(heli, *hp) );
	}

	// Add new picks (in pick time order) and get a set of quakes that had new picks linked to them.
	// (note: later picks may still be processed before earlier picks due to latency/concurrency)
	set<int> quake_ids;
	for (binder_picks_set_t :: const_iterator bp = bpicks.begin(); bp != bpicks.end(); bp++)
	{
		int res_quake_id;
		if ( AddAndLinkPick( *bp, &res_quake_id ) )
			quake_ids.insert( res_quake_id );
	}

	// Reprocess quakes

	secs_t secs_now = SecsNow();
	bool isAlarm = false;

	for (vector<quake_t>::iterator q = quakes.begin(); q != quakes.end(); q++)
	{
		bool quake_has_new_picks = (quake_ids.find(q->id) != quake_ids.end());
		bool quake_alive = (secs_now - q->secs_creation) <= param_binder_quakes_life;

		if (quake_alive)
			isAlarm = true;

		// Recalc location and magnitude during the life of the quake, but also whenever there are new picks.
		// But make sure the new data is not too late, i.e. the real-time, not-lagging streams are still
		// in their buffers, otherwise the magnitude calculation would be performed on the new data only!
		if ( (quake_alive || quake_has_new_picks) && (secs_now - q->secs_creation < 60*2) )
		{
			bool hasNewLoc = false, hasNewMag = false;

			// Location: calc on new picks or if enough time has passed since the last estimate

			if (	quake_has_new_picks ||
					( param_locate_use_non_triggering_stations && ((secs_now - q->secs_located) >= param_locate_period) )	)
				hasNewLoc = CalcQuakeLoc( *q );

			// Magnitude: calc continuously (new waveform data may be available)

			if (q->secs_located)
				hasNewMag = CalcQuakeMag( *q );

			// Log QUAKE message: when mag is available, on loc or mag changes

			if ((q->mag != -1) && (hasNewLoc || hasNewMag))
			{
				bool logToMail	=	!q->mail_sent;
				LogQuake( *q, logToMail );
			}

			bool mustSendAlarm = (q->mag != -1) && ( hasNewLoc || hasNewMag || ((secs_now - q->secs_alarm_sent) >= param_alarm_max_period) );

			// Targets PGA, PGV and Alarms: when mag is available, on loc or mag changes

			if (mustSendAlarm)
			{
				q->secs_alarm_sent = SecsNow();

				vector<station_t *> empty;

				bool logToLog	=	true;
				bool logToMail	=	!q->mail_sent;
				SendTargetsAlarm( *q, empty, targets, logToLog, logToMail );
			}

			// Stations PGA, PGV: when mag is available, on loc or mag changes

			if (mustSendAlarm)
			{
				targets_t empty;

				bool logToLog	=	true;
				bool logToMail	=	false;
				SendTargetsAlarm( *q, stations, empty, logToLog, logToMail );
			}

			// Broker alarm: when mag is available, on loc or mag changes

			if ( (q->mag != -1) && ( hasNewLoc || hasNewMag ) )
			{
				bool logToLog	=	true;
				bool logToMail	=	false;
				SendBrokerAlarm( *q, stations, broker, logToLog, logToMail );
			}

			// KML: update on loc, mag changes

			if (hasNewLoc || hasNewMag)
				q->estimates.push_back( quake_estimate_t( q->picks, q->origin, q->mag, q->mag_min, q->mag_max ) );
		}

		// Last step: save a screenshot, a KML animation and send the Mail

		if ( (!q->mail_sent) && q->secs_located && (q->mag != -1) && !quake_alive )
		{
			string fileprefix = q->filename();

			// Screenshot

			cout << SecsToString(SecsNow()) << ": Save Screenshot..." << endl;
			// SaveScreenShot( fileprefix, SCR_OVERWRITE, PNG_COMPRESSION_BEST );

			// KML

			SaveQuakeKML( fileprefix, *q, stations );

			// Finish Mail log with the final estimate of PGx (at stations only)
			{
				targets_t empty;

				bool logToLog	=	false;
				bool logToMail	=	true;
				SendTargetsAlarm( *q, stations, empty, logToLog, logToMail );
			}

			// Save Mail log to file

			string maillogfilename = fileprefix + ".mail.log";
			ofstream maillogfile( maillogfilename.c_str(), ios::out | ios::trunc );
			if (maillogfile)
			{
				maillogfile << q->mail_log;
				maillogfile.flush();
				maillogfile.close();
			}

			// Send Mail

			MailQuake( *q );
		}
	}

	// Sound

	if (config_sound)
	{
		// Alarm sound while a quake is being processed

		if (isAlarm)
		{
			if (!Sound_Alarm()->IsPlaying())
				Sound_Alarm()->Play();
		}
		else
		{
			Sound_Alarm()->Stop();
		}

		// Start shaking sound when first target has been reached by the P-waves of the latest earthquake

		if (isAlarm && !targets.empty() && !quakes.empty())
		{
			targets_t::iterator t = targets.begin();
			quake_t q = quakes.back();

			float P_ttime	=	t->CalcTravelTime('P', q.origin);
			float remaining	=	float( q.origin.time + secs_t(P_ttime) - SecsNow() );

			if ( remaining <= 1.0f && !Sound_Shaking()->IsPlaying() )
				Sound_Shaking()->Play();
		}
	}

	// Non critical stuff (suspend while processing a quake)

	if (!isAlarm)
	{
		// Heartbeat

		secs_now = SecsNow();
		if ( (realtime || param_alarm_during_simulation) && param_alarm_heartbeat_secs && ((secs_now - secs_heartbeat_sent) >= param_alarm_heartbeat_secs) )
		{
			string hertbeat = SecsToString(secs_now) + ": HEARTBEAT";

			cout << hertbeat << endl;

			secs_heartbeat_sent = SecsNow();

			targets.SendAlarm(hertbeat);
			SendBrokerHeartBeat( broker );
		}

		// Log the statistics of latencies and reset them

		secs_now = SecsNow();
		if (param_latency_log_period_secs && (secs_now - secs_latencies_logged) > param_latency_log_period_secs)
		{
			secs_latencies_logged = secs_now;
			for (vector<station_t *> :: const_iterator s = stations.begin(); s != stations.end(); s++)
			{
				if ((*s)->z != NULL)
				{
					(*s)->z->LogMeanLatencies();
					(*s)->z->ResetMeanLatencies();
				}
			}
		}
	}
}


// Preload media files
void PreLoad_Binder()
{
	if (config_sound)
	{
		Sound_Alarm();
		Sound_Shaking();
	}
}
