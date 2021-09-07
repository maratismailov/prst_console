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

	Save a raw RGB image in memory as a PNG file (using libpng)

*******************************************************************************/

#include <fstream>

#include "./save_png.h"

#include "png.h"


void png_write_data(png_structp png_ptr, png_bytep area, png_size_t size)
{
  ofstream* out = (ofstream *) png_get_io_ptr(png_ptr);
  out->write((const char *)area, size);
}

void png_io_flush(png_structp png_ptr)
{
  ofstream* out = (ofstream *) png_get_io_ptr(png_ptr);
  out->flush();
}

bool Save_PNG(const string & filename, byte *image_data, unsigned width, unsigned height, PNG_COMPRESSION_T compression)
{
	int z_compression;

	switch (compression)
	{
		case PNG_COMPRESSION_NONE:	z_compression = 0;	break;
		case PNG_COMPRESSION_FAST:	z_compression = 1;	break;
		case PNG_COMPRESSION_BEST:	z_compression = 9;	break;
		default:
			z_compression = 0;
			Fatal_Error( "Unknown compression level for saving a screenshot" );
	}

	// Open PNG file for writing
	ofstream* out = new ofstream(filename.c_str(), ios_base::binary);
	if (!out)
		return false;

	// Allocate basic libpng structures
	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);
	if (!png_ptr)
	{
		out->close();
		delete out;
		return false;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_write_struct(&png_ptr, 0);
		out->close();
		delete out;
		return false;
	}

	// setjmp() must be called in every function
	// that calls a PNG-reading libpng function
	if (setjmp(png_jmpbuf(png_ptr)))
	{
		png_destroy_write_struct(&png_ptr, &info_ptr);
		out->close();
		delete out;
		return false;
	}

	// set the zlib compression level
	png_set_compression_level(png_ptr, z_compression);

	png_set_write_fn(png_ptr, out, png_write_data, png_io_flush);

	// write PNG info to structure
	png_set_IHDR(	png_ptr,						info_ptr,
					width,							height,					8,
					PNG_COLOR_TYPE_RGB,				PNG_INTERLACE_NONE,
					PNG_COMPRESSION_TYPE_DEFAULT,	PNG_FILTER_TYPE_DEFAULT		);

	png_write_info(png_ptr, info_ptr);

	// Allocate pointers for each line
	png_bytepp row_pointers = new png_bytep[height];
	if (!row_pointers)
	{
		png_destroy_write_struct(&png_ptr, &info_ptr);
		out->close();
		delete out;
		return false;
	}

	// how many bytes in each row
	png_uint_32 rowbytes = png_get_rowbytes(png_ptr, info_ptr);

	// set the individual row_pointers to point at the correct offsets
	for (png_uint_32 i = 0; i < height; ++i)
		row_pointers[i] = (png_byte*)(image_data + i * rowbytes);

	// now we can go ahead and just write the whole image
	png_write_image(png_ptr, row_pointers);

	png_write_end(png_ptr, 0);

	png_destroy_write_struct(&png_ptr, &info_ptr);

	delete [] row_pointers;

	out->close();
	delete out;
	return true;
}
