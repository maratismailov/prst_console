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

	rtmag_t - Compute PDF for the earthquake magnitude, by multiplying the Gaussians
	          PDFs for each time window/station, which in turn are based on the measured
			  peak displacement and distance. Also multiply with the Gutenberg-Richter.

	Based on: Lancieri M., Zollo A., "Bayesian approach to the real-time estimation
	          of magnitude from the early P and S wave displacement peaks",
			  J Geophys Res 2008; 113(B12), doi: 10.1029/2007JB005386

*******************************************************************************/

#ifndef RTMAG_H_DEF
#define RTMAG_H_DEF

#include "global.h"

enum magtype_t { MAG_S = 0, MAG_P_SHORT, MAG_P_LONG, MAG_SIZE };
enum magcomp_t { MAGCOMP_VERTICAL = 0, MAGCOMP_HORIZONTAL, MAGCOMP_ALL };
enum magfilt_t { MAGFILT_LOW = 0, MAGFILT_HIGH };

struct magwin_t
{
	string label;
	float duration;
	magcomp_t comp;
	float A,  B,  C;
	float dA, dB, dC;
	float SE;
	float Msat;
};

class rtmag_t
{
private:

	double m_min, m_max, m_step;

	double *samples, *buffer, *gr;
	int num_samples;

	static magwin_t magwin[MAG_SIZE];
	static double gr_beta;

	struct maginput_t
	{
		string label;
		magtype_t magtype;
		double PD, R, dR;
	};

	vector<maginput_t> inputs;

	// Calc the initial distribution (Gutenberg-Richter)
	void CalcGR();

	double CalcIntegral(double *buf);

	double Normalize(double *buf);

	void FindMagnitudeMean ( double *buf, float *mag );
	void FindMagnitudeError( double *buf, float *mag_min, float *mag_max );

	void RemoveMagnitudeOutliers( stringstream & rtmag_log );

public:

	rtmag_t();
	~rtmag_t();

	void Init(double _m_min, double _m_max, double _m_step, const string & filename);

	void ClearPeaks();
	int GetNumPeaks();
	void AddPeak(const string & label, magtype_t magtype, float PD, float R, float dR);
	void CalcMagnitude(float *mag, float *mag_min, float *mag_max, stringstream & rtmag_log);

	const string & GetLabel(magtype_t magtype) const
	{
		return magwin[magtype].label;
	}

	magcomp_t GetComponents(magtype_t magtype) const
	{
		return magwin[magtype].comp;
	}

	float GetDuration(magtype_t magtype) const
	{
		return magwin[magtype].duration;
	}

	// FIXME legacy for calculating the average magnitude
	float Mag( magtype_t magtype, float PD, float R ) const;
	float UnboundMag( magtype_t magtype, float PD, float R ) const;

	// For debug
	void SaveMagDistribution();
};

extern rtmag_t rtmag;

#endif
