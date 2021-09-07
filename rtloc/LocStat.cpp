/*
 * @file LocStat.c location statistics
 * 
 * Copyright (C) 2006-2007 Claudio Satriano <satriano@na.infn.it>
 * This file is part of RTLoc.
 * 
 * RTLoc is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * RTLoc is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * You should have received a copy of the GNU General Public License
 * along with RTLoc; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */ 

#include "rtloclib.h"
extern float tnow;

//TODO: simplify the API, maybe with a "hypocenter" data class
//luca
//void LocStat (GridDesc *Grid, double f_prob_max, GridDesc *Pgrid, GridDesc *Sgrid, Vect3D *ml_hypo, struct Station *station, int evid, struct Pick *pick, struct Control *params)
void LocStat (GridDesc *Grid, double f_prob_max, GridDesc *Pgrid, GridDesc *Sgrid, Vect3D *mean, Vect3D *ml_hypo, struct Station *station, int evid, struct Pick *pick, struct Control *params,		Mtrx3D *cov, Ellipsoid3D *ell, float *ml_otime)
{
	int i;

	int statid=0;
	int npick=0;
	float origtime=0;
	float rms=0;

	int nsta;

	int ix, iy, iz;
	int numx, numy, numz;

	Vect3D fmean = {0.0, 0.0, 0.0};
//luca
//	Mtrx3D cov = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
cov->xx = cov->xy = cov->xz = 0;
cov->yx = cov->yy = cov->yz = 0;
cov->zx = cov->zy = cov->zz = 0;
	Vect3D fhypo = {0.0, 0.0, 0.0};

	double x, y, z, xx, xy, xz, yy, yz, zz;
	float val;

//luca
//	float norm;
double norm;

	float *ttP, *ttS;
//luca
//	float tP=0, tS=0;
//	float tPobs=0; //measured P arrival time
//	Vect3D sta;
//	float dist=0;
//	float *ml_dist, *mean_dist;
//	float *d_log_dist;

//luca
//	Vect3D mean;
//	Ellipsoid3D ell;

	nsta=params->nsta;

	ttP=(float *) calloc(nsta, sizeof(float));
	ttS=(float *) calloc(nsta, sizeof(float));

/* ORIGIN TIME */
	i=0;
	npick=0;
	do {
		if ( pick[i].evid == evid ) {

			statid = pick[i].statid;

			ttP[npick] = ReadAbsInterpGrid3d(/*NULL,*/ &(Pgrid[statid]), ml_hypo->x, ml_hypo->y, ml_hypo->z);

			origtime += pick[i].time - ttP[npick];

			npick++;
		}
	} while ( pick[i++].next != NULL );
	origtime /= npick;
//luca
*ml_otime = origtime;

	i=0;
	npick=0;
	do {
		if ( pick[i].evid == evid ) {

			rms += pow(pick[i].time - (ttP[npick] + origtime), 2);

			npick++;
		}
	} while ( pick[i++].next != NULL );
	rms /= npick;
	rms = sqrt(rms);



/* MEAN and MAX LIKELIHOOD*/
//luca
//	mean.x = 0.0;
//	mean.y = 0.0;
//	mean.z = 0.0;
mean->x = 0.0;
mean->y = 0.0;
mean->z = 0.0;
	norm = 0.0;
	for (ix = 0; ix < Grid->numx; ix++) {
		for (iy = 0; iy < Grid->numy; iy++) {
			for (iz = 0; iz < Grid->numz; iz++) {
				val = Grid->array[ix][iy][iz];
//fprintf(stderr, "%d %d %d %f %f %f  ", ix, iy, iz, val, f_prob_max, val/f_prob_max);
				//if( val/f_prob_max < params->pdfcut ) val=0;
				if( val/f_prob_max < params->pdfcut ) continue;
//fprintf(stderr, "%f\n", val);

//luca
//				mean.x += ix * val;
//				mean.y += iy * val;
//				mean.z += iz * val;
mean->x += ix * val;
mean->y += iy * val;
mean->z += iz * val;
				norm += val;
			}
		}
	}
//luca
//	mean.x /= norm;
//	mean.y /= norm;
//	mean.z /= norm;
mean->x /= norm;
mean->y /= norm;
mean->z /= norm;

//luca
//	fmean.x = Grid->origx + mean.x*Grid->dx;
//	fmean.y = Grid->origy + mean.y*Grid->dy;
//	fmean.z = Grid->origz + mean.z*Grid->dz;
fmean.x = Grid->origx + mean->x*Grid->dx;
fmean.y = Grid->origy + mean->y*Grid->dy;
fmean.z = Grid->origz + mean->z*Grid->dz;

	fhypo.x = ml_hypo->x;
	fhypo.y = ml_hypo->y;
	fhypo.z = ml_hypo->z;

//luca
#if 0


/* FIRST LOOP ON STATIONS*/
	ml_dist = (float *) calloc(nsta, sizeof(float));
	mean_dist = (float *) calloc(nsta, sizeof(float));
	d_log_dist = (float *) calloc(nsta, sizeof(float));

	for (i=0; i<nsta; i++) {

		sta.x = station[i].desc.x;
		sta.y = station[i].desc.y;
		sta.z = station[i].desc.z;

		ml_dist[i] = sqrt (
			pow(sta.x - fhypo.x, 2) +
			pow(sta.y - fhypo.y, 2) +
			pow(sta.z - fhypo.z, 2) 
		);
		mean_dist[i] = sqrt (
			pow(sta.x - fmean.x, 2) +
			pow(sta.y - fmean.y, 2) +
			pow(sta.z - fmean.z, 2) 
		);
		d_log_dist[i] = 0;
	}


#endif

/* COVARIANCE */
/* calculate covariance following eq. (6-12), T & V, 1982 */
	numx = Grid->numx;
	numy = Grid->numy;
	numz = Grid->numz;

        cov->xx=cov->yy=cov->zz=0;
	norm=0;
	for (ix=0; ix<numx; ix++) {
		x = Grid->origx + (double) ix * Grid->dx;
		xx = x * x;
		for (iy=0; iy<numy; iy++) {
			y = Grid->origy + (double) iy * Grid->dy;
			yy = y * y;
			xy = x * y;
			for (iz=0; iz<numz; iz++) {
				z = Grid->origz + (double) iz * Grid->dz;
				zz = z * z;
				xz = x * z;
				yz = y * z;

				val = Grid->array[ix][iy][iz];
				//if( val/f_prob_max < params->pdfcut ) val=0;
				if( val/f_prob_max < params->pdfcut ) continue;

				cov->xx += (double) val * xx;
				cov->xy += (double) val * xy;
				cov->xz += (double) val * xz;

				cov->yy += (double) val * yy;
				cov->yz += (double) val * yz;

				cov->zz += (double) val * zz;

				norm += val;


				/* LOOP FOR ERROR ON STATION DISTANCE */
				//for (i=0; i<nsta; i++) {
				//	sta.x = station[i].desc.x;
				//	sta.y = station[i].desc.y;
				//	sta.z = station[i].desc.z;
				//	
				//	dist = sqrt (
				//		pow(sta.x - x, 2) +
				//		pow(sta.y - y, 2) +
				//		pow(sta.z - z, 2) 
				//	);
				//	d_log_dist[i] += val * pow( log(dist) - log(mean_dist[i]), 2 );
				//}
				/****/

			}
		}
	}

	cov->xx = cov->xx / norm - fmean.x * fmean.x;
	cov->xy = cov->xy / norm - fmean.x * fmean.y;
	cov->xz = cov->xz / norm - fmean.x * fmean.z;

	cov->yx = cov->xy;
	cov->yy = cov->yy / norm - fmean.y * fmean.y;
	cov->yz = cov->yz / norm - fmean.y * fmean.z;

	cov->zx = cov->xz;
	cov->zy = cov->yz;
	cov->zz = cov->zz / norm - fmean.z * fmean.z;

//luca
if (cov->xx < 0 || !_finite(cov->xx))
	cov->xx = 0;
if (cov->yy < 0 || !_finite(cov->yy))
	cov->yy = 0;
if (cov->zz < 0 || !_finite(cov->zz))
	cov->zz = 0;


/* Ellipsoid */
//luca
//	SetConstants();
//	ell=CalcErrorEllipsoid(&cov, 3.53);
if (f_prob_max && norm)
*ell = CalcErrorEllipsoid(cov, 3.53);
else
*ell = EllipsoidNULL;
	/* 3.53: value for 68% conf
	   (see Num Rec, 2nd ed, sec 15.6) */

/* PRINT HYPOCENTER STATISTICS */
//luca
//	printstat("RTLOC\n");
	printstat("TIME %4.2f\n", tnow);
	printstat("HYPOCENTER x %6.3f y %6.3f z %6.3f OT %6.3f\n", fhypo.x, fhypo.y, fhypo.z, origtime);
	printstat("QUALITY RMS %6.3f Nphs %d\n", rms, npick);
//luca
//	printstat("STATISTICS  ExpectX %6.3f Y %6.3f Z %6.3f CovXX %6.3f XY %6.3f XZ %6.3f YY %6.3f YZ %6.3f ZZ %6.3f", fmean.x, fmean.y, fmean.z, cov.xx, cov.xy, cov.xz, cov.yy, cov.yz, cov.zz);
//	printstat(" EllAz1  %.1lf Dip1  %.1lf Len1  %.2le", ell.az1, ell.dip1, ell.len1);
//	printstat(" Az2  %.1lf Dip2  %.1lf Len2  %.2le", ell.az2, ell.dip2, ell.len2);
//	printstat(" Len3  %.2le\n", ell.len3);
printstat("STATISTICS  ExpectX %6.3f Y %6.3f Z %6.3f CovXX %6.3f XY %6.3f XZ %6.3f YY %6.3f YZ %6.3f ZZ %6.3f\n", fmean.x, fmean.y, fmean.z, cov->xx, cov->xy, cov->xz, cov->yy, cov->yz, cov->zz);
printstat("ELLIPSE EllAz1  %.1lf Dip1  %.1lf Len1  %.2le  Az2  %.1lf Dip2  %.1lf Len2  %.2le Len3  %.2le\n", ell->az1, ell->dip1, ell->len1, ell->az2, ell->dip2, ell->len2, ell->len3);


//luca
#if 0	

/* PRINT STATION STATISTICS */
	for (i=0; i<nsta; i++) {
		//d_log_dist[i] /= norm;
		//d_log_dist[i] = sqrt(d_log_dist[i]) / log(10);


		ttP[i] = ReadAbsInterpGrid3d(NULL, &(Pgrid[i]), ml_hypo->x, ml_hypo->y, ml_hypo->z);
		ttS[i] = ReadAbsInterpGrid3d(NULL, &(Sgrid[i]), ml_hypo->x, ml_hypo->y, ml_hypo->z);

		tP = ttP[i] + origtime;
		tS = ttS[i] + origtime;


		sta.x = station[i].desc.x;
		sta.y = station[i].desc.y;
		sta.z = station[i].desc.z;

		//Propagazione dell'errore
		float d_dist = 1/mean_dist[i] * ( 
					abs(sta.x - fmean.x) * sqrt(cov.xx) +
					abs(sta.y - fmean.y) * sqrt(cov.yy) +
					abs(sta.z - fmean.z) * sqrt(cov.zz)
				);
		d_log_dist [i] = 1/mean_dist[i] * d_dist / log(10);

		//if ( station[i].evid[evid] == 1) 
		tPobs = -12345;
		if (station[i].pickid >= 0) {
			tPobs = pick[station[i].pickid].time;
		}
		printstat("STA  %s  Dist %6.3f D_log10_Dist %6.3f Ptime %6.3f  Stime %6.3f Pobs %6.3f\n", station[i].name, ml_dist[i], d_log_dist[i], tP, tS, tPobs);
	}

	
	printstat("END_RTLOC\n\n");
#endif
	free(ttP);
	free(ttS);
#if 0
	free(ml_dist);
	free(mean_dist);
	free(d_log_dist);
#endif
}
