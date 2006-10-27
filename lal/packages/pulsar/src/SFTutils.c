/*
 * Copyright (C) 2005 Reinhard Prix
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with with program; see the file COPYING. If not, write to the 
 *  Free Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *  MA  02111-1307  USA
 */

/**
 * \author Reinhard Prix, Badri Krishnan
 * \date 2005
 * \file 
 * \ingroup SFTfileIO
 * \brief Utility functions for handling of SFTtype and SFTVector's.
 *
 * $Id$
 *
 */

/*---------- INCLUDES ----------*/
#include <gsl/gsl_sort_double.h>

#include <lal/AVFactories.h>
#include <lal/SeqFactories.h>
#include <lal/NormalizeSFTRngMed.h>

#include "SFTutils.h"

NRCSID( SFTUTILSC, "$Id$" );

/*---------- DEFINES ----------*/
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#define MAX(x,y) ((x) > (y) ? (x) : (y))

#define TRUE (1==1)
#define FALSE (1==0)

/*----- SWITCHES -----*/

/*---------- internal types ----------*/

/*---------- Global variables ----------*/
/* empty struct initializers */
static LALStatus empty_status;

const SFTtype empty_SFTtype;
const SFTVector empty_SFTVector;
const PSDVector empty_PSDVector;
const MultiSFTVector empty_MultiSFTVector;
const MultiPSDVector empty_MultiPSDVector;
const MultiNoiseWeights empty_MultiNoiseWeights;
const MultiREAL4TimeSeries empty_MultiREAL4TimeSeries;
const LIGOTimeGPSVector empty_LIGOTimeGPSVector;
const MultiLIGOTimeGPSVector empty_MultiLIGOTimeGPSVector;
const LALStringVector empty_LALStringVector;

/*---------- internal prototypes ----------*/
static int create_LISA_detectors (LALDetector *Detector, CHAR detIndex );

/*==================== FUNCTION DEFINITIONS ====================*/

/** Create one SFT-struct. Allows for numBins == 0.
 */
void
LALCreateSFTtype (LALStatus *status, 
		  SFTtype **output, 	/**< [out] allocated SFT-struct */
		  UINT4 numBins)	/**< number of frequency-bins */
{
  SFTtype *sft = NULL;

  INITSTATUS( status, "LALCreateSFTtype", SFTUTILSC);
  ATTATCHSTATUSPTR( status );

  ASSERT (output != NULL, status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);
  ASSERT (*output == NULL, status, SFTUTILS_ENONULL,  SFTUTILS_MSGENONULL);

  sft = LALCalloc (1, sizeof(*sft) );
  if (sft == NULL) {
    ABORT (status, SFTUTILS_EMEM, SFTUTILS_MSGEMEM);
  }

  if ( numBins )
    {
      LALCCreateVector (status->statusPtr, &(sft->data), numBins);
      BEGINFAIL (status) { 
	LALFree (sft);
      } ENDFAIL (status);
    }
  else
    sft->data = NULL;	/* no data, just header */

  *output = sft;

  DETATCHSTATUSPTR( status );
  RETURN (status);

} /* LALCreateSFTtype() */


/** Create a whole vector of \c numSFT SFTs with \c SFTlen frequency-bins 
 */
void
LALCreateSFTVector (LALStatus *status, 
		    SFTVector **output, /**< [out] allocated SFT-vector */
		    UINT4 numSFTs, 	/**< number of SFTs */
		    UINT4 numBins)	/**< number of frequency-bins per SFT */
{
  UINT4 iSFT, j;
  SFTVector *vect;	/* vector to be returned */

  INITSTATUS( status, "LALCreateSFTVector", SFTUTILSC);
  ATTATCHSTATUSPTR( status );

  ASSERT (output != NULL, status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);
  ASSERT (*output == NULL, status, SFTUTILS_ENONULL,  SFTUTILS_MSGENONULL);

  vect = LALCalloc (1, sizeof(*vect) );
  if (vect == NULL) {
    ABORT (status, SFTUTILS_EMEM, SFTUTILS_MSGEMEM);
  }

  vect->length = numSFTs;

  vect->data = LALCalloc (1, numSFTs * sizeof ( *vect->data ) );
  if (vect->data == NULL) {
    LALFree (vect);
    ABORT (status, SFTUTILS_EMEM, SFTUTILS_MSGEMEM);
  }

  for (iSFT=0; iSFT < numSFTs; iSFT ++)
    {
      COMPLEX8Vector *data = NULL;

      /* allow SFTs with 0 bins: only header */
      if ( numBins )
	{
	  LALCCreateVector (status->statusPtr, &data , numBins);
	  BEGINFAIL (status)  /* crap, we have to de-allocate as far as we got so far... */
	    {
	      for (j=0; j<iSFT; j++)
		LALCDestroyVector (status->statusPtr, (COMPLEX8Vector**)&(vect->data[j].data) );
	      LALFree (vect->data);
	      LALFree (vect);
	    } ENDFAIL (status);
	}
      
      vect->data[iSFT].data = data;

    } /* for iSFT < numSFTs */

  *output = vect;

  DETATCHSTATUSPTR( status );
  RETURN (status);

} /* LALCreateSFTVector() */




void LALCreateMultiSFTVector ( LALStatus *status,
			       MultiSFTVector **out,  /**< [out] multi sft vector created */
			       UINT4 length,          /**< number of sft data points */
			       UINT4Vector *numsft    /**< number of sfts in each sftvect */
			       )     
{

  UINT4 k, j, numifo;
  MultiSFTVector *multSFTVec=NULL;
  
  INITSTATUS (status, "LALCreateMultiSFTs", SFTUTILSC);
  ATTATCHSTATUSPTR (status); 

  ASSERT ( out, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL );
  ASSERT ( *out == NULL, status, SFTUTILS_ENONULL, SFTUTILS_MSGENONULL );  
  ASSERT ( length, status, SFTUTILS_EINPUT, SFTUTILS_MSGEINPUT );
  ASSERT ( numsft, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL );
  ASSERT ( numsft->length > 0, status, SFTUTILS_EINPUT, SFTUTILS_MSGEINPUT );
  ASSERT ( numsft->data, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL );

  if ( (multSFTVec = (MultiSFTVector *)LALCalloc(1, sizeof(MultiSFTVector))) == NULL){
    ABORT ( status, SFTUTILS_EMEM, SFTUTILS_MSGEMEM );
  }

  numifo = numsft->length;
  multSFTVec->length = numifo;

  if ( (multSFTVec->data = (SFTVector **)LALCalloc( 1, numifo*sizeof(SFTVector *))) == NULL) {
    ABORT ( status, SFTUTILS_EMEM, SFTUTILS_MSGEMEM );
  }
  
  for ( k = 0; k < numifo; k++) {
    LALCreateSFTVector (status->statusPtr, multSFTVec->data + k, numsft->data[k], length);    
      BEGINFAIL ( status ) {
	for ( j = 0; j < k-1; j++)
	  LALDestroySFTVector ( status->statusPtr, multSFTVec->data + j );
	LALFree( multSFTVec->data);
	LALFree( multSFTVec);
      } ENDFAIL(status);
  } /* loop over ifos */

  *out = multSFTVec;

  DETATCHSTATUSPTR (status);
  RETURN(status);

} /* LALLoadMultiSFTs() */






/** Destroy an SFT-struct.
 */
void
LALDestroySFTtype (LALStatus *status, 
		   SFTtype **sft)	/**< SFT-struct to free */
{

  INITSTATUS( status, "LALDestroySFTtype", SFTUTILSC);
  ATTATCHSTATUSPTR (status);

  ASSERT (sft != NULL, status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);

  if (*sft == NULL) 
    goto finished;

  if ( (*sft)->data )
    {
      if ( (*sft)->data->data )
	LALFree ( (*sft)->data->data );
      LALFree ( (*sft)->data );
    }
  
  LALFree ( (*sft) );

  *sft = NULL;

 finished:
  DETATCHSTATUSPTR( status );
  RETURN (status);

} /* LALDestroySFTtype() */


/** Destroy an SFT-vector
 */
void
LALDestroySFTVector (LALStatus *status, 
		     SFTVector **vect)	/**< the SFT-vector to free */
{
  UINT4 i;
  SFTtype *sft;

  INITSTATUS( status, "LALDestroySFTVector", SFTUTILSC);
  ATTATCHSTATUSPTR( status );

  ASSERT (vect != NULL, status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);
  
  if ( *vect == NULL )	/* nothing to be done */
    goto finished;
  
  for (i=0; i < (*vect)->length; i++)
    {
      sft = &( (*vect)->data[i] );
      if ( sft->data )
	{
	  if ( sft->data->data )
	    LALFree ( sft->data->data );
	  LALFree ( sft->data );
	}
    }

  LALFree ( (*vect)->data );
  LALFree ( *vect );

  *vect = NULL;
  
 finished:
  DETATCHSTATUSPTR( status );
  RETURN (status);

} /* LALDestroySFTVector() */




/** Destroy a PSD-vector
 */
void
LALDestroyPSDVector (LALStatus *status, 
		     PSDVector **vect)	/**< the SFT-vector to free */
{
  UINT4 i;
  REAL8FrequencySeries *psd;

  INITSTATUS( status, "LALDestroyPSDVector", SFTUTILSC);
  ATTATCHSTATUSPTR( status );

  ASSERT (vect != NULL, status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);

  if ( *vect == NULL )	/* nothing to be done */
    goto finished;

  for (i=0; i < (*vect)->length; i++)
    {
      psd = &( (*vect)->data[i] );
      if ( psd->data )
	{
	  if ( psd->data->data )
	    LALFree ( psd->data->data );
	  LALFree ( psd->data );
	}
    }

  LALFree ( (*vect)->data );
  LALFree ( *vect );

  *vect = NULL;

 finished:
  DETATCHSTATUSPTR( status );
  RETURN (status);

} /* LALDestroyPSDVector() */


/** Destroy a multi SFT-vector
 */
void
LALDestroyMultiSFTVector (LALStatus *status, 
		          MultiSFTVector **multvect)	/**< the SFT-vector to free */
{
  UINT4 i;

  INITSTATUS( status, "LALDestroyMultiSFTVector", SFTUTILSC);
  ATTATCHSTATUSPTR( status );

  ASSERT (multvect != NULL, status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);

  if ( *multvect == NULL )	/* nothing to be done */
    goto finished;

  for ( i = 0; i < (*multvect)->length; i++)
      LALDestroySFTVector( status->statusPtr, (*multvect)->data + i);
  
  LALFree( (*multvect)->data );
  LALFree( *multvect );
  
  *multvect = NULL;

 finished:
  DETATCHSTATUSPTR( status );
  RETURN (status);

} /* LALDestroyMultiSFTVector() */



/** Destroy a multi PSD-vector
 */
void
LALDestroyMultiPSDVector (LALStatus *status, 
		          MultiPSDVector **multvect)	/**< the SFT-vector to free */
{
  UINT4 i;

  INITSTATUS( status, "LALDestroyMultiPSDVector", SFTUTILSC);
  ATTATCHSTATUSPTR( status );

  ASSERT (multvect != NULL, status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);

  if ( *multvect == NULL )
    goto finished;

  for ( i = 0; i < (*multvect)->length; i++) {    
    LALDestroyPSDVector( status->statusPtr, (*multvect)->data + i);
  }

  LALFree( (*multvect)->data );
  LALFree( *multvect );
  
  *multvect = NULL;
  
 finished:
  DETATCHSTATUSPTR( status );
  RETURN (status);

} /* LALDestroySFTVector() */



/** Copy an entire SFT-type into another. 
 * We require the destination-SFT to have a NULL data-entry, as the  
 * corresponding data-vector will be allocated here and copied into
 *
 * Note: the source-SFT is allowed to have a NULL data-entry,
 * in which case only the header is copied.
 */
void
LALCopySFT (LALStatus *status, 
	    SFTtype *dest, 	/**< [out] copied SFT (needs to be allocated already) */
	    const SFTtype *src)	/**< input-SFT to be copied */
{


  INITSTATUS( status, "LALCopySFT", SFTUTILSC);
  ATTATCHSTATUSPTR ( status );

  ASSERT (dest,  status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);
  ASSERT (dest->data == NULL, status, SFTUTILS_ENONULL, SFTUTILS_MSGENONULL );
  ASSERT (src, status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);

  /* copy complete head (including data-pointer, but this will be separately alloc'ed and copied in the next step) */
  memcpy ( dest, src, sizeof(*dest) );

  /* copy data (if there's any )*/
  if ( src->data )
    {
      UINT4 numBins = src->data->length;      
      if ( (dest->data = XLALCreateCOMPLEX8Vector ( numBins )) == NULL ) {
	ABORT ( status, SFTUTILS_EMEM, SFTUTILS_MSGEMEM ); 
      }
      memcpy (dest->data->data, src->data->data, numBins * sizeof (src->data->data[0]));
    }

  DETATCHSTATUSPTR (status);
  RETURN (status);

} /* LALCopySFT() */


/** Concatenate two SFT-vectors to a new one.
 *
 * Note: the input-vectors are *copied* completely, so they can safely
 * be free'ed after concatenation. 
 */
void
LALConcatSFTVectors (LALStatus *status,
		     SFTVector **outVect,	/**< [out] concatenated SFT-vector */
		     const SFTVector *inVect1,	/**< input-vector 1 */
		     const SFTVector *inVect2 ) /**< input-vector 2 */
{
  UINT4 numBins1, numBins2;
  UINT4 numSFTs1, numSFTs2;
  UINT4 i;
  SFTVector *ret = NULL;

  INITSTATUS( status, "LALConcatSFTVector", SFTUTILSC);
  ATTATCHSTATUSPTR (status); 

  ASSERT (outVect,  status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);
  ASSERT ( *outVect == NULL,  status, SFTUTILS_ENONULL,  SFTUTILS_MSGENONULL);
  ASSERT (inVect1 && inVect1->data, status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);
  ASSERT (inVect2 && inVect2->data, status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);


  /* NOTE: we allow SFT-entries with NULL data-entries, i.e. carrying only the headers */
  if ( inVect1->data[0].data )
    numBins1 = inVect1->data[0].data->length;
  else
    numBins1 = 0;
  if ( inVect2->data[0].data )
    numBins2 = inVect2->data[0].data->length;
  else
    numBins2 = 0;

  if ( numBins1 != numBins2 )
    {
      LALPrintError ("\nERROR: the SFT-vectors must have the same number of frequency-bins!\n\n");
      ABORT ( status, SFTUTILS_EINPUT,  SFTUTILS_MSGEINPUT);
    }
  
  numSFTs1 = inVect1 -> length;
  numSFTs2 = inVect2 -> length;

  TRY ( LALCreateSFTVector ( status->statusPtr, &ret, numSFTs1 + numSFTs2, numBins1 ), status );
  
  /* copy the *complete* SFTs (header+ data !) one-by-one */
  for (i=0; i < numSFTs1; i ++)
    {
      LALCopySFT ( status->statusPtr, &(ret->data[i]), &(inVect1->data[i]) );
      BEGINFAIL ( status ) {
	LALDestroySFTVector ( status->statusPtr, &ret );
      } ENDFAIL(status);
    } /* for i < numSFTs1 */

  for (i=0; i < numSFTs2; i ++)
    {
      LALCopySFT ( status->statusPtr, &(ret->data[numSFTs1 + i]), &(inVect2->data[i]) );
      BEGINFAIL ( status ) {
	LALDestroySFTVector ( status->statusPtr, &ret );
      } ENDFAIL(status);
    } /* for i < numSFTs1 */

  (*outVect) = ret;
  
  DETATCHSTATUSPTR (status); 
  RETURN (status);

} /* LALConcatSFTVectors() */



/** Append the given SFTtype to the SFT-vector (no SFT-specific checks are done!) */
void
LALAppendSFT2Vector (LALStatus *status,
		     SFTVector *vect,		/**< destinatino SFTVector to append to */
		     const SFTtype *sft)	/**< the SFT to append */
{
  UINT4 oldlen;
  INITSTATUS( status, "LALAppendSFT2Vector", SFTUTILSC);
  ATTATCHSTATUSPTR (status); 

  ASSERT ( sft, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL );
  ASSERT ( vect, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL );

  oldlen = vect->length;
  
  if ( (vect->data = LALRealloc ( vect->data, (oldlen + 1)*sizeof( *vect->data ) )) == NULL ) {
    ABORT ( status, SFTUTILS_EMEM, SFTUTILS_MSGEMEM ); 
  }
  memset ( &(vect->data[oldlen]), 0, sizeof( vect->data[0] ) );
  vect->length ++;

  TRY ( LALCopySFT( status->statusPtr, &vect->data[oldlen], sft ), status);

  DETATCHSTATUSPTR(status);
  RETURN(status);

} /* LALAppendSFT2Vector() */



/** Append the given string to the string-vector */
void
LALAppendString2Vector (LALStatus *status,
			LALStringVector *vect,	/**< destination vector to append to */
			const CHAR *string)	/**< string to append */
{
  UINT4 oldlen;
  INITSTATUS( status, "LALAppendString2Vector", SFTUTILSC);

  ASSERT ( string, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL );
  ASSERT ( vect, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL );

  oldlen = vect->length;
  
  if ( (vect->data = LALRealloc ( vect->data, (oldlen + 1)*sizeof( *vect->data ) )) == NULL ) {
    ABORT ( status, SFTUTILS_EMEM, SFTUTILS_MSGEMEM ); 
  }
  vect->length ++;

  if ( (vect->data[oldlen] = LALCalloc(1, strlen(string) + 1 )) == NULL ) {
    ABORT ( status, SFTUTILS_EMEM, SFTUTILS_MSGEMEM ); 
  }

  strcpy ( vect->data[oldlen], string );
  
  RETURN(status);

} /* LALAppendString2Vector() */



/** XLAL-interface: Free a string-vector ;) */
void
XLALDestroyStringVector ( LALStringVector *vect )
{
  UINT4 i;

  if ( !vect )
    return;
  
  if ( vect->data )
    {
      for ( i=0; i < vect->length; i++ )
	{
	  if ( vect->data[i] )
	    LALFree ( vect->data[i] );
	}
      
      LALFree ( vect->data );
    }
  
  LALFree ( vect );
  
  return;

} /* XLALDestroyStringVector() */

/** LAL-interface: Free a string-vector ;) */
void
LALDestroyStringVector ( LALStatus *status,
			 LALStringVector **vect )
{
  INITSTATUS( status, "LALDestroyStringVector", SFTUTILSC);

  ASSERT ( vect, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL );
  
  if ( (*vect) ) 
    {
      XLALDestroyStringVector ( (*vect) );
      (*vect) = NULL;
    }

  RETURN(status);

} /* LALDestroyStringVector() */



/** Allocate a LIGOTimeGPSVector */
LIGOTimeGPSVector *
XLALCreateTimestampVector (UINT4 length)
{
  LIGOTimeGPSVector *out = NULL;

  out = LALCalloc (1, sizeof(LIGOTimeGPSVector));
  if (out == NULL) 
    XLAL_ERROR_NULL ( "XLALCreateTimestampVector", XLAL_ENOMEM );

  out->length = length;
  out->data = LALCalloc (1, length * sizeof(LIGOTimeGPS));
  if (out->data == NULL) {
    LALFree (out);
    XLAL_ERROR_NULL ( "XLALCreateTimestampVector", XLAL_ENOMEM );
  }

  return out;

} /* XLALCreateTimestampVector() */



/** LAL-interface: Allocate a LIGOTimeGPSVector */
void
LALCreateTimestampVector (LALStatus *status, 
			  LIGOTimeGPSVector **vect, 	/**< [out] allocated timestamp-vector  */
			  UINT4 length)			/**< number of elements */
{
  LIGOTimeGPSVector *out = NULL;

  INITSTATUS( status, "LALCreateTimestampVector", SFTUTILSC);

  ASSERT (vect != NULL, status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);
  ASSERT (*vect == NULL, status, SFTUTILS_ENONULL,  SFTUTILS_MSGENONULL);

  if ( (out = XLALCreateTimestampVector( length )) == NULL ) {
    XLALClearErrno();
    ABORT (status,  SFTUTILS_EMEM,  SFTUTILS_MSGEMEM);
  }

  *vect = out;

  RETURN (status);
  
} /* LALCreateTimestampVector() */


/** De-allocate a LIGOTimeGPSVector */
void
XLALDestroyTimestampVector ( LIGOTimeGPSVector *vect)
{
  if ( !vect )
    return;

  LALFree ( vect->data );
  LALFree ( vect );
  
  return;
  
} /* XLALDestroyTimestampVector() */


/** De-allocate a LIGOTimeGPSVector
 */
void
LALDestroyTimestampVector (LALStatus *status, 
			   LIGOTimeGPSVector **vect)	/**< timestamps-vector to be freed */
{
  INITSTATUS( status, "LALDestroyTimestampVector", SFTUTILSC);

  ASSERT (vect != NULL, status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);

  if ( *vect == NULL ) 
    goto finished;

  XLALDestroyTimestampVector ( (*vect) );
  
  (*vect) = NULL;

 finished:
  RETURN (status);
  
} /* LALDestroyTimestampVector() */



/** Given a start-time, duration and 'stepsize' tStep, returns a list of timestamps
 * covering this time-stretch.
 */
void
LALMakeTimestamps(LALStatus *status,
		  LIGOTimeGPSVector **timestamps, 	/**< [out] timestamps-vector */
		  LIGOTimeGPS tStart, 			/**< GPS start-time */
		  REAL8 duration, 			/**< duration in seconds */
		  REAL8 tStep)				/**< length of one (SFT) timestretch in seconds */
{
  UINT4 i;
  UINT4 numSFTs;
  LIGOTimeGPS tt;
  LIGOTimeGPSVector *ts = NULL;

  INITSTATUS( status, "LALMakeTimestamps", SFTUTILSC);
  ATTATCHSTATUSPTR (status);

  ASSERT (timestamps != NULL, status, SFTUTILS_ENULL, 
	  SFTUTILS_MSGENULL);
  ASSERT (*timestamps == NULL,status, SFTUTILS_ENONULL, 
	  SFTUTILS_MSGENONULL);

  numSFTs = ceil( duration / tStep );			/* >= 1 !*/
  if ( (ts = LALCalloc (1, sizeof( *ts )) ) == NULL ) {
    ABORT (status,  SFTUTILS_EMEM,  SFTUTILS_MSGEMEM);
  }

  ts->length = numSFTs;
  if ( (ts->data = LALCalloc (1, numSFTs * sizeof (*ts->data) )) == NULL) {
    ABORT (status,  SFTUTILS_EMEM,  SFTUTILS_MSGEMEM);
  }

  tt = tStart;	/* initialize to start-time */
  for (i = 0; i < numSFTs; i++)
    {
      ts->data[i] = tt;
      /* get next time-stamp */
      /* NOTE: we add the interval tStep successively (rounded correctly to ns each time!)
       * instead of using iSFT*Tsft, in order to avoid possible ns-rounding problems
       * with REAL8 intervals, which becomes critial from about 100days on...
       */
      LALAddFloatToGPS( status->statusPtr, &tt, &tt, tStep);	/* can't fail */

    } /* for i < numSFTs */

  *timestamps = ts;

  DETATCHSTATUSPTR( status );
  RETURN( status );
  
} /* LALMakeTimestamps() */


/** Extract timstamps-vector from the given SFTVector.
 */
void
LALGetSFTtimestamps (LALStatus *status,
		     LIGOTimeGPSVector **timestamps,	/**< [out] extracted timestamps */
		     const SFTVector *sfts )		/**< input SFT-vector  */
{
  UINT4 numSFTs;
  UINT4 i;
  LIGOTimeGPSVector *ret = NULL;

  INITSTATUS (status, "LALGetSFTtimestamps", SFTUTILSC );
  ATTATCHSTATUSPTR (status); 

  ASSERT ( timestamps, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL );
  ASSERT ( sfts, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL );
  ASSERT ( sfts->length > 0, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL );
  ASSERT ( *timestamps == NULL, status, SFTUTILS_ENONULL, SFTUTILS_MSGENONULL );

  numSFTs = sfts->length;

  TRY ( LALCreateTimestampVector ( status->statusPtr, &ret, numSFTs ), status );

  for ( i=0; i < numSFTs; i ++ )
    ret->data[i] = sfts->data[i].epoch;

  /* done: return Ts-vector */
  (*timestamps) = ret;

  DETATCHSTATUSPTR(status);
  RETURN(status);

} /* LALGetSFTtimestamps() */




/** Extract/construct the unique 2-character "channel prefix" from the given 
 * "detector-name", which unfortunately will not always follow any of the 
 * official detector-naming conventions given in the Frames-Spec LIGO-T970130-F-E
 * This function therefore sometime has to do some creative guessing:
 *
 * NOTE: in case the channel-number can not be deduced from the name, 
 * it is set to '1', and a warning will be printed if lalDebugLevel > 0.
 *
 * NOTE2: the returned string is allocated here!
 */
CHAR *
XLALGetChannelPrefix ( const CHAR *name )
{
  CHAR *channel = LALCalloc( 3, sizeof(CHAR) );  /* 2 chars + \0 */

  if ( !channel ) {
    XLAL_ERROR_NULL ( "XLALGetChannelPrefix", XLAL_ENOMEM );
  }
  if ( !name ) {
    LALFree ( channel );
    XLAL_ERROR_NULL ( "XLALGetChannelPrefix", XLAL_EINVAL );
  }

  /* first handle (currently) unambiguous ones */
  if ( strstr( name, "ALLEGRO") || strstr ( name, "A1") )
    strcpy ( channel, "A1");
  else if ( strstr(name, "NIOBE") || strstr( name, "B1") )
    strcpy ( channel, "B1");
  else if ( strstr(name, "EXPLORER") || strstr( name, "E1") )
    strcpy ( channel, "E1");
  else if ( strstr(name, "GEO") || strstr(name, "G1") )
    strcpy ( channel, "G1" );
  else if ( strstr(name, "ACIGA") || strstr (name, "K1") )
    strcpy ( channel, "K1" );
  else if ( strstr(name, "LLO") || strstr(name, "Livingston") || strstr(name, "L1") )
    strcpy ( channel, "L1" );
  else if ( strstr(name, "Nautilus") || strstr(name, "N1") )
    strcpy ( channel, "N1" );
  else if ( strstr(name, "AURIGA") || strstr(name,"O1") )
    strcpy ( channel, "O1" );
  else if ( strstr(name, "CIT_40") || strstr(name, "Caltech-40") || strstr(name, "P1") )
    strcpy ( channel, "P1" );
  else if ( strstr(name, "TAMA") || strstr(name, "T1") )
    strcpy (channel, "T1" );
  /* currently the only real ambiguity arises with H1 vs H2 */
  else if ( strstr(name, "LHO") || strstr(name, "Hanford") || strstr(name, "H1") || strstr(name, "H2") )
    {
      if ( strstr(name, "LHO_2k") ||  strstr(name, "H2") )
	strcpy ( channel, "H2" );
      else if ( strstr(name, "LHO_4k") ||  strstr(name, "H1") )
	strcpy ( channel, "H1" );
      else /* otherwise: guess */
	{
	  strcpy ( channel, "H1" );
	  if ( lalDebugLevel ) 
	    LALPrintError("WARNING: Detector-name '%s' not unique, guessing '%s'\n", name, channel );
	} 
    } /* if LHO */
  /* LISA channel names are simply left unchanged */
  else if ( strstr(name, "Z1") || strstr(name, "Z2") || strstr(name, "Z3") )
    {
      strncpy ( channel, name, 2);
      channel[2] = 0;
    }
  /* try matching VIRGO last, because 'V1','V2' might be used as version-numbers
   * also in some input-strings */
  else if ( strstr(name, "Virgo") || strstr(name, "VIRGO") || strstr(name, "V1") || strstr(name, "V2") )
    {
      if ( strstr(name, "Virgo_CITF") || strstr(name, "V1") )
	strcpy ( channel, "V1" );
      else if ( strstr(name, "Virgo") || strstr(name, "VIRGO") || strstr(name, "V2") )
	strcpy ( channel, "V2" );
    } /* if Virgo */

    
  if ( channel[0] == 0 )
    {
      if ( lalDebugLevel ) LALPrintError ( "\nERROR: unknown detector-name '%s'\n\n", name );
      XLAL_ERROR_NULL ( "XLALGetChannelPrefix", XLAL_EINVAL );
    }
  else
    return channel;

} /* XLALGetChannelPrefix() */


/** Find the site geometry-information 'LALDetector' (mis-nomer!) given a detector-name.
 * The LALDetector struct is allocated here.
 */
LALDetector * 
XLALGetSiteInfo ( const CHAR *name )
{
  CHAR *channel;
  LALDetector *site;

  /* first turn the free-form 'detector-name' into a well-defined channel-prefix */
  if ( ( channel = XLALGetChannelPrefix ( name ) ) == NULL ) {
    XLAL_ERROR_NULL ( "XLALGetSiteInfo()", XLAL_EINVAL );
  }

  if ( ( site = LALCalloc ( 1, sizeof( *site) )) == NULL ) {
    XLAL_ERROR_NULL ( "XLALGetSiteInfo()", XLAL_ENOMEM );
  }
  
  switch ( channel[0] )
    {
    case 'T':
      (*site) = lalCachedDetectors[LAL_TAMA_300_DETECTOR];
      break;
    case 'V':
      (*site) = lalCachedDetectors[LAL_VIRGO_DETECTOR];
      break;
    case 'G':
      (*site) = lalCachedDetectors[LAL_GEO_600_DETECTOR];
      break;
    case 'H':
      if ( channel[1] == '1' )
	(*site) = lalCachedDetectors[LAL_LHO_4K_DETECTOR];
      else
	(*site) = lalCachedDetectors[LAL_LHO_2K_DETECTOR];
      break;
    case 'L':
      (*site) = lalCachedDetectors[LAL_LLO_4K_DETECTOR];
      break;
    case 'P':
      (*site) = lalCachedDetectors[LAL_CIT_40_DETECTOR];
      break;
    case 'N':
      (*site) = lalCachedDetectors[LAL_NAUTILUS_DETECTOR];
      break;

    case 'Z':       /* create dummy-sites for LISA  */
      if ( create_LISA_detectors ( site, channel[1] ) != 0 ) 
	{
	  LALPrintError("\nFailed to created LISA detector '%d'\n\n", channel[1]);
	  LALFree ( site );
	  LALFree ( channel );
	  XLAL_ERROR_NULL ( "XLALGetSiteInfo()", XLAL_EFUNC );
	}
      break;

    default:
      LALPrintError ( "\nSorry, I don't have the site-info for '%c%c'\n\n", channel[0], channel[1]);
      LALFree(site);
      LALFree(channel);
      XLAL_ERROR_NULL ( "XLALGetSiteInfo()", XLAL_EINVAL );
      break;
    } /* switch channel[0] */

  LALFree ( channel );

  return site;
  
} /* XLALGetSiteInfo() */


/** Set up the \em LALDetector struct representing LISA X, Y, Z TDI observables.
 * INPUT: detIndex = 1, 2, 3: detector-tensor corresponding to TDIs X, Y, Z respectively.
 * return -1 on ERROR, 0 if OK
 */
static int 
create_LISA_detectors (LALDetector *Detector,	/**< [out] LALDetector */
		       CHAR detIndex		/**< [in] which TDI observable: '1' = X, '2'= Y, '3' = Z */
		       )
{
  LALFrDetector detector_params;
  LALDetector Detector1;
  LALStatus status = empty_status;

  if ( !Detector )
    return -1;

  switch ( detIndex )
    {
    case '1':
      strcpy ( detector_params.name, "Z1: LISA TDI X" );
      break;
    case '2':
      strcpy ( detector_params.name, "Z2: LISA TDI Y" );
      break;
    case '3':
      strcpy ( detector_params.name, "Z3: LISA TDI Z" );
      break;
    default:
      LALPrintError ("\nIllegal LISA TDI index '%c': must be one of {'1', '2', '3'}.\n\n", detIndex );
      return -1;      
      break;
    } /* switch (detIndex) */

  /* fill frDetector with dummy numbers: meaningless for LISA */
  detector_params.vertexLongitudeRadians = 0;
  detector_params.vertexLatitudeRadians = 0;
  detector_params.vertexElevation = 0;
  detector_params.xArmAltitudeRadians = 0;
  detector_params.xArmAzimuthRadians = 0;

  LALCreateDetector(&status, &Detector1, &detector_params, LALDETECTORTYPE_IFODIFF );
  if ( status.statusCode != 0 )
    return -1;

  (*Detector) = Detector1;

  return 0;
  
} /* create_LISA_detectors */

/** Computes weight factors arising from SFTs with different noise 
    floors -- it multiplies an existing weight vector */
void LALComputeNoiseWeights  (LALStatus        *status, 
			      REAL8Vector      *weightV,
			      const SFTVector  *sftVect,
			      INT4             blkSize,
			      UINT4            excludePercentile) 
{

  UINT4 lengthVect, lengthSFT, lengthPSD, halfLengthPSD;
  UINT4 j, excludeIndex;
  SFTtype *sft;
  REAL8FrequencySeries periodo;
  REAL8Sequence mediansV, inputV;
  LALRunningMedianPar rngMedPar;

  /* --------------------------------------------- */
  INITSTATUS (status, "LALComputeNoiseWeights", SFTUTILSC);
  ATTATCHSTATUSPTR (status); 

  /*   Make sure the arguments are not NULL: */ 
  ASSERT (weightV, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL);
  ASSERT (sftVect, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL);
  ASSERT (blkSize > 0, status,  SFTUTILS_EINPUT, SFTUTILS_MSGEINPUT);
  ASSERT (weightV->data,status, SFTUTILS_ENULL, SFTUTILS_MSGENULL);
  ASSERT (sftVect->data,status, SFTUTILS_ENULL, SFTUTILS_MSGENULL);
  ASSERT (excludePercentile <= 100, status, SFTUTILS_EINPUT, SFTUTILS_MSGEINPUT);
  /* -------------------------------------------   */

  /* Make sure there is no size mismatch */
  ASSERT (weightV->length == sftVect->length, status, 
	  SFTUTILS_EINPUT, SFTUTILS_MSGEINPUT);
  /* -------------------------------------------   */

  /* Make sure there are elements to be computed*/
  ASSERT (sftVect->length, status, SFTUTILS_EINPUT, SFTUTILS_MSGEINPUT);
  

  /* set various lengths */
  lengthVect = sftVect->length;
  lengthSFT = sftVect->data->data->length;
  ASSERT( lengthSFT > 0, status,  SFTUTILS_EINPUT, SFTUTILS_MSGEINPUT);
  lengthPSD = lengthSFT - blkSize + 1;

  /* make sure blksize is not too big */
  ASSERT(lengthPSD > 0, status, SFTUTILS_EINPUT, SFTUTILS_MSGEINPUT);

  halfLengthPSD = lengthPSD/2; /* integer division */

  /* allocate memory for periodogram */
  periodo.data = NULL;
  periodo.data = (REAL8Sequence *)LALMalloc(sizeof(REAL8Sequence));
  periodo.data->length = lengthSFT;
  periodo.data->data = (REAL8 *)LALMalloc( lengthSFT * sizeof(REAL8));

  /* allocate memory for vector of medians */
  mediansV.length = lengthPSD;
  mediansV.data = (REAL8 *)LALMalloc(lengthPSD * sizeof(REAL8));

  /* rng med block size */
  rngMedPar.blocksize = blkSize;

  /* calculate index in psd medians vector from which to calculate mean */
  excludeIndex =  (excludePercentile * halfLengthPSD) ; /* integer arithmetic */
  excludeIndex /= 100; /* integer arithmetic */

  /* loop over sfts and calculate weights */
  for (j=0; j<lengthVect; j++) {
    REAL8 sumMed = 0.0;
    UINT4 k;

    sft = sftVect->data + j;

    /* calculate the periodogram */
    TRY (LALSFTtoPeriodogram (status->statusPtr, &periodo, sft), status);
    
    /* calculate the running median */
    inputV.length = lengthSFT;
    inputV.data = periodo.data->data;
    TRY( LALDRunningMedian2(status->statusPtr, &mediansV, &inputV, rngMedPar), status);

    /* now sort the mediansV.data vector and exclude the top and last percentiles */
    gsl_sort(mediansV.data, 1, mediansV.length);

    /* sum median excluding appropriate elements */ 
    for (k = excludeIndex; k < lengthPSD - excludeIndex; k++) {
      sumMed += mediansV.data[k];
    }

    /* weight is proportional to 1/sumMed */    
    weightV->data[j] /= sumMed;
    
  } /* end of loop over sfts */

  /* remember to normalize weights immediately after leaving this function */

  /* free memory */
  LALFree(mediansV.data);
  LALFree(periodo.data->data);
  LALFree(periodo.data);

  DETATCHSTATUSPTR (status);
   /* normal exit */
  RETURN (status);

} /* LALComputeNoiseWeights() */


/** Computes weight factors arising from MultiSFTs with different noise 
 * floors -- it multiplies an existing weight vector 
 * The 'normalization' returned is the factor by which the inverse PSD's S^-1_{X,a}
 * have been normalized to give the noise-weights w_{X,a}
 */
void LALComputeMultiNoiseWeights  (LALStatus             *status, 
				   MultiNoiseWeights     **out,
				   const MultiPSDVector  *multipsd,
				   UINT4                 blocksRngMed,
				   UINT4                 excludePercentile) 
{
  REAL8 Tsft_Sn=0.0, Tsft_sumSn=0.0;
  UINT4 i, k, j, numifos, numsfts, lengthsft, numsftsTot;
  UINT4 excludeIndex, halfLengthPSD, lengthPSD;
  MultiNoiseWeights *weights;
  REAL8 Tsft = 1.0 / multipsd->data[0]->data[0].deltaF;
  REAL8 Tsft_calS;	/* overall noise-normalization */

  INITSTATUS (status, "LALComputeMultiNoiseWeights", SFTUTILSC);
  ATTATCHSTATUSPTR (status); 

  ASSERT ( multipsd, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL);
  ASSERT ( multipsd->data, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL);
  ASSERT ( multipsd->length, status, SFTUTILS_EINPUT, SFTUTILS_MSGEINPUT);

  ASSERT ( out, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL);
  ASSERT ( *out == NULL, status, SFTUTILS_ENULL, SFTUTILS_MSGENULL);

  numifos = multipsd->length;

  if ( (weights = (MultiNoiseWeights *)LALCalloc(1, sizeof(MultiNoiseWeights))) == NULL ){
    ABORT (status,  SFTUTILS_EMEM,  SFTUTILS_MSGEMEM);
  }

  weights->length = numifos;
  if ( (weights->data = (REAL8Vector **)LALCalloc( numifos, sizeof(REAL8Vector *))) == NULL) {
    ABORT (status,  SFTUTILS_EMEM,  SFTUTILS_MSGEMEM);      
  }

  numsftsTot = 0;
  for ( k = 0; k < numifos; k++) 
    {
      numsfts = multipsd->data[k]->length;
      numsftsTot += numsfts;

      /* create k^th weights vector */
      LALDCreateVector ( status->statusPtr, weights->data + k, numsfts);
      BEGINFAIL( status ) {
	for ( i = 0; i < k-1; i++)
	  LALDDestroyVector (status->statusPtr, weights->data + i);
	LALFree (weights->data);
	LALFree (weights);
      } ENDFAIL(status);
      
      /* loop over psds and calculate weights -- one for each sft */
      for ( j = 0; j < numsfts; j++) 
	{
	  REAL8FrequencySeries *psd;
	  UINT4 halfBlock = blocksRngMed/2;
	  
	  psd = multipsd->data[k]->data + j;
	  
	  lengthsft = psd->data->length;
	  if ( lengthsft < blocksRngMed ) {
	    ABORT ( status, SFTUTILS_EINPUT, SFTUTILS_MSGEINPUT);
	  }

	  lengthPSD = lengthsft - blocksRngMed + 1;
	  halfLengthPSD = lengthPSD/2;

	  /* calculate index in psd medians vector from which to calculate mean */
	  excludeIndex =  excludePercentile * halfLengthPSD ; /* integer arithmetic */
	  excludeIndex /= 100; /* integer arithmetic */

	  for ( Tsft_Sn = 0.0, i = halfBlock + excludeIndex; i < lengthsft - halfBlock - excludeIndex; i++)
	    Tsft_Sn += psd->data->data[i];
	  Tsft_Sn /= lengthsft - 2*halfBlock - 2*excludeIndex;

	  Tsft_sumSn += Tsft_Sn; /* sumSn is just a normalization factor */

	  weights->data[k]->data[j] = 1.0/Tsft_Sn;
	} /* end loop over sfts for each ifo */

    } /* end loop over ifos */

  Tsft_calS = Tsft_sumSn/numsftsTot;	/* overall noise-normalization is the average Tsft * <S_{X,\alpha}> */

  /* make weights of order unity by myltiplying by sumSn/total number of sfts */
  for ( k = 0; k < numifos; k++) {
    numsfts = weights->data[k]->length;    
    for ( j = 0; j < numsfts; j++) 
      weights->data[k]->data[j] *= Tsft_calS;
  }

  weights->Sinv_Tsft = Tsft*Tsft / Tsft_calS;		/* 'Sinv * Tsft' normalization factor */

  *out = weights;

  
  DETATCHSTATUSPTR (status);
   /* normal exit */
  RETURN (status);
}


void 
LALDestroyMultiNoiseWeights  (LALStatus         *status, 
			      MultiNoiseWeights **weights) 
{
  UINT4 k;

  INITSTATUS (status, "LALDestroyMultiNoiseWeights", SFTUTILSC);
  ATTATCHSTATUSPTR (status); 

  ASSERT ( weights != NULL, status, SFTUTILS_ENULL,  SFTUTILS_MSGENULL);

  if (*weights == NULL) 
    goto finished;

  for ( k = 0; k < (*weights)->length; k++)
    LALDDestroyVector (status->statusPtr, (*weights)->data + k);      

  LALFree( (*weights)->data );
  LALFree(*weights);
  
  *weights = NULL;

 finished:
  DETATCHSTATUSPTR (status);
  RETURN (status);

} /* LALDestroyMultiNoiseWeights() */
