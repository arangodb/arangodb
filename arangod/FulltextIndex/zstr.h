/* zstr.h - header file for the z-string module  */
/*   R. A. Parker    3.5.2012  */

typedef struct
{
    uint64_t * dat;
    long * dlen;
    int alloc;
    int firstix;
    int lastix;
}   ZSTR;

ZSTR * ZStrCons(int elts);
void ZStrDest(ZSTR * z);
void ZStrClear(ZSTR * z);
int ZStrBitsIn(uint64_t a, long bits, ZSTR * z);
uint64_t ZStrBitsOut(ZSTR * z, long bits);
uint64_t ZStrBitsPeek(ZSTR * z, long bits);
long ZStrLen(ZSTR * z);
void ZStrNormalize(ZSTR * z); 

typedef struct
{
    int t;          /* code type            */
    int s;          /* segments             */
    int tmax;       /* Top to translate     */
    int bits;       /* that determine len   */
    uint64_t * X;   /* first of segment     */
    uint64_t * C;   /* code added           */
    uint8_t * L;    /* length in bits       */
    uint8_t * SG;   /* segment for top bits */
    uint8_t * TX;   /* translate table      */
    uint8_t * UX;   /* untranslate table    */
}   ZCOD;

int ZStrEnc(ZSTR * z, ZCOD * zc, uint64_t a);
uint64_t ZStrDec(ZSTR * z, ZCOD * zc);
uint64_t ZStrXlate(ZCOD * zc, uint64_t a);
uint64_t ZStrUnXl(ZCOD * zc, uint64_t a);
int ZStrLastEnc(ZSTR * z, uint64_t a);
uint64_t ZStrLastDec(ZSTR * z);

typedef struct
{
    uint64_t x1;
}   CTX;

void ZStrCxClear(ZCOD * zc, CTX * ctx);
int ZStrCxEnc(ZSTR * z, ZCOD * zc, CTX * ctx, uint64_t a);
uint64_t ZStrCxDec(ZSTR * z, ZCOD * zc, CTX * ctx);


int ZStrMaxLen(ZSTR * z, int fmt);
int ZStrExtract(ZSTR * z, void * x, int fmt);
int ZStrInsert(ZSTR * z, void * x, int fmt);
int ZStrExtCompare(void * x, void * y, int fmt);
int ZStrExtLen(void * x, int fmt);

typedef struct
{
    uint16_t ** pst;    /* 1281 pointers to start          */
    uint16_t ** ptp;    /* 1281 pointers to top            */
    uint64_t * mal;     /* 1281 number of bytes allocated  */
    uint64_t * stcnt;   /* 1281 number of strings in clump */
    uint16_t inuse[6];
    uint16_t * list;    /* final list                      */
    uint64_t listw;     /* number of uint16s in final list */
    uint64_t listm;     /* number of uint16's malloc'd     */
    uint64_t cnt;       /* number if strings in list       */
}   STEX;

STEX * ZStrSTCons(int fmt);
void ZStrSTDest(STEX * st);
int ZStrSTAppend(STEX * st, ZSTR * z);
int ZStrSTSort(STEX * st);
void * ZStrSTFind(STEX * st, void * x);

typedef struct
{
    uint64_t kperw;  /* K keys per word */
    uint64_t kmax;   /* (prime) number of keys */
    uint64_t tiptop; /* number of spaces in tuber  */
    uint64_t wct;    /* number of 64-bit words */
    long lenlen;     /* length of length string */
    uint64_t mult;   /* length bits per initial 1-bit */
    uint64_t * tub;  /* tuber data pointer */
    uint64_t freekey;   /* free keys  */
    uint64_t freebit;   /* free bits  */
    uint64_t fuses;     /* number of block fuses  */
}   TUBER;

#define TUBER_BITS_8  1
#define TUBER_BITS_16 2
#define TUBER_BITS_32 3
#define TUBER_BITS_64 4

TUBER * ZStrTuberCons(size_t size, int options);
void ZStrTuberDest(TUBER * t);
void ZStrTuberStats(TUBER * t, uint64_t * stats);
int ZStrTuberRead(TUBER * t, uint64_t kkey, ZSTR * z);
int ZStrTuberUpdate(TUBER * t, uint64_t kkey, ZSTR * z);
int ZStrTuberDelete(TUBER * t, uint64_t kkey);
#define INSFAIL 128000
uint64_t ZStrTuberIns(TUBER * t, uint64_t d1, uint64_t d2);
uint64_t ZStrTuberK(TUBER * t,  uint64_t d1,
                                uint64_t d2, uint64_t keyb);

/* end of zstr.h */


