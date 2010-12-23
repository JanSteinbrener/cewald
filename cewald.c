#include "cewald.h"
#include <sys/stat.h>
#include <errno.h>


int main(int argc, char *argv[]) {
  char szDirname[CEWALD_MAX_STRLEN], szOutput[CEWALD_MAX_STRLEN];
  char szErrorString[CEWALD_MAX_STRLEN],szScript[CEWALD_MAX_STRLEN];
  char *pchar;
  Parameters sDataParams;
  int nIArg, nStatus,nIndex;
  int nMyRank,nP;
  dm_array_real_struct sDataCube;
  dm_array_real_struct sThisAdi;
  dm_array_int_struct sCounterCube;

  dm_init(&nP,&nMyRank);

  /* hardwired defaults */
  nIArg = 1;

  while (nIArg < argc) {
    strcpy(szScript,argv[nIArg]);
    nIArg++;
  }
  
  /* Make sure file exists */
  if (access(szScript, F_OK) == -1) {
    printf("[Error] cewald: %s does not exist. \n",szScript);
    exit(1);
  }

  /* Output filename */
  //strcpy(szDirname,"");
  pchar = strrchr(szScript,'/');
  strncpy(szDirname,szScript,pchar-szScript+1);
  szDirname[pchar-szScript+1] = '\000';
  strcpy(szOutput,szDirname);
  strcpy(szOutput+(strlen(szDirname)),"cewald_output.h5");

  if ((nStatus = cewald_check_script(szScript, &(sDataParams.nNFiles),
				     szErrorString)) != 0) {
    printf("%s\n",szErrorString);
    exit(1);
  }
  
  /* Allocate memory */
  sDataParams.paszFilenames = 
    (char *)malloc(sizeof(char)*CEWALD_MAX_STRLEN*sDataParams.nNFiles);
  sDataParams.padThetaXRadians = 
    (double *)malloc(sizeof(double)*sDataParams.nNFiles);
  sDataParams.padThetaYRadians = 
    (double *)malloc(sizeof(double)*sDataParams.nNFiles);
  sDataParams.padThetaZRadians = 
    (double *)malloc(sizeof(double)*sDataParams.nNFiles);
  sDataParams.panXCenterOffset = 
    (int *)malloc(sizeof(int)*sDataParams.nNFiles);
  sDataParams.panYCenterOffset = 
    (int *)malloc(sizeof(int)*sDataParams.nNFiles);
  sDataParams.panPhotonScaling = 
    (int *)malloc(sizeof(int)*sDataParams.nNFiles);
  

  /* load the filenames into the created array 
     nStatus = cewald_load_filenames(szDirname,&sDataParams); */

  /* load angles and filenames */
  nStatus = cewald_initialize_dataset(szScript,&sDataParams,nMyRank);

  /* sort angles 
     nStatus = cewald_sort_by_first_angle(&sDataParams); */

  /* determine size and allocate memory */
  cewald_initialize_data(&sDataParams,&sDataCube,&sCounterCube,
			 &sThisAdi,nP);
  
  /* Main loop */
  for (nIndex=0;nIndex<(sDataParams.nNFiles);nIndex++) {

    if (cewald_load_adi(nIndex,&sDataParams,&sThisAdi,nMyRank,nP,
			szErrorString) < 0) {
      printf("%s\n",szErrorString);
      exit(1);
    }

    cewald_ewald_n_rotate(&sThisAdi,&sDataParams,nIndex,
			  &sDataCube,&sCounterCube);
  }

  cewald_average(&sDataCube,&sCounterCube);
 
  cewald_write_h5(&sDataParams,&sDataCube,szOutput,szErrorString,nMyRank,nP);

  /* free allocated memory */
  free(sDataParams.paszFilenames);
  free(sDataParams.padThetaXRadians);
  free(sDataParams.padThetaYRadians);
  free(sDataParams.padThetaZRadians);
  free(sDataParams.panXCenterOffset);
  free(sDataParams.panYCenterOffset);
  free(sDataParams.panPhotonScaling);
  free(sDataCube.real_array);
  free(sCounterCube.int_array);
  free(sThisAdi.real_array);

  dm_exit();
}

/*-------------------------------------------------------------------------*/
int cewald_check_script(char *szScript,
			int *pnNFiles,
			char *szErrorString)
{
  
  FILE *pfFile;
  int nNFiles=0;
  int nNHeaderLines=CEWALD_HEADER_LINES;
  int count;
  size_t tThisStrlen;
  char pszThisLine[CEWALD_MAX_STRLEN];
  char *psSubString;
  struct stat st;

  if ((pfFile = fopen(szScript,"r")) == NULL) {
    sprintf(szErrorString,"[ERROR] cewald_check_script: Cannot open %s.",
	    szScript);
    return(1);
  }
  
  /* Get current line and check access */
  while (fgets(pszThisLine,CEWALD_MAX_STRLEN, pfFile) ) {
    /* Extract filename, check access */ 
    psSubString = strrchr(pszThisLine,',');
    if (nNHeaderLines-- <= 0) {
      if (psSubString != NULL) {
	count=0;
	psSubString++;
	/* remove leading whitespace */ 
	while (isspace(*psSubString)) psSubString++;
	tThisStrlen = strlen(psSubString);
	
	/* remove trailing whitespace */
	psSubString+=tThisStrlen-1;
	while (isspace(*psSubString)) {
	  *psSubString-- = '\000';
	  ++count;
	}
	psSubString-=(tThisStrlen-count-1);
	if (stat(psSubString, &st) == -1 && errno == ENOENT){
	  printf("[WARNING] cewald_check_script: Cannot access %s\n",
		 psSubString);
	} else {
	  nNFiles++;
	}
      }
    }
  }

#ifdef VERBOSE
  printf("Found %d h5-files in %s\n",nNFiles,szScript);
#endif

  *pnNFiles = nNFiles;

  fclose(pfFile);
  return(0);
}
  
/*-------------------------------------------------------------------------
int cewald_scan_directory(char *szDirname,
			  int *pnNFiles)
{
  
  DIR *pDir;
  struct dirent *psDirEntry;
  int nNFiles=0;

  pDir = opendir(szDirname);
  while ((psDirEntry = readdir(pDir)) != NULL) {
    /* Check for .h5 files 
    if (strstr(psDirEntry->d_name,".h5") != NULL) {
      nNFiles++;
    }
  }
  closedir(pDir);

#ifdef VERBOSE
  printf("Found %d h5-files in %s\n",nNFiles,szDirname);
#endif

  *pnNFiles = nNFiles;
  return(0);
  } */

/*-------------------------------------------------------------------------*/
int cewald_load_filenames(char *szDirname,
			  Parameters *psDataParams)
{
  
  DIR *pDir;
  struct dirent *psDirEntry;
  int nIChar, nIOffset, nThisStrlen, nLastIndex,nIName;
  int nDirnameStrlen;
  char chThisChar;

  nDirnameStrlen = strlen(szDirname);
  /* Check for trailing path separator */
  if ((strrchr(szDirname,'/') - szDirname +1) != nDirnameStrlen) {
      strcat(szDirname,"/");
      nDirnameStrlen += 1;
  } 
#ifdef VERBOSE
  printf("Loading filenames.\n");
#endif

  /* Scan the directory */
  pDir = opendir(szDirname);
  nIName = 0;
  while ((psDirEntry = readdir(pDir)) != NULL) {
    /* Check for .h5 files */
    if (strstr(psDirEntry->d_name,".h5") != NULL) {
      nIOffset = nIName*CEWALD_MAX_STRLEN;
      /* Add the directory */
      strcpy((psDataParams->paszFilenames+nIOffset),szDirname);
      nIChar = 0;
      nLastIndex = CEWALD_MAX_STRLEN-1;
      /* Add h5 filename */
      nThisStrlen = strlen(psDirEntry->d_name);

      if ((nDirnameStrlen + nThisStrlen) > CEWALD_MAX_STRLEN) {
	printf("Not enough memory for filename array!\n");
	break;
      }
      while (nIChar+nDirnameStrlen < nLastIndex) {
	chThisChar = *(psDirEntry->d_name+nIChar);
	if ((nIChar < nThisStrlen) && 
	    (chThisChar != '\n') &&
	    (chThisChar != '\r')) {
	  *(psDataParams->paszFilenames+nIChar+nDirnameStrlen+nIOffset) = 
	    chThisChar;
	} else {
	  *(psDataParams->paszFilenames+nIChar+nDirnameStrlen+nIOffset) = 
	    '\000';
	}
	nIChar++;
      }
      /* Make sure the string is null-terminated */
      *(psDataParams->paszFilenames+nLastIndex+nIOffset) = '\000';
      
      nIName++;
    }
  }
  closedir(pDir);
  return(0);
}

/*-------------------------------------------------------------------------*/
int cewald_sort_by_first_angle(Parameters *psDataParams)
{
  int nIDot,nI,nJ;
  int nThisMinI;
  double dThisAngle,dThisMinAngle,dPreviousMinAngle;
  double adTempXAngles[psDataParams->nNFiles];
  double adTempYAngles[psDataParams->nNFiles];
  double adTempZAngles[psDataParams->nNFiles];
  int anTempXCenter[psDataParams->nNFiles];
  int anTempYCenter[psDataParams->nNFiles];
  int anTempScaling[psDataParams->nNFiles];  
  char *paszTempNames;

  /* Copy all arrays */
  memcpy(adTempXAngles,psDataParams->padThetaXRadians,
	 psDataParams->nNFiles*sizeof(double));
  memcpy(adTempYAngles,psDataParams->padThetaYRadians,
	 psDataParams->nNFiles*sizeof(double));
  memcpy(adTempZAngles,psDataParams->padThetaZRadians,
	 psDataParams->nNFiles*sizeof(double));
  memcpy(anTempXCenter,psDataParams->panXCenterOffset,
	 psDataParams->nNFiles*sizeof(int));
  memcpy(anTempYCenter,psDataParams->panYCenterOffset,
	 psDataParams->nNFiles*sizeof(int));
  memcpy(anTempScaling,psDataParams->panPhotonScaling,
	 psDataParams->nNFiles*sizeof(int));
  paszTempNames = 
    (char *)malloc(sizeof(char)*CEWALD_MAX_STRLEN*psDataParams->nNFiles);
  memcpy(paszTempNames,psDataParams->paszFilenames,
	 (psDataParams->nNFiles*CEWALD_MAX_STRLEN*sizeof(char)));
  cewald_erase_strarr(psDataParams->paszFilenames,psDataParams->nNFiles);

  nIDot = psDataParams->nNFiles/30;
  /* This is not ideal */
  for (nI=0;nI<psDataParams->nNFiles;nI++) {
#ifdef VERBOSE
    if (nI == 0) printf("Sorting by angle in ascending order.\n");
    if ((nI%nIDot) == 0) {
      printf(".");
    }
#endif
    /* This is below minimum angle of -pi */
    if (nI == 0) dPreviousMinAngle = -4.;
    for (nJ=0;nJ<psDataParams->nNFiles;nJ++){
      /* This is above maximum angle of +pi */
      if (nJ == 0 ) dThisMinAngle = +4.;
      dThisAngle = adTempXAngles[nJ];
      if ((dThisAngle < dThisMinAngle) && (dThisAngle > dPreviousMinAngle)) {
	  dThisMinAngle = dThisAngle;
	  nThisMinI=nJ;
      }  
    }
    dPreviousMinAngle = dThisMinAngle;
    //    printf("%d: %d\n",nI,nThisMinI);
    /* re-assign the new values */
    strcpy(psDataParams->paszFilenames+nI*CEWALD_MAX_STRLEN,
	   paszTempNames+nThisMinI*CEWALD_MAX_STRLEN);
    //    printf("%d: %f\n",nI,adTempXAngles[nThisMinI]);
    *(psDataParams->padThetaXRadians+nI)=adTempXAngles[nThisMinI];
    *(psDataParams->padThetaYRadians+nI)=adTempYAngles[nThisMinI];
    *(psDataParams->padThetaZRadians+nI)=adTempZAngles[nThisMinI];
    *(psDataParams->panXCenterOffset+nI)=anTempXCenter[nThisMinI];
    *(psDataParams->panYCenterOffset+nI)=anTempYCenter[nThisMinI];
    *(psDataParams->panPhotonScaling+nI)=anTempScaling[nThisMinI];
 }
  printf("\n\n");

  free(paszTempNames);
  return(0);
}

/*-------------------------------------------------------------------------*/
int cewald_initialize_dataset(char *szScript,
			      Parameters *psDataParams,
			      int nMyRank)
{
  FILE *pfFile;
  char pszThisLine[CEWALD_MAX_STRLEN];
  char *psSubString;
  char *psPos;
  int nIName=0;
  int nNHeaderLines=CEWALD_HEADER_LINES;
  size_t tThisStrlen;
  int count;
  hid_t hFileId;
  int nNX,nNY,nNZ, nErrorIsPresent;
  int nIDot;
  dm_adi_struct sThisAdiStruct;
  char szErrorString[CEWALD_MAX_STRLEN];
  struct stat st;


  if ((pfFile = fopen(szScript,"r")) == NULL) {
    sprintf(szErrorString,"[ERROR] cewald_initialize_dataset: Cannot open %s.",
	    szScript);
    return(1);
  }
  
  /* Get current line and check access */
  while (fgets(pszThisLine,CEWALD_MAX_STRLEN, pfFile) ) {
    /* first CEWALD_HEADER_LINES are header */
    if (nNHeaderLines-- > 0) {
      psPos = strtok(pszThisLine,",\n");
      if (strcmp(psPos,"wavelength_m") == 0) {
	psDataParams->fLambdaMeters = strtod(strtok(NULL,",\n"),NULL);
#ifdef VERBOSE
	printf("Wavelength (m): %e\n",psDataParams->fLambdaMeters);
#endif
      } else if (strcmp(psPos,"ccd_pixelsize_m") == 0) {
	psDataParams->fCCDPixelSizeMeters = strtod(strtok(NULL,",\n"),NULL);
#ifdef VERBOSE
	printf("CCD Pixelsize (m): %e\n",psDataParams->fCCDPixelSizeMeters);
#endif
      } else if (strcmp(psPos,"ccd_z_m") == 0) {
	psDataParams->fCCDZMeters = strtod(strtok(NULL,",\n"),NULL);
#ifdef VERBOSE
	printf("CCD Z (m): %e\n",psDataParams->fCCDZMeters);
#endif
      }
    } else {
      /* Extract filenames and angles */ 
      psSubString = strrchr(pszThisLine,',');
      if (psSubString != NULL) {
	count=0;
	psSubString++;
	/* remove leading whitespace */ 
	while (isspace(*psSubString)) psSubString++;
	tThisStrlen = strlen(psSubString);
	
	/* remove trailing whitespace */
	psSubString+=tThisStrlen-1;
	while (isspace(*psSubString)) {
	  *psSubString-- = '\000';
	  ++count;
	}
	psSubString-=(tThisStrlen-count-1);
	if (stat(psSubString, &st) == 0 || errno != ENOENT) {
	  strcpy(psDataParams->paszFilenames+nIName*CEWALD_MAX_STRLEN,
		 psSubString);
	  psPos = strtok(pszThisLine,",");
	  *(psDataParams->padThetaXRadians+nIName) =
	    atof(psPos);
	  psPos = strtok(NULL,",");
	  *(psDataParams->padThetaYRadians+nIName) =
	    atof(psPos);
	  psPos = strtok(NULL,",");
	  *(psDataParams->padThetaZRadians+nIName) =
	    atof(psPos);
	  /* Assume files are pre-centered for now */
	  *(psDataParams->panXCenterOffset+nIName) = 0;
	  *(psDataParams->panYCenterOffset+nIName) = 0;
	  nIName++;
	}
      }
    }
  }
  
  /* Get size from first array */
  if (dm_h5_openread(psDataParams->paszFilenames,
		     &hFileId,szErrorString,nMyRank) 
      != DM_FILEIO_SUCCESS) {
    printf("%s\n",szErrorString);
    exit(1);
  }
  
  if (dm_h5_read_adi_info(hFileId,&nNX,&nNY,&nNZ,&nErrorIsPresent,
			  &sThisAdiStruct,szErrorString,nMyRank) 
      == DM_FILEIO_FAILURE) {
    printf("Error reading \"/adi\" group info\n");
    dm_h5_close(hFileId,nMyRank);
    exit(1);
  }

  /* Don't forget to release the identifier */
  dm_h5_close(hFileId,nMyRank);
 
  psDataParams->nNX = nNX;
  psDataParams->nNY = nNY;
  
} 

/*-------------------------------------------------------------------------*/
void cewald_erase_strarr(char *paszStrarr, int nNLines) {
  int nI,nJ;

  for (nI=0;nI<nNLines;nI++) {
    for (nJ=0;nJ<CEWALD_MAX_STRLEN;nJ++) {     
      *(paszStrarr+(nI*CEWALD_MAX_STRLEN+nJ)) = '\000';
    }
  }
}

/*-------------------------------------------------------------------------*/
void cewald_rotate_vector(double *padOld3DVector, 
			  int *panNew3DVector, 
			  double dPhi,
			  double dTheta, 
			  double dPsi) 
{  
  int nNewY, nNewZ;
  double dOldX, dOldY, dOldZ;

  dOldX = (double)*(padOld3DVector + 0);
  dOldY = (double)*(padOld3DVector + 1);
  dOldZ = (double)*(padOld3DVector + 2);

  *(panNew3DVector + 0) = 
    dm_round((dm_array_real)
	     ((cos(dPsi)*cos(dPhi)-sin(dPsi)*cos(dTheta)*sin(dPhi))*dOldX +
	      (sin(dPsi)*cos(dPhi)+cos(dPsi)*cos(dTheta)*sin(dPhi))*dOldY +
	      (sin(dPhi)*sin(dTheta))*dOldZ));

  *(panNew3DVector + 1) = 
    dm_round((dm_array_real)
	     (-(cos(dPsi)*sin(dPhi)+sin(dPsi)*cos(dTheta)*cos(dPhi))*dOldX +
	      (cos(dPsi)*cos(dTheta)*cos(dPhi)-sin(dPsi)*sin(dPhi))*dOldY +
	      (sin(dTheta)*cos(dPhi))*dOldZ));    
	     
  *(panNew3DVector + 2) = 
    dm_round((dm_array_real)
	     ((sin(dTheta)*sin(dPsi))*dOldX -
	      (cos(dPsi)*sin(dTheta))*dOldY +
	      (cos(dTheta))*dOldZ));
}

/*-------------------------------------------------------------------------*/
void cewald_initialize_data(Parameters *psDataParams,
			    dm_array_real_struct *psDataCube,
			    dm_array_int_struct *psCounterCube,
			    dm_array_real_struct *psThisAdi,
			    int nP)
{
  int nN, nI;
  
  /* Determine size of 3D cubes */
  if (psDataParams->nNX > (1LL << 9)) {
    nN = (1LL << 10);
  } else if (psDataParams->nNX > (1LL << 8)) {
    nN = (1LL << 9);
  } else {
    nN = (1LL << 8);
  }
    
  /* Allocate memory */
  psDataCube->nx = nN;
  psDataCube->ny = nN;
  psDataCube->nz = nN;
  psDataCube->npix = (dm_array_index_t)nN*
    (dm_array_index_t)nN*
    (dm_array_index_t)nN;
  DM_ARRAY_REAL_STRUCT_INIT(psDataCube,psDataCube->npix,nP);

  psCounterCube->nx = nN;
  psCounterCube->ny = nN;
  psCounterCube->nz = nN;
  psCounterCube->npix = (dm_array_index_t)nN*
    (dm_array_index_t)nN*
    (dm_array_index_t)nN;
  DM_ARRAY_INT_STRUCT_INIT(psCounterCube,psCounterCube->npix,nP);

  psThisAdi->nx = psDataParams->nNX;
  psThisAdi->ny = psDataParams->nNY;
  psThisAdi->nz = 1;
  psThisAdi->npix = (dm_array_index_t)psDataParams->nNX*
    (dm_array_index_t)psDataParams->nNY;
  DM_ARRAY_REAL_STRUCT_INIT(psThisAdi,psThisAdi->npix,nP);
  
  /* Initialize counter cube and data cube to all 0s */
  for (nI=0;nI<psCounterCube->local_npix;nI++) {
    *(psCounterCube->int_array + nI) = 0;
    *(psDataCube->real_array + nI) = (dm_array_real)0.0;
  }
  printf("Initialized Data\n");
}

/*-------------------------------------------------------------------------*/
int cewald_load_adi(int nIndex,
		    Parameters *psDataParams,
		    dm_array_real_struct *psThisAdi,
		    int nMyRank,
		    int nP,
		    char *szErrorString)
{
  hid_t hH5Id;
  dm_array_real_struct sDummyError;

  /* for now just allocate memory for the error but don't 
   * pass it along.
   */
  sDummyError.nx = psThisAdi->nx;
  sDummyError.ny = psThisAdi->ny;
  sDummyError.nz = psThisAdi->nz;
  sDummyError.npix = psThisAdi->npix;
  DM_ARRAY_REAL_STRUCT_INIT((&sDummyError),sDummyError.npix,nP);

  /* Zero this adi */
  dm_array_zero_real(psThisAdi);

  /* Get this adi */
  if (dm_h5_openread((psDataParams->paszFilenames+nIndex*CEWALD_MAX_STRLEN),
		     &hH5Id,szErrorString,nMyRank) 
      != DM_FILEIO_SUCCESS) {
    return(-1);
  }
  
  if (dm_h5_read_adi(hH5Id,psThisAdi,&sDummyError,
		     szErrorString,nMyRank,nP) != DM_FILEIO_SUCCESS) {
    dm_h5_close(hH5Id,nMyRank);
    return(-1);
  }

  dm_h5_close(hH5Id,nMyRank);

  /* Crop this adi */
  dm_array_crop_2d_real(psThisAdi,*(psDataParams->panXCenterOffset+nIndex),
			*(psDataParams->panYCenterOffset+nIndex),
			nMyRank,nP);

  free(sDummyError.real_array);
  return(0);
}

/*-------------------------------------------------------------------------*/
void cewald_ewald_n_rotate(dm_array_real_struct *psThisAdi,
			   Parameters *psDataParams,
			   int nIndex,
			   dm_array_real_struct *psDataCube,
			   dm_array_int_struct *psCounterCube)
{
  int nIX, nIY, nQIX, nQIY;
  dm_array_index_t nThisQIndex, nThisIndex;
  double daIQ[3];
  int naIQ[3];
  double dDenominator, dConstant;
  
  /* Map each pixel of each given adi array */
  for (nIY=0; nIY<psThisAdi->ny; nIY++) {
    for (nIX=0; nIX<psThisAdi->nx; nIX++) {
	
      /* the formula is working from the center of the array 
       * but our input arrays are fft-centered
       */
      if (nIX < psThisAdi->nx/2) {
	nQIX = nIX;
      } else {
	nQIX = -(psThisAdi->nx - nIX);
      }

      if (nIY < psThisAdi->ny/2) {
	nQIY = nIY;
      } else {
	nQIY = -(psThisAdi->ny - nIY);
      }
 
      /*nQIX = nIX - psThisAdi->nx/2;
	nQIY = nIY - psThisAdi->ny/2;*/

      dDenominator = 
	sqrt((double)(psDataParams->fCCDPixelSizeMeters *
		      psDataParams->fCCDPixelSizeMeters) *
	     ((double)(nQIX*nQIX) + (double)(nQIY*nQIY)) + 
	     (double)(psDataParams->fCCDZMeters * 
		      psDataParams->fCCDZMeters));
      
      dConstant = 
	sqrt((double)(psDataParams->fCCDZMeters*psDataParams->fCCDZMeters) + 
	     0.25*((double)(psDataParams->nNX*psDataParams->nNX) *
		   (double)(psDataParams->fCCDPixelSizeMeters*
			    psDataParams->fCCDPixelSizeMeters)));
      
      /* Map each pixel onto Ewald sphere */
      daIQ[0] = (double)(nQIX)*dConstant / dDenominator; 
      daIQ[1] = (double)(nQIY)*dConstant / dDenominator; 
      daIQ[2] = dConstant / (double)(psDataParams->fCCDPixelSizeMeters) * 
	((double)(psDataParams->fCCDZMeters) / dDenominator - 1.0);

      /* Rotate this vector to find final position. Note that X,Y,Z correspond 
       * to the Euler angles Phi,Theta,Psi in the x-convention. 
       */
      cewald_rotate_vector(daIQ,naIQ,
			   *(psDataParams->padThetaXRadians + nIndex),
			   *(psDataParams->padThetaYRadians + nIndex),
			   *(psDataParams->padThetaZRadians + nIndex));

      /* Now map into cube unless index lies outside */
      if ((naIQ[0] >= -psDataCube->nx/2) && (naIQ[0] < psDataCube->nx/2) &&
	  (naIQ[1] >= -psDataCube->ny/2) && (naIQ[1] < psDataCube->ny/2) &&
	  (naIQ[2] >= -psDataCube->nz/2) && (naIQ[2] < psDataCube->nz/2)) {

	/* New vector is still with respect to center of array.
	 * Shift negative indices to the upper positive half
	 */
	if (naIQ[0] < 0) {
	  naIQ[0] += psDataCube->nx;
	}
	if (naIQ[1] < 0) {
	  naIQ[1] += psDataCube->ny;
	}
	if (naIQ[2] < 0) {
	  naIQ[2] += psDataCube->nz;
	}

	/*naIQ[0] += psDataCube->nx/2;
	naIQ[1] += psDataCube->ny/2;
	naIQ[2] += psDataCube->nz/2;*/
	
      /* Now map into cube unless it exceeds its size 
      if ((naIQ[0] < psDataCube->nx) && (naIQ[0] >= 0) &&
	  (naIQ[1] < psDataCube->ny) && (naIQ[1] >= 0) &&
	  (naIQ[2] < psDataCube->nz) && (naIQ[2] >= 0)) {*/
	
	/* Calculate index for 3D cube - SHOULD THIS BE psDataCube->n*
	 * instead of psThisAdi->n*?
	 */
	nThisQIndex = (dm_array_index_t)naIQ[0] + (dm_array_index_t)naIQ[1]*
	  (dm_array_index_t)psDataCube->nx + (dm_array_index_t)naIQ[2]*
	  (dm_array_index_t)psDataCube->nx *
	  (dm_array_index_t)psDataCube->ny;

	/* Calculate index for 2D array */ 
	nThisIndex = (dm_array_index_t)nIX + (dm_array_index_t)psThisAdi->nx*
	  (dm_array_index_t)nIY;

	/* Map it only if non-zero */
	if ((*(psThisAdi->real_array + nThisIndex)) != (dm_array_real)0) {
	  *(psDataCube->real_array + nThisQIndex)+=
	    *(psThisAdi->real_array + nThisIndex);
	  
	  /* Augment counter by 1 */
	  (*(psCounterCube->int_array + nThisQIndex))++;
	}
      }
    }
  }
  printf("Done with file %d of %d\n",(nIndex+1),psDataParams->nNFiles);
}

/*-------------------------------------------------------------------------*/ 
void cewald_average(dm_array_real_struct *psDataCube,
		    dm_array_int_struct *psCounterCube)
{
  int nI;

  for (nI=0;nI<psDataCube->local_npix;nI++) {
    if (*(psCounterCube->int_array + nI) != 0) {
      *(psDataCube->real_array + nI) /= 
	*(psCounterCube->int_array + nI);
    }
  }
}

/*-------------------------------------------------------------------------*/
void cewald_write_h5(Parameters *psDataParams,
		     dm_array_real_struct *psDataCube,
		     char *szOutput,
		     char *szErrorString,
		     int nMyRank,
		     int nP)

{
  hid_t hH5Id;
  dm_ainfo_struct sAinfo;
  dm_comment_struct sComments;
  dm_adi_struct sAdi;
  dm_array_real_struct sDummyError;
  int nI;
  clock_t t;

  sDummyError.nx = 0;
  sDummyError.ny = 0;
  sDummyError.nz = 0;
  sDummyError.npix = 0;

  if (dm_h5_create(szOutput,&hH5Id,szErrorString,nMyRank) 
      != DM_FILEIO_SUCCESS) {
    printf("%s\n",szErrorString);
    exit(1);
  }
  
  /* Prepare the ainfo structure */
  sAinfo.n_frames_max = psDataParams->nNFiles;
  sAinfo.string_length = CEWALD_MAX_STRLEN;
   
  sAinfo.file_directory =
    (char *)malloc(sAinfo.string_length);    
  sAinfo.filename_array = 
    (char *)malloc(sAinfo.n_frames_max*
		   sAinfo.string_length);
  sAinfo.systime_array = 
    (char *)malloc(sAinfo.n_frames_max*
		   sAinfo.string_length);
  sAinfo.theta_x_radians_array = 
    (double *)malloc(sAinfo.n_frames_max*sizeof(double));
  sAinfo.theta_y_radians_array = 
    (double *)malloc(sAinfo.n_frames_max*sizeof(double));
  sAinfo.theta_z_radians_array = 
    (double *)malloc(sAinfo.n_frames_max*sizeof(double));
  sAinfo.xcenter_offset_pixels_array = 
    (double *)malloc(sAinfo.n_frames_max*sizeof(double));
  sAinfo.ycenter_offset_pixels_array = 
    (double *)malloc(sAinfo.n_frames_max*sizeof(double));
 
  /* Important to clear the structure first!!! */
  dm_clear_ainfo(&sAinfo);
  
  sAinfo.n_frames = psDataParams->nNFiles;
  
  for (nI=0;nI<psDataParams->nNFiles;nI++) {
    dm_add_filename_to_ainfo((psDataParams->paszFilenames+nI*CEWALD_MAX_STRLEN), 
			     &sAinfo);
    
    if (dm_add_double_to_ainfo("xcenter",*(psDataParams->panXCenterOffset), 
			       &sAinfo,szErrorString) != DM_FILEIO_SUCCESS) {
      printf("%s\n",szErrorString);
      exit(1);
    }
    if (dm_add_double_to_ainfo("ycenter",*(psDataParams->panYCenterOffset),
			       &sAinfo,szErrorString) != DM_FILEIO_SUCCESS) {
      printf("%s\n",szErrorString);
      exit(1);
    }
    if (dm_add_double_to_ainfo("theta_x",*(psDataParams->padThetaXRadians), 
			       &sAinfo,szErrorString) != DM_FILEIO_SUCCESS) {
      printf("%s\n",szErrorString);
      exit(1);
    }
    if (dm_add_double_to_ainfo("theta_y",*(psDataParams->padThetaYRadians), 
			       &sAinfo,szErrorString) != DM_FILEIO_SUCCESS) {
      printf("%s\n",szErrorString);
      exit(1);
    }
    if (dm_add_double_to_ainfo("theta_z",*(psDataParams->padThetaZRadians), 
			       &sAinfo,szErrorString) != DM_FILEIO_SUCCESS) {
      printf("%s\n",szErrorString);
      exit(1);
    }

  }
  
  if (dm_h5_write_ainfo(hH5Id,&sAinfo,
			szErrorString,nMyRank) != DM_FILEIO_SUCCESS) {
    printf("%s\n",szErrorString);
    dm_h5_close(hH5Id,nMyRank);
    exit(1);
  }
  
  
  /* Prepare the adi_structure */
  sAdi.lambda_meters = psDataParams->fLambdaMeters;
  sAdi.camera_z_meters = psDataParams->fCCDZMeters;
  sAdi.camera_x_pixelsize_meters = psDataParams->fCCDPixelSizeMeters;
  sAdi.camera_y_pixelsize_meters = psDataParams->fCCDPixelSizeMeters;
  sAdi.xcenter_offset_pixels = 0;
  sAdi.ycenter_offset_pixels = 0;

  
  if (dm_h5_write_adi(hH5Id,&sAdi,psDataCube,&sDummyError,
		      szErrorString,nMyRank,nP) != DM_FILEIO_SUCCESS) {
    printf("%s\n",szErrorString);
    dm_h5_close(hH5Id,nMyRank);
    exit(1);
  }

  /* Write comments */
  sComments.n_strings_max = 5;
  sComments.string_length = CEWALD_MAX_STRLEN;
  sComments.string_array = 
    (char *)malloc(sComments.n_strings_max*
		   sComments.string_length);
  sComments.specimen_name = 
    (char *)malloc(sComments.string_length);
  sComments.collection_date = 
      (char *)malloc(sComments.string_length);

  dm_clear_comments(&sComments);
  
  dm_add_string_to_comments("Assembled by CEWALD.",
			    &sComments);
  dm_add_specimen_name_to_comments("Glass Aug 16th, 2008",&sComments);
  time(&t);
  dm_add_collection_date_to_comments(ctime(&t),&sComments);
  

  if (dm_h5_create_comments(hH5Id,&sComments,
			    szErrorString,nMyRank) != DM_FILEIO_SUCCESS) {
    printf("%s\n",szErrorString);
    dm_h5_close(hH5Id,nMyRank);
    exit(1);
  }

  dm_h5_close(hH5Id,nMyRank);
  
  /* free memory for Ainfo-struct */
  free(sAinfo.filename_array);
  free(sAinfo.file_directory);
  free(sAinfo.systime_array);
  free(sAinfo.theta_x_radians_array);
  free(sAinfo.theta_y_radians_array);
  free(sAinfo.theta_z_radians_array);
  free(sAinfo.xcenter_offset_pixels_array);
  free(sAinfo.ycenter_offset_pixels_array);

  /* free memory from comments */
  free(sComments.string_array);
  free(sComments.specimen_name);
  free(sComments.collection_date);

 #ifdef VERBOSE
  printf("Wrote %s.\n",szOutput);
 #endif
}
