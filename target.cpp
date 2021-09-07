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

	Targets are vulnerable buildings (without sensors) recipients of alarm messages.
	The target_t class stores a place_t position and an IP/port to send alarms to.

*******************************************************************************/

#include <string>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <algorithm>

#include "target.h"

#include "config.h"
#include "rtloc.h"

targets_t targets;
const int targets_t::ALARM_CHAN = 0;
const int targets_t::ALARM_PACK_SIZE = 1500;	// 1500 byte MTU
const int targets_t::ALARM_DATA_SIZE = targets_t::ALARM_PACK_SIZE - 8 - 20;	// 8 byte UDP header + 20 byte IP header

target_t :: target_t(const string & _fullname, const string & _name, bool _shown, float _lon, float _lat, float _dep, const string & _hostname, Uint16 _port)
:	gridplace_t(_name, _lon, _lat, _dep), fullname(_fullname), shown(_shown), hostname(_hostname), port(_port)
{
	int err = SDLNet_ResolveHost(&ipaddress, hostname.c_str(), port);
	if (err)
		Fatal_Error("SDL_net - Can't resolve IP address \"" + hostname + ":" + ToString(port) + "\": " + string(SDLNet_GetError()));

	// Consider invalid addresses (0.0.0.0, 255.255.255.255) as placeholders for targets with no IP address
	if (ipaddress.host == INADDR_ANY || ipaddress.host == INADDR_NONE || ipaddress.host == INADDR_BROADCAST)
	{
		ipaddress.host = 0;
		ipaddress.port = 0;
	}
}

targets_t :: targets_t()
:	sock(NULL), pack(NULL)
{
}

targets_t :: ~targets_t()
{
	SDLNet_UDP_Close(sock);
	sock = NULL;

	SDLNet_FreePacket(pack);
	pack = NULL;
}

void targets_t :: SendAlarm(const string & s, const IPaddress * const ipaddress)
{
	if (sock == NULL)
		return;

	if (ipaddress != NULL && ipaddress->host == 0)
		return;

	int len = min(int(s.length()), ALARM_DATA_SIZE);

	pack->len = len;
	memcpy(pack->data, s.data(), len);

	int numsent;
	if (ipaddress != NULL)
	{
		// Send to a single target
		pack->address.host = ipaddress->host;
		pack->address.port = ipaddress->port;
		numsent = SDLNet_UDP_Send(sock, -1, pack);
	}
	else
	{
		// Send to all targets
		numsent = SDLNet_UDP_Send(sock, ALARM_CHAN, pack);
	}
}

void targets_t :: Load(const string & filename)
{
	const std::streamsize w1 = 20, w2 = 10, w3 = 5, w4 = 10, w5 = 10, w6 = 4, w7 = 4*3+3, w8 = 4;

	cout << endl;
	cout << "==================================================================================================" << endl;
	cout << "    Targets (" << filename << ")" << endl;
	cout << endl;
	cout <<	setw(w1)	<<	"FullName"			<<	" | "	<<
			setw(w2)	<<	"Name"				<<	" | "	<<
			setw(w3)	<<	"Shown"				<<	" | "	<<
			setw(w4)	<<	"Lon"				<<	" | "	<<
			setw(w5)	<<	"Lat"				<<	" | "	<<
			setw(w6)	<<	"Elev"				<<	" | "	<<
			setw(w7)	<<	"IP"				<<	" | "	<<
			setw(w8)	<<	"Port"				<<	endl;
	cout << "==================================================================================================" << endl;

	ifstream f(filename.c_str());
	if (f)
	{
		for(;;)
		{
			string fullname, name;
			int shown;
			float lon, lat, dep;
			string hostname;
			int port;

			SkipComments(f);

			fullname = ReadQuotedString(f, "Parsing target full name in file \"" + filename + "\"");
			f >> name >> shown >> hostname >> port;

			if (f.fail())
			{
				if (fullname.empty() && f.eof())
					break;

				Fatal_Error("Parsing target \"" + fullname + "\" in file \"" + filename + "\". Use this format: \"Full Name\"  Name  Shown  IP  Port");
			}

			if (fullname.empty())
				Fatal_Error("Empty target full name (or without double quotes) in \"" + filename + "\"");

			if (port < 0 || port > 65535)
				Fatal_Error("Invalid UDP port \"" + ToString(port) + "\" in \"" + filename + "\"");

			if (port == 0 && hostname != "0.0.0.0")
				Fatal_Error("Invalid UDP port \"0\" (can only be used with IP address 0.0.0.0) in \"" + filename + "\"");

			// Get target coordinates from RTLoc grids
			rtloc.GetStationLonLatDep(name, &lon, &lat, &dep);

			recipients.push_back( target_t(fullname, name, shown ? true : false, lon, lat, dep, hostname, (Uint16)port) );

			Uint32 ip32		=	recipients.back().ipaddress.host;
			string ipstring	=	ToString((ip32 >>  0) & 0xff) + "." +
								ToString((ip32 >>  8) & 0xff) + "." +
								ToString((ip32 >> 16) & 0xff) + "." +
								ToString((ip32 >> 24) & 0xff);

			cout <<	setw(w1)	<<	fullname	<<	" | "	<<
					setw(w2)	<<	name		<<	" | "	<<
					setw(w3)	<<	shown		<<	" | "	<<
					setw(w4)	<<	lon			<<	" | "	<<
					setw(w5)	<<	lat			<<	" | "	<<
					setw(w6)	<<	-dep * 1000	<<	" | "	<<
					setw(w7)	<<	ipstring	<<	" | "	<<
					setw(w8)	<<	port		<<	endl;
		}
	}
	else
		cout <<	"No Targets" << endl;

	cout << "==================================================================================================" << endl;


	// UDP Alarm

	if (realtime || param_alarm_during_simulation)
	{
		// Bind targets

		for (targets_t::iterator t = begin(); t != end(); t++)
		{
			if (t->ipaddress.host == 0)
				continue;

			// Open socket and allocate packet if we have at least one real IP address
			if (sock == NULL)
			{
				sock = SDLNet_UDP_Open(0);
				if (sock == NULL)
					Fatal_Error("SDL_net - Can't open socket for sending UDP alarms: " + string(SDLNet_GetError()));

				pack = SDLNet_AllocPacket(ALARM_PACK_SIZE);
				if (pack == NULL)
					Fatal_Error("SDL_net - Can't allocate UDP packet for sending alarms: " + string(SDLNet_GetError()));
			}

			int err = SDLNet_UDP_Bind(sock, ALARM_CHAN, &t->ipaddress);
			if (err)
				Fatal_Error("SDL_net - Can't bind IP address \"" + t->hostname + ":" + ToString(t->port) + "\" to socket :" + string(SDLNet_GetError()));
		}
	}
}
