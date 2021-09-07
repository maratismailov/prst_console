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

#ifndef SOUND_H_DEF
#define SOUND_H_DEF

#include "SDL_mixer.h"

#include "global.h"

#include "sharedobj.h"

// Sounds are reference counted objects
class sound_t;
typedef		sharedptr_t<sound_t>		soundptr_t;

class sound_t : public sharedobj_t
{
	friend class sharedptr_t<sound_t>;

	Mix_Chunk *data;

	sound_t(string _filename, bool loop = false);
	~sound_t();

	bool loop;

public:
	static const int NUMCHANNELS;

	void Play(float volume = 1);
	void Stop(bool all_channels = false);
	void SetPaused(bool paused, bool all_channels = false);

	bool IsPlaying();
	bool IsLooping();

	void SetVolume(float volume, bool all_channels = false);
};

// Music

void AllSounds_Stop();
void AllSounds_SetPaused(bool paused);

#endif
