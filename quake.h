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

	quake_t - earthquake source parameters and related information:
	          e.g. ID, linked picks, PRESTo estimates and alarms data

*******************************************************************************/

#ifndef QUAKE_H_DEF
#define QUAKE_H_DEF

#include <string>
#include <limits>

#include "global.h"

#include "heli.h"
#include "origin.h"

class binder_pick_t
{

public:

	heli_t *heli;
	pick_t pick;

	binder_pick_t(heli_t * _heli, pick_t _pick) : heli(_heli), pick(_pick) {}

	// a == b <-> !(a < b) && !(b < a)
	bool operator < (const binder_pick_t & rhs) const
	{
		if (!(pick < rhs.pick) && !(rhs.pick < pick))	// equal pick times
			return heli < rhs.heli;

		return (pick < rhs.pick);
	}

	bool CheckApparentVel( const binder_pick_t & bp ) const;
};

typedef set<binder_pick_t> binder_picks_set_t;

class quake_estimate_t
{
public:
	secs_t secs_estimate;
	binder_picks_set_t picks;
	origin_t origin;
	float mag, mag_min, mag_max;

	quake_estimate_t( const binder_picks_set_t & _picks, const origin_t & _origin, float _mag, float _mag_min, float _mag_max );
};

class quake_t
{
public:

	secs_t secs_creation, secs_located, secs_alarm_sent;
	int alarm_seq;
	int id;
	binder_picks_set_t picks;
	origin_t origin;
	float mag_s, mag_p;
	float mag, mag_min, mag_max;

	string mail_log;
	bool mail_sent;

	vector<quake_estimate_t> estimates;

	quake_t(int _id)
		:	secs_creation(SecsNow()), secs_located(0), secs_alarm_sent(0),
			alarm_seq(0),
			id(_id),
			origin(0,0,0),
			mag_s(-1), mag_p(-1),
			mag(-1), mag_min(-1), mag_max(-1),
			mail_log(""), mail_sent(false)
	{}

	void LinkPick(const binder_pick_t & new_pick);

	string filename() const;

	friend ostream& operator<< (ostream& os, const quake_t& q);
};

string MagToString(const float mag);

#endif
