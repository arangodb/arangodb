// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2002-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  uenumtst.c
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:2
*
*   created on: 2002jul08
*   created by: Vladimir Weinstein
*/

#include "cintltst.h"
#include "uenumimp.h"
#include "cmemory.h"
#include "cstring.h"
#include "unicode/ustring.h"

static char quikBuf[256];
static char* quikU2C(const UChar* str, int32_t len) {
    u_UCharsToChars(str, quikBuf, len);
    quikBuf[len] = 0;
    return quikBuf;
}

static const char* test1[] = {
    "first",
    "second",
    "third",
    "fourth"
};

struct chArrayContext {
    int32_t currIndex;
    int32_t maxIndex;
    char *currChar;
    UChar *currUChar;
    char **array;
};

typedef struct chArrayContext chArrayContext;

#define cont ((chArrayContext *)en->context)

static void U_CALLCONV
chArrayClose(UEnumeration *en) {
    if(cont->currUChar != NULL) {
        free(cont->currUChar);
        cont->currUChar = NULL;
    }
    free(en);
}

static int32_t U_CALLCONV
chArrayCount(UEnumeration *en, UErrorCode *status) {
    return cont->maxIndex;
}

static const UChar* U_CALLCONV 
chArrayUNext(UEnumeration *en, int32_t *resultLength, UErrorCode *status) {
    if(cont->currIndex >= cont->maxIndex) {
        return NULL;
    }
    
    if(cont->currUChar == NULL) {
        cont->currUChar = (UChar *)malloc(1024*sizeof(UChar));
    }
    
    cont->currChar = (cont->array)[cont->currIndex];
    *resultLength = (int32_t)strlen(cont->currChar);
    u_charsToUChars(cont->currChar, cont->currUChar, *resultLength);
    cont->currIndex++;
    return cont->currUChar;
}

static const char* U_CALLCONV
chArrayNext(UEnumeration *en, int32_t *resultLength, UErrorCode *status) {
    if(cont->currIndex >= cont->maxIndex) {
        return NULL;
    }
    
    cont->currChar = (cont->array)[cont->currIndex];
    *resultLength = (int32_t)strlen(cont->currChar);
    cont->currIndex++;
    return cont->currChar;
}

static void U_CALLCONV
chArrayReset(UEnumeration *en, UErrorCode *status) {
    cont->currIndex = 0;
}

chArrayContext myCont = {
    0, 0,
    NULL, NULL,
    NULL
};

UEnumeration chEnum = {
    NULL,
    &myCont,
    chArrayClose,
    chArrayCount,
    chArrayUNext,
    chArrayNext,
    chArrayReset
};

static const UEnumeration emptyEnumerator = {
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
};

static const UEnumeration emptyPartialEnumerator = {
    NULL,
    NULL,
    NULL,
    NULL,
    uenum_unextDefault,
    NULL,
    NULL,
};

/********************************************************************/
static const UChar _first[] = {102,105,114,115,116,0};    /* "first"  */
static const UChar _second[]= {115,101,99,111,110,100,0}; /* "second" */
static const UChar _third[] = {116,104,105,114,100,0};    /* "third"  */
static const UChar _fourth[]= {102,111,117,114,116,104,0};/* "fourth" */

static const UChar* test2[] = {
    _first, _second, _third, _fourth
};

struct uchArrayContext {
    int32_t currIndex;
    int32_t maxIndex;
    UChar *currUChar;
    UChar **array;
};

typedef struct uchArrayContext uchArrayContext;

#define ucont ((uchArrayContext *)en->context)

static void U_CALLCONV
uchArrayClose(UEnumeration *en) {
    free(en);
}

static int32_t U_CALLCONV
uchArrayCount(UEnumeration *en, UErrorCode *status) {
    return ucont->maxIndex;
}

static const UChar* U_CALLCONV
uchArrayUNext(UEnumeration *en, int32_t *resultLength, UErrorCode *status) {
    if(ucont->currIndex >= ucont->maxIndex) {
        return NULL;
    }
    
    ucont->currUChar = (ucont->array)[ucont->currIndex];
    *resultLength = u_strlen(ucont->currUChar);
    ucont->currIndex++;
    return ucont->currUChar;
}

static void U_CALLCONV
uchArrayReset(UEnumeration *en, UErrorCode *status) {
    ucont->currIndex = 0;
}

uchArrayContext myUCont = {
    0, 0,
    NULL, NULL
};

UEnumeration uchEnum = {
    NULL,
    &myUCont,
    uchArrayClose,
    uchArrayCount,
    uchArrayUNext,
    uenum_nextDefault,
    uchArrayReset
};

/********************************************************************/

static UEnumeration *getchArrayEnum(const char** source, int32_t size) {
    UEnumeration *en = (UEnumeration *)malloc(sizeof(UEnumeration));
    memcpy(en, &chEnum, sizeof(UEnumeration));
    cont->array = (char **)source;
    cont->maxIndex = size;
    return en;
}

static void EnumerationTest(void) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t len = 0;
    UEnumeration *en = getchArrayEnum(test1, UPRV_LENGTHOF(test1));
    const char *string = NULL;
    const UChar *uString = NULL;
    while ((string = uenum_next(en, &len, &status))) {
        log_verbose("read \"%s\", length %i\n", string, len);
    }
    uenum_reset(en, &status);
    while ((uString = uenum_unext(en, &len, &status))) {
        log_verbose("read \"%s\" (UChar), length %i\n", quikU2C(uString, len), len);
    }
    
    uenum_close(en);
}

static void EmptyEnumerationTest(void) {
    UErrorCode status = U_ZERO_ERROR;
    UEnumeration *emptyEnum = uprv_malloc(sizeof(UEnumeration));

    uprv_memcpy(emptyEnum, &emptyEnumerator, sizeof(UEnumeration));
    if (uenum_count(emptyEnum, &status) != -1 || status != U_UNSUPPORTED_ERROR) {
        log_err("uenum_count failed\n");
    }
    status = U_ZERO_ERROR;
    if (uenum_next(emptyEnum, NULL, &status) != NULL || status != U_UNSUPPORTED_ERROR) {
        log_err("uenum_next failed\n");
    }
    status = U_ZERO_ERROR;
    if (uenum_unext(emptyEnum, NULL, &status) != NULL || status != U_UNSUPPORTED_ERROR) {
        log_err("uenum_unext failed\n");
    }
    status = U_ZERO_ERROR;
    uenum_reset(emptyEnum, &status);
    if (status != U_UNSUPPORTED_ERROR) {
        log_err("uenum_reset failed\n");
    }
    uenum_close(emptyEnum);

    status = U_ZERO_ERROR;
    if (uenum_next(NULL, NULL, &status) != NULL || status != U_ZERO_ERROR) {
        log_err("uenum_next(NULL) failed\n");
    }
    status = U_ZERO_ERROR;
    if (uenum_unext(NULL, NULL, &status) != NULL || status != U_ZERO_ERROR) {
        log_err("uenum_unext(NULL) failed\n");
    }
    status = U_ZERO_ERROR;
    uenum_reset(NULL, &status);
    if (status != U_ZERO_ERROR) {
        log_err("uenum_reset(NULL) failed\n");
    }

    emptyEnum = uprv_malloc(sizeof(UEnumeration));
    uprv_memcpy(emptyEnum, &emptyPartialEnumerator, sizeof(UEnumeration));
    status = U_ZERO_ERROR;
    if (uenum_unext(emptyEnum, NULL, &status) != NULL || status != U_UNSUPPORTED_ERROR) {
        log_err("partial uenum_unext failed\n");
    }
    uenum_close(emptyEnum);
}

static UEnumeration *getuchArrayEnum(const UChar** source, int32_t size) {
    UEnumeration *en = (UEnumeration *)malloc(sizeof(UEnumeration));
    memcpy(en, &uchEnum, sizeof(UEnumeration));
    ucont->array = (UChar **)source;
    ucont->maxIndex = size;
    return en;
}

static void DefaultNextTest(void) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t len = 0;
    UEnumeration *en = getuchArrayEnum(test2, UPRV_LENGTHOF(test2));
    const char *string = NULL;
    const UChar *uString = NULL;
    while ((uString = uenum_unext(en, &len, &status))) {
        log_verbose("read \"%s\" (UChar), length %i\n", quikU2C(uString, len), len);
    }
    if (U_FAILURE(status)) {
        log_err("FAIL: uenum_unext => %s\n", u_errorName(status));
    }
    uenum_reset(en, &status);
    while ((string = uenum_next(en, &len, &status))) {
        log_verbose("read \"%s\", length %i\n", string, len);
    }
    if (U_FAILURE(status)) {
        log_err("FAIL: uenum_next => %s\n", u_errorName(status));
    }
    
    uenum_close(en);
}

static void verifyEnumeration(int line, UEnumeration *u, const char * const * compareToChar, const UChar * const * compareToUChar, int32_t expect_count) {
  UErrorCode status = U_ZERO_ERROR;
  int32_t got_count,i,len;
  const char *c;
  UChar buf[1024];

  log_verbose("%s:%d: verifying enumeration..\n", __FILE__, line);

  uenum_reset(u, &status);
  if(U_FAILURE(status)) {
    log_err("%s:%d: FAIL: could not reset char strings enumeration: %s\n", __FILE__, line, u_errorName(status));
    return;
  }

  got_count = uenum_count(u, &status);
  if(U_FAILURE(status)) {
    log_err("%s:%d: FAIL: could not count char strings enumeration: %s\n", __FILE__, line, u_errorName(status));
    return;
  }
  
  if(got_count!=expect_count) {
    log_err("%s:%d: FAIL: expect count %d got %d\n", __FILE__, line, expect_count, got_count);
  } else {
    log_verbose("%s:%d: OK: got count %d\n", __FILE__, line, got_count);
  }

  if(compareToChar!=NULL) { /* else, not invariant */
    for(i=0;i<got_count;i++) {
      c = uenum_next(u,&len, &status);
      if(U_FAILURE(status)) {
        log_err("%s:%d: FAIL: could not iterate to next after %d: %s\n", __FILE__, line, i, u_errorName(status));
        return;
      }
      if(c==NULL) {
        log_err("%s:%d: FAIL: got NULL for next after %d: %s\n", __FILE__, line, i, u_errorName(status));
        return;
      }
      
      if(strcmp(c,compareToChar[i])) {
        log_err("%s:%d: FAIL: string #%d expected '%s' got '%s'\n", __FILE__, line, i, compareToChar[i], c);
      } else {
        log_verbose("%s:%d: OK: string #%d got '%s'\n", __FILE__, line, i, c);
      }
      
      if(len!=strlen(compareToChar[i])) {
        log_err("%s:%d: FAIL: string #%d expected len %d got %d\n", __FILE__, line, i, strlen(compareToChar[i]), len);
      } else {
        log_verbose("%s:%d: OK: string #%d got len %d\n", __FILE__, line, i, len);
      }
    }
  }

  /* now try U */
  uenum_reset(u, &status);
  if(U_FAILURE(status)) {
    log_err("%s:%d: FAIL: could not reset again char strings enumeration: %s\n", __FILE__, line, u_errorName(status));
    return;
  }

  for(i=0;i<got_count;i++) {
    const UChar *ustr = uenum_unext(u,&len, &status);
    if(U_FAILURE(status)) {
      log_err("%s:%d: FAIL: could not iterate to unext after %d: %s\n", __FILE__, line, i, u_errorName(status));
      return;
    }
    if(ustr==NULL) {
      log_err("%s:%d: FAIL: got NULL for unext after %d: %s\n", __FILE__, line, i, u_errorName(status));
      return;
    }
    if(compareToChar!=NULL) {
      u_charsToUChars(compareToChar[i], buf, (int32_t)strlen(compareToChar[i])+1);
      if(u_strncmp(ustr,buf,len)) {
        int j;
        log_err("%s:%d: FAIL: ustring #%d expected '%s' got '%s'\n", __FILE__, line, i, compareToChar[i], austrdup(ustr));
        for(j=0;ustr[j]&&buf[j];j++) {
          log_verbose("  @ %d\t<U+%04X> vs <U+%04X>\n", j, ustr[j],buf[j]);
        }
      } else {
        log_verbose("%s:%d: OK: ustring #%d got '%s'\n", __FILE__, line, i, compareToChar[i]);
      }
      
      if(len!=strlen(compareToChar[i])) {
        log_err("%s:%d: FAIL: ustring #%d expected len %d got %d\n", __FILE__, line, i, strlen(compareToChar[i]), len);
      } else {
        log_verbose("%s:%d: OK: ustring #%d got len %d\n", __FILE__, line, i, len);
      }
    }

    if(compareToUChar!=NULL) {
      if(u_strcmp(ustr,compareToUChar[i])) {
        int j;
        log_err("%s:%d: FAIL: ustring #%d expected '%s' got '%s'\n", __FILE__, line, i, austrdup(compareToUChar[i]), austrdup(ustr));
        for(j=0;ustr[j]&&compareToUChar[j];j++) {
          log_verbose("  @ %d\t<U+%04X> vs <U+%04X>\n", j, ustr[j],compareToUChar[j]);
        }
      } else {
        log_verbose("%s:%d: OK: ustring #%d got '%s'\n", __FILE__, line, i, austrdup(compareToUChar[i]));
      }
      
      if(len!=u_strlen(compareToUChar[i])) {
        log_err("%s:%d: FAIL: ustring #%d expected len %d got %d\n", __FILE__, line, i, u_strlen(compareToUChar[i]), len);
      } else {
        log_verbose("%s:%d: OK: ustring #%d got len %d\n", __FILE__, line, i, len);
      }
    }
  }
}





static void TestCharStringsEnumeration(void)  {
  UErrorCode status = U_ZERO_ERROR;

  /* //! [uenum_openCharStringsEnumeration] */
  const char* strings[] = { "Firstly", "Secondly", "Thirdly", "Fourthly" };
  UEnumeration *u = uenum_openCharStringsEnumeration(strings, 4, &status);
  /* //! [uenum_openCharStringsEnumeration] */
  if(U_FAILURE(status)) {
    log_err("FAIL: could not open char strings enumeration: %s\n", u_errorName(status));
    return;
  }

  verifyEnumeration(__LINE__, u, strings, NULL, 4);

  uenum_close(u);
}

static void TestUCharStringsEnumeration(void)  {
  UErrorCode status = U_ZERO_ERROR;
  /* //! [uenum_openUCharStringsEnumeration] */
  static const UChar nko_1[] = {0x07c1,0}, nko_2[] = {0x07c2,0}, nko_3[] = {0x07c3,0}, nko_4[] = {0x07c4,0};
  static const UChar* ustrings[] = {  nko_1, nko_2, nko_3, nko_4  };
  UEnumeration *u = uenum_openUCharStringsEnumeration(ustrings, 4, &status);
  /* //! [uenum_openUCharStringsEnumeration] */
  if(U_FAILURE(status)) {
    log_err("FAIL: could not open uchar strings enumeration: %s\n", u_errorName(status));
    return;
  }

  verifyEnumeration(__LINE__, u, NULL, ustrings, 4);
  uenum_close(u);


  u =  uenum_openUCharStringsEnumeration(test2, 4, &status);
  if(U_FAILURE(status)) {
    log_err("FAIL: could not reopen uchar strings enumeration: %s\n", u_errorName(status));
    return;
  }
  verifyEnumeration(__LINE__, u, test1, NULL, 4); /* same string */
  uenum_close(u);
  
}

void addEnumerationTest(TestNode** root);

void addEnumerationTest(TestNode** root)
{
    addTest(root, &EnumerationTest, "tsutil/uenumtst/EnumerationTest");
    addTest(root, &EmptyEnumerationTest, "tsutil/uenumtst/EmptyEnumerationTest");
    addTest(root, &DefaultNextTest, "tsutil/uenumtst/DefaultNextTest");
    addTest(root, &TestCharStringsEnumeration, "tsutil/uenumtst/TestCharStringsEnumeration");
    addTest(root, &TestUCharStringsEnumeration, "tsutil/uenumtst/TestUCharStringsEnumeration");
}
