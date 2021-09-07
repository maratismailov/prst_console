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

#include <fstream>
#include <cstring>
#include "SDL_image.h"

#include "texture.h"

// Static data members visible to every smart pointer to a texture :
//
//		pool		-	list of every currently loaded texture
//		path		-	name of the directory where to load textures from

template<> texptr_t::pool_t texptr_t::pool;
template<> const string texptr_t::path("texture/");


// Free any allocated resources (destroy the OpenGL texture ...)
void texture_t :: Cleanup()
{
	for (unsigned i = 0; i < m_ids.size(); i++)
		glDeleteTextures(1,&m_ids[i]);
}

GLuint texture_t :: Generate_GL_Texture(void *pixels, int components, int format, int type, bool BuildMipmaps)
{
	// Create and bind an OpenGL named texture

	glGenTextures(1,&m_curr_id);
	Bind();

	// Wrapping mode is REPEAT. MIN & MAG filters are LINEAR (either MIPMAP or not)

	glTexParameteri(GL_TEXTURE_2D,	GL_TEXTURE_WRAP_S,	GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D,	GL_TEXTURE_WRAP_T,	GL_REPEAT);

	if (BuildMipmaps)
	{
		glTexParameteri(GL_TEXTURE_2D,	GL_TEXTURE_MIN_FILTER,	GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,	GL_TEXTURE_MAG_FILTER,	GL_LINEAR);

		gluBuild2DMipmaps(
			GL_TEXTURE_2D,components,	// target, components (1..4)
			m_w,m_h,					// width, height
			format,type,				// format (color_index,rgb..),type (byte,bitmap..)
			pixels	);					// pixels
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D,	GL_TEXTURE_MIN_FILTER,	GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D,	GL_TEXTURE_MAG_FILTER,	GL_LINEAR);

		glTexImage2D(
			GL_TEXTURE_2D,0,components,	// target, level (mipmap), components (1..4)
			m_w,m_h,					// width, height
			0,format,type,				// border (0,1), format (color_index,rgb..),type (byte,bitmap..)
			pixels	);					// pixels
	}

	return m_curr_id;
}


texture_t :: texture_t(int w, int h, void *pixels, int components, int format, int type, bool BuildMipmaps)
:	sharedobj_t(""),
	m_w(w),
 	m_h(h),
	m_color(1,1,1,1),
	m_animduration(0),
	m_animframes(1)
{
	m_ids.push_back( Generate_GL_Texture(pixels, components, format, type, BuildMipmaps) );
	m_curr_id = m_ids[0];
}


// Parse the texture info file (specifying: animation, etc..)
void texture_t :: LoadInfo()
{
	string s = GetFilename() + ".txt";
	ifstream f(s.c_str());
	if (!f)	return;

	string error_message = "Loading texture info file \"" + s + "\"";
	cerr << error_message << endl;

	while ( (f >> s).good() )
	{
		if	(s == "animframes")
		{
			if ( !(f >> m_animframes).good() || (m_animframes < 2))
				Fatal_Error(error_message + ": Invalid animframes");
		}
		else if	(s == "animduration")
		{
			if ( !(f >> m_animduration).good() || (m_animduration <= 0))
				Fatal_Error(error_message + ": Invalid animduration");
		}
		else
		{
			Cleanup();
			Fatal_Error(error_message + ": Unrecognized keyword: \"" + s + "\"");
		}
	}

	if (m_animduration && (m_animframes == 1))
		Fatal_Error(error_message + ": Missing animframes");

	if (!m_animduration && (m_animframes != 1))
		Fatal_Error(error_message + ": Missing animduration");
}


texture_t :: texture_t(const string & _filename, bool BuildMipmaps)
:	sharedobj_t( _filename ),
	m_animduration(0),
	m_animframes(1)
{
	LoadInfo();

	// Load from file to the SDL surface source_image
	// NOTE: SDL pads mem buffer to 4-bytes per line.
	// So OpenGL (UN)PACK_ALIGNMENT must be set to 4, the default (glPixelStorei)

	for (int i = 0; i < m_animframes; i++)
	{
		string filename = GetFilename();

		if (i != 0)
			filename = InsertBeforeExtension(filename, ToString(i) + ".");

		cerr << "Loading image file \"" << filename << "\"" << endl;

		SDL_Surface *source_image = IMG_Load(filename.c_str());
		if (!source_image)
		{
			Cleanup();
			Fatal_Error("Couldn't load file \"" + filename + "\"");
		}

		// We must now convert the image to an OpenGL compatible pixel format:
		// First up, see whether this is a RGB or a RGBA image ...

		Uint8 bytesPerPixel;
		GLenum glFormat;

		if (source_image->format->BytesPerPixel < 4)
		{
			bytesPerPixel = 3;
			glFormat = GL_RGB;
		}
		else
		{
			bytesPerPixel = 4;
			glFormat = GL_RGBA;
		}

		// ... Second, Set up the desired SDL pixel format (compatible with either GL_RGB or GL_RGBA) ...

		SDL_PixelFormat destination_pixel_format;
		memset(&destination_pixel_format,0,sizeof(destination_pixel_format));
		destination_pixel_format.BytesPerPixel  =   bytesPerPixel;
		destination_pixel_format.BitsPerPixel   =   bytesPerPixel * 8;
		// masks & shifts to retrieve the r,g,b,a components:
		// SDL interprets each pixel as a 32-bit number, so our masks must depend
		// on the endianness (byte order) of the machine
		#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		int shift = 32 - bytesPerPixel*8;
		destination_pixel_format.Rmask = 0xff000000>>shift;
		destination_pixel_format.Gmask = 0x00ff0000>>shift;
		destination_pixel_format.Bmask = 0x0000ff00>>shift;
		destination_pixel_format.Amask = 0x000000ff>>shift;
		destination_pixel_format.Rshift = 24-shift;
		destination_pixel_format.Gshift = 16-shift;
		destination_pixel_format.Bshift = 8-shift;
		destination_pixel_format.Ashift = 0;
		#else
		destination_pixel_format.Rmask = 0x000000ff;
		destination_pixel_format.Gmask = 0x0000ff00;
		destination_pixel_format.Bmask = 0x00ff0000;
		destination_pixel_format.Amask = 0xff000000;
		destination_pixel_format.Rshift = 0;
		destination_pixel_format.Gshift = 8;
		destination_pixel_format.Bshift = 16;
		destination_pixel_format.Ashift = 24;
		#endif

		// ... Lastly, convert the source image pixel format to the desired one (if they don't match already)

		SDL_Surface *destination_image;

		if (	destination_pixel_format.BytesPerPixel	!=	source_image->format->BytesPerPixel ||
				destination_pixel_format.Rmask			!=	source_image->format->Rmask ||
				destination_pixel_format.Gmask			!=	source_image->format->Gmask ||
				destination_pixel_format.Bmask			!=	source_image->format->Bmask ||
				destination_pixel_format.Amask			!=	source_image->format->Amask	)
		{
			destination_image =	SDL_ConvertSurface(
									source_image,
									&destination_pixel_format,
									SDL_SWSURFACE	// no need to upload to the graphics card just yet
			);

			SDL_FreeSurface(source_image);

			if (!destination_image)
			{
				Cleanup();
				Fatal_Error("Couldn't convert surface to RGB(A) for texture \"" + filename + "\"");
			}
		}
		else
			destination_image =	source_image;

		if (i == 0)
		{
			m_w = destination_image->w;
			m_h	= destination_image->h;
#if 0
			// Compute the average color
			int x,y;
			unsigned int r = 0, g = 0, b = 0;
			for (y = 0; y < m_h; y += 4)
				for (x = 0; x < m_w; x += 4)
				{
					byte *pixels = (byte *)destination_image->pixels + (x + y * m_w) * bytesPerPixel;
					r += pixels[0];
					g += pixels[1];
					b += pixels[2];
				}

			float scale = float( (m_w / 4) * (m_h / 4) * 255 );
			const float amount = 0.2f;
			m_color.r = Interp( r / scale, 1.0f, amount);
			m_color.g = Interp( g / scale, 1.0f, amount);
			m_color.b = Interp( b / scale, 1.0f, amount);
#endif
		}
		else
		{
			if ( (m_w != destination_image->w) || (m_h != destination_image->h) )
			{
				Cleanup();
				Fatal_Error("Animation frame " + ToString(i) + " is not the same size as frame 0");
			}
		}

		// We can now generate an OpenGL texture and free any SDL surface

		m_ids.push_back( Generate_GL_Texture(destination_image->pixels,bytesPerPixel,glFormat,GL_UNSIGNED_BYTE,BuildMipmaps) );
		SDL_FreeSurface(destination_image);
	}

	m_curr_id = m_ids[0];
}

texture_t :: ~texture_t()
{
	Cleanup();
}

void texture_t :: UpdateAnimFrame(float time)
{
	if ( !IsAnimated() )
		return;

	m_curr_id	=	m_ids[ int(time / m_animduration * m_animframes) % m_animframes ];
}
