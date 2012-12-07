/* zstr.c - the Z-string module   */
/*   R. A. Parker    3.12.2012   */
/* bugfixed in merge - adjtop call added  */
/* bugfixed in tuber - wraparound */
/* bugfix shift of 64 not happening */
/* bugfix peeking after last word  */
/* bugfix use firstix not z->firstix  */
/* bugfix page turn */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "zstr.h"

ZSTR * ZStrCons(int elts)
{
    ZSTR * z;
    z=malloc(sizeof(ZSTR));
    if(z==NULL) return NULL;
    z->dat=malloc(elts*sizeof(uint64_t));
    if(z->dat==NULL)
    {
        free(z);
        return NULL;
    }
    z->dlen=malloc(elts*sizeof(long));
    if(z->dlen==NULL)
    {
        free(z->dat);
        free(z);
        return NULL;
    }
    z->alloc=elts;
    z->firstix=0;
    z->lastix=0;
    z->dat[0]=0;
    z->dlen[0]=0;
    return z;
}

void ZStrDest(ZSTR * z)
{
    free(z->dat);
    free(z->dlen);
    free(z);
}

void ZStrClear(ZSTR * z)
{
    z->firstix=0;
    z->lastix=0;
    z->dat[0]=0;
    z->dlen[0]=0;
}

int ZStrBitsIn(uint64_t a, long bits, ZSTR * z)
{
    long clen;
    void * ptr;
    clen=z->dlen[z->lastix];
    if(clen+bits <= 64)
    {
        z->dat[z->lastix]=(z->dat[z->lastix]<<bits) + a;
        z->dlen[z->lastix]=clen+bits;
    }
    else
    {
        if(z->lastix+1 >= z->alloc)
        {
            z->alloc=(z->alloc + z->alloc/4 + 2);
            ptr=realloc(z->dat,z->alloc*sizeof(uint64_t));
            if(ptr==NULL) return 1;
            z->dat=ptr;
            ptr=realloc(z->dlen,z->alloc*sizeof(long));
            if(ptr==NULL) return 1;
            z->dlen=ptr;
        }
        z->lastix++;
        z->dat[z->lastix]=a;
        z->dlen[z->lastix]=bits;
    }
    return 0;
}

uint64_t ZStrBitsOut(ZSTR * z, long bits)
{
    uint64_t s,t;
    long slen,wlen;
    s=0;
    slen=0;
    while( slen+z->dlen[z->firstix] <= bits)
    {
        s<<=z->dlen[z->firstix];
        s+=z->dat[z->firstix];
        slen+=z->dlen[z->firstix];
        if(z->firstix==z->lastix)
        {
            z->dlen[z->firstix]=0;
            z->dat[z->firstix]=0;
            return s<<(bits-slen);
        }
        z->firstix++; 
    }
    wlen=bits-slen;
    if(wlen==0) return s;
    s<<=wlen;
    t=z->dat[z->firstix]>>(z->dlen[z->firstix]-wlen);
    s+=t;
    z->dat[z->firstix]^=(t<<(z->dlen[z->firstix]-wlen));
    z->dlen[z->firstix]-=wlen;
    return s;
}

uint64_t ZStrBitsPeek(ZSTR * z, long bits)
{
    uint64_t s;
    int firstix;
    long slen,wlen;
    s=0;
    slen=0;
    firstix=z->firstix;
    while( slen+z->dlen[firstix] <= bits)
    {
        s<<=z->dlen[firstix];
        s+=z->dat[firstix];
        slen+=z->dlen[firstix];
/* bugfix peeking after last word  */
        if(firstix==z->lastix)
            return s<<(bits-slen);
        firstix++; 
    }
    wlen=bits-slen;
    if(wlen==0) return s;
    s<<=wlen;
/* bugfix use firstix not z->firstix  */
    s+=z->dat[firstix]>>(z->dlen[firstix]-wlen);
    return s;
}

long ZStrLen(ZSTR * z)
{
    long tot;
    int i;
    tot=0;
    for(i=z->firstix;i<=z->lastix;i++) tot+=z->dlen[i];
    return tot;
}

void ZStrNormalize(ZSTR * z)
{
    while(z->lastix>z->firstix)
    {
        if(z->dat[z->lastix]!=0) break;
        z->lastix--;
    }
    if(z->dat[z->lastix]==0)
    {
        z->dlen[z->lastix]=0;
        return;
    }
    while( (z->dat[z->lastix]&1)==0 )
    {
        z->dat[z->lastix]>>=1;
        z->dlen[z->lastix]--;
    }
}

int ZStrEnc(ZSTR * z, ZCOD * zc, uint64_t a)
{
    int seg;
    switch (zc->t)
    {
      case 1:
        for(seg=1;seg<=zc->s;seg++)
            if(a<zc->X[seg]) break;
        seg--;
        return ZStrBitsIn(a-zc->X[seg]+zc->C[seg],zc->L[seg],z);
      case 2:
        if(a<=zc->tmax) a=zc->TX[a];
        for(seg=1;seg<=zc->s;seg++)
            if(a<zc->X[seg]) break;
        seg--;
        return ZStrBitsIn(a-zc->X[seg]+zc->C[seg],zc->L[seg],z);
      default:
        printf("invalid ZCOD type %d\n",zc->t);
        exit(16);
    }
}

uint64_t ZStrDec(ZSTR * z, ZCOD * zc)
{
    int seg;
    uint64_t topbit,s;
    switch (zc->t)
    {
      case 1:
        topbit=ZStrBitsPeek(z,zc->bits);
        seg=zc->SG[topbit];
        s=ZStrBitsOut(z,zc->L[seg]);
        return (s-zc->C[seg])+zc->X[seg];
      case 2:
        topbit=ZStrBitsPeek(z,zc->bits);
        seg=zc->SG[topbit];
        s=ZStrBitsOut(z,zc->L[seg]);
        s = (s-zc->C[seg])+zc->X[seg];
        if(s<=zc->tmax) s=zc->UX[s];
        return s;
      default:
        printf("invalid ZCOD type %d\n",zc->t);
        exit(18);
    }
}

uint64_t ZStrXlate(ZCOD * zc, uint64_t a)
{
    if(a<=zc->tmax) return zc->TX[a];
    return a;
}

uint64_t ZStrUnXl(ZCOD * zc, uint64_t a)
{
    if(a<=zc->tmax) return zc->UX[a];
    return a;
}

int ZStrLastEnc(ZSTR * z, uint64_t a)
{
    uint64_t b;
    long len;
    if(a==0) return 0;
    b=a;
    len=1;
    while(b>1)
    {
        len++;
        b>>=1;
    }
    a-=b<<(len-1);
    return ZStrBitsIn(1+(a<<1),len,z);
}

uint64_t ZStrLastDec(ZSTR * z)
{
    long len;
    uint64_t num,x;
    len=ZStrLen(z);

    if(len==0) num=0;
    else
    {
        num=ZStrBitsOut(z,len);
        x=1;
        x<<=len;
        num+=x;
    }
    return (num>>1);
}

void ZStrCxClear(ZCOD * zc, CTX * ctx)
{
    ctx->x1=0;
}

int ZStrCxEnc(ZSTR * z, ZCOD * zc, CTX * ctx, uint64_t a)
{
    int seg;
    uint64_t b;
    switch (zc->t)
    {
      case 1:
      case 2:
        return ZStrEnc(z,zc,a);
      case 3:
        b=a-ctx->x1;
        ctx->x1=a;
        for(seg=1;seg<=zc->s;seg++)
            if(b<zc->X[seg]) break;
        seg--;
        return ZStrBitsIn(b-zc->X[seg]+zc->C[seg],zc->L[seg],z);
      default:
        printf("invalid ZCOD type %d\n",zc->t);
        exit(17);
    }
}

uint64_t ZStrCxDec(ZSTR * z, ZCOD * zc, CTX * ctx)
{
    int seg;
    uint64_t topbit,s;
    switch (zc->t)
    {
      case 1:
      case 2:
        return ZStrDec(z,zc);
      case 3:
        topbit=ZStrBitsPeek(z,zc->bits);
        seg=zc->SG[topbit];
        s=ZStrBitsOut(z,zc->L[seg]);
        s = (s-zc->C[seg])+zc->X[seg];
        ctx->x1+=s;
        return ctx->x1;
      default:
        printf("invalid ZCOD type %d\n",zc->t);
        exit(17);
    }
}


int ZStrMaxLen(ZSTR * z, int fmt)
{
    uint64_t x;
    if(fmt==2) x=15;
    else
    {
        printf("unknown format %d in ZStrMaxLen\n",fmt);
        exit(33);
    }
    return 1+(ZStrLen(z)/x);
}

int ZStrExtract(ZSTR * z, void * x, int fmt)
{
    uint16_t * x2;
    uint64_t s;
    int len;
    int words;
    words=1;
    if(fmt==2)
    {
        x2=(uint16_t *)x;
        ZStrNormalize(z);
        len=ZStrLen(z);
        while(len>14)
        {
            words++;
            s=ZStrBitsPeek(z,15);
            if( (s&1)==1 )
            {
                s=ZStrBitsOut(z,15);
                *(x2++)=1+(s<<1);
                len-=15;
            }
            else
            {
                s=ZStrBitsOut(z,16);
                *(x2++)=1+s;
/* next line looks unsafe, but all non-zero z-strings have      */
/* their last bit 1, so if length is 15, previous case applies  */
                len-=16;
            }
        }
        s=ZStrBitsOut(z,14);
        *x2 = s<<2;
        return words;
    }
    printf("Format %d not known in ZStrExtract\n",fmt);
    return 0;
}

int ZStrInsert(ZSTR * z, void * x, int fmt)
{
    uint16_t * x2;
    uint64_t s;
    int r;
    if(fmt==2)
    {
        x2=(uint16_t *)x;
        ZStrClear(z);
        while(1)
        {
            s=*(x2++);
            if( (s&3)==0 )
            {
                r=ZStrBitsIn(s>>2,14,z);
                if(r!=0) return r;
                ZStrNormalize(z);
                return 0;
            }
            if( (s&3)==3 )
                r=ZStrBitsIn(s>>1,15,z);
            else
                r=ZStrBitsIn(s-1,16,z);
            if(r!=0) return r;
        }
    }
    return 1;
}            

int ZStrExtLen(void * x, int fmt)
{
    uint16_t * w;
    int len;
    w = (uint16_t *) x;
    len=1;
    while(((*(w++))&3)!=0) len++;
    return len;
}

STEX * ZStrSTCons(int fmt)
{
    STEX * st;
    int i;
    st=malloc(sizeof(STEX));
    if(st==NULL) return NULL;
    st->pst=malloc(1281*sizeof(uint16_t *));
    if(st->pst==NULL)
    {
        free(st);
        return NULL;
    }
    st->ptp=malloc(1281*sizeof(uint16_t *));
    if(st->ptp==NULL)
    {
        free(st->pst);
        free(st);
        return NULL;
    }
    st->mal=malloc(1281*sizeof(uint64_t));
    if(st->mal==NULL)
    {
        free(st->ptp);
        free(st->pst);
        free(st);
        return NULL;
    }
    st->stcnt=malloc(1281*sizeof(uint64_t));
    if(st->stcnt==NULL)
    {
        free(st->mal);
        free(st->ptp);
        free(st->pst);
        free(st);
        return NULL;
    }
    for(i=0;i<1281;i++)
        st->mal[i]=0;
    for(i=0;i<6;i++) st->inuse[i]=0;
    st->listm=0;
    return st;
}

void ZStrSTDest(STEX * st)
{
    int i;
    for(i=0;i<1281;i++)
        if(st->mal[i]!=0) free(st->pst[i]);
    if(st->listm!=0) free(st->list);
    free(st->pst);
    free(st->ptp);
    free(st->mal);
    free(st->stcnt);
    free(st);
}

int ZStrExtCompare(void * a, void * b, int fmt)
{
    uint16_t *a1, *b1;
    a1=(uint16_t *) a;
    b1=(uint16_t *) b;
    while(1)
    {
        if((*a1) < (*b1)) return -1;
        if((*a1) > (*b1)) return 1;
        if(((*a1)&3)==0)
        {
            if(((*b1)&3)==0)
                return 0;
            return -1;
        }
        if(((*b1)&3)==0)
            return 1;
        a1++;
        b1++;
    }
}

typedef struct
{
    STEX * st;
    uint16_t pq[256];
    uint16_t ch[128];
} SICH;
#define DEBUG

#ifdef DEBUG

void dumpheap(SICH * si)
{
    STEX * st;
    int i,dat,ch;
    st=si->st;
    for(i=1;i<=50;i++)
    {
        dat=0xABCD;
        if(i<128) ch=si->ch[i];
           else   ch=-1;
        if(si->pq[i]<1280) dat=*(st->pst[si->pq[i]]);
        printf("nd %3d pq %3d ch %3d dt %x\n",
            i,si->pq[i],ch,dat);
    }
}

#endif

/* the first letter of variables are used . . .  */

/* h  int 1-255 index si (pq,ch) heap numbers    */
/*  si      look them up in SICH pq and you get  */

/* s  uint16_t 0-1278 index st.  slot numbers    */
/*  st      look them up in STEX st (pst,etc)    */

#define EXPIRED 10000

static void pqadvance(SICH * si, int htop)
{
    uint16_t snode;
    STEX * st;
    st=si->st;
    snode=si->pq[htop];
    st->stcnt[snode]--;
    if(st->stcnt[snode]==0)
    {
        si->pq[htop]=EXPIRED;
        return;
    }
    while((*(st->ptp[snode])&3)!=0) st->ptp[snode]++;
    st->ptp[snode]++;
    return;
}

static int heapcomp(SICH * si, int ha, int hb)
{
    STEX * st;
    int r;
    uint16_t *wa,*wb;
    st=si->st;
    if(si->pq[hb]==EXPIRED) return -1;
    if(si->pq[ha]==EXPIRED) return 1;
    wa=st->ptp[si->pq[ha]];
    wb=st->ptp[si->pq[hb]];
    r= ZStrExtCompare((void*)wa,(void*)wb,2);
    return r;
}

/* v  int 0-7 index spath.  level of operation   */
/*  spath      look them up in spath to get an h */

static void adjtop(SICH * si, int htop)
{
    int spath[8];                /* h = spath(v)        */
    int vlev;
    int hcur,hpar,hsib;   /* 1-255 heap points   */
    int r;
    uint16_t temp;
    vlev=0;
    hcur=htop;
    while(1)    /* loop over all strings to insert  */
    {
/* populate the special path */
        while(1)
        {
            spath[vlev]=hcur;
            if(hcur>=128) break;
            if( (si->pq[hcur]==EXPIRED) && (hcur!=htop) ) break;
            hcur=2*hcur+si->ch[hcur];
            vlev++;
        }
        while(1)       /* find the correct place to put hcur  */
        {
            if(vlev==0) return;
            r = heapcomp(si,htop,hcur);
            if(r!=-1) break;
            vlev--;
            hcur=spath[vlev];
        }
        if(r==1) while(1)   /* bump up */
        {
            if(vlev==0) return;
            hpar=spath[vlev-1];
            hsib=hcur^1;
            r = heapcomp(si,htop,hsib);
            if(r==0) break;
            if(r==1) si->ch[hpar]^=1;
            temp=si->pq[hcur];
            si->pq[hcur]=si->pq[htop];
            si->pq[htop]=temp;
            vlev--;
            hcur=spath[vlev];
        }  
        pqadvance(si, htop);
    }
}

/* Return pointer to last string <= x */

void * ZStrSTFind(STEX * st, void * x)
{
    uint16_t *wx, *w3, *w1, *w2;
    int i;
    if(st->listw==0) return NULL; /* list is empty         */
    wx = (uint16_t *) x;
    w1=st->list;                  /* very first word       */
    w3=w1+st->listw-2;            /* just before last word */
    i=ZStrExtCompare( (void*)w1, (void*)wx,2);
    if(i>0) return NULL;          /* first word bigger     */
    while(w3>=w1)
    {
        if(((*w3)&3)==0) break;
        w3--;
    }
    w3++;                         /* first word of last string */
/* x1 and x3 point to first and last string */
    while(w1!=w3)
    {
        w2=w1+(w3-w1)/2;
        while(w2>=w1)
        {
            if(((*w2)&3)==0) break;
            w2--;
        }
        w2++;
        if(w2==w1)              /* no earlier start - try later */
        {
            w2=w1+(w3-w1)/2;
            while(w2<w3)
            {
                if(((*w2)&3)==0) break;
                w2++;
            }
            w2++;
            if(w2>=w3) return w1;
        }
        i=ZStrExtCompare( (void*)w2, (void*)wx,2);
        if(i>0) w3=w2;
         else   w1=w2;
    }
    return w1;
}

static int merge(STEX * st, int layer)
{
    uint16_t sfst,slst,snpl,ssc,i;
    uint16_t *wout, *w1;
    SICH si;
    size_t mem;
    int hcur,r;
    if(st->inuse[layer]==0) return 0;
    si.st=st;
    sfst=256*layer;
    slst=sfst+st->inuse[layer];    /* one more than last  */
    snpl=256*(layer+1)+st->inuse[layer+1];  /* new place */
    hcur=1;
    mem=0;
    for(i=sfst;i<slst;i++)
    {
        mem+=(st->ptp[i]-st->pst[i])*sizeof(uint16_t);
        st->ptp[i]=st->pst[i];
        si.pq[hcur++]=i;
    }
    while(hcur<256) si.pq[hcur++]=EXPIRED;

    if(mem>st->mal[snpl])
    {
        if(st->mal[snpl]!=0) free(st->pst[snpl]);
        st->pst[snpl]=malloc(mem);
        if(st->pst[snpl]==NULL) return 1;
        st->mal[snpl]=mem;
    }
    st->stcnt[snpl]=0;
    hcur=127;
    while(hcur>=1)
    {
        r=0;
        while(r==0)
        {
            r=heapcomp(&si,2*hcur,2*hcur+1);
            if(r!=0) break;
            pqadvance(&si,2*hcur);
            adjtop(&si,2*hcur);     /* bugfix added  */
        }
        if(r==-1) si.ch[hcur]=0;
          else    si.ch[hcur]=1;
        adjtop(&si,hcur);  
        hcur--;
    }

    wout=st->pst[snpl];
    while(si.pq[1]!=EXPIRED)
    {
        ssc=si.pq[1];
        w1=st->ptp[ssc];
        while(((*w1)&3)!=0) *(wout++) =*(w1++);
        *(wout++) =*(w1++);
        st->ptp[ssc]=w1;
        st->stcnt[ssc]--;
        if(st->stcnt[ssc]==0)
            si.pq[1]=EXPIRED;
        adjtop(&si,1);
        st->stcnt[snpl]++;
    }
    st->ptp[snpl]=wout;
    st->inuse[layer]=0;
    st->inuse[layer+1]++;
    if(st->inuse[layer+1] == 255) return merge(st,layer+1);
    return 0;
}

int ZStrSTAppend(STEX * st, ZSTR * z)
{
    size_t len;
    int sno;
    len=ZStrMaxLen(z,2)*sizeof(uint16_t);
    sno=st->inuse[0];
    if(len>st->mal[sno])
    {
        if(st->mal[sno]!=0) free(st->pst[sno]);
        st->pst[sno]=malloc(len);
        if(st->pst[sno]==NULL) return 1;
        st->mal[sno]=len;
    }
    len=ZStrExtract(z,st->pst[sno],2);
    st->ptp[sno]=st->pst[sno]+len;
    st->stcnt[sno]=1;
    st->inuse[0]++;
    if(st->inuse[0]>=255) return merge(st,0);
    return 0;
}

int ZStrSTSort(STEX * st)
{
    int lev,lev2,mxlev,r;
    uint16_t sans;
    lev=0;
    mxlev=0;
    while(lev<6)
    {
/* check to find maximum level */
        for(lev2=0;lev2<6;lev2++)
            if(st->inuse[lev2]!=0) mxlev=lev2;
        if( (lev==mxlev) && (st->inuse[lev]==1) ) break;
        r=merge(st,lev);
        if(r!=0) return r;
        lev++;
        continue;
    }
    if(st->listm!=0) free (st->list);
    if(st->inuse[lev]==0)   /* nothing there at all!  */
    {
        st->listw=0;
        st->listm=0;
        return 0;
    }
    sans=256*lev;
    st->list=st->pst[sans];
    st->listw=st->ptp[sans]-st->pst[sans];
    st->listm=st->mal[sans];
    st->cnt=st->stcnt[sans];
    st->mal[sans]=0;
    return 0;
}

TUBER * ZStrTuberCons(size_t size, int options)
{
    TUBER * t;
    int i;
    t=malloc(sizeof(TUBER));
    if(t==NULL) return NULL;
/* compute number of K-keys per word from options */
    i=options&7;
    t->kperw=0;
    if(i==1) t->kperw=8;
    if(i==2) t->kperw=4;
    if(i==3) t->kperw=2;
    if(i==4) t->kperw=1;
    if(t->kperw == 0)
    {
        printf("Invalid options field in ZStrTuberCons\n");
        exit(35);
    }
/* compute maximum K-key from suggested size  */
    t->kmax=(size*t->kperw)/8;
    t->kmax++;
    if( (t->kmax%2) == 0) t->kmax++;
    while(1)
    {
        t->kmax+=2;
        for(i=3;i<47;i++)
             if( (t->kmax%i)==0) break;
        if(i==47) break; 
    }
    t->wct = (t->kmax+t->kperw-1)/t->kperw;
    t->tiptop=t->wct*t->kperw;
    t->tub = malloc(8*t->wct);
    if(t->tub == NULL)
    {
        free(t);
        return NULL;
    }
    for(i=0;i<t->wct;i++) t->tub[i]=0x8000000000000000ll;
    t->lenlen=3;
    t->mult=8;
    if(t->kperw==2)
    {
        t->lenlen=4;
        t->mult=16;
    }
    if(t->kperw==1)
    {
        t->lenlen=5;
        t->mult=32;
    }
    t->freekey=t->kmax;
    t->freebit=(t->wct*63)-(t->kmax*(t->lenlen+1));
    t->fuses=0;
    return t;
}

void ZStrTuberDest(TUBER * t)
{
    free(t->tub);
    free(t);
}

typedef struct
{
    TUBER * tub;
    uint64_t curw;  /* up on tub->tub */
    long curb;      /* 0-62 */
    long hdrlen;
} CuR;

static void copycur(CuR * c1, CuR * c2)
{
    c2->tub=c1->tub;
    c2->curw=c1->curw;
    c2->curb=c1->curb;
}

static uint64_t getbits(CuR * cur, long bits)
{
    uint64_t got,got1;
    uint64_t one;
    long newbits;
    uint64_t x;
    TUBER * tub;

    one=1;
    tub=cur->tub;
    if(bits+cur->curb < 63)
    {
        got=tub->tub[cur->curw];
        cur->curb+=bits;
        got>>=(63-cur->curb);
    }
    else
    {
        got=tub->tub[cur->curw];
        newbits=bits+cur->curb-63;
        cur->curb=newbits;
        cur->curw++;
        if(cur->curw>=tub->wct) cur->curw=0;
        got1=tub->tub[cur->curw];
        got1<<=1;
/* bugfix shift of 64 not happening */
        if(newbits!=0)
            got=(got<<newbits)+(got1>>(64-newbits));
    }   
    x = got&((one<<bits)-one);
    return x;
}

static void skipbits(CuR * cur, long bits)
{
    TUBER * t;
    if(bits<=0) return;
    t=cur->tub;
    cur->curw+=(bits/63);
    cur->curb+=(bits%63);
    if(cur->curb>62)
    {
        cur->curw++;
        cur->curb-=63;
    }
    while(cur->curw>=t->wct) cur->curw-=t->wct;
}

static void putbits(CuR * cur, uint64_t data, long bits)
{
    TUBER * tub;
    uint64_t x1,x2;
    uint64_t one;
    long newbits;
    tub=cur->tub;
    one=1;
    x2=(one<<(63-cur->curb))-one;  /* mask for ~old bits  */
    if(bits+cur->curb < 63)
    {
        x1=(one<<(63-cur->curb-bits))-one;
        x2=x1^x2;   /* new bits mask */
        x1=~x2;     /* old bits mask */
        x1=x1&tub->tub[cur->curw];   /*old bits (inc. top one)  */
        tub->tub[cur->curw]=x1+((data<<(63-cur->curb-bits))&x2);
        cur->curb+=bits;
        return;
    }
    x1=~x2;
    x1=x1&tub->tub[cur->curw];   /* old bits  */
    newbits=cur->curb+bits-63;
    tub->tub[cur->curw]=x1+(data>>(newbits));
    cur->curw++;
    if(cur->curw>=tub->wct) cur->curw=0;
    cur->curb=newbits;
    x1=((one<<(63-newbits))-one)|0x8000000000000000;  /* keep these */
    x2=tub->tub[cur->curw]&x1;   
    tub->tub[cur->curw]=x2+((data<<(63-newbits))&(~x1));
    return;
}

static long gethdr(CuR * cur)
{
    TUBER * t;
    long dlen;
    uint64_t h;
    t=cur->tub;
    h=getbits(cur,t->lenlen+1);
    cur->hdrlen=t->lenlen+1;
    dlen=h;
    dlen-=2;
    if(h<3) return dlen;
    dlen=0;
    while( (h>>t->lenlen)!=0 )
    {
        h-=t->mult;
        h=(h<<1)+getbits(cur,1);
        cur->hdrlen++;
        dlen+=t->mult;
    }
    dlen+=h;
    dlen-=2;
    return dlen;
}

void ZStrTuberStats(TUBER * t, uint64_t * stats)
{
    uint64_t d1,d2;
    d1=(t->fuses*100)/t->wct;
    d2=(t->freebit*100)/( (t->wct*63)-(t->kmax*(t->lenlen+1)) );
    d2=100-d2;
    if(d2>d1) d1=d2;
    d2=(100*t->freekey)/t->kmax;
    d2=100-d2;
    if(d2>d1) d1=d2;
    d2=((t->wct*8)*(d1+1))/50;
printf("fuse %d freebit %d freekey %d kmax %d wct %d lenlen %d\n",
     (int)t->fuses, (int)t->freebit, (int)t->freekey, (int)t->kmax,
        (int)t->wct, (int)t->lenlen);

    if(d2<72*t->fuses)d2=72*t->fuses;
    stats[0]=d1;
    stats[1]=d2;
}

typedef struct
{
    TUBER * tub;
    uint64_t first;
    uint64_t last;
    uint64_t words;
}  BlK;

/* Set cur to point to the wanted string */
static void locate(TUBER * t, uint64_t kkey, BlK * blk, CuR * cur)
{
    uint64_t curkkey;
    long dlen;
/* fill in the BlK structure with first, last and number of words  */
    blk->last = blk->first = kkey/t->kperw;
    blk->words=1;
    blk->tub=t;
    if(blk->first>0) blk->first--;
    else blk->first=t->wct-1; 
    while( (t->tub[blk->first]>>63)==0)
    {
        if(blk->first>0) blk->first--;
        else blk->first=t->wct-1;  
        blk->words++;
    }
    blk->first++;
    if(blk->first >= t->wct) blk->first=0;
    while( (t->tub[blk->last]>>63)==0)
    {
        blk->last++;
        if(blk->last >= t->wct) blk->last=0;
        blk->words++;
    }
/* set the CuR structure to point to the required string  */
    cur->tub=t;
    cur->curw=blk->first;
    cur->curb=0;
    curkkey=blk->first*t->kperw;
    while(curkkey!=kkey)
    {
        dlen=gethdr(cur);
        if(dlen>0)skipbits(cur,dlen);
        curkkey++;
/* bugfixed in tuber - wraparound */
        if(curkkey==t->kperw*t->wct) curkkey=0;
    }
}

/* grabs specified number of kkeys from cur*/
/* returns number of free bits, or -1 if memory allocation occurs */ 
long grabrest(CuR * cur, BlK * blk, uint64_t kkeys, ZSTR * z)
{
    uint64_t i,b;
    long j,k,freeb;
    int r;
    TUBER * t;
    CuR cur1;

    t = blk->tub;
    for(i=0;i<kkeys;i++)
    {
        copycur(cur,&cur1);
        j=gethdr(&cur1);
        k=cur1.hdrlen;
        while(k>63)
        {
            b=getbits(cur,63);
            r=ZStrBitsIn(b,63,z);
            if(r!=0) return -1;
            k-=63;
        }
        b=getbits(cur,k);
        r=ZStrBitsIn(b,k,z);
        if(r!=0) return -1;
        while(j>63)
        {
            b=getbits(cur,63);
            r=ZStrBitsIn(b,63,z);
            if(r!=0) return -1;
            j-=63;
        }
        if(j>0)
        {
            b=getbits(cur,j);
            r=ZStrBitsIn(b,j,z);
            if(r!=0) return -1;
        };
    }
/* bugfix page turn  */
    if(cur->curb==0)
    {
        cur->curb=63;
        if(cur->curw!=0) cur->curw--;
              else       cur->curw = t->wct-1;
    }
/* end of bugfix page turn */
    freeb=63-cur->curb;
    while(cur->curw!=blk->last)
    {
        freeb+=63;
        cur->curw++;
        if(cur->curw>=t->wct) cur->curw=0;
    }
    return freeb;
}

static long blkfuse(BlK * blk, CuR * cur, ZSTR * z)
{
    TUBER * t;
    uint64_t kkeys;
    t=blk->tub;
    blk->last++;
    if(blk->last >= t->wct) blk->last=0;
    cur->curw=blk->last;
    cur->curb=0;
    blk->words++;
    kkeys=t->kperw;
    while( (t->tub[blk->last]>>63)==0)
    {
        blk->last++;
        if(blk->last >= t->wct) blk->last=0;
        blk->words++;
        kkeys+=t->kperw;
    }
    return grabrest(cur,blk,kkeys,z);
}

void movebits(ZSTR * z, long bits, CuR * cur)
{
    uint64_t j;
    long bt;
    bt=bits;
    while(bt>60)
    {
        j=ZStrBitsOut(z,60);
        bt-=60;
        putbits(cur,j,60);
    }
    j=ZStrBitsOut(z,bt);
    putbits(cur,j,bt);
}


int ZStrTuberRead(TUBER * t, uint64_t kkey, ZSTR * z)
{
    long i;
    int r;
    uint64_t j;
    BlK blk;
    CuR cur;
    locate(t,kkey,&blk,&cur);
    i=gethdr(&cur);
    if(i==-2) return 1;
    ZStrClear(z);
    if(i==-1) return 0;
    while(i>60)
    {
        j=getbits(&cur,60);
        r=ZStrBitsIn(j,60,z);
        if(r!=0) return 2;
        i-=60;
    }
    if(i>0)
    {
        j=getbits(&cur,i);
        r=ZStrBitsIn(j,i,z);
        if(r!=0) return 2;
    }
    r=ZStrBitsIn(1,1,z);
    if(r!=0) return 2;
    return 0;  
}

uint64_t ZStrTuberIns(TUBER * t, uint64_t d1, uint64_t d2)
{
    BlK blk;
    CuR cur,cur1;
    uint64_t kkey,keyb;
    int i;
/* first find a keyb that works  */
    for(keyb=0;keyb<65536;keyb++)
    {
        kkey=ZStrTuberK(t,d1,d2,keyb);
        locate(t,kkey,&blk, &cur);
        copycur(&cur,&cur1);
        i=gethdr(&cur);
        if(i==-2) break;
    }
    if(keyb==65536) return INSFAIL;
/* equal size so change from key-not-found to zero */
    putbits(&cur1,1,(cur.tub)->lenlen+1);
    t->freekey--;
    return keyb;
}

int ZStrTuberUpdate(TUBER * t, uint64_t kkey, ZSTR * z)
{
    BlK blk;
    CuR cur;
    CuR cur1;
    ZSTR * z1;
    long i1,i2,i3,j,k,b1,sparebits,bitlen,spb1;
    int i;
    uint64_t kkeys;
    uint64_t w,m1,m2;
    int fuseflag;
    locate(t,kkey,&blk, &cur);
    copycur(&cur,&cur1);
    i1=gethdr(&cur1);
    if(i1<0)
        i1=0;
    i3=i1;
    i1+=cur1.hdrlen;   /* current total length in tuber */
    j=ZStrLen(z);
    k=j+1;
    b1=0;
    while(k>=(t->mult))
    {
        b1++;
        k-=t->mult;
    }
/* so b1 is the number of 1-bits in the header             */
/* and k is the value of the remainder of the header bits  */
/* and j is the length of the z-string part (inc. last 1)  */
    i2=b1+j+t->lenlen;
    if(j==0) i2++;
/* so now i2 is the new length  */
    if(i2==i1)     /* same length case  */
    {
        for(i=0;i<b1;i++) putbits(&cur,1,1);
        putbits(&cur,0,1);
        putbits(&cur,k,t->lenlen);
        if(j>1)movebits(z,j-1,&cur);
        return 0;
    }
    t->freebit-=i2;
    t->freebit+=i1;
    skipbits(&cur1,i3);
    kkeys=((blk.last+1)*t->kperw)-1;
    if(kkeys>=kkey) kkeys=kkeys-kkey;
          else      kkeys=t->tiptop+kkeys-kkey;
    z1=ZStrCons(kkeys/t->wct+7);  /* first shot  */
    if(z1==NULL) return 1;
    sparebits=grabrest(&cur1,&blk,kkeys,z1);
    if(sparebits==-1) return 1;
    fuseflag=0;
    while(sparebits+i1<i2)
    {
        t->fuses++;
        spb1=blkfuse(&blk,&cur1,z1);
        if(spb1==-1) return 2;
        sparebits += spb1;
        fuseflag=1;
        if(blk.words > (t->wct/3)) return 2;
    }
    sparebits=sparebits+i1-i2;

    if(fuseflag==1)
    {
        m1=0x7fffffffffffffffull;
        m2=0x8000000000000000ull;
        w=blk.first;
        while(w!=blk.last)
        {
            t->tub[w]&=m1;
            w++;
            if(w>=t->wct) w=0;
        }
        t->tub[w]|=m2;
    }
    bitlen=ZStrLen(z); 
    for(i=0;i<b1;i++) putbits(&cur,1,1);
    putbits(&cur,0,1);
    putbits(&cur,k,t->lenlen);
    if(j>1) movebits(z,j-1,&cur);
    bitlen=ZStrLen(z1);
    movebits(z1,bitlen,&cur);
    ZStrClear(z1);
    movebits(z1,sparebits,&cur);
    ZStrDest(z1);
    return 0;
}

int ZStrTuberDelete(TUBER * t, uint64_t kkey)
{
    BlK blk;
    CuR cur;
    CuR cur1;
    ZSTR * z;
    int r;
    long i1,bitlen;
    uint64_t kkeys;

    locate(t,kkey,&blk, &cur);
    copycur(&cur,&cur1);
    i1=gethdr(&cur1);
    t->freebit+=cur1.hdrlen;

    skipbits(&cur1,i1);
    kkeys=((blk.last+1)*t->kperw)-1;
    if(kkeys>=kkey) kkeys=kkeys-kkey;
          else      kkeys=t->tiptop+kkeys-kkey;
    z=ZStrCons(kkeys/t->wct+7);  /* about right  */
    if(z==NULL) return 1;
    r=grabrest(&cur1,&blk,kkeys,z);
    if(r!=0) return 1;
    bitlen=ZStrLen(z);  /* probably should compute in grabrest */
    putbits(&cur,0,t->lenlen+1);  /* put in key-not-present */
    movebits(z,bitlen,&cur);
    t->freekey++;
    t->freebit+=i1;

    t->freebit-=t->lenlen;
    return 0;
}

/* end of zstr.c */
