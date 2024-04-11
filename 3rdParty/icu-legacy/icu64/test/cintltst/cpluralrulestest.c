// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * Copyright (c) 2011-2014, International Business Machines Corporation
 * and others. All Rights Reserved.
 ********************************************************************/
/* C API TEST FOR PLURAL RULES */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <stdbool.h>

#include "unicode/upluralrules.h"
#include "unicode/ustring.h"
#include "unicode/uenum.h"
#include "unicode/unumberformatter.h"
#include "unicode/unumberrangeformatter.h"
#include "cintltst.h"
#include "cmemory.h"
#include "cstring.h"

static void TestPluralRules(void);
static void TestOrdinalRules(void);
static void TestGetKeywords(void);
static void TestFormatted(void);
static void TestSelectRange(void);

void addPluralRulesTest(TestNode** root);

#define TESTCASE(x) addTest(root, &x, "tsformat/cpluralrulestest/" #x)

void addPluralRulesTest(TestNode** root)
{
    TESTCASE(TestPluralRules);
    TESTCASE(TestOrdinalRules);
    TESTCASE(TestGetKeywords);
    TESTCASE(TestFormatted);
    TESTCASE(TestSelectRange);
}

typedef struct {
    const char * locale;
    double       number;
    const char * keywordExpected;
    const char * keywordExpectedForDecimals;
} PluralRulesTestItem;

/* Just a small set of tests for now, other functionality is tested in the C++ tests */
static const PluralRulesTestItem testItems[] = {
    { "en",   0, "other", "other" },
    { "en", 0.5, "other", "other" },
    { "en",   1, "one",   "other" },
    { "en", 1.5, "other", "other" },
    { "en",   2, "other", "other" },
    { "fr",   0, "one",   "one" },
    { "fr", 0.5, "one",   "one" },
    { "fr",   1, "one",   "one" },
    { "fr", 1.5, "one",   "one" },
    { "fr",   2, "other", "other" },
    { "ru",   0, "many",  "other" },
    { "ru", 0.5, "other", "other" },
    { "ru",   1, "one",   "other" },
    { "ru", 1.5, "other", "other" },
    { "ru",   2, "few",   "other" },
    { "ru",   5, "many",  "other" },
    { "ru",  10, "many",  "other" },
    { "ru",  11, "many",  "other" },
    { NULL,   0, NULL,    NULL }
};

static const UChar twoDecimalPat[] = { 0x23,0x30,0x2E,0x30,0x30,0 }; /* "#0.00" */

enum {
    kKeywordBufLen = 32
};

static void TestPluralRules(void)
{
    const PluralRulesTestItem * testItemPtr;
    log_verbose("\nTesting uplrules_open() and uplrules_select() with various parameters\n");
    for ( testItemPtr = testItems; testItemPtr->locale != NULL; ++testItemPtr ) {
        UErrorCode status = U_ZERO_ERROR;
        UPluralRules* uplrules = uplrules_open(testItemPtr->locale, &status);
        if ( U_SUCCESS(status) ) {
            UNumberFormat* unumfmt;
            UChar keyword[kKeywordBufLen];
            UChar keywordExpected[kKeywordBufLen];
            int32_t keywdLen = uplrules_select(uplrules, testItemPtr->number, keyword, kKeywordBufLen, &status);
            if (keywdLen >= kKeywordBufLen) {
                keyword[kKeywordBufLen-1] = 0;
            }
            if ( U_SUCCESS(status) ) {
                u_unescape(testItemPtr->keywordExpected, keywordExpected, kKeywordBufLen);
                if ( u_strcmp(keyword, keywordExpected) != 0 ) {
                    char bcharBuf[kKeywordBufLen];
                    log_data_err("ERROR: uplrules_select for locale %s, number %.1f: expect %s, get %s\n",
                             testItemPtr->locale, testItemPtr->number, testItemPtr->keywordExpected, u_austrcpy(bcharBuf,keyword) );
                }
            } else {
                log_err("FAIL: uplrules_select for locale %s, number %.1f: %s\n",
                        testItemPtr->locale, testItemPtr->number, myErrorName(status) );
            }

            status = U_ZERO_ERROR;
            unumfmt = unum_open(UNUM_PATTERN_DECIMAL, twoDecimalPat, -1, testItemPtr->locale, NULL, &status);
            if ( U_SUCCESS(status) ) {
                keywdLen = uplrules_selectWithFormat(uplrules, testItemPtr->number, unumfmt, keyword, kKeywordBufLen, &status);
                if (keywdLen >= kKeywordBufLen) {
                    keyword[kKeywordBufLen-1] = 0;
                }
                if ( U_SUCCESS(status) ) {
                    u_unescape(testItemPtr->keywordExpectedForDecimals, keywordExpected, kKeywordBufLen);
                    if ( u_strcmp(keyword, keywordExpected) != 0 ) {
                        char bcharBuf[kKeywordBufLen];
                        log_data_err("ERROR: uplrules_selectWithFormat for locale %s, number %.1f: expect %s, get %s\n",
                                 testItemPtr->locale, testItemPtr->number, testItemPtr->keywordExpectedForDecimals, u_austrcpy(bcharBuf,keyword) );
                    }
                } else {
                    log_err("FAIL: uplrules_selectWithFormat for locale %s, number %.1f: %s\n",
                            testItemPtr->locale, testItemPtr->number, myErrorName(status) );
                }
                unum_close(unumfmt);
            } else {
                log_err("FAIL: unum_open for locale %s: %s\n", testItemPtr->locale, myErrorName(status) );
            }

            uplrules_close(uplrules);
        } else {
            log_err("FAIL: uplrules_open for locale %s: %s\n", testItemPtr->locale, myErrorName(status) );
        }
    }
}

static void TestOrdinalRules(void) {
    U_STRING_DECL(two, "two", 3);
    UChar keyword[8];
    int32_t length;
    UErrorCode errorCode = U_ZERO_ERROR;
    UPluralRules* upr = uplrules_openForType("en", UPLURAL_TYPE_ORDINAL, &errorCode);
    if (U_FAILURE(errorCode)) {
        log_err("uplrules_openForType(en, ordinal) failed - %s\n", u_errorName(errorCode));
        return;
    }
    U_STRING_INIT(two, "two", 3);
    length = uplrules_select(upr, 2., keyword, 8, &errorCode);
    if (U_FAILURE(errorCode) || u_strCompare(keyword, length, two, 3, false) != 0) {
        log_data_err("uplrules_select(en-ordinal, 2) failed - %s\n", u_errorName(errorCode));
    }
    uplrules_close(upr);
}

/* items for TestGetKeywords */

/* all possible plural keywords, in alphabetical order */
static const char* knownKeywords[] = {
    "few",
    "many",
    "one",
    "other",
    "two",
    "zero"
};
enum {
    kNumKeywords = UPRV_LENGTHOF(knownKeywords)
};

/* Return the index of keyword in knownKeywords[], or -1 if not found */
static int32_t getKeywordIndex(const char* keyword) {
    int32_t i, compare;
    for (i = 0; i < kNumKeywords && (compare = uprv_strcmp(keyword,knownKeywords[i])) >= 0; i++) {
        if (compare == 0) {
        	return i;
        }
    }
    return -1;
}

typedef struct {
    const char* locale;
    const char* keywords[kNumKeywords + 1];
} KeywordsForLang;

static const KeywordsForLang getKeywordsItems[] = {
    { "zh", { "other" } },
    { "en", { "one", "other" } },
    { "fr", { "one", "many", "other" } },
    { "lv", { "zero", "one", "other" } },
    { "hr", { "one", "few", "other" } },
    { "sl", { "one", "two", "few", "other" } },
    { "he", { "one", "two", "other" } },
    { "cs", { "one", "few", "many", "other" } },
    { "ar", { "zero", "one", "two", "few", "many" , "other" } },
    { NULL, { NULL } }
};

static void TestGetKeywords(void) {
    /*
     * We don't know the order in which the enumeration will return keywords,
     * so we have an array with known keywords in a fixed order and then
     * parallel arrays of flags for expected and actual results that indicate
     * which keywords are expected to be or actually are found.
     */
    const KeywordsForLang* itemPtr = getKeywordsItems;
    for (; itemPtr->locale != NULL; itemPtr++) {
        UPluralRules* uplrules;
        UEnumeration* uenum;
        UBool expectKeywords[kNumKeywords];
        UBool getKeywords[kNumKeywords];
        int32_t i, iKnown;
        UErrorCode status = U_ZERO_ERROR;
        
        /* initialize arrays for expected and get results */
        for (i = 0; i < kNumKeywords; i++) {
            expectKeywords[i] = false;
            getKeywords[i] = false;
        }
        for (i = 0; i < kNumKeywords && itemPtr->keywords[i] != NULL; i++) {
            iKnown = getKeywordIndex(itemPtr->keywords[i]);
            if (iKnown >= 0) {
                expectKeywords[iKnown] = true;
            }
        }
        
        uplrules = uplrules_openForType(itemPtr->locale, UPLURAL_TYPE_CARDINAL, &status);
        if (U_FAILURE(status)) {
            log_err("FAIL: uplrules_openForType for locale %s, UPLURAL_TYPE_CARDINAL: %s\n", itemPtr->locale, myErrorName(status) );
            continue;
        }
        uenum = uplrules_getKeywords(uplrules, &status);
        if (U_FAILURE(status)) {
            log_err("FAIL: uplrules_getKeywords for locale %s: %s\n", itemPtr->locale, myErrorName(status) );
        } else {
            const char* keyword;
            int32_t keywordLen, keywordCount = 0;
            while ((keyword = uenum_next(uenum, &keywordLen, &status)) != NULL && U_SUCCESS(status)) {
                iKnown = getKeywordIndex(keyword);
                if (iKnown < 0) {
                    log_err("FAIL: uplrules_getKeywords for locale %s, unknown keyword %s\n", itemPtr->locale, keyword );
                } else {
                    getKeywords[iKnown] = true;
                }
                keywordCount++;
            }
            if (keywordCount > kNumKeywords) {
                log_err("FAIL: uplrules_getKeywords for locale %s, got too many keywords %d\n", itemPtr->locale, keywordCount );
            }
            if (uprv_memcmp(expectKeywords, getKeywords, kNumKeywords) != 0) {
                log_err("FAIL: uplrules_getKeywords for locale %s, got wrong keyword set; with reference to knownKeywords:\n"
                        "        expected { %d %d %d %d %d %d },\n"
                        "        got      { %d %d %d %d %d %d }\n", itemPtr->locale, 
                        expectKeywords[0], expectKeywords[1], expectKeywords[2], expectKeywords[3], expectKeywords[4], expectKeywords[5],
                        getKeywords[0], getKeywords[1], getKeywords[2], getKeywords[3], getKeywords[4], getKeywords[5] );
            }
            uenum_close(uenum);
        }
        
        uplrules_close(uplrules);
    }
}

static void TestFormatted(void) {
    UErrorCode ec = U_ZERO_ERROR;
    UNumberFormatter* unumf = NULL;
    UFormattedNumber* uresult = NULL;
    UPluralRules* uplrules = NULL;

    uplrules = uplrules_open("hr", &ec);
    if (!assertSuccess("open plural rules", &ec)) {
        goto cleanup;
    }

    unumf = unumf_openForSkeletonAndLocale(u".00", -1, "hr", &ec);
    if (!assertSuccess("open unumf", &ec)) {
        goto cleanup;
    }

    uresult = unumf_openResult(&ec);
    if (!assertSuccess("open result", &ec)) {
        goto cleanup;
    }

    unumf_formatDouble(unumf, 100.2, uresult, &ec);
    if (!assertSuccess("format", &ec)) {
        goto cleanup;
    }

    UChar buffer[40];
    uplrules_selectFormatted(uplrules, uresult, buffer, 40, &ec);
    if (!assertSuccess("select", &ec)) {
        goto cleanup;
    }

    assertUEquals("0.20 is plural category 'other' in hr", u"other", buffer);

cleanup:
    uplrules_close(uplrules);
    unumf_close(unumf);
    unumf_closeResult(uresult);
}

static void TestSelectRange(void) {
    UErrorCode ec = U_ZERO_ERROR;
    UNumberRangeFormatter* unumrf = NULL;
    UFormattedNumberRange* uresult = NULL;
    UPluralRules* uplrules = NULL;

    int32_t d1 = 102;
    int32_t d2 = 201;

    // Locale sl has interesting data: one + two => few
    uplrules = uplrules_open("sl", &ec);
    if (!assertSuccess("open plural rules", &ec)) {
        goto cleanup;
    }

    unumrf = unumrf_openForSkeletonWithCollapseAndIdentityFallback(
        u"",
        0,
        UNUM_RANGE_COLLAPSE_AUTO,
        UNUM_IDENTITY_FALLBACK_APPROXIMATELY,
        "sl",
        NULL,
        &ec);
    if (!assertSuccess("open unumrf", &ec)) {
        goto cleanup;
    }

    uresult = unumrf_openResult(&ec);
    if (!assertSuccess("open result", &ec)) {
        goto cleanup;
    }

    unumrf_formatDoubleRange(unumrf, d1, d2, uresult, &ec);
    if (!assertSuccess("format", &ec)) {
        goto cleanup;
    }

    UChar buffer[40];
    int32_t len = uplrules_selectForRange(uplrules, uresult, buffer, 40, &ec);
    if (!assertSuccess("select", &ec)) {
        goto cleanup;
    }

    assertUEquals("102-201 is plural category 'few' in sl", u"few", buffer);
    assertIntEquals("Length should be as expected", u_strlen(buffer), len);

cleanup:
    uplrules_close(uplrules);
    unumrf_close(unumrf);
    unumrf_closeResult(uresult);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
