/*----------------------------------------------------------------------- 
 * 
 * File Name: LALXMGRInterface.c
 *
 * Author: Brady, P. R., and Brown, D. A.
 * 
 * Revision: $Id$
 * 
 *-----------------------------------------------------------------------
 */

#if 0
<lalVerbatim file="LALXMGRInterfaceCV">
Author: Brady, P. R., and Brown D. A.
$Id$
</lalVerbatim>

<lalLaTeX>
\subsection{Module \texttt{LALXMGRInterface.c}}
\label{ss:LALXMGRInterface.c}

Functions for creating XMGR graphs from LAL structures and functions.

\vfill{\footnotesize\input{LALXMGRInterfaceCV}}
</lalLaTeX>
#endif

#include <math.h>
#include <lal/LALStdlib.h>
#include <lal/LALStd.h>
#include <lal/LALConstants.h>
#include <lal/AVFactories.h>
#include <lal/LALXMGRInterface.h>

NRCSID (LALXMGRINTERFACEC, "$Id$");

/* ---------------------------------------------------------------------- */
void
LALXMGROpenFile (
    LALStatus          *status,
    FILE              **fp,
    CHAR               *title,
    CHAR               *fileName
    )
{
  const char    xmgrHeader[] = 
    "# ACE/gr parameter file\n#\n"
    "@version 40102\n"
    "@page layout free\n"
    "@ps linewidth begin 1\n"
    "@ps linewidth increment 2\n"
    "@hardcopy device 1\n"
    "@page 5\n"
    "@page inout 5\n"
    "@link page off\n"
    "@default linestyle 1\n"
    "@default linewidth 1\n"
    "@default color 1\n"
    "@default char size 1.000000\n"
    "@default font 4\n"
    "@default font source 0\n"
    "@default symbol size 1.000000\n"
    "@timestamp off\n"
    "@timestamp 0.03, 0.03\n"
    "@timestamp linewidth 1\n"
    "@timestamp color 1\n"
    "@timestamp rot 0\n"
    "@timestamp font 4\n"
    "@timestamp char size 1.000000\n"
    "@timestamp def \"Wed Jan  2 19:55:05 2002\"\n";

  const CHAR    titleHeader[] =
    "@with string\n"
    "@    string on\n"
    "@    string loctype view\n"
    "@    string 0.10, 0.95\n"
    "@    string linewidth 2\n"
    "@    string color 1\n"
    "@    string rot 0\n"
    "@    string font 4\n"
    "@    string just 0\n"
    "@    string char size 0.750000\n";

  INITSTATUS( status, "LALXMGROpenFile", LALXMGRINTERFACEC );

  ASSERT( !fp, status, 
      LALXMGRINTERFACEH_ENNUL, LALXMGRINTERFACEH_MSGENNUL );
  ASSERT( fileName, status, 
      LALXMGRINTERFACEH_ENULL, LALXMGRINTERFACEH_MSGENULL );

  if ( ! (*fp = fopen( fileName, "w" )) )
  {
    ABORT( status, LALXMGRINTERFACEH_ENULL, LALXMGRINTERFACEH_MSGEOPEN );
  }

  fprintf( *fp, "%s", xmgrHeader );

  if ( title )
  {
    fprintf( *fp, "%s", titleHeader );
    fprintf( *fp,  "@    string def \"%s\"\n", title );
  }

  RETURN( status );
}


/* ---------------------------------------------------------------------- */
void
LALXMGRCloseFile ( 
    LALStatus          *status,
    FILE               *fp
    )
{
  INITSTATUS( status, "LALXMGRCloseFile", LALXMGRINTERFACEC );

  ASSERT( fp, status, LALXMGRINTERFACEH_ENULL, LALXMGRINTERFACEH_MSGENULL );

  if ( ! fclose( fp ) )
  {
    ABORT( status, LALXMGRINTERFACEH_EFCLO, LALXMGRINTERFACEH_MSGEFCLO );
  }

  RETURN( status );
}

/* ---------------------------------------------------------------------- */
void
LALXMGRCreateGraph (
    LALStatus          *status,
    XMGRGraphVector    *graphVec
    )
{
  XMGRGraph            *graph;
  XMGRGraph            *newGraph;

  INITSTATUS( status, "LALXMGRCloseFile", LALXMGRINTERFACEC );
  ATTATCHSTATUSPTR( status );

  ASSERT( graphVec, status, 
      LALXMGRINTERFACEH_ENULL, LALXMGRINTERFACEH_MSGENULL );

  graph = graphVec->data;


  /* 
   *
   * allocate enough memory in the graph array for the graphs
   *
   */


  if ( graphVec->length > 5 )
  {
    ABORT( status, LALXMGRINTERFACEH_ENGRA, LALXMGRINTERFACEH_MSGENGRA );
  }

  graph = (XMGRGraph *) 
    LALRealloc( graph, ++(graphVec->length) * sizeof(XMGRGraph) );
  if ( ! graphVec->data )
  {
    ABORT( status, LALXMGRINTERFACEH_EALOC, LALXMGRINTERFACEH_MSGEALOC );
  }

  /* zero the array element we have just created */
  newGraph = graph + graphVec->length - 1;
  memset( newGraph, 0, sizeof(XMGRGraph) );
  

  /*
   *
   * allocate memory for the parameter structures in the graph
   *
   */


  /* always create xy graphs at the moment: may want to extend this */
  LALCHARCreateVector( status->statusPtr, &(newGraph->type), (UINT4) 3 );
  CHECKSTATUSPTR( status );

  sprintf( newGraph->type->data, "xy" );

  newGraph->xaxis = (XMGRAxisParams *) LALCalloc( 1, sizeof(XMGRAxisParams) );
  newGraph->yaxis = (XMGRAxisParams *) LALCalloc( 1, sizeof(XMGRAxisParams) );

  if ( ! graph->xaxis || ! graph->yaxis )
  {
    ABORT( status, LALXMGRINTERFACEH_EALOC, LALXMGRINTERFACEH_MSGEALOC );
  }


  /*
   *
   * set and reset the viewport coords depending on number of graphs
   *
   */


  {
    UINT4       i;
    REAL4       w, h;   /* width and height of graphs */
    REAL4       xshift = 0.02;

    /* we use the same algorithm from dataviewer (with a few modifications) */
    for ( i = 0; i < graphVec->length; ++i )
    {
      graph[i].viewx[0] = 0.0;
      graph[i].viewy[0] = 0.0;
    }
    switch ( graphVec->length )
    {
      case 1: 
        graph[0].viewx[0] = 0.06; graph[0].viewy[0] = 0.028;
        w = 0.90;
        h = 0.88;
        break;
      case 2: 
        graph[0].viewx[0] = 0.06; graph[0].viewy[0] = 0.026;
        graph[1].viewx[0] = 0.06; graph[1].viewy[0] = 0.51; 
        w = 0.90;
        h = 0.42;
        break;
      case 3: 
        graph[3].viewx[0] = 0.54; graph[3].viewy[0] = 0.57; 
      case 4: 
        graph[0].viewx[0] = 0.04; graph[0].viewy[0] = 0.10; 
        graph[1].viewx[0] = 0.04; graph[1].viewy[0] = 0.57;
        graph[2].viewx[0] = 0.54; graph[2].viewy[0] = 0.10; 
        w = 0.45;
        h = 0.35;
        break;
      case 5: 
        graph[5].viewx[0] = 0.56; graph[5].viewy[0] = 0.62; 
      case 6: 
        graph[0].viewx[0] = 0.10; graph[0].viewy[0] = 0.02; 
        graph[1].viewx[0] = 0.10; graph[1].viewy[0] = 0.32; 
        graph[2].viewx[0] = 0.10; graph[2].viewy[0] = 0.62; 
        graph[3].viewx[0] = 0.56; graph[3].viewy[0] = 0.02;
        graph[4].viewx[0] = 0.56; graph[4].viewy[0] = 0.32; 
        w = 0.37;
        h = 0.26;
        break;
      default:
        ABORT( status, LALXMGRINTERFACEH_ENGRA, LALXMGRINTERFACEH_MSGENGRA );
    }

    for ( i = 0; i < graphVec->length; ++i )
    {
      graph[i].viewy[1] = graph[i].viewy[1] + h;
      graph[i].viewx[1] = graph[i].viewx[0] + w;
      graph[i].viewx[0] += xshift;
    }
  }


  /* XXX */


  DETATCHSTATUSPTR( status );
  RETURN( status );
}

/* ---------------------------------------------------------------------- */
void
LALXMGRGPSTimeToTitle(
    LALStatus          *status,
    CHARVector         *title,
    LIGOTimeGPS        *startGPS,
    LIGOTimeGPS        *stopGPS,
    CHAR               *comment
    )
{
  LALLeapSecAccuracy    accuracy = LALLEAPSEC_STRICT;
  CHARVector           *startString = NULL;
  CHARVector           *stopString  = NULL;
  LALDate               thisDate;

  INITSTATUS( status, "LALXMGRGPSTimeToTitle", LALXMGRINTERFACEC );
  ATTATCHSTATUSPTR( status );

  ASSERT( title, status, 
      LALXMGRINTERFACEH_ENULL, LALXMGRINTERFACEH_MSGENULL );
  ASSERT( startGPS, status, 
      LALXMGRINTERFACEH_ENULL, LALXMGRINTERFACEH_MSGENULL );
  ASSERT( stopGPS, status, 
      LALXMGRINTERFACEH_ENULL, LALXMGRINTERFACEH_MSGENULL );
  ASSERT( comment, status, 
      LALXMGRINTERFACEH_ENULL, LALXMGRINTERFACEH_MSGENULL );

  LALCHARCreateVector( status->statusPtr, &startString, (UINT4) 64 );
  CHECKSTATUSPTR( status );
  LALCHARCreateVector( status->statusPtr, &stopString, (UINT4) 64 );
  CHECKSTATUSPTR( status );

  LALGPStoUTC( status->statusPtr, &thisDate, startGPS, &accuracy );
  CHECKSTATUSPTR( status );
  LALDateString( status->statusPtr, startString, &thisDate );
  CHECKSTATUSPTR( status );

  LALGPStoUTC( status->statusPtr, &thisDate, stopGPS, &accuracy );
  CHECKSTATUSPTR( status );
  LALDateString( status->statusPtr, stopString, &thisDate );
  CHECKSTATUSPTR( status );

  LALSnprintf( title->data, title->length * sizeof(CHAR), 
      "%s from %s to %s", comment, startString->data, stopString->data );

  LALCHARDestroyVector( status->statusPtr, &startString );
  CHECKSTATUSPTR( status );
  LALCHARDestroyVector( status->statusPtr, &stopString);
  CHECKSTATUSPTR( status );

  DETATCHSTATUSPTR( status );
  RETURN( status );
}

