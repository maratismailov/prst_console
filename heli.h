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

	Helicorders abstract class (heli_t):

	waveform data buffer attached to a data source, that can also be drawn on-screen.
	Data acquisition runs in its own thread.

	Derived classes are sac_t, which streams 1-sec packets from a SAC file
	and slink_t that streams packets from a SeedLink server.
	timeseries_t implements a sparse time series (e.g. magnitude graph) as
	an helicorder (in a hacky way).

*******************************************************************************/

#ifndef HELI_H_DEF
#define HELI_H_DEF

#include <set>
#include <list>
#include <deque>
#include "SDL_thread.h"
#include "libslink.h"
#undef min
#undef max

#include "global.h"

#include "state.h"
#include "sac_header.h"
#include "place.h"
#include "origin.h"
#include "rtmag.h"

/*******************************************************************************

	online_mean_t

	http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#On-line_algorithm

*******************************************************************************/

class online_mean_t
{
private:

	string name;
	unsigned long num;
	double mean, m2;
	double min, max;

public:

	online_mean_t(const string & _name);

	void Reset();
	void Add(double x);

	const string & GetName()	const	{	return name;	}
	unsigned long GetNum()		const	{	return num;		}
	double GetMean()			const	{	return mean;	}
	double GetMin()				const	{	return min;		}
	double GetMax()				const	{	return max;		}
	double GetSigma()			const;
};

/*******************************************************************************

	pick_t

*******************************************************************************/

class pick_t
{
public:

	static const int NO_QUAKE;

	secs_t t;
	float dt;
	int polarity;
	int quake_id;
	float disp[MAG_SIZE];
	float quake_mag[MAG_SIZE];
	float quake_rms;

	pick_t( secs_t _t, float _dt, int _polarity );
	pick_t();	// prevent the compiler from generating a default constructor

	// a == b <-> !(a < b) && !(b < a)
	bool operator < (const pick_t & rhs) const
	{
//		return (rhs.t - t) >= .001f;
		return t < rhs.t;
	}
	
	bool Overlaps(const pick_t & rhs, float secs) const
	{
		return abs(t - rhs.t) < dt + rhs.dt + secs;
	}

	friend ostream& operator<< (ostream& os, const pick_t& p);
};

typedef set<pick_t> picks_set_t;

/*******************************************************************************

	timespans_t - A collection of time spans on the helicorder.
                  Used to record time ranges containing clipped samples.

*******************************************************************************/

class timespan_t
{
private:
	secs_t t0, t1;

public:
	timespan_t(secs_t _t0, secs_t _t1) :
		t0(_t0), t1(_t1)
	{ }

	bool Overlaps(secs_t _t0, secs_t _t1) const;
	void Extend(secs_t _t0, secs_t _t1);

	secs_t GetT0() const
	{
		return t0;
	}
	secs_t GetT1() const
	{
		return t1;
	}
};

class timespans_t
{
private:
	typedef list<timespan_t> timespan_container_t;
	timespan_container_t timespans;

public:

	typedef timespan_container_t::iterator iterator;
	typedef timespan_container_t::const_iterator const_iterator;

	bool Overlaps(secs_t t0, secs_t t1) const;

	void Add(secs_t t0, secs_t t1);
	void PurgeBefore(secs_t tmin);
	void Clear();

	timespan_container_t::iterator begin()
	{
		return timespans.begin();
	}
	timespan_container_t::iterator end()
	{
		return timespans.end();
	}
	timespan_container_t::const_iterator begin() const
	{
		return timespans.begin();
	}
	timespan_container_t::const_iterator end() const
	{
		return timespans.end();
	}
};

/*******************************************************************************

	heli_t - Abstract base class for helicorders.
	         Generic Init, Start, Stop, Update (data acquisition)

*******************************************************************************/

struct mean_data_t
{
	float		sum;
	unsigned	samples;
	mean_data_t( float _sum, int _samples ) : sum(_sum), samples(_samples) {}
};

class station_t;

class heli_t
{
public:

	enum heli_err_t
	{
		ERR_NONE		=	0,
		ERR_NODATA		=	1,
		ERR_FATAL		=	2
	};

	heli_err_t SetError(heli_err_t err);
	heli_err_t GetError() const;

protected:

	string url;

	SDL_Thread	*thread;

	static int Update_ThreadFunc(void *heli_ptr);
	heli_err_t CreateThread();
	void DestroyThread();

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

private:

	bool isGraph;

	SDL_mutex	*mutex;

	heli_err_t	error;
	secs_t		error_secs;

	float *samples, *buffer;
	int num_samples;

	secs_t end_time;
	float samples_per_sec;

	secs_t secs_packet_received;

	// Latencies
	secs_t secs_latency_updated;
	secs_t latency_data, latency_feed;
	online_mean_t latency_data_mean, latency_feed_mean;

	double dmean;
	float depmin, depmax;

	deque<mean_data_t> mean_data;

	// fill with 0
	void ClearSamples();

	// convert time0 to a float offset into the samples
	inline float SecsToOffset(secs_t time)
	{
		secs_t start_time	=	end_time - secs_t(num_samples) / NonZero(samples_per_sec);
		return float(time - start_time);
	}

	inline float GetSample(float t0)
	{
		int sample_index = int(t0 * samples_per_sec + .5f);

		if ((sample_index < 0) || (sample_index >= num_samples))
			return 0;

		return samples[sample_index];
	}

	inline void GetSampleBar(float t0, float t1, float *s_min, float *s_max)
	{
		int sample0_index = int(t0 * samples_per_sec + .5f);
		int sample1_index = int(t1 * samples_per_sec + .5f);

		*s_min = *s_max = 0;

		if ((sample1_index < 0) || (sample0_index >= num_samples))
			return;

		if (sample0_index < 0)
			sample0_index = 0;

		if (sample1_index >= num_samples)
			sample1_index = num_samples - 1;

		float *s		=	&samples[sample0_index];
		float *s_last	=	&samples[sample1_index];

		for (; s <= s_last; s++)
		{
			float sample = *s;
			if (sample)
			{
				if (sample < *s_min || *s_min == 0) *s_min = sample;
				if (sample > *s_max || *s_max == 0) *s_max = sample;
			}
		}
	}

	timespans_t clipspans;	// time spans containing clipped samples

	picks_set_t picks, new_picks;

	void *picker_mem;
	void *picker_picks;
	int  picker_num_picks;

	void FreePicker();

protected:

	void RmeanOverOneSecPackets(float *dest, int samples_count, int sps);

	void PurgeOldPicks();
	void ClearPicks();
	bool AddPick( const pick_t & p );

	bool HasClipping(secs_t t0, secs_t t1);

public:

	static inline void swap32(unsigned char p[4])
	{
		std::swap(p[0],p[3]);
		std::swap(p[1],p[2]);
	}
	static inline void swap16(unsigned char p[2])
	{
		std::swap(p[0],p[1]);
	}

	bool exitThread;

	string GetUrl()		const	{	return url;			}
	int GetNumSamples()	const	{	return num_samples;	}

	virtual secs_t Secs_T0()	const	{ return 0; }

	float GetMax(secs_t t0, secs_t t1);

	heli_t()
	:	latency_data_mean("Ld"), latency_feed_mean("Lf")
	{
		thread = NULL;
		mutex = NULL;
		exitThread = false;

		url = "";

		samples = NULL;
		buffer = NULL;
		num_samples = 0;

		picker_mem = NULL;
		picker_picks = NULL;
		picker_num_picks = 0;

		Stop();
	}

	virtual ~heli_t()
	{
		Stop();

		delete [] samples;
		delete [] buffer;
	}

	virtual heli_err_t Init(const string & url, int num_samples, station_t *_station, bool _isGraph = false);
	virtual void Start() = 0;
	virtual void Stop();
	virtual heli_err_t GetData(float *& samples_new, int & num_samples_new, float & samples_per_sec_new, secs_t & end_time_new) = 0;

	// *** Important: this function is run from several threads (one per channel).
	heli_err_t Update();
	
	void Draw(const win_t & win, float x, float y, float w, float h, float alpha, const string & title, secs_t time0, float duration, bool use_counts);

	bool ComputePicks(
		const float  *samples_new,
		const int    num_samples_new,
		const secs_t start_time_new,

		const double filterWindow,		// in seconds, determines how far back in time the previous samples are examined.  The filter window will be adjusted upwards to be an integer N power of 2 times the sample interval (deltaTime).  Then numRecursive = N + 1 "filter bands" are created.  For each filter band n = 0,N  the data samples are processed through a simple recursive filter backwards from the current sample, and picking statistics and characteristic function are generated.  Picks are generated based on the maximum of the characteristic function values over all filter bands relative to the threshold values threshold1 and threshold2.
		const double longTermWindow,	// determines: a) a stabilisation delay time after the beginning of data; before this delay time picks will not be generated. b) the decay constant of a simple recursive filter to accumulate/smooth all picking statistics and characteristic functions for all filter bands.
		const double threshold1,		// sets the threshold to trigger a pick event (potential pick).  This threshold is reached when the (clipped) characteristic function for any filter band exceeds threshold1.
		const double threshold2,		// sets the threshold to declare a pick (pick will be accepted when tUpEvent reached).  This threshold is reached when the integral of the (clipped) characteristic function for any filter band over the window tUpEvent exceeds threshold2 * tUpEvent (i.e. the average (clipped) characteristic function over tUpEvent is greater than threshold2)..
		const double tUpEvent			// determines the maximum time the integral of the (clipped) characteristic function is accumulated after threshold1 is reached (pick event triggered) to check for this integral exceeding threshold2 * tUpEvent (pick declared).
	);

	void GetNewPicks(picks_set_t & result);
	void UpdatePick(const pick_t & pick);

	void GetSamples(secs_t pick_time, float duration, float **dest, int *num);
	void CalcDisplacementSamples(float fmin, float fmax, secs_t pick_time, float duration, float **dest, int *num);

	bool IsLaggingOrFuture();
	secs_t EndTime();

	void LogMeanLatencies();
	void ResetMeanLatencies();

	station_t *station;
};

/*******************************************************************************

	sac_t - SAC file helicorder derived from heli_t.
	        Handle SAC file in Init, Start, Stop, Update (data acquisition)

*******************************************************************************/

// FIXME move to static member of sac_t
extern secs_t sacs_seconds_t0;

class sac_t : public heli_t
{
private:

	float *sacsamples;
	secs_t secs_t0;

	sac_header_t hdr;

	int sac_seq;
	secs_t sac_seq_secs;
	float sac_seq_lag;
	unsigned int sac_seq_seed;

	string KtoString(const char *k) const;
	secs_t GetSACReferenceSecs() const;

	void SetRandLag();

public:

	string GetSACStation() const;
	float GetSACSampleRate() const;
	char ValidComponent( char cmp ) const;
	char GetSACComponent() const;
	bool GetSACEvent(float *lon, float *lat, float *dep, float *mag) const;

	secs_t Secs_T0() const	{ return secs_t0; }

	sac_t()
	{
		sacsamples = 0;
		sac_seq = -1;
		sac_seq_secs = -1;
		sac_seq_lag = 0;
		secs_t0 = 0;
	}

	~sac_t()
	{
		Stop();
		delete [] sacsamples;
	}

	virtual heli_err_t Init(const string & filename, int num_samples, station_t *_station);
	void Start();
	void Stop();
	heli_err_t GetData(float *& samples_new, int & num_samples_new, float & samples_per_sec_new, secs_t & end_time_new);
};


/*******************************************************************************

	slink_t - SeedLink helicorder
	          Handle SeedLink stream in Init, Start, Stop, Update (data acquisition)

*******************************************************************************/

class slink_t : public heli_t
{
private:

	SLCD *slconn;
	SLMSrecord *msr;

	string ip, streams;

public:

	slink_t()
	{
		slconn	=	NULL;
		msr		=	NULL;
	}

	~slink_t()
	{
		Stop();
	}

	virtual heli_err_t Init(const string & filename, int num_samples, station_t *_station);
	void Start();
	void Stop();
	heli_err_t GetData(float *& samples_new, int & num_samples_new, float & samples_per_sec_new, secs_t & end_time_new);
};

/*******************************************************************************

	timeseries_t - Magnitude graph derived from heli_t

*******************************************************************************/

class timeseries_t : public heli_t
{
private:

	struct sample_t
	{
		secs_t time;
		float val_min, val_avg, val_max;
	};

	list<sample_t> data;

	float buf[100];

public:

	timeseries_t()
	{
	}

	~timeseries_t()
	{
		Stop();
	}

	void Start();
	void Stop();
	heli_err_t GetData(float *& samples_new, int & num_samples_new, float & samples_per_sec_new, secs_t & end_time_new);

	void Add(secs_t time, float val_min, float val_avg, float val_max);
	void SetMarker(secs_t time);
};

/*******************************************************************************

	station_t - A station is a place_t, an IP address and 3 data streams (heli_t)

*******************************************************************************/

class station_t : public gridplace_t
{
public:
	bool isAccel;
	float clipvalue;	// when abs(counts) > clipvalue, the waveform is considered clipped

	float factor;		// counts -> m/s^2 (or m/s)
	string ipaddress, net;

	string channel_z, channel_n, channel_e;
	heli_t *z, *n, *e;

	station_t();
	station_t(const string & _name, float _lon, float _lat, float _dep, bool _isAccel, float _clipvalue, float _factor, const string & _ipaddress, const string & _net, const string & _channel_z, const string & _channel_n, const string & _channel_e);
	~station_t();

	void CombineComponents(magcomp_t comp, float *dz, int dz_num, float *dn, int dn_num, float *de, int de_num, float **out_first, float **out_last);
	float CalcPickSNR(magcomp_t comp, secs_t pick_time, float secs_before, float secs_delay, float secs_after);
	void CalcPeakDisplacement(float fmin, float fmax, const string & label, magcomp_t comp, secs_t pick_time, float duration, float *disp_val, secs_t *disp_time);
};

struct StationName : public binary_function< station_t, string, bool >
{
	bool operator () ( const station_t & station, const string & name ) const
	{
		return station.name == name;
	}
};

#endif
