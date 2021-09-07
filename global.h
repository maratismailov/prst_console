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

#ifndef GLOBAL_H_DEF
#define GLOBAL_H_DEF

#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <time.h>
#include "SDL_timer.h"

using namespace std;

#if defined(_MSC_VER)
	#define __NORETURN __declspec(noreturn)
#elif defined(__GNUC__)
	#define __NORETURN __attribute__((noreturn))
#else
	#define __NORETURN
#endif

#ifndef FLOAT_PI
	#define FLOAT_PI 3.1415926535897932384626433832795f
#endif


// typedef unsigned char byte;

#define KM		(10.0f)

// path of the various directories where the program data resides

extern const string PATH_FONT;
extern const string PATH_DATA;
extern const string PATH_CONFIG;

/*******************************************************************************

	Time

*******************************************************************************/

// Thread-safe variants of the time functions, defined in a cross-platform way
tm *mygmtime_r(const time_t *timep, tm *result);
tm *mylocaltime_r(const time_t *timep, tm *result);
char *myasctime_r(const tm *tm, char *result, size_t size);

typedef double secs_t;
extern secs_t globaltime; // time since the program was started
secs_t SecsNow();
string SecsToString_HHMMSS(secs_t secs);
string SecsToString(secs_t secs);
string SecsToKMLString(secs_t secs);
string IntervalToString(secs_t secs);

typedef Uint32 ticks_t;	// milliseconds (note: this wraps-around after ~49 days! Hence use the helper functions below)
ticks_t TicksDifference(ticks_t t_new, ticks_t t_old);
ticks_t TicksElapsedSince(ticks_t t_old);

//	Simulated time

class simutime_t
{
private:

	static bool isPaused;
	static ticks_t ticks0, ticks_pause, ticks_offset;
	static secs_t secs_t0;

public:

	simutime_t();

	static void SetT0(secs_t _secs_t0);
	static secs_t Get();
	static void SetPaused(bool paused);
	static bool GetPaused();
	static void Reset();
};

bool GetPaused();
void SetPaused( bool _paused );

/*******************************************************************************

	Colors

*******************************************************************************/

struct color_t
{
	float r,g,b,a;
	color_t() {};
	color_t(float _r, float _g, float _b, float _a)
	:	r(_r),g(_g),b(_b),a(_a)		{ }

	const bool operator == (const color_t & c)	const
	{
		return (r == c.r) && (g == c.g) && (b == c.b) && (a == c.a);
	}

	const color_t operator + (const color_t & v) const
	{
		return color_t(r + v.r, g + v.g, b + v.b, a /*+ v.a*/);
	}

	const color_t operator - (const color_t & v) const
	{
		return color_t(r - v.r, g - v.g, b - v.b, a /*- v.a*/);
	}

	const color_t operator * (const float f) const
	{
		return color_t(r * f, g * f, b * f, a /** f*/);
	}

	const color_t operator / (const float f) const
	{
		return color_t(r / f, g / f, b / f, a /*/ f*/);
	}

	color_t operator += (const color_t & v)
	{
		r += v.r;	g += v.g;	b += v.b;
		return *this;
	}

	color_t operator -= (const color_t & v)
	{
		r -= v.r;	g -= v.g;	b -= v.b;
		return *this;
	}

	color_t operator *= (const float f)
	{
		r *= f;	g *= f;	b *= f;
		return *this;
	}

	color_t operator /= (const float f)
	{
		r /= f;	g /= f;	b /= f;
		return *this;
	}

	friend ostream& operator<< (ostream& os, const color_t & c);
};

// a gradient between 2 colors
struct colors_t
{
	float r,g,b,a;
	float r2,g2,b2,a2;
	colors_t() {};
	colors_t(color_t col)
	:	r(col.r),g(col.g),b(col.b),a(col.a),r2(col.r),g2(col.g),b2(col.b),a2(col.a)		{ }

	colors_t(	float _r,  float _g,  float _b,  float _a,
				float _r2, float _g2, float _b2, float _a2	)
	:	r(_r),g(_g),b(_b),a(_a),r2(_r2),g2(_g2),b2(_b2),a2(_a2)		{ }

	colors_t(float _r,  float _g,  float _b,  float _a)
	:	r(_r),g(_g),b(_b),a(_a),r2(_r),g2(_g),b2(_b),a2(_a)		{ }

	friend ostream& operator<< (ostream& os, const colors_t & c);
};


/*******************************************************************************

	Exiting from the application

*******************************************************************************/

// Fatal error, log a message and exit
__NORETURN void Fatal_Error(const string & errstr);

// Error free exit
__NORETURN void Exit();


/*******************************************************************************

	Functions dealing with numerical types

*******************************************************************************/

struct range_t
{
	float min, max, val;
};

// Radians <-> Degrees conversion

inline float RadsToDegs(float rads)
{
	return rads * (180.0f/FLOAT_PI);
}

inline float DegsToRads(float degs)
{
	return degs * (FLOAT_PI/180.0f);
}


// Linear interpolation, clamping and wrapping

template< class T >
inline const T Interp(const T & t1, const T & t2, const float x)
{
	return T( t1 + (t2 - t1) * x );
}

template < class T >
inline void Clamp(T &t, const T & min, const T & max)
{
	if		(t > max)	t = max;
	else if	(t < min)	t = min;
};

template < class T >
void Wrap(T &t, const T & min, const T & max)
{
	T span = max - min;
	t = t - floor((t-min) / span) * span;
};


// Random numbers

int myrand_r(unsigned int *seedp); // This algorithm is mentioned in the ISO C standard, here extended for 32 bits.

// A random float between 0 and 1

inline float FRand(unsigned int *seedp)
{
	return float(myrand_r(seedp)) / 0x7fffffff;
}

// A random sample from a Gaussian distribution of mean m and standard deviation s

float GaussianRand(unsigned int *seedp, float m, float s);

// Use to avoid a division by 0

template < class T >
inline T NonZero(T t)
{
	return t ? t : 1;
}

int RoundToInt(float x);
int RoundToInt(double x);

template<class T>
T Sqr(const T t)
{
	return t * t;
}

/*******************************************************************************

	Functions dealing with strings

*******************************************************************************/

string ToUpper(const string & s);
string ToLower(const string & s);

// Strip the path, return the file name
string StripPath(const string & s);

string InsertBeforeExtension(const string & _s, const string & s_insert);

// Read and Write a quoted string (e.g. "abc") from a file
string  ReadQuotedString (istream &f, const string & error_message);
void	WriteQuotedString(ostream &f, const string & s);

// If the next char in f is a '#' (a comment), skip to the beginning
// of the first line that is not a comment
void SkipComments(istream &f);

// Trim spaces from the sides of the string
void Trim(string & s, const string & whitespace = " ");

// Universal conversion from number to string
template<class T>
string ToString(const T t)
{
	stringstream ss;	// no static (mt-safety)
	ss << t;
	return ss.str();
}

string OneDecimal(float x);
string TwoDecimals(float x);

string & Replace(string & s, const string & search, const string & replace);

#endif
