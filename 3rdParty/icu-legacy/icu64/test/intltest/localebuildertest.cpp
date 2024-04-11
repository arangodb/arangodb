// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include <memory>

#include "cmemory.h"
#include "cstring.h"
#include "localebuildertest.h"
#include "unicode/localebuilder.h"
#include "unicode/strenum.h"

LocaleBuilderTest::LocaleBuilderTest()
{
}

LocaleBuilderTest::~LocaleBuilderTest()
{
}

void LocaleBuilderTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestAddRemoveUnicodeLocaleAttribute);
    TESTCASE_AUTO(TestAddRemoveUnicodeLocaleAttributeWellFormed);
    TESTCASE_AUTO(TestAddUnicodeLocaleAttributeIllFormed);
    TESTCASE_AUTO(TestLocaleBuilder);
    TESTCASE_AUTO(TestLocaleBuilderBasic);
    TESTCASE_AUTO(TestLocaleBuilderBasicWithExtensionsOnDefaultLocale);
    TESTCASE_AUTO(TestPosixCases);
    TESTCASE_AUTO(TestSetExtensionOthers);
    TESTCASE_AUTO(TestSetExtensionPU);
    TESTCASE_AUTO(TestSetExtensionT);
    TESTCASE_AUTO(TestSetExtensionU);
    TESTCASE_AUTO(TestSetExtensionValidateOthersIllFormed);
    TESTCASE_AUTO(TestSetExtensionValidateOthersWellFormed);
    TESTCASE_AUTO(TestSetExtensionValidatePUIllFormed);
    TESTCASE_AUTO(TestSetExtensionValidatePUWellFormed);
    TESTCASE_AUTO(TestSetExtensionValidateTIllFormed);
    TESTCASE_AUTO(TestSetExtensionValidateTWellFormed);
    TESTCASE_AUTO(TestSetExtensionValidateUIllFormed);
    TESTCASE_AUTO(TestSetExtensionValidateUWellFormed);
    TESTCASE_AUTO(TestSetLanguageIllFormed);
    TESTCASE_AUTO(TestSetLanguageWellFormed);
    TESTCASE_AUTO(TestSetLocale);
    TESTCASE_AUTO(TestSetRegionIllFormed);
    TESTCASE_AUTO(TestSetRegionWellFormed);
    TESTCASE_AUTO(TestSetScriptIllFormed);
    TESTCASE_AUTO(TestSetScriptWellFormed);
    TESTCASE_AUTO(TestSetUnicodeLocaleKeywordIllFormedKey);
    TESTCASE_AUTO(TestSetUnicodeLocaleKeywordIllFormedValue);
    TESTCASE_AUTO(TestSetUnicodeLocaleKeywordWellFormed);
    TESTCASE_AUTO(TestSetVariantIllFormed);
    TESTCASE_AUTO(TestSetVariantWellFormed);
    TESTCASE_AUTO_END;
}

void LocaleBuilderTest::Verify(LocaleBuilder& bld, const char* expected, const char* msg) {
    UErrorCode status = U_ZERO_ERROR;
    UErrorCode copyStatus = U_ZERO_ERROR;
    UErrorCode errorStatus = U_ILLEGAL_ARGUMENT_ERROR;
    if (bld.copyErrorTo(copyStatus)) {
        errln(msg, u_errorName(copyStatus));
    }
    if (!bld.copyErrorTo(errorStatus) || errorStatus != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Should always get the previous error and return false");
    }
    Locale loc = bld.build(status);
    if (U_FAILURE(status)) {
        errln(msg, u_errorName(status));
    }
    if (status != copyStatus) {
        errln(msg, u_errorName(status));
    }
    std::string tag = loc.toLanguageTag<std::string>(status);
    if (U_FAILURE(status)) {
        errln("loc.toLanguageTag() got Error: %s\n",
              u_errorName(status));
    }
    if (tag != expected) {
        errln("should get \"%s\", but got \"%s\"\n", expected, tag.c_str());
    }
}

void LocaleBuilderTest::TestLocaleBuilder() {
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
        {"L", "en", "R", "CA", "L", nullptr, "T", "und-CA", "_CA"},
        {"L", "en", "R", "CA", "L", "", "T", "und-CA", "_CA"},
        {"L", "en", "R", "FR", "L", "fr", "T", "fr-FR", "fr_FR"},
        {"L", "123", "X"},
        {"R", "us", "T", "und-US", "_US"},
        {"R", "usa", "X"},
        {"R", "123", "L", "it", "R", nullptr, "T", "it", "it"},
        {"R", "123", "L", "it", "R", "", "T", "it", "it"},
        {"R", "123", "L", "en", "T", "en-123", "en_123"},
        {"S", "LATN", "L", "DE", "T", "de-Latn", "de_Latn"},
        {"L", "De", "S", "latn", "R", "de", "S", "", "T", "de-DE", "de_DE"},
        {"L", "De", "S", "Arab", "R", "de", "S", nullptr, "T", "de-DE", "de_DE"},
        {"S", "latin", "X"},
        {"V", "1234", "L", "en", "T", "en-1234", "en__1234"},
        {"V", "1234", "L", "en", "V", "5678", "T", "en-5678", "en__5678"},
        {"V", "1234", "L", "en", "V", nullptr, "T", "en", "en"},
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
        {"U", "ja_JP@calendar=japanese;currency=JPY", "K", "ca", nullptr, "T",
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
        {"E", "z", "ExtZ", "L", "en", "E", "z", nullptr, "T", "en", "en"},
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
    LocaleBuilder bld;
    for (int tidx = 0; tidx < UPRV_LENGTHOF(TESTCASES); tidx++) {
        const char* (&testCase)[14] = TESTCASES[tidx];
        std::string actions;
        for (int p = 0; p < UPRV_LENGTHOF(testCase); p++) {
             if (testCase[p] == nullptr) {
                 actions += " (nullptr)";
                 break;
             }
             if (p > 0) actions += " ";
             actions += testCase[p];
        }
        int i = 0;
        const char* method;
        status = U_ZERO_ERROR;
        bld.clear();
        while (true) {
            status = U_ZERO_ERROR;
            UErrorCode copyStatus = U_ZERO_ERROR;
            method = testCase[i++];
            if (strcmp("L", method) == 0) {
                bld.setLanguage(testCase[i++]);
                bld.copyErrorTo(copyStatus);
                bld.build(status);
            } else if (strcmp("S", method) == 0) {
                bld.setScript(testCase[i++]);
                bld.copyErrorTo(copyStatus);
                bld.build(status);
            } else if (strcmp("R", method) == 0) {
                bld.setRegion(testCase[i++]);
                bld.copyErrorTo(copyStatus);
                bld.build(status);
            } else if (strcmp("V", method) == 0) {
                bld.setVariant(testCase[i++]);
                bld.copyErrorTo(copyStatus);
                bld.build(status);
            } else if (strcmp("K", method) == 0) {
                const char* key = testCase[i++];
                const char* type = testCase[i++];
                bld.setUnicodeLocaleKeyword(key, type);
                bld.copyErrorTo(copyStatus);
                bld.build(status);
            } else if (strcmp("A", method) == 0) {
                bld.addUnicodeLocaleAttribute(testCase[i++]);
                bld.copyErrorTo(copyStatus);
                bld.build(status);
            } else if (strcmp("E", method) == 0) {
                const char* key = testCase[i++];
                const char* value = testCase[i++];
                bld.setExtension(key[0], value);
                bld.copyErrorTo(copyStatus);
                bld.build(status);
            } else if (strcmp("P", method) == 0) {
                bld.setExtension('x', testCase[i++]);
                bld.copyErrorTo(copyStatus);
                bld.build(status);
            } else if (strcmp("U", method) == 0) {
                bld.setLocale(Locale(testCase[i++]));
                bld.copyErrorTo(copyStatus);
                bld.build(status);
            } else if (strcmp("B", method) == 0) {
                bld.setLanguageTag(testCase[i++]);
                bld.copyErrorTo(copyStatus);
                bld.build(status);
            }
            // clear / remove
            else if (strcmp("C", method) == 0) {
                bld.clear();
                bld.copyErrorTo(copyStatus);
                bld.build(status);
            } else if (strcmp("N", method) == 0) {
                bld.clearExtensions();
                bld.copyErrorTo(copyStatus);
                bld.build(status);
            } else if (strcmp("D", method) == 0) {
                bld.removeUnicodeLocaleAttribute(testCase[i++]);
                bld.copyErrorTo(copyStatus);
                bld.build(status);
            }
            // result
            else if (strcmp("X", method) == 0) {
                if (U_SUCCESS(status)) {
                    errln("FAIL: No error return - test case: %s", actions.c_str());
                }
            } else if (strcmp("T", method) == 0) {
                status = U_ZERO_ERROR;
                Locale loc = bld.build(status);
                if (status != copyStatus) {
                    errln("copyErrorTo not matching");
                }
                if (U_FAILURE(status) ||
                    strcmp(loc.getName(), testCase[i + 1]) != 0) {
                    errln("FAIL: Wrong locale ID - %s %s %s", loc.getName(),
                            " for test case: ", actions.c_str());
                }
                std::string langtag = loc.toLanguageTag<std::string>(status);
                if (U_FAILURE(status) || langtag != testCase[i]) {
                    errln("FAIL: Wrong language tag - %s %s %s", langtag.c_str(),
                            " for test case: ", actions.c_str());
                }
                break;
            } else {
                // Unknown test method
                errln("Unknown test case method: There is an error in the test case data.");
                break;
            }
            if (status != copyStatus) {
                errln("copyErrorTo not matching");
            }
            if (U_FAILURE(status)) {
                if (strcmp("X", testCase[i]) == 0) {
                    // This failure is expected
                    break;
                } else {
                    errln("FAIL: U_ILLEGAL_ARGUMENT_ERROR at offset %d %s %s", i,
                          " in test case: ", actions.c_str());
                    break;
                }
            }
            if (strcmp("T", method) == 0) {
                break;
            }
        }  // while(true)
    }  // for TESTCASES
}

void LocaleBuilderTest::TestLocaleBuilderBasic() {
    LocaleBuilder bld;
    bld.setLanguage("zh");
    Verify(bld, "zh", "setLanguage('zh') got Error: %s\n");

    bld.setScript("Hant");
    Verify(bld, "zh-Hant", "setScript('Hant') got Error: %s\n");

    bld.setRegion("SG");
    Verify(bld, "zh-Hant-SG", "setRegion('SG') got Error: %s\n");

    bld.setRegion("HK");
    bld.setScript("Hans");
    Verify(bld, "zh-Hans-HK",
           "setRegion('HK') and setScript('Hans') got Error: %s\n");

    bld.setVariant("revised");
    Verify(bld, "zh-Hans-HK-revised",
           "setVariant('revised') got Error: %s\n");

    bld.setUnicodeLocaleKeyword("nu", "thai");
    Verify(bld, "zh-Hans-HK-revised-u-nu-thai",
           "setUnicodeLocaleKeyword('nu', 'thai'') got Error: %s\n");

    bld.setUnicodeLocaleKeyword("co", "pinyin");
    Verify(bld, "zh-Hans-HK-revised-u-co-pinyin-nu-thai",
           "setUnicodeLocaleKeyword('co', 'pinyin'') got Error: %s\n");

    bld.setUnicodeLocaleKeyword("nu", "latn");
    Verify(bld, "zh-Hans-HK-revised-u-co-pinyin-nu-latn",
           "setUnicodeLocaleKeyword('nu', 'latn'') got Error: %s\n");

    bld.setUnicodeLocaleKeyword("nu", nullptr);
    Verify(bld, "zh-Hans-HK-revised-u-co-pinyin",
           "setUnicodeLocaleKeyword('nu', ''') got Error: %s\n");

    bld.setUnicodeLocaleKeyword("co", nullptr);
    Verify(bld, "zh-Hans-HK-revised",
           "setUnicodeLocaleKeyword('nu', nullptr) got Error: %s\n");

    bld.setScript("");
    Verify(bld, "zh-HK-revised",
           "setScript('') got Error: %s\n");

    bld.setVariant("");
    Verify(bld, "zh-HK",
           "setVariant('') got Error: %s\n");

    bld.setRegion("");
    Verify(bld, "zh",
           "setRegion('') got Error: %s\n");
}

void LocaleBuilderTest::TestLocaleBuilderBasicWithExtensionsOnDefaultLocale() {
    // Change the default locale to one with extension tags.
    UErrorCode status = U_ZERO_ERROR;
    Locale originalDefault;
    Locale::setDefault(Locale::createFromName("en-US-u-hc-h12"), status);
    if (U_FAILURE(status)) {
        errln("ERROR: Could not change the default locale");
        return;
    }

    // Invoke the basic test now that the default locale has been changed.
    TestLocaleBuilderBasic();

    Locale::setDefault(originalDefault, status);
    if (U_FAILURE(status)) {
        errln("ERROR: Could not restore the default locale");
    }
}

void LocaleBuilderTest::TestSetLanguageWellFormed() {
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
    for (const char* lang : wellFormedLanguages) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setLanguage(lang);
        Locale loc = bld.build(status);
        if (U_FAILURE(status)) {
            errln("setLanguage(\"%s\") got Error: %s\n",
                  lang, u_errorName(status));
        }
    }
}

void LocaleBuilderTest::TestSetLanguageIllFormed() {
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
    for (const char* ill : illFormed) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setLanguage(ill);
        Locale loc = bld.build(status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("setLanguage(\"%s\") should fail but has no Error\n", ill);
        }
    }
}

void LocaleBuilderTest::TestSetScriptWellFormed() {
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
    for (const char* script : wellFormedScripts) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setScript(script);
        Locale loc = bld.build(status);
        if (U_FAILURE(status)) {
            errln("setScript(\"%s\") got Error: %s\n",
                  script, u_errorName(status));
        }
    }
}

void LocaleBuilderTest::TestSetScriptIllFormed() {
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
    for (const char* ill : illFormed) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setScript(ill);
        Locale loc = bld.build(status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("setScript(\"%s\") should fail but has no Error\n", ill);
        }
    }
}

void LocaleBuilderTest::TestSetRegionWellFormed() {
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
    for (const char* region : wellFormedRegions) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setRegion(region);
        Locale loc = bld.build(status);
        if (U_FAILURE(status)) {
            errln("setRegion(\"%s\") got Error: %s\n",
                  region, u_errorName(status));
        }
    }
}

void LocaleBuilderTest::TestSetRegionIllFormed() {
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
    for (const char* ill : illFormed) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setRegion(ill);
        Locale loc = bld.build(status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("setRegion(\"%s\") should fail but has no Error\n", ill);
        }
    }
}

void LocaleBuilderTest::TestSetVariantWellFormed() {
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
    for (const char* variant : wellFormedVariants) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setVariant(variant);
        Locale loc = bld.build(status);
        if (U_FAILURE(status)) {
            errln("setVariant(\"%s\") got Error: %s\n",
                  variant, u_errorName(status));
        }
    }
}

void LocaleBuilderTest::TestSetVariantIllFormed() {
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
    for (const char* ill : illFormed) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setVariant(ill);
        Locale loc = bld.build(status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("setVariant(\"%s\") should fail but has no Error\n", ill);
        }
    }
}

void LocaleBuilderTest::TestSetUnicodeLocaleKeywordWellFormed() {
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
        LocaleBuilder bld;
        bld.setUnicodeLocaleKeyword(wellFormed_key_value[i],
                                    wellFormed_key_value[i + 1]);
        Locale loc = bld.build(status);
        if (U_FAILURE(status)) {
            errln("setUnicodeLocaleKeyword(\"%s\", \"%s\") got Error: %s\n",
                  wellFormed_key_value[i],
                  wellFormed_key_value[i + 1],
                  u_errorName(status));
        }
    }
}

void LocaleBuilderTest::TestSetUnicodeLocaleKeywordIllFormedKey() {
    static const char* illFormed[] = {
        "34",
        "ab-cde",
        "123",
        "b3",
        "zyzabcdef",
        "Z0",
    };
    for (const char* ill : illFormed) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setUnicodeLocaleKeyword(ill, "abc");
        Locale loc = bld.build(status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("setUnicodeLocaleKeyword(\"%s\", \"abc\") should fail but has no Error\n",
                  ill);
        }
    }
}

void LocaleBuilderTest::TestSetUnicodeLocaleKeywordIllFormedValue() {
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
    for (const char* ill : illFormed) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setUnicodeLocaleKeyword("ab", ill);
        Locale loc = bld.build(status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("setUnicodeLocaleKeyword(\"ab\", \"%s\") should fail but has no Error\n",
                  ill);
        }
    }
}

void LocaleBuilderTest::TestAddRemoveUnicodeLocaleAttribute() {
    LocaleBuilder bld;
    UErrorCode status = U_ZERO_ERROR;
    Locale loc = bld.setLanguage("fr")
                    .addUnicodeLocaleAttribute("abc")
                    .addUnicodeLocaleAttribute("aBc")
                    .addUnicodeLocaleAttribute("EFG")
                    .addUnicodeLocaleAttribute("efghi")
                    .addUnicodeLocaleAttribute("efgh")
                    .addUnicodeLocaleAttribute("efGhi")
                    .addUnicodeLocaleAttribute("EFg")
                    .addUnicodeLocaleAttribute("hijk")
                    .addUnicodeLocaleAttribute("EFG")
                    .addUnicodeLocaleAttribute("HiJK")
                    .addUnicodeLocaleAttribute("aBc")
                    .build(status);
    if (U_FAILURE(status)) {
        errln("addUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
    }
    std::string expected("fr-u-abc-efg-efgh-efghi-hijk");
    std::string actual = loc.toLanguageTag<std::string>(status);
    if (U_FAILURE(status) || expected != actual) {
        errln("Should get \"%s\" but get \"%s\"\n", expected.c_str(), actual.c_str());
    }

    // remove "efgh" in the middle with different casing.
    loc = bld.removeUnicodeLocaleAttribute("eFgH").build(status);
    if (U_FAILURE(status)) {
        errln("removeUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
    }
    expected = "fr-u-abc-efg-efghi-hijk";
    actual = loc.toLanguageTag<std::string>(status);
    if (U_FAILURE(status) || expected != actual) {
        errln("Should get \"%s\" but get \"%s\"\n", expected.c_str(), actual.c_str());
    }

    // remove non-existing attributes.
    loc = bld.removeUnicodeLocaleAttribute("efgh").build(status);
    if (U_FAILURE(status)) {
        errln("removeUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
    }
    actual = loc.toLanguageTag<std::string>(status);
    if (U_FAILURE(status) || expected != actual) {
        errln("Should get \"%s\" but get \"%s\"\n", expected.c_str(), actual.c_str());
    }

    // remove "abc" in the beginning with different casing.
    loc = bld.removeUnicodeLocaleAttribute("ABC").build(status);
    if (U_FAILURE(status)) {
        errln("removeUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
    }
    expected = "fr-u-efg-efghi-hijk";
    actual = loc.toLanguageTag<std::string>(status);
    if (U_FAILURE(status) || expected != actual) {
        errln("Should get \"%s\" but get \"%s\"\n", expected.c_str(), actual.c_str());
    }

    // remove non-existing substring in the end.
    loc = bld.removeUnicodeLocaleAttribute("hij").build(status);
    if (U_FAILURE(status)) {
        errln("removeUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
    }
    actual = loc.toLanguageTag<std::string>(status);
    if (U_FAILURE(status) || expected != actual) {
        errln("Should get \"%s\" but get \"%s\"\n", expected.c_str(), actual.c_str());
    }

    // remove "hijk" in the end with different casing.
    loc = bld.removeUnicodeLocaleAttribute("hIJK").build(status);
    if (U_FAILURE(status)) {
        errln("removeUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
    }
    expected = "fr-u-efg-efghi";
    actual = loc.toLanguageTag<std::string>(status);
    if (U_FAILURE(status) || expected != actual) {
        errln("Should get \"%s\" but get \"%s\"\n", expected.c_str(), actual.c_str());
    }

    // remove "efghi" in the end with different casing.
    loc = bld.removeUnicodeLocaleAttribute("EFGhi").build(status);
    if (U_FAILURE(status)) {
        errln("removeUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
    }
    expected = "fr-u-efg";
    actual = loc.toLanguageTag<std::string>(status);
    if (U_FAILURE(status) || expected != actual) {
        errln("Should get \"%s\" but get \"%s\"\n", expected.c_str(), actual.c_str());
    }

    // remove "efg" in as the only one, with different casing.
    loc = bld.removeUnicodeLocaleAttribute("EFG").build(status);
    if (U_FAILURE(status)) {
        errln("removeUnicodeLocaleAttribute() got Error: %s\n",
              u_errorName(status));
    }
    expected = "fr";
    actual = loc.toLanguageTag<std::string>(status);
    if (U_FAILURE(status) || expected != actual) {
        errln("Should get \"%s\" but get \"%s\"\n", expected.c_str(), actual.c_str());
    }

}

void LocaleBuilderTest::TestAddRemoveUnicodeLocaleAttributeWellFormed() {
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
    LocaleBuilder bld;
    for (int i = 0; i < UPRV_LENGTHOF(wellFormedAttributes); i++) {
        if (i % 5 == 0) {
            bld.clear();
        }
        UErrorCode status = U_ZERO_ERROR;
        bld.addUnicodeLocaleAttribute(wellFormedAttributes[i]);
        Locale loc = bld.build(status);
        if (U_FAILURE(status)) {
            errln("addUnicodeLocaleAttribute(\"%s\") got Error: %s\n",
                  wellFormedAttributes[i], u_errorName(status));
        }
        if (i > 2) {
            bld.removeUnicodeLocaleAttribute(wellFormedAttributes[i - 1]);
            loc = bld.build(status);
            if (U_FAILURE(status)) {
                errln("removeUnicodeLocaleAttribute(\"%s\") got Error: %s\n",
                      wellFormedAttributes[i - 1], u_errorName(status));
            }
            bld.removeUnicodeLocaleAttribute(wellFormedAttributes[i - 3]);
            loc = bld.build(status);
            if (U_FAILURE(status)) {
                errln("removeUnicodeLocaleAttribute(\"%s\") got Error: %s\n",
                      wellFormedAttributes[i - 3], u_errorName(status));
            }
        }
    }
}

void LocaleBuilderTest::TestAddUnicodeLocaleAttributeIllFormed() {
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
    for (const char* ill : illFormed) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.addUnicodeLocaleAttribute(ill);
        Locale loc = bld.build(status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("addUnicodeLocaleAttribute(\"%s\") should fail but has no Error\n",
                  ill);
        }
    }
}

void LocaleBuilderTest::TestSetExtensionU() {
    LocaleBuilder bld;
    bld.setLanguage("zh");
    Verify(bld, "zh",
           "setLanguage(\"zh\") got Error: %s\n");

    bld.setExtension('u', "co-stroke");
    Verify(bld, "zh-u-co-stroke",
           "setExtension('u', \"co-stroke\") got Error: %s\n");

    bld.setExtension('U', "ca-islamic");
    Verify(bld, "zh-u-ca-islamic",
           "setExtension('U', \"zh-u-ca-islamic\") got Error: %s\n");

    bld.setExtension('u', "ca-chinese");
    Verify(bld, "zh-u-ca-chinese",
           "setExtension('u', \"ca-chinese\") got Error: %s\n");

    bld.setExtension('U', "co-pinyin");
    Verify(bld, "zh-u-co-pinyin",
           "setExtension('U', \"co-pinyin\") got Error: %s\n");

    bld.setRegion("TW");
    Verify(bld, "zh-TW-u-co-pinyin",
           "setRegion(\"TW\") got Error: %s\n");

    bld.setExtension('U', "");
    Verify(bld, "zh-TW",
           "setExtension('U', \"\") got Error: %s\n");

    bld.setExtension('u', "abc-defg-kr-face");
    Verify(bld, "zh-TW-u-abc-defg-kr-face",
           "setExtension('u', \"abc-defg-kr-face\") got Error: %s\n");

    bld.setExtension('U', "ca-japanese");
    Verify(bld, "zh-TW-u-ca-japanese",
           "setExtension('U', \"ca-japanese\") got Error: %s\n");

}

void LocaleBuilderTest::TestSetExtensionValidateUWellFormed() {
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
    for (const char* extension : wellFormedExtensions) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setExtension('u', extension);
        Locale loc = bld.build(status);
        if (U_FAILURE(status)) {
            errln("setExtension('u', \"%s\") got Error: %s\n",
                  extension, u_errorName(status));
        }
    }
}

void LocaleBuilderTest::TestSetExtensionValidateUIllFormed() {
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
    for (const char* ill : illFormed) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setExtension('u', ill);
        Locale loc = bld.build(status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("setExtension('u', \"%s\") should fail but has no Error\n",
                  ill);
        }
    }
}

void LocaleBuilderTest::TestSetExtensionT() {
    LocaleBuilder bld;
    bld.setLanguage("fr");
    Verify(bld, "fr",
           "setLanguage(\"fr\") got Error: %s\n");

    bld.setExtension('T', "zh");
    Verify(bld, "fr-t-zh",
           "setExtension('T', \"zh\") got Error: %s\n");

    bld.setExtension('t', "zh-Hant-TW-1234-A9-123-456ABCDE");
    Verify(bld, "fr-t-zh-hant-tw-1234-a9-123-456abcde",
           "setExtension('t', \"zh-Hant-TW-1234-A9-123-456ABCDE\") got Error: %s\n");

    bld.setExtension('T', "a9-123");
    Verify(bld, "fr-t-a9-123",
           "setExtension('T', \"a9-123\") got Error: %s\n");

    bld.setRegion("MX");
    Verify(bld, "fr-MX-t-a9-123",
           "setRegion(\"MX\") got Error: %s\n");

    bld.setScript("Hans");
    Verify(bld, "fr-Hans-MX-t-a9-123",
           "setScript(\"Hans\") got Error: %s\n");

    bld.setVariant("9abc-abcde");
    Verify(bld, "fr-Hans-MX-9abc-abcde-t-a9-123",
           "setVariant(\"9abc-abcde\") got Error: %s\n");

    bld.setExtension('T', "");
    Verify(bld, "fr-Hans-MX-9abc-abcde",
           "bld.setExtension('T', \"\") got Error: %s\n");
}

void LocaleBuilderTest::TestSetExtensionValidateTWellFormed() {
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
    for (const char* extension : wellFormedExtensions) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setExtension('t', extension);
        Locale loc = bld.build(status);
        if (U_FAILURE(status)) {
            errln("setExtension('t', \"%s\") got Error: %s\n",
                  extension, u_errorName(status));
        }
    }
}

void LocaleBuilderTest::TestSetExtensionValidateTIllFormed() {
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
    for (const char* ill : illFormed) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setExtension('t', ill);
        Locale loc = bld.build(status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("setExtension('t', \"%s\") should fail but has no Error\n",
                  ill);
        }
    }
}

void LocaleBuilderTest::TestSetExtensionPU() {
    LocaleBuilder bld;
    bld.setLanguage("ar");
    Verify(bld, "ar",
           "setLanguage(\"ar\") got Error: %s\n");

    bld.setExtension('X', "a-b-c-d-e");
    Verify(bld, "ar-x-a-b-c-d-e",
           "setExtension('X', \"a-b-c-d-e\") got Error: %s\n");

    bld.setExtension('x', "0-1-2-3");
    Verify(bld, "ar-x-0-1-2-3",
           "setExtension('x', \"0-1-2-3\") got Error: %s\n");

    bld.setExtension('X', "0-12345678-x-x");
    Verify(bld, "ar-x-0-12345678-x-x",
           "setExtension('x', \"ar-x-0-12345678-x-x\") got Error: %s\n");

    bld.setRegion("TH");
    Verify(bld, "ar-TH-x-0-12345678-x-x",
           "setRegion(\"TH\") got Error: %s\n");

    bld.setExtension('X', "");
    Verify(bld, "ar-TH",
           "setExtension(\"X\") got Error: %s\n");
}

void LocaleBuilderTest::TestSetExtensionValidatePUWellFormed() {
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
    for (const char* extension : wellFormedExtensions) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setExtension('x', extension);
        Locale loc = bld.build(status);
        if (U_FAILURE(status)) {
            errln("setExtension('x', \"%s\") got Error: %s\n",
                  extension, u_errorName(status));
        }
    }
}

void LocaleBuilderTest::TestSetExtensionValidatePUIllFormed() {
    static const char* illFormed[] = {
        "123456789",  // Too long
        "abcdefghi",  // Too long
        "ab-123456789",  // Second subtag too long
        "abcdefghi-12",  // First subtag too long
        "a-ab-987654321",  // Third subtag too long
        "987654321-a-0-3",  // First subtag too long
    };
    for (const char* ill : illFormed) {
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setExtension('x', ill);
        Locale loc = bld.build(status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("setExtension('x', \"%s\") should fail but has no Error\n",
                  ill);
        }
    }
}

void LocaleBuilderTest::TestSetExtensionOthers() {
    LocaleBuilder bld;
    bld.setLanguage("fr");
    Verify(bld, "fr",
           "setLanguage(\"fr\") got Error: %s\n");

    bld.setExtension('Z', "ab");
    Verify(bld, "fr-z-ab",
           "setExtension('Z', \"ab\") got Error: %s\n");

    bld.setExtension('0', "xyz12345-abcdefg");
    Verify(bld, "fr-0-xyz12345-abcdefg-z-ab",
           "setExtension('0', \"xyz12345-abcdefg\") got Error: %s\n");

    bld.setExtension('a', "01-12345678-ABcdef");
    Verify(bld, "fr-0-xyz12345-abcdefg-a-01-12345678-abcdef-z-ab",
           "setExtension('a', \"01-12345678-ABcdef\") got Error: %s\n");

    bld.setRegion("TH");
    Verify(bld, "fr-TH-0-xyz12345-abcdefg-a-01-12345678-abcdef-z-ab",
           "setRegion(\"TH\") got Error: %s\n");

    bld.setScript("Arab");
    Verify(bld, "fr-Arab-TH-0-xyz12345-abcdefg-a-01-12345678-abcdef-z-ab",
           "setRegion(\"Arab\") got Error: %s\n");

    bld.setExtension('A', "97");
    Verify(bld, "fr-Arab-TH-0-xyz12345-abcdefg-a-97-z-ab",
           "setExtension('a', \"97\") got Error: %s\n");

    bld.setExtension('a', "");
    Verify(bld, "fr-Arab-TH-0-xyz12345-abcdefg-z-ab",
           "setExtension('a', \"\") got Error: %s\n");

    bld.setExtension('0', "");
    Verify(bld, "fr-Arab-TH-z-ab",
           "setExtension('0', \"\") got Error: %s\n");
}

void LocaleBuilderTest::TestSetExtensionValidateOthersWellFormed() {
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
    const int32_t aToZLen = static_cast<int32_t>(uprv_strlen(aToZ));
    int32_t i = 0;
    for (const char* extension : wellFormedExtensions) {
        char ch = aToZ[i];
        i = (i + 1) % aToZLen;
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setExtension(ch, extension);
        Locale loc = bld.build(status);
        if (U_FAILURE(status)) {
            errln("setExtension('%c', \"%s\") got Error: %s\n",
                  ch, extension, u_errorName(status));
        }
    }

    const char* someChars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789`~!@#$%^&*()-_=+;:,.<>?";
    const int32_t someCharsLen = static_cast<int32_t>(uprv_strlen(someChars));
    for (int32_t i = 0; i < someCharsLen; i++) {
        char ch = someChars[i];
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setExtension(ch, wellFormedExtensions[ch % UPRV_LENGTHOF(wellFormedExtensions)]);
        Locale loc = bld.build(status);
        if (uprv_isASCIILetter(ch) || ('0' <= ch && ch <= '9')) {
            if (ch != 't' && ch != 'T' && ch != 'u' && ch != 'U' && ch != 'x' && ch != 'X') {
                if (U_FAILURE(status)) {
                    errln("setExtension('%c', \"%s\") got Error: %s\n",
                          ch, wellFormedExtensions[ch % UPRV_LENGTHOF(wellFormedExtensions)], u_errorName(status));
                }
            }
        } else {
            if (status != U_ILLEGAL_ARGUMENT_ERROR) {
                errln("setExtension('%c', \"%s\") should fail but has no Error\n",
                      ch, wellFormedExtensions[ch % UPRV_LENGTHOF(wellFormedExtensions)]);
            }
        }

    }
}

void LocaleBuilderTest::TestSetExtensionValidateOthersIllFormed() {
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
    const int32_t aToZLen = static_cast<int32_t>(uprv_strlen(aToZ));
    int32_t i = 0;
    for (const char* ill : illFormed) {
        char ch = aToZ[i];
        i = (i + 1) % aToZLen;
        UErrorCode status = U_ZERO_ERROR;
        LocaleBuilder bld;
        bld.setExtension(ch, ill);
        Locale loc = bld.build(status);
        if (status != U_ILLEGAL_ARGUMENT_ERROR) {
            errln("setExtension('%c', \"%s\") should fail but has no Error\n",
                  ch, ill);
        }
    }
}

void LocaleBuilderTest::TestSetLocale() {
    LocaleBuilder bld1, bld2;
    UErrorCode status = U_ZERO_ERROR;
    Locale l1 = bld1.setLanguage("en")
        .setScript("Latn")
        .setRegion("MX")
        .setVariant("3456-abcde")
        .addUnicodeLocaleAttribute("456")
        .addUnicodeLocaleAttribute("123")
        .setUnicodeLocaleKeyword("nu", "thai")
        .setUnicodeLocaleKeyword("co", "stroke")
        .setUnicodeLocaleKeyword("ca", "chinese")
        .build(status);
    if (U_FAILURE(status) || l1.isBogus()) {
        errln("build got Error: %s\n", u_errorName(status));
    }
    status = U_ZERO_ERROR;
    Locale l2 = bld1.setLocale(l1).build(status);
    if (U_FAILURE(status) || l2.isBogus()) {
        errln("build got Error: %s\n", u_errorName(status));
    }

    if (l1 != l2) {
        errln("Two locales should be the same, but one is '%s' and the other is '%s'",
              l1.getName(), l2.getName());
    }
}

void LocaleBuilderTest::TestPosixCases() {
    UErrorCode status = U_ZERO_ERROR;
    Locale l1 = Locale::forLanguageTag("en-US-u-va-posix", status);
    if (U_FAILURE(status) || l1.isBogus()) {
        errln("build got Error: %s\n", u_errorName(status));
    }
    LocaleBuilder bld;
    bld.setLanguage("en")
        .setRegion("MX")
        .setScript("Arab")
        .setUnicodeLocaleKeyword("nu", "Thai")
        .setExtension('x', "1");
    // All of above should be cleared by the setLocale call.
    Locale l2 = bld.setLocale(l1).build(status);
    if (U_FAILURE(status) || l2.isBogus()) {
        errln("build got Error: %s\n", u_errorName(status));
    }
    if (l1 != l2) {
        errln("The result locale should be the set as the setLocale %s but got %s\n",
              l1.toLanguageTag<std::string>(status).c_str(),
              l2.toLanguageTag<std::string>(status).c_str());
    }
    Locale posix("en-US-POSIX");
    if (posix != l2) {
        errln("The result locale should be the set as %s but got %s\n",
              posix.getName(), l2.getName());
    }
}
