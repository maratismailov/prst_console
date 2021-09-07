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

	Band Pass Filter

*******************************************************************************/

#ifndef FILTER_H_DEF
#define FILTER_H_DEF

void Filter(float * b_first, float * b_last, float fmin, float fmax, float dt);

// Remove mean
template< typename T >
void Rmean(T * b_first, T * b_last)
{
	T *b;
	T sum	=	0;

	for (b = b_first; b <= b_last; b++)
		sum += *b;

	T mean	=	sum / (b_last - b_first + 1);

	for (b = b_first; b <= b_last; b++)
		*b -= mean;
}

// Replace sample buffer with its integral
template< typename T >
void Integrate(T * b_first, T * b_last, T dt)
{
	T *b;
	T half_dt = dt / 2;
	T sample_prev, sample_curr;

	sample_prev = *b_first;
	*b_first = 0;
	for (b = b_first+1; b <= b_last; b++)
	{
		sample_curr	=	*b;
		*b			=	*(b-1) + half_dt * (sample_prev + sample_curr);
		sample_prev	=	sample_curr;
	}
}

// Calc integral of a sample buffer
template< typename T >
T Integral(T * b_first, T * b_last, T dt)
{
	if (b_first == b_last)
		return 0;

	// (x[0]+x[1])*dt/2 + (x[1]+x[2])*dt/2 + .. + (x[n-3]+x[n-2])*dt/2 + (x[n-2]+x[n-1])*dt/2 = 
	// = x[0]*dt/2 + (x[1]+ .. x[n-2])*dt + x[n-1]*dt/2
	T *b;
	T sum	=	(*b_first + *b_last) / 2;

	for (b = b_first+1; b <= b_last-1; b++)
		sum += *b;

	return sum * dt;
}

#endif
