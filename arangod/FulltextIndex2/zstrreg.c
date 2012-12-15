/* zstring regression program  */
/* R. A. Parker     15.11.2012  */

#include <stdio.h>
#include <stdint.h>
#include "zstr.h"

int err;

void ckint(int x, int was, int shdbe)
{
    if(was==shdbe) return;
    err++;
    printf("Error %d, was %x (%d), should be %x\n",x,was,was,shdbe);
}

void ZDUMP(ZSTR * z)
{
    int i;
    printf("alloc %d firstix %d lastix %d\n",
            z->alloc,z->firstix,z->lastix);
    for(i=z->firstix;i<=z->lastix;i++)
        printf("ix %d,  val %16llx length %d\n",
             i,(unsigned long long)z->dat[i],(int)z->dlen[i]);
}

void TUBDUMP(TUBER * t)
{
    long i1,i2,i3,i4,i5,i6;
    int i;
    long long ff;
    i1=t->kperw;
    i2=t->kmax;
    i3=t->wct;
    i4=t->tiptop;
    i5=t->lenlen;
    i6=t->mult;
    printf("kperw %ld,  kmax %ld, wct %ld, ",
              i1,i2,i3);
    printf("tiptop %ld,  lenlen %ld, mult %ld\n",
              i4,i5,i6);
    for(i=0;i<i3;i++)
    {
        ff=(long long)t->tub[i];
        printf("%16llx ",ff);
        if(i%5==4) printf("\n");
    }
    if((i%5)!=0) printf("\n");   
}

int main(int argc, char ** argv)
{
    uint16_t y[10];
/* first test code  0xx  10xxx 11xxxx  */
/*                  0-3   4-11  12-27  */

    uint64_t tx1[]={0,4,12,28};
    uint64_t tc1[]={0,0x10,0x30};
    uint8_t  tl1[]={3,5,6};
    uint8_t tsg1[]={0,0,1,2};
    ZCOD zc1 = {1,3,0,2,tx1,tc1,tl1,tsg1,tl1,tl1};

/* second test code  0xx  10xxx 11xxxx  */
/*                   0-3   4-11  12-27  */
/* after translation 0 1 2 3 4 5 6      */
/*        goes to    4 5 0 2 1 6 3      */

    uint64_t tx2[]={0,4,12,28};
    uint64_t tc2[]={0,0x10,0x30};
    uint8_t  tl2[]={3,5,6};
    uint8_t tsg2[]={0,0,1,2};
    uint8_t ttx2[]={4,5,0,2,1,6,3};
    uint8_t tux2[]={2,4,3,6,0,1,5};
    ZCOD zc2 = {2,3,6,2,tx2,tc2,tl2,tsg2,ttx2,tux2};

/* third test code  0xx  10xxx 11xxxx  */
/*   with delta     0-3   4-11  12-27  */

    uint64_t tx3[]={0,4,12,28};
    uint64_t tc3[]={0,0x10,0x30};
    uint8_t  tl3[]={3,5,6};
    uint8_t tsg3[]={0,0,1,2};
    ZCOD zc3 = {3,3,0,2,tx3,tc3,tl3,tsg3,tl3,tl3};

    STEX * st;
    uint16_t sw0[]={0x0000};
    uint16_t sw2[]={0xFFFC};

    TUBER * t1;
    uint64_t stats[20];

    ZSTR * z1;
    CTX ctx;
    uint64_t i,j,k;
    uint64_t nokeys;
    long len;
    uint64_t d1,d2;
    uint64_t b0,b1,b2,b3,b4,b5,b6,b7;
    uint64_t k0,k1,k2,k3,k4,k5,k6,k7;
    uint16_t * fw1;
    int q;
    err=0;

/*                                                     */
/* 001 - 020 First batch to just exercise the simple   */
/*           bit handling routines a little            */

    z1=ZStrCons(3);
    ckint(1,z1->alloc,3);      /* did it allocate OK   */
    len=ZStrLen(z1);
    ckint(2,len,0);            /* len=0 at start       */
    ZStrBitsIn(0x05A792,24,z1);
    len=ZStrLen(z1);
    ckint(3,len,24);           /* len=24 now           */
    ZStrBitsIn(0xF,4,z1);
    len=ZStrLen(z1);
    ckint(4,len,28);           /* len=28 now           */
    j=ZStrBitsPeek(z1,16);
    ckint(5,j,0x05A7);         /* first 16 bits        */
    len=ZStrLen(z1);
    ckint(6,len,28);           /* length the same      */
    j=ZStrBitsOut(z1,8);
    ckint(7,j,0x05);           /* first 8 bits         */
    len=ZStrLen(z1);
    ckint(8,len,20);           /* length 20 now        */
    j=ZStrBitsPeek(z1,28);
    ckint(9,j,0xA792F00);      /* first 28 bits!       */
    j=ZStrBitsOut(z1,12);
    ckint(10,j,0xA79);         /* last 12 bits         */
    len=ZStrLen(z1);
    ckint(11,len,8);           /* length 8 now         */
    ZStrBitsIn(0xC0,8,z1);
    len=ZStrLen(z1);
    ckint(12,len,16);          /* length 16            */  
    j=ZStrBitsPeek(z1,16);
    ckint(13,j,0x2FC0);        /* 0x2FC0 -> Normalize  */
    ZStrNormalize(z1);
    j=ZStrBitsPeek(z1,16);
    ckint(14,j,0x2FC0);        /* still 0x2FC0         */
    len=ZStrLen(z1);
    ckint(15,len,10);          /* length 11 now        */  
    ZStrClear(z1);
    len=ZStrLen(z1);
    ckint(16,len,0);           /* length 0 now         */  
    j=ZStrBitsPeek(z1,28);
    ckint(17,j,0);             /* last 28 bits all 0   */
    j=ZStrBitsOut(z1,12);
    ckint(18,j,0);             /* last 12 bits all 0   */
    ZStrDest(z1);

/*                                                     */
/* 0021 - 039 Next batch to test basic Enc/Decode      */
/*                                                     */
/* test code  0xx   10xxx 11xxxx                       */
/*            0-3   4-11  12-27  (28+ illegal)         */

    z1=ZStrCons(3);
    ZStrCxEnc(z1,&zc1,&ctx,3);
    len=ZStrLen(z1);
    ckint(21,len,3);           /* length 3 now         */  
    j=ZStrBitsPeek(z1,5);
    ckint(22,j,0xC);           /* 011 00               */
    j=ZStrCxDec(z1,&zc1,&ctx);
    ckint(23,j,3);
    len=ZStrLen(z1);
    ckint(24,len,0);           /* length 0 now         */
    ZStrClear(z1);
    ZStrCxEnc(z1,&zc1,&ctx,27); /* put in limit values  */
    ZStrCxEnc(z1,&zc1,&ctx,4);
    ZStrCxEnc(z1,&zc1,&ctx,3);
    ZStrCxEnc(z1,&zc1,&ctx,12);
    ZStrCxEnc(z1,&zc1,&ctx,11);
    ZStrCxEnc(z1,&zc1,&ctx,0);
    len=ZStrLen(z1);
    ckint(25,len,28);          /* length should be 28 */  
    ZStrNormalize(z1);
    len=ZStrLen(z1);
    ckint(26,len,25);          /* length should be 25 */
    j=ZStrCxDec(z1,&zc1,&ctx);
    ckint(27,j,27);
    j=ZStrCxDec(z1,&zc1,&ctx);
    ckint(28,j,4);
    j=ZStrCxDec(z1,&zc1,&ctx);
    ckint(29,j,3);
    j=ZStrCxDec(z1,&zc1,&ctx);
    ckint(30,j,12);
    j=ZStrCxDec(z1,&zc1,&ctx);
    ckint(31,j,11);
    j=ZStrCxDec(z1,&zc1,&ctx);
    ckint(32,j,0);
    ZStrClear(z1);
    j=0;
    for(i=0;i<1000;i++)
    {
        j+=11;
        if(j>27) j-=28;
        ZStrCxEnc(z1,&zc1,&ctx,j);
    }
    ZStrNormalize(z1);
    j=0;
    for(i=0;i<1000;i++)
    {
        j+=11;
        if(j>27) j-=28;
        k=ZStrCxDec(z1,&zc1,&ctx);
        ckint(33,k,j);
    }
    len=ZStrLen(z1);
    ckint(34,len,0);
    ZStrNormalize(z1);
    ZStrDest(z1);

/*                                                     */
/* 0041 - 059 Next batch to test type 2 Enc/Decode     */

/* second test code  0xx  10xxx 11xxxx  */
/*                   0-3   4-11  12-27  */
/* after translation 0 1 2 3 4 5 6      */
/*        goes to    4 5 0 2 1 6 3      */                   

    z1=ZStrCons(3);
    ZStrCxEnc(z1,&zc2,&ctx,6);
    len=ZStrLen(z1);
    ckint(41,len,3);           /* length 3 now         */  
    j=ZStrBitsPeek(z1,5);
    ckint(42,j,0xC);           /* 011 00               */
    j=ZStrCxDec(z1,&zc2,&ctx);
    ckint(43,j,6);
    len=ZStrLen(z1);
    ckint(44,len,0);           /* length 0 now         */
    ZStrClear(z1);
    ZStrCxEnc(z1,&zc2,&ctx,27); /* put in limit values  */
    ZStrCxEnc(z1,&zc2,&ctx,0);  /* 4  */
    ZStrCxEnc(z1,&zc2,&ctx,6);  /* 3  */
    ZStrCxEnc(z1,&zc2,&ctx,12);
    ZStrCxEnc(z1,&zc2,&ctx,11);
    ZStrCxEnc(z1,&zc2,&ctx,2);  /* 0  */
    len=ZStrLen(z1);
    ckint(45,len,28);          /* length should be 28 */  
    ZStrNormalize(z1);
    len=ZStrLen(z1);
    ckint(46,len,25);          /* length should be 25 */
    j=ZStrCxDec(z1,&zc2,&ctx);
    ckint(47,j,27);
    j=ZStrCxDec(z1,&zc2,&ctx);
    ckint(48,j,0);
    j=ZStrCxDec(z1,&zc2,&ctx);
    ckint(49,j,6);
    j=ZStrCxDec(z1,&zc2,&ctx);
    ckint(50,j,12);
    j=ZStrCxDec(z1,&zc2,&ctx);
    ckint(51,j,11);
    j=ZStrCxDec(z1,&zc2,&ctx);
    ckint(52,j,2);
    ZStrClear(z1);
    j=0;
    for(i=0;i<1000;i++)
    {
        j+=11;
        if(j>27) j-=28;
        ZStrCxEnc(z1,&zc2,&ctx,j);
    }
    ZStrNormalize(z1);
    j=0;
    for(i=0;i<1000;i++)
    {
        j+=11;
        if(j>27) j-=28;
        k=ZStrCxDec(z1,&zc2,&ctx);
        ckint(53,k,j);
    }
    len=ZStrLen(z1);
    ckint(54,len,0);
    ZStrNormalize(z1);
    ZStrDest(z1);

/*                                                     */
/* 0060 - 079 Test Xlate and UnXl                      */
/* after translation 0 1 2 3 4 5 6      */
/*        goes to    4 5 0 2 1 6 3      */ 

    k=ZStrXlate(&zc2,0);
    ckint(60,k,4);
    k=ZStrXlate(&zc2,1);
    ckint(61,k,5);
    k=ZStrXlate(&zc2,2);
    ckint(62,k,0);
    k=ZStrXlate(&zc2,3);
    ckint(63,k,2);
    k=ZStrXlate(&zc2,4);
    ckint(64,k,1);
    k=ZStrXlate(&zc2,5);
    ckint(65,k,6);
    k=ZStrXlate(&zc2,6);
    ckint(66,k,3);
    k=ZStrXlate(&zc2,7);
    ckint(67,k,7);
    k=ZStrXlate(&zc2,17);
    ckint(68,k,17);
    k=ZStrXlate(&zc2,77777);
    ckint(69,k,77777);

    k=ZStrUnXl(&zc2,0);
    ckint(70,k,2);
    k=ZStrUnXl(&zc2,1);
    ckint(71,k,4);
    k=ZStrUnXl(&zc2,2);
    ckint(72,k,3);
    k=ZStrUnXl(&zc2,3);
    ckint(73,k,6);
    k=ZStrUnXl(&zc2,4);
    ckint(74,k,0);
    k=ZStrUnXl(&zc2,5);
    ckint(75,k,1);
    k=ZStrUnXl(&zc2,6);
    ckint(76,k,5);
    k=ZStrUnXl(&zc2,7);
    ckint(77,k,7);
    k=ZStrUnXl(&zc2,17);
    ckint(78,k,17);
    k=ZStrUnXl(&zc2,7777);
    ckint(79,k,7777);

/*                                                     */
/* 0080 - 099 Test Enc/Decode of type 3 (delta) code   */
/*                                                     */
/* test code  0xx   10xxx 11xxxx         DELTA         */
/*            0-3   4-11  12-27  (28+ illegal)         */

    z1=ZStrCons(3);
    ZStrCxClear(&zc3,&ctx);
    ZStrCxEnc(z1,&zc3,&ctx,3);
    ZStrCxEnc(z1,&zc3,&ctx,5);
    ZStrCxEnc(z1,&zc3,&ctx,9); /* 011 010 10000        */
    len=ZStrLen(z1);
    ckint(80,len,11);          /* length 11 now        */  
    j=ZStrBitsPeek(z1,10);
    ckint(81,j,0x1A8);         /* 011 010 1000         */
    ZStrNormalize(z1);
    j=ZStrBitsPeek(z1,10);
    ckint(82,j,0x1A8);         /* 011 010 1000         */
    ZStrCxClear(&zc3,&ctx);
    j=ZStrCxDec(z1,&zc3,&ctx);
    ckint(83,j,3);
    j=ZStrCxDec(z1,&zc3,&ctx);
    ckint(84,j,5);
    j=ZStrCxDec(z1,&zc3,&ctx);
    ckint(85,j,9);
    len=ZStrLen(z1);
    ckint(86,len,0);           /* length 0 now         */
    j=ZStrCxDec(z1,&zc3,&ctx);
    ckint(87,j,9);
    j=ZStrCxDec(z1,&zc3,&ctx);
    ckint(88,j,9);

    ZStrClear(z1);
    ZStrCxClear(&zc3,&ctx);
    j=0;
    for(i=0;i<1000;i++)
    {
        j+=4;
        ZStrCxEnc(z1,&zc3,&ctx,j);
    }
    ZStrNormalize(z1);
    ZStrCxClear(&zc3,&ctx);
    j=0;
    for(i=0;i<1000;i++)
    {
        j+=4;
        k=ZStrCxDec(z1,&zc3,&ctx);
        ckint(89,k,j);
    }
    len=ZStrLen(z1);
    ckint(90,len,0);
    ZStrDest(z1);
/*                                                     */
/*  100 - 119 Test Extract, Insert and ExtLen          */
    z1=ZStrCons(3);
    ZStrBitsIn(0xDEADBEEF,32,z1);
    len=ZStrLen(z1);
    ckint(100,len,32);         /* len=32 now           */
    len=ZStrMaxLen(z1,2);
    ckint(101,len,3);
    len=ZStrExtract(z1,(void *)y,2);
    ckint(102,len,3);
    ckint(103,y[0],0xDEAE);
    ckint(104,y[1],0xBEEF);
    ckint(105,y[2],0x8000);
    ZStrDest(z1);
    z1=ZStrCons(5);
    ZStrInsert(z1,(void *)y,2);
    len=ZStrLen(z1);
    ckint(106,len,32);
    j=ZStrBitsOut(z1,32);
    ckint(107,j,0xDEADBEEF);
    ZStrDest(z1);

/*  200 - 299 - test the codes and hashes  */
/*            not yet written              */ 

/*  300 - 399 test the tuber things        */

    z1=ZStrCons(3);
    for(q=0;q<400;q+=100)
    {
        if(q==0)   t1 = ZStrTuberCons(152,TUBER_BITS_8);
        if(q==100) t1 = ZStrTuberCons(152,TUBER_BITS_16);
        if(q==200) t1 = ZStrTuberCons(152,TUBER_BITS_32);
        if(q==300) t1 = ZStrTuberCons(152,TUBER_BITS_64);

        nokeys=t1->kmax;
        d1=nokeys/2;
        d2=0;
/* try inserting three items with same keya      */
/* should get keybe as 0, 1 and 2 respectively.  */
/* this relies on three inserts working with keyb*/
/* coming out as 0, 1 and 2.  Change construction*/
/* size if this doesn't work.                    */
        b0=ZStrTuberIns(t1,d1,d2);
        ckint(300+q,b0,0);
        b1=ZStrTuberIns(t1,d1,d2);
        ckint(301+q,b1,1);
        b2=ZStrTuberIns(t1,d1,d2);
        ckint(302+q,b2,2);
        k0=ZStrTuberK(t1,d1,d2,b0);
        k1=ZStrTuberK(t1,d1,d2,b1);
        k2=ZStrTuberK(t1,d1,d2,b2);
        ZStrTuberDelete(t1,k0);

        ZStrBitsIn(0xDEAD,16,z1);
        ZStrNormalize(z1);
        ZStrTuberUpdate(t1,k1,z1);
        ZStrClear(z1);
        j=ZStrTuberRead(t1,k1,z1);
        ckint(303+q,j,0);
        len=ZStrLen(z1);
        ckint(304+q,len,16);
        j=ZStrBitsOut(z1,16);
        ckint(305+q,j,0xDEAD);             /* get our data back */
        ZStrTuberDest(t1);
    }
    ZStrDest(z1);
/*                                                     */
/*  700 - 799 STEX testing - sorting the words         */
/*                                                     */

    z1=ZStrCons(3); 
    st=ZStrSTCons(2);
    ZStrBitsIn(0xDB,8,z1);
    ZStrSTAppend(st,z1);
    ZStrSTSort(st);
    fw1=ZStrSTFind(st,(void*) sw0);
    if(fw1!=NULL) ckint(704,*fw1,7777);
    fw1=ZStrSTFind(st,(void*) sw2);
    ckint(705,*fw1,0xDB00);
    ZStrSTDest(st);
    st=ZStrSTCons(2);
    for(i=1;i<100;i++)
    {
        j=(17*i)%97;
        ZStrClear(z1);
        ZStrBitsIn(j,8,z1);
        ZStrSTAppend(st,z1);
    }
    ZStrSTSort(st);
    ZStrSTDest(st);
    ZStrDest(z1);

/*                                                     */
/*  800 - 810 LastEnc and LastDec testing              */
/*                                                     */

    z1=ZStrCons(5);
    for(i=0;i<10000;i++)
    {
        ZStrClear(z1);
        ZStrLastEnc(z1,i);
        j=ZStrLastDec(z1);
        ckint(800,j,i);
    }
    ZStrDest(z1);
    

    printf("End of z-string regression - %d errors\n",err);
    return 0;
}

/* end of zstring regression module  */
