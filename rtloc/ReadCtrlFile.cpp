/*
 * @ReadCtrlFile.c parse the control file
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

//TODO: check if ctrfile is valid!

//luca
#include "../global.h"
#include "../config.h"
#include "rtloclib.h"
#define PARAMSIZE 30

extern char *statfilename;
extern char *logfilename;

//luca
//FILE *ReadCtrlFile (char *ctrlfilename, struct Control *ctrl)
FILE *ReadCtrlFile (const char *ctrlfilename, struct Control *ctrl)
{
	FILE *ctrlfile;
	int nsta;
//luca
//	int npick;

	char line[LINEBUFSIZE];
	char param[PARAMSIZE];

//luca
//	char runname[80];


	/* Default values */
//luca
//	sprintf (ctrl->outfilename, "out");
	ctrl->sigma = 1;
//luca
//	ctrl->probthreshold = 0.01;
//	ctrl->tnow = 0;
//	ctrl->dt = 1;
//	ctrl->endtime = 1;
	ctrl->sum = 0;
	ctrl->pow = 0;
	ctrl->renorm = 0;
//luca
//	ctrl->maxrms = 1;
	ctrl->search_type = SEARCH_GRID;
	ctrl->pdfcut = -9999;

	if ((ctrlfile = fopen (ctrlfilename, "r")) == NULL) {
		perror (ctrlfilename);
//		exit (0);
		Fatal_Error("RTLoc: can't open file " + string(ctrlfilename));
	}

	nsta=0;
//luca
/*
	npick=0;

	while ( fgets (line, LINEBUFSIZE, ctrlfile) != NULL ) {
 		if ( sscanf(line, "%s", param) < 0 ) continue;
		if ( !strncmp (param, "#", 1) || isspace(param[0]) ) continue;
		if ( !strcmp (param, "STA") ) nsta++;
		if ( !strcmp (param, "PICK") ) npick++;
	}

	ctrl->nsta=nsta;
	ctrl->npick=npick;
	rewind (ctrlfile);
*/
	while ( fgets (line, LINEBUFSIZE, ctrlfile) != NULL ) {
 		if ( sscanf(line, "%s", param) < 0 ) continue;
		if ( !strncmp (param, "#", 1) || isspace(param[0]) ) continue;
//luca
//		if ( !strcmp (param, "OUTFILE") )
//			sscanf (line, "%*s %s", ctrl->outfilename);
		if ( !strcmp (param, "SIGMA") )
			sscanf (line, "%*s %f", &ctrl->sigma);
//luca
//		if ( !strcmp (param, "TNOW") )
//			sscanf (line, "%*s %f", &ctrl->tnow);
//		if ( !strcmp (param, "DELTA") )
//			sscanf (line, "%*s %f", &ctrl->dt);
//		if ( !strcmp (param, "ENDTIME") )
//			sscanf (line, "%*s %f", &ctrl->endtime);
//		if ( !strcmp (param, "PROBTHRESHOLD") )
//			sscanf (line, "%*s %f", &ctrl->probthreshold);
		else if ( !strcmp (param, "SUM") )
			sscanf (line, "%*s %d", &ctrl->sum);
		else if ( !strcmp (param, "POW") )
			sscanf (line, "%*s %d", &ctrl->pow);
		else if ( !strcmp (param, "RENORM") )
			sscanf (line, "%*s %d", &ctrl->renorm);
//luca
//		if ( !strcmp (param, "MAXRMS") )
//			sscanf (line, "%*s %f", &ctrl->maxrms);
//		if ( !strcmp (param, "RUN") )
//			sscanf (line, "%*s %s", runname);
		// AJL 20070110
		else if ( !strcmp (param, "SEARCH") )
			GetSearchType(line, ctrl);
			//sscanf (line, "%*s %f", &ctrl->maxrms);
		// -AJL
		else if ( !strcmp (param, "PDFCUT") )
			sscanf (line, "%*s %f", &ctrl->pdfcut);
//luca
else if ( !strcmp (param, "STA") )
	nsta++;
else
	Fatal_Error("RTLoc: unknown parameter " + string(param) + " in " + string(ctrlfilename));
	}
//luca
/*
	statfilename = (char *) calloc(100, sizeof(char));
	sprintf (statfilename, "%s.rtloc.stat", runname);

	logfilename = (char *) calloc(100, sizeof(char));
	sprintf (logfilename, "%s.rtloc.log", runname);
*/
ctrl->nsta=nsta;

	rewind (ctrlfile);
	return ctrlfile;
}


struct Station *ReadStation (FILE *ctrlfile, struct Station *station, int nsta)
{
	int n;

	char line[LINEBUFSIZE];
	char param[PARAMSIZE];


	if ( (station = (struct Station *) calloc ((size_t) nsta, (size_t) sizeof (struct Station))) == NULL ) {
		fprintf (stderr, "Error allocating memory for stations\n");
//		exit (0);
		Fatal_Error("RTLoc: can't allocate memory for stations");
	}


	n=0;
	while ( fgets (line, LINEBUFSIZE, ctrlfile) != NULL ) {
 		if ( sscanf(line, "%s", param) < 0 ) continue;
		if ( !strncmp (param, "#", 1) || isspace(param[0]) ) continue;
		if ( !strcmp (param, "STA") ) {
//			sscanf (line, "%*s %s %s %s", station[n].name, station[n].Pfile, station[n].Sfile);
sscanf (line, "%*s %s", station[n].name);
strncpy(station[n].Pfile, string(net_dir + "time/layer.P." + string(station[n].name) + ".time").c_str(), sizeof(station[n].Pfile)-1);
strncpy(station[n].Sfile, string(net_dir + "time/layer.S." + string(station[n].name) + ".time").c_str(), sizeof(station[n].Sfile)-1);

			station[n].statid = n;
			//We allocate space for 10 evid
			//TODO: check realloc()
			station[n].evid = (int *) calloc (10, sizeof (int));
			station[n].pickid = -1;
			n++;
		}
	}

	rewind (ctrlfile);
	return station;
}


struct Pick *ReadPick (FILE *ctrlfile, struct Pick *pick, int npick, struct Station *station, int nsta)
{
	int n;
	int statid;

	char line[LINEBUFSIZE];
	char param[PARAMSIZE];

	char stname[10];
	float buf;


	if ( (pick = (struct Pick *) calloc ((size_t) npick, (size_t) sizeof (struct Pick))) == NULL ) {
		fprintf (stderr, "Error allocating memory for picks\n");
		exit (0);
	}

	n=0;
	while ( fgets (line, LINEBUFSIZE, ctrlfile) != NULL ) {
 		if ( sscanf(line, "%s", param) < 0 ) continue;
		if ( !strncmp (param, "#", 1) || isspace(param[0]) ) continue;
		if ( !strcmp (param, "PICK") ) {
			sscanf (line, "%*s %s %f", stname, &buf);
			pick[n].time = buf;
			pick[n].pickid = n;
			statid = stat_lookup (station, nsta, stname);
			/* TODO: controllare se statid != -1*/
			/* vuol dire che non c'è il file dei tempi per la stazione*/
			pick[n].statid = statid;
			station[statid].pickid = n; //associamo l'id del pick alla stazione
			pick[n].evid = -1; //Pick starts unassociated
			if ( n == npick-1 )
				pick[n].next = NULL;
			else
				pick[n].next = &pick[n+1];
			n++;
		}
	}

	rewind (ctrlfile);
	return pick;
}



// AJL 20070110

/** function to read search type ***/

int GetSearchType(char* line1, struct Control *ctrl)
{
	int istat;
	//int ierr;

	char search_type[LINEBUFSIZE];
	OcttreeParams *octtreeParams;



	istat = sscanf(line1, "%*s %s", search_type);

	if (istat != 1)
		return(-1);

	if (strcmp(search_type, "GRID") == 0) {

		ctrl->search_type = SEARCH_GRID;

	} else if (strcmp(search_type, "OCT") == 0) {

		ctrl->search_type = SEARCH_OCTTREE;

		octtreeParams = &ctrl->octtreeParams;

		istat = sscanf(line1, "%*s %*s %d %d %d %lf %d %d",
			       &(octtreeParams->init_num_cells_x),
			       &(octtreeParams->init_num_cells_y), &(octtreeParams->init_num_cells_z),
			       &(octtreeParams->min_node_size), &(octtreeParams->max_num_nodes),
			       &(octtreeParams->stop_on_min_node_size));

		/*
		sprintf(MsgStr,
			"LOCSEARCH:  Type: %s  init_num_cells_x %d  init_num_cells_y %d  init_num_cells_z %d  min_node_size %f  max_num_nodes %d  stop_on_min_node_size %d\n",
			search_type, octtreeParams->init_num_cells_x, octtreeParams->init_num_cells_y,
			octtreeParams->init_num_cells_z,
			octtreeParams->min_node_size, octtreeParams->max_num_nodes,
			octtreeParams->stop_on_min_node_size);
		printlog(MsgStr);
		//putmsg(3, MsgStr);
		*/

		/*
		ierr = 0;
		if (checkRangeInt("LOCSEARCH", "init_num_cells_x",
		    octtreeParams->init_num_cells_x, 1, 0, 0, 0) != 0)
			ierr = -1;
		if (checkRangeInt("LOCSEARCH", "init_num_cells_y",
		    octtreeParams->init_num_cells_y, 1, 0, 0, 0) != 0)
			ierr = -1;
		if (checkRangeInt("LOCSEARCH", "init_num_cells_z",
		    octtreeParams->init_num_cells_z, 1, 0, 0, 0) != 0)
			ierr = -1;
		if (checkRangeDouble("LOCSEARCH", "min_node_size",
		    octtreeParams->min_node_size, 1, 0.0, 0, 0.0) != 0)
			ierr = -1;
		if (checkRangeInt("LOCSEARCH", "max_num_nodes",
		    octtreeParams->max_num_nodes, 1, 0, 0, 0) != 0)
			ierr = -1;
		if (checkRangeInt("LOCSEARCH", "num_scatter",
		    octtreeParams->num_scatter, 1, 0, 0, 0) != 0)
			ierr = -1;
		if (ierr < 0)
			return(-1);
		*/

		if (istat < 6)
			return(-1);

	}


	return(0);
}


// -AJL

