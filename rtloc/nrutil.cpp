
#include <stdlib.h>
#include <stdio.h>
//luca
//#include "./nrutil.h"
#include "nrutil.h"

/*  (C) Copr. 1986-92 Numerical Recipes Software */

//luca
void nrerror(//error_text)
//char error_text[];
char error_text[])
{
//luca
//	void exit();

	fprintf(stderr,"Numerical Recipes run-time error...\n");
	fprintf(stderr,"%s\n",error_text);
	fprintf(stderr,"...now exiting to system...\n");
	exit(1);
}



/** function to print error and return (AJL Jan1998) */
//luca
int nrerror_return(//error_text)
//char error_text[];
char error_text[])
{
//luca
//	void exit();

	fprintf(stderr,"Numerical Recipes run-time error: %s\n", error_text);
	return(-1);
}



//luca
float *fvector(//nl,nh)
//int nl,nh;
int nl, int nh)
{
	float *v;

	v=(float *)malloc((unsigned) (nh-nl+1)*sizeof(float));
	if (!v) nrerror("allocation failure in vector()");
	return v-nl;
}

//luca
int *ivector(//nl,nh)
//int nl,nh;
int nl, int nh)
{
	int *v;

	v=(int *)malloc((unsigned) (nh-nl+1)*sizeof(int));
	if (!v) nrerror("allocation failure in ivector()");
	return v-nl;
}

//luca
double *dvector(//nl,nh)
//int nl,nh;
int nl, int nh)
{
	double *v;

	v=(double *)malloc((unsigned) (nh-nl+1)*sizeof(double));
	if (!v) nrerror("allocation failure in dvector()");
	return v-nl;
}



//luca
float **matrix(//nrl,nrh,ncl,nch)
//int nrl,nrh,ncl,nch;
int nrl, int nrh, int ncl, int nch)
{
	int i;
	float **m;

	m=(float **) malloc((unsigned) (nrh-nrl+1)*sizeof(float*));
	if (!m) nrerror("allocation failure 1 in matrix()");
	m -= nrl;

	for(i=nrl;i<=nrh;i++) {
		m[i]=(float *) malloc((unsigned) (nch-ncl+1)*sizeof(float));
		if (!m[i]) nrerror("allocation failure 2 in matrix()");
		m[i] -= ncl;
	}
	return m;
}

//luca
double **dmatrix(//nrl,nrh,ncl,nch)
//int nrl,nrh,ncl,nch;
int nrl, int nrh, int ncl, int nch)
{
	int i;
	double **m;

	m=(double **) malloc((unsigned) (nrh-nrl+1)*sizeof(double*));
	if (!m) nrerror("allocation failure 1 in dmatrix()");
	m -= nrl;

	for(i=nrl;i<=nrh;i++) {
		m[i]=(double *) malloc((unsigned) (nch-ncl+1)*sizeof(double));
		if (!m[i]) nrerror("allocation failure 2 in dmatrix()");
		m[i] -= ncl;
	}
	return m;
}

//luca
int **imatrix(//nrl,nrh,ncl,nch)
//int nrl,nrh,ncl,nch;
int nrl, int nrh, int ncl, int nch)
{
	int i,**m;

	m=(int **)malloc((unsigned) (nrh-nrl+1)*sizeof(int*));
	if (!m) nrerror("allocation failure 1 in imatrix()");
	m -= nrl;

	for(i=nrl;i<=nrh;i++) {
		m[i]=(int *)malloc((unsigned) (nch-ncl+1)*sizeof(int));
		if (!m[i]) nrerror("allocation failure 2 in imatrix()");
		m[i] -= ncl;
	}
	return m;
}



//luca
float **submatrix(//a,oldrl,oldrh,oldcl,oldch,newrl,newcl)
//float **a;
//int oldrl,oldrh,oldcl,oldch,newrl,newcl;
float **a, int oldrl, int oldrh, int oldcl, int oldch, int newrl, int newcl)
{
	int i,j;
	float **m;

	m=(float **) malloc((unsigned) (oldrh-oldrl+1)*sizeof(float*));
	if (!m) nrerror("allocation failure in submatrix()");
	m -= newrl;

	for(i=oldrl,j=newrl;i<=oldrh;i++,j++) m[j]=a[i]+oldcl-newcl;

	return m;
}



//luca
void free_vector(//v,nl,nh)
//float *v;
//int nl,nh;
float *v, int nl, int nh)
{
	free((char*) (v+nl));
}

//luca
void free_ivector(//v,nl,nh)
//int *v,nl,nh;
int *v, int nl, int nh)
{
	free((char*) (v+nl));
}

//luca
void free_dvector(//v,nl,nh)
//double *v;
//int nl,nh;
double *v, int nl, int nh)
{
	free((char*) (v+nl));
}



//luca
void free_matrix(//m,nrl,nrh,ncl,nch)
//float **m;
//int nrl,nrh,ncl,nch;
float **m, int nrl, int nrh, int ncl, int nch)
{
	int i;

	for(i=nrh;i>=nrl;i--) free((char*) (m[i]+ncl));
	free((char*) (m+nrl));
}

//luca
void free_dmatrix(//m,nrl,nrh,ncl,nch)
//double **m;
//int nrl,nrh,ncl,nch;
double **m, int nrl, int nrh, int ncl, int nch)
{
	int i;

	for(i=nrh;i>=nrl;i--) free((char*) (m[i]+ncl));
	free((char*) (m+nrl));
}

//luca
void free_imatrix(//m,nrl,nrh,ncl,nch)
//int **m;
//int nrl,nrh,ncl,nch;
int **m, int nrl, int nrh, int ncl, int nch)
{
	int i;

	for(i=nrh;i>=nrl;i--) free((char*) (m[i]+ncl));
	free((char*) (m+nrl));
}



//luca
void free_submatrix(//b,nrl,nrh,ncl,nch)
//float **b;
//int nrl,nrh,ncl,nch;
float **b, int nrl, int nrh, int ncl, int nch)
{
	free((char*) (b+nrl));
}



//luca
float **convert_matrix(//a,nrl,nrh,ncl,nch)
//float *a;
//int nrl,nrh,ncl,nch;
float *a, int nrl, int nrh, int ncl, int nch)
{
	int i,j,nrow,ncol;
	float **m;

	nrow=nrh-nrl+1;
	ncol=nch-ncl+1;
	m = (float **) malloc((unsigned) (nrow)*sizeof(float*));
	if (!m) nrerror("allocation failure in convert_matrix()");
	m -= nrl;
	for(i=0,j=nrl;i<=nrow-1;i++,j++) m[j]=a+ncol*i-ncl;
	return m;
}



//luca
void free_convert_matrix(//b,nrl,nrh,ncl,nch)
//float **b;
//int nrl,nrh,ncl,nch;
float **b, int nrl, int nrh, int ncl, int nch)
{
	free((char*) (b+nrl));
}
