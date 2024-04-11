// © 2023 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/ctest.h"
#include "unicode/uloc.h"
#include "unicode/ulocale.h"
#include "cintltst.h"
#include "cmemory.h"
#include "cstring.h"

#define WHERE __FILE__ ":" XLINE(__LINE__) " "
#define XLINE(s) LINE(s)
#define LINE(s) #s

#define TESTCASE(name) addTest(root, &name, "tsutil/ulocaletst/" #name)

enum {
    LANGUAGE = 0,
    SCRIPT,
    REGION,
    VAR,
    NAME,
    BASENAME,
};
static const char* const rawData[][6] = {
    { "en", "", "US", "", "en_US", "en_US"},
    { "fr", "", "FR", "", "fr_FR", "fr_FR"},
    { "ca", "", "ES", "", "ca_ES", "ca_ES"},
    { "el", "", "GR", "", "el_GR", "el_GR"},
    { "no", "", "NO", "NY", "no_NO_NY", "no_NO_NY"},
    { "it", "", "", "", "it", "it"},
    { "xx", "", "YY", "", "xx_YY", "xx_YY"},
    { "zh", "Hans", "CN", "", "zh_Hans_CN", "zh_Hans_CN"},
    { "zh", "Hans", "CN", "", "zh_Hans_CN", "zh_Hans_CN"},
    { "x-klingon", "Latn", "ZX", "", "x-klingon_Latn_ZX.utf32be@special", "x-klingon_Latn_ZX.utf32be@special"},
};

static void TestBasicGetters(void) {
    for (int32_t i = 0; i < UPRV_LENGTHOF(rawData); i++)  {
        UErrorCode status = U_ZERO_ERROR;
        ULocale* l = ulocale_openForLocaleID(rawData[i][NAME], -1, &status);
        if (assertSuccess(WHERE "ulocale_openForLocaleID()", &status)) {
            assertEquals(WHERE "ulocale_getLanguage()", rawData[i][LANGUAGE], ulocale_getLanguage(l));
            assertEquals(WHERE "ulocale_getScript()", rawData[i][SCRIPT], ulocale_getScript(l));
            assertEquals(WHERE "ulocale_getRegion()", rawData[i][REGION], ulocale_getRegion(l));
            assertEquals(WHERE "ulocale_getVariant()", rawData[i][VAR], ulocale_getVariant(l));
            assertEquals(WHERE "ulocale_getLocaleID()", rawData[i][NAME], ulocale_getLocaleID(l));
            assertEquals(WHERE "ulocale_getBaseName()", rawData[i][BASENAME], ulocale_getBaseName(l));
            ulocale_close(l);
        }
    }
}

static void VerifyMatch(const char* localeID, const char* tag) {
    UErrorCode status = U_ZERO_ERROR;
    ULocale* fromID = ulocale_openForLocaleID(localeID, -1, &status);
    if (assertSuccess(WHERE "ulocale_openForLocaleID()", &status)) {
        ULocale* fromTag = ulocale_openForLanguageTag(tag, -1, &status);
        if (assertSuccess(WHERE "ulocale_openForLanguageTag()", &status)) {
            assertEquals(tag, ulocale_getLocaleID(fromID), ulocale_getLocaleID(fromTag));
            ulocale_close(fromTag);
        }
        ulocale_close(fromID);
    }
}

static void TestForLanguageTag(void) {
    VerifyMatch("en_US", "en-US");
    VerifyMatch("en_GB_OXENDICT", "en-GB-oed");
    VerifyMatch("af@calendar=coptic;t=ar-i0-handwrit;x=foo", "af-t-ar-i0-handwrit-u-ca-coptic-x-foo");
    VerifyMatch("en_GB", "en-GB");
    VerifyMatch("en_GB@1=abc-efg;a=xyz", "en-GB-1-abc-efg-a-xyz"); // ext
    VerifyMatch("sl__1994_BISKE_ROZAJ", "sl-rozaj-biske-1994"); // var

    UErrorCode status = U_ZERO_ERROR;
    ULocale* result_ill = ulocale_openForLanguageTag("!", -1, &status);
    assertIntEquals("!", U_ILLEGAL_ARGUMENT_ERROR, status);
    assertPtrEquals("!", NULL, result_ill);
    ulocale_close(result_ill);

    VerifyMatch("", NULL);

    // ICU-21433
    VerifyMatch("__1994_BISKE_ROZAJ", "und-1994-biske-rozaj");
    VerifyMatch("de__1994_BISKE_ROZAJ", "de-1994-biske-rozaj");
    VerifyMatch("@x=private", "und-x-private");
    VerifyMatch("de__1994_BISKE_ROZAJ@x=private", "de-1994-biske-rozaj-x-private");
    VerifyMatch("__1994_BISKE_ROZAJ@x=private", "und-1994-biske-rozaj-x-private");
}

static void TestGetKeywords(void) {
    UErrorCode status = U_ZERO_ERROR;
    ULocale* l = ulocale_openForLocaleID("de@calendar=buddhist;collation=phonebook", -1, &status);
    if (assertSuccess(WHERE "ulocale_openForLocaleID()", &status)) {
        UEnumeration* en = ulocale_getKeywords(l, &status);
        if (assertSuccess(WHERE "ulocale_getKeywords()", &status)) {
            assertIntEquals("uenum_count()", 2, uenum_count(en, &status));
            bool hasCalendar = false;
            bool hasCollation = false;
            const char* key = NULL;
            while ((key = uenum_next(en, NULL, &status)) != NULL) {
                if (uprv_strcmp(key, "calendar") == 0) {
                    hasCalendar = true;
                } else if (uprv_strcmp(key, "collation") == 0) {
                    hasCollation = true;
                }
            }
            assertTrue(
                WHERE "ulocale_getKeywords() should return UEnumeration that has \"calendar\"",
                hasCalendar);
            assertTrue(
                WHERE "ulocale_getKeywords() should return UEnumeration that has \"collation\"",
                hasCollation);
            uenum_close(en);
        }
        ulocale_close(l);
    }
}

static void TestGetKeywordsEmpty(void) {
    UErrorCode status = U_ZERO_ERROR;
    ULocale* l = ulocale_openForLocaleID("de", -1, &status);
    if (assertSuccess(WHERE "ulocale_openForLocaleID()", &status)) {
        UEnumeration* en = ulocale_getKeywords(l, &status);
        if (assertSuccess(WHERE "ulocale_getKeywords()", &status)) {
            assertPtrEquals(WHERE "ulocale_getKeyword()", NULL, en);
            uenum_close(en);
        }
        ulocale_close(l);
    }
}

static void TestGetKeywordsWithPrivateUse(void) {
    UErrorCode status = U_ZERO_ERROR;
    ULocale* l = ulocale_openForLanguageTag("en-US-u-ca-gregory-x-foo", -1, &status);
    if (assertSuccess(WHERE "ulocale_openForLanguageTag()", &status)) {
        UEnumeration* en = ulocale_getKeywords(l, &status);
        if (assertSuccess(WHERE "ulocale_getKeywords()", &status)) {
            assertIntEquals("uenum_count()", 2, uenum_count(en, &status));
            bool hasCalendar = false;
            bool hasX = false;
            const char* key = NULL;
            while ((key = uenum_next(en, NULL, &status)) != NULL) {
                if (uprv_strcmp(key, "calendar") == 0) {
                    hasCalendar = true;
                } else if (uprv_strcmp(key, "x") == 0) {
                    hasX = true;
                }
            }
            assertTrue(WHERE "ulocale_getKeywords() should return UEnumeration that has \"calendar\"",
                       hasCalendar);
            assertTrue(WHERE "ulocale_getKeywords() should return UEnumeration that has \"x\"",
                       hasX);
            uenum_close(en);
        }
        ulocale_close(l);
    }
}

static void TestGetUnicodeKeywords(void) {
    UErrorCode status = U_ZERO_ERROR;
    ULocale* l = ulocale_openForLocaleID("de@calendar=buddhist;collation=phonebook", -1, &status);
    if (assertSuccess(WHERE "ulocale_openForLocaleID()", &status)) {
        UEnumeration* en = ulocale_getUnicodeKeywords(l, &status);
        if (assertSuccess(WHERE "ulocale_getUnicodeKeywords()", &status)) {
            assertIntEquals("uenum_count()", 2, uenum_count(en, &status));
            bool hasCa = false;
            bool hasCo = false;
            const char* key = NULL;
            while ((key = uenum_next(en, NULL, &status)) != NULL) {
                if (uprv_strcmp(key, "ca") == 0) {
                    hasCa = true;
                } else if (uprv_strcmp(key, "co") == 0) {
                    hasCo = true;
                }
            }
            assertTrue(
                WHERE "ulocale_getUnicodeKeywords() should return UEnumeration that has \"ca\"",
                hasCa);
            assertTrue(
                WHERE "ulocale_getUnicodeKeywords() should return UEnumeration that has \"co\"",
                hasCo);
            uenum_close(en);
        }
        ulocale_close(l);
    }
}

static void TestGetUnicodeKeywordsEmpty(void) {
    UErrorCode status = U_ZERO_ERROR;
    ULocale* l = ulocale_openForLocaleID("de", -1, &status);
    if (assertSuccess(WHERE "ulocale_openForLocaleID()", &status)) {
        UEnumeration* en = ulocale_getUnicodeKeywords(l, &status);
        if (assertSuccess(WHERE "ulocale_getUnicodeKeywords()", &status)) {
            assertPtrEquals("ulocale_getUnicodeKeyword()", NULL, en);
            uenum_close(en);
        }
        ulocale_close(l);
    }
}

static void TestGetUnicodeKeywordsWithPrivateUse(void) {
    UErrorCode status = U_ZERO_ERROR;
    ULocale* l = ulocale_openForLanguageTag("en-US-u-ca-gregory-x-foo", -1, &status);
    if (assertSuccess(WHERE "ulocale_openForLanguageTag()", &status)) {
        UEnumeration* en = ulocale_getUnicodeKeywords(l, &status);
        if (assertSuccess(WHERE "ulocale_getUnicodeKeywords()", &status)) {
            int32_t count = uenum_count(en, &status);
            if (count != 1) {
                log_knownIssue("ICU-22457", "uenum_count() should be 1 but get %d\n", count);
            }
            const char* key = uenum_next(en, NULL, &status);
            if (assertSuccess(WHERE "uenum_next()", &status)) {
                assertEquals(WHERE "ulocale_getUnicodeKeywords() should return UEnumeration that has \"ca\"",
                             "ca", key);
            }
            uenum_close(en);
        }
        ulocale_close(l);
    }
}

static void TestGetKeywordValue(void) {
    UErrorCode status = U_ZERO_ERROR;
    ULocale* l = ulocale_openForLanguageTag("fa-u-nu-thai", -1, &status);
    if (assertSuccess(WHERE "ulocale_openForLanguageTag()", &status)) {
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocale_getKeywordValue(l, "numbers", -1, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (assertSuccess(WHERE "ulocale_getKeywordValue()", &status)) {
            assertEquals(WHERE "ulocale_getKeywordValue(\"numbers\")", "thai", buffer);
        }

        ulocale_getKeywordValue(l, "calendar", -1, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (assertSuccess(WHERE "ulocale_getKeywordValue()", &status)) {
            assertEquals(WHERE "ulocale_getKeywordValue(\"calendar\")", "", buffer);
        }
        ulocale_getKeywordValue(l, "collation", -1, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (assertSuccess(WHERE "ulocale_getKeywordValue()", &status)) {
            assertEquals(WHERE "ulocale_getKeywordValue(\"collation\")", "", buffer);
        }
        ulocale_close(l);
    }
}

static void TestGetUnicodeKeywordValue(void) {
    UErrorCode status = U_ZERO_ERROR;
    ULocale* l = ulocale_openForLanguageTag("fa-u-nu-thai", -1, &status);
    if (assertSuccess(WHERE "ulocale_openForLanguageTag()", &status)) {
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocale_getUnicodeKeywordValue(l, "nu", -1, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (assertSuccess(WHERE "ulocale_getUnicodeKeywordValue()", &status)) {
            assertEquals(WHERE "ulocale_getUnicodeKeywordValue(\"nu\")", "thai", buffer);
        }

        ulocale_getUnicodeKeywordValue(l, "ca", -1, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (U_FAILURE(status) || buffer[0] != '\0') {
            log_knownIssue(
                "ICU-22459",
                WHERE "ulocale_getUnicodeKeywordValue(\"fa-u-nu-thai\", \"ca\") should not return error and should return empty string.");
        }
        ulocale_getUnicodeKeywordValue(l, "co", -1, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (U_FAILURE(status) || buffer[0] != '\0') {
            log_knownIssue(
                "ICU-22459",
                WHERE "ulocale_getUnicodeKeywordValue(\"fa-u-nu-thai\", \"co\") should not return error and should return empty string.");
        }
        ulocale_close(l);
    }
}

void addULocaleTest(TestNode** root);
void addULocaleTest(TestNode** root)
{
    TESTCASE(TestBasicGetters);
    TESTCASE(TestForLanguageTag);
    TESTCASE(TestGetKeywords);
    TESTCASE(TestGetKeywordsEmpty);
    TESTCASE(TestGetKeywordsWithPrivateUse);
    TESTCASE(TestGetUnicodeKeywords);
    TESTCASE(TestGetUnicodeKeywordsEmpty);
    TESTCASE(TestGetUnicodeKeywordsWithPrivateUse);
    TESTCASE(TestGetKeywordValue);
    TESTCASE(TestGetUnicodeKeywordValue);
}

