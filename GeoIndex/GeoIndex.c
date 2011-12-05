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
/// @author R. A. Parker
/// @author Copyright 2011, triagens GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

/* GeoIndex.c -   GeoIndex algorithms                */
/* Version 2.0 3.12.2011 R. A. Parker                */

#ifdef GEO_TRIAGENS
#include "GeoIndex/GeoIndex.h"
#else
#include "GeoIndex2.h"
#endif

    /* Radius of the earth used for distances  */
#define EARTHRADIUS 6371000.0

    /* How many results to allow for internally   */
    /* at the start of a "within distance" search  */
    /* (it grows if it needs to so is not critical */
#define GEORESULTSTART 100000

#define GEOSLOTSTART 50
#define GEOPOTSTART 100

typedef struct
{
    GeoIndex * gi;
    GeoCoordinate * gc;
    double x;
    double y;
    double z;
    GeoString gs;
    GeoFix fixdist[GeoIndexFIXEDPOINTS];
    double snmd;
    GeoFix distrej[GeoIndexFIXEDPOINTS];
}    GeoDetailedPoint;

typedef struct
{
    int pointsct;
    int allocpoints;
    int * slot;
    double * snmd;
}    GeoResults;


typedef struct
{
    GeoResults * gr;
    GeoDetailedPoint * gd;
    int stacksize;
    int potid[46];
}   GeoStack;


typedef struct
{
    GeoIndex * gi;
    int pathlength;
    int path[46];
}    GeoPath;

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

void GeoIndexFreePot(GeoIndex * gi, int pot)
{
    gi->pots[pot].LorLeaf=gi->pots[0].LorLeaf;
    gi->pots[0].LorLeaf = pot;
}

int GeoIndexNewPot(GeoIndex * gi)
{
    int newpotct,j;
    long long x,y;
    GeoPot * gp;
    if( gi->pots[0].LorLeaf == 0)
    {
/* do the growth calculation in long long to make sure it doesn't  */
/* overflow when the size gets to be near 2^31                     */
        x = gi->potct;
        y=100+GeoIndexGROW;
        x=x*y + 99;
        y=100;
        x=x/y;
        if(x>1000000000L) return -2;
        newpotct= (int) x;
        gp = realloc(gi->pots,newpotct*sizeof(GeoPot));
        if(gp!=NULL) gi->pots=gp;
            else     return -2;
        for(j=gi->potct;j<newpotct;j++) GeoIndexFreePot(gi,j);
        gi->potct=newpotct;
    }
    j= gi->pots[0].LorLeaf;
    gi->pots[0].LorLeaf=gi->pots[j].LorLeaf;
    return j;
}

GeoIndex * GeoIndex_new(void)
{
    GeoIndex * gi;
    int i,j;
    double lat, lon, x, y, z;

    gi = malloc(sizeof(GeoIndex));
    if(gi==NULL) return gi;
/* try to allocate all the things we need  */
    gi->pots       = malloc(GEOPOTSTART*sizeof(GeoPot));
    gi->gc         = malloc(GEOSLOTSTART*sizeof(GeoCoordinate));

/* if any of them fail, free the ones that succeeded  */
/* and then return the NULL pointer for our user      */
    if ( ( gi->pots      == NULL) ||
         ( gi->gc         == NULL) )
    {
        if ( gi->pots       != NULL) free(gi->pots);
        if ( gi->gc         != NULL) free(gi->gc);
        free(gi);
        return NULL;
    }

/* initialize chain of empty slots  */
    for(i=0;i<GEOSLOTSTART;i++)
    {
        if(i<GEOSLOTSTART-1) (gi->gc[i]).latitude=i+1;
             else            (gi->gc[i]).latitude=0;
    }

/* similarly set up free chain of empty pots  */
    for(i=0;i<GEOPOTSTART;i++)
    {
        if(i<GEOPOTSTART-1)  gi->pots[i].LorLeaf=i+1;
             else            gi->pots[i].LorLeaf=0;
    }

    gi->potct  = GEOPOTSTART;
    gi->slotct = GEOSLOTSTART;

/* set up the fixed points structure  */

/* this code assumes GeoIndexFIXEDPOINTS is eight!  */

    for(i=0;i<GeoIndexFIXEDPOINTS;i++)
    {
        if(i==0)
        {
            lat = 90.0;
            lon = 0.0;
        }
        if(i==1)
        {
            lat =  -90.0;
            lon = 0.0;
        }
        if(i==2)
        {
            lat =  19.471220634490691369246;
            lon = 0.0;
        }
        if(i==3)
        {
            lat =  -19.471220634490691369246;
            lon = 180.0;
        }
        if(i==4)
        {
            lat =  19.471220634490691369246;
            lon = 120.0;
        }
        if(i==5)
        {
            lat =  -19.471220634490691369246;
            lon = -60.0;
        }
        if(i==6)
        {
            lat =  19.471220634490691369246;
            lon = -120.0;
        }
        if(i==7)
        {
            lat =  -19.471220634490691369246;
            lon = 60.0;
        }
        z=sin(lat*M_PI/180.0);
        x=cos(lat*M_PI/180.0)*cos(lon*M_PI/180.0);
        y=cos(lat*M_PI/180.0)*sin(lon*M_PI/180.0);
        (gi->fixed.x)[i]=x;
        (gi->fixed.y)[i]=y;
        (gi->fixed.z)[i]=z;
    }
/* set up the root pot  */

    j=GeoIndexNewPot(gi);
    gi->pots[j].LorLeaf=0;   /* leaf pot  */
    gi->pots[j].RorPoints=0;   /* with no points in it!  */
    gi->pots[j].middle = 0ll;
    gi->pots[j].start  = 0ll;
    gi->pots[j].end = 0x1FFFFFFFFFFFFFll;
    gi->pots[j].level = 1;
    for(i=0;i<GeoIndexFIXEDPOINTS;i++)
        gi->pots[j].maxdist[i]=0;
    return gi;
}

void GeoIndex_free(GeoIndex * gi)
{
    free(gi->gc);
    free(gi->pots);
    free(gi);
}


    /* 2^25 / 90 rounded down.  Used to convert */
    /* degrees of longitude and latitude into   */
    /* integers for use making a GeoString      */
#define STRINGPERDEGREE 372827.01
#define HILBERTMAX 67108863
#define ARCSINFIX 41720.0

GeoString GeoMkHilbert(GeoCoordinate * c)
{
    double x1,y1;
    GeoString z;
    int x,y;
    int i,nz,temp;
    y1=c->latitude+90.0;
    z=0;
    x1=c->longitude;
    if(c->longitude < 0.0)
    {
        x1=c->longitude+180.0;
        z=1;
    }
    x=(int) (x1*STRINGPERDEGREE);
    y=(int) (y1*STRINGPERDEGREE);
    for(i=0;i<26;i++)
    {
        z<<=2;
        nz=((y>>24)&2)+(x>>25);
        x = (x<<1)&(HILBERTMAX);
        y = (y<<1)&(HILBERTMAX);
        if(nz==0)
        {
            temp=x;
            x=y;
            y=temp;
        }
        if(nz==1)
        {
            temp=HILBERTMAX-x;
            x=HILBERTMAX-y;
            y=temp;
            z+=3;
        }
        if(nz==2)
        {
            z+=1;
        }
        if(nz==3)
        {
            z+=2;
        }
    }
    return z+1ll;

}

void GeoMkDetail(GeoIndex * gi, GeoDetailedPoint * gd, GeoCoordinate * c)
{
/* entire routine takes about 0.94 microseconds  */
    double x1,y1,z1,snmd;
    int i;
    gd->gi=gi;
    gd->gc = c;
/* The GeoString computation takes about 0.17 microseconds  */
    gd->gs=GeoMkHilbert(c);
/* This part takes about 0.32 microseconds  */
    gd->z=sin(c->latitude*M_PI/180.0);
    gd->x=cos(c->latitude*M_PI/180.0)*cos(c->longitude*M_PI/180.0);
    gd->y=cos(c->latitude*M_PI/180.0)*sin(c->longitude*M_PI/180.0);
/* and this bit takes about 0.45 microseconds  */
    for(i=0;i<GeoIndexFIXEDPOINTS;i++)
    {
        x1=(gi->fixed.x)[i];
        y1=(gi->fixed.y)[i];
        z1=(gi->fixed.z)[i];
        snmd=(x1-gd->x)*(x1-gd->x)+(y1-gd->y)*(y1-gd->y)+
                          (z1-gd->z)*(z1-gd->z);
        (gd->fixdist)[i] = asin(sqrt(snmd)/2.0)*ARCSINFIX;
    }
}

double GeoMetersToSNMD(double meters)
{
    double angle,hnmd;
    angle=0.5*meters/EARTHRADIUS;
    hnmd=sin(angle);   /* half normalized mole distance  */
    if(angle>=M_PI/2.0) return 4.0;
    else               return hnmd*hnmd*4.0;
}

void GeoSetDistance(GeoDetailedPoint * gd, double snmd)
{
    GeoFix gf;
    int i;
    gd->snmd = snmd;
    gf = asin(sqrt(snmd)/2.0)*ARCSINFIX;
    gf++;
    for(i=0;i<GeoIndexFIXEDPOINTS;i++)
    {
        if((gd->fixdist)[i]<=gf) (gd->distrej)[i]=0;
               else              (gd->distrej)[i]=(gd->fixdist)[i]-gf;
    }
}

void GeoStackSet (GeoStack * gk, GeoDetailedPoint * gd, GeoResults * gr)
{
    int pot;
    GeoIndex * gi;
    GeoPot * gp;
    gi=gd->gi;
    gk->gr = gr;
    gk->gd = gd;
    gk->stacksize = 0;
    pot=1;
    while(1)
    {
        gp=gi->pots+pot;
        if(gp->LorLeaf==0) break;
        if(gp->middle>gd->gs)
        {
            gk->potid[gk->stacksize]=gp->RorPoints;
            pot=gp->LorLeaf;
        }
        else
        {
            gk->potid[gk->stacksize]=gp->LorLeaf;
            pot=gp->RorPoints;
        }
        gk->stacksize++;
    }
    gk->potid[gk->stacksize]=pot;
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
    gres->snmd = dd;
/* no need to initialize maxsnmd */
    return gres;
}

void GeoResultsStartCount(GeoResults * gr)
{
    int i;
    for(i=0;i<gr->allocpoints;i++)
    {
        gr->slot[i]=0;
        gr->snmd[i]=10.0;
    }
}

void GeoResultsInsertPoint(GeoResults * gr, int slot, double snmd)
{
    int i,j1,j2,temp;
    if(snmd>=gr->snmd[0]) return;
    if(gr->slot[0]==0) gr->pointsct++;
    i=0;     /* i is now considered empty  */
    while(1)
    {
        j1=2*i+1;
        j2=2*i+2;
        if(j1<gr->allocpoints)
        {
            if(j2<gr->allocpoints)
            {
                if(gr->snmd[j1]>gr->snmd[j2])
                {
                    temp=j1;
                    j1=j2;
                    j2=temp;
                }
                    /* so now j2 is >= j1 */
                if(gr->snmd[j2]<=snmd)
                {
                    gr->snmd[i]=snmd;
                    gr->slot[i]=slot;
                    return;
                }
                gr->snmd[i]=gr->snmd[j2];
                gr->slot[i]=gr->slot[j2];
                i=j2;
                continue;
            }
            if(gr->snmd[j1]<=snmd)
            {
                gr->snmd[i]=snmd;
                gr->slot[i]=slot;
                return;
            }
            gr->snmd[i]=gr->snmd[j1];
            gr->slot[i]=gr->slot[j1];
            i=j1;
            continue;
        }
        gr->snmd[i]=snmd;
        gr->slot[i]=slot;
        return;
    }
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
    dd=realloc(gr->snmd, newsiz*sizeof(double));
    if( (sa==NULL) || (dd==NULL) )
    {
        if(sa!=NULL) gr->slot = sa;
        if(dd!=NULL) gr->snmd = dd;
        return -1;
    }
    gr->slot = sa;
    gr->snmd = dd;
    gr->allocpoints = newsiz;
    return 0;
}

GeoCoordinates * GeoAnswers (GeoIndex * gi, GeoResults * gr)
{
    GeoCoordinates * ans;
    GeoCoordinate  * gc;
    int i,j,slot;
    double mole;
    ans = malloc(sizeof(GeoCoordinates));;
    gc  = malloc(gr->pointsct * sizeof(GeoCoordinate));
    if( (ans==NULL) || (gc==NULL) )
    {
        if(ans!=NULL) free(ans);
        if(gc!=NULL) free(gc);
        free(gr->slot);
        free(gr->snmd);
        free(gr);
        return NULL;
    }
    ans->length = gr->pointsct;
    ans->coordinates = gc;
    j=0;
    for(i=0;i<gr->allocpoints;i++)
    {
        if(j>=gr->pointsct) break;
        slot=gr->slot[i];
        if(slot==0) continue;
        ans->coordinates[j].latitude =
                     (gi->gc)[slot].latitude;
        ans->coordinates[j].longitude =
                     (gi->gc)[slot].longitude;
        ans->coordinates[j].data =
                     (gi->gc)[slot].data;
        mole=sqrt(gr->snmd[i]);
        if(mole >  2.0) mole = 2.0; /* make sure arcsin succeeds! */
        gr->snmd[j]= 2.0 * EARTHRADIUS * asin(mole/2.0);
        j++;
    }
    ans->distances = gr->snmd;
    free(gr->slot);
    free(gr);
    return ans;
}

int GeoPotJunk(GeoDetailedPoint * gd, int pot)
{
    int i;
    GeoPot * gp;
    gp=(gd->gi)->pots + pot;
    for(i=0;i<GeoIndexFIXEDPOINTS;i++)
        if(gp->maxdist[i]<gd->distrej[i]) return 1;
    return 0;
}

double GeoSNMD(GeoDetailedPoint * gd, GeoCoordinate * c)
{
    double x,y,z;
    z=sin(c->latitude*M_PI/180.0);
    x=cos(c->latitude*M_PI/180.0)*cos(c->longitude*M_PI/180.0);
    y=cos(c->latitude*M_PI/180.0)*sin(c->longitude*M_PI/180.0);
    return (x-gd->x)*(x-gd->x) + (y-gd->y)*(y-gd->y) +
                  (z-gd->z)*(z-gd->z);
}

GeoCoordinates * GeoIndex_PointsWithinRadius(GeoIndex * gi,
                    GeoCoordinate * c, double d)
{
    GeoResults * gres;
    GeoCoordinates * answer;
    GeoDetailedPoint gd;
    GeoStack gk;
    GeoPot * gp;
    int r,pot,slot,i;
    double snmd,maxsnmd;
    gres=GeoResultsCons(100);
    if(gres==NULL) return NULL;
    GeoMkDetail(gi,&gd,c);
    GeoStackSet(&gk,&gd,gres);
    maxsnmd=GeoMetersToSNMD(d);
    GeoSetDistance(&gd,maxsnmd);
    gk.stacksize++;
    while(gk.stacksize>=1)
    {
        gk.stacksize--;
        pot=gk.potid[gk.stacksize];
        if(GeoPotJunk(&gd,pot))
            continue;
        gp=gi->pots+pot;
        if(gp->LorLeaf==0)
        {
            for(i=0;i<gp->RorPoints;i++)
            {
                slot=gp->points[i];
                snmd=GeoSNMD(&gd,gi->gc+slot);
                if(snmd>maxsnmd) continue;
                r = GeoResultsGrow(gres);
                if(r==-1)
                {
                    free(gres->snmd);
                    free(gres->slot);
                    free(gres);
                    return NULL;
                }
                gres->slot[gres->pointsct]=slot;
                gres->snmd[gres->pointsct]=snmd;
                gres->pointsct++;
            }
        }
        else
        {
            gk.potid[gk.stacksize++]=gp->LorLeaf;
            gk.potid[gk.stacksize++]=gp->RorPoints;
        }
    }
    answer=GeoAnswers(gi,gres);
    return answer;   /* note - this may be NULL  */
}

GeoCoordinates * GeoIndex_NearestCountPoints(GeoIndex * gi,
                    GeoCoordinate * c, int count)
{
    GeoResults * gr;
    GeoDetailedPoint gd;
    GeoCoordinates * answer;
    GeoStack gk;
    GeoPot * gp;
    int pot,slot,i,left;
    double snmd;

    gr=GeoResultsCons(count);
    if(gr==NULL) return NULL;
    GeoMkDetail(gi,&gd,c);
    GeoStackSet(&gk,&gd,gr);
    GeoResultsStartCount(gr);
    left=count;

    while(gk.stacksize>=0)
    {
        pot=gk.potid[gk.stacksize--];
        gp=gi->pots+pot;
        if(left<=0)
        {
            GeoSetDistance(&gd,gr->snmd[0]);
            if(GeoPotJunk(&gd,pot)) continue;
        }
        if(gp->LorLeaf==0)
        {
            for(i=0;i<gp->RorPoints;i++)
            {
                slot=gp->points[i];
                snmd=GeoSNMD(&gd,gi->gc+slot);
                GeoResultsInsertPoint(gr,slot,snmd);
                left--;
                if(left<-1) left=-1;
            }
        }
        else
        {
            if(gd.gs>gp->middle)
            {
                gk.potid[++gk.stacksize]=gp->LorLeaf;
                gk.potid[++gk.stacksize]=gp->RorPoints;
            }
            else
            {
                gk.potid[++gk.stacksize]=gp->RorPoints;
                gk.potid[++gk.stacksize]=gp->LorLeaf;
            }
        }
    }
    answer=GeoAnswers(gi,gr);
    return answer;   /* note - this may be NULL  */
}

void GeoIndexFreeSlot(GeoIndex * gi, int slot)
{
    gi->gc[slot].latitude=gi->gc[0].latitude;
    gi->gc[0].latitude = slot;
}

int GeoIndexNewSlot(GeoIndex * gi)
{
    int newslotct,j;
    long long x,y;
    GeoCoordinate * gc;
    if(gi->gc[0].latitude == 0.0)
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
        gc = realloc(gi->gc,newslotct*sizeof(GeoCoordinate));
        if(gc!=NULL) gi->gc=gc;
            else    return -2;
        for(j=gi->slotct;j<newslotct;j++) GeoIndexFreeSlot(gi,j);
        gi->slotct=newslotct;
    }
    j= (int) (gi->gc[0].latitude);
    gi->gc[0].latitude=gi->gc[j].latitude;
    return j;
}

int GeoFind(GeoPath * gt, GeoDetailedPoint * gd)
{
    int pot,pot1;
    int i;
    int slot;
    GeoIndex * gi;
    GeoCoordinate * gc;
    GeoPot * gp;
    gi = gd->gi;
    gt->gi = gi;
    pot=1;
    gt->pathlength=0;
    while(1)
    {
        gp=gi->pots+pot;
        gt->path[gt->pathlength]=pot;
        gt->pathlength++;
        if(gp->LorLeaf == 0) break;
        if(gp->middle > gd->gs)
            pot=gp->LorLeaf;
        else
            pot=gp->RorPoints;
    }
/* so we have a pot such that top is bigger but bottom isn't  */
    while(1)   /* so look for an exact match  */
    {
        for(i=0;i<gp->RorPoints;i++)
        {
            slot=gp->points[i];
            gc=gi->gc+slot;
            if( ( (gd->gc)->latitude ==gc->latitude )  &&
                ( (gd->gc)->longitude==gc->longitude)  &&
                ( (gd->gc)->data     ==gc->data     )  )
            {
                gt->path[gt->pathlength]=i;
                return 1;
            }
        }
        if(gp->start<gd->gs) break;
/* need to find the predecessor of this pot  */
/* this is expected to be a rare event, so   */
/* no time is wasted  to simplify this!      */
        while(1)
        {
            gt->pathlength--;
            pot1=gt->path[gt->pathlength-1];
            gp=gi->pots+pot1;
            if(pot==gp->RorPoints) break;  /* cannot go off the front  */
            pot=pot1;
        }
        gp=gi->pots+pot1;
        pot=gp->LorLeaf;
/* now we have a pot whose iterated right child we want  */
        while(1)
        {
            gp=gi->pots+pot;
            gt->path[gt->pathlength]=pot;
            gt->pathlength++;
            if(gp->LorLeaf == 0) break;
            pot=gp->RorPoints;
        }
    }
    return 2;
}

void GeoPopulateMaxdist(GeoIndex * gi, GeoPot * gp, GeoString * gsa)
{
    int i,j;
    GeoDetailedPoint gd;
    gsa[0]=0x1FFFFFFFFFFFFFll;
    gsa[1]=0ll;
    for(j=0;j<GeoIndexFIXEDPOINTS;j++) gp->maxdist[j]=0;
    for(i=0;i<gp->RorPoints;i++)
    {
        GeoMkDetail(gi,&gd,gi->gc+gp->points[i]);
        for(j=0;j<GeoIndexFIXEDPOINTS;j++)
            if(gd.fixdist[j]>gp->maxdist[j])
                gp->maxdist[j] = gd.fixdist[j];
        if(gd.gs<gsa[0]) gsa[0]=gd.gs;
        if(gd.gs>gsa[1]) gsa[1]=gd.gs;
    }
    gp->level=1;
}

int GeoGetPot(GeoPath * gt, int height)
{
    return gt->path[gt->pathlength-height];
}

void GeoAdjust(GeoIndex * gi, int potx) /* the kids are alright */
{
    int poty,potz;    /* x = (yz)  */
    int i;
    GeoPot * gpx;
    GeoPot * gpy;
    GeoPot * gpz;
    gpx=gi->pots + potx;
    poty=gpx->LorLeaf;
    gpy=gi->pots + poty;
    potz=gpx->RorPoints;
    gpz=gi->pots + potz;
    gpx->start=gpy->start;
    gpx->end  =gpz->end;
    gpx->middle=gpz->start;
    gpx->level=gpy->level;
    if( (gpz->level) > gpx->level)
        gpx->level = gpz->level;
    gpx->level++;
    for(i=0;i<GeoIndexFIXEDPOINTS;i++)
    {
        gpx->maxdist[i]=gpy->maxdist[i];
        if( gpx->maxdist[i]<gpz->maxdist[i])
            gpx->maxdist[i]=gpz->maxdist[i];

    }
}

void RotateLeft(GeoIndex * gi, int pote)
{
    int pota,potb,potc,potd;
    GeoPot * gpd;
    GeoPot * gpe;
    gpe=gi->pots + pote;
    potd=gpe->RorPoints;
    gpd=gi->pots + potd;
    pota=gpe->LorLeaf;
    potb=gpd->LorLeaf;
    potc=gpd->RorPoints;
    gpd->LorLeaf=pota;
    gpd->RorPoints=potb;
    GeoAdjust(gi,potd);
    gpe->LorLeaf=potd;
    gpe->RorPoints=potc;
    GeoAdjust(gi,pote);
}

void RotateRight(GeoIndex * gi, int pote)
{
    int pota,potb,potc,potd;
    GeoPot * gpd;
    GeoPot * gpe;
    gpe=gi->pots + pote;
    potd=gpe->LorLeaf;
    gpd=gi->pots + potd;
    pota=gpd->LorLeaf;
    potb=gpd->RorPoints;
    potc=gpe->RorPoints;
    gpd->LorLeaf=potb;
    gpd->RorPoints=potc;
    GeoAdjust(gi,potd);
    gpe->LorLeaf=pota;
    gpe->RorPoints=potd;
    GeoAdjust(gi,pote);
}

int GeoIndex_insert(GeoIndex * gi, GeoCoordinate * c)
{
    int i,j,js,slot,pot,pot1,pot2;
    int potx,pota,poty,potz;
    int lvx,lv1,lva,lvy,lvz;
    int height,rebalance;
    GeoDetailedPoint gd;
    GeoPath gt;
    GeoPot * gp;
    GeoPot * gp1;
    GeoPot * gp2;
    GeoPot * gpx;
    GeoPot * gpy;
    GeoPot * gpz;
    GeoPot * gpa;
    GeoString gsa[2];
    GeoString mings, gs;
    rebalance=0;
    if(c->longitude < -180.0) return -3;
    if(c->longitude >  180.0) return -3;
    if(c->latitude  <  -90.0) return -3;
    if(c->latitude  >   90.0) return -3;
    GeoMkDetail(gi,&gd,c);
    i = GeoFind(&gt,&gd);
    if(i==1) return -1;
    pot=gt.path[gt.pathlength-1];
    gp=gi->pots + pot;
/* new point, so we try to put it in  */
    slot=GeoIndexNewSlot(gi);
    if(slot==-2) return -2;     /* no room  :(  */
    gi->gc[slot].latitude =c->latitude;
    gi->gc[slot].longitude=c->longitude;
    gi->gc[slot].data     =c->data;
/* check first if we are going to need two new pots, and  */
/* if we are, go get them now before we get  too tangled  */
    if(gp->RorPoints == GeoIndexPOTSIZE)
    {
        rebalance=1;
        pot1=GeoIndexNewPot(gi);
        pot2=GeoIndexNewPot(gi);
        gp=gi->pots + pot;   /* may have re-alloced!  */
        if( (pot1==-2) || (pot2==-2) )
        {
            GeoIndexFreeSlot(gi,slot);
            if(pot1!=-2) GeoIndexFreePot(gi,pot1);
            if(pot2!=-2) GeoIndexFreePot(gi,pot2);
            return -2;
        }
        gp1=gi->pots + pot1;
        gp2=gi->pots + pot2;
/* pot is old one, pot1 and pot2 are the new ones  */
        gp1->LorLeaf=0;    /* leaf pot  */
        gp1->RorPoints=0;    /* no points in it yet  */
/* first move the points from pot to pot2          */
        gp2->LorLeaf=0;    /* leaf pot  */
        gp2->RorPoints=gp->RorPoints;
        for(i=0;i<gp->RorPoints;i++)
            gp2->points[i]=gp->points[i];
/* move the first half of the points from pot2 to pot1 */
        for(i=0;i<GeoIndexPOTSIZE/2;i++)
        {
            mings=0x1FFFFFFFFFFFFFll;
            js=0;
            for(j=0;j<gp2->RorPoints;j++)
            {
                gs=GeoMkHilbert(gi->gc + gp2->points[j]);
                if(gs<mings)
                {
                    mings=gs;
                    js=j;
                }
            }
            gp1->points[gp1->RorPoints]=gp2->points[js];
            gp2->points[js]=gp2->points[gp2->RorPoints-1];
            gp2->RorPoints--;
            gp1->RorPoints++;
        }
        GeoPopulateMaxdist(gi,gp2,gsa);
        mings=gsa[0];
        GeoPopulateMaxdist(gi,gp1,gsa);
        mings=(mings+gsa[1])/2ll;
        gp1->start=gp->start;
        gp1->end=mings;
        gp2->start=mings;
        gp2->end=gp->end;
        gp->LorLeaf=pot1;
        gp->RorPoints=pot2;
        GeoAdjust(gi,pot);
        gt.pathlength++;
        if(gd.gs<mings)
        {
            gp=gp1;
            gt.path[gt.pathlength-1]=pot1;
        }
        else
        {
            gp=gp2;
            gt.path[gt.pathlength-1]=pot2;
        }
    }
/* so we have a pot and a path we can use   */
/* gp is the pot, gt set correctly          */
    gp->points[gp->RorPoints]=slot;
    gp->RorPoints++;
/* now propagate the maxdistances */
    for(i=0;i<GeoIndexFIXEDPOINTS;i++)
    {
        j=gt.pathlength-1;
        while(j>=0)
        {
            if(gd.fixdist[i] > gi->pots[gt.path[j]].maxdist[i])
                gi->pots[gt.path[j]].maxdist[i] = gd.fixdist[i];
            else break;
            j--;
        }
    }
/* just need to balance the tree  */
    if(rebalance==0) return 0;
    height=2;
    while(1)
    {
        potx=GeoGetPot(&gt,height);
        gpx=gi->pots + potx;
        lvx=gpx->level;
        if(potx==1) break;
            /* root pot ?      */
        pot1=GeoGetPot(&gt,height+1); /* pot1=parent(x)  */
        gp1=gi->pots + pot1;
        lv1=gp1->level;
        if(lv1>lvx) break;
        if(gp1->LorLeaf==potx) /* gpx is the left child? */
        {
            pota=gp1->RorPoints;   /* 1 = (xa)  */
            gpa=gi->pots+pota;
            lva=gpa->level;
            if( (lva+1) == lv1)  /* so it is legal to up lev(1) */
            {
                gp1->level++;
                height++;
                continue;
            }
            poty=gpx->RorPoints;
            gpy=gi->pots + poty;
            lvy=gpy->level;
            potz=gpx->LorLeaf;
            gpz=gi->pots + potz;
            lvz=gpz->level;
            if(lvy<=lvz)
            {
                RotateRight(gi,pot1);
                height++;
                continue;
            }
            RotateLeft(gi,potx);
            RotateRight(gi,pot1);
        }
        else                 /* gpx is the right child */
        {
            pota=gp1->LorLeaf;   /* 1 = (ax)  */
            gpa=gi->pots+pota;
            lva=gpa->level;
            if( (lva+1) == lv1)  /* so it is legal to up lev(1) */
            {
                gp1->level++;
                height++;
                continue;
            }
            poty=gpx->LorLeaf;
            gpy=gi->pots + poty;
            lvy=gpy->level;
            potz=gpx->RorPoints;
            gpz=gi->pots + potz;
            lvz=gpz->level;
            if(lvy<=lvz)
            {
                RotateLeft(gi,pot1);
                height++;
                continue;
            }
            RotateRight(gi,potx);
            RotateLeft(gi,pot1);
        }
    }
    return 0;
}

int GeoIndex_remove(GeoIndex * gi, GeoCoordinate * c)
{
    GeoDetailedPoint gd;
    GeoPot * gp;
    GeoPath gt;
    int i,pot,potix,slot,pathix;
    GeoMkDetail(gi,&gd,c);
    i = GeoFind(&gt,&gd);
    if(i!=1) return -1;
    pot=gt.path[gt.pathlength-1];
    gp=gi->pots + pot;
    potix = gt.path[gt.pathlength];
    slot=gp->points[potix];
    GeoIndexFreeSlot(gi,slot);
    gp->points[potix]=gp->points[gp->RorPoints-1];
    gp->RorPoints--;
    if(  (2*gp->RorPoints)<GeoIndexPOTSIZE )
    {
/* whole pile of stuff to do  */
    }
    pathix=gt.pathlength-1;
    while(pathix>0)
    {
        pathix--;
        pot=gt.path[pathix];
        GeoAdjust(gi,pot);
    }
    return 0;
}

void GeoIndex_CoordinatesFree(GeoCoordinates * clist)
{
    free(clist->coordinates);
    free(clist->distances);
    free(clist);
}

int GeoIndex_hint(GeoIndex * gi, int hint)
{
    return 0;
}

#ifdef DEBUG

void RecursivePotDump (GeoIndex * gi, FILE * f, int pot)
{
    int i;
    GeoPot * gp;
    GeoCoordinate * gc;
    gp=gi->pots + pot;
    fprintf(f,"GP. pot %d level %d  Kids %d %d\n",
               pot, gp->level, gp->LorLeaf,gp->RorPoints);
    fprintf(f,"strings %llx %llx %llx\n",gp->start,gp->middle,gp->end);
    fprintf(f,"maxdists ");
    for(i=0;i<GeoIndexFIXEDPOINTS;i++)
        fprintf(f," %x",gp->maxdist[i]);
    fprintf(f,"\n");
    if(gp->LorLeaf==0)
    {
        fprintf(f,"Leaf pot containing %d points . . .\n",gp->RorPoints);
        for(i=0;i<gp->RorPoints;i++)
        {
            fprintf(f,"Child %d Point %d  ",i,gp->points[i]);
            gc=gi->gc + gp->points[i];
            fprintf(f,"Lat.  %9.4f,  Long. %9.4f",
                gc->latitude, gc->longitude);
#if DEBUG==2
            fprintf(f," %s",(char *) gc->data);
#endif
            fprintf(f,"\n");
        }
    }
    else
    {
        fprintf(f,"\nPot %d - Left  Child of pot %d\n",
                      gp->LorLeaf,pot);
        RecursivePotDump (gi,f,gp->LorLeaf);
        fprintf(f,"\nPot %d - Right Child of pot %d\n",
                      gp->RorPoints,pot);
        RecursivePotDump (gi,f,gp->RorPoints);
    }
}

void GeoIndex_INDEXDUMP (GeoIndex * gi, FILE * f)
{
    fprintf(f,"Dump of entire index.  %d pots and %d slots allocated\n",
                              gi->potct, gi->slotct);
    RecursivePotDump(gi,f,1);
}

int GeoIndex_INDEXVALID(GeoIndex * gi)
{
    return 0;
}

#endif

/* end of GeoIndex.c  */
