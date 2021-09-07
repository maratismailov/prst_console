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

	pgx_t - container for user-supplied formulas to compute a peak parameter
	        (and uncertainty) at a target site. i.e. PGA / dPGA at the target
			from magnitude, distance and depth of the earthquake.

			The formula is evaluated at run time using the muParser library.

*******************************************************************************/

#ifndef PGX_H_DEF
#define PGX_H_DEF

#include "muParser.h"

#include "global.h"

using namespace mu;

// *** Important: do not use pgx_t objects from threads other than the main one
class pgx_t
{
private:

	Parser parser;

	value_type pgxMag, pgxR_epi, pgxDep;

public:

	pgx_t();

	void Init(const string & filename);

	// Calc peak ground motion at a distance in cm/s or cm/s^2
	void CalcPeak( float Mag, float R_epi, float Dep, range_t *peak );
};

extern pgx_t pga, pgv;

#endif
