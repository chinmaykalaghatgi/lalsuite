/*-----------------------------------------------------------------------
 *
 * File Name: TimeFreq.h
 *
 * Author: 
 *
 * Revision: $Id$
 *
 *-----------------------------------------------------------------------
 *
 * NAME
 * TimeFreq.h
 *
 * SYNOPSIS
 * #include "TimeFreq.h"
 *
 * DESCRIPTION 
 * Header file for the TFR package (computation of time-frequency
 * representation for the detection of gravitational waves from
 * unmodeled astrophysical sources)
 *
 * DIAGNOSTICS
 * ??
 *----------------------------------------------------------------------- */

 /*
 * 2. include-loop protection (see below). Note the naming convention!
 */

#ifndef _TIMEFREQ_H
#define _TIMEFREQ_H

/*
 * 3. Includes. This header may include others; if so, they go immediately 
 *    after include-loop protection. Includes should appear in the following 
 *    order: 
 *    a. Standard library includes
 *    b. LDAS includes
 *    c. LAL includes
 *    Includes should be double-guarded!
 */

#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include "LALStdlib.h"
#include "LALConstants.h"
#include "RealFFT.h"
#include "AVFactories.h"

NRCSID (TIMEFREQH, "$Id$");

/*
 * 5. Macros. But, note that macros are deprecated. 
 */

/*
 * 8. Structure, enum, union, etc., typdefs.
 */

#define CREATETFR_ENULL 1
#define CREATETFR_ENNUL 2
#define CREATETFR_EFROW 4
#define CREATETFR_ETCOL 8
#define CREATETFR_EMALL 16

#define CREATETFR_MSGENULL "Null pointer" /* ENULL */
#define CREATETFR_MSGENNUL "Non-null pointer" /* ENNUL */
#define CREATETFR_MSGEFROW "Illegal number of freq bins" /* EFROW */
#define CREATETFR_MSGETCOL "Illegal number of time instants" /* ETCOL */
#define CREATETFR_MSGEMALL "Malloc failure" /* EMALL */

#define DESTROYTFR_ENULL 1

#define DESTROYTFR_MSGENULL "Null pointer" /* ENULL */

#define CREATETFP_ENULL 1
#define CREATETFP_ENNUL 2
#define CREATETFP_EMALL 4
#define CREATETFP_EWSIZ 8
#define CREATETFP_ETYPE 16

#define CREATETFP_MSGENULL "Null pointer" /* ENULL */
#define CREATETFP_MSGENNUL "Non-null pointer" /* ENNUL */
#define CREATETFP_MSGEMALL "Malloc failure" /* EMALLOC */
#define CREATETFP_MSGEWSIZ "Invalid window length" /* ESIZ */
#define CREATETFP_MSGETYPE "Unknown TFR type" /* ETYPE */

#define DESTROYTFP_ENULL 1
#define DESTROYTFP_ETYPE 2

#define DESTROYTFP_MSGENULL "Null pointer" /* ENULL */
#define DESTROYTFP_MSGETYPE "Unknown TFR type" /* ETYPE */

#define TFR_ENULL 1
#define TFR_ENAME 2
#define TFR_EFROW 4
#define TFR_EWSIZ 8
#define TFR_ESAME 16
#define TFR_EBADT 32

#define TFR_MSGENULL "Null pointer" /* ENULL */
#define TFR_MSGENAME "TFR type mismatched" /* ENAME */
#define TFR_MSGEFROW "Invalid number of freq bins" /* EFROW */
#define TFR_MSGEWSIZ "Invalid window length" /* EWSIZ */
#define TFR_MSGESAME "Input/Output data vectors are the same" /* ESAME */
#define TFR_MSGEBADT "Invalid time instant" /* ETBAD */

/* Available TFR types */

#define TIME_FREQ_REP_NAMELIST {"Undefined","Spectrogram","WignerVille", "PSWignerVille","RSpectrogram"} 

typedef enum tagTimeFreqRepType {
Undefined, Spectrogram, WignerVille, PSWignerVille, RSpectrogram
} TimeFreqRepType; 

/* Time-Frequency Representation structure */

typedef struct tagTimeFreqRep {
  TimeFreqRepType type;             /* type of the TFR */
  INT4 fRow;                        /* number of freq bins in the TFR matrix */
  INT4 tCol;                        /* number of time bins in the TFR matrix */
  REAL4 *freqBin;	            /* freqs for each row of the matrix */
  INT4 *timeInstant;                /* time instants for each column of the TFR */
  REAL4 **map;                      /* TFR */
} TimeFreqRep;

/* TFR parameter structure */

typedef struct tagTimeFreqParam {
  TimeFreqRepType type;                   /* type of the TFR */
  REAL4Vector *windowT;                   /* (Sp, Rsp and Pswv) Window */
  REAL4Vector *windowF;                   /* (Pswv) Window */
} TimeFreqParam;

/* For memory allocation of the TFR and parameter structure */

typedef struct tagCreateTimeFreqIn {
  TimeFreqRepType type;             /* type of the TFR */
  INT4 fRow;                        /* number of freq bins in the TFR matrix */
  INT4 tCol;                        /* number of time bins in the TFR matrix */
  INT4 wlengthT;                    /* (Sp, Pswv and Rsp) Window length */
  INT4 wlengthF;                    /* (Pswv) Window */
} CreateTimeFreqIn;
 
/*
 * 9. Functions Declarations (i.e., prototypes).
 */

void CreateTimeFreqRep (Status*, TimeFreqRep**, CreateTimeFreqIn*);
void CreateTimeFreqParam (Status*, TimeFreqParam**, CreateTimeFreqIn*);
void DestroyTimeFreqRep (Status*, TimeFreqRep**);
void DestroyTimeFreqParam (Status*, TimeFreqParam**);
void TfrSp (Status*, REAL4Vector*, TimeFreqRep*, TimeFreqParam*);
void TfrWv (Status*, REAL4Vector*, TimeFreqRep*, TimeFreqParam*);
void TfrPswv (Status*, REAL4Vector*, TimeFreqRep*, TimeFreqParam*);
void TfrRsp (Status*, REAL4Vector*, TimeFreqRep*, TimeFreqParam*);
void Dwindow (Status*, REAL4Vector*, REAL4Vector*);

#endif
