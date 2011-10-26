////////////////////////////////////////////////////////////////////////////////
/// @brief geo index
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2010-2011 triagens GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
///
/// @author R. A. Parker
/// @author Copyright 2011-2010, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

/* GeoIndex.c -   GeoIndex algorithms                */
/* Version 1.0 24.9.2011 R. A. Parker                */

#include "GeoIndex.h"

    /* Radius of the earth used for distances  */
#define EARTHRADIUS 6371000.0

    /* geostring used to indicate end of free chain */
#define GEOENDING -9000000000LL

    /* How big an "empty" index should be     */
#define GEOSTART 4

    /* How many results to allow for internally   */
    /* at the start of a "within distance" search  */
    /* (it grows if it needs to so is not critical */
#define GEORESULTSTART 100

typedef struct
{
    int pointsct;
    int allocpoints;
    double maxdistance;
    int * slot;
    double * distance;
}    GeoResults;

typedef struct
{
    GeoIndex * gi;
    GeoResults * gr;
    int topofstack;
    int startslotid[32];
    int endslotid[32];
    GeoString targetgs;
    double targetlat;
    double targetlong;
}   GeoStack;

/* Create a new GeoIndex for the user  */

GeoIndex * GeoIndex_new(void)
{
    GeoIndex * gi;
    int i;

    gi = malloc(sizeof(GeoIndex));
    if(gi==NULL) return gi;
    gi->slotct     = GEOSTART;

/* try to allocate all the things we need  */
    gi->sortslot   = malloc(GEOSTART*sizeof(int));
    gi->geostrings = malloc(GEOSTART*sizeof(GeoString));
    gi->gc         = malloc(GEOSTART*sizeof(GeoCoordinate));

/* if any of them fail, free the ones that succeeded  */
/* and then return the NULL pointer for our user      */
    if ( ( gi->sortslot   == NULL) ||
         ( gi->geostrings == NULL) ||
         ( gi->gc         == NULL) )
    {
        if ( gi->sortslot   != NULL) free(gi->sortslot);
        if ( gi->geostrings != NULL) free(gi->geostrings);
        if ( gi->gc         != NULL) free(gi->gc);
        free(gi);
        return NULL;
    }

/* initialize chain of empty slots  */
    for(i=0;i<GEOSTART-1;i++) gi->geostrings[i]=-(i+1);
    gi->geostrings[GEOSTART-1]=GEOENDING;

/* set occslots to say there are no occupied slots        */
/* and therefore no used entries in the sortslots either  */
    gi->occslots=0;
    return gi;
}

void GeoIndex_free(GeoIndex * gi)
{
    free(gi->gc);
    free(gi->geostrings);
    free(gi->sortslot);
    free(gi);
}

double GeoIndex_distance(GeoCoordinate * c1, GeoCoordinate * c2)
{
    double x1,y1,z1,x2,y2,z2,mole;
    z1=sin(c1->latitude*M_PI/180.0);
    x1=cos(c1->latitude*M_PI/180.0)*cos(c1->longitude*M_PI/180.0);
    y1=cos(c1->latitude*M_PI/180.0)*sin(c1->longitude*M_PI/180.0);
    z2=sin(c2->latitude*M_PI/180.0);
    x2=cos(c2->latitude*M_PI/180.0)*cos(c2->longitude*M_PI/180.0);
    y2=cos(c2->latitude*M_PI/180.0)*sin(c2->longitude*M_PI/180.0);
    mole=sqrt((x1-x2)*(x1-x2) + (y1-y2)*(y1-y2) + (z1-z2)*(z1-z2));
    if(mole >  2.0) mole = 2.0; /* make sure arcsin succeeds! */
    return 2.0 * EARTHRADIUS * asin(mole/2.0);
}

void GeoIndexFreeSlot(GeoIndex * gi, int slot)
{
    gi->geostrings[slot]=gi->geostrings[0];
    gi->geostrings[0]= (GeoString) -slot;
}

int GeoIndexNewSlot(GeoIndex * gi)
{
    long long x, y;
    GeoString * gs;
    GeoCoordinate * gc;
    int * ss;
    int newslotct,j;
    if(gi->geostrings[0]==GEOENDING)
    {
/* do the growth calculation in long long to make sure it doesn't  */
/* overflow when the size gets to be near 2^31                     */
        x = gi->slotct;
        y=100+GeoIndexGROW;
        x=x*y + 99;
        y=100;
        x=x/y;
        if(x>2000000000L) return -2;
        newslotct= (int) x;
        gs = realloc(gi->geostrings,newslotct*sizeof(GeoString));
        gc = realloc(gi->gc,newslotct*sizeof(GeoCoordinate));
        ss = realloc(gi->sortslot,newslotct*sizeof(int));
        if(gs!=NULL) gi->geostrings=gs;
        if(gc!=NULL) gi->gc=gc;
        if(ss!=NULL) gi->sortslot=ss;
        if(gs==NULL) return -2;
        if(gc==NULL) return -2;
        if(ss==NULL) return -2;
        for(j=gi->slotct;j<newslotct;j++) GeoIndexFreeSlot(gi,j);
        gi->slotct=newslotct;
    }
    j= (int) -(gi->geostrings[0]);
    gi->geostrings[0]=gi->geostrings[j];
    return j;
}

    /* 2^25 / 90 rounded down.  Used to convert */
    /* degrees of longitude and latitude into   */
    /* integers for use making a GeoString      */
#define STRINGPERDEGREE 372827.0222

GeoString GeoMkString(GeoCoordinate * c)
{
    double x1,y1;
    int x,y,i;
    int xm,ym;
    GeoString z;
    xm=0x4000000;
    ym=0x2000000;
    x1=c->longitude+180.0;
    y1=c->latitude+90;
    x=(int) (x1*STRINGPERDEGREE);
    y=(int) (y1*STRINGPERDEGREE);
    z=0;
    for(i=0;i<26;i++)
    {
        z+= (x&xm) + (y&ym);
        z=z<<1;
        xm=xm>>1;
        ym=ym>>1;
    }
    z+= (x&xm);
    return z;
}

void GeoStackSet (GeoIndex * gi, GeoStack * gk, 
        GeoCoordinate * c, GeoResults * gr)
{
    gk->gi = gi;
    gk->gr = gr;

    if(gi->occslots==0)
    {
        gk->topofstack=-1;
    }
    else
    {
        gk->topofstack = 0;
        gk->startslotid[0]=0;
        gk->endslotid[0]=gi->occslots-1;
    }
    gk->targetgs = GeoMkString(c);
    gk->targetlong = c->longitude;
    gk->targetlat = c->latitude;
}

/*  Routine to look for the targetgs in the interval   */
/*  that lies at the top of the stack - usually the    */
/*  whole index, when the stack has just been set      */
/*  The returned "int" is the highest sortslot such    */
/*  that all higher ones are bigger GeoStrings         */
/*  Note - this may be one less than the bottom of the */
/*  interval, and may indeed be -1                     */

int GeoFind(GeoStack * gk)
{
    int sstop, ssbot, ssmid;
    GeoIndex * gi;
    if(gk->topofstack==-1) return -1;  /* empty stack */
    ssbot=gk->startslotid[gk->topofstack];
    sstop=gk->endslotid[gk->topofstack];
    gi=gk->gi;
    if(gk->targetgs < gi->geostrings[gi->sortslot[ssbot]])
        return ssbot-1;
    if(gk->targetgs >= gi->geostrings[gi->sortslot[sstop]])
        return sstop;
/* from now on, ssbot is always too low, sstop always high enough  */
    while(sstop>(ssbot+1))
    {
        ssmid=(sstop+ssbot)/2;
/*  if ssmid == sstop, it is ssbot that will be set to it         */
        if(gk->targetgs < gi->geostrings[gi->sortslot[ssmid]])
            sstop=ssmid;
        else 
            ssbot=ssmid;
    }
    return ssbot;
}

int GeoIndex_insert(GeoIndex * gi, GeoCoordinate * c)
{
    GeoStack gk;
    int topslot,putslot,slot,i;
    if(c->longitude < -180.0) return -3;
    if(c->longitude >  180.0) return -3;
    if(c->latitude  <  -90.0) return -3;
    if(c->latitude  >   90.0) return -3;
    GeoStackSet(gi, &gk, c, (GeoResults *)NULL);
    topslot = GeoFind(&gk);
    putslot = topslot+1;   /* we plan to put it here  */

/* there may be any number of equal entries here, so we must   */
/* test them to ensure that we do not do a duplicate insertion */
    while(topslot > -1)
    {
        slot=gi->sortslot[topslot];
        if(gi->geostrings[slot]>gk.targetgs) break;
        if(   (c->latitude  == ((gi->gc)[slot]).latitude)     &&
              (c->longitude == ((gi->gc)[slot]).longitude)    &&
              (c->data      == ((gi->gc)[slot]).data)  )  return -1;
        topslot--;            
    }
/* All OK.  we're going to try to put it in.  */
    slot=GeoIndexNewSlot(gi);
    if(slot==-2) return -2;     /* no room  :(  */
    gi->geostrings[slot]=gk.targetgs;
    (gi->gc)[slot].latitude = c->latitude;
    (gi->gc)[slot].longitude=c->longitude;
    (gi->gc)[slot].data=c->data;

/* shuffle up the sortslots to make room!  */
    for(i=gi->occslots;i>putslot;i--) 
        gi->sortslot[i] = gi->sortslot[i-1];
/* put it in  */
    gi->sortslot[putslot]=slot;
    gi->occslots++;
    return 0;
}

int GeoIndex_remove(GeoIndex * gi, GeoCoordinate * c)
{
    int sortslot,slot,i;
    GeoStack gk;
    GeoStackSet(gi, &gk, c, (GeoResults *) NULL);
    sortslot = GeoFind(&gk);

/* there may be any number of equal entries here, so we must   */
/* test them all to find the one we want                       */
    while(sortslot > -1)
    {
        slot=gi->sortslot[sortslot];
        if(gi->geostrings[slot]>gk.targetgs) return -1;
        if(   (c->latitude  == (gi->gc)[slot].latitude)    &&
              (c->longitude == (gi->gc)[slot].longitude)   &&
              (c->data      == (gi->gc)[slot].data)        )
        {
            GeoIndexFreeSlot(gi,slot);
            for(i=sortslot;i<gi->occslots-1;i++) 
                gi->sortslot[i] = gi->sortslot[i+1];
            gi->occslots--;
            return 0;
        }
        sortslot--;            
    }
    return -1;
}

GeoResults * GeoResultsCons(int alloc)
{
    GeoResults * gres;
    int * sa;
    double * dd;
    gres=malloc(sizeof(GeoResults));
    sa=malloc(alloc*sizeof(int));
    dd=malloc(alloc*sizeof(double));
    if( (gres==NULL) ||
         (sa==NULL)  ||
         (dd==NULL)   )
    {
        if(gres!=NULL) free(gres);
        if(sa!=NULL)   free(sa);
        if(dd!=NULL)   free(dd);
        return NULL;
    }
    gres->pointsct = 0;
    gres->allocpoints = alloc;
    gres->slot = sa;
    gres->distance = dd;
/* no need to initialize maxdistance */
    return gres;
}

int GeoResultsGrow(GeoResults * gr)
{
    int newsiz;
    int * sa;
    double * dd;
    if(gr->pointsct < gr->allocpoints) return 0;
        /* otherwise grow by about 50%  */
    newsiz=gr->pointsct + (gr->pointsct/2) + 1;
    if(newsiz > 1000000000) return -1;
    sa=realloc(gr->slot, newsiz*sizeof(int));
    dd=realloc(gr->distance, newsiz*sizeof(double));
    if( (sa==NULL) || (dd==NULL) )
    {
        if(sa!=NULL) gr->slot = sa;
        if(dd!=NULL) gr->distance = dd;
        return -1;
    }
    gr->slot = sa;
    gr->distance = dd;
    gr->allocpoints = newsiz;
    return 0;
}

GeoCoordinates * GeoAnswers (GeoIndex * gi, GeoResults * gr)
{
    GeoCoordinates * ans;
    GeoCoordinate  * gc;
    int i;
    ans = malloc(sizeof(GeoCoordinates));;
    gc  = malloc(gr->pointsct * sizeof(GeoCoordinate));
    if( (ans==NULL) || (gc==NULL) )
    {
        if(ans!=NULL) free(ans);
        if(gc!=NULL) free(gc);
        free(gr->slot);
        free(gr->distance);
        free(gr);
        return NULL;
    }
    ans->length = gr->pointsct;
    ans->coordinates = gc;
    ans->distances = gr->distance;
    for(i=0;i<gr->pointsct;i++)
    {
        ans->coordinates[i].latitude =
                     (gi->gc)[gr->slot[i]].latitude;
        ans->coordinates[i].longitude = 
                     (gi->gc)[gr->slot[i]].longitude;
        ans->coordinates[i].data =
                     (gi->gc)[gr->slot[i]].data;
    }
    free(gr->slot);
    free(gr);
    return ans;
}

/* "Reverse-Engineer" a GeoString - convert it into     */
/*  the given GeoCoordinate                             */

void GeoReverse(GeoString gs, GeoCoordinate * gc)
{
    int lon,lat;
    int i,j,k;
    double d;
    j=0;
    lon=0;
    lat=0;

/* un-interleave the bits of the GeoString   */

    for(i=0;i<27;i++)
    {
        k=(gs>>j)&3;
        lon+=(k&1)<<i;
        k>>=1;
        lat+=(k&1)<<i;
        j+=2;
    }

/* convert each integer to a double and convert to degrees  */

    d=lon;
    gc->longitude=(d/STRINGPERDEGREE)-180.0;
    d=lat;
    gc->latitude=(d/STRINGPERDEGREE)-90.0;
}

double GeoMinDistance(GeoStack * gk, GeoString k1, GeoString k2)
{
    GeoString xor,msb,mst;
    int bits;
    GeoCoordinate d1, d2;

    double md,md1;
    double latdiff;
    double lon1, lon2;
    double x,y,phi;
    GeoCoordinate c1, c2;
/* if the point is in the interval, fast answer clearly zero   */
    if(  (k1<=gk->targetgs) && (k2>=gk->targetgs) ) return 0.0;

/* compute mst - (top) mask for bits where k1 and k2 agree            */
/*         msb - (bottom) mask for lower bits where they may differ   */

    xor = k1 ^ k2;
    bits=0;
    while(xor!=0)
    {
        bits++;
        xor>>=1;
    }
    msb=(0xFFFFFFFFFFFFFFLL)>>(56-bits);
    mst=(0xFFFFFFFFFFFFFFLL)^msb;
/* compute d1 as coordinates of lower corner  */
    GeoReverse(k1&mst,&d1);
/* and d2 as coordinates of upper corner      */
    GeoReverse((k1&mst)+msb,&d2);
/* d1 negative d2 positive values  */
    if( (d1.longitude<=gk->targetlong) && (d2.longitude>=gk->targetlong) )
    {
        if(d1.latitude<=gk->targetlat)
        {
            if(d2.latitude>=gk->targetlat)
                return 0.0;        /* in the box  */
            latdiff=d2.latitude-gk->targetlat;
        }
        else latdiff=d1.latitude-gk->targetlat;
        md = EARTHRADIUS*M_PI*fabs(latdiff)/180.0 - 3.0;
        if(md<0.0) md=0.0;
        return md;
    }
    lon1=fabs(d1.longitude-gk->targetlong);
    lon2=fabs(d2.longitude-gk->targetlong);
    if(lon1>180.0) lon1=360.0-lon1;
    if(lon2>180.0) lon2=360.0-lon2;
    if(lon2<lon1) lon1=lon2;
    if(lon1<90.0)
    {
        c1.longitude=0.0;
        c2.longitude=lon1;
        c1.latitude=gk->targetlat;
        x=cos(gk->targetlat*M_PI/180.0)*cos(lon1*M_PI/180.0);
        y=sin(gk->targetlat*M_PI/180.0);
        phi=atan2(y,x)*180.0/M_PI;
        if( phi>=d1.latitude )
        {
            if(phi<=d2.latitude)  c2.latitude=phi;
            else            c2.latitude=d2.latitude;
        }
        else                c2.latitude=d1.latitude;
        md=GeoIndex_distance(&c1,&c2) - 2.0;
        if(md<0.0) md=0.0;
        return md; 
    }
    c1.longitude=0.0;
    c2.longitude=lon1;
    c2.latitude=gk->targetlat;
    c1.latitude=d1.latitude;
    md=GeoIndex_distance(&c1,&c2) - 2.0;
    c1.latitude=d2.latitude;
    md1=GeoIndex_distance(&c1,&c2) - 2.0;
    if(md1<md) md=md1;
    if(md<0.0) md=0.0;
    return md;
}

GeoCoordinates * GeoIndex_PointsWithinRadius(GeoIndex * gi,
                    GeoCoordinate * c, double d)
{
    GeoStack gk;
    GeoCoordinates * answer;
    GeoResults * gres;
    double dist;
    int i,slot,r;
    int sstop, ssbot, ssmid;
    GeoString gstop, gsbot;
    gres=GeoResultsCons(100);
    if(gres==NULL) return NULL;
    GeoStackSet(gi,&gk,c,gres);
    while (gk.topofstack > -1)
    {
        ssbot=gk.startslotid[gk.topofstack];
        gsbot=gi->geostrings[gi->sortslot[ssbot]];
        sstop=gk.endslotid[gk.topofstack];
        gstop=gi->geostrings[gi->sortslot[sstop]];
        dist=GeoMinDistance(&gk,gsbot,gstop);
        if(dist>d)
        {
            gk.topofstack--;
            continue;
        }
        if(  (sstop-ssbot) < GeoIndexSMALLINTERVAL )
        {
            for(i=ssbot;i<=sstop;i++)
            {
                slot=gi->sortslot[i];
                dist=GeoIndex_distance(c,gi->gc + slot);
                if(dist<=d)
                {
                    r=GeoResultsGrow(gres);
                    if(r==-1)
                    {
                        free(gres->distance);
                        free(gres->slot);
                        free(gres);
                        return NULL;
                    }
                    gres->slot[gres->pointsct]=slot;
                    gres->distance[gres->pointsct]=dist;
                    gres->pointsct++;
                }        
            }
            gk.topofstack--;
            continue;
        }
        ssmid=(sstop+ssbot)/2;
        gk.startslotid[gk.topofstack]=ssbot;
        gk.endslotid[gk.topofstack]=ssmid;

        gk.topofstack++;
        gk.startslotid[gk.topofstack]=ssmid+1;
        gk.endslotid[gk.topofstack]=sstop;
 
    }
    answer=GeoAnswers(gi,gres);
    return answer;   /* note - this may be NULL  */
}

GeoCoordinates * GeoIndex_NearestCountPoints(GeoIndex * gi,
                    GeoCoordinate * c, int count)
{
    GeoResults * gres;
    GeoCoordinates * answer;
    GeoStack gk;
    GeoString gsbot, gstop;
    int ssbot, sstop, ssmid, slot, i, k;
    double maxdist, dist, newmaxdist;
    gres=GeoResultsCons(count);
    if(gres==NULL) return NULL;
    GeoStackSet(gi,&gk,c,gres);
/* Phase 1 - get initial stack with nearer intervals maybe at top */
    while (gk.topofstack > -1)  /*allowing for an empty index  */
    {
        ssbot=gk.startslotid[gk.topofstack];
        sstop=gk.endslotid[gk.topofstack];
        if(  (sstop-ssbot) < GeoIndexSMALLINTERVAL ) break;
        ssmid=(sstop+ssbot)/2;
        slot=gi->sortslot[ssmid];
        if(gi->geostrings[slot] > gk.targetgs)
        {
            gk.startslotid[gk.topofstack]=ssbot;
            gk.endslotid[gk.topofstack]=ssmid;
            gk.topofstack++;
            gk.startslotid[gk.topofstack]=ssmid+1;
            gk.endslotid[gk.topofstack]=sstop;
        }
        else
        {
            gk.startslotid[gk.topofstack]=ssmid+1;
            gk.endslotid[gk.topofstack]=sstop;
            gk.topofstack++;
            gk.startslotid[gk.topofstack]=ssbot;
            gk.endslotid[gk.topofstack]=ssmid;
        }
    }

/* Phase 2 - populate the results list with first points found  */
    maxdist=0.0;    /* maximum distance = low-values            */
    while (gk.topofstack > -1)  /* allowing for early emptying  */
    {
        ssbot=gk.startslotid[gk.topofstack];
        sstop=gk.endslotid[gk.topofstack];
        slot=gi->sortslot[ssbot];
        dist=GeoIndex_distance(c,gi->gc + slot);
        if(dist>maxdist) maxdist=dist;
        gres->slot[gres->pointsct]=slot;
        gres->distance[gres->pointsct]=dist;
        gres->pointsct++;
        ssbot++;
        if(ssbot>sstop)
        {
            gk.topofstack--;
        }
        else
        {
            gk.startslotid[gk.topofstack]=ssbot;
        }
        if(gres->pointsct>=count) break;   /* normal termination  */
    }
/* Phase 3 - put in the rest of the points as needed  */
/* maxdist is the largest value in the list, so we must beat that  */
    while (gk.topofstack > -1)  /* done when stack empty  */
    {
        ssbot=gk.startslotid[gk.topofstack];
        gsbot=gi->geostrings[gi->sortslot[ssbot]];
        sstop=gk.endslotid[gk.topofstack];
        gstop=gi->geostrings[gi->sortslot[sstop]];
        dist=GeoMinDistance(&gk,gsbot,gstop);
        if(dist>=maxdist)
        {
            gk.topofstack--;    /* throw away the whole interval  */
            continue;
        }
        if(  (sstop-ssbot) < GeoIndexSMALLINTERVAL )
        {
            for(i=ssbot;i<=sstop;i++)
            {
                slot=gi->sortslot[i];
                dist=GeoIndex_distance(c,gi->gc + slot);
                if(dist<=maxdist)
                {
                    newmaxdist=0.0;
                    for(k=0;k<count;k++)
                    {
                        if(gres->distance[k] == maxdist)
                        {
                            gres->slot[k]=slot;
                            gres->distance[k]=dist;
                            dist=1000000000.0; /* put in just once */
                        }
                        if(gres->distance[k]>newmaxdist)
                            newmaxdist=gres->distance[k];
                    }
                    maxdist=newmaxdist;
                }        
            }
            gk.topofstack--;
            continue;
        }
/* maybe an argument for putting the nearest one on top  ??  */
        ssmid=(sstop+ssbot)/2;
        gk.startslotid[gk.topofstack]=ssbot;
        gk.endslotid[gk.topofstack]=ssmid;
        gk.topofstack++;
        gk.startslotid[gk.topofstack]=ssmid+1;
        gk.endslotid[gk.topofstack]=sstop;
    }
    answer=GeoAnswers(gi,gres);
    return answer;     /* Note - this may be NULL  */
}

void GeoIndex_CoordinatesFree(GeoCoordinates * clist)
{
    free(clist->coordinates);
    free(clist->distances);
    free(clist);
}

/* the following code can all be deleted if required  */

#ifdef DEBUG

void GeoIndex_INDEXDUMP (GeoIndex * gi, FILE * f)
{
    int i,j,k,dig;
    GeoString gs;
    fprintf(f,"Maximum Slots currently %d\n",gi->slotct);
    fprintf(f,"Number of entries in the index = %d,",gi->occslots);
    fprintf(f,"\n");
    fprintf(f,"Latitude   Longitude      GeoString        ");
#if DEBUG == 2
    fprintf(f,"    Data");
#endif
    fprintf(f,"\n");
    for(i=0;i<gi->occslots;i++)
    {
        j=gi->sortslot[i];
        fprintf(f,"%7.2f   %7.2f       ",
                     gi->gc[j].latitude,gi->gc[j].longitude);
        gs=gi->geostrings[j];
        for(k=0;k<18;k++)
        {
            dig=(gs>>58)&15;
            fprintf(f,"%x",dig);
            gs<<=4;
        }
#if DEBUG == 2
        fprintf(f,"    %s",(char *)gi->gc[j].data);
#endif
        fprintf(f,"\n");
    }
}

int GeoIndex_INDEXVALID(GeoIndex * gi)
{
    int i,j;
    GeoString gs,freeslot;
    for(i=0;i<gi->occslots;i++)
    {
        j=gi->sortslot[i];
        gs = GeoMkString(&(gi->gc[j]));
        if(gi->geostrings[j]!=gs) 
            return -1;
    }
    gs=0;
    for(i=0;i<gi->occslots;i++)
    {
        j=gi->sortslot[i];
        if(gi->geostrings[j]<gs)
            return -2;
        gs=gi->geostrings[j];
    }
    freeslot=0;
    for(i=gi->occslots+1;i<gi->slotct;i++)
        freeslot=-gi->geostrings[freeslot];
    if(gi->geostrings[freeslot]!=GEOENDING) 
        return -3;
    return 0;
}

#endif

/* end of GeoIndex.c  */
