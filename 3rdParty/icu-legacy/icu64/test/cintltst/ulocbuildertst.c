// © 2023 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "cintltst.h"
#include "cstring.h"
#include "unicode/uloc.h"
#include "unicode/ulocbuilder.h"
#include "unicode/ulocale.h"

#define WHERE __FILE__ ":" XLINE(__LINE__) " "
#define XLINE(s) LINE(s)
#define LINE(s) #s

#ifndef UPRV_LENGTHOF
#define UPRV_LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))
#endif
void addLocaleBuilderTest(TestNode** root);

static void Verify(ULocaleBuilder* bld, const char* expected, const char* msg) {
    UErrorCode status = U_ZERO_ERROR;
    UErrorCode copyStatus = U_ZERO_ERROR;
    UErrorCode errorStatus = U_ILLEGAL_ARGUMENT_ERROR;
    if (ulocbld_copyErrorTo(bld, &copyStatus)) {
        log_err(msg, u_errorName(copyStatus));
    }
    if (!ulocbld_copyErrorTo(bld, &errorStatus)) {
        log_err("Should always get the previous error and return false");
    }
    char tag[ULOC_FULLNAME_CAPACITY];
    ulocbld_buildLanguageTag(bld, tag, ULOC_FULLNAME_CAPACITY, &status);

    if (U_FAILURE(status)) {
        log_err(msg, u_errorName(status));
    }
    if (status != copyStatus) {
        log_err(msg, u_errorName(status));
    }
    if (strcmp(tag, expected) != 0) {
        log_err("should get \"%s\", but got \"%s\"\n", expected, tag);
    }
}

static void TestLocaleBuilder(void) {

    // The following test data are copy from
    // icu4j/main/core/src/test/java/com/ibm/icu/dev/test/util/LocaleBuilderTest.java
    // "L": +1 = language
    // "S": +1 = script
    // "R": +1 = region
    // "V": +1 = variant
    // "K": +1 = Unicode locale key / +2 = Unicode locale type
    // "A": +1 = Unicode locale attribute
    // "E": +1 = extension letter / +2 = extension value
    // "P": +1 = private use
    // "U": +1 = ULocale
    // "B": +1 = BCP47 language tag
    // "C": Clear all
    // "N": Clear extensions
    // "D": +1 = Unicode locale attribute to be removed
    // "X": indicates an exception must be thrown
    // "T": +1 = expected language tag / +2 = expected locale string
    const char* TESTCASES[][14] = {
        {"L", "en", "R", "us", "T", "en-US", "en_US"},
        {"L", "en", "R", "CA", "L", NULL, "T", "und-CA", "_CA"},
        {"L", "en", "R", "CA", "L", "", "T", "und-CA", "_CA"},
        {"L", "en", "R", "FR", "L", "fr", "T", "fr-FR", "fr_FR"},
        {"L", "123", "X"},
        {"R", "us", "T", "und-US", "_US"},
        {"R", "usa", "X"},
        {"R", "123", "L", "it", "R", NULL, "T", "it", "it"},
        {"R", "123", "L", "it", "R", "", "T", "it", "it"},
        {"R", "123", "L", "en", "T", "en-123", "en_123"},
        {"S", "LATN", "L", "DE", "T", "de-Latn", "de_Latn"},
        {"L", "De", "S", "latn", "R", "de", "S", "", "T", "de-DE", "de_DE"},
        {"L", "De", "S", "Arab", "R", "de", "S", NULL, "T", "de-DE", "de_DE"},
        {"S", "latin", "X"},
        {"V", "1234", "L", "en", "T", "en-1234", "en__1234"},
        {"V", "1234", "L", "en", "V", "5678", "T", "en-5678", "en__5678"},
        {"V", "1234", "L", "en", "V", NULL, "T", "en", "en"},
        {"V", "1234", "L", "en", "V", "", "T", "en", "en"},
        {"V", "123", "X"},
        {"U", "en_US", "T", "en-US", "en_US"},
        {"U", "en_US_WIN", "X"},
        {"B", "fr-FR-1606nict-u-ca-gregory-x-test", "T",
          "fr-FR-1606nict-u-ca-gregory-x-test",
          "fr_FR_1606NICT@calendar=gregorian;x=test"},
        {"B", "ab-cde-fghij", "T", "cde-fghij", "cde__FGHIJ"},
        {"B", "und-CA", "T", "und-CA", "_CA"},
        // Blocked by ICU-20327
        // {"B", "en-US-x-test-lvariant-var", "T", "en-US-x-test-lvariant-var",
        // "en_US_VAR@x=test"},
        {"B", "en-US-VAR", "X"},
        {"U", "ja_JP@calendar=japanese;currency=JPY", "L", "ko", "T",
          "ko-JP-u-ca-japanese-cu-jpy", "ko_JP@calendar=japanese;currency=JPY"},
        {"U", "ja_JP@calendar=japanese;currency=JPY", "K", "ca", NULL, "T",
          "ja-JP-u-cu-jpy", "ja_JP@currency=JPY"},
        {"U", "ja_JP@calendar=japanese;currency=JPY", "E", "u",
          "attr1-ca-gregory", "T", "ja-JP-u-attr1-ca-gregory",
          "ja_JP@attribute=attr1;calendar=gregorian"},
        {"U", "en@colnumeric=yes", "K", "kn", "true", "T", "en-u-kn",
          "en@colnumeric=yes"},
        {"L", "th", "R", "th", "K", "nu", "thai", "T", "th-TH-u-nu-thai",
          "th_TH@numbers=thai"},
        {"U", "zh_Hans", "R", "sg", "K", "ca", "badcalendar", "X"},
        {"U", "zh_Hans", "R", "sg", "K", "cal", "gregory", "X"},
        {"E", "z", "ExtZ", "L", "en", "T", "en-z-extz", "en@z=extz"},
        {"E", "z", "ExtZ", "L", "en", "E", "z", "", "T", "en", "en"},
        {"E", "z", "ExtZ", "L", "en", "E", "z", NULL, "T", "en", "en"},
        {"E", "a", "x", "X"},
        {"E", "a", "abc_def", "T", "und-a-abc-def", "@a=abc-def"},
        // Design limitation - typeless u extension keyword 0a below is interpreted as a boolean value true/yes.
        // With the legacy keyword syntax, "yes" is used for such boolean value instead of "true".
        // However, once the legacy keyword is translated back to BCP 47 u extension, key "0a" is unknown,
        // so "yes" is preserved - not mapped to "true". We could change the code to automatically transform
        // key = alphanum alpha
        {"L", "en", "E", "u", "bbb-aaa-0a", "T", "en-u-aaa-bbb-0a",
         "en@0a=yes;attribute=aaa-bbb"},
        {"L", "fr", "R", "FR", "P", "Yoshito-ICU", "T", "fr-FR-x-yoshito-icu",
          "fr_FR@x=yoshito-icu"},
        {"L", "ja", "R", "jp", "K", "ca", "japanese", "T", "ja-JP-u-ca-japanese",
          "ja_JP@calendar=japanese"},
        {"K", "co", "PHONEBK", "K", "ca", "gregory", "L", "De", "T",
          "de-u-ca-gregory-co-phonebk", "de@calendar=gregorian;collation=phonebook"},
        {"E", "o", "OPQR", "E", "a", "aBcD", "T", "und-a-abcd-o-opqr", "@a=abcd;o=opqr"},
        {"E", "u", "nu-thai-ca-gregory", "L", "TH", "T", "th-u-ca-gregory-nu-thai",
          "th@calendar=gregorian;numbers=thai"},
        {"L", "en", "K", "tz", "usnyc", "R", "US", "T", "en-US-u-tz-usnyc",
          "en_US@timezone=America/New_York"},
        {"L", "de", "K", "co", "phonebk", "K", "ks", "level1", "K", "kk",
          "true", "T", "de-u-co-phonebk-kk-ks-level1",
          "de@collation=phonebook;colnormalization=yes;colstrength=primary"},
        {"L", "en", "R", "US", "K", "ca", "gregory", "T", "en-US-u-ca-gregory",
          "en_US@calendar=gregorian"},
        {"L", "en", "R", "US", "K", "cal", "gregory", "X"},
        {"L", "en", "R", "US", "K", "ca", "gregorian", "X"},
        {"L", "en", "R", "US", "K", "kn", "true", "T", "en-US-u-kn",
          "en_US@colnumeric=yes"},
        {"B", "de-DE-u-co-phonebk", "C", "L", "pt", "T", "pt", "pt"},
        {"B", "ja-jp-u-ca-japanese", "N", "T", "ja-JP", "ja_JP"},
        {"B", "es-u-def-abc-co-trad", "A", "hij", "D", "def", "T",
          "es-u-abc-hij-co-trad", "es@attribute=abc-hij;collation=traditional"},
        {"B", "es-u-def-abc-co-trad", "A", "hij", "D", "def", "D", "def", "T",
          "es-u-abc-hij-co-trad", "es@attribute=abc-hij;collation=traditional"},
        {"L", "en", "A", "aa", "X"},
        {"B", "fr-u-attr1-cu-eur", "D", "attribute1", "X"},
    };
    UErrorCode status = U_ZERO_ERROR;
    ULocaleBuilder* bld = ulocbld_open();
    char tag[ULOC_FULLNAME_CAPACITY];
    char locale[ULOC_FULLNAME_CAPACITY];
    for (int tidx = 0; tidx < UPRV_LENGTHOF(TESTCASES); tidx++) {
        char actions[1000];
        actions[0] = '\0';
        for (int p = 0; p < UPRV_LENGTHOF(TESTCASES[tidx]); p++) {
             if (TESTCASES[tidx][p] == NULL) {
                 strcpy(actions, " (nullptr)");
                 break;
             }
             if (p > 0) strcpy(actions, " ");
             strcpy(actions, TESTCASES[tidx][p]);
        }
        int i = 0;
        const char* method;
        status = U_ZERO_ERROR;
        ulocbld_clear(bld);
        while (true) {
            status = U_ZERO_ERROR;
            UErrorCode copyStatus = U_ZERO_ERROR;
            method = TESTCASES[tidx][i++];
            if (strcmp("L", method) == 0) {
                ulocbld_setLanguage(bld, TESTCASES[tidx][i++], -1);
                ulocbld_copyErrorTo(bld, &copyStatus);
                ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
            } else if (strcmp("S", method) == 0) {
                ulocbld_setScript(bld, TESTCASES[tidx][i++], -1);
                ulocbld_copyErrorTo(bld, &copyStatus);
                ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
            } else if (strcmp("R", method) == 0) {
                ulocbld_setRegion(bld, TESTCASES[tidx][i++], -1);
                ulocbld_copyErrorTo(bld, &copyStatus);
                ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
            } else if (strcmp("V", method) == 0) {
                ulocbld_setVariant(bld, TESTCASES[tidx][i++], -1);
                ulocbld_copyErrorTo(bld, &copyStatus);
                ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
            } else if (strcmp("K", method) == 0) {
                const char* key = TESTCASES[tidx][i++];
                const char* type = TESTCASES[tidx][i++];
                ulocbld_setUnicodeLocaleKeyword(bld, key, -1, type, -1);
                ulocbld_copyErrorTo(bld, &copyStatus);
                ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
            } else if (strcmp("A", method) == 0) {
                ulocbld_addUnicodeLocaleAttribute(bld, TESTCASES[tidx][i++], -1);
                ulocbld_copyErrorTo(bld, &copyStatus);
                ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
            } else if (strcmp("E", method) == 0) {
                const char* key = TESTCASES[tidx][i++];
                const char* value = TESTCASES[tidx][i++];
                ulocbld_setExtension(bld, key[0], value, -1);
                ulocbld_copyErrorTo(bld, &copyStatus);
                ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
            } else if (strcmp("P", method) == 0) {
                ulocbld_setExtension(bld, 'x', TESTCASES[tidx][i++], -1);
                ulocbld_copyErrorTo(bld, &copyStatus);
                ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
            } else if (strcmp("U", method) == 0) {
                ulocbld_setLocale(bld, TESTCASES[tidx][i++], -1);
                ulocbld_copyErrorTo(bld, &copyStatus);
                ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
            } else if (strcmp("B", method) == 0) {
                ulocbld_setLanguageTag(bld, TESTCASES[tidx][i++], -1);
                ulocbld_copyErrorTo(bld, &copyStatus);
                ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
            }
            // clear / remove
            else if (strcmp("C", method) == 0) {
                ulocbld_clear(bld);
                ulocbld_copyErrorTo(bld, &copyStatus);
                ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
            } else if (strcmp("N", method) == 0) {
                ulocbld_clearExtensions(bld);
                ulocbld_copyErrorTo(bld, &copyStatus);
                ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
            } else if (strcmp("D", method) == 0) {
                ulocbld_removeUnicodeLocaleAttribute(bld, TESTCASES[tidx][i++], -1);
                ulocbld_copyErrorTo(bld, &copyStatus);
                ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
            }
            // result
            else if (strcmp("X", method) == 0) {
                if (U_SUCCESS(status)) {
                    log_err("FAIL: No error return - test case: %s", actions);
                }
            } else if (strcmp("T", method) == 0) {
                status = U_ZERO_ERROR;
                ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
                if (status != copyStatus) {
                    log_err("copyErrorTo not matching");
                }
                if (U_FAILURE(status) ||
                    strcmp(locale, TESTCASES[tidx][i + 1]) != 0) {
                    log_err("FAIL: Wrong locale ID - %s %s %s", locale,
                            " for test case: ", actions);
                }
                ulocbld_buildLanguageTag(bld, tag, ULOC_FULLNAME_CAPACITY, &status);
                if (U_FAILURE(status) || strcmp(tag, TESTCASES[tidx][i]) != 0) {
                    log_err("FAIL: Wrong language tag - %s %s %s", tag,
                            " for test case: ", actions);
                }
                break;
            } else {
                // Unknown test method
                log_err("Unknown test case method: There is an error in the test case data.");
                break;
            }
            if (status != copyStatus) {
                log_err("copyErrorTo not matching");
            }
            if (U_FAILURE(status)) {
                if (strcmp("X", TESTCASES[tidx][i]) == 0) {
                    // This failure is expected
                    break;
                } else {
                    log_err("FAIL: U_ILLEGAL_ARGUMENT_ERROR at offset %d %s %s", i,
                          " in test case: ", actions);
                    break;
                }
            }
            if (strcmp("T", method) == 0) {
                break;
            }
        }  // while(true)
    }  // for TESTCASES
    ulocbld_close(bld);
}

static void TestLocaleBuilderBasic(void) {
    ULocaleBuilder* bld = ulocbld_open();
    ulocbld_setLanguage(bld, "zh", -1);
    Verify(bld, "zh", "ulocbld_setLanguage('zh') got Error: %s\n");
    ulocbld_setScript(bld, "Hant", -1);
    Verify(bld, "zh-Hant", "ulocbld_setScript('Hant') got Error: %s\n");

    ulocbld_setRegion(bld, "SG", -1);
    Verify(bld, "zh-Hant-SG", "ulocbld_setRegion('SG') got Error: %s\n");

    ulocbld_setRegion(bld, "HK", -1);
    ulocbld_setScript(bld, "Hans", -1);

    Verify(bld, "zh-Hans-HK",
           "ulocbld_setRegion('HK') and ulocbld_setScript('Hans') got Error: %s\n");

    ulocbld_setVariant(bld, "revised###", 7);
    Verify(bld, "zh-Hans-HK-revised",
           "ulocbld_setVariant('revised') got Error: %s\n");

    ulocbld_setUnicodeLocaleKeyword(bld, "nu", -1, "thai###", 4);
    Verify(bld, "zh-Hans-HK-revised-u-nu-thai",
           "ulocbld_setUnicodeLocaleKeyword('nu', 'thai'') got Error: %s\n");

    ulocbld_setUnicodeLocaleKeyword(bld, "co###", 2, "pinyin", -1);
    Verify(bld, "zh-Hans-HK-revised-u-co-pinyin-nu-thai",
           "ulocbld_setUnicodeLocaleKeyword('co', 'pinyin'') got Error: %s\n");

    ulocbld_setUnicodeLocaleKeyword(bld, "nu", -1, "latn###", 4);
    Verify(bld, "zh-Hans-HK-revised-u-co-pinyin-nu-latn",
           "ulocbld_setUnicodeLocaleKeyword('nu', 'latn'') got Error: %s\n");

    ulocbld_setUnicodeLocaleKeyword(bld, "nu", -1, "latn", -1);
    ulocbld_setUnicodeLocaleKeyword(bld, "nu", -1, NULL, 0);
    Verify(bld, "zh-Hans-HK-revised-u-co-pinyin",
           "ulocbld_setUnicodeLocaleKeyword('nu', ''') got Error: %s\n");


    ulocbld_setUnicodeLocaleKeyword(bld, "co", -1, NULL, 0);
    Verify(bld, "zh-Hans-HK-revised",
           "ulocbld_setUnicodeLocaleKeyword('nu', NULL) got Error: %s\n");

    ulocbld_setScript(bld, "", -1);
    Verify(bld, "zh-HK-revised",
           "ulocbld_setScript('') got Error: %s\n");

    ulocbld_setVariant(bld, "", -1);
    Verify(bld, "zh-HK",
           "ulocbld_setVariant('') got Error: %s\n");

    ulocbld_setRegion(bld, "", -1);
    Verify(bld, "zh",
           "ulocbld_setRegion('') got Error: %s\n");

    ulocbld_close(bld);
}

static void TestLocaleBuilderBasicWithExtensionsOnDefaultLocale(void) {
    // Change the default locale to one with extension tags.
    UErrorCode status = U_ZERO_ERROR;
    char originalDefault[ULOC_FULLNAME_CAPACITY];
    strcpy(originalDefault, uloc_getDefault());
    uloc_setDefault("en-US-u-hc-h12", &status);
    if (U_FAILURE(status)) {
        log_err("ERROR: Could not change the default locale");
        return;
    }

    // Invoke the basic test now that the default locale has been changed.
    TestLocaleBuilderBasic();

    uloc_setDefault(originalDefault, &status);
    if (U_FAILURE(status)) {
        log_err("ERROR: Could not restore the default locale");
    }
}

static void TestSetLanguageWellFormed(void) {
    // http://www.unicode.org/reports/tr35/tr35.html#unicode_language_subtag
    // unicode_language_subtag = alpha{2,3} | alpha{5,8};
    // ICUTC decided also support alpha{4}
    static const char* wellFormedLanguages[] = {
        "",

        // alpha{2}
        "en",
        "NE",
        "eN",
        "Ne",

        // alpha{3}
        "aNe",
        "zzz",
        "AAA",

        // alpha{4}
        "ABCD",
        "abcd",

        // alpha{5}
        "efgij",
        "AbCAD",
        "ZAASD",

        // alpha{6}
        "efgijk",
        "AADGFE",
        "AkDfFz",

        // alpha{7}
        "asdfads",
        "ADSFADF",
        "piSFkDk",

        // alpha{8}
        "oieradfz",
        "IADSFJKR",
        "kkDSFJkR",
    };
    for(int32_t i=0;i<UPRV_LENGTHOF(wellFormedLanguages);i++) {
        const char* lang = wellFormedLanguages[i];
        UErrorCode status = U_ZERO_ERROR;
        ULocaleBuilder* bld = ulocbld_open();
        ulocbld_setLanguage(bld, lang, -1);
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocbld_buildLocaleID(bld, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (U_FAILURE(status)) {
            log_err("setLanguage(\"%s\") got Error: %s\n",
                  lang, u_errorName(status));
        }
        ulocbld_close(bld);
    }
}

static void TestSetLanguageIllFormed(void) {
    static const char* illFormed[] = {
        "a",
        "z",
        "A",
        "F",
        "2",
        "0",
        "9",
        "{",
        ".",
        "[",
        "]",
        "\\",

        "e1",
        "N2",
        "3N",
        "4e",
        "e:",
        "43",
        "a9",

        "aN0",
        "z1z",
        "2zz",
        "3A3",
        "456",
        "af)",

        // Per 2019-01-23 ICUTC, we still accept 4alpha as tlang. see ICU-20321.
        // "latn",
        // "Arab",
        // "LATN",

        "e)gij",
        "Ab3AD",
        "ZAAS8",

        "efgi[]",
        "AA9GFE",
        "7kD3Fz",
        "as8fads",
        "0DSFADF",
        "'iSFkDk",

        "oieradf+",
        "IADSFJK-",
        "kkDSFJk0",

        // alpha{9}
        "oieradfab",
        "IADSFJKDE",
        "kkDSFJkzf",
    };
    for(int32_t i=0;i<UPRV_LENGTHOF(illFormed);i++) {
        const char* ill = illFormed[i];
        UErrorCode status = U_ZERO_ERROR;
        ULocaleBuilder* bld = ulocbld_open();
        ulocbld_setLanguage(bld, ill, -1);
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocbld_buildLocaleID(bld, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            log_err("setLanguage(\"%s\") should fail but has no Error\n", ill);
        }
        ulocbld_close(bld);
    }
}

static void TestSetScriptWellFormed(void) {
    // http://www.unicode.org/reports/tr35/tr35.html#unicode_script_subtag
    // unicode_script_subtag = alpha{4} ;
    static const char* wellFormedScripts[] = {
        "",

        "Latn",
        "latn",
        "lATN",
        "laTN",
        "arBN",
        "ARbn",
        "adsf",
        "aADF",
        "BSVS",
        "LATn",
    };
    for(int32_t i=0;i<UPRV_LENGTHOF(wellFormedScripts);i++) {
        const char* script = wellFormedScripts[i];
        UErrorCode status = U_ZERO_ERROR;
        ULocaleBuilder* bld = ulocbld_open();
        ulocbld_setScript(bld, script, -1);
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocbld_buildLocaleID(bld, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (U_FAILURE(status)) {
            log_err("setScript(\"%s\") got Error: %s\n",
                  script, u_errorName(status));
        }
        ulocbld_close(bld);
    }
}
static void TestSetScriptIllFormed(void) {
    static const char* illFormed[] = {
        "a",
        "z",
        "A",
        "F",
        "2",
        "0",
        "9",
        "{",
        ".",
        "[",
        "]",
        "\\",

        "e1",
        "N2",
        "3N",
        "4e",
        "e:",
        "43",
        "a9",

        "aN0",
        "z1z",
        "2zz",
        "3A3",
        "456",
        "af)",

        "0atn",
        "l1tn",
        "lA2N",
        "la4N",
        "arB5",
        "1234",

        "e)gij",
        "Ab3AD",
        "ZAAS8",

        "efgi[]",
        "AA9GFE",
        "7kD3Fz",

        "as8fads",
        "0DSFADF",
        "'iSFkDk",

        "oieradf+",
        "IADSFJK-",
        "kkDSFJk0",

        // alpha{9}
        "oieradfab",
        "IADSFJKDE",
        "kkDSFJkzf",
    };
    for(int32_t i=0;i<UPRV_LENGTHOF(illFormed);i++) {
        const char* ill = illFormed[i];
        UErrorCode status = U_ZERO_ERROR;
        ULocaleBuilder* bld = ulocbld_open();
        ulocbld_setScript(bld, ill, -1);
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocbld_buildLocaleID(bld, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            log_err("setScript(\"%s\") should fail but has no Error\n", ill);
        }
        ulocbld_close(bld);
    }
}

static void TestSetRegionWellFormed(void) {
    // http://www.unicode.org/reports/tr35/tr35.html#unicode_region_subtag
    // unicode_region_subtag = (alpha{2} | digit{3})
    static const char* wellFormedRegions[] = {
        "",

        // alpha{2}
        "en",
        "NE",
        "eN",
        "Ne",

        // digit{3}
        "000",
        "999",
        "123",
        "987"
    };
    for(int32_t i=0;i<UPRV_LENGTHOF(wellFormedRegions);i++) {
        const char* region = wellFormedRegions[i];
        UErrorCode status = U_ZERO_ERROR;
        ULocaleBuilder* bld = ulocbld_open();
        ulocbld_setRegion(bld, region, -1);
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocbld_buildLocaleID(bld, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (U_FAILURE(status)) {
            log_err("setRegion(\"%s\") got Error: %s\n",
                  region, u_errorName(status));
        }
        ulocbld_close(bld);
    }
}

static void TestSetRegionIllFormed(void) {
    static const char* illFormed[] = {
        "a",
        "z",
        "A",
        "F",
        "2",
        "0",
        "9",
        "{",
        ".",
        "[",
        "]",
        "\\",

        "e1",
        "N2",
        "3N",
        "4e",
        "e:",
        "43",
        "a9",

        "aN0",
        "z1z",
        "2zz",
        "3A3",
        "4.6",
        "af)",

        "0atn",
        "l1tn",
        "lA2N",
        "la4N",
        "arB5",
        "1234",

        "e)gij",
        "Ab3AD",
        "ZAAS8",

        "efgi[]",
        "AA9GFE",
        "7kD3Fz",

        "as8fads",
        "0DSFADF",
        "'iSFkDk",

        "oieradf+",
        "IADSFJK-",
        "kkDSFJk0",

        // alpha{9}
        "oieradfab",
        "IADSFJKDE",
        "kkDSFJkzf",
    };
    for(int32_t i=0;i<UPRV_LENGTHOF(illFormed);i++) {
        const char* ill = illFormed[i];
        UErrorCode status = U_ZERO_ERROR;
        ULocaleBuilder* bld = ulocbld_open();
        ulocbld_setRegion(bld, ill, -1);
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocbld_buildLocaleID(bld, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            log_err("setRegion(\"%s\") should fail but has no Error\n", ill);
        }
        ulocbld_close(bld);
    }
}

static void TestSetVariantWellFormed(void) {
    // http://www.unicode.org/reports/tr35/tr35.html#unicode_variant_subtag
    // (sep unicode_variant_subtag)*
    // unicode_variant_subtag = (alphanum{5,8} | digit alphanum{3}) ;
    static const char* wellFormedVariants[] = {
        "",

        // alphanum{5}
        "efgij",
        "AbCAD",
        "ZAASD",
        "0AASD",
        "A1CAD",
        "ef2ij",
        "ads3X",
        "owqF4",

        // alphanum{6}
        "efgijk",
        "AADGFE",
        "AkDfFz",
        "0ADGFE",
        "A9DfFz",
        "AADG7E",

        // alphanum{7}
        "asdfads",
        "ADSFADF",
        "piSFkDk",
        "a0dfads",
        "ADSF3DF",
        "piSFkD9",

        // alphanum{8}
        "oieradfz",
        "IADSFJKR",
        "kkDSFJkR",
        "0ADSFJKR",
        "12345679",

        // digit alphanum{3}
        "0123",
        "1abc",
        "20EF",
        "30EF",
        "8A03",
        "3Ax3",
        "9Axy",

        // (sep unicode_variant_subtag)*
        "0123-4567",
        "0ab3-ABCDE",
        "9ax3-xByD9",
        "9ax3-xByD9-adfk934a",

        "0123_4567",
        "0ab3_ABCDE",
        "9ax3_xByD9",
        "9ax3_xByD9_adfk934a",

        "9ax3-xByD9_adfk934a",
        "9ax3_xByD9-adfk934a",
    };
    for(int32_t i=0;i<UPRV_LENGTHOF(wellFormedVariants);i++) {
        const char* variant = wellFormedVariants[i];
        UErrorCode status = U_ZERO_ERROR;
        ULocaleBuilder* bld = ulocbld_open();
        ulocbld_setVariant(bld, variant, -1);
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocbld_buildLocaleID(bld, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (U_FAILURE(status)) {
            log_err("setVariant(\"%s\") got Error: %s\n",
                  variant, u_errorName(status));
        }
        ulocbld_close(bld);
    }
}

static void TestSetVariantIllFormed(void) {
    static const char* illFormed[] = {
        "a",
        "z",
        "A",
        "F",
        "2",
        "0",
        "9",
        "{",
        ".",
        "[",
        "]",
        "\\",

        "e1",
        "N2",
        "3N",
        "4e",
        "e:",
        "43",
        "a9",
        "en",
        "NE",
        "eN",
        "Ne",

        "aNe",
        "zzz",
        "AAA",
        "aN0",
        "z1z",
        "2zz",
        "3A3",
        "4.6",
        "af)",
        "345",
        "923",

        "Latn",
        "latn",
        "lATN",
        "laTN",
        "arBN",
        "ARbn",
        "adsf",
        "aADF",
        "BSVS",
        "LATn",
        "l1tn",
        "lA2N",
        "la4N",
        "arB5",
        "abc3",
        "A3BC",

        "e)gij",
        "A+3AD",
        "ZAA=8",

        "efgi[]",
        "AA9]FE",
        "7k[3Fz",

        "as8f/ds",
        "0DSFAD{",
        "'iSFkDk",

        "oieradf+",
        "IADSFJK-",
        "k}DSFJk0",

        // alpha{9}
        "oieradfab",
        "IADSFJKDE",
        "kkDSFJkzf",
        "123456789",

        "-0123",
        "-0123-4567",
        "0123-4567-",
        "-123-4567",
        "_0123",
        "_0123_4567",
        "0123_4567_",
        "_123_4567",

        "-abcde-figjk",
        "abcde-figjk-",
        "-abcde-figjk-",
        "_abcde_figjk",
        "abcde_figjk_",
        "_abcde_figjk_",
    };
    for(int32_t i=0;i<UPRV_LENGTHOF(illFormed);i++) {
        const char* ill = illFormed[i];
        UErrorCode status = U_ZERO_ERROR;
        ULocaleBuilder* bld = ulocbld_open();
        ulocbld_setVariant(bld, ill, -1);
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocbld_buildLocaleID(bld, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            log_err("setVariant(\"%s\") should fail but has no Error\n", ill);
        }
        ulocbld_close(bld);
    }
}

static void TestSetUnicodeLocaleKeywordWellFormed(void) {
    // http://www.unicode.org/reports/tr35/tr35.html#unicode_locale_extensions
    // keyword = key (sep type)? ;
    // key = alphanum alpha ;
    // type = alphanum{3,8} (sep alphanum{3,8})* ;
    static const char* wellFormed_key_value[] = {
        "aa", "123",
        "3b", "zyzbcdef",
        "0Z", "1ZB30zk9-abc",
        "cZ", "2ck30zfZ-adsf023-234kcZ",
        "ZZ", "Lant",
        "ko", "",
    };
    for (int i = 0; i < UPRV_LENGTHOF(wellFormed_key_value); i += 2) {
        UErrorCode status = U_ZERO_ERROR;
        ULocaleBuilder* bld = ulocbld_open();
        ulocbld_setUnicodeLocaleKeyword(bld, wellFormed_key_value[i], -1,
                                        wellFormed_key_value[i + 1], -1);
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocbld_buildLocaleID(bld, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (U_FAILURE(status)) {
            log_err("setUnicodeLocaleKeyword(\"%s\", \"%s\") got Error: %s\n",
                  wellFormed_key_value[i],
                  wellFormed_key_value[i + 1],
                  u_errorName(status));
        }
        ulocbld_close(bld);
    }
}

static void TestSetUnicodeLocaleKeywordIllFormedKey(void) {
    static const char* illFormed[] = {
        "34",
        "ab-cde",
        "123",
        "b3",
        "zyzabcdef",
        "Z0",
    };
    for (int i = 0; i < UPRV_LENGTHOF(illFormed); i++) {
        const char* ill = illFormed[i];
        UErrorCode status = U_ZERO_ERROR;
        ULocaleBuilder* bld = ulocbld_open();
        ulocbld_setUnicodeLocaleKeyword(bld, ill, -1, "abc", 3);
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocbld_buildLocaleID(bld, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            log_err("setUnicodeLocaleKeyword(\"%s\", \"abc\") should fail but has no Error\n",
                  ill);
        }
        ulocbld_close(bld);
    }
}

static void TestSetUnicodeLocaleKeywordIllFormedValue(void) {
    static const char* illFormed[] = {
        "34",
        "ab-",
        "-cd",
        "-ef-",
        "zyzabcdef",
        "ab-abc",
        "1ZB30zfk9-abc",
        "2ck30zfk9-adsf023-234kcZ",
    };
    ULocaleBuilder* bld = ulocbld_open();
    for (int i = 0; i < UPRV_LENGTHOF(illFormed); i++) {
        const char* ill = illFormed[i];
        ulocbld_clear(bld);
        UErrorCode status = U_ZERO_ERROR;
        ulocbld_setUnicodeLocaleKeyword(bld, "ab", 2, ill, -1);
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocbld_buildLocaleID(bld, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            log_err("setUnicodeLocaleKeyword(\"ab\", \"%s\") should fail but has no Error\n",
                  ill);
        }
    }
    ulocbld_close(bld);
}

static void TestAddRemoveUnicodeLocaleAttribute(void) {
    ULocaleBuilder* bld = ulocbld_open();
    ulocbld_setLanguage(bld, "fr", -1);
    ulocbld_addUnicodeLocaleAttribute(bld, "abc", -1);
    ulocbld_addUnicodeLocaleAttribute(bld, "aBc", -1);
    ulocbld_addUnicodeLocaleAttribute(bld, "EFG123", 3);
    ulocbld_addUnicodeLocaleAttribute(bld, "efghi###", 5);
    ulocbld_addUnicodeLocaleAttribute(bld, "efgh", -1);
    ulocbld_addUnicodeLocaleAttribute(bld, "efGhi", -1);
    ulocbld_addUnicodeLocaleAttribute(bld, "EFg", -1);
    ulocbld_addUnicodeLocaleAttribute(bld, "hijk", -1);
    ulocbld_addUnicodeLocaleAttribute(bld, "EFG", -1);
    ulocbld_addUnicodeLocaleAttribute(bld, "HiJK", -1);
    ulocbld_addUnicodeLocaleAttribute(bld, "aBc", -1);
    UErrorCode status = U_ZERO_ERROR;
    char locale[ULOC_FULLNAME_CAPACITY];
    ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
    if (U_FAILURE(status)) {
        log_err("addUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
    }
    char languageTag[ULOC_FULLNAME_CAPACITY];
    ulocbld_buildLanguageTag(bld, languageTag, ULOC_FULLNAME_CAPACITY, &status);
    const char* expected = "fr-u-abc-efg-efgh-efghi-hijk";
    if (U_FAILURE(status) || strcmp(languageTag, expected) != 0) {
        log_err("Should get \"%s\" but get \"%s\"\n", expected, languageTag);
    }

    // remove "efgh" in the middle with different casing.
    ulocbld_removeUnicodeLocaleAttribute(bld, "eFgH", -1);
    ulocbld_buildLanguageTag(bld, languageTag, ULOC_FULLNAME_CAPACITY, &status);
    expected = "fr-u-abc-efg-efghi-hijk";
    if (U_FAILURE(status) || strcmp(languageTag, expected) != 0) {
        log_err("removeUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
        log_err("Should get \"%s\" but get \"%s\"\n", expected, languageTag);
    }

    // remove non-existing attributes.
    ulocbld_removeUnicodeLocaleAttribute(bld, "efgh", -1);
    ulocbld_buildLanguageTag(bld, languageTag, ULOC_FULLNAME_CAPACITY, &status);
    if (U_FAILURE(status) || strcmp(languageTag, expected) != 0) {
        log_err("removeUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
        log_err("Should get \"%s\" but get \"%s\"\n", expected, languageTag);
    }

    // remove "abc" in the beginning with different casing.
    ulocbld_removeUnicodeLocaleAttribute(bld, "ABC", -1);
    ulocbld_buildLanguageTag(bld, languageTag, ULOC_FULLNAME_CAPACITY, &status);
    expected = "fr-u-efg-efghi-hijk";
    if (U_FAILURE(status) || strcmp(languageTag, expected) != 0) {
        log_err("removeUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
        log_err("Should get \"%s\" but get \"%s\"\n", expected, languageTag);
    }

    // remove non-existing substring in the end.
    ulocbld_removeUnicodeLocaleAttribute(bld, "hij", -1);
    ulocbld_buildLanguageTag(bld, languageTag, ULOC_FULLNAME_CAPACITY, &status);
    if (U_FAILURE(status) || strcmp(languageTag, expected) != 0) {
        log_err("removeUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
        log_err("Should get \"%s\" but get \"%s\"\n", expected, languageTag);
    }

    // remove "hijk" in the end with different casing.
    ulocbld_removeUnicodeLocaleAttribute(bld, "hIJK", -1);
    ulocbld_buildLanguageTag(bld, languageTag, ULOC_FULLNAME_CAPACITY, &status);
    expected = "fr-u-efg-efghi";
    if (U_FAILURE(status) || strcmp(languageTag, expected) != 0) {
        log_err("removeUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
        log_err("Should get \"%s\" but get \"%s\"\n", expected, languageTag);
    }

    // remove "efghi" in the end with different casing.
    ulocbld_removeUnicodeLocaleAttribute(bld, "EFGhi", -1);
    ulocbld_buildLanguageTag(bld, languageTag, ULOC_FULLNAME_CAPACITY, &status);
    expected = "fr-u-efg";
    if (U_FAILURE(status) || strcmp(languageTag, expected) != 0) {
        log_err("removeUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
        log_err("Should get \"%s\" but get \"%s\"\n", expected, languageTag);
    }

    // remove "efg" in as the only one, with different casing.
    ulocbld_removeUnicodeLocaleAttribute(bld, "EFG", -1);
    ulocbld_buildLanguageTag(bld, languageTag, ULOC_FULLNAME_CAPACITY, &status);
    expected = "fr";
    if (U_FAILURE(status) || strcmp(languageTag, expected) != 0) {
        log_err("removeUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
        log_err("Should get \"%s\" but get \"%s\"\n", expected, languageTag);
    }
    ulocbld_close(bld);
}

static void TestAddRemoveUnicodeLocaleAttributeWellFormed(void) {
    // http://www.unicode.org/reports/tr35/tr35.html#unicode_locale_extensions
    // attribute = alphanum{3,8} ;
    static const char* wellFormedAttributes[] = {
        // alphanum{3}
        "AbC",
        "ZAA",
        "0AA",
        "x3A",
        "xa8",

        // alphanum{4}
        "AbCA",
        "ZASD",
        "0ASD",
        "A3a4",
        "zK90",

        // alphanum{5}
        "efgij",
        "AbCAD",
        "ZAASD",
        "0AASD",
        "A1CAD",
        "ef2ij",
        "ads3X",
        "owqF4",

        // alphanum{6}
        "efgijk",
        "AADGFE",
        "AkDfFz",
        "0ADGFE",
        "A9DfFz",
        "AADG7E",

        // alphanum{7}
        "asdfads",
        "ADSFADF",
        "piSFkDk",
        "a0dfads",
        "ADSF3DF",
        "piSFkD9",

        // alphanum{8}
        "oieradfz",
        "IADSFJKR",
        "kkDSFJkR",
    };
    ULocaleBuilder* bld = ulocbld_open();
    for (int i = 0; i < UPRV_LENGTHOF(wellFormedAttributes); i++) {
        if (i % 5 == 0) {
            ulocbld_clear(bld);
        }
        ulocbld_addUnicodeLocaleAttribute(bld, wellFormedAttributes[i], -1);
        UErrorCode status = U_ZERO_ERROR;
        if (ulocbld_copyErrorTo(bld, &status)) {
            log_err("addUnicodeLocaleAttribute(\"%s\") got Error: %s\n",
                  wellFormedAttributes[i], u_errorName(status));
        }
        if (i > 2) {
            ulocbld_removeUnicodeLocaleAttribute(bld, wellFormedAttributes[i - 1], -1);
            if (ulocbld_copyErrorTo(bld, &status)) {
                log_err("removeUnicodeLocaleAttribute(\"%s\") got Error: %s\n",
                      wellFormedAttributes[i - 1], u_errorName(status));
            }
            ulocbld_removeUnicodeLocaleAttribute(bld, wellFormedAttributes[i - 3], -1);
            if (ulocbld_copyErrorTo(bld, &status)) {
                log_err("removeUnicodeLocaleAttribute(\"%s\") got Error: %s\n",
                      wellFormedAttributes[i - 3], u_errorName(status));
            }
        }
    }
    ulocbld_close(bld);
}

static void TestAddUnicodeLocaleAttributeIllFormed(void) {
    static const char* illFormed[] = {
        "aa",
        "34",
        "ab-",
        "-cd",
        "-ef-",
        "zyzabcdef",
        "123456789",
        "ab-abc",
        "1ZB30zfk9-abc",
        "2ck30zfk9-adsf023-234kcZ",
    };
    for (int i = 0; i < UPRV_LENGTHOF(illFormed); i++) {
        const char* ill = illFormed[i];
        UErrorCode status = U_ZERO_ERROR;
        ULocaleBuilder* bld = ulocbld_open();
        ulocbld_addUnicodeLocaleAttribute(bld, ill, -1);
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocbld_buildLocaleID(bld, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            log_err("addUnicodeLocaleAttribute(\"%s\") should fail but has no Error\n",
                  ill);
        }
        ulocbld_close(bld);
    }
}

static void TestSetExtensionU(void) {
    ULocaleBuilder* bld = ulocbld_open();
    ulocbld_setLanguage(bld, "zhABC", 2);
    Verify(bld, "zh",
           "ulocbld_setLanguage(\"zh\") got Error: %s\n");

    ulocbld_setExtension(bld, 'u', "co-stroke", -1);
    Verify(bld, "zh-u-co-stroke",
           "ulocbld_setExtension('u', \"co-stroke\") got Error: %s\n");

    ulocbld_setExtension(bld, 'U', "ca-islamicABCDE", 10);
    Verify(bld, "zh-u-ca-islamic",
           "ulocbld_setExtension('U', \"zh-u-ca-islamic\") got Error: %s\n");

    ulocbld_setExtension(bld, 'u', "ca-chinese", 10);
    Verify(bld, "zh-u-ca-chinese",
           "ulocbld_setExtension('u', \"ca-chinese\") got Error: %s\n");

    ulocbld_setExtension(bld, 'U', "co-pinyin1234", 9);
    Verify(bld, "zh-u-co-pinyin",
           "ulocbld_setExtension('U', \"co-pinyin\") got Error: %s\n");

    ulocbld_setRegion(bld, "TW123", 2);
    Verify(bld, "zh-TW-u-co-pinyin",
           "ulocbld_setRegion(\"TW\") got Error: %s\n");

    ulocbld_setExtension(bld, 'U', "", 0);
    Verify(bld, "zh-TW",
           "ulocbld_setExtension('U', \"\") got Error: %s\n");

    ulocbld_setExtension(bld, 'u', "abc-defg-kr-face", -1);
    Verify(bld, "zh-TW-u-abc-defg-kr-face",
           "ulocbld_setExtension('u', \"abc-defg-kr-face\") got Error: %s\n");

    ulocbld_setExtension(bld, 'U', "ca-japanese", -1);
    Verify(bld, "zh-TW-u-ca-japanese",
           "ulocbld_setExtension('U', \"ca-japanese\") got Error: %s\n");

    ulocbld_close(bld);
}

static void TestSetExtensionValidateUWellFormed(void) {
    static const char* wellFormedExtensions[] = {
        // keyword
        //   keyword = key (sep type)? ;
        //   key = alphanum alpha ;
        //   type = alphanum{3,8} (sep alphanum{3,8})* ;
        "3A",
        "ZA",
        "az-abc",
        "zz-123",
        "7z-12345678",
        "kb-A234567Z",
        // (sep keyword)+
        "1z-ZZ",
        "2z-ZZ-123",
        "3z-ZZ-123-cd",
        "0z-ZZ-123-cd-efghijkl",
        // attribute
        "abc",
        "456",
        "87654321",
        "ZABADFSD",
        // (sep attribute)+
        "abc-ZABADFSD",
        "123-ZABADFSD",
        "K2K-12345678",
        "K2K-12345678-zzz",
        // (sep attribute)+ (sep keyword)*
        "K2K-12345678-zz",
        "K2K-12345678-zz-0z",
        "K2K-12345678-9z-AZ-abc",
        "K2K-12345678-zz-9A-234",
        "K2K-12345678-zk0-abc-efg-zz-9k-234",
    };
    for (int i = 0; i < UPRV_LENGTHOF(wellFormedExtensions); i++) {
        const char* extension = wellFormedExtensions[i];
        UErrorCode status = U_ZERO_ERROR;
        ULocaleBuilder* bld = ulocbld_open();
        ulocbld_setExtension(bld, 'u', extension, -1);
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocbld_buildLocaleID(bld, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (U_FAILURE(status)) {
            log_err("setExtension('u', \"%s\") got Error: %s\n",
                  extension, u_errorName(status));
        }
        ulocbld_close(bld);
    }
}

static void TestSetExtensionValidateUIllFormed(void) {
    static const char* illFormed[] = {
        // bad key
        "-",
        "-ab",
        "ab-",
        "abc-",
        "-abc",
        "0",
        "a",
        "A0",
        "z9",
        "09",
        "90",
        // bad keyword
        "AB-A0",
        "AB-efg-A0",
        "xy-123456789",
        "AB-Aa-",
        "AB-Aac-",
        // bad attribute
        "abcdefghi",
        "abcdefgh-",
        "abcdefgh-abcdefghi",
        "abcdefgh-1",
        "abcdefgh-a",
        "abcdefgh-a2345678z",
    };
    for (int i = 0; i < UPRV_LENGTHOF(illFormed); i++) {
        const char* ill = illFormed[i];
        UErrorCode status = U_ZERO_ERROR;
        ULocaleBuilder* bld = ulocbld_open();
        ulocbld_setExtension(bld, 'u', ill, -1);
        char buffer[ULOC_FULLNAME_CAPACITY];
        ulocbld_buildLocaleID(bld, buffer, ULOC_FULLNAME_CAPACITY, &status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            log_err("setExtension('u', \"%s\") should fail but has no Error\n",
                  ill);
        }
        ulocbld_close(bld);
    }
}

static void TestSetExtensionT(void) {
    ULocaleBuilder* bld = ulocbld_open();
    ulocbld_setLanguage(bld, "fr", 2);
    Verify(bld, "fr",
           "ulocbld_setLanguage(\"fr\") got Error: %s\n");

    ulocbld_setExtension(bld, 'T', "zh", -1);
    Verify(bld, "fr-t-zh",
           "ulocbld_setExtension('T', \"zh\") got Error: %s\n");

    ulocbld_setExtension(bld, 't', "zh-Hant-TW-1234-A9-123-456ABCDE", -1);
    Verify(bld, "fr-t-zh-hant-tw-1234-a9-123-456abcde",
           "ulocbld_setExtension('t', \"zh-Hant-TW-1234-A9-123-456ABCDE\") got Error: %s\n");

    ulocbld_setExtension(bld, 'T', "a9-123", -1);
    Verify(bld, "fr-t-a9-123",
           "ulocbld_setExtension('T', \"a9-123\") got Error: %s\n");

    ulocbld_setRegion(bld, "MX###", 2);
    Verify(bld, "fr-MX-t-a9-123",
           "ulocbld_setRegion(\"MX\") got Error: %s\n");

    ulocbld_setScript(bld, "Hans##", 4);
    Verify(bld, "fr-Hans-MX-t-a9-123",
           "ulocbld_setScript(\"Hans\") got Error: %s\n");

    ulocbld_setVariant(bld, "9abc-abcde1234", 10 );
    Verify(bld, "fr-Hans-MX-9abc-abcde-t-a9-123",
           "ulocbld_setVariant(\"9abc-abcde\") got Error: %s\n");

    ulocbld_setExtension(bld, 'T', "", 0);
    Verify(bld, "fr-Hans-MX-9abc-abcde",
           "ulocbld_setExtension('T', \"\") got Error: %s\n");

    ulocbld_close(bld);
}

static void TestSetExtensionValidateTWellFormed(void) {
    // ((sep tlang (sep tfield)*) | (sep tfield)+)
    static const char* wellFormedExtensions[] = {
        // tlang
        //  tlang = unicode_language_subtag (sep unicode_script_subtag)?
        //          (sep unicode_region_subtag)?  (sep unicode_variant_subtag)* ;
        // unicode_language_subtag
        "en",
        "abc",
        "abcde",
        "ABCDEFGH",
        // unicode_language_subtag sep unicode_script_subtag
        "en-latn",
        "abc-arab",
        "ABCDEFGH-Thai",
        // unicode_language_subtag sep unicode_script_subtag sep unicode_region_subtag
        "en-latn-ME",
        "abc-arab-RU",
        "ABCDEFGH-Thai-TH",
        "en-latn-409",
        "abc-arab-123",
        "ABCDEFGH-Thai-456",
        // unicode_language_subtag sep unicode_region_subtag
        "en-ME",
        "abc-RU",
        "ABCDEFGH-TH",
        "en-409",
        "abc-123",
        "ABCDEFGH-456",
        // unicode_language_subtag sep unicode_script_subtag sep unicode_region_subtag
        // sep (sep unicode_variant_subtag)*
        "en-latn-ME-abcde",
        "abc-arab-RU-3abc-abcdef",
        "ABCDEFGH-Thai-TH-ADSFS-9xyz-abcdef",
        "en-latn-409-xafsa",
        "abc-arab-123-ADASDF",
        "ABCDEFGH-Thai-456-9sdf-ADASFAS",
        // (sep tfield)+
        "A0-abcde",
        "z9-abcde123",
        "z9-abcde123-a1-abcde",
        // tlang (sep tfield)*
        "fr-A0-abcde",
        "fr-FR-A0-abcde",
        "fr-123-z9-abcde123-a1-abcde",
        "fr-Latn-FR-z9-abcde123-a1-abcde",
        "gab-Thai-TH-abcde-z9-abcde123-a1-abcde",
        "gab-Thai-TH-0bde-z9-abcde123-a1-abcde",
    };
    ULocaleBuilder* bld = ulocbld_open();
    for (int i = 0; i < UPRV_LENGTHOF(wellFormedExtensions); i++) {
        ulocbld_clear(bld);
        const char* extension = wellFormedExtensions[i];
        ulocbld_setExtension(bld, 't', extension, -1);
        UErrorCode status = U_ZERO_ERROR;
        if (ulocbld_copyErrorTo(bld, &status)) {
            log_err("ulocbld_setExtension('t', \"%s\") got Error: %s\n",
                  extension, u_errorName(status));
        }
    }
    ulocbld_close(bld);
}

static void TestSetExtensionValidateTIllFormed(void) {
    static const char* illFormed[] = {
        "a",
        "a-",
        "0",
        "9-",
        "-9",
        "-z",
        "Latn",
        "Latn-",
        "en-",
        "nob-",
        "-z9",
        "a3",
        "a3-",
        "3a",
        "0z-",
        "en-123-a1",
        "en-TH-a1",
        "gab-TH-a1",
        "gab-Thai-a1",
        "gab-Thai-TH-a1",
        "gab-Thai-TH-0bde-a1",
        "gab-Thai-TH-0bde-3b",
        "gab-Thai-TH-0bde-z9-a1",
        "gab-Thai-TH-0bde-z9-3b",
        "gab-Thai-TH-0bde-z9-abcde123-3b",
        "gab-Thai-TH-0bde-z9-abcde123-ab",
        "gab-Thai-TH-0bde-z9-abcde123-ab",
        "gab-Thai-TH-0bde-z9-abcde123-a1",
        "gab-Thai-TH-0bde-z9-abcde123-a1-",
        "gab-Thai-TH-0bde-z9-abcde123-a1-a",
        "gab-Thai-TH-0bde-z9-abcde123-a1-ab",
        // ICU-21408
        "root",
    };
    ULocaleBuilder* bld = ulocbld_open();
    for (int i = 0; i < UPRV_LENGTHOF(illFormed); i++) {
        ulocbld_clear(bld);
        const char* ill = illFormed[i];
        UErrorCode status = U_ZERO_ERROR;
        ulocbld_setExtension(bld, 't', ill, -1);
        if (!ulocbld_copyErrorTo(bld, &status) || status != U_ILLEGAL_ARGUMENT_ERROR) {
            log_err("setExtension('t', \"%s\") should fail but has no Error\n",
                  ill);
        }
    }
    ulocbld_close(bld);
}

static void TestSetExtensionPU(void) {
    ULocaleBuilder* bld = ulocbld_open();
    ulocbld_setLanguage(bld, "ar123", 2);
    Verify(bld, "ar",
           "ulocbld_setLanguage(\"ar\") got Error: %s\n");

    ulocbld_setExtension(bld, 'X', "a-b-c-d-e12345", 9);
    Verify(bld, "ar-x-a-b-c-d-e",
           "ulocbld_setExtension('X', \"a-b-c-d-e\") got Error: %s\n");

    ulocbld_setExtension(bld, 'x', "0-1-2-3", -1);
    Verify(bld, "ar-x-0-1-2-3",
           "ulocbld_setExtension('x', \"0-1-2-3\") got Error: %s\n");

    ulocbld_setExtension(bld, 'X', "0-12345678-x-x", -1);
    Verify(bld, "ar-x-0-12345678-x-x",
           "ulocbld_setExtension('x', \"ar-x-0-12345678-x-x\") got Error: %s\n");

    ulocbld_setRegion(bld, "TH123", 2);
    Verify(bld, "ar-TH-x-0-12345678-x-x",
           "ulocbld_setRegion(\"TH\") got Error: %s\n");

    ulocbld_setExtension(bld, 'X', "", -1);
    Verify(bld, "ar-TH",
           "ulocbld_setExtension(\"X\") got Error: %s\n");
    ulocbld_close(bld);
}

static void TestSetExtensionValidatePUWellFormed(void) {
    // ((sep tlang (sep tfield)*) | (sep tfield)+)
    static const char* wellFormedExtensions[] = {
        "a",  // Short subtag
        "z",  // Short subtag
        "0",  // Short subtag, digit
        "9",  // Short subtag, digit
        "a-0",  // Two short subtag, alpha and digit
        "9-z",  // Two short subtag, digit and alpha
        "ab",
        "abc",
        "abcefghi",  // Long subtag
        "87654321",
        "01",
        "234",
        "0a-ab-87654321",  // Three subtags
        "87654321-ab-00-3A",  // Four subtabs
        "a-9-87654321",  // Three subtags with short and long subtags
        "87654321-ab-0-3A",
    };
    ULocaleBuilder* bld = ulocbld_open();
    for (int i = 0; i < UPRV_LENGTHOF(wellFormedExtensions); i++) {
        ulocbld_clear(bld);
        const char* extension = wellFormedExtensions[i];
        UErrorCode status = U_ZERO_ERROR;
        ulocbld_setExtension(bld, 'x', extension, -1);
        if (ulocbld_copyErrorTo(bld, &status) || U_FAILURE(status)) {
            log_err("setExtension('x', \"%s\") got Error: %s\n",
                  extension, u_errorName(status));
        }
    }
    ulocbld_close(bld);
}

static void TestSetExtensionValidatePUIllFormed(void) {
    static const char* illFormed[] = {
        "123456789",  // Too long
        "abcdefghi",  // Too long
        "ab-123456789",  // Second subtag too long
        "abcdefghi-12",  // First subtag too long
        "a-ab-987654321",  // Third subtag too long
        "987654321-a-0-3",  // First subtag too long
    };
    ULocaleBuilder* bld = ulocbld_open();
    for (int i = 0; i < UPRV_LENGTHOF(illFormed); i++) {
        const char* ill = illFormed[i];
        ulocbld_clear(bld);
        ulocbld_setExtension(bld, 'x', ill, -1);
        UErrorCode status = U_ZERO_ERROR;
        if (!ulocbld_copyErrorTo(bld, &status) ||status != U_ILLEGAL_ARGUMENT_ERROR) {
            log_err("ulocbld_setExtension('x', \"%s\") should fail but has no Error\n",
                  ill);
        }
    }
    ulocbld_close(bld);
}

static void TestSetExtensionOthers(void) {
    ULocaleBuilder* bld = ulocbld_open();
    ulocbld_setLanguage(bld, "fr", -1);
    Verify(bld, "fr",
           "ulocbld_setLanguage(\"fr\") got Error: %s\n");

    ulocbld_setExtension(bld, 'Z', "ab1234", 2);
    Verify(bld, "fr-z-ab",
           "ulocbld_setExtension('Z', \"ab\") got Error: %s\n");

    ulocbld_setExtension(bld, '0', "xyz12345-abcdefg", -1);
    Verify(bld, "fr-0-xyz12345-abcdefg-z-ab",
           "ulocbld_setExtension('0', \"xyz12345-abcdefg\") got Error: %s\n");

    ulocbld_setExtension(bld, 'a', "01-12345678-ABcdef", -1);
    Verify(bld, "fr-0-xyz12345-abcdefg-a-01-12345678-abcdef-z-ab",
           "ulocbld_setExtension('a', \"01-12345678-ABcdef\") got Error: %s\n");

    ulocbld_setRegion(bld, "TH1234", 2);
    Verify(bld, "fr-TH-0-xyz12345-abcdefg-a-01-12345678-abcdef-z-ab",
           "ulocbld_setRegion(\"TH\") got Error: %s\n");

    ulocbld_setScript(bld, "Arab", -1);
    Verify(bld, "fr-Arab-TH-0-xyz12345-abcdefg-a-01-12345678-abcdef-z-ab",
           "ulocbld_setRegion(\"Arab\") got Error: %s\n");

    ulocbld_setExtension(bld, 'A', "97", 2);
    Verify(bld, "fr-Arab-TH-0-xyz12345-abcdefg-a-97-z-ab",
           "ulocbld_setExtension('a', \"97\") got Error: %s\n");

    ulocbld_setExtension(bld, 'a', "", 0);
    Verify(bld, "fr-Arab-TH-0-xyz12345-abcdefg-z-ab",
           "ulocbld_setExtension('a', \"\") got Error: %s\n");

    ulocbld_setExtension(bld, '0', "", -1);
    Verify(bld, "fr-Arab-TH-z-ab",
           "ulocbld_setExtension('0', \"\") got Error: %s\n");
    ulocbld_close(bld);
}

static void TestSetExtensionValidateOthersWellFormed(void) {
    static const char* wellFormedExtensions[] = {
        "ab",
        "abc",
        "abcefghi",
        "01",
        "234",
        "87654321",
        "0a-ab-87654321",
        "87654321-ab-00-3A",
    };

    const char * aToZ = "abcdefghijklmnopqrstuvwxyz";
    const int32_t aToZLen = strlen(aToZ);
    ULocaleBuilder* bld = ulocbld_open();
    for (int i = 0; i < UPRV_LENGTHOF(wellFormedExtensions); i++) {
        const char* extension = wellFormedExtensions[i];
        ulocbld_clear(bld);
        char ch = aToZ[i];
        i = (i + 1) % aToZLen;
        UErrorCode status = U_ZERO_ERROR;
        ulocbld_setExtension(bld, ch, extension, -1);
        if (ulocbld_copyErrorTo(bld, &status) || U_FAILURE(status)) {
            log_err("ulocbld_setExtension('%c', \"%s\") got Error: %s\n",
                  ch, extension, u_errorName(status));
        }
    }

    const char* someChars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789`~!@#$%^&*()-_=+;:,.<>?";
    const int32_t someCharsLen = strlen(someChars);
    for (int32_t i = 0; i < someCharsLen; i++) {
        char ch = someChars[i];
        UErrorCode status = U_ZERO_ERROR;
        ulocbld_clear(bld);
        ulocbld_setExtension(bld, ch, wellFormedExtensions[ch % UPRV_LENGTHOF(wellFormedExtensions)], -1);
        if (uprv_isASCIILetter(ch) || ('0' <= ch && ch <= '9')) {
            if (ch != 't' && ch != 'T' && ch != 'u' && ch != 'U' && ch != 'x' && ch != 'X') {
                if (ulocbld_copyErrorTo(bld, &status) || U_FAILURE(status)) {
                    log_err("setExtension('%c', \"%s\") got Error: %s\n",
                          ch, wellFormedExtensions[ch % UPRV_LENGTHOF(wellFormedExtensions)], u_errorName(status));
                }
            }
        } else {
            if (!ulocbld_copyErrorTo(bld, &status) || status != U_ILLEGAL_ARGUMENT_ERROR) {
                log_err("setExtension('%c', \"%s\") should fail but has no Error\n",
                      ch, wellFormedExtensions[ch % UPRV_LENGTHOF(wellFormedExtensions)]);
            }
        }

    }
    ulocbld_close(bld);
}

static void TestSetExtensionValidateOthersIllFormed(void) {
    static const char* illFormed[] = {
        "0",  // Too short
        "a",  // Too short
        "123456789",  // Too long
        "abcdefghi",  // Too long
        "ab-123456789",  // Second subtag too long
        "abcdefghi-12",  // First subtag too long
        "a-ab-87654321",  // Third subtag too long
        "87654321-a-0-3",  // First subtag too long
    };
    const char * aToZ = "abcdefghijklmnopqrstuvwxyz";
    const int32_t aToZLen = strlen(aToZ);
    ULocaleBuilder* bld = ulocbld_open();
    for (int i = 0; i < UPRV_LENGTHOF(illFormed); i++) {
        const char* ill = illFormed[i];
        char ch = aToZ[i];
        ulocbld_clear(bld);
        i = (i + 1) % aToZLen;
        UErrorCode status = U_ZERO_ERROR;
        ulocbld_setExtension(bld, ch, ill, -1);
        if (!ulocbld_copyErrorTo(bld, &status) || status != U_ILLEGAL_ARGUMENT_ERROR) {
            log_err("setExtension('%c', \"%s\") should fail but has no Error\n",
                  ch, ill);
        }
    }
    ulocbld_close(bld);
}

static void TestSetLocale(void) {
    ULocaleBuilder* bld1 = ulocbld_open();
    ULocaleBuilder* bld2 = ulocbld_open();
    UErrorCode status = U_ZERO_ERROR;

    ulocbld_setLanguage(bld1, "en", -1);
    ulocbld_setScript(bld1, "Latn", -1);
    ulocbld_setRegion(bld1, "MX", -1);
    ulocbld_setVariant(bld1, "3456-abcde", -1);
    ulocbld_addUnicodeLocaleAttribute(bld1, "456", -1);
    ulocbld_addUnicodeLocaleAttribute(bld1, "123", -1);
    ulocbld_setUnicodeLocaleKeyword(bld1, "nu", -1, "thai", -1);
    ulocbld_setUnicodeLocaleKeyword(bld1, "co", -1, "stroke", -1);
    ulocbld_setUnicodeLocaleKeyword(bld1, "ca", -1, "chinese", -1);
    char locale1[ULOC_FULLNAME_CAPACITY];
    ulocbld_buildLocaleID(bld1, locale1, ULOC_FULLNAME_CAPACITY, &status);

    if (U_FAILURE(status)) {
        log_err("build got Error: %s\n", u_errorName(status));
    }
    ulocbld_setLocale(bld2, locale1, -1);
    char locale2[ULOC_FULLNAME_CAPACITY];
    ulocbld_buildLocaleID(bld2, locale2, ULOC_FULLNAME_CAPACITY, &status);
    if (U_FAILURE(status)) {
        log_err("build got Error: %s\n", u_errorName(status));
    }
    if (strcmp(locale1, locale2) != 0) {
        log_err("Two locales should be the same, but one is '%s' and the other is '%s'",
              locale1, locale2);
    }
    ulocbld_close(bld1);
    ulocbld_close(bld2);
}

static void TestBuildULocale(void) {
    ULocaleBuilder* bld1 = ulocbld_open();
    UErrorCode status = U_ZERO_ERROR;

    ulocbld_setLanguage(bld1, "fr", -1);
    ULocale* fr = ulocbld_buildULocale(bld1, &status);
    if (assertSuccess(WHERE "ulocbld_buildULocale()", &status)) {
        assertEquals(WHERE "ulocale_getLanguage()", "fr", ulocale_getLanguage(fr));
    }

    ulocbld_setLanguage(bld1, "ar", -1);
    ulocbld_setScript(bld1, "Arab", -1);
    ulocbld_setRegion(bld1, "EG", -1);
    ulocbld_setVariant(bld1, "3456-abcde", -1);
    ulocbld_setUnicodeLocaleKeyword(bld1, "nu", -1, "thai", -1);
    ulocbld_setUnicodeLocaleKeyword(bld1, "co", -1, "stroke", -1);
    ulocbld_setUnicodeLocaleKeyword(bld1, "ca", -1, "chinese", -1);
    ULocale* l = ulocbld_buildULocale(bld1, &status);

    if (assertSuccess(WHERE "ulocbld_buildULocale()", &status)) {
        assertEquals(WHERE "ulocale_getLanguage()", "ar", ulocale_getLanguage(l));
        assertEquals(WHERE "ulocale_getScript()", "Arab", ulocale_getScript(l));
        assertEquals(WHERE "ulocale_getRegion()", "EG", ulocale_getRegion(l));
        assertEquals(WHERE "ulocale_getVariant()", "3456_ABCDE", ulocale_getVariant(l));
        char buf[ULOC_FULLNAME_CAPACITY];
        assertIntEquals(WHERE "ulocale_getUnicodeKeywordValue(\"nu\")", 4,
                     ulocale_getUnicodeKeywordValue(l, "nu", -1, buf, ULOC_FULLNAME_CAPACITY, &status));
        if (assertSuccess(WHERE "ulocale_getUnicodeKeywordValue(\"nu\")", &status)) {
            assertEquals(WHERE "ulocale_getUnicodeKeywordValue(\"nu\")", "thai", buf);
        }

        status = U_ZERO_ERROR;
        assertIntEquals(WHERE "ulocale_getUnicodeKeywordValue(\"co\")", 6,
                     ulocale_getUnicodeKeywordValue(l, "co", -1, buf, ULOC_FULLNAME_CAPACITY, &status));
        if (assertSuccess(WHERE "ulocale_getUnicodeKeywordValue(\"co\")", &status)) {
            assertEquals(WHERE "ulocale_getUnicodeKeywordValue(\"co\")", "stroke", buf);
        }

        status = U_ZERO_ERROR;
        assertIntEquals(WHERE "ulocale_getUnicodeKeywordValue(\"ca\")", 7,
                     ulocale_getUnicodeKeywordValue(l, "ca", -1, buf, ULOC_FULLNAME_CAPACITY, &status));
        if (assertSuccess(WHERE "ulocale_getUnicodeKeywordValue(\"ca\")", &status)) {
            assertEquals(WHERE "ulocale_getUnicodeKeywordValue(\"ca\")", "chinese", buf);
        }
        ulocale_close(l);
    }
    ulocbld_adoptULocale(bld1, fr);
    char buf[ULOC_FULLNAME_CAPACITY];
    ulocbld_buildLocaleID(bld1, buf, ULOC_FULLNAME_CAPACITY, &status);
    if (assertSuccess(WHERE "ulocbld_buildULocale()", &status)) {
        assertEquals(WHERE "ulocbld_buildULocale()", "fr", buf);
    }
    ulocbld_close(bld1);
}


static void TestPosixCases(void) {
    UErrorCode status = U_ZERO_ERROR;
    ULocaleBuilder* bld = ulocbld_open();
    ulocbld_setLanguage(bld, "en", -1);
    ulocbld_setRegion(bld, "MX", -1);
    ulocbld_setScript(bld, "Arab", -1);
    ulocbld_setUnicodeLocaleKeyword(bld, "nu", -1, "Thai", -1);
    ulocbld_setExtension(bld, 'x', "1", -1);
    // All of above should be cleared by the setLocale call.
    const char* posix = "en_US_POSIX";
    ulocbld_setLocale(bld, posix, -1);
    char locale[ULOC_FULLNAME_CAPACITY];
    ulocbld_buildLocaleID(bld, locale, ULOC_FULLNAME_CAPACITY, &status);
    if (U_FAILURE(status)) {
        log_err("build got Error: %s\n", u_errorName(status));
    }
    if (strcmp(posix, locale) != 0) {
        log_err("The result locale should be the set as the setLocale %s but got %s\n",
                posix, locale);
    }
    ulocbld_close(bld);
}

#define TESTCASE(name) addTest(root, &name, "tsutil/ulocbuildertst/" #name)
void addLocaleBuilderTest(TestNode** root)
{
    TESTCASE(TestLocaleBuilder);
    TESTCASE(TestLocaleBuilderBasic);
    TESTCASE(TestLocaleBuilderBasicWithExtensionsOnDefaultLocale);
    TESTCASE(TestSetLanguageWellFormed);
    TESTCASE(TestSetLanguageIllFormed);
    TESTCASE(TestSetScriptWellFormed);
    TESTCASE(TestSetScriptIllFormed);
    TESTCASE(TestSetRegionWellFormed);
    TESTCASE(TestSetRegionIllFormed);
    TESTCASE(TestSetVariantWellFormed);
    TESTCASE(TestSetVariantIllFormed);
    TESTCASE(TestSetUnicodeLocaleKeywordWellFormed);
    TESTCASE(TestSetUnicodeLocaleKeywordIllFormedKey);
    TESTCASE(TestSetUnicodeLocaleKeywordIllFormedValue);
    TESTCASE(TestAddRemoveUnicodeLocaleAttribute);
    TESTCASE(TestAddRemoveUnicodeLocaleAttributeWellFormed);
    TESTCASE(TestAddUnicodeLocaleAttributeIllFormed);
    TESTCASE(TestSetExtensionU);
    TESTCASE(TestSetExtensionValidateUWellFormed);
    TESTCASE(TestSetExtensionValidateUIllFormed);
    TESTCASE(TestSetExtensionT);
    TESTCASE(TestSetExtensionValidateTWellFormed);
    TESTCASE(TestSetExtensionValidateTIllFormed);
    TESTCASE(TestSetExtensionPU);
    TESTCASE(TestSetExtensionValidatePUWellFormed);
    TESTCASE(TestSetExtensionValidatePUIllFormed);
    TESTCASE(TestSetExtensionOthers);
    TESTCASE(TestSetExtensionValidateOthersWellFormed);
    TESTCASE(TestSetExtensionValidateOthersIllFormed);
    TESTCASE(TestSetLocale);
    TESTCASE(TestBuildULocale);
    TESTCASE(TestPosixCases);
}
