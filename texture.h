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

	A texture is a reference counted object containing the OpenGL
	texture image ID + some properties

*******************************************************************************/

#ifndef TEXTURE_H_DEF
#define TEXTURE_H_DEF

#include "SDL_opengl.h"
#ifdef __APPLE__
#	include <OpenGL/glu.h>
#else
#	include <GL/glu.h>
#endif

#include "global.h"

#include "sharedobj.h"

// Textures are reference counted objects
class texture_t;
typedef		sharedptr_t<texture_t>		texptr_t;

class texture_t : public sharedobj_t
{
	friend class sharedptr_t<texture_t>;

private:

	int		m_w, m_h;		// width and height in pixels
	color_t	m_color;		// computed average color

	// Animation
	float	m_animduration;	// in seconds
	int		m_animframes;	// 1 for not animated textures
	vector<GLuint>	m_ids;	// gl texture "name" for every animation frame
	GLuint	m_curr_id;		// current animation frame

	// A texture is usually created loading its image from a file, but also
	// by first generating an image in memory (e.g. lightmaps) and supplying
	// its pointer to the constructor

	texture_t(const string & filename, bool BuildMipmaps = true);
	texture_t(int w, int h, void *pixels, int components, int format, int type, bool BuildMipmaps = true);

	~texture_t();

	void	LoadInfo();	// Load the texture info file (.txt) for sound, animation etc.

	void	Cleanup();	// Free any allocated resources (destroy the OpenGL texture ...)

public:

	// Create an OpenGL texture from raw data in memory and return its id
	GLuint	Generate_GL_Texture(void *pixels, int components, int format, int type, bool BuildMipmaps);

	int		GetW()			{ return m_w; }		// Width and height in pixels
	int		GetH()			{ return m_h; }
	color_t	GetColor()		{ return m_color; }

	void	Bind()			{ glBindTexture( GL_TEXTURE_2D, m_curr_id ); }	// Bind the current texture
	void	Bind_1D()		{ glBindTexture( GL_TEXTURE_1D, m_curr_id ); }	// Bind the current texture

	// Animation
	bool	IsAnimated()	{ return (m_animframes > 1); };
	void	UpdateAnimFrame(float time);
};

#endif
