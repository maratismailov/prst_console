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

*******************************************************************************/

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <cstring>

#include "rtmag.h"

#include "config.h"

magwin_t rtmag_t :: magwin[MAG_SIZE];
double rtmag_t :: gr_beta;
rtmag_t rtmag;

/*******************************************************************************

	M = ( log10(Pd) - A - C * log10(R_hypo_km / 10) ) / B

	**************

	Japan:

	M             A                 B                  C            SE
	-------------------------------------------------------------------
	2P  |  -6.93 +/- 0.48  |  0.75 +/- 0.09  |  -1.13 +/- 0.06  |  0.32
	4P  |  -6.46 +/- 0.48  |  0.70 +/- 0.09  |  -1.05 +/- 0.10  |  0.40
	2S  |  -6.34 +/- 0.48  |  0.81 +/- 0.09  |  -1.33 +/- 0.05  |  0.37

	Gutenberg-Richter Beta = 1.3 (Uetzu 1980)

	**************

	Europe:

	M		A			B			C
	-----------------------------------------
	2P	|	-6.31	|	0.70	|	-1.05
	4P	|	-6.46	|	0.70	|	-1.05
	2S	|	-5.77	|	0.71	|	-0.71

	Gutenberg-Richter Beta = 1.0

	**************

	Pd_3sec vs Mw, using European earthquakes (including L'Aquila 2009 sequence):

	log10(Pd)   = C  + B  * Mw + A * log10(R_hypo_km)	[1]
	log10(Pd10) = A' + B' * Mw							[2]

	From [1] and [2]

	log10(Pd) =	C  + B  * Mw + A * log10(10) + A * log10(R_hypo_km/10) =

				A' + B' * Mw + A * log10(R_hypo_km/10)	[3]

	Where:

	A					B				C
	-1.93 (+/-0.1021)	0.77 (+/-0.03)	-4.85 (+/-0.179)
	
	A'					B'
	-6.84 (+/-0.14)		0.78 (+/-0.03)

	Mw            A                 B                  C             SE
	--------------------------------------------------------------------
	3P  |  -6.84 +/- 0.14  |  0.78 +/- 0.03  |  -1.93 +/- 0.1021 |  0.48

*******************************************************************************/

rtmag_t :: rtmag_t()
:	samples(NULL), buffer(NULL), gr(NULL)
{
}

rtmag_t :: ~rtmag_t()
{
	delete [] samples;
	delete [] buffer;
	delete [] gr;
}

void rtmag_t :: ClearPeaks()
{
	inputs.clear();
}

int rtmag_t :: GetNumPeaks()
{
	return int(inputs.size());
}

void rtmag_t :: Init(double _m_min, double _m_max, double _m_step, const string & filename)
{
	magwin[MAG_S].duration			=	(float)param_magnitude_s_secs;
	magwin[MAG_S].label				=	ToString((int)param_magnitude_s_secs)		+ "S";

	magwin[MAG_P_SHORT].duration	=	(float)param_magnitude_p_secs_short;
	magwin[MAG_P_SHORT].label		=	ToString((int)param_magnitude_p_secs_short)	+ "P";

	magwin[MAG_P_LONG].duration		=	(float)param_magnitude_p_secs_long;
	magwin[MAG_P_LONG].label		=	ToString((int)param_magnitude_p_secs_long)	+ "P";

	m_min  = _m_min;
	m_max  = _m_max;
	m_step = _m_step;

	num_samples = int ( ceil( (m_max - m_min) / m_step ) + 1 );

	delete [] samples;
	delete [] buffer;
	delete [] gr;

	samples	=	new double[num_samples];
	buffer	=	new double[num_samples];
	gr		=	new double[num_samples];

	CalcGR();

	ClearPeaks();

	// Load magwin

	cout << endl;
	cout << "==================================================================================================" << endl;
	cout << "    Magnitude (" << filename << ")" << endl;
	cout << endl;
	cout << "    M = ( log10(Pd) - A - C * log10(R_hypo_km / 10) ) / B" << endl;
	cout << "    For M >= Msat, the PDF is constant" << endl;
	cout << endl;
	cout << "    For each magnitude window, ie SECS_WAVE in {2P, 4P, 2S} and COMP in {z,ne,zne}:" << endl;
	cout << "    SECS_WAVE | COMP | A | dA | B | dB | C | dC | SE | Msat" << endl;
	cout << endl;
	cout << "    Gutenberg-Richter beta coefficient:" << endl;
	cout << "    GR        | Beta" << endl;
	cout << "==================================================================================================" << endl;

	ifstream f(filename.c_str());
	if (!f)
		Fatal_Error("Couldn't open magnitude file \"" + filename + "\"");

	string maglabel;
	int secs;
	char phase;
	string comp;
	float A, B, C, dA, dB, dC, SE, Msat;
	int magtype;

	const float UNDEF = -12345.0f;

	// Init coefficients to "undefined"

	for (magtype = 0; magtype < MAG_SIZE; magtype++)
	{
		magwin[magtype].comp = MAGCOMP_ALL;
		magwin[magtype].A = magwin[magtype].dA =
		magwin[magtype].B = magwin[magtype].dB =
		magwin[magtype].C = magwin[magtype].dC =
		magwin[magtype].SE = magwin[magtype].Msat = UNDEF;
	}
	gr_beta = (double)UNDEF;
	
	// Read them from rtmag.txt
	for(;;)
	{
		streampos pos;

		SkipComments(f);

		pos = f.tellg();

		f >> maglabel;

		if (!f)
			break;

		if (maglabel == "GR")
		{
			if (gr_beta != (double)UNDEF)
				Fatal_Error("Gutenberg-Richter Beta coefficient is defined more than once in magnitude file \"" + filename + "\"");

			f >> gr_beta;

			if (!f)
				Fatal_Error("Invalid Gutenberg-Richter Beta coefficient in magnitude file \"" + filename + "\" (default: 0)");
		}
		else
		{
			f.seekg(pos);

			f >> secs >> phase >> comp >> A >> dA >> B >> dB >> C >> dC >> SE >> Msat;

			if (!f)
				Fatal_Error("Not enough columns for line starting with \"" + maglabel + "\" in magnitude file \"" + filename + "\"");

			if (phase != 'P' && phase != 'S')
				Fatal_Error("Wrong phase \"" + maglabel + "\" in magnitude file \"" + filename + "\"");

			comp = ToLower(comp);
			if (comp != "z" && comp != "ne" && comp != "zne")
				Fatal_Error("Wrong components \"" + comp + "\" in magnitude file \"" + filename + "\"");

			if ( maglabel == magwin[MAG_S].label )
				magtype = MAG_S;
			else if ( maglabel == magwin[MAG_P_SHORT].label )
				magtype = MAG_P_SHORT;
			else if ( maglabel == magwin[MAG_P_LONG].label )
				magtype = MAG_P_LONG;
			else
				magtype = MAG_SIZE;

			if ( magtype != MAG_SIZE )
			{
				if (magwin[magtype].A != UNDEF)
					Fatal_Error(magwin[magtype].label + " coefficients are defined more than once in magnitude file \"" + filename + "\"");

				if (comp == "z")
					magwin[magtype].comp = MAGCOMP_VERTICAL;
				else if (comp == "ne")
					magwin[magtype].comp = MAGCOMP_HORIZONTAL;
				else
					magwin[magtype].comp = MAGCOMP_ALL;

				magwin[magtype].A	 = A;	magwin[magtype].dA = dA;
				magwin[magtype].B	 = B;	magwin[magtype].dB = dB;
				magwin[magtype].C	 = C;	magwin[magtype].dC = dC;
				magwin[magtype].SE = SE;
				magwin[magtype].Msat = Msat;
			}
		}
	}

	// Log values and error out if some are still "undefined"
	for (magtype = 0; magtype < MAG_SIZE; magtype++)
	{
		if  (	(param_magnitude_s_secs       && magtype == MAG_S)			|| 
				(param_magnitude_p_secs_short && magtype == MAG_P_SHORT)	|| 
				(param_magnitude_p_secs_long  && magtype == MAG_P_LONG)		)
		{
			if (magwin[magtype].A == UNDEF)
				Fatal_Error(magwin[magtype].label + " coefficients are missing from magnitude file \"" + filename + "\"");

			// Expand magnitude label to include the used components
			if (magwin[magtype].comp == MAGCOMP_VERTICAL)
				magwin[magtype].label += "z";
			else if (magwin[magtype].comp == MAGCOMP_HORIZONTAL)
				magwin[magtype].label += "ne";
			else
				/*magwin[magtype] += "zne"*/;

			cout <<	setw(6) << magwin[magtype].label											<< " | " <<
					setw(6) << magwin[magtype].A  << " +/- " << setw(4) << magwin[magtype].dA	<< " | " <<
					setw(6) << magwin[magtype].B  << " +/- " << setw(4) << magwin[magtype].dB	<< " | " <<
					setw(6) << magwin[magtype].C  << " +/- " << setw(4) << magwin[magtype].dC	<< " | " <<
					" +/- " << setw(4) << magwin[magtype].SE										<< " | " <<
					magwin[magtype].Msat															<<
					endl;
		}
	}

	if (gr_beta == (double)UNDEF)
		Fatal_Error("Gutenberg-Richter Beta coefficient is missing from magnitude file \"" + filename + "\"");
	else
		cout <<	setw(6) << "GR" << " | " << setw(6) << gr_beta << endl;

	cout << "==================================================================================================" << endl;
}

float rtmag_t :: UnboundMag( magtype_t magtype, float PD, float R ) const
{
	if (PD == -1)
		return -1;

	float A = magwin[magtype].A;
	float B = magwin[magtype].B;
	float C = magwin[magtype].C;

	return ( log10(PD) - A - C * log10(R / 10.0f) ) / B;
}

float rtmag_t :: Mag( magtype_t magtype, float PD, float R ) const
{
	if (PD == -1)
		return -1;

	float mag = UnboundMag(magtype, PD, R);
	Clamp( mag, float(m_min), float(m_max) );

	return mag;
}

void rtmag_t :: AddPeak(const string & label, magtype_t magtype, float PD, float R, float dR)
{
	if ( (PD == -1) || (PD == 0) || (R == 0) )
		return;

	struct maginput_t in = { label, magtype, double(PD), double(R), double(dR) };
	inputs.push_back( in );
}

void rtmag_t :: CalcGR()
{
	double scale	=	1.0;
	if (gr_beta)
		scale /= exp( -gr_beta * m_min ) - exp( -gr_beta * m_max );

	double m = m_min;
	double *s = gr, *end = gr + num_samples;
	for (; s < end; s++, m += m_step)
	{
		*s = scale * exp( -gr_beta * m );
	}
}

float CalcMedian(vector<float> & inputs)
{
	size_t size = inputs.size();
	if (!size)
		return 0;

	sort(inputs.begin(), inputs.end());

	if (size & 1)
		return inputs[size/2];
	else
		return (inputs[size/2-1] + inputs[size/2]) / 2;
}

// Iglewicz and Hoaglin's robust test for multiple outliers (two sided test)
void rtmag_t :: RemoveMagnitudeOutliers( stringstream & rtmag_log )
{
	if (inputs.size() < 3)
		return;

	// PD -> M
	vector<float> mags;
	for (vector<maginput_t>::iterator in = inputs.begin(); in != inputs.end(); ++in)
		mags.push_back( UnboundMag(in->magtype, float(in->PD), float(in->R)) );

	// Compute MED = median(Mi)
	float MED = CalcMedian(mags);

	// Compute MAD = median(abs(Mi-MED))
	vector<float> devs;
	for (vector<float>::iterator in = mags.begin(); in != mags.end(); ++in)
		devs.push_back( abs(*in - MED) );
	float MAD = CalcMedian(devs);

	// Values with score abs(0.6745 * (Mi - MED) / MAD) > Threshold (e.g. 3.5) are outliers
	const float thresh = float(param_magnitude_outlier_threshold);
	for (vector<maginput_t>::iterator in = inputs.begin(); in != inputs.end();)
	{
		float mag = UnboundMag(in->magtype, float(in->PD), float(in->R));
		float Z = 0.6745f * (mag - MED) / MAD;
		if ( abs(Z) >= thresh )
		{
			rtmag_log << SecsToString(SecsNow()) << ": BADMAG " << in->label << " Mag: " << mag << " Med: " << MED << " MAD: " << MAD << " score: " << Z << endl;
			in = inputs.erase(in);
		}
		else
			++in;
	}
}

void rtmag_t :: CalcMagnitude(float *mag, float *mag_min, float *mag_max, stringstream & rtmag_log)
{
	// See: JOURNAL OF GEOPHYSICAL RESEARCH, VOL. 113, B12302, doi:10.1029/2007JB005386, 2008, pag. 5

	*mag = *mag_min = *mag_max = -1;

	// Remove strong magnitude outliers
	// (e.g. very high values from a broken sensor, very low values from a wrong pick etc.)
	if (param_magnitude_outlier_threshold)
		RemoveMagnitudeOutliers(rtmag_log);

	if (inputs.empty())
		return;

	// Init samples with the GR

	memcpy(samples, gr, num_samples * sizeof(samples[0]));

	// Multiply with the lognormal distribution for each magnitude type and station

	vector<maginput_t>::const_iterator in = inputs.begin(), in_end = inputs.end();
	for ( ; in != in_end; in++ )
	{
		magwin_t *t		=	&magwin[in->magtype];

		const double R0		=	10.0;

		double log_PD		=	log10( double(in->PD) );
		double sigma_log_PD	=	t->SE + abs(log10( in->R / R0 ) * t->dC) + abs(t->C * (1.0 / (in->R / R0)) * (1.0 / log(10.0)) * in->dR / R0);

		if (!sigma_log_PD)
			sigma_log_PD = 1.0;

		double scale		=	1.0 / ( sqrt( 2 * FLOAT_PI ) * sigma_log_PD * in->PD );

		double exp_scale	=	1.0 / ( -2 * Sqr( sigma_log_PD ) );

		double mu_m			=	t->A + t->B * m_min + t->C * log10( in->R / R0 );
		double dmu_m		=	t->B * m_step;

		// Saturation for M > Msat

		double mu_m_sat = t->A + t->B * t->Msat + t->C * log10( in->R / R0 );

		// Calc new distribution in buffer

		double *b	=	buffer;
		double *end	=	buffer + num_samples;
		for (; b < end; b++)
		{
			*b = scale * exp( exp_scale * Sqr( log_PD - mu_m ) );

			if (mu_m < mu_m_sat)
				mu_m += dmu_m;
		}

		rtmag_log << SecsToString(SecsNow()) << ": RTMAG " << in->label << " PD: " << in->PD << " sigma_log_PD: " << sigma_log_PD << " scale: " << scale;
		double sum = Normalize(buffer);
		rtmag_log << " this_integral: " << sum;

		// for debug only
		FindMagnitudeMean( buffer, mag );
		rtmag_log << " this_mag: " << *mag;

		// Multiply with previous distribution

		b			=	buffer;
		end			=	buffer + num_samples;
		double *s	=	samples;
		for (; b < end; b++, s++)
		{
			// FIXME multiply by a large constant, or the integral becomes vanishingly small (yielding a magnitude of 0)
			*s *= *b * 1e150;
		}

		sum = Normalize(samples);
		rtmag_log << " overall_integral: " << sum;

		// for debug only
		FindMagnitudeMean( samples, mag );
		FindMagnitudeError( samples, mag_min, mag_max );
		rtmag_log << " overall_mag: " << *mag << " (" << *mag_min << ", " << *mag_max << ")" << endl;
	}

	// Find mean and error

	FindMagnitudeMean( samples, mag );

	FindMagnitudeError( samples, mag_min, mag_max );

}

void rtmag_t :: FindMagnitudeMean( double *buf, float *mag )
{
	double *s_sel	=	buf;
	double val_sel	=	*s_sel;

	double *s		=	buf + 1;
	double *end		=	buf + num_samples;

	for (; s < end; s++)
	{
		if (*s > val_sel)
		{
			val_sel	=	*s;
			s_sel	=	s;
		}
	}
	*mag = float( m_min + (s_sel - buf) * m_step );
}

void rtmag_t :: FindMagnitudeError( double *buf, float *mag_min, float *mag_max )
{
	const double half_m_step = m_step / 2;

	double integral = CalcIntegral(buf);

	const double range_sum_min = ( integral * .05 ) / half_m_step;
	const double range_sum_max = ( integral * .95 ) / half_m_step;

	double range_m_min = m_min;
	double range_m_max = m_max;

	double prev_sample, sum;

	sum = 0;
	prev_sample = 0;

	double *s	=	buf;
	double *end	=	buf + num_samples;
	for (; s < end; s++)
	{
		sum += (prev_sample + *s);
		prev_sample = *s;

		if (sum >= range_sum_min)
			break;
	}
	range_m_min = m_min + (s - buf) * m_step;
	Clamp(range_m_min, m_min, m_max);

	++s;
	for (; s < end; s++)
	{
		sum += (prev_sample + *s);
		prev_sample = *s;

		if (sum >= range_sum_max)
			break;
	}
	range_m_max = m_min + (s - buf) * m_step;
	Clamp(range_m_max, m_min, m_max);

	if (range_m_min > range_m_max)
		swap (range_m_min, range_m_max);

	*mag_min = float( range_m_min );
	*mag_max = float( range_m_max );
}

double rtmag_t :: CalcIntegral(double *buf)
{
	double *b, *last;

	// Calc full integral

	double sum = buf[0]/2 + buf[num_samples-1]/2;

	b		=	buf + 1;
	last	=	buf + num_samples - 2;
	for (; b <= last; b++)
		sum += *b;

	return sum * m_step;
}

double rtmag_t :: Normalize(double *buf)
{
	double *b, *last;

	double sum = CalcIntegral(buf);
	if (!sum)
		return sum;

	b		=	buf;
	last	=	buf + num_samples - 1;
	for (; b <= last; b++)
		*b /= sum;

	return sum;
}

void rtmag_t :: SaveMagDistribution()
{
	string filename;
	filename = SecsToString(SecsNow());
	filename = Replace(filename, " ", "_");
	filename = Replace(filename, ":", ".");
	filename += ".rtmag";

	if (realtime)
		filename = net_dir + filename;
	else
		filename = sacs_dir + filename;

	ofstream f(filename.c_str(), ios::trunc);
	if (f)
	{
		double *b   = samples;
		double *end = samples + num_samples;
		for (; b < end; b++)
		{
			f << m_min + (b - samples) * m_step << " " << *b << endl;
		}
	}
}
