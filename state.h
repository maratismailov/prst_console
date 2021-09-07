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

 At any given time the program has a list of active "states" (state_t state),
 each characterized by 4 functions:

	- 2 functions to handle input events (Keyboad, Mouse)
	- 1 to update the status (Update)
	- 1 to draw the screen (Draw)

 Indeed, the main loop of the application basically consists of calling:
 Keyboard/Mouse until the input events queue is empty, Update then Draw,
 for each state in the list of states.

 Additionally each state has an Init function called when the state is added
 (e.g. init time) and an End function called when removing the state (e.g. program end).

 Each state runs in its own "window" on the screen (win_t), decided at init time.

*******************************************************************************/

#ifndef STATE_H_DEF
#define STATE_H_DEF

#include <vector>
#include "SDL.h"

#include "global.h"

#include "geometry.h"

// Time quantum in seconds
#define DELTA_T		(1.0/30.0)

extern int screen_w;
extern int screen_h;

class win_t
{
	float x0,y0,w0,h0;
	float x1,y1,w1,h1;
	float scale;
	bool selected;
	secs_t idle_time;

public:

	int GetX() const;
	int GetW() const;
	int GetY() const;
	int GetH() const;

	bool IsSelected() const;
	bool IsAboveAll() const;
	bool IsPointInside(int x, int y) const;
	bool IsIdle() const;

	void SetSelected(bool _selected);
	void SetScale(float _scale);
	void ResetIdle();

	void Update();

	win_t(	float _x0, float _y0, float _w0, float _h0,
			float _x1, float _y1, float _w1, float _h1	);

	win_t() { }
};


class state_t
{
public:

	void Keyboard	(SDL_KeyboardEvent &event);	// Handle any keyboard event (key pressed, released)
	void Mouse		(SDL_Event &event);			// Handle any mouse event (motion, buttons, wheel)
	void Update		();							// Update the status
	void Draw		();							// Draw the screen

	void Set	(const win_t & win, void(*_init)(),void(*_keyboard)(SDL_KeyboardEvent &event, win_t & win),void(*_mouse)(SDL_Event &event, win_t & win),void(*_update)(win_t & win),void(*_draw)(win_t & win),void(*_end)());
	void Add	(const win_t & win, void(*_init)(),void(*_keyboard)(SDL_KeyboardEvent &event, win_t & win),void(*_mouse)(SDL_Event &event, win_t & win),void(*_update)(win_t & win),void(*_draw)(win_t & win),void(*_end)());

	void End();
	void EndAll();

	void WinSelect(int mouse_x, int mouse_y);

private:

	struct statedata_t {
		win_t	win;
		void (*keyboard)	(SDL_KeyboardEvent &event, win_t & win);
		void (*mouse)		(SDL_Event &event, win_t & win);
		void (*update)		(win_t & win);
		void (*draw)		(win_t & win);
		void (*end)			();
	};

	vector<statedata_t>	statedata;
};


extern state_t state;

void WorldToScreen(const vec3_t & pos, float *x, float *y);

/*******************************************************************************

	User input state

*******************************************************************************/

struct userinput_t
{
	// true when the corresponding used defined key is pressed:
	bool	left, right, up, down, reset;

	// mouse
	int		mousex, mousey;
	int		mousemovex, mousemovey;
	int		mousemoveWheel;
	bool	mousepressLeft, mousepressRight;

	void	ResetMouseMove()	{ mousemovex = mousemovey = mousemoveWheel = 0; }
};

extern userinput_t userinput;

#endif
