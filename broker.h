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

	broker_t - Creates a thread to deliver messages to a remote STOMP broker

*******************************************************************************/

#ifndef BROKER_H_DEF
#define BROKER_H_DEF

//#include <unordered_map>	// requires c++11
#include <map>
#include <deque>
#include "SDL_net.h"

#include "global.h"

// typedef unordered_map<string,string> stomp_headers_t;	// requires c++11
typedef map<string,string> stomp_headers_t;

enum net_err_t { ERR_NONE, ERR_SEND, ERR_RECV, ERR_STOMP };

enum conn_state_t { IDLE, CLOSE, RESOLVE, OPEN, WAIT_CONNECTED, CONNECTED };

class broker_state_t
{
private:
	conn_state_t state;
	secs_t secs_change;
public:
	broker_state_t()				{ state = IDLE;		secs_change = -1; }
	void Set(conn_state_t _state)	{ state = _state;	secs_change = SecsNow(); }
	conn_state_t Get()	{ return state;}
	secs_t SecsFromChange()	{ return SecsNow() - secs_change; }
};

class broker_t
{
private:

	broker_state_t state;
	typedef deque<string> messages_t;

	SDL_Thread	*thread;
	SDL_mutex	*mutex;
	bool exitThread;

	static int Update_ThreadFunc(void *broker_ptr);
	void CreateThread();
	void DestroyThread();
	void Update();

	char *recv_buf;
	stringstream ss;
	void ResetReceive();
	bool GetLine(stringstream & ss, string & s, char delim = '\n');

	messages_t messages;
	void Lock()
	{
		if (mutex)
			SDL_LockMutex(mutex);
	}
	void Unlock()
	{
		if (mutex)
			SDL_UnlockMutex(mutex);
	}

	string hostname, dest, user, pass;
	Uint16 port;

	void SanityCheck(const string & filename);
	void Init();

	// TCP
	TCPsocket sock;
	SDLNet_SocketSet sockset;
	IPaddress ipaddress;

	// STOMP
	void LogFrame(const string & command, const stomp_headers_t & headers, const string & body);
	string EncodeFrame(const string & command, const stomp_headers_t & headers, const string & body) const;
	net_err_t ParseFrame();

	net_err_t SendFrame(const string & command, const stomp_headers_t & headers, const string & body);
	net_err_t SendConnect();
	net_err_t SendMessage(const string & body);
	net_err_t Receive();

public:

	broker_t();
	~broker_t();
	void Load(const string & filename);
	const string Hostname() const { return hostname; }
	secs_t SecsFromConnection();
	void Start();
	void Stop();

	void SendAlarm(const string & s);	// called by the main thread
};

extern broker_t broker;

#endif
