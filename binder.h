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

#ifndef BINDER_H_DEF
#define BINDER_H_DEF

#include "global.h"

#include "broker.h"
#include "heli.h"
#include "quake.h"
#include "target.h"

class binder_t
{
private:

	binder_picks_set_t picks;
	vector<quake_t> quakes;

	void PurgeOldPicks();

	bool CheckPickAssoc(const binder_picks_set_t & picks, const binder_pick_t & new_pick, secs_t secs) const;
	void LinkAssocPicks(quake_t & q, secs_t secs);
	void FindCoincPicks(const binder_pick_t & new_pick, const quake_t *q, vector<binder_picks_set_t::iterator> & best_picks_iter);
	bool AddAndLinkPick(const binder_pick_t & new_pick, int *res_quake_id);

	bool CalcAndAddPickMagnitude( magfilt_t magfilt, magtype_t magtype, binder_pick_t & bp, const origin_t & origin, stringstream & rtmag_log );
	bool CalcAndAddAllPickMagnitudes( magfilt_t magfilt, binder_pick_t & bp, const origin_t & origin, stringstream & rtmag_log );

	void PurgeOldQuakes();

	bool CalcQuakeLoc( quake_t & q );
	bool CalcQuakeMag( quake_t & q );

	int quake_id;
	void LoadQuakeId();
	void SaveQuakeId();

	void LogQuake( quake_t & q, bool logToMail );
	void SendTargetsAlarm( quake_t & q, const vector<station_t *> & stations, targets_t & targets, bool logToLog, bool logToMail );
	void SendBrokerAlarm( quake_t & q, const vector<station_t *> & stations, broker_t & broker, bool logToLog, bool logToMail );
	void SendBrokerHeartBeat( broker_t & broker );
	void SaveQuakeKML( const string & fileprefix, quake_t & q, const vector<station_t *> & stations );
	void MailQuake( quake_t & q );

	static int MailQuake_ThreadFunc(void *quake_copy_ptr);

	secs_t secs_heartbeat_sent;
	secs_t secs_latencies_logged;

public:

	// For e.g. drawing
	const vector<quake_t>    & Quakes()			const { return quakes; }
	const quake_t            & Quake(int id)	const { return quakes[id - quakes[0].id]	;	}
	const binder_picks_set_t & Picks()			const { return picks;  }

	binder_t();

	void Init();

	void Reset();

	void Run(vector<station_t *> & stations);

	secs_t SecsFromLastQuake();
	secs_t SecsFromLastAlarm();
	secs_t SecsFromLastHeartbeat();
	secs_t SecsFromBrokerConnection();

	timeseries_t magheli;

	void Draw();
};

extern binder_t binder;

void PreLoad_Binder();

#endif
