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

#include <cmath>
#include <fstream>

#include "pgx.h"

pgx_t pga, pgv;

/*******************************************************************************

	Example attenuation relations:

	Low Magnitude (ShakeMap's Small.pm):

	log10(PGA) = A + B * (Mag - 6) + C * log10(sqrt(R_epicenter_km^2 + H^2)) [cm/s^2]

	PGA:

	  A         B          C       H     sigma
	--------------------------------------------
	4.037  |  0.572  |  -1.757  |  6  |  0.836

	PGV:

	  A         B          C       H     sigma
	--------------------------------------------
	2.223  |  0.740  |  -1.368  |  6  |  0.753



	High Magnitude (Europe, Akkar & Bommer 2007):

	log10(PGA) = b1 + b2 * Mag + b3 * Mag^2 + (b4 + b5 * Mag) * log10( sqrt( R_epi^2 + b6^2 ) ) [cm/s^2]
	sigma_log10(pgx) = sqrt( (s1 + s1m * Mag)^2 + (s2 + s2m * Mag)^2 )

	PGA:

	  B1       B2         B3         B4         B5        B6       S1         S1M       S2        S2M
	---------------------------------------------------------------------------------------------------
    1.647  |  0.767  |  -0.074  |  -3.162  |  0.321  |  7.682  |  0.557  |  -0.049  |  0.189  |  -0.017

	PGV:

	  B1       B2         B3         B4         B5        B6       S1         S1M       S2        S2M
	---------------------------------------------------------------------------------------------------
    -1.36  |  1.063  |  -0.079  |  -2.948  |  0.306  |  5.547  |  0.85   |  -0.096  |  0.313  |  -0.040


	High Magnitude (Japan, Kanno et al., 2006):

	log10(PGA) = 0.56 * M -0.0031 * R - log10( R + 0.0055 * 10^(0.5 * M)) + 0.26

*******************************************************************************/

pgx_t :: pgx_t()
{
}

void pgx_t :: Init( const string & filename )
{
	// Load formula

	cout << endl;
	cout << "==================================================================================================" << endl;
	cout << "    Peak Ground Motion (" << filename << ")" << endl;
	cout << endl;
	cout << "    Two formulas to calculate from the earthquake magnitude, epicentral distance (km) and depth (km)," << endl;
	cout << "    the log10 of peak ground motion (in cm/s or cm/s^2) and associated error:" << endl;
	cout << endl;
	cout << "    log10_pgx(Mag, R_epi, Dep)" << endl;
	cout << "    err_log10_pgx(Mag, R_epi, Dep)" << endl;
	cout << "==================================================================================================" << endl;

	ifstream f(filename.c_str());
	if (!f)
		Fatal_Error("Couldn't open peak ground motion file \"" + filename + "\"");

	// PGX formula
	SkipComments(f);
	string formula_pgx = "";
	do
	{
		getline(f, formula_pgx);
		Trim(formula_pgx, " \t");
	}
	while (formula_pgx.empty() && !f.fail() && !f.eof());

	// PGX error formula
	SkipComments(f);
	string formula_err = "";
	do
	{
		getline(f, formula_err);
		Trim(formula_err, " \t");
	}
	while (formula_err.empty() && !f.fail() && !f.eof());

	if (f.fail() && !f.eof())
		Fatal_Error("Reading formulas in peak ground motion file \"" + filename + "\"");

	cout <<	"    " << formula_pgx << endl;
	cout <<	"    " << formula_err << endl;

	cout << "==================================================================================================" << endl;

	parser.DefineVar("Mag",		&pgxMag);
	parser.DefineVar("R_epi",	&pgxR_epi);
	parser.DefineVar("Dep",		&pgxDep);

	parser.SetExpr( formula_pgx + (formula_err.empty() ? "" : ("," + formula_err)) );

	// Test the formulas

	pgxMag		=	5;
	pgxR_epi	=	150;
	pgxDep		=	50;

	try
	{
		int resnum = -1;
		(void)parser.Eval(resnum);
		if (resnum != 2)
			Fatal_Error("Wrong number of formulas (" + ToString(resnum) + ") in peak ground motion file \"" + filename + "\".\nTwo expected, separated by new-lines:\nlog10_pgx(Mag, R_epi, Dep)\nerr_log10_pgx(Mag, R_epi, Dep)");
	}
	catch(Parser::exception_type &e)
	{
		Fatal_Error("Invalid formulas in peak ground motion file \"" + filename + "\":\n" + e.GetMsg());
	}
}

// Calc peak ground motion at a distance in cm/s or cm/s^2
void pgx_t :: CalcPeak( float Mag, float R_epi, float Dep, range_t *peak )
{
	if (Mag == -1)
	{
		peak->min = peak->max = peak->val = 0;
		return;
	}

	pgxMag		=	Mag;
	pgxR_epi	=	R_epi;
	pgxDep		=	Dep;

	int resnum;
	value_type *res = parser.Eval(resnum);

	float log10_peak		=	res[0];
	float error_log10_peak	=	res[1];

	peak->min	=	pow( 10.0f, log10_peak - error_log10_peak);
	peak->max	=	pow( 10.0f, log10_peak + error_log10_peak);
	peak->val	=	pow( 10.0f, log10_peak );

	// Return 0 on NaN / Infinite as it's less hassle on the users this way (shouldn't happen anyway)
	// Note: VC provides the non-standard _isnan and _finite (instead of isnan and isfinite)

	if ( !_finite(peak->min) )
		peak->min = 0;
	if ( !_finite(peak->max) )
		peak->max = 0;
	if ( !_finite(peak->val) )
		peak->val = 0;
}
