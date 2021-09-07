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


	2D graphics routines:

		- textured quads
		- screen fade in/out
		- save a screenshot
		- textured proportional fonts
		- icons


*******************************************************************************/

#ifndef GRAPHICS2D_H_DEF
#define GRAPHICS2D_H_DEF

#include "global.h"

#include "geometry.h"
#include "texture.h"
#include "save_png.h"

// Draw a textured quad on screen. Note that texture may be null for an untextured quad
void DrawQuad(texptr_t texture, float x, float y, float w, float h, colors_t colors = colors_t(1,1,1,1), float rads = 0, GLenum glblend_src = GL_SRC_ALPHA, GLenum glblend_dst = GL_ONE_MINUS_SRC_ALPHA,
float u0 = 0, float v0 = 0,
float u1 = 1, float v1 = 0,
float u2 = 1, float v2 = 1,
float u3 = 0, float v3 = 1 );

//	Screen X size = 1.0f when printing. Screen Y size = SCRY = 1 / Aspect ratio
#define SCRY				(3.0f/4.0f)
//#define SCRY				(1024.0f/1280.0f)
//#define SCRY				(1.0f/1.6f)

// Screen fade in / out

extern int screen_w, screen_h;	// screen size in pixels

// Save a screenShot
enum SCR_DEST_T
{
	SCR_OVERWRITE,
	SCR_NEXT
};
// void SaveScreenShot( const string & fileprefix, enum SCR_DEST_T destination , enum PNG_COMPRESSION_T compression );

// Fading
void StartFadeIn (float duration = 1.6f, const color_t & color = color_t(0,0,0,0));
void StartFadeOut(float duration = 1.6f, const color_t & color = color_t(0,0,0,0));
void DrawFade();
bool IsFading();

class debugtext_t	// List of debug messages printed on screen
{
private:

	vector<string> text;

public:

	void Add(const string & s);
	void Draw();
};

extern debugtext_t debugtext;


/*
	A textured proportional font.

	It's loaded from disk as an ascii file that specifies:
	- a texture filename
	- texture coordinates of every character whose ascii code is between '!' and '~'
	- the width of the ' ' char (since it isn't in the texture, of course)
	- the width of the space between characters
*/

const unsigned char FONT_FIRST	=	'!';
const unsigned char FONT_LAST	=	'~';
const unsigned char FONT_NUM	=	FONT_LAST - FONT_FIRST + 1;

// flags when printing
#define FONT_X_IS_CENTER	(1 << 0)
#define FONT_X_IS_MAX		(1 << 1)
#define FONT_Y_IS_CENTER	(1 << 2)
#define FONT_Y_IS_MAX		(1 << 3)
#define FONT_ANIMATE		(1 << 4)
#define FONT_FLASH			(1 << 5)
#define FONT_NORENDER		(1 << 6)
#define FONT_CENTER			(FONT_X_IS_CENTER | FONT_Y_IS_CENTER)

class font_t
{
public:

	// Load from disk
	font_t(string filename);

	/*
		Print a string in s to screen. The on screen position (x,y) is given
		as two floats: the upper left corner of the screen is (0,0) and the
		screen width amounts to 1:

         x = 0.0f          x = 1.0f

            -----------------  <- y = 0.0f
           |                 |
           |                 |
           |                 |
           |                 |
           |                 |
            -----------------  <- y = SCRY (e.g. 3/4 since the screen aspect ratio is 4/3)

		w/h specify the size of the widest character (1.0f is the width of the screen).

		The optional flags parameter is the binary OR of some FONT_xxx constants
		and specifies the alignment (e.g. FONT_X_IS_CENTER will center the string
		on the supplied x coordinate) as well as special rendering effects (e.g.
		FONT_ANIMATE will animate the rotation and size of the letters with time).

		The optional colors parameter supplies 2 rgba quadruples: e.g. (red,white)
		will draw the letters with a vertical gradient from red (top) to white (bottom).
	*/
	void Print(	const string & s,
				float x, float y, float w, float h,
				int flags = 0,
				const colors_t & colors = colors_t(1,1,1,1),
				const vector<vec3_t> *offsets = NULL, int offs_i = 0);

	// On screen coordinates of the top right pixel in the last print string
	// (useful when printing multiple strings, one right to the other, with different colors and/or size)
	float CurrX()	const	{ return m_curr_x; }
	float CurrY()	const	{ return m_curr_y; }

private:

	texptr_t m_texture;

	// texture coordinates and size (in pixels) of every character
	struct
	{
		float u0,v0,u1,v1;
		int w,h;
	} m_charinfo[FONT_NUM];

	// The following integers are in pixels:
	int m_bigspace;		// size of the ' ' character
	int m_smallspace;	// distance between two characters
	int m_max_w;		// width of the widest character in the texture

	// On screen coordinates of the bottom right pixel in the last print string
	float m_curr_x, m_curr_y;

	// Calculate width and height (in pixels) of a string
	void CalcTextSize(const string & s, float *width, float *height);
};

// These 2 fonts are used throughout the program
font_t & SmallFont();
font_t & ArialFont();
font_t & BigFont();

/*******************************************************************************

	Icons

*******************************************************************************/

class icon_t
{
private:

	float x,y;
	float scale;
	string text;

	bool active;
	secs_t active_time;

	color_t color_active, color_inactive;

	bool IsPointInside(int x, int y) const;


public:

	const static float FONTSIZE;

	float GetW() const;
	float GetH() const;

	void Draw( float alpha ) const;

	bool IsActive() const;
	void SetActive(bool _active);

	bool Click( int mouse_x, int mouse_y );

	float GetAlpha() const;

	icon_t() { }
	icon_t(	string _text, float _x, float _y, bool _active = true,
			const color_t & _color_active   = color_t(1.0f,1.0f,0.0f,1.0f),
			const color_t & _color_inactive = color_t(0.0f,0.0f,0.4f,1.0f), float _scale = 1.0f);
};


enum SCREENCORNER_T { TOP_LEFT, TOP_RIGHT, BOTTOM_LEFT, BOTTOM_RIGHT, CORNERS_N };

void DrawCompanyLogo(SCREENCORNER_T corner, secs_t time);
void DrawCompanyLogo(float x0, float y0, float x1, float y1);

void WorldToScreen(const vec3_t & pos, float *x, float *y);

#endif
