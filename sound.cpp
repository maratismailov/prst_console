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

	Sound handling.

	A simple interface to use audio samples: Play, Stop, Pause.

*******************************************************************************/

#include "SDL_mixer.h"

#include "sound.h"

#include "config.h"

const int sound_t :: NUMCHANNELS = 32;

sound_t :: sound_t(string _filename, bool _loop)
:	sharedobj_t(_filename),
	loop(_loop)
{
	if (!config_sound)
		return;

	string filename = GetFilename();

	cerr << "Loading sound file \"" << filename << "\"" << endl;

	data = Mix_LoadWAV(filename.c_str());
	if (!data)
		Fatal_Error("Couldn't load file \"" + filename + "\"");
}

sound_t :: ~sound_t()
{
	if (!config_sound)
		return;

	Mix_FreeChunk(data);
	data = NULL;
}

void sound_t :: Play(float volume)
{
	if (!config_sound)
		return;

	int channel = Mix_PlayChannel(-1, data, loop ? -1 : 0);
	if (channel >= 0)
		Mix_Volume(channel, int(128 * volume +.5f));
}

void sound_t :: Stop(bool all_channels)
{
	if (!config_sound)
		return;

	for (int channel = 0; channel < NUMCHANNELS; channel++)
		if (Mix_GetChunk(channel) == data)
		{
			Mix_HaltChannel(channel);
			if (!all_channels)
				break;
		}
}

void sound_t :: SetPaused(bool paused, bool all_channels)
{
	if (!config_sound)
		return;

	for (int channel = 0; channel < NUMCHANNELS; channel++)
		if (Mix_GetChunk(channel) == data)
		{
			paused ? Mix_Pause(channel) : Mix_Resume(channel);
			if (!all_channels)
				break;
		}
}

bool sound_t :: IsPlaying()
{
	if (!config_sound)
		return true;

	for (int channel = 0; channel < NUMCHANNELS; channel++)
		if (Mix_Playing(channel) && Mix_GetChunk(channel) == data)
			return true;

	return false;
}

bool sound_t :: IsLooping()
{
	if (!config_sound)
		return false;

	return loop && IsPlaying();
}

void sound_t :: SetVolume(float volume, bool all_channels)
{
	if (!config_sound)
		return;

	for (int channel = 0; channel < NUMCHANNELS; channel++)
		if (Mix_GetChunk(channel) == data)
		{
			Mix_Volume(channel,int(128 * volume +.5f));
			if (!all_channels)
				break;
		}
}

/*******************************************************************************

	Music

*******************************************************************************/

void AllSounds_Stop()
{
	soundptr_t::AllObjects(&sound_t::Stop,true);
}

void AllSounds_SetPaused(bool paused)
{
	soundptr_t::AllObjects(&sound_t::SetPaused,paused,true);
}

// Static data members visible to every smart pointer to a sound :
//
//		pool		-	list of every currently loaded sound
//		path		-	name of the directory where to load sounds from

template<> soundptr_t::pool_t soundptr_t::pool;
template<> const string soundptr_t::path("sound/");
