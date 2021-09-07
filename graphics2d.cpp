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

#include <fstream>
#include <cstring>
#include <algorithm>

#include "graphics2d.h"

/*******************************************************************************

	Textured quad

*******************************************************************************/

void DrawQuad(texptr_t texture, float x, float y, float w, float h, colors_t colors, float rads, GLenum glblend_src, GLenum glblend_dst,
float u0, float v0, float u1, float v1, float u2, float v2, float u3, float v3)
{
	glPushAttrib( GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT );

	// Rotate around the center
	glMatrixMode( GL_MODELVIEW );
	glPushMatrix();
	glLoadIdentity();
	float center_x = x+w/2;
	float center_y = y+h/2;
	glTranslatef(+center_x,+center_y,0);
	glRotatef(RadsToDegs(rads),0,0,1);
	glTranslatef(-center_x,-center_y,0);


	// Enter 2D mode
	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();
	glOrtho(
		 0,1,		// left-right
		 SCRY,0,	// bottom-top
		-1,1		// near far
	);

	glDisable( GL_CULL_FACE );
	glDisable( GL_LIGHTING );
	glDisable( GL_DEPTH_TEST );
	glDepthMask( GL_FALSE );

	if ( texture.IsNULL() )
	{
		glDisable ( GL_TEXTURE_2D );
	}
	else
	{
		glEnable ( GL_TEXTURE_2D );
		texture->Bind();
	}

	glEnable ( GL_BLEND );
	glBlendFunc(glblend_src, glblend_dst);

	glBegin(GL_QUADS);

		glColor4f(colors.r,colors.g,colors.b,colors.a);
		glTexCoord2f( u0, v0 );		glVertex2f( x,   y   );
		glTexCoord2f( u1, v1 );		glVertex2f( x+w, y   );

		glColor4f(colors.r2,colors.g2,colors.b2,colors.a2);
		glTexCoord2f( u2, v2 );		glVertex2f( x+w, y+h );
		glTexCoord2f( u3, v3 );		glVertex2f( x,   y+h );

	glEnd();

	// Restore the rendering state
	glMatrixMode( GL_PROJECTION );
	glPopMatrix();

	glMatrixMode( GL_MODELVIEW );
	glPopMatrix();

	glPopAttrib();
}


/*******************************************************************************

	Screen Fade In / Out

*******************************************************************************/

int screen_w , screen_h;	// screen size in pixels

enum fade_dir_t		{ FADE_IN, FADE_OUT, FADE_NONE };
enum fade_style_t	{ FADE_NORMAL = 0, FADE_TOP, FADE_TOPLEFT, FADE_TOPRIGHT, FADE_STYLES_N };

class fade_t
{

public:

	fade_dir_t		dir;
	color_t			color;
	fade_style_t	style;

	secs_t			t0, duration;
	bool			isFading;

	fade_t() : dir(FADE_NONE), color(0,0,0,0), isFading(false) { }
};

fade_t fade;

bool IsFading()
{
	return fade.isFading;
}

void StartFade(fade_dir_t dir, float duration, const color_t & color)
{
	if (fade.dir == dir)
		return;

	fade.isFading	=	true;

	fade.dir		=	dir;
	fade.style		=	fade_style_t(rand() % FADE_STYLES_N);
	fade.duration	=	duration;
	fade.color		=	color;

	fade.t0			=	globaltime;
}

void StartFadeIn(float duration, const color_t & color)
{
	StartFade(FADE_IN, duration, color);
}

void StartFadeOut(float duration, const color_t & color)
{
	StartFade(FADE_OUT, duration, color);
}

void DrawFadeQuadRot(float rads)
{
	float d = sqrt(1 + SCRY*SCRY)*1.02f;
	float w = d;
	float h = d * cosf(abs(atan(SCRY)) - abs(rads));
	DrawQuad( NULL, 0.5f-w/2,SCRY/2-h/2, w,h,
	 colors_t(	fade.color.r,fade.color.g,fade.color.b,fade.color.a*2-1,
				fade.color.r,fade.color.g,fade.color.b,fade.color.a*2 ),
	 rads );
}


void DrawFade()
{
	if (fade.dir == FADE_NONE)
		return;

	secs_t elapsed = globaltime - fade.t0;

	if (elapsed > fade.duration)
	{
		fade.isFading = false;
		if (fade.dir == FADE_OUT)
		{
			glClearColor (fade.color.r, fade.color.g, fade.color.b, 0);
			glClear(GL_COLOR_BUFFER_BIT);
			glClearColor (0, 0, 0, 0);
		}
		return;
	}

	double amount = elapsed / fade.duration;
	Clamp(amount, 0.0, 1.0);
	fade.color.a = float(amount);

	if (fade.dir == FADE_IN)
		fade.color.a = 1 - fade.color.a;

	switch(fade.style)
	{
		case FADE_TOP:
			DrawFadeQuadRot(0);
			break;

		case FADE_TOPLEFT:
			DrawFadeQuadRot(-atan(SCRY));
			break;

		case FADE_TOPRIGHT:
			DrawFadeQuadRot(+atan(SCRY));
			break;

		default:
		case FADE_NORMAL:
			DrawQuad( NULL, 0,0, 1,SCRY, colors_t(fade.color.r,fade.color.g,fade.color.b,fade.color.a) );
			break;
	}
}


/*******************************************************************************

	Save a screenshot to disk. Filename is:

	fileprefix.png (overwriting a previous screenshot) if destination is SCR_OVERWRITE.

	fileprefix_0000.png, fileprefix_0001.png, fileprefix_0002.png ... (skipping existing files) if destination is SCR_NEXT.

*******************************************************************************/

// void SaveScreenShot( const string & fileprefix, enum SCR_DEST_T destination , enum PNG_COMPRESSION_T compression )
// {
// 	static int screenshot_num;	// Remember the last saved screenshot number
// 	string filename;
// 	fstream f;

// 	// Filename to save to

// 	switch ( destination )
// 	{
// 		case SCR_OVERWRITE:
// 			filename = fileprefix + ".png";
// 			break;

// 		case SCR_NEXT:
// 			// Loop through possible screenshot filenames until one that is not saved yet is found
// 			for(;;)
// 			{
// 				filename = fileprefix + '_' + ToString(screenshot_num + 10000).substr(1) + ".png";
// 				++screenshot_num;
// 				f.open(filename.c_str(),ios_base::in);
// 				if (!f.is_open())
// 					break;
// 				f.close();
// 			}
// 			break;

// 		default:
// 			return;
// 	}

// 	// Allocate memory for an rgb image as big as the screen (plus one row that we'll use as buffer)

// 	byte *rgb_data = NULL;	// pointer to the rgb data

// //	int row_len = (screen_w * 3 + (4-1)) / 4 * 4;	// len of a row in bytes (padding to 4-bytes, default OpenGL (UN)PACK_ALIGNMENT!)
// 	int row_len = screen_w * 3;

// 	try			{	rgb_data = new byte[row_len * (screen_h+1)];	}
// 	catch(...)  {	delete [] rgb_data; return;	}

// 	byte *row_buffer = rgb_data + row_len * screen_h;	// pointer to the row buffer (use last row as buffer)

// 	// Wait for the screen to be completely drawn, then copy its contents to the memory we just allocated

// 	glFinish();

// 	glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);
// 		glPixelStorei(GL_PACK_ALIGNMENT, 1);	// allow odd screen width
// 		glReadPixels(0,0,screen_w,screen_h,GL_RGB,GL_UNSIGNED_BYTE,rgb_data);
// 	glPopClientAttrib();

// 	// OpenGL writes in memory starting with lines at the bottom of the screen and moving upwards.
// 	// To properly save in PNG format, we need to vertically flip the rgb data to match what SDL expects

// 	for (int y = 0 ; y < screen_h / 2; y ++)
// 	{
// 		byte *row_top		=	rgb_data + row_len * y;
// 		byte *row_bottom	=	rgb_data + row_len * (screen_h-1-y);
// 		memcpy(row_buffer,	row_top,   	row_len);
// 		memcpy(row_top,		row_bottom,	row_len);
// 		memcpy(row_bottom,	row_buffer,	row_len);
// 	}

// 	Save_PNG(filename, rgb_data, screen_w, screen_h, compression);

// 	delete [] rgb_data;
// }

/*******************************************************************************

	Debug Text

*******************************************************************************/

void debugtext_t :: Add(const string & s)
{
	if (text.size()<100)
		text.push_back(s);
}
void debugtext_t :: Draw()
{
	const float FONTSIZE = SCRY/63;
	int i, n = int(text.size());
	for (i = 0; i < n; i++)
		SmallFont().Print(text[i], 0,SCRY - (n - i)*FONTSIZE, FONTSIZE,FONTSIZE);
	text.clear();
}

debugtext_t debugtext;


/*******************************************************************************

	Load a font from disk. The file format of a FONT file (ascii) is:

		FONT "texture_filename"
		texture_width
		texture_height
		bigspace
		smallspace
		u0 v0 u1 v1 (for char '!')
		u0 v0 u1 v1 (for char '"')
		..
		u0 v0 u1 v1 (for char '~')

	all numbers are integers

*******************************************************************************/

font_t :: font_t(string filename)
{
	// Open the file

	filename = PATH_FONT + StripPath(filename);

	ifstream f(filename.c_str());

	if (!f)	Fatal_Error("Couldn't open font file \"" + filename + "\"");

	string error_message = "Loading font file \"" + filename + "\"";
	cerr << error_message << endl;

	string s;

	// Load the texture

	f >> s;
	if (s != "FONT")	Fatal_Error("Not a FONT file \"" + filename + "\"");

	m_texture = texptr_t( ReadQuotedString(f, error_message), true );	// mipmapping on?

	int w,h;
	f >> w;
	f >> h;

	f >> m_bigspace;
	f >> m_smallspace;

	// Load the texture coordinates for every character

	m_max_w = 0;

	for (int i = 0; i < FONT_NUM; i++)
	{
		int u0,v0, u1,v1;

		f >> u0;	f >> v0;
		f >> u1;	f >> v1;

		m_charinfo[i].w	=	u1 - u0 + 1;
		m_charinfo[i].h	=	v1 - v0 + 1;

		if (m_charinfo[i].w > m_max_w)	m_max_w = m_charinfo[i].w;

		m_charinfo[i].u0	=	(float(u0) + 0.5f) / w;
		m_charinfo[i].v0	=	(float(v0) + 0.5f) / h;
		m_charinfo[i].u1	=	(float(u1) + 0.5f) / w;
		m_charinfo[i].v1	=	(float(v1) + 0.5f) / h;
	}
}

// Calculate width and height (in pixels) of a string
void font_t :: CalcTextSize(const string & s, float *width, float *height)
{
	int w = 0;
	int h = 0;
	int len = int(s.size());
	for (int i = 0; i < len; i++)
	{
		unsigned char code = s[i];

		if (code == ' ')
		{
			w += m_bigspace;
			if (h < m_bigspace)	h = m_bigspace;
			continue;
		}

		Clamp(code, FONT_FIRST, FONT_LAST);
		code -= FONT_FIRST;

		w += m_charinfo[code].w;
		if (h < m_charinfo[code].h)	h = m_charinfo[code].h;
	}

	if (len)
		w += (len-1) * m_smallspace;

	*width	=	float( w );
	*height	=	float( h );
}

void font_t :: Print(	const string & s, float x, float y, float w, float h,
						int flags,
						const colors_t & colors,
						const vector<vec3_t> *offsets, int offs_i)
{
	bool render = !(flags & FONT_NORENDER);

	if (flags & FONT_FLASH)
		if (((int)(globaltime / 0.5f))&1)
			render = false;

	// Enter 2d mode (if any rendering is to be done)

	if (render)
	{
		glPushAttrib( GL_ENABLE_BIT | GL_DEPTH_BUFFER_BIT );

		// Projection matrix
		glMatrixMode( GL_PROJECTION );
		glPushMatrix();
		glLoadIdentity();
		glOrtho(
			 0,1,		// left-right
			 SCRY,0,	// bottom-top
			-1,1		// near far
		);

		// Modelview matrix
		glMatrixMode( GL_MODELVIEW );
		glPushMatrix();
		glLoadIdentity();

		m_texture->Bind();

		glDisable( GL_LIGHTING );
		glDisable( GL_DEPTH_TEST );
		glDisable( GL_CULL_FACE );

		glEnable ( GL_BLEND );
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glEnable ( GL_TEXTURE_2D );
	}

	float scale_w = w / m_max_w;
	float scale_h = h / m_max_w;

	// Adjust the position if an alignment flag is on

	if ( flags & (FONT_X_IS_MAX | FONT_Y_IS_MAX | FONT_X_IS_CENTER | FONT_Y_IS_CENTER) )
	{
		float tw,th;
		CalcTextSize(s, &tw, &th);

		tw *= scale_w;
		th *= scale_h;

		if      ( flags & FONT_X_IS_MAX )		x -= tw;
		else if ( flags & FONT_X_IS_CENTER )	x -= tw/2;

		if      ( flags & FONT_Y_IS_MAX )		y -= th;
		else if ( flags & FONT_Y_IS_CENTER )	y -= th/2;
	}

	// Draw

	vec3_t offs(0,0,0);
	int len = int(s.size());

	m_curr_y = y;

	for (int i = 0; i < len; i++)
	{
		unsigned char code = s[i];

		if (code == ' ')
		{
			x += m_bigspace * scale_w;
			if (m_curr_y < y + m_bigspace * scale_h)	m_curr_y = y + m_bigspace * scale_h;
				continue;
		}

		Clamp(code, FONT_FIRST, FONT_LAST);
		code -= FONT_FIRST;

		float u0	=	m_charinfo[code].u0;
		float v0	=	m_charinfo[code].v0;
		float u1	=	m_charinfo[code].u1;
		float v1	=	m_charinfo[code].v1;

		float w		=	float( m_charinfo[code].w );
		float h		=	float( m_charinfo[code].h );

		w *= scale_w;
		h *= scale_h;

		if (render)
		{
			if (flags & FONT_ANIMATE)
			{
				// Rotate and zoom each character through a periodic motion
				const float Wr = (2*FLOAT_PI) / 2.5;
				const float Ws = (2*FLOAT_PI) / 2.0;
				const float Ar = 10.0f;
				const float As = 1 / 8.0f;

//				float fr = (x+y) * 1000;
				float fr = float( (i+code) * 1000 );
				float fs = fr/2;

				float scale = float( As * sin(fs + globaltime * Ws) + 1);
				float rot   = float( Ar * sin(fr + globaltime * Wr)    );

				float center_x = x+w/2;
				float center_y = y+h/2;

				glLoadIdentity();

				glTranslatef(+center_x,+center_y,0);
				glRotatef(rot,0,0,1);
				glScalef(scale,scale,1);
				glTranslatef(-center_x,-center_y,0);
			}

			if (offsets)
				offs = (*offsets)[i+offs_i];

			glBegin(GL_QUADS);

				glColor4f( colors.r, colors.g, colors.b, colors.a );
				glTexCoord2f( u0, v0 );		glVertex2f( x   + offs.x, y   + offs.y );
				glTexCoord2f( u1, v0 );		glVertex2f( x+w + offs.x, y   + offs.y );

				glColor4f( colors.r2, colors.g2, colors.b2, colors.a2 );
				glTexCoord2f( u1, v1 );		glVertex2f( x+w + offs.x, y+h + offs.y );
				glTexCoord2f( u0, v1 );		glVertex2f( x   + offs.x, y+h + offs.y );

			glEnd();
		}

		x += w + m_smallspace * scale_w;
		if (m_curr_y < y + h)	m_curr_y = y + h;
	}

	m_curr_x	=	x;

	// If there was rendering restore the rendering state
	if (render)
	{
		glPopMatrix();

		glMatrixMode( GL_PROJECTION );
		glPopMatrix();
		glMatrixMode( GL_MODELVIEW );

		glPopAttrib();
	}
}


font_t & SmallFont()
{
	static font_t font("small.font");
	return font;
}

font_t & BigFont()
{
	static font_t font("big.font");
	return font;
}

font_t & ArialFont()
{
	static font_t font("arial.font");
	return font;
}

/*******************************************************************************

	Icons

*******************************************************************************/

const float icon_t :: FONTSIZE = SCRY/50;

texptr_t Tex_Icon()
{
	static texptr_t tex("icon.png");
	return tex;
}

icon_t :: icon_t(string _text, float _x, float _y, bool _active, const color_t & _color_active, const color_t & _color_inactive, float _scale)
:	x(_x),y(_y),scale(_scale),
	text(_text),
	active(_active),
	active_time(globaltime),
	color_active(_color_active),color_inactive(_color_inactive)
{
}

float icon_t :: GetAlpha() const
{
	double alpha = (globaltime - active_time) / 0.3;
	Clamp(alpha, 0.0, 1.0);

	if (!IsActive())
		alpha = 1.0 - alpha;

	return float(alpha);
}

bool icon_t :: Click( int mouse_x, int mouse_y )
{
	float alpha = GetAlpha();

	if ( ((alpha != 0.0f) && (alpha != 1.0f)) || !IsPointInside(mouse_x,mouse_y) )
		return false;

	SetActive(!IsActive());
	return true;
}

bool icon_t :: IsActive() const
{
	return active;
}

void icon_t :: SetActive(bool _active)
{
	if (active != _active)
	{
		active = _active;
		active_time = globaltime;
	}
}

bool icon_t :: IsPointInside(int mouse_x, int mouse_y) const
{
	float cx = mouse_x * 1.0f / (screen_w - 1);
	float cy = mouse_y * SCRY / (screen_h - 1);

	return (cx >= x) && (cy >= y) && (cx < x+GetW()) && (cy < y+GetH());
}

float icon_t :: GetW() const
{
	return text.length() * icon_t::FONTSIZE * scale * 0.9f;
}

float icon_t :: GetH() const
{
	return icon_t::FONTSIZE * scale * 1.5f;
}

void icon_t :: Draw( float alpha ) const
{
	color_t button_color;

	float amount = GetAlpha();

	button_color = Interp(color_inactive, color_active, amount);
	button_color.a = alpha * Interp(0.5f,1.0f,amount);

/*
	if (!IsActive())
	{
		alpha *= 0.5f;
		button_color = colors_inactive;
		button_color.a = alpha;
	}
	else
	{
		button_color = colors_active;
		button_color.a = alpha;
	}
*/

	DrawQuad(Tex_Icon(),x+SCRY/300,y+SCRY/300,GetW(),GetH(),color_t(button_color.r/4,button_color.g/4,button_color.b/4,button_color.a/4));
	DrawQuad(Tex_Icon(),x,y,GetW(),GetH(),button_color);
	SmallFont().Print(text,x+GetW()/2,y+icon_t::FONTSIZE*scale/2*1.5f,icon_t::FONTSIZE*scale,icon_t::FONTSIZE*scale, FONT_CENTER, color_t(1,1,1,alpha));
}

/*******************************************************************************

	Logo

*******************************************************************************/

texptr_t Tex_CompanyLogo()
{
	static texptr_t tex("companylogo.png");
	return tex;
}

void DrawCompanyLogo(SCREENCORNER_T corner, secs_t time)
{
	const float MYSIZE_MIN	=	SCRY * 0.037f;
	const float MYSIZE_MAX	=	SCRY * 0.045f;
	const double ANGVEL_ROT	=	(2*M_PI) / 20.0;
	const float RADS_ROT	=	DegsToRads(8);

	float LOGOSIZE_Y		=	Interp(MYSIZE_MIN, MYSIZE_MAX, float(sin(time * 2 * M_PI / 25.0 + 40) + 1)/2);
	float LOGOSIZE_X		=	LOGOSIZE_Y * Tex_CompanyLogo()->GetW() / Tex_CompanyLogo()->GetH();

	const float ALPHA			=	0.8f;
	const secs_t T0				=	1.0f;
//	const float T1				=	10000.0f;
	const secs_t TIME_SLIDING	=	3.0;

	if ( time < T0 )
		return;

	// sliding amount from the outside of the screen to the corner
	float sliding;
	if (time <= T0+TIME_SLIDING)		sliding = float(max(0.0, (time - T0) / TIME_SLIDING));
//	else if (time >= T1-TIME_SLIDING)	sliding = float(min(1.0, (T1 - time) / TIME_SLIDING));
	else								sliding = 1.0f;

	// corner position
	float x0,y0, x1,y1;
	switch(corner)
	{
		default:
		case TOP_LEFT:		x0 = -LOGOSIZE_X;	y0 = -LOGOSIZE_Y;	x1 = 0;				y1 = 0;					break;
		case TOP_RIGHT:		x0 = 1;				y0 = -LOGOSIZE_Y;	x1 = 1-LOGOSIZE_X;	y1 = 0;					break;
		case BOTTOM_LEFT:	x0 = -LOGOSIZE_X;	y0 = SCRY;			x1 = 0;				y1 = SCRY-LOGOSIZE_Y;	break;
		case BOTTOM_RIGHT:	x0 = 1;				y0 = SCRY;			x1 = 1-LOGOSIZE_X;	y1 = SCRY-LOGOSIZE_Y;	break;
	}

	// move a little away from the corner towards the center
	float s = .995f;
	x1 = 0.5f + (x1-.5f)*s;
	y1 = SCRY/2 + (y1-SCRY/2)*s;

	// apply sliding
	x1 = Interp(x0, x1, sliding);
	y1 = Interp(y0, y1, sliding);

	DrawQuad(Tex_CompanyLogo(),x1,y1,LOGOSIZE_X,LOGOSIZE_Y, colors_t(1,1,1,ALPHA * sliding), RADS_ROT * float(sin(time*ANGVEL_ROT)));
}

void DrawCompanyLogo(float x0, float y0, float x1, float y1)
{
	float w = x1-x0;
	float h = y1-y0;
	float tw = float(Tex_CompanyLogo()->GetW());
	float th = float(Tex_CompanyLogo()->GetH());
	float ph = w * th/tw;
	if (ph > h)
		w = h * tw/th;
	else
		h = ph;

	DrawQuad(Tex_CompanyLogo(),(x0+x1)/2-w/2,(y0+y1)/2-h/2,w,h);
}


void WorldToScreen(const vec3_t & pos, float *x, float *y)
{
	const GLint Viewport[4] = { 0, 0, screen_w, screen_h };
	GLdouble Modelview[16], Projection[16], x0,y0,z0;

	glGetDoublev(GL_MODELVIEW_MATRIX, Modelview);
	glGetDoublev(GL_PROJECTION_MATRIX, Projection);
//	vec3_t right(Modelview[0*4 + 0],Modelview[1*4 + 0],Modelview[2*4 + 0]);
//	vec3_t up   (Modelview[0*4 + 1],Modelview[1*4 + 1],Modelview[2*4 + 1]);

	gluProject(pos.x,pos.y,pos.z, Modelview, Projection, Viewport, &x0, &y0, &z0);
	*x = float(x0 / screen_w);		*y = float((1 - y0/screen_h)*SCRY);
}
