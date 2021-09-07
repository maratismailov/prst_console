//luca
//int gaussj();
//int dgaussj();
//void 	DisplayMatrix();
//void 	DisplayDMatrix();
//int svdcmp0();
//int svdcmp();

int  gaussj(float **a, int n, float **b, int m);
int  dgaussj(double **a, int n, double **b, int m);
void DisplayMatrix(char* name, float** matrix, int num_rows, int num_cols);
void DisplayDMatrix(char* name, double** matrix, int num_rows, int num_cols);
int  svdcmp0(float **a, int m, int n, float *w, float **v);
int  svdcmp(float **a, int m, int n, float *w, float **v);
