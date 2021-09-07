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

#include <sstream>
#include <iomanip>
#include <fstream>
#include <limits>
#include <algorithm>
#include "SDL.h"

#include "heli.h"

extern "C" {
#include "picker/FilterPicker5.h"
}
#include "filter.h"

#include "config.h"

using namespace std;

const int pick_t::NO_QUAKE = -1;

const float sac_header_t :: UNDEF = -12345;

/*******************************************************************************

	online_mean_t

	http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#On-line_algorithm

*******************************************************************************/

void online_mean_t :: Reset()
{
	num		=	0;
	mean	=	0;
	m2		=	0;
	min		=	+numeric_limits<double>::max();
	max		=	-numeric_limits<double>::max();
}

online_mean_t :: online_mean_t(const string & _name)
:	name(_name)
{
	Reset();
}

void online_mean_t :: Add(double x)
{
	if (x > max)
		max = x;

	if (x < min)
		min = x;

	++num;
	double delta = x - mean;
	mean	+=	delta / num;
	m2		+=	delta * (x - mean);	// This expression uses the new value of mean
}

double online_mean_t :: GetSigma() const
{
	if (num)
		return sqrt(m2 / num);

	return 0;
}


ostream& operator<< (ostream& os, const online_mean_t& m)
{
	os	<<		m.GetName()	<<	": "			<<	m.GetMean()		<<	" +- "	<<	m.GetSigma()	<<
		" "	<<	m.GetName()	<<	"_min: "		<<	m.GetMin()		<<
		" "	<<	m.GetName()	<<	"_max: "		<<	m.GetMax()		<<
		" "	<<	m.GetName()	<<	"_samples: "	<<	m.GetNum()		;

	return os;
}

/*******************************************************************************

	station_t - A station is a place_t, an IP address and 3 data streams (heli_t)

*******************************************************************************/

station_t :: station_t()
	: z(NULL), n(NULL), e(NULL)
{}

station_t :: station_t(const string & _name, float _lon, float _lat, float _dep, bool _isAccel, float _clipvalue, float _factor, const string & _ipaddress, const string & _net, const string & _channel_z, const string & _channel_n, const string & _channel_e)
	:	gridplace_t(_name, _lon, _lat, _dep),
		isAccel(_isAccel), clipvalue(_clipvalue), factor(_factor), ipaddress(_ipaddress), net(_net), channel_z(_channel_z), channel_n(_channel_n), channel_e(_channel_e),
		z(NULL), n(NULL), e(NULL)
{}

station_t :: ~station_t()
{
	delete z;
	delete n;
	delete e;
}

/*
	Combine up to three component buffers (according to comp) to obtain the vector module.
	Replace a missing horizontal component (or the one with less samples) with the other one.
	When using three components and both horizontals are missing, replace them with the vertical.
	The module waveform is written to one of the input buffers, overwriting it.
	Return pointers to the first and last sample of the output, or NULL if not enough data is available.
*/
void station_t :: CombineComponents( magcomp_t comp, float *dz, int dz_num, float *dn, int dn_num, float *de, int de_num, float **out_first, float **out_last )
{
	*out_first = *out_last = NULL;

	float *b_first, *b_last, *b;
	switch (comp)
	{
		case MAGCOMP_VERTICAL:
		{
			if (dz == NULL)
				return;		// nothing to do

			b_first		=	dz;
			b_last		=	dz + dz_num - 1;
			b			=	dz;
			for (; b <= b_last; b++)
			{
				*b = abs( *b );
			}
		}
		break;

		case MAGCOMP_HORIZONTAL:
		{
			if ( dn == NULL && de == NULL )
				return;		// nothing to do

			// We now have at least one component (either dn or de is not NULL)

			// Make sure the horizontal components are both present with an equal number of samples:

			// if an horizontal component is missing or has less samples than the other one, duplicate the other one
			if ( dn == NULL || dn_num < de_num )
			{
				dn		=	de;
				dn_num	=	de_num;
			}
			else if ( de == NULL || de_num < dn_num )
			{
				de		=	dn;
				de_num	=	dn_num;
			}

			b_first		=	dn;
			b_last		=	dn + dn_num - 1;
			b			=	dn;
			float *b1	=	de;
			for (; b <= b_last; b++,b1++)
			{
				// FIXME optimize removing sqrt
				*b = sqrt( Sqr(*b) + Sqr(*b1) );
			}
		}
		break;

		case MAGCOMP_ALL:
		{
			if ( dz == NULL && dn == NULL && de == NULL )
				return;		// nothing to do

			// Now have at least one component (either dn, de, or dz is not NULL)

			// Make sure the horizontal components are both present with an equal number of samples:

			// if an horizontal component is missing or has less samples than the other one, duplicate the other one
			if ( dn == NULL || dn_num < de_num )
			{
				dn		=	de;			// this may be NULL
				dn_num	=	de_num;
			}
			else if ( de == NULL || de_num < dn_num )
			{
				de		=	dn;			// this may be NULL
				de_num	=	dn_num;
			}

			// Now the vertical component is not NULL and/or both horizontal components are not null

			// Make sure the horizontal and vertical components are present with an equal number of samples:

			if ( dz == NULL || dz_num < dn_num )
			{
				// If the vertical component is missing or has less samples than the horizontal ones, duplicate one horizontal component
				dz		=	dn;
				dz_num	=	dn_num;
			}
			else if ( dn == NULL || dn_num < dz_num )
			{
				// If the horizontal components are missing or have less samples than the vertical one, use the vertical one three times
				dn		=	dz;
				dn_num	=	dz_num;

				de		=	dz;
				de_num	=	dz_num;
			}

			b_first		=	dz;
			b_last		=	dz + dz_num - 1;
			b			=	dz;
			float *b1	=	dn;
			float *b2	=	de;
			for (; b <= b_last; b++,b1++,b2++)
			{
				// FIXME optimize removing sqrt
				*b = sqrt( Sqr(*b) + Sqr(*b1) + Sqr(*b2) );
			}
		}
		break;

		default:
			return;
	}

	*out_first = b_first;
	*out_last  = b_last;
}

void heli_t :: GetSamples(secs_t pick_time, float duration, float **dest, int *num)
{
	Lock();

	secs_t start_time = end_time - secs_t(num_samples) / NonZero(samples_per_sec);

	*num = RoundToInt( samples_per_sec * duration );

	int first	=	RoundToInt( float(pick_time - start_time) * samples_per_sec );
	int last	=	first + *num - 1;

	if ( end_time == -1 || first < 0 || last < 0 || first >= num_samples || last >= num_samples ||
	     HasClipping(pick_time, pick_time + duration)	)
	{
		*dest = NULL;
		*num  = 0;

		Unlock();
		return;
	}

	float *s_first	=	&samples[first];

	float *b_first	=	&buffer[first];
	float *b_last	=	&buffer[last];

	*dest = b_first;

	memcpy(b_first, s_first, (b_last - b_first + 1) * sizeof(*b_first));

	Unlock();
}

/*
	Calc the Signal-To-Noise ratio of the unprocessed vector waveform (e.g. acceleration after mean removal on components specified by comp)
	relative to a pick: i.e. the ratio between the maximum after the arrival (pick_time + secs_delay over secs_after seconds)
	and the RMS before the pick (over secs_before seconds). Return -1 if not enough data is available.
*/
float station_t :: CalcPickSNR(magcomp_t comp, secs_t pick_time, float secs_before, float secs_delay, float secs_after)
{
	float *dz     = NULL, *dn     = NULL, *de = NULL;
	int    dz_num = 0,     dn_num = 0,     de_num = 0;

	// Fill a samples buffer for each needed component.
	// Clipping is checked in GetSamples

	float duration = secs_before + secs_delay + secs_after;
	if (comp != MAGCOMP_HORIZONTAL)
	{
		// Z
		if (z != NULL)
			z->GetSamples( pick_time - secs_t(secs_before), duration, &dz, &dz_num );
	}
	if (comp != MAGCOMP_VERTICAL)
	{
		// N
		if (n != NULL)
			n->GetSamples( pick_time - secs_t(secs_before), duration, &dn, &dn_num );

		// E
		if (e != NULL)
			e->GetSamples( pick_time - secs_t(secs_before), duration, &de, &de_num );
	}

	// Remove means
	if (dz != NULL)	Rmean(dz, dz + dz_num - 1);
	if (dn != NULL)	Rmean(dn, dn + dn_num - 1);
	if (de != NULL)	Rmean(de, de + de_num - 1);

	// Combine the samples buffers to obtain the vector module
	float *b_first, *b_last, *b;
	CombineComponents(comp, dz, dz_num, dn, dn_num, de, de_num, &b_first, &b_last);
	if (b_first == NULL)
		return -1;

	int num_duration = int(b_last - b_first + 1);

	// Calc pick sample index in the module buffer
	int num_before = RoundToInt( (secs_before / duration) * num_duration );
	Clamp(num_before, 0, num_duration - 1);
	float *b_pick = b_first + num_before;

	// Calc arrival sample index in the module buffer
	int num_arrival = RoundToInt( ((secs_before + secs_delay) / duration) * num_duration );
	Clamp(num_arrival, 0, num_duration - 1);
	float *b_arrival = b_first + num_arrival;

	// Calc RMS before the pick
	float rms = 0;
	for (b = b_first; b < b_pick; b++)
		rms += Sqr(*b);
	rms = sqrt( rms / (b_pick - 1 - b_first + 1) );

	// Find the maximum after the arrival
	float peak = 0;
	for (b = b_arrival; b <= b_last; b++)
	{
		if (*b > peak)
			peak = *b;
	}

	return peak / NonZero(rms);
}

void station_t :: CalcPeakDisplacement( float fmin, float fmax, const string & label, magcomp_t comp, secs_t pick_time, float duration, float *disp_val, secs_t *disp_time )
{
	float *dz     = NULL, *dn     = NULL, *de = NULL;
	int    dz_num = 0,     dn_num = 0,     de_num = 0;

	*disp_val	=	-1;
	*disp_time	=	0;

	// Fill a displacement buffer for each needed component.
	// Clipping is checked in CalcDisplacementSamples

	if (comp != MAGCOMP_HORIZONTAL)
	{
		// Z
		if (z != NULL)
			z->CalcDisplacementSamples( fmin, fmax, pick_time, duration, &dz, &dz_num );
	}
	if (comp != MAGCOMP_VERTICAL)
	{
		// N
		if (n != NULL)
			n->CalcDisplacementSamples( fmin, fmax, pick_time, duration, &dn, &dn_num );

		// E
		if (e != NULL)
			e->CalcDisplacementSamples( fmin, fmax, pick_time, duration, &de, &de_num );
	}

	// Combine the displacement buffers to obtain the displacement vector module
	float *b_first, *b_last, *b;
	CombineComponents(comp, dz, dz_num, dn, dn_num, de, de_num, &b_first, &b_last);
	if (b_first == NULL)
		return;

	// Write the just computed displacement to disk for debugging

	if (!realtime && param_simulation_write_displacement)
	{
		string debugname;

		debugname = sacs_dir + name + "." + label + ".sg2";
		ofstream f(debugname.c_str(), ios::trunc);
		if (f)
		{
			secs_t dt = duration / (b_last - b_first + 1);

			f << scientific << setprecision(20);

			f <<	"SG2K_ASCII" <<
					" sampleInt=" << dt <<
					" year=1970 jday=1 hour=0 min=0 sec=0.0 begTime=" << pick_time <<
					" sta=" << name <<
					" ampUnits=meters" <<
					endl;

			secs_t t = 0;
			for (b = b_first; b <= b_last; b++,t+=dt)
				f << t << " " << *b * factor << endl;

			f << "END_SG2K_ASCII" << endl;
		}
	}

	// Find the peak

	float peak		=	0;
	float *peak_b	=	b_first;
	for (b = b_first; b <= b_last; b++)
	{
		if (*b > peak)
		{
			peak	=	*b;
			peak_b	=	b;
		}
	}

	*disp_val	=	peak * factor;
	*disp_time	=	pick_time + (peak_b - b_first) * (duration / (b_last - b_first + 1));
}

/*******************************************************************************

	pick_t

*******************************************************************************/

pick_t :: pick_t( secs_t _t, float _dt, int _polarity ) :
		t(_t), dt(_dt), polarity(_polarity),
		quake_id(pick_t::NO_QUAKE),
		quake_rms(-1)
{
	for (int magtype = 0; magtype < MAG_SIZE; magtype++)
		disp[magtype] = quake_mag[magtype] = -1;
}

ostream& operator<< (ostream& os, const pick_t& p)
{
	return os << SecsToString(p.t) << " " << p.dt << " " << p.polarity;
}

/*******************************************************************************

	timespans_t

*******************************************************************************/

bool timespan_t :: Overlaps(secs_t _t0, secs_t _t1) const
{
	return	((_t0 >= t0) && (_t0 <= t1))	||
			((_t1 >= t0) && (_t1 <= t1))	||
			((_t0 <= t0) && (_t1 >= t1))	;
}

void timespan_t :: Extend(secs_t _t0, secs_t _t1)
{
	t0 = min(t0, _t0);
	t1 = max(t1, _t1);
}

bool timespans_t :: Overlaps(secs_t t0, secs_t t1) const
{
	for (timespan_container_t::const_iterator c = timespans.begin(); c != timespans.end(); c++)
	{
		if (c->Overlaps(t0, t1))
			return true;
	}
	return false;
}

void timespans_t :: Add(secs_t t0, secs_t t1)
{
	bool added = false;
	for (timespan_container_t::iterator c = timespans.begin(); c != timespans.end(); c++)
	{
		// In order to avoid tiny gaps beetween time spans (due to e.g. zero crossing
		// between clipped samples), don't be too strict in checking for overlapping
		if (c->Overlaps(t0 - 0.1 , t1 + 0.1))
		{
			c->Extend(t0, t1);
			added = true;
		}
	}

	if (!added)
		timespans.push_back(timespan_t(t0, t1));
}

void timespans_t :: PurgeBefore(secs_t tmin)
{
	timespan_container_t::iterator c = timespans.begin();
	while (c != timespans.end())
	{
		if (c->GetT1() < tmin)
			c = timespans.erase( c );
		else
			++c;
	}
}

void timespans_t :: Clear()
{
	timespans.clear();
}

/*******************************************************************************

	heli_t - Abstract base class for helicorders.
	         Generic Init, Start, Stop, Update (data acquisition)

*******************************************************************************/

void heli_t :: Stop()
{
	DestroyThread();

	end_time = -1;
	samples_per_sec = 0;

	secs_packet_received = secs_latency_updated = SecsNow();	// useful to calc secs since no data
	latency_data = 0;
	latency_feed = 0;
	ResetMeanLatencies();

	dmean = 0;
	depmin = +numeric_limits<float>::max();
	depmax = -numeric_limits<float>::max();

	ClearSamples();

	SetError(ERR_NONE);
}

heli_t::heli_err_t heli_t :: Init(const string & _url, int _num_samples, station_t *_station, bool _isGraph)
{
	isGraph = _isGraph;

	heli_t :: Stop();

	url			=	_url;
	num_samples	=	_num_samples;
	station		=	_station;

	delete [] samples;
	delete [] buffer;
	samples = new float[num_samples];
	buffer  = new float[num_samples];
	if (samples == NULL || buffer == NULL)
	{
		num_samples = 0;
		return SetError(ERR_FATAL);
	}

	ClearSamples();

	return SetError(ERR_NONE);
}

heli_t::heli_err_t heli_t :: Update()
{
	float *samples_new;
	int num_samples_new;
	float samples_per_sec_new;
	secs_t end_time_new;

	// Check for new data. Locking (if needed) is handled in the class specific GetData method, since it
	// should avoid blocking access for too long (e.g. SeedLink) or the rendering thread will be stalled.

	heli_err_t err = GetData(samples_new, num_samples_new, samples_per_sec_new, end_time_new);

	// Gaps simulation (drop received packets at the given frequency)

	if (err == ERR_NONE && !isGraph && param_debug_gaps_period && param_debug_gaps_duration)
		if ( end_time_new - ((long int)(end_time_new / param_debug_gaps_period))*param_debug_gaps_period < param_debug_gaps_duration )
			err = SetError(ERR_NODATA);

	if (err != ERR_NONE)
	{
		// Update feed latency at least every second when not receiving packets
		secs_t now = SecsNow();
		if ( now - secs_latency_updated > 1 )
		{
			Lock();
			secs_latency_updated = now;
			latency_feed = now - secs_packet_received;
			// do not add to mean
			Unlock();
		}
		return err;
	}

	Lock();

	// Update latencies on new packets

	secs_t now = SecsNow();

	latency_data = now - end_time_new;
	latency_data_mean.Add(latency_data);

	latency_feed = now - secs_packet_received;
	latency_feed_mean.Add(latency_feed);

	secs_packet_received = secs_latency_updated = now;

	secs_t start_time_new	=	end_time_new   - secs_t(num_samples_new) / samples_per_sec_new;

	// Reset the picker on gaps/time jumps (the picker can't handle non-continuous time spans)

	if ( !isGraph && end_time != -1 && abs(start_time_new - end_time) > 0.05 )
		FreePicker();

	// Reset all (samples, picker, mean) on sample rate changes

	if ( samples_per_sec != samples_per_sec_new )
	{
		samples_per_sec = samples_per_sec_new;
		ClearSamples();

		end_time = end_time_new;
	}

	// Update the time spans containing clipped samples

	if (!isGraph)
	{
		if (end_time != -1)
			clipspans.PurgeBefore(end_time - secs_t(num_samples) / samples_per_sec);

		if (param_waveform_clipping_secs > 0 && station->clipvalue > 0)
		{
			float *s_first_new	=	&samples_new[0];
			float *s_last_new	=	&samples_new[num_samples_new - 1];

			// Find the first clipped sample
			float *s_clip	=	NULL;
			for (float *s = s_first_new; s <= s_last_new; s++)
			{
				if ( abs(*s) >= station->clipvalue )
				{
					s_clip = s;
					break;
				}
			}

			// Mark as clipped several seconds after the first clipped sample
			if (s_clip != NULL)
			{
				secs_t t_clip = end_time_new - secs_t(num_samples_new - 1 - (s_clip - samples_new)) / (samples_per_sec_new - 1);
				clipspans.Add(t_clip, t_clip + float(param_waveform_clipping_secs));
			}
		}
	}

	// Scroll old data to the left, putting the new end sample at the end of the buffer (if it's in the future wrt the buffer end)

	secs_t secs_scroll	=	end_time_new - end_time;
	if (secs_scroll > 0)
	{
		// Scroll out "samples_scroll" samples (limit large doubles to num samples before casting to avoid casting errors)
	    int samples_scroll = RoundToInt( min(secs_scroll * samples_per_sec_new, (secs_t)num_samples) );
		if ((num_samples - samples_scroll) > 0)
			memmove(samples, samples + samples_scroll, (num_samples - samples_scroll) * sizeof(samples[0]));

		// Clear to the end of the buffer (in case of gaps)

		float *s = samples + (num_samples - samples_scroll);
		int s_num = samples_scroll;
		while (s_num--)
			*s++ = 0;

		end_time = end_time_new;
	}

	// Copy new packet in the buffer

	secs_t start_time		=	end_time - secs_t(num_samples) / samples_per_sec;

	secs_t delta_time_new	=	start_time_new - start_time;	// time offset of the new packet wrt the buffer start time

	int sample_index;
	int samples_count;
	float *dest;
	if (delta_time_new >= 0)
	{
		// new packet starts after buffer start time
		sample_index	=	int(delta_time_new * samples_per_sec_new + 0.5);
		dest			=	samples + sample_index;
		samples_count	=	min(num_samples_new, num_samples - sample_index);

		if (samples_count > 0)
			memmove(dest, samples_new, samples_count * sizeof(samples[0]));
	}
	else
	{
		// new packet starts before buffer start time
		sample_index	=	int(-delta_time_new * samples_per_sec_new + 0.5);
		dest			=	samples;
		samples_count	=	min(num_samples_new - sample_index, num_samples);

		if (samples_count > 0)
			memmove(dest, samples_new + sample_index, samples_count * sizeof(samples[0]));
	}

	// Process new packet

	if (!isGraph)
	{
		// Picking (only vertical component)

		if (station->z == this)
			ComputePicks(samples_new, num_samples_new, start_time_new,
				param_picker_filterWindow, param_picker_longTermWindow, param_picker_threshold1, param_picker_threshold2, param_picker_tUpEvent);

		// Remove mean

		RmeanOverOneSecPackets(dest, samples_count, RoundToInt(samples_per_sec));
	}

	Unlock();

	return SetError(ERR_NONE);
}

void heli_t :: ClearSamples()
{
	float *s = samples;
	if (s != NULL)
		for (int num = num_samples; num > 0; num--)
			*s++ = 0;

	ClearPicks();
	FreePicker();

	mean_data.clear();

	clipspans.Clear();
}

/*******************************************************************************

	heli_t - Thread handling

*******************************************************************************/

int heli_t :: Update_ThreadFunc(void *heli_ptr)
{
	heli_t *heli = (heli_t *)heli_ptr;

	while (!heli->exitThread)
	{
		heli->Update();
		SDL_Delay(1000/10);	// update 10 times a second
	}

	return 0;
}

heli_t::heli_err_t heli_t :: CreateThread()
{
	mutex = SDL_CreateMutex();
	if (mutex == NULL)
		return SetError(ERR_FATAL);

	thread = SDL_CreateThread( Update_ThreadFunc, "heli", this );
	if (thread == NULL)
		return SetError(ERR_FATAL);

	return ERR_NONE;
}

void heli_t :: DestroyThread()
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

/*******************************************************************************

	heli_t - Errors

*******************************************************************************/

heli_t::heli_err_t heli_t :: SetError(heli_err_t err)
{
	error = err;
	error_secs = SecsNow();

	return error;
}

heli_t::heli_err_t heli_t :: GetError()	const
{
	return error;
}

/*******************************************************************************

	heli_t - Latencies

*******************************************************************************/

bool heli_t :: IsLaggingOrFuture()
{
	if (isGraph)
		return false;

	Lock();

	secs_t end = end_time;

	Unlock();

	secs_t now = SecsNow();

	// Is end not defined, in the past or in the future?
	return (end == -1) || (abs(now - end) >= param_display_heli_lag_threshold);
}

void heli_t :: ResetMeanLatencies()
{
	Lock();

	latency_data_mean.Reset();
	latency_feed_mean.Reset();

	Unlock();
}

void heli_t :: LogMeanLatencies()
{
	Lock();

	cout << SecsToString(SecsNow()) << ": LATENCY " << station->name <<
			" " << latency_data_mean <<
			" " << latency_feed_mean <<
			endl;

	Unlock();
}

secs_t heli_t :: EndTime()
{
	Lock();

	secs_t end = end_time;

	Unlock();

	return end;
}

/*******************************************************************************

	heli_t - Picking

*******************************************************************************/

void heli_t :: FreePicker()
{
	free_PickList((PickData **)picker_picks, picker_num_picks);
	picker_picks = NULL;
	picker_num_picks = 0;

	free_FilterPicker5_Memory((FilterPicker5_Memory**)&picker_mem);
	picker_mem = NULL;
}

void heli_t :: ClearPicks()
{
	picks.clear();
	new_picks.clear();
}

void heli_t :: GetNewPicks(picks_set_t & result)
{
	Lock();

	result = new_picks;

	picks.insert(new_picks.begin(),new_picks.end());
	new_picks.clear();

	Unlock();
}

/**	Update Early Warning parameters associated to this pick (quake, displacements, magnitudes)

	This method will also log a LINK message when the pick is first linked to a quake

	@param pick		updated pick to copy over corresponding heli pick
*/
void heli_t :: UpdatePick(const pick_t & pick)
{
	Lock();

	picks_set_t::iterator p = picks.find(pick);	// this is set::find and uses pick_t::operator<

	if ( p != picks.end() )
	{
		// FIXME: circumvent the fact we aren't allowed to change set elements
		*const_cast<pick_t *>(&(*p)) = pick;
	}

	Unlock();
}

// Delete picks older than the earliest sample in the buffer
void heli_t :: PurgeOldPicks()
{
	secs_t start_time = end_time - secs_t(num_samples) / samples_per_sec;

	picks_set_t::iterator p = picks.begin();
	while (p != picks.end())
	{
		if (p->t < start_time)
//			p = picks.erase(p);	// requires c++11
			picks.erase(p++);
		else
			++p;
	}
}

bool heli_t :: AddPick( const pick_t & p )
{
	bool new_pick_found = new_picks.insert( p ).second;

	PurgeOldPicks();

	return new_pick_found;
}

/**	Compute picks in the new sample packet and add them to the pick list

	@param filterWindow		in seconds, determines how far back in time the previous samples are examined.
							The filter window will be adjusted upwards to be an integer N power of 2 times the sample interval (deltaTime).
							Then numRecursive = N + 1 "filter bands" are created.
							For each filter band n = 0,N  the data samples are processed through a simple recursive filter backwards from the current sample,
							and picking statistics and characteristic function are generated.
							Picks are generated based on the maximum of the characteristic function values over all filter bands relative to the threshold values threshold1 and threshold2.

	@param longTermWindow	determines:
							a) a stabilisation delay time after the beginning of data; before this delay time picks will not be generated.
							b) the decay constant of a simple recursive filter to accumulate/smooth all picking statistics and characteristic functions for all filter bands.

	@param threshold1		sets the threshold to trigger a pick event (potential pick).
							This threshold is reached when the (clipped) characteristic function for any filter band exceeds threshold1.

	@param threshold2		sets the threshold to declare a pick (pick will be accepted when tUpEvent reached).
							This threshold is reached when the integral of the (clipped) characteristic function for any filter band over the window tUpEvent exceeds threshold2 * tUpEvent
							(i.e. the average (clipped) characteristic function over tUpEvent is greater than threshold2)..

	@param tUpEvent			determines the maximum time the integral of the (clipped) characteristic function is accumulated
							after threshold1 is reached (pick event triggered) to check for this integral exceeding threshold2 * tUpEvent (pick declared).
*/
bool heli_t :: ComputePicks(
	const float  *samples_new,
	const int    num_samples_new,
	const secs_t start_time_new,

	const double filterWindow,
	const double longTermWindow,
	const double threshold1,
	const double threshold2,
	const double tUpEvent
)
{
	bool new_picks_found = false;

	Lock();

	if (samples_per_sec != 0)
	{
		double dt = 1.0 / samples_per_sec;

		// We use picker_picks as just a buffer to hold the resulting picks of the last packet.
		// To avoid it growing arbitrarily large, free it from time to time.
		if (picker_num_picks >= 12)	// better use a threshold smaller than SIZE_INCREMENT (16)
		{
			free_PickList((PickData **)picker_picks, picker_num_picks);
			picker_picks = NULL;
			picker_num_picks = 0;
		}

		int picker_num_picks_before = picker_num_picks;

		Pick_FP5(	dt,	samples_new,	num_samples_new,
					filterWindow,		longTermWindow,		threshold1,		threshold2,		tUpEvent,
					(FilterPicker5_Memory **)&picker_mem,	TRUE_INT,
					(PickData ***)&picker_picks,
					&picker_num_picks,
					url.c_str()
		);

		for (int i = picker_num_picks_before; i < picker_num_picks; i++)
		{
			PickData *pick_fp5 = ((PickData **)picker_picks)[i];

			pick_t p(
				start_time_new + ((pick_fp5->indices[0] + pick_fp5->indices[1]) / 2 ) * dt,
				float( (pick_fp5->indices[1] - pick_fp5->indices[0]) / 2 * dt ),
				pick_fp5->polarity
			);

			if ( AddPick( p ) )
				new_picks_found = true;
		}
	}

	Unlock();

	return new_picks_found;
}

/*******************************************************************************

	heli_t - Data processing for EW parameters (Displacement etc.)

*******************************************************************************/

// Remove from a sample buffer the mean calculated over the last number of seconds, and update said mean with the new data
void heli_t :: RmeanOverOneSecPackets(float *dest, int samples_count, int sps)
{
	if ( (param_waveform_rmean_secs <= 0) || (samples_count <= 0) )
		return;

	// Loop over one-second packets
	// FIXME we assume the samples buffer is at least one second
	float *b_first = dest, *b_last = dest + sps - 1, *b_max = dest + samples_count - 1;

	for (;;)
	{
		if (b_first > b_max)
			break;

		// stop at the last sample.
		// Pretend the last packet to be one second (even if it may be as short as a single sample)
		// FIXME if the rmean window is long we may store several very short packets in the history
		if (b_last > b_max)
			b_last = b_max;

		float *b;

		// calc current packet mean
		float sum_curr = 0;
		unsigned samps_curr = unsigned(b_last - b_first + 1);
		for (b = b_first; b <= b_last; ++b)
			sum_curr += *b;

		// add current packet mean to the history
		mean_data.push_back( mean_data_t( sum_curr, samps_curr ) );

		// remove the oldest packet mean from the history, if needed
		while (mean_data.size() > unsigned(param_waveform_rmean_secs))
			mean_data.pop_front();

		// calc mean over the history (it includes the current packet)
		float sum_all = 0;
		unsigned samps_all = 0;
		for (deque<mean_data_t>::const_iterator m = mean_data.begin(); m != mean_data.end(); ++m)
		{
			sum_all		+=	m->sum;
			samps_all	+=	m->samples;
		}
		float mean_all = sum_all / samps_all;

		// remove mean over the history from the current packet
		for (b = b_first; b <= b_last; ++b)
			*b -= mean_all;

		// move to next packet
		b_first += sps;
		b_last  += sps;
	}
}

bool heli_t :: HasClipping(secs_t t0, secs_t t1)
{
	Lock();

	bool hasClipping = clipspans.Overlaps(t0, t1);

	Unlock();
	return hasClipping;
}

void heli_t :: CalcDisplacementSamples( float fmin, float fmax, secs_t pick_time, float duration, float **dest, int *num )
{
	Lock();

	// Calc displacement over a larger window than requested (it should give a more accurate integral)
	float secs_before = float(param_magnitude_secs_before_window);

	secs_t start_time = end_time - secs_t(num_samples) / NonZero(samples_per_sec);

	*num = RoundToInt( samples_per_sec * (duration+secs_before) );

	int first	=	RoundToInt( (float(pick_time - start_time) - secs_before) * samples_per_sec );
	int last	=	first + *num - 1;

	if ( end_time == -1 || first < 0 || last < 0 || first >= num_samples || last >= num_samples ||
	     HasClipping(pick_time - secs_before, pick_time + duration)	)
	{
		*dest = NULL;
		*num  = 0;

		Unlock();
		return;
	}

	float dt = 1.0f / samples_per_sec;

	float *s_first/*, *s_last*/;
	float *b_first, *b_last;

	s_first	=	&samples[first];
//	s_last	=	&samples[last];

	b_first	=	&buffer[first];
	b_last	=	&buffer[last];

	*dest = b_first;

	memcpy(b_first, s_first, (b_last - b_first + 1) * sizeof(*b_first));

	Integrate(b_first, b_last, dt);
	if (station->isAccel)
		Integrate(b_first, b_last, dt);
	Filter(b_first, b_last, fmin, fmax, dt);

	// Skip secs_before seconds to return the time window that was actually requested

	int samples_before = RoundToInt(secs_before * samples_per_sec);
	*dest += samples_before;
	*num  -= samples_before;

	Unlock();
}

/*******************************************************************************

	sac_t - SAC file helicorder derived from heli_t.
	        Handle SAC file in Init, Start, Stop, Update (data acquisition)

*******************************************************************************/

void sac_t :: Stop()
{
	heli_t :: Stop();

	sac_seq = -1;
	sac_seq_secs = -1;
	sac_seq_lag = 0;
}

void sac_t :: Start()
{
	Stop();

	CreateThread();

	// Each thread gets a unique random sequence for lag simulation (but always the same between runs)
	sac_seq_seed = 0;
	for (string::iterator sp = url.begin(); sp != url.end(); ++sp)
		sac_seq_seed = sac_seq_seed * 33 + *sp;
}

heli_t::heli_err_t sac_t :: Init(const string & filename, int _num_samples, station_t *_station)
{
	heli_t :: Init(filename, _num_samples, _station);

	FILE *f = fopen(filename.c_str(),"rb");
	if (!f)
	{
		cerr << endl << "Error, could not open file: " << filename << endl;
		return SetError(ERR_FATAL);
	}

	// Read header

	if ( fread(&hdr, sizeof(hdr), 1, f) != 1 )
	{
		fclose(f);
		Fatal_Error("Header too short in SAC file \"" + filename + "\"");
	}

	bool swap = false;

	switch( hdr.NVHDR )
	{
		case 0x00000006:	swap = false;	break;
		case 0x06000000:	swap = true;	cerr << "Byte swapping SAC file" << endl;	break;
		default:			fclose(f);
							Fatal_Error("Unsupported or invalid header in SAC file \"" + filename + "\": NVHDR should be 6");
	}

	if (swap)
		for (unsigned char *p = (unsigned char *)(&hdr)+0; p < (unsigned char *)(&hdr)+110*4; p += 4)
			swap32(p);

	// Sanity check on the header

	if (hdr.NPTS == sac_header_t::UNDEF)
		Fatal_Error("Incomplete header in SAC file \"" + filename + "\": NPTS is undefined");

	if (hdr.DELTA == sac_header_t::UNDEF)
		Fatal_Error("Incomplete header in SAC file \"" + filename + "\": DELTA is undefined");

	float samprate = GetSACSampleRate();
	if ( abs(samprate - RoundToInt(samprate)) >= .001f )
		Fatal_Error("Non-integer sample rate in SAC file \"" + filename + "\": " + ToString(samprate) + ". Use INTERP command within SAC to adjust it.");

	// Fill in incomplete header where possible

	if (hdr.B == sac_header_t::UNDEF)
		hdr.B = 0;

	if (hdr.E == sac_header_t::UNDEF)
		hdr.E = hdr.B + hdr.NPTS * 1.0f/RoundToInt(1.0f / hdr.DELTA);

	// Time of the first sample

	secs_t0 = GetSACReferenceSecs() + hdr.B;

	// Read samples

	delete [] sacsamples;
	sacsamples = new float[hdr.NPTS];
	if (!sacsamples)
	{
		cerr << "Out of memory reading " + filename << endl;
		fclose(f);
		return SetError(ERR_FATAL);
	}

	if ( fread(sacsamples, hdr.NPTS * 4, 1, f) != 1 )
	{
		fclose(f);
		Fatal_Error("Less samples than expected in SAC file \"" + filename + "\"");
	}

	fclose(f);

	// Convert to little endian (Intel format)

	if (swap)
		for (float *s = sacsamples; s < sacsamples + hdr.NPTS; s++)
			swap32((unsigned char *)s);

	// Eventually write displacement trace for debug

	if (param_simulation_write_displacement)
	{
		// For each file.sac do: mean removal, filter (low mag / high mag), integration (twice).
		// Write the results to file.sac.rmean, file.sac.filter, file.sac.disp.

		float *sacbuffer = new float[hdr.NPTS];
		if (!sacbuffer)
		{
			cerr << "Out of memory writing displacement for " + filename << endl;
			return SetError(ERR_NONE);
		}

		float dt = 1.0f/RoundToInt(1.0f / hdr.DELTA);

		const string label = "mag";

		// Loop on filter (high or low magnitude)
		bool isMagHigh = false;
		for( ;; )
		{
			float *b_first	=	&sacbuffer[0];
			float *b_last	=	&sacbuffer[hdr.NPTS-1];

			memcpy(sacbuffer, sacsamples, hdr.NPTS * sizeof(sacbuffer[0]));

			// Remove mean

			RmeanOverOneSecPackets(sacbuffer, hdr.NPTS, RoundToInt(1.0f / hdr.DELTA));

			string debugname;
			FILE *f;

			if (!isMagHigh)	// just once
			{
				debugname = filename + ".rmean";
				f = fopen(debugname.c_str(),"wb");
				if (f)
				{
					fwrite(&hdr, sizeof(hdr), 1, f);
					fwrite(sacbuffer, hdr.NPTS, 4, f);
					fclose(f);
				}
				else
				{
					cerr << "Could not open file for writing: " << debugname << endl;
				}
			}

			string mag_range_label = isMagHigh ? "high" : "low";

			// Filter (according to magnitude range)

			float fmin = isMagHigh ? float(param_magnitude_high_fmin) : float(param_magnitude_low_fmin);
			float fmax = isMagHigh ? float(param_magnitude_high_fmax) : float(param_magnitude_low_fmax);

			Filter(b_first, b_last, fmin, fmax, dt);

			debugname = filename + "." + label + "." + mag_range_label + ".filter";
			f = fopen(debugname.c_str(),"wb");
			if (f)
			{
				fwrite(&hdr, sizeof(hdr), 1, f);
				fwrite(sacbuffer, hdr.NPTS, 4, f);
				fclose(f);
			}
			else
			{
				cerr << "Could not open file for writing: " << debugname << endl;
			}

			// Integrate to obtain displacement

			Integrate(b_first, b_last, dt);
			if (station->isAccel)
				Integrate(b_first, b_last, dt);

			debugname = filename + "." + label + "." + mag_range_label + ".disp";
			f = fopen(debugname.c_str(),"wb");
			if (f)
			{
				fwrite(&hdr, sizeof(hdr), 1, f);
				fwrite(sacbuffer, hdr.NPTS, 4, f);
				fclose(f);
			}
			else
			{
				cerr << "Could not open file for writing: " << debugname << endl;
			}

			if (isMagHigh)
				break;
			else
				isMagHigh = true;
		}

		delete [] sacbuffer;
	}

	return SetError(ERR_NONE);
}

void sac_t :: SetRandLag()
{
	if (param_simulation_lag_mean > 0 || param_simulation_lag_sigma > 0)
	{
		sac_seq_lag = GaussianRand(&sac_seq_seed, float(param_simulation_lag_mean), float(param_simulation_lag_sigma));
		if (sac_seq_lag < 0)
			sac_seq_lag = 0;
	}
	else 
		sac_seq_lag = 0;

//	cout << SecsToString(SecsNow()) << ": LAG " << station->name << " " << sac_seq_lag << endl;
}

// Return a new packet of data. The object is being concurrently read (e.g. by the rendering thread) and in some cases written.
// So Lock as appropriate, but keep locking to a minimum to avoid stalling e.g. the rendering.
heli_t::heli_err_t sac_t :: GetData(float *& samples_new, int & num_samples_new, float & samples_per_sec_new, secs_t & end_time_new)
{
	const float secs_per_packet = 1.0f;

	if (GetError() == ERR_FATAL)
		return SetError(ERR_FATAL);

	if (simutime_t :: GetPaused())
		return SetError(ERR_NODATA);

	// t0 is the time passed since the SAC begin time

	secs_t begin_secs = GetSACReferenceSecs() + hdr.B;
	secs_t t0 = simutime_t :: Get() - begin_secs;

	// No data before begin time

	if (t0 < 0)
		return SetError(ERR_NODATA);

	// The packet starting at "sac_seq_secs" seconds after the begin time is pending

	if (sac_seq_secs == -1)
	{
		sac_seq_secs = 0;	// these shouldn't be accessed by other threads other than acquisition
		SetRandLag();
	}

	// No data if the pending packet starts after the SAC end time

	if (sac_seq_secs > hdr.E)
		return SetError(ERR_NODATA);

	// Wait past the last sample of the pending packet (plus optional transmission lag)

	if (t0 < sac_seq_secs + secs_per_packet + sac_seq_lag)
		return SetError(ERR_NODATA);

	// Return the pending packet of data

	t0 = sac_seq_secs;
	sac_seq_secs += secs_per_packet;
	SetRandLag();

	// Calc an *integer* samples-per-second value from DELTA
	// (1/DELTA is an approximation, such as 124.9999)
	samples_per_sec_new = float(RoundToInt(1.0f / hdr.DELTA));

	int sample0 = int(t0 * samples_per_sec_new);
	int sample1 = int(sample0 + samples_per_sec_new * secs_per_packet - 1);

	Clamp(sample0, 0, hdr.NPTS-1);
	Clamp(sample1, 0, hdr.NPTS-1);

	samples_new		=	sacsamples + sample0;
	num_samples_new =	sample1 - sample0 + 1;

	end_time_new	=	begin_secs + (sample1 + 1) / samples_per_sec_new;

//	cerr << std::fixed << std::setprecision(2) << simutime_t :: Get() << " " << GetSACStation() << "." << GetSACComponent() << ":" << sac_seq_secs << " " << num_samples_new << " " << samples_per_sec_new  << " " << end_time_new << endl;

	return SetError(ERR_NONE);
}

string sac_t :: KtoString(const char *k) const
{
	string s;
	s.clear();

	int i,j;

	for(j = 7; j >= 0; j--)
		if (k[j] != ' ' && k[j] != 0)
			break;

	for(i = 0; i <= j; i++)
		s += k[i];

	return s;
}

/*******************************************************************************

	sac_t - Header

*******************************************************************************/

string sac_t :: GetSACStation() const
{
	string ret;
	ret = KtoString( hdr.KSTNM );
	if ( ret.size() )
		return ret;

	// If the station name is not set in the header, use the first part of the filename (up to the first dot)

	ret = StripPath(url);
	return ret.substr(0,ret.find('.'));
}

bool sac_t :: GetSACEvent(float *lon, float *lat, float *dep, float *mag) const
{
	*lon = hdr.EVLO;
	*lat = hdr.EVLA;
	*dep = hdr.EVDP;
	*mag = hdr.UNUSED1;
	return (*lon != sac_header_t :: UNDEF) && (*lat != sac_header_t :: UNDEF) && (*dep != sac_header_t :: UNDEF);
}

float sac_t :: GetSACSampleRate() const
{
	return 1.0f / hdr.DELTA;
}

char sac_t :: ValidComponent( char cmp ) const
{
	switch ( toupper(cmp) )
	{
		case '0':
		case 'U':
		case 'Z':	return 'Z';

		case '1':
		case 'N':	return 'N';

		case '2':
		case 'E':	return 'E';
	}
	return 0;
}

char sac_t :: GetSACComponent() const
{
	string ret;

	ret = KtoString( hdr.KCMPNM );
	if ( ret.size() && ValidComponent( ret[ret.size()-1] ) )
		return ValidComponent( ret[ret.size()-1] );

	ret = ToUpper(StripPath(url));

	if (	(ret.find("_UD.")  != string::npos)	||
			(ret.find(".UD.")  != string::npos)	||
			(ret.find(".UD1.") != string::npos)	||
			(ret.find(".U.")   != string::npos)	||
			(ret.find("_U.")   != string::npos)	||
			(ret.find(".Z.")   != string::npos)	||
			(ret.find("_Z.")   != string::npos)	||
			(ret.find(".C00.") != string::npos)	||
			(ret.find(".0.")   != string::npos)	)
		return 'Z';

	if (	(ret.find("_NS.")  != string::npos)	||
			(ret.find(".NS.")  != string::npos)	||
			(ret.find(".N.")   != string::npos)	||
			(ret.find("_N.")   != string::npos)	||
			(ret.find(".C01.") != string::npos)	||
			(ret.find(".1.")   != string::npos)	)
		return 'N';

	if (	(ret.find("_EW.")  != string::npos)	||
			(ret.find(".EW.")  != string::npos)	||
			(ret.find(".E.")   != string::npos)	||
			(ret.find("_E.")   != string::npos)	||
			(ret.find(".C02.") != string::npos)	||
			(ret.find(".2.")   != string::npos)	)
		return 'E';

//	ret = ret.substr(ret.find('.')+1,1);
//	if ( ret.size() && ValidComponent( ret[0] ) )
//		return ValidComponent( ret[0] );

	return '?';
}

secs_t sac_t :: GetSACReferenceSecs() const
{
	int month, mday;

	int year	=	(hdr.NZYEAR != sac_header_t::UNDEF) ? hdr.NZYEAR : 1970;
	int jday	=	(hdr.NZJDAY != sac_header_t::UNDEF) ? hdr.NZJDAY : 1;
	int hour	=	(hdr.NZHOUR != sac_header_t::UNDEF) ? hdr.NZHOUR : 0;
	int min		=	(hdr.NZMIN  != sac_header_t::UNDEF) ? hdr.NZMIN  : 0;
	int sec		=	(hdr.NZSEC  != sac_header_t::UNDEF) ? hdr.NZSEC  : 0;
	int msec	=	(hdr.NZMSEC != sac_header_t::UNDEF) ? hdr.NZMSEC : 0;

	sl_doy2md(year, jday, &month, &mday);
	tm sac_tm = { sec, min, hour, mday, month-1, year-1900, 0, 0, 0 };
	time_t now = time(NULL);
	tm tm_buf;
	time_t dtime = mktime(mylocaltime_r(&now, &tm_buf)) - mktime(mygmtime_r(&now, &tm_buf));
	time_t sac_time = mktime(&sac_tm) + dtime;	// local time -> GMT

	return secs_t(sac_time) + secs_t(msec/1000.0f);
}

/*******************************************************************************

	timeseries_t - Magnitude graph derived from heli_t

*******************************************************************************/

void timeseries_t :: Stop()
{
	heli_t :: Stop();

	data.clear();
}

void timeseries_t :: Start()
{
	Stop();

	if (CreateThread() != ERR_NONE)
		return;

	SetError(ERR_NONE);
}

// Return a new packet of data. The object is being concurrently read (e.g. by the rendering thread) and in some cases written.
// So Lock as appropriate, but keep locking to a minimum to avoid stalling e.g. the rendering.
heli_t::heli_err_t timeseries_t :: GetData(float *& samples_new, int & num_samples_new, float & samples_per_sec_new, secs_t & end_time_new)
{
	Lock();	// data vector may be being modified by the Add method from the main thread

	if (data.empty())
	{
		Unlock();
		return SetError(ERR_NODATA);
	}

	sample_t sample = data.front();
	data.pop_front();

	Unlock();

	const int	rate	=	sizeof(buf) / sizeof(buf[0]);
	const int	num		=	(10+4+10);

	for (int i = 0; i < num; i++)
	{
		if ( i != num/2-1 && i != num/2 && i != num/2+1 && i != num/2+2 )
			buf[i] = sample.val_avg;
		else
			buf[i] = (i & 1) ? sample.val_max : sample.val_min;
	}

	end_time_new		=	sample.time + secs_t(num/2) / rate;
	samples_new			=	buf;
	num_samples_new		=	num;
	samples_per_sec_new =	rate;

	return SetError(ERR_NONE);
}

void timeseries_t :: Add(secs_t time, float val_min, float val_avg, float val_max)
{
	Lock();

	sample_t sample = { time, val_min, val_avg, val_max };

	data.push_back( sample );

	Unlock();
}

void timeseries_t :: SetMarker(secs_t time)
{
	Lock();

	ClearPicks();
	AddPick( pick_t(time,0.1f,0) );
	picks_set_t temp;
	GetNewPicks(temp);

	Unlock();
}

/*******************************************************************************

	slink_t - SeedLink helicorder
	          Handle SeedLink stream in Init, Start, Stop, Update (data acquisition)

*******************************************************************************/

void slink_t :: Stop()
{
	heli_t :: Stop();

	ip.clear();
	streams.clear();

	if (slconn != NULL)
	{
		sl_disconnect(slconn);

		slconn->sladdr = NULL;	// sl_freeslcd wants to free it !?
		sl_freeslcd(slconn);
		slconn = NULL;
	}

	if (msr != NULL)
	{
		sl_msr_free(&msr);
		msr = NULL;
	}
}

void slink_t :: Start()
{
	Stop();

	slconn = sl_newslcd();
	if (slconn == NULL)
	{
		SetError(ERR_FATAL);
		return;
	}

	string::size_type pos = url.find('/');

	ip		=	url.substr(0,pos);
	streams	=	url.substr(pos+1);

	slconn->sladdr		=	const_cast<char *>( ip.c_str() );
	slconn->netto		=	RoundToInt(float(param_slink_timeout_secs));
	slconn->netdly		=	RoundToInt(float(param_slink_delay_secs));
	slconn->keepalive	=	RoundToInt(float(param_slink_keepalive_secs));

	msr = sl_msr_new();
	if (msr == NULL)
	{
		SetError(ERR_FATAL);
		return;
	}

	if (sl_parse_streamlist(slconn, streams.c_str(), "") != 1)
	{
		SetError(ERR_FATAL);
		return;
	}

	if (CreateThread() != ERR_NONE)
		return;

	SetError(ERR_NONE);
}

heli_t::heli_err_t slink_t :: Init(const string & _url, int _num_samples, station_t *_station)
{
	Stop();
	heli_t :: Init(_url, _num_samples, _station);

	return SetError(ERR_NONE);
}

// Return a new packet of data. The object is being concurrently read (e.g. by the rendering thread) and in some cases written.
// So Lock as appropriate, but keep locking to a minimum to avoid stalling e.g. the rendering.
heli_t::heli_err_t slink_t :: GetData(float *& samples_new, int & num_samples_new, float & samples_per_sec_new, secs_t & end_time_new)
{
	if (slconn == NULL)
		return SetError(ERR_FATAL);

	SLpacket *slpack;
	int err = sl_collect_nb(slconn, &slpack);
	switch (err)
	{
		case SLPACKET:		break;

		case SLNOPACKET:	if (slconn->link == -1)
									return SetError(ERR_FATAL);
							else	return SetError(ERR_NODATA);

		default:					return SetError(ERR_FATAL);
	}

	if (sl_packettype(slpack) != SLDATA)
		return SetError(ERR_NODATA);

	if (sl_msr_parse(NULL, slpack->msrecord, &msr, 1, 1) == NULL)
		return SetError(ERR_NODATA);

	// Samples
	samples_new = (float *)msr->datasamples;
	if (samples_new == NULL)
		return SetError(ERR_NODATA);

	// Number of samples
	num_samples_new		=	msr->numsamples;
	if (num_samples_new <= 0)
		return SetError(ERR_NODATA);

	// Sample rate
	double samprate;
	sl_msr_dsamprate(msr, &samprate);
	samples_per_sec_new	=	float(samprate);
	if (samples_per_sec_new <= 0.0f || samples_per_sec_new > 2000.0f)
		return SetError(ERR_NODATA);

	// End time
	double depochtime = sl_msr_depochstime(msr);
	end_time_new	=	secs_t(depochtime) + secs_t(num_samples_new) / samples_per_sec_new;
	if (end_time_new <= 0.0f || abs(SecsNow() - end_time_new) >= 3600*24.0f)
		return SetError(ERR_NODATA);

	// Copy samples, converting from ints to floats
	// FIXME: assert, possibly at compile time, if sizeof(int) != sizeof(float)
	float *samples_end = (float *)(msr->datasamples+num_samples_new);
	for (float *s = samples_new; s < samples_end; s++)
		*s = float(*((int *)s));

	return SetError(ERR_NONE);
}
