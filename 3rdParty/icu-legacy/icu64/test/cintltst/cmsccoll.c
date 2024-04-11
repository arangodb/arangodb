// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2001-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/*******************************************************************************
*
* File cmsccoll.C
*
*******************************************************************************/
/**
 * These are the tests specific to ICU 1.8 and above, that I didn't know where
 * to fit.
 */

#include <stdbool.h>
#include <stdio.h>

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/ucol.h"
#include "unicode/ucoleitr.h"
#include "unicode/uloc.h"
#include "cintltst.h"
#include "ccolltst.h"
#include "callcoll.h"
#include "unicode/ustring.h"
#include "string.h"
#include "ucol_imp.h"
#include "cmemory.h"
#include "cstring.h"
#include "uassert.h"
#include "unicode/parseerr.h"
#include "unicode/ucnv.h"
#include "unicode/ures.h"
#include "unicode/uscript.h"
#include "unicode/utf16.h"
#include "uparse.h"
#include "putilimp.h"


#define MAX_TOKEN_LEN 16

typedef UCollationResult tst_strcoll(void *collator, const int object,
                        const UChar *source, const int sLen,
                        const UChar *target, const int tLen);



const static char cnt1[][10] = {

  "AA",
  "AC",
  "AZ",
  "AQ",
  "AB",
  "ABZ",
  "ABQ",
  "Z",
  "ABC",
  "Q",
  "B"
};

const static char cnt2[][10] = {
  "DA",
  "DAD",
  "DAZ",
  "MAR",
  "Z",
  "DAVIS",
  "MARK",
  "DAV",
  "DAVI"
};

static void IncompleteCntTest(void)
{
  UErrorCode status = U_ZERO_ERROR;
  UChar temp[90];
  UChar t1[90];
  UChar t2[90];

  UCollator *coll =  NULL;
  uint32_t i = 0, j = 0;
  uint32_t size = 0;

  u_uastrcpy(temp, " & Z < ABC < Q < B");

  coll = ucol_openRules(temp, u_strlen(temp), UCOL_OFF, UCOL_DEFAULT_STRENGTH, NULL,&status);

  if(U_SUCCESS(status)) {
    size = UPRV_LENGTHOF(cnt1);
    for(i = 0; i < size-1; i++) {
      for(j = i+1; j < size; j++) {
        UCollationElements *iter;
        u_uastrcpy(t1, cnt1[i]);
        u_uastrcpy(t2, cnt1[j]);
        doTest(coll, t1, t2, UCOL_LESS);
        /* synwee : added collation element iterator test */
        iter = ucol_openElements(coll, t2, u_strlen(t2), &status);
        if (U_FAILURE(status)) {
          log_err("Creation of iterator failed\n");
          break;
        }
        backAndForth(iter);
        ucol_closeElements(iter);
      }
    }
  }

  ucol_close(coll);


  u_uastrcpy(temp, " & Z < DAVIS < MARK <DAV");
  coll = ucol_openRules(temp, u_strlen(temp), UCOL_OFF, UCOL_DEFAULT_STRENGTH,NULL, &status);

  if(U_SUCCESS(status)) {
    size = UPRV_LENGTHOF(cnt2);
    for(i = 0; i < size-1; i++) {
      for(j = i+1; j < size; j++) {
        UCollationElements *iter;
        u_uastrcpy(t1, cnt2[i]);
        u_uastrcpy(t2, cnt2[j]);
        doTest(coll, t1, t2, UCOL_LESS);

        /* synwee : added collation element iterator test */
        iter = ucol_openElements(coll, t2, u_strlen(t2), &status);
        if (U_FAILURE(status)) {
          log_err("Creation of iterator failed\n");
          break;
        }
        backAndForth(iter);
        ucol_closeElements(iter);
      }
    }
  }

  ucol_close(coll);


}

const static char shifted[][20] = {
  "black bird",
  "black-bird",
  "blackbird",
  "black Bird",
  "black-Bird",
  "blackBird",
  "black birds",
  "black-birds",
  "blackbirds"
};

const static UCollationResult shiftedTert[] = {
  UCOL_EQUAL,
  UCOL_EQUAL,
  UCOL_EQUAL,
  UCOL_LESS,
  UCOL_EQUAL,
  UCOL_EQUAL,
  UCOL_LESS,
  UCOL_EQUAL,
  UCOL_EQUAL
};

const static char nonignorable[][20] = {
  "black bird",
  "black Bird",
  "black birds",
  "black-bird",
  "black-Bird",
  "black-birds",
  "blackbird",
  "blackBird",
  "blackbirds"
};

static void BlackBirdTest(void) {
  UErrorCode status = U_ZERO_ERROR;
  UChar t1[90];
  UChar t2[90];

  uint32_t i = 0, j = 0;
  uint32_t size = 0;
  UCollator *coll = ucol_open("en_US", &status);

  ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_OFF, &status);
  ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_NON_IGNORABLE, &status);

  if(U_SUCCESS(status)) {
    size = UPRV_LENGTHOF(nonignorable);
    for(i = 0; i < size-1; i++) {
      for(j = i+1; j < size; j++) {
        u_uastrcpy(t1, nonignorable[i]);
        u_uastrcpy(t2, nonignorable[j]);
        doTest(coll, t1, t2, UCOL_LESS);
      }
    }
  }

  ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, &status);
  ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_QUATERNARY, &status);

  if(U_SUCCESS(status)) {
    size = UPRV_LENGTHOF(shifted);
    for(i = 0; i < size-1; i++) {
      for(j = i+1; j < size; j++) {
        u_uastrcpy(t1, shifted[i]);
        u_uastrcpy(t2, shifted[j]);
        doTest(coll, t1, t2, UCOL_LESS);
      }
    }
  }

  ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_TERTIARY, &status);
  if(U_SUCCESS(status)) {
    size = UPRV_LENGTHOF(shifted);
    for(i = 1; i < size; i++) {
      u_uastrcpy(t1, shifted[i-1]);
      u_uastrcpy(t2, shifted[i]);
      doTest(coll, t1, t2, shiftedTert[i]);
    }
  }

  ucol_close(coll);
}

const static UChar testSourceCases[][MAX_TOKEN_LEN] = {
    {0x0041/*'A'*/, 0x0300, 0x0301, 0x0000},
    {0x0041/*'A'*/, 0x0300, 0x0316, 0x0000},
    {0x0041/*'A'*/, 0x0300, 0x0000},
    {0x00C0, 0x0301, 0x0000},
    /* this would work with forced normalization */
    {0x00C0, 0x0316, 0x0000}
};

const static UChar testTargetCases[][MAX_TOKEN_LEN] = {
    {0x0041/*'A'*/, 0x0301, 0x0300, 0x0000},
    {0x0041/*'A'*/, 0x0316, 0x0300, 0x0000},
    {0x00C0, 0},
    {0x0041/*'A'*/, 0x0301, 0x0300, 0x0000},
    /* this would work with forced normalization */
    {0x0041/*'A'*/, 0x0316, 0x0300, 0x0000}
};

const static UCollationResult results[] = {
    UCOL_GREATER,
    UCOL_EQUAL,
    UCOL_EQUAL,
    UCOL_GREATER,
    UCOL_EQUAL
};

static void FunkyATest(void)
{

    int32_t i;
    UErrorCode status = U_ZERO_ERROR;
    UCollator  *myCollation;
    myCollation = ucol_open("en_US", &status);
    if(U_FAILURE(status)){
        log_err_status(status, "ERROR: in creation of rule based collator: %s\n", myErrorName(status));
        return;
    }
    log_verbose("Testing some A letters, for some reason\n");
    ucol_setAttribute(myCollation, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    ucol_setStrength(myCollation, UCOL_TERTIARY);
    for (i = 0; i < 4 ; i++)
    {
        doTest(myCollation, testSourceCases[i], testTargetCases[i], results[i]);
    }
    ucol_close(myCollation);
}

UColAttributeValue caseFirst[] = {
    UCOL_OFF,
    UCOL_LOWER_FIRST,
    UCOL_UPPER_FIRST
};


UColAttributeValue alternateHandling[] = {
    UCOL_NON_IGNORABLE,
    UCOL_SHIFTED
};

UColAttributeValue caseLevel[] = {
    UCOL_OFF,
    UCOL_ON
};

UColAttributeValue strengths[] = {
    UCOL_PRIMARY,
    UCOL_SECONDARY,
    UCOL_TERTIARY,
    UCOL_QUATERNARY,
    UCOL_IDENTICAL
};

#if 0
static const char * strengthsC[] = {
    "UCOL_PRIMARY",
    "UCOL_SECONDARY",
    "UCOL_TERTIARY",
    "UCOL_QUATERNARY",
    "UCOL_IDENTICAL"
};

static const char * caseFirstC[] = {
    "UCOL_OFF",
    "UCOL_LOWER_FIRST",
    "UCOL_UPPER_FIRST"
};


static const char * alternateHandlingC[] = {
    "UCOL_NON_IGNORABLE",
    "UCOL_SHIFTED"
};

static const char * caseLevelC[] = {
    "UCOL_OFF",
    "UCOL_ON"
};

/* not used currently - does not test only prints */
static void PrintMarkDavis(void)
{
  UErrorCode status = U_ZERO_ERROR;
  UChar m[256];
  uint8_t sortkey[256];
  UCollator *coll = ucol_open("en_US", &status);
  uint32_t h,i,j,k, sortkeysize;
  uint32_t sizem = 0;
  char buffer[512];
  uint32_t len = 512;

  log_verbose("PrintMarkDavis");

  u_uastrcpy(m, "Mark Davis");
  sizem = u_strlen(m);


  m[1] = 0xe4;

  for(i = 0; i<sizem; i++) {
    fprintf(stderr, "\\u%04X ", m[i]);
  }
  fprintf(stderr, "\n");

  for(h = 0; h<UPRV_LENGTHOF(caseFirst); h++) {
    ucol_setAttribute(coll, UCOL_CASE_FIRST, caseFirst[i], &status);
    fprintf(stderr, "caseFirst: %s\n", caseFirstC[h]);

    for(i = 0; i<UPRV_LENGTHOF(alternateHandling); i++) {
      ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, alternateHandling[i], &status);
      fprintf(stderr, "  AltHandling: %s\n", alternateHandlingC[i]);

      for(j = 0; j<UPRV_LENGTHOF(caseLevel); j++) {
        ucol_setAttribute(coll, UCOL_CASE_LEVEL, caseLevel[j], &status);
        fprintf(stderr, "    caseLevel: %s\n", caseLevelC[j]);

        for(k = 0; k<UPRV_LENGTHOF(strengths); k++) {
          ucol_setAttribute(coll, UCOL_STRENGTH, strengths[k], &status);
          sortkeysize = ucol_getSortKey(coll, m, sizem, sortkey, 256);
          fprintf(stderr, "      strength: %s\n      Sortkey: ", strengthsC[k]);
          fprintf(stderr, "%s\n", ucol_sortKeyToString(coll, sortkey, buffer, &len));
        }

      }

    }

  }
}
#endif

static void BillFairmanTest(void) {
/*
** check for actual locale via ICU resource bundles
**
** lp points to the original locale ("fr_FR_....")
*/

    UResourceBundle *lr,*cr;
    UErrorCode              lec = U_ZERO_ERROR;
    const char *lp = "fr_FR_you_ll_never_find_this_locale";

    log_verbose("BillFairmanTest\n");

    lr = ures_open(NULL,lp,&lec);
    if (lr) {
        cr = ures_getByKey(lr,"collations",0,&lec);
        if (cr) {
            lp = ures_getLocaleByType(cr, ULOC_ACTUAL_LOCALE, &lec);
            if (lp) {
                if (U_SUCCESS(lec)) {
                    if(strcmp(lp, "fr") != 0) {
                        log_err("Wrong locale for French Collation Data, expected \"fr\" got %s", lp);
                    }
                }
            }
            ures_close(cr);
        }
        ures_close(lr);
    }
}

const static char chTest[][20] = {
  "c",
  "C",
  "ca", "cb", "cx", "cy", "CZ",
  "c\\u030C", "C\\u030C",
  "h",
  "H",
  "ha", "Ha", "harly", "hb", "HB", "hx", "HX", "hy", "HY",
  "ch", "cH", "Ch", "CH",
  "cha", "charly", "che", "chh", "chch", "chr",
  "i", "I", "iarly",
  "r", "R",
  "r\\u030C", "R\\u030C",
  "s",
  "S",
  "s\\u030C", "S\\u030C",
  "z", "Z",
  "z\\u030C", "Z\\u030C"
};

static void TestChMove(void) {
    UChar t1[256] = {0};
    UChar t2[256] = {0};

    uint32_t i = 0, j = 0;
    uint32_t size = 0;
    UErrorCode status = U_ZERO_ERROR;

    UCollator *coll = ucol_open("cs", &status);

    if(U_SUCCESS(status)) {
        size = UPRV_LENGTHOF(chTest);
        for(i = 0; i < size-1; i++) {
            for(j = i+1; j < size; j++) {
                u_unescape(chTest[i], t1, 256);
                u_unescape(chTest[j], t2, 256);
                doTest(coll, t1, t2, UCOL_LESS);
            }
        }
    }
    else {
        log_data_err("Can't open collator");
    }
    ucol_close(coll);
}




/*
const static char impTest[][20] = {
  "\\u4e00",
    "a",
    "A",
    "b",
    "B",
    "\\u4e01"
};
*/


static void TestImplicitTailoring(void) {
  static const struct {
    const char *rules;
    const char *data[10];
    const uint32_t len;
  } tests[] = {
      {
        /* Tailor b and c before U+4E00. */
        "&[before 1]\\u4e00 < b < c "
        /* Now, before U+4E00 is c; put d and e after that. */
        "&[before 1]\\u4e00 < d < e",
        { "b", "c", "d", "e", "\\u4e00"}, 5 },
      { "&\\u4e00 < a <<< A < b <<< B",   { "\\u4e00", "a", "A", "b", "B", "\\u4e01"}, 6 },
      { "&[before 1]\\u4e00 < \\u4e01 < \\u4e02", { "\\u4e01", "\\u4e02", "\\u4e00"}, 3},
      { "&[before 1]\\u4e01 < \\u4e02 < \\u4e03", { "\\u4e02", "\\u4e03", "\\u4e01"}, 3}
  };

  int32_t i = 0;

  for(i = 0; i < UPRV_LENGTHOF(tests); i++) {
      genericRulesStarter(tests[i].rules, tests[i].data, tests[i].len);
  }

/*
  UChar t1[256] = {0};
  UChar t2[256] = {0};

  const char *rule = "&\\u4e00 < a <<< A < b <<< B";

  uint32_t i = 0, j = 0;
  uint32_t size = 0;
  uint32_t ruleLen = 0;
  UErrorCode status = U_ZERO_ERROR;
  UCollator *coll = NULL;
  ruleLen = u_unescape(rule, t1, 256);

  coll = ucol_openRules(t1, ruleLen, UCOL_OFF, UCOL_TERTIARY,NULL, &status);

  if(U_SUCCESS(status)) {
    size = UPRV_LENGTHOF(impTest);
    for(i = 0; i < size-1; i++) {
      for(j = i+1; j < size; j++) {
        u_unescape(impTest[i], t1, 256);
        u_unescape(impTest[j], t2, 256);
        doTest(coll, t1, t2, UCOL_LESS);
      }
    }
  }
  else {
    log_err("Can't open collator");
  }
  ucol_close(coll);
  */
}

static void TestFCDProblem(void) {
  UChar t1[256] = {0};
  UChar t2[256] = {0};

  const char *s1 = "\\u0430\\u0306\\u0325";
  const char *s2 = "\\u04D1\\u0325";

  UErrorCode status = U_ZERO_ERROR;
  UCollator *coll = ucol_open("", &status);
  u_unescape(s1, t1, 256);
  u_unescape(s2, t2, 256);

  ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_OFF, &status);
  doTest(coll, t1, t2, UCOL_EQUAL);

  ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
  doTest(coll, t1, t2, UCOL_EQUAL);

  ucol_close(coll);
}

/*
The largest normalization form is 18 for NFKC/NFKD, 4 for NFD and 3 for NFC
We're only using NFC/NFD in this test.
*/
#define NORM_BUFFER_TEST_LEN 18
typedef struct {
  UChar32 u;
  UChar NFC[NORM_BUFFER_TEST_LEN];
  UChar NFD[NORM_BUFFER_TEST_LEN];
} tester;

static void TestComposeDecompose(void) {
    /* [[:NFD_Inert=false:][:NFC_Inert=false:]] */
    static const UChar UNICODESET_STR[] = {
        0x5B,0x5B,0x3A,0x4E,0x46,0x44,0x5F,0x49,0x6E,0x65,0x72,0x74,0x3D,0x66,0x61,
        0x6C,0x73,0x65,0x3A,0x5D,0x5B,0x3A,0x4E,0x46,0x43,0x5F,0x49,0x6E,0x65,0x72,
        0x74,0x3D,0x66,0x61,0x6C,0x73,0x65,0x3A,0x5D,0x5D,0
    };
    int32_t noOfLoc;
    int32_t i = 0, j = 0;

    UErrorCode status = U_ZERO_ERROR;
    const char *locName = NULL;
    uint32_t nfcSize;
    uint32_t nfdSize;
    tester **t;
    uint32_t noCases = 0;
    UCollator *coll = NULL;
    UChar32 u = 0;
    UChar comp[NORM_BUFFER_TEST_LEN];
    uint32_t len = 0;
    UCollationElements *iter;
    USet *charsToTest = uset_openPattern(UNICODESET_STR, -1, &status);
    int32_t charsToTestSize;

    noOfLoc = uloc_countAvailable();

    coll = ucol_open("", &status);
    if (U_FAILURE(status)) {
        log_data_err("Error opening collator -> %s (Are you missing data?)\n", u_errorName(status));
        uset_close(charsToTest);
        return;
    }
    charsToTestSize = uset_size(charsToTest);
    if (charsToTestSize <= 0) {
        log_err("Set was zero. Missing data?\n");
        uset_close(charsToTest);
        return;
    }
    t = (tester **)malloc(charsToTestSize * sizeof(tester *));
    t[0] = (tester *)malloc(sizeof(tester));
    log_verbose("Testing UCA extensively for %d characters\n", charsToTestSize);

    for(u = 0; u < charsToTestSize; u++) {
        UChar32 ch = uset_charAt(charsToTest, u);
        len = 0;
        U16_APPEND_UNSAFE(comp, len, ch);
        nfcSize = unorm_normalize(comp, len, UNORM_NFC, 0, t[noCases]->NFC, NORM_BUFFER_TEST_LEN, &status);
        nfdSize = unorm_normalize(comp, len, UNORM_NFD, 0, t[noCases]->NFD, NORM_BUFFER_TEST_LEN, &status);

        if(nfcSize != nfdSize || (uprv_memcmp(t[noCases]->NFC, t[noCases]->NFD, nfcSize * sizeof(UChar)) != 0)
          || (len != nfdSize || (uprv_memcmp(comp, t[noCases]->NFD, nfdSize * sizeof(UChar)) != 0))) {
            t[noCases]->u = ch;
            if(len != nfdSize || (uprv_memcmp(comp, t[noCases]->NFD, nfdSize * sizeof(UChar)) != 0)) {
                u_strncpy(t[noCases]->NFC, comp, len);
                t[noCases]->NFC[len] = 0;
            }
            noCases++;
            t[noCases] = (tester *)malloc(sizeof(tester));
            uprv_memset(t[noCases], 0, sizeof(tester));
        }
    }
    log_verbose("Testing %d/%d of possible test cases\n", noCases, charsToTestSize);
    uset_close(charsToTest);
    charsToTest = NULL;

    for(u=0; u<(UChar32)noCases; u++) {
        if(!ucol_equal(coll, t[u]->NFC, -1, t[u]->NFD, -1)) {
            log_err("Failure: codePoint %05X fails TestComposeDecompose in the UCA\n", t[u]->u);
            doTest(coll, t[u]->NFC, t[u]->NFD, UCOL_EQUAL);
        }
    }
    /*
    for(u = 0; u < charsToTestSize; u++) {
      if(!(u&0xFFFF)) {
        log_verbose("%08X ", u);
      }
      uprv_memset(t[noCases], 0, sizeof(tester));
      t[noCases]->u = u;
      len = 0;
      U16_APPEND_UNSAFE(comp, len, u);
      comp[len] = 0;
      nfcSize = unorm_normalize(comp, len, UNORM_NFC, 0, t[noCases]->NFC, NORM_BUFFER_TEST_LEN, &status);
      nfdSize = unorm_normalize(comp, len, UNORM_NFD, 0, t[noCases]->NFD, NORM_BUFFER_TEST_LEN, &status);
      doTest(coll, comp, t[noCases]->NFD, UCOL_EQUAL);
      doTest(coll, comp, t[noCases]->NFC, UCOL_EQUAL);
    }
    */

    ucol_close(coll);

    log_verbose("Testing locales, number of cases = %i\n", noCases);
    for(i = 0; i<noOfLoc; i++) {
        status = U_ZERO_ERROR;
        locName = uloc_getAvailable(i);
        if(hasCollationElements(locName)) {
            char cName[256];
            UChar name[256];
            int32_t nameSize = uloc_getDisplayName(locName, NULL, name, sizeof(cName), &status);

            for(j = 0; j<nameSize; j++) {
                cName[j] = (char)name[j];
            }
            cName[nameSize] = 0;
            log_verbose("\nTesting locale %s (%s)\n", locName, cName);

            coll = ucol_open(locName, &status);
            ucol_setStrength(coll, UCOL_IDENTICAL);
            iter = ucol_openElements(coll, t[u]->NFD, u_strlen(t[u]->NFD), &status);

            for(u=0; u<(UChar32)noCases; u++) {
                if(!ucol_equal(coll, t[u]->NFC, -1, t[u]->NFD, -1)) {
                    log_err("Failure: codePoint %05X fails TestComposeDecompose for locale %s\n", t[u]->u, cName);
                    doTest(coll, t[u]->NFC, t[u]->NFD, UCOL_EQUAL);
                    log_verbose("Testing NFC\n");
                    ucol_setText(iter, t[u]->NFC, u_strlen(t[u]->NFC), &status);
                    backAndForth(iter);
                    log_verbose("Testing NFD\n");
                    ucol_setText(iter, t[u]->NFD, u_strlen(t[u]->NFD), &status);
                    backAndForth(iter);
                }
            }
            ucol_closeElements(iter);
            ucol_close(coll);
        }
    }
    for(u = 0; u <= (UChar32)noCases; u++) {
        free(t[u]);
    }
    free(t);
}

static void TestEmptyRule(void) {
  UErrorCode status = U_ZERO_ERROR;
  UChar rulez[] = { 0 };
  UCollator *coll = ucol_openRules(rulez, 0, UCOL_OFF, UCOL_TERTIARY,NULL, &status);

  ucol_close(coll);
}

static void TestUCARules(void) {
  UErrorCode status = U_ZERO_ERROR;
  UChar b[256];
  UChar *rules = b;
  uint32_t ruleLen = 0;
  UCollator *UCAfromRules = NULL;
  UCollator *coll = ucol_open("", &status);
  if(status == U_FILE_ACCESS_ERROR) {
    log_data_err("Is your data around?\n");
    return;
  } else if(U_FAILURE(status)) {
    log_err("Error opening collator\n");
    return;
  }
  ruleLen = ucol_getRulesEx(coll, UCOL_FULL_RULES, rules, 256);

  log_verbose("TestUCARules\n");
  if(ruleLen > 256) {
    rules = (UChar *)malloc((ruleLen+1)*sizeof(UChar));
    ruleLen = ucol_getRulesEx(coll, UCOL_FULL_RULES, rules, ruleLen);
  }
  log_verbose("Rules length is %d\n", ruleLen);
  UCAfromRules = ucol_openRules(rules, ruleLen, UCOL_OFF, UCOL_TERTIARY, NULL,&status);
  if(U_SUCCESS(status)) {
    ucol_close(UCAfromRules);
  } else {
    log_verbose("Unable to create a collator from UCARules!\n");
  }
/*
  u_unescape(blah, b, 256);
  ucol_getSortKey(coll, b, 1, res, 256);
*/
  ucol_close(coll);
  if(rules != b) {
    free(rules);
  }
}


/* Pinyin tonal order */
/*
    A < .. (\u0101) < .. (\u00e1) < .. (\u01ce) < .. (\u00e0)
          (w/macron)<  (w/acute)<   (w/caron)<   (w/grave)
    E < .. (\u0113) < .. (\u00e9) < .. (\u011b) < .. (\u00e8)
    I < .. (\u012b) < .. (\u00ed) < .. (\u01d0) < .. (\u00ec)
    O < .. (\u014d) < .. (\u00f3) < .. (\u01d2) < .. (\u00f2)
    U < .. (\u016b) < .. (\u00fa) < .. (\u01d4) < .. (\u00f9)
      < .. (\u01d6) < .. (\u01d8) < .. (\u01da) < .. (\u01dc) <
.. (\u00fc)

However, in testing we got the following order:
    A < .. (\u00e1) < .. (\u00e0) < .. (\u01ce) < .. (\u0101)
          (w/acute)<   (w/grave)<   (w/caron)<   (w/macron)
    E < .. (\u00e9) < .. (\u00e8) < .. (\u00ea) < .. (\u011b) <
.. (\u0113)
    I < .. (\u00ed) < .. (\u00ec) < .. (\u01d0) < .. (\u012b)
    O < .. (\u00f3) < .. (\u00f2) < .. (\u01d2) < .. (\u014d)
    U < .. (\u00fa) < .. (\u00f9) < .. (\u01d4) < .. (\u00fc) <
.. (\u01d8)
      < .. (\u01dc) < .. (\u01da) < .. (\u01d6) < .. (\u016b)
*/

static void TestBefore(void) {
  const static char *data[] = {
      "\\u0101", "\\u00e1", "\\u01ce", "\\u00e0", "A",
      "\\u0113", "\\u00e9", "\\u011b", "\\u00e8", "E",
      "\\u012b", "\\u00ed", "\\u01d0", "\\u00ec", "I",
      "\\u014d", "\\u00f3", "\\u01d2", "\\u00f2", "O",
      "\\u016b", "\\u00fa", "\\u01d4", "\\u00f9", "U",
      "\\u01d6", "\\u01d8", "\\u01da", "\\u01dc", "\\u00fc"
  };
  genericRulesStarter(
    "&[before 1]a<\\u0101<\\u00e1<\\u01ce<\\u00e0"
    "&[before 1]e<\\u0113<\\u00e9<\\u011b<\\u00e8"
    "&[before 1]i<\\u012b<\\u00ed<\\u01d0<\\u00ec"
    "&[before 1]o<\\u014d<\\u00f3<\\u01d2<\\u00f2"
    "&[before 1]u<\\u016b<\\u00fa<\\u01d4<\\u00f9"
    "&u<\\u01d6<\\u01d8<\\u01da<\\u01dc<\\u00fc",
    data, UPRV_LENGTHOF(data));
}

#if 0
/* superceded by TestBeforePinyin */
static void TestJ784(void) {
  const static char *data[] = {
      "A", "\\u0101", "\\u00e1", "\\u01ce", "\\u00e0",
      "E", "\\u0113", "\\u00e9", "\\u011b", "\\u00e8",
      "I", "\\u012b", "\\u00ed", "\\u01d0", "\\u00ec",
      "O", "\\u014d", "\\u00f3", "\\u01d2", "\\u00f2",
      "U", "\\u016b", "\\u00fa", "\\u01d4", "\\u00f9",
      "\\u00fc",
           "\\u01d6", "\\u01d8", "\\u01da", "\\u01dc"
  };
  genericLocaleStarter("zh", data, UPRV_LENGTHOF(data));
}
#endif

static void TestUpperCaseFirst(void) {
  const static char *data[] = {
    "I",
      "i",
      "Y",
      "y"
  };
  genericLocaleStarter("da", data, UPRV_LENGTHOF(data));
}

static void TestJ815(void) {
  const static char *data[] = {
    "aa",
      "Aa",
      "ab",
      "Ab",
      "ad",
      "Ad",
      "ae",
      "Ae",
      "\\u00e6",
      "\\u00c6",
      "af",
      "Af",
      "b",
      "B"
  };
  genericLocaleStarter("fr", data, UPRV_LENGTHOF(data));
  genericRulesStarter("[backwards 2]&A<<\\u00e6/e<<<\\u00c6/E", data, UPRV_LENGTHOF(data));
}


static void TestCase(void)
{
    const static UChar gRules[MAX_TOKEN_LEN] =
    /*" & 0 < 1,\u2461<a,A"*/
    { 0x0026, 0x0030, 0x003C, 0x0031, 0x002C, 0x2460, 0x003C, 0x0061, 0x002C, 0x0041, 0x0000 };

    const static UChar testCase[][MAX_TOKEN_LEN] =
    {
        /*0*/ {0x0031 /*'1'*/, 0x0061/*'a'*/, 0x0000},
        /*1*/ {0x0031 /*'1'*/, 0x0041/*'A'*/, 0x0000},
        /*2*/ {0x2460 /*circ'1'*/, 0x0061/*'a'*/, 0x0000},
        /*3*/ {0x2460 /*circ'1'*/, 0x0041/*'A'*/, 0x0000}
    };

    const static UCollationResult caseTestResults[][9] =
    {
        { UCOL_LESS,    UCOL_LESS, UCOL_LESS,    UCOL_EQUAL, UCOL_LESS,    UCOL_LESS, UCOL_EQUAL, UCOL_EQUAL, UCOL_LESS },
        { UCOL_GREATER, UCOL_LESS, UCOL_LESS,    UCOL_EQUAL, UCOL_LESS,    UCOL_LESS, UCOL_EQUAL, UCOL_EQUAL, UCOL_GREATER },
        { UCOL_LESS,    UCOL_LESS, UCOL_LESS,    UCOL_EQUAL, UCOL_GREATER, UCOL_LESS, UCOL_EQUAL, UCOL_EQUAL, UCOL_LESS },
        { UCOL_GREATER, UCOL_LESS, UCOL_GREATER, UCOL_EQUAL, UCOL_LESS,    UCOL_LESS, UCOL_EQUAL, UCOL_EQUAL, UCOL_GREATER }
    };

    const static UColAttributeValue caseTestAttributes[][2] =
    {
        { UCOL_LOWER_FIRST, UCOL_OFF},
        { UCOL_UPPER_FIRST, UCOL_OFF},
        { UCOL_LOWER_FIRST, UCOL_ON},
        { UCOL_UPPER_FIRST, UCOL_ON}
    };
    int32_t i,j,k;
    UErrorCode status = U_ZERO_ERROR;
    UCollationElements *iter;
    UCollator  *myCollation;
    myCollation = ucol_open("en_US", &status);

    if(U_FAILURE(status)){
        log_err_status(status, "ERROR: in creation of rule based collator: %s\n", myErrorName(status));
        return;
    }
    log_verbose("Testing different case settings\n");
    ucol_setStrength(myCollation, UCOL_TERTIARY);

    for(k = 0; k<4; k++) {
      ucol_setAttribute(myCollation, UCOL_CASE_FIRST, caseTestAttributes[k][0], &status);
      ucol_setAttribute(myCollation, UCOL_CASE_LEVEL, caseTestAttributes[k][1], &status);
      log_verbose("Case first = %d, Case level = %d\n", caseTestAttributes[k][0], caseTestAttributes[k][1]);
      for (i = 0; i < 3 ; i++) {
        for(j = i+1; j<4; j++) {
          doTest(myCollation, testCase[i], testCase[j], caseTestResults[k][3*i+j-1]);
        }
      }
    }
    ucol_close(myCollation);

    myCollation = ucol_openRules(gRules, u_strlen(gRules), UCOL_OFF, UCOL_TERTIARY,NULL, &status);
    if(U_FAILURE(status)){
        log_err("ERROR: in creation of rule based collator: %s\n", myErrorName(status));
        return;
    }
    log_verbose("Testing different case settings with custom rules\n");
    ucol_setStrength(myCollation, UCOL_TERTIARY);

    for(k = 0; k<4; k++) {
      ucol_setAttribute(myCollation, UCOL_CASE_FIRST, caseTestAttributes[k][0], &status);
      ucol_setAttribute(myCollation, UCOL_CASE_LEVEL, caseTestAttributes[k][1], &status);
      for (i = 0; i < 3 ; i++) {
        for(j = i+1; j<4; j++) {
          log_verbose("k:%d, i:%d, j:%d\n", k, i, j);
          doTest(myCollation, testCase[i], testCase[j], caseTestResults[k][3*i+j-1]);
          iter=ucol_openElements(myCollation, testCase[i], u_strlen(testCase[i]), &status);
          backAndForth(iter);
          ucol_closeElements(iter);
          iter=ucol_openElements(myCollation, testCase[j], u_strlen(testCase[j]), &status);
          backAndForth(iter);
          ucol_closeElements(iter);
        }
      }
    }
    ucol_close(myCollation);
    {
      const static char *lowerFirst[] = {
        "h",
        "H",
        "ch",
        "Ch",
        "CH",
        "cha",
        "chA",
        "Cha",
        "ChA",
        "CHa",
        "CHA",
        "i",
        "I"
      };

      const static char *upperFirst[] = {
        "H",
        "h",
        "CH",
        "Ch",
        "ch",
        "CHA",
        "CHa",
        "ChA",
        "Cha",
        "chA",
        "cha",
        "I",
        "i"
      };
      log_verbose("mixed case test\n");
      log_verbose("lower first, case level off\n");
      genericRulesStarter("[caseFirst lower]&H<ch<<<Ch<<<CH", lowerFirst, UPRV_LENGTHOF(lowerFirst));
      log_verbose("upper first, case level off\n");
      genericRulesStarter("[caseFirst upper]&H<ch<<<Ch<<<CH", upperFirst, UPRV_LENGTHOF(upperFirst));
      log_verbose("lower first, case level on\n");
      genericRulesStarter("[caseFirst lower][caseLevel on]&H<ch<<<Ch<<<CH", lowerFirst, UPRV_LENGTHOF(lowerFirst));
      log_verbose("upper first, case level on\n");
      genericRulesStarter("[caseFirst upper][caseLevel on]&H<ch<<<Ch<<<CH", upperFirst, UPRV_LENGTHOF(upperFirst));
    }

}

static void TestIncrementalNormalize(void) {

    /*UChar baseA     =0x61;*/
    UChar baseA     =0x41;
/*    UChar baseB     = 0x42;*/
    static const UChar ccMix[]   = {0x316, 0x321, 0x300};
    /*UChar ccMix[]   = {0x61, 0x61, 0x61};*/
    /*
        0x316 is combining grave accent below, cc=220
        0x321 is combining palatalized hook below, cc=202
        0x300 is combining grave accent, cc=230
    */

#define MAXSLEN 2000
    /*int          maxSLen   = 64000;*/
    int          sLen;
    int          i;

    UCollator        *coll;
    UErrorCode       status = U_ZERO_ERROR;
    UCollationResult result;

    int32_t myQ = getTestOption(QUICK_OPTION);

    if(getTestOption(QUICK_OPTION) < 0) {
        setTestOption(QUICK_OPTION, 1);
    }

    {
        /* Test 1.  Run very long unnormalized strings, to force overflow of*/
        /*          most buffers along the way.*/
        UChar            strA[MAXSLEN+1];
        UChar            strB[MAXSLEN+1];

        coll = ucol_open("en_US", &status);
        if(status == U_FILE_ACCESS_ERROR) {
          log_data_err("Is your data around?\n");
          return;
        } else if(U_FAILURE(status)) {
          log_err("Error opening collator\n");
          return;
        }
        ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);

        /*for (sLen = 257; sLen<MAXSLEN; sLen++) {*/
        /*for (sLen = 4; sLen<MAXSLEN; sLen++) {*/
        /*for (sLen = 1000; sLen<1001; sLen++) {*/
        for (sLen = 500; sLen<501; sLen++) {
        /*for (sLen = 40000; sLen<65000; sLen+=1000) {*/
            strA[0] = baseA;
            strB[0] = baseA;
            for (i=1; i<=sLen-1; i++) {
                strA[i] = ccMix[i % 3];
                strB[sLen-i] = ccMix[i % 3];
            }
            strA[sLen]   = 0;
            strB[sLen]   = 0;

            ucol_setStrength(coll, UCOL_TERTIARY);   /* Do test with default strength, which runs*/
            doTest(coll, strA, strB, UCOL_EQUAL);    /*   optimized functions in the impl*/
            ucol_setStrength(coll, UCOL_IDENTICAL);   /* Do again with the slow, general impl.*/
            doTest(coll, strA, strB, UCOL_EQUAL);
        }
    }

    setTestOption(QUICK_OPTION, myQ);


    /*  Test 2:  Non-normal sequence in a string that extends to the last character*/
    /*         of the string.  Checks a couple of edge cases.*/

    {
        static const UChar strA[] = {0x41, 0x41, 0x300, 0x316, 0};
        static const UChar strB[] = {0x41, 0xc0, 0x316, 0};
        ucol_setStrength(coll, UCOL_TERTIARY);
        doTest(coll, strA, strB, UCOL_EQUAL);
    }

    /*  Test 3:  Non-normal sequence is terminated by a surrogate pair.*/

    {
      /* New UCA  3.1.1.
       * test below used a code point from Desseret, which sorts differently
       * than d800 dc00
       */
        /*UChar strA[] = {0x41, 0x41, 0x300, 0x316, 0xD801, 0xDC00, 0};*/
        static const UChar strA[] = {0x41, 0x41, 0x300, 0x316, 0xD800, 0xDC01, 0};
        static const UChar strB[] = {0x41, 0xc0, 0x316, 0xD800, 0xDC00, 0};
        ucol_setStrength(coll, UCOL_TERTIARY);
        doTest(coll, strA, strB, UCOL_GREATER);
    }

    /*  Test 4:  Embedded nulls do not terminate a string when length is specified.*/

    {
        static const UChar strA[] = {0x41, 0x00, 0x42, 0x00};
        static const UChar strB[] = {0x41, 0x00, 0x00, 0x00};
        char  sortKeyA[50];
        char  sortKeyAz[50];
        char  sortKeyB[50];
        char  sortKeyBz[50];
        int   r;

        /* there used to be -3 here. Hmmmm.... */
        /*result = ucol_strcoll(coll, strA, -3, strB, -3);*/
        result = ucol_strcoll(coll, strA, 3, strB, 3);
        if (result != UCOL_GREATER) {
            log_err("ERROR 1 in test 4\n");
        }
        result = ucol_strcoll(coll, strA, -1, strB, -1);
        if (result != UCOL_EQUAL) {
            log_err("ERROR 2 in test 4\n");
        }

        ucol_getSortKey(coll, strA,  3, (uint8_t *)sortKeyA, sizeof(sortKeyA));
        ucol_getSortKey(coll, strA, -1, (uint8_t *)sortKeyAz, sizeof(sortKeyAz));
        ucol_getSortKey(coll, strB,  3, (uint8_t *)sortKeyB, sizeof(sortKeyB));
        ucol_getSortKey(coll, strB, -1, (uint8_t *)sortKeyBz, sizeof(sortKeyBz));

        r = strcmp(sortKeyA, sortKeyAz);
        if (r <= 0) {
            log_err("Error 3 in test 4\n");
        }
        r = strcmp(sortKeyA, sortKeyB);
        if (r <= 0) {
            log_err("Error 4 in test 4\n");
        }
        r = strcmp(sortKeyAz, sortKeyBz);
        if (r != 0) {
            log_err("Error 5 in test 4\n");
        }

        ucol_setStrength(coll, UCOL_IDENTICAL);
        ucol_getSortKey(coll, strA,  3, (uint8_t *)sortKeyA, sizeof(sortKeyA));
        ucol_getSortKey(coll, strA, -1, (uint8_t *)sortKeyAz, sizeof(sortKeyAz));
        ucol_getSortKey(coll, strB,  3, (uint8_t *)sortKeyB, sizeof(sortKeyB));
        ucol_getSortKey(coll, strB, -1, (uint8_t *)sortKeyBz, sizeof(sortKeyBz));

        r = strcmp(sortKeyA, sortKeyAz);
        if (r <= 0) {
            log_err("Error 6 in test 4\n");
        }
        r = strcmp(sortKeyA, sortKeyB);
        if (r <= 0) {
            log_err("Error 7 in test 4\n");
        }
        r = strcmp(sortKeyAz, sortKeyBz);
        if (r != 0) {
            log_err("Error 8 in test 4\n");
        }
        ucol_setStrength(coll, UCOL_TERTIARY);
    }


    /*  Test 5:  Null characters in non-normal source strings.*/

    {
        static const UChar strA[] = {0x41, 0x41, 0x300, 0x316, 0x00, 0x42, 0x00};
        static const UChar strB[] = {0x41, 0x41, 0x300, 0x316, 0x00, 0x00, 0x00};
        char  sortKeyA[50];
        char  sortKeyAz[50];
        char  sortKeyB[50];
        char  sortKeyBz[50];
        int   r;

        result = ucol_strcoll(coll, strA, 6, strB, 6);
        if (result != UCOL_GREATER) {
            log_err("ERROR 1 in test 5\n");
        }
        result = ucol_strcoll(coll, strA, -1, strB, -1);
        if (result != UCOL_EQUAL) {
            log_err("ERROR 2 in test 5\n");
        }

        ucol_getSortKey(coll, strA,  6, (uint8_t *)sortKeyA, sizeof(sortKeyA));
        ucol_getSortKey(coll, strA, -1, (uint8_t *)sortKeyAz, sizeof(sortKeyAz));
        ucol_getSortKey(coll, strB,  6, (uint8_t *)sortKeyB, sizeof(sortKeyB));
        ucol_getSortKey(coll, strB, -1, (uint8_t *)sortKeyBz, sizeof(sortKeyBz));

        r = strcmp(sortKeyA, sortKeyAz);
        if (r <= 0) {
            log_err("Error 3 in test 5\n");
        }
        r = strcmp(sortKeyA, sortKeyB);
        if (r <= 0) {
            log_err("Error 4 in test 5\n");
        }
        r = strcmp(sortKeyAz, sortKeyBz);
        if (r != 0) {
            log_err("Error 5 in test 5\n");
        }

        ucol_setStrength(coll, UCOL_IDENTICAL);
        ucol_getSortKey(coll, strA,  6, (uint8_t *)sortKeyA, sizeof(sortKeyA));
        ucol_getSortKey(coll, strA, -1, (uint8_t *)sortKeyAz, sizeof(sortKeyAz));
        ucol_getSortKey(coll, strB,  6, (uint8_t *)sortKeyB, sizeof(sortKeyB));
        ucol_getSortKey(coll, strB, -1, (uint8_t *)sortKeyBz, sizeof(sortKeyBz));

        r = strcmp(sortKeyA, sortKeyAz);
        if (r <= 0) {
            log_err("Error 6 in test 5\n");
        }
        r = strcmp(sortKeyA, sortKeyB);
        if (r <= 0) {
            log_err("Error 7 in test 5\n");
        }
        r = strcmp(sortKeyAz, sortKeyBz);
        if (r != 0) {
            log_err("Error 8 in test 5\n");
        }
        ucol_setStrength(coll, UCOL_TERTIARY);
    }


    /*  Test 6:  Null character as base of a non-normal combining sequence.*/

    {
        static const UChar strA[] = {0x41, 0x0, 0x300, 0x316, 0x41, 0x302, 0x00};
        static const UChar strB[] = {0x41, 0x0, 0x302, 0x316, 0x41, 0x300, 0x00};

        result = ucol_strcoll(coll, strA, 5, strB, 5);
        if (result != UCOL_LESS) {
            log_err("Error 1 in test 6\n");
        }
        result = ucol_strcoll(coll, strA, -1, strB, -1);
        if (result != UCOL_EQUAL) {
            log_err("Error 2 in test 6\n");
        }
    }

    ucol_close(coll);
}



#if 0
static void TestGetCaseBit(void) {
  static const char *caseBitData[] = {
    "a", "A", "ch", "Ch", "CH",
      "\\uFF9E", "\\u0009"
  };

  static const uint8_t results[] = {
    UCOL_LOWER_CASE, UCOL_UPPER_CASE, UCOL_LOWER_CASE, UCOL_MIXED_CASE, UCOL_UPPER_CASE,
      UCOL_UPPER_CASE, UCOL_LOWER_CASE
  };

  uint32_t i, blen = 0;
  UChar b[256] = {0};
  UErrorCode status = U_ZERO_ERROR;
  UCollator *UCA = ucol_open("", &status);
  uint8_t res = 0;

  for(i = 0; i<UPRV_LENGTHOF(results); i++) {
    blen = u_unescape(caseBitData[i], b, 256);
    res = ucol_uprv_getCaseBits(UCA, b, blen, &status);
    if(results[i] != res) {
      log_err("Expected case = %02X, got %02X for %04X\n", results[i], res, b[0]);
    }
  }
}
#endif

static void TestHangulTailoring(void) {
    static const char *koreanData[] = {
        "\\uac00", "\\u4f3d", "\\u4f73", "\\u5047", "\\u50f9", "\\u52a0", "\\u53ef", "\\u5475",
            "\\u54e5", "\\u5609", "\\u5ac1", "\\u5bb6", "\\u6687", "\\u67b6", "\\u67b7", "\\u67ef",
            "\\u6b4c", "\\u73c2", "\\u75c2", "\\u7a3c", "\\u82db", "\\u8304", "\\u8857", "\\u8888",
            "\\u8a36", "\\u8cc8", "\\u8dcf", "\\u8efb", "\\u8fe6", "\\u99d5",
            "\\u4EEE", "\\u50A2", "\\u5496", "\\u54FF", "\\u5777", "\\u5B8A", "\\u659D", "\\u698E",
            "\\u6A9F", "\\u73C8", "\\u7B33", "\\u801E", "\\u8238", "\\u846D", "\\u8B0C"
    };

    const char *rules =
        "&\\uac00 <<< \\u4f3d <<< \\u4f73 <<< \\u5047 <<< \\u50f9 <<< \\u52a0 <<< \\u53ef <<< \\u5475 "
        "<<< \\u54e5 <<< \\u5609 <<< \\u5ac1 <<< \\u5bb6 <<< \\u6687 <<< \\u67b6 <<< \\u67b7 <<< \\u67ef "
        "<<< \\u6b4c <<< \\u73c2 <<< \\u75c2 <<< \\u7a3c <<< \\u82db <<< \\u8304 <<< \\u8857 <<< \\u8888 "
        "<<< \\u8a36 <<< \\u8cc8 <<< \\u8dcf <<< \\u8efb <<< \\u8fe6 <<< \\u99d5 "
        "<<< \\u4EEE <<< \\u50A2 <<< \\u5496 <<< \\u54FF <<< \\u5777 <<< \\u5B8A <<< \\u659D <<< \\u698E "
        "<<< \\u6A9F <<< \\u73C8 <<< \\u7B33 <<< \\u801E <<< \\u8238 <<< \\u846D <<< \\u8B0C";


  UErrorCode status = U_ZERO_ERROR;
  UChar rlz[2048] = { 0 };
  uint32_t rlen = u_unescape(rules, rlz, 2048);

  UCollator *coll = ucol_openRules(rlz, rlen, UCOL_DEFAULT, UCOL_DEFAULT,NULL, &status);
  if(status == U_FILE_ACCESS_ERROR) {
    log_data_err("Is your data around?\n");
    return;
  } else if(U_FAILURE(status)) {
    log_err("Error opening collator\n");
    return;
  }

  log_verbose("Using start of korean rules\n");

  if(U_SUCCESS(status)) {
    genericOrderingTest(coll, koreanData, UPRV_LENGTHOF(koreanData));
  } else {
    log_err("Unable to open collator with rules %s\n", rules);
  }

  ucol_close(coll);

  log_verbose("Using ko__LOTUS locale\n");
  genericLocaleStarter("ko__LOTUS", koreanData, UPRV_LENGTHOF(koreanData));
}

/*
 * The secondary/tertiary compression middle byte
 * as used by the current implementation.
 * Subject to change as the sort key compression changes.
 * See class CollationKeys.
 */
enum {
    SEC_COMMON_MIDDLE = 0x25,  /* range 05..45 */
    TER_ONLY_COMMON_MIDDLE = 0x65  /* range 05..C5 */
};

static void TestCompressOverlap(void) {
    UChar       secstr[150];
    UChar       tertstr[150];
    UErrorCode  status = U_ZERO_ERROR;
    UCollator  *coll;
    uint8_t     result[500];
    uint32_t    resultlen;
    int         count = 0;
    uint8_t    *tempptr;

    coll = ucol_open("", &status);

    if (U_FAILURE(status)) {
        log_err_status(status, "Collator can't be created -> %s\n", u_errorName(status));
        return;
    }
    while (count < 149) {
        secstr[count] = 0x0020; /* [06, 05, 05] */
        tertstr[count] = 0x0020;
        count ++;
    }

    /* top down compression ----------------------------------- */
    secstr[count] = 0x0332; /* [, 87, 05] */
    tertstr[count] = 0x3000; /* [06, 05, 07] */

    /* no compression secstr should have 150 secondary bytes, tertstr should
    have 150 tertiary bytes.
    with correct compression, secstr should have 6 secondary
    bytes (149/33 rounded up + accent), tertstr should have > 2 tertiary bytes */
    resultlen = ucol_getSortKey(coll, secstr, 150, result, UPRV_LENGTHOF(result));
    (void)resultlen;    /* Suppress set but not used warning. */
    tempptr = (uint8_t *)uprv_strchr((char *)result, 1) + 1;
    while (*(tempptr + 1) != 1) {
        /* the last secondary collation element is not checked since it is not
        part of the compression */
        if (*tempptr < SEC_COMMON_MIDDLE) {
            log_err("Secondary top down compression overlapped\n");
        }
        tempptr ++;
    }

    /* tertiary top/bottom/common for en_US is similar to the secondary
    top/bottom/common */
    resultlen = ucol_getSortKey(coll, tertstr, 150, result, UPRV_LENGTHOF(result));
    tempptr = (uint8_t *)uprv_strrchr((char *)result, 1) + 1;
    while (*(tempptr + 1) != 0) {
        /* the last secondary collation element is not checked since it is not
        part of the compression */
        if (*tempptr < TER_ONLY_COMMON_MIDDLE) {
            log_err("Tertiary top down compression overlapped\n");
        }
        tempptr ++;
    }

    /* bottom up compression ------------------------------------- */
    secstr[count] = 0;
    tertstr[count] = 0;
    resultlen = ucol_getSortKey(coll, secstr, 150, result, UPRV_LENGTHOF(result));
    tempptr = (uint8_t *)uprv_strchr((char *)result, 1) + 1;
    while (*(tempptr + 1) != 1) {
        /* the last secondary collation element is not checked since it is not
        part of the compression */
        if (*tempptr > SEC_COMMON_MIDDLE) {
            log_err("Secondary bottom up compression overlapped\n");
        }
        tempptr ++;
    }

    /* tertiary top/bottom/common for en_US is similar to the secondary
    top/bottom/common */
    resultlen = ucol_getSortKey(coll, tertstr, 150, result, UPRV_LENGTHOF(result));
    tempptr = (uint8_t *)uprv_strrchr((char *)result, 1) + 1;
    while (*(tempptr + 1) != 0) {
        /* the last secondary collation element is not checked since it is not
        part of the compression */
        if (*tempptr > TER_ONLY_COMMON_MIDDLE) {
            log_err("Tertiary bottom up compression overlapped\n");
        }
        tempptr ++;
    }

    ucol_close(coll);
}

static void TestCyrillicTailoring(void) {
  static const char *test[] = {
    "\\u0410b",
      "\\u0410\\u0306a",
      "\\u04d0A"
  };

    /* Russian overrides contractions, so this test is not valid anymore */
    /*genericLocaleStarter("ru", test, 3);*/

    // Most of the following are commented out because UCA 8.0
    // drops most of the Cyrillic contractions from the default order.
    // See CLDR ticket #7246 "root collation: remove Cyrillic contractions".

    // genericLocaleStarter("root", test, 3);
    // genericRulesStarter("&\\u0410 = \\u0410", test, 3);
    // genericRulesStarter("&Z < \\u0410", test, 3);
    genericRulesStarter("&\\u0410 = \\u0410 < \\u04d0", test, 3);
    genericRulesStarter("&Z < \\u0410 < \\u04d0", test, 3);
    // genericRulesStarter("&\\u0410 = \\u0410 < \\u0410\\u0301", test, 3);
    // genericRulesStarter("&Z < \\u0410 < \\u0410\\u0301", test, 3);
}

static void TestSuppressContractions(void) {

  static const char *testNoCont2[] = {
      "\\u0410\\u0302a",
      "\\u0410\\u0306b",
      "\\u0410c"
  };
  static const char *testNoCont[] = {
      "a\\u0410",
      "A\\u0410\\u0306",
      "\\uFF21\\u0410\\u0302"
  };

  genericRulesStarter("[suppressContractions [\\u0400-\\u047f]]", testNoCont, 3);
  genericRulesStarter("[suppressContractions [\\u0400-\\u047f]]", testNoCont2, 3);
}

static void TestContraction(void) {
    const static char *testrules[] = {
        "&A = AB / B",
        "&A = A\\u0306/\\u0306",
        "&c = ch / h"
    };
    const static UChar testdata[][2] = {
        {0x0041 /* 'A' */, 0x0042 /* 'B' */},
        {0x0041 /* 'A' */, 0x0306 /* combining breve */},
        {0x0063 /* 'c' */, 0x0068 /* 'h' */}
    };
    const static UChar testdata2[][2] = {
        {0x0063 /* 'c' */, 0x0067 /* 'g' */},
        {0x0063 /* 'c' */, 0x0068 /* 'h' */},
        {0x0063 /* 'c' */, 0x006C /* 'l' */}
    };
#if 0
    /*
     * These pairs of rule strings are not guaranteed to yield the very same mappings.
     * In fact, LDML 24 recommends an improved way of creating mappings
     * which always yields different mappings for such pairs. See
     * http://www.unicode.org/reports/tr35/tr35-33/tr35-collation.html#Orderings
     */
    const static char *testrules3[] = {
        "&z < xyz &xyzw << B",
        "&z < xyz &xyz << B / w",
        "&z < ch &achm << B",
        "&z < ch &a << B / chm",
        "&\\ud800\\udc00w << B",
        "&\\ud800\\udc00 << B / w",
        "&a\\ud800\\udc00m << B",
        "&a << B / \\ud800\\udc00m",
    };
#endif

    UErrorCode  status   = U_ZERO_ERROR;
    UCollator  *coll;
    UChar       rule[256] = {0};
    uint32_t    rlen     = 0;
    int         i;

    for (i = 0; i < UPRV_LENGTHOF(testrules); i ++) {
        UCollationElements *iter1;
        int j = 0;
        log_verbose("Rule %s for testing\n", testrules[i]);
        rlen = u_unescape(testrules[i], rule, 32);
        coll = ucol_openRules(rule, rlen, UCOL_ON, UCOL_TERTIARY,NULL, &status);
        if (U_FAILURE(status)) {
            log_err_status(status, "Collator creation failed %s -> %s\n", testrules[i], u_errorName(status));
            return;
        }
        iter1 = ucol_openElements(coll, testdata[i], 2, &status);
        if (U_FAILURE(status)) {
            log_err("Collation iterator creation failed\n");
            return;
        }
        while (j < 2) {
            UCollationElements *iter2 = ucol_openElements(coll,
                                                         &(testdata[i][j]),
                                                         1, &status);
            int32_t ce;
            if (U_FAILURE(status)) {
                log_err("Collation iterator creation failed\n");
                return;
            }
            ce = ucol_next(iter2, &status);
            while (ce != UCOL_NULLORDER) {
                if (ucol_next(iter1, &status) != ce) {
                    log_err("Collation elements in contraction split does not match\n");
                    return;
                }
                ce = ucol_next(iter2, &status);
            }
            j ++;
            ucol_closeElements(iter2);
        }
        if (ucol_next(iter1, &status) != UCOL_NULLORDER) {
            log_err("Collation elements not exhausted\n");
            return;
        }
        ucol_closeElements(iter1);
        ucol_close(coll);
    }

    rlen = u_unescape("& a < b < c < ch < d & c = ch / h", rule, 256);
    coll = ucol_openRules(rule, rlen, UCOL_ON, UCOL_TERTIARY,NULL, &status);
    if (ucol_strcoll(coll, testdata2[0], 2, testdata2[1], 2) != UCOL_LESS) {
        log_err("Expected \\u%04x\\u%04x < \\u%04x\\u%04x\n",
                testdata2[0][0], testdata2[0][1], testdata2[1][0],
                testdata2[1][1]);
        return;
    }
    if (ucol_strcoll(coll, testdata2[1], 2, testdata2[2], 2) != UCOL_LESS) {
        log_err("Expected \\u%04x\\u%04x < \\u%04x\\u%04x\n",
                testdata2[1][0], testdata2[1][1], testdata2[2][0],
                testdata2[2][1]);
        return;
    }
    ucol_close(coll);
#if 0  /* see above */
    for (i = 0; i < UPRV_LENGTHOF(testrules3); i += 2) {
        log_verbose("testrules3 i==%d  \"%s\" vs. \"%s\"\n", i, testrules3[i], testrules3[i + 1]);
        UCollator          *coll1,
                           *coll2;
        UCollationElements *iter1,
                           *iter2;
        UChar               ch = 0x0042 /* 'B' */;
        uint32_t            ce;
        rlen = u_unescape(testrules3[i], rule, 32);
        coll1 = ucol_openRules(rule, rlen, UCOL_ON, UCOL_TERTIARY,NULL, &status);
        rlen = u_unescape(testrules3[i + 1], rule, 32);
        coll2 = ucol_openRules(rule, rlen, UCOL_ON, UCOL_TERTIARY,NULL, &status);
        if (U_FAILURE(status)) {
            log_err("Collator creation failed %s\n", testrules[i]);
            return;
        }
        iter1 = ucol_openElements(coll1, &ch, 1, &status);
        iter2 = ucol_openElements(coll2, &ch, 1, &status);
        if (U_FAILURE(status)) {
            log_err("Collation iterator creation failed\n");
            return;
        }
        ce = ucol_next(iter1, &status);
        if (U_FAILURE(status)) {
            log_err("Retrieving ces failed\n");
            return;
        }
        while (ce != UCOL_NULLORDER) {
            uint32_t ce2 = (uint32_t)ucol_next(iter2, &status);
            if (ce == ce2) {
                log_verbose("CEs match: %08x\n", ce);
            } else {
                log_err("CEs do not match: %08x vs. %08x\n", ce, ce2);
                return;
            }
            ce = ucol_next(iter1, &status);
            if (U_FAILURE(status)) {
                log_err("Retrieving ces failed\n");
                return;
            }
        }
        if (ucol_next(iter2, &status) != UCOL_NULLORDER) {
            log_err("CEs not exhausted\n");
            return;
        }
        ucol_closeElements(iter1);
        ucol_closeElements(iter2);
        ucol_close(coll1);
        ucol_close(coll2);
    }
#endif
}

static void TestExpansion(void) {
    const static char *testrules[] = {
#if 0
        /*
         * This seems to have tested that M was not mapped to an expansion.
         * I believe the old builder just did that because it computed the extension CEs
         * at the very end, which was a bug.
         * Among other problems, it violated the core tailoring principle
         * by making an earlier rule depend on a later one.
         * And, of course, if M did not get an expansion, then it was primary different from K,
         * unlike what the rule &K<<M says.
         */
        "&J << K / B & K << M",
#endif
        "&J << K / B << M"
    };
    const static UChar testdata[][3] = {
        {0x004A /*'J'*/, 0x0041 /*'A'*/, 0},
        {0x004D /*'M'*/, 0x0041 /*'A'*/, 0},
        {0x004B /*'K'*/, 0x0041 /*'A'*/, 0},
        {0x004B /*'K'*/, 0x0043 /*'C'*/, 0},
        {0x004A /*'J'*/, 0x0043 /*'C'*/, 0},
        {0x004D /*'M'*/, 0x0043 /*'C'*/, 0}
    };

    UErrorCode  status   = U_ZERO_ERROR;
    UCollator  *coll;
    UChar       rule[256] = {0};
    uint32_t    rlen     = 0;
    int         i;

    for (i = 0; i < UPRV_LENGTHOF(testrules); i ++) {
        int j = 0;
        log_verbose("Rule %s for testing\n", testrules[i]);
        rlen = u_unescape(testrules[i], rule, 32);
        coll = ucol_openRules(rule, rlen, UCOL_ON, UCOL_TERTIARY,NULL, &status);
        if (U_FAILURE(status)) {
            log_err_status(status, "Collator creation failed %s -> %s\n", testrules[i], u_errorName(status));
            return;
        }

        for (j = 0; j < 5; j ++) {
            doTest(coll, testdata[j], testdata[j + 1], UCOL_LESS);
        }
        ucol_close(coll);
    }
}

#if 0
/* this test tests the current limitations of the engine */
/* it always fail, so it is disabled by default */
static void TestLimitations(void) {
  /* recursive expansions */
  {
    static const char *rule = "&a=b/c&d=c/e";
    static const char *tlimit01[] = {"add","b","adf"};
    static const char *tlimit02[] = {"aa","b","af"};
    log_verbose("recursive expansions\n");
    genericRulesStarter(rule, tlimit01, UPRV_LENGTHOF(tlimit01));
    genericRulesStarter(rule, tlimit02, UPRV_LENGTHOF(tlimit02));
  }
  /* contractions spanning expansions */
  {
    static const char *rule = "&a<<<c/e&g<<<eh";
    static const char *tlimit01[] = {"ad","c","af","f","ch","h"};
    static const char *tlimit02[] = {"ad","c","ch","af","f","h"};
    log_verbose("contractions spanning expansions\n");
    genericRulesStarter(rule, tlimit01, UPRV_LENGTHOF(tlimit01));
    genericRulesStarter(rule, tlimit02, UPRV_LENGTHOF(tlimit02));
  }
  /* normalization: nulls in contractions */
  {
    static const char *rule = "&a<<<\\u0000\\u0302";
    static const char *tlimit01[] = {"a","\\u0000\\u0302\\u0327"};
    static const char *tlimit02[] = {"\\u0000\\u0302\\u0327","a"};
    static const UColAttribute att[] = { UCOL_DECOMPOSITION_MODE };
    static const UColAttributeValue valOn[] = { UCOL_ON };
    static const UColAttributeValue valOff[] = { UCOL_OFF };

    log_verbose("NULL in contractions\n");
    genericRulesStarterWithOptions(rule, tlimit01, 2, att, valOn, 1);
    genericRulesStarterWithOptions(rule, tlimit02, 2, att, valOn, 1);
    genericRulesStarterWithOptions(rule, tlimit01, 2, att, valOff, 1);
    genericRulesStarterWithOptions(rule, tlimit02, 2, att, valOff, 1);

  }
  /* normalization: contractions spanning normalization */
  {
    static const char *rule = "&a<<<\\u0000\\u0302";
    static const char *tlimit01[] = {"a","\\u0000\\u0302\\u0327"};
    static const char *tlimit02[] = {"\\u0000\\u0302\\u0327","a"};
    static const UColAttribute att[] = { UCOL_DECOMPOSITION_MODE };
    static const UColAttributeValue valOn[] = { UCOL_ON };
    static const UColAttributeValue valOff[] = { UCOL_OFF };

    log_verbose("contractions spanning normalization\n");
    genericRulesStarterWithOptions(rule, tlimit01, 2, att, valOn, 1);
    genericRulesStarterWithOptions(rule, tlimit02, 2, att, valOn, 1);
    genericRulesStarterWithOptions(rule, tlimit01, 2, att, valOff, 1);
    genericRulesStarterWithOptions(rule, tlimit02, 2, att, valOff, 1);

  }
  /* variable top:  */
  {
    /*static const char *rule2 = "&\\u2010<x=[variable top]<z";*/
    static const char *rule = "&\\u2010<x<[variable top]=z";
    /*static const char *rule3 = "&' '<x<[variable top]=z";*/
    static const char *tlimit01[] = {" ", "z", "zb", "a", " b", "xb", "b", "c" };
    static const char *tlimit02[] = {"-", "-x", "x","xb", "-z", "z", "zb", "-a", "a", "-b", "b", "c"};
    static const char *tlimit03[] = {" ", "xb", "z", "zb", "a", " b", "b", "c" };
    static const UColAttribute att[] = { UCOL_ALTERNATE_HANDLING, UCOL_STRENGTH };
    static const UColAttributeValue valOn[] = { UCOL_SHIFTED, UCOL_QUATERNARY };
    static const UColAttributeValue valOff[] = { UCOL_NON_IGNORABLE, UCOL_TERTIARY };

    log_verbose("variable top\n");
    genericRulesStarterWithOptions(rule, tlimit03, UPRV_LENGTHOF(tlimit03), att, valOn, UPRV_LENGTHOF(att));
    genericRulesStarterWithOptions(rule, tlimit01, UPRV_LENGTHOF(tlimit01), att, valOn, UPRV_LENGTHOF(att));
    genericRulesStarterWithOptions(rule, tlimit02, UPRV_LENGTHOF(tlimit02), att, valOn, UPRV_LENGTHOF(att));
    genericRulesStarterWithOptions(rule, tlimit01, UPRV_LENGTHOF(tlimit01), att, valOff, UPRV_LENGTHOF(att));
    genericRulesStarterWithOptions(rule, tlimit02, UPRV_LENGTHOF(tlimit02), att, valOff, UPRV_LENGTHOF(att));

  }
  /* case level */
  {
    static const char *rule = "&c<ch<<<cH<<<Ch<<<CH";
    static const char *tlimit01[] = {"c","CH","Ch","cH","ch"};
    static const char *tlimit02[] = {"c","CH","cH","Ch","ch"};
    static const UColAttribute att[] = { UCOL_CASE_FIRST};
    static const UColAttributeValue valOn[] = { UCOL_UPPER_FIRST};
    /*static const UColAttributeValue valOff[] = { UCOL_OFF};*/
    log_verbose("case level\n");
    genericRulesStarterWithOptions(rule, tlimit01, UPRV_LENGTHOF(tlimit01), att, valOn, UPRV_LENGTHOF(att));
    genericRulesStarterWithOptions(rule, tlimit02, UPRV_LENGTHOF(tlimit02), att, valOn, UPRV_LENGTHOF(att));
    /*genericRulesStarterWithOptions(rule, tlimit01, UPRV_LENGTHOF(tlimit01), att, valOff, UPRV_LENGTHOF(att));*/
    /*genericRulesStarterWithOptions(rule, tlimit02, UPRV_LENGTHOF(tlimit02), att, valOff, UPRV_LENGTHOF(att));*/
  }

}
#endif

static void TestBocsuCoverage(void) {
  UErrorCode status = U_ZERO_ERROR;
  const char *testString = "\\u0041\\u0441\\u4441\\U00044441\\u4441\\u0441\\u0041";
  UChar       test[256] = {0};
  uint32_t    tlen     = u_unescape(testString, test, 32);
  uint8_t key[256]     = {0};
  uint32_t klen         = 0;

  UCollator *coll = ucol_open("", &status);
  if(U_SUCCESS(status)) {
  ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_IDENTICAL, &status);

  klen = ucol_getSortKey(coll, test, tlen, key, 256);
  (void)klen;    /* Suppress set but not used warning. */

  ucol_close(coll);
  } else {
    log_data_err("Couldn't open UCA\n");
  }
}

static void TestVariableTopSetting(void) {
  UErrorCode status = U_ZERO_ERROR;
  uint32_t varTopOriginal = 0, varTop1, varTop2;
  UCollator *coll = ucol_open("", &status);
  if(U_SUCCESS(status)) {

  static const UChar nul = 0;
  static const UChar space = 0x20;
  static const UChar dot = 0x2e;  /* punctuation */
  static const UChar degree = 0xb0;  /* symbol */
  static const UChar dollar = 0x24;  /* currency symbol */
  static const UChar zero = 0x30;  /* digit */

  varTopOriginal = ucol_getVariableTop(coll, &status);
  log_verbose("ucol_getVariableTop(root) -> %08x\n", varTopOriginal);
  ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, &status);

  varTop1 = ucol_setVariableTop(coll, &space, 1, &status);
  varTop2 = ucol_getVariableTop(coll, &status);
  log_verbose("ucol_setVariableTop(space) -> %08x\n", varTop1);
  if(U_FAILURE(status) || varTop1 != varTop2 ||
      !ucol_equal(coll, &nul, 0, &space, 1) ||
      ucol_equal(coll, &nul, 0, &dot, 1) ||
      ucol_equal(coll, &nul, 0, &degree, 1) ||
      ucol_equal(coll, &nul, 0, &dollar, 1) ||
      ucol_equal(coll, &nul, 0, &zero, 1) ||
      ucol_greaterOrEqual(coll, &space, 1, &dot, 1)) {
    log_err("ucol_setVariableTop(space) did not work - %s\n", u_errorName(status));
  }

  varTop1 = ucol_setVariableTop(coll, &dot, 1, &status);
  varTop2 = ucol_getVariableTop(coll, &status);
  log_verbose("ucol_setVariableTop(dot) -> %08x\n", varTop1);
  if(U_FAILURE(status) || varTop1 != varTop2 ||
      !ucol_equal(coll, &nul, 0, &space, 1) ||
      !ucol_equal(coll, &nul, 0, &dot, 1) ||
      ucol_equal(coll, &nul, 0, &degree, 1) ||
      ucol_equal(coll, &nul, 0, &dollar, 1) ||
      ucol_equal(coll, &nul, 0, &zero, 1) ||
      ucol_greaterOrEqual(coll, &dot, 1, &degree, 1)) {
    log_err("ucol_setVariableTop(dot) did not work - %s\n", u_errorName(status));
  }

  varTop1 = ucol_setVariableTop(coll, &degree, 1, &status);
  varTop2 = ucol_getVariableTop(coll, &status);
  log_verbose("ucol_setVariableTop(degree) -> %08x\n", varTop1);
  if(U_FAILURE(status) || varTop1 != varTop2 ||
      !ucol_equal(coll, &nul, 0, &space, 1) ||
      !ucol_equal(coll, &nul, 0, &dot, 1) ||
      !ucol_equal(coll, &nul, 0, &degree, 1) ||
      ucol_equal(coll, &nul, 0, &dollar, 1) ||
      ucol_equal(coll, &nul, 0, &zero, 1) ||
      ucol_greaterOrEqual(coll, &degree, 1, &dollar, 1)) {
    log_err("ucol_setVariableTop(degree) did not work - %s\n", u_errorName(status));
  }

  varTop1 = ucol_setVariableTop(coll, &dollar, 1, &status);
  varTop2 = ucol_getVariableTop(coll, &status);
  log_verbose("ucol_setVariableTop(dollar) -> %08x\n", varTop1);
  if(U_FAILURE(status) || varTop1 != varTop2 ||
      !ucol_equal(coll, &nul, 0, &space, 1) ||
      !ucol_equal(coll, &nul, 0, &dot, 1) ||
      !ucol_equal(coll, &nul, 0, &degree, 1) ||
      !ucol_equal(coll, &nul, 0, &dollar, 1) ||
      ucol_equal(coll, &nul, 0, &zero, 1) ||
      ucol_greaterOrEqual(coll, &dollar, 1, &zero, 1)) {
    log_err("ucol_setVariableTop(dollar) did not work - %s\n", u_errorName(status));
  }

  log_verbose("Testing setting variable top to contractions\n");
  {
    UChar first[4] = { 0 };
    first[0] = 0x0040;
    first[1] = 0x0050;
    first[2] = 0x0000;

    status = U_ZERO_ERROR;
    ucol_setVariableTop(coll, first, -1, &status);

    if(U_SUCCESS(status)) {
      log_err("Invalid contraction succeeded in setting variable top!\n");
    }

  }

  log_verbose("Test restoring variable top\n");

  status = U_ZERO_ERROR;
  ucol_restoreVariableTop(coll, varTopOriginal, &status);
  if(varTopOriginal != ucol_getVariableTop(coll, &status)) {
    log_err("Couldn't restore old variable top\n");
  }

  log_verbose("Testing calling with error set\n");

  status = U_INTERNAL_PROGRAM_ERROR;
  varTop1 = ucol_setVariableTop(coll, &space, 1, &status);
  varTop2 = ucol_getVariableTop(coll, &status);
  ucol_restoreVariableTop(coll, varTop2, &status);
  varTop1 = ucol_setVariableTop(NULL, &dot, 1, &status);
  varTop2 = ucol_getVariableTop(NULL, &status);
  ucol_restoreVariableTop(NULL, varTop2, &status);
  if(status != U_INTERNAL_PROGRAM_ERROR) {
    log_err("Bad reaction to passed error!\n");
  }
  ucol_close(coll);
  } else {
    log_data_err("Couldn't open UCA collator\n");
  }
}

static void TestMaxVariable(void) {
  UErrorCode status = U_ZERO_ERROR;
  UColReorderCode oldMax, max;
  UCollator *coll;

  static const UChar nul = 0;
  static const UChar space = 0x20;
  static const UChar dot = 0x2e;  /* punctuation */
  static const UChar degree = 0xb0;  /* symbol */
  static const UChar dollar = 0x24;  /* currency symbol */
  static const UChar zero = 0x30;  /* digit */

  coll = ucol_open("", &status);
  if(U_FAILURE(status)) {
    log_data_err("Couldn't open root collator\n");
    return;
  }

  oldMax = ucol_getMaxVariable(coll);
  log_verbose("ucol_getMaxVariable(root) -> %04x\n", oldMax);
  ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, &status);

  ucol_setMaxVariable(coll, UCOL_REORDER_CODE_SPACE, &status);
  max = ucol_getMaxVariable(coll);
  log_verbose("ucol_setMaxVariable(space) -> %04x\n", max);
  if(U_FAILURE(status) || max != UCOL_REORDER_CODE_SPACE ||
      !ucol_equal(coll, &nul, 0, &space, 1) ||
      ucol_equal(coll, &nul, 0, &dot, 1) ||
      ucol_equal(coll, &nul, 0, &degree, 1) ||
      ucol_equal(coll, &nul, 0, &dollar, 1) ||
      ucol_equal(coll, &nul, 0, &zero, 1) ||
      ucol_greaterOrEqual(coll, &space, 1, &dot, 1)) {
    log_err("ucol_setMaxVariable(space) did not work - %s\n", u_errorName(status));
  }

  ucol_setMaxVariable(coll, UCOL_REORDER_CODE_PUNCTUATION, &status);
  max = ucol_getMaxVariable(coll);
  log_verbose("ucol_setMaxVariable(punctuation) -> %04x\n", max);
  if(U_FAILURE(status) || max != UCOL_REORDER_CODE_PUNCTUATION ||
      !ucol_equal(coll, &nul, 0, &space, 1) ||
      !ucol_equal(coll, &nul, 0, &dot, 1) ||
      ucol_equal(coll, &nul, 0, &degree, 1) ||
      ucol_equal(coll, &nul, 0, &dollar, 1) ||
      ucol_equal(coll, &nul, 0, &zero, 1) ||
      ucol_greaterOrEqual(coll, &dot, 1, &degree, 1)) {
    log_err("ucol_setMaxVariable(punctuation) did not work - %s\n", u_errorName(status));
  }

  ucol_setMaxVariable(coll, UCOL_REORDER_CODE_SYMBOL, &status);
  max = ucol_getMaxVariable(coll);
  log_verbose("ucol_setMaxVariable(symbol) -> %04x\n", max);
  if(U_FAILURE(status) || max != UCOL_REORDER_CODE_SYMBOL ||
      !ucol_equal(coll, &nul, 0, &space, 1) ||
      !ucol_equal(coll, &nul, 0, &dot, 1) ||
      !ucol_equal(coll, &nul, 0, &degree, 1) ||
      ucol_equal(coll, &nul, 0, &dollar, 1) ||
      ucol_equal(coll, &nul, 0, &zero, 1) ||
      ucol_greaterOrEqual(coll, &degree, 1, &dollar, 1)) {
    log_err("ucol_setMaxVariable(symbol) did not work - %s\n", u_errorName(status));
  }

  ucol_setMaxVariable(coll, UCOL_REORDER_CODE_CURRENCY, &status);
  max = ucol_getMaxVariable(coll);
  log_verbose("ucol_setMaxVariable(currency) -> %04x\n", max);
  if(U_FAILURE(status) || max != UCOL_REORDER_CODE_CURRENCY ||
      !ucol_equal(coll, &nul, 0, &space, 1) ||
      !ucol_equal(coll, &nul, 0, &dot, 1) ||
      !ucol_equal(coll, &nul, 0, &degree, 1) ||
      !ucol_equal(coll, &nul, 0, &dollar, 1) ||
      ucol_equal(coll, &nul, 0, &zero, 1) ||
      ucol_greaterOrEqual(coll, &dollar, 1, &zero, 1)) {
    log_err("ucol_setMaxVariable(currency) did not work - %s\n", u_errorName(status));
  }

  log_verbose("Test restoring maxVariable\n");
  status = U_ZERO_ERROR;
  ucol_setMaxVariable(coll, oldMax, &status);
  if(oldMax != ucol_getMaxVariable(coll)) {
    log_err("Couldn't restore old maxVariable\n");
  }

  log_verbose("Testing calling with error set\n");
  status = U_INTERNAL_PROGRAM_ERROR;
  ucol_setMaxVariable(coll, UCOL_REORDER_CODE_SPACE, &status);
  max = ucol_getMaxVariable(coll);
  if(max != oldMax || status != U_INTERNAL_PROGRAM_ERROR) {
    log_err("Bad reaction to passed error!\n");
  }
  ucol_close(coll);
}

static void TestNonChars(void) {
  static const char *test[] = {
      "\\u0000",  /* ignorable */
      "\\uFFFE",  /* special merge-sort character with minimum non-ignorable weights */
      "\\uFDD0", "\\uFDEF",
      "\\U0001FFFE", "\\U0001FFFF",  /* UCA 6.0: noncharacters are treated like unassigned, */
      "\\U0002FFFE", "\\U0002FFFF",  /* not like ignorable. */
      "\\U0003FFFE", "\\U0003FFFF",
      "\\U0004FFFE", "\\U0004FFFF",
      "\\U0005FFFE", "\\U0005FFFF",
      "\\U0006FFFE", "\\U0006FFFF",
      "\\U0007FFFE", "\\U0007FFFF",
      "\\U0008FFFE", "\\U0008FFFF",
      "\\U0009FFFE", "\\U0009FFFF",
      "\\U000AFFFE", "\\U000AFFFF",
      "\\U000BFFFE", "\\U000BFFFF",
      "\\U000CFFFE", "\\U000CFFFF",
      "\\U000DFFFE", "\\U000DFFFF",
      "\\U000EFFFE", "\\U000EFFFF",
      "\\U000FFFFE", "\\U000FFFFF",
      "\\U0010FFFE", "\\U0010FFFF",
      "\\uFFFF"  /* special character with maximum primary weight */
  };
  UErrorCode status = U_ZERO_ERROR;
  UCollator *coll = ucol_open("en_US", &status);

  log_verbose("Test non characters\n");

  if(U_SUCCESS(status)) {
    genericOrderingTestWithResult(coll, test, 35, UCOL_LESS);
  } else {
    log_err_status(status, "Unable to open collator\n");
  }

  ucol_close(coll);
}

static void TestExtremeCompression(void) {
  static char *test[4];
  int32_t j = 0, i = 0;

  for(i = 0; i<4; i++) {
    test[i] = (char *)malloc(2048*sizeof(char));
  }

  for(j = 20; j < 500; j++) {
    for(i = 0; i<4; i++) {
      uprv_memset(test[i], 'a', (j-1)*sizeof(char));
      test[i][j-1] = (char)('a'+i);
      test[i][j] = 0;
    }
    genericLocaleStarter("en_US", (const char **)test, 4);
  }


  for(i = 0; i<4; i++) {
    free(test[i]);
  }
}

#if 0
static void TestExtremeCompression(void) {
  static char *test[4];
  int32_t j = 0, i = 0;
  UErrorCode status = U_ZERO_ERROR;
  UCollator *coll = ucol_open("en_US", status);
  for(i = 0; i<4; i++) {
    test[i] = (char *)malloc(2048*sizeof(char));
  }
  for(j = 10; j < 2048; j++) {
    for(i = 0; i<4; i++) {
      uprv_memset(test[i], 'a', (j-2)*sizeof(char));
      test[i][j-1] = (char)('a'+i);
      test[i][j] = 0;
    }
  }
  genericLocaleStarter("en_US", (const char **)test, 4);

  for(j = 10; j < 2048; j++) {
    for(i = 0; i<1; i++) {
      uprv_memset(test[i], 'a', (j-1)*sizeof(char));
      test[i][j] = 0;
    }
  }
  for(i = 0; i<4; i++) {
    free(test[i]);
  }
}
#endif

static void TestSurrogates(void) {
  static const char *test[] = {
    "z","\\ud900\\udc25",  "\\ud805\\udc50",
       "\\ud800\\udc00y",  "\\ud800\\udc00r",
       "\\ud800\\udc00f",  "\\ud800\\udc00",
       "\\ud800\\udc00c", "\\ud800\\udc00b",
       "\\ud800\\udc00fa", "\\ud800\\udc00fb",
       "\\ud800\\udc00a",
       "c", "b"
  };

  static const char *rule =
    "&z < \\ud900\\udc25   < \\ud805\\udc50"
       "< \\ud800\\udc00y  < \\ud800\\udc00r"
       "< \\ud800\\udc00f  << \\ud800\\udc00"
       "< \\ud800\\udc00fa << \\ud800\\udc00fb"
       "< \\ud800\\udc00a  < c < b" ;

  genericRulesStarter(rule, test, 14);
}

/* This is a test for prefix implementation, used by JIS X 4061 collation rules */
static void TestPrefix(void) {
  uint32_t i;

  static const struct {
    const char *rules;
    const char *data[50];
    const uint32_t len;
  } tests[] = {
    { "&z <<< z|a",
      {"zz", "za"}, 2 },

    { "&z <<< z|   a",
      {"zz", "za"}, 2 },
    { "[strength I]"
      "&a=\\ud900\\udc25"
      "&z<<<\\ud900\\udc25|a",
      {"aa", "az", "\\ud900\\udc25z", "\\ud900\\udc25a", "zz"}, 4 },
  };


  for(i = 0; i<UPRV_LENGTHOF(tests); i++) {
    genericRulesStarter(tests[i].rules, tests[i].data, tests[i].len);
  }
}

/* This test uses data supplied by Masashiko Maedera to test the implementation */
/* JIS X 4061 collation order implementation                                   */
static void TestNewJapanese(void) {

  static const char * const test1[] = {
      "\\u30b7\\u30e3\\u30fc\\u30ec",
      "\\u30b7\\u30e3\\u30a4",
      "\\u30b7\\u30e4\\u30a3",
      "\\u30b7\\u30e3\\u30ec",
      "\\u3061\\u3087\\u3053",
      "\\u3061\\u3088\\u3053",
      "\\u30c1\\u30e7\\u30b3\\u30ec\\u30fc\\u30c8",
      "\\u3066\\u30fc\\u305f",
      "\\u30c6\\u30fc\\u30bf",
      "\\u30c6\\u30a7\\u30bf",
      "\\u3066\\u3048\\u305f",
      "\\u3067\\u30fc\\u305f",
      "\\u30c7\\u30fc\\u30bf",
      "\\u30c7\\u30a7\\u30bf",
      "\\u3067\\u3048\\u305f",
      "\\u3066\\u30fc\\u305f\\u30fc",
      "\\u30c6\\u30fc\\u30bf\\u30a1",
      "\\u30c6\\u30a7\\u30bf\\u30fc",
      "\\u3066\\u3047\\u305f\\u3041",
      "\\u3066\\u3048\\u305f\\u30fc",
      "\\u3067\\u30fc\\u305f\\u30fc",
      "\\u30c7\\u30fc\\u30bf\\u30a1",
      "\\u3067\\u30a7\\u305f\\u30a1",
      "\\u30c7\\u3047\\u30bf\\u3041",
      "\\u30c7\\u30a8\\u30bf\\u30a2",
      "\\u3072\\u3086",
      "\\u3073\\u3085\\u3042",
      "\\u3074\\u3085\\u3042",
      "\\u3073\\u3085\\u3042\\u30fc",
      "\\u30d3\\u30e5\\u30a2\\u30fc",
      "\\u3074\\u3085\\u3042\\u30fc",
      "\\u30d4\\u30e5\\u30a2\\u30fc",
      "\\u30d2\\u30e5\\u30a6",
      "\\u30d2\\u30e6\\u30a6",
      "\\u30d4\\u30e5\\u30a6\\u30a2",
      "\\u3073\\u3085\\u30fc\\u3042\\u30fc",
      "\\u30d3\\u30e5\\u30fc\\u30a2\\u30fc",
      "\\u30d3\\u30e5\\u30a6\\u30a2\\u30fc",
      "\\u3072\\u3085\\u3093",
      "\\u3074\\u3085\\u3093",
      "\\u3075\\u30fc\\u308a",
      "\\u30d5\\u30fc\\u30ea",
      "\\u3075\\u3045\\u308a",
      "\\u3075\\u30a5\\u308a",
      "\\u3075\\u30a5\\u30ea",
      "\\u30d5\\u30a6\\u30ea",
      "\\u3076\\u30fc\\u308a",
      "\\u30d6\\u30fc\\u30ea",
      "\\u3076\\u3045\\u308a",
      "\\u30d6\\u30a5\\u308a",
      "\\u3077\\u3046\\u308a",
      "\\u30d7\\u30a6\\u30ea",
      "\\u3075\\u30fc\\u308a\\u30fc",
      "\\u30d5\\u30a5\\u30ea\\u30fc",
      "\\u3075\\u30a5\\u308a\\u30a3",
      "\\u30d5\\u3045\\u308a\\u3043",
      "\\u30d5\\u30a6\\u30ea\\u30fc",
      "\\u3075\\u3046\\u308a\\u3043",
      "\\u30d6\\u30a6\\u30ea\\u30a4",
      "\\u3077\\u30fc\\u308a\\u30fc",
      "\\u3077\\u30a5\\u308a\\u30a4",
      "\\u3077\\u3046\\u308a\\u30fc",
      "\\u30d7\\u30a6\\u30ea\\u30a4",
      "\\u30d5\\u30fd",
      "\\u3075\\u309e",
      "\\u3076\\u309d",
      "\\u3076\\u3075",
      "\\u3076\\u30d5",
      "\\u30d6\\u3075",
      "\\u30d6\\u30d5",
      "\\u3076\\u309e",
      "\\u3076\\u3077",
      "\\u30d6\\u3077",
      "\\u3077\\u309d",
      "\\u30d7\\u30fd",
      "\\u3077\\u3075",
};

  static const char *test2[] = {
    "\\u306f\\u309d", /* H\\u309d */
    "\\u30cf\\u30fd", /* K\\u30fd */
    "\\u306f\\u306f", /* HH */
    "\\u306f\\u30cf", /* HK */
    "\\u30cf\\u30cf", /* KK */
    "\\u306f\\u309e", /* H\\u309e */
    "\\u30cf\\u30fe", /* K\\u30fe */
    "\\u306f\\u3070", /* HH\\u309b */
    "\\u30cf\\u30d0", /* KK\\u309b */
    "\\u306f\\u3071", /* HH\\u309c */
    "\\u30cf\\u3071", /* KH\\u309c */
    "\\u30cf\\u30d1", /* KK\\u309c */
    "\\u3070\\u309d", /* H\\u309b\\u309d */
    "\\u30d0\\u30fd", /* K\\u309b\\u30fd */
    "\\u3070\\u306f", /* H\\u309bH */
    "\\u30d0\\u30cf", /* K\\u309bK */
    "\\u3070\\u309e", /* H\\u309b\\u309e */
    "\\u30d0\\u30fe", /* K\\u309b\\u30fe */
    "\\u3070\\u3070", /* H\\u309bH\\u309b */
    "\\u30d0\\u3070", /* K\\u309bH\\u309b */
    "\\u30d0\\u30d0", /* K\\u309bK\\u309b */
    "\\u3070\\u3071", /* H\\u309bH\\u309c */
    "\\u30d0\\u30d1", /* K\\u309bK\\u309c */
    "\\u3071\\u309d", /* H\\u309c\\u309d */
    "\\u30d1\\u30fd", /* K\\u309c\\u30fd */
    "\\u3071\\u306f", /* H\\u309cH */
    "\\u30d1\\u30cf", /* K\\u309cK */
    "\\u3071\\u3070", /* H\\u309cH\\u309b */
    "\\u3071\\u30d0", /* H\\u309cK\\u309b */
    "\\u30d1\\u30d0", /* K\\u309cK\\u309b */
    "\\u3071\\u3071", /* H\\u309cH\\u309c */
    "\\u30d1\\u30d1", /* K\\u309cK\\u309c */
  };
  /*
  static const char *test3[] = {
    "\\u221er\\u221e",
    "\\u221eR#",
    "\\u221et\\u221e",
    "#r\\u221e",
    "#R#",
    "#t%",
    "#T%",
    "8t\\u221e",
    "8T\\u221e",
    "8t#",
    "8T#",
    "8t%",
    "8T%",
    "8t8",
    "8T8",
    "\\u03c9r\\u221e",
    "\\u03a9R%",
    "rr\\u221e",
    "rR\\u221e",
    "Rr\\u221e",
    "RR\\u221e",
    "RT%",
    "rt8",
    "tr\\u221e",
    "tr8",
    "TR8",
    "tt8",
    "\\u30b7\\u30e3\\u30fc\\u30ec",
  };
  */
  static const UColAttribute att[] = { UCOL_STRENGTH };
  static const UColAttributeValue val[] = { UCOL_QUATERNARY };

  static const UColAttribute attShifted[] = { UCOL_STRENGTH, UCOL_ALTERNATE_HANDLING};
  static const UColAttributeValue valShifted[] = { UCOL_QUATERNARY, UCOL_SHIFTED };

  genericLocaleStarterWithOptions("ja", test1, UPRV_LENGTHOF(test1), att, val, 1);
  genericLocaleStarterWithOptions("ja", test2, UPRV_LENGTHOF(test2), att, val, 1);
  /*genericLocaleStarter("ja", test3, UPRV_LENGTHOF(test3));*/
  genericLocaleStarterWithOptions("ja", test1, UPRV_LENGTHOF(test1), attShifted, valShifted, 2);
  genericLocaleStarterWithOptions("ja", test2, UPRV_LENGTHOF(test2), attShifted, valShifted, 2);
}

static void TestStrCollIdenticalPrefix(void) {
  const char* rule = "&\\ud9b0\\udc70=\\ud9b0\\udc71";
  const char* test[] = {
    "ab\\ud9b0\\udc70",
    "ab\\ud9b0\\udc71"
  };
  genericRulesStarterWithResult(rule, test, UPRV_LENGTHOF(test), UCOL_EQUAL);
}
/* Contractions should have all their canonically equivalent */
/* strings included */
static void TestContractionClosure(void) {
  static const struct {
    const char *rules;
    const char *data[10];
    const uint32_t len;
  } tests[] = {
    {   "&b=\\u00e4\\u00e4",
      { "b", "\\u00e4\\u00e4", "a\\u0308a\\u0308", "\\u00e4a\\u0308", "a\\u0308\\u00e4" }, 5},
    {   "&b=\\u00C5",
      { "b", "\\u00C5", "A\\u030A", "\\u212B" }, 4},
  };
  uint32_t i;


  for(i = 0; i<UPRV_LENGTHOF(tests); i++) {
    genericRulesStarterWithResult(tests[i].rules, tests[i].data, tests[i].len, UCOL_EQUAL);
  }
}

/* This tests also fails*/
static void TestBeforePrefixFailure(void) {
  static const struct {
    const char *rules;
    const char *data[10];
    const uint32_t len;
  } tests[] = {
    { "&g <<< a"
      "&[before 3]\\uff41 <<< x",
      {"x", "\\uff41"}, 2 },
    {   "&\\u30A7=\\u30A7=\\u3047=\\uff6a"
        "&\\u30A8=\\u30A8=\\u3048=\\uff74"
        "&[before 3]\\u30a7<<<\\u30a9",
      {"\\u30a9", "\\u30a7"}, 2 },
    {   "&[before 3]\\u30a7<<<\\u30a9"
        "&\\u30A7=\\u30A7=\\u3047=\\uff6a"
        "&\\u30A8=\\u30A8=\\u3048=\\uff74",
      {"\\u30a9", "\\u30a7"}, 2 },
  };
  uint32_t i;


  for(i = 0; i<UPRV_LENGTHOF(tests); i++) {
    genericRulesStarter(tests[i].rules, tests[i].data, tests[i].len);
  }

#if 0
  const char* rule1 =
        "&\\u30A7=\\u30A7=\\u3047=\\uff6a"
        "&\\u30A8=\\u30A8=\\u3048=\\uff74"
        "&[before 3]\\u30a7<<<\\u30c6|\\u30fc";
  const char* rule2 =
        "&[before 3]\\u30a7<<<\\u30c6|\\u30fc"
        "&\\u30A7=\\u30A7=\\u3047=\\uff6a"
        "&\\u30A8=\\u30A8=\\u3048=\\uff74";
  const char* test[] = {
      "\\u30c6\\u30fc\\u30bf",
      "\\u30c6\\u30a7\\u30bf",
  };
  genericRulesStarter(rule1, test, UPRV_LENGTHOF(test));
  genericRulesStarter(rule2, test, UPRV_LENGTHOF(test));
/* this piece of code should be in some sort of verbose mode     */
/* it gets the collation elements for elements and prints them   */
/* This is useful when trying to see whether the problem is      */
  {
    UErrorCode status = U_ZERO_ERROR;
    uint32_t i = 0;
    UCollationElements *it = NULL;
    uint32_t CE;
    UChar string[256];
    uint32_t uStringLen;
    UCollator *coll = NULL;

    uStringLen = u_unescape(rule1, string, 256);

    coll = ucol_openRules(string, uStringLen, UCOL_DEFAULT, UCOL_DEFAULT, NULL, &status);

    /*coll = ucol_open("ja_JP_JIS", &status);*/
    it = ucol_openElements(coll, string, 0, &status);

    for(i = 0; i < UPRV_LENGTHOF(test); i++) {
      log_verbose("%s\n", test[i]);
      uStringLen = u_unescape(test[i], string, 256);
      ucol_setText(it, string, uStringLen, &status);

      while((CE=ucol_next(it, &status)) != UCOL_NULLORDER) {
        log_verbose("%08X\n", CE);
      }
      log_verbose("\n");

    }

    ucol_closeElements(it);
    ucol_close(coll);
  }
#endif
}

static void TestPrefixCompose(void) {
  const char* rule1 =
        "&\\u30a7<<<\\u30ab|\\u30fc=\\u30ac|\\u30fc";
  /*
  const char* test[] = {
      "\\u30c6\\u30fc\\u30bf",
      "\\u30c6\\u30a7\\u30bf",
  };
  */
  {
    UErrorCode status = U_ZERO_ERROR;
    /*uint32_t i = 0;*/
    /*UCollationElements *it = NULL;*/
/*    uint32_t CE;*/
    UChar string[256];
    uint32_t uStringLen;
    UCollator *coll = NULL;

    uStringLen = u_unescape(rule1, string, 256);

    coll = ucol_openRules(string, uStringLen, UCOL_DEFAULT, UCOL_DEFAULT, NULL, &status);
    ucol_close(coll);
  }


}

/*
[last variable] last variable value
[last primary ignorable] largest CE for primary ignorable
[last secondary ignorable] largest CE for secondary ignorable
[last tertiary ignorable] largest CE for tertiary ignorable
[top] guaranteed to be above all implicit CEs, for now and in the future (in 1.8)
*/

static void TestRuleOptions(void) {
  /* values here are hardcoded and are correct for the current UCA
   * when the UCA changes, one might be forced to change these
   * values.
   */

  /*
   * These strings contain the last character before [variable top]
   * and the first and second characters (by primary weights) after it.
   * See FractionalUCA.txt. For example:
      [last variable [0C FE, 05, 05]] # U+10A7F OLD SOUTH ARABIAN NUMERIC INDICATOR
      [variable top = 0C FE]
      [first regular [0D 0A, 05, 05]] # U+0060 GRAVE ACCENT
     and
      00B4; [0D 0C, 05, 05]
   *
   * Note: Starting with UCA 6.0, the [variable top] collation element
   * is not the weight of any character or string,
   * which means that LAST_VARIABLE_CHAR_STRING sorts before [last variable].
   */
#define LAST_VARIABLE_CHAR_STRING "\\U00010A7F"
#define FIRST_REGULAR_CHAR_STRING "\\u0060"
#define SECOND_REGULAR_CHAR_STRING "\\u00B4"

  /*
   * This string has to match the character that has the [last regular] weight
   * which changes with each UCA version.
   * See the bottom of FractionalUCA.txt which says something like
      [last regular [7A FE, 05, 05]] # U+1342E EGYPTIAN HIEROGLYPH AA032
   *
   * Note: Starting with UCA 6.0, the [last regular] collation element
   * is not the weight of any character or string,
   * which means that LAST_REGULAR_CHAR_STRING sorts before [last regular].
   */
#define LAST_REGULAR_CHAR_STRING "\\U0001342E"

  static const struct {
    const char *rules;
    const char *data[10];
    const uint32_t len;
  } tests[] = {
#if 0
    /* "you cannot go before ...": The parser now sets an error for such nonsensical rules. */
    /* - all befores here amount to zero */
    { "&[before 3][first tertiary ignorable]<<<a",
        { "\\u0000", "a"}, 2
    }, /* you cannot go before first tertiary ignorable */

    { "&[before 3][last tertiary ignorable]<<<a",
        { "\\u0000", "a"}, 2
    }, /* you cannot go before last tertiary ignorable */
#endif
    /*
     * However, there is a real secondary ignorable (artificial addition in FractionalUCA.txt),
     * and it *is* possible to "go before" that.
     */
    { "&[before 3][first secondary ignorable]<<<a",
        { "\\u0000", "a"}, 2
    },

    { "&[before 3][last secondary ignorable]<<<a",
        { "\\u0000", "a"}, 2
    },

    /* 'normal' befores */

    /*
     * Note: With a "SPACE first primary" boundary CE in FractionalUCA.txt,
     * it is not possible to tailor &[first primary ignorable]<a or &[last primary ignorable]<a
     * because there is no tailoring space before that boundary.
     * Made the tests work by tailoring to a space instead.
     */
    { "&[before 3][first primary ignorable]<<<c<<<b &' '<a",  /* was &[first primary ignorable]<a */
        {  "c", "b", "\\u0332", "a" }, 4
    },

    /* we don't have a code point that corresponds to
     * the last primary ignorable
     */
    { "&[before 3][last primary ignorable]<<<c<<<b &' '<a",  /* was &[last primary ignorable]<a */
        {  "\\u0332", "\\u20e3", "c", "b", "a" }, 5
    },

    { "&[before 3][first variable]<<<c<<<b &[first variable]<a",
        {  "c", "b", "\\u0009", "a", "\\u000a" }, 5
    },

    { "&[last variable]<a &[before 3][last variable]<<<c<<<b ",
        { LAST_VARIABLE_CHAR_STRING, "c", "b", /* [last variable] */ "a", FIRST_REGULAR_CHAR_STRING }, 5
    },

    { "&[first regular]<a"
      "&[before 1][first regular]<b",
      { "b", FIRST_REGULAR_CHAR_STRING, "a", SECOND_REGULAR_CHAR_STRING }, 4
    },

    { "&[before 1][last regular]<b"
      "&[last regular]<a",
        { LAST_REGULAR_CHAR_STRING, "b", /* [last regular] */ "a", "\\u4e00" }, 4
    },

    { "&[before 1][first implicit]<b"
      "&[first implicit]<a",
        { "b", "\\u4e00", "a", "\\u4e01"}, 4
    },
#if 0  /* The current builder does not support tailoring to unassigned-implicit CEs (seems unnecessary, adds complexity). */
    { "&[before 1][last implicit]<b"
      "&[last implicit]<a",
        { "b", "\\U0010FFFD", "a" }, 3
    },
#endif
    { "&[last variable]<z"
      "&' '<x"  /* was &[last primary ignorable]<x, see above */
      "&[last secondary ignorable]<<y"
      "&[last tertiary ignorable]<<<w"
      "&[top]<u",
      {"\\ufffb",  "w", "y", "\\u20e3", "x", LAST_VARIABLE_CHAR_STRING, "z", "u"}, 7
    }

  };
  uint32_t i;

  for(i = 0; i<UPRV_LENGTHOF(tests); i++) {
    genericRulesStarter(tests[i].rules, tests[i].data, tests[i].len);
  }
}


static void TestOptimize(void) {
  /* this is not really a test - just trying out
   * whether copying of UCA contents will fail
   * Cannot really test, since the functionality
   * remains the same.
   */
  static const struct {
    const char *rules;
    const char *data[10];
    const uint32_t len;
  } tests[] = {
    /* - all befores here amount to zero */
    { "[optimize [\\uAC00-\\uD7FF]]",
    { "a", "b"}, 2}
  };
  uint32_t i;

  for(i = 0; i<UPRV_LENGTHOF(tests); i++) {
    genericRulesStarter(tests[i].rules, tests[i].data, tests[i].len);
  }
}

/*
cycheng@ca.ibm.c... we got inconsistent results when using the UTF-16BE iterator and the UTF-8 iterator.
weiv    ucol_strcollIter?
cycheng@ca.ibm.c... e.g. s1 = 0xfffc0062, and s2 = d8000021
weiv    these are the input strings?
cycheng@ca.ibm.c... yes, using the utf-16 iterator and UCA with normalization on, we have s1 > s2
weiv    will check - could be a problem with utf-8 iterator
cycheng@ca.ibm.c... but if we use the utf-8 iterator, i.e. s1 = efbfbc62 and s2 = eda08021, we have s1 < s2
weiv    hmmm
cycheng@ca.ibm.c... note that we have a standalone high surrogate
weiv    that doesn't sound right
cycheng@ca.ibm.c... we got the same inconsistent results on AIX and Win2000
weiv    so you have two strings, you convert them to utf-8 and to utf-16BE
cycheng@ca.ibm.c... yes
weiv    and then do the comparison
cycheng@ca.ibm.c... in one case, the input strings are in utf8, and in the other case the input strings are in utf-16be
weiv    utf-16 strings look like a little endian ones in the example you sent me
weiv    It could be a bug - let me try to test it out
cycheng@ca.ibm.c... ok
cycheng@ca.ibm.c... we can wait till the conf. call
cycheng@ca.ibm.c... next weke
weiv    that would be great
weiv    hmmm
weiv    I might be wrong
weiv    let me play with it some more
cycheng@ca.ibm.c... ok
cycheng@ca.ibm.c... also please check s3 = 0x0e3a0062  and s4 = 0x0e400021. both are in utf-16be
cycheng@ca.ibm.c... seems with icu 2.2 we have s3 > s4, but not in icu 2.4 that's built for db2
cycheng@ca.ibm.c... also s1 & s2 that I sent you earlier are also in utf-16be
weiv    ok
cycheng@ca.ibm.c... i ask sherman to send you more inconsistent data
weiv    thanks
cycheng@ca.ibm.c... the 4 strings we sent are just samples
*/
#if 0
static void Alexis(void) {
  UErrorCode status = U_ZERO_ERROR;
  UCollator *coll = ucol_open("", &status);


  const char utf16be[2][4] = {
    { (char)0xd8, (char)0x00, (char)0x00, (char)0x21 },
    { (char)0xff, (char)0xfc, (char)0x00, (char)0x62 }
  };

  const char utf8[2][4] = {
    { (char)0xed, (char)0xa0, (char)0x80, (char)0x21 },
    { (char)0xef, (char)0xbf, (char)0xbc, (char)0x62 },
  };

  UCharIterator iterU161, iterU162;
  UCharIterator iterU81, iterU82;

  UCollationResult resU16, resU8;

  uiter_setUTF16BE(&iterU161, utf16be[0], 4);
  uiter_setUTF16BE(&iterU162, utf16be[1], 4);

  uiter_setUTF8(&iterU81, utf8[0], 4);
  uiter_setUTF8(&iterU82, utf8[1], 4);

  ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);

  resU16 = ucol_strcollIter(coll, &iterU161, &iterU162, &status);
  resU8 = ucol_strcollIter(coll, &iterU81, &iterU82, &status);


  if(resU16 != resU8) {
    log_err("different results\n");
  }

  ucol_close(coll);
}
#endif

#define CMSCOLL_ALEXIS2_BUFFER_SIZE 256
static void Alexis2(void) {
  UErrorCode status = U_ZERO_ERROR;
  UChar U16Source[CMSCOLL_ALEXIS2_BUFFER_SIZE], U16Target[CMSCOLL_ALEXIS2_BUFFER_SIZE];
  char U16BESource[CMSCOLL_ALEXIS2_BUFFER_SIZE], U16BETarget[CMSCOLL_ALEXIS2_BUFFER_SIZE];
  char U8Source[CMSCOLL_ALEXIS2_BUFFER_SIZE], U8Target[CMSCOLL_ALEXIS2_BUFFER_SIZE];
  int32_t U16LenS = 0, U16LenT = 0, U16BELenS = 0, U16BELenT = 0, U8LenS = 0, U8LenT = 0;

  UConverter *conv = NULL;

  UCharIterator U16BEItS, U16BEItT;
  UCharIterator U8ItS, U8ItT;

  UCollationResult resU16, resU16BE, resU8;

  static const char* const pairs[][2] = {
    { "\\ud800\\u0021", "\\uFFFC\\u0062"},
    { "\\u0435\\u0308\\u0334", "\\u0415\\u0334\\u0340" },
    { "\\u0E40\\u0021", "\\u00A1\\u0021"},
    { "\\u0E40\\u0021", "\\uFE57\\u0062"},
    { "\\u5F20", "\\u5F20\\u4E00\\u8E3F"},
    { "\\u0000\\u0020", "\\u0000\\u0020\\u0000"},
    { "\\u0020", "\\u0020\\u0000"}
/*
5F20 (my result here)
5F204E008E3F
5F20 (your result here)
*/
  };

  int32_t i = 0;

  UCollator *coll = ucol_open("", &status);
  if(status == U_FILE_ACCESS_ERROR) {
    log_data_err("Is your data around?\n");
    return;
  } else if(U_FAILURE(status)) {
    log_err("Error opening collator\n");
    return;
  }
  ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
  conv = ucnv_open("UTF16BE", &status);
  for(i = 0; i < UPRV_LENGTHOF(pairs); i++) {
    U16LenS = u_unescape(pairs[i][0], U16Source, CMSCOLL_ALEXIS2_BUFFER_SIZE);
    U16LenT = u_unescape(pairs[i][1], U16Target, CMSCOLL_ALEXIS2_BUFFER_SIZE);

    resU16 = ucol_strcoll(coll, U16Source, U16LenS, U16Target, U16LenT);

    log_verbose("Result of strcoll is %i\n", resU16);

    U16BELenS = ucnv_fromUChars(conv, U16BESource, CMSCOLL_ALEXIS2_BUFFER_SIZE, U16Source, U16LenS, &status);
    U16BELenT = ucnv_fromUChars(conv, U16BETarget, CMSCOLL_ALEXIS2_BUFFER_SIZE, U16Target, U16LenT, &status);
    (void)U16BELenS;    /* Suppress set but not used warnings. */
    (void)U16BELenT;

    /* use the original sizes, as the result from converter is in bytes */
    uiter_setUTF16BE(&U16BEItS, U16BESource, U16LenS);
    uiter_setUTF16BE(&U16BEItT, U16BETarget, U16LenT);

    resU16BE = ucol_strcollIter(coll, &U16BEItS, &U16BEItT, &status);

    log_verbose("Result of U16BE is %i\n", resU16BE);

    if(resU16 != resU16BE) {
      log_verbose("Different results between UTF16 and UTF16BE for %s & %s\n", pairs[i][0], pairs[i][1]);
    }

    u_strToUTF8(U8Source, CMSCOLL_ALEXIS2_BUFFER_SIZE, &U8LenS, U16Source, U16LenS, &status);
    u_strToUTF8(U8Target, CMSCOLL_ALEXIS2_BUFFER_SIZE, &U8LenT, U16Target, U16LenT, &status);

    uiter_setUTF8(&U8ItS, U8Source, U8LenS);
    uiter_setUTF8(&U8ItT, U8Target, U8LenT);

    resU8 = ucol_strcollIter(coll, &U8ItS, &U8ItT, &status);

    if(resU16 != resU8) {
      log_verbose("Different results between UTF16 and UTF8 for %s & %s\n", pairs[i][0], pairs[i][1]);
    }

  }

  ucol_close(coll);
  ucnv_close(conv);
}

static void TestHebrewUCA(void) {
  UErrorCode status = U_ZERO_ERROR;
  static const char *first[] = {
    "d790d6b8d79cd795d6bcd7a9",
    "d790d79cd79ed7a7d799d799d7a1",
    "d790d6b4d79ed795d6bcd7a9",
  };

  char utf8String[3][256];
  UChar utf16String[3][256];

  int32_t i = 0, j = 0;
  int32_t sizeUTF8[3];
  int32_t sizeUTF16[3];

  UCollator *coll = ucol_open("", &status);
  if (U_FAILURE(status)) {
      log_err_status(status, "Could not open UCA collation %s\n", u_errorName(status));
      return;
  }
  /*ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);*/

  for(i = 0; i < UPRV_LENGTHOF(first); i++) {
    sizeUTF8[i] = u_parseUTF8(first[i], -1, utf8String[i], 256, &status);
    u_strFromUTF8(utf16String[i], 256, &sizeUTF16[i], utf8String[i], sizeUTF8[i], &status);
    log_verbose("%i: ");
    for(j = 0; j < sizeUTF16[i]; j++) {
      /*log_verbose("\\u%04X", utf16String[i][j]);*/
      log_verbose("%04X", utf16String[i][j]);
    }
    log_verbose("\n");
  }
  for(i = 0; i < UPRV_LENGTHOF(first)-1; i++) {
    for(j = i + 1; j < UPRV_LENGTHOF(first); j++) {
      doTest(coll, utf16String[i], utf16String[j], UCOL_LESS);
    }
  }

  ucol_close(coll);

}

static void TestPartialSortKeyTermination(void) {
  static const char* cases[] = {
    "\\u1234\\u1234\\udc00",
    "\\udc00\\ud800\\ud800"
  };

  int32_t i;

  UErrorCode status = U_ZERO_ERROR;

  UCollator *coll = ucol_open("", &status);

  UCharIterator iter;

  UChar currCase[256];
  int32_t length = 0;
  int32_t pKeyLen = 0;

  uint8_t key[256];

  for(i = 0; i < UPRV_LENGTHOF(cases); i++) {
    uint32_t state[2] = {0, 0};
    length = u_unescape(cases[i], currCase, 256);
    uiter_setString(&iter, currCase, length);
    pKeyLen = ucol_nextSortKeyPart(coll, &iter, state, key, 256, &status);
    (void)pKeyLen;   /* Suppress set but not used warning. */

    log_verbose("Done\n");

  }
  ucol_close(coll);
}

static void TestSettings(void) {
  static const char* cases[] = {
    "apple",
      "Apple"
  };

  static const char* locales[] = {
    "",
      "en"
  };

  UErrorCode status = U_ZERO_ERROR;

  int32_t i = 0, j = 0;

  UChar source[256], target[256];
  int32_t sLen = 0, tLen = 0;

  UCollator *collateObject = NULL;
  for(i = 0; i < UPRV_LENGTHOF(locales); i++) {
    collateObject = ucol_open(locales[i], &status);
    ucol_setStrength(collateObject, UCOL_PRIMARY);
    ucol_setAttribute(collateObject, UCOL_CASE_LEVEL , UCOL_OFF, &status);
    for(j = 1; j < UPRV_LENGTHOF(cases); j++) {
      sLen = u_unescape(cases[j-1], source, 256);
      source[sLen] = 0;
      tLen = u_unescape(cases[j], target, 256);
      source[tLen] = 0;
      doTest(collateObject, source, target, UCOL_EQUAL);
    }
    ucol_close(collateObject);
  }
}

static int32_t TestEqualsForCollator(const char* locName, UCollator *source, UCollator *target) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t errorNo = 0;
    const UChar *sourceRules = NULL;
    int32_t sourceRulesLen = 0;
    UParseError parseError;
    UColAttributeValue french = UCOL_OFF;

    if(!ucol_equals(source, target)) {
        log_err("Same collators, different address not equal\n");
        errorNo++;
    }
    ucol_close(target);
    if(uprv_strcmp(locName, ucol_getLocaleByType(source, ULOC_ACTUAL_LOCALE, &status)) == 0) {
        target = ucol_safeClone(source, NULL, NULL, &status);
        if(U_FAILURE(status)) {
            log_err("Error creating clone\n");
            errorNo++;
            return errorNo;
        }
        if(!ucol_equals(source, target)) {
            log_err("Collator different from it's clone\n");
            errorNo++;
        }
        french = ucol_getAttribute(source, UCOL_FRENCH_COLLATION, &status);
        if(french == UCOL_ON) {
            ucol_setAttribute(target, UCOL_FRENCH_COLLATION, UCOL_OFF, &status);
        } else {
            ucol_setAttribute(target, UCOL_FRENCH_COLLATION, UCOL_ON, &status);
        }
        if(U_FAILURE(status)) {
            log_err("Error setting attributes\n");
            errorNo++;
            return errorNo;
        }
        if(ucol_equals(source, target)) {
            log_err("Collators same even when options changed\n");
            errorNo++;
        }
        ucol_close(target);

        sourceRules = ucol_getRules(source, &sourceRulesLen);
        target = ucol_openRules(sourceRules, sourceRulesLen, UCOL_DEFAULT, UCOL_DEFAULT, &parseError, &status);
        if(U_FAILURE(status)) {
            log_err("Error instantiating target from rules - %s\n", u_errorName(status));
            errorNo++;
            return errorNo;
        }
        /* Note: The tailoring rule string is an optional data item. */
        if(!ucol_equals(source, target) && sourceRulesLen != 0) {
            log_err("Collator different from collator that was created from the same rules\n");
            errorNo++;
        }
        ucol_close(target);
    }
    return errorNo;
}


static void TestEquals(void) {
    /* ucol_equals is not currently a public API. There is a chance that it will become
    * something like this.
    */
    /* test whether the two collators instantiated from the same locale are equal */
    UErrorCode status = U_ZERO_ERROR;
    UParseError parseError;
    int32_t noOfLoc = uloc_countAvailable();
    const char *locName = NULL;
    UCollator *source = NULL, *target = NULL;
    int32_t i = 0;

    const char* rules[] = {
        "&l < lj <<< Lj <<< LJ",
        "&n < nj <<< Nj <<< NJ",
        "&ae <<< \\u00e4",
        "&AE <<< \\u00c4"
    };
    /*
    const char* badRules[] = {
    "&l <<< Lj",
    "&n < nj <<< nJ <<< NJ",
    "&a <<< \\u00e4",
    "&AE <<< \\u00c4 <<< x"
    };
    */

    UChar sourceRules[1024], targetRules[1024];
    int32_t sourceRulesSize = 0, targetRulesSize = 0;
    int32_t rulesSize = UPRV_LENGTHOF(rules);

    for(i = 0; i < rulesSize; i++) {
        sourceRulesSize += u_unescape(rules[i], sourceRules+sourceRulesSize, 1024 - sourceRulesSize);
        targetRulesSize += u_unescape(rules[rulesSize-i-1], targetRules+targetRulesSize, 1024 - targetRulesSize);
    }

    source = ucol_openRules(sourceRules, sourceRulesSize, UCOL_DEFAULT, UCOL_DEFAULT, &parseError, &status);
    if(status == U_FILE_ACCESS_ERROR) {
        log_data_err("Is your data around?\n");
        return;
    } else if(U_FAILURE(status)) {
        log_err("Error opening collator\n");
        return;
    }
    target = ucol_openRules(targetRules, targetRulesSize, UCOL_DEFAULT, UCOL_DEFAULT, &parseError, &status);
    if(!ucol_equals(source, target)) {
        log_err("Equivalent collators not equal!\n");
    }
    ucol_close(source);
    ucol_close(target);

    source = ucol_open("root", &status);
    target = ucol_open("root", &status);
    log_verbose("Testing root\n");
    if(!ucol_equals(source, source)) {
        log_err("Same collator not equal\n");
    }
    if(TestEqualsForCollator("root", source, target)) {
        log_err("Errors for root\n");
    }
    ucol_close(source);

    for(i = 0; i<noOfLoc; i++) {
        status = U_ZERO_ERROR;
        locName = uloc_getAvailable(i);
        /*if(hasCollationElements(locName)) {*/
        log_verbose("Testing equality for locale %s\n", locName);
        source = ucol_open(locName, &status);
        target = ucol_open(locName, &status);
        if (U_FAILURE(status)) {
            log_err("Error opening collator for locale %s  %s\n", locName, u_errorName(status));
            continue;
        }
        if(TestEqualsForCollator(locName, source, target)) {
            log_err("Errors for locale %s\n", locName);
        }
        ucol_close(source);
        /*}*/
    }
}

static void TestJ2726(void) {
    UChar a[2] = { 0x61, 0x00 }; /*"a"*/
    UChar aSpace[3] = { 0x61, 0x20, 0x00 }; /*"a "*/
    UChar spaceA[3] = { 0x20, 0x61, 0x00 }; /*" a"*/
    UErrorCode status = U_ZERO_ERROR;
    UCollator *coll = ucol_open("en", &status);
    ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, &status);
    ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_PRIMARY, &status);
    doTest(coll, a, aSpace, UCOL_EQUAL);
    doTest(coll, aSpace, a, UCOL_EQUAL);
    doTest(coll, a, spaceA, UCOL_EQUAL);
    doTest(coll, spaceA, a, UCOL_EQUAL);
    doTest(coll, spaceA, aSpace, UCOL_EQUAL);
    doTest(coll, aSpace, spaceA, UCOL_EQUAL);
    ucol_close(coll);
}

static void NullRule(void) {
    UChar r[3] = {0};
    UErrorCode status = U_ZERO_ERROR;
    UCollator *coll = ucol_openRules(r, 1, UCOL_DEFAULT, UCOL_DEFAULT, NULL, &status);
    if(U_SUCCESS(status)) {
        log_err("This should have been an error!\n");
        ucol_close(coll);
    } else {
        status = U_ZERO_ERROR;
    }
    coll = ucol_openRules(r, 0, UCOL_DEFAULT, UCOL_DEFAULT, NULL, &status);
    if(U_FAILURE(status)) {
        log_err_status(status, "Empty rules should have produced a valid collator -> %s\n", u_errorName(status));
    } else {
        ucol_close(coll);
    }
}

/**
 * Test for CollationElementIterator previous and next for the whole set of
 * unicode characters with normalization on.
 */
static void TestNumericCollation(void)
{
    UErrorCode status = U_ZERO_ERROR;

    const static char *basicTestStrings[]={
    "hello1",
    "hello2",
    "hello2002",
    "hello2003",
    "hello123456",
    "hello1234567",
    "hello10000000",
    "hello100000000",
    "hello1000000000",
    "hello10000000000",
    };

    const static char *preZeroTestStrings[]={
    "avery10000",
    "avery010000",
    "avery0010000",
    "avery00010000",
    "avery000010000",
    "avery0000010000",
    "avery00000010000",
    "avery000000010000",
    };

    const static char *thirtyTwoBitNumericStrings[]={
    "avery42949672960",
    "avery42949672961",
    "avery42949672962",
    "avery429496729610"
    };

     const static char *longNumericStrings[]={
     /* Some of these sort out of the order that would expected if digits-as-numbers handled arbitrarily-long digit strings.
        In fact, a single collation element can represent a maximum of 254 digits as a number. Digit strings longer than that
        are treated as multiple collation elements. */
    "num9234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123z", /*253digits, num + 9.23E252 + z */
    "num10000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", /*254digits, num + 1.00E253 */
    "num100000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000", /*255digits, num + 1.00E253 + 0, out of numeric order but expected */
    "num12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234", /*254digits, num + 1.23E253 */
    "num123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345", /*255digits, num + 1.23E253 + 5 */
    "num1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456", /*256digits, num + 1.23E253 + 56 */
    "num12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567", /*257digits, num + 1.23E253 + 567 */
    "num12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234a", /*254digits, num + 1.23E253 + a, out of numeric order but expected */
    "num92345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234", /*254digits, num + 9.23E253, out of numeric order but expected */
    "num92345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234a", /*254digits, num + 9.23E253 + a, out of numeric order but expected */
    };

    const static char *supplementaryDigits[] = {
      "\\uD835\\uDFCE", /* 0 */
      "\\uD835\\uDFCF", /* 1 */
      "\\uD835\\uDFD0", /* 2 */
      "\\uD835\\uDFD1", /* 3 */
      "\\uD835\\uDFCF\\uD835\\uDFCE", /* 10 */
      "\\uD835\\uDFCF\\uD835\\uDFCF", /* 11 */
      "\\uD835\\uDFCF\\uD835\\uDFD0", /* 12 */
      "\\uD835\\uDFD0\\uD835\\uDFCE", /* 20 */
      "\\uD835\\uDFD0\\uD835\\uDFCF", /* 21 */
      "\\uD835\\uDFD0\\uD835\\uDFD0" /* 22 */
    };

    const static char *foreignDigits[] = {
      "\\u0661",
        "\\u0662",
        "\\u0663",
      "\\u0661\\u0660",
      "\\u0661\\u0662",
      "\\u0661\\u0663",
      "\\u0662\\u0660",
      "\\u0662\\u0662",
      "\\u0662\\u0663",
      "\\u0663\\u0660",
      "\\u0663\\u0662",
      "\\u0663\\u0663"
    };

    const static char *evenZeroes[] = {
      "2000",
      "2001",
        "2002",
        "2003"
    };

    UColAttribute att = UCOL_NUMERIC_COLLATION;
    UColAttributeValue val = UCOL_ON;

    /* Open our collator. */
    UCollator* coll = ucol_open("root", &status);
    if (U_FAILURE(status)){
        log_err_status(status, "ERROR: in using ucol_open() -> %s\n",
              myErrorName(status));
        return;
    }
    genericLocaleStarterWithOptions("root", basicTestStrings, UPRV_LENGTHOF(basicTestStrings), &att, &val, 1);
    genericLocaleStarterWithOptions("root", thirtyTwoBitNumericStrings, UPRV_LENGTHOF(thirtyTwoBitNumericStrings), &att, &val, 1);
    genericLocaleStarterWithOptions("root", longNumericStrings, UPRV_LENGTHOF(longNumericStrings), &att, &val, 1);
    genericLocaleStarterWithOptions("en_US", foreignDigits, UPRV_LENGTHOF(foreignDigits), &att, &val, 1);
    genericLocaleStarterWithOptions("root", supplementaryDigits, UPRV_LENGTHOF(supplementaryDigits), &att, &val, 1);
    genericLocaleStarterWithOptions("root", evenZeroes, UPRV_LENGTHOF(evenZeroes), &att, &val, 1);

    /* Setting up our collator to do digits. */
    ucol_setAttribute(coll, UCOL_NUMERIC_COLLATION, UCOL_ON, &status);
    if (U_FAILURE(status)){
        log_err("ERROR: in setting UCOL_NUMERIC_COLLATION as an attribute\n %s\n",
              myErrorName(status));
        return;
    }

    /*
       Testing that prepended zeroes still yield the correct collation behavior.
       We expect that every element in our strings array will be equal.
    */
    genericOrderingTestWithResult(coll, preZeroTestStrings, UPRV_LENGTHOF(preZeroTestStrings), UCOL_EQUAL);

    ucol_close(coll);
}

static void TestTibetanConformance(void)
{
    const char* test[] = {
        "\\u0FB2\\u0591\\u0F71\\u0061",
        "\\u0FB2\\u0F71\\u0061"
    };

    UErrorCode status = U_ZERO_ERROR;
    UCollator *coll = ucol_open("", &status);
    UChar source[100];
    UChar target[100];
    int result;
    ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    if (U_SUCCESS(status)) {
        u_unescape(test[0], source, 100);
        u_unescape(test[1], target, 100);
        doTest(coll, source, target, UCOL_EQUAL);
        result = ucol_strcoll(coll, source, -1,   target, -1);
        log_verbose("result %d\n", result);
        if (UCOL_EQUAL != result) {
            log_err("Tibetan comparison error\n");
        }
    }
    ucol_close(coll);

    genericLocaleStarterWithResult("", test, 2, UCOL_EQUAL);
}

static void TestPinyinProblem(void) {
    static const char *test[] = { "\\u4E56\\u4E56\\u7761", "\\u4E56\\u5B69\\u5B50" };
    genericLocaleStarter("zh__PINYIN", test, UPRV_LENGTHOF(test));
}

/**
 * Iterate through the given iterator, checking to see that all the strings
 * in the expected array are present.
 * @param expected array of strings we expect to see, or NULL
 * @param expectedCount number of elements of expected, or 0
 */
static int32_t checkUEnumeration(const char* msg,
                                 UEnumeration* iter,
                                 const char** expected,
                                 int32_t expectedCount) {
    UErrorCode ec = U_ZERO_ERROR;
    int32_t i = 0, n, j, bit;
    int32_t seenMask = 0;

    U_ASSERT(expectedCount >= 0 && expectedCount < 31); /* [sic] 31 not 32 */
    n = uenum_count(iter, &ec);
    if (!assertSuccess("count", &ec)) return -1;
    log_verbose("%s = [", msg);
    for (;; ++i) {
        const char* s = uenum_next(iter, NULL, &ec);
        if (!assertSuccess("snext", &ec) || s == NULL) break;
        if (i != 0) log_verbose(",");
        log_verbose("%s", s);
        /* check expected list */
        for (j=0, bit=1; j<expectedCount; ++j, bit<<=1) {
            if ((seenMask&bit) == 0 &&
                uprv_strcmp(s, expected[j]) == 0) {
                seenMask |= bit;
                break;
            }
        }
    }
    log_verbose("] (%d)\n", i);
    assertTrue("count verified", i==n);
    /* did we see all expected strings? */
    for (j=0, bit=1; j<expectedCount; ++j, bit<<=1) {
        if ((seenMask&bit)!=0) {
            log_verbose("Ok: \"%s\" seen\n", expected[j]);
        } else {
            log_err("FAIL: \"%s\" not seen\n", expected[j]);
        }
    }
    return n;
}

/**
 * Test new API added for separate collation tree.
 */
static void TestSeparateTrees(void) {
    UErrorCode ec = U_ZERO_ERROR;
    UEnumeration *e = NULL;
    int32_t n = -1;
    UBool isAvailable;
    char loc[256];

    static const char* AVAIL[] = { "en", "de" };

    static const char* KW[] = { "collation" };

    static const char* KWVAL[] = { "phonebook", "stroke" };

#if !UCONFIG_NO_SERVICE
    e = ucol_openAvailableLocales(&ec);
    if (e != NULL) {
        assertSuccess("ucol_openAvailableLocales", &ec);
        assertTrue("ucol_openAvailableLocales!=0", e!=0);
        n = checkUEnumeration("ucol_openAvailableLocales", e, AVAIL, UPRV_LENGTHOF(AVAIL));
        (void)n;    /* Suppress set but not used warnings. */
        /* Don't need to check n because we check list */
        uenum_close(e);
    } else {
        log_data_err("Error calling ucol_openAvailableLocales() -> %s (Are you missing data?)\n", u_errorName(ec));
    }
#endif

    e = ucol_getKeywords(&ec);
    if (e != NULL) {
        assertSuccess("ucol_getKeywords", &ec);
        assertTrue("ucol_getKeywords!=0", e!=0);
        n = checkUEnumeration("ucol_getKeywords", e, KW, UPRV_LENGTHOF(KW));
        /* Don't need to check n because we check list */
        uenum_close(e);
    } else {
        log_data_err("Error calling ucol_getKeywords() -> %s (Are you missing data?)\n", u_errorName(ec));
    }

    e = ucol_getKeywordValues(KW[0], &ec);
    if (e != NULL) {
        assertSuccess("ucol_getKeywordValues", &ec);
        assertTrue("ucol_getKeywordValues!=0", e!=0);
        n = checkUEnumeration("ucol_getKeywordValues", e, KWVAL, UPRV_LENGTHOF(KWVAL));
        /* Don't need to check n because we check list */
        uenum_close(e);
    } else {
        log_data_err("Error calling ucol_getKeywordValues() -> %s (Are you missing data?)\n", u_errorName(ec));
    }

    /* Try setting a warning before calling ucol_getKeywordValues */
    ec = U_USING_FALLBACK_WARNING;
    e = ucol_getKeywordValues(KW[0], &ec);
    if (assertSuccess("ucol_getKeywordValues [with warning code set]", &ec)) {
        assertTrue("ucol_getKeywordValues!=0 [with warning code set]", e!=0);
        n = checkUEnumeration("ucol_getKeywordValues [with warning code set]", e, KWVAL, UPRV_LENGTHOF(KWVAL));
        /* Don't need to check n because we check list */
        uenum_close(e);
    }

    /*
U_CAPI int32_t U_EXPORT2
ucol_getFunctionalEquivalent(char* result, int32_t resultCapacity,
                             const char* locale, UBool* isAvailable,
                             UErrorCode* status);
}
*/
    n = ucol_getFunctionalEquivalent(loc, sizeof(loc), "collation", "de",
                                     &isAvailable, &ec);
    if (assertSuccess("getFunctionalEquivalent", &ec)) {
        assertEquals("getFunctionalEquivalent(de)", "root", loc);
        assertTrue("getFunctionalEquivalent(de).isAvailable==true",
                   isAvailable == true);
    }

    n = ucol_getFunctionalEquivalent(loc, sizeof(loc), "collation", "de_DE",
                                     &isAvailable, &ec);
    if (assertSuccess("getFunctionalEquivalent", &ec)) {
        assertEquals("getFunctionalEquivalent(de_DE)", "root", loc);
        assertTrue("getFunctionalEquivalent(de_DE).isAvailable==false",
                   isAvailable == false);
    }
}

/* supercedes TestJ784 */
static void TestBeforePinyin(void) {
    const static char rules[] = {
        "&[before 2]A<<\\u0101<<<\\u0100<<\\u00E1<<<\\u00C1<<\\u01CE<<<\\u01CD<<\\u00E0<<<\\u00C0"
        "&[before 2]e<<\\u0113<<<\\u0112<<\\u00E9<<<\\u00C9<<\\u011B<<<\\u011A<<\\u00E8<<<\\u00C8"
        "&[before 2]i<<\\u012B<<<\\u012A<<\\u00ED<<<\\u00CD<<\\u01D0<<<\\u01CF<<\\u00EC<<<\\u00CC"
        "&[before 2]o<<\\u014D<<<\\u014C<<\\u00F3<<<\\u00D3<<\\u01D2<<<\\u01D1<<\\u00F2<<<\\u00D2"
        "&[before 2]u<<\\u016B<<<\\u016A<<\\u00FA<<<\\u00DA<<\\u01D4<<<\\u01D3<<\\u00F9<<<\\u00D9"
        "&U<<\\u01D6<<<\\u01D5<<\\u01D8<<<\\u01D7<<\\u01DA<<<\\u01D9<<\\u01DC<<<\\u01DB<<\\u00FC"
    };

    const static char *test[] = {
        "l\\u0101",
        "la",
        "l\\u0101n",
        "lan ",
        "l\\u0113",
        "le",
        "l\\u0113n",
        "len"
    };

    const static char *test2[] = {
        "x\\u0101",
        "x\\u0100",
        "X\\u0101",
        "X\\u0100",
        "x\\u00E1",
        "x\\u00C1",
        "X\\u00E1",
        "X\\u00C1",
        "x\\u01CE",
        "x\\u01CD",
        "X\\u01CE",
        "X\\u01CD",
        "x\\u00E0",
        "x\\u00C0",
        "X\\u00E0",
        "X\\u00C0",
        "xa",
        "xA",
        "Xa",
        "XA",
        "x\\u0101x",
        "x\\u0100x",
        "x\\u00E1x",
        "x\\u00C1x",
        "x\\u01CEx",
        "x\\u01CDx",
        "x\\u00E0x",
        "x\\u00C0x",
        "xax",
        "xAx"
    };

    genericRulesStarter(rules, test, UPRV_LENGTHOF(test));
    genericLocaleStarter("zh", test, UPRV_LENGTHOF(test));
    genericRulesStarter(rules, test2, UPRV_LENGTHOF(test2));
    genericLocaleStarter("zh", test2, UPRV_LENGTHOF(test2));
}

static void TestBeforeTightening(void) {
    static const struct {
        const char *rules;
        UErrorCode expectedStatus;
    } tests[] = {
        { "&[before 1]a<x", U_ZERO_ERROR },
        { "&[before 1]a<<x", U_INVALID_FORMAT_ERROR },
        { "&[before 1]a<<<x", U_INVALID_FORMAT_ERROR },
        { "&[before 1]a=x", U_INVALID_FORMAT_ERROR },
        { "&[before 2]a<x",U_INVALID_FORMAT_ERROR },
        { "&[before 2]a<<x",U_ZERO_ERROR },
        { "&[before 2]a<<<x",U_INVALID_FORMAT_ERROR },
        { "&[before 2]a=x",U_INVALID_FORMAT_ERROR },
        { "&[before 3]a<x",U_INVALID_FORMAT_ERROR  },
        { "&[before 3]a<<x",U_INVALID_FORMAT_ERROR  },
        { "&[before 3]a<<<x",U_ZERO_ERROR },
        { "&[before 3]a=x",U_INVALID_FORMAT_ERROR  },
        { "&[before I]a = x",U_INVALID_FORMAT_ERROR }
    };

    int32_t i = 0;

    UErrorCode status = U_ZERO_ERROR;
    UChar rlz[RULE_BUFFER_LEN] = { 0 };
    uint32_t rlen = 0;

    UCollator *coll = NULL;


    for(i = 0; i < UPRV_LENGTHOF(tests); i++) {
        rlen = u_unescape(tests[i].rules, rlz, RULE_BUFFER_LEN);
        coll = ucol_openRules(rlz, rlen, UCOL_DEFAULT, UCOL_DEFAULT,NULL, &status);
        if(status != tests[i].expectedStatus) {
            log_err_status(status, "Opening a collator with rules %s returned error code %s, expected %s\n",
                tests[i].rules, u_errorName(status), u_errorName(tests[i].expectedStatus));
        }
        ucol_close(coll);
        status = U_ZERO_ERROR;
    }

}

/*
&m < a
&[before 1] a < x <<< X << q <<< Q < z
assert: m <<< M < x <<< X << q <<< Q < z < a < n

&m < a
&[before 2] a << x <<< X << q <<< Q < z
assert: m <<< M < x <<< X << q <<< Q << a < z < n

&m < a
&[before 3] a <<< x <<< X << q <<< Q < z
assert: m <<< M < x <<< X <<< a << q <<< Q < z < n


&m << a
&[before 1] a < x <<< X << q <<< Q < z
assert: x <<< X << q <<< Q < z < m <<< M << a < n

&m << a
&[before 2] a << x <<< X << q <<< Q < z
assert: m <<< M << x <<< X << q <<< Q << a < z < n

&m << a
&[before 3] a <<< x <<< X << q <<< Q < z
assert: m <<< M << x <<< X <<< a << q <<< Q < z < n


&m <<< a
&[before 1] a < x <<< X << q <<< Q < z
assert: x <<< X << q <<< Q < z < n < m <<< a <<< M

&m <<< a
&[before 2] a << x <<< X << q <<< Q < z
assert:  x <<< X << q <<< Q << m <<< a <<< M < z < n

&m <<< a
&[before 3] a <<< x <<< X << q <<< Q < z
assert: m <<< x <<< X <<< a <<< M  << q <<< Q < z < n


&[before 1] s < x <<< X << q <<< Q < z
assert: r <<< R < x <<< X << q <<< Q < z < s < n

&[before 2] s << x <<< X << q <<< Q < z
assert: r <<< R < x <<< X << q <<< Q << s < z < n

&[before 3] s <<< x <<< X << q <<< Q < z
assert: r <<< R < x <<< X <<< s << q <<< Q < z < n


&[before 1] \u24DC < x <<< X << q <<< Q < z
assert: x <<< X << q <<< Q < z < n < m <<< \u24DC <<< M

&[before 2] \u24DC << x <<< X << q <<< Q < z
assert:  x <<< X << q <<< Q << m <<< \u24DC <<< M < z < n

&[before 3] \u24DC <<< x <<< X << q <<< Q < z
assert: m <<< x <<< X <<< \u24DC <<< M  << q <<< Q < z < n
*/


#if 0
/* requires features not yet supported */
static void TestMoreBefore(void) {
    static const struct {
        const char* rules;
        const char* order[16];
        int32_t size;
    } tests[] = {
        { "&m < a &[before 1] a < x <<< X << q <<< Q < z",
        { "m","M","x","X","q","Q","z","a","n" }, 9},
        { "&m < a &[before 2] a << x <<< X << q <<< Q < z",
        { "m","M","x","X","q","Q","a","z","n" }, 9},
        { "&m < a &[before 3] a <<< x <<< X << q <<< Q < z",
        { "m","M","x","X","a","q","Q","z","n" }, 9},
        { "&m << a &[before 1] a < x <<< X << q <<< Q < z",
        { "x","X","q","Q","z","m","M","a","n" }, 9},
        { "&m << a &[before 2] a << x <<< X << q <<< Q < z",
        { "m","M","x","X","q","Q","a","z","n" }, 9},
        { "&m << a &[before 3] a <<< x <<< X << q <<< Q < z",
        { "m","M","x","X","a","q","Q","z","n" }, 9},
        { "&m <<< a &[before 1] a < x <<< X << q <<< Q < z",
        { "x","X","q","Q","z","n","m","a","M" }, 9},
        { "&m <<< a &[before 2] a << x <<< X << q <<< Q < z",
        { "x","X","q","Q","m","a","M","z","n" }, 9},
        { "&m <<< a &[before 3] a <<< x <<< X << q <<< Q < z",
        { "m","x","X","a","M","q","Q","z","n" }, 9},
        { "&[before 1] s < x <<< X << q <<< Q < z",
        { "r","R","x","X","q","Q","z","s","n" }, 9},
        { "&[before 2] s << x <<< X << q <<< Q < z",
        { "r","R","x","X","q","Q","s","z","n" }, 9},
        { "&[before 3] s <<< x <<< X << q <<< Q < z",
        { "r","R","x","X","s","q","Q","z","n" }, 9},
        { "&[before 1] \\u24DC < x <<< X << q <<< Q < z",
        { "x","X","q","Q","z","n","m","\\u24DC","M" }, 9},
        { "&[before 2] \\u24DC << x <<< X << q <<< Q < z",
        { "x","X","q","Q","m","\\u24DC","M","z","n" }, 9},
        { "&[before 3] \\u24DC <<< x <<< X << q <<< Q < z",
        { "m","x","X","\\u24DC","M","q","Q","z","n" }, 9}
    };

    int32_t i = 0;

    for(i = 0; i < UPRV_LENGTHOF(tests); i++) {
        genericRulesStarter(tests[i].rules, tests[i].order, tests[i].size);
    }
}
#endif

static void TestTailorNULL( void ) {
    const static char* rule = "&a <<< '\\u0000'";
    UErrorCode status = U_ZERO_ERROR;
    UChar rlz[RULE_BUFFER_LEN] = { 0 };
    uint32_t rlen = 0;
    UChar a = 1, null = 0;
    UCollationResult res = UCOL_EQUAL;

    UCollator *coll = NULL;


    rlen = u_unescape(rule, rlz, RULE_BUFFER_LEN);
    coll = ucol_openRules(rlz, rlen, UCOL_DEFAULT, UCOL_DEFAULT,NULL, &status);

    if(U_FAILURE(status)) {
        log_err_status(status, "Could not open default collator! -> %s\n", u_errorName(status));
    } else {
        res = ucol_strcoll(coll, &a, 1, &null, 1);

        if(res != UCOL_LESS) {
            log_err("NULL was not tailored properly!\n");
        }
    }

    ucol_close(coll);
}

static void
TestUpperFirstQuaternary(void)
{
  const char* tests[] = { "B", "b", "Bb", "bB" };
  UColAttribute att[] = { UCOL_STRENGTH, UCOL_CASE_FIRST };
  UColAttributeValue attVals[] = { UCOL_QUATERNARY, UCOL_UPPER_FIRST };
  genericLocaleStarterWithOptions("root", tests, UPRV_LENGTHOF(tests), att, attVals, UPRV_LENGTHOF(att));
}

static void
TestJ4960(void)
{
  const char* tests[] = { "\\u00e2T", "aT" };
  UColAttribute att[] = { UCOL_STRENGTH, UCOL_CASE_LEVEL };
  UColAttributeValue attVals[] = { UCOL_PRIMARY, UCOL_ON };
  const char* tests2[] = { "a", "A" };
  const char* rule = "&[first tertiary ignorable]=A=a";
  UColAttribute att2[] = { UCOL_CASE_LEVEL };
  UColAttributeValue attVals2[] = { UCOL_ON };
  /* Test whether we correctly ignore primary ignorables on case level when */
  /* we have only primary & case level */
  genericLocaleStarterWithOptionsAndResult("root", tests, UPRV_LENGTHOF(tests), att, attVals, UPRV_LENGTHOF(att), UCOL_EQUAL);
  /* Test whether ICU4J will make case level for sortkeys that have primary strength */
  /* and case level */
  genericLocaleStarterWithOptions("root", tests2, UPRV_LENGTHOF(tests2), att, attVals, UPRV_LENGTHOF(att));
  /* Test whether completely ignorable letters have case level info (they shouldn't) */
  genericRulesStarterWithOptionsAndResult(rule, tests2, UPRV_LENGTHOF(tests2), att2, attVals2, UPRV_LENGTHOF(att2), UCOL_EQUAL);
}

static void
TestJ5223(void)
{
  static const char *test = "this is a test string";
  UChar ustr[256];
  int32_t ustr_length = u_unescape(test, ustr, 256);
  unsigned char sortkey[256];
  int32_t sortkey_length;
  UErrorCode status = U_ZERO_ERROR;
  static UCollator *coll = NULL;
  coll = ucol_open("root", &status);
  if(U_FAILURE(status)) {
    log_err_status(status, "Couldn't open UCA -> %s\n", u_errorName(status));
    return;
  }
  ucol_setStrength(coll, UCOL_PRIMARY);
  ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_PRIMARY, &status);
  ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
  if (U_FAILURE(status)) {
    log_err("Failed setting attributes\n");
    return;
  }
  sortkey_length = ucol_getSortKey(coll, ustr, ustr_length, NULL, 0);
  if (sortkey_length > 256) return;

  /* we mark the position where the null byte should be written in advance */
  sortkey[sortkey_length-1] = 0xAA;

  /* we set the buffer size one byte higher than needed */
  sortkey_length = ucol_getSortKey(coll, ustr, ustr_length, sortkey,
    sortkey_length+1);

  /* no error occurs (for me) */
  if (sortkey[sortkey_length-1] == 0xAA) {
    log_err("Hit bug at first try\n");
  }

  /* we mark the position where the null byte should be written again */
  sortkey[sortkey_length-1] = 0xAA;

  /* this time we set the buffer size to the exact amount needed */
  sortkey_length = ucol_getSortKey(coll, ustr, ustr_length, sortkey,
    sortkey_length);

  /* now the trailing null byte is not written */
  if (sortkey[sortkey_length-1] == 0xAA) {
    log_err("Hit bug at second try\n");
  }

  ucol_close(coll);
}

/* Regression test for Thai partial sort key problem */
static void
TestJ5232(void)
{
    const static char *test[] = {
        "\\u0e40\\u0e01\\u0e47\\u0e1a\\u0e40\\u0e25\\u0e47\\u0e21",
        "\\u0e40\\u0e01\\u0e47\\u0e1a\\u0e40\\u0e25\\u0e48\\u0e21"
    };

    genericLocaleStarter("th", test, UPRV_LENGTHOF(test));
}

static void
TestJ5367(void)
{
    const static char *test[] = { "a", "y" };
    const char* rules = "&Ny << Y &[first secondary ignorable] <<< a";
    genericRulesStarter(rules, test, UPRV_LENGTHOF(test));
}

static void
TestVI5913(void)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t i, j;
    UCollator *coll =NULL;
    uint8_t  resColl[100], expColl[100];
    int32_t  rLen, tLen, ruleLen, sLen, kLen;
    UChar rule[256]={0x26, 0x62, 0x3c, 0x1FF3, 0};  /* &b<0x1FF3-omega with Ypogegrammeni*/
    UChar rule2[256]={0x26, 0x7a, 0x3c, 0x0161, 0};  /* &z<s with caron*/
    /*
     * Note: Just tailoring &z<ae^ does not work as expected:
     * The UCA spec requires for discontiguous contractions that they
     * extend an *existing match* by one combining mark at a time.
     * Therefore, ae must be a contraction so that the builder finds
     * discontiguous contractions for ae^, for example with an intervening underdot.
     * Only then do we get the expected tail closure with a\u1EC7, a\u1EB9\u0302, etc.
     */
    UChar rule3[256]={
        0x26, 0x78, 0x3c, 0x61, 0x65,      /* &x<ae */
        0x26, 0x7a, 0x3c, 0x0061, 0x00ea,  /* &z<a+e with circumflex.*/
        0};
    static const UChar tData[][20]={
        {0x1EAC, 0},
        {0x0041, 0x0323, 0x0302, 0},
        {0x1EA0, 0x0302, 0},
        {0x00C2, 0x0323, 0},
        {0x1ED8, 0},  /* O with dot and circumflex */
        {0x1ECC, 0x0302, 0},
        {0x1EB7, 0},
        {0x1EA1, 0x0306, 0},
    };
    static const UChar tailorData[][20]={
        {0x1FA2, 0},  /* Omega with 3 combining marks */
        {0x03C9, 0x0313, 0x0300, 0x0345, 0},
        {0x1FF3, 0x0313, 0x0300, 0},
        {0x1F60, 0x0300, 0x0345, 0},
        {0x1F62, 0x0345, 0},
        {0x1FA0, 0x0300, 0},
    };
    static const UChar tailorData2[][20]={
        {0x1E63, 0x030C, 0},  /* s with dot below + caron */
        {0x0073, 0x0323, 0x030C, 0},
        {0x0073, 0x030C, 0x0323, 0},
    };
    static const UChar tailorData3[][20]={
        {0x007a, 0},  /*  z */
        {0x0061, 0x0065, 0},  /*  a + e */
        {0x0061, 0x00ea, 0}, /* a + e with circumflex */
        {0x0061, 0x1EC7, 0},  /* a+ e with dot below and circumflex */
        {0x0061, 0x1EB9, 0x0302, 0}, /* a + e with dot below + combining circumflex */
        {0x0061, 0x00EA, 0x0323, 0},  /* a + e with circumflex + combining dot below */
        {0x00EA, 0x0323, 0},  /* e with circumflex + combining dot below */
        {0x00EA, 0},  /* e with circumflex  */
    };

    /* Test Vietnamese sort. */
    coll = ucol_open("vi", &status);
    if(U_FAILURE(status)) {
        log_err_status(status, "Couldn't open collator -> %s\n", u_errorName(status));
        return;
    }
    log_verbose("\n\nVI collation:");
    if ( !ucol_equal(coll, tData[0], u_strlen(tData[0]), tData[2], u_strlen(tData[2])) ) {
        log_err("\\u1EAC not equals to \\u1EA0+\\u0302\n");
    }
    if ( !ucol_equal(coll, tData[0], u_strlen(tData[0]), tData[3], u_strlen(tData[3])) ) {
        log_err("\\u1EAC not equals to \\u00c2+\\u0323\n");
    }
    if ( !ucol_equal(coll, tData[5], u_strlen(tData[5]), tData[4], u_strlen(tData[4])) ) {
        log_err("\\u1ED8 not equals to \\u1ECC+\\u0302\n");
    }
    if ( !ucol_equal(coll, tData[7], u_strlen(tData[7]), tData[6], u_strlen(tData[6])) ) {
        log_err("\\u1EB7 not equals to \\u1EA1+\\u0306\n");
    }

    for (j=0; j<8; j++) {
        tLen = u_strlen(tData[j]);
        log_verbose("\n Data :%s  \tlen: %d key: ", tData[j], tLen);
        rLen = ucol_getSortKey(coll, tData[j], tLen, resColl, 100);
        for(i = 0; i<rLen; i++) {
            log_verbose(" %02X", resColl[i]);
        }
    }

    ucol_close(coll);

    /* Test Romanian sort. */
    coll = ucol_open("ro", &status);
    log_verbose("\n\nRO collation:");
    if ( !ucol_equal(coll, tData[0], u_strlen(tData[0]), tData[1], u_strlen(tData[1])) ) {
        log_err("\\u1EAC not equals to \\u1EA0+\\u0302\n");
    }
    if ( !ucol_equal(coll, tData[4], u_strlen(tData[4]), tData[5], u_strlen(tData[5])) ) {
        log_err("\\u1EAC not equals to \\u00c2+\\u0323\n");
    }
    if ( !ucol_equal(coll, tData[6], u_strlen(tData[6]), tData[7], u_strlen(tData[7])) ) {
        log_err("\\u1EB7 not equals to \\u1EA1+\\u0306\n");
    }

    for (j=4; j<8; j++) {
        tLen = u_strlen(tData[j]);
        log_verbose("\n Data :%s  \tlen: %d key: ", tData[j], tLen);
        rLen = ucol_getSortKey(coll, tData[j], tLen, resColl, 100);
        for(i = 0; i<rLen; i++) {
            log_verbose(" %02X", resColl[i]);
        }
    }
    ucol_close(coll);

    /* Test the precomposed Greek character with 3 combining marks. */
    log_verbose("\n\nTailoring test: Greek character with 3 combining marks");
    ruleLen = u_strlen(rule);
    coll = ucol_openRules(rule, ruleLen, UCOL_OFF, UCOL_TERTIARY, NULL,&status);
    if (U_FAILURE(status)) {
        log_err("ucol_openRules failed with %s\n", u_errorName(status));
        return;
    }
    sLen = u_strlen(tailorData[0]);
    for (j=1; j<6; j++) {
        tLen = u_strlen(tailorData[j]);
        if ( !ucol_equal(coll, tailorData[0], sLen, tailorData[j], tLen))  {
            log_err("\n \\u1FA2 not equals to data[%d]:%s\n", j, tailorData[j]);
        }
    }
    /* Test getSortKey. */
    tLen = u_strlen(tailorData[0]);
    kLen=ucol_getSortKey(coll, tailorData[0], tLen, expColl, 100);
    for (j=0; j<6; j++) {
        tLen = u_strlen(tailorData[j]);
        rLen = ucol_getSortKey(coll, tailorData[j], tLen, resColl, 100);
        if ( kLen!=rLen || uprv_memcmp(expColl, resColl, rLen*sizeof(uint8_t))!=0 ) {
            log_err("\n Data[%d] :%s  \tlen: %d key: ", j, tailorData[j], tLen);
            for(i = 0; i<rLen; i++) {
                log_err(" %02X", resColl[i]);
            }
        }
    }
    ucol_close(coll);

    log_verbose("\n\nTailoring test for s with caron:");
    ruleLen = u_strlen(rule2);
    coll = ucol_openRules(rule2, ruleLen, UCOL_OFF, UCOL_TERTIARY, NULL,&status);
    tLen = u_strlen(tailorData2[0]);
    kLen=ucol_getSortKey(coll, tailorData2[0], tLen, expColl, 100);
    for (j=1; j<3; j++) {
        tLen = u_strlen(tailorData2[j]);
        rLen = ucol_getSortKey(coll, tailorData2[j], tLen, resColl, 100);
        if ( kLen!=rLen || uprv_memcmp(expColl, resColl, rLen*sizeof(uint8_t))!=0 ) {
            log_err("\n After tailoring Data[%d] :%s  \tlen: %d key: ", j, tailorData[j], tLen);
            for(i = 0; i<rLen; i++) {
                log_err(" %02X", resColl[i]);
            }
        }
    }
    ucol_close(coll);

    log_verbose("\n\nTailoring test for &z< ae with circumflex:");
    ruleLen = u_strlen(rule3);
    coll = ucol_openRules(rule3, ruleLen, UCOL_OFF, UCOL_TERTIARY, NULL,&status);
    tLen = u_strlen(tailorData3[3]);
    kLen=ucol_getSortKey(coll, tailorData3[3], tLen, expColl, 100);
    log_verbose("\n Test Data[3] :%s  \tlen: %d key: ", aescstrdup(tailorData3[3], tLen), tLen);
    for(i = 0; i<kLen; i++) {
        log_verbose(" %02X", expColl[i]);
    }
    for (j=4; j<6; j++) {
        tLen = u_strlen(tailorData3[j]);
        rLen = ucol_getSortKey(coll, tailorData3[j], tLen, resColl, 100);

        if ( kLen!=rLen || uprv_memcmp(expColl, resColl, rLen*sizeof(uint8_t))!=0 ) {
            log_err("\n After tailoring Data[%d] :%s  \tlen: %d key: ", j, aescstrdup(tailorData3[j], tLen), tLen);
            for(i = 0; i<rLen; i++) {
                log_err(" %02X", resColl[i]);
            }
        }

        log_verbose("\n Test Data[%d] :%s  \tlen: %d key: ", j, aescstrdup(tailorData3[j], tLen), tLen);
         for(i = 0; i<rLen; i++) {
             log_verbose(" %02X", resColl[i]);
         }
    }
    ucol_close(coll);
}

static void
TestTailor6179(void)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t i;
    UCollator *coll =NULL;
    uint8_t  resColl[100];
    int32_t  rLen, tLen, ruleLen;
    /* &[last primary ignorable]<< a  &[first primary ignorable]<<b */
    static const UChar rule1[]={
            0x26,0x5B,0x6C,0x61,0x73,0x74,0x20,0x70,0x72,0x69,0x6D,0x61,0x72,0x79,
            0x20,0x69,0x67,0x6E,0x6F,0x72,0x61,0x62,0x6C,0x65,0x5D,0x3C,0x3C,0x20,0x61,0x20,
            0x26,0x5B,0x66,0x69,0x72,0x73,0x74,0x20,0x70,0x72,0x69,0x6D,0x61,0x72,0x79,0x20,
            0x69,0x67,0x6E,0x6F,0x72,0x61,0x62,0x6C,0x65,0x5D,0x3C,0x3C,0x62,0x20, 0};
    /* &[last secondary ignorable]<<< a &[first secondary ignorable]<<<b */
    static const UChar rule2[]={
            0x26,0x5B,0x6C,0x61,0x73,0x74,0x20,0x73,0x65,0x63,0x6F,0x6E,0x64,0x61,
            0x72,0x79,0x20,0x69,0x67,0x6E,0x6F,0x72,0x61,0x62,0x6C,0x65,0x5D,0x3C,0x3C,0x3C,
            0x61,0x20,0x26,0x5B,0x66,0x69,0x72,0x73,0x74,0x20,0x73,0x65,0x63,0x6F,0x6E,
            0x64,0x61,0x72,0x79,0x20,0x69,0x67,0x6E,0x6F,0x72,0x61,0x62,0x6C,0x65,0x5D,0x3C,
            0x3C,0x3C,0x20,0x62,0};

    static const UChar tData1[][4]={
        {0x61, 0},
        {0x62, 0},
        { 0xFDD0,0x009E, 0}
    };
    static const UChar tData2[][4]={
        {0x61, 0},
        {0x62, 0},
        { 0xFDD0,0x009E, 0}
     };

    /*
     * These values from FractionalUCA.txt will change,
     * and need to be updated here.
     * TODO: Make this not check for particular sort keys.
     * Instead, test that we get CEs before & after other ignorables; see ticket #6179.
     */
    static const uint8_t firstPrimaryIgnCE[]={1, 0x83, 1, 5, 0};
    static const uint8_t lastPrimaryIgnCE[]={1, 0xFC, 1, 5, 0};
    static const uint8_t firstSecondaryIgnCE[]={1, 1, 0xfe, 0};
    static const uint8_t lastSecondaryIgnCE[]={1, 1, 0xff, 0};

    UParseError parseError;

    /* Test [Last Primary ignorable] */

    log_verbose("Tailoring test: &[last primary ignorable]<<a  &[first primary ignorable]<<b\n");
    ruleLen = u_strlen(rule1);
    coll = ucol_openRules(rule1, ruleLen, UCOL_OFF, UCOL_TERTIARY, NULL,&status);
    if (U_FAILURE(status)) {
        log_err_status(status, "Tailoring test: &[last primary ignorable] failed! -> %s\n", u_errorName(status));
        return;
    }
    tLen = u_strlen(tData1[0]);
    rLen = ucol_getSortKey(coll, tData1[0], tLen, resColl, 100);
    if (rLen != UPRV_LENGTHOF(lastPrimaryIgnCE) || uprv_memcmp(resColl, lastPrimaryIgnCE, rLen) != 0) {
        log_err("Bad result for &[lpi]<<a...: Data[%d] :%s  \tlen: %d key: ", 0, tData1[0], rLen);
        for(i = 0; i<rLen; i++) {
            log_err(" %02X", resColl[i]);
        }
        log_err("\n");
    }
    tLen = u_strlen(tData1[1]);
    rLen = ucol_getSortKey(coll, tData1[1], tLen, resColl, 100);
    if (rLen != UPRV_LENGTHOF(firstPrimaryIgnCE) || uprv_memcmp(resColl, firstPrimaryIgnCE, rLen) != 0) {
        log_err("Bad result for &[lpi]<<a...: Data[%d] :%s  \tlen: %d key: ", 1, tData1[1], rLen);
        for(i = 0; i<rLen; i++) {
            log_err(" %02X", resColl[i]);
        }
        log_err("\n");
    }
    ucol_close(coll);


    /* Test [Last Secondary ignorable] */
    log_verbose("Tailoring test: &[last secondary ignorable]<<<a  &[first secondary ignorable]<<<b\n");
    ruleLen = u_strlen(rule2);
    coll = ucol_openRules(rule2, ruleLen, UCOL_OFF, UCOL_TERTIARY, &parseError, &status);
    if (U_FAILURE(status)) {
        log_err("Tailoring test: &[last secondary ignorable] failed! -> %s\n", u_errorName(status));
        log_info("  offset=%d  \"%s\" | \"%s\"\n",
                 parseError.offset, aescstrdup(parseError.preContext, -1), aescstrdup(parseError.postContext, -1));
        return;
    }
    tLen = u_strlen(tData2[0]);
    rLen = ucol_getSortKey(coll, tData2[0], tLen, resColl, 100);
    if (rLen != UPRV_LENGTHOF(lastSecondaryIgnCE) || uprv_memcmp(resColl, lastSecondaryIgnCE, rLen) != 0) {
        log_err("Bad result for &[lsi]<<<a...: Data[%d] :%s  \tlen: %d key: ", 0, tData2[0], rLen);
        for(i = 0; i<rLen; i++) {
            log_err(" %02X", resColl[i]);
        }
        log_err("\n");
    }
    tLen = u_strlen(tData2[1]);
    rLen = ucol_getSortKey(coll, tData2[1], tLen, resColl, 100);
    if (rLen != UPRV_LENGTHOF(firstSecondaryIgnCE) || uprv_memcmp(resColl, firstSecondaryIgnCE, rLen) != 0) {
      log_err("Bad result for &[lsi]<<<a...: Data[%d] :%s  \tlen: %d key: ", 1, tData2[1], rLen);
      for(i = 0; i<rLen; i++) {
        log_err(" %02X", resColl[i]);
      }
      log_err("\n");
    }
    ucol_close(coll);
}

static void
TestUCAPrecontext(void)
{
    UErrorCode status = U_ZERO_ERROR;
    int32_t i, j;
    UCollator *coll =NULL;
    uint8_t  resColl[100], prevColl[100];
    int32_t  rLen, tLen, ruleLen;
    UChar rule1[256]= {0x26, 0xb7, 0x3c, 0x61, 0}; /* & middle-dot < a */
    UChar rule2[256]= {0x26, 0x4C, 0xb7, 0x3c, 0x3c, 0x61, 0};
    /* & l middle-dot << a  a is an expansion. */

    UChar tData1[][20]={
            { 0xb7, 0},  /* standalone middle dot(0xb7) */
            { 0x387, 0}, /* standalone middle dot(0x387) */
            { 0x61, 0},  /* a */
            { 0x6C, 0},  /* l */
            { 0x4C, 0x0332, 0},  /* l with [first primary ignorable] */
            { 0x6C, 0xb7, 0},  /* l with middle dot(0xb7) */
            { 0x6C, 0x387, 0}, /* l with middle dot(0x387) */
            { 0x4C, 0xb7, 0},  /* L with middle dot(0xb7) */
            { 0x4C, 0x387, 0}, /* L with middle dot(0x387) */
            { 0x6C, 0x61, 0x387, 0}, /* la  with middle dot(0x387) */
            { 0x4C, 0x61, 0xb7, 0},  /* La with middle dot(0xb7) */
     };

    log_verbose("\n\nEN collation:");
    coll = ucol_open("en", &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "Tailoring test: &z <<a|- failed! -> %s\n", u_errorName(status));
        return;
    }
    for (j=0; j<11; j++) {
        tLen = u_strlen(tData1[j]);
        rLen = ucol_getSortKey(coll, tData1[j], tLen, resColl, 100);
        if ((j>0) && (strcmp((char *)resColl, (char *)prevColl)<0)) {
            log_err("\n Expecting greater key than previous test case: Data[%d] :%s.",
                    j, tData1[j]);
        }
        log_verbose("\n Data[%d] :%s  \tlen: %d key: ", j, tData1[j], rLen);
        for(i = 0; i<rLen; i++) {
            log_verbose(" %02X", resColl[i]);
        }
        uprv_memcpy(prevColl, resColl, sizeof(uint8_t)*(rLen+1));
     }
     ucol_close(coll);


     log_verbose("\n\nJA collation:");
     coll = ucol_open("ja", &status);
     if (U_FAILURE(status)) {
         log_err("Tailoring test: &z <<a|- failed!");
         return;
     }
     for (j=0; j<11; j++) {
         tLen = u_strlen(tData1[j]);
         rLen = ucol_getSortKey(coll, tData1[j], tLen, resColl, 100);
         if ((j>0) && (strcmp((char *)resColl, (char *)prevColl)<0)) {
             log_err("\n Expecting greater key than previous test case: Data[%d] :%s.",
                     j, tData1[j]);
         }
         log_verbose("\n Data[%d] :%s  \tlen: %d key: ", j, tData1[j], rLen);
         for(i = 0; i<rLen; i++) {
             log_verbose(" %02X", resColl[i]);
         }
         uprv_memcpy(prevColl, resColl, sizeof(uint8_t)*(rLen+1));
      }
      ucol_close(coll);


      log_verbose("\n\nTailoring test: & middle dot < a ");
      ruleLen = u_strlen(rule1);
      coll = ucol_openRules(rule1, ruleLen, UCOL_OFF, UCOL_TERTIARY, NULL,&status);
      if (U_FAILURE(status)) {
          log_err("Tailoring test: & middle dot < a failed!");
          return;
      }
      for (j=0; j<11; j++) {
          tLen = u_strlen(tData1[j]);
          rLen = ucol_getSortKey(coll, tData1[j], tLen, resColl, 100);
          if ((j>0) && (strcmp((char *)resColl, (char *)prevColl)<0)) {
              log_err("\n Expecting greater key than previous test case: Data[%d] :%s.",
                      j, tData1[j]);
          }
          log_verbose("\n Data[%d] :%s  \tlen: %d key: ", j, tData1[j], rLen);
          for(i = 0; i<rLen; i++) {
              log_verbose(" %02X", resColl[i]);
          }
          uprv_memcpy(prevColl, resColl, sizeof(uint8_t)*(rLen+1));
       }
       ucol_close(coll);


       log_verbose("\n\nTailoring test: & l middle-dot << a ");
       ruleLen = u_strlen(rule2);
       coll = ucol_openRules(rule2, ruleLen, UCOL_OFF, UCOL_TERTIARY, NULL,&status);
       if (U_FAILURE(status)) {
           log_err("Tailoring test: & l middle-dot << a failed!");
           return;
       }
       for (j=0; j<11; j++) {
           tLen = u_strlen(tData1[j]);
           rLen = ucol_getSortKey(coll, tData1[j], tLen, resColl, 100);
           if ((j>0) && (j!=3) && (strcmp((char *)resColl, (char *)prevColl)<0)) {
               log_err("\n Expecting greater key than previous test case: Data[%d] :%s.",
                       j, tData1[j]);
           }
           if ((j==3)&&(strcmp((char *)resColl, (char *)prevColl)>0)) {
               log_err("\n Expecting smaller key than previous test case: Data[%d] :%s.",
                       j, tData1[j]);
           }
           log_verbose("\n Data[%d] :%s  \tlen: %d key: ", j, tData1[j], rLen);
           for(i = 0; i<rLen; i++) {
               log_verbose(" %02X", resColl[i]);
           }
           uprv_memcpy(prevColl, resColl, sizeof(uint8_t)*(rLen+1));
        }
        ucol_close(coll);
}

static void
TestOutOfBuffer5468(void)
{
    static const char *test = "\\u4e00";
    UChar ustr[256];
    int32_t ustr_length = u_unescape(test, ustr, 256);
    unsigned char shortKeyBuf[1];
    int32_t sortkey_length;
    UErrorCode status = U_ZERO_ERROR;
    static UCollator *coll = NULL;

    coll = ucol_open("root", &status);
    if(U_FAILURE(status)) {
      log_err_status(status, "Couldn't open UCA -> %s\n", u_errorName(status));
      return;
    }
    ucol_setStrength(coll, UCOL_PRIMARY);
    ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_PRIMARY, &status);
    ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    if (U_FAILURE(status)) {
      log_err("Failed setting attributes\n");
      return;
    }

    sortkey_length = ucol_getSortKey(coll, ustr, ustr_length, shortKeyBuf, sizeof(shortKeyBuf));
    if (sortkey_length != 4) {
        log_err("expecting length of sortKey is 4  got:%d ", sortkey_length);
    }
    log_verbose("length of sortKey is %d", sortkey_length);
    ucol_close(coll);
}

#define TSKC_DATA_SIZE 5
#define TSKC_BUF_SIZE  50
static void
TestSortKeyConsistency(void)
{
    UErrorCode icuRC = U_ZERO_ERROR;
    UCollator* ucol;
    UChar data[] = { 0xFFFD, 0x0006, 0x0006, 0x0006, 0xFFFD};

    uint8_t bufFull[TSKC_DATA_SIZE][TSKC_BUF_SIZE];
    uint8_t bufPart[TSKC_DATA_SIZE][TSKC_BUF_SIZE];
    int32_t i, j, i2;

    ucol = ucol_openFromShortString("LEN_S4", false, NULL, &icuRC);
    if (U_FAILURE(icuRC))
    {
        log_err_status(icuRC, "ucol_openFromShortString failed -> %s\n", u_errorName(icuRC));
        return;
    }

    for (i = 0; i < TSKC_DATA_SIZE; i++)
    {
        UCharIterator uiter;
        uint32_t state[2] = { 0, 0 };
        int32_t dataLen = i+1;
        for (j=0; j<TSKC_BUF_SIZE; j++)
            bufFull[i][j] = bufPart[i][j] = 0;

        /* Full sort key */
        ucol_getSortKey(ucol, data, dataLen, bufFull[i], TSKC_BUF_SIZE);

        /* Partial sort key */
        uiter_setString(&uiter, data, dataLen);
        ucol_nextSortKeyPart(ucol, &uiter, state, bufPart[i], TSKC_BUF_SIZE, &icuRC);
        if (U_FAILURE(icuRC))
        {
            log_err("ucol_nextSortKeyPart failed\n");
            ucol_close(ucol);
            return;
        }

        for (i2=0; i2<i; i2++)
        {
            UBool fullMatch = true;
            UBool partMatch = true;
            for (j=0; j<TSKC_BUF_SIZE; j++)
            {
                fullMatch = fullMatch && (bufFull[i][j] != bufFull[i2][j]);
                partMatch = partMatch && (bufPart[i][j] != bufPart[i2][j]);
            }
            if (fullMatch != partMatch) {
                log_err(fullMatch ? "full key was consistent, but partial key changed\n"
                                  : "partial key was consistent, but full key changed\n");
                ucol_close(ucol);
                return;
            }
        }
    }

    /*=============================================*/
   ucol_close(ucol);
}

/* ticket: 6101 */
static void TestCroatianSortKey(void) {
    const char* collString = "LHR_AN_CX_EX_FX_HX_NX_S3";
    UErrorCode status = U_ZERO_ERROR;
    UCollator *ucol;
    UCharIterator iter;

    static const UChar text[] = { 0x0044, 0xD81A };

    size_t length = UPRV_LENGTHOF(text);

    uint8_t textSortKey[32];
    size_t lenSortKey = 32;
    size_t actualSortKeyLen;
    uint32_t uStateInfo[2] = { 0, 0 };

    ucol = ucol_openFromShortString(collString, false, NULL, &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ucol_openFromShortString error in Craotian test. -> %s\n", u_errorName(status));
        return;
    }

    uiter_setString(&iter, text, (int32_t)length);

    actualSortKeyLen = ucol_nextSortKeyPart(
        ucol, &iter, (uint32_t*)uStateInfo,
        textSortKey, (int32_t)lenSortKey, &status
        );

    if (actualSortKeyLen == lenSortKey) {
        log_err("ucol_nextSortKeyPart did not give correct result in Croatian test.\n");
    }

    ucol_close(ucol);
}

/* ticket: 6140 */
/* This test ensures that codepoints such as 0x3099 are flagged correctly by the collator since
 * they are both Hiragana and Katakana
 */
#define SORTKEYLEN 50
static void TestHiragana(void) {
    UErrorCode status = U_ZERO_ERROR;
    UCollator* ucol;
    UCollationResult strcollresult;
    UChar data1[] = { 0x3058, 0x30B8 }; /* Hiragana and Katakana letter Zi */
    UChar data2[] = { 0x3057, 0x3099, 0x30B7, 0x3099 };
    int32_t data1Len = UPRV_LENGTHOF(data1);
    int32_t data2Len = UPRV_LENGTHOF(data2);
    int32_t i, j;
    uint8_t sortKey1[SORTKEYLEN];
    uint8_t sortKey2[SORTKEYLEN];

    UCharIterator uiter1;
    UCharIterator uiter2;
    uint32_t state1[2] = { 0, 0 };
    uint32_t state2[2] = { 0, 0 };
    int32_t keySize1;
    int32_t keySize2;

    ucol = ucol_openFromShortString("LJA_AN_CX_EX_FX_HO_NX_S4", false, NULL,
            &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "Error status: %s; Unable to open collator from short string.\n", u_errorName(status));
        return;
    }

    /* Start of full sort keys */
    /* Full sort key1 */
    keySize1 = ucol_getSortKey(ucol, data1, data1Len, sortKey1, SORTKEYLEN);
    /* Full sort key2 */
    keySize2 = ucol_getSortKey(ucol, data2, data2Len, sortKey2, SORTKEYLEN);
    if (keySize1 == keySize2) {
        for (i = 0; i < keySize1; i++) {
            if (sortKey1[i] != sortKey2[i]) {
                log_err("Full sort keys are different. Should be equal.");
            }
        }
    } else {
        log_err("Full sort keys sizes doesn't match: %d %d", keySize1, keySize2);
    }
    /* End of full sort keys */

    /* Start of partial sort keys */
    /* Partial sort key1 */
    uiter_setString(&uiter1, data1, data1Len);
    keySize1 = ucol_nextSortKeyPart(ucol, &uiter1, state1, sortKey1, SORTKEYLEN, &status);
    /* Partial sort key2 */
    uiter_setString(&uiter2, data2, data2Len);
    keySize2 = ucol_nextSortKeyPart(ucol, &uiter2, state2, sortKey2, SORTKEYLEN, &status);
    if (U_SUCCESS(status) && keySize1 == keySize2) {
        for (j = 0; j < keySize1; j++) {
            if (sortKey1[j] != sortKey2[j]) {
                log_err("Partial sort keys are different. Should be equal");
            }
        }
    } else {
        log_err("Error Status: %s or Partial sort keys sizes doesn't match: %d %d", u_errorName(status), keySize1, keySize2);
    }
    /* End of partial sort keys */

    /* Start of strcoll */
    /* Use ucol_strcoll() to determine ordering */
    strcollresult = ucol_strcoll(ucol, data1, data1Len, data2, data2Len);
    if (strcollresult != UCOL_EQUAL) {
        log_err("Result from ucol_strcoll() should be UCOL_EQUAL.");
    }

    ucol_close(ucol);
}

/* Convenient struct for running collation tests */
typedef struct {
  const UChar source[MAX_TOKEN_LEN];  /* String on left */
  const UChar target[MAX_TOKEN_LEN];  /* String on right */
  UCollationResult result;            /* -1, 0 or +1, depending on collation */
} OneTestCase;

/*
 * Utility function to test one collation test case.
 * @param testcases Array of test cases.
 * @param n_testcases Size of the array testcases.
 * @param str_rules Array of rules.  These rules should be specifying the same rule in different formats.
 * @param n_rules Size of the array str_rules.
 */
static void doTestOneTestCase(const OneTestCase testcases[],
                              int n_testcases,
                              const char* str_rules[],
                              int n_rules)
{
  int rule_no, testcase_no;
  UChar rule[500]; 
  int32_t length = 0;
  UErrorCode status = U_ZERO_ERROR;
  UParseError parse_error;
  UCollator  *myCollation;

  for (rule_no = 0; rule_no < n_rules; ++rule_no) {

    length = u_unescape(str_rules[rule_no], rule, 500);
    if (length == 0) {
        log_err("ERROR: The rule cannot be unescaped: %s\n");
        return;
    }
    myCollation = ucol_openRules(rule, length, UCOL_ON, UCOL_TERTIARY, &parse_error, &status);
    if(U_FAILURE(status)){
        log_err_status(status, "ERROR: in creation of rule based collator: %s\n", myErrorName(status));
        log_info("  offset=%d  \"%s\" | \"%s\"\n",
                 parse_error.offset,
                 aescstrdup(parse_error.preContext, -1),
                 aescstrdup(parse_error.postContext, -1));
        return;
    }
    log_verbose("Testing the <<* syntax\n");
    ucol_setAttribute(myCollation, UCOL_NORMALIZATION_MODE, UCOL_ON, &status);
    ucol_setStrength(myCollation, UCOL_TERTIARY);
    for (testcase_no = 0; testcase_no < n_testcases; ++testcase_no) {
      doTest(myCollation,
             testcases[testcase_no].source,
             testcases[testcase_no].target,
             testcases[testcase_no].result
             );
    }
    ucol_close(myCollation);
  }
}

const static OneTestCase rangeTestcases[] = {
  { {0x0061},                            {0x0062},                          UCOL_LESS }, /* "a" < "b" */
  { {0x0062},                            {0x0063},                          UCOL_LESS }, /* "b" < "c" */
  { {0x0061},                            {0x0063},                          UCOL_LESS }, /* "a" < "c" */

  { {0x0062},                            {0x006b},                          UCOL_LESS }, /* "b" << "k" */
  { {0x006b},                            {0x006c},                          UCOL_LESS }, /* "k" << "l" */
  { {0x0062},                            {0x006c},                          UCOL_LESS }, /* "b" << "l" */
  { {0x0061},                            {0x006c},                          UCOL_LESS }, /* "a" < "l" */
  { {0x0061},                            {0x006d},                          UCOL_LESS },  /* "a" < "m" */

  { {0x0079},                            {0x006d},                          UCOL_LESS },  /* "y" < "f" */
  { {0x0079},                            {0x0067},                          UCOL_LESS },  /* "y" < "g" */
  { {0x0061},                            {0x0068},                          UCOL_LESS },  /* "y" < "h" */
  { {0x0061},                            {0x0065},                          UCOL_LESS },  /* "g" < "e" */

  { {0x0061},                            {0x0031},                          UCOL_EQUAL }, /* "a" = "1" */
  { {0x0061},                            {0x0032},                          UCOL_EQUAL }, /* "a" = "2" */
  { {0x0061},                            {0x0033},                          UCOL_EQUAL }, /* "a" = "3" */
  { {0x0061},                            {0x0066},                          UCOL_LESS }, /* "a" < "f" */
  { {0x006c, 0x0061},                    {0x006b, 0x0062},                  UCOL_LESS },  /* "la" < "123" */
  { {0x0061, 0x0061, 0x0061},            {0x0031, 0x0032, 0x0033},          UCOL_EQUAL }, /* "aaa" = "123" */
  { {0x0062},                            {0x007a},                          UCOL_LESS },  /* "b" < "z" */
  { {0x0061, 0x007a, 0x0062},            {0x0032, 0x0079, 0x006d},          UCOL_LESS }, /* "azm" = "2yc" */
};

static int nRangeTestcases = UPRV_LENGTHOF(rangeTestcases);

const static OneTestCase rangeTestcasesSupplemental[] = {
  { {0x4e00},                            {0xfffb},                          UCOL_LESS }, /* U+4E00 < U+FFFB */
  { {0xfffb},                            {0xd800, 0xdc00},                  UCOL_LESS }, /* U+FFFB < U+10000 */
  { {0xd800, 0xdc00},                    {0xd800, 0xdc01},                  UCOL_LESS }, /* U+10000 < U+10001 */
  { {0x4e00},                            {0xd800, 0xdc01},                  UCOL_LESS }, /* U+4E00 < U+10001 */
  { {0xd800, 0xdc01},                    {0xd800, 0xdc02},                  UCOL_LESS }, /* U+10000 < U+10001 */
  { {0xd800, 0xdc01},                    {0xd800, 0xdc02},                  UCOL_LESS }, /* U+10000 < U+10001 */
  { {0x4e00},                            {0xd800, 0xdc02},                  UCOL_LESS }, /* U+4E00 < U+10001 */
};

static int nRangeTestcasesSupplemental = UPRV_LENGTHOF(rangeTestcasesSupplemental);

const static OneTestCase rangeTestcasesQwerty[] = {
  { {0x0071},                            {0x0077},                          UCOL_LESS }, /* "q" < "w" */
  { {0x0077},                            {0x0065},                          UCOL_LESS }, /* "w" < "e" */

  { {0x0079},                            {0x0075},                          UCOL_LESS }, /* "y" < "u" */
  { {0x0071},                            {0x0075},                          UCOL_LESS }, /* "q" << "u" */

  { {0x0074},                            {0x0069},                          UCOL_LESS }, /* "t" << "i" */
  { {0x006f},                            {0x0070},                          UCOL_LESS }, /* "o" << "p" */

  { {0x0079},                            {0x0065},                          UCOL_LESS },  /* "y" < "e" */
  { {0x0069},                            {0x0075},                          UCOL_LESS },  /* "i" < "u" */

  { {0x0071, 0x0075, 0x0065, 0x0073, 0x0074},
    {0x0077, 0x0065, 0x0072, 0x0065},                                       UCOL_LESS }, /* "quest" < "were" */
  { {0x0071, 0x0075, 0x0061, 0x0063, 0x006b},
    {0x0071, 0x0075, 0x0065, 0x0073, 0x0074},                               UCOL_LESS }, /* "quack" < "quest" */
};

static int nRangeTestcasesQwerty = UPRV_LENGTHOF(rangeTestcasesQwerty);

static void TestSameStrengthList(void)
{
  const char* strRules[] = {
    /* Normal */
    "&a<b<c<d &b<<k<<l<<m &k<<<x<<<y<<<z  &y<f<g<h<e &a=1=2=3", 

    /* Lists */
    "&a<*bcd &b<<*klm &k<<<*xyz &y<*fghe &a=*123", 
  };
  doTestOneTestCase(rangeTestcases, nRangeTestcases, strRules, UPRV_LENGTHOF(strRules));
}

static void TestSameStrengthListQuoted(void)
{
  const char* strRules[] = {
    /* Lists with quoted characters */
    "&\\u0061<*bcd &b<<*klm &k<<<*xyz &y<*f\\u0067\\u0068e &a=*123",
    "&'\\u0061'<*bcd &b<<*klm &k<<<*xyz &y<*f'\\u0067\\u0068'e &a=*123",

    "&\\u0061<*b\\u0063d &b<<*klm &k<<<*xyz &\\u0079<*fgh\\u0065 &a=*\\u0031\\u0032\\u0033",
    "&'\\u0061'<*b'\\u0063'd &b<<*klm &k<<<*xyz &'\\u0079'<*fgh'\\u0065' &a=*'\\u0031\\u0032\\u0033'",

    "&\\u0061<*\\u0062c\\u0064 &b<<*klm &k<<<*xyz  &y<*fghe &a=*\\u0031\\u0032\\u0033", 
    "&'\\u0061'<*'\\u0062'c'\\u0064' &b<<*klm &k<<<*xyz  &y<*fghe &a=*'\\u0031\\u0032\\u0033'", 
  };
  doTestOneTestCase(rangeTestcases, nRangeTestcases, strRules, UPRV_LENGTHOF(strRules));
}

static void TestSameStrengthListSupplemental(void)
{
  const char* strRules[] = {
    "&\\u4e00<\\ufffb<\\U00010000<\\U00010001<\\U00010002",
    "&\\u4e00<\\ufffb<\\ud800\\udc00<\\ud800\\udc01<\\ud800\\udc02",
    "&\\u4e00<*\\ufffb\\U00010000\\U00010001\\U00010002",
    "&\\u4e00<*\\ufffb\\ud800\\udc00\\ud800\\udc01\\ud800\\udc02",
  };
  doTestOneTestCase(rangeTestcasesSupplemental, nRangeTestcasesSupplemental, strRules, UPRV_LENGTHOF(strRules));
}

static void TestSameStrengthListQwerty(void)
{
  const char* strRules[] = {
    "&q<w<e<r &w<<t<<y<<u &t<<<i<<<o<<<p &o=a=s=d",   /* Normal */
    "&q<*wer &w<<*tyu &t<<<*iop &o=*asd",             /* Lists  */
    "&\\u0071<\\u0077<\\u0065<\\u0072 &\\u0077<<\\u0074<<\\u0079<<\\u0075 &\\u0074<<<\\u0069<<<\\u006f<<<\\u0070 &\\u006f=\\u0061=\\u0073=\\u0064",
    "&'\\u0071'<\\u0077<\\u0065<\\u0072 &\\u0077<<'\\u0074'<<\\u0079<<\\u0075 &\\u0074<<<\\u0069<<<'\\u006f'<<<\\u0070 &\\u006f=\\u0061='\\u0073'=\\u0064",
    "&\\u0071<*\\u0077\\u0065\\u0072 &\\u0077<<*\\u0074\\u0079\\u0075 &\\u0074<<<*\\u0069\\u006f\\u0070 &\\u006f=*\\u0061\\u0073\\u0064",

    /* Quoted characters also will work if two quoted characters are not consecutive.  */
    "&\\u0071<*'\\u0077'\\u0065\\u0072 &\\u0077<<*\\u0074'\\u0079'\\u0075 &\\u0074<<<*\\u0069\\u006f'\\u0070' &'\\u006f'=*\\u0061\\u0073\\u0064",

    /* Consecutive quoted characters do not work, because a '' will be treated as a quote character. */
    /* "&\\u0071<*'\\u0077''\\u0065''\\u0072' &\\u0077<<*'\\u0074''\\u0079''\\u0075' &\\u0074<<<*'\\u0069''\\u006f''\\u0070' &'\\u006f'=*\\u0061\\u0073\\u0064",*/

 };
  doTestOneTestCase(rangeTestcasesQwerty, nRangeTestcasesQwerty, strRules, UPRV_LENGTHOF(strRules));
}

static void TestSameStrengthListQuotedQwerty(void)
{
  const char* strRules[] = {
    "&q<w<e<r &w<<t<<y<<u &t<<<i<<<o<<<p &o=a=s=d",   /* Normal */
    "&q<*wer &w<<*tyu &t<<<*iop &o=*asd",             /* Lists  */
    "&q<*w'e'r &w<<*'t'yu &t<<<*io'p' &o=*'a's'd'",   /* Lists with quotes */

    /* Lists with continuous quotes may not work, because '' will be treated as a quote character. */
    /* "&q<*'w''e''r' &w<<*'t''y''u' &t<<<*'i''o''p' &o=*'a''s''d'", */
   };
  doTestOneTestCase(rangeTestcasesQwerty, nRangeTestcasesQwerty, strRules, UPRV_LENGTHOF(strRules));
}

static void TestSameStrengthListRanges(void)
{
  const char* strRules[] = {
    "&a<*b-d &b<<*k-m &k<<<*x-z &y<*f-he &a=*1-3",
  };
  doTestOneTestCase(rangeTestcases, nRangeTestcases, strRules, UPRV_LENGTHOF(strRules));
}

static void TestSameStrengthListSupplementalRanges(void)
{
  const char* strRules[] = {
    /* Note: U+FFFD..U+FFFF are not tailorable, so a range cannot include them. */
    "&\\u4e00<*\\ufffb\\U00010000-\\U00010002",
  };
  doTestOneTestCase(rangeTestcasesSupplemental, nRangeTestcasesSupplemental, strRules, UPRV_LENGTHOF(strRules));
}

static void TestSpecialCharacters(void)
{
  const char* strRules[] = {
    /* Normal */
    "&';'<'+'<','<'-'<'&'<'*'",

    /* List */
    "&';'<*'+,-&*'",

    /* Range */
    "&';'<*'+'-'-&*'", 
  };

  const static OneTestCase specialCharacterStrings[] = {
    { {0x003b}, {0x002b}, UCOL_LESS },  /* ; < + */
    { {0x002b}, {0x002c}, UCOL_LESS },  /* + < , */
    { {0x002c}, {0x002d}, UCOL_LESS },  /* , < - */
    { {0x002d}, {0x0026}, UCOL_LESS },  /* - < & */
  };
  doTestOneTestCase(specialCharacterStrings, UPRV_LENGTHOF(specialCharacterStrings), strRules, UPRV_LENGTHOF(strRules));
}

static void TestPrivateUseCharacters(void)
{
  const char* strRules[] = {
    /* Normal */
    "&'\\u5ea7'<'\\uE2D8'<'\\uE2D9'<'\\uE2DA'<'\\uE2DB'<'\\uE2DC'<'\\u4e8d'",
    "&\\u5ea7<\\uE2D8<\\uE2D9<\\uE2DA<\\uE2DB<\\uE2DC<\\u4e8d", 
  };

  const static OneTestCase privateUseCharacterStrings[] = {
    { {0x5ea7}, {0xe2d8}, UCOL_LESS },
    { {0xe2d8}, {0xe2d9}, UCOL_LESS },
    { {0xe2d9}, {0xe2da}, UCOL_LESS },
    { {0xe2da}, {0xe2db}, UCOL_LESS },
    { {0xe2db}, {0xe2dc}, UCOL_LESS },
    { {0xe2dc}, {0x4e8d}, UCOL_LESS },
  };
  doTestOneTestCase(privateUseCharacterStrings, UPRV_LENGTHOF(privateUseCharacterStrings), strRules, UPRV_LENGTHOF(strRules));
}

static void TestPrivateUseCharactersInList(void)
{
  const char* strRules[] = {
    /* List */
    "&'\\u5ea7'<*'\\uE2D8\\uE2D9\\uE2DA\\uE2DB\\uE2DC\\u4e8d'",
    /* "&'\\u5ea7'<*\\uE2D8'\\uE2D9\\uE2DA'\\uE2DB'\\uE2DC\\u4e8d'", */
    "&\\u5ea7<*\\uE2D8\\uE2D9\\uE2DA\\uE2DB\\uE2DC\\u4e8d",
  };

  const static OneTestCase privateUseCharacterStrings[] = {
    { {0x5ea7}, {0xe2d8}, UCOL_LESS },
    { {0xe2d8}, {0xe2d9}, UCOL_LESS },
    { {0xe2d9}, {0xe2da}, UCOL_LESS },
    { {0xe2da}, {0xe2db}, UCOL_LESS },
    { {0xe2db}, {0xe2dc}, UCOL_LESS },
    { {0xe2dc}, {0x4e8d}, UCOL_LESS },
  };
  doTestOneTestCase(privateUseCharacterStrings, UPRV_LENGTHOF(privateUseCharacterStrings), strRules, UPRV_LENGTHOF(strRules));
}

static void TestPrivateUseCharactersInRange(void)
{
  const char* strRules[] = {
    /* Range */
    "&'\\u5ea7'<*'\\uE2D8'-'\\uE2DC\\u4e8d'",
    "&\\u5ea7<*\\uE2D8-\\uE2DC\\u4e8d",
    /* "&\\u5ea7<\\uE2D8'\\uE2D8'-'\\uE2D9'\\uE2DA-\\uE2DB\\uE2DC\\u4e8d", */
  };

  const static OneTestCase privateUseCharacterStrings[] = {
    { {0x5ea7}, {0xe2d8}, UCOL_LESS },
    { {0xe2d8}, {0xe2d9}, UCOL_LESS },
    { {0xe2d9}, {0xe2da}, UCOL_LESS },
    { {0xe2da}, {0xe2db}, UCOL_LESS },
    { {0xe2db}, {0xe2dc}, UCOL_LESS },
    { {0xe2dc}, {0x4e8d}, UCOL_LESS },
  };
  doTestOneTestCase(privateUseCharacterStrings, UPRV_LENGTHOF(privateUseCharacterStrings), strRules, UPRV_LENGTHOF(strRules));
}

static void TestInvalidListsAndRanges(void)
{
  const char* invalidRules[] = {
    /* Range not in starred expression */
    "&\\ufffe<\\uffff-\\U00010002",

    /* Range without start */
    "&a<*-c",

    /* Range without end */
    "&a<*b-",

    /* More than one hyphen */
    "&a<*b-g-l",

    /* Range in the wrong order */
    "&a<*k-b",

  };

  UChar rule[500];
  UErrorCode status = U_ZERO_ERROR;
  UParseError parse_error;
  int n_rules = UPRV_LENGTHOF(invalidRules);
  int rule_no;
  int length;
  UCollator  *myCollation;

  for (rule_no = 0; rule_no < n_rules; ++rule_no) {

    length = u_unescape(invalidRules[rule_no], rule, 500);
    if (length == 0) {
        log_err("ERROR: The rule cannot be unescaped: %s\n");
        return;
    }
    myCollation = ucol_openRules(rule, length, UCOL_ON, UCOL_TERTIARY, &parse_error, &status);
    (void)myCollation;      /* Suppress set but not used warning. */
    if(!U_FAILURE(status)){
      log_err("ERROR: Could not cause a failure as expected: \n");
    }
    status = U_ZERO_ERROR;
  }
}

/*
 * This test ensures that characters placed before a character in a different script have the same lead byte
 * in their collation key before and after script reordering.
 */
static void TestBeforeRuleWithScriptReordering(void)
{
    UParseError error;
    UErrorCode status = U_ZERO_ERROR;
    UCollator  *myCollation;
    char srules[500] = "&[before 1]\\u03b1 < \\u0e01";
    UChar rules[500];
    uint32_t rulesLength = 0;
    int32_t reorderCodes[1] = {USCRIPT_GREEK};
    UCollationResult collResult;

    uint8_t baseKey[256];
    uint32_t baseKeyLength;
    uint8_t beforeKey[256];
    uint32_t beforeKeyLength;

    UChar base[] = { 0x03b1 }; /* base */
    int32_t baseLen = UPRV_LENGTHOF(base);

    UChar before[] = { 0x0e01 }; /* ko kai */
    int32_t beforeLen = UPRV_LENGTHOF(before);

    /*UChar *data[] = { before, base };
    genericRulesStarter(srules, data, 2);*/
    
    log_verbose("Testing the &[before 1] rule with [reorder grek]\n");

    (void)beforeKeyLength;   /* Suppress set but not used warnings. */
    (void)baseKeyLength;

    /* build collator */
    log_verbose("Testing the &[before 1] rule with [scriptReorder grek]\n");

    rulesLength = u_unescape(srules, rules, UPRV_LENGTHOF(rules));
    myCollation = ucol_openRules(rules, rulesLength, UCOL_ON, UCOL_TERTIARY, &error, &status);
    if(U_FAILURE(status)) {
        log_err_status(status, "ERROR: in creation of rule based collator: %s\n", myErrorName(status));
        return;
    }

    /* check collation results - before rule applied but not script reordering */
    collResult = ucol_strcoll(myCollation, base, baseLen, before, beforeLen);
    if (collResult != UCOL_GREATER) {
        log_err("Collation result not correct before script reordering = %d\n", collResult);
    }

    /* check the lead byte of the collation keys before script reordering */
    baseKeyLength = ucol_getSortKey(myCollation, base, baseLen, baseKey, 256);
    beforeKeyLength = ucol_getSortKey(myCollation, before, beforeLen, beforeKey, 256);
    if (baseKey[0] != beforeKey[0]) {
      log_err("Different lead byte for sort keys using before rule and before script reordering. base character lead byte = %02x, before character lead byte = %02x\n", baseKey[0], beforeKey[0]);
   }

    /* reorder the scripts */
    ucol_setReorderCodes(myCollation, reorderCodes, 1, &status);
    if(U_FAILURE(status)) {
        log_err_status(status, "ERROR: while setting script order: %s\n", myErrorName(status));
        return;
    }

    /* check collation results - before rule applied and after script reordering */
    collResult = ucol_strcoll(myCollation, base, baseLen, before, beforeLen);
    if (collResult != UCOL_GREATER) {
        log_err("Collation result not correct after script reordering = %d\n", collResult);
    }
    
    /* check the lead byte of the collation keys after script reordering */
    ucol_getSortKey(myCollation, base, baseLen, baseKey, 256);
    ucol_getSortKey(myCollation, before, beforeLen, beforeKey, 256);
    if (baseKey[0] != beforeKey[0]) {
        log_err("Different lead byte for sort keys using before rule and after script reordering. base character lead byte = %02x, before character lead byte = %02x\n", baseKey[0], beforeKey[0]);
    }

    ucol_close(myCollation);
}

/*
 * Test that in a primary-compressed sort key all bytes except the first one are unchanged under script reordering.
 */
static void TestNonLeadBytesDuringCollationReordering(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UCollator  *myCollation;
    int32_t reorderCodes[1] = {USCRIPT_GREEK};

    uint8_t baseKey[256];
    uint32_t baseKeyLength;
    uint8_t reorderKey[256];
    uint32_t reorderKeyLength;

    UChar testString[] = { 0x03b1, 0x03b2, 0x03b3 };
    
    uint32_t i;


    log_verbose("Testing non-lead bytes in a sort key with and without reordering\n");

    /* build collator tertiary */
    myCollation = ucol_open("", &status);
    ucol_setStrength(myCollation, UCOL_TERTIARY);
    if(U_FAILURE(status)) {
        log_err_status(status, "ERROR: in creation of collator: %s\n", myErrorName(status));
        return;
    }
    baseKeyLength = ucol_getSortKey(myCollation, testString, UPRV_LENGTHOF(testString), baseKey, 256);

    ucol_setReorderCodes(myCollation, reorderCodes, UPRV_LENGTHOF(reorderCodes), &status);
    if(U_FAILURE(status)) {
        log_err_status(status, "ERROR: setting reorder codes: %s\n", myErrorName(status));
        return;
    }
    reorderKeyLength = ucol_getSortKey(myCollation, testString, UPRV_LENGTHOF(testString), reorderKey, 256);
    
    if (baseKeyLength != reorderKeyLength) {
        log_err("Key lengths not the same during reordering.\n");
        return;
    }
    
    for (i = 1; i < baseKeyLength; i++) {
        if (baseKey[i] != reorderKey[i]) {
            log_err("Collation key bytes not the same at position %d.\n", i);
            return;
        }
    } 
    ucol_close(myCollation);

    /* build collator quaternary */
    myCollation = ucol_open("", &status);
    ucol_setStrength(myCollation, UCOL_QUATERNARY);
    if(U_FAILURE(status)) {
        log_err_status(status, "ERROR: in creation of collator: %s\n", myErrorName(status));
        return;
    }
    baseKeyLength = ucol_getSortKey(myCollation, testString, UPRV_LENGTHOF(testString), baseKey, 256);

    ucol_setReorderCodes(myCollation, reorderCodes, UPRV_LENGTHOF(reorderCodes), &status);
    if(U_FAILURE(status)) {
        log_err_status(status, "ERROR: setting reorder codes: %s\n", myErrorName(status));
        return;
    }
    reorderKeyLength = ucol_getSortKey(myCollation, testString, UPRV_LENGTHOF(testString), reorderKey, 256);
    
    if (baseKeyLength != reorderKeyLength) {
        log_err("Key lengths not the same during reordering.\n");
        return;
    }
    
    for (i = 1; i < baseKeyLength; i++) {
        if (baseKey[i] != reorderKey[i]) {
            log_err("Collation key bytes not the same at position %d.\n", i);
            return;
        }
    }
    ucol_close(myCollation);
}

/*
 * Test reordering API.
 */
static void TestReorderingAPI(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UCollator  *myCollation;
    int32_t reorderCodes[3] = {USCRIPT_GREEK, USCRIPT_HAN, UCOL_REORDER_CODE_PUNCTUATION};
    int32_t duplicateReorderCodes[] = {USCRIPT_HIRAGANA, USCRIPT_GREEK, UCOL_REORDER_CODE_CURRENCY, USCRIPT_KATAKANA};
    int32_t reorderCodesStartingWithDefault[] = {UCOL_REORDER_CODE_DEFAULT, USCRIPT_GREEK, USCRIPT_HAN, UCOL_REORDER_CODE_PUNCTUATION};
    int32_t reorderCodeNone = UCOL_REORDER_CODE_NONE;
    UCollationResult collResult;
    int32_t retrievedReorderCodesLength;
    int32_t retrievedReorderCodes[10];
    UChar greekString[] = { 0x03b1 };
    UChar punctuationString[] = { 0x203e };
    int loopIndex;

    log_verbose("Testing non-lead bytes in a sort key with and without reordering\n");

    /* build collator tertiary */
    myCollation = ucol_open("", &status);
    ucol_setStrength(myCollation, UCOL_TERTIARY);
    if(U_FAILURE(status)) {
        log_err_status(status, "ERROR: in creation of collator: %s\n", myErrorName(status));
        return;
    }

    /* set the reorderding */
    ucol_setReorderCodes(myCollation, reorderCodes, UPRV_LENGTHOF(reorderCodes), &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: setting reorder codes: %s\n", myErrorName(status));
        return;
    }
    
    /* get the reordering */
    retrievedReorderCodesLength = ucol_getReorderCodes(myCollation, NULL, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR) {
        log_err_status(status, "ERROR: getting error codes should have returned U_BUFFER_OVERFLOW_ERROR : %s\n", myErrorName(status));
        return;
    }
    status = U_ZERO_ERROR;
    if (retrievedReorderCodesLength != UPRV_LENGTHOF(reorderCodes)) {
        log_err_status(status, "ERROR: retrieved reorder codes length was %d but should have been %d\n", retrievedReorderCodesLength, UPRV_LENGTHOF(reorderCodes));
        return;
    }
    /* now let's really get it */
    retrievedReorderCodesLength = ucol_getReorderCodes(myCollation, retrievedReorderCodes, UPRV_LENGTHOF(retrievedReorderCodes), &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: getting reorder codes: %s\n", myErrorName(status));
        return;
    }
    if (retrievedReorderCodesLength != UPRV_LENGTHOF(reorderCodes)) {
        log_err_status(status, "ERROR: retrieved reorder codes length was %d but should have been %d\n", retrievedReorderCodesLength, UPRV_LENGTHOF(reorderCodes));
        return;
    }
    for (loopIndex = 0; loopIndex < retrievedReorderCodesLength; loopIndex++) {
        if (retrievedReorderCodes[loopIndex] != reorderCodes[loopIndex]) {
            log_err_status(status, "ERROR: retrieved reorder code doesn't match set reorder code at index %d\n", loopIndex);
            return;
        }
    }
    collResult = ucol_strcoll(myCollation, greekString, UPRV_LENGTHOF(greekString), punctuationString, UPRV_LENGTHOF(punctuationString));
    if (collResult != UCOL_LESS) {
        log_err_status(status, "ERROR: collation result should have been UCOL_LESS\n");
        return;
    }
    
    /* clear the reordering */
    ucol_setReorderCodes(myCollation, NULL, 0, &status);    
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: setting reorder codes to NULL: %s\n", myErrorName(status));
        return;
    }

    /* get the reordering again */
    retrievedReorderCodesLength = ucol_getReorderCodes(myCollation, NULL, 0, &status);
    if (retrievedReorderCodesLength != 0) {
        log_err_status(status, "ERROR: retrieved reorder codes length was %d but should have been %d\n", retrievedReorderCodesLength, 0);
        return;
    }

    collResult = ucol_strcoll(myCollation, greekString, UPRV_LENGTHOF(greekString), punctuationString, UPRV_LENGTHOF(punctuationString));
    if (collResult != UCOL_GREATER) {
        log_err_status(status, "ERROR: collation result should have been UCOL_GREATER\n");
        return;
    }

    /* clear the reordering using [NONE] */
    ucol_setReorderCodes(myCollation, &reorderCodeNone, 1, &status);    
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: setting reorder codes to [NONE]: %s\n", myErrorName(status));
        return;
    }

    /* get the reordering again */
    retrievedReorderCodesLength = ucol_getReorderCodes(myCollation, NULL, 0, &status);
    if (retrievedReorderCodesLength != 0) {
        log_err_status(status,
                       "ERROR: [NONE] retrieved reorder codes length was %d but should have been 0\n",
                       retrievedReorderCodesLength);
        return;
    }

    /* test for error condition on duplicate reorder codes */
    ucol_setReorderCodes(myCollation, duplicateReorderCodes, UPRV_LENGTHOF(duplicateReorderCodes), &status);
    if (!U_FAILURE(status)) {
        log_err_status(status, "ERROR: setting duplicate reorder codes did not generate a failure\n");
        return;
    }
    
    status = U_ZERO_ERROR;
    /* test for reorder codes after a reset code */
    ucol_setReorderCodes(myCollation, reorderCodesStartingWithDefault, UPRV_LENGTHOF(reorderCodesStartingWithDefault), &status);
    if (!U_FAILURE(status)) {
        log_err_status(status, "ERROR: reorderd code sequence starting with default and having following codes didn't cause an error\n");
        return;
    }
    
    ucol_close(myCollation);
}

/*
 * Test reordering API.
 */
static void TestReorderingAPIWithRuleCreatedCollator(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UCollator  *myCollation;
    UChar rules[90];
    static const int32_t rulesReorderCodes[2] = {USCRIPT_HAN, USCRIPT_GREEK};
    static const int32_t reorderCodes[3] = {USCRIPT_GREEK, USCRIPT_HAN, UCOL_REORDER_CODE_PUNCTUATION};
    static const int32_t onlyDefault[1] = {UCOL_REORDER_CODE_DEFAULT};
    UCollationResult collResult;
    int32_t retrievedReorderCodesLength;
    int32_t retrievedReorderCodes[10];
    static const UChar greekString[] = { 0x03b1 };
    static const UChar punctuationString[] = { 0x203e };
    static const UChar hanString[] = { 0x65E5, 0x672C };
    int loopIndex;

    log_verbose("Testing non-lead bytes in a sort key with and without reordering\n");

    /* build collator from rules */
    u_uastrcpy(rules, "[reorder Hani Grek]");
    myCollation = ucol_openRules(rules, u_strlen(rules), UCOL_DEFAULT, UCOL_TERTIARY, NULL, &status);
    if(U_FAILURE(status)) {
        log_err_status(status, "ERROR: in creation of collator: %s\n", myErrorName(status));
        return;
    }

    /* get the reordering */
    retrievedReorderCodesLength = ucol_getReorderCodes(myCollation, retrievedReorderCodes, UPRV_LENGTHOF(retrievedReorderCodes), &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: getting reorder codes: %s\n", myErrorName(status));
        return;
    }
    if (retrievedReorderCodesLength != UPRV_LENGTHOF(rulesReorderCodes)) {
        log_err_status(status, "ERROR: retrieved reorder codes length was %d but should have been %d\n", retrievedReorderCodesLength, UPRV_LENGTHOF(rulesReorderCodes));
        return;
    }
    for (loopIndex = 0; loopIndex < retrievedReorderCodesLength; loopIndex++) {
        if (retrievedReorderCodes[loopIndex] != rulesReorderCodes[loopIndex]) {
            log_err_status(status, "ERROR: retrieved reorder code doesn't match set reorder code at index %d\n", loopIndex);
            return;
        }
    }
    collResult = ucol_strcoll(myCollation, greekString, UPRV_LENGTHOF(greekString), hanString, UPRV_LENGTHOF(hanString));
    if (collResult != UCOL_GREATER) {
        log_err_status(status, "ERROR: collation result should have been UCOL_GREATER\n");
        return;
    }

    /* set the reordering */
    ucol_setReorderCodes(myCollation, reorderCodes, UPRV_LENGTHOF(reorderCodes), &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: setting reorder codes: %s\n", myErrorName(status));
        return;
    }

    /* get the reordering */
    retrievedReorderCodesLength = ucol_getReorderCodes(myCollation, NULL, 0, &status);
    if (status != U_BUFFER_OVERFLOW_ERROR) {
        log_err_status(status, "ERROR: getting error codes should have returned U_BUFFER_OVERFLOW_ERROR : %s\n", myErrorName(status));
        return;
    }
    status = U_ZERO_ERROR;
    if (retrievedReorderCodesLength != UPRV_LENGTHOF(reorderCodes)) {
        log_err_status(status, "ERROR: retrieved reorder codes length was %d but should have been %d\n", retrievedReorderCodesLength, UPRV_LENGTHOF(reorderCodes));
        return;
    }
    /* now let's really get it */
    retrievedReorderCodesLength = ucol_getReorderCodes(myCollation, retrievedReorderCodes, UPRV_LENGTHOF(retrievedReorderCodes), &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: getting reorder codes: %s\n", myErrorName(status));
        return;
    }
    if (retrievedReorderCodesLength != UPRV_LENGTHOF(reorderCodes)) {
        log_err_status(status, "ERROR: retrieved reorder codes length was %d but should have been %d\n", retrievedReorderCodesLength, UPRV_LENGTHOF(reorderCodes));
        return;
    }
    for (loopIndex = 0; loopIndex < retrievedReorderCodesLength; loopIndex++) {
        if (retrievedReorderCodes[loopIndex] != reorderCodes[loopIndex]) {
            log_err_status(status, "ERROR: retrieved reorder code doesn't match set reorder code at index %d\n", loopIndex);
            return;
        }
    }
    collResult = ucol_strcoll(myCollation, greekString, UPRV_LENGTHOF(greekString), punctuationString, UPRV_LENGTHOF(punctuationString));
    if (collResult != UCOL_LESS) {
        log_err_status(status, "ERROR: collation result should have been UCOL_LESS\n");
        return;
    }

    /* clear the reordering */
    ucol_setReorderCodes(myCollation, NULL, 0, &status);    
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: setting reorder codes to NULL: %s\n", myErrorName(status));
        return;
    }

    /* get the reordering again */
    retrievedReorderCodesLength = ucol_getReorderCodes(myCollation, NULL, 0, &status);
    if (retrievedReorderCodesLength != 0) {
        log_err_status(status, "ERROR: retrieved reorder codes length was %d but should have been %d\n", retrievedReorderCodesLength, 0);
        return;
    }

    collResult = ucol_strcoll(myCollation, greekString, UPRV_LENGTHOF(greekString), punctuationString, UPRV_LENGTHOF(punctuationString));
    if (collResult != UCOL_GREATER) {
        log_err_status(status, "ERROR: collation result should have been UCOL_GREATER\n");
        return;
    }

    /* reset the reordering */
    ucol_setReorderCodes(myCollation, onlyDefault, 1, &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: setting reorder codes to {default}: %s\n", myErrorName(status));
        return;
    }
    retrievedReorderCodesLength = ucol_getReorderCodes(myCollation, retrievedReorderCodes, UPRV_LENGTHOF(retrievedReorderCodes), &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: getting reorder codes: %s\n", myErrorName(status));
        return;
    }
    if (retrievedReorderCodesLength != UPRV_LENGTHOF(rulesReorderCodes)) {
        log_err_status(status, "ERROR: retrieved reorder codes length was %d but should have been %d\n", retrievedReorderCodesLength, UPRV_LENGTHOF(rulesReorderCodes));
        return;
    }
    for (loopIndex = 0; loopIndex < retrievedReorderCodesLength; loopIndex++) {
        if (retrievedReorderCodes[loopIndex] != rulesReorderCodes[loopIndex]) {
            log_err_status(status, "ERROR: retrieved reorder code doesn't match set reorder code at index %d\n", loopIndex);
            return;
        }
    }

    ucol_close(myCollation);
}

static UBool containsExpectedScript(const int32_t scripts[], int32_t length, int32_t expectedScript) {
    int32_t i;
    for (i = 0; i < length; ++i) {
        if (expectedScript == scripts[i]) { return true; }
    }
    return false;
}

static void TestEquivalentReorderingScripts(void) {
    // Beginning with ICU 55, collation reordering moves single scripts
    // rather than groups of scripts,
    // except where scripts share a range and sort primary-equal.
    UErrorCode status = U_ZERO_ERROR;
    int32_t equivalentScripts[100];
    int32_t length;
    int i;
    int32_t prevScript;
    /* These scripts are expected to be equivalent. */
    static const int32_t expectedScripts[] = {
        USCRIPT_HIRAGANA,
        USCRIPT_KATAKANA,
        USCRIPT_KATAKANA_OR_HIRAGANA
    };

    equivalentScripts[0] = 0;
    length = ucol_getEquivalentReorderCodes(
            USCRIPT_GOTHIC, equivalentScripts, UPRV_LENGTHOF(equivalentScripts), &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR/Gothic: retrieving equivalent reorder codes: %s\n", myErrorName(status));
        return;
    }
    if (length != 1 || equivalentScripts[0] != USCRIPT_GOTHIC) {
        log_err("ERROR/Gothic: retrieved equivalent scripts wrong: "
                "length expected 1, was = %d; expected [%d] was [%d]\n",
                length, USCRIPT_GOTHIC, equivalentScripts[0]);
    }

    length = ucol_getEquivalentReorderCodes(
            USCRIPT_HIRAGANA, equivalentScripts, UPRV_LENGTHOF(equivalentScripts), &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR/Hiragana: retrieving equivalent reorder codes: %s\n", myErrorName(status));
        return;
    }
    if (length != UPRV_LENGTHOF(expectedScripts)) {
        log_err("ERROR/Hiragana: retrieved equivalent script length wrong: "
                "expected %d, was = %d\n",
                UPRV_LENGTHOF(expectedScripts), length);
    }
    prevScript = -1;
    for (i = 0; i < length; ++i) {
        int32_t script = equivalentScripts[i];
        if (script <= prevScript) {
            log_err("ERROR/Hiragana: equivalent scripts out of order at index %d\n", i);
        }
        prevScript = script;
    }
    for (i = 0; i < UPRV_LENGTHOF(expectedScripts); i++) {
        if (!containsExpectedScript(equivalentScripts, length, expectedScripts[i])) {
            log_err("ERROR/Hiragana: equivalent scripts do not contain %d\n",
                    expectedScripts[i]);
        }
    }

    length = ucol_getEquivalentReorderCodes(
            USCRIPT_KATAKANA, equivalentScripts, UPRV_LENGTHOF(equivalentScripts), &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR/Katakana: retrieving equivalent reorder codes: %s\n", myErrorName(status));
        return;
    }
    if (length != UPRV_LENGTHOF(expectedScripts)) {
        log_err("ERROR/Katakana: retrieved equivalent script length wrong: "
                "expected %d, was = %d\n",
                UPRV_LENGTHOF(expectedScripts), length);
    }
    for (i = 0; i < UPRV_LENGTHOF(expectedScripts); i++) {
        if (!containsExpectedScript(equivalentScripts, length, expectedScripts[i])) {
            log_err("ERROR/Katakana: equivalent scripts do not contain %d\n",
                    expectedScripts[i]);
        }
    }

    length = ucol_getEquivalentReorderCodes(
            USCRIPT_KATAKANA_OR_HIRAGANA, equivalentScripts, UPRV_LENGTHOF(equivalentScripts), &status);
    if (U_FAILURE(status) || length != UPRV_LENGTHOF(expectedScripts)) {
        log_err("ERROR/Hrkt: retrieved equivalent script length wrong: "
                "expected %d, was = %d\n",
                UPRV_LENGTHOF(expectedScripts), length);
    }

    length = ucol_getEquivalentReorderCodes(
            USCRIPT_HAN, equivalentScripts, UPRV_LENGTHOF(equivalentScripts), &status);
    if (U_FAILURE(status) || length != 3) {
        log_err("ERROR/Hani: retrieved equivalent script length wrong: "
                "expected 3, was = %d\n", length);
    }
    length = ucol_getEquivalentReorderCodes(
            USCRIPT_SIMPLIFIED_HAN, equivalentScripts, UPRV_LENGTHOF(equivalentScripts), &status);
    if (U_FAILURE(status) || length != 3) {
        log_err("ERROR/Hans: retrieved equivalent script length wrong: "
                "expected 3, was = %d\n", length);
    }
    length = ucol_getEquivalentReorderCodes(
            USCRIPT_TRADITIONAL_HAN, equivalentScripts, UPRV_LENGTHOF(equivalentScripts), &status);
    if (U_FAILURE(status) || length != 3) {
        log_err("ERROR/Hant: retrieved equivalent script length wrong: "
                "expected 3, was = %d\n", length);
    }

    length = ucol_getEquivalentReorderCodes(
            USCRIPT_MEROITIC_CURSIVE, equivalentScripts, UPRV_LENGTHOF(equivalentScripts), &status);
    if (U_FAILURE(status) || length != 2) {
        log_err("ERROR/Merc: retrieved equivalent script length wrong: "
                "expected 2, was = %d\n", length);
    }
    length = ucol_getEquivalentReorderCodes(
            USCRIPT_MEROITIC_HIEROGLYPHS, equivalentScripts, UPRV_LENGTHOF(equivalentScripts), &status);
    if (U_FAILURE(status) || length != 2) {
        log_err("ERROR/Mero: retrieved equivalent script length wrong: "
                "expected 2, was = %d\n", length);
    }
}

static void TestReorderingAcrossCloning(void) 
{
    UErrorCode status = U_ZERO_ERROR;
    UCollator  *myCollation;
    int32_t reorderCodes[3] = {USCRIPT_GREEK, USCRIPT_HAN, UCOL_REORDER_CODE_PUNCTUATION};
    UCollator *clonedCollation;
    int32_t retrievedReorderCodesLength;
    int32_t retrievedReorderCodes[10];
    int loopIndex;

    log_verbose("Testing non-lead bytes in a sort key with and without reordering\n");

    /* build collator tertiary */
    myCollation = ucol_open("", &status);
    ucol_setStrength(myCollation, UCOL_TERTIARY);
    if(U_FAILURE(status)) {
        log_err_status(status, "ERROR: in creation of collator: %s\n", myErrorName(status));
        return;
    }

    /* set the reorderding */
    ucol_setReorderCodes(myCollation, reorderCodes, UPRV_LENGTHOF(reorderCodes), &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: setting reorder codes: %s\n", myErrorName(status));
        return;
    }
    
    /* clone the collator */
    clonedCollation = ucol_safeClone(myCollation, NULL, NULL, &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: cloning collator: %s\n", myErrorName(status));
        return;
    }
    
    /* get the reordering */
    retrievedReorderCodesLength = ucol_getReorderCodes(clonedCollation, retrievedReorderCodes, UPRV_LENGTHOF(retrievedReorderCodes), &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: getting reorder codes: %s\n", myErrorName(status));
        return;
    }
    if (retrievedReorderCodesLength != UPRV_LENGTHOF(reorderCodes)) {
        log_err_status(status, "ERROR: retrieved reorder codes length was %d but should have been %d\n", retrievedReorderCodesLength, UPRV_LENGTHOF(reorderCodes));
        return;
    }
    for (loopIndex = 0; loopIndex < retrievedReorderCodesLength; loopIndex++) {
        if (retrievedReorderCodes[loopIndex] != reorderCodes[loopIndex]) {
            log_err_status(status, "ERROR: retrieved reorder code doesn't match set reorder code at index %d\n", loopIndex);
            return;
        }
    }
    
    /*uprv_free(buffer);*/
    ucol_close(myCollation);
    ucol_close(clonedCollation);
}

/*
 * Utility function to test one collation reordering test case set.
 * @param testcases Array of test cases.
 * @param n_testcases Size of the array testcases.
 * @param reorderTokens Array of reordering codes.
 * @param reorderTokensLen Size of the array reorderTokens.
 */
static void doTestOneReorderingAPITestCase(const OneTestCase testCases[], uint32_t testCasesLen, const int32_t reorderTokens[], int32_t reorderTokensLen)
{
    uint32_t testCaseNum;
    UErrorCode status = U_ZERO_ERROR;
    UCollator  *myCollation;

    myCollation = ucol_open("", &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: in creation of collator: %s\n", myErrorName(status));
        return;
    }
    ucol_setReorderCodes(myCollation, reorderTokens, reorderTokensLen, &status);
    if(U_FAILURE(status)) {
        log_err_status(status, "ERROR: while setting script order: %s\n", myErrorName(status));
        return;
    }
        
    for (testCaseNum = 0; testCaseNum < testCasesLen; ++testCaseNum) {
        doTest(myCollation,
            testCases[testCaseNum].source,
            testCases[testCaseNum].target,
            testCases[testCaseNum].result
        );
    }
    ucol_close(myCollation);
}

static void TestGreekFirstReorder(void)
{
    const char* strRules[] = {
        "[reorder Grek]"
    };

    const int32_t apiRules[] = {
        USCRIPT_GREEK
    };
    
    const static OneTestCase privateUseCharacterStrings[] = {
        { {0x0391}, {0x0391}, UCOL_EQUAL },
        { {0x0041}, {0x0391}, UCOL_GREATER },
        { {0x03B1, 0x0041}, {0x03B1, 0x0391}, UCOL_GREATER },
        { {0x0060}, {0x0391}, UCOL_LESS },
        { {0x0391}, {0xe2dc}, UCOL_LESS },
        { {0x0391}, {0x0060}, UCOL_GREATER },
    };

    /* Test rules creation */
    doTestOneTestCase(privateUseCharacterStrings, UPRV_LENGTHOF(privateUseCharacterStrings), strRules, UPRV_LENGTHOF(strRules));

    /* Test collation reordering API */
    doTestOneReorderingAPITestCase(privateUseCharacterStrings, UPRV_LENGTHOF(privateUseCharacterStrings), apiRules, UPRV_LENGTHOF(apiRules));
}

static void TestGreekLastReorder(void)
{
    const char* strRules[] = {
        "[reorder Zzzz Grek]"
    };

    const int32_t apiRules[] = {
        USCRIPT_UNKNOWN, USCRIPT_GREEK
    };
    
    const static OneTestCase privateUseCharacterStrings[] = {
        { {0x0391}, {0x0391}, UCOL_EQUAL },
        { {0x0041}, {0x0391}, UCOL_LESS },
        { {0x03B1, 0x0041}, {0x03B1, 0x0391}, UCOL_LESS },
        { {0x0060}, {0x0391}, UCOL_LESS },
        { {0x0391}, {0xe2dc}, UCOL_GREATER },
    };
    
    /* Test rules creation */
    doTestOneTestCase(privateUseCharacterStrings, UPRV_LENGTHOF(privateUseCharacterStrings), strRules, UPRV_LENGTHOF(strRules));

    /* Test collation reordering API */
    doTestOneReorderingAPITestCase(privateUseCharacterStrings, UPRV_LENGTHOF(privateUseCharacterStrings), apiRules, UPRV_LENGTHOF(apiRules));
}

static void TestNonScriptReorder(void)
{
    const char* strRules[] = {
        "[reorder Grek Symbol DIGIT Latn Punct space Zzzz cURRENCy]"
    };

    const int32_t apiRules[] = {
        USCRIPT_GREEK, UCOL_REORDER_CODE_SYMBOL, UCOL_REORDER_CODE_DIGIT, USCRIPT_LATIN, 
        UCOL_REORDER_CODE_PUNCTUATION, UCOL_REORDER_CODE_SPACE, USCRIPT_UNKNOWN, 
        UCOL_REORDER_CODE_CURRENCY
    };

    const static OneTestCase privateUseCharacterStrings[] = {
        { {0x0391}, {0x0041}, UCOL_LESS },
        { {0x0041}, {0x0391}, UCOL_GREATER },
        { {0x0060}, {0x0041}, UCOL_LESS },
        { {0x0060}, {0x0391}, UCOL_GREATER },
        { {0x0024}, {0x0041}, UCOL_GREATER },
    };
    
    /* Test rules creation */
    doTestOneTestCase(privateUseCharacterStrings, UPRV_LENGTHOF(privateUseCharacterStrings), strRules, UPRV_LENGTHOF(strRules));

    /* Test collation reordering API */
    doTestOneReorderingAPITestCase(privateUseCharacterStrings, UPRV_LENGTHOF(privateUseCharacterStrings), apiRules, UPRV_LENGTHOF(apiRules));
}

static void TestHaniReorder(void)
{
    const char* strRules[] = {
        "[reorder Hani]"
    };
    const int32_t apiRules[] = {
        USCRIPT_HAN
    };

    const static OneTestCase privateUseCharacterStrings[] = {
        { {0x4e00}, {0x0041}, UCOL_LESS },
        { {0x4e00}, {0x0060}, UCOL_GREATER },
        { {0xD86D, 0xDF40}, {0x0041}, UCOL_LESS },
        { {0xD86D, 0xDF40}, {0x0060}, UCOL_GREATER },
        { {0x4e00}, {0xD86D, 0xDF40}, UCOL_LESS },
        { {0xfa27}, {0x0041}, UCOL_LESS },
        { {0xD869, 0xDF00}, {0x0041}, UCOL_LESS },
    };
    
    /* Test rules creation */
    doTestOneTestCase(privateUseCharacterStrings, UPRV_LENGTHOF(privateUseCharacterStrings), strRules, UPRV_LENGTHOF(strRules));

    /* Test collation reordering API */
    doTestOneReorderingAPITestCase(privateUseCharacterStrings, UPRV_LENGTHOF(privateUseCharacterStrings), apiRules, UPRV_LENGTHOF(apiRules));
}

static void TestHaniReorderWithOtherRules(void)
{
    const char* strRules[] = {
        "[reorder Hani] &b<a"
    };
    /*const int32_t apiRules[] = {
        USCRIPT_HAN
    };*/

    const static OneTestCase privateUseCharacterStrings[] = {
        { {0x4e00}, {0x0041}, UCOL_LESS },
        { {0x4e00}, {0x0060}, UCOL_GREATER },
        { {0xD86D, 0xDF40}, {0x0041}, UCOL_LESS },
        { {0xD86D, 0xDF40}, {0x0060}, UCOL_GREATER },
        { {0x4e00}, {0xD86D, 0xDF40}, UCOL_LESS },
        { {0xfa27}, {0x0041}, UCOL_LESS },
        { {0xD869, 0xDF00}, {0x0041}, UCOL_LESS },
        { {0x0062}, {0x0061}, UCOL_LESS },
    };
    
    /* Test rules creation */
    doTestOneTestCase(privateUseCharacterStrings, UPRV_LENGTHOF(privateUseCharacterStrings), strRules, UPRV_LENGTHOF(strRules));
}

static void TestMultipleReorder(void)
{
    const char* strRules[] = {
        "[reorder Grek Zzzz DIGIT Latn Hani]"
    };

    const int32_t apiRules[] = {
        USCRIPT_GREEK, USCRIPT_UNKNOWN, UCOL_REORDER_CODE_DIGIT, USCRIPT_LATIN, USCRIPT_HAN
    };
        
    const static OneTestCase collationTestCases[] = {
        { {0x0391}, {0x0041}, UCOL_LESS},
        { {0x0031}, {0x0041}, UCOL_LESS},
        { {0x0041}, {0x4e00}, UCOL_LESS},
    };

    /* Test rules creation */
    doTestOneTestCase(collationTestCases, UPRV_LENGTHOF(collationTestCases), strRules, UPRV_LENGTHOF(strRules));

    /* Test collation reordering API */
    doTestOneReorderingAPITestCase(collationTestCases, UPRV_LENGTHOF(collationTestCases), apiRules, UPRV_LENGTHOF(apiRules));
}

/*
 * Test that covers issue reported in ticket 8814
 */
static void TestReorderWithNumericCollation(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UCollator  *myCollation;
    UCollator  *myReorderCollation;
    int32_t reorderCodes[] = {UCOL_REORDER_CODE_SPACE, UCOL_REORDER_CODE_PUNCTUATION, UCOL_REORDER_CODE_SYMBOL, UCOL_REORDER_CODE_DIGIT, USCRIPT_GREEK,USCRIPT_LATIN, USCRIPT_HEBREW, UCOL_REORDER_CODE_OTHERS};
    /* UChar fortyS[] = { 0x0034, 0x0030, 0x0053 };
    UChar fortyThreeP[] = { 0x0034, 0x0033, 0x0050 }; */
    UChar fortyS[] = { 0x0053 };
    UChar fortyThreeP[] = { 0x0050 };
    uint8_t fortyS_sortKey[128];
    int32_t fortyS_sortKey_Length;
    uint8_t fortyThreeP_sortKey[128];
    int32_t fortyThreeP_sortKey_Length;
    uint8_t fortyS_sortKey_reorder[128];
    int32_t fortyS_sortKey_reorder_Length;
    uint8_t fortyThreeP_sortKey_reorder[128];
    int32_t fortyThreeP_sortKey_reorder_Length;
    UCollationResult collResult;
    UCollationResult collResultReorder;

    log_verbose("Testing reordering with and without numeric collation\n");

    /* build collator tertiary with numeric */
    myCollation = ucol_open("", &status);
    /*
    ucol_setStrength(myCollation, UCOL_TERTIARY);
    */
    ucol_setAttribute(myCollation, UCOL_NUMERIC_COLLATION, UCOL_ON, &status);
    if(U_FAILURE(status)) {
        log_err_status(status, "ERROR: in creation of collator: %s\n", myErrorName(status));
        return;
    }
    
    /* build collator tertiary with numeric and reordering */
    myReorderCollation = ucol_open("", &status);
    /*
    ucol_setStrength(myReorderCollation, UCOL_TERTIARY);
    */
    ucol_setAttribute(myReorderCollation, UCOL_NUMERIC_COLLATION, UCOL_ON, &status);
    ucol_setReorderCodes(myReorderCollation, reorderCodes, UPRV_LENGTHOF(reorderCodes), &status);
    if(U_FAILURE(status)) {
        log_err_status(status, "ERROR: in creation of collator: %s\n", myErrorName(status));
        return;
    }
    
    fortyS_sortKey_Length = ucol_getSortKey(myCollation, fortyS, UPRV_LENGTHOF(fortyS), fortyS_sortKey, 128);
    fortyThreeP_sortKey_Length = ucol_getSortKey(myCollation, fortyThreeP, UPRV_LENGTHOF(fortyThreeP), fortyThreeP_sortKey, 128);
    fortyS_sortKey_reorder_Length = ucol_getSortKey(myReorderCollation, fortyS, UPRV_LENGTHOF(fortyS), fortyS_sortKey_reorder, 128);
    fortyThreeP_sortKey_reorder_Length = ucol_getSortKey(myReorderCollation, fortyThreeP, UPRV_LENGTHOF(fortyThreeP), fortyThreeP_sortKey_reorder, 128);

    if (fortyS_sortKey_Length < 0 || fortyThreeP_sortKey_Length < 0 || fortyS_sortKey_reorder_Length < 0 || fortyThreeP_sortKey_reorder_Length < 0) {
        log_err_status(status, "ERROR: couldn't generate sort keys\n");
        return;
    }
    collResult = ucol_strcoll(myCollation, fortyS, UPRV_LENGTHOF(fortyS), fortyThreeP, UPRV_LENGTHOF(fortyThreeP));
    collResultReorder = ucol_strcoll(myReorderCollation, fortyS, UPRV_LENGTHOF(fortyS), fortyThreeP, UPRV_LENGTHOF(fortyThreeP));
    /*
    fprintf(stderr, "\tcollResult = %x\n", collResult);
    fprintf(stderr, "\tcollResultReorder = %x\n", collResultReorder);
    fprintf(stderr, "\nfortyS\n");
    for (i = 0; i < fortyS_sortKey_Length; i++) {
        fprintf(stderr, "%x --- %x\n", fortyS_sortKey[i], fortyS_sortKey_reorder[i]);
    }
    fprintf(stderr, "\nfortyThreeP\n");
    for (i = 0; i < fortyThreeP_sortKey_Length; i++) {
        fprintf(stderr, "%x --- %x\n", fortyThreeP_sortKey[i], fortyThreeP_sortKey_reorder[i]);
    }
    */
    if (collResult != collResultReorder) {
        log_err_status(status, "ERROR: collation results should have been the same.\n");
        return;
    }
    
    ucol_close(myCollation);
    ucol_close(myReorderCollation);
}

static int compare_uint8_t_arrays(const uint8_t* a, const uint8_t* b)
{
  for (; *a == *b; ++a, ++b) {
    if (*a == 0) {
      return 0;
    }
  }
  return (*a < *b ? -1 : 1);
}

static void TestImportRulesDeWithPhonebook(void)
{
  const char* normalRules[] = {
    "&a<\\u00e6<\\u00c6<\\u00dc<\\u00fc",
    "&a<<\\u00e6<<\\u00c6<<\\u00dc<<\\u00fc",
    "&a<<\\u00e6<<<\\u00c6<<\\u00dc<<\\u00fc",
  };
  const OneTestCase normalTests[] = {
    { {0x00e6}, {0x00c6}, UCOL_LESS},
    { {0x00fc}, {0x00dc}, UCOL_GREATER},
  };

  const char* importRules[] = {
    "&a<\\u00e6<\\u00c6<\\u00dc<\\u00fc[import de-u-co-phonebk]",
    "&a<<\\u00e6<<\\u00c6<<\\u00dc<<\\u00fc[import de-u-co-phonebk]",
    "&a<<\\u00e6<<<\\u00c6<<\\u00dc<<\\u00fc[import de-u-co-phonebk]",
  };
  const OneTestCase importTests[] = {
    { {0x00e6}, {0x00c6}, UCOL_LESS},
    { {0x00fc}, {0x00dc}, UCOL_LESS},
  };

  doTestOneTestCase(normalTests, UPRV_LENGTHOF(normalTests), normalRules, UPRV_LENGTHOF(normalRules));
  doTestOneTestCase(importTests, UPRV_LENGTHOF(importTests), importRules, UPRV_LENGTHOF(importRules));
}

#if 0
static void TestImportRulesFiWithEor(void)
{
  /* DUCET. */
  const char* defaultRules[] = {
    "&a<b",                                    /* Dummy rule. */
  };

  const OneTestCase defaultTests[] = {
    { {0x0110}, {0x00F0}, UCOL_LESS},
    { {0x00a3}, {0x00a5}, UCOL_LESS},
    { {0x0061}, {0x0061, 0x00a3}, UCOL_LESS},
  };

  /* European Ordering rules: ignore currency characters. */
  const char* eorRules[] = {
    "[import root-u-co-eor]",
  };

  const OneTestCase eorTests[] = {
    { {0x0110}, {0x00F0}, UCOL_LESS},
    { {0x00a3}, {0x00a5}, UCOL_EQUAL},
    { {0x0061}, {0x0061, 0x00a3}, UCOL_EQUAL},
  };

  const char* fiStdRules[] = {
    "[import fi-u-co-standard]",
  };

  const OneTestCase fiStdTests[] = {
    { {0x0110}, {0x00F0}, UCOL_GREATER},
    { {0x00a3}, {0x00a5}, UCOL_LESS},
    { {0x0061}, {0x0061, 0x00a3}, UCOL_LESS},
  };

  /* Both European Ordering Rules and Fi Standard Rules. */
  const char* eorFiStdRules[] = {
    "[import root-u-co-eor][import fi-u-co-standard]",
  };

  /* This is essentially same as the one before once fi.txt is updated with import. */
  const char* fiEorRules[] = {
    "[import fi-u-co-eor]",
  };

  const OneTestCase fiEorTests[] = {
    { {0x0110}, {0x00F0}, UCOL_GREATER},
    { {0x00a3}, {0x00a5}, UCOL_EQUAL},
    { {0x0061}, {0x0061, 0x00a3}, UCOL_EQUAL},
  };

  doTestOneTestCase(defaultTests, UPRV_LENGTHOF(defaultTests), defaultRules, UPRV_LENGTHOF(defaultRules));
  doTestOneTestCase(eorTests, UPRV_LENGTHOF(eorTests), eorRules, UPRV_LENGTHOF(eorRules));
  doTestOneTestCase(fiStdTests, UPRV_LENGTHOF(fiStdTests), fiStdRules, UPRV_LENGTHOF(fiStdRules));
  doTestOneTestCase(fiEorTests, UPRV_LENGTHOF(fiEorTests), eorFiStdRules, UPRV_LENGTHOF(eorFiStdRules));

  log_knownIssue("8962", NULL);
  /* TODO: Fix ICU ticket #8962 by uncommenting the following test after fi.txt is updated with the following rule:
        eor{
            Sequence{
                "[import root-u-co-eor][import fi-u-co-standard]"
            }
            Version{"21.0"}
        }
  */
  /* doTestOneTestCase(fiEorTests, UPRV_LENGTHOF(fiEorTests), fiEorRules, UPRV_LENGTHOF(fiEorRules)); */

}
#endif

#if 0
/*
 * This test case tests inclusion with the unihan rules, but this cannot be included now, unless
 * the resource files are built with -includeUnihanColl option.
 * TODO: Uncomment this function and make it work when unihan rules are built by default.
 */
static void TestImportRulesCJKWithUnihan(void)
{
  /* DUCET. */
  const char* defaultRules[] = {
    "&a<b",                                    /* Dummy rule. */
  };

  const OneTestCase defaultTests[] = {
    { {0x3402}, {0x4e1e}, UCOL_GREATER},
  };

  /* European Ordering rules: ignore currency characters. */
  const char* unihanRules[] = {
    "[import ko-u-co-unihan]",
  };

  const OneTestCase unihanTests[] = {
    { {0x3402}, {0x4e1e}, UCOL_LESS},
  };

  doTestOneTestCase(defaultTests, UPRV_LENGTHOF(defaultTests), defaultRules, UPRV_LENGTHOF(defaultRules));
  doTestOneTestCase(unihanTests, UPRV_LENGTHOF(unihanTests), unihanRules, UPRV_LENGTHOF(unihanRules));

}
#endif

static void TestImport(void)
{
    UCollator* vicoll;
    UCollator* escoll;
    UCollator* viescoll;
    UCollator* importviescoll;
    UParseError error;
    UErrorCode status = U_ZERO_ERROR;
    UChar* virules;
    int32_t viruleslength;
    UChar* esrules;
    int32_t esruleslength;
    UChar* viesrules;
    int32_t viesruleslength;
    char srules[500] = "[import vi][import es]";
    UChar rules[500];
    uint32_t length = 0;
    int32_t itemCount;
    int32_t i, k;
    UChar32 start;
    UChar32 end;
    UChar str[500];
    int32_t strLength;

    uint8_t sk1[500];
    uint8_t sk2[500];

    UBool b;
    USet* tailoredSet;
    USet* importTailoredSet;


    vicoll = ucol_open("vi", &status);
    if(U_FAILURE(status)){
        log_err_status(status, "ERROR: Call ucol_open(\"vi\", ...): %s\n", myErrorName(status));
        return;
    }

    virules = (UChar*) ucol_getRules(vicoll, &viruleslength);
    if(viruleslength == 0) {
        log_data_err("missing vi tailoring rule string\n");
        ucol_close(vicoll);
        return;
    }
    escoll = ucol_open("es", &status);
    esrules = (UChar*) ucol_getRules(escoll, &esruleslength);
    viesrules = (UChar*)uprv_malloc((viruleslength+esruleslength+1)*sizeof(UChar*));
    viesrules[0] = 0;
    u_strcat(viesrules, virules);
    u_strcat(viesrules, esrules);
    viesruleslength = viruleslength + esruleslength;
    viescoll = ucol_openRules(viesrules, viesruleslength, UCOL_ON, UCOL_TERTIARY, &error, &status);

    /* u_strFromUTF8(rules, 500, &length, srules, strlen(srules), &status); */
    length = u_unescape(srules, rules, 500);
    importviescoll = ucol_openRules(rules, length, UCOL_ON, UCOL_TERTIARY, &error, &status);
    if(U_FAILURE(status)){
        log_err_status(status, "ERROR: in creation of rule based collator: %s\n", myErrorName(status));
        return;
    }

    tailoredSet = ucol_getTailoredSet(viescoll, &status);
    importTailoredSet = ucol_getTailoredSet(importviescoll, &status);

    if(!uset_equals(tailoredSet, importTailoredSet)){
        log_err("Tailored sets not equal");
    }

    uset_close(importTailoredSet);

    itemCount = uset_getItemCount(tailoredSet);

    for( i = 0; i < itemCount; i++){
        strLength = uset_getItem(tailoredSet, i, &start, &end, str, 500, &status);
        if(strLength < 2){
            for (; start <= end; start++){
                k = 0;
                U16_APPEND(str, k, 500, start, b);
                (void)b;    /* Suppress set but not used warning. */
                ucol_getSortKey(viescoll, str, 1, sk1, 500);
                ucol_getSortKey(importviescoll, str, 1, sk2, 500);
                if(compare_uint8_t_arrays(sk1, sk2) != 0){
                    log_err("Sort key for %s not equal\n", str);
                    break;
                }
            }
        }else{
            ucol_getSortKey(viescoll, str, strLength, sk1, 500);
            ucol_getSortKey(importviescoll, str, strLength, sk2, 500);
            if(compare_uint8_t_arrays(sk1, sk2) != 0){
                log_err("ZZSort key for %s not equal\n", str);
                break;
            }

        }
    }

    uset_close(tailoredSet);

    uprv_free(viesrules);

    ucol_close(vicoll);
    ucol_close(escoll);
    ucol_close(viescoll);
    ucol_close(importviescoll);
}

static void TestImportWithType(void)
{
    UCollator* vicoll;
    UCollator* decoll;
    UCollator* videcoll;
    UCollator* importvidecoll;
    UParseError error;
    UErrorCode status = U_ZERO_ERROR;
    const UChar* virules;
    int32_t viruleslength;
    const UChar* derules;
    int32_t deruleslength;
    UChar* viderules;
    int32_t videruleslength;
    const char srules[500] = "[import vi][import de-u-co-phonebk]";
    UChar rules[500];
    uint32_t length = 0;
    int32_t itemCount;
    int32_t i, k;
    UChar32 start;
    UChar32 end;
    UChar str[500];
    int32_t strLength;

    uint8_t sk1[500];
    uint8_t sk2[500];

    USet* tailoredSet;
    USet* importTailoredSet;

    vicoll = ucol_open("vi", &status);
    if(U_FAILURE(status)){
        log_err_status(status, "ERROR: in creation of rule based collator: %s\n", myErrorName(status));
        return;
    }
    virules = ucol_getRules(vicoll, &viruleslength);
    if(viruleslength == 0) {
        log_data_err("missing vi tailoring rule string\n");
        ucol_close(vicoll);
        return;
    }
    /* decoll = ucol_open("de@collation=phonebook", &status); */
    decoll = ucol_open("de-u-co-phonebk", &status);
    if(U_FAILURE(status)){
        log_err_status(status, "ERROR: in creation of rule based collator: %s\n", myErrorName(status));
        return;
    }


    derules = ucol_getRules(decoll, &deruleslength);
    viderules = (UChar*)uprv_malloc((viruleslength+deruleslength+1)*sizeof(UChar*));
    viderules[0] = 0;
    u_strcat(viderules, virules);
    u_strcat(viderules, derules);
    videruleslength = viruleslength + deruleslength;
    videcoll = ucol_openRules(viderules, videruleslength, UCOL_ON, UCOL_TERTIARY, &error, &status);

    /* u_strFromUTF8(rules, 500, &length, srules, strlen(srules), &status); */
    length = u_unescape(srules, rules, 500);
    importvidecoll = ucol_openRules(rules, length, UCOL_ON, UCOL_TERTIARY, &error, &status);
    if(U_FAILURE(status)){
        log_err_status(status, "ERROR: in creation of rule based collator: %s\n", myErrorName(status));
        return;
    }

    tailoredSet = ucol_getTailoredSet(videcoll, &status);
    importTailoredSet = ucol_getTailoredSet(importvidecoll, &status);

    if(!uset_equals(tailoredSet, importTailoredSet)){
        log_err("Tailored sets not equal");
    }

    uset_close(importTailoredSet);

    itemCount = uset_getItemCount(tailoredSet);

    for( i = 0; i < itemCount; i++){
        strLength = uset_getItem(tailoredSet, i, &start, &end, str, 500, &status);
        if(strLength < 2){
            for (; start <= end; start++){
                k = 0;
                U16_APPEND_UNSAFE(str, k, start);
                ucol_getSortKey(videcoll, str, 1, sk1, 500);
                ucol_getSortKey(importvidecoll, str, 1, sk2, 500);
                if(compare_uint8_t_arrays(sk1, sk2) != 0){
                    log_err("Sort key for %s not equal\n", str);
                    break;
                }
            }
        }else{
            ucol_getSortKey(videcoll, str, strLength, sk1, 500);
            ucol_getSortKey(importvidecoll, str, strLength, sk2, 500);
            if(compare_uint8_t_arrays(sk1, sk2) != 0){
                log_err("Sort key for %s not equal\n", str);
                break;
            }

        }
    }

    uset_close(tailoredSet);

    uprv_free(viderules);

    ucol_close(videcoll);
    ucol_close(importvidecoll);
    ucol_close(vicoll);
    ucol_close(decoll);
}

/* 'IV INTERNATIONAL SCIENTIFIC - PRACTICAL CONFERENCE "GEOPOLITICS, GEOECONOMICS AND INTERNATIONAL RELATIONS PROBLEMS" 22-23 June 2010, St. Petersburg, Russia' */
static const UChar longUpperStr1[]= { /* 155 chars */
    0x49, 0x56, 0x20, 0x49, 0x4E, 0x54, 0x45, 0x52, 0x4E, 0x41, 0x54, 0x49, 0x4F, 0x4E, 0x41, 0x4C,
    0x20, 0x53, 0x43, 0x49, 0x45, 0x4E, 0x54, 0x49, 0x46, 0x49, 0x43, 0x20, 0x2D, 0x20, 0x50, 0x52,
    0x41, 0x43, 0x54, 0x49, 0x43, 0x41, 0x4C, 0x20, 0x43, 0x4F, 0x4E, 0x46, 0x45, 0x52, 0x45, 0x4E,
    0x43, 0x45, 0x20, 0x22, 0x47, 0x45, 0x4F, 0x50, 0x4F, 0x4C, 0x49, 0x54, 0x49, 0x43, 0x53, 0x2C,
    0x20, 0x47, 0x45, 0x4F, 0x45, 0x43, 0x4F, 0x4E, 0x4F, 0x4D, 0x49, 0x43, 0x53, 0x20, 0x41, 0x4E,
    0x44, 0x20, 0x49, 0x4E, 0x54, 0x45, 0x52, 0x4E, 0x41, 0x54, 0x49, 0x4F, 0x4E, 0x41, 0x4C, 0x20,
    0x52, 0x45, 0x4C, 0x41, 0x54, 0x49, 0x4F, 0x4E, 0x53, 0x20, 0x50, 0x52, 0x4F, 0x42, 0x4C, 0x45,
    0x4D, 0x53, 0x22, 0x20, 0x32, 0x32, 0x2D, 0x32, 0x33, 0x20, 0x4A, 0x75, 0x6E, 0x65, 0x20, 0x32,
    0x30, 0x31, 0x30, 0x2C, 0x20, 0x53, 0x74, 0x2E, 0x20, 0x50, 0x65, 0x74, 0x65, 0x72, 0x73, 0x62,
    0x75, 0x72, 0x67, 0x2C, 0x20, 0x52, 0x75, 0x73, 0x73, 0x69, 0x61
};

/* 'BACEDIFOGUHAJEKILOMUNAPE ' with diacritics on vowels, repeated 5 times */
static const UChar longUpperStr2[]= { /* 125 chars, > 128 collation elements */
    0x42,0xC1,0x43,0xC9,0x44,0xCD,0x46,0xD3,0x47,0xDA,0x48,0xC0,0x4A,0xC8,0x4B,0xCC,0x4C,0xD2,0x4D,0xD9,0x4E,0xC2,0x50,0xCA,0x20,
    0x42,0xC1,0x43,0xC9,0x44,0xCD,0x46,0xD3,0x47,0xDA,0x48,0xC0,0x4A,0xC8,0x4B,0xCC,0x4C,0xD2,0x4D,0xD9,0x4E,0xC2,0x50,0xCA,0x20,
    0x42,0xC1,0x43,0xC9,0x44,0xCD,0x46,0xD3,0x47,0xDA,0x48,0xC0,0x4A,0xC8,0x4B,0xCC,0x4C,0xD2,0x4D,0xD9,0x4E,0xC2,0x50,0xCA,0x20,
    0x42,0xC1,0x43,0xC9,0x44,0xCD,0x46,0xD3,0x47,0xDA,0x48,0xC0,0x4A,0xC8,0x4B,0xCC,0x4C,0xD2,0x4D,0xD9,0x4E,0xC2,0x50,0xCA,0x20,
    0x42,0xC1,0x43,0xC9,0x44,0xCD,0x46,0xD3,0x47,0xDA,0x48,0xC0,0x4A,0xC8,0x4B,0xCC,0x4C,0xD2,0x4D,0xD9,0x4E,0xC2,0x50,0xCA,0x20
};

/* 'ABCDEFGHIJKLMNOPQRSTUVWXYZ ' repeated 12 times */
static const UChar longUpperStr3[]= { /* 324 chars */
    0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x20,
    0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x20,
    0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x20,
    0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x20,
    0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x20,
    0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x20,
    0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x20,
    0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x20,
    0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x20,
    0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x20,
    0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x20,
    0x41,0x42,0x43,0x44,0x45,0x46,0x47,0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F,0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x20
};

typedef struct {
    const UChar * longUpperStrPtr;
    int32_t       longUpperStrLen;
} LongUpperStrItem;

/* String pointers must be in reverse collation order of the corresponding strings */
static const LongUpperStrItem longUpperStrItems[] = {
    { longUpperStr1, UPRV_LENGTHOF(longUpperStr1) },
    { longUpperStr2, UPRV_LENGTHOF(longUpperStr2) },
    { longUpperStr3, UPRV_LENGTHOF(longUpperStr3) },
    { NULL,          0                           }
};

enum { kCollKeyLenMax = 850 }; /* may change with collation changes */

/* Text fix for #8445; without fix, could have crash due to stack or heap corruption */
static void TestCaseLevelBufferOverflow(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UCollator * ucol = ucol_open("root", &status);
    if ( U_SUCCESS(status) ) {
        ucol_setAttribute(ucol, UCOL_CASE_LEVEL, UCOL_ON, &status);
        if ( U_SUCCESS(status) ) {
            const LongUpperStrItem * itemPtr;
            uint8_t sortKeyA[kCollKeyLenMax], sortKeyB[kCollKeyLenMax];
            for ( itemPtr = longUpperStrItems; itemPtr->longUpperStrPtr != NULL; itemPtr++ ) {
                int32_t sortKeyLen;
                if (itemPtr > longUpperStrItems) {
                    uprv_strcpy((char *)sortKeyB, (char *)sortKeyA);
                }
                sortKeyLen = ucol_getSortKey(ucol, itemPtr->longUpperStrPtr, itemPtr->longUpperStrLen, sortKeyA, kCollKeyLenMax);
                if (sortKeyLen <= 0 || sortKeyLen > kCollKeyLenMax) {
                    log_err("ERROR sort key length from ucol_getSortKey is %d\n", sortKeyLen);
                    break;
                }
                if ( itemPtr > longUpperStrItems ) {
                    int compareResult = uprv_strcmp((char *)sortKeyA, (char *)sortKeyB);
                    if (compareResult >= 0) {
                        log_err("ERROR in sort key comparison result, expected -1, got %d\n", compareResult);
                    }
                }
            }
        } else {
            log_err_status(status, "ERROR in ucol_setAttribute UCOL_CASE_LEVEL on: %s\n", myErrorName(status));
        }
        ucol_close(ucol);
    } else {
        log_err_status(status, "ERROR in ucol_open for root: %s\n", myErrorName(status));
    }
}

/* Test for #10595 */
static const UChar testJapaneseName[] = {0x4F50, 0x3005, 0x6728, 0x002C, 0x6B66, 0}; /* Sa sa Ki, Takeshi */
#define KEY_PART_SIZE 16

static void TestNextSortKeyPartJaIdentical(void)
{
    UErrorCode status = U_ZERO_ERROR;
    UCollator *coll;
    uint8_t keyPart[KEY_PART_SIZE];
    UCharIterator iter;
    uint32_t state[2] = {0, 0};
    int32_t keyPartLen;
    
    coll = ucol_open("ja", &status);
    ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_IDENTICAL, &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: in creation of Japanese collator with identical strength: %s\n", myErrorName(status));
        return;
    }

    uiter_setString(&iter, testJapaneseName, 5);
    keyPartLen = KEY_PART_SIZE;
    while (keyPartLen == KEY_PART_SIZE) {
        keyPartLen = ucol_nextSortKeyPart(coll, &iter, state, keyPart, KEY_PART_SIZE, &status);
        if (U_FAILURE(status)) {
            log_err_status(status, "ERROR: in iterating next sort key part: %s\n", myErrorName(status));
            break;
        }
    }

    ucol_close(coll);
}

#define TEST(x) addTest(root, &x, "tscoll/cmsccoll/" # x)

void addMiscCollTest(TestNode** root)
{
    TEST(TestRuleOptions);
    TEST(TestBeforePrefixFailure);
    TEST(TestContractionClosure);
    TEST(TestPrefixCompose);
    TEST(TestStrCollIdenticalPrefix);
    TEST(TestPrefix);
    TEST(TestNewJapanese);
    /*TEST(TestLimitations);*/
    TEST(TestNonChars);
    TEST(TestExtremeCompression);
    TEST(TestSurrogates);
    TEST(TestVariableTopSetting);
    TEST(TestMaxVariable);
    TEST(TestBocsuCoverage);
    TEST(TestCyrillicTailoring);
    TEST(TestCase);
    TEST(IncompleteCntTest);
    TEST(BlackBirdTest);
    TEST(FunkyATest);
    TEST(BillFairmanTest);
    TEST(TestChMove);
    TEST(TestImplicitTailoring);
    TEST(TestFCDProblem);
    TEST(TestEmptyRule);
    /*TEST(TestJ784);*/ /* 'zh' locale has changed - now it is getting tested by TestBeforePinyin */
    TEST(TestJ815);
    TEST(TestUpperCaseFirst);
    TEST(TestBefore);
    TEST(TestHangulTailoring);
    TEST(TestUCARules);
    TEST(TestIncrementalNormalize);
    TEST(TestComposeDecompose);
    TEST(TestCompressOverlap);
    TEST(TestContraction);
    TEST(TestExpansion);
    /*TEST(PrintMarkDavis);*/ /* this test doesn't test - just prints sortkeys */
    /*TEST(TestGetCaseBit);*/ /*this one requires internal things to be exported */
    TEST(TestOptimize);
    TEST(TestSuppressContractions);
    TEST(Alexis2);
    TEST(TestHebrewUCA);
    TEST(TestPartialSortKeyTermination);
    TEST(TestSettings);
    TEST(TestEquals);
    TEST(TestJ2726);
    TEST(NullRule);
    TEST(TestNumericCollation);
    TEST(TestTibetanConformance);
    TEST(TestPinyinProblem);
    TEST(TestSeparateTrees);
    TEST(TestBeforePinyin);
    TEST(TestBeforeTightening);
    /*TEST(TestMoreBefore);*/
    TEST(TestTailorNULL);
    TEST(TestUpperFirstQuaternary);
    TEST(TestJ4960);
    TEST(TestJ5223);
    TEST(TestJ5232);
    TEST(TestJ5367);
    TEST(TestHiragana);
    TEST(TestSortKeyConsistency);
    TEST(TestVI5913);  /* VI, RO tailored rules */
    TEST(TestCroatianSortKey);
    TEST(TestTailor6179);
    TEST(TestUCAPrecontext);
    TEST(TestOutOfBuffer5468);
    TEST(TestSameStrengthList);

    TEST(TestSameStrengthListQuoted);
    TEST(TestSameStrengthListSupplemental);
    TEST(TestSameStrengthListQwerty);
    TEST(TestSameStrengthListQuotedQwerty);
    TEST(TestSameStrengthListRanges);
    TEST(TestSameStrengthListSupplementalRanges);
    TEST(TestSpecialCharacters);
    TEST(TestPrivateUseCharacters);
    TEST(TestPrivateUseCharactersInList);
    TEST(TestPrivateUseCharactersInRange);
    TEST(TestInvalidListsAndRanges);
    TEST(TestImportRulesDeWithPhonebook);
    /* TEST(TestImportRulesFiWithEor); EOR rules removed from CLDR 21 */
    /* TEST(TestImportRulesCJKWithUnihan); */
    TEST(TestImport);
    TEST(TestImportWithType);

    TEST(TestBeforeRuleWithScriptReordering);
    TEST(TestNonLeadBytesDuringCollationReordering);
    TEST(TestReorderingAPI);
    TEST(TestReorderingAPIWithRuleCreatedCollator);
    TEST(TestEquivalentReorderingScripts);
    TEST(TestGreekFirstReorder);
    TEST(TestGreekLastReorder);
    TEST(TestNonScriptReorder);
    TEST(TestHaniReorder);
    TEST(TestHaniReorderWithOtherRules);
    TEST(TestMultipleReorder);
    TEST(TestReorderingAcrossCloning);
    TEST(TestReorderWithNumericCollation);
    
    TEST(TestCaseLevelBufferOverflow);
    TEST(TestNextSortKeyPartJaIdentical);
}

#endif /* #if !UCONFIG_NO_COLLATION */
