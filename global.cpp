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


	Globally used classes, variables, defines and functions


*******************************************************************************/

#include <cstdio>
#include <iomanip>
#include <algorithm>

#include "global.h"

#include "config.h"
#include "state.h"
#include "version.h"
#include "sound.h"

#include "libslink.h"

const string PATH_FONT		=	"font/";
const string PATH_DATA		=	"data/";
const string PATH_CONFIG	=	"config/";

/*******************************************************************************

	Time

*******************************************************************************/

secs_t globaltime = 0;
secs_t SecsNow()
{
	if (!realtime)
		return simutime_t :: Get();

#if defined(WIN32)

  static const __int64 SECS_BETWEEN_EPOCHS = 11644473600;
  static const __int64 SECS_TO_100NS = 10000000; /* 10^7 */

  __int64 UnixTime;
  SYSTEMTIME SystemTime;
  FILETIME FileTime;
  double depoch;

  GetSystemTime(&SystemTime);
  SystemTimeToFileTime(&SystemTime, &FileTime);

  /* Get the full win32 epoch value, in 100ns */
  UnixTime = ((__int64)FileTime.dwHighDateTime << 32) +
    FileTime.dwLowDateTime;

  /* Convert to the Unix epoch */
  UnixTime -= (SECS_BETWEEN_EPOCHS * SECS_TO_100NS);

  UnixTime /= SECS_TO_100NS; /* now convert to seconds */

  if ( (double)UnixTime != UnixTime )
    {
      sl_log_r (NULL, 2, 0, "slp_dtime(): resulting value is too big for a double value\n");
    }

  depoch = (double) UnixTime + ((double) SystemTime.wMilliseconds / 1000.0);

  return depoch;

#else

  struct timeval tv;

  gettimeofday (&tv, (struct timezone *) 0);
  return ((double) tv.tv_sec + ((double) tv.tv_usec / 1000000.0));

#endif
/* End of slp_dtime() */

}

tm *mygmtime_r(const time_t *timep, tm *result)
{
#if defined(_MSC_VER)
	if ( !gmtime_s(result, timep) )
		return result;
	else
		return NULL;
#else
	return gmtime_r(timep, result);
#endif
}

tm *mylocaltime_r(const time_t *timep, tm *result)
{
#if defined(_MSC_VER)
	if ( !localtime_s(result, timep) )
		return result;
	else
		return NULL;
#else
	return localtime_r(timep, result);
#endif
}

char *myasctime_r(const tm *tm, char *result, size_t size)
{
#if defined(_MSC_VER)
	if ( !asctime_s(result, size, tm) )
		return result;
	else
		return NULL;
#else
	return asctime_r(tm, result);
#endif
}

// Convert secs to a tm struct (GMT). Default to Epoch timestamp if secs is out of range.
static tm *secs2tm(secs_t secs, tm *tm_ptr)
{
	time_t t	=	time_t(secs);
	tm *gmt		=	mygmtime_r(&t, tm_ptr);

	if (gmt != NULL)
		return gmt;

	t = 0;
	return mygmtime_r(&t, tm_ptr);
}

string SecsToString_HHMMSS(secs_t secs)
{
    tm tm_buf;
	char buf[100];	// allocate on the heap for thread safety

	char *cs = myasctime_r(secs2tm(secs, &tm_buf), buf, sizeof(buf));

    string s;
	for (int i = 11; i < 19; i++)
		s.push_back(cs[i]);

	return s;
}

string SecsToString(secs_t secs)
{
	// Round to 2 decimal digits
//	secs = floor(secs*100 + 0.5)/100;
	secs = secs + 0.005;

    tm tm_buf;
	tm *gmt = secs2tm(secs, &tm_buf);

	char s[100];	// allocate on the heap for thread safety

	sprintf (s, "%04d-%02d-%02d %02d:%02d:%02d.%02d",
		gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday,
		gmt->tm_hour, gmt->tm_min, gmt->tm_sec, int( (secs - floor(secs))*100 )
	);

	return string(s);
}

string SecsToKMLString(secs_t secs)
{
    tm tm_buf;
	tm *gmt = secs2tm(secs, &tm_buf);

	char s[100];	// allocate on the heap for thread safety

	sprintf (s, "%04d-%02d-%02dT%02d:%02d:%05.2lfZ",
		gmt->tm_year + 1900, gmt->tm_mon + 1, gmt->tm_mday,
		gmt->tm_hour, gmt->tm_min, gmt->tm_sec + (secs - floor(secs))
	);

	return string(s);
}

string IntervalToString(secs_t secs)
{
	const char *sign = (secs < 0) ? "-" : "";
	secs = abs(secs);

	// 99d23h59m59.9s
	Clamp(secs, 0.0, 3600*24*99.0);

	int isecs	=	int(secs);

	int sec		=	(isecs            ) % 60;
	int min		=	(isecs /       60 ) % 60;
	int hor		=	(isecs /     3600 ) % 24;
	int day		=	(isecs / (3600*24));
	int msec	=	int( (secs - isecs)*10 );

	char s[100];	// allocate on the heap for thread safety

	if (day)		sprintf (s, "%s" "%dd%dh%dm%d.%ds", sign, day, hor, min, sec, msec);
	else if (hor)	sprintf (s, "%s"    "%dh%dm%d.%ds", sign,      hor, min, sec, msec);
	else if (min)	sprintf (s, "%s"       "%dm%d.%ds", sign,           min, sec, msec);
	else			sprintf (s, "%s"          "%d.%ds", sign,                sec, msec);

	return string(s);
}

// Handle ticks wrap-around nicely
ticks_t TicksDifference(ticks_t t_new, ticks_t t_old)
{
	if (t_new >= t_old)
		return t_new - t_old;
	else
		return ((~(ticks_t)0) - t_old) + t_new + 1;
}

ticks_t TicksElapsedSince(ticks_t t_old)
{
	return TicksDifference(SDL_GetTicks(), t_old);
}

//	Simulated time

ticks_t simutime_t :: ticks0, simutime_t :: ticks_pause, simutime_t :: ticks_offset;
secs_t simutime_t :: secs_t0;
bool simutime_t :: isPaused;

simutime_t :: simutime_t()
{
	secs_t0 = 0;
	isPaused = false;
	ticks0 = ticks_pause = ticks_offset = 0;
}

void simutime_t :: Reset()
{
	isPaused = false;
	ticks0 = TicksElapsedSince(0);
	ticks_pause = ticks_offset = 0;
}

void simutime_t :: SetT0(secs_t _secs_t0)
{
	Reset();
	secs_t0 = _secs_t0;
}

bool simutime_t :: GetPaused()
{
	return isPaused;
}

void simutime_t :: SetPaused(bool _paused)
{
	if (_paused == isPaused)
		return;

	if (_paused)
	{
		ticks_pause = TicksElapsedSince(0);
	}
	else
	{
		ticks_offset -= TicksElapsedSince(0) - ticks_pause;
	}

	isPaused = _paused;
}

secs_t simutime_t :: Get()
{
	ticks_t ticks_now = isPaused ? ticks_pause : TicksElapsedSince(0);
	return secs_t0 + secs_t(ticks_now - ticks0 + ticks_offset) / 1000 * secs_t(param_simulation_speed);
}

bool paused = false;

bool GetPaused()
{
	return paused;
}
void SetPaused( bool _paused )
{
	paused = _paused;
	simutime_t :: SetPaused( paused );
	AllSounds_SetPaused(paused);
}

/*******************************************************************************

	Colors

*******************************************************************************/

// output in KML format (hex ABGR)
ostream& operator<< (ostream& os, const color_t & c)
{
	return os	<< hex << setfill('0') <<
				setw(2) << RoundToInt(c.a*255) <<
				setw(2) << RoundToInt(c.b*255) <<
				setw(2) << RoundToInt(c.g*255) <<
				setw(2) << RoundToInt(c.r*255) <<
				dec << setfill(' ');
}

ostream& operator<< (ostream& os, const colors_t & c)
{
	return os	<< hex << setfill('0') <<
				setw(2) << RoundToInt(c.a*255) <<
				setw(2) << RoundToInt(c.b*255) <<
				setw(2) << RoundToInt(c.g*255) <<
				setw(2) << RoundToInt(c.r*255) <<
				dec << setfill(' ');
}

/*******************************************************************************

	Exiting from the application

*******************************************************************************/

void Fatal_Error(const string & errstr)
{
	cout << endl << SecsToString(SecsNow()) << ": FATAL ERROR" << endl;

	cerr << endl << "***ERROR: " << errstr << endl;
	cerr.flush();
	fclose(stderr);

	// SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, string(APP_NAME + " Error").c_str(), errstr.c_str(), NULL);
	printf(errstr.c_str());

	exit(EXIT_FAILURE);
}

void Exit()
{
	cout << endl << SecsToString(SecsNow()) << ": STOPPING" << endl;

	AllSounds_Stop();
	state.EndAll();
//	Save_Config();
	exit(EXIT_SUCCESS);
}

/*******************************************************************************

	Functions dealing with numerical types

*******************************************************************************/

int RoundToInt(float x)
{
	return int((x > 0) ? (x+.5f) : (x-.5f));
}
int RoundToInt(double x)
{
	return int((x > 0) ? (x+.5) : (x-.5));
}

/*
 * Reentrant random function from POSIX.1c.
 * Copyright (C) 1996-2014 Free Software Foundation, Inc.
 * This file is part of the GNU C Library.
 * Contributed by Ulrich Drepper <drepper at cygnus.com <mailto:drepper at cygnus.com>>, 1996.
*/
// This algorithm is mentioned in the ISO C standard, here extended for 32 bits.
int myrand_r(unsigned int *seedp)
{
	unsigned int next = *seedp;
	int result;

	next *= 1103515245;
	next += 12345;
	result = (unsigned int) (next / 65536) % 2048;

	next *= 1103515245;
	next += 12345;
	result <<= 10;
	result ^= (unsigned int) (next / 65536) % 1024;

	next *= 1103515245;
	next += 12345;
	result <<= 10;
	result ^= (unsigned int) (next / 65536) % 1024;

	*seedp = next;

	return result;
}

// Random sample from a Gaussian distribution of mean m and standard deviation s.
// Implements the Polar form of the Box-Muller Transformation
// (c) Copyright 1994, Everett F. Carter Jr.
// Permission is granted by the author to use this software for any application provided this copyright notice is preserved.
float GaussianRand(unsigned int *seedp, float m, float s)
{
	float x1, x2, w, y;

	do
	{
		x1 = 2.0f * FRand(seedp) - 1.0f;
		x2 = 2.0f * FRand(seedp) - 1.0f;
		w = x1 * x1 + x2 * x2;
	}
	while ( w >= 1.0f );

	w = sqrt( (-2.0f * log( w ) ) / w );
	y = x1 * w;

	return( m + y * s );
}

/*******************************************************************************

	Functions dealing with strings

*******************************************************************************/

string StripPath(const string & _s)
{
	string s;
	size_t i, n = _s.length();
	size_t pos = _s.find_last_of("/\\");

	if (pos == string::npos)	i = 0;
	else						i = pos+1;

	s.resize(n-i);
	for (size_t j = 0; i < n; i++,j++)
        s[j] = (char)_s[i];

	return s;
}

string InsertBeforeExtension(const string & _s, const string & s_insert)
{
	string	s	=	_s;
	size_t	pos	=	_s.find_last_of(".");

	return s.insert(pos+1, s_insert);
}


string ReadQuotedString(istream &f, const string & error_message)
{
	char c;
	f >> c;

	if (!f)
		return "";

	if (c != '"')
		Fatal_Error(error_message + ": opening '\"' expected");

	string s;
	while( f.get(c) && (c != '"') )
	{
		s += c;
	}

	if (c != '"')
		Fatal_Error(error_message + ": closing '\"' expected");

	return s;
}

string ToUpper(const string & s)
{
	string ret = s;
	transform(ret.begin(), ret.end(), ret.begin(), (int(*)(int))toupper);
	return ret;
}
string ToLower(const string & s)
{
	string ret = s;
	transform(ret.begin(), ret.end(), ret.begin(), (int(*)(int))tolower);
	return ret;
}


void WriteQuotedString(ostream &f, const string & s)
{
	f << '"' << s << '"';
}

void SkipComments(istream &f)
{
	string s;
	streampos pos;

	while (f)
	{
		pos = f.tellg();
		f >> s;
		if (s.length() && s[0] == '#')
		{
			char c;
			while (f && f.get(c) && c != '\n')
				;
		}
		else
		{
			f.seekg(pos);
			f.clear();
			break;
		}
	}
}

void Trim(string & s, const string & whitespace)
{
	string::size_type pos = s.find_last_not_of(whitespace);
	if (pos != string::npos)
	{
		// Trim right
		s.erase(pos + 1);

		// Trim left
		pos = s.find_first_not_of(whitespace);
		if (pos != string::npos)
			s.erase(0, pos);
	}
	else
		s.clear();
}

string OneDecimal(float x)
{
	stringstream ss;
	ss << fixed << setprecision(1) << x;
	return ss.str();
}

string TwoDecimals(float x)
{
	stringstream ss;
	ss << fixed << setprecision(2) << x;
	return ss.str();
}

string & Replace(string & s, const string & search, const string & replace)
{
	string::size_type search_size	=	search.size();

	if (!search_size)
		return s;

	string::size_type replace_size	=	replace.size();
	string::size_type idx			=	0;

	for(;;)
	{
		idx = s.find(search, idx);
		if (idx == string::npos)
			break;
		s.replace(idx, search_size, replace);
		idx += replace_size;
	}

	return s;
}
