/*
 **********************************************************************
 * Copyright (c) 2011,International Business Machines
 * Corporation and others.  All Rights Reserved.
 **********************************************************************
 */

#include "unicode/utimer.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>

#include "sieve.h"

/* prime number sieve */

U_CAPI double uprv_calcSieveTime() {
#if 1
#define SIEVE_SIZE U_LOTS_OF_TIMES /* standardized size */
#else
#define SIEVE_SIZE  <something_smaller>
#endif

#define SIEVE_PRINT 0

  char sieve[SIEVE_SIZE];
  UTimer a,b;
  int i,k;
  
  utimer_getTime(&a);
  for(int j=0;j<SIEVE_SIZE;j++) {
    sieve[j]=1;
  }
  sieve[0]=0;
  utimer_getTime(&b);
  

#if SIEVE_PRINT
  printf("init %d: %.9f\n", SIEVE_SIZE,utimer_getDeltaSeconds(&a,&b));
#endif

  utimer_getTime(&a);
  for(i=2;i<SIEVE_SIZE/2;i++) {
    for(k=i*2;k<SIEVE_SIZE;k+=i) {
      sieve[k]=0;
    }
  }
  utimer_getTime(&b);
#if SIEVE_PRINT
  printf("sieve %d: %.9f\n", SIEVE_SIZE,utimer_getDeltaSeconds(&a,&b));

  if(SIEVE_PRINT>0) {
    k=0;
    for(i=2;i<SIEVE_SIZE && k<SIEVE_PRINT;i++) {
      if(sieve[i]) {
        printf("%d ", i);
        k++;
      }
    }
    puts("");
  }  
  {
    k=0;
    for(i=0;i<SIEVE_SIZE;i++) {
      if(sieve[i]) k++;
    }
    printf("Primes: %d\n", k);
  }
#endif

  return utimer_getDeltaSeconds(&a,&b);
}
static int comdoub(const void *aa, const void *bb) 
{
  const double *a = (const double*)aa;
  const double *b = (const double*)bb;
  
  return (*a==*b)?0:((*a<*b)?-1:1);
}

double midpoint(double *times, double i, int n) {
  double fl = floor(i);
  double ce = ceil(i);
  if(ce>=n) ce=n;
  if(fl==ce) {
    return times[(int)fl];
  } else {
    return (times[(int)fl]+times[(int)ce])/2;
  }
}

double medianof(double *times, int n, int type) {
  switch(type) {
  case 1:
    return midpoint(times,n/4,n);
  case 2:
    return midpoint(times,n/2,n);
  case 3:
    return midpoint(times,(n/2)+(n/4),n);
  }
  return -1;
}

double qs(double *times, int n, double *q1, double *q2, double *q3) {
  *q1 = medianof(times,n,1);
  *q2 = medianof(times,n,2);
  *q3 = medianof(times,n,3);
  return *q3-*q1;
}

U_CAPI double uprv_getMeanTime(double *times, uint32_t timeCount, double *marginOfError) {
  double q1,q2,q3;
  int n = timeCount;

  /* calculate medians */
  qsort(times,n,sizeof(times[0]),comdoub);
  double iqr = qs(times,n,&q1,&q2,&q3);
  double rangeMin=  (q1-(1.5*iqr));
  double rangeMax =  (q3+(1.5*iqr));

  /* Throw out outliers */
  int newN = n;
#if U_DEBUG
  printf("iqr: %.9f, q1=%.9f, q2=%.9f, q3=%.9f, max=%.9f, n=%d\n", iqr,q1,q2,q3,(double)-1, n);
#endif
  for(int i=0;i<newN;i++) {
    if(times[i]<rangeMin || times[i]>rangeMax) {
#if U_DEBUG
      printf("Knocking out: %.9f from [%.9f:%.9f]\n", times[i], rangeMin, rangeMax);
#endif
      times[i--] = times[--newN]; // bring down a new value
    }
  }

  /* if we removed any outliers, recalculate iqr */
  if(newN<n) {
#if U_DEBUG
    printf("Kicked out %d, retrying..\n", n-newN);
#endif
    n = newN;

    qsort(times,n,sizeof(times[0]),comdoub);
    double iqr = qs(times,n,&q1,&q2,&q3);
    rangeMin=  (q1-(1.5*iqr));
    rangeMax =  (q3+(1.5*iqr));
  }

  /* calculate min/max and mean */
  double minTime = times[0];
  double maxTime = times[0];
  double meanTime = times[0];
  for(int i=1;i<n;i++) {
    if(minTime>times[i]) minTime=times[i];
    if(maxTime<times[i]) maxTime=times[i];
    meanTime+=times[i];
  }
  meanTime /= n;

  /* caculate standard deviation */
  double sd = 0;
  for(int i=0;i<n;i++) {
#if U_DEBUG
    printf("  %d: %.9f\n", i, times[i]);
#endif
    sd += (times[i]-meanTime)*(times[i]-meanTime);
  }
  sd = sqrt(sd/(n-1));

#if U_DEBUG
  printf("sd: %.9f, mean: %.9f\n", sd, meanTime);
  printf("min: %.9f, q1=%.9f, q2=%.9f, q3=%.9f, max=%.9f, n=%d\n", minTime,q1,q2,q3,maxTime, n);
  printf("iqr/sd = %.9f\n", iqr/sd);
#endif

  /* 1.960 = z sub 0.025 */
  *marginOfError = 1.960 * (sd/sqrt(n));
  /*printf("Margin of Error = %.4f (95%% confidence)\n", me);*/

  return meanTime;
}

UBool calcSieveTime = FALSE;
double meanSieveTime = 0.0;
double meanSieveME = 0.0;

U_CAPI double uprv_getSieveTime(double *marginOfError) {
  if(calcSieveTime==FALSE) {
#define SAMPLES 50
    double times[SAMPLES];
    
    for(int i=0;i<SAMPLES;i++) {
      times[i] = uprv_calcSieveTime();
#if U_DEBUG
      printf("#%d/%d: %.9f\n", i,SAMPLES, times[i]);
#endif
    }
    
    meanSieveTime = uprv_getMeanTime(times, SAMPLES,&meanSieveME);
    calcSieveTime=TRUE;
  }
  if(marginOfError!=NULL) {
    *marginOfError = meanSieveME;
  }
  return meanSieveTime;
}
