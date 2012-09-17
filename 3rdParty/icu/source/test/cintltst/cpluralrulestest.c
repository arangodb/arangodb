/********************************************************************
 * Copyright (c) 2011, International Business Machines Corporation
 * and others. All Rights Reserved.
 ********************************************************************/
/* C API TEST FOR PLURAL RULES */

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/upluralrules.h"
#include "unicode/ustring.h"
#include "cintltst.h"
#include "cmemory.h"

static void TestPluralRules(void);

void addPluralRulesTest(TestNode** root);

#define TESTCASE(x) addTest(root, &x, "tsformat/cpluralrulestest/" #x)

void addPluralRulesTest(TestNode** root)
{
    TESTCASE(TestPluralRules);
}

typedef struct {
    const char * locale;
    double       number;
    const char * keywordExpected;
} PluralRulesTestItem;

/* Just a small set of tests for now, other functionality is tested in the C++ tests */
static const PluralRulesTestItem testItems[] = {
    { "en",   0, "other" },
    { "en", 0.5, "other" },
    { "en",   1, "one" },
    { "en", 1.5, "other" },
    { "en",   2, "other" },
    { "fr",   0, "one" },
    { "fr", 0.5, "one" },
    { "fr",   1, "one" },
    { "fr", 1.5, "one" },
    { "fr",   2, "other" },
    { "ru",   0, "many" },
    { "ru", 0.5, "other" },
    { "ru",   1, "one" },
    { "ru", 1.5, "other" },
    { "ru",   2, "few" },
    { "ru",   5, "many" },
    { "ru",  10, "many" },
    { "ru",  11, "many" },
    { NULL,   0, NULL }
};

enum {
    kKeywordBufLen = 32
};

static void TestPluralRules()
{
    const PluralRulesTestItem * testItemPtr;
    log_verbose("\nTesting uplrules_open() and uplrules_select() with various parameters\n");
    for ( testItemPtr = testItems; testItemPtr->locale != NULL; ++testItemPtr ) {
        UErrorCode status = U_ZERO_ERROR;
        UPluralRules* uplrules = uplrules_open(testItemPtr->locale, &status);
        if ( U_SUCCESS(status) ) {
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
            uplrules_close(uplrules);
        } else {
            log_err("FAIL: uplrules_open for locale %s: %s\n", testItemPtr->locale, myErrorName(status) );
        }
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
