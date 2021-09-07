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

#include <fstream>
#include <iomanip>
#include <map>
#include "SDL.h"
#include "SDL_image.h"

#include "config.h"

#include "graphics2d.h"
#include "version.h"
#include "sac_header.h"

// Tweakable parameters:

const string	CONFIG_FILENAME		=		"config.txt";
string			PARAMS_FILENAME;

string net_name, event_name, net_dir, sacs_dir;
bool realtime = true;

int
		config_screen_index,
		config_screen_w,
		config_screen_h,
		config_fullscreen,
		config_vsync,
		config_sound;

double
		param_simulation_speed,
		param_simulation_write_displacement,
		param_simulation_movie_period,
		param_simulation_lag_mean,
		param_simulation_lag_sigma;

double
		param_debug_gaps_period,
		param_debug_gaps_duration,
		param_debug_save_rtloc,
		param_debug_save_rtmag;

double
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

double
		param_slink_timeout_secs,
		param_slink_delay_secs,
		param_slink_keepalive_secs,
		param_slink_log_verbosity;

int
		param_waveform_rmean_secs;
double
		param_waveform_clipping_secs,
		param_waveform_min_snr;

double
		param_picker_filterWindow,
		param_picker_longTermWindow,
		param_picker_threshold1,
		param_picker_threshold2,
		param_picker_tUpEvent;

double
		param_binder_stations_for_coincidence,
		param_binder_secs_for_coincidence,
		param_binder_secs_for_association,
		param_binder_quakes_separation,
		param_binder_quakes_life,
		param_binder_apparent_vel_min,
		param_binder_apparent_vel_max,
		param_binder_apparent_vel_stations_spacing,
		param_binder_apparent_vel_max_distance;

double
		param_locate_period,
		param_locate_force_sac,
		param_locate_force_lon,
		param_locate_force_lat,
		param_locate_force_dep,
		param_locate_use_non_triggering_stations,
		param_locate_ignore_error;

double
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

double
		param_alarm_heartbeat_secs,
		param_alarm_during_simulation,
		param_alarm_max_period;

double
		param_latency_log_period_secs;

/*******************************************************************************

	Video modes

*******************************************************************************/
/*
void videomodes_t :: GetSize(int index, int *x, int *y) const
{
	*x = modes[index].w;
	*y = modes[index].h;
}


void videomodes_t :: GetNames(vector<string>& names) const
{
	names.clear();
	int i,n = modes.size();
	for (i = 0; i < n; i++)
	{
		int x,y;
		GetSize(i, &x, &y);
		names.push_back(ToString(x) + " x " + ToString(y));
	}
}


int videomodes_t :: FindIndexOfClosest(int req_x, int req_y) const
{
	int best_eval = -1;
	int best_i = -1;

	int req_area = req_x * req_y;

	int modesN = modes.size();
	for (int i = 0; i < modesN; i++)
	{
		int eval = abs(modes[i].w*modes[i].h - req_area);
		if ((best_i < 0) || (eval < best_eval))
		{
			best_eval = eval;
			best_i = i;
		}
	}

	return best_i;
}


const videomodes_t & VideoModes()
{
	static videomodes_t videomodes;
	return videomodes;
}


videomodes_t :: videomodes_t()
{
	// Available screen modes (width and height) and their number.
	// Fullscreen must be enforced or there would be no constraints
	SDL_PixelFormat format;

	format.palette = NULL;
	format.BitsPerPixel = 24;
	format.BytesPerPixel = 3;
#if SDL_BYTEORDER == SDL_LIL_ENDIAN
	format.Rshift = 0;
	format.Gshift = 8;
	format.Bshift = 16;
#else
	format.Rshift = 24;
	format.Gshift = 16;
	format.Bshift = 8;
#endif
	format.Ashift = 0;
	format.Rmask = 0xff << format.Rshift;
	format.Gmask = 0xff << format.Gshift;
	format.Bmask = 0xff << format.Bshift;
	format.Amask = 0;
	format.Rloss = 0;
	format.Gloss = 0;
	format.Bloss = 0;
	format.Aloss = 8;
//	format.colorkey = 0;
//	format.alpha = 0;

	SDL_Rect **rects = SDL_ListModes( &format, SDL_WINDOW_FULLSCREEN );

	// Insert the resolutions into our vector of modes, from smaller to bigger
	// (the opposite of how SDL returned them in rects)
	if ( rects == (SDL_Rect **)0 )
	{
		Fatal_Error("No video mode is compatible.");
	}
	else if ( rects == (SDL_Rect **)-1 )
	{
		// We can choose any resolution (no full screen mode available ???)
		SDL_Rect default_modes[] =		// 4/3 aspect ratio windows
		{	{  320,  240 },
			{  400,  300 },
			{  512,  384 },
			{  640,  480 },
			{  800,  600 },
			{ 1024,  768 },
			{ 1152,  864 },
			{ 1280,  960 }	};

  		for (int i = 0; i < sizeof(default_modes)/sizeof(default_modes[0]); i++)
			modes.push_back( default_modes[i] );
	}
	else
	{
		for(int i = 0; rects[i]; i++)
			modes.insert(modes.begin(),*rects[i]);
	}
}
*/
SDL_Window *win = NULL;
SDL_GLContext glcontext = NULL;

void Swap_Screen()
{
	SDL_GL_SwapWindow(win);
}

void Destroy_Screen()
{
	if (glcontext && win != NULL)
	{
		SDL_GL_MakeCurrent(win,NULL);
		SDL_GL_DeleteContext(glcontext);
	}
}

void Create_Screen(int screen_i)
{
	if (!config_screen_w && !config_screen_h)
	{
		// Automatic window resolution

		SDL_DisplayMode dmode;
		if (SDL_GetCurrentDisplayMode(screen_i, &dmode) < 0)
			Fatal_Error("Failed automatic window size selection.\nTry specifying non-zero window width and height in " + PATH_CONFIG + CONFIG_FILENAME);

		// 90% of the Desktop resolution in windowed mode, same as the Desktop resolution in full-screen
		const float smaller = config_fullscreen ? 1.0f : 0.9f;
		if (dmode.w >= dmode.h)
		{
			screen_h = int(dmode.h * smaller / 3) * 3;
			screen_w = screen_h / 3 * 4;

			if (dmode.w < int(screen_w * smaller))
			{
				screen_w = int(dmode.w * smaller / 4) * 4;
				screen_h = screen_w / 4 * 3;
			}
		}
		else
		{
			screen_w = int(dmode.w * smaller / 4) * 4;
			screen_h = screen_w / 4 * 3;

			if (dmode.h < int(screen_h * smaller))
			{
				screen_h = int(dmode.h * smaller / 3) * 3;
				screen_w = screen_h / 3 * 4;
			}
		}
	}
	else
	{
		// User-specified window resolution

		screen_w = config_screen_w;
		screen_h = config_screen_h;
	}

	// Adjust resolution to those available in full-screen mode
/*
	if ( config_fullscreen )
	{
		config_screen_index	=	VideoModes().FindIndexOfClosest(screen_w, screen_h);
		VideoModes().GetSize(config_screen_index, &screen_w, &screen_h);
	}
*/

	cerr << "Creating a " << screen_w << " x " << screen_h << " window on screen " << screen_i << endl;

	int num_screens = SDL_GetNumVideoDisplays();
	if (num_screens < 0)
		Fatal_Error(SDL_GetError());
	else if (screen_i > num_screens-1)
	{
		cerr << "Can't open user supplied screen " << screen_i << ": only " << num_screens << " screen(s) available." << endl;
		cerr << "Using screen 0.." << endl;
		screen_i = 0;
	}

	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

	win = SDL_CreateWindow(
		APP_TITLE.c_str(),
		SDL_WINDOWPOS_CENTERED_DISPLAY(screen_i), SDL_WINDOWPOS_CENTERED_DISPLAY(screen_i),
		screen_w, screen_h,
		SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | (config_fullscreen ? SDL_WINDOW_FULLSCREEN : 0)
	);
	if (win == NULL)
		Fatal_Error(SDL_GetError());

	int wx, wy, ww, wh;
	SDL_GetWindowPosition(win, &wx, &wy);
	SDL_GetWindowSize(win, &ww, &wh);
	cerr << "Created a " << ww << " x " << wh << " window at coordinates: " << wx << ", " << wy << endl;

	// Window icon

	string filename_icon = texptr_t::path + "app_icon.png";
	cerr << "Loading icon from image file \"" << filename_icon << "\"" << endl;

	SDL_Surface *icon = IMG_Load(filename_icon.c_str());
	if (icon)
	{
		SDL_SetWindowIcon(win, icon);
		SDL_FreeSurface(icon);
	}
	else
		cerr << "***ERROR: SDL_image - " << SDL_GetError() << endl;

	// OpenGL Context

	glcontext = SDL_GL_CreateContext(win);
	if (!glcontext)
		Fatal_Error(SDL_GetError());

	if (SDL_GL_MakeCurrent(win, glcontext) < 0)
		Fatal_Error(SDL_GetError());
}

/*******************************************************************************

	Configuration files

*******************************************************************************/

// Sanity checks on the parameters

string Check_Params()
{
	string errors;

	if (param_simulation_movie_period && param_simulation_movie_period < 0.1)
		errors += "\n\"simulation_movie_period\" must be 0 (disabled) or greater than 0.1\n";

	if (param_display_heli_width < 0.1 || param_display_heli_width > 0.9)
		errors += "\n\"display_heli_width\" must be in the range [0.1, 0.9]\n";

	if (param_display_heli_max_num < 1)
		errors += "\n\"display_heli_max_num\" must be greater or equal to 1\n";

	if (param_display_heli_lag_threshold < 1.0)
		errors += "\n\"display_heli_lag_threshold\" must be greater or equal to 1.0\n";

	// Mag
	if (param_magnitude_max_value < 0 || param_magnitude_max_value > 10)
		errors += "\n\"magnitude_max_value\" must be in the range [0,10]\n";

	// Mag windows
	if (param_magnitude_p_secs_short < 0 || param_magnitude_p_secs_short > 9)
		errors += "\n\"magnitude_p_secs_short\" must be in the range [0,9]\n";
	if (param_magnitude_p_secs_long < 0 || param_magnitude_p_secs_long > 9)
		errors += "\n\"magnitude_p_secs_long\" must be in the range [0,9]\n";
	if (param_magnitude_s_secs < 0 || param_magnitude_s_secs > 9)
		errors += "\n\"magnitude_s_secs\" must be in the range [0,9]\n";
	if (param_magnitude_p_secs_short > param_magnitude_p_secs_long)
		errors += "\n\"magnitude_p_secs_long\" must be greater than \"magnitude_p_secs_short\"\n";

	// Frequencies
	if ((param_magnitude_low_fmin <= 0) || (param_magnitude_low_fmax <= 0) || (param_magnitude_low_fmin >= param_magnitude_low_fmax))
		errors += "\nInvalid frequencies, it must be:\n 0 < \"magnitude_low_fmin\" < \"magnitude_low_fmax\"\n";
	if ((param_magnitude_high_fmin <= 0) || (param_magnitude_high_fmax <= 0) || (param_magnitude_high_fmin >= param_magnitude_high_fmax))
		errors += "\nInvalid frequencies, it must be:\n 0 < \"magnitude_high_fmin\" < \"magnitude_high_fmax\"\n";

	// Periods
	if (param_simulation_movie_period && param_simulation_movie_period < 0.1)
		errors += "\n\"simulation_movie_period\" must be 0 (disabled) or greater than 0.1\n";

	if (param_latency_log_period_secs && param_latency_log_period_secs < 60)
		errors += "\n\"latency_log_period_secs\" must be 0 (disabled) or greater than 60 (1 minute)\n";

	if (param_waveform_clipping_secs && param_waveform_clipping_secs < 1)
		errors += "\n\"waveform_clipping_secs\" must be 0 (disabled) or greater than 1.0\n";

	if (param_alarm_max_period < 0.2)
		errors += "\n\"alarm_max_period\" must be 0.2 seconds or greater\n";

	return errors;
}

// Load the parameters from file

void Load_Params()
{
	PARAMS_FILENAME = PATH_CONFIG + "params.txt";

	string filename = PARAMS_FILENAME;
	ifstream f(filename.c_str());

	// Read the configuration data from file into a map of <name,value> pairs

	string errors = "";

	typedef	map<string,double> cfgmap_t;
	cfgmap_t cfgmap;
	if (!f)
	{
		Fatal_Error( "Parameters file \"" + filename + "\" is not accessible." );
	}
	else
	{
		cerr << "Loading parameters from \"" << filename << "\"" << endl;

		for(;;)
		{
			string name;
			double value;

			SkipComments(f);

			f >> name >> value;

			if (f.fail())
			{
				if (name.empty() && f.eof())
					break;

				Fatal_Error("Parsing parameter \"" + name + "\" in file \"" + filename + "\".");
			}

			// Error out on duplicates

			if (cfgmap.find(name) == cfgmap.end())
				cfgmap[name] = value;
			else
				errors += "\nDuplicate parameter: \"" + name + "\".\n";
		}
	}

	cfgmap_t :: iterator i;

	#define READ_PARAM(_s,_default)								\
		i = cfgmap.find(#_s);									\
																\
		if (i != cfgmap.end())									\
		{														\
			param_##_s = i->second;								\
			cfgmap.erase(i);									\
			cout << setw(40) << #_s" = " << param_##_s << endl;	\
		}														\
		else													\
		{														\
			errors += "\nMissing parameter \"" + ToString(#_s) + "\" (default value: " + ToString(_default) + ").\n";	\
		}

	cout << endl;
	cout << "==================================================================================================" << endl;
	cout << "    Parameters (" << filename << ")" << endl;
	cout << "==================================================================================================" << endl;

	// Simulation

	READ_PARAM(		simulation_speed,						1.0		)
	READ_PARAM(		simulation_write_displacement,			0.0		)
	READ_PARAM(		simulation_movie_period,				0.0		)
	READ_PARAM(		simulation_lag_mean,					0.0		)
	READ_PARAM(		simulation_lag_sigma,					0.0		)

	// Debug

	READ_PARAM(		debug_gaps_period,						0.0		)
	READ_PARAM(		debug_gaps_duration,					0.0		)
	READ_PARAM(		debug_save_rtloc,						0.0		)
	READ_PARAM(		debug_save_rtmag,						0.0		)

	// Display

	READ_PARAM(		display_heli_min_accel,					0.01	)
	READ_PARAM(		display_heli_min_vel,					0.001	)
	READ_PARAM(		display_heli_secs,						60.0	)
	READ_PARAM(		display_heli_width,						0.55	)
	READ_PARAM(		display_heli_max_num,					100		)
	READ_PARAM(		display_heli_lag_threshold,				10.0	)
	READ_PARAM(		display_map_fixed_size,					1		)
	READ_PARAM(		display_map_station_scale,				1.0		)
	READ_PARAM(		display_real_quake,						1		)
	READ_PARAM(		display_heli_show_mag,					1		)

	// SeedLink

	READ_PARAM(		slink_timeout_secs,						60		)
	READ_PARAM(		slink_delay_secs,						10		)
	READ_PARAM(		slink_keepalive_secs,					0		)
	READ_PARAM(		slink_log_verbosity,					0		)

	// Waveform

	READ_PARAM(		waveform_rmean_secs,					30		)
	READ_PARAM(		waveform_clipping_secs,					30.0	)
	READ_PARAM(		waveform_min_snr,						5.0		)

	// Picker

	READ_PARAM(		picker_filterWindow,					0.5		)
	READ_PARAM(		picker_longTermWindow,					5.0		)
	READ_PARAM(		picker_threshold1,						10.0	)
	READ_PARAM(		picker_threshold2,						10.0	)
	READ_PARAM(		picker_tUpEvent,						0.5		)

	// Binder

	READ_PARAM(		binder_stations_for_coincidence,		3.0		)
	READ_PARAM(		binder_secs_for_coincidence,			2.0		)
	READ_PARAM(		binder_secs_for_association,			15.0	)
	READ_PARAM(		binder_quakes_separation,				30.0	)
	READ_PARAM(		binder_quakes_life,						40.0	)
	READ_PARAM(		binder_apparent_vel_min,				3.85	)
	READ_PARAM(		binder_apparent_vel_max,				20.0	)
	READ_PARAM(		binder_apparent_vel_stations_spacing,	30.0	)
	READ_PARAM(		binder_apparent_vel_max_distance,		120.0	)

	// Locate

	READ_PARAM(		locate_period,							0.5		)
	READ_PARAM(		locate_force_sac,						0.0		)
	READ_PARAM(		locate_force_lon,						sac_header_t::UNDEF	)
	READ_PARAM(		locate_force_lat,						sac_header_t::UNDEF	)
	READ_PARAM(		locate_force_dep,						sac_header_t::UNDEF	)
	READ_PARAM(		locate_use_non_triggering_stations,		1.0		)
	READ_PARAM(		locate_ignore_error,					0.0		)

	// Magnitude

	READ_PARAM(		magnitude_max_value,					8.0		)
	READ_PARAM(		magnitude_outlier_threshold,			3.5		)
	READ_PARAM(		magnitude_low_fmin,						1.0		)
	READ_PARAM(		magnitude_low_fmax,						25.0	)
	READ_PARAM(		magnitude_high_threshold,				5.0		)
	READ_PARAM(		magnitude_high_fmin,					0.075	)
	READ_PARAM(		magnitude_high_fmax,					3.0		)
	READ_PARAM(		magnitude_secs_before_window,			5.0		)
	READ_PARAM(		magnitude_p_secs_short,					2.0		)
	READ_PARAM(		magnitude_p_secs_long,					4.0		)
	READ_PARAM(		magnitude_s_secs,						2.0		)
	READ_PARAM(		magnitude_p_can_overlap_s,				0.0		)

	// Alarms

	READ_PARAM(		alarm_heartbeat_secs,					60.0	)
	READ_PARAM(		alarm_during_simulation,				0.0		)
	READ_PARAM(		alarm_max_period,						1.0		)

	// Latencies

	READ_PARAM(		latency_log_period_secs,				60.0*10	)

	#undef READ_PARAM

	for (cfgmap_t::const_iterator c = cfgmap.begin(); c != cfgmap.end(); c++)
	{
		errors += "\nUnknown parameter \"" + c->first + "\".\n";
	}

	cout << "==================================================================================================" << endl;

	// Sanity checks

	if (errors.empty())	// in order to not perform sanity checks on missing or duplicate parameters
		errors = Check_Params();

	// Report all errors and exit in that case

	if (!errors.empty())
		Fatal_Error("Errors found in file \"" + filename + "\":\n" + errors);
}

// Sanity checks on the configuration

string Check_Config()
{
	string errors;

	if ( config_screen_w < 0 || config_screen_h < 0 || config_screen_w * 3 != 4 * config_screen_h )	// including 0,0
		errors +=	"\nInvalid \"screen_w\" or \"screen_h\" value.\n"
					"They specify the window width and height in pixels (or 0 for auto).\n"
					"Also make sure width/height equals 4/3\n"
					"(e.g. 1280x960, 1024x768, 800x600, ...)\n";

	return errors;
}

// Load the configuration from file

void Load_Config()
{
	string filename = PATH_CONFIG + CONFIG_FILENAME;
	ifstream f(filename.c_str());

	// Read the configuration data from file into a map of <name,value> pairs

	string errors = "";

	typedef	map<string,int> cfgmap_t;
	cfgmap_t cfgmap;
	if (!f)
	{
		Fatal_Error( "Configuration file \"" + filename + "\" is not accessible." );
	}
	else
	{
		cerr << "Loading configuration from \"" << filename << "\"" << endl;

		for(;;)
		{
			string name;
			int value;

			SkipComments(f);

			f >> name >> value;

			if (f.fail())
			{
				if (name.empty() && f.eof())
					break;

				Fatal_Error("Parsing configuration \"" + name + "\" in file \"" + filename + "\".");
			}

			// Error out on duplicates

			if (cfgmap.find(name) == cfgmap.end())
				cfgmap[name] = value;
			else
				errors += "\nDuplicate configuration: \"" + name + "\".\n";
		}
	}

	cfgmap_t :: iterator i;

	#define READ_CONFIG(_s,_default)								\
		i = cfgmap.find(#_s);										\
																	\
		if (i != cfgmap.end())										\
		{															\
			config_##_s = i->second;									\
			cfgmap.erase(i);										\
			cout << setw(40) << #_s" = " << config_##_s << endl;	\
		}															\
		else														\
		{															\
			errors += "\nMissing configuration \"" + ToString(#_s) + "\" (default value: " + ToString(_default) + ").\n";	\
		}


	cout << endl;
	cout << "==================================================================================================" << endl;
	cout << "    Configuration (" << filename << ")" << endl;
	cout << "==================================================================================================" << endl;

	READ_CONFIG(		screen_w,			0							)	// auto resolution
	READ_CONFIG(		screen_h,			0							)	// ""
	READ_CONFIG(		vsync,				0							)
	READ_CONFIG(		fullscreen,			0							)
	READ_CONFIG(		sound,				0							)

	#undef READ_CONFIG

	for (cfgmap_t::const_iterator c = cfgmap.begin(); c != cfgmap.end(); c++)
	{
		errors += "\nUnknown configuration\"" + c->first + "\".\n";
	}

	cout << "==================================================================================================" << endl;

	// Sanity checks:

	if (errors.empty())	// in order to not perform sanity checks on missing or duplicate parameters
		errors = Check_Config();

	// Report all errors and exit in that case

	if (!errors.empty())
		Fatal_Error("Errors found in file \"" + filename + "\":\n" + errors);
}
