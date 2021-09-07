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

	Save an raw RGB image in memory as a PNG file (using libpng)

*******************************************************************************/

#ifndef SAVE_PNG_H_DEF
#define SAVE_PNG_H_DEF

#include "global.h"

enum PNG_COMPRESSION_T
{
	PNG_COMPRESSION_NONE,
	PNG_COMPRESSION_FAST,
	PNG_COMPRESSION_BEST
};

// bool Save_PNG(const string & filename, byte *image_data, unsigned width, unsigned height, PNG_COMPRESSION_T compression);

#endif
