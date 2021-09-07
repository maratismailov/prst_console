/*
 * @file rtloclib.h definitions and prototypes
 * 
 * Copyright (C) 2006-2007 Claudio Satriano <satriano@na.infn.it> and
 * Anthony Lomax <anthony@alomax.net>
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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

#include "GridLib.h"

#define LINEBUFSIZE 120


// AJL 20070110
#define SEARCH_GRID  	0
#define SEARCH_OCTTREE  1
typedef struct
{
	int init_num_cells_x, init_num_cells_y, init_num_cells_z;
		// num nodes for each side of initial Tree3D
	double min_node_size;	// size of smallest side of smallest allowed node
	int max_num_nodes;	// maximum number of nodes to evaluate
	int stop_on_min_node_size;	// if 1, stop search when first min_node_size reached, if 0 stop subdividing a given cell when min_node_size reached
}
OcttreeParams;
// -AJL


struct Pick {
	int pickid;
	float time;
	int statid;
	int evid;
	struct Pick *next; //lista a puntatori?
};

struct Station {
	int statid;
//luca
//	char name[10];
	char name[80];
	char Pfile[80];
	char Sfile[80];
	int *evid; //event array (1=associated to the corresponding event, 0=not associated)
	int pickid;
	SourceDesc desc; 
};

struct Control {
//luca
//	char outfilename[80];
	int nsta;
	int npick;
//luca
//	float tnow;
//	float dt;
//	float endtime;
	float sigma;
//luca
//	float probthreshold;
	int sum;
	int pow;
	int renorm;
//luca
//	float maxrms;
	// AJL 20070110
	int search_type;
	OcttreeParams octtreeParams;		/* Octtree parameters */
	// -AJL
	float pdfcut;
};


//luca
//FILE *ReadCtrlFile (char *ctrlfilename, struct Control *ctrl);
FILE *ReadCtrlFile (const char *ctrlfilename, struct Control *ctrl);
struct Station *ReadStation (FILE *ctrlfile, struct Station *station, int nsta);
struct Pick *ReadPick (FILE *ctrlfile, struct Pick *pick, int npick, struct Station *station, int nsta);

int GetSearchType(char* line1, struct Control *ctrl);

int stat_lookup (struct Station *sta, int nsta, char *stname);

INLINE double do_edt (double tta, double ttb, double ta, double tb);
//luca
//INLINE double normalize (double edt, int nsta);
INLINE double normalize (double edt, double nsta);

//luca
//float SearchEdt (GridDesc *Grid, struct Pick *pick, struct Station *station, GridDesc *Pgrid, GridDesc *Sgrid, int evid, struct Control *params, int writeToDisk, int *imean);
float SearchEdt (GridDesc *Grid, struct Pick *pick, struct Station *station, int nsta_working, GridDesc *Pgrid, GridDesc *Sgrid, int evid, struct Control *params, int writeToDisk,	Vect3D *mean, Vect3D *ml_hypo, Mtrx3D *cov, Ellipsoid3D *ell, float *ml_otime);

INLINE double calcEDTProb (struct Pick *pick, struct Station *station, 
			   GridDesc *Pgrid, GridDesc *Sgrid, int evid,
			   double xloc, double yloc, double zloc, int interpolate,
			   int *ntriggered, int *nevaluated, double *prob_max, double *total_weight, Vect3D *ml_hypo, struct Control *params, double *Pmax);

double GridSearch (GridDesc *Grid, struct Pick *pick, struct Station *station,
//luca
int nsta_working, 
		  GridDesc *Pgrid, GridDesc *Sgrid, int evid,
		  struct Control *params, int *nevaluated,
		  double *prob_max, Vect3D *ml_hypo);

//double OctTreeSearch (GridDesc *Grid, struct Pick *pick, struct Station *station,
//		     GridDesc *Pgrid, GridDesc *Sgrid,
//		     int evid, struct Control *params, int *imean, int *nevaluated,
//		     double *norm, double mean[3], double *prob_max, double *ml_hypo);

//float Read4dBuf (float *buf, int ix, int iy, int numy, int iz, int numz, int is, int nums);

void initLocGrid (GridDesc *prototype, GridDesc *locgrid);

void printlog (const char *format, ...);
void printstat (const char *format, ...);


//luca
//float GetRms (int *imean, GridDesc *Pgrid, GridDesc *Sgrid, struct Pick *pick, int pickid, GridDesc *Grid, struct Control *params);
float GetRms (Vect3D *mean, GridDesc *Pgrid, GridDesc *Sgrid, struct Pick *pick, int pickid, GridDesc *Grid, struct Control *params);

//luca
//void LocStat (GridDesc *Grid, double f_prob_max, GridDesc *Pgrid, GridDesc *Sgrid, Vect3D *ml_hypo, struct Station *station, int evid, struct Pick *pick, struct Control *params);
void LocStat (GridDesc *Grid, double f_prob_max, GridDesc *Pgrid, GridDesc *Sgrid, Vect3D *mean, Vect3D *ml_hypo, struct Station *station, int evid, struct Pick *pick, struct Control *params,		Mtrx3D *cov, Ellipsoid3D *ell, float *ml_otime);


/* OctTreeSearch.c */
//luca
//double LocOctree(GridDesc *Grid, struct Pick *pick, int npick, struct Station *station, int nsta, GridDesc *Pgrid, GridDesc *Sgrid, HypoDesc *phypo, OcttreeParams *octtreeParams, Tree3D *pOctTree, int evid, int *nevaluated, double *prob_max, double *f_prob_max, double *poct_node_value_max, double *poct_tree_integral, Vect3D *ml_hypo, struct Control *params);
double LocOctree(GridDesc *Grid, struct Pick *pick, int npick, struct Station *station, int nsta, int nsta_working, GridDesc *Pgrid, GridDesc *Sgrid, HypoDesc *phypo, OcttreeParams *octtreeParams, Tree3D *pOctTree, int evid, int *nevaluated, double *prob_max, double *f_prob_max, double *poct_node_value_max, double *poct_tree_integral, Vect3D *ml_hypo, struct Control *params);

//luca
//double OctTreeSearch(GridDesc *Grid, struct Pick *pick, struct Station *station, GridDesc *Pgrid, GridDesc *Sgrid, int evid, struct Control *params, int *nevaluated, double *prob_max, Vect3D *ml_hypo);
double OctTreeSearch(GridDesc *Grid, struct Pick *pick, struct Station *station, int nsta_working, GridDesc *Pgrid, GridDesc *Sgrid, int evid, struct Control *params, int *nevaluated, double *prob_max, Vect3D *ml_hypo);
Tree3D *InitializeOcttree(GridDesc *ptgrid, OcttreeParams *octtreeParams);

