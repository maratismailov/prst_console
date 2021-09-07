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

// Tested with ActiveMQ 5.10.0, Apollo 1.7 and HornetQ 2.4.0 (all on Windows 7)

/*******************************************************************************

	broker_t - Creates a thread to deliver messages to a remote STOMP broker

*******************************************************************************/

#include <iomanip>
#include <fstream>

#include "broker.h"

#include "config.h"

// Size of receive buffer in bytes
#define RECV_BUF_SIZE (1024*64)

// keep at most this many unsent messages
#define MESSAGE_LIST_SIZE		(100)
	
// Seconds to wait for a CONNECTED message
#define WAIT_CONNECTED_TIMEOUT	(5)

// Seconds to wait after receiving an ERROR message
#define STOMP_ERROR_DELAY		(5)

// Seconds before retrying on network error
#define RESOLVE_DELAY			(3)
#define OPEN_DELAY				(3)

broker_t broker;

/*******************************************************************************

	broker_t - STOMP frame

*******************************************************************************/

void broker_t :: LogFrame(const string & command, const stomp_headers_t & headers, const string & body)
{
	cerr << SecsToString(SecsNow()) << ": Received STOMP frame from " << hostname << ": " << command << " ";
	for (stomp_headers_t::const_iterator h = headers.begin(); h != headers.end(); ++h)
		cerr << h->first  << ":" << h->second << " ";
	cerr << body << endl;
}

string broker_t :: EncodeFrame(const string & command, const stomp_headers_t & headers, const string & body) const
{
	string s;

	s += command + "\n";

	for (stomp_headers_t::const_iterator h = headers.begin(); h != headers.end(); ++h)
		s += h->first + ":" + h->second + "\n";

	size_t body_size = body.size();
	if (body_size)
		s += "content-length:" + ToString(body_size) + "\n";

	s += "\n";

	s += body;

	s += '\0';

	return s;
}

net_err_t broker_t :: SendFrame(const string & command, const stomp_headers_t & headers, const string & body)
{
	string msg = EncodeFrame(command, headers, body);

	int len = int(msg.length());
	int result = SDLNet_TCP_Send(sock, msg.c_str(), len);
	if (result != len)
	{
		cerr << SecsToString(SecsNow()) << ": Can't send STOMP frame: " << SDLNet_GetError() << endl;
		return ERR_SEND;
	}
	return ERR_NONE;
}

net_err_t broker_t :: SendConnect()
{
	cerr << SecsToString(SecsNow()) << ": Sending STOMP CONNECT" << endl;
	stomp_headers_t headers;
	headers["login"] = user;
	headers["passcode"] = pass;
	return SendFrame("CONNECT", headers, "");
}
net_err_t broker_t :: SendMessage(const string & body)
{
	cerr << SecsToString(SecsNow()) << ": Sending STOMP SEND" << endl;
	stomp_headers_t headers;
	headers["destination"] = dest;
	return SendFrame("SEND", headers, body);
}

net_err_t broker_t :: Receive()
{
	if (SDLNet_CheckSockets(sockset, 0) != 1)
		return ERR_NONE;

	int len = SDLNet_TCP_Recv(sock, recv_buf, RECV_BUF_SIZE);
	if (len <= 0)
		return ERR_RECV;

	ss.clear();
	ss.seekp(0, ios::end);
	ss.write(recv_buf, len);

	return ParseFrame();
}

void broker_t :: ResetReceive()
{
	ss.str("");
	ss.clear();
}

// return true if delim is found
bool broker_t :: GetLine(stringstream & ss, string & s, char delim)
{
	s = "";
	ss.clear();
	while (true)
	{
		int c = ss.get();
		if (c == EOF)
			return false;
		if (c == delim)
			return true;
		s += c;
	}
	return false;
}

net_err_t broker_t :: ParseFrame()
{
	string s, command, body;
	stomp_headers_t headers;
	size_t pos;

	ss.clear();
	ss.seekg(0, ios::beg);

	// Parse command (but skip newlines from previous frame)
	do
	{
		if (!GetLine(ss, s))
			return ERR_NONE;
	}
	while (s.empty());
	command = s;

	// Parse headers
	while (true)
	{
		if (!GetLine(ss, s))
			return ERR_NONE;

		if (s.empty())
			break;

		pos = s.find(':');
		if (pos == string::npos)
			return ERR_NONE;

		headers[s.substr(0, pos)] = s.substr(pos+1, string::npos);
	}

	// Parse body
	if (!GetLine(ss, s, '\0'))
		return ERR_NONE;
	pos = (size_t)ss.tellg();
	body = s;

	// Remove parsed frame
	ss.str(ss.str().substr(pos));

	// Log frame
	LogFrame(command, headers, body);

	// Act based on the frame contents
	if (command == "CONNECTED")
	{
		state.Set(CONNECTED);
	}
	else if (command == "ERROR")
	{
		return ERR_STOMP;
	}

	return ERR_NONE;
}

/*******************************************************************************

	broker_t - Thread handling

*******************************************************************************/

// FIXME state access is not thread safe. This function is only used to display the connection icon though
secs_t broker_t :: SecsFromConnection()
{
	if (!Hostname().empty() && state.Get() == CONNECTED)
		return state.SecsFromChange();

	return 0;
}

void broker_t :: Update()
{
	while (!exitThread)
	{
		switch (state.Get())
		{
			case CLOSE:
			{
				if (sock)
				{
					SDLNet_TCP_DelSocket(sockset, sock);
					SDLNet_TCP_Close(sock);
					sock = NULL;
				}

				ResetReceive();
				state.Set(RESOLVE);
			}
			break;

			case RESOLVE:
			{
				int err = SDLNet_ResolveHost(&ipaddress, hostname.c_str(), port);
				if (err)
				{
					cerr << SecsToString(SecsNow()) << ": Can't resolve hostname \"" << hostname << ":" << port << "\"" << endl;
					SDL_Delay(RESOLVE_DELAY * 1000);
				}
				else
				{
					state.Set(OPEN);
				}
			}
			break;

			case OPEN:
			{
				if (sock)
				{
					state.Set(CLOSE);
					break;
				}

				sock = SDLNet_TCP_Open(&ipaddress);
				if (!sock)
				{
					cerr << SecsToString(SecsNow()) << ": Can't connect to host \"" << hostname << ":" << port << "\": " << SDLNet_GetError() << endl;
					SDL_Delay(OPEN_DELAY * 1000);
				}
				else
				{
					if (SDLNet_TCP_AddSocket(sockset, sock) != 1)
					{
						cerr << SecsToString(SecsNow()) << ": Can't add socket to socket set: " << SDLNet_GetError() << endl;
						state.Set(CLOSE);
					}
					else
					{
						if (SendConnect() == ERR_NONE)
							state.Set(WAIT_CONNECTED);
						else
							state.Set(CLOSE);
					}
				}
			}
			break;

			case WAIT_CONNECTED:
			{
				net_err_t err = Receive();
				if (err != ERR_NONE)
				{
					if (err == ERR_STOMP)
						SDL_Delay(STOMP_ERROR_DELAY * 1000);
					state.Set(CLOSE);
					break;
				}

				if (state.SecsFromChange() >= WAIT_CONNECTED_TIMEOUT)
					state.Set(CLOSE);
			}
			break;

			case CONNECTED:
			{
				net_err_t err = Receive();
				if (err != ERR_NONE)
				{
					if (err == ERR_STOMP)
						SDL_Delay(STOMP_ERROR_DELAY * 1000);
					state.Set(CLOSE);
					break;
				}

				bool has_message = false;
				string message;

				Lock();
				if (!messages.empty())
				{
					has_message = true;

					message = messages.front();
					messages.pop_front();

					if (messages.size() > MESSAGE_LIST_SIZE)
						messages.resize(MESSAGE_LIST_SIZE);
				}
				Unlock();

				if (!has_message)
				{
					SDL_Delay(1000/100);
					break;
				}

				if (SendMessage(message) != ERR_NONE)
					state.Set(CLOSE);
			}
			break;
		}
	}
}

int broker_t :: Update_ThreadFunc(void *broker_ptr)
{
	broker_t *broker = (broker_t *)broker_ptr;
	broker->Update();
	return 0;
}

void broker_t :: CreateThread()
{
	mutex = SDL_CreateMutex();
	if (mutex == NULL)
		Fatal_Error("Can't create broker mutex");

	thread = SDL_CreateThread( Update_ThreadFunc, "broker", this );
	if (thread == NULL)
		Fatal_Error("Can't create broker thread");
}

void broker_t :: DestroyThread()
{
	if (thread != NULL)
	{
		exitThread = true;
		SDL_WaitThread(thread,NULL);
		exitThread = false;

		thread = NULL;
	}

	if (mutex != NULL)
	{
		SDL_DestroyMutex(mutex);
		mutex = NULL;
	}
}

void broker_t :: Start()
{
	Stop();
	CreateThread();
}

void broker_t :: Stop()
{
	DestroyThread();
}

broker_t :: broker_t()
{
	thread = NULL;
	mutex = NULL;
	exitThread = false;

	hostname = dest = user = pass = "";
	port = 0;

	sock = NULL;
	sockset = NULL;

	recv_buf = NULL;
}

broker_t :: ~broker_t()
{
	Stop();

	if (sockset)
	{
		SDLNet_FreeSocketSet(sockset);
		sockset = NULL;
	}

	delete [] recv_buf;
}

void broker_t :: SendAlarm(const string & s)
{
	Lock();
	messages.push_back(s);
	Unlock();
}

void broker_t :: SanityCheck(const string & filename)
{
	if (port < 0 || port > 65535)
		Fatal_Error("Invalid port \"" + ToString(port) + "\" in \"" + filename + "\"");

	if (dest.empty())
		Fatal_Error("Empty destination in \"" + filename + "\"");

	if (user.empty() && !pass.empty())
		Fatal_Error("Empty user with password in \"" + filename + "\"");
}

void broker_t :: Init()
{
	recv_buf = new char[RECV_BUF_SIZE];

	sockset = SDLNet_AllocSocketSet(1);
	if (!sockset)
		Fatal_Error("SDL_net - Can't allocate a socket set");

	state.Set(CLOSE);
}

void broker_t :: Load(const string & filename)
{
	const std::streamsize w1 = 30, w2 = 6, w3 = 30, w4 = 10, w5 = 8;

	cout << endl;
	cout << "==================================================================================================" << endl;
	cout << "    Broker (" << filename << ")" << endl;
	cout << endl;
	cout <<	setw(w1)	<<	"Host"		<<	" | "	<<
			setw(w2)	<<	"Port"		<<	" | "	<<
			setw(w3)	<<	"Dest"		<<	" | "	<<
			setw(w4)	<<	"User"		<<	" | "	<<
			setw(w5)	<<	"Pass"		<<	endl;
	cout << "==================================================================================================" << endl;

	ifstream f(filename.c_str());
	if (f)
	{
		string _hostname, _dest, _user, _pass;
		int _port;

		SkipComments(f);

		f >> _hostname >> _port;
		if (f.fail())
		{
			if ( !(_hostname.empty() && f.eof()) )
				Fatal_Error("Parsing broker \"" + _hostname + "\" in file \"" + filename + "\". Use this format: Host Port \"Dest\" \"User\" \"Pass\"");
		}
		else
		{
			_dest	=	ReadQuotedString(f, "Parsing destination in file \"" + filename + "\"");
			_user	=	ReadQuotedString(f, "Parsing user in file \"" + filename + "\"");
			_pass	=	ReadQuotedString(f, "Parsing pass in file \"" + filename + "\"");

			cout <<	setw(w1)	<<	_hostname	<<	" | "	<<
					setw(w2)	<<	_port		<<	" | "	<<
					setw(w3)	<<	_dest		<<	" | "	<<
					setw(w4)	<<	_user		<<	" | "	<<
					setw(w5)	<<	"******"	<<	endl;

			hostname	=	_hostname;
			port		=	_port;
			dest		=	_dest;
			user		=	_user;
			pass		=	_pass;

			SanityCheck(filename);

			Init();
		}
	}

	if (hostname.empty())
		cout <<	"No Brokers" << endl;

	cout << "==================================================================================================" << endl;

	if (hostname == "0.0.0.0")
		hostname = "";
}
