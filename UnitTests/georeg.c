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

/* regression testing program for GeoIndex module     */
/*  R.A.P. 1.1 5.11.2011                              */

#ifdef GEO_TRIAGENS
#include "GeoIndex/GeoIndex.h"
#else
#include "GeoIndex.h"
#endif

int errors;

char ix[1000];  /* working array for (void *) data  */
char iy[30];

void icheck(int e, int a, int b)
{
    if(a==b) return;
    errors++;
    printf("Error %d, should be %d, was %d\n",e,a,b);
}

void pcheck(int e, char * a, char * b)
{
    if(a==b) return;
    errors++;
    if(b>a) printf("Error %d, pointer was %ld too high\n",e,(long)(b-a));
    if(b<a) printf("Error %d, pointer was %ld too low \n",e,(long)(a-b));
}

void dcheck(int e, double a, double b, double bar)
{
    if(b<a-bar)
    {
        errors++;
        printf("Error %d, should be %16.9f, was %16.9f\n",e,a,b);
    }
    if(b>a+bar)
    {
        errors++;
        printf("Error %d, should be %16.9f, was %16.9f\n",e,a,b);
    }
}

void gccheck(int e, GeoCoordinates * gc, int ct, char * bytes)
{
    int i,good;
    icheck(e,ct,gc->length);
    for(i=0;i<1000;i++) ix[i]=0;
    for(i=0;i<gc->length;i++)
        *((char *)gc->coordinates[i].data)=1;
    for(i=0;i<25;i++)
        iy[i]='A'+ix[4*i]+2*ix[4*i+1]+4*ix[4*i+2]+8*ix[4*i+3];
    iy[25]=0;
    good=1;
    for(i=0;i<25;i++)  if(bytes[i]!=iy[i]) good=0;
    if(good==0)
    {
        errors++;
        printf("Error %d, should be %s, was %s\n",e+1,bytes,iy);
    }
    GeoIndex_CoordinatesFree(gc);
}

void gicheck(int e, GeoIndex* gi)
{
    int r;
    r=0;
#ifdef DEBUG
    r = GeoIndex_INDEXVALID(gi);
#endif
    if(r==0) return;
    errors++;
    printf("Error %d, Index invalidity code %d\n",e,r);
}

void gcmass(int e, GeoCoordinates * gc, int ct, int hash)
{
    int i,j;
    icheck(e,ct,gc->length);
    for(i=0;i<1000;i++) ix[i]=0;
    for(i=0;i<gc->length;i++)
        (*((char *)gc->coordinates[i].data))++;
    j=0;
    for(i=0;i<1000;i++)
    {
        j=j+ix[i];
        j=j*7;
        j=j%123456791;
    }
    icheck(e+1,hash,j);
    GeoIndex_CoordinatesFree(gc);
}

int main(int argc, char ** argv)
{
    GeoIndex * gi;
    GeoCoordinate gcp;
    GeoCoordinate gcp1, gcp2, gcp3, gcp4;
    GeoCoordinates * list1;
    double la,lo;
    double d1;
    int i,j,r,mode;

    if(argc!=2)
    {
        printf("usage georeg <n> to run test n\n");
        exit(1);
    }
    mode=atoi(argv[1]);

    errors=0;

/*           1-9 some real world distance          */
/*   1 is London    +51.500000 -0.166666           */
/*   2 is Honolulu  +21.306111 -157.859722         */
/*   3 is Auckland  -36.916667 +174.783333         */
/*   4 is Jo'burg   -26.166667  +28.033333         */

        gcp1.latitude  =   51.5;
        gcp1.longitude =   -0.166666;
        gcp2.latitude  =   21.306111;
        gcp2.longitude = -157.859722;
        d1 = GeoIndex_distance(&gcp1, &gcp2);
        dcheck(1,11624035.0, d1, 11000.0);
        gcp3.latitude  = -36.916667;
        gcp3.longitude = 174.783333;
        d1 = GeoIndex_distance(&gcp1, &gcp3);
        dcheck(2,18332948.0, d1, 18000.0);
        gcp4.latitude  = -26.166667;
        gcp4.longitude = +28.033333;
        d1 = GeoIndex_distance(&gcp1, &gcp4);
        dcheck(3,9059681.0, d1, 9000.0);
        d1 = GeoIndex_distance(&gcp2, &gcp3);
        dcheck(4,7076628.0, d1, 7000.0);
        d1 = GeoIndex_distance(&gcp2, &gcp4);
        dcheck(5,19194970.0, d1, 19000.0);
        d1 = GeoIndex_distance(&gcp3, &gcp4);
        dcheck(6,12177171.0, d1, 12000.0);

/*               10 - 19                           */

/* first some easily recognizable GeoStrings       */
/* mainly for debugging rather than regression     */

    gi=GeoIndex_new();

    for(i=0;i<50;i++)
    {
        gcp.latitude  = 90.0;
        gcp.longitude = 180.0;
        gcp.data = ix + i;
        r = GeoIndex_insert(gi,&gcp);
        icheck(10,0,r);
    }
    gcp.latitude = 0.0;
    gcp.longitude= 25.5;
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,1);
    icheck(11,1,list1->length);
    gcp.latitude  = 89.9;
    gcp.longitude = -180.0;
    gcp.data = ix + 64;
    r = GeoIndex_insert(gi,&gcp);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,1);
    gccheck(13,list1,  1,"AAAAAAAAAAAAAAAABAAAAAAAA");
    gicheck(14,gi);

    GeoIndex_free(gi);

/*               20 - 39                           */

/* Make sure things work with an empty index       */

    gi=GeoIndex_new();

/* do both searches with an empty index  */

    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,3);
    gccheck(20,list1,  0,"AAAAAAAAAAAAAAAAAAAAAAAAA");
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,100000.0);
    gccheck(21,list1,0,"AAAAAAAAAAAAAAAAAAAAAAAAA");

/* stick in Jo'burg  */
    gcp4.data=ix + 4;
    r = GeoIndex_insert(gi,&gcp4);
    icheck(22,0,r);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,3);
    gccheck(23,list1,  1,"ABAAAAAAAAAAAAAAAAAAAAAAA");
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,10000000.0);
    gccheck(24,list1,1,"ABAAAAAAAAAAAAAAAAAAAAAAA");

/* then take it out again and repeat tests with empty index */
    r = GeoIndex_remove(gi,&gcp4);
    icheck(25,0,r);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,3);
    gccheck(26,list1,  0,"AAAAAAAAAAAAAAAAAAAAAAAAA");
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,100000.0);
    gccheck(27,list1,0,"AAAAAAAAAAAAAAAAAAAAAAAAA");

/* try to delete from an empty index  */

    r = GeoIndex_remove(gi,&gcp2);
    icheck(28,-1,r);

/* stick in Auckland into emptied index  */
    gcp3.data=ix + 3;
    r = GeoIndex_insert(gi,&gcp3);
    icheck(29,0,r);

    gicheck(30,gi);
    GeoIndex_free(gi);
/*                                                 */
/*              50 - 69                            */
/*             =========                           */
/*                                                 */

/* now some tests on invalid data  */
    gi=GeoIndex_new();

    gcp.latitude  = 91.2;
    gcp.longitude = 40.0;
    gcp.data = NULL;
    i = GeoIndex_insert(gi,&gcp);
    icheck(50,-3,i);

    gcp.latitude  = 90.000000001;
    i = GeoIndex_insert(gi,&gcp);
    icheck(51,-3,i);

    gcp.latitude  = 90.0;
    i = GeoIndex_insert(gi,&gcp);
    icheck(52,0,i);

    gcp.latitude  = 0.0;
    i = GeoIndex_insert(gi,&gcp);
    icheck(53,0,i);

    gcp.latitude  = -90.0;
    i = GeoIndex_insert(gi,&gcp);
    icheck(54,0,i);

    gcp.latitude  = -90.000000001;
    i = GeoIndex_insert(gi,&gcp);
    icheck(55,-3,i);

    gcp.latitude  = 333390.1;
    i = GeoIndex_insert(gi,&gcp);
    icheck(56,-3,i);

    gcp.latitude  = 40.2;
    gcp.longitude = -333440.0;
    i = GeoIndex_insert(gi,&gcp);
    icheck(57,-3,i);

    gcp.longitude = -181.0;
    i = GeoIndex_insert(gi,&gcp);
    icheck(58,-3,i);

    gcp.longitude = -180.00000001;
    i = GeoIndex_insert(gi,&gcp);
    icheck(59,-3,i);

    gcp.longitude = -180.0;
    i = GeoIndex_insert(gi,&gcp);
    icheck(60,0,i);

    gcp.longitude = -81.0;
    i = GeoIndex_insert(gi,&gcp);
    icheck(61,0,i);

    gcp.longitude = 0.0;
    i = GeoIndex_insert(gi,&gcp);
    icheck(62,0,i);

    gcp.longitude = 180.0;
    i = GeoIndex_insert(gi,&gcp);
    icheck(63,0,i);

    gcp.longitude = 180.00000001;
    i = GeoIndex_insert(gi,&gcp);
    icheck(64,-3,i);

    gcp.latitude  = 91.1;
    gcp.longitude = 181.1;
    i = GeoIndex_insert(gi,&gcp);
    icheck(65,-3,i);

    gicheck(66,gi);

    GeoIndex_free(gi);

/*                                                 */
/*              70 - 99                            */
/*             =========                           */
/*                                                 */

/* now some tests inserting and deleting in        */
/*   in some chaotic ways                          */

    gi=GeoIndex_new();

    gcp.latitude  = 0.0;
    gcp.longitude = 40.0;
    gcp.data = &ix[4];
    i = GeoIndex_insert(gi,&gcp);
    icheck(70,0,i);

    gcp.data = &ix[5];
    i = GeoIndex_remove(gi,&gcp);
    icheck(71,-1,i);

    gcp.longitude = 40.000001;
    gcp.data = &ix[4];
    i = GeoIndex_remove(gi,&gcp);
    icheck(72,-1,i);

    gcp.latitude  = 0.0000000001;
    gcp.longitude = 40.0;
    i = GeoIndex_remove(gi,&gcp);
    icheck(73,-1,i);

    gcp.latitude  = 0.0;
    i = GeoIndex_remove(gi,&gcp);
    icheck(74,0,i);

    i = GeoIndex_remove(gi,&gcp);
    icheck(75,-1,i);

    for(j=1;j<=8;j++)
    {
        gcp.latitude  = 0.0;
        lo=j;
        lo=lo*10;
        gcp.longitude = lo;
        gcp.data = &ix[j];
        i = GeoIndex_insert(gi,&gcp);
        icheck(76,0,i);
    }

    gcp.latitude = 0.0;
    gcp.longitude= 25.5;
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,1);
    icheck(77,1,list1->length);
    dcheck(78,0.0,list1->coordinates[0].latitude,0.0);
    dcheck(79,30.0,list1->coordinates[0].longitude,0.0);
    pcheck(80,&ix[3],(char *)list1->coordinates[0].data);
    gcp.longitude= 24.5;
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,1);
    icheck(81,1,list1->length);
    dcheck(82,0.0,list1->coordinates[0].latitude,0.0);
    dcheck(83,20.0,list1->coordinates[0].longitude,0.0);
    pcheck(84,&ix[2],(char *)list1->coordinates[0].data);

    gcp.latitude  = 1.0;
    gcp.longitude = 40.0;
    gcp.data = &ix[14];
    i = GeoIndex_insert(gi,&gcp);
    icheck(85,0,i);

    gcp.longitude = 8000.0;
    i = GeoIndex_insert(gi,&gcp);
    icheck(86,-3,i);

    gcp.latitude  = 800.0;
    gcp.longitude = 80.0;
    i = GeoIndex_insert(gi,&gcp);
    icheck(86,-3,i);

    gcp.latitude  = 800.0;
    gcp.longitude = 80.0;
    i = GeoIndex_remove(gi,&gcp);
    icheck(87,-1,i);

    gcp.latitude  = 1.0;
    gcp.longitude = 40.0;
    gcp.data = &ix[14];
    i = GeoIndex_remove(gi,&gcp);
    icheck(88,0,i);

    for(j=1;j<10;j++)
    {
        gcp.latitude  = 0.0;
        gcp.longitude = 40.0;
        gcp.data = &ix[20+j];
        i = GeoIndex_insert(gi,&gcp);
        icheck(89,0,i);
    }

    for(j=1;j<10;j++)
    {
        gcp.latitude  = 0.0;
        gcp.longitude = 40.0;
        gcp.data = &ix[20+j];
        i = GeoIndex_remove(gi,&gcp);
        icheck(90,0,i);
    }

    gcp.latitude = 0.0;
    gcp.longitude= 35.5;
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,1);
    icheck(91,1,list1->length);
    dcheck(92,0.0,list1->coordinates[0].latitude,0.0);
    dcheck(93,40.0,list1->coordinates[0].longitude,0.0);
    pcheck(94,&ix[4],(char *)list1->coordinates[0].data);

    list1 = GeoIndex_NearestCountPoints(gi,&gcp,10);
    gccheck(95,list1,  8,"OPBAAAAAAAAAAAAAAAAAAAAAA");

    gcp.latitude  = 0.0;
    gcp.longitude = 20.0;
    gcp.data = &ix[2];
    i = GeoIndex_remove(gi,&gcp);
    icheck(96,0,i);

    list1 = GeoIndex_NearestCountPoints(gi,&gcp,10);
    gccheck(97,list1,  7,"KPBAAAAAAAAAAAAAAAAAAAAAA");

    gicheck(98,gi);
    GeoIndex_free(gi);

/*                                                 */
/*             100 - 199                           */
/*             =========                           */
/*                                                 */

/* first main batch of tests                       */
/* insert 10 x 10 array of points near south pole  */
/* then do some searches, results checked against  */
/* the same run with full table scan               */
    gi=GeoIndex_new();
    la=-89.0;
    for(i=0;i<10;i++)
    {
        gcp.latitude = la;
        lo=17.0;
        for(j=0;j<10;j++)
        {
            gcp.longitude = lo;
            gcp.data = ix+(10*i+j);
            r=GeoIndex_insert(gi,&gcp);
            icheck(100,0,r);
            lo+=1.0;
        }
        la+=1.0;
    }
    gcp.latitude = -83.2;
    gcp.longitude = 19.2;
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,7);
    gccheck(110,list1,  7,"AAAAAAAAAAAAAAAPHAAAAAAAA");
    gcp.latitude = -83.2;
    gcp.longitude = 19.2;
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,110);
    gccheck(112,list1,100,"PPPPPPPPPPPPPPPPPPPPPPPPP");
    gcp.latitude = -88.2;
    gcp.longitude = 13.2;
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,10);
    gccheck(114,list1, 10,"AAMPPAAAAAAAAAAAAAAAAAAAA");
    gcp.latitude = -83.2;
    gcp.longitude = -163.2;
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,2);
    gccheck(116,list1,  2,"AADAAAAAAAAAAAAAAAAAAAAAA");
    gcp.latitude = -87.2;
    gcp.longitude = 13.2;
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,1);
    gccheck(118,list1,  1,"AAAAABAAAAAAAAAAAAAAAAAAA");
    gcp.latitude = -83.2;
    gcp.longitude = 19.2;
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,7);
    gccheck(120,list1,  7,"AAAAAAAAAAAAAAAPHAAAAAAAA");
    gcp.latitude = -83.7;
    gcp.longitude = 19.7;
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,7);
    gccheck(122,list1,  7,"AAAAAAAAAAAAMPBAAAAAAAAAA");
    gcp.latitude = -83.2;
    gcp.longitude = 19.2;
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,50);
    gccheck(124,list1, 50,"AAAAAAAAAAPPPPPPPPPPPPDAA");
    gcp.latitude = -83.2;
    gcp.longitude = 19.2;
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,11);
    gccheck(126,list1, 11,"AAAAAAAAAAAAADAPPBAAAAAAA");
    gcp.latitude = -83.2;
    gcp.longitude = 19.2;
    list1 = GeoIndex_NearestCountPoints(gi,&gcp,4);
    gccheck(128,list1,  4,"AAAAAAAAAAAAAAAOBAAAAAAAA");
    gcp.latitude = -83.2;
    gcp.longitude = 19.2;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp,100000.0);
    gccheck(150,list1,16,"AAAAAAAAAAAAMPAPPDAAAAAAA");
    gicheck(151,gi);

    GeoIndex_free(gi);

/*                                                 */
/*             200 - 299                           */
/*             =========                           */
/*                                                 */

/* Batch of tests around the north pole,  where    */
/* the data is a known set of points, so that the  */
/* distances can be computed manually              */

    gi=GeoIndex_new();
    for(i=1;i<=10;i++)
    {
        if(i==1)
        {
            gcp.latitude = 89.97989055;
            gcp.longitude=-153.434948;
        }
        if(i==2)
        {
            gcp.latitude = 89.95978111;
            gcp.longitude=-116.565051;
        }
        if(i==3)
        {
            gcp.latitude = 89.95978111;
            gcp.longitude=153.434948;
        }
        if(i==4)
        {
            gcp.latitude = 89.87123674;
            gcp.longitude=12.0947571;
        }
        if(i==5)
        {
            gcp.latitude = 89.85498874;
            gcp.longitude=-7.1250163;
        }
        if(i==6)
        {
            gcp.latitude = 89.89746156;
            gcp.longitude=-164.744881;
        }
        if(i==7)
        {
            gcp.latitude = 89.77516959;
            gcp.longitude=163.739795;
        }
        if(i==8)
        {
            gcp.latitude = 89.8905928;
            gcp.longitude=170.537677;
        }
        if(i==9)
        {
            gcp.latitude = 89.85498874;
            gcp.longitude=-172.874983;
        }
        if(i==10)
        {
            gcp.latitude = 89.73367322;
            gcp.longitude=168.310630;
        }
        gcp.data=&ix[i];
        r = GeoIndex_insert(gi,&gcp);
        icheck(201,0,r);
    }
    gcp1.latitude=89.95157004;
    gcp1.longitude=21.8014095;
    gcp2.latitude=89.86390794;
    gcp2.longitude=172.405356;
    gcp3.latitude=89.90961929;
    gcp3.longitude=-174.289406;
    d1=GeoIndex_distance(&gcp1,&gcp2);
    dcheck(202,20000.0,d1,0.01);    /* 20 Km */
    d1=GeoIndex_distance(&gcp1,&gcp3);
    dcheck(203,15297.058,d1,0.01);  /* sqrt(234) Km  */
    d1=GeoIndex_distance(&gcp3,&gcp2);
    dcheck(204,5830.952,d1,0.01);   /* sqrt (34) Km  */

    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,1);
    dcheck(205,7615.773,list1->distances[0],0.01);   /* sqrt (58) Km  */
    gccheck(206,list1,  1,"CAAAAAAAAAAAAAAAAAAAAAAAA");

    list1 = GeoIndex_NearestCountPoints(gi,&gcp2,1);
    dcheck(208,3000.0,list1->distances[0],0.01);   /* 3  Km  */
    gccheck(209,list1,  1,"AABAAAAAAAAAAAAAAAAAAAAAA");

    list1 = GeoIndex_NearestCountPoints(gi,&gcp3,1);
    dcheck(211,2236.068,list1->distances[0],0.01);   /* sqrt (5) Km  */
    gccheck(212,list1,  1,"AEAAAAAAAAAAAAAAAAAAAAAAA");

    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,8000.0);
    dcheck(214,7615.773,list1->distances[0],0.01);   /* sqrt (58) Km  */
    gccheck(215,list1,  1,"CAAAAAAAAAAAAAAAAAAAAAAAA");

    list1 = GeoIndex_PointsWithinRadius(gi,&gcp2,3100.0);
    dcheck(218,3000.0,list1->distances[0],0.01);   /* 3  Km  */
    gccheck(219,list1,  1,"AABAAAAAAAAAAAAAAAAAAAAAA");

    list1 = GeoIndex_PointsWithinRadius(gi,&gcp3,2400.0);
    dcheck(221,2236.068,list1->distances[0],0.01);   /* sqrt (5) Km  */
    gccheck(222,list1,  1,"AEAAAAAAAAAAAAAAAAAAAAAAA");

    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,11000.0);
    gccheck(224,list1,  4,"OBAAAAAAAAAAAAAAAAAAAAAAA");

    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,13000.0);
    gccheck(226,list1,  5,"ODAAAAAAAAAAAAAAAAAAAAAAA");

    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,4);
    gccheck(228,list1,  4,"OBAAAAAAAAAAAAAAAAAAAAAAA");

    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,5);
    if(list1->length==5)
    {
        for(i=0;i<5;i++)
        {
            j=( (char *) list1->coordinates[i].data) - ix;
            if(j==1)
            {
                dcheck(230, 89.97989055,list1->coordinates[i].latitude,0.0);
                dcheck(231,-153.434948,list1->coordinates[i].longitude,0.0);
                dcheck(232,7615.773,list1->distances[i],0.01); /* S(58) */
            }
            if(j==2)
            {
                dcheck(233, 89.95978111,list1->coordinates[i].latitude,0.0);
                dcheck(234,-116.565051,list1->coordinates[i].longitude,0.0);
                dcheck(235,9219.544,list1->distances[i],0.01); /* S(85) */
            }
            if(j==3)
            {
                dcheck(236, 89.95978111,list1->coordinates[i].latitude,0.0);
                dcheck(237,153.434948,list1->coordinates[i].longitude,0.0);
                dcheck(238,9000.0,list1->distances[i],0.01); /* 9 */
            }
            if(j==4)
            {
                dcheck(239, 89.87123674,list1->coordinates[i].latitude,0.0);
                dcheck(240,12.0947571,list1->coordinates[i].longitude,0.0);
                dcheck(241,9055.385,list1->distances[i],0.01); /* S(82) */
            }
            if(j==5)
            {
                dcheck(242,89.85498874,list1->coordinates[i].latitude,0.0);
                dcheck(243,-7.1250163,list1->coordinates[i].longitude,0.0);
                dcheck(244,11704.700,list1->distances[i],0.01); /* S(137) */
            }
        }
    }

    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,13000.0);
    if(list1->length==5)
    {
        for(i=0;i<5;i++)
        {
            j=( (char *) list1->coordinates[i].data) - ix;
            if(j==1)
            {
                dcheck(250, 89.97989055,list1->coordinates[i].latitude,0.0);
                dcheck(251,-153.434948,list1->coordinates[i].longitude,0.0);
                dcheck(252,7615.773,list1->distances[i],0.01); /* S(58) */
            }
            if(j==2)
            {
                dcheck(253, 89.95978111,list1->coordinates[i].latitude,0.0);
                dcheck(254,-116.565051,list1->coordinates[i].longitude,0.0);
                dcheck(255,9219.544,list1->distances[i],0.01); /* S(85) */
            }
            if(j==3)
            {
                dcheck(256, 89.95978111,list1->coordinates[i].latitude,0.0);
                dcheck(257,153.434948,list1->coordinates[i].longitude,0.0);
                dcheck(258,9000.0,list1->distances[i],0.01); /* 9 */
            }
            if(j==4)
            {
                dcheck(259, 89.87123674,list1->coordinates[i].latitude,0.0);
                dcheck(260,12.0947571,list1->coordinates[i].longitude,0.0);
                dcheck(261,9055.385,list1->distances[i],0.01); /* S(82) */
            }
            if(j==5)
            {
                dcheck(262,89.85498874,list1->coordinates[i].latitude,0.0);
                dcheck(263,-7.1250163,list1->coordinates[i].longitude,0.0);
                dcheck(264,11704.700,list1->distances[i],0.01); /* S(137) */
            }
        }
    }
    gccheck(265,list1,  5,"ODAAAAAAAAAAAAAAAAAAAAAAA");

    gicheck(266,gi);
    GeoIndex_free(gi);

/*                                                 */
/*             300 - 399                           */
/*             =========                           */
/*                                                 */

/* batch of massive tests.  These are computed     */
/* only by using (early versions of) this program  */
/* so are more a way of checking it hasn't changed */

    gi=GeoIndex_new();
    la=1.23456789;
    lo=9.87654321;
    for(i=1;i<=40000;i++)
    {
        gcp.latitude = la;
        gcp.longitude= lo;
        gcp.data     = &ix[i%1000];
        r = GeoIndex_insert(gi,&gcp);
        icheck(300,0,r);
        if(i==1322)
        {
            gcp.data++;
            r = GeoIndex_insert(gi,&gcp);
            icheck(299,0,r);
        }
        la+=1.5394057761;
        if(la>90.0) la-=180.0;
        lo+=17.23223155421;
        if(lo>180) lo-=360.0;
    }

    gcp1.latitude = -31.593944;
    gcp1.longitude= -162.445323;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,1130000.0);
    gcmass(301,list1, 239, 32200456);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,54);
    gcmass(303,list1,54,72071221);

    gcp1.latitude = -11.423432;
    gcp1.longitude= 102.612232;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,2130000.0);
    gcmass(305,list1, 733, 48457753);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,91);
    gcmass(307,list1,91,63984952);

    gcp1.latitude = 17.593944;
    gcp1.longitude= 67.812193;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,730000.0);
    gcmass(309,list1, 85, 22185085);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,11);
    gcmass(311,list1,11,82037911);

    gcp1.latitude = 37.812212;
    gcp1.longitude= 19.445323;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,7770000.0);
    gcmass(313,list1, 14278, 119090367);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,1);
    gcmass(315,list1,1,119956719);

    gcp1.latitude = -89.100232;
    gcp1.longitude= -179.999999;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,130000.0);
    gcmass(317,list1, 215, 74977411);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,99);
    gcmass(319,list1,99,112001146);

    gcp1.latitude = 89.593944;
    gcp1.longitude= 179.999999;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,1130000.0);
    gcmass(321,list1, 2256, 75029418);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,54);
    gcmass(323,list1,54,36804959);

    gcp1.latitude = 0.0;
    gcp1.longitude= -179.999999;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,130000.0);
    gcmass(325,list1, 2, 71248308);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,99);
    gcmass(327,list1,99,112901191);

    gcp1.latitude = 0.0;
    gcp1.longitude= 179.999999;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,1130000.0);
    gcmass(329,list1, 198, 25827040);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,54);
    gcmass(331,list1,54,307010);

    gcp1.latitude = 40.0;
    gcp1.longitude= -179.999999;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,130000.0);
    gcmass(333,list1, 5, 21280562);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,99);
    gcmass(335,list1,99,91097429);

    gcp1.latitude = 40.0;
    gcp1.longitude= 179.999999;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,1130000.0);
    gcmass(337,list1, 265, 24184562);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,54);
    gcmass(339,list1,54,63183532);

    gcp1.latitude = -40.0;
    gcp1.longitude= -179.999999;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,130000.0);
    gcmass(341,list1, 1, 19180182);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,99);
    gcmass(343,list1,99,42678696);

    gcp1.latitude = -40.0;
    gcp1.longitude= 179.999999;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,1130000.0);
    gcmass(345,list1, 255, 56578515);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,54);
    gcmass(347,list1,54,78977277);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,754);
    gcmass(349,list1,754,12017442);

    gcp1.latitude = -10.0;
    gcp1.longitude= 89.999999;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,1130000.0);
    gcmass(351,list1, 203, 87076953);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,354);
    gcmass(353,list1,354,2200966);

    gcp1.latitude = -20.0;
    gcp1.longitude= 79.999999;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,830000.0);
    gcmass(355,list1, 116, 94145787);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,854);
    gcmass(357,list1,854,55393902);

    gcp1.latitude = 10.0;
    gcp1.longitude= 49.0;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,1830000.08);
    gcmass(359,list1, 546, 36769740);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,154);
    gcmass(361,list1,154,55726091);

    gcp1.latitude = 55.0;
    gcp1.longitude= 179.77;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,3130000.0);
    gcmass(363,list1, 2934, 117773481);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,222);
    gcmass(365,list1,222,26557002);

    gcp1.latitude = 89,4;
    gcp1.longitude= 179.9;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,930000.0);
    gcmass(367,list1, 1857, 12304881);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,100);
    gcmass(369,list1,100,67148848);

    gcp1.latitude = -89.8;
    gcp1.longitude= 0.0;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,880000.0);
    gcmass(371,list1, 1756, 72919993);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,555);
    gcmass(373,list1,555,49499403);

    gcp1.latitude = 89.9;
    gcp1.longitude= 0.0;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,3330000.0);
    gcmass(375,list1, 6653, 85304684);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,101);
    gcmass(377,list1,101,119146068);

    gcp1.latitude = 45.0;
    gcp1.longitude= 45.9;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,3330000.0);
    gcmass(379,list1,2588,90353278);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,99);
    gcmass(381,list1,99,26847261);

    gcp1.latitude = -60.0;
    gcp1.longitude= -45.00001;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,1730000.0);
    gcmass(383,list1, 968, 26785900);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,2);
    gcmass(385,list1,2,122455317);

    gcp1.latitude = 44.999999;
    gcp1.longitude= 45.0000001;
    list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,430000.0);
    gcmass(387,list1, 45, 121133559);
    list1 = GeoIndex_NearestCountPoints(gi,&gcp1,111);
    gcmass(389,list1,111,15866065);

    if(mode==2)
    {
        gcp1.latitude = 44.999999;
        gcp1.longitude= 45.0000001;
        printf("start of 50000 x points within radius (43 found)\n");
        for(i=0;i<50000;i++)
        {
            list1 = GeoIndex_PointsWithinRadius(gi,&gcp1,430000.0);
            GeoIndex_CoordinatesFree(list1);
        }
        printf("End of timing test\n");
    }

    if(mode==3)
    {
        printf("start of 10000 x points by count (2)\n");
        for(i=0;i<4;i++)
        {
            if(i==0)
            {
                gcp1.latitude = 44.999999;
                gcp1.longitude= 45.0000001;
            }
            if(i==1)
            {
                gcp1.latitude = -44.999999;
                gcp1.longitude= 45.0000001;
            }
            if(i==2)
            {
                gcp1.latitude = 44.999999;
                gcp1.longitude= -45.0000001;
            }
            if(i==3)
            {
                gcp1.latitude = -44.999999;
                gcp1.longitude= -45.0000001;
            }
            for(j=0;j<2500;j++)
            {
                list1 = GeoIndex_NearestCountPoints(gi,&gcp1,2);
                GeoIndex_CoordinatesFree(list1);
            }
        }
        printf("End of timing test\n");
    }

    gicheck(390,gi);

    GeoIndex_free(gi);

    if(errors==0) {
        printf("Georeg Regression Program succeeded\n");
        return 0;
    }
    else {
        printf("Georeg Regression Program failed with %d errors\n",
            errors);
        return 1;
    }
}

/* end of georeg.c  */
