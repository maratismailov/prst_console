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

	Band Pass Filters (2 or 4 Poles, 1 Pass)

*******************************************************************************/

#include <cmath>

#include "global.h"

#include "filter.h"

/*******************************************************************************

Difference equations for a two-pole Band-Pass filter.

Consider y[k] to be the filtered field of the initial field x[k], the recursive
difference equation for the filter is:

	y[k] = a0/b0 * (x[k] - x[k-2]) - b1/b0 * y[k-1] - b2/b0 * y[k-2]

Where:

	a0 = Bc
	b0 = c^2 + Bc + 1
	b1 = -2(c^2 - 1)
	b2 = c^2 - Bc + 1

The value of c is related to the central frequency fc and the time sample Dt:

	c = ctg( wc * Dt / 2 )	[where wc = 2 * PI * fc]

While B is the inverse of the quality factor, that is the ratio between
the bandwidth and the central frequency:

	B = (fmax - fmin) / fc

*******************************************************************************/

void Filter_2_Poles(float * b_first, float * b_last, float fmin, float fc, float fmax, float dt)
{
	float B = (fmax - fmin) / fc;
	float c = 1.0f / tan(2 * FLOAT_PI * fc * dt / 2.0f);

	float a0 = B * c;
	float b0 = c*c + a0 + 1;
	float b1 = - 2 * (c*c - 1);
	float b2 = c*c - a0 + 1;

	float c0 = a0 / b0;
	float c1 = b1 / b0;
	float c2 = b2 / b0;

	float x;
	float x_1 = 0;
	float x_2 = 0;

	float y;
	float y_1 = 0;
	float y_2 = 0;

	for (float *b = b_first; b <= b_last; b++)
	{
		//	y[k] = a0/b0 * (x[k] - x[k-2]) - b1/b0 * y[k-1] - b2/b0 * y[k-2]
		x = *b;
		y = c0 * (x - x_2) - c1 * y_1 - c2 * y_2;
		*b = y;

		x_2 = x_1;
		x_1 = x;

		y_2 = y_1;
		y_1 = y;
	}
}

/*******************************************************************************

Difference equations for a four-pole Band-Pass filter.

Consider y[k] to be the filtered field of the initial field x[k], the recursive
difference equation for the filter is:

	y[k] = a0/b0 * (x[k] - 2 * x[k-2] + x[k-4]) - b1/b0 * y[k-1] - b2/b0 * y[k-2] - b3/b0 * y[k-3] - b4/b0 * y[k-4]

Where:

	a0 = B^2 * c^2
	b0 = a0 + sqrt(2) * B * c * (1 + c^2) + (1 + c^2)^2
	b1 = -2 * sqrt(2) * (c^2 - 1) * (B * c + sqrt(2) * c^2 + sqrt(2))
	b2 = -2 * (a0 -3 * c^4 + 2 * c^2 - 3)
	b3 = 2 * sqrt(2) * (c^2 - 1) * (B * c - sqrt(2) * c^2 - sqrt(2))
	b4 = a0 - sqrt(2) * B * c * (1 + c^2) + (1 + c^2)^2

The value of c is related to the central frequency fc and the time sample Dt:

	c = ctg( wc * Dt / 2 )	[where wc = 2 * PI * fc]

While B is the inverse of the quality factor, that is the ratio between
the bandwidth and the central frequency:

	B = (fmax - fmin) / fc

*******************************************************************************/

void Filter_4_Poles(float * b_first, float * b_last, float fmin, float fc, float fmax, float dt)
{
	float B = (fmax - fmin) / fc;
	float c = 1.0f / tan(2 * FLOAT_PI * dt / 2.0f);

	float a0 = B*B * c*c;
	float b0 = a0 + sqrt(2.0f) * B * c * (1 + c*c) + Sqr(1 + c*c);
	float b1 = -2 * sqrt(2.0f) * (c*c - 1) * (B * c + sqrt(2.0f) * c*c + sqrt(2.0f));
	float b2 = -2 * (a0 - 3 * c*c*c*c + 2 * c*c - 3);
	float b3 = 2 * sqrt(2.0f) * (c*c - 1) * (B * c - sqrt(2.0f) * c*c - sqrt(2.0f));
	float b4 = a0 - sqrt(2.0f) * B * c * (1 + c*c) + Sqr(1 + c*c);

	float c0 = a0 / b0;
	float c1 = b1 / b0;
	float c2 = b2 / b0;
	float c3 = b3 / b0;
	float c4 = b4 / b0;

	float x;
	float x_1 = 0;
	float x_2 = 0;
	float x_3 = 0;
	float x_4 = 0;

	float y;
	float y_1 = 0;
	float y_2 = 0;
	float y_3 = 0;
	float y_4 = 0;

	for (float *b = b_first; b <= b_last; b++)
	{
		//	y[k] = a0/b0 * (x[k] - 2 * x[k-2] + x[k-4]) - b1/b0 * y[k-1] - b2/b0 * y[k-2] - b3/b0 * y[k-3] - b4/b0 * y[k-4]
		x = *b;
		y = c0 * (x - 2 * x_2 + x_4) - c1 * y_1 - c2 * y_2 - c3 * y_3 - c4 * y_4;
		*b = y;

		x_4 = x_3;
		x_3 = x_2;
		x_2 = x_1;
		x_1 = x;

		y_4 = y_3;
		y_3 = y_2;
		y_2 = y_1;
		y_1 = y;
	}
}

/*******************************************************************************
  MatLab filter, 2 poles 1 pass:

	function bandpass

	trace=rsac('FKO010.sac');

	f_min=0.075;
	f_max=3;

	dt = trace(1,3)
	nyquist = 0.5/dt;

	% Operation to get displacement 
	% rmean- rtrend
	trace(:,2)=detrend(trace(:,2),'constant');
	trace(:,2)=detrend(trace(:,2),'linear');
	% double integration
	trace(:,2)=cumtrapz(trace(:,2))*dt;
	trace(:,2)=cumtrapz(trace(:,2))*dt;

	%high pass filtering 
	c=tan(pi*f_min*dt);
	c=1./c;

	la0=c^2;
	lb0=c^2+sqrt(2.)*c+1;
	lb0=1./lb0;
	lb1=-2.0*(c^2-1);
	lb2=c^2-sqrt(2.)*c+1;

	la0=la0*lb0;
	lb1=lb1*lb0;
	lb2=lb2*lb0;

	trace_suppl(1:2,2)=0.;
	for i = 3:nx
		trace_suppl(i,2) = la0 * (trace(i,2) -2*trace(i-1,2)+trace(i-2,2)) - lb1 * trace_suppl(i-1,2) -  lb2 * trace_suppl(i-2,2);
	end

	%low pass filtering 
	c=tan(pi*f_max*dt);
	c=1./c;
	la0=1.;
	lb0=c^2+sqrt(2.)*c+1;
	lb0=1./lb0;
	lb1=-2.0*(c^2-1);
	lb2=c^2-sqrt(2.)*c+1;

	la0=la0*lb0;
	lb1=lb1*lb0;
	lb2=lb2*lb0;

	trace=trace_suppl;
	trace_suppl(1:2,2)=0.;
	for i = 3:nx
		trace_suppl(i,2) = la0 * (trace(i,2) +2*trace(i-1,2)+trace(i-2,2)) - lb1 * trace_suppl(i-1,2) -  lb2 * trace_suppl(i-2,2);
	end

	%final array results into trace_suppl
*******************************************************************************/

void Filter_2_Poles_Matlab(float * b_first, float * b_last, float fmin, float fmax, float dt)
{
	float a0, b0,b1,b2, c, c0,c1,c2;
	float x, x_1, x_2;
	float y, y_1, y_2;
	float *b;

	// fmin,fmax must not be greater than the Nyquist frequency.

	float fny = (1.0f / dt) / 2;
	Clamp(fmin, 0.0f, fny);
	Clamp(fmax, 0.0f, fny);

	// High pass

	c = 1.0f / tan(FLOAT_PI * fmin * dt);

	a0 = c*c;

	b0 = c*c + sqrt(2.0f) * c + 1;
	b1 = -2 * (c*c - 1);
	b2 = c*c - sqrt(2.0f) * c + 1;

	c0 = a0 / b0;
	c1 = b1 / b0;
	c2 = b2 / b0;

	x_1 = x_2 = 0;
	y_1 = y_2 = 0;

	for (b = b_first; b <= b_last; b++)
	{
		// trace_suppl(i,2) = la0 * (trace(i,2) -2*trace(i-1,2)+trace(i-2,2)) - lb1 * trace_suppl(i-1,2) -  lb2 * trace_suppl(i-2,2);

		x = *b;
		y = c0 * (x - 2 * x_1 + x_2) - c1 * y_1 - c2 * y_2;
		*b = y;

		x_2 = x_1;
		x_1 = x;

		y_2 = y_1;
		y_1 = y;
	}

	// Low pass

	c = 1.0f / tan(FLOAT_PI * fmax * dt);

	a0 = 1;

	b0 = c*c + sqrt(2.0f) * c + 1;
	b1 = -2 * (c*c - 1);
	b2 = c*c - sqrt(2.0f) * c + 1;

	c0 = a0 / b0;
	c1 = b1 / b0;
	c2 = b2 / b0;

	x_1 = x_2 = 0;
	y_1 = y_2 = 0;

	for (b = b_first; b <= b_last; b++)
	{
//		trace_suppl(i,2) = la0 * (trace(i,2) +2*trace(i-1,2)+trace(i-2,2)) - lb1 * trace_suppl(i-1,2) -  lb2 * trace_suppl(i-2,2);
		x = *b;
		y = c0 * (x + 2 * x_1 + x_2) - c1 * y_1 - c2 * y_2;
		*b = y;

		x_2 = x_1;
		x_1 = x;

		y_2 = y_1;
		y_1 = y;
	}
}


void Filter(float * b_first, float * b_last, float fmin, float fmax, float dt)
{
//	Filter_2_Poles(b_first, b_last, fmin, fc, fmax, dt);
	Filter_2_Poles_Matlab(b_first, b_last, fmin, fmax, dt);
}
