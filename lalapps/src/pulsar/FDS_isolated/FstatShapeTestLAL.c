/************************************ <lalVerbatim file="FstatShapeTestLALCV">
Author: Itoh, Yousuke 
$Id$
************************************* </lalVerbatim> */
/*
 * !!!EXPERIMENTAL VERSION!!!
 *
 * Purposes are to make this code more modular to prepare for a 
 * possible future functionization of ComputeFstatistic and 
 * for c-code-based search pipelines including MC experiments.
 */
/*
  ./FstatShapeTestLAL -o FaFb00.001 -t FaFb01.001 > FST.txt
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <float.h>

#include <lal/LALStdlib.h>
#include <lal/LALDatatypes.h>
#include <lal/LALConstants.h>
#include <lal/LALDemod.h>

#include <lalapps.h>


RCSID( "$Id$" );


/*----------------------------------------------------------------------------------*/
/* Error code for Fstatistic shape test main part. */
#define FSTATSHAPETESTC_ENULL 		1
#define FSTATSHAPETESTC_ENONULL		2
#define FSTATSHAPETESTC_EFILEIO         3
#define FSTATSHAPETESTC_EMEMORY         4
#define FSTATSHAPETESTC_EODDDATA	5


#define FSTATSHAPETESTC_MSGENULL 	"Input argument pointer is NULL."
#define FSTATSHAPETESTC_MSGENONULL	"Output argument pointer is not NULL."
#define FSTATSHAPETESTC_MSGEFILIO 	"File I/O error."
#define FSTATSHAPETESTC_MSGEMEMORY 	"Memory allocation error."
#define FSTATSHAPETESTC_MSGEODDDATA	"Strange data."
/*----------------------------------------------------------------------------------*/
/*----------------------------------------------------------------------------------*/
/* Error code for ChiSquare distribution computation. */
#define FSTATSHAPETESTC_EDATADOMAIN	6
#define FSTATSHAPETESTC_EPARAMDOMAIN	7
#define FSTATSHAPETESTC_EINPUTDOMAIN	8
#define FSTATSHAPETESTC_EDOFDOMAIN	9


#define FSTATSHAPETESTC_MSGEDATADOMAIN	"Data should be non-negative."
#define FSTATSHAPETESTC_MSGEPARAMDOMAIN	"Parameter domain error."
#define FSTATSHAPETESTC_MSGEINPUTDOMAIN	"Input argument(s) domain error."
#define FSTATSHAPETESTC_MSGEDOFDOMAIN	"Degrees of freedom should be non-negative."
/*----------------------------------------------------------------------------------*/


/* structure typedef */
typedef struct tagFSTControlParameters {
  INT4  nData; /* Number of bins where the veto signal and observed data overlapped. */
  INT4  indexOffset;/* Starting frequencies bins difference between the observed data file and the veto signal file. */
  INT4  indexStepO; /* Distance (in the unit of dFreq) of two neibouring frequency bins of the observed data file */
  INT4  indexStepT; /* Distance (in the unit of dFreq) of two neibouring frequency bins of veto signal file */
  AMCoeffs amc;     /* a(t), b(t), A coefficient of JKS = (a(t)||a(t)), B, C, and D. */
} FSTControlParameters; 

typedef struct tagFSTClustInfo {
  INT4  nData; /* number of the data points in the cluster */
  REAL8 startFreq; /* starting frequency of the cluster */
  REAL8 deltaFreq; /* frequency resolution */
  REAL8 fmax;  /* Frequency at which the maximum of F stat occurs. */
  REAL8 FSmax; /* Maximum of the F Statistic in the cluster. */
  AMCoeffs amc; /* a(t), b(t), A coefficient of JKS = (a(t)||a(t)), B, C, and D. */
} FSTClustInfo; /* Files headers information */

typedef struct tagFSTUserInput {
  BOOLEAN computeProbFlag; /* compute probability or not */
  CHAR *dbglvl; /* lalDebugLevel */
  REAL8 sigLevel; /* significance level for hypothesis test */
  CHAR *obsvdatafile; /* observed data filename */
  CHAR *testdatafile; /* veto signal filename */
} FSTUserInput; /* Variables user can specify. */

typedef struct tagFSTFstatPair {
  REAL8Vector *freqObsv; /* Frequency vector in the observed data file */
  REAL8Vector *freqTest; /* Frequency vector in the test data (= veto signal) file */
  LALFstat *FaFbObsv;    /* F, Fa, Fb of the observed data file */
  LALFstat *FaFbTest;    /* F, Fa, Fb of the test data file */
} FSTFstatPair;

typedef struct tagFSTClustInfoPair {
  FSTClustInfo ObsvCI;
  FSTClustInfo TestCI;
} FSTClustInfoPair;



typedef struct tagFSTFVetoStat {
  REAL8 vetoStatistic; /* veto statistic. This follows chi-square distribution in the perfectly matched case. */
  REAL8 dof;           /* degrees of freedom */
} FSTFVetoStat;


/*-----------------------------------------------------------------*/
/*                                                                 */
/*-----------------------------------------------------------------*/
INT4 main(INT4 argc, CHAR ** argv)
{
  /* initialize status */
  LALStatus status = blank_status;  

  REAL8 probability = 0.0;  /* Confidence level = Chi-Square cumulative probability from 0 to data */
  BOOLEAN rejection = 0;    /* Reject the  hypothesis if 1. */
  FSTUserInput cla; /* user input: command line arguments */

  FSTClustInfoPair clustInfoPair;
  FSTFstatPair *FaFbPair;
  FSTFVetoStat vetoStat;

  /* function prototype */ 
  void HandleComArg( LALStatus *, FSTUserInput *, INT4 argc, CHAR ** argv );
  void ReadClusterInfo( LALStatus *, FSTClustInfoPair *, FSTUserInput * );
  void ReadData( LALStatus *, FSTFstatPair *, FSTUserInput * );
  void ComputeVetoStatistic( LALStatus *, 
                             FSTFVetoStat *, /* output veto statistic and degrees of freedom. */ 
			     FSTFstatPair *FaFbPair, /* input Fa Fb of observed data and veto signal. */
			     FSTClustInfoPair *clustInfoPair /* input cluster information of observed data and veto signal. */);
  void LALChi2CDFP( LALStatus *, REAL8 *chi2cdfp, const REAL8 *data, const REAL8 *dof );


  /* set LAL error-handler */
  /* exits with the returned status code if there is an error. */
  lal_errhandler = LAL_ERR_EXIT; 
  /* lal_errhandler = LAL_ERR_RTRN;  */


  LAL_CALL( HandleComArg( &status, &cla, argc, argv ), &status );
  if( status.statusCode ) {
    REPORTSTATUS( &status );
    exit(1);
  } 

  set_debug_level( cla.dbglvl );


  /* Read the header information.  */
  LAL_CALL( ReadClusterInfo( &status, &clustInfoPair, &cla ), &status );
  if( status.statusCode ) {
    REPORTSTATUS( &status );
    exit(1);
  } 

  /* Memory allocation for reading data. */
  FaFbPair = (FSTFstatPair *) LALMalloc(sizeof(FSTFstatPair));
  FaFbPair->FaFbObsv = (LALFstat *) LALMalloc(sizeof(LALFstat));
  FaFbPair->FaFbTest = (LALFstat *) LALMalloc(sizeof(LALFstat));

  FaFbPair->FaFbObsv->F  = (REAL8 *)     LALMalloc( clustInfoPair.ObsvCI.nData * sizeof(REAL8) );
  FaFbPair->FaFbObsv->Fa = (COMPLEX16 *) LALMalloc( clustInfoPair.ObsvCI.nData * sizeof(COMPLEX16) );
  FaFbPair->FaFbObsv->Fb = (COMPLEX16 *) LALMalloc( clustInfoPair.ObsvCI.nData * sizeof(COMPLEX16) );

  FaFbPair->FaFbTest->F  = (REAL8 *)     LALMalloc( clustInfoPair.TestCI.nData * sizeof(REAL8) );
  FaFbPair->FaFbTest->Fa = (COMPLEX16 *) LALMalloc( clustInfoPair.TestCI.nData * sizeof(COMPLEX16) );
  FaFbPair->FaFbTest->Fb = (COMPLEX16 *) LALMalloc( clustInfoPair.TestCI.nData * sizeof(COMPLEX16) );

  if ( ( FaFbPair == NULL ) ||  
       ( FaFbPair->FaFbObsv == NULL ) ||
       ( FaFbPair->FaFbTest == NULL ) ||
       ( FaFbPair->FaFbObsv->F == NULL ) || 
       ( FaFbPair->FaFbObsv->Fa == NULL ) || 
       ( FaFbPair->FaFbObsv->Fb == NULL ) || 
       ( FaFbPair->FaFbTest->F == NULL ) ||
       ( FaFbPair->FaFbTest->Fa == NULL ) ||
       ( FaFbPair->FaFbTest->Fb == NULL ) ) {
    fprintf(stderr,"Memory allocation error in F statistic Shape test code ");
    exit(1);
  }

  FaFbPair->freqObsv = NULL;
  FaFbPair->freqTest = NULL;
  LAL_CALL( LALDCreateVector( &status, &(FaFbPair->freqObsv), clustInfoPair.ObsvCI.nData), &status );
  if( status.statusCode ) {
    REPORTSTATUS( &status );
    exit(1);
  } 
  LAL_CALL( LALDCreateVector( &status, &(FaFbPair->freqTest), clustInfoPair.TestCI.nData), &status );
  if( status.statusCode ) {
    REPORTSTATUS( &status );
    exit(1);
  } 

  /* read the data, and store them into FaFbPair.  */
  LAL_CALL( ReadData( &status, FaFbPair, &cla  ), &status );
  if( status.statusCode ) {
    REPORTSTATUS( &status );
    exit(1);
  } 

  /* Main routine */
  LAL_CALL( ComputeVetoStatistic( &status, &vetoStat,  FaFbPair, &clustInfoPair ), &status );
  if( status.statusCode ) {
    REPORTSTATUS( &status );
    exit(1);
  } 


  /* Free Memory */
  LAL_CALL( LALDDestroyVector( &status, &(FaFbPair->freqObsv) ), &status );
  if( status.statusCode ) {
    REPORTSTATUS( &status );
    exit(1);
  } 
  LAL_CALL( LALDDestroyVector( &status, &(FaFbPair->freqTest) ), &status );
  if( status.statusCode ) {
    REPORTSTATUS( &status );
    exit(1);
  } 

  LALFree( FaFbPair->FaFbObsv->F );
  LALFree( FaFbPair->FaFbObsv->Fa );
  LALFree( FaFbPair->FaFbObsv->Fb );
  LALFree( FaFbPair->FaFbTest->F );
  LALFree( FaFbPair->FaFbTest->Fa );
  LALFree( FaFbPair->FaFbTest->Fb );
  LALFree( FaFbPair->FaFbObsv );
  LALFree( FaFbPair->FaFbTest );
  LALFree( FaFbPair );


  /* Final result */
  fprintf(stdout, "%14.9f %14.6g %5.1f %10.5f ",
	  clustInfoPair.ObsvCI.fmax, clustInfoPair.ObsvCI.FSmax, vetoStat.dof, vetoStat.vetoStatistic);
  if( cla.computeProbFlag ) {
    {
      LAL_CALL( LALChi2CDFP( &status, &probability, &(vetoStat.vetoStatistic), &(vetoStat.dof) ), &status );
    }
    if( 1.0 - probability < cla.sigLevel ) {
      rejection = 1; /* We reject the null hypothesis */
    }
    /* NOTE: The code calls chi2cdfp() that is an integral from 0 to x of 
     *       chi2pdf(). Therefore, the probability here is confidence level. 
     *       The significance level is 1 - confidence level. 
     */
    fprintf(stdout, "%8.5e %d", 1.0 - probability, rejection);
  }
  fprintf(stdout,"\n");


  LALCheckMemoryLeaks();
  return status.statusCode;
} /* INT4 main() */



/*-----------------------------------------------------------------*/
/*                                                                 */
/*-----------------------------------------------------------------*/
void 
HandleComArg( LALStatus *status, 
	      FSTUserInput *cla, /* output */ 
	      INT4 argc,      /* input */
	      CHAR *argv[]    /* input */) 
{
  INT4 option;
  extern CHAR *optarg;
  
  void showHelp( LALStatus *status, FSTUserInput *cla );

  INITSTATUS( status, "HandleComArg", rcsid );

  /* First initialization of all the cla variables. */
  cla->computeProbFlag = 0;
  cla->dbglvl = "0";
  cla->sigLevel = 0.01;
  cla->obsvdatafile = "FaFbObsv.txt";
  cla->testdatafile = "FaFbtest.txt";


  /* scan through the list of arguments on the command line 
     and get the input data filename*/
  
  while ( ( option = getopt(argc, argv,"Chpo:t:s:l:") ) != -1 ) {
    switch (option) {
   case 'C':
     /* Verbose Output for debugging */
     cla->computeProbFlag = 1;
      break;
    case 'o':
      /* Name of observed data file */
      cla->obsvdatafile = optarg;
      break;
    case 't':
      /* Name of test data file */
      cla->testdatafile = optarg;
      break;
    case 'l':
      /* lalDebugLevel */
      cla->dbglvl = optarg;
      break;
    case 's':
      /* Significance level */
      cla->sigLevel = atof(optarg);
      break;
    case 'h':
      /* help */
      showHelp( status, cla );
      break;
    default:
      /* unrecognized option */
      fprintf( stderr, "Unrecognized option argument %c\n", option );
      exit(1);
      break;
    }
  }

  RETURN( status );
} /* void HandleComArg() */


/*-----------------------------------------------------------------*/
/* ShowHelp                                                        */
/*-----------------------------------------------------------------*/
void 
showHelp( LALStatus *status, 
	  FSTUserInput *cla /* input */) 
{
  INITSTATUS( status, "showHelp", rcsid );

  fprintf(stderr,"Usage: FstatShapeTest [-hC] [-o <>][-t <>][-v <>][-l <>]\n");
  fprintf(stderr,
	  "-o: <CHAR STRING: filename> File <filename> contains the observed data to be vetoed: [%s]\n",cla->obsvdatafile);
  fprintf(stderr,
	  "-t: <CHAR STRING: filename> File <filename> contains the veto signal: [%s]\n",cla->testdatafile);
  fprintf(stderr,"-v <INT2: 0, 1 or 2>: Output in a verbose way. Mainly for debugging. [0]\n");
  fprintf(stderr,"-l <CHAR:>: lalDebugLevel. [0]\n");
  fprintf(stderr,"-C : Compute Probability assuming chi square ditributoin. [False]\n");
  fprintf(stderr,"FstatSphapeTest -h : Show this help\n");
  fprintf(stderr,"Example: ./FstatShapeTest -o <ObservedDataFile> -t <VetoDataFile>\n");
  fprintf(stderr,"Output from left to right:\n");
  fprintf(stderr,"(1) Frequency at which the maximum of F occurs\n");
  fprintf(stderr,"(2) Frequency at which the minimum of the veto statistic occurs\n");
  fprintf(stderr,"(3) Maximum of F in the cluster\n");
  fprintf(stderr,"(4) Degrees of freedom\n");
  fprintf(stderr,"(5) Veto statistic\n");
  fprintf(stderr,"[(6) Chi square Probability]\n");
  fprintf(stderr,"[(7) If 0, the observed signal is consistent with the veto signal, \n");
  fprintf(stderr,"with the significance level = %f.]\n", cla->sigLevel);
  exit(0);

  /* should not be here */
  RETURN( status );
} /* void showHelp() */





/*-----------------------------------------------------------------*/
/*                                                                 */
/*-----------------------------------------------------------------*/
void 
ReadClusterInfo( LALStatus *status, 
		 FSTClustInfoPair *clustInfoPair, /* output */
		 FSTUserInput *cla /* input */)
{ 
  const INT4 maxDataPoints = 16384; /* = 2^14 */

  INT4  nObsv,nTest; 
  REAL8 startFreqO,deltaFreqO; 
  REAL8 startFreqT,deltaFreqT;
  REAL8 Aobsv,Bobsv,Cobsv;
  REAL8 Atest,Btest,Ctest;
  REAL8 fmaxObsv,fmaxTest;
  REAL8 Fmaxo,Fmaxt;
  const REAL8 errorTol = LAL_REAL4_EPS;
  FILE *fpobsv, *fptest;

  INITSTATUS( status, "ReadClusterInfo", rcsid );
  ASSERT ( cla, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  
  ASSERT ( clustInfoPair != NULL, status, FSTATSHAPETESTC_ENONULL , FSTATSHAPETESTC_MSGENONULL );  

  /* Counts the number of points in the cluster */
  /* read the hader of the observed data*/
  fpobsv = fopen(cla->obsvdatafile,"r");
  if(fpobsv==NULL) {
    fprintf(stderr,"File open error in FstatShapeTest: %s\n",cla->obsvdatafile);
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }
  if(fscanf(fpobsv,"%d",&nObsv)==EOF) {    
    fprintf(stderr,"File format error in FstatShapeTest: %s\n",cla->obsvdatafile);
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }
  if(fscanf(fpobsv,"%lf %lf",&fmaxObsv,&Fmaxo)==EOF) {    
    fprintf(stderr,"File format error in FstatShapeTest: %s\n",cla->obsvdatafile);
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }
  if(fscanf(fpobsv,"%lf %lf",&startFreqO,&deltaFreqO)==EOF) {    
    fprintf(stderr,"File format error in FstatShapeTest: %s\n",cla->obsvdatafile);
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }
  if(fscanf(fpobsv,"%lf %lf %lf",&Aobsv,&Bobsv,&Cobsv)==EOF) {    
    fprintf(stderr,"File format error in FstatShapeTest: %s\n",cla->obsvdatafile);
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }
  fclose(fpobsv);



  /* read the hader of the test data*/
  fptest = fopen(cla->testdatafile,"r");
  if(fptest==NULL) {
    fprintf(stderr,"File open error in FstatShapeTest: %s\n",cla->testdatafile);
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }
  if(fscanf(fptest,"%d",&nTest)==EOF) {    
    fprintf(stderr,"File format error in FstatShapeTest: %s\n",cla->testdatafile);
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }
  if(fscanf(fptest,"%lf %lf",&fmaxTest,&Fmaxt)==EOF) {    
    fprintf(stderr,"File format error in FstatShapeTest: %s\n",cla->obsvdatafile);
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }
  if(fscanf(fptest,"%lf %lf",&startFreqT,&deltaFreqT)==EOF) {    
    fprintf(stderr,"File format error in FstatShapeTest: %s\n",cla->obsvdatafile);
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }
  if(fscanf(fptest,"%lf %lf %lf",&Atest,&Btest,&Ctest)==EOF) {    
    fprintf(stderr,"File format error in FstatShapeTest: %s\n",cla->obsvdatafile);
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }
  fclose(fptest);


  if(nObsv>maxDataPoints) {
    fprintf(stderr,
	    "Error: Number of the data points exceeds maxDataPoints.\n");
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }
  if(nTest>maxDataPoints) {
    fprintf(stderr,
	    "Error: Number of the data points exceeds maxDataPoints.\n");
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }

  /* Check consistency betwee the data files.*/
  if(fabs(Aobsv-Atest)>fabs(Aobsv)*errorTol) { 
    fprintf(stderr,
	    "Error: A of data is different from A of test.\n");
    ABORT( status, FSTATSHAPETESTC_EODDDATA, FSTATSHAPETESTC_MSGEODDDATA );
  }
  if(fabs(Bobsv-Btest)>fabs(Bobsv)*errorTol) {
    fprintf(stderr,
	    "Error: A of data is different from B of test.\n");
    ABORT( status, FSTATSHAPETESTC_EODDDATA, FSTATSHAPETESTC_MSGEODDDATA );
  }
  if(fabs(Cobsv-Ctest)>fabs(Cobsv)*errorTol) { 
    fprintf(stderr,
	    "Error: A of data is different from C of test.\n");
    ABORT( status, FSTATSHAPETESTC_EODDDATA, FSTATSHAPETESTC_MSGEODDDATA );
  }

  /* Then get the header information. */
  clustInfoPair->ObsvCI.nData = nObsv;
  clustInfoPair->ObsvCI.startFreq = startFreqO;
  clustInfoPair->ObsvCI.deltaFreq = deltaFreqO;
  clustInfoPair->ObsvCI.fmax  = fmaxObsv;
  clustInfoPair->ObsvCI.FSmax = Fmaxo;


  /* FIX ME ! 
   * Fstat shape test code does not use neither a(t) nor b(t), but other codes does...
  clustInfoPair->ObsvCI.amc.a = NULL;
  clustInfoPair->ObsvCI.amc.b = NULL:
   */

  clustInfoPair->ObsvCI.amc.A = Aobsv;
  clustInfoPair->ObsvCI.amc.B = Bobsv;
  clustInfoPair->ObsvCI.amc.C = Cobsv;
  clustInfoPair->ObsvCI.amc.D = Aobsv * Bobsv - Cobsv * Cobsv;



  clustInfoPair->TestCI.nData = nTest; 
  clustInfoPair->TestCI.startFreq = startFreqT;
  clustInfoPair->TestCI.deltaFreq = deltaFreqT;
  clustInfoPair->TestCI.fmax  = fmaxTest;
  clustInfoPair->TestCI.FSmax = Fmaxt;


  /* FIX ME ! 
   * Fstat shape test code does not use neither a(t) nor b(t), but other codes does...
  clustInfoPair->TestCI.amc.a = NULL;
  clustInfoPair->TestCI.amc.b = NULL;
   */

  clustInfoPair->TestCI.amc.A = Atest;
  clustInfoPair->TestCI.amc.B = Btest;
  clustInfoPair->TestCI.amc.C = Ctest;
  clustInfoPair->TestCI.amc.D = Atest * Btest - Ctest * Ctest;



  RETURN( status );
} /* void ReadClusterInfo() */


/*-----------------------------------------------------------------*/
/*                                                                 */
/*-----------------------------------------------------------------*/
void 
ComputeVetoStatistic( LALStatus *status, 
		      FSTFVetoStat *vetoStatistic, /* output */
		      FSTFstatPair *FaFbPair /* input */,
		      FSTClustInfoPair *clustInfoPair /* input */)
{ 
  UINT4 iiter; /* counter */
  REAL8 errorTol = LAL_REAL4_EPS/1000.0; /* 1.1920e-7/10^3 = 1e-10 */
  REAL8 fmaxT,startFreqT;
  REAL8 sftmp;
  REAL8Vector *searchFreq = NULL;  /* Frequency toward which the veto signal will be shifted. */
  REAL8 vetoStatMin = LAL_REAL4_MAX; /* 3.4e38 */
  REAL8 vetoStat = 0.0; /* veto statistic: ideally follows Chi^2 distribution. */
  REAL8 df = 0.0;       /* degrees of freedom of the chi square distribution */
  REAL8 searchFreqmin;
  REAL8 threshold, thr = 0.5; /* threshold to find "summits = local maxima" of the cluster. */

  FSTControlParameters CP; 


  /* function prototype */ 
  void SummitFinder( LALStatus *, REAL8Vector *output, FSTFstatPair *FaFbPair, REAL8 *threshold );
  void ShiftData( LALStatus *, FSTClustInfo *test,  REAL8 *shift );
  void RearrangeData( LALStatus *, FSTControlParameters *, FSTClustInfoPair * );
  void ComputeVetoStatisticCore( LALStatus *, REAL8 *vetoStat, FSTFstatPair *FaFbPair, FSTControlParameters * );

  INITSTATUS( status, "ComputeVetoStatistic", rcsid );
  ATTATCHSTATUSPTR( status );
  ASSERT ( clustInfoPair, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  
  ASSERT ( FaFbPair, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  
  ASSERT ( vetoStatistic != NULL, status, FSTATSHAPETESTC_ENONULL , FSTATSHAPETESTC_MSGENONULL );  

  /* Coefficients required to compute our veto statistic F stat. */
  CP.amc.a = NULL; /* we do not use this. */
  CP.amc.b = NULL; /* we do not use this. */
  CP.amc.A = clustInfoPair->ObsvCI.amc.A;
  CP.amc.B = clustInfoPair->ObsvCI.amc.B;
  CP.amc.C = clustInfoPair->ObsvCI.amc.C;
  CP.amc.D = clustInfoPair->ObsvCI.amc.D;

  /* Store into searchFreq all the frequency bins where observed F stats exceed hard-coded threshold. */
  threshold = (clustInfoPair->ObsvCI.FSmax) * thr;
  TRY( LALDCreateVector( status->statusPtr, &searchFreq, clustInfoPair->ObsvCI.nData ), status );
  SummitFinder( status->statusPtr, searchFreq, FaFbPair, &threshold );
  BEGINFAIL( status )
    TRY( LALDDestroyVector( status->statusPtr, &searchFreq ), status );
  ENDFAIL( status );

  /* Keep the following variables, as the rhs's would be modified in ShiftData(). 
     We need the lhs's to initialize the rhs's at each iteration. */ 
  fmaxT = clustInfoPair->TestCI.fmax;
  startFreqT = clustInfoPair->TestCI.startFreq;

  /* ---------------------------------------------
   * Main loop over searchFreq[] 
   *
   * ---------------------------------------------*/
  for( iiter = 0; iiter < searchFreq->length; iiter++ ) {  /* Start main loop */

    sftmp = searchFreq->data[iiter];

    /* The following variables can be modified in rearrangeData() */
    /* For this reason, we initialize it at each iteration. */
    CP.nData = clustInfoPair->ObsvCI.nData;
    CP.indexOffset = 0;
    CP.indexStepO = 1;
    CP.indexStepT = 1;

    /* The following variables can be modified in ShiftData() below. */
    /* For this reason, we initialize it at each iteration. */
    clustInfoPair->TestCI.fmax = fmaxT;
    clustInfoPair->TestCI.startFreq = startFreqT;


    /* Shift the test data (veto signal) so that the veto signal has its maximum at searchFreq Hz. */
    /* ShiftData allocates memory for searchFreq */
    ShiftData( status->statusPtr, &(clustInfoPair->TestCI), &sftmp );
    BEGINFAIL( status )
      TRY( LALDDestroyVector( status->statusPtr, &searchFreq ), status );
    ENDFAIL( status );
 
  /* rearrange the data if necessary */
    if(
       ( (clustInfoPair->ObsvCI.nData) != (clustInfoPair->TestCI.nData) ) ||
       ( fabs( clustInfoPair->ObsvCI.startFreq - clustInfoPair->TestCI.startFreq ) > errorTol ) ||
       ( fabs( clustInfoPair->ObsvCI.deltaFreq - clustInfoPair->TestCI.deltaFreq ) > errorTol )
       ) 
      {
	RearrangeData( status->statusPtr, &CP, clustInfoPair );
	BEGINFAIL( status )
	  TRY( LALDDestroyVector( status->statusPtr, &searchFreq ), status );
	ENDFAIL( status );
      }

    ComputeVetoStatisticCore( status->statusPtr, &vetoStat, FaFbPair, &CP );
    BEGINFAIL( status )
      TRY( LALDDestroyVector( status->statusPtr, &searchFreq ), status );
    ENDFAIL( status );

  /* chi square test */
  /* \sum_{n=0}^{nData} 2 F[irec] is our veto statistic which 
     IN THE IDEAL CASE follows chi square distribution with 4*nData-4 degrees of freedom.
  */
    if( vetoStatMin > vetoStat ) {
      /* Find searchFreq where we get minimum veto statistic */ 
      /* keep the frequency, the minimum veto statistic and dof.  */ 
      vetoStatMin = vetoStat;
      df = 4.0*(CP.nData) - 4.0;
      searchFreqmin= sftmp;
    }

    if( lalDebugLevel == 1 ) {
      fprintf(stdout,"%14.9f  %14.9f %14.6g %4d %10.5f \n",
	      clustInfoPair->ObsvCI.fmax, sftmp, clustInfoPair->ObsvCI.FSmax, CP.nData, vetoStat);
    }
  }   /* End main loop: for(iiter=0;iiter < searchFreq->length;iiter++) {... */


    if( lalDebugLevel == 1 ) {
      fprintf(stdout,"%14.9f  %14.9f %14.6g %5.1f %10.5f \n",
	      clustInfoPair->ObsvCI.fmax, searchFreqmin, clustInfoPair->ObsvCI.FSmax, df, vetoStatMin );
    }

  /* This is the final result */
  vetoStatistic->vetoStatistic = vetoStatMin;
  vetoStatistic->dof = df;

  TRY( LALDDestroyVector( status->statusPtr, &searchFreq ), status );

  DETATCHSTATUSPTR( status );
  RETURN( status );
} /* void ComputeVetoStatistic() */


/*-----------------------------------------------------------------*/
                                                                    
/*-----------------------------------------------------------------*/
void 
SummitFinder( LALStatus *status, 
	      REAL8Vector *searchFreq, /*  output. Memory must be pre-allocated */
	      FSTFstatPair *FaFbPair,  /*  input */
	      REAL8 *threshold /* input */)
{
  UINT4 ic,jc; /* counter */

  INITSTATUS( status, "SummitFinder", rcsid );
  ASSERT ( threshold, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  
  ASSERT ( FaFbPair, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  

  jc = 0;
  for( ic = 0; ic < (FaFbPair->freqObsv->length); ic++ ) {
    if( FaFbPair->FaFbObsv->F[ic] >= *threshold ) {
      searchFreq->data[jc] = FaFbPair->freqObsv->data[ic];
      jc++;
    }
  }

  /* jc = number of frequency bins where F stats exceed the threshold = thr*Fmax . */
  ATTATCHSTATUSPTR( status );
  LALDResizeVector( status->statusPtr, &searchFreq, jc );
  BEGINFAIL( status ) 
    TRY(LALDDestroyVector( status->statusPtr, &searchFreq ), status );
  ENDFAIL( status );
  DETATCHSTATUSPTR( status );

  RETURN( status );
} /* void SummitFinder() */


/*-----------------------------------------------------------------*/
                                                                    
/*-----------------------------------------------------------------*/
void 
ShiftData( LALStatus *status, 
	   FSTClustInfo *TestCI, /* input/output */
	   REAL8 *searchFreq     /* input */)
{
  /* This routine just shift the test data (veto signal) so that the frequency 
   * where the maximum amplitude occurs coincide with that of the 
   * observed data to be tested. 
   */ 

  REAL8 startFreqT;
  REAL8 fmaxT;
  REAL8 freqoffset = 0.0;
  INITSTATUS( status, "ShiftData", rcsid );
  ASSERT ( searchFreq, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  


  /* pass the data to the local variables */
  fmaxT  = TestCI->fmax;
  startFreqT = TestCI->startFreq;

  /* We shift the veto signal by searchFreq */
  freqoffset = (*searchFreq) - fmaxT;
  TestCI->startFreq = startFreqT + freqoffset;
  TestCI->fmax  = fmaxT + freqoffset;

  RETURN( status );
} /* void ShiftData() */


/*-----------------------------------------------------------------*/
  
/*-----------------------------------------------------------------*/
void 
RearrangeData( LALStatus *status, 
	       FSTControlParameters *CP, /* output */
	       FSTClustInfoPair *clustInfoPair  /* intput */)
{
  REAL8 myRound(REAL8);

  REAL8 startFreqO,startFreqT,startFreq;
  REAL8 deltaFreqO,deltaFreqT,deltaFreqLCM;
  INT4  nObsv,nTest,nData;
  INT4 indexOffset=0;
  INT4 indexStepO=1,indexStepT=1;
  REAL8 errorTol = LAL_REAL4_EPS;

  INITSTATUS( status, "RearrangeData", rcsid );
  ASSERT ( clustInfoPair, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  


  /* pass the data to the local variables */
  nObsv = clustInfoPair->ObsvCI.nData;
  startFreqO = clustInfoPair->ObsvCI.startFreq;
  deltaFreqO = clustInfoPair->ObsvCI.deltaFreq;

  nTest = clustInfoPair->TestCI.nData;
  startFreqT = clustInfoPair->TestCI.startFreq;
  deltaFreqT = clustInfoPair->TestCI.deltaFreq;


  /* Maybe find LCM of deltaFreqT and deltaFreqO in the future. 
   * But now, for simplicity we just check if the frequency resolutions 
   * are same and use the one of the observed data file. 
   */
  if( fabs( deltaFreqO - deltaFreqT ) > deltaFreqO * errorTol ) {
    fprintf(stderr,"The frequency resolution must be the same\n");
    ABORT( status, FSTATSHAPETESTC_EODDDATA, FSTATSHAPETESTC_MSGEODDDATA );
  }
  deltaFreqLCM = deltaFreqO;

  if( startFreqO - startFreqT != errorTol ) {
    if( fabs( fmod( startFreqO - startFreqT, deltaFreqLCM ) ) > errorTol ) {
      fprintf(stderr,"The starting frequency inconsistency\n");
    ABORT( status, FSTATSHAPETESTC_EODDDATA, FSTATSHAPETESTC_MSGEODDDATA );
    }
  }
  indexOffset = (INT4) myRound( ( startFreqO - startFreqT ) / deltaFreqLCM );
  indexStepO = (INT4) myRound( deltaFreqLCM / deltaFreqO );
  indexStepT = (INT4) myRound( deltaFreqLCM / deltaFreqT );

  
  /* if deltaFreqLCM is really LCM, need to recompute nData. 
   * But we do not do it here. 
   */
  if( indexOffset >= 0 ) {
    startFreq = startFreqO;
    if( nObsv + indexOffset >= nTest ) {
      nData = nTest - indexOffset;
    } else {
      nData = nObsv;
    }
  } else {
    startFreq = startFreqT;
    if( nObsv + indexOffset >= nTest) {
      nData = nTest;
    } else {
      nData = nObsv + indexOffset;
    }
  }


  /* pass the data to the global variables */
  CP->nData = nData;
  CP->indexOffset = indexOffset;
  CP->indexStepO = indexStepO;
  CP->indexStepT = indexStepT;


  /* Check the ranges */
  if ( nData < 0 )  {
    fprintf(stderr,"number of the data points cannot be negative.\n");
    ABORT( status, FSTATSHAPETESTC_EODDDATA, FSTATSHAPETESTC_MSGEODDDATA );
  }
  if ( startFreq < 0 )  {
    fprintf(stderr,"starting frequency cannot be negative.\n");
    ABORT( status, FSTATSHAPETESTC_EODDDATA, FSTATSHAPETESTC_MSGEODDDATA );
  }
  if ( deltaFreqLCM < 0 )  {
    fprintf(stderr,"frequency resolution cannot be negative.\n");
    ABORT( status, FSTATSHAPETESTC_EODDDATA, FSTATSHAPETESTC_MSGEODDDATA );
  }
  if ( indexStepO <= 0 )  {
    fprintf(stderr,"index step should be positive.%d\n",indexStepO);
    ABORT( status, FSTATSHAPETESTC_EODDDATA, FSTATSHAPETESTC_MSGEODDDATA );
  }
  if ( indexStepT <= 0 )  {
    fprintf(stderr,"index step should  be positive.%d\n",indexStepT);
    ABORT( status, FSTATSHAPETESTC_EODDDATA, FSTATSHAPETESTC_MSGEODDDATA );
  }


  RETURN( status );
} /* RearrangeData() */

/*-----------------------------------------------------------------
 * Replacement for C99 round. 
 *-----------------------------------------------------------------*/
REAL8 myRound( REAL8 x )
{
  REAL8 sign=1.0;
  REAL8 roundedValue=0.0;
  REAL8 remainder=0.0;

  if(x<0) sign=-1.0;
  roundedValue= floor(sign*x);
  remainder=sign*x-roundedValue;
  if(remainder>=0.5) 
    roundedValue=roundedValue+1.0;
  roundedValue=sign*roundedValue;

  return roundedValue;
} /* REAL8 myRound() */



/*-----------------------------------------------------------------*/
/*                                                                 */
/*-----------------------------------------------------------------*/

void 
ReadData( LALStatus *status, 
	  FSTFstatPair *FaFbPair, /* output. Memory must be pre-allocated.  */
	  FSTUserInput *cla       /* input = filenames */)
{
  UINT4 irec;
  CHAR buff[500];
  const UINT4 Nheadlines=4;
  INT4 count;

  FILE *fpobsv,*fptest;

  INITSTATUS( status, "ReadData", rcsid );
  ASSERT ( cla, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  

  if( ( fpobsv = fopen(cla->obsvdatafile,"r") ) == NULL ) {
    fprintf(stderr,"File open error in FstatShapeTest: %s\n",cla->obsvdatafile);
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }
  if( ( fptest = fopen(cla->testdatafile,"r") ) == NULL ) {
    fprintf(stderr,"File open error in FstatShapeTest: %s\n",cla->testdatafile);
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }


  /* skip the header */
  /* depend on data format specification */
  for(irec=0;irec<Nheadlines;irec++) {
    fgets(buff,sizeof(buff),fpobsv);
    fgets(buff,sizeof(buff),fptest);
  }


  /* data input begin */
  for( irec = 0; irec < FaFbPair->freqObsv->length; irec++ ) {
    count = fscanf(fpobsv,"%lf %lf %lf %lf %lf %lf",
		   &(FaFbPair->freqObsv->data[irec]),
		   &(FaFbPair->FaFbObsv->Fa[irec].re),
		   &(FaFbPair->FaFbObsv->Fa[irec].im),
		   &(FaFbPair->FaFbObsv->Fb[irec].re),
		   &(FaFbPair->FaFbObsv->Fb[irec].im),
		   &(FaFbPair->FaFbObsv->F[irec])
		   );
    if ( count == EOF ) break;
  }
  if( irec != FaFbPair->freqObsv->length ) {
    fprintf(stderr,"Data read error\n");
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }


  for( irec = 0; irec < FaFbPair->freqTest->length; irec++ ) {
    count = fscanf(fptest,"%lf %lf %lf %lf %lf %lf",
		   &(FaFbPair->freqTest->data[irec]),
		   &(FaFbPair->FaFbTest->Fa[irec].re),
		   &(FaFbPair->FaFbTest->Fa[irec].im),
		   &(FaFbPair->FaFbTest->Fb[irec].re),
		   &(FaFbPair->FaFbTest->Fb[irec].im),
		   &(FaFbPair->FaFbTest->F[irec])
		   );
    if ( count == EOF ) break;
  }
  if( irec != FaFbPair->freqTest->length ) {
    fprintf(stderr,"Data read error\n");
    ABORT( status, FSTATSHAPETESTC_EFILEIO, FSTATSHAPETESTC_MSGEFILIO );
  }

  fclose(fpobsv);
  fclose(fptest);

  RETURN( status );
} /* void ReadData()  */

/*-----------------------------------------------------------------*/
/*                                                                 */
/*-----------------------------------------------------------------*/
void 
ComputeVetoStatisticCore( LALStatus *status, 
			  REAL8 *vetoStatistic, /* outputs */
			  FSTFstatPair *FaFbPair,  /* inputs */
			  FSTControlParameters *CP   /* parameters */)
{ 
  INT4 irec;    /* counter */
  INT4 jindO, jindT;
  INT4 indexOffset;
  INT4 indexStepO, indexStepT;

  REAL8 Acoef,Bcoef,Ccoef,Dcoef;

  REAL8 RFa,IFa,RFb,IFb;
  REAL8 FaSq,FbSq,FaFb;
  REAL8 vetoStatBin; /* veto statistic at each frequency bin. */

  INITSTATUS( status, "ComputeVetoStatisticCore", rcsid );
  ASSERT ( CP, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  
  ASSERT ( FaFbPair, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  
  ASSERT ( vetoStatistic != NULL, status, FSTATSHAPETESTC_ENONULL , FSTATSHAPETESTC_MSGENONULL );  

  indexOffset = CP->indexOffset;
  indexStepO  = CP->indexStepO;
  indexStepT  = CP->indexStepT;

  Acoef = CP->amc.A;
  Bcoef = CP->amc.B;
  Ccoef = CP->amc.C;
  Dcoef = CP->amc.D;


  /* compute veto statistic. */
  /* WARNING 1: 
   *   Here we assume that Fa and Fb are already 
   *  normaized by M (the number of sfts). 
   *  See the pulsargroup document or LALDemod document.
   *
   *  This means that we use 
   *  (veto stat) = 2.0 * (4.0/D)*(B*FaSq + A*FbSq - 2.0*C*FaFb); 
   *  instead of 
   *  (veto stat) = 2.0 * (4.0/(M*D))*(B*FaSq + A*FbSq - 2.0*C*FaFb); 
   */

  /* WARNING 2: 
   *  Here we assume that the one sided noise power spectrum density 
   *  Sh is properly normalized. Namely, the Fa and Fb must be 
   *  multiplied by B*log(2.0) if one uses running median for an 
   *  estimate of Sh. B is the sample bias of the sample median 
   *  estimate.
  */
    
  /* WARNING 3: 
   * We multiply 2. 
   * If FaFbTest were 0, the output will be 2*F.  
   *
   */

  /*----------------------------------------------------------------
   * First we subtract veto signal from real data. 
   * Then compute veto statistic. 
   *----------------------------------------------------------------*/

  *vetoStatistic = 0.0;
  vetoStatBin = 0.0;

  if( indexOffset >= 0 ) {
    for( irec=0; irec < (CP->nData); irec++ ) {
      jindO = irec * indexStepO;
      jindT = irec * indexStepT + indexOffset;

      RFa = FaFbPair->FaFbObsv->Fa[jindO].re - FaFbPair->FaFbTest->Fa[jindT].re;
      IFa = FaFbPair->FaFbObsv->Fa[jindO].im - FaFbPair->FaFbTest->Fa[jindT].im;
      RFb = FaFbPair->FaFbObsv->Fb[jindO].re - FaFbPair->FaFbTest->Fb[jindT].re;
      IFb = FaFbPair->FaFbObsv->Fb[jindO].im - FaFbPair->FaFbTest->Fb[jindT].im;

      FaSq  = RFa*RFa + IFa*IFa;
      FbSq  = RFb*RFb + IFb*IFb;
      FaFb  = RFa*RFb + IFa*IFb;

      vetoStatBin = (4.0/Dcoef) * ( Bcoef*FaSq + Acoef*FbSq - 2.0*Ccoef*FaFb );
      *vetoStatistic += vetoStatBin;

      if( lalDebugLevel == 2 )
	fprintf(stdout,"%22.16g\n", 2.0*vetoStatBin);
    }
  } else {
    for( irec=0; irec < (CP->nData); irec++ ) {
      jindO = irec * indexStepO - indexOffset;
      jindT = irec * indexStepT; 

      RFa = FaFbPair->FaFbObsv->Fa[jindO].re - FaFbPair->FaFbTest->Fa[jindT].re;
      IFa = FaFbPair->FaFbObsv->Fa[jindO].im - FaFbPair->FaFbTest->Fa[jindT].im;
      RFb = FaFbPair->FaFbObsv->Fb[jindO].re - FaFbPair->FaFbTest->Fb[jindT].re;
      IFb = FaFbPair->FaFbObsv->Fb[jindO].im - FaFbPair->FaFbTest->Fb[jindT].im;

      FaSq  = RFa*RFa + IFa*IFa;
      FbSq  = RFb*RFb + IFb*IFb;
      FaFb  = RFa*RFb + IFa*IFb;

      vetoStatBin = (4.0/Dcoef) * ( Bcoef*FaSq + Acoef*FbSq - 2.0*Ccoef*FaFb );
      *vetoStatistic += vetoStatBin;

      if( lalDebugLevel == 2 )
	fprintf( stdout, "%22.16g\n", 2.0*vetoStatBin );
    }
  }

  /* NOTE: Note the factor 2!! 
   * As 2F follows chi-square distribution with 4 degrees of freedom and 
   * it is our veto statistic. 
   */
  *vetoStatistic = (*vetoStatistic) * 2.0;


  RETURN( status );
} /* ComputeVetoStatisticCore() */






/*-----------------------------------------------------------------
 * FUNCTION: Chi-Square cumulative distribution function. 
 *
 * chi2cdfp(x,dof) = integral from 0 to x of chi2pdf(t,dof) dt
 * DOMAIN OF DEFINITION: dof>0 && data >=0.                    
 * AUTHOR: Yousuke Itoh                     
 *-----------------------------------------------------------------*/
void 
LALChi2CDFP( LALStatus *status, 
	     REAL8 *cdfp, /* output chi-square cumulative probability */
	     const REAL8 *data, /* input sample data */ 
	     const REAL8 *dof /* input Degrees of freedom  */)
{
  REAL8 x, a;
  void LALGammaInc( LALStatus *status, REAL8 *output, const REAL8 *input, const REAL8 *param );

  INITSTATUS( status, "LALChi2CDFP", rcsid );

  /* This traps coding errors in the calling routine. */
  ASSERT ( data, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  
  ASSERT ( dof, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  
  ASSERT ( cdfp != NULL, status, FSTATSHAPETESTC_ENONULL , FSTATSHAPETESTC_MSGENONULL );  

  if ( *dof <= 0.0 ) {
    *cdfp = -1.0; /* should be nan ... 0<=CDF<= 1 */
    ABORT (status, FSTATSHAPETESTC_EDOFDOMAIN  , FSTATSHAPETESTC_MSGEDOFDOMAIN );
  } 
  if ( *data < 0.0 ) {
    *cdfp = -1.0; /* should be nan ... 0<=CDF<= 1 */
    ABORT (status, FSTATSHAPETESTC_EDATADOMAIN  , FSTATSHAPETESTC_MSGEDATADOMAIN );
  } 

  if ( *data == 0.0 ) {
    *cdfp = 0.0;
    RETURN( status );
  }

  x = 0.5*(*data);
  a = 0.5*(*dof);

  ATTATCHSTATUSPTR( status );
  TRY( LALGammaInc( status->statusPtr, cdfp, &x, &a ), status);
  DETATCHSTATUSPTR( status );

  /* clean up */
  RETURN( status );
} /* LALChi2CDFP() */



/*-----------------------------------------------------------------
 * FUNCTION: Incomplete Gamma function. 
 * output = gammainc(input,param)
 * DOMAIN OF DEFINITION: input>=0 && param>=0.
 *
 * Following matlab.                                           
 * gammainc(x,a) = 1 ./ gamma(a) .* integral from 0 to x of t^(a-1) exp(-t) dt
 *
 * AUTHOR: Yousuke Itoh                     
 *                     
 *-----------------------------------------------------------------*/
void 
LALGammaInc( LALStatus *status, 
	     REAL8 *output, 
	     const REAL8 *input, 
	     const REAL8 *param )
{
  REAL8 paramMax = 1048576.0; /* = 2^20; */
  REAL8 x, a;
  REAL8 errormax = LAL_REAL8_EPS;
  REAL8 a0, a1, b0, b1, del, sum, gmln;
  REAL8 fac, n, g, gold, ana, anf;


  void LALGammaLn( LALStatus *status, REAL8 *out, const REAL8 *in );

  INITSTATUS( status, "LALGammaInc", rcsid );

  /* This traps coding errors in the calling routine. */
  ASSERT ( input, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  
  ASSERT ( param, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  
  ASSERT ( output != NULL, status, FSTATSHAPETESTC_ENONULL , FSTATSHAPETESTC_MSGENONULL );  

  /* copy inputs into local variables. */
  x = *input;
  a = *param;

  /* Although the MatLab gammainc() returns 1.0 for negative x, */
  if ( x < 0.0 ) {
    *output = 1.0; /* should be nan ... */
    ABORT (status, FSTATSHAPETESTC_EINPUTDOMAIN  , FSTATSHAPETESTC_MSGEINPUTDOMAIN );
  } 
  if ( a < 0.0 ) {
    *output = - 1.0; /* should be nan ... */
    ABORT (status, FSTATSHAPETESTC_EPARAMDOMAIN  , FSTATSHAPETESTC_MSGEPARAMDOMAIN );
  } 
  if ( a == 0.0 ) {
    *output = 1.0; 
    RETURN( status );
  } 
  if ( x == 0.0 ) {
    *output = 0.0; 
    RETURN( status );
  } 


  ATTATCHSTATUSPTR( status );

  if ( a < paramMax ) {
    /* Series expansion for x < a + 1 */
    if ( x < a + 1.0 ) {
      a1 = a;
      sum = 1 / a1;
      del = sum;
      while ( fabs(del) >= 100.0*errormax*fabs(sum) ) {
	a1++;
	del = x * del / a1;
	sum += del;
      }
      TRY( LALGammaLn(status->statusPtr, &gmln, &a), status );
      *output = sum * exp( -x + a*log(x) - gmln );
    } else { 
      /* Continued fraction for x >= a + 1. */
      a0 = 1.0;
      a1 = x;
      b0 = 0.0;
      b1 = a0;
      fac = 1.0;
      n = 1.0;
      g = b1;
      gold = b0;
      while ( fabs(g-gold) >= 100.0*errormax*fabs(g) ) {
	gold = g;
	ana = n - a;
	a0 = (a1 + a0*ana) * fac;
	b0 = (b1 + b0*ana) * fac;
	anf = n*fac;
	a1 = x*a0 + anf*a1;
	b1 = x*b0 + anf*b1;
	fac = 1/a1;
	g = b1*fac;
	n++;
      }
      TRY( LALGammaLn(status->statusPtr, &gmln, &a), status );
      *output = 1.0 - exp( -x + a*log(x) - gmln ) * g;
    }  /*   if(x < a+1.0) */
  } else {
    /* a >= paramMax */
    x = paramMax - 1.0/3.0 + sqrt( paramMax / a ) * (x - ( a - 1.0 / 3.0 ) );
    if ( x < 0.0 ) {
      x = 0.0;
    }
    a = paramMax;
    TRY( LALGammaInc(status->statusPtr, output, &x, &a), status );
  } /*   if(a<paramMax)  */

  DETATCHSTATUSPTR( status );

  /* clean up */
  RETURN( status );
} /* LALGammaInc() */





/*-----------------------------------------------------------------
 * FUNCTION: output = Log(Gamma(input))
 * DOMAIN OF DEFINITION: input>=0.
 *
 *
 * Following matlab.                                           
 *
 * Matlab says 
 *%This is based on a FORTRAN program by W. J. Cody,
 *%   Argonne National Laboratory, NETLIB/SPECFUN, June 16, 1988.
 *%
 *% References:
 *%
 *%  1) W. J. Cody and K. E. Hillstrom, 'Chebyshev Approximations for
 *%     the Natural Logarithm of the Gamma Function,' Math. Comp. 21,
 *%     1967, pp. 198-203.
 *%
 *%  2) K. E. Hillstrom, ANL/AMD Program ANLC366S, DGAMMA/DLGAMA, May,
 *%     1969.
 *% 
 *%  3) Hart, Et. Al., Computer Approximations, Wiley and sons, New
 *%     York, 1968.
 *
 *
 * AUTHOR: Yousuke Itoh                     
 *-----------------------------------------------------------------*/
void 
LALGammaLn( LALStatus *status, 
	    REAL8 *output, 
	    const REAL8 *input )
{
  REAL8 x;
  REAL8 xden, xnum, xm1, xm2, xm4;
  REAL8 corr, r, xsq, spi;
  INT4 ic;


  REAL8 d1 = -5.772156649015328605195174e-1;
  REAL8 p1[] = {4.945235359296727046734888e0, 2.018112620856775083915565e2, 
		2.290838373831346393026739e3, 1.131967205903380828685045e4, 
		2.855724635671635335736389e4, 3.848496228443793359990269e4, 
		2.637748787624195437963534e4, 7.225813979700288197698961e3};
  REAL8 q1[] = {6.748212550303777196073036e1, 1.113332393857199323513008e3, 
		7.738757056935398733233834e3, 2.763987074403340708898585e4, 
		5.499310206226157329794414e4, 6.161122180066002127833352e4, 
		3.635127591501940507276287e4, 8.785536302431013170870835e3};
  REAL8 d2 = 4.227843350984671393993777e-1;
  REAL8 p2[] = {4.974607845568932035012064e0, 5.424138599891070494101986e2, 
		1.550693864978364947665077e4, 1.847932904445632425417223e5, 
		1.088204769468828767498470e6, 3.338152967987029735917223e6, 
		5.106661678927352456275255e6, 3.074109054850539556250927e6};
  REAL8 q2[] = {1.830328399370592604055942e2, 7.765049321445005871323047e3, 
		1.331903827966074194402448e5, 1.136705821321969608938755e6, 
		5.267964117437946917577538e6, 1.346701454311101692290052e7, 
		1.782736530353274213975932e7, 9.533095591844353613395747e6};
  REAL8 d4 = 1.791759469228055000094023e0;
  REAL8 p4[] = {1.474502166059939948905062e4, 2.426813369486704502836312e6, 
		1.214755574045093227939592e8, 2.663432449630976949898078e9, 
		2.940378956634553899906876e10, 1.702665737765398868392998e11, 
		4.926125793377430887588120e11, 5.606251856223951465078242e11};
  REAL8 q4[] = {2.690530175870899333379843e3, 6.393885654300092398984238e5, 
		4.135599930241388052042842e7, 1.120872109616147941376570e9, 
		1.488613728678813811542398e10, 1.016803586272438228077304e11, 
		3.417476345507377132798597e11, 4.463158187419713286462081e11};
  REAL8 c[] = {-1.910444077728e-03, 8.4171387781295e-04, 
	       -5.952379913043012e-04, 7.93650793500350248e-04, 
	       -2.777777777777681622553e-03, 8.333333333333333331554247e-02, 
	       5.7083835261e-03};

  INITSTATUS( status, "LALGammaLn", rcsid );


  /* This traps coding errors in the calling routine. */
  ASSERT ( input, status, FSTATSHAPETESTC_ENULL , FSTATSHAPETESTC_MSGENULL );  
  ASSERT ( output != NULL, status, FSTATSHAPETESTC_ENONULL , FSTATSHAPETESTC_MSGENONULL );  

  x = *input;

  /* When x=0, the output should be ln(0)=-inf. I avoid it ... */ 
  if ( x <= 0 ) {
    ABORT (status, FSTATSHAPETESTC_EINPUTDOMAIN  , FSTATSHAPETESTC_MSGEINPUTDOMAIN );
  }
  *output = x;

  /* 0 < x <= LAL_REAL8_EPS = 2.2204e-16 */
  if ( x <= LAL_REAL8_EPS ) {
    *output = - log(x);
    RETURN( status );
  }

  /* LAL_REAL8_EPS <= x <= 0.5 */
  if ( (x > LAL_REAL8_EPS) && ( x <= 0.5 ) ) {
    xden = 1.0;
    xnum = 0.0;
    for(ic=0;ic<8;ic++) {
      xnum = xnum*x + p1[ic];
      xden = xden*x + q1[ic];
    }
    *output = - log(x) + (x * ( d1 + x * (xnum / xden) ));
    RETURN( status );
  }

  /* 0.5 < x <= 0.6796875 */
  if ( (x > 0.5) && ( x <= 0.6796875 ) ) {
    xm1 = (x - 0.5) - 0.5;
    xden = 1.0;
    xnum = 0.0;
    for(ic=0;ic<8;ic++) {
      xnum = xnum*xm1 + p2[ic];
      xden = xden*xm1 + q2[ic];
    }
    *output = - log(x) + xm1 * ( d2 + xm1 * (xnum / xden) );
    RETURN( status );
  }


  /* 0.6796875 < x <= 1.5 */
  if ( (x > 0.6796875) && ( x <= 1.5 ) ) {
    xm1 = (x - 0.5) - 0.5;
    xden = 1.0;
    xnum = 0.0;
    for(ic=0;ic<8;ic++) {
      xnum = xnum*xm1 + p1[ic];
      xden = xden*xm1 + q1[ic];
    }
    *output = xm1 * ( d1 + xm1 * (xnum / xden) );
    RETURN( status );
  }


  /* 1.5 < x <= 4.0 */
  if ( (x > 1.5) && ( x <= 4.0 ) ) {
    xm2 = x - 2.0;
    xden = 1.0;
    xnum = 0.0;
    for(ic=0;ic<8;ic++) {
      xnum = xnum*xm2 + p2[ic];
      xden = xden*xm2 + q2[ic];
    }
    *output = xm2 * ( d2 + xm2 * (xnum / xden) );
    RETURN( status );
  }


  /* 4.0 < x <= 12.0 */
  if ( (x > 4.0) && ( x <= 12.0 ) ) {
    xm4 = x - 4.0;
    xden = - 1.0;
    xnum = 0.0;
    for(ic=0;ic<8;ic++) {
      xnum = xnum*xm4 + p4[ic];
      xden = xden*xm4 + q4[ic];
    }
    *output = d4 + xm4 * (xnum / xden);
    RETURN( status );
  }


  /* 12.0 < x  */
  if ( (x > 12.0) ) {
    r = c[6];
    xsq = x * x;
    for(ic=0;ic<6;ic++) {
      r = r/xsq + c[ic];
    }
    r = r/x;
    corr = log(x);
    spi = 0.9189385332046727417803297;
    *output = r + spi - 0.5*corr + x*(corr - 1.0);
    RETURN( status );
  }

} /* LALGammaLn() */




