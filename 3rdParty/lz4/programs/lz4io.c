/*
  LZ4io.c - LZ4 File/Stream Interface
  Copyright (C) Yann Collet 2011-2017

  GPL v2 License

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

  You can contact the author at :
  - LZ4 source repository : https://github.com/lz4/lz4
  - LZ4 public forum : https://groups.google.com/forum/#!forum/lz4c
*/
/*
  Note : this is stand-alone program.
  It is not part of LZ4 compression library, it is a user code of the LZ4 library.
  - The license of LZ4 library is BSD.
  - The license of xxHash library is BSD.
  - The license of this source file is GPLv2.
*/


/*-************************************
*  Compiler options
**************************************/
#ifdef _MSC_VER    /* Visual Studio */
#  pragma warning(disable : 4127)    /* disable: C4127: conditional expression is constant */
#endif
#if defined(__MINGW32__) && !defined(_POSIX_SOURCE)
#  define _POSIX_SOURCE 1          /* disable %llu warnings with MinGW on Windows */
#endif


/*****************************
*  Includes
*****************************/
#include "platform.h"  /* Large File Support, SET_BINARY_MODE, SET_SPARSE_FILE_MODE, PLATFORM_POSIX_VERSION, __64BIT__ */
#include "util.h"      /* UTIL_getFileStat, UTIL_setFileStat */
#include <stdio.h>     /* fprintf, fopen, fread, stdin, stdout, fflush, getchar */
#include <stdlib.h>    /* malloc, free */
#include <string.h>    /* strerror, strcmp, strlen */
#include <time.h>      /* clock */
#include <sys/types.h> /* stat64 */
#include <sys/stat.h>  /* stat64 */
#include "lz4.h"       /* still required for legacy format */
#include "lz4hc.h"     /* still required for legacy format */
#define LZ4F_STATIC_LINKING_ONLY
#include "lz4frame.h"
#include "lz4io.h"


/*****************************
*  Constants
*****************************/
#define KB *(1 <<10)
#define MB *(1 <<20)
#define GB *(1U<<30)

#define _1BIT  0x01
#define _2BITS 0x03
#define _3BITS 0x07
#define _4BITS 0x0F
#define _8BITS 0xFF

#define MAGICNUMBER_SIZE    4
#define LZ4IO_MAGICNUMBER   0x184D2204
#define LZ4IO_SKIPPABLE0    0x184D2A50
#define LZ4IO_SKIPPABLEMASK 0xFFFFFFF0
#define LEGACY_MAGICNUMBER  0x184C2102

#define CACHELINE 64
#define LEGACY_BLOCKSIZE   (8 MB)
#define MIN_STREAM_BUFSIZE (192 KB)
#define LZ4IO_BLOCKSIZEID_DEFAULT 7
#define LZ4_MAX_DICT_SIZE (64 KB)


/**************************************
*  Macros
**************************************/
#define DISPLAY(...)         fprintf(stderr, __VA_ARGS__)
#define DISPLAYOUT(...)      fprintf(stdout, __VA_ARGS__)
#define DISPLAYLEVEL(l, ...) if (g_displayLevel>=l) { DISPLAY(__VA_ARGS__); }
static int g_displayLevel = 0;   /* 0 : no display  ; 1: errors  ; 2 : + result + interaction + warnings ; 3 : + progression; 4 : + information */

#define DISPLAYUPDATE(l, ...) if (g_displayLevel>=l) { \
            if ( ((clock() - g_time) > refreshRate)    \
              || (g_displayLevel>=4) ) {               \
                g_time = clock();                      \
                DISPLAY(__VA_ARGS__);                  \
                if (g_displayLevel>=4) fflush(stderr); \
        }   }
static const clock_t refreshRate = CLOCKS_PER_SEC / 6;
static clock_t g_time = 0;
#define LZ4IO_STATIC_ASSERT(c)   { enum { LZ4IO_static_assert = 1/(int)(!!(c)) }; }   /* use after variable declarations */


/**************************************
*  Local Parameters
**************************************/

struct LZ4IO_prefs_s {
    int passThrough;
    int overwrite;
    int testMode;
    int blockSizeId;
    size_t blockSize;
    int blockChecksum;
    int streamChecksum;
    int blockIndependence;
    int sparseFileSupport;
    int contentSizeFlag;
    int useDictionary;
    unsigned favorDecSpeed;
    const char* dictionaryFilename;
    int removeSrcFile;
};

/**************************************
*  Exceptions
***************************************/
#ifndef DEBUG
#  define DEBUG 0
#endif
#define DEBUGOUTPUT(...) if (DEBUG) DISPLAY(__VA_ARGS__);
#define EXM_THROW(error, ...)                                             \
{                                                                         \
    DEBUGOUTPUT("Error defined at %s, line %i : \n", __FILE__, __LINE__); \
    DISPLAYLEVEL(1, "Error %i : ", error);                                \
    DISPLAYLEVEL(1, __VA_ARGS__);                                         \
    DISPLAYLEVEL(1, " \n");                                               \
    exit(error);                                                          \
}


/**************************************
*  Version modifiers
**************************************/
#define EXTENDED_ARGUMENTS
#define EXTENDED_HELP
#define EXTENDED_FORMAT
#define DEFAULT_DECOMPRESSOR LZ4IO_decompressLZ4F


/* ************************************************** */
/* ****************** Parameters ******************** */
/* ************************************************** */

LZ4IO_prefs_t* LZ4IO_defaultPreferences(void)
{
    LZ4IO_prefs_t* const ret = (LZ4IO_prefs_t*)malloc(sizeof(*ret));
    if (!ret) EXM_THROW(21, "Allocation error : not enough memory");
    ret->passThrough = 0;
    ret->overwrite = 1;
    ret->testMode = 0;
    ret->blockSizeId = LZ4IO_BLOCKSIZEID_DEFAULT;
    ret->blockSize = 0;
    ret->blockChecksum = 0;
    ret->streamChecksum = 1;
    ret->blockIndependence = 1;
    ret->sparseFileSupport = 1;
    ret->contentSizeFlag = 0;
    ret->useDictionary = 0;
    ret->favorDecSpeed = 0;
    ret->dictionaryFilename = NULL;
    ret->removeSrcFile = 0;
    return ret;
}

void LZ4IO_freePreferences(LZ4IO_prefs_t* prefs)
{
    free(prefs);
}


int LZ4IO_setDictionaryFilename(LZ4IO_prefs_t* const prefs, const char* dictionaryFilename)
{
    prefs->dictionaryFilename = dictionaryFilename;
    prefs->useDictionary = dictionaryFilename != NULL;
    return prefs->useDictionary;
}

/* Default setting : passThrough = 0; return : passThrough mode (0/1) */
int LZ4IO_setPassThrough(LZ4IO_prefs_t* const prefs, int yes)
{
   prefs->passThrough = (yes!=0);
   return prefs->passThrough;
}


/* Default setting : overwrite = 1; return : overwrite mode (0/1) */
int LZ4IO_setOverwrite(LZ4IO_prefs_t* const prefs, int yes)
{
   prefs->overwrite = (yes!=0);
   return prefs->overwrite;
}

/* Default setting : testMode = 0; return : testMode (0/1) */
int LZ4IO_setTestMode(LZ4IO_prefs_t* const prefs, int yes)
{
   prefs->testMode = (yes!=0);
   return prefs->testMode;
}

/* blockSizeID : valid values : 4-5-6-7 */
size_t LZ4IO_setBlockSizeID(LZ4IO_prefs_t* const prefs, unsigned bsid)
{
    static const size_t blockSizeTable[] = { 64 KB, 256 KB, 1 MB, 4 MB };
    static const unsigned minBlockSizeID = 4;
    static const unsigned maxBlockSizeID = 7;
    if ((bsid < minBlockSizeID) || (bsid > maxBlockSizeID)) return 0;
    prefs->blockSizeId = (int)bsid;
    prefs->blockSize = blockSizeTable[(unsigned)prefs->blockSizeId-minBlockSizeID];
    return prefs->blockSize;
}

size_t LZ4IO_setBlockSize(LZ4IO_prefs_t* const prefs, size_t blockSize)
{
    static const size_t minBlockSize = 32;
    static const size_t maxBlockSize = 4 MB;
    unsigned bsid = 0;
    if (blockSize < minBlockSize) blockSize = minBlockSize;
    if (blockSize > maxBlockSize) blockSize = maxBlockSize;
    prefs->blockSize = blockSize;
    blockSize--;
    /* find which of { 64k, 256k, 1MB, 4MB } is closest to blockSize */
    while (blockSize >>= 2)
        bsid++;
    if (bsid < 7) bsid = 7;
    prefs->blockSizeId = (int)(bsid-3);
    return prefs->blockSize;
}

/* Default setting : 1 == independent blocks */
int LZ4IO_setBlockMode(LZ4IO_prefs_t* const prefs, LZ4IO_blockMode_t blockMode)
{
    prefs->blockIndependence = (blockMode == LZ4IO_blockIndependent);
    return prefs->blockIndependence;
}

/* Default setting : 0 == no block checksum */
int LZ4IO_setBlockChecksumMode(LZ4IO_prefs_t* const prefs, int enable)
{
    prefs->blockChecksum = (enable != 0);
    return prefs->blockChecksum;
}

/* Default setting : 1 == checksum enabled */
int LZ4IO_setStreamChecksumMode(LZ4IO_prefs_t* const prefs, int enable)
{
    prefs->streamChecksum = (enable != 0);
    return prefs->streamChecksum;
}

/* Default setting : 0 (no notification) */
int LZ4IO_setNotificationLevel(int level)
{
    g_displayLevel = level;
    return g_displayLevel;
}

/* Default setting : 1 (auto: enabled on file, disabled on stdout) */
int LZ4IO_setSparseFile(LZ4IO_prefs_t* const prefs, int enable)
{
    prefs->sparseFileSupport = 2*(enable!=0);  /* 2==force enable */
    return prefs->sparseFileSupport;
}

/* Default setting : 0 (disabled) */
int LZ4IO_setContentSize(LZ4IO_prefs_t* const prefs, int enable)
{
    prefs->contentSizeFlag = (enable!=0);
    return prefs->contentSizeFlag;
}

/* Default setting : 0 (disabled) */
void LZ4IO_favorDecSpeed(LZ4IO_prefs_t* const prefs, int favor)
{
    prefs->favorDecSpeed = (favor!=0);
}

void LZ4IO_setRemoveSrcFile(LZ4IO_prefs_t* const prefs, unsigned flag)
{
  prefs->removeSrcFile = (flag>0);
}



/* ************************************************************************ **
** ********************** LZ4 File / Pipe compression ********************* **
** ************************************************************************ */

static int LZ4IO_isSkippableMagicNumber(unsigned int magic) {
    return (magic & LZ4IO_SKIPPABLEMASK) == LZ4IO_SKIPPABLE0;
}


/** LZ4IO_openSrcFile() :
 * condition : `srcFileName` must be non-NULL.
 * @result : FILE* to `dstFileName`, or NULL if it fails */
static FILE* LZ4IO_openSrcFile(const char* srcFileName)
{
    FILE* f;

    if (!strcmp (srcFileName, stdinmark)) {
        DISPLAYLEVEL(4,"Using stdin for input\n");
        f = stdin;
        SET_BINARY_MODE(stdin);
    } else {
        f = fopen(srcFileName, "rb");
        if ( f==NULL ) DISPLAYLEVEL(1, "%s: %s \n", srcFileName, strerror(errno));
    }

    return f;
}

/** FIO_openDstFile() :
 *  prefs is writable, because sparseFileSupport might be updated.
 *  condition : `dstFileName` must be non-NULL.
 * @result : FILE* to `dstFileName`, or NULL if it fails */
static FILE* LZ4IO_openDstFile(const char* dstFileName, const LZ4IO_prefs_t* const prefs)
{
    FILE* f;
    assert(dstFileName != NULL);

    if (!strcmp (dstFileName, stdoutmark)) {
        DISPLAYLEVEL(4, "Using stdout for output \n");
        f = stdout;
        SET_BINARY_MODE(stdout);
        if (prefs->sparseFileSupport==1) {
            DISPLAYLEVEL(4, "Sparse File Support automatically disabled on stdout ;"
                            " to force-enable it, add --sparse command \n");
        }
    } else {
        if (!prefs->overwrite && strcmp (dstFileName, nulmark)) {  /* Check if destination file already exists */
            FILE* const testf = fopen( dstFileName, "rb" );
            if (testf != NULL) {  /* dest exists, prompt for overwrite authorization */
                fclose(testf);
                if (g_displayLevel <= 1) {  /* No interaction possible */
                    DISPLAY("%s already exists; not overwritten  \n", dstFileName);
                    return NULL;
                }
                DISPLAY("%s already exists; do you wish to overwrite (y/N) ? ", dstFileName);
                {   int ch = getchar();
                    if ((ch!='Y') && (ch!='y')) {
                        DISPLAY("    not overwritten  \n");
                        return NULL;
                    }
                    while ((ch!=EOF) && (ch!='\n')) ch = getchar();  /* flush rest of input line */
        }   }   }
        f = fopen( dstFileName, "wb" );
        if (f==NULL) DISPLAYLEVEL(1, "%s: %s\n", dstFileName, strerror(errno));
    }

    /* sparse file */
    {   int const sparseMode = (prefs->sparseFileSupport - (f==stdout)) > 0;
        if (f && sparseMode) { SET_SPARSE_FILE_MODE(f); }
    }

    return f;
}



/***************************************
*   Legacy Compression
***************************************/

/* unoptimized version; solves endianess & alignment issues */
static void LZ4IO_writeLE32 (void* p, unsigned value32)
{
    unsigned char* const dstPtr = (unsigned char*)p;
    dstPtr[0] = (unsigned char)value32;
    dstPtr[1] = (unsigned char)(value32 >> 8);
    dstPtr[2] = (unsigned char)(value32 >> 16);
    dstPtr[3] = (unsigned char)(value32 >> 24);
}

static int LZ4IO_LZ4_compress(const char* src, char* dst, int srcSize, int dstSize, int cLevel)
{
    (void)cLevel;
    return LZ4_compress_fast(src, dst, srcSize, dstSize, 1);
}

/* LZ4IO_compressFilename_Legacy :
 * This function is intentionally "hidden" (not published in .h)
 * It generates compressed streams using the old 'legacy' format */
int LZ4IO_compressFilename_Legacy(const char* input_filename, const char* output_filename,
                                  int compressionlevel, const LZ4IO_prefs_t* prefs)
{
    typedef int (*compress_f)(const char* src, char* dst, int srcSize, int dstSize, int cLevel);
    compress_f const compressionFunction = (compressionlevel < 3) ? LZ4IO_LZ4_compress : LZ4_compress_HC;
    unsigned long long filesize = 0;
    unsigned long long compressedfilesize = MAGICNUMBER_SIZE;
    char* in_buff;
    char* out_buff;
    const int outBuffSize = LZ4_compressBound(LEGACY_BLOCKSIZE);
    FILE* const finput = LZ4IO_openSrcFile(input_filename);
    FILE* foutput;
    clock_t clockEnd;

    /* Init */
    clock_t const clockStart = clock();
    if (finput == NULL)
        EXM_THROW(20, "%s : open file error ", input_filename);

    foutput = LZ4IO_openDstFile(output_filename, prefs);
    if (foutput == NULL) {
        fclose(finput);
        EXM_THROW(20, "%s : open file error ", input_filename);
    }

    /* Allocate Memory */
    in_buff = (char*)malloc(LEGACY_BLOCKSIZE);
    out_buff = (char*)malloc((size_t)outBuffSize + 4);
    if (!in_buff || !out_buff)
        EXM_THROW(21, "Allocation error : not enough memory");

    /* Write Archive Header */
    LZ4IO_writeLE32(out_buff, LEGACY_MAGICNUMBER);
    if (fwrite(out_buff, 1, MAGICNUMBER_SIZE, foutput) != MAGICNUMBER_SIZE)
        EXM_THROW(22, "Write error : cannot write header");

    /* Main Loop */
    while (1) {
        int outSize;
        /* Read Block */
        size_t const inSize = fread(in_buff, (size_t)1, (size_t)LEGACY_BLOCKSIZE, finput);
        if (inSize == 0) break;
        assert(inSize <= LEGACY_BLOCKSIZE);
        filesize += inSize;

        /* Compress Block */
        outSize = compressionFunction(in_buff, out_buff+4, (int)inSize, outBuffSize, compressionlevel);
        assert(outSize >= 0);
        compressedfilesize += (unsigned long long)outSize+4;
        DISPLAYUPDATE(2, "\rRead : %i MB  ==> %.2f%%   ",
                (int)(filesize>>20), (double)compressedfilesize/filesize*100);

        /* Write Block */
        assert(outSize > 0);
        assert(outSize < outBuffSize);
        LZ4IO_writeLE32(out_buff, (unsigned)outSize);
        if (fwrite(out_buff, 1, (size_t)outSize+4, foutput) != (size_t)(outSize+4)) {
            EXM_THROW(24, "Write error : cannot write compressed block");
    }   }
    if (ferror(finput)) EXM_THROW(25, "Error while reading %s ", input_filename);

    /* Status */
    clockEnd = clock();
    if (clockEnd==clockStart) clockEnd+=1;  /* avoid division by zero (speed) */
    filesize += !filesize;   /* avoid division by zero (ratio) */
    DISPLAYLEVEL(2, "\r%79s\r", "");   /* blank line */
    DISPLAYLEVEL(2,"Compressed %llu bytes into %llu bytes ==> %.2f%%\n",
        filesize, compressedfilesize, (double)compressedfilesize / filesize * 100);
    {   double const seconds = (double)(clockEnd - clockStart) / CLOCKS_PER_SEC;
        DISPLAYLEVEL(4,"Done in %.2f s ==> %.2f MB/s\n", seconds,
                        (double)filesize / seconds / 1024 / 1024);
    }

    /* Close & Free */
    free(in_buff);
    free(out_buff);
    fclose(finput);
    if (strcmp(output_filename,stdoutmark)) fclose(foutput);   /* do not close stdout */

    return 0;
}

#define FNSPACE 30
/* LZ4IO_compressMultipleFilenames_Legacy :
 * This function is intentionally "hidden" (not published in .h)
 * It generates multiple compressed streams using the old 'legacy' format */
int LZ4IO_compressMultipleFilenames_Legacy(
                            const char** inFileNamesTable, int ifntSize,
                            const char* suffix,
                            int compressionLevel, const LZ4IO_prefs_t* prefs)
{
    int i;
    int missed_files = 0;
    char* dstFileName = (char*)malloc(FNSPACE);
    size_t ofnSize = FNSPACE;
    const size_t suffixSize = strlen(suffix);

    if (dstFileName == NULL) return ifntSize;   /* not enough memory */

    /* loop on each file */
    for (i=0; i<ifntSize; i++) {
        size_t const ifnSize = strlen(inFileNamesTable[i]);
        if (!strcmp(suffix, stdoutmark)) {
            missed_files += LZ4IO_compressFilename_Legacy(
                                    inFileNamesTable[i], stdoutmark,
                                    compressionLevel, prefs);
            continue;
        }

        if (ofnSize <= ifnSize+suffixSize+1) {
            free(dstFileName);
            ofnSize = ifnSize + 20;
            dstFileName = (char*)malloc(ofnSize);
            if (dstFileName==NULL) {
                return ifntSize;
        }   }
        strcpy(dstFileName, inFileNamesTable[i]);
        strcat(dstFileName, suffix);

        missed_files += LZ4IO_compressFilename_Legacy(
                                inFileNamesTable[i], dstFileName,
                                compressionLevel, prefs);
    }

    /* Close & Free */
    free(dstFileName);

    return missed_files;
}


/*********************************************
*  Compression using Frame format
*********************************************/

typedef struct {
    void*  srcBuffer;
    size_t srcBufferSize;
    void*  dstBuffer;
    size_t dstBufferSize;
    LZ4F_compressionContext_t ctx;
    LZ4F_CDict* cdict;
} cRess_t;

static void* LZ4IO_createDict(size_t* dictSize, const char* const dictFilename)
{
    size_t readSize;
    size_t dictEnd = 0;
    size_t dictLen = 0;
    size_t dictStart;
    size_t circularBufSize = LZ4_MAX_DICT_SIZE;
    char*  circularBuf = (char*)malloc(circularBufSize);
    char*  dictBuf;
    FILE* dictFile;

    if (!circularBuf) EXM_THROW(25, "Allocation error : not enough memory for circular buffer");
    if (!dictFilename) EXM_THROW(25, "Dictionary error : no filename provided");

    dictFile = LZ4IO_openSrcFile(dictFilename);
    if (!dictFile) EXM_THROW(25, "Dictionary error : could not open dictionary file");

    /* opportunistically seek to the part of the file we care about. If this */
    /* fails it's not a problem since we'll just read everything anyways.    */
    if (strcmp(dictFilename, stdinmark)) {
        (void)UTIL_fseek(dictFile, -LZ4_MAX_DICT_SIZE, SEEK_END);
    }

    do {
        readSize = fread(circularBuf + dictEnd, 1, circularBufSize - dictEnd, dictFile);
        dictEnd = (dictEnd + readSize) % circularBufSize;
        dictLen += readSize;
    } while (readSize>0);

    if (dictLen > LZ4_MAX_DICT_SIZE) {
        dictLen = LZ4_MAX_DICT_SIZE;
    }

    *dictSize = dictLen;

    dictStart = (circularBufSize + dictEnd - dictLen) % circularBufSize;

    if (dictStart == 0) {
        /* We're in the simple case where the dict starts at the beginning of our circular buffer. */
        dictBuf = circularBuf;
        circularBuf = NULL;
    } else {
        /* Otherwise, we will alloc a new buffer and copy our dict into that. */
        dictBuf = (char *)malloc(dictLen ? dictLen : 1);
        if (!dictBuf) EXM_THROW(25, "Allocation error : not enough memory");

        memcpy(dictBuf, circularBuf + dictStart, circularBufSize - dictStart);
        memcpy(dictBuf + circularBufSize - dictStart, circularBuf, dictLen - (circularBufSize - dictStart));
    }

    fclose(dictFile);
    free(circularBuf);

    return dictBuf;
}

static LZ4F_CDict* LZ4IO_createCDict(const LZ4IO_prefs_t* const prefs)
{
    size_t dictionarySize;
    void* dictionaryBuffer;
    LZ4F_CDict* cdict;
    if (!prefs->useDictionary) return NULL;
    dictionaryBuffer = LZ4IO_createDict(&dictionarySize, prefs->dictionaryFilename);
    if (!dictionaryBuffer) EXM_THROW(25, "Dictionary error : could not create dictionary");
    cdict = LZ4F_createCDict(dictionaryBuffer, dictionarySize);
    free(dictionaryBuffer);
    return cdict;
}

static cRess_t LZ4IO_createCResources(const LZ4IO_prefs_t* const prefs)
{
    const size_t blockSize = prefs->blockSize;
    cRess_t ress;

    LZ4F_errorCode_t const errorCode = LZ4F_createCompressionContext(&(ress.ctx), LZ4F_VERSION);
    if (LZ4F_isError(errorCode)) EXM_THROW(30, "Allocation error : can't create LZ4F context : %s", LZ4F_getErrorName(errorCode));

    /* Allocate Memory */
    ress.srcBuffer = malloc(blockSize);
    ress.srcBufferSize = blockSize;
    ress.dstBufferSize = LZ4F_compressFrameBound(blockSize, NULL);   /* cover worst case */
    ress.dstBuffer = malloc(ress.dstBufferSize);
    if (!ress.srcBuffer || !ress.dstBuffer) EXM_THROW(31, "Allocation error : not enough memory");

    ress.cdict = LZ4IO_createCDict(prefs);

    return ress;
}

static void LZ4IO_freeCResources(cRess_t ress)
{
    free(ress.srcBuffer);
    free(ress.dstBuffer);

    LZ4F_freeCDict(ress.cdict);
    ress.cdict = NULL;

    { LZ4F_errorCode_t const errorCode = LZ4F_freeCompressionContext(ress.ctx);
      if (LZ4F_isError(errorCode)) EXM_THROW(38, "Error : can't free LZ4F context resource : %s", LZ4F_getErrorName(errorCode)); }
}

/*
 * LZ4IO_compressFilename_extRess()
 * result : 0 : compression completed correctly
 *          1 : missing or pb opening srcFileName
 */
static int
LZ4IO_compressFilename_extRess(cRess_t ress,
                               const char* srcFileName, const char* dstFileName,
                               int compressionLevel, const LZ4IO_prefs_t* const io_prefs)
{
    unsigned long long filesize = 0;
    unsigned long long compressedfilesize = 0;
    FILE* dstFile;
    void* const srcBuffer = ress.srcBuffer;
    void* const dstBuffer = ress.dstBuffer;
    const size_t dstBufferSize = ress.dstBufferSize;
    const size_t blockSize = io_prefs->blockSize;
    size_t readSize;
    LZ4F_compressionContext_t ctx = ress.ctx;   /* just a pointer */
    LZ4F_preferences_t prefs;

    /* Init */
    FILE* const srcFile = LZ4IO_openSrcFile(srcFileName);
    if (srcFile == NULL) return 1;
    dstFile = LZ4IO_openDstFile(dstFileName, io_prefs);
    if (dstFile == NULL) { fclose(srcFile); return 1; }
    memset(&prefs, 0, sizeof(prefs));

    /* Set compression parameters */
    prefs.autoFlush = 1;
    prefs.compressionLevel = compressionLevel;
    prefs.frameInfo.blockMode = (LZ4F_blockMode_t)io_prefs->blockIndependence;
    prefs.frameInfo.blockSizeID = (LZ4F_blockSizeID_t)io_prefs->blockSizeId;
    prefs.frameInfo.blockChecksumFlag = (LZ4F_blockChecksum_t)io_prefs->blockChecksum;
    prefs.frameInfo.contentChecksumFlag = (LZ4F_contentChecksum_t)io_prefs->streamChecksum;
    prefs.favorDecSpeed = io_prefs->favorDecSpeed;
    if (io_prefs->contentSizeFlag) {
      U64 const fileSize = UTIL_getOpenFileSize(srcFile);
      prefs.frameInfo.contentSize = fileSize;   /* == 0 if input == stdin */
      if (fileSize==0)
          DISPLAYLEVEL(3, "Warning : cannot determine input content size \n");
    }

    /* read first block */
    readSize  = fread(srcBuffer, (size_t)1, blockSize, srcFile);
    if (ferror(srcFile)) EXM_THROW(30, "Error reading %s ", srcFileName);
    filesize += readSize;

    /* single-block file */
    if (readSize < blockSize) {
        /* Compress in single pass */
        size_t const cSize = LZ4F_compressFrame_usingCDict(ctx, dstBuffer, dstBufferSize, srcBuffer, readSize, ress.cdict, &prefs);
        if (LZ4F_isError(cSize))
            EXM_THROW(31, "Compression failed : %s", LZ4F_getErrorName(cSize));
        compressedfilesize = cSize;
        DISPLAYUPDATE(2, "\rRead : %u MB   ==> %.2f%%   ",
                      (unsigned)(filesize>>20), (double)compressedfilesize/(filesize+!filesize)*100);   /* avoid division by zero */

        /* Write Block */
        if (fwrite(dstBuffer, 1, cSize, dstFile) != cSize) {
            EXM_THROW(32, "Write error : failed writing single-block compressed frame");
    }   }

    else

    /* multiple-blocks file */
    {
        /* Write Frame Header */
        size_t const headerSize = LZ4F_compressBegin_usingCDict(ctx, dstBuffer, dstBufferSize, ress.cdict, &prefs);
        if (LZ4F_isError(headerSize)) EXM_THROW(33, "File header generation failed : %s", LZ4F_getErrorName(headerSize));
        if (fwrite(dstBuffer, 1, headerSize, dstFile) != headerSize)
            EXM_THROW(34, "Write error : cannot write header");
        compressedfilesize += headerSize;

        /* Main Loop - one block at a time */
        while (readSize>0) {
            size_t const outSize = LZ4F_compressUpdate(ctx, dstBuffer, dstBufferSize, srcBuffer, readSize, NULL);
            if (LZ4F_isError(outSize))
                EXM_THROW(35, "Compression failed : %s", LZ4F_getErrorName(outSize));
            compressedfilesize += outSize;
            DISPLAYUPDATE(2, "\rRead : %u MB   ==> %.2f%%   ",
                        (unsigned)(filesize>>20), (double)compressedfilesize/filesize*100);

            /* Write Block */
            if (fwrite(dstBuffer, 1, outSize, dstFile) != outSize)
                EXM_THROW(36, "Write error : cannot write compressed block");

            /* Read next block */
            readSize  = fread(srcBuffer, (size_t)1, (size_t)blockSize, srcFile);
            filesize += readSize;
        }
        if (ferror(srcFile)) EXM_THROW(37, "Error reading %s ", srcFileName);

        /* End of Frame mark */
        {   size_t const endSize = LZ4F_compressEnd(ctx, dstBuffer, dstBufferSize, NULL);
            if (LZ4F_isError(endSize))
                EXM_THROW(38, "End of frame error : %s", LZ4F_getErrorName(endSize));
            if (fwrite(dstBuffer, 1, endSize, dstFile) != endSize)
                EXM_THROW(39, "Write error : cannot write end of frame");
            compressedfilesize += endSize;
    }   }

    /* Release file handlers */
    fclose (srcFile);
    if (strcmp(dstFileName,stdoutmark)) fclose (dstFile);  /* do not close stdout */

    /* Copy owner, file permissions and modification time */
    {   stat_t statbuf;
        if (strcmp (srcFileName, stdinmark)
         && strcmp (dstFileName, stdoutmark)
         && strcmp (dstFileName, nulmark)
         && UTIL_getFileStat(srcFileName, &statbuf)) {
            UTIL_setFileStat(dstFileName, &statbuf);
    }   }

    if (io_prefs->removeSrcFile) {  /* remove source file : --rm */
        if (remove(srcFileName))
            EXM_THROW(40, "Remove error : %s: %s", srcFileName, strerror(errno));
    }

    /* Final Status */
    DISPLAYLEVEL(2, "\r%79s\r", "");
    DISPLAYLEVEL(2, "Compressed %llu bytes into %llu bytes ==> %.2f%%\n",
                    filesize, compressedfilesize,
                    (double)compressedfilesize / (filesize + !filesize /* avoid division by zero */ ) * 100);

    return 0;
}


int LZ4IO_compressFilename(const char* srcFileName, const char* dstFileName, int compressionLevel, const LZ4IO_prefs_t* prefs)
{
    UTIL_time_t const timeStart = UTIL_getTime();
    clock_t const cpuStart = clock();
    cRess_t const ress = LZ4IO_createCResources(prefs);

    int const result = LZ4IO_compressFilename_extRess(ress, srcFileName, dstFileName, compressionLevel, prefs);

    /* Free resources */
    LZ4IO_freeCResources(ress);

    /* Final Status */
    {   clock_t const cpuEnd = clock();
        double const cpuLoad_s = (double)(cpuEnd - cpuStart) / CLOCKS_PER_SEC;
        U64 const timeLength_ns = UTIL_clockSpanNano(timeStart);
        double const timeLength_s = (double)timeLength_ns / 1000000000;
        DISPLAYLEVEL(4, "Completed in %.2f sec  (cpu load : %.0f%%)\n",
                        timeLength_s, (cpuLoad_s / timeLength_s) * 100);
    }

    return result;
}


int LZ4IO_compressMultipleFilenames(
                              const char** inFileNamesTable, int ifntSize,
                              const char* suffix,
                              int compressionLevel,
                              const LZ4IO_prefs_t* prefs)
{
    int i;
    int missed_files = 0;
    char* dstFileName = (char*)malloc(FNSPACE);
    size_t ofnSize = FNSPACE;
    const size_t suffixSize = strlen(suffix);
    cRess_t ress;

    if (dstFileName == NULL) return ifntSize;   /* not enough memory */
    ress = LZ4IO_createCResources(prefs);

    /* loop on each file */
    for (i=0; i<ifntSize; i++) {
        size_t const ifnSize = strlen(inFileNamesTable[i]);
        if (!strcmp(suffix, stdoutmark)) {
            missed_files += LZ4IO_compressFilename_extRess(ress,
                                    inFileNamesTable[i], stdoutmark,
                                    compressionLevel, prefs);
            continue;
        }
        if (ofnSize <= ifnSize+suffixSize+1) {
            free(dstFileName);
            ofnSize = ifnSize + 20;
            dstFileName = (char*)malloc(ofnSize);
            if (dstFileName==NULL) {
                LZ4IO_freeCResources(ress);
                return ifntSize;
        }   }
        strcpy(dstFileName, inFileNamesTable[i]);
        strcat(dstFileName, suffix);

        missed_files += LZ4IO_compressFilename_extRess(ress,
                                inFileNamesTable[i], dstFileName,
                                compressionLevel, prefs);
    }

    /* Close & Free */
    LZ4IO_freeCResources(ress);
    free(dstFileName);

    return missed_files;
}


/* ********************************************************************* */
/* ********************** LZ4 file-stream Decompression **************** */
/* ********************************************************************* */

/* It's presumed that s points to a memory space of size >= 4 */
static unsigned LZ4IO_readLE32 (const void* s)
{
    const unsigned char* const srcPtr = (const unsigned char*)s;
    unsigned value32 = srcPtr[0];
    value32 += (unsigned)srcPtr[1] <<  8;
    value32 += (unsigned)srcPtr[2] << 16;
    value32 += (unsigned)srcPtr[3] << 24;
    return value32;
}


static unsigned
LZ4IO_fwriteSparse(FILE* file,
                   const void* buffer, size_t bufferSize,
                   int sparseFileSupport,
                   unsigned storedSkips)
{
    const size_t sizeT = sizeof(size_t);
    const size_t maskT = sizeT -1 ;
    const size_t* const bufferT = (const size_t*)buffer;   /* Buffer is supposed malloc'ed, hence aligned on size_t */
    const size_t* ptrT = bufferT;
    size_t bufferSizeT = bufferSize / sizeT;
    const size_t* const bufferTEnd = bufferT + bufferSizeT;
    const size_t segmentSizeT = (32 KB) / sizeT;
    int const sparseMode = (sparseFileSupport - (file==stdout)) > 0;

    if (!sparseMode) {  /* normal write */
        size_t const sizeCheck = fwrite(buffer, 1, bufferSize, file);
        if (sizeCheck != bufferSize) EXM_THROW(70, "Write error : cannot write decoded block");
        return 0;
    }

    /* avoid int overflow */
    if (storedSkips > 1 GB) {
        int const seekResult = UTIL_fseek(file, 1 GB, SEEK_CUR);
        if (seekResult != 0) EXM_THROW(71, "1 GB skip error (sparse file support)");
        storedSkips -= 1 GB;
    }

    while (ptrT < bufferTEnd) {
        size_t seg0SizeT = segmentSizeT;
        size_t nb0T;

        /* count leading zeros */
        if (seg0SizeT > bufferSizeT) seg0SizeT = bufferSizeT;
        bufferSizeT -= seg0SizeT;
        for (nb0T=0; (nb0T < seg0SizeT) && (ptrT[nb0T] == 0); nb0T++) ;
        storedSkips += (unsigned)(nb0T * sizeT);

        if (nb0T != seg0SizeT) {   /* not all 0s */
            errno = 0;
            {   int const seekResult = UTIL_fseek(file, storedSkips, SEEK_CUR);
                if (seekResult) EXM_THROW(72, "Sparse skip error(%d): %s ; try --no-sparse", (int)errno, strerror(errno));
            }
            storedSkips = 0;
            seg0SizeT -= nb0T;
            ptrT += nb0T;
            {   size_t const sizeCheck = fwrite(ptrT, sizeT, seg0SizeT, file);
                if (sizeCheck != seg0SizeT) EXM_THROW(73, "Write error : cannot write decoded block");
        }   }
        ptrT += seg0SizeT;
    }

    if (bufferSize & maskT) {  /* size not multiple of sizeT : implies end of block */
        const char* const restStart = (const char*)bufferTEnd;
        const char* restPtr = restStart;
        size_t const restSize =  bufferSize & maskT;
        const char* const restEnd = restStart + restSize;
        for (; (restPtr < restEnd) && (*restPtr == 0); restPtr++) ;
        storedSkips += (unsigned) (restPtr - restStart);
        if (restPtr != restEnd) {
            int const seekResult = UTIL_fseek(file, storedSkips, SEEK_CUR);
            if (seekResult) EXM_THROW(74, "Sparse skip error ; try --no-sparse");
            storedSkips = 0;
            {   size_t const sizeCheck = fwrite(restPtr, 1, (size_t)(restEnd - restPtr), file);
                if (sizeCheck != (size_t)(restEnd - restPtr)) EXM_THROW(75, "Write error : cannot write decoded end of block");
        }   }
    }

    return storedSkips;
}

static void LZ4IO_fwriteSparseEnd(FILE* file, unsigned storedSkips)
{
    if (storedSkips>0) {   /* implies sparseFileSupport>0 */
        const char lastZeroByte[1] = { 0 };
        if (UTIL_fseek(file, storedSkips-1, SEEK_CUR) != 0)
            EXM_THROW(69, "Final skip error (sparse file)\n");
        if (fwrite(lastZeroByte, 1, 1, file) != 1)
            EXM_THROW(69, "Write error : cannot write last zero\n");
    }
}


static unsigned g_magicRead = 0;   /* out-parameter of LZ4IO_decodeLegacyStream() */
static unsigned long long LZ4IO_decodeLegacyStream(FILE* finput, FILE* foutput, const LZ4IO_prefs_t* prefs)
{
    unsigned long long streamSize = 0;
    unsigned storedSkips = 0;

    /* Allocate Memory */
    char* const in_buff  = (char*)malloc((size_t)LZ4_compressBound(LEGACY_BLOCKSIZE));
    char* const out_buff = (char*)malloc(LEGACY_BLOCKSIZE);
    if (!in_buff || !out_buff) EXM_THROW(51, "Allocation error : not enough memory");

    /* Main Loop */
    while (1) {
        unsigned int blockSize;

        /* Block Size */
        {   size_t const sizeCheck = fread(in_buff, 1, 4, finput);
            if (sizeCheck == 0) break;                   /* Nothing to read : file read is completed */
            if (sizeCheck != 4) EXM_THROW(52, "Read error : cannot access block size "); }
            blockSize = LZ4IO_readLE32(in_buff);       /* Convert to Little Endian */
            if (blockSize > LZ4_COMPRESSBOUND(LEGACY_BLOCKSIZE)) {
            /* Cannot read next block : maybe new stream ? */
            g_magicRead = blockSize;
            break;
        }

        /* Read Block */
        { size_t const sizeCheck = fread(in_buff, 1, blockSize, finput);
          if (sizeCheck!=blockSize) EXM_THROW(52, "Read error : cannot access compressed block !"); }

        /* Decode Block */
        {   int const decodeSize = LZ4_decompress_safe(in_buff, out_buff, (int)blockSize, LEGACY_BLOCKSIZE);
            if (decodeSize < 0) EXM_THROW(53, "Decoding Failed ! Corrupted input detected !");
            streamSize += (unsigned long long)decodeSize;
            /* Write Block */
            storedSkips = LZ4IO_fwriteSparse(foutput, out_buff, (size_t)decodeSize, prefs->sparseFileSupport, storedSkips); /* success or die */
    }   }
    if (ferror(finput)) EXM_THROW(54, "Read error : ferror");

    LZ4IO_fwriteSparseEnd(foutput, storedSkips);

    /* Free */
    free(in_buff);
    free(out_buff);

    return streamSize;
}



typedef struct {
    void*  srcBuffer;
    size_t srcBufferSize;
    void*  dstBuffer;
    size_t dstBufferSize;
    FILE*  dstFile;
    LZ4F_decompressionContext_t dCtx;
    void*  dictBuffer;
    size_t dictBufferSize;
} dRess_t;

static void LZ4IO_loadDDict(dRess_t* ress, const LZ4IO_prefs_t* const prefs)
{
    if (!prefs->useDictionary) {
        ress->dictBuffer = NULL;
        ress->dictBufferSize = 0;
        return;
    }

    ress->dictBuffer = LZ4IO_createDict(&ress->dictBufferSize, prefs->dictionaryFilename);
    if (!ress->dictBuffer) EXM_THROW(25, "Dictionary error : could not create dictionary");
}

static const size_t LZ4IO_dBufferSize = 64 KB;
static dRess_t LZ4IO_createDResources(const LZ4IO_prefs_t* const prefs)
{
    dRess_t ress;

    /* init */
    LZ4F_errorCode_t const errorCode = LZ4F_createDecompressionContext(&ress.dCtx, LZ4F_VERSION);
    if (LZ4F_isError(errorCode)) EXM_THROW(60, "Can't create LZ4F context : %s", LZ4F_getErrorName(errorCode));

    /* Allocate Memory */
    ress.srcBufferSize = LZ4IO_dBufferSize;
    ress.srcBuffer = malloc(ress.srcBufferSize);
    ress.dstBufferSize = LZ4IO_dBufferSize;
    ress.dstBuffer = malloc(ress.dstBufferSize);
    if (!ress.srcBuffer || !ress.dstBuffer) EXM_THROW(61, "Allocation error : not enough memory");

    LZ4IO_loadDDict(&ress, prefs);

    ress.dstFile = NULL;
    return ress;
}

static void LZ4IO_freeDResources(dRess_t ress)
{
    LZ4F_errorCode_t errorCode = LZ4F_freeDecompressionContext(ress.dCtx);
    if (LZ4F_isError(errorCode)) EXM_THROW(69, "Error : can't free LZ4F context resource : %s", LZ4F_getErrorName(errorCode));
    free(ress.srcBuffer);
    free(ress.dstBuffer);
    free(ress.dictBuffer);
}


static unsigned long long
LZ4IO_decompressLZ4F(dRess_t ress,
                     FILE* const srcFile, FILE* const dstFile,
                     const LZ4IO_prefs_t* const prefs)
{
    unsigned long long filesize = 0;
    LZ4F_errorCode_t nextToLoad;
    unsigned storedSkips = 0;

    /* Init feed with magic number (already consumed from FILE* sFile) */
    {   size_t inSize = MAGICNUMBER_SIZE;
        size_t outSize= 0;
        LZ4IO_writeLE32(ress.srcBuffer, LZ4IO_MAGICNUMBER);
        nextToLoad = LZ4F_decompress_usingDict(ress.dCtx, ress.dstBuffer, &outSize, ress.srcBuffer, &inSize, ress.dictBuffer, ress.dictBufferSize, NULL);
        if (LZ4F_isError(nextToLoad)) EXM_THROW(62, "Header error : %s", LZ4F_getErrorName(nextToLoad));
    }

    /* Main Loop */
    for (;nextToLoad;) {
        size_t readSize;
        size_t pos = 0;
        size_t decodedBytes = ress.dstBufferSize;

        /* Read input */
        if (nextToLoad > ress.srcBufferSize) nextToLoad = ress.srcBufferSize;
        readSize = fread(ress.srcBuffer, 1, nextToLoad, srcFile);
        if (!readSize) break;   /* reached end of file or stream */

        while ((pos < readSize) || (decodedBytes == ress.dstBufferSize)) {  /* still to read, or still to flush */
            /* Decode Input (at least partially) */
            size_t remaining = readSize - pos;
            decodedBytes = ress.dstBufferSize;
            nextToLoad = LZ4F_decompress_usingDict(ress.dCtx, ress.dstBuffer, &decodedBytes, (char*)(ress.srcBuffer)+pos, &remaining, ress.dictBuffer, ress.dictBufferSize, NULL);
            if (LZ4F_isError(nextToLoad)) EXM_THROW(66, "Decompression error : %s", LZ4F_getErrorName(nextToLoad));
            pos += remaining;

            /* Write Block */
            if (decodedBytes) {
                if (!prefs->testMode)
                    storedSkips = LZ4IO_fwriteSparse(dstFile, ress.dstBuffer, decodedBytes, prefs->sparseFileSupport, storedSkips);
                filesize += decodedBytes;
                DISPLAYUPDATE(2, "\rDecompressed : %u MB  ", (unsigned)(filesize>>20));
            }

            if (!nextToLoad) break;
        }
    }
    /* can be out because readSize == 0, which could be an fread() error */
    if (ferror(srcFile)) EXM_THROW(67, "Read error");

    if (!prefs->testMode) LZ4IO_fwriteSparseEnd(dstFile, storedSkips);
    if (nextToLoad!=0) EXM_THROW(68, "Unfinished stream");

    return filesize;
}


/* LZ4IO_passThrough:
 * just output the same content as input, no decoding.
 * This is a capability of zcat, and by extension lz4cat
 * MNstore : contain the first MAGICNUMBER_SIZE bytes already read from finput
 */
#define PTSIZE  (64 KB)
#define PTSIZET (PTSIZE / sizeof(size_t))
static unsigned long long
LZ4IO_passThrough(FILE* finput, FILE* foutput,
                  unsigned char MNstore[MAGICNUMBER_SIZE],
                  int sparseFileSupport)
{
	size_t buffer[PTSIZET];
    size_t readBytes = 1;
    unsigned long long total = MAGICNUMBER_SIZE;
    unsigned storedSkips = 0;

    if (fwrite(MNstore, 1, MAGICNUMBER_SIZE, foutput) != MAGICNUMBER_SIZE) {
        EXM_THROW(50, "Pass-through write error");
    }
    while (readBytes) {
        readBytes = fread(buffer, 1, sizeof(buffer), finput);
        total += readBytes;
        storedSkips = LZ4IO_fwriteSparse(foutput, buffer, readBytes, sparseFileSupport, storedSkips);
    }
    if (ferror(finput)) EXM_THROW(51, "Read Error");

    LZ4IO_fwriteSparseEnd(foutput, storedSkips);
    return total;
}


/** Safely handle cases when (unsigned)offset > LONG_MAX */
static int fseek_u32(FILE *fp, unsigned offset, int where)
{
    const unsigned stepMax = 1U << 30;
    int errorNb = 0;

    if (where != SEEK_CUR) return -1;  /* Only allows SEEK_CUR */
    while (offset > 0) {
        unsigned s = offset;
        if (s > stepMax) s = stepMax;
        errorNb = UTIL_fseek(fp, (long) s, SEEK_CUR);
        if (errorNb != 0) break;
        offset -= s;
    }
    return errorNb;
}

#define ENDOFSTREAM ((unsigned long long)-1)
static unsigned long long
selectDecoder(dRess_t ress,
              FILE* finput, FILE* foutput,
              const LZ4IO_prefs_t* const prefs)
{
    unsigned char MNstore[MAGICNUMBER_SIZE];
    unsigned magicNumber;
    static unsigned nbFrames = 0;

    /* init */
    nbFrames++;

    /* Check Archive Header */
    if (g_magicRead) {  /* magic number already read from finput (see legacy frame)*/
        magicNumber = g_magicRead;
        g_magicRead = 0;
    } else {
        size_t const nbReadBytes = fread(MNstore, 1, MAGICNUMBER_SIZE, finput);
        if (nbReadBytes==0) { nbFrames = 0; return ENDOFSTREAM; }   /* EOF */
        if (nbReadBytes != MAGICNUMBER_SIZE)
          EXM_THROW(40, "Unrecognized header : Magic Number unreadable");
        magicNumber = LZ4IO_readLE32(MNstore);   /* Little Endian format */
    }
    if (LZ4IO_isSkippableMagicNumber(magicNumber))
        magicNumber = LZ4IO_SKIPPABLE0;   /* fold skippable magic numbers */

    switch(magicNumber)
    {
    case LZ4IO_MAGICNUMBER:
        return LZ4IO_decompressLZ4F(ress, finput, foutput, prefs);
    case LEGACY_MAGICNUMBER:
        DISPLAYLEVEL(4, "Detected : Legacy format \n");
        return LZ4IO_decodeLegacyStream(finput, foutput, prefs);
    case LZ4IO_SKIPPABLE0:
        DISPLAYLEVEL(4, "Skipping detected skippable area \n");
        {   size_t const nbReadBytes = fread(MNstore, 1, 4, finput);
            if (nbReadBytes != 4)
                EXM_THROW(42, "Stream error : skippable size unreadable");
        }
        {   unsigned const size = LZ4IO_readLE32(MNstore);
            int const errorNb = fseek_u32(finput, size, SEEK_CUR);
            if (errorNb != 0)
                EXM_THROW(43, "Stream error : cannot skip skippable area");
        }
        return 0;
    EXTENDED_FORMAT;  /* macro extension for custom formats */
    default:
        if (nbFrames == 1) {  /* just started */
            /* Wrong magic number at the beginning of 1st stream */
            if (!prefs->testMode && prefs->overwrite && prefs->passThrough) {
                nbFrames = 0;
                return LZ4IO_passThrough(finput, foutput, MNstore, prefs->sparseFileSupport);
            }
            EXM_THROW(44,"Unrecognized header : file cannot be decoded");
        }
        {   long int const position = ftell(finput);  /* only works for files < 2 GB */
            DISPLAYLEVEL(2, "Stream followed by undecodable data ");
            if (position != -1L)
                DISPLAYLEVEL(2, "at position %i ", (int)position);
            DISPLAYLEVEL(2, "\n");
        }
        return ENDOFSTREAM;
    }
}


static int
LZ4IO_decompressSrcFile(dRess_t ress,
                        const char* input_filename, const char* output_filename,
                        const LZ4IO_prefs_t* const prefs)
{
    FILE* const foutput = ress.dstFile;
    unsigned long long filesize = 0;

    /* Init */
    FILE* const finput = LZ4IO_openSrcFile(input_filename);
    if (finput==NULL) return 1;
    assert(foutput != NULL);

    /* Loop over multiple streams */
    for ( ; ; ) {  /* endless loop, see break condition */
        unsigned long long const decodedSize =
                        selectDecoder(ress, finput, foutput, prefs);
        if (decodedSize == ENDOFSTREAM) break;
        filesize += decodedSize;
    }

    /* Close input */
    fclose(finput);
    if (prefs->removeSrcFile) {  /* --rm */
        if (remove(input_filename))
            EXM_THROW(45, "Remove error : %s: %s", input_filename, strerror(errno));
    }

    /* Final Status */
    DISPLAYLEVEL(2, "\r%79s\r", "");
    DISPLAYLEVEL(2, "%-20.20s : decoded %llu bytes \n", input_filename, filesize);
    (void)output_filename;

    return 0;
}


static int
LZ4IO_decompressDstFile(dRess_t ress,
                        const char* input_filename, const char* output_filename,
                        const LZ4IO_prefs_t* const prefs)
{
    stat_t statbuf;
    int stat_result = 0;
    FILE* const foutput = LZ4IO_openDstFile(output_filename, prefs);
    if (foutput==NULL) return 1;   /* failure */

    if ( strcmp(input_filename, stdinmark)
      && UTIL_getFileStat(input_filename, &statbuf))
        stat_result = 1;

    ress.dstFile = foutput;
    LZ4IO_decompressSrcFile(ress, input_filename, output_filename, prefs);

    fclose(foutput);

    /* Copy owner, file permissions and modification time */
    if ( stat_result != 0
      && strcmp (output_filename, stdoutmark)
      && strcmp (output_filename, nulmark)) {
        UTIL_setFileStat(output_filename, &statbuf);
        /* should return value be read ? or is silent fail good enough ? */
    }

    return 0;
}


int LZ4IO_decompressFilename(const char* input_filename, const char* output_filename, const LZ4IO_prefs_t* prefs)
{
    dRess_t const ress = LZ4IO_createDResources(prefs);
    clock_t const start = clock();

    int const missingFiles = LZ4IO_decompressDstFile(ress, input_filename, output_filename, prefs);

    clock_t const end = clock();
    double const seconds = (double)(end - start) / CLOCKS_PER_SEC;
    DISPLAYLEVEL(4, "Done in %.2f sec  \n", seconds);

    LZ4IO_freeDResources(ress);
    return missingFiles;
}


int LZ4IO_decompressMultipleFilenames(
                            const char** inFileNamesTable, int ifntSize,
                            const char* suffix,
                            const LZ4IO_prefs_t* prefs)
{
    int i;
    int skippedFiles = 0;
    int missingFiles = 0;
    char* outFileName = (char*)malloc(FNSPACE);
    size_t ofnSize = FNSPACE;
    size_t const suffixSize = strlen(suffix);
    dRess_t ress = LZ4IO_createDResources(prefs);

    if (outFileName==NULL) EXM_THROW(70, "Memory allocation error");
    ress.dstFile = LZ4IO_openDstFile(stdoutmark, prefs);

    for (i=0; i<ifntSize; i++) {
        size_t const ifnSize = strlen(inFileNamesTable[i]);
        const char* const suffixPtr = inFileNamesTable[i] + ifnSize - suffixSize;
        if (!strcmp(suffix, stdoutmark)) {
            missingFiles += LZ4IO_decompressSrcFile(ress, inFileNamesTable[i], stdoutmark, prefs);
            continue;
        }
        if (ofnSize <= ifnSize-suffixSize+1) {
            free(outFileName);
            ofnSize = ifnSize + 20;
            outFileName = (char*)malloc(ofnSize);
            if (outFileName==NULL) EXM_THROW(71, "Memory allocation error");
        }
        if (ifnSize <= suffixSize  ||  strcmp(suffixPtr, suffix) != 0) {
            DISPLAYLEVEL(1, "File extension doesn't match expected LZ4_EXTENSION (%4s); will not process file: %s\n", suffix, inFileNamesTable[i]);
            skippedFiles++;
            continue;
        }
        memcpy(outFileName, inFileNamesTable[i], ifnSize - suffixSize);
        outFileName[ifnSize-suffixSize] = '\0';
        missingFiles += LZ4IO_decompressDstFile(ress, inFileNamesTable[i], outFileName, prefs);
    }

    LZ4IO_freeDResources(ress);
    free(outFileName);
    return missingFiles + skippedFiles;
}


/* ********************************************************************* */
/* **********************   LZ4 --list command   *********************** */
/* ********************************************************************* */

typedef enum
{
    lz4Frame = 0,
    legacyFrame,
    skippableFrame
} LZ4IO_frameType_t;

typedef struct {
    LZ4F_frameInfo_t lz4FrameInfo;
    LZ4IO_frameType_t frameType;
} LZ4IO_frameInfo_t;

#define LZ4IO_INIT_FRAMEINFO  { LZ4F_INIT_FRAMEINFO, lz4Frame }

typedef struct {
    const char* fileName;
    unsigned long long fileSize;
    unsigned long long frameCount;
    LZ4IO_frameInfo_t frameSummary;
    unsigned short eqFrameTypes;
    unsigned short eqBlockTypes;
    unsigned short allContentSize;
} LZ4IO_cFileInfo_t;

#define LZ4IO_INIT_CFILEINFO  { NULL, 0ULL, 0, LZ4IO_INIT_FRAMEINFO, 1, 1, 1 }

typedef enum { LZ4IO_LZ4F_OK, LZ4IO_format_not_known, LZ4IO_not_a_file } LZ4IO_infoResult;

static const char * LZ4IO_frameTypeNames[] = {"LZ4Frame", "LegacyFrame", "SkippableFrame" };

/* Read block headers and skip block data
   Return total blocks size for this frame including block headers,
   block checksums and content checksums.
   returns 0 in case it can't succesfully skip block data.
   Assumes SEEK_CUR after frame header.
 */
static unsigned long long
LZ4IO_skipBlocksData(FILE* finput,
               const LZ4F_blockChecksum_t blockChecksumFlag,
               const LZ4F_contentChecksum_t contentChecksumFlag)
{
    unsigned char blockInfo[LZ4F_BLOCK_HEADER_SIZE];
    unsigned long long totalBlocksSize = 0;
    for (;;) {
        if (!fread(blockInfo, 1, LZ4F_BLOCK_HEADER_SIZE, finput)) {
            if (feof(finput)) return totalBlocksSize;
            return 0;
        }
        totalBlocksSize += LZ4F_BLOCK_HEADER_SIZE;
        {   const unsigned long nextCBlockSize = LZ4IO_readLE32(&blockInfo) & 0x7FFFFFFFU;
            const unsigned long nextBlock = nextCBlockSize + (blockChecksumFlag * LZ4F_BLOCK_CHECKSUM_SIZE);
            if (nextCBlockSize == 0) {
                /* Reached EndMark */
                if (contentChecksumFlag) {
                    /* Skip content checksum */
                    if (UTIL_fseek(finput, LZ4F_CONTENT_CHECKSUM_SIZE, SEEK_CUR) != 0) {
                        return 0;
                    }
                    totalBlocksSize += LZ4F_CONTENT_CHECKSUM_SIZE;
                }
                break;
            }
            totalBlocksSize += nextBlock;
            /* skip to the next block */
            assert(nextBlock < LONG_MAX);
            if (UTIL_fseek(finput, (long)nextBlock, SEEK_CUR) != 0) return 0;
    }   }
    return totalBlocksSize;
}

/* For legacy frames only.
   Read block headers and skip block data.
   Return total blocks size for this frame including block headers.
   or 0 in case it can't succesfully skip block data.
   This works as long as legacy block header size = magic number size.
   Assumes SEEK_CUR after frame header.
 */
static unsigned long long LZ4IO_skipLegacyBlocksData(FILE* finput)
{
    unsigned char blockInfo[LZIO_LEGACY_BLOCK_HEADER_SIZE];
    unsigned long long totalBlocksSize = 0;
    LZ4IO_STATIC_ASSERT(LZIO_LEGACY_BLOCK_HEADER_SIZE == MAGICNUMBER_SIZE);
    for (;;) {
        if (!fread(blockInfo, 1, LZIO_LEGACY_BLOCK_HEADER_SIZE, finput)) {
            if (feof(finput)) return totalBlocksSize;
            return 0;
        }
        {   const unsigned int nextCBlockSize = LZ4IO_readLE32(&blockInfo);
            if ( nextCBlockSize == LEGACY_MAGICNUMBER ||
                    nextCBlockSize == LZ4IO_MAGICNUMBER ||
                    LZ4IO_isSkippableMagicNumber(nextCBlockSize)) {
                /* Rewind back. we want cursor at the begining of next frame.*/
                if (fseek(finput, -LZIO_LEGACY_BLOCK_HEADER_SIZE, SEEK_CUR) != 0) {
                    return 0;
                }
                break;
            }
            totalBlocksSize += LZIO_LEGACY_BLOCK_HEADER_SIZE + nextCBlockSize;
            /* skip to the next block */
            if (UTIL_fseek(finput, nextCBlockSize, SEEK_CUR) != 0) {
                return 0;
    }   }   }
    return totalBlocksSize;
}

/* LZ4IO_blockTypeID:
 * return human-readable block type, following command line convention
 * buffer : must be a valid memory area of at least 4 bytes */
const char* LZ4IO_blockTypeID(LZ4F_blockSizeID_t sizeID, LZ4F_blockMode_t blockMode, char buffer[4])
{
    buffer[0] = 'B';
    assert(sizeID >= 4); assert(sizeID <= 7);
    buffer[1] = (char)(sizeID + '0');
    buffer[2] = (blockMode == LZ4F_blockIndependent) ? 'I' : 'D';
    buffer[3] = 0;
    return buffer;
}

/* buffer : must be valid memory area of at least 10 bytes */
static const char* LZ4IO_toHuman(long double size, char *buf)
{
    const char units[] = {"\0KMGTPEZY"};
    size_t i = 0;
    for (; size >= 1024; i++) size /= 1024;
    sprintf(buf, "%.2Lf%c", size, units[i]);
    return buf;
}

/* Get filename without path prefix */
static const char* LZ4IO_baseName(const char* input_filename)
{
    const char* b = strrchr(input_filename, '/');
    if (!b) b = strrchr(input_filename, '\\');
    if (!b) return input_filename;
    return b + 1;
}

/* Report frame/s information (--list) in verbose mode (-v).
 * Will populate file info with fileName and frameSummary where applicable.
 * - TODO :
 *  + report nb of blocks, hence max. possible decompressed size (when not reported in header)
 */
static LZ4IO_infoResult
LZ4IO_getCompressedFileInfo(LZ4IO_cFileInfo_t* cfinfo, const char* input_filename)
{
    LZ4IO_infoResult result = LZ4IO_format_not_known;  /* default result (error) */
    unsigned char buffer[LZ4F_HEADER_SIZE_MAX];
    FILE* const finput = LZ4IO_openSrcFile(input_filename);

    if (finput == NULL) return LZ4IO_not_a_file;
    cfinfo->fileSize = UTIL_getOpenFileSize(finput);

    while (!feof(finput)) {
        LZ4IO_frameInfo_t frameInfo = LZ4IO_INIT_FRAMEINFO;
        unsigned magicNumber;
        /* Get MagicNumber */
        {   size_t const nbReadBytes = fread(buffer, 1, MAGICNUMBER_SIZE, finput);
            if (nbReadBytes == 0) { break; } /* EOF */
            result = LZ4IO_format_not_known;  /* default result (error) */
            if (nbReadBytes != MAGICNUMBER_SIZE) {
                EXM_THROW(40, "Unrecognized header : Magic Number unreadable");
        }   }
        magicNumber = LZ4IO_readLE32(buffer);   /* Little Endian format */
        if (LZ4IO_isSkippableMagicNumber(magicNumber))
            magicNumber = LZ4IO_SKIPPABLE0;   /* fold skippable magic numbers */

        switch (magicNumber) {
        case LZ4IO_MAGICNUMBER:
            if (cfinfo->frameSummary.frameType != lz4Frame) cfinfo->eqFrameTypes = 0;
            /* Get frame info */
            {   const size_t readBytes = fread(buffer + MAGICNUMBER_SIZE, 1, LZ4F_HEADER_SIZE_MIN - MAGICNUMBER_SIZE, finput);
                if (!readBytes || ferror(finput)) EXM_THROW(71, "Error reading %s", input_filename);
            }
            {   size_t hSize = LZ4F_headerSize(&buffer, LZ4F_HEADER_SIZE_MIN);
                if (LZ4F_isError(hSize)) break;
                if (hSize > (LZ4F_HEADER_SIZE_MIN + MAGICNUMBER_SIZE)) {
                    /* We've already read LZ4F_HEADER_SIZE_MIN so read any extra until hSize*/
                    const size_t readBytes = fread(buffer + LZ4F_HEADER_SIZE_MIN, 1, hSize - LZ4F_HEADER_SIZE_MIN, finput);
                    if (!readBytes || ferror(finput)) EXM_THROW(72, "Error reading %s", input_filename);
                }
                /* Create decompression context */
                {   LZ4F_dctx* dctx;
                    if ( LZ4F_isError(LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION)) ) break;
                    {   unsigned const frameInfoError = LZ4F_isError(LZ4F_getFrameInfo(dctx, &frameInfo.lz4FrameInfo, buffer, &hSize));
                        LZ4F_freeDecompressionContext(dctx);
                        if (frameInfoError) break;
                        if ((cfinfo->frameSummary.lz4FrameInfo.blockSizeID != frameInfo.lz4FrameInfo.blockSizeID ||
                                cfinfo->frameSummary.lz4FrameInfo.blockMode != frameInfo.lz4FrameInfo.blockMode)
                                && cfinfo->frameCount != 0)
                            cfinfo->eqBlockTypes = 0;
                        {   const unsigned long long totalBlocksSize = LZ4IO_skipBlocksData(finput,
                                    frameInfo.lz4FrameInfo.blockChecksumFlag,
                                    frameInfo.lz4FrameInfo.contentChecksumFlag);
                            if (totalBlocksSize) {
                                char bTypeBuffer[5];
                                LZ4IO_blockTypeID(frameInfo.lz4FrameInfo.blockSizeID, frameInfo.lz4FrameInfo.blockMode, bTypeBuffer);
                                DISPLAYLEVEL(3, "    %6llu %14s %5s %8s",
                                             cfinfo->frameCount + 1,
                                             LZ4IO_frameTypeNames[frameInfo.frameType],
                                             bTypeBuffer,
                                             frameInfo.lz4FrameInfo.contentChecksumFlag ? "XXH32" : "-");
                                if (frameInfo.lz4FrameInfo.contentSize) {
                                    {   double const ratio = (double)(totalBlocksSize + hSize) / frameInfo.lz4FrameInfo.contentSize * 100;
                                        DISPLAYLEVEL(3, " %20llu %20llu %9.2f%%\n",
                                                     totalBlocksSize + hSize,
                                                     frameInfo.lz4FrameInfo.contentSize,
                                                     ratio);
                                    }
                                    /* Now we've consumed frameInfo we can use it to store the total contentSize */
                                    frameInfo.lz4FrameInfo.contentSize += cfinfo->frameSummary.lz4FrameInfo.contentSize;
                                }
                                else {
                                    DISPLAYLEVEL(3, " %20llu %20s %9s \n", totalBlocksSize + hSize, "-", "-");
                                    cfinfo->allContentSize = 0;
                                }
                                result = LZ4IO_LZ4F_OK;
            }   }   }   }   }
            break;
        case LEGACY_MAGICNUMBER:
            frameInfo.frameType = legacyFrame;
            if (cfinfo->frameSummary.frameType != legacyFrame && cfinfo->frameCount != 0) cfinfo->eqFrameTypes = 0;
            cfinfo->eqBlockTypes = 0;
            cfinfo->allContentSize = 0;
            {   const unsigned long long totalBlocksSize = LZ4IO_skipLegacyBlocksData(finput);
                if (totalBlocksSize) {
                    DISPLAYLEVEL(3, "    %6llu %14s %5s %8s %20llu %20s %9s\n",
                                 cfinfo->frameCount + 1,
                                 LZ4IO_frameTypeNames[frameInfo.frameType],
                                 "-", "-",
                                 totalBlocksSize + 4,
                                 "-", "-");
                    result = LZ4IO_LZ4F_OK;
            }   }
            break;
        case LZ4IO_SKIPPABLE0:
            frameInfo.frameType = skippableFrame;
            if (cfinfo->frameSummary.frameType != skippableFrame && cfinfo->frameCount != 0) cfinfo->eqFrameTypes = 0;
            cfinfo->eqBlockTypes = 0;
            cfinfo->allContentSize = 0;
            {   size_t const nbReadBytes = fread(buffer, 1, 4, finput);
                if (nbReadBytes != 4)
                    EXM_THROW(42, "Stream error : skippable size unreadable");
            }
            {   unsigned const size = LZ4IO_readLE32(buffer);
                int const errorNb = fseek_u32(finput, size, SEEK_CUR);
                if (errorNb != 0)
                    EXM_THROW(43, "Stream error : cannot skip skippable area");
                DISPLAYLEVEL(3, "    %6llu %14s %5s %8s %20u %20s %9s\n",
                             cfinfo->frameCount + 1,
                             "SkippableFrame",
                             "-", "-", size + 8, "-", "-");

                result = LZ4IO_LZ4F_OK;
            }
            break;
        default:
            {   long int const position = ftell(finput);  /* only works for files < 2 GB */
                DISPLAYLEVEL(3, "Stream followed by undecodable data ");
                if (position != -1L)
                    DISPLAYLEVEL(3, "at position %i ", (int)position);
                DISPLAYLEVEL(3, "\n");
            }
        break;
        }
        if (result != LZ4IO_LZ4F_OK) break;
        cfinfo->frameSummary = frameInfo;
        cfinfo->frameCount++;
    }  /* while (!feof(finput)) */
    fclose(finput);
    return result;
}


int LZ4IO_displayCompressedFilesInfo(const char** inFileNames, size_t ifnIdx)
{
    int result = 0;
    size_t idx = 0;
    if (g_displayLevel < 3) {
        DISPLAYOUT("%10s %14s %5s %11s %13s %9s   %s\n",
                "Frames", "Type", "Block", "Compressed", "Uncompressed", "Ratio", "Filename");
    }
    for (; idx < ifnIdx; idx++) {
        /* Get file info */
        LZ4IO_cFileInfo_t cfinfo = LZ4IO_INIT_CFILEINFO;
        cfinfo.fileName = LZ4IO_baseName(inFileNames[idx]);
        if (!UTIL_isRegFile(inFileNames[idx])) {
            DISPLAYLEVEL(1, "lz4: %s is not a regular file \n", inFileNames[idx]);
            return 0;
        }
        DISPLAYLEVEL(3, "%s(%llu/%llu)\n", cfinfo.fileName, (unsigned long long)idx + 1, (unsigned  long long)ifnIdx);
        DISPLAYLEVEL(3, "    %6s %14s %5s %8s %20s %20s %9s\n",
                     "Frame", "Type", "Block", "Checksum", "Compressed", "Uncompressed", "Ratio")
        {   LZ4IO_infoResult const op_result = LZ4IO_getCompressedFileInfo(&cfinfo, inFileNames[idx]);
            if (op_result != LZ4IO_LZ4F_OK) {
                assert(op_result == LZ4IO_format_not_known);
                DISPLAYLEVEL(1, "lz4: %s: File format not recognized \n", inFileNames[idx]);
                return 0;
        }   }
        DISPLAYLEVEL(3, "\n");
        if (g_displayLevel < 3) {
            /* Display Summary */
            {   char buffers[3][10];
                DISPLAYOUT("%10llu %14s %5s %11s %13s ",
                        cfinfo.frameCount,
                        cfinfo.eqFrameTypes ? LZ4IO_frameTypeNames[cfinfo.frameSummary.frameType] : "-" ,
                        cfinfo.eqBlockTypes ? LZ4IO_blockTypeID(cfinfo.frameSummary.lz4FrameInfo.blockSizeID,
                                                                cfinfo.frameSummary.lz4FrameInfo.blockMode, buffers[0]) : "-",
                        LZ4IO_toHuman((long double)cfinfo.fileSize, buffers[1]),
                        cfinfo.allContentSize ? LZ4IO_toHuman((long double)cfinfo.frameSummary.lz4FrameInfo.contentSize, buffers[2]) : "-");
                if (cfinfo.allContentSize) {
                    double const ratio = (double)cfinfo.fileSize / cfinfo.frameSummary.lz4FrameInfo.contentSize * 100;
                    DISPLAYOUT("%9.2f%%  %s \n", ratio, cfinfo.fileName);
                } else {
                    DISPLAYOUT("%9s   %s\n",
                            "-",
                            cfinfo.fileName);
        }   }   }  /* if (g_displayLevel < 3) */
    }  /* for (; idx < ifnIdx; idx++) */

    return result;
}
