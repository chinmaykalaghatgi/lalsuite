/*
<lalVerbatim file="SimulatePopcornCV">
Author: Tania Regimbau
$Id$
</lalVerbatim> 
<lalLaTeX>
\subsection{Module \texttt{SimulatePopcorn.c}}
\label{ss:SimulatePopcorn.c}

Routine for simulating whitened time-domain signals in a pair 
of detectors that arises from low duty cycle astrophysical backgrounds

\subsubsection*{Prototypes}
\input{SimulatePopcornCP}
\idx{LALSimPopcornTimeSeries()}

\subsubsection*{Description}

This routines  simulate stochastic backgrounds of astrophysical origin produced by the superposition of 'burst sources' since the beginning of the stellar activity. Depending on the ratio between the burst duration and the mean arrival time interval between events, such signals may be sequences of resolved bursts, 'popcorn noises' or continuous backgrounds. 

\subsubsection*{Algorithm}

The two unwhitened time series are produced according to the procedure discribed in Coward,Burman & Blair, 2002, MNRAS, 329.
 1) the arrival time of the events is randomly selected assuming a Poisson statistic.
 2) for each event, the distance z to the source is randomly selected. The probability distribution is given by normalizing the differential cosmic star formation rate. 
3) the resulting signal is the sum of the individual strain amplitudes expressed in our frame.

The frequency domain strains $\widetilde{o}_{1}$ and $\widetilde{o}_{2} in the output of the two detectors are constructed as follow:
\begin {equation}
\widetilde{o}_{1}  = \widetilde{R}_{1}\widetilde{h}_{1}
\end {equation}
\begin {equation}
\widetilde{o}_{2}  = \widetilde{R}_{2}(\widetilde{h}_{1}\gamma + \widetilde{h}_{1}\sqrt{1-\gamma^{2}})
\end {equation}
where  $widetilde{h}_{i}$ is the FFT and $\widetilde{R}_{i}$  the response function of the ith detector. 
In the second equation,  $\gamma$ is the overlap reduction function.

Then the inverse FFTs give the whitened time series $o_{1}$ and $o_{2}$.

\subsubsection*{Uses}
\begin{verbatim}
LALForwardRealFFT()
LALReverseRealFFT()
LALOverlapReductionFunction()
LALUniformDeviate()
\end{verbatim}

\subsubsection*{Notes}

The cosmological model considered here corresponds to a flat Einstein de Sitter Universe with $\Omega_{matter}=1$, $\Omega_{vacuum}=0 and $h_{0}=0.5 but the code can be easily modified to take into account any cosmological model. 
The same for the cosmic star formation rate (Madau \& Pozetti, 1999, MNRAS, 312, 9).

</lalLaTeX> */

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <time.h>
#include <lal/LALStdlib.h>
#include <lal/LALConstants.h>
#include <lal/StochasticCrossCorrelation.h> 
#include <lal/AVFactories.h>
#include <lal/RealFFT.h>
#include <lal/ComplexFFT.h>
#include <lal/PrintFTSeries.h>
#include <lal/Units.h>
#include <lal/PrintVector.h>
#include <lal/Random.h>
#include <lal/DetectorSite.h>

#include "SimulatePopcorn.h"

NRCSID (SIMULATEPOPCORNC, "$Id$");


static void Rcfunc (REAL4 *Rc, REAL4 z);
static void dVfunc (REAL4 *dV, REAL4 z);
static void pzfunc (REAL4 *result, REAL4 z);
static void dLfunc (REAL4 *result, REAL4 z);

/*Madau cosmic star formation rate */
static void Rcfunc (REAL4 *result, REAL4 z)
{
  *result = (0.23*exp(3.4*z)/(44.7+exp(3.8*z)));
  return;
}

/*comovile volume element: flat Einstein de Sitter universe with omega_matter=1 */
static void dVfunc (REAL4 *result, REAL4 z)
{
  REAL4 zplus;
  zplus=1.+z;
  *result=(1.-(1./sqrt(zplus)))*(1.-(1./sqrt(zplus)))/sqrt(zplus*zplus*zplus);
  return;
}

/*probability density of z */
/*(dR/dz)/(Int[dR/dz,{z,0,5}]) where dR/dz = Rc*dV/(1+z) */

static void  pzfunc (REAL4 *result, REAL4 z)
{
  REAL4 dV, Rc;
  dVfunc(&dV,z);
  Rcfunc(&Rc,z);
  *result=315.917*Rc*dV/(1.+z);
  return;
}

/*distance luminosity: dL=(2c/Ho)(1+z)[1-(1+z)^-0.5] */
static void  dLfunc (REAL4 *result, REAL4 z)
 {  
   *result=12000.*(1.+z)*(1.-(1./sqrt(1.+z)));
   return;
 }


  
void
LALSimPopcornTimeSeries (  LALStatus                *status,
			   SimPopcornOutputStruc    *output,
			   SimPopcornInputStruc     *input,
			   SimPopcornParamsStruc     *params 
			   )

   
{
    
  /*time serie parameters */
  UINT4 length, srate, N, Nhalf, starttime;
  REAL8 deltat;
  REAL4Vector *h = NULL;
  REAL4Vector *hvec[2] = {NULL,NULL};

  /* frequency  serie parameters */
  UINT4 Nfreq;
  REAL8 f0=0.;
  REAL8 deltaf;    
  REAL8 fref; 
  REAL8 Href;
  COMPLEX8 resp0;
  COMPLEX8 resp1;
  COMPLEX8Vector *Hvec[2] = {NULL,NULL};

  /* source parameters*/  
  REAL4LALWform  *wformfunc;
  REAL4 wform, duration, lambda;


  /* others */
  UINT4 UE = 10000000;
  REAL4 devent;
  REAL4Vector *z = NULL;
  REAL4Vector *tevent = NULL;
  REAL4 t,x, ampl, dlum, norm;
   /* counters */ 
  UINT4 i, j, k, detect, dataset, Ndataset, nevent, inf, sup, jref;
  
  /* random generator */
  RandomParams *randParams=NULL;
  REAL4 alea1, alea2, reject;
  UINT4 seed;
 
  /* LAL structure needed as input/output for computing overlap 
     reduction function */
  LALDetectorPair detectors;
  REAL4FrequencySeries overlap;
  OverlapReductionFunctionParameters ORFparameters;
  REAL4 gamma;
  INT2 site0, site1;  
  /* Plans for FFTs and reverse FFTs */ 
  RealFFTPlan      *pfwd=NULL;  
  RealFFTPlan      *prev=NULL;  
  
  /* initialize status pointer */
  INITSTATUS (status, "LALSimPopcornTimeSeries",SIMULATEPOPCORNC );
  ATTATCHSTATUSPTR (status);  


  /***** check params/input/output  *****/

  /** output **/
  ASSERT(output !=NULL, status, 
        SIMULATEPOPCORNH_ENULLP,
        SIMULATEPOPCORNH_MSGENULLP);
  ASSERT(output->SimPopcorn0->data !=NULL, status, 
        SIMULATEPOPCORNH_ENULLP,
        SIMULATEPOPCORNH_MSGENULLP);
  ASSERT(output->SimPopcorn1->data !=NULL, status, 
        SIMULATEPOPCORNH_ENULLP,
        SIMULATEPOPCORNH_MSGENULLP);
  ASSERT(output->SimPopcorn0->data->data !=NULL, status, 
        SIMULATEPOPCORNH_ENULLP,
        SIMULATEPOPCORNH_MSGENULLP);
  ASSERT(output->SimPopcorn1->data->data !=NULL, status, 
        SIMULATEPOPCORNH_ENULLP,
        SIMULATEPOPCORNH_MSGENULLP);

  /** input **/
  ASSERT(input != NULL, status, 
        SIMULATEPOPCORNH_ENULLP,
        SIMULATEPOPCORNH_MSGENULLP);
  
  ASSERT(input->inputduration > 0, status, 
        SIMULATEPOPCORNH_EBV,
        SIMULATEPOPCORNH_MSGEBV);
  
  ASSERT(input->inputlambda > 0, status, 
        SIMULATEPOPCORNH_EBV,
        SIMULATEPOPCORNH_MSGEBV);
  
  /* First detector's complex response (whitening filter) */
  ASSERT(input->wfilter0 != NULL, status, 
        SIMULATEPOPCORNH_ENULLP,
        SIMULATEPOPCORNH_MSGENULLP);

  ASSERT(input->wfilter0->data != NULL, status, 
        SIMULATEPOPCORNH_ENULLP,
        SIMULATEPOPCORNH_MSGENULLP);
  
  ASSERT(input->wfilter0->data->data != NULL, status, 
        SIMULATEPOPCORNH_ENULLP,
        SIMULATEPOPCORNH_MSGENULLP);

  /* Second detector's complex response (whitening filter)  */
  ASSERT(input->wfilter1 != NULL, status, 
        SIMULATEPOPCORNH_ENULLP,
        SIMULATEPOPCORNH_MSGENULLP);

  ASSERT(input->wfilter1->data != NULL, status, 
        SIMULATEPOPCORNH_ENULLP,
        SIMULATEPOPCORNH_MSGENULLP);

  ASSERT(input->wfilter1->data->data != NULL, status, 
        SIMULATEPOPCORNH_ENULLP,
        SIMULATEPOPCORNH_MSGENULLP);

   
  /** parameters **/
  /* length of the time series is non-zero  */
  ASSERT(params->paramslength > 0, status, 
        SIMULATEPOPCORNH_EBV,
        SIMULATEPOPCORNH_MSGEBV);

  /* sample rate of the time series is non-zero */
  ASSERT(params->paramssrate > 0, status, 
        SIMULATEPOPCORNH_EBV,
	SIMULATEPOPCORNH_MSGEBV);
    
  /** read input parameters **/
  
  duration=input->inputduration;
  lambda=input->inputlambda;
  site0 = input->inputsite0;
  site1 = input->inputsite1;
  Ndataset = input->inputNdataset;
  wformfunc=input->inputwform;
  length = params->paramslength;
  starttime = params->paramsstarttime;
  srate = params->paramssrate;
  fref = params->paramsfref;
  seed = params->paramsseed;

  N=length*srate; Nhalf=N/2; Nfreq=Nhalf+1;
  deltat = 1./srate;  
  deltaf = 1./length;
   
 
  /** check for mismatches **/
  
  if (input->wfilter0->data->length != Nfreq) 
    {
      ABORT(status,
	   SIMULATEPOPCORNH_EMMLEN,
	   SIMULATEPOPCORNH_MSGEMMLEN);
    }
  if (input->wfilter1->data->length != Nfreq) 
    {
      ABORT(status,
	   SIMULATEPOPCORNH_EMMLEN,
	   SIMULATEPOPCORNH_MSGEMMLEN);
    }

     
  if (input->wfilter0->f0 != f0) 
    {
      ABORT(status,
	   SIMULATEPOPCORNH_ENONNULLFMIN,
	   SIMULATEPOPCORNH_MSGENONNULLFMIN);
    }
  if (input->wfilter1->f0 != f0) 
    {
      ABORT(status,
	   SIMULATEPOPCORNH_ENONNULLFMIN,
	   SIMULATEPOPCORNH_MSGENONNULLFMIN);
    }

  if (input->wfilter0->deltaF != deltaf) 
    {
      ABORT(status,
	   SIMULATEPOPCORNH_EMMDELTA,
	   SIMULATEPOPCORNH_MSGEMMDELTA);
    }
  if (input->wfilter1->deltaF != deltaf) 
    {
      ABORT(status,
	   SIMULATEPOPCORNH_EMMDELTA,
	   SIMULATEPOPCORNH_MSGEMMDELTA);
    }
  
  /*********** everything is O.K here  ***********/
    
  LALCreateForwardRealFFTPlan( status->statusPtr, &pfwd, N, 0 );
  LALCreateReverseRealFFTPlan( status->statusPtr, &prev, N, 0 );
  LALSCreateVector( status->statusPtr, &h, N );
  
  
  for (detect=0;detect<2;detect++) {
  LALSCreateVector( status->statusPtr, &hvec[detect], N );
  LALCCreateVector( status->statusPtr, &Hvec[detect], Nfreq );}
  
  LALSCreateVector( status->statusPtr, &z, UE );
  LALSCreateVector( status->statusPtr, &tevent, UE );

  LALCreateRandomParams(status->statusPtr, &randParams, seed );
   
  for (dataset=0;dataset<Ndataset;dataset++){  
  
  i=0;tevent->data[0]=-1.*duration;nevent=0;
  while(tevent->data[i]<length+0.5*duration)
   { 
     i++;   
     /*time interval between events (Poisson statistic)  */
     LALUniformDeviate( status->statusPtr, &alea1, randParams);
     devent=-lambda*log(alea1);
     tevent->data[i]=tevent->data[i-1]+devent;
     
     /*z */
     do { 
     LALUniformDeviate( status->statusPtr, &alea1, randParams);
     LALUniformDeviate( status->statusPtr, &alea2, randParams);
     pzfunc(&reject,5.*alea1);}
     while (alea2>reject);
     z->data[i]=5.*alea1;  
   }   
  nevent=i;
  


/*strain amplitude in the time domain   */

  inf=1;sup=1;
  for (j=0;j<N;j++){hvec[dataset]->data[j]=0.;}  
  for (j=0;j<N;j++)
   {
     t=j*deltat;
     ampl=0.;
     while((t-tevent->data[inf])>duration){inf++;}
     while((tevent->data[sup]-t)<=0.5*duration){sup++;}
     for(k=inf;k<sup;k++)
       {
	x=(t-tevent->data[k])/(z->data[k]+1.);
        dLfunc(&dlum,z->data[k]);
        wformfunc(&wform,x);
        hvec[dataset]->data[j]=(wform/dlum)+hvec[dataset]->data[j];
       }
   }

   /* Fourier transform */
   LALForwardRealFFT( status->statusPtr, Hvec[dataset], hvec[dataset], pfwd ); 
   }

   LALSDestroyVector(status->statusPtr,&z);
   LALSDestroyVector(status->statusPtr,&tevent);
   LALDestroyRandomParams(status->statusPtr,&randParams);

   if (fref==-1.) {norm=1.;}
    else {

    if (fref < 0.) 
     {
      ABORT(status,
	   SIMULATEPOPCORNH_EBV,
	   SIMULATEPOPCORNH_MSGEBV);
     }
     
     jref=fref*length;
     if (Ndataset==2) {
      Href=0.5*(sqrt(Hvec[0]->data[jref].re*Hvec[0]->data[jref].re
                  +Hvec[0]->data[jref].im*Hvec[0]->data[jref].im)
             +sqrt(Hvec[1]->data[jref].re*Hvec[1]->data[jref].re
                  +Hvec[1]->data[jref].im*Hvec[1]->data[jref].im));}
     else { 
      Href=sqrt(Hvec[0]->data[jref].re*Hvec[0]->data[jref].re
             +Hvec[0]->data[jref].im*Hvec[0]->data[jref].im);} 
   
     norm=2.E-19*sqrt(length)*sqrt(fref*fref*fref)/Href;
     }
 
    for (detect=0;detect<2;detect++)
      for (j=0;j<Nfreq;j++) {
	Hvec[detect]->data[j].re= Hvec[detect]->data[j].re*norm;
        Hvec[detect]->data[j].im= Hvec[detect]->data[j].im*norm;}
      
   
   if (Ndataset==2)
    {
      overlap.data = NULL;
      LALSCreateVector(status->statusPtr, &(overlap.data),Nfreq);
      ORFparameters.length = Nfreq;
      ORFparameters.f0 = f0;
      ORFparameters.deltaF = deltaf;
      detectors.detectorOne  = lalCachedDetectors[site0];
      detectors.detectorTwo  = lalCachedDetectors[site1];
      
      LALOverlapReductionFunction(status->statusPtr,&overlap,
                                  &detectors,&ORFparameters);

      for (j=0;j<Nfreq;j++)
       {
        resp0 = input->wfilter0->data->data[j];
        resp1 = input->wfilter1->data->data[j];
	gamma=overlap.data->data[j]; 
        Hvec[1]->data[j].re=(Hvec[0]->data[j].re*gamma 
                     +sqrt(1-gamma*gamma)*Hvec[1]->data[j].re)*resp1.re;
        
        Hvec[1]->data[j].im=(Hvec[1]->data[j].im*gamma
                     +sqrt(1-gamma*gamma)*Hvec[1]->data[j].im)*resp1.im;
        Hvec[0]->data[j].re=Hvec[0]->data[j].re*resp0.re;
        Hvec[0]->data[j].im=Hvec[0]->data[j].im*resp0.im;
        }
       LALSDestroyVector(status->statusPtr, &(overlap.data));   
    }
   else {
     for (j=0;j<Nfreq;j++)
      {
       resp0 = input->wfilter0->data->data[j];
       resp1 = input->wfilter1->data->data[j];
       Hvec[0]->data[j].re=Hvec[0]->data[j].re*resp0.re;
       Hvec[0]->data[j].im=Hvec[0]->data[j].im*resp0.im;
       Hvec[1]->data[j].re=Hvec[0]->data[j].re*resp1.re;
       Hvec[1]->data[j].im=Hvec[1]->data[j].im*resp1.im;
      }}
    
   
    /* Inverse Fourier transform */
   for (detect=0;detect<2;detect++)
     LALReverseRealFFT( status->statusPtr, hvec[detect], Hvec[detect], prev );
   
   
   LALDestroyRealFFTPlan( status->statusPtr, &pfwd );
   LALDestroyRealFFTPlan( status->statusPtr, &prev );

  /* assign parameters and data to output */
  
   for (j=0;j<N;j++) {
     output->SimPopcorn0->data->data[j] = (hvec[0]->data[j])/N;
     output->SimPopcorn1->data->data[j] = (hvec[1]->data[j])/N;}

   output->SimPopcorn0->f0 = f0;
   output->SimPopcorn0->deltaT = deltat;
   output->SimPopcorn0->epoch.gpsSeconds = starttime;
   output->SimPopcorn0->epoch.gpsNanoSeconds = 0;
   output->SimPopcorn0->sampleUnits = lalADCCountUnit;
   /*strncpy(output->SimPopcorn0->name,"Popcorn0", LALNameLength); */
   
   output->SimPopcorn1->f0 = f0;
   output->SimPopcorn1->deltaT = deltat;
   output->SimPopcorn1->epoch.gpsSeconds = starttime;
   output->SimPopcorn1->epoch.gpsNanoSeconds = 0;
   output->SimPopcorn1->sampleUnits = lalADCCountUnit;
   /*strncpy(output->SimPopcorn1->name,"Popcorn1", LALNameLength); */


  for (detect=0;detect<2;detect++) {
  LALSDestroyVector(status->statusPtr,&hvec[detect]);
  LALCDestroyVector(status->statusPtr,&Hvec[detect]);}
 
  CHECKSTATUSPTR (status);
  DETATCHSTATUSPTR (status);
  RETURN (status);
 
  
}


  
