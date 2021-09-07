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

	Defines, variables and classes dealing with the configuration file (user preferences)

*******************************************************************************/

#ifndef CONFIG_H_DEF
#define CONFIG_H_DEF

#include "SDL.h"

#include "global.h"

// Configuration file

void Load_Config();
void Load_Params();

// Video modes

class videomodes_t
{
public:

	videomodes_t();

	void GetNames(vector<string>& names) const;

	int FindIndexOfClosest(int req_x, int req_y) const;

	void GetSize(int index, int *x, int *y) const;

private:

	vector<SDL_Rect> modes;
};

const videomodes_t & VideoModes();

void Create_Screen(int screen_i);
void Destroy_Screen();
void Swap_Screen();

// User configurable parameters given at the command line:

extern string net_name, event_name, net_dir, sacs_dir;
extern bool realtime;

// User configurable parameters stored in the configuration files:

extern int
		config_screen_index,
		config_screen_w,
		config_screen_h,
		config_fullscreen,
		config_vsync,
		config_sound;

extern double
		param_simulation_speed,
		param_simulation_write_displacement,
		param_simulation_movie_period,
		param_simulation_lag_mean,
		param_simulation_lag_sigma;

extern double
		param_debug_gaps_period,
		param_debug_gaps_duration,
		param_debug_save_rtloc,
		param_debug_save_rtmag;

extern double
		param_display_heli_min_accel,
		param_display_heli_min_vel,
		param_display_heli_secs,
		param_display_heli_width,
		param_display_heli_max_num,
		param_display_heli_lag_threshold,
		param_display_map_fixed_size,
		param_display_map_station_scale,
		param_display_real_quake,
		param_display_heli_show_mag;

extern double
		param_slink_timeout_secs,
		param_slink_delay_secs,
		param_slink_keepalive_secs,
		param_slink_log_verbosity;

extern int
		param_waveform_rmean_secs;
extern double
		param_waveform_clipping_secs,
		param_waveform_min_snr;

extern double
		param_picker_filterWindow,		// in seconds, determines how far back in time the previous samples are examined.  The filter window will be adjusted upwards to be an integer N power of 2 times the sample interval (deltaTime).  Then numRecursive = N + 1 "filter bands" are created.  For each filter band n = 0,N  the data samples are processed through a simple recursive filter backwards from the current sample, and picking statistics and characteristic function are generated.  Picks are generated based on the maximum of the characteristic function values over all filter bands relative to the threshold values threshold1 and threshold2.
		param_picker_longTermWindow,	// determines: a) a stabilisation delay time after the beginning of data; before this delay time picks will not be generated. b) the decay constant of a simple recursive filter to accumulate/smooth all picking statistics and characteristic functions for all filter bands.
		param_picker_threshold1,		// sets the threshold to trigger a pick event (potential pick).  This threshold is reached when the (clipped) characteristic function for any filter band exceeds threshold1.
		param_picker_threshold2,		// sets the threshold to declare a pick (pick will be accepted when tUpEvent reached).  This threshold is reached when the integral of the (clipped) characteristic function for any filter band over the window tUpEvent exceeds threshold2 * tUpEvent (i.e. the average (clipped) characteristic function over tUpEvent is greater than threshold2)..
		param_picker_tUpEvent;			// determines the maximum time the integral of the (clipped) characteristic function is accumulated after threshold1 is reached (pick event triggered) to check for this integral exceeding threshold2 * tUpEvent (pick declared).

extern double
		param_binder_stations_for_coincidence,
		param_binder_secs_for_coincidence,
		param_binder_secs_for_association,
		param_binder_quakes_separation,
		param_binder_quakes_life,
		param_binder_apparent_vel_min,
		param_binder_apparent_vel_max,
		param_binder_apparent_vel_stations_spacing,
		param_binder_apparent_vel_max_distance;

extern double
		param_locate_period,
		param_locate_force_sac,
		param_locate_force_lon,
		param_locate_force_lat,
		param_locate_force_dep,
		param_locate_use_non_triggering_stations,
		param_locate_ignore_error;

extern double
		param_magnitude_max_value,
		param_magnitude_outlier_threshold,
		param_magnitude_low_fmin,
		param_magnitude_low_fmax,
		param_magnitude_high_threshold,
		param_magnitude_high_fmin,
		param_magnitude_high_fmax,
		param_magnitude_secs_before_window,
		param_magnitude_p_secs_short,
		param_magnitude_p_secs_long,
		param_magnitude_s_secs,
		param_magnitude_p_can_overlap_s;

extern double
		param_alarm_heartbeat_secs,
		param_alarm_during_simulation,
		param_alarm_max_period;

extern double
		param_latency_log_period_secs;

#endif
