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

	Targets are vulnerable structures (without sensors) recipients of alarm messages.
	The target_t class stores a place_t position and an IP/port to send alarms to.

*******************************************************************************/

#ifndef TARGET_H_DEF
#define TARGET_H_DEF

#include <string>
#include <vector>
#include "SDL_net.h"

#include "global.h"

#include "place.h"

class target_t : public gridplace_t
{
public:
	string fullname;
	bool shown;
	string hostname;
	Uint16 port;
	IPaddress ipaddress;

	target_t(const string & _fullname, const string & _name, bool _shown, float _lon, float _lat, float _dep, const string & _hostname, Uint16 _port);
};

class targets_t
{
private:

	const static int ALARM_CHAN;
	const static int ALARM_DATA_SIZE;
	const static int ALARM_PACK_SIZE;

	typedef vector<target_t> target_container_t;
	target_container_t recipients;

	// UDP
	UDPsocket sock;
	UDPpacket *pack;

public:

	targets_t();
	~targets_t();

	typedef target_container_t::iterator iterator;

	void Load(const string & filename);

	target_container_t::iterator begin()
	{
		return recipients.begin();
	}
	target_container_t::iterator end()
	{
		return recipients.end();
	}
	bool empty() const
	{
		return recipients.empty();
	}

	void SendAlarm(const string & s, const IPaddress * const ipaddress = NULL);
};

extern targets_t targets;

#endif
