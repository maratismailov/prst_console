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

	OpenGL initialization and extensions

*******************************************************************************/

#include "glext.h"

// Extensions

glActiveTextureARB_t			glActiveTextureARB_ptr;
glClientActiveTextureARB_t		glClientActiveTextureARB_ptr;
glLockArraysEXT_t				glLockArraysEXT_ptr;
glUnlockArraysEXT_t				glUnlockArraysEXT_ptr;
wglSwapIntervalEXT_t			wglSwapIntervalEXT_ptr;

void SetVSync(bool on)
{
	if (wglSwapIntervalEXT_ptr)
	{
		wglSwapIntervalEXT_ptr(on ? 1 : 0);
		cerr << "Vsync set to " << on << endl;
	}
	else
	{
		cerr << "Vsync control not available" << endl;
	}
}

GLint texunits;
bool UseMultiTex;

// Set up non-changing OpenGL parameters
void Init_OpenGL()
{
	glFrontFace(GL_CW);
	glCullFace(GL_BACK);
	glEnable(GL_CULL_FACE);

	glDisable( GL_BLEND );

	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_TRUE);
	glDepthFunc(GL_LEQUAL);

	glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );


	glMatrixMode(GL_MODELVIEW);

	// FIXME remove APIENTRY in header
	glActiveTextureARB_ptr			=	(glActiveTextureARB_t)			SDL_GL_GetProcAddress("glActiveTextureARB");
	glClientActiveTextureARB_ptr	=	(glClientActiveTextureARB_t)	SDL_GL_GetProcAddress("glClientActiveTextureARB");
	glLockArraysEXT_ptr				=	(glLockArraysEXT_t)				SDL_GL_GetProcAddress("glLockArraysEXT");
	glUnlockArraysEXT_ptr			=	(glUnlockArraysEXT_t)			SDL_GL_GetProcAddress("glUnlockArraysEXT");
	wglSwapIntervalEXT_ptr			=	(wglSwapIntervalEXT_t)			SDL_GL_GetProcAddress("wglSwapIntervalEXT");

	if (glActiveTextureARB_ptr)	glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &texunits);
	else					    texunits = 1;

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnable( GL_TEXTURE_2D );

	UseMultiTex = true;

	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
}
