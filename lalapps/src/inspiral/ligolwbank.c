/*----------------------------------------------------------------------- 
 * 
 * File Name: ligolwbank.c
 *
 * Author: Brown, D. A.
 * 
 * Revision: $Id$
 * 
 *-----------------------------------------------------------------------
 */


#include "ligolwbank.h"

#if 0
struct MetaTableDirectory
{
  char *name;
  int   pos;
  int   idx;
};
#endif

#define CLOBBER_EVENTS \
    while ( *eventHead ); \
    { \
      thisEvent = *eventHead; \
      *eventHead = (*eventHead)->next; \
      LALFree( thisEvent ); \
      thisEvent = NULL; \
    }

int
SnglInspiralTableFromLIGOLw (
    SnglInspiralTable **eventHead,
    CHAR               *fileName,
    INT4                startEvent,
    INT4                stopEvent
    )
{
  int                                   i, j, nrows;
  int                                   mioStatus;
  SnglInspiralTable                    *thisEvent = NULL;
  struct MetaioParseEnvironment         parseEnv;
  const  MetaioParseEnv                 env = &parseEnv;
  MetaTableDirectory tableDir[] =
  {
    {"ifo",                     -1, 0},
    {"search",                  -1, 1},
    {"channel",                 -1, 2},
    {"end_time",                -1, 3},
    {"end_time_ns",             -1, 4},
    {"impulse_time",            -1, 5},
    {"impulse_time_ns",         -1, 6},
    {"template_duration",       -1, 7},
    {"event_duration",          -1, 8},
    {"amplitude",               -1, 9},
    {"eff_distance",            -1, 10},
    {"coa_phase",               -1, 11},
    {"mass1",                   -1, 12},
    {"mass2",                   -1, 13},
    {"mchirp",                  -1, 14},
    {"eta",                     -1, 15},
    {"tau0",                    -1, 16},
    {"tau2",                    -1, 17},
    {"tau3",                    -1, 18},
    {"tau4",                    -1, 19},
    {"tau5",                    -1, 20},
    {"ttotal",                  -1, 21},
    {"snr",                     -1, 22},
    {"chisq",                   -1, 23},
    {"chisq_dof",               -1, 24},
    {"sigmasq",                 -1, 25},
    {NULL,                       0, 0}
  };

  /* check that the bank handle and pointer are vaid */
  if ( ! eventHead )
  {
    fprintf( stderr, "null pointer passed as handle to event list" );
    return -1;
  }
  if ( *eventHead )
  {
    fprintf( stderr, "non-null pointer passed as pointer to event list" );
    return -1;
  }
  
  /* open the sngl_inspiral table template bank file */
  mioStatus = MetaioOpenTable( env, fileName, "sngl_inspiral" );
  if ( mioStatus )
  {
    fprintf( stdout, "no sngl_inspiral table in file %s\n", fileName );
    return 0;
  }

  /* figure out the column positions of the template parameters */
  for ( i = 0; tableDir[i].name; ++i )
  {
    if ( (tableDir[i].pos = MetaioFindColumn( env, tableDir[i].name )) < 0 )
    {
      fprintf( stderr, "unable to find column %s\n", tableDir[i].name );
      MetaioClose(env);
      return -1;
    }
  }

  /* loop over the rows in the file */
  i = nrows = 0;
  while ( (mioStatus = MetaioGetRow(env)) == 1 ) 
  {
    /* count the rows in the file */
    i++;

    /* stop parsing if we have reach the last row requested */
    if ( stopEvent > -1 && i > stopEvent )
    {
      break;
    }

    /* if we have reached the first requested row, parse the row */
    if ( i > startEvent )
    {
      /* allocate memory for the template we are about to read in */
      if ( ! *eventHead )
      {
        thisEvent = *eventHead = (SnglInspiralTable *) 
          LALCalloc( 1, sizeof(SnglInspiralTable) );
      }
      else
      {
        thisEvent = thisEvent->next = (SnglInspiralTable *) 
          LALCalloc( 1, sizeof(SnglInspiralTable) );
      }
      if ( ! thisEvent )
      {
        fprintf( stderr, "could not allocate inspiral template\n" );
        CLOBBER_EVENTS;
        MetaioClose( env );
        return -1;
      }
      
      /* parse the contents of the row into the InspiralTemplate structure */
      for ( j = 0; tableDir[j].name; ++j )
      {
        REAL4 r4colData = env->ligo_lw.table.elt[tableDir[j].pos].data.real_4;
        REAL8 r8colData = env->ligo_lw.table.elt[tableDir[j].pos].data.real_8;
        INT4  i4colData = env->ligo_lw.table.elt[tableDir[j].pos].data.int_4s;

        if ( tableDir[j].idx == 0 )
        {
          LALSnprintf( thisEvent->ifo, LIGOMETA_IFO_MAX * sizeof(CHAR), 
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 1 )
        {
          LALSnprintf( thisEvent->search, LIGOMETA_SEARCH_MAX * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 2 )
        {
          LALSnprintf( thisEvent->channel, LIGOMETA_CHANNEL_MAX * sizeof(CHAR),
              "%s", env->ligo_lw.table.elt[tableDir[j].pos].data.lstring.data );
        }
        else if ( tableDir[j].idx == 3 )
        {
          thisEvent->end_time.gpsSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 4 )
        {
          thisEvent->end_time.gpsNanoSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 5 )
        {
          thisEvent->impulse_time.gpsSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 6 )
        {
          thisEvent->impulse_time.gpsNanoSeconds = i4colData;
        }
        else if ( tableDir[j].idx == 7 )
        {
          thisEvent->template_duration = r8colData;
        }
        else if ( tableDir[j].idx == 8 )
        {
          thisEvent->event_duration = r8colData;
        }
        else if ( tableDir[j].idx == 9 )
        {
          thisEvent->amplitude = r4colData;
        }
        else if ( tableDir[j].idx == 10 )
        {
          thisEvent->eff_distance = r4colData;
        }
        else if ( tableDir[j].idx == 11 )
        {
          thisEvent->coa_phase = r4colData;
        }
        else if ( tableDir[j].idx == 12 )
        {
          thisEvent->mass1 = r4colData;
        }
        else if ( tableDir[j].idx == 13 )
        {
          thisEvent->mass2 = r4colData;
        }
        else if ( tableDir[j].idx == 14 )
        {
          thisEvent->mchirp = r4colData;
        }
        else if ( tableDir[j].idx == 15 )
        {
          thisEvent->eta = r4colData;
        }
        else if ( tableDir[j].idx == 16 )
        {
          thisEvent->tau0 = r4colData;
        }
        else if ( tableDir[j].idx == 17 )
        {
          thisEvent->tau2 = r4colData;
        }
        else if ( tableDir[j].idx == 18 )
        {
          thisEvent->tau3 = r4colData;
        }
        else if ( tableDir[j].idx == 19 )
        {
          thisEvent->tau4 = r4colData;
        }
        else if ( tableDir[j].idx == 20 )
        {
          thisEvent->tau5 = r4colData;
        }
        else if ( tableDir[j].idx == 21 )
        {
          thisEvent->ttotal = r4colData;
        }
        else if ( tableDir[j].idx == 22 )
        {
          thisEvent->snr = r4colData;
        }
        else if ( tableDir[j].idx == 23 )
        {
          thisEvent->chisq = r4colData;
        }
        else if ( tableDir[j].idx == 24 )
        {
          thisEvent->chisq_dof = i4colData;
        }
        else if ( tableDir[j].idx == 25 )
        {
          thisEvent->sigmasq = r8colData;
        }
        else
        {
          CLOBBER_EVENTS;
          fprintf( stderr, "unknown column while parsing sngl_inspiral\n" );
          return -1;
        }
      }

      /* count the number of template parsed */
      nrows++;
    }
  }

  if ( mioStatus == -1 )
  {
    fprintf( stderr, "error parsing after row %d\n", i );
    CLOBBER_EVENTS;
    MetaioClose( env );
    return -1;
  }

  /* we have sucesfully parsed temples */
  MetaioClose( env );
  return nrows;  
}

#undef CLOBBER_EVENTS

#define CLOBBER_BANK \
    while ( *bankHead ); \
    { \
      thisTmplt = *bankHead; \
      *bankHead = (*bankHead)->next; \
      LALFree( thisTmplt ); \
      thisTmplt = NULL; \
    }

int
InspiralTmpltBankFromLIGOLw (
    InspiralTemplate  **bankHead,
    CHAR               *fileName,
    INT4                startTmplt,
    INT4                stopTmplt
    )
{
  int                                   i, j, nrows;
  int                                   mioStatus;
  InspiralTemplate                     *thisTmplt = NULL;
  struct MetaioParseEnvironment         parseEnv;
  const  MetaioParseEnv                 env = &parseEnv;
  int   pParParam;
  int   pParValue;
  REAL4 minMatch = 0;
  MetaTableDirectory tableDir[] =
  {
    {"mass1",   -1, 0},
    {"mass2",   -1, 1},
    {"mchirp",  -1, 2},
    {"eta",     -1, 3},
    {"tau0",    -1, 4},
    {"tau2",    -1, 5},
    {"tau3",    -1, 6},
    {"tau4",    -1, 7},
    {"tau5",    -1, 8},
    {"ttotal",  -1, 9},
    {NULL,      0, 0}
  };

  /* check that the bank handle and pointer are vaid */
  if ( ! bankHead )
  {
    fprintf( stderr, "null pointer passed as handle to template bank" );
    return -1;
  }
  if ( *bankHead )
  {
    fprintf( stderr, "non-null pointer passed as pointer to template bank" );
    return -1;
  }
  

  /* open the procress_params table from the bank file */
  mioStatus = MetaioOpenTable( env, fileName, "process_params" );
  if ( mioStatus )
  {
    fprintf( stderr, "error opening process_params table from file %s\n", 
        fileName );
    return -1;
  }

  /* figure out where the param and value columns are */
  if ( (pParParam = MetaioFindColumn( env, "param" )) < 0 )
  {
    fprintf( stderr, "unable to find column param in process_params\n" );
    MetaioClose(env);
    return -1;
  }
  if ( (pParValue = MetaioFindColumn( env, "value" )) < 0 )
  {
    fprintf( stderr, "unable to find column value in process_params\n" );
    MetaioClose(env);
    return -1;
  }

  /* get the minimal match of the bank from the process params */
  while ( (mioStatus = MetaioGetRow(env)) == 1 )
  {
    if ( ! strcmp( env->ligo_lw.table.elt[pParParam].data.lstring.data, 
          "--minimal-match" ) )
    {
      minMatch = (REAL4) 
        atof( env->ligo_lw.table.elt[pParValue].data.lstring.data );
    }
  }

  MetaioClose( env );
  
  /* open the sngl_inspiral table template bank file */
  mioStatus = MetaioOpenTable( env, fileName, "sngl_inspiral" );
  if ( mioStatus )
  {
    fprintf( stdout, "no sngl_inspiral table in file %s\n", fileName );
    return 0;
  }

  /* figure out the column positions of the template parameters */
  for ( i = 0; tableDir[i].name; ++i )
  {
    if ( (tableDir[i].pos = MetaioFindColumn( env, tableDir[i].name )) < 0 )
    {
      fprintf( stderr, "unable to find column %s\n", tableDir[i].name );
      MetaioClose(env);
      return -1;
    }
  }

  /* loop over the rows in the file */
  i = nrows = 0;
  while ( (mioStatus = MetaioGetRow(env)) == 1 ) 
  {
    /* count the rows in the file */
    i++;

    /* stop parsing if we have reach the last row requested */
    if ( stopTmplt > -1 && i > stopTmplt )
    {
      break;
    }

    /* if we have reached the first requested row, parse the row */
    if ( i > startTmplt )
    {
      /* allocate memory for the template we are about to read in */
      if ( ! *bankHead )
      {
        thisTmplt = *bankHead = (InspiralTemplate *) 
          LALCalloc( 1, sizeof(InspiralTemplate) );
      }
      else
      {
        thisTmplt = thisTmplt->next = (InspiralTemplate *) 
          LALCalloc( 1, sizeof(InspiralTemplate) );
      }
      if ( ! thisTmplt )
      {
        fprintf( stderr, "could not allocate inspiral template\n" );
        CLOBBER_BANK;
        MetaioClose( env );
        return -1;
      }
      
      /* parse the contents of the row into the InspiralTemplate structure */
      for ( j = 0; tableDir[j].name; ++j )
      {
        REAL4 colData = env->ligo_lw.table.elt[tableDir[j].pos].data.real_4;
        if ( tableDir[j].idx == 0 )
        {
          thisTmplt->mass1 = colData;
        }
        else if ( tableDir[j].idx == 1 )
        {
          thisTmplt->mass2 = colData;
        }
        else if ( tableDir[j].idx == 2 )
        {
          thisTmplt->chirpMass = colData;
        }
        else if ( tableDir[j].idx == 3 )
        {
          thisTmplt->eta = colData;
        }
        else if ( tableDir[j].idx == 4 )
        {
          thisTmplt->t0 = colData;
        }
        else if ( tableDir[j].idx == 5 )
        {
          thisTmplt->t2 = colData;
        }
        else if ( tableDir[j].idx == 6 )
        {
          thisTmplt->t3 = colData;
        }
        else if ( tableDir[j].idx == 7 )
        {
          thisTmplt->t4 = colData;
        }
        else if ( tableDir[j].idx == 8 )
        {
          thisTmplt->t5 = colData;
        }
        else if ( tableDir[j].idx == 9 )
        {
          thisTmplt->tC = colData;
        }
        else
        {
          CLOBBER_BANK;
          fprintf( stderr, "unknown column while parsing\n" );
          return -1;
        }
      }

      /* compute derived mass parameters */
      thisTmplt->totalMass = thisTmplt->mass1 + thisTmplt->mass2;
      thisTmplt->mu = thisTmplt->mass1 * thisTmplt->mass2 / 
        thisTmplt->totalMass;
      
      /* set the match determined from the bank generation process params */
      thisTmplt->minMatch = minMatch;

      /* count the number of template parsed */
      thisTmplt->number = nrows++;
    }
  }

  if ( mioStatus == -1 )
  {
    fprintf( stderr, "error parsing after row %d\n", i );
    CLOBBER_BANK;
    MetaioClose( env );
    return -1;
  }

  /* we have sucesfully parsed temples */
  MetaioClose( env );
  return nrows;  
}

#define CLOBBER_SIM \
    while ( *simHead ); \
    { \
      thisSim = *simHead; \
      *simHead = (*simHead)->next; \
      LALFree( thisSim ); \
      thisSim = NULL; \
    }

int
SimInspiralTableFromLIGOLw (
    SimInspiralTable   **simHead,
    CHAR                *fileName,
    INT4                 startTime,
    INT4                 endTime
    )
{
  int                                   i, j, nrows;
  int                                   mioStatus;
  SimInspiralTable                     *thisSim = NULL;
  struct MetaioParseEnvironment         parseEnv;
  const  MetaioParseEnv                 env = &parseEnv;
  MetaTableDirectory tableDir[] =
  {
    {"geocent_end_time",    -1, 0},
    {"geocent_end_time_ns", -1, 1},
    {"mtotal",              -1, 2},
    {"eta",                 -1, 3},
    {"distance",            -1, 4},
    {"longitude",           -1, 5},
    {"latitude",            -1, 6},
    {"inclination",         -1, 7},
    {"coa_phase",           -1, 8},
    {"polarization",        -1, 9},
    {NULL,                   0, 0}
  };

  /* check that the bank handle and pointer are vaid */
  if ( ! simHead )
  {
    fprintf( stderr, "null pointer passed as handle to simulation list" );
    return -1;
  }
  if ( *simHead )
  {
    fprintf( stderr, "non-null pointer passed as pointer to simulation list" );
    return -1;
  }
  
  /* open the sngl_inspiral table template bank file */
  mioStatus = MetaioOpenTable( env, fileName, "sim_inspiral" );
  if ( mioStatus )
  {
    fprintf( stderr, "error opening sim_inspiral table from file %s\n", 
        fileName );
    return -1;
  }

  /* figure out the column positions of the template parameters */
  for ( i = 0; tableDir[i].name; ++i )
  {
    if ( (tableDir[i].pos = MetaioFindColumn( env, tableDir[i].name )) < 0 )
    {
      fprintf( stderr, "unable to find column %s\n", tableDir[i].name );
      MetaioClose(env);
      return -1;
    }
  }

  /* loop over the rows in the file */
  i = nrows = 0;
  while ( (mioStatus = MetaioGetRow(env)) == 1 ) 
  {
    INT4 geo_time = env->ligo_lw.table.elt[tableDir[0].pos].data.int_4s;

      /* get the injetcion time and check that it is within the time window */
      if ( geo_time > startTime && geo_time < endTime )
      {
        /* allocate memory for the template we are about to read in */
        if ( ! *simHead )
        {
          thisSim = *simHead = (SimInspiralTable *) 
            LALCalloc( 1, sizeof(SimInspiralTable) );
        }
        else
        {
          thisSim = thisSim->next = (SimInspiralTable *) 
            LALCalloc( 1, sizeof(SimInspiralTable) );
        }
        if ( ! thisSim )
        {
          fprintf( stderr, "could not allocate inspiral simulation\n" );
          CLOBBER_SIM;
          MetaioClose( env );
          return -1;
        }

        thisSim->geocent_end_time.gpsSeconds = geo_time;
        thisSim->geocent_end_time.gpsNanoSeconds = 
          env->ligo_lw.table.elt[tableDir[1].pos].data.int_4s;

        /* parse the rest of the row into the SimInspiralTable structure */
        for ( j = 2; tableDir[j].name; ++j )
        {
          REAL4 realData = env->ligo_lw.table.elt[tableDir[j].pos].data.real_4;

          if ( tableDir[j].idx == 2 )
          {
            thisSim->mtotal = realData;
          }
          else if ( tableDir[j].idx == 3 )
          {
            thisSim->eta = realData;
          }
          else if ( tableDir[j].idx == 4 )
          {
            thisSim->distance = realData * 1.0e6 * LAL_PC_SI;
          }
          else if ( tableDir[j].idx == 5 )
          {
            thisSim->longitude = realData;
          }
          else if ( tableDir[j].idx == 6 )
          {
            thisSim->latitude = realData;
          }
          else if ( tableDir[j].idx == 7 )
          {
            thisSim->inclination = realData;
          }
          else if ( tableDir[j].idx == 8 )
          {
            thisSim->coa_phase = realData;
          }
          else if ( tableDir[j].idx == 9 )
          {
            thisSim->polarization = realData;
          }
          else
          {
            CLOBBER_SIM;
            fprintf( stderr, "unknown column while parsing\n" );
            return -1;
          }

          /* increase the count of rows parsed */
          ++nrows;
        }
      }
  }

  if ( mioStatus == -1 )
  {
    fprintf( stderr, "error parsing after row %d\n", i );
    CLOBBER_SIM;
    MetaioClose( env );
    return -1;
  }

  /* we have sucesfully parsed temples */
  MetaioClose( env );
  return nrows;  
}

#undef CLOBBER_SIM

