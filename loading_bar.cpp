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


	Functions to have a progress bar displayed during disk loading


*******************************************************************************/

#include "loading_bar.h"

#include "graphics2d.h"

// Textures of the loading bar and its frame
static texptr_t& Tex_bar()
{
	static texptr_t tex("loadingbar.png",false);
	return tex;
}
static texptr_t& Tex_frame()
{
	static texptr_t tex("loadingframe.png",false);
	return tex;
}

/*
	loadingPercent_t is a structure to hold the current loading state:

	- minp and maxp are the initial and final percentage, of the whole loading, that are being accomplished
	- currp lies between the two and is the current percentage done
	- nextp is the percentage that will be reached when SetNextPercent will be called again
*/

class loadingPercent_t
{
private:

	int minp, maxp, currp, nextp;

	int		Calc(int x)			{ return int(Interp(float(minp),float(maxp),float(x)/100)); }

public:

	loadingPercent_t(int _minp, int _maxp) : minp(_minp),maxp(_maxp),currp(0),nextp(0)	{ }

	void SetNextPercent(int next_percent)
	{
		currp = nextp;
		nextp = next_percent;
	}

	int		CalcCurr()			{ return Calc(currp); }
	int		CalcNext()			{ return Calc(nextp); }
};

// A stack of nested loadings
vector<loadingPercent_t> loadingPercent;


// Draw the loading bar. Amount decides the percentage to draw in the horizontal direction:
// 0.0-1.0 translates to 0-100%
static void Draw_LoadingBar(texptr_t tex, float amount)
{
	// On screen dimensions of the bar. Assume it was drawn on a 800x600 screen
	// and keep the same on screen size
	float w = tex->GetW() / 800.0f;
	float h = tex->GetH() / 800.0f;	// 800, not 600 to keep the aspect ratio of the texture
									// that we assume is already correct (i.e. that of the window)

	glDrawBuffer(GL_FRONT);

	DrawQuad(tex, 1 -.03f - w, SCRY - .03f - h, w*amount, h, colors_t(1,1,1,1),0, GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,
		0,0, amount,0, amount,1, 0,1);

	glDrawBuffer(GL_BACK);

	glFinish();	// Screen must be drawn or there is no point in a loading bar (not much to draw either)
}

void LoadingBar_Start()
{
	int minp, maxp;

	// Is this the start of the whole loading (e.g. the stack of nested loadings is empty)?
	if (loadingPercent.empty())
	{
		minp = 0;
		maxp = 100;

		// Draw the frame then

		Draw_LoadingBar(Tex_frame(),1);
	}
	else
	{
		minp = loadingPercent.back().CalcCurr();
		maxp = loadingPercent.back().CalcNext();
	}

	loadingPercent.push_back( loadingPercent_t(minp,maxp) );
}

void LoadingBar_End()
{
	LoadingBar_SetNextPercent(100);
	LoadingBar_SetNextPercent(100);
	loadingPercent.pop_back();
}

void LoadingBar_SetNextPercent(float next_percent)
{
	loadingPercent.back().SetNextPercent(int(next_percent));

	int curr_percent = loadingPercent.back().CalcCurr();

	Draw_LoadingBar(Tex_bar(),float(curr_percent)/100);
}
