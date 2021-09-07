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

	Application entry point & Main loop

	Note:	SDL_Quit and Mix_CloseAudio get called before global objects are destroyed.
			This means that the OpenGL context is gone before we destroy global texture objects,
			display lists etc. and SDL_mixer is gone before we destroy global sounds.

			That's why texture & sound objects (or objects containing them) that are
			not dynamically created must be enclosed in a function that returns a reference
			to a static local variable like this:

				texptr_t Tex1()
				{
					static texptr_t tex("tex1.png");
					return tex;
				}

*******************************************************************************/

#include <fstream>
#include <iomanip>
#include <vector>
#include <string>
#include "SDL.h"
#include "SDL_opengl.h"
#include "SDL_revision.h"
#include "SDL_net.h"
#include "SDL_image.h"
#include "SDL_mixer.h"
#include "png.h"
#include "libslink.h"
#include "muParser.h"

#include "main.h"

#include "graphics2d.h"
#include "glext.h"
#include "sound.h"
#include "config.h"
#include "state.h"
#include "gui.h"
#include "version.h"
#include "loading_bar.h"

/*******************************************************************************

	Local variables / functions

*******************************************************************************/

namespace
{

bool	quit;

secs_t simutime_next_pause = 0;
secs_t simutime_next_screenshot = 0;

bool	show_fps		=	false,	// display the frames-per-second counter?
		limit_speed		=	true;	// Run the program at exact pace, or at maximum speed?

// Preload media files
void PreLoad_Main()
{
}

// Shut down the video subsytem
void Quit_Video()
{
	Destroy_Screen();
	SDL_Quit();
}

// Shut down the audio subsytem
void Quit_Audio()
{
	Mix_CloseAudio();
	Mix_Quit();
}

// Shut down the network subsytem
void Quit_Net()
{
	SDLNet_Quit();
}

// Make sure a clear screen is displayed, by clearing both the front and back buffers
void Clear_Screen()
{
	glDrawBuffer(GL_FRONT_AND_BACK);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawBuffer(GL_BACK);
}

void Init_Video_And_Audio()
{
	// Initialize SDL and shut it down on exit
	// The VC++ debugger requires SDL_INIT_NOPARACHUTE to work
	if (SDL_Init( SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE ) < 0)
		Fatal_Error(SDL_GetError());

	atexit(Quit_Video);

	// Initialize SDL_mixer and shut it down on exit (if sound on)
	if (config_sound)
	{
		int mixflags = 0;	// bitmask of MIX_INIT_<filetype>
		if ( (Mix_Init(mixflags) & mixflags) != mixflags )
			cerr << "Not all requested sound formats are supported: " << string(Mix_GetError()) << endl;

		if ( Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT, 2, 4096) < 0 )
		{
			cerr << "Can't open audio streams, running without sounds. Sound Library Error: " << string(Mix_GetError()) << endl;
			config_sound = 0;
		}
		else
		{
			Mix_AllocateChannels(sound_t::NUMCHANNELS);
			atexit(Quit_Audio);
		}
	}

	Create_Screen(0);

	Init_OpenGL();
	SetVSync(config_vsync ? true : false);
	Clear_Screen();

	// Preload media files
	PreLoad_Main();
}

void Init_Net()
{
	if (SDLNet_Init() == -1)
		Fatal_Error("SDL_net - " + string(SDLNet_GetError()));

	atexit(Quit_Net);
}

/*
 Handle non state-specific keyboard events.

 Any keyboard event must pass through this function first and if it is a
 non state-specific event (e.g. quick exit from the application, toggle fps
 counter display and the like) it will be processed and true will be returned
 (meaning the event should not reach state-specific routines).
*/

bool Keyboard_Default(SDL_KeyboardEvent &event)
{
	bool pressed	=	(event.state == SDL_PRESSED);
	int mod			=	event.keysym.mod;

	switch( event.keysym.sym )
	{
	case SDLK_ESCAPE:
		// FIXME: ESC should be disabled in realtime mode for security reasons.
		//        But there would be no way to quit when in full-screen mode.
//		if (!realtime)
		if ( pressed && (mod & KMOD_SHIFT) )
			quit = true;
		break;

	// Toggle the speed limit
	case SDLK_F10:
		if (pressed)
			limit_speed = !limit_speed;
		break;

	// Toggle frames-per-second display
	case SDLK_F1:
		if (pressed)
			show_fps = !show_fps;
		break;

	// Save a screenshot
	case SDLK_F2:
	// case SDLK_PRINTSCREEN:
	// 	if (pressed)
	// 		SaveScreenShot( (realtime ? net_dir : sacs_dir) + "screenshot", SCR_NEXT, PNG_COMPRESSION_BEST );
	// 	break;

	// Pause
	case SDLK_p:
		if (!realtime)
		{
			if (pressed)
			{
				if ( GetPaused() && ((mod & KMOD_SHIFT) || (mod & KMOD_CTRL)) )
					simutime_next_pause = simutime_t :: Get() + 0.5f;
				else
					simutime_next_pause = 0;

				SetPaused( !GetPaused() );
			}
		}
		break;

	default:
		return false;
	}

	return true;
}

void Print_GL_Error()
{
	GLenum error = glGetError();
	if (error)
	{
		const float	FONTSIZE = SCRY/25.0f;
		const char *error_s = (const char *)( gluErrorString(error) );
		string s(error_s,strlen(error_s));
		SmallFont().Print(s, 1,0, FONTSIZE,FONTSIZE, FONT_X_IS_MAX);
		printf(error_s);
	}
}

unsigned frames;		// drawn frames till now
unsigned fps;
unsigned percent_draw;

void Update_FPS(ticks_t ticks_frame, ticks_t ticks_draw)
{
	static ticks_t	elapsed = 0,		// elapsed ticks since last call.
					elapsed_draw = 0;	// elapsed ticks spent drawing since last call.

	elapsed			+=	ticks_frame;
	elapsed_draw	+=	ticks_draw;
	++frames;

	const ticks_t WAIT = 1000;	// Update the fps counter every few seconds

	if (elapsed >= WAIT)
	{
		fps				=	unsigned( (frames       * 1000.f) / elapsed + .5f );
		percent_draw	=	unsigned( (elapsed_draw *  100.f) / elapsed + .5f );

		frames			=	0;
		elapsed_draw	=	0;
		elapsed 		-=	WAIT;
	}
}

void Print_FPS()
{
	if (show_fps)
	{
		const float	FONTSIZE = SCRY/30.0f;
		string s = "Draw " + ToString(percent_draw) + "% - FPS " + ToString(fps);
		SmallFont().Print(s, 1,0, FONTSIZE,FONTSIZE, FONT_X_IS_MAX, colors_t(1,1,1,1, 1,0.3f,0.3f,1));
		SmallFont().Print(ToString(screen_w) + "x" + ToString(screen_h), 1,FONTSIZE, FONTSIZE,FONTSIZE, FONT_X_IS_MAX, colors_t(1,1,1,1, 1,0.3f,0.3f,1));
	}
}


void Process_Pending_Events()
{
	SDL_Event event;
	while ( SDL_PollEvent(&event) == 1 )
	{
		switch (event.type)
		{
			case SDL_QUIT:
				quit = true;
				break;

			case SDL_WINDOWEVENT:
				switch (event.window.event)
				{
					case SDL_WINDOWEVENT_CLOSE:
						quit = true;
						break;
				}
				break;

			case SDL_MOUSEBUTTONUP:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEWHEEL:
			case SDL_MOUSEMOTION:

				switch (event.type)
				{
					// Mouse motion
					case SDL_MOUSEMOTION:
					{
						userinput.mousex	=	event.motion.x;
						userinput.mousey	=	event.motion.y;

						// Accumulate the movements in the event queue
						userinput.mousemovex	+=	event.motion.xrel;
						userinput.mousemovey	+=	event.motion.yrel;
					}
					break;

					// Mouse buttons
					case SDL_MOUSEBUTTONUP:
					case SDL_MOUSEBUTTONDOWN:
					{
						bool pressed = (event.button.state == SDL_PRESSED);

						switch( event.button.button )
						{
							case SDL_BUTTON_LEFT:		userinput.mousepressLeft	=	pressed;	break;
							case SDL_BUTTON_RIGHT:		userinput.mousepressRight	=	pressed;	break;
						}
					}
					break;

					// Mouse wheel
					case SDL_MOUSEWHEEL:
						if (event.wheel.y > 0)	++userinput.mousemoveWheel;
						else					--userinput.mousemoveWheel;
						break;
				}

				state.Mouse(event);
				break;

			case SDL_KEYUP:
			case SDL_KEYDOWN:
				if (!event.key.repeat)	// ignore repeats
				{
					if (!Keyboard_Default(event.key))
						state.Keyboard(event.key);
				}
				break;
		}
	}
}


void slink_logprint(const char *s)
{
	stringstream ss;
	ss << SecsToString(SecsNow()) << ": SLINK " << s;
	cerr << ss.rdbuf();
}

// Init the log with information about the program version, libraries version and command line parameters

string VerToString(const SDL_version ver, const char *rev = NULL)
{
	return ToString((unsigned)ver.major)+"."+ToString((unsigned)ver.minor)+"."+ToString((unsigned)ver.patch) + (rev == NULL ? "" : " (" + string(rev) + ")");
}

unsigned VerToInt(const SDL_version ver)
{
	return ver.major * 1000 + ver.minor * 100 + ver.patch;
}

void Fatal_Error_Library(const string & libname, const string & minver)
{
	Fatal_Error("Please update your copy of " + libname + " library. Required version is " + minver + " or higher.");
}

void Init_Log()
{
	cout << APP_TITLE << endl;

	// Platform, 64- or 32-bit executable
	bool x64 = (sizeof(void *) == 8);
	cout << "Running on " << SDL_GetPlatform() << ", " << (x64 ? "64" : "32") << "-bit executable" << endl;

	const std::streamsize w1 = 10, w2 = 28, w3 = 28;

	// Log the version of all libraries used (either dynamically loaded or statically linked)

	cout << endl;
	cout << "==================================================================================================" << endl;
	cout << "    Libraries" << endl;
	cout << endl;
	cout <<	setw(w1)	<<	"Name"				<<	" | "	<<
			setw(w2)	<<	"Expected Version:"	<<	" | "	<<
			setw(w3)	<<	"Found Version:"	<<	endl;
	cout << "==================================================================================================" << endl;

	// muParser
	// {
	// 	cout <<	setw(w1)	<<	"muParser"															<<	" | "	<<
	// 			setw(w2)	<<	string(MUP_VERSION) + " (" + string(MUP_VERSION_DATE) + ")"			<<	" | "	<<
	// 			setw(w3)	<<	"n/a"				 												<<	endl;
	// }

	// Slink
	{
		cout <<	setw(w1)	<<	"Slink"																<<	" | "	<<
				setw(w2)	<<	string(LIBSLINK_VERSION) + " (" + string(LIBSLINK_RELEASE) + ")"	<<	" | "	<<
				setw(w3)	<<	"n/a"				 												<<	endl;
	}

	// SDL
	{
		SDL_version sdl_compiled;
		SDL_version sdl_linked;

		SDL_VERSION(&sdl_compiled);
		SDL_GetVersion(&sdl_linked);

		const char *sdl_compiled_rev	=	SDL_REVISION;
		const char *sdl_linked_rev		=	SDL_GetRevision();

		if (sdl_compiled_rev == NULL)	sdl_compiled_rev	=	"";
		if (sdl_linked_rev == NULL)		sdl_linked_rev		=	"";

		cout <<	setw(w1)	<<	"SDL"											<<	" | "	<<
				setw(w2)	<<	VerToString(sdl_compiled, sdl_compiled_rev)		<<	" | "	<<
				setw(w3)	<<	VerToString(sdl_linked,   sdl_linked_rev)		<<	endl;

		if (VerToInt(sdl_linked) < VerToInt(sdl_compiled))
			Fatal_Error_Library("SDL", VerToString(sdl_compiled));
	}

	// SDL_image
	{
		SDL_version img_compiled;

		SDL_IMAGE_VERSION(&img_compiled);
		const SDL_version *img_linked = IMG_Linked_Version();

		cout <<	setw(w1)	<<	"SDL_image"					<<	" | "	<<
				setw(w2)	<<	VerToString(img_compiled)	<<	" | "	<<
				setw(w3)	<<	VerToString(*img_linked)	<<	endl;

		if (VerToInt(*img_linked) < VerToInt(img_compiled))
			Fatal_Error_Library("SDL_image", VerToString(img_compiled));
	}

	// SDL_mixer
	{
		SDL_version mix_compiled;

		SDL_MIXER_VERSION(&mix_compiled);
		const SDL_version *mix_linked = Mix_Linked_Version();

		cout <<	setw(w1)	<<	"SDL_mixer"					<<	" | "	<<
				setw(w2)	<<	VerToString(mix_compiled)	<<	" | "	<<
				setw(w3)	<<	VerToString(*mix_linked)	<<	endl;

		if (VerToInt(*mix_linked) < VerToInt(mix_compiled))
			Fatal_Error_Library("SDL_mixer", VerToString(mix_compiled));
	}

	// SDL_net
	{
		SDL_version net_compiled;

		SDL_NET_VERSION(&net_compiled);
		const SDL_version *net_linked = SDLNet_Linked_Version();

		cout <<	setw(w1)	<<	"SDL_net"					<<	" | "	<<
				setw(w2)	<<	VerToString(net_compiled)	<<	" | "	<<
				setw(w3)	<<	VerToString(*net_linked)	<<	endl;

		if (VerToInt(*net_linked) < VerToInt(net_compiled))
			Fatal_Error_Library("SDL_net", VerToString(net_compiled));
	}

	// PNG
	{
		cout <<	setw(w1)	<<	"PNG"					<<	" | "	<<
				setw(w2)	<<	PNG_LIBPNG_VER_STRING	<<	" | "	<<
				setw(w3)	<<	png_libpng_ver			<<	endl;
	}

	cout << "==================================================================================================" << endl;



	cout << endl;
	cout << "==================================================================================================" << endl;
	cout << "    Command Line Arguments" << endl;
	cout << "==================================================================================================" << endl;
	cout << "    " << net_name << "    " << event_name << endl;
	cout << "==================================================================================================" << endl;
}


}	// namespace

/*******************************************************************************

	Global variables / functions

*******************************************************************************/

int main(int argc, char *argv[])
{
	string out_filename, err_filename;

	if ( argc < 2 || argc > 3 )
	{
		Fatal_Error(
			"\n" +
			APP_BLURB + "\n" +
			"\n" +
			"Wrong number of parameters. Syntax is:\n" +
			"\n" +
			 StripPath(argv[0]) + " network-name [earthquake-name]\n" +
			"\n"
		);
	}

	net_name	=	argv[1];

	net_dir = PATH_DATA + net_name + "/";


	if ( argc == 2 )
	{
		realtime = true;

		string date_prefix;

		date_prefix = SecsToString(SecsNow()).substr(0,19);
		date_prefix = Replace(date_prefix, " ", "_");
		date_prefix = Replace(date_prefix, ":", ".");

		out_filename = net_dir + date_prefix + "_realtime.log";
		err_filename = net_dir + date_prefix + "_realtime.err";
	}
	else if ( argc == 3 )
	{
		realtime = false;

		event_name	=	argv[2];

		sacs_dir = net_dir + event_name + "/";

		out_filename = sacs_dir + event_name + ".log";
		err_filename = sacs_dir + event_name + ".err";
	}
	freopen(out_filename.c_str(),"w",stdout);
	cout << "if0\n";

	// Redirect cout and cerr (main log and error log)

	if ( freopen(out_filename.c_str(),"w",stdout) == NULL ){
		cout << "if01\n";
		Fatal_Error("Can't open main log \"" + out_filename + "\" for writing");
	}

	cout << "if1\n";
	

	if ( freopen(err_filename.c_str(),"w",stderr) == NULL )
		Fatal_Error("Can't open error log \"" + err_filename + "\" for writing");

	cout << "if2\n";
	

	sl_loginit(
		RoundToInt(float(param_slink_log_verbosity)),
		slink_logprint, NULL,
		slink_logprint, "ERROR "
	);
	cout << "sl_loginit\n";


	Init_Log();
	cout << "Init_Log";

	// Load the user's preferences (we need the screen size now)

	Load_Config();
	cout << "Load_Config";

	Load_Params();
	cout << "Load_Params";


	// Initialize the video and audio subsystems, and open the screen

	// Init_Video_And_Audio();

	// Initialize the network subsystem

	Init_Net();

	cout << "Init_Net";


	// Set the initial state

	LoadingBar_Start();
	LoadingBar_SetNextPercent(100);

		State_Add_GUI();

	LoadingBar_End();

	// Main loop (process pending events, update the status, draw the screen)

	quit = false;
	SetPaused( false );
	while ( !quit )
	{
		ticks_t ticks_start = TicksElapsedSince(0);

		// Process every event in the events queue

		Process_Pending_Events();

		// Update the status

		if (simutime_next_pause && simutime_next_pause <= simutime_t :: Get())
		{
			simutime_next_pause = 0;
			SetPaused( true );
		}

		state.Update();

		ticks_t ticks_update = TicksElapsedSince(0);

		// Draw the screen and overlay a fade in/out effect

		state.Draw();
		DrawFade();
		ticks_t ticks_draw	=	TicksElapsedSince(0);

		// Display debug messages

		debugtext.Draw();

		// Save a screenshot at a fixed pace (if in movie mode)

		// if (!realtime && param_simulation_movie_period && simutime_t::Get() >= simutime_next_screenshot)
		// {
		// 	// Calc next screenshot time before saving (or the movie frame rate would be biased by the time it takes to save the screenshot)
		// 	simutime_next_screenshot = floor(simutime_t::Get()/param_simulation_movie_period) * param_simulation_movie_period + param_simulation_movie_period;
		// 	SaveScreenShot( sacs_dir + "movie", SCR_NEXT, PNG_COMPRESSION_NONE );
		// }

		// Display an error string should we ever hit an OpenGL error

		Print_GL_Error();

		// On top of everything, show a frames-per-second counter

		Print_FPS();

		// Sync simulated time to real time

		if (limit_speed)
		{
			const ticks_t DELAY = static_cast<ticks_t>(DELTA_T * 1000/* + .5f*/);	// DELAY must be in ticks (milliseconds)
			ticks_t elapsed = TicksElapsedSince(ticks_start);

			if (DELAY > elapsed + 2)
				SDL_Delay(DELAY - elapsed);
		}

		Update_FPS(TicksElapsedSince(ticks_start), TicksDifference(ticks_draw, ticks_update));

		// We're done drawing, so swap the back and front buffers

		Swap_Screen();

		globaltime += DELTA_T;

		userinput.ResetMouseMove();
	}

	// Error free exit

	Exit();

	return 0;	// unreachable
}
