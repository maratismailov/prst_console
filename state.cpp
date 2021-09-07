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

	- 2 functions to handle input events (Keyboard, Mouse)
	- 1 to update the status (Update)
	- 1 to draw the screen (Draw)

 Indeed, the main loop of the application basically consists of calling:
 Keyboard/Mouse until the input events queue is empty, Update then Draw,
 for each state in the list of states.

 Additionally each state has an Init function called when the state is added
 (e.g. init time) and an End function called when removing the state (e.g. program end).

 Each state runs in its own "window" on the screen (win_t), decided at init time.

 Note: PRESTo only uses one state, full screen area.

*******************************************************************************/

#include "state.h"

#include "graphics2d.h"


state_t state;

userinput_t userinput;

texptr_t Tex_Background()
{
	static texptr_t tex("bg.png");
	return tex;
}

/*******************************************************************************

	State

*******************************************************************************/

void state_t :: WinSelect(int mouse_x, int mouse_y)
{
	int win_i;

	for (win_i = int(statedata.size()-1); win_i >= 0 ; win_i--)
	{
		if (statedata[win_i].win.IsPointInside(mouse_x,mouse_y))
		{
			if (statedata[win_i].win.IsSelected())
			{
				statedata[win_i].win.SetSelected(false);
				break;
			}
			else
			{
				for (int i = 0; i < int(statedata.size()) ; i++)
					statedata[i].win.SetSelected(i == win_i);
				break;
			}
		}
	}
	
	if (win_i >= 0)
		swap(statedata[win_i],statedata.back());
}

void state_t :: Keyboard(SDL_KeyboardEvent &event)
{
	for (int i = 0; i < int(statedata.size()); i++)
	{
//		if (statedata[i].win.IsSelected())
		if (statedata[i].win.IsAboveAll())
		{
			statedata[i].win.ResetIdle();
			statedata[i].keyboard(event, statedata[i].win);
		}
	}
}
void state_t :: Mouse(SDL_Event &event)
{
	for (int i = 0; i < int(statedata.size()); i++)
	{
//		if (statedata[i].win.IsSelected())
		if (statedata[i].win.IsAboveAll())
		{
//			if ( event.type != SDL_MOUSEMOTION )
				statedata[i].win.ResetIdle();
			statedata[i].mouse(event, statedata[i].win);
		}
	}
}
void state_t :: Update()
{
	int win_i = -1;

	for (int i = 0; i < int(statedata.size()); i++)
	{
		statedata[i].win.Update();

		if (statedata[i].win.IsSelected())
		{
			statedata[i].update(statedata[i].win);
			win_i = i;
		}
	}

	if (win_i >= 0)
		return;

	for (int i = 0; i < int(statedata.size()); i++)
		if (win_i != i)
			statedata[i].update(statedata[i].win);
}
void state_t :: Draw()
{
	int win_i = -1;

	for (int i = 0; i < int(statedata.size()); i++)
	{
		if (statedata[i].win.IsAboveAll())
			win_i = i;
	}

	for (int i = 0; i < int(statedata.size()) ; i++)
	{
		if ((win_i >= 0) && (i != win_i))
			continue;

		win_t &win = statedata[i].win;

		if (win_i < 0)
			DrawQuad(Tex_Background(), 0,0, 1,SCRY);

		if (win_i < 0)
		{
			glViewport(win.GetX()+4, win.GetY()-3, win.GetW(), win.GetH());
			DrawQuad(NULL, 0,0, 1,SCRY, colors_t(0,0,0,0.4f));

			glViewport(win.GetX()-1, win.GetY()-1, win.GetW()+2, win.GetH()+2);
			glScissor (win.GetX()-1, win.GetY()-1, win.GetW()+2, win.GetH()+2);
			glEnable(GL_SCISSOR_TEST);
			glClearColor(0.0f,0.0f,0.3f,1.0f);
			glClear(GL_COLOR_BUFFER_BIT);
			glClearColor(0.0f,0.0f,0.0f,1.0f);
			glDisable(GL_SCISSOR_TEST);
		}

		glViewport(win.GetX(), win.GetY(), win.GetW(), win.GetH());
		glScissor (win.GetX(), win.GetY(), win.GetW(), win.GetH());

		statedata[i].draw(win);

		glViewport(0,0, screen_w, screen_h);
	}
}

void state_t :: EndAll()
{
	for (int i = 0; i < int(statedata.size()); i++)
		statedata[i].end();

	statedata.clear();
}

void state_t :: End()
{
	if (statedata.size())
		statedata[statedata.size()-1].end();

	statedata.pop_back();
}

void state_t :: Add(const win_t & _win, void(*_init)(),void(*_keyboard)(SDL_KeyboardEvent &event, win_t & win),void(*_mouse)(SDL_Event &event, win_t & win),void(*_update)(win_t & win),void(*_draw)(win_t & win),void(*_end)())
{
	_init();
	struct statedata_t s = { _win,_keyboard,_mouse,_update,_draw,_end };
	statedata.push_back(s);
}

void state_t :: Set(const win_t & _win, void(*_init)(),void(*_keyboard)(SDL_KeyboardEvent &event, win_t & win),void(*_mouse)(SDL_Event &event, win_t & win),void(*_update)(win_t & win),void(*_draw)(win_t & win),void(*_end)())
{
	EndAll();
	Add(_win,_init,_keyboard,_mouse,_update,_draw,_end);
}

/*******************************************************************************

	Windows

*******************************************************************************/

int win_t :: GetX()	const { return RoundToInt(Interp(x0,x1,scale) * screen_w); }
int win_t :: GetY()	const { return RoundToInt(Interp(y0,y1,scale) * screen_w); }
int win_t :: GetW()	const { return RoundToInt(Interp(w0,w1,scale) * screen_w); }
int win_t :: GetH()	const { return RoundToInt(Interp(h0,h1,scale) * screen_w); }

bool win_t :: IsSelected() const
{
	return selected;
}

bool win_t :: IsPointInside(int x, int y) const
{
	y = screen_h - y - 1;
	return (x >= GetX()) && (y >= GetY()) && (x < GetX()+GetW()) && (y < GetY()+GetH());
}

bool win_t :: IsAboveAll() const
{
	return selected && (scale >= 1.0f);
}

bool win_t :: IsIdle() const
{
	return /*(!IsSelected()) ||*/ (idle_time > 20.0f);
}

void win_t :: SetSelected(bool _selected)
{
	if (!selected && _selected)
		ResetIdle();

	selected = _selected;
}

void win_t :: ResetIdle()
{
	idle_time = 0;
}

void win_t :: Update()
{
	idle_time += DELTA_T;

	const float d_scale = float(1.0/0.5 * DELTA_T);

	if (selected)		scale += d_scale;
	else if (!selected)	scale -= d_scale;

	Clamp(scale, 0.0f, 1.0f);
}

void win_t :: SetScale(float _scale)
{
	scale = _scale;
}

win_t :: win_t(	float _x0, float _y0, float _w0, float _h0,
				float _x1, float _y1, float _w1, float _h1	)

:	x0(_x0),y0(SCRY-(_y0+_h0)),w0(_w0),h0(_h0),
	x1(_x1),y1(SCRY-(_y1+_h1)),w1(_w1),h1(_h1),
	scale(1),selected(true),idle_time(0)
{
float s = .97f;
x0 = x0 + w0 / 2 - w0 / 2 * s;
y0 = y0 + h0 / 2 - h0 / 2 * s;
w0 *= s;
h0 *= s;
}
