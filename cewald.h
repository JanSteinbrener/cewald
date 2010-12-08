#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../util/dm.h"
#include "../util/dm_fileio.h"
#include "../util/dm_array.h"

#define CEWALD_MAX_FILES 500
#define CEWALD_MAX_STRLEN 200
#define CEWALD_HEADER_LINES 3

typedef struct {
  int nNFiles;
  char *paszFilenames; 
  char *paszSystimes;
  double *padThetaXRadians;
  double *padThetaYRadians;
  double *padThetaZRadians;
  int *panXCenterOffset;
  int *panYCenterOffset;
  int *panPhotonScaling;
  float fLambdaMeters;
  float fCCDZMeters;
  float fCCDPixelSizeMeters;
  int nNX; /* This value only for initial array size! */
  int nNY; 
  dm_array_index_t nIndexOffset;
} Parameters;


/*-------------------------------------------------------------------------*/
int cewald_scan_directory(char *szDirname, int *pnNFiles);

/*-------------------------------------------------------------------------*/
int cewald_check_script(char *szScript, int *pnNFiles, char *szErrorString);


/*-------------------------------------------------------------------------*/
int cewald_load_filenames(char *szDirname,
			  Parameters *psDataParams);

/*-------------------------------------------------------------------------*/
int cewald_initialize_dataset(char *szScript,
			      Parameters *psDataParams,
			      int nMyRank);

/*-------------------------------------------------------------------------*/
int cewald_sort_by_first_angle(Parameters *psDataParams);

/*-------------------------------------------------------------------------*/
void cewald_erase_strarr(char *paszStrarr, int nNLines);

/* This will rotate a given vector by the Euler angles Phi,Theta,Psi
 * in the x-convention. The input vector is double precision. The output
 * vector is the round result of the input vector after rotation.
 */
void cewald_rotate_vector(double *padOld3DVector, 
			  int *panNew3DVector, 
			  double dPhi,
			  double dTheta,
			  double dPsi);

/*-------------------------------------------------------------------------*/
void cewald_initialize_data(Parameters *psDataParams,
			    dm_array_real_struct *psDataCube,
			    dm_array_int_struct *psCounterCube,
			    dm_array_real_struct *psThisADI,
			    int nP);

/*-------------------------------------------------------------------------*/
int cewald_load_adi(int nIndex, 
		    Parameters *psDataParams,
		    dm_array_real_struct *psThisADI,
		    int nMyRank,
		    int nP,
		    char *szErrorString);

/*-------------------------------------------------------------------------*/
void cewald_ewald_n_rotate(dm_array_real_struct *psThisAdi,
			   Parameters *psDataParams,
			   int nIndex,
			   dm_array_real_struct *psDataCube,
			   dm_array_int_struct *psCounterCube);

/*-------------------------------------------------------------------------*/
void cewald_average(dm_array_real_struct *psDataCube,
		    dm_array_int_struct *psCounterCube);

/*-------------------------------------------------------------------------*/
void cewald_write_h5(Parameters *psDataParams,
		     dm_array_real_struct *psDataCube,
		     char *szOutput,
		     char *szErrorString,
		     int nMyRank,
		     int nP);

