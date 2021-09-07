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

#ifndef GLEXT_H_DEF
#define GLEXT_H_DEF

#include "SDL.h"
#include "SDL_opengl.h"

#include "global.h"

// Extensions

typedef void (APIENTRY *			glActiveTextureARB_t		)	(GLenum);
typedef void (APIENTRY *			glClientActiveTextureARB_t	)	(GLenum);
typedef void (APIENTRY *			glLockArraysEXT_t			)	(int,int);
typedef void (APIENTRY *			glUnlockArraysEXT_t			)	();
typedef bool (APIENTRY *			wglSwapIntervalEXT_t		)	(int);

extern glActiveTextureARB_t			glActiveTextureARB_ptr;
extern glClientActiveTextureARB_t	glClientActiveTextureARB_ptr;
extern glLockArraysEXT_t			glLockArraysEXT_ptr;
extern glUnlockArraysEXT_t			glUnlockArraysEXT_ptr;
extern wglSwapIntervalEXT_t			wglSwapIntervalEXT_ptr;
void SetVSync(bool on);

extern GLint texunits;
extern bool UseMultiTex;

void Init_OpenGL();	// Set up non-changing OpenGL parameters

#endif
