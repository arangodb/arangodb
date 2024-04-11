// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include <algorithm>
#include <functional>
#include <iterator>
#include <set>
#include <utility>
#include <cctype>

#include "loctest.h"
#include "unicode/localebuilder.h"
#include "unicode/localpointer.h"
#include "unicode/decimfmt.h"
#include "unicode/ucurr.h"
#include "unicode/smpdtfmt.h"
#include "unicode/strenum.h"
#include "unicode/dtfmtsym.h"
#include "unicode/brkiter.h"
#include "unicode/coll.h"
#include "unicode/ustring.h"
#include "unicode/std_string.h"
#include "charstr.h"
#include "cmemory.h"
#include "cstring.h"
#include <stdio.h>
#include <string.h>
#include "putilimp.h"
#include "hash.h"
#include "locmap.h"
#include "uparse.h"
#include "ulocimp.h"

static const char* const rawData[33][8] = {

        // language code
        {   "en",   "fr",   "ca",   "el",   "no",   "it",   "xx",   "zh"  },
        // script code
        {   "",     "",     "",     "",     "",     "",     "",     "Hans" },
        // country code
        {   "US",   "FR",   "ES",   "GR",   "NO",   "",     "YY",   "CN"  },
        // variant code
        {   "",     "",     "",     "",     "NY",   "",     "",   ""    },
        // full name
        {   "en_US",    "fr_FR",    "ca_ES",    "el_GR",    "no_NO_NY", "it",   "xx_YY",   "zh_Hans_CN" },
        // ISO-3 language
        {   "eng",  "fra",  "cat",  "ell",  "nor",  "ita",  "",   "zho"   },
        // ISO-3 country
        {   "USA",  "FRA",  "ESP",  "GRC",  "NOR",  "",     "",   "CHN"   },
        // LCID
        {   "409", "40c", "403", "408", "814", "10",     "0",   "804"  },

        // display language (English)
        {   "English",  "French",   "Catalan", "Greek",    "Norwegian",    "Italian",  "xx",   "Chinese" },
        // display script (English)
        {   "",     "",     "",     "",     "",   "",     "",   "Simplified Han" },
        // display country (English)
        {   "United States",    "France",   "Spain",  "Greece",   "Norway",   "",     "YY",   "China" },
        // display variant (English)
        {   "",     "",     "",     "",     "NY",   "",     "",   ""},
        // display name (English)
        // Updated no_NO_NY English display name for new pattern-based algorithm
        // (part of Euro support).
        {   "English (United States)", "French (France)", "Catalan (Spain)", "Greek (Greece)", "Norwegian (Norway, NY)", "Italian", "xx (YY)", "Chinese (Simplified, China)" },

        // display language (French)
        {   "anglais",  "fran\\u00E7ais",   "catalan", "grec",    "norv\\u00E9gien",    "italien", "xx", "chinois" },
        // display script (French)
        {   "",     "",     "",     "",     "",     "",     "",   "sinogrammes simplifi\\u00E9s" },
        // display country (French)
        {   "\\u00C9tats-Unis",    "France",   "Espagne",  "Gr\\u00E8ce",   "Norv\\u00E8ge", "", "YY", "Chine" },
        // display variant (French)
        {   "",     "",     "",     "",     "NY",     "",     "",   "" },
        // display name (French)
        //{   "anglais (Etats-Unis)", "francais (France)", "catalan (Espagne)", "grec (Grece)", "norvegien (Norvege,Nynorsk)", "italien", "xx (YY)" },
        {   "anglais (\\u00C9tats-Unis)", "fran\\u00E7ais (France)", "catalan (Espagne)", "grec (Gr\\u00E8ce)", "norv\\u00E9gien (Norv\\u00E8ge, NY)", "italien", "xx (YY)", "chinois (simplifi\\u00E9, Chine)" },


        /* display language (Catalan) */
        {   "angl\\u00E8s", "franc\\u00E8s", "catal\\u00E0", "grec",  "noruec", "itali\\u00E0", "", "xin\\u00E8s" },
        /* display script (Catalan) */
        {   "", "", "",                    "", "", "", "", "han simplificat" },
        /* display country (Catalan) */
        {   "Estats Units", "Fran\\u00E7a", "Espanya",  "Gr\\u00E8cia", "Noruega", "", "", "Xina" },
        /* display variant (Catalan) */
        {   "", "", "",                    "", "NY", "", "" },
        /* display name (Catalan) */
        {   "angl\\u00E8s (Estats Units)", "franc\\u00E8s (Fran\\u00E7a)", "catal\\u00E0 (Espanya)", "grec (Gr\\u00E8cia)", "noruec (Noruega, NY)", "itali\\u00E0", "", "xin\\u00E8s (simplificat, Xina)" },

        // display language (Greek)[actual values listed below]
        {   "\\u0391\\u03b3\\u03b3\\u03bb\\u03b9\\u03ba\\u03ac",
            "\\u0393\\u03b1\\u03bb\\u03bb\\u03b9\\u03ba\\u03ac",
            "\\u039a\\u03b1\\u03c4\\u03b1\\u03bb\\u03b1\\u03bd\\u03b9\\u03ba\\u03ac",
            "\\u0395\\u03bb\\u03bb\\u03b7\\u03bd\\u03b9\\u03ba\\u03ac",
            "\\u039d\\u03bf\\u03c1\\u03b2\\u03b7\\u03b3\\u03b9\\u03ba\\u03ac",
            "\\u0399\\u03c4\\u03b1\\u03bb\\u03b9\\u03ba\\u03ac",
            "",
            "\\u039A\\u03B9\\u03BD\\u03B5\\u03B6\\u03B9\\u03BA\\u03AC"
        },
        // display script (Greek)
        {   "", "", "", "", "", "", "", "\\u0391\\u03c0\\u03bb\\u03bf\\u03c0\\u03bf\\u03b9\\u03b7\\u03bc\\u03ad\\u03bd\\u03bf \\u03a7\\u03b1\\u03bd" },
        // display country (Greek)[actual values listed below]
        {   "\\u0397\\u03BD\\u03C9\\u03BC\\u03AD\\u03BD\\u03B5\\u03C2 \\u03A0\\u03BF\\u03BB\\u03B9\\u03C4\\u03B5\\u03AF\\u03B5\\u03C2",
            "\\u0393\\u03b1\\u03bb\\u03bb\\u03af\\u03b1",
            "\\u0399\\u03c3\\u03c0\\u03b1\\u03bd\\u03af\\u03b1",
            "\\u0395\\u03bb\\u03bb\\u03ac\\u03b4\\u03b1",
            "\\u039d\\u03bf\\u03c1\\u03b2\\u03b7\\u03b3\\u03af\\u03b1",
            "",
            "",
            "\\u039A\\u03AF\\u03BD\\u03B1"
        },
        // display variant (Greek)
        {   "", "", "", "", "NY", "", "" },
        // display name (Greek)[actual values listed below]
        {   "\\u0391\\u03b3\\u03b3\\u03bb\\u03b9\\u03ba\\u03ac (\\u0397\\u03BD\\u03C9\\u03BC\\u03AD\\u03BD\\u03B5\\u03C2 \\u03A0\\u03BF\\u03BB\\u03B9\\u03C4\\u03B5\\u03AF\\u03B5\\u03C2)",
            "\\u0393\\u03b1\\u03bb\\u03bb\\u03b9\\u03ba\\u03ac (\\u0393\\u03b1\\u03bb\\u03bb\\u03af\\u03b1)",
            "\\u039a\\u03b1\\u03c4\\u03b1\\u03bb\\u03b1\\u03bd\\u03b9\\u03ba\\u03ac (\\u0399\\u03c3\\u03c0\\u03b1\\u03bd\\u03af\\u03b1)",
            "\\u0395\\u03bb\\u03bb\\u03b7\\u03bd\\u03b9\\u03ba\\u03ac (\\u0395\\u03bb\\u03bb\\u03ac\\u03b4\\u03b1)",
            "\\u039d\\u03bf\\u03c1\\u03b2\\u03b7\\u03b3\\u03b9\\u03ba\\u03ac (\\u039d\\u03bf\\u03c1\\u03b2\\u03b7\\u03b3\\u03af\\u03b1, NY)",
            "\\u0399\\u03c4\\u03b1\\u03bb\\u03b9\\u03ba\\u03ac",
            "",
            "\\u039A\\u03B9\\u03BD\\u03B5\\u03B6\\u03B9\\u03BA\\u03AC (\\u0391\\u03c0\\u03bb\\u03bf\\u03c0\\u03bf\\u03b9\\u03b7\\u03bc\\u03ad\\u03bd\\u03bf, \\u039A\\u03AF\\u03BD\\u03B1)"
        },

        // display language (<root>)
        {   "English",  "French",   "Catalan", "Greek",    "Norwegian",    "Italian",  "xx", "" },
        // display script (<root>)
        {   "",     "",     "",     "",     "",   "",     "", ""},
        // display country (<root>)
        {   "United States",    "France",   "Spain",  "Greece",   "Norway",   "",     "YY", "" },
        // display variant (<root>)
        {   "",     "",     "",     "",     "Nynorsk",   "",     "", ""},
        // display name (<root>)
        //{   "English (United States)", "French (France)", "Catalan (Spain)", "Greek (Greece)", "Norwegian (Norway,Nynorsk)", "Italian", "xx (YY)" },
        {   "English (United States)", "French (France)", "Catalan (Spain)", "Greek (Greece)", "Norwegian (Norway,NY)", "Italian", "xx (YY)", "" }
};


/*
 Usage:
    test_assert(    Test (should be true)  )

   Example:
       test_assert(i==3);

   the macro is ugly but makes the tests pretty.
*/

#define test_assert(test) UPRV_BLOCK_MACRO_BEGIN { \
    if(!(test)) \
        errln("FAIL: " #test " was not true. In " __FILE__ " on line %d", __LINE__ ); \
    else \
        logln("PASS: asserted " #test); \
} UPRV_BLOCK_MACRO_END

/*
 Usage:
    test_assert_print(    Test (should be true),  printable  )

   Example:
       test_assert(i==3, toString(i));

   the macro is ugly but makes the tests pretty.
*/

#define test_assert_print(test,print) UPRV_BLOCK_MACRO_BEGIN { \
    if(!(test)) \
        errln("FAIL: " #test " was not true. " + UnicodeString(print) ); \
    else \
        logln("PASS: asserted " #test "-> " + UnicodeString(print)); \
} UPRV_BLOCK_MACRO_END


#define test_dumpLocale(l) UPRV_BLOCK_MACRO_BEGIN { \
    logln(#l " = " + UnicodeString(l.getName(), "")); \
} UPRV_BLOCK_MACRO_END

LocaleTest::LocaleTest()
: dataTable(nullptr)
{
    setUpDataTable();
}

LocaleTest::~LocaleTest()
{
    if (dataTable != nullptr) {
        for (int32_t i = 0; i < 33; i++) {
            delete []dataTable[i];
        }
        delete []dataTable;
        dataTable = nullptr;
    }
}

void LocaleTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestBug11421);         // Must run early in list to trigger failure.
    TESTCASE_AUTO(TestBasicGetters);
    TESTCASE_AUTO(TestVariantLengthLimit);
    TESTCASE_AUTO(TestSimpleResourceInfo);
    TESTCASE_AUTO(TestDisplayNames);
    TESTCASE_AUTO(TestSimpleObjectStuff);
    TESTCASE_AUTO(TestPOSIXParsing);
    TESTCASE_AUTO(TestGetAvailableLocales);
    TESTCASE_AUTO(TestDataDirectory);
    TESTCASE_AUTO(TestISO3Fallback);
    TESTCASE_AUTO(TestGetLangsAndCountries);
    TESTCASE_AUTO(TestSimpleDisplayNames);
    TESTCASE_AUTO(TestUninstalledISO3Names);
    TESTCASE_AUTO(TestAtypicalLocales);
#if !UCONFIG_NO_FORMATTING
    TESTCASE_AUTO(TestThaiCurrencyFormat);
    TESTCASE_AUTO(TestEuroSupport);
#endif
    TESTCASE_AUTO(TestToString);
#if !UCONFIG_NO_FORMATTING
    TESTCASE_AUTO(Test4139940);
    TESTCASE_AUTO(Test4143951);
#endif
    TESTCASE_AUTO(Test4147315);
    TESTCASE_AUTO(Test4147317);
    TESTCASE_AUTO(Test4147552);
    TESTCASE_AUTO(TestVariantParsing);
    TESTCASE_AUTO(Test20639_DeprecatesISO3Language);
#if !UCONFIG_NO_FORMATTING
    TESTCASE_AUTO(Test4105828);
#endif
    TESTCASE_AUTO(TestSetIsBogus);
    TESTCASE_AUTO(TestParallelAPIValues);
    TESTCASE_AUTO(TestAddLikelySubtags);
    TESTCASE_AUTO(TestMinimizeSubtags);
    TESTCASE_AUTO(TestAddLikelyAndMinimizeSubtags);
    TESTCASE_AUTO(TestDataDrivenLikelySubtags);
    TESTCASE_AUTO(TestKeywordVariants);
    TESTCASE_AUTO(TestCreateUnicodeKeywords);
    TESTCASE_AUTO(TestKeywordVariantParsing);
    TESTCASE_AUTO(TestCreateKeywordSet);
    TESTCASE_AUTO(TestCreateKeywordSetEmpty);
    TESTCASE_AUTO(TestCreateKeywordSetWithPrivateUse);
    TESTCASE_AUTO(TestCreateUnicodeKeywordSet);
    TESTCASE_AUTO(TestCreateUnicodeKeywordSetEmpty);
    TESTCASE_AUTO(TestCreateUnicodeKeywordSetWithPrivateUse);
    TESTCASE_AUTO(TestGetKeywordValueStdString);
    TESTCASE_AUTO(TestGetUnicodeKeywordValueStdString);
    TESTCASE_AUTO(TestSetKeywordValue);
    TESTCASE_AUTO(TestSetKeywordValueStringPiece);
    TESTCASE_AUTO(TestSetUnicodeKeywordValueStringPiece);
    TESTCASE_AUTO(TestGetBaseName);
#if !UCONFIG_NO_FILE_IO
    TESTCASE_AUTO(TestGetLocale);
#endif
    TESTCASE_AUTO(TestVariantWithOutCountry);
    TESTCASE_AUTO(TestCanonicalization);
    TESTCASE_AUTO(TestCurrencyByDate);
    TESTCASE_AUTO(TestGetVariantWithKeywords);
    TESTCASE_AUTO(TestIsRightToLeft);
    TESTCASE_AUTO(TestBug13277);
    TESTCASE_AUTO(TestBug13554);
    TESTCASE_AUTO(TestBug20410);
    TESTCASE_AUTO(TestBug20900);
    TESTCASE_AUTO(TestLocaleCanonicalizationFromFile);
    TESTCASE_AUTO(TestKnownCanonicalizedListCorrect);
    TESTCASE_AUTO(TestConstructorAcceptsBCP47);
    TESTCASE_AUTO(TestForLanguageTag);
    TESTCASE_AUTO(TestForLanguageTagLegacyTagBug21676);
    TESTCASE_AUTO(TestToLanguageTag);
    TESTCASE_AUTO(TestToLanguageTagOmitTrue);
    TESTCASE_AUTO(TestMoveAssign);
    TESTCASE_AUTO(TestMoveCtor);
    TESTCASE_AUTO(TestBug20407iVariantPreferredValue);
    TESTCASE_AUTO(TestBug13417VeryLongLanguageTag);
    TESTCASE_AUTO(TestBug11053UnderlineTimeZone);
    TESTCASE_AUTO(TestUnd);
    TESTCASE_AUTO(TestUndScript);
    TESTCASE_AUTO(TestUndRegion);
    TESTCASE_AUTO(TestUndCAPI);
    TESTCASE_AUTO(TestRangeIterator);
    TESTCASE_AUTO(TestPointerConvertingIterator);
    TESTCASE_AUTO(TestTagConvertingIterator);
    TESTCASE_AUTO(TestCapturingTagConvertingIterator);
    TESTCASE_AUTO(TestSetUnicodeKeywordValueInLongLocale);
    TESTCASE_AUTO(TestSetUnicodeKeywordValueNullInLongLocale);
    TESTCASE_AUTO(TestCanonicalize);
    TESTCASE_AUTO(TestLeak21419);
    TESTCASE_AUTO(TestNullDereferenceWrite21597);
    TESTCASE_AUTO(TestLongLocaleSetKeywordAssign);
    TESTCASE_AUTO(TestLongLocaleSetKeywordMoveAssign);
#if !UCONFIG_NO_FORMATTING
    TESTCASE_AUTO(TestSierraLeoneCurrency21997);
#endif
    TESTCASE_AUTO_END;
}

void LocaleTest::TestBasicGetters() {
    UnicodeString   temp;

    int32_t i;
    for (i = 0; i <= MAX_LOCALES; i++) {
        Locale testLocale("");
        if (rawData[SCRIPT][i] && rawData[SCRIPT][i][0] != 0) {
            testLocale = Locale(rawData[LANG][i], rawData[SCRIPT][i], rawData[CTRY][i], rawData[VAR][i]);
        }
        else {
            testLocale = Locale(rawData[LANG][i], rawData[CTRY][i], rawData[VAR][i]);
        }
        logln("Testing " + (UnicodeString)testLocale.getName() + "...");

        if ( (temp=testLocale.getLanguage()) != (dataTable[LANG][i]))
            errln("  Language code mismatch: " + temp + " versus "
                        + dataTable[LANG][i]);
        if ( (temp=testLocale.getScript()) != (dataTable[SCRIPT][i]))
            errln("  Script code mismatch: " + temp + " versus "
                        + dataTable[SCRIPT][i]);
        if ( (temp=testLocale.getCountry()) != (dataTable[CTRY][i]))
            errln("  Country code mismatch: " + temp + " versus "
                        + dataTable[CTRY][i]);
        if ( (temp=testLocale.getVariant()) != (dataTable[VAR][i]))
            errln("  Variant code mismatch: " + temp + " versus "
                        + dataTable[VAR][i]);
        if ( (temp=testLocale.getName()) != (dataTable[NAME][i]))
            errln("  Locale name mismatch: " + temp + " versus "
                        + dataTable[NAME][i]);
    }

    logln("Same thing without variant codes...");
    for (i = 0; i <= MAX_LOCALES; i++) {
        Locale testLocale("");
        if (rawData[SCRIPT][i] && rawData[SCRIPT][i][0] != 0) {
            testLocale = Locale(rawData[LANG][i], rawData[SCRIPT][i], rawData[CTRY][i]);
        }
        else {
            testLocale = Locale(rawData[LANG][i], rawData[CTRY][i]);
        }
        logln("Testing " + (temp=testLocale.getName()) + "...");

        if ( (temp=testLocale.getLanguage()) != (dataTable[LANG][i]))
            errln("Language code mismatch: " + temp + " versus "
                        + dataTable[LANG][i]);
        if ( (temp=testLocale.getScript()) != (dataTable[SCRIPT][i]))
            errln("Script code mismatch: " + temp + " versus "
                        + dataTable[SCRIPT][i]);
        if ( (temp=testLocale.getCountry()) != (dataTable[CTRY][i]))
            errln("Country code mismatch: " + temp + " versus "
                        + dataTable[CTRY][i]);
        if (testLocale.getVariant()[0] != 0)
            errln("Variant code mismatch: something versus \"\"");
    }

    logln("Testing long language names and getters");
    Locale  test8 = Locale::createFromName("x-klingon-latn-zx.utf32be@special");

    temp = test8.getLanguage();
    if (temp != UnicodeString("x-klingon") )
        errln("Language code mismatch: " + temp + "  versus \"x-klingon\"");

    temp = test8.getScript();
    if (temp != UnicodeString("Latn") )
        errln("Script code mismatch: " + temp + "  versus \"Latn\"");

    temp = test8.getCountry();
    if (temp != UnicodeString("ZX") )
        errln("Country code mismatch: " + temp + "  versus \"ZX\"");

    temp = test8.getVariant();
    //if (temp != UnicodeString("SPECIAL") )
    //    errln("Variant code mismatch: " + temp + "  versus \"SPECIAL\"");
    // As of 3.0, the "@special" will *not* be parsed by uloc_getName()
    if (temp != UnicodeString("") )
        errln("Variant code mismatch: " + temp + "  versus \"\"");

    if (Locale::getDefault() != Locale::createFromName(nullptr))
        errln("Locale::getDefault() == Locale::createFromName(nullptr)");

    /*----------*/
    // NOTE: There used to be a special test for locale names that had language or
    // country codes that were longer than two letters.  The new version of Locale
    // doesn't support anything that isn't an officially recognized language or
    // country code, so we no longer support this feature.

    Locale bogusLang("THISISABOGUSLANGUAGE"); // Jitterbug 2864: language code too long
    if(!bogusLang.isBogus()) {
        errln("Locale(\"THISISABOGUSLANGUAGE\").isBogus()==false");
    }

    bogusLang=Locale("eo");
    if( bogusLang.isBogus() ||
        strcmp(bogusLang.getLanguage(), "eo")!=0 ||
        *bogusLang.getCountry()!=0 ||
        *bogusLang.getVariant()!=0 ||
        strcmp(bogusLang.getName(), "eo")!=0
    ) {
        errln("assignment to bogus Locale does not unbogus it or sets bad data");
    }

    Locale a("eo_DE@currency=DEM");
    Locale *pb=a.clone();
    if(pb==&a || *pb!=a) {
        errln("Locale.clone() failed");
    }
    delete pb;
}

void LocaleTest::TestVariantLengthLimit() {
    static constexpr char valid[] =
        "_"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678";

    static constexpr char invalid[] =
        "_"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678"
        "_12345678X";  // One character too long.

    constexpr const char* variantsExpected = valid + 2;  // Skip initial "__".

    Locale validLocale(valid);
    if (validLocale.isBogus()) {
        errln("Valid locale is unexpectedly bogus.");
    } else if (uprv_strcmp(variantsExpected, validLocale.getVariant()) != 0) {
        errln("Expected variants \"%s\" but got variants \"%s\"\n",
              variantsExpected, validLocale.getVariant());
    }

    Locale invalidLocale(invalid);
    if (!invalidLocale.isBogus()) {
        errln("Invalid locale is unexpectedly NOT bogus.");
    }
}

void LocaleTest::TestParallelAPIValues() {
    logln("Test synchronization between C and C++ API");
    if (strcmp(Locale::getChinese().getName(), ULOC_CHINESE) != 0) {
        errln("Differences for ULOC_CHINESE Locale");
    }
    if (strcmp(Locale::getEnglish().getName(), ULOC_ENGLISH) != 0) {
        errln("Differences for ULOC_ENGLISH Locale");
    }
    if (strcmp(Locale::getFrench().getName(), ULOC_FRENCH) != 0) {
        errln("Differences for ULOC_FRENCH Locale");
    }
    if (strcmp(Locale::getGerman().getName(), ULOC_GERMAN) != 0) {
        errln("Differences for ULOC_GERMAN Locale");
    }
    if (strcmp(Locale::getItalian().getName(), ULOC_ITALIAN) != 0) {
        errln("Differences for ULOC_ITALIAN Locale");
    }
    if (strcmp(Locale::getJapanese().getName(), ULOC_JAPANESE) != 0) {
        errln("Differences for ULOC_JAPANESE Locale");
    }
    if (strcmp(Locale::getKorean().getName(), ULOC_KOREAN) != 0) {
        errln("Differences for ULOC_KOREAN Locale");
    }
    if (strcmp(Locale::getSimplifiedChinese().getName(), ULOC_SIMPLIFIED_CHINESE) != 0) {
        errln("Differences for ULOC_SIMPLIFIED_CHINESE Locale");
    }
    if (strcmp(Locale::getTraditionalChinese().getName(), ULOC_TRADITIONAL_CHINESE) != 0) {
        errln("Differences for ULOC_TRADITIONAL_CHINESE Locale");
    }


    if (strcmp(Locale::getCanada().getName(), ULOC_CANADA) != 0) {
        errln("Differences for ULOC_CANADA Locale");
    }
    if (strcmp(Locale::getCanadaFrench().getName(), ULOC_CANADA_FRENCH) != 0) {
        errln("Differences for ULOC_CANADA_FRENCH Locale");
    }
    if (strcmp(Locale::getChina().getName(), ULOC_CHINA) != 0) {
        errln("Differences for ULOC_CHINA Locale");
    }
    if (strcmp(Locale::getPRC().getName(), ULOC_PRC) != 0) {
        errln("Differences for ULOC_PRC Locale");
    }
    if (strcmp(Locale::getFrance().getName(), ULOC_FRANCE) != 0) {
        errln("Differences for ULOC_FRANCE Locale");
    }
    if (strcmp(Locale::getGermany().getName(), ULOC_GERMANY) != 0) {
        errln("Differences for ULOC_GERMANY Locale");
    }
    if (strcmp(Locale::getItaly().getName(), ULOC_ITALY) != 0) {
        errln("Differences for ULOC_ITALY Locale");
    }
    if (strcmp(Locale::getJapan().getName(), ULOC_JAPAN) != 0) {
        errln("Differences for ULOC_JAPAN Locale");
    }
    if (strcmp(Locale::getKorea().getName(), ULOC_KOREA) != 0) {
        errln("Differences for ULOC_KOREA Locale");
    }
    if (strcmp(Locale::getTaiwan().getName(), ULOC_TAIWAN) != 0) {
        errln("Differences for ULOC_TAIWAN Locale");
    }
    if (strcmp(Locale::getUK().getName(), ULOC_UK) != 0) {
        errln("Differences for ULOC_UK Locale");
    }
    if (strcmp(Locale::getUS().getName(), ULOC_US) != 0) {
        errln("Differences for ULOC_US Locale");
    }
}


void LocaleTest::TestSimpleResourceInfo() {
    UnicodeString   temp;
    char            temp2[20];
    UErrorCode err = U_ZERO_ERROR;
    int32_t i = 0;

    for (i = 0; i <= MAX_LOCALES; i++) {
        Locale testLocale(rawData[LANG][i], rawData[CTRY][i], rawData[VAR][i]);
        logln("Testing " + (temp=testLocale.getName()) + "...");

        if ( (temp=testLocale.getISO3Language()) != (dataTable[LANG3][i]))
            errln("  ISO-3 language code mismatch: " + temp
                + " versus " + dataTable[LANG3][i]);
        if ( (temp=testLocale.getISO3Country()) != (dataTable[CTRY3][i]))
            errln("  ISO-3 country code mismatch: " + temp
                + " versus " + dataTable[CTRY3][i]);

        snprintf(temp2, sizeof(temp2), "%x", (int)testLocale.getLCID());
        if (UnicodeString(temp2) != dataTable[LCID][i])
            errln((UnicodeString)"  LCID mismatch: " + temp2 + " versus "
                + dataTable[LCID][i]);

        if(U_FAILURE(err))
        {
            errln((UnicodeString)"Some error on number " + i + u_errorName(err));
        }
        err = U_ZERO_ERROR;
    }

    Locale locale("en");
    if(strcmp(locale.getName(), "en") != 0||
        strcmp(locale.getLanguage(), "en") != 0) {
        errln("construction of Locale(en) failed\n");
    }
    /*-----*/

}

/*
 * Jitterbug 2439 -- markus 20030425
 *
 * The lookup of display names must not fall back through the default
 * locale because that yields useless results.
 */
void
LocaleTest::TestDisplayNames()
{
    Locale  english("en", "US");
    Locale  french("fr", "FR");
    Locale  croatian("ca", "ES");
    Locale  greek("el", "GR");

    logln("  In locale = en_US...");
    doTestDisplayNames(english, DLANG_EN);
    logln("  In locale = fr_FR...");
    doTestDisplayNames(french, DLANG_FR);
    logln("  In locale = ca_ES...");
    doTestDisplayNames(croatian, DLANG_CA);
    logln("  In locale = el_GR...");
    doTestDisplayNames(greek, DLANG_EL);

    UnicodeString s;
    UErrorCode status = U_ZERO_ERROR;

#if !UCONFIG_NO_FORMATTING
    DecimalFormatSymbols symb(status);
    /* Check to see if ICU supports this locale */
    if (symb.getLocale(ULOC_VALID_LOCALE, status) != Locale("root")) {
        /* test that the default locale has a display name for its own language */
        /* Currently, there is no language information in the "tl" data file so this test will fail if default locale is "tl" */
        if (uprv_strcmp(Locale().getLanguage(), "tl") != 0) {
            Locale().getDisplayLanguage(Locale(), s);
            if(s.length()<=3 && s.charAt(0)<=0x7f) {
                /* check <=3 to reject getting the language code as a display name */
                dataerrln("unable to get a display string for the language of the default locale: " + s);
            }

            /*
             * API coverage improvements: call
             * Locale::getDisplayLanguage(UnicodeString &) and
             * Locale::getDisplayCountry(UnicodeString &)
             */
            s.remove();
            Locale().getDisplayLanguage(s);
            if(s.length()<=3 && s.charAt(0)<=0x7f) {
                dataerrln("unable to get a display string for the language of the default locale [2]: " + s);
            }
        }
    }
    else {
        logln("Default locale %s is unsupported by ICU\n", Locale().getName());
    }
    s.remove();
#endif

    french.getDisplayCountry(s);
    if(s.isEmpty()) {
        errln("unable to get any default-locale display string for the country of fr_FR\n");
    }
    s.remove();
    Locale("zh", "Hant").getDisplayScript(s);
    if(s.isEmpty()) {
        errln("unable to get any default-locale display string for the country of zh_Hant\n");
    }
}

void LocaleTest::TestSimpleObjectStuff() {
    Locale  test1("aa", "AA");
    Locale  test2("aa", "AA");
    Locale  test3(test1);
    Locale  test4("zz", "ZZ");
    Locale  test5("aa", "AA", "");
    Locale  test6("aa", "AA", "ANTARES");
    Locale  test7("aa", "AA", "JUPITER");
    Locale  test8 = Locale::createFromName("aa-aa-jupiTER"); // was "aa-aa.utf8@jupiter" but in 3.0 getName won't normalize that

    // now list them all for debugging usage.
    test_dumpLocale(test1);
    test_dumpLocale(test2);
    test_dumpLocale(test3);
    test_dumpLocale(test4);
    test_dumpLocale(test5);
    test_dumpLocale(test6);
    test_dumpLocale(test7);
    test_dumpLocale(test8);

    // Make sure things compare to themselves!
    test_assert(test1 == test1);
    test_assert(test2 == test2);
    test_assert(test3 == test3);
    test_assert(test4 == test4);
    test_assert(test5 == test5);
    test_assert(test6 == test6);
    test_assert(test7 == test7);
    test_assert(test8 == test8);

    // make sure things are not equal to themselves.
    test_assert(!(test1 != test1));
    test_assert(!(test2 != test2));
    test_assert(!(test3 != test3));
    test_assert(!(test4 != test4));
    test_assert(!(test5 != test5));
    test_assert(!(test6 != test6));
    test_assert(!(test7 != test7));
    test_assert(!(test8 != test8));

    // make sure things that are equal to each other don't show up as unequal.
    test_assert(!(test1 != test2));
    test_assert(!(test2 != test1));
    test_assert(!(test1 != test3));
    test_assert(!(test2 != test3));
    test_assert(test5 == test1);
    test_assert(test6 != test2);
    test_assert(test6 != test5);

    test_assert(test6 != test7);

    // test for things that shouldn't compare equal.
    test_assert(!(test1 == test4));
    test_assert(!(test2 == test4));
    test_assert(!(test3 == test4));

    test_assert(test7 == test8);

    // test for hash codes to be the same.
    int32_t hash1 = test1.hashCode();
    int32_t hash2 = test2.hashCode();
    int32_t hash3 = test3.hashCode();

    test_assert(hash1 == hash2);
    test_assert(hash1 == hash3);
    test_assert(hash2 == hash3);

    // test that the assignment operator works.
    test4 = test1;
    logln("test4=test1;");
    test_dumpLocale(test4);
    test_assert(test4 == test4);

    test_assert(!(test1 != test4));
    test_assert(!(test2 != test4));
    test_assert(!(test3 != test4));
    test_assert(test1 == test4);
    test_assert(test4 == test1);

    // test assignments with a variant
    logln("test7 = test6");
    test7 = test6;
    test_dumpLocale(test7);
    test_assert(test7 == test7);
    test_assert(test7 == test6);
    test_assert(test7 != test5);

    logln("test6 = test1");
    test6=test1;
    test_dumpLocale(test6);
    test_assert(test6 != test7);
    test_assert(test6 == test1);
    test_assert(test6 == test6);
}

// A class which exposes constructors that are implemented in terms of the POSIX parsing code.
class POSIXLocale : public Locale
{
public:
    POSIXLocale(const UnicodeString& l)
        :Locale()
    {
      char *ch;
      ch = new char[l.length() + 1];
      ch[l.extract(0, 0x7fffffff, ch, "")] = 0;
      setFromPOSIXID(ch);
      delete [] ch;
    }
    POSIXLocale(const char *l)
        :Locale()
    {
        setFromPOSIXID(l);
    }
};

void LocaleTest::TestPOSIXParsing()
{
    POSIXLocale  test1("ab_AB");
    POSIXLocale  test2(UnicodeString("ab_AB"));
    Locale  test3("ab","AB");

    POSIXLocale test4("ab_AB_Antares");
    POSIXLocale test5(UnicodeString("ab_AB_Antares"));
    Locale  test6("ab", "AB", "Antares");

    test_dumpLocale(test1);
    test_dumpLocale(test2);
    test_dumpLocale(test3);
    test_dumpLocale(test4);
    test_dumpLocale(test5);
    test_dumpLocale(test6);

    test_assert(test1 == test1);

    test_assert(test1 == test2);
    test_assert(test2 == test3);
    test_assert(test3 == test1);

    test_assert(test4 == test5);
    test_assert(test5 == test6);
    test_assert(test6 == test4);

    test_assert(test1 != test4);
    test_assert(test5 != test3);
    test_assert(test5 != test2);

    int32_t hash1 = test1.hashCode();
    int32_t hash2 = test2.hashCode();
    int32_t hash3 = test3.hashCode();

    test_assert(hash1 == hash2);
    test_assert(hash2 == hash3);
    test_assert(hash3 == hash1);
}

void LocaleTest::TestGetAvailableLocales()
{
    int32_t locCount = 0;
    const Locale* locList = Locale::getAvailableLocales(locCount);

    if (locCount == 0)
        dataerrln("getAvailableLocales() returned an empty list!");
    else {
        logln(UnicodeString("Number of locales returned = ") + locCount);
        UnicodeString temp;
        for(int32_t i = 0; i < locCount; ++i)
            logln(locList[i].getName());
    }
    // I have no idea how to test this function...
}

// This test isn't applicable anymore - getISO3Language is
// independent of the data directory
void LocaleTest::TestDataDirectory()
{
/*
    char            oldDirectory[80];
    const char*     temp;
    UErrorCode       err = U_ZERO_ERROR;
    UnicodeString   testValue;

    temp = Locale::getDataDirectory();
    strcpy(oldDirectory, temp);
    logln(UnicodeString("oldDirectory = ") + oldDirectory);

    Locale  test(Locale::US);
    test.getISO3Language(testValue);
    logln("first fetch of language retrieved " + testValue);
    if (testValue != "eng")
        errln("Initial check of ISO3 language failed: expected \"eng\", got \"" + testValue + "\"");

    {
        char *path;
        path=IntlTest::getTestDirectory();
        Locale::setDataDirectory( path );
    }

    test.getISO3Language(testValue);
    logln("second fetch of language retrieved " + testValue);
    if (testValue != "xxx")
        errln("setDataDirectory() failed: expected \"xxx\", got \"" + testValue + "\"");

    Locale::setDataDirectory(oldDirectory);
    test.getISO3Language(testValue);
    logln("third fetch of language retrieved " + testValue);
    if (testValue != "eng")
        errln("get/setDataDirectory() failed: expected \"eng\", got \"" + testValue + "\"");
*/
}

//===========================================================

void LocaleTest::doTestDisplayNames(Locale& displayLocale, int32_t compareIndex) {
    UnicodeString   temp;

    for (int32_t i = 0; i <= MAX_LOCALES; i++) {
        Locale testLocale("");
        if (rawData[SCRIPT][i] && rawData[SCRIPT][i][0] != 0) {
            testLocale = Locale(rawData[LANG][i], rawData[SCRIPT][i], rawData[CTRY][i], rawData[VAR][i]);
        }
        else {
            testLocale = Locale(rawData[LANG][i], rawData[CTRY][i], rawData[VAR][i]);
        }
        logln("  Testing " + (temp=testLocale.getName()) + "...");

        UnicodeString  testLang;
        UnicodeString  testScript;
        UnicodeString  testCtry;
        UnicodeString  testVar;
        UnicodeString  testName;

        testLocale.getDisplayLanguage(displayLocale, testLang);
        testLocale.getDisplayScript(displayLocale, testScript);
        testLocale.getDisplayCountry(displayLocale, testCtry);
        testLocale.getDisplayVariant(displayLocale, testVar);
        testLocale.getDisplayName(displayLocale, testName);

        UnicodeString  expectedLang;
        UnicodeString  expectedScript;
        UnicodeString  expectedCtry;
        UnicodeString  expectedVar;
        UnicodeString  expectedName;

        expectedLang = dataTable[compareIndex][i];
        if (expectedLang.length() == 0)
            expectedLang = dataTable[DLANG_EN][i];

        expectedScript = dataTable[compareIndex + 1][i];
        if (expectedScript.length() == 0)
            expectedScript = dataTable[DSCRIPT_EN][i];

        expectedCtry = dataTable[compareIndex + 2][i];
        if (expectedCtry.length() == 0)
            expectedCtry = dataTable[DCTRY_EN][i];

        expectedVar = dataTable[compareIndex + 3][i];
        if (expectedVar.length() == 0)
            expectedVar = dataTable[DVAR_EN][i];

        expectedName = dataTable[compareIndex + 4][i];
        if (expectedName.length() == 0)
            expectedName = dataTable[DNAME_EN][i];

        if (testLang != expectedLang)
            dataerrln("Display language (" + UnicodeString(displayLocale.getName()) + ") of (" + UnicodeString(testLocale.getName()) + ") got " + testLang + " expected " + expectedLang);
        if (testScript != expectedScript)
            dataerrln("Display script (" + UnicodeString(displayLocale.getName()) + ") of (" + UnicodeString(testLocale.getName()) + ") got " + testScript + " expected " + expectedScript);
        if (testCtry != expectedCtry)
            dataerrln("Display country (" + UnicodeString(displayLocale.getName()) + ") of (" + UnicodeString(testLocale.getName()) + ") got " + testCtry + " expected " + expectedCtry);
        if (testVar != expectedVar)
            dataerrln("Display variant (" + UnicodeString(displayLocale.getName()) + ") of (" + UnicodeString(testLocale.getName()) + ") got " + testVar + " expected " + expectedVar);
        if (testName != expectedName)
            dataerrln("Display name (" + UnicodeString(displayLocale.getName()) + ") of (" + UnicodeString(testLocale.getName()) + ") got " + testName + " expected " + expectedName);
    }
}

//---------------------------------------------------
// table of valid data
//---------------------------------------------------



void LocaleTest::setUpDataTable()
{
    if (dataTable == nullptr) {
        dataTable = new UnicodeString*[33];

        for (int32_t i = 0; i < 33; i++) {
            dataTable[i] = new UnicodeString[8];
            for (int32_t j = 0; j < 8; j++) {
                dataTable[i][j] = CharsToUnicodeString(rawData[i][j]);
            }
        }
    }
}

// ====================


/**
 * @bug 4011756 4011380
 */
void
LocaleTest::TestISO3Fallback()
{
    Locale test("xx", "YY");

    const char * result;

    result = test.getISO3Language();

    // Conform to C API usage

    if (!result || (result[0] != 0))
        errln("getISO3Language() on xx_YY returned " + UnicodeString(result) + " instead of \"\"");

    result = test.getISO3Country();

    if (!result || (result[0] != 0))
        errln("getISO3Country() on xx_YY returned " + UnicodeString(result) + " instead of \"\"");
}

/**
 * @bug 4106155 4118587
 */
void
LocaleTest::TestGetLangsAndCountries()
{
    // It didn't seem right to just do an exhaustive test of everything here, so I check
    // for the following things:
    // 1) Does each list have the right total number of entries?
    // 2) Does each list contain certain language and country codes we think are important
    //     (the G7 countries, plus a couple others)?
    // 3) Does each list have every entry formatted correctly? (i.e., two characters,
    //     all lower case for the language codes, all upper case for the country codes)
    // 4) Is each list in sorted order?
    int32_t testCount = 0;
    const char * const * test = Locale::getISOLanguages();
    const char spotCheck1[ ][4] = { "en", "es", "fr", "de", "it",
                                    "ja", "ko", "zh", "th", "he",
                                    "id", "iu", "ug", "yi", "za" };

    int32_t i;

    for(testCount = 0;test[testCount];testCount++)
      ;

    /* TODO: Change this test to be more like the cloctst version? */
    if (testCount != 601)
        errln("Expected getISOLanguages() to return 601 languages; it returned %d", testCount);
    else {
        for (i = 0; i < 15; i++) {
            int32_t j;
            for (j = 0; j < testCount; j++)
              if (uprv_strcmp(test[j],spotCheck1[i])== 0)
                    break;
            if (j == testCount || (uprv_strcmp(test[j],spotCheck1[i])!=0))
                errln("Couldn't find " + (UnicodeString)spotCheck1[i] + " in language list.");
        }
    }
    for (i = 0; i < testCount; i++) {
        UnicodeString testee(test[i],"");
        UnicodeString lc(test[i],"");
        if (testee != lc.toLower())
            errln(lc + " is not all lower case.");
        if ( (testee.length() != 2) && (testee.length() != 3))
            errln(testee + " is not two or three characters long.");
        if (i > 0 && testee.compare(test[i - 1]) <= 0)
            errln(testee + " appears in an out-of-order position in the list.");
    }

    test = Locale::getISOCountries();
    UnicodeString spotCheck2 [] = { "US", "CA", "GB", "FR", "DE",
                                    "IT", "JP", "KR", "CN", "TW",
                                    "TH" };
    int32_t spot2Len = 11;
    for(testCount=0;test[testCount];testCount++)
      ;

    if (testCount != 254){
        errln("Expected getISOCountries to return 254 countries; it returned %d", testCount);
    }else {
        for (i = 0; i < spot2Len; i++) {
            int32_t j;
            for (j = 0; j < testCount; j++)
              {
                UnicodeString testee(test[j],"");

                if (testee == spotCheck2[i])
                    break;
              }
                UnicodeString testee(test[j],"");
            if (j == testCount || testee != spotCheck2[i])
                errln("Couldn't find " + spotCheck2[i] + " in country list.");
        }
    }
    for (i = 0; i < testCount; i++) {
        UnicodeString testee(test[i],"");
        UnicodeString uc(test[i],"");
        if (testee != uc.toUpper())
            errln(testee + " is not all upper case.");
        if (testee.length() != 2)
            errln(testee + " is not two characters long.");
        if (i > 0 && testee.compare(test[i - 1]) <= 0)
            errln(testee + " appears in an out-of-order position in the list.");
    }

    // This getAvailableLocales and getISO3Language
    {
        int32_t numOfLocales;
        Locale  enLoc ("en");
        const Locale *pLocales = Locale::getAvailableLocales(numOfLocales);

        for (int i = 0; i < numOfLocales; i++) {
            const Locale    &loc(pLocales[i]);
            UnicodeString   name;
            char        szName[200];

            loc.getDisplayName (enLoc, name);
            name.extract (0, 200, szName, sizeof(szName));

            if (strlen(loc.getISO3Language()) == 0) {
                errln("getISO3Language() returned an empty string for: " + name);
            }
        }
    }
}

/**
 * @bug 4118587
 */
void
LocaleTest::TestSimpleDisplayNames()
{
    // This test is different from TestDisplayNames because TestDisplayNames checks
    // fallback behavior, combination of language and country names to form locale
    // names, and other stuff like that.  This test just checks specific language
    // and country codes to make sure we have the correct names for them.
    char languageCodes[] [4] = { "he", "id", "iu", "ug", "yi", "za" };
    UnicodeString languageNames [] = { "Hebrew", "Indonesian", "Inuktitut", "Uyghur", "Yiddish",
                               "Zhuang" };

    for (int32_t i = 0; i < 6; i++) {
        UnicodeString test;
        Locale l(languageCodes[i], "", "");
        l.getDisplayLanguage(Locale::getUS(), test);
        if (test != languageNames[i])
            dataerrln("Got wrong display name for " + UnicodeString(languageCodes[i]) + ": Expected \"" +
                  languageNames[i] + "\", got \"" + test + "\".");
    }
}

/**
 * @bug 4118595
 */
void
LocaleTest::TestUninstalledISO3Names()
{
    // This test checks to make sure getISO3Language and getISO3Country work right
    // even for locales that are not installed.
    const char iso2Languages [][4] = {     "am", "ba", "fy", "mr", "rn",
                                        "ss", "tw", "zu" };
    const char iso3Languages [][5] = {     "amh", "bak", "fry", "mar", "run",
                                        "ssw", "twi", "zul" };

    int32_t i;

    for (i = 0; i < 8; i++) {
      UErrorCode err = U_ZERO_ERROR;

      UnicodeString test;
        Locale l(iso2Languages[i], "", "");
        test = l.getISO3Language();
        if((test != iso3Languages[i]) || U_FAILURE(err))
            errln("Got wrong ISO3 code for " + UnicodeString(iso2Languages[i]) + ": Expected \"" +
                    iso3Languages[i] + "\", got \"" + test + "\"." + UnicodeString(u_errorName(err)));
    }

    char iso2Countries [][4] = {     "AF", "BW", "KZ", "MO", "MN",
                                        "SB", "TC", "ZW" };
    char iso3Countries [][4] = {     "AFG", "BWA", "KAZ", "MAC", "MNG",
                                        "SLB", "TCA", "ZWE" };

    for (i = 0; i < 8; i++) {
      UErrorCode err = U_ZERO_ERROR;
        Locale l("", iso2Countries[i], "");
        UnicodeString test(l.getISO3Country(), "");
        if (test != iso3Countries[i])
            errln("Got wrong ISO3 code for " + UnicodeString(iso2Countries[i]) + ": Expected \"" +
                    UnicodeString(iso3Countries[i]) + "\", got \"" + test + "\"." + u_errorName(err));
    }
}

/**
 * @bug 4092475
 * I could not reproduce this bug.  I'm pretty convinced it was fixed with the
 * big locale-data reorg of 10/28/97.  The lookup logic for language and country
 * display names was also changed at that time in that check-in.    --rtg 3/20/98
 */
void
LocaleTest::TestAtypicalLocales()
{
    Locale localesToTest [] = { Locale("de", "CA"),
                                  Locale("ja", "ZA"),
                                   Locale("ru", "MX"),
                                   Locale("en", "FR"),
                                   Locale("es", "DE"),
                                   Locale("", "HR"),
                                   Locale("", "SE"),
                                   Locale("", "DO"),
                                   Locale("", "BE") };

    UnicodeString englishDisplayNames [] = { "German (Canada)",
                                     "Japanese (South Africa)",
                                     "Russian (Mexico)",
                                     "English (France)",
                                     "Spanish (Germany)",
                                     "Unknown language (Croatia)",
                                     "Unknown language (Sweden)",
                                     "Unknown language (Dominican Republic)",
                                     "Unknown language (Belgium)" };
    UnicodeString frenchDisplayNames []= { "allemand (Canada)",
                                     "japonais (Afrique du Sud)",
                                     "russe (Mexique)",
                                     "anglais (France)",
                                     "espagnol (Allemagne)",
                                     u"langue indéterminée (Croatie)",
                                     u"langue indéterminée (Suède)",
                                     u"langue indéterminée (République dominicaine)",
                                     u"langue indéterminée (Belgique)" };
    UnicodeString spanishDisplayNames [] = {
                                     u"alemán (Canadá)",
                                     u"japonés (Sudáfrica)",
                                     u"ruso (México)",
                                     u"inglés (Francia)",
                                     u"español (Alemania)",
                                     "lengua desconocida (Croacia)",
                                     "lengua desconocida (Suecia)",
                                     u"lengua desconocida (República Dominicana)",
                                     u"lengua desconocida (Bélgica)" };
    // De-Anglicizing root required the change from
    // English display names to ISO Codes - ram 2003/09/26
    UnicodeString invDisplayNames [] = { "German (Canada)",
                                     "Japanese (South Africa)",
                                     "Russian (Mexico)",
                                     "English (France)",
                                     "Spanish (Germany)",
                                     "Unknown language (Croatia)",
                                     "Unknown language (Sweden)",
                                     "Unknown language (Dominican Republic)",
                                     "Unknown language (Belgium)" };

    int32_t i;
    UErrorCode status = U_ZERO_ERROR;
    Locale saveLocale;
    Locale::setDefault(Locale::getUS(), status);
    for (i = 0; i < 9; ++i) {
        UnicodeString name;
        localesToTest[i].getDisplayName(Locale::getUS(), name);
        logln(name);
        if (name != englishDisplayNames[i])
        {
            dataerrln("Lookup in English failed: expected \"" + englishDisplayNames[i]
                        + "\", got \"" + name + "\"");
            logln("Locale name was-> " + (name=localesToTest[i].getName()));
        }
    }

    for (i = 0; i < 9; i++) {
        UnicodeString name;
        localesToTest[i].getDisplayName(Locale("es", "ES"), name);
        logln(name);
        if (name != spanishDisplayNames[i])
            dataerrln("Lookup in Spanish failed: expected \"" + spanishDisplayNames[i]
                        + "\", got \"" + name + "\"");
    }

    for (i = 0; i < 9; i++) {
        UnicodeString name;
        localesToTest[i].getDisplayName(Locale::getFrance(), name);
        logln(name);
        if (name != frenchDisplayNames[i])
            dataerrln("Lookup in French failed: expected \"" + frenchDisplayNames[i]
                        + "\", got \"" + name + "\"");
    }

    for (i = 0; i < 9; i++) {
        UnicodeString name;
        localesToTest[i].getDisplayName(Locale("inv", "IN"), name);
        logln(name + " Locale fallback to be, and data fallback to root");
        if (name != invDisplayNames[i])
            dataerrln("Lookup in INV failed: expected \"" + prettify(invDisplayNames[i])
                        + "\", got \"" + prettify(name) + "\"");
        localesToTest[i].getDisplayName(Locale("inv", "BD"), name);
        logln(name + " Data fallback to root");
        if (name != invDisplayNames[i])
            dataerrln("Lookup in INV failed: expected \"" + prettify(invDisplayNames[i])
                        + "\", got \"" + prettify(name )+ "\"");
    }
    Locale::setDefault(saveLocale, status);
}

#if !UCONFIG_NO_FORMATTING

/**
 * @bug 4135752
 * This would be better tested by the LocaleDataTest.  Will move it when I
 * get the LocaleDataTest working again.
 */
void
LocaleTest::TestThaiCurrencyFormat()
{
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormat *thaiCurrency = dynamic_cast<DecimalFormat*>(NumberFormat::createCurrencyInstance(
                    Locale("th", "TH"), status));
    UnicodeString posPrefix(u"\u0E3F");
    UnicodeString temp;

    if(U_FAILURE(status) || !thaiCurrency)
    {
        dataerrln("Couldn't get th_TH currency -> " + UnicodeString(u_errorName(status)));
        return;
    }
    if (thaiCurrency->getPositivePrefix(temp) != posPrefix)
        errln("Thai currency prefix wrong: expected Baht sign, got \"" +
                        thaiCurrency->getPositivePrefix(temp) + "\"");
    if (thaiCurrency->getPositiveSuffix(temp) != "")
        errln("Thai currency suffix wrong: expected \"\", got \"" +
                        thaiCurrency->getPositiveSuffix(temp) + "\"");

    delete thaiCurrency;
}

/**
 * @bug 4122371
 * Confirm that Euro support works.  This test is pretty rudimentary; all it does
 * is check that any locales with the EURO variant format a number using the
 * Euro currency symbol.
 *
 * ASSUME: All locales encode the Euro character "\u20AC".
 * If this is changed to use the single-character Euro symbol, this
 * test must be updated.
 *
 */
void
LocaleTest::TestEuroSupport()
{
    char16_t euro = 0x20ac;
    const UnicodeString EURO_CURRENCY(&euro, 1, 1); // Look for this UnicodeString in formatted Euro currency
    const char* localeArr[] = {
                            "ca_ES",
                            "de_AT",
                            "de_DE",
                            "de_LU",
                            "el_GR",
                            "en_BE",
                            "en_IE",
                            "en_GB@currency=EUR",
                            "en_US@currency=EUR",
                            "es_ES",
                            "eu_ES",
                            "fi_FI",
                            "fr_BE",
                            "fr_FR",
                            "fr_LU",
                            "ga_IE",
                            "gl_ES",
                            "it_IT",
                            "nl_BE",
                            "nl_NL",
                            "pt_PT",
                            nullptr
                        };
    const char** locales = localeArr;

    UErrorCode status = U_ZERO_ERROR;

    UnicodeString temp;

    for (;*locales!=nullptr;locales++) {
        Locale loc (*locales);
        UnicodeString temp;
        NumberFormat *nf = NumberFormat::createCurrencyInstance(loc, status);
        UnicodeString pos;

        if (U_FAILURE(status)) {
            dataerrln("Error calling NumberFormat::createCurrencyInstance(%s)", *locales);
            continue;
        }

        nf->format(271828.182845, pos);
        UnicodeString neg;
        nf->format(-271828.182845, neg);
        if (pos.indexOf(EURO_CURRENCY) >= 0 &&
            neg.indexOf(EURO_CURRENCY) >= 0) {
            logln("Ok: " + (temp=loc.getName()) +
                ": " + pos + " / " + neg);
        }
        else {
            errln("Fail: " + (temp=loc.getName()) +
                " formats without " + EURO_CURRENCY +
                ": " + pos + " / " + neg +
                "\n*** THIS FAILURE MAY ONLY MEAN THAT LOCALE DATA HAS CHANGED ***");
        }

        delete nf;
    }

    UnicodeString dollarStr("USD", ""), euroStr("EUR", ""), genericStr((char16_t)0x00a4), resultStr;
    char16_t tmp[4];
    status = U_ZERO_ERROR;

    ucurr_forLocale("en_US", tmp, 4, &status);
    resultStr.setTo(tmp);
    if (dollarStr != resultStr) {
        errcheckln(status, "Fail: en_US didn't return USD - %s", u_errorName(status));
    }
    ucurr_forLocale("en_US@currency=EUR", tmp, 4, &status);
    resultStr.setTo(tmp);
    if (euroStr != resultStr) {
        errcheckln(status, "Fail: en_US@currency=EUR didn't return EUR - %s", u_errorName(status));
    }
    ucurr_forLocale("en_GB@currency=EUR", tmp, 4, &status);
    resultStr.setTo(tmp);
    if (euroStr != resultStr) {
        errcheckln(status, "Fail: en_GB@currency=EUR didn't return EUR - %s", u_errorName(status));
    }
    ucurr_forLocale("en_US_Q", tmp, 4, &status);
    resultStr.setTo(tmp);
    if (dollarStr != resultStr) {
        errcheckln(status, "Fail: en_US_Q didn't fallback to en_US - %s", u_errorName(status));
    }
    int32_t invalidLen = ucurr_forLocale("en_QQ", tmp, 4, &status);
    if (invalidLen || U_SUCCESS(status)) {
        errln("Fail: en_QQ didn't return nullptr");
    }

    // The currency keyword value is as long as the destination buffer.
    // It should detect the overflow internally, and default to the locale's currency.
    tmp[0] = u'¤';
    status = U_ZERO_ERROR;
    int32_t length = ucurr_forLocale("en_US@currency=euro", tmp, 4, &status);
    if (U_FAILURE(status) || dollarStr != UnicodeString(tmp, length)) {
        if (U_SUCCESS(status) && tmp[0] == u'¤') {
            errln("Fail: ucurr_forLocale(en_US@currency=euro) succeeded without writing output");
        } else {
            errln("Fail: ucurr_forLocale(en_US@currency=euro) != USD - %s", u_errorName(status));
        }
    }
}

#endif

/**
 * @bug 4139504
 * toString() doesn't work with language_VARIANT.
 */
void
LocaleTest::TestToString() {
    Locale DATA [] = {
        Locale("xx", "", ""),
        Locale("", "YY", ""),
        Locale("", "", "ZZ"),
        Locale("xx", "YY", ""),
        Locale("xx", "", "ZZ"),
        Locale("", "YY", "ZZ"),
        Locale("xx", "YY", "ZZ"),
    };

    const char DATA_S [][20] = {
        "xx",
        "_YY",
        "__ZZ",
        "xx_YY",
        "xx__ZZ",
        "_YY_ZZ",
        "xx_YY_ZZ",
    };

    for (int32_t i=0; i < 7; ++i) {
      const char *name;
      name = DATA[i].getName();

      if (strcmp(name, DATA_S[i]) != 0)
        {
            errln("Fail: Locale.getName(), got:" + UnicodeString(name) + ", expected: " + DATA_S[i]);
        }
        else
            logln("Pass: Locale.getName(), got:" + UnicodeString(name) );
    }
}

#if !UCONFIG_NO_FORMATTING

/**
 * @bug 4139940
 * Couldn't reproduce this bug -- probably was fixed earlier.
 *
 * ORIGINAL BUG REPORT:
 * -- basically, hungarian for monday shouldn't have an \u00f4
 * (o circumflex)in it instead it should be an o with 2 inclined
 * (right) lines over it..
 *
 * You may wonder -- why do all this -- why not just add a line to
 * LocaleData?  Well, I could see by inspection that the locale file had the
 * right character in it, so I wanted to check the rest of the pipeline -- a
 * very remote possibility, but I wanted to be sure.  The other possibility
 * is that something is wrong with the font mapping subsystem, but we can't
 * test that here.
 */
void
LocaleTest::Test4139940()
{
    Locale mylocale("hu", "", "");
    UDate mydate = date(98,3,13); // A Monday
    UErrorCode status = U_ZERO_ERROR;
    SimpleDateFormat df_full("EEEE", mylocale, status);
    if(U_FAILURE(status)){
        dataerrln(UnicodeString("Could not create SimpleDateFormat object for locale hu. Error: ") + UnicodeString(u_errorName(status)));
        return;
    }
    UnicodeString str;
    FieldPosition pos(FieldPosition::DONT_CARE);
    df_full.format(mydate, str, pos);
    // Make sure that o circumflex (\u00F4) is NOT there, and
    // o double acute (\u0151) IS.
    char16_t ocf = 0x00f4;
    char16_t oda = 0x0151;

    if (str.indexOf(oda) < 0 || str.indexOf(ocf) >= 0) {
      /* If the default calendar of the default locale is not "gregorian" this test will fail. */
      LocalPointer<Calendar> defaultCalendar(Calendar::createInstance(status));
      if (strcmp(defaultCalendar->getType(), "gregorian") == 0) {
        errln("Fail: Monday in Hungarian is wrong - oda's index is %d and ocf's is %d",
              str.indexOf(oda), str.indexOf(ocf));
      } else {
        logln(UnicodeString("An error is produce in non Gregorian calendar."));
      }
      logln(UnicodeString("String is: ") + str );
    }
}

UDate
LocaleTest::date(int32_t y, int32_t m, int32_t d, int32_t hr, int32_t min, int32_t sec)
{
    UErrorCode status = U_ZERO_ERROR;
    Calendar *cal = Calendar::createInstance(status);
    if (cal == nullptr)
        return 0.0;
    cal->clear();
    cal->set(1900 + y, m, d, hr, min, sec); // Add 1900 to follow java.util.Date protocol
    UDate dt = cal->getTime(status);
    if (U_FAILURE(status))
        return 0.0;

    delete cal;
    return dt;
}

/**
 * @bug 4143951
 * Russian first day of week should be Monday. Confirmed.
 */
void
LocaleTest::Test4143951()
{
    UErrorCode status = U_ZERO_ERROR;
    Calendar *cal = Calendar::createInstance(Locale("ru", "", ""), status);
    if(U_SUCCESS(status)) {
      if (cal->getFirstDayOfWeek(status) != UCAL_MONDAY) {
          dataerrln("Fail: First day of week in Russia should be Monday");
      }
    }
    delete cal;
}

#endif

/**
 * @bug 4147315
 * java.util.Locale.getISO3Country() works wrong for non ISO-3166 codes.
 * Should throw an exception for unknown locales
 */
void
LocaleTest::Test4147315()
{
  UnicodeString temp;
    // Try with codes that are the wrong length but happen to match text
    // at a valid offset in the mapping table
    Locale locale("xxx", "CCC");

    const char *result = locale.getISO3Country();

    // Change to conform to C api usage
    if((result==nullptr)||(result[0] != 0))
      errln("ERROR: getISO3Country() returns: " + UnicodeString(result,"") +
                " for locale '" + (temp=locale.getName()) + "' rather than exception" );
}

/**
 * @bug 4147317
 * java.util.Locale.getISO3Language() works wrong for non ISO-3166 codes.
 * Should throw an exception for unknown locales
 */
void
LocaleTest::Test4147317()
{
    UnicodeString temp;
    // Try with codes that are the wrong length but happen to match text
    // at a valid offset in the mapping table
    Locale locale("xxx", "CCC");

    const char *result = locale.getISO3Language();

    // Change to conform to C api usage
    if((result==nullptr)||(result[0] != 0))
      errln("ERROR: getISO3Language() returns: " + UnicodeString(result,"") +
                " for locale '" + (temp=locale.getName()) + "' rather than exception" );
}

/*
 * @bug 4147552
 */
void
LocaleTest::Test4147552()
{
    Locale locales [] = {     Locale("no", "NO"),
                            Locale("no", "NO", "B"),
                             Locale("no", "NO", "NY")
    };

    UnicodeString edn("Norwegian (Norway, B)");
    UnicodeString englishDisplayNames [] = {
                                                "Norwegian (Norway)",
                                                 edn,
                                                 // "Norwegian (Norway,B)",
                                                 //"Norwegian (Norway,NY)"
                                                 "Norwegian (Norway, NY)"
    };
    UnicodeString ndn("norsk (Norge, B");
    UnicodeString norwegianDisplayNames [] = {
                                                "norsk (Norge)",
                                                "norsk (Norge, B)",
                                                //ndn,
                                                 "norsk (Noreg, NY)"
                                                 //"Norsk (Noreg, Nynorsk)"
    };
    UErrorCode status = U_ZERO_ERROR;

    Locale saveLocale;
    Locale::setDefault(Locale::getEnglish(), status);
    for (int32_t i = 0; i < 3; ++i) {
        Locale loc = locales[i];
        UnicodeString temp;
        if (loc.getDisplayName(temp) != englishDisplayNames[i])
           dataerrln("English display-name mismatch: expected " +
                   englishDisplayNames[i] + ", got " + loc.getDisplayName(temp));
        if (loc.getDisplayName(loc, temp) != norwegianDisplayNames[i])
            dataerrln("Norwegian display-name mismatch: expected " +
                   norwegianDisplayNames[i] + ", got " +
                   loc.getDisplayName(loc, temp));
    }
    Locale::setDefault(saveLocale, status);
}

void
LocaleTest::TestVariantParsing()
{
    Locale en_US_custom("en", "US", "De Anza_Cupertino_California_United States_Earth");

    UnicodeString dispName("English (United States, DE ANZA_CUPERTINO_CALIFORNIA_UNITED STATES_EARTH)");
    UnicodeString dispVar("DE ANZA_CUPERTINO_CALIFORNIA_UNITED STATES_EARTH");

    UnicodeString got;

    en_US_custom.getDisplayVariant(Locale::getUS(), got);
    if(got != dispVar) {
        errln("FAIL: getDisplayVariant()");
        errln("Wanted: " + dispVar);
        errln("Got   : " + got);
    }

    en_US_custom.getDisplayName(Locale::getUS(), got);
    if(got != dispName) {
        dataerrln("FAIL: getDisplayName()");
        dataerrln("Wanted: " + dispName);
        dataerrln("Got   : " + got);
    }

    Locale shortVariant("fr", "FR", "foo");
    shortVariant.getDisplayVariant(got);

    if(got != "FOO") {
        errln("FAIL: getDisplayVariant()");
        errln("Wanted: foo");
        errln("Got   : " + got);
    }

    Locale bogusVariant("fr", "FR", "_foo");
    bogusVariant.getDisplayVariant(got);

    if(got != "FOO") {
        errln("FAIL: getDisplayVariant()");
        errln("Wanted: foo");
        errln("Got   : " + got);
    }

    Locale bogusVariant2("fr", "FR", "foo_");
    bogusVariant2.getDisplayVariant(got);

    if(got != "FOO") {
        errln("FAIL: getDisplayVariant()");
        errln("Wanted: foo");
        errln("Got   : " + got);
    }

    Locale bogusVariant3("fr", "FR", "_foo_");
    bogusVariant3.getDisplayVariant(got);

    if(got != "FOO") {
        errln("FAIL: getDisplayVariant()");
        errln("Wanted: foo");
        errln("Got   : " + got);
    }
}

void LocaleTest::Test20639_DeprecatesISO3Language() {
    IcuTestErrorCode status(*this, "Test20639_DeprecatesISO3Language");

    const struct TestCase {
        const char* localeName;
        const char* expectedISO3Language;
    } cases[] = {
        {"nb", "nob"},
        {"no", "nor"}, // why not nob?
        {"he", "heb"},
        {"iw", "heb"},
        {"ro", "ron"},
        {"mo", "mol"},
    };
    for (const auto& cas : cases) {
        Locale loc(cas.localeName);
        const char* actual = loc.getISO3Language();
        assertEquals(cas.localeName, cas.expectedISO3Language, actual);
    }
}

#if !UCONFIG_NO_FORMATTING

/**
 * @bug 4105828
 * Currency symbol in zh is wrong.  We will test this at the NumberFormat
 * end to test the whole pipe.
 */
void
LocaleTest::Test4105828()
{
    Locale LOC [] = { Locale::getChinese(),  Locale("zh", "CN", ""),
                     Locale("zh", "TW", ""), Locale("zh", "HK", "") };
    UErrorCode status = U_ZERO_ERROR;
    for (int32_t i = 0; i < 4; ++i) {
        NumberFormat *fmt = NumberFormat::createPercentInstance(LOC[i], status);
        if(U_FAILURE(status)) {
            dataerrln("Couldn't create NumberFormat - %s", u_errorName(status));
            return;
        }
        UnicodeString result;
        FieldPosition pos(FieldPosition::DONT_CARE);
        fmt->format((int32_t)1, result, pos);
        UnicodeString temp;
        if(result != "100%") {
            errln(UnicodeString("Percent for ") + LOC[i].getDisplayName(temp) + " should be 100%, got " + result);
        }
        delete fmt;
    }
}

#endif

// Tests setBogus and isBogus APIs for Locale
// Jitterbug 1735
void
LocaleTest::TestSetIsBogus() {
    Locale l("en_US");
    l.setToBogus();
    if(l.isBogus() != true) {
        errln("After setting bogus, didn't return true");
    }
    l = "en_US"; // This should reset bogus
    if(l.isBogus() != false) {
        errln("After resetting bogus, didn't return false");
    }
}


void
LocaleTest::TestAddLikelySubtags() {
    IcuTestErrorCode status(*this, "TestAddLikelySubtags()");

    static const Locale min("sv");
    static const Locale max("sv_Latn_SE");

    Locale result(min);
    result.addLikelySubtags(status);
    status.errIfFailureAndReset("\"%s\"", min.getName());
    assertEquals("addLikelySubtags", max.getName(), result.getName());
}


void
LocaleTest::TestMinimizeSubtags() {
    IcuTestErrorCode status(*this, "TestMinimizeSubtags()");

    static const Locale max("zh_Hant_TW");
    static const Locale min("zh_TW");

    Locale result(max);
    result.minimizeSubtags(status);
    status.errIfFailureAndReset("\"%s\"", max.getName());
    assertEquals("minimizeSubtags", min.getName(), result.getName());
}


void
LocaleTest::TestAddLikelyAndMinimizeSubtags() {
    IcuTestErrorCode status(*this, "TestAddLikelyAndMinimizeSubtags()");

    static const struct {
        const char* const from;
        const char* const add;
        const char* const remove;
    } full_data[] = {
        {
            "und",
            "en_Latn_US",
            "en"
        },
        {
            "und_AQ",
            "en_Latn_AQ",
            "en_AQ"
        }, {
            "und_Zzzz_AQ",
            "en_Latn_AQ",
            "en_AQ"
        }, {
            "und_Latn_AQ",
            "en_Latn_AQ",
            "en_AQ"
        }, {
            "und_Moon_AQ",
            "en_Moon_AQ",
            "en_Moon_AQ"
        }, {
            "aa",
            "aa_Latn_ET",
            "aa"
        }, {
            "af",
            "af_Latn_ZA",
            "af"
        }, {
            "ak",
            "ak_Latn_GH",
            "ak"
        }, {
            "am",
            "am_Ethi_ET",
            "am"
        }, {
            "ar",
            "ar_Arab_EG",
            "ar"
        }, {
            "as",
            "as_Beng_IN",
            "as"
        }, {
            "az",
            "az_Latn_AZ",
            "az"
        }, {
            "be",
            "be_Cyrl_BY",
            "be"
        }, {
            "bg",
            "bg_Cyrl_BG",
            "bg"
        }, {
            "bn",
            "bn_Beng_BD",
            "bn"
        }, {
            "bo",
            "bo_Tibt_CN",
            "bo"
        }, {
            "bs",
            "bs_Latn_BA",
            "bs"
        }, {
            "ca",
            "ca_Latn_ES",
            "ca"
        }, {
            "ch",
            "ch_Latn_GU",
            "ch"
        }, {
            "chk",
            "chk_Latn_FM",
            "chk"
        }, {
            "cs",
            "cs_Latn_CZ",
            "cs"
        }, {
            "cy",
            "cy_Latn_GB",
            "cy"
        }, {
            "da",
            "da_Latn_DK",
            "da"
        }, {
            "de",
            "de_Latn_DE",
            "de"
        }, {
            "dv",
            "dv_Thaa_MV",
            "dv"
        }, {
            "dz",
            "dz_Tibt_BT",
            "dz"
        }, {
            "ee",
            "ee_Latn_GH",
            "ee"
        }, {
            "el",
            "el_Grek_GR",
            "el"
        }, {
            "en",
            "en_Latn_US",
            "en"
        }, {
            "es",
            "es_Latn_ES",
            "es"
        }, {
            "et",
            "et_Latn_EE",
            "et"
        }, {
            "eu",
            "eu_Latn_ES",
            "eu"
        }, {
            "fa",
            "fa_Arab_IR",
            "fa"
        }, {
            "fi",
            "fi_Latn_FI",
            "fi"
        }, {
            "fil",
            "fil_Latn_PH",
            "fil"
        }, {
            "fj",
            "fj_Latn_FJ",
            "fj"
        }, {
            "fo",
            "fo_Latn_FO",
            "fo"
        }, {
            "fr",
            "fr_Latn_FR",
            "fr"
        }, {
            "fur",
            "fur_Latn_IT",
            "fur"
        }, {
            "ga",
            "ga_Latn_IE",
            "ga"
        }, {
            "gaa",
            "gaa_Latn_GH",
            "gaa"
        }, {
            "gl",
            "gl_Latn_ES",
            "gl"
        }, {
            "gn",
            "gn_Latn_PY",
            "gn"
        }, {
            "gu",
            "gu_Gujr_IN",
            "gu"
        }, {
            "ha",
            "ha_Latn_NG",
            "ha"
        }, {
            "haw",
            "haw_Latn_US",
            "haw"
        }, {
            "he",
            "he_Hebr_IL",
            "he"
        }, {
            "hi",
            "hi_Deva_IN",
            "hi"
        }, {
            "hr",
            "hr_Latn_HR",
            "hr"
        }, {
            "ht",
            "ht_Latn_HT",
            "ht"
        }, {
            "hu",
            "hu_Latn_HU",
            "hu"
        }, {
            "hy",
            "hy_Armn_AM",
            "hy"
        }, {
            "id",
            "id_Latn_ID",
            "id"
        }, {
            "ig",
            "ig_Latn_NG",
            "ig"
        }, {
            "ii",
            "ii_Yiii_CN",
            "ii"
        }, {
            "is",
            "is_Latn_IS",
            "is"
        }, {
            "it",
            "it_Latn_IT",
            "it"
        }, {
            "ja",
            "ja_Jpan_JP",
            "ja"
        }, {
            "ka",
            "ka_Geor_GE",
            "ka"
        }, {
            "kaj",
            "kaj_Latn_NG",
            "kaj"
        }, {
            "kam",
            "kam_Latn_KE",
            "kam"
        }, {
            "kk",
            "kk_Cyrl_KZ",
            "kk"
        }, {
            "kl",
            "kl_Latn_GL",
            "kl"
        }, {
            "km",
            "km_Khmr_KH",
            "km"
        }, {
            "kn",
            "kn_Knda_IN",
            "kn"
        }, {
            "ko",
            "ko_Kore_KR",
            "ko"
        }, {
            "kok",
            "kok_Deva_IN",
            "kok"
        }, {
            "kpe",
            "kpe_Latn_LR",
            "kpe"
        }, {
            "ku",
            "ku_Latn_TR",
            "ku"
        }, {
            "ky",
            "ky_Cyrl_KG",
            "ky"
        }, {
            "la",
            "la_Latn_VA",
            "la"
        }, {
            "ln",
            "ln_Latn_CD",
            "ln"
        }, {
            "lo",
            "lo_Laoo_LA",
            "lo"
        }, {
            "lt",
            "lt_Latn_LT",
            "lt"
        }, {
            "lv",
            "lv_Latn_LV",
            "lv"
        }, {
            "mg",
            "mg_Latn_MG",
            "mg"
        }, {
            "mh",
            "mh_Latn_MH",
            "mh"
        }, {
            "mk",
            "mk_Cyrl_MK",
            "mk"
        }, {
            "ml",
            "ml_Mlym_IN",
            "ml"
        }, {
            "mn",
            "mn_Cyrl_MN",
            "mn"
        }, {
            "mr",
            "mr_Deva_IN",
            "mr"
        }, {
            "ms",
            "ms_Latn_MY",
            "ms"
        }, {
            "mt",
            "mt_Latn_MT",
            "mt"
        }, {
            "my",
            "my_Mymr_MM",
            "my"
        }, {
            "na",
            "na_Latn_NR",
            "na"
        }, {
            "ne",
            "ne_Deva_NP",
            "ne"
        }, {
            "niu",
            "niu_Latn_NU",
            "niu"
        }, {
            "nl",
            "nl_Latn_NL",
            "nl"
        }, {
            "nn",
            "nn_Latn_NO",
            "nn"
        }, {
            "no",
            "no_Latn_NO",
            "no"
        }, {
            "nr",
            "nr_Latn_ZA",
            "nr"
        }, {
            "nso",
            "nso_Latn_ZA",
            "nso"
        }, {
            "om",
            "om_Latn_ET",
            "om"
        }, {
            "or",
            "or_Orya_IN",
            "or"
        }, {
            "pa",
            "pa_Guru_IN",
            "pa"
        }, {
            "pa_Arab",
            "pa_Arab_PK",
            "pa_PK"
        }, {
            "pa_PK",
            "pa_Arab_PK",
            "pa_PK"
        }, {
            "pap",
            "pap_Latn_CW",
            "pap"
        }, {
            "pau",
            "pau_Latn_PW",
            "pau"
        }, {
            "pl",
            "pl_Latn_PL",
            "pl"
        }, {
            "ps",
            "ps_Arab_AF",
            "ps"
        }, {
            "pt",
            "pt_Latn_BR",
            "pt"
        }, {
            "rn",
            "rn_Latn_BI",
            "rn"
        }, {
            "ro",
            "ro_Latn_RO",
            "ro"
        }, {
            "ru",
            "ru_Cyrl_RU",
            "ru"
        }, {
            "rw",
            "rw_Latn_RW",
            "rw"
        }, {
            "sa",
            "sa_Deva_IN",
            "sa"
        }, {
            "se",
            "se_Latn_NO",
            "se"
        }, {
            "sg",
            "sg_Latn_CF",
            "sg"
        }, {
            "si",
            "si_Sinh_LK",
            "si"
        }, {
            "sid",
            "sid_Latn_ET",
            "sid"
        }, {
            "sk",
            "sk_Latn_SK",
            "sk"
        }, {
            "sl",
            "sl_Latn_SI",
            "sl"
        }, {
            "sm",
            "sm_Latn_WS",
            "sm"
        }, {
            "so",
            "so_Latn_SO",
            "so"
        }, {
            "sq",
            "sq_Latn_AL",
            "sq"
        }, {
            "sr",
            "sr_Cyrl_RS",
            "sr"
        }, {
            "ss",
            "ss_Latn_ZA",
            "ss"
        }, {
            "st",
            "st_Latn_ZA",
            "st"
        }, {
            "sv",
            "sv_Latn_SE",
            "sv"
        }, {
            "sw",
            "sw_Latn_TZ",
            "sw"
        }, {
            "ta",
            "ta_Taml_IN",
            "ta"
        }, {
            "te",
            "te_Telu_IN",
            "te"
        }, {
            "tet",
            "tet_Latn_TL",
            "tet"
        }, {
            "tg",
            "tg_Cyrl_TJ",
            "tg"
        }, {
            "th",
            "th_Thai_TH",
            "th"
        }, {
            "ti",
            "ti_Ethi_ET",
            "ti"
        }, {
            "tig",
            "tig_Ethi_ER",
            "tig"
        }, {
            "tk",
            "tk_Latn_TM",
            "tk"
        }, {
            "tkl",
            "tkl_Latn_TK",
            "tkl"
        }, {
            "tn",
            "tn_Latn_ZA",
            "tn"
        }, {
            "to",
            "to_Latn_TO",
            "to"
        }, {
            "tpi",
            "tpi_Latn_PG",
            "tpi"
        }, {
            "tr",
            "tr_Latn_TR",
            "tr"
        }, {
            "ts",
            "ts_Latn_ZA",
            "ts"
        }, {
            "tt",
            "tt_Cyrl_RU",
            "tt"
        }, {
            "tvl",
            "tvl_Latn_TV",
            "tvl"
        }, {
            "ty",
            "ty_Latn_PF",
            "ty"
        }, {
            "uk",
            "uk_Cyrl_UA",
            "uk"
        }, {
            "und",
            "en_Latn_US",
            "en"
        }, {
            "und_AD",
            "ca_Latn_AD",
            "ca_AD"
        }, {
            "und_AE",
            "ar_Arab_AE",
            "ar_AE"
        }, {
            "und_AF",
            "fa_Arab_AF",
            "fa_AF"
        }, {
            "und_AL",
            "sq_Latn_AL",
            "sq"
        }, {
            "und_AM",
            "hy_Armn_AM",
            "hy"
        }, {
            "und_AO",
            "pt_Latn_AO",
            "pt_AO"
        }, {
            "und_AR",
            "es_Latn_AR",
            "es_AR"
        }, {
            "und_AS",
            "sm_Latn_AS",
            "sm_AS"
        }, {
            "und_AT",
            "de_Latn_AT",
            "de_AT"
        }, {
            "und_AW",
            "nl_Latn_AW",
            "nl_AW"
        }, {
            "und_AX",
            "sv_Latn_AX",
            "sv_AX"
        }, {
            "und_AZ",
            "az_Latn_AZ",
            "az"
        }, {
            "und_Arab",
            "ar_Arab_EG",
            "ar"
        }, {
            "und_Arab_IN",
            "ur_Arab_IN",
            "ur_IN"
        }, {
            "und_Arab_PK",
            "ur_Arab_PK",
            "ur"
        }, {
            "und_Arab_SN",
            "ar_Arab_SN",
            "ar_SN"
        }, {
            "und_Armn",
            "hy_Armn_AM",
            "hy"
        }, {
            "und_BA",
            "bs_Latn_BA",
            "bs"
        }, {
            "und_BD",
            "bn_Beng_BD",
            "bn"
        }, {
            "und_BE",
            "nl_Latn_BE",
            "nl_BE"
        }, {
            "und_BF",
            "fr_Latn_BF",
            "fr_BF"
        }, {
            "und_BG",
            "bg_Cyrl_BG",
            "bg"
        }, {
            "und_BH",
            "ar_Arab_BH",
            "ar_BH"
        }, {
            "und_BI",
            "rn_Latn_BI",
            "rn"
        }, {
            "und_BJ",
            "fr_Latn_BJ",
            "fr_BJ"
        }, {
            "und_BN",
            "ms_Latn_BN",
            "ms_BN"
        }, {
            "und_BO",
            "es_Latn_BO",
            "es_BO"
        }, {
            "und_BR",
            "pt_Latn_BR",
            "pt"
        }, {
            "und_BT",
            "dz_Tibt_BT",
            "dz"
        }, {
            "und_BY",
            "be_Cyrl_BY",
            "be"
        }, {
            "und_Beng",
            "bn_Beng_BD",
            "bn"
        }, {
            "und_Beng_IN",
            "bn_Beng_IN",
            "bn_IN"
        }, {
            "und_CD",
            "sw_Latn_CD",
            "sw_CD"
        }, {
            "und_CF",
            "fr_Latn_CF",
            "fr_CF"
        }, {
            "und_CG",
            "fr_Latn_CG",
            "fr_CG"
        }, {
            "und_CH",
            "de_Latn_CH",
            "de_CH"
        }, {
            "und_CI",
            "fr_Latn_CI",
            "fr_CI"
        }, {
            "und_CL",
            "es_Latn_CL",
            "es_CL"
        }, {
            "und_CM",
            "fr_Latn_CM",
            "fr_CM"
        }, {
            "und_CN",
            "zh_Hans_CN",
            "zh"
        }, {
            "und_CO",
            "es_Latn_CO",
            "es_CO"
        }, {
            "und_CR",
            "es_Latn_CR",
            "es_CR"
        }, {
            "und_CU",
            "es_Latn_CU",
            "es_CU"
        }, {
            "und_CV",
            "pt_Latn_CV",
            "pt_CV"
        }, {
            "und_CY",
            "el_Grek_CY",
            "el_CY"
        }, {
            "und_CZ",
            "cs_Latn_CZ",
            "cs"
        }, {
            "und_Cyrl",
            "ru_Cyrl_RU",
            "ru"
        }, {
            "und_Cyrl_KZ",
            "ru_Cyrl_KZ",
            "ru_KZ"
        }, {
            "und_DE",
            "de_Latn_DE",
            "de"
        }, {
            "und_DJ",
            "aa_Latn_DJ",
            "aa_DJ"
        }, {
            "und_DK",
            "da_Latn_DK",
            "da"
        }, {
            "und_DO",
            "es_Latn_DO",
            "es_DO"
        }, {
            "und_DZ",
            "ar_Arab_DZ",
            "ar_DZ"
        }, {
            "und_Deva",
            "hi_Deva_IN",
            "hi"
        }, {
            "und_EC",
            "es_Latn_EC",
            "es_EC"
        }, {
            "und_EE",
            "et_Latn_EE",
            "et"
        }, {
            "und_EG",
            "ar_Arab_EG",
            "ar"
        }, {
            "und_EH",
            "ar_Arab_EH",
            "ar_EH"
        }, {
            "und_ER",
            "ti_Ethi_ER",
            "ti_ER"
        }, {
            "und_ES",
            "es_Latn_ES",
            "es"
        }, {
            "und_ET",
            "am_Ethi_ET",
            "am"
        }, {
            "und_Ethi",
            "am_Ethi_ET",
            "am"
        }, {
            "und_Ethi_ER",
            "ti_Ethi_ER",
            "ti_ER"
        }, {
            "und_FI",
            "fi_Latn_FI",
            "fi"
        }, {
            "und_FM",
            "en_Latn_FM",
            "en_FM"
        }, {
            "und_FO",
            "fo_Latn_FO",
            "fo"
        }, {
            "und_FR",
            "fr_Latn_FR",
            "fr"
        }, {
            "und_GA",
            "fr_Latn_GA",
            "fr_GA"
        }, {
            "und_GE",
            "ka_Geor_GE",
            "ka"
        }, {
            "und_GF",
            "fr_Latn_GF",
            "fr_GF"
        }, {
            "und_GL",
            "kl_Latn_GL",
            "kl"
        }, {
            "und_GN",
            "fr_Latn_GN",
            "fr_GN"
        }, {
            "und_GP",
            "fr_Latn_GP",
            "fr_GP"
        }, {
            "und_GQ",
            "es_Latn_GQ",
            "es_GQ"
        }, {
            "und_GR",
            "el_Grek_GR",
            "el"
        }, {
            "und_GT",
            "es_Latn_GT",
            "es_GT"
        }, {
            "und_GU",
            "en_Latn_GU",
            "en_GU"
        }, {
            "und_GW",
            "pt_Latn_GW",
            "pt_GW"
        }, {
            "und_Geor",
            "ka_Geor_GE",
            "ka"
        }, {
            "und_Grek",
            "el_Grek_GR",
            "el"
        }, {
            "und_Gujr",
            "gu_Gujr_IN",
            "gu"
        }, {
            "und_Guru",
            "pa_Guru_IN",
            "pa"
        }, {
            "und_HK",
            "zh_Hant_HK",
            "zh_HK"
        }, {
            "und_HN",
            "es_Latn_HN",
            "es_HN"
        }, {
            "und_HR",
            "hr_Latn_HR",
            "hr"
        }, {
            "und_HT",
            "ht_Latn_HT",
            "ht"
        }, {
            "und_HU",
            "hu_Latn_HU",
            "hu"
        }, {
            "und_Hani",
            "zh_Hani_CN",
            "zh_Hani"
        }, {
            "und_Hans",
            "zh_Hans_CN",
            "zh"
        }, {
            "und_Hant",
            "zh_Hant_TW",
            "zh_TW"
        }, {
            "und_Hebr",
            "he_Hebr_IL",
            "he"
        }, {
            "und_ID",
            "id_Latn_ID",
            "id"
        }, {
            "und_IL",
            "he_Hebr_IL",
            "he"
        }, {
            "und_IN",
            "hi_Deva_IN",
            "hi"
        }, {
            "und_IQ",
            "ar_Arab_IQ",
            "ar_IQ"
        }, {
            "und_IR",
            "fa_Arab_IR",
            "fa"
        }, {
            "und_IS",
            "is_Latn_IS",
            "is"
        }, {
            "und_IT",
            "it_Latn_IT",
            "it"
        }, {
            "und_JO",
            "ar_Arab_JO",
            "ar_JO"
        }, {
            "und_JP",
            "ja_Jpan_JP",
            "ja"
        }, {
            "und_Jpan",
            "ja_Jpan_JP",
            "ja"
        }, {
            "und_KG",
            "ky_Cyrl_KG",
            "ky"
        }, {
            "und_KH",
            "km_Khmr_KH",
            "km"
        }, {
            "und_KM",
            "ar_Arab_KM",
            "ar_KM"
        }, {
            "und_KP",
            "ko_Kore_KP",
            "ko_KP"
        }, {
            "und_KR",
            "ko_Kore_KR",
            "ko"
        }, {
            "und_KW",
            "ar_Arab_KW",
            "ar_KW"
        }, {
            "und_KZ",
            "ru_Cyrl_KZ",
            "ru_KZ"
        }, {
            "und_Khmr",
            "km_Khmr_KH",
            "km"
        }, {
            "und_Knda",
            "kn_Knda_IN",
            "kn"
        }, {
            "und_Kore",
            "ko_Kore_KR",
            "ko"
        }, {
            "und_LA",
            "lo_Laoo_LA",
            "lo"
        }, {
            "und_LB",
            "ar_Arab_LB",
            "ar_LB"
        }, {
            "und_LI",
            "de_Latn_LI",
            "de_LI"
        }, {
            "und_LK",
            "si_Sinh_LK",
            "si"
        }, {
            "und_LS",
            "st_Latn_LS",
            "st_LS"
        }, {
            "und_LT",
            "lt_Latn_LT",
            "lt"
        }, {
            "und_LU",
            "fr_Latn_LU",
            "fr_LU"
        }, {
            "und_LV",
            "lv_Latn_LV",
            "lv"
        }, {
            "und_LY",
            "ar_Arab_LY",
            "ar_LY"
        }, {
            "und_Laoo",
            "lo_Laoo_LA",
            "lo"
        }, {
            "und_Latn_ES",
            "es_Latn_ES",
            "es"
        }, {
            "und_Latn_ET",
            "en_Latn_ET",
            "en_ET"
        }, {
            "und_Latn_GB",
            "en_Latn_GB",
            "en_GB"
        }, {
            "und_Latn_GH",
            "ak_Latn_GH",
            "ak"
        }, {
            "und_Latn_ID",
            "id_Latn_ID",
            "id"
        }, {
            "und_Latn_IT",
            "it_Latn_IT",
            "it"
        }, {
            "und_Latn_NG",
            "en_Latn_NG",
            "en_NG"
        }, {
            "und_Latn_TR",
            "tr_Latn_TR",
            "tr"
        }, {
            "und_Latn_ZA",
            "en_Latn_ZA",
            "en_ZA"
        }, {
            "und_MA",
            "ar_Arab_MA",
            "ar_MA"
        }, {
            "und_MC",
            "fr_Latn_MC",
            "fr_MC"
        }, {
            "und_MD",
            "ro_Latn_MD",
            "ro_MD"
        }, {
            "und_ME",
            "sr_Latn_ME",
            "sr_ME"
        }, {
            "und_MG",
            "mg_Latn_MG",
            "mg"
        }, {
            "und_MK",
            "mk_Cyrl_MK",
            "mk"
        }, {
            "und_ML",
            "bm_Latn_ML",
            "bm"
        }, {
            "und_MM",
            "my_Mymr_MM",
            "my"
        }, {
            "und_MN",
            "mn_Cyrl_MN",
            "mn"
        }, {
            "und_MO",
            "zh_Hant_MO",
            "zh_MO"
        }, {
            "und_MQ",
            "fr_Latn_MQ",
            "fr_MQ"
        }, {
            "und_MR",
            "ar_Arab_MR",
            "ar_MR"
        }, {
            "und_MT",
            "mt_Latn_MT",
            "mt"
        }, {
            "und_MV",
            "dv_Thaa_MV",
            "dv"
        }, {
            "und_MX",
            "es_Latn_MX",
            "es_MX"
        }, {
            "und_MY",
            "ms_Latn_MY",
            "ms"
        }, {
            "und_MZ",
            "pt_Latn_MZ",
            "pt_MZ"
        }, {
            "und_Mlym",
            "ml_Mlym_IN",
            "ml"
        }, {
            "und_Mymr",
            "my_Mymr_MM",
            "my"
        }, {
            "und_NC",
            "fr_Latn_NC",
            "fr_NC"
        }, {
            "und_NE",
            "ha_Latn_NE",
            "ha_NE"
        }, {
            "und_NG",
            "en_Latn_NG",
            "en_NG"
        }, {
            "und_NI",
            "es_Latn_NI",
            "es_NI"
        }, {
            "und_NL",
            "nl_Latn_NL",
            "nl"
        }, {
            "und_NO",
            "nb_Latn_NO",
            "nb"
        }, {
            "und_NP",
            "ne_Deva_NP",
            "ne"
        }, {
            "und_NR",
            "en_Latn_NR",
            "en_NR"
        }, {
            "und_OM",
            "ar_Arab_OM",
            "ar_OM"
        }, {
            "und_Orya",
            "or_Orya_IN",
            "or"
        }, {
            "und_PA",
            "es_Latn_PA",
            "es_PA"
        }, {
            "und_PE",
            "es_Latn_PE",
            "es_PE"
        }, {
            "und_PF",
            "fr_Latn_PF",
            "fr_PF"
        }, {
            "und_PG",
            "tpi_Latn_PG",
            "tpi"
        }, {
            "und_PH",
            "fil_Latn_PH",
            "fil"
        }, {
            "und_PL",
            "pl_Latn_PL",
            "pl"
        }, {
            "und_PM",
            "fr_Latn_PM",
            "fr_PM"
        }, {
            "und_PR",
            "es_Latn_PR",
            "es_PR"
        }, {
            "und_PS",
            "ar_Arab_PS",
            "ar_PS"
        }, {
            "und_PT",
            "pt_Latn_PT",
            "pt_PT"
        }, {
            "und_PW",
            "pau_Latn_PW",
            "pau"
        }, {
            "und_PY",
            "gn_Latn_PY",
            "gn"
        }, {
            "und_QA",
            "ar_Arab_QA",
            "ar_QA"
        }, {
            "und_RE",
            "fr_Latn_RE",
            "fr_RE"
        }, {
            "und_RO",
            "ro_Latn_RO",
            "ro"
        }, {
            "und_RS",
            "sr_Cyrl_RS",
            "sr"
        }, {
            "und_RU",
            "ru_Cyrl_RU",
            "ru"
        }, {
            "und_RW",
            "rw_Latn_RW",
            "rw"
        }, {
            "und_SA",
            "ar_Arab_SA",
            "ar_SA"
        }, {
            "und_SD",
            "ar_Arab_SD",
            "ar_SD"
        }, {
            "und_SE",
            "sv_Latn_SE",
            "sv"
        }, {
            "und_SG",
            "en_Latn_SG",
            "en_SG"
        }, {
            "und_SI",
            "sl_Latn_SI",
            "sl"
        }, {
            "und_SJ",
            "nb_Latn_SJ",
            "nb_SJ"
        }, {
            "und_SK",
            "sk_Latn_SK",
            "sk"
        }, {
            "und_SM",
            "it_Latn_SM",
            "it_SM"
        }, {
            "und_SN",
            "fr_Latn_SN",
            "fr_SN"
        }, {
            "und_SO",
            "so_Latn_SO",
            "so"
        }, {
            "und_SR",
            "nl_Latn_SR",
            "nl_SR"
        }, {
            "und_ST",
            "pt_Latn_ST",
            "pt_ST"
        }, {
            "und_SV",
            "es_Latn_SV",
            "es_SV"
        }, {
            "und_SY",
            "ar_Arab_SY",
            "ar_SY"
        }, {
            "und_Sinh",
            "si_Sinh_LK",
            "si"
        }, {
            "und_Syrc",
            "syr_Syrc_IQ",
            "syr"
        }, {
            "und_TD",
            "fr_Latn_TD",
            "fr_TD"
        }, {
            "und_TG",
            "fr_Latn_TG",
            "fr_TG"
        }, {
            "und_TH",
            "th_Thai_TH",
            "th"
        }, {
            "und_TJ",
            "tg_Cyrl_TJ",
            "tg"
        }, {
            "und_TK",
            "tkl_Latn_TK",
            "tkl"
        }, {
            "und_TL",
            "pt_Latn_TL",
            "pt_TL"
        }, {
            "und_TM",
            "tk_Latn_TM",
            "tk"
        }, {
            "und_TN",
            "ar_Arab_TN",
            "ar_TN"
        }, {
            "und_TO",
            "to_Latn_TO",
            "to"
        }, {
            "und_TR",
            "tr_Latn_TR",
            "tr"
        }, {
            "und_TV",
            "tvl_Latn_TV",
            "tvl"
        }, {
            "und_TW",
            "zh_Hant_TW",
            "zh_TW"
        }, {
            "und_Taml",
            "ta_Taml_IN",
            "ta"
        }, {
            "und_Telu",
            "te_Telu_IN",
            "te"
        }, {
            "und_Thaa",
            "dv_Thaa_MV",
            "dv"
        }, {
            "und_Thai",
            "th_Thai_TH",
            "th"
        }, {
            "und_Tibt",
            "bo_Tibt_CN",
            "bo"
        }, {
            "und_UA",
            "uk_Cyrl_UA",
            "uk"
        }, {
            "und_UY",
            "es_Latn_UY",
            "es_UY"
        }, {
            "und_UZ",
            "uz_Latn_UZ",
            "uz"
        }, {
            "und_VA",
            "it_Latn_VA",
            "it_VA"
        }, {
            "und_VE",
            "es_Latn_VE",
            "es_VE"
        }, {
            "und_VN",
            "vi_Latn_VN",
            "vi"
        }, {
            "und_VU",
            "bi_Latn_VU",
            "bi"
        }, {
            "und_WF",
            "fr_Latn_WF",
            "fr_WF"
        }, {
            "und_WS",
            "sm_Latn_WS",
            "sm"
        }, {
            "und_YE",
            "ar_Arab_YE",
            "ar_YE"
        }, {
            "und_YT",
            "fr_Latn_YT",
            "fr_YT"
        }, {
            "und_Yiii",
            "ii_Yiii_CN",
            "ii"
        }, {
            "ur",
            "ur_Arab_PK",
            "ur"
        }, {
            "uz",
            "uz_Latn_UZ",
            "uz"
        }, {
            "uz_AF",
            "uz_Arab_AF",
            "uz_AF"
        }, {
            "uz_Arab",
            "uz_Arab_AF",
            "uz_AF"
        }, {
            "ve",
            "ve_Latn_ZA",
            "ve"
        }, {
            "vi",
            "vi_Latn_VN",
            "vi"
        }, {
            "wal",
            "wal_Ethi_ET",
            "wal"
        }, {
            "wo",
            "wo_Latn_SN",
            "wo"
        }, {
            "wo_SN",
            "wo_Latn_SN",
            "wo"
        }, {
            "xh",
            "xh_Latn_ZA",
            "xh"
        }, {
            "yo",
            "yo_Latn_NG",
            "yo"
        }, {
            "zh",
            "zh_Hans_CN",
            "zh"
        }, {
            "zh_HK",
            "zh_Hant_HK",
            "zh_HK"
        }, {
            "zh_Hani",
            "zh_Hani_CN",
            "zh_Hani"
        }, {
            "zh_Hant",
            "zh_Hant_TW",
            "zh_TW"
        }, {
            "zh_MO",
            "zh_Hant_MO",
            "zh_MO"
        }, {
            "zh_TW",
            "zh_Hant_TW",
            "zh_TW"
        }, {
            "zu",
            "zu_Latn_ZA",
            "zu"
        }, {
            "und",
            "en_Latn_US",
            "en"
        }, {
            "und_ZZ",
            "en_Latn_US",
            "en"
        }, {
            "und_CN",
            "zh_Hans_CN",
            "zh"
        }, {
            "und_TW",
            "zh_Hant_TW",
            "zh_TW"
        }, {
            "und_HK",
            "zh_Hant_HK",
            "zh_HK"
        }, {
            "und_AQ",
            "en_Latn_AQ",
            "en_AQ"
        }, {
            "und_Zzzz",
            "en_Latn_US",
            "en"
        }, {
            "und_Zzzz_ZZ",
            "en_Latn_US",
            "en"
        }, {
            "und_Zzzz_CN",
            "zh_Hans_CN",
            "zh"
        }, {
            "und_Zzzz_TW",
            "zh_Hant_TW",
            "zh_TW"
        }, {
            "und_Zzzz_HK",
            "zh_Hant_HK",
            "zh_HK"
        }, {
            "und_Zzzz_AQ",
            "en_Latn_AQ",
            "en_AQ"
        }, {
            "und_Latn",
            "en_Latn_US",
            "en"
        }, {
            "und_Latn_ZZ",
            "en_Latn_US",
            "en"
        }, {
            "und_Latn_CN",
            "za_Latn_CN",
            "za"
        }, {
            "und_Latn_TW",
            "trv_Latn_TW",
            "trv"
        }, {
            "und_Latn_HK",
            "en_Latn_HK",
            "en_HK"
        }, {
            "und_Latn_AQ",
            "en_Latn_AQ",
            "en_AQ"
        }, {
            "und_Hans",
            "zh_Hans_CN",
            "zh"
        }, {
            "und_Hans_ZZ",
            "zh_Hans_CN",
            "zh"
        }, {
            "und_Hans_CN",
            "zh_Hans_CN",
            "zh"
        }, {
            "und_Hans_TW",
            "zh_Hans_TW",
            "zh_Hans_TW"
        }, {
            "und_Hans_HK",
            "zh_Hans_HK",
            "zh_Hans_HK"
        }, {
            "und_Hans_AQ",
            "zh_Hans_AQ",
            "zh_AQ"
        }, {
            "und_Hant",
            "zh_Hant_TW",
            "zh_TW"
        }, {
            "und_Hant_ZZ",
            "zh_Hant_TW",
            "zh_TW"
        }, {
            "und_Hant_CN",
            "zh_Hant_CN",
            "zh_Hant_CN"
        }, {
            "und_Hant_TW",
            "zh_Hant_TW",
            "zh_TW"
        }, {
            "und_Hant_HK",
            "zh_Hant_HK",
            "zh_HK"
        }, {
            "und_Hant_AQ",
            "zh_Hant_AQ",
            "zh_Hant_AQ"
        }, {
            "und_Moon",
            "en_Moon_US",
            "en_Moon"
        }, {
            "und_Moon_ZZ",
            "en_Moon_US",
            "en_Moon"
        }, {
            "und_Moon_CN",
            "zh_Moon_CN",
            "zh_Moon"
        }, {
            "und_Moon_TW",
            "zh_Moon_TW",
            "zh_Moon_TW"
        }, {
            "und_Moon_HK",
            "zh_Moon_HK",
            "zh_Moon_HK"
        }, {
            "und_Moon_AQ",
            "en_Moon_AQ",
            "en_Moon_AQ"
        }, {
            "es",
            "es_Latn_ES",
            "es"
        }, {
            "es_ZZ",
            "es_Latn_ES",
            "es"
        }, {
            "es_CN",
            "es_Latn_CN",
            "es_CN"
        }, {
            "es_TW",
            "es_Latn_TW",
            "es_TW"
        }, {
            "es_HK",
            "es_Latn_HK",
            "es_HK"
        }, {
            "es_AQ",
            "es_Latn_AQ",
            "es_AQ"
        }, {
            "es_Zzzz",
            "es_Latn_ES",
            "es"
        }, {
            "es_Zzzz_ZZ",
            "es_Latn_ES",
            "es"
        }, {
            "es_Zzzz_CN",
            "es_Latn_CN",
            "es_CN"
        }, {
            "es_Zzzz_TW",
            "es_Latn_TW",
            "es_TW"
        }, {
            "es_Zzzz_HK",
            "es_Latn_HK",
            "es_HK"
        }, {
            "es_Zzzz_AQ",
            "es_Latn_AQ",
            "es_AQ"
        }, {
            "es_Latn",
            "es_Latn_ES",
            "es"
        }, {
            "es_Latn_ZZ",
            "es_Latn_ES",
            "es"
        }, {
            "es_Latn_CN",
            "es_Latn_CN",
            "es_CN"
        }, {
            "es_Latn_TW",
            "es_Latn_TW",
            "es_TW"
        }, {
            "es_Latn_HK",
            "es_Latn_HK",
            "es_HK"
        }, {
            "es_Latn_AQ",
            "es_Latn_AQ",
            "es_AQ"
        }, {
            "es_Hans",
            "es_Hans_ES",
            "es_Hans"
        }, {
            "es_Hans_ZZ",
            "es_Hans_ES",
            "es_Hans"
        }, {
            "es_Hans_CN",
            "es_Hans_CN",
            "es_Hans_CN"
        }, {
            "es_Hans_TW",
            "es_Hans_TW",
            "es_Hans_TW"
        }, {
            "es_Hans_HK",
            "es_Hans_HK",
            "es_Hans_HK"
        }, {
            "es_Hans_AQ",
            "es_Hans_AQ",
            "es_Hans_AQ"
        }, {
            "es_Hant",
            "es_Hant_ES",
            "es_Hant"
        }, {
            "es_Hant_ZZ",
            "es_Hant_ES",
            "es_Hant"
        }, {
            "es_Hant_CN",
            "es_Hant_CN",
            "es_Hant_CN"
        }, {
            "es_Hant_TW",
            "es_Hant_TW",
            "es_Hant_TW"
        }, {
            "es_Hant_HK",
            "es_Hant_HK",
            "es_Hant_HK"
        }, {
            "es_Hant_AQ",
            "es_Hant_AQ",
            "es_Hant_AQ"
        }, {
            "es_Moon",
            "es_Moon_ES",
            "es_Moon"
        }, {
            "es_Moon_ZZ",
            "es_Moon_ES",
            "es_Moon"
        }, {
            "es_Moon_CN",
            "es_Moon_CN",
            "es_Moon_CN"
        }, {
            "es_Moon_TW",
            "es_Moon_TW",
            "es_Moon_TW"
        }, {
            "es_Moon_HK",
            "es_Moon_HK",
            "es_Moon_HK"
        }, {
            "es_Moon_AQ",
            "es_Moon_AQ",
            "es_Moon_AQ"
        }, {
            "zh",
            "zh_Hans_CN",
            "zh"
        }, {
            "zh_ZZ",
            "zh_Hans_CN",
            "zh"
        }, {
            "zh_CN",
            "zh_Hans_CN",
            "zh"
        }, {
            "zh_TW",
            "zh_Hant_TW",
            "zh_TW"
        }, {
            "zh_HK",
            "zh_Hant_HK",
            "zh_HK"
        }, {
            "zh_AQ",
            "zh_Hans_AQ",
            "zh_AQ"
        }, {
            "zh_Zzzz",
            "zh_Hans_CN",
            "zh"
        }, {
            "zh_Zzzz_ZZ",
            "zh_Hans_CN",
            "zh"
        }, {
            "zh_Zzzz_CN",
            "zh_Hans_CN",
            "zh"
        }, {
            "zh_Zzzz_TW",
            "zh_Hant_TW",
            "zh_TW"
        }, {
            "zh_Zzzz_HK",
            "zh_Hant_HK",
            "zh_HK"
        }, {
            "zh_Zzzz_AQ",
            "zh_Hans_AQ",
            "zh_AQ"
        }, {
            "zh_Latn",
            "zh_Latn_CN",
            "zh_Latn"
        }, {
            "zh_Latn_ZZ",
            "zh_Latn_CN",
            "zh_Latn"
        }, {
            "zh_Latn_CN",
            "zh_Latn_CN",
            "zh_Latn"
        }, {
            "zh_Latn_TW",
            "zh_Latn_TW",
            "zh_Latn_TW"
        }, {
            "zh_Latn_HK",
            "zh_Latn_HK",
            "zh_Latn_HK"
        }, {
            "zh_Latn_AQ",
            "zh_Latn_AQ",
            "zh_Latn_AQ"
        }, {
            "zh_Hans",
            "zh_Hans_CN",
            "zh"
        }, {
            "zh_Hans_ZZ",
            "zh_Hans_CN",
            "zh"
        }, {
            "zh_Hans_TW",
            "zh_Hans_TW",
            "zh_Hans_TW"
        }, {
            "zh_Hans_HK",
            "zh_Hans_HK",
            "zh_Hans_HK"
        }, {
            "zh_Hans_AQ",
            "zh_Hans_AQ",
            "zh_AQ"
        }, {
            "zh_Hant",
            "zh_Hant_TW",
            "zh_TW"
        }, {
            "zh_Hant_ZZ",
            "zh_Hant_TW",
            "zh_TW"
        }, {
            "zh_Hant_CN",
            "zh_Hant_CN",
            "zh_Hant_CN"
        }, {
            "zh_Hant_AQ",
            "zh_Hant_AQ",
            "zh_Hant_AQ"
        }, {
            "zh_Moon",
            "zh_Moon_CN",
            "zh_Moon"
        }, {
            "zh_Moon_ZZ",
            "zh_Moon_CN",
            "zh_Moon"
        }, {
            "zh_Moon_CN",
            "zh_Moon_CN",
            "zh_Moon"
        }, {
            "zh_Moon_TW",
            "zh_Moon_TW",
            "zh_Moon_TW"
        }, {
            "zh_Moon_HK",
            "zh_Moon_HK",
            "zh_Moon_HK"
        }, {
            "zh_Moon_AQ",
            "zh_Moon_AQ",
            "zh_Moon_AQ"
        }, {
            "art",
            "",
            ""
        }, {
            "art_ZZ",
            "",
            ""
        }, {
            "art_CN",
            "",
            ""
        }, {
            "art_TW",
            "",
            ""
        }, {
            "art_HK",
            "",
            ""
        }, {
            "art_AQ",
            "",
            ""
        }, {
            "art_Zzzz",
            "",
            ""
        }, {
            "art_Zzzz_ZZ",
            "",
            ""
        }, {
            "art_Zzzz_CN",
            "",
            ""
        }, {
            "art_Zzzz_TW",
            "",
            ""
        }, {
            "art_Zzzz_HK",
            "",
            ""
        }, {
            "art_Zzzz_AQ",
            "",
            ""
        }, {
            "art_Latn",
            "",
            ""
        }, {
            "art_Latn_ZZ",
            "",
            ""
        }, {
            "art_Latn_CN",
            "",
            ""
        }, {
            "art_Latn_TW",
            "",
            ""
        }, {
            "art_Latn_HK",
            "",
            ""
        }, {
            "art_Latn_AQ",
            "",
            ""
        }, {
            "art_Hans",
            "",
            ""
        }, {
            "art_Hans_ZZ",
            "",
            ""
        }, {
            "art_Hans_CN",
            "",
            ""
        }, {
            "art_Hans_TW",
            "",
            ""
        }, {
            "art_Hans_HK",
            "",
            ""
        }, {
            "art_Hans_AQ",
            "",
            ""
        }, {
            "art_Hant",
            "",
            ""
        }, {
            "art_Hant_ZZ",
            "",
            ""
        }, {
            "art_Hant_CN",
            "",
            ""
        }, {
            "art_Hant_TW",
            "",
            ""
        }, {
            "art_Hant_HK",
            "",
            ""
        }, {
            "art_Hant_AQ",
            "",
            ""
        }, {
            "art_Moon",
            "",
            ""
        }, {
            "art_Moon_ZZ",
            "",
            ""
        }, {
            "art_Moon_CN",
            "",
            ""
        }, {
            "art_Moon_TW",
            "",
            ""
        }, {
            "art_Moon_HK",
            "",
            ""
        }, {
            "art_Moon_AQ",
            "",
            ""
        }, {
            "aae_Latn_IT",
            "aae_Latn_IT",
            "aae"
        }, {
            "aae_Thai_CO",
            "aae_Thai_CO",
            "aae_Thai_CO"
        }, {
            "und_CW",
            "pap_Latn_CW",
            "pap"
        }, {
            "zh_Hant",
            "zh_Hant_TW",
            "zh_TW"
        }, {
            "zh_Hani",
            "zh_Hani_CN",
            "zh_Hani"
        }, {
            "und",
            "en_Latn_US",
            "en"
        }, {
            "und_Thai",
            "th_Thai_TH",
            "th"
        }, {
            "und_419",
            "es_Latn_419",
            "es_419"
        }, {
            "und_150",
            "en_Latn_150",
            "en_150"
        }, {
            "und_AT",
            "de_Latn_AT",
            "de_AT"
        }, {
            "und_US",
            "en_Latn_US",
            "en"
        }, {
            // ICU-22547
            // unicode_language_id = "root" |
            //   (unicode_language_subtag (sep unicode_script_subtag)?  | unicode_script_subtag)
            //     (sep unicode_region_subtag)?  (sep unicode_variant_subtag)* ;
            // so "aaaa" is a well-formed unicode_language_id
            "aaaa",
            "aaaa",
            "aaaa",
        }, {
            // ICU-22546
            "und-Zzzz",
            "en_Latn_US", // If change, please also update common/unicode/locid.h
            "en"
        }, {
            // ICU-22546
            "en",
            "en_Latn_US", // If change, please also update common/unicode/locid.h
            "en"
        }, {
            // ICU-22546
            "de",
            "de_Latn_DE", // If change, please also update common/unicode/locid.h
            "de"
        }, {
            // ICU-22546
            "sr",
            "sr_Cyrl_RS", // If change, please also update common/unicode/locid.h
            "sr"
        }, {
            // ICU-22546
            "sh",
            "sh",// If change, please also update common/unicode/locid.h
            "sh"
        }, {
            // ICU-22546
            "zh_Hani",
            "zh_Hani_CN", // If change, please also update common/unicode/locid.h
            "zh_Hani"
        }, {
            // ICU-22545
            "en_XA",
            "en_XA",
            "en_XA",
        }, {
            // ICU-22545
            "en_XB",
            "en_XB",
            "en_XB",
        }, {
            // ICU-22545
            "en_XC",
            "en_XC",
            "en_XC",
        }
    };

    for (const auto& item : full_data) {
        const char* const org = item.from;
        const char* const exp = item.add;
        Locale res(org);
        res.addLikelySubtags(status);
        status.errIfFailureAndReset("\"%s\"", org);
        if (exp[0]) {
            assertEquals("addLikelySubtags", exp, res.getName());
        } else {
            assertEquals("addLikelySubtags", org, res.getName());
        }
    }

    for (const auto& item : full_data) {
        const char* const org = item.from;
        const char* const exp = item.remove;
        Locale res(org);
        res.minimizeSubtags(status);
        status.errIfFailureAndReset("\"%s\"", org);
        if (exp[0]) {
            assertEquals("minimizeSubtags", exp, res.getName());
        } else {
            assertEquals("minimizeSubtags", org, res.getName());
        }
    }
}

void
LocaleTest::TestKeywordVariants() {
    static const struct {
        const char *localeID;
        const char *expectedLocaleID;
        //const char *expectedLocaleIDNoKeywords;
        //const char *expectedCanonicalID;
        const char *expectedKeywords[10];
        int32_t numKeywords;
        UErrorCode expectedStatus;
    } testCases[] = {
        {
            "de_DE@  currency = euro; C o ll A t i o n   = Phonebook   ; C alen dar = buddhist   ", 
            "de_DE@calendar=buddhist;collation=Phonebook;currency=euro", 
            //"de_DE",
            //"de_DE@calendar=buddhist;collation=Phonebook;currency=euro", 
            {"calendar", "collation", "currency"},
            3,
            U_ZERO_ERROR
        },
        {
            "de_DE@euro",
            "de_DE@euro",
            //"de_DE",
            //"de_DE@currency=EUR",
            {"","","","","","",""},
            0,
            U_INVALID_FORMAT_ERROR /* must have '=' after '@' */
        }
    };
    UErrorCode status = U_ZERO_ERROR;

    int32_t i = 0, j = 0;
    const char *result = nullptr;
    StringEnumeration *keywords;
    int32_t keyCount = 0;
    const char *keyword = nullptr;
    const UnicodeString *keywordString;
    int32_t keywordLen = 0;

    for(i = 0; i < UPRV_LENGTHOF(testCases); i++) {
        status = U_ZERO_ERROR;
        Locale l(testCases[i].localeID);
        keywords = l.createKeywords(status);

        if(status != testCases[i].expectedStatus) {
            err("Expected to get status %s. Got %s instead\n",
                u_errorName(testCases[i].expectedStatus), u_errorName(status));
        }
        status = U_ZERO_ERROR;
        if(keywords) {
            if((keyCount = keywords->count(status)) != testCases[i].numKeywords) {
                err("Expected to get %i keywords, got %i\n", testCases[i].numKeywords, keyCount);
            }
            if(keyCount) {
                for(j = 0;;) {
                    if((j&1)==0) {
                        if((keyword = keywords->next(&keywordLen, status)) == nullptr) {
                            break;
                        }
                        if(strcmp(keyword, testCases[i].expectedKeywords[j]) != 0) {
                            err("Expected to get keyword value %s, got %s\n", testCases[i].expectedKeywords[j], keyword);
                        }
                    } else {
                        if((keywordString = keywords->snext(status)) == nullptr) {
                            break;
                        }
                        if(*keywordString != UnicodeString(testCases[i].expectedKeywords[j], "")) {
                            err("Expected to get keyword UnicodeString %s, got %s\n", testCases[i].expectedKeywords[j], keyword);
                        }
                    }
                    j++;

                    if(j == keyCount / 2) {
                        // replace keywords with a clone of itself
                        StringEnumeration *k2 = keywords->clone();
                        if(k2 == nullptr || keyCount != k2->count(status)) {
                            errln("KeywordEnumeration.clone() failed");
                        } else {
                            delete keywords;
                            keywords = k2;
                        }
                    }
                }
                keywords->reset(status); // Make sure that reset works.
                for(j = 0;;) {
                    if((keyword = keywords->next(&keywordLen, status)) == nullptr) {
                        break;
                    }
                    if(strcmp(keyword, testCases[i].expectedKeywords[j]) != 0) {
                        err("Expected to get keyword value %s, got %s\n", testCases[i].expectedKeywords[j], keyword);
                    }
                    j++;
                }
            }
            delete keywords;
        }
        result = l.getName();
        if(uprv_strcmp(testCases[i].expectedLocaleID, result) != 0) {
            err("Expected to get \"%s\" from \"%s\". Got \"%s\" instead\n",
                testCases[i].expectedLocaleID, testCases[i].localeID, result);
        }

    }

}


void
LocaleTest::TestCreateUnicodeKeywords() {
    IcuTestErrorCode status(*this, "TestCreateUnicodeKeywords()");

    static const Locale l("de@calendar=buddhist;collation=phonebook");

    LocalPointer<StringEnumeration> keys(l.createUnicodeKeywords(status));
    status.errIfFailureAndReset("\"%s\"", l.getName());

    assertEquals("count", 2, keys->count(status));

    const char* key;
    int32_t resultLength;

    key = keys->next(&resultLength, status);
    status.errIfFailureAndReset("key #1");
    assertEquals("resultLength", 2, resultLength);
    assertTrue("key != nullptr", key != nullptr);
    if (key != nullptr) {
        assertEquals("calendar", "ca", key);
    }

    key = keys->next(&resultLength, status);
    status.errIfFailureAndReset("key #2");
    assertEquals("resultLength", 2, resultLength);
    assertTrue("key != nullptr", key != nullptr);
    if (key != nullptr) {
        assertEquals("collation", "co", key);
    }

    key = keys->next(&resultLength, status);
    status.errIfFailureAndReset("end of keys");
    assertEquals("resultLength", 0, resultLength);
    assertTrue("key == nullptr", key == nullptr);

    const UnicodeString* skey;
    keys->reset(status);  // KeywordEnumeration::reset() never touches status.

    skey = keys->snext(status);
    status.errIfFailureAndReset("skey #1");
    assertTrue("skey != nullptr", skey != nullptr);
    if (skey != nullptr) {
        assertEquals("calendar", "ca", *skey);
    }

    skey = keys->snext(status);
    status.errIfFailureAndReset("skey #2");
    assertTrue("skey != nullptr", skey != nullptr);
    if (skey != nullptr) {
        assertEquals("collation", "co", *skey);
    }

    skey = keys->snext(status);
    status.errIfFailureAndReset("end of keys");
    assertTrue("skey == nullptr", skey == nullptr);
}


void
LocaleTest::TestKeywordVariantParsing() {
    static const struct {
        const char *localeID;
        const char *keyword;
        const char *expectedValue;
    } testCases[] = {
        { "de_DE@  C o ll A t i o n   = Phonebook   ", "collation", "Phonebook" },
        { "de_DE", "collation", ""},
        { "de_DE@collation= PHONEBOOK", "collation", "PHONEBOOK" },
        { "de_DE@ currency = euro   ; CoLLaTion   = PHONEBOOk   ", "collation", "PHONEBOOk" },
    };

    UErrorCode status = U_ZERO_ERROR;

    int32_t i = 0;
    int32_t resultLen = 0;
    char buffer[256];

    for(i = 0; i < UPRV_LENGTHOF(testCases); i++) {
        *buffer = 0;
        Locale l(testCases[i].localeID);
        resultLen = l.getKeywordValue(testCases[i].keyword, buffer, 256, status);
        (void)resultLen;  // Suppress unused variable warning.
        if(uprv_strcmp(testCases[i].expectedValue, buffer) != 0) {
            err("Expected to extract \"%s\" from \"%s\" for keyword \"%s\". Got \"%s\" instead\n",
                testCases[i].expectedValue, testCases[i].localeID, testCases[i].keyword, buffer);
        }
    }
}

void
LocaleTest::TestCreateKeywordSet() {
    IcuTestErrorCode status(*this, "TestCreateKeywordSet()");

    static const Locale l("de@calendar=buddhist;collation=phonebook");

    std::set<std::string> result;
    l.getKeywords<std::string>(
            std::insert_iterator<decltype(result)>(result, result.begin()),
            status);
    status.errIfFailureAndReset("\"%s\"", l.getName());

    assertEquals("set::size()", 2, static_cast<int32_t>(result.size()));
    assertTrue("set::find(\"calendar\")",
               result.find("calendar") != result.end());
    assertTrue("set::find(\"collation\")",
               result.find("collation") != result.end());
}

void
LocaleTest::TestCreateKeywordSetEmpty() {
    IcuTestErrorCode status(*this, "TestCreateKeywordSetEmpty()");

    static const Locale l("de");

    std::set<std::string> result;
    l.getKeywords<std::string>(
            std::insert_iterator<decltype(result)>(result, result.begin()),
            status);
    status.errIfFailureAndReset("\"%s\"", l.getName());

    assertEquals("set::size()", 0, static_cast<int32_t>(result.size()));
}

void
LocaleTest::TestCreateKeywordSetWithPrivateUse() {
    IcuTestErrorCode status(*this, "TestCreateKeywordSetWithPrivateUse()");

    static const char tag[] = "en-US-u-ca-gregory-x-foo";
    static const Locale l = Locale::forLanguageTag(tag, status);
    std::set<std::string> result;
    l.getKeywords<std::string>(
                 std::insert_iterator<decltype(result)>(result, result.begin()),
            status);
    status.errIfFailureAndReset("getKeywords \"%s\"", l.getName());
    assertTrue("getKeywords set::find(\"calendar\")",
               result.find("calendar") != result.end());
    assertTrue("getKeywords set::find(\"ca\")",
               result.find("ca") == result.end());
    assertTrue("getKeywords set::find(\"x\")",
               result.find("x") != result.end());
    assertTrue("getKeywords set::find(\"foo\")",
               result.find("foo") == result.end());
}

void
LocaleTest::TestCreateUnicodeKeywordSet() {
    IcuTestErrorCode status(*this, "TestCreateUnicodeKeywordSet()");

    static const Locale l("de@calendar=buddhist;collation=phonebook");

    std::set<std::string> result;
    l.getUnicodeKeywords<std::string>(
            std::insert_iterator<decltype(result)>(result, result.begin()),
            status);
    status.errIfFailureAndReset("\"%s\"", l.getName());

    assertEquals("set::size()", 2, static_cast<int32_t>(result.size()));
    assertTrue("set::find(\"ca\")",
               result.find("ca") != result.end());
    assertTrue("set::find(\"co\")",
               result.find("co") != result.end());

    LocalPointer<StringEnumeration> se(l.createUnicodeKeywords(status), status);
    status.errIfFailureAndReset("\"%s\" createUnicodeKeywords()", l.getName());
    assertEquals("count()", 2, se->count(status));
    status.errIfFailureAndReset("\"%s\" count()", l.getName());
}

void
LocaleTest::TestCreateUnicodeKeywordSetEmpty() {
    IcuTestErrorCode status(*this, "TestCreateUnicodeKeywordSetEmpty()");

    static const Locale l("de");

    std::set<std::string> result;
    l.getUnicodeKeywords<std::string>(
            std::insert_iterator<decltype(result)>(result, result.begin()),
            status);
    status.errIfFailureAndReset("\"%s\"", l.getName());

    assertEquals("set::size()", 0, static_cast<int32_t>(result.size()));

    LocalPointer<StringEnumeration> se(l.createUnicodeKeywords(status), status);
    assertTrue("createUnicodeKeywords", se.isNull());
    status.expectErrorAndReset(U_MEMORY_ALLOCATION_ERROR);
}

void
LocaleTest::TestCreateUnicodeKeywordSetWithPrivateUse() {
    IcuTestErrorCode status(*this, "TestCreateUnicodeKeywordSetWithPrivateUse()");

    static const char tag[] = "en-US-u-ca-gregory-x-foo";
    static const Locale l = Locale::forLanguageTag(tag, status);

    std::set<std::string> result;
    l.getUnicodeKeywords<std::string>(
            std::insert_iterator<decltype(result)>(result, result.begin()),
            status);
    status.errIfFailureAndReset("getUnicodeKeywords \"%s\"", l.getName());
    assertTrue("getUnicodeKeywords set::find(\"ca\")",
               result.find("ca") != result.end());
    assertTrue("getUnicodeKeywords set::find(\"x\")",
               result.find("x") == result.end());
    assertTrue("getUnicodeKeywords set::find(\"foo\")",
               result.find("foo") == result.end());
    assertEquals("set::size()", 1, static_cast<int32_t>(result.size()));

    LocalPointer<StringEnumeration> se(l.createUnicodeKeywords(status), status);
    status.errIfFailureAndReset("\"%s\" createUnicodeKeywords()", l.getName());
    assertEquals("count()", 1, se->count(status));
    status.errIfFailureAndReset("\"%s\" count()", l.getName());
}

void
LocaleTest::TestGetKeywordValueStdString() {
    IcuTestErrorCode status(*this, "TestGetKeywordValueStdString()");

    static const char tag[] = "fa-u-nu-latn";
    static const char keyword[] = "numbers";
    static const char expected[] = "latn";

    Locale l = Locale::forLanguageTag(tag, status);
    status.errIfFailureAndReset("\"%s\"", tag);

    std::string result = l.getKeywordValue<std::string>(keyword, status);
    status.errIfFailureAndReset("\"%s\"", keyword);
    assertEquals(keyword, expected, result.c_str());
}

void
LocaleTest::TestGetUnicodeKeywordValueStdString() {
    IcuTestErrorCode status(*this, "TestGetUnicodeKeywordValueStdString()");

    static const char keyword[] = "co";
    static const char expected[] = "phonebk";

    static const Locale l("de@calendar=buddhist;collation=phonebook");

    std::string result = l.getUnicodeKeywordValue<std::string>(keyword, status);
    status.errIfFailureAndReset("\"%s\"", keyword);
    assertEquals(keyword, expected, result.c_str());
}

void
LocaleTest::TestSetKeywordValue() {
    static const struct {
        const char *keyword;
        const char *value;
    } testCases[] = {
        { "collation", "phonebook" },
        { "currency", "euro" },
        { "calendar", "buddhist" }
    };

    IcuTestErrorCode status(*this, "TestSetKeywordValue()");

    int32_t i = 0;
    int32_t resultLen = 0;
    char buffer[256];

    Locale l(Locale::getGerman());

    for(i = 0; i < UPRV_LENGTHOF(testCases); i++) {
        l.setKeywordValue(testCases[i].keyword, testCases[i].value, status);
        if(U_FAILURE(status)) {
            err("FAIL: Locale::setKeywordValue failed - %s\n", u_errorName(status));
        }

        *buffer = 0;
        resultLen = l.getKeywordValue(testCases[i].keyword, buffer, 256, status);
        (void)resultLen;  // Suppress unused variable warning.
        if(uprv_strcmp(testCases[i].value, buffer) != 0) {
            err("Expected to extract \"%s\" for keyword \"%s\". Got \"%s\" instead\n",
                testCases[i].value, testCases[i].keyword, buffer);
        }
    }

    // Test long locale
    {
        status.errIfFailureAndReset();
        const char* input =
            "de__POSIX@colnormalization=no;colstrength=primary;currency=eur;"
            "em=default;kv=space;lb=strict;lw=normal;measure=metric;"
            "numbers=latn;rg=atzzzz;sd=atat1";
        const char* expected =
            "de__POSIX@colnormalization=no;colstrength=primary;currency=eur;"
            "em=default;kv=space;lb=strict;lw=normal;measure=metric;"
            "numbers=latn;rg=atzzzz;sd=atat1;ss=none";
        // Bug ICU-21385
        Locale l2(input);
        l2.setKeywordValue("ss", "none", status);
        assertEquals("", expected, l2.getName());
        status.errIfFailureAndReset();
    }
}

void
LocaleTest::TestSetKeywordValueStringPiece() {
    IcuTestErrorCode status(*this, "TestSetKeywordValueStringPiece()");
    Locale l(Locale::getGerman());

    l.setKeywordValue(StringPiece("collation"), StringPiece("phonebook"), status);
    l.setKeywordValue(StringPiece("calendarxxx", 8), StringPiece("buddhistxxx", 8), status);

    static const char expected[] = "de@calendar=buddhist;collation=phonebook";
    assertEquals("", expected, l.getName());
}

void
LocaleTest::TestSetUnicodeKeywordValueStringPiece() {
    IcuTestErrorCode status(*this, "TestSetUnicodeKeywordValueStringPiece()");
    Locale l(Locale::getGerman());

    l.setUnicodeKeywordValue(StringPiece("co"), StringPiece("phonebk"), status);
    status.errIfFailureAndReset();

    l.setUnicodeKeywordValue(StringPiece("caxxx", 2), StringPiece("buddhistxxx", 8), status);
    status.errIfFailureAndReset();

    static const char expected[] = "de@calendar=buddhist;collation=phonebook";
    assertEquals("", expected, l.getName());

    l.setUnicodeKeywordValue("cu", nullptr, status);
    status.errIfFailureAndReset();
    assertEquals("", expected, l.getName());

    l.setUnicodeKeywordValue("!!", nullptr, status);
    assertEquals("status", U_ILLEGAL_ARGUMENT_ERROR, status.reset());
    assertEquals("", expected, l.getName());

    l.setUnicodeKeywordValue("co", "!!", status);
    assertEquals("status", U_ILLEGAL_ARGUMENT_ERROR, status.reset());
    assertEquals("", expected, l.getName());

    l.setUnicodeKeywordValue("co", nullptr, status);
    status.errIfFailureAndReset();

    l.setUnicodeKeywordValue("ca", "", status);
    status.errIfFailureAndReset();

    assertEquals("", Locale::getGerman().getName(), l.getName());
}

void
LocaleTest::TestGetBaseName() {
    static const struct {
        const char *localeID;
        const char *baseName;
    } testCases[] = {
        { "de_DE@  C o ll A t i o n   = Phonebook   ", "de_DE" },
        { "de@currency = euro; CoLLaTion   = PHONEBOOk", "de" },
        { "ja@calendar = buddhist", "ja" },
        { "de-u-co-phonebk", "de"}
    };

    int32_t i = 0;

    for(i = 0; i < UPRV_LENGTHOF(testCases); i++) {
        Locale loc(testCases[i].localeID);
        if(strcmp(testCases[i].baseName, loc.getBaseName())) {
            errln("For locale \"%s\" expected baseName \"%s\", but got \"%s\"",
                testCases[i].localeID, testCases[i].baseName, loc.getBaseName());
            return;
        }
    }

    // Verify that adding a keyword to an existing Locale doesn't change the base name.
    UErrorCode status = U_ZERO_ERROR;
    Locale loc2("en-US");
    if (strcmp("en_US", loc2.getBaseName())) {
        errln("%s:%d Expected \"en_US\", got \"%s\"", __FILE__, __LINE__, loc2.getBaseName());
    }
    loc2.setKeywordValue("key", "value", status);
    if (strcmp("en_US@key=value", loc2.getName())) {
        errln("%s:%d Expected \"en_US@key=value\", got \"%s\"", __FILE__, __LINE__, loc2.getName());
    }
    if (strcmp("en_US", loc2.getBaseName())) {
        errln("%s:%d Expected \"en_US\", got \"%s\"", __FILE__, __LINE__, loc2.getBaseName());
    }
}

/**
 * Compare two locale IDs.  If they are equal, return 0.  If `string'
 * starts with `prefix' plus an additional element, that is, string ==
 * prefix + '_' + x, then return 1.  Otherwise return a value < 0.
 */
static UBool _loccmp(const char* string, const char* prefix) {
    int32_t slen = (int32_t)strlen(string),
            plen = (int32_t)strlen(prefix);
    int32_t c = uprv_strncmp(string, prefix, plen);
    /* 'root' is "less than" everything */
    if (prefix[0] == '\0') {
        return string[0] != '\0';
    }
    if (c) return -1; /* mismatch */
    if (slen == plen) return 0;
    if (string[plen] == '_') return 1;
    return -2; /* false match, e.g. "en_USX" cmp "en_US" */
}

/**
 * Check the relationship between requested locales, and report problems.
 * The caller specifies the expected relationships between requested
 * and valid (expReqValid) and between valid and actual (expValidActual).
 * Possible values are:
 * "gt" strictly greater than, e.g., en_US > en
 * "ge" greater or equal,      e.g., en >= en
 * "eq" equal,                 e.g., en == en
 */
void LocaleTest::_checklocs(const char* label,
                            const char* req,
                            const Locale& validLoc,
                            const Locale& actualLoc,
                            const char* expReqValid,
                            const char* expValidActual) {
    const char* valid = validLoc.getName();
    const char* actual = actualLoc.getName();
    int32_t reqValid = _loccmp(req, valid);
    int32_t validActual = _loccmp(valid, actual);
    if (((0 == uprv_strcmp(expReqValid, "gt") && reqValid > 0) ||
         (0 == uprv_strcmp(expReqValid, "ge") && reqValid >= 0) ||
         (0 == uprv_strcmp(expReqValid, "eq") && reqValid == 0)) &&
        ((0 == uprv_strcmp(expValidActual, "gt") && validActual > 0) ||
         (0 == uprv_strcmp(expValidActual, "ge") && validActual >= 0) ||
         (0 == uprv_strcmp(expValidActual, "eq") && validActual == 0))) {
        logln("%s; req=%s, valid=%s, actual=%s",
              label, req, valid, actual);
    } else {
        dataerrln("FAIL: %s; req=%s, valid=%s, actual=%s.  Require (R %s V) and (V %s A)",
              label, req, valid, actual,
              expReqValid, expValidActual);
    }
}

void LocaleTest::TestGetLocale() {
#if !UCONFIG_NO_SERVICE
    const char *req;
    Locale valid, actual, reqLoc;
    
    // Calendar
#if !UCONFIG_NO_FORMATTING
    {
        UErrorCode ec = U_ZERO_ERROR;  // give each resource type its own error code
        req = "en_US_BROOKLYN";
        Calendar* cal = Calendar::createInstance(Locale::createFromName(req), ec);
        if (U_FAILURE(ec)) {
            dataerrln("FAIL: Calendar::createInstance failed - %s", u_errorName(ec));
        } else {
            valid = cal->getLocale(ULOC_VALID_LOCALE, ec);
            actual = cal->getLocale(ULOC_ACTUAL_LOCALE, ec);
            if (U_FAILURE(ec)) {
                errln("FAIL: Calendar::getLocale() failed");
            } else {
                _checklocs("Calendar", req, valid, actual);
            }
            /* Make sure that it fails correctly */
            ec = U_FILE_ACCESS_ERROR;
            if (cal->getLocale(ULOC_VALID_LOCALE, ec).getName()[0] != 0) {
                errln("FAIL: Calendar::getLocale() failed to fail correctly. It should have returned \"\"");
            }
            ec = U_ZERO_ERROR;
        }
        delete cal;
    }
#endif

    // DecimalFormat, DecimalFormatSymbols
#if !UCONFIG_NO_FORMATTING
    {
        UErrorCode ec = U_ZERO_ERROR;  // give each resource type its own error code
        req = "fr_FR_NICE";
        NumberFormat* nf = NumberFormat::createInstance(Locale::createFromName(req), ec);
        if (U_FAILURE(ec)) {
            dataerrln("FAIL: NumberFormat::createInstance failed - %s", u_errorName(ec));
        } else {
            DecimalFormat* dec = dynamic_cast<DecimalFormat*>(nf);
            if (dec == nullptr) {
                errln("FAIL: NumberFormat::createInstance does not return a DecimalFormat");
                return;
            }
            valid = dec->getLocale(ULOC_VALID_LOCALE, ec);
            actual = dec->getLocale(ULOC_ACTUAL_LOCALE, ec);
            if (U_FAILURE(ec)) {
                errln("FAIL: DecimalFormat::getLocale() failed");
            } else {
                _checklocs("DecimalFormat", req, valid, actual);
            }

            const DecimalFormatSymbols* sym = dec->getDecimalFormatSymbols();
            if (sym == nullptr) {
                errln("FAIL: getDecimalFormatSymbols returned nullptr");
                return;
            }
            valid = sym->getLocale(ULOC_VALID_LOCALE, ec);
            actual = sym->getLocale(ULOC_ACTUAL_LOCALE, ec);
            if (U_FAILURE(ec)) {
                errln("FAIL: DecimalFormatSymbols::getLocale() failed");
            } else {
                _checklocs("DecimalFormatSymbols", req, valid, actual);
            }        
        }
        delete nf;
    }
#endif

    // DateFormat, DateFormatSymbols
#if !UCONFIG_NO_FORMATTING
    {
        UErrorCode ec = U_ZERO_ERROR;  // give each resource type its own error code
        req = "de_CH_LUCERNE";
        DateFormat* df =
            DateFormat::createDateInstance(DateFormat::kDefault,
                                           Locale::createFromName(req));
        if (df == nullptr) {
            dataerrln("Error calling DateFormat::createDateInstance()");
        } else {
            SimpleDateFormat* dat = dynamic_cast<SimpleDateFormat*>(df);
            if (dat == nullptr) {
                errln("FAIL: DateFormat::createInstance does not return a SimpleDateFormat");
                return;
            }
            valid = dat->getLocale(ULOC_VALID_LOCALE, ec);
            actual = dat->getLocale(ULOC_ACTUAL_LOCALE, ec);
            if (U_FAILURE(ec)) {
                errln("FAIL: SimpleDateFormat::getLocale() failed");
            } else {
                _checklocs("SimpleDateFormat", req, valid, actual);
            }
    
            const DateFormatSymbols* sym = dat->getDateFormatSymbols();
            if (sym == nullptr) {
                errln("FAIL: getDateFormatSymbols returned nullptr");
                return;
            }
            valid = sym->getLocale(ULOC_VALID_LOCALE, ec);
            actual = sym->getLocale(ULOC_ACTUAL_LOCALE, ec);
            if (U_FAILURE(ec)) {
                errln("FAIL: DateFormatSymbols::getLocale() failed");
            } else {
                _checklocs("DateFormatSymbols", req, valid, actual);
            }        
        }
        delete df;
    }
#endif

    // BreakIterator
#if !UCONFIG_NO_BREAK_ITERATION
    {
        UErrorCode ec = U_ZERO_ERROR;  // give each resource type its own error code
        req = "es_ES_BARCELONA";
        reqLoc = Locale::createFromName(req);
        BreakIterator* brk = BreakIterator::createWordInstance(reqLoc, ec);
        if (U_FAILURE(ec)) {
            dataerrln("FAIL: BreakIterator::createWordInstance failed - %s", u_errorName(ec));
        } else {
            valid = brk->getLocale(ULOC_VALID_LOCALE, ec);
            actual = brk->getLocale(ULOC_ACTUAL_LOCALE, ec);
            if (U_FAILURE(ec)) {
                errln("FAIL: BreakIterator::getLocale() failed");
            } else {
                _checklocs("BreakIterator", req, valid, actual);
            }
        
            // After registering something, the behavior should be different
            URegistryKey key = BreakIterator::registerInstance(brk, reqLoc, UBRK_WORD, ec);
            brk = nullptr; // registerInstance adopts
            if (U_FAILURE(ec)) {
                errln("FAIL: BreakIterator::registerInstance() failed");
            } else {
                brk = BreakIterator::createWordInstance(reqLoc, ec);
                if (U_FAILURE(ec)) {
                    errln("FAIL: BreakIterator::createWordInstance failed");
                } else {
                    valid = brk->getLocale(ULOC_VALID_LOCALE, ec);
                    actual = brk->getLocale(ULOC_ACTUAL_LOCALE, ec);
                    if (U_FAILURE(ec)) {
                        errln("FAIL: BreakIterator::getLocale() failed");
                    } else {
                        // N.B.: now expect valid==actual==req
                        _checklocs("BreakIterator(registered)",
                                   req, valid, actual, "eq", "eq");
                    }
                }
                // No matter what, unregister
                BreakIterator::unregister(key, ec);
                if (U_FAILURE(ec)) {
                    errln("FAIL: BreakIterator::unregister() failed");
                }
                delete brk;
                brk = nullptr;
            }

            // After unregistering, should behave normally again
            brk = BreakIterator::createWordInstance(reqLoc, ec);
            if (U_FAILURE(ec)) {
                errln("FAIL: BreakIterator::createWordInstance failed");
            } else {
                valid = brk->getLocale(ULOC_VALID_LOCALE, ec);
                actual = brk->getLocale(ULOC_ACTUAL_LOCALE, ec);
                if (U_FAILURE(ec)) {
                    errln("FAIL: BreakIterator::getLocale() failed");
                } else {
                    _checklocs("BreakIterator(unregistered)", req, valid, actual);
                }
            }
        }
        delete brk;
    }
#endif

    // Collator
#if !UCONFIG_NO_COLLATION
    {
        UErrorCode ec = U_ZERO_ERROR;  // give each resource type its own error code

        checkRegisteredCollators(nullptr); // Don't expect any extras

        req = "hi_IN_BHOPAL";
        reqLoc = Locale::createFromName(req);
        Collator* coll = Collator::createInstance(reqLoc, ec);
        if (U_FAILURE(ec)) {
            dataerrln("FAIL: Collator::createInstance failed - %s", u_errorName(ec));
        } else {
            valid = coll->getLocale(ULOC_VALID_LOCALE, ec);
            actual = coll->getLocale(ULOC_ACTUAL_LOCALE, ec);
            if (U_FAILURE(ec)) {
                errln("FAIL: Collator::getLocale() failed");
            } else {
                _checklocs("Collator", req, valid, actual);
            }

            // After registering something, the behavior should be different
            URegistryKey key = Collator::registerInstance(coll, reqLoc, ec);
            coll = nullptr; // registerInstance adopts
            if (U_FAILURE(ec)) {
                errln("FAIL: Collator::registerInstance() failed");
            } else {
                coll = Collator::createInstance(reqLoc, ec);
                if (U_FAILURE(ec)) {
                    errln("FAIL: Collator::createWordInstance failed");
                } else {
                    valid = coll->getLocale(ULOC_VALID_LOCALE, ec);
                    actual = coll->getLocale(ULOC_ACTUAL_LOCALE, ec);
                    if (U_FAILURE(ec)) {
                        errln("FAIL: Collator::getLocale() failed");
                    } else {
                        // N.B.: now expect valid==actual==req
                        _checklocs("Collator(registered)",
                                   req, valid, actual, "eq", "eq");
                    }
                }
                checkRegisteredCollators(req); // include hi_IN_BHOPAL

                // No matter what, unregister
                Collator::unregister(key, ec);
                if (U_FAILURE(ec)) {
                    errln("FAIL: Collator::unregister() failed");
                }
                delete coll;
                coll = nullptr;
            }

            // After unregistering, should behave normally again
            coll = Collator::createInstance(reqLoc, ec);
            if (U_FAILURE(ec)) {
                errln("FAIL: Collator::createInstance failed");
            } else {
                valid = coll->getLocale(ULOC_VALID_LOCALE, ec);
                actual = coll->getLocale(ULOC_ACTUAL_LOCALE, ec);
                if (U_FAILURE(ec)) {
                    errln("FAIL: Collator::getLocale() failed");
                } else {
                    _checklocs("Collator(unregistered)", req, valid, actual);
                }
            }
        }
        delete coll;

        checkRegisteredCollators(nullptr); // extra should be gone again
    }
#endif
#endif
}

#if !UCONFIG_NO_COLLATION
/**
 * Compare Collator::getAvailableLocales(int) [ "old", returning an array ]
 *   with  Collator::getAvailableLocales()    [ "new", returning a StringEnumeration ]
 * These should be identical (check their API docs) EXCEPT that
 * if expectExtra is non-nullptr, it will be in the "new" array but not "old".
 * Does not return any status but calls errln on error.
 * @param expectExtra an extra locale, will be in "new" but not "old". Or nullptr.
 */
void LocaleTest::checkRegisteredCollators(const char *expectExtra) {
    UErrorCode status = U_ZERO_ERROR;
    int32_t count1=0,count2=0;
    Hashtable oldHash(status);
    Hashtable newHash(status);
    assertSuccess(WHERE, status);

    UnicodeString expectStr(expectExtra?expectExtra:"n/a", "");

    // the 'old' list (non enumeration)
    const Locale*  oldList = Collator::getAvailableLocales(count1);
    if(oldList == nullptr) {
        dataerrln("Error: Collator::getAvailableLocales(count) returned nullptr");
        return;
    }

    // the 'new' list (enumeration)
    LocalPointer<StringEnumeration> newEnum(Collator::getAvailableLocales());
    if(newEnum.isNull()) {
       errln("Error: collator::getAvailableLocales() returned nullptr");
       return;
    }

    // OK. Let's add all of the OLD
    // then check for any in the NEW not in OLD
    // then check for any in OLD not in NEW.

    // 1. add all of OLD
    for(int32_t i=0;i<count1;i++) {
        const UnicodeString key(oldList[i].getName(), "");
        int32_t oldI = oldHash.puti(key, 1, status);
        if( oldI == 1 ){
            errln("Error: duplicate key %s in Collator::getAvailableLocales(count) list.\n",
                oldList[i].getName());
            return;
        }
        if(expectExtra != nullptr && !strcmp(expectExtra, oldList[i].getName())) {
            errln("Inexplicably, Collator::getAvailableCollators(count) had registered collator %s. This shouldn't happen, so I am going to consider it an error.\n", expectExtra);
        }
    }

    // 2. add all of NEW
    const UnicodeString *locStr;
    UBool foundExpected = false;
    while((locStr = newEnum->snext(status)) && U_SUCCESS(status)) {
        count2++;

        if(expectExtra != nullptr && expectStr == *locStr) {
            foundExpected = true;
            logln(UnicodeString("Found expected registered collator: ","") + expectStr);
        }
        (void)foundExpected;    // Hush unused variable compiler warning.

        if( oldHash.geti(*locStr) == 0 ) {
            if(expectExtra != nullptr && expectStr==*locStr) {
                logln(UnicodeString("As expected, Collator::getAvailableLocales(count) is missing registered collator ") + expectStr);
            } else {
                errln(UnicodeString("Error: Collator::getAvailableLocales(count) is missing: ","")
                    + *locStr);
            }
        }
        newHash.puti(*locStr, 1, status);
    }

    // 3. check all of OLD again
    for(int32_t i=0;i<count1;i++) {
        const UnicodeString key(oldList[i].getName(), "");
        int32_t newI = newHash.geti(key);
        if(newI == 0) {
            errln(UnicodeString("Error: Collator::getAvailableLocales() is missing: ","")
                + key);
        }
    }

    int32_t expectCount2 = count1;
    if(expectExtra != nullptr) {
        expectCount2 ++; // if an extra item registered, bump the expect count
    }

    assertEquals("Collator::getAvail() count", expectCount2, count2);
}
#endif



void LocaleTest::TestVariantWithOutCountry() {
    Locale loc("en","","POSIX");
    if (0 != strcmp(loc.getVariant(), "POSIX")) {
        errln("FAIL: en__POSIX didn't get parsed correctly - name is %s - expected %s got %s", loc.getName(), "POSIX", loc.getVariant());
    }
    Locale loc2("en","","FOUR");
    if (0 != strcmp(loc2.getVariant(), "FOUR")) {
        errln("FAIL: en__FOUR didn't get parsed correctly - name is %s - expected %s got %s", loc2.getName(), "FOUR", loc2.getVariant());
    }
    Locale loc3("en","Latn","","FOUR");
    if (0 != strcmp(loc3.getVariant(), "FOUR")) {
        errln("FAIL: en_Latn__FOUR didn't get parsed correctly - name is %s - expected %s got %s", loc3.getName(), "FOUR", loc3.getVariant());
    }
    Locale loc4("","Latn","","FOUR");
    if (0 != strcmp(loc4.getVariant(), "FOUR")) {
        errln("FAIL: _Latn__FOUR didn't get parsed correctly - name is %s - expected %s got %s", loc4.getName(), "FOUR", loc4.getVariant());
    }
    Locale loc5("","Latn","US","FOUR");
    if (0 != strcmp(loc5.getVariant(), "FOUR")) {
        errln("FAIL: _Latn_US_FOUR didn't get parsed correctly - name is %s - expected %s got %s", loc5.getName(), "FOUR", loc5.getVariant());
    }
    Locale loc6("de-1901");
    if (0 != strcmp(loc6.getVariant(), "1901")) {
        errln("FAIL: de-1901 didn't get parsed correctly - name is %s - expected %s got %s", loc6.getName(), "1901", loc6.getVariant());
    }
}

static Locale _canonicalize(int32_t selector, /* 0==createFromName, 1==createCanonical, 2==Locale ct */
                            const char* localeID) {
    switch (selector) {
    case 0:
        return Locale::createFromName(localeID);
    case 1:
        return Locale::createCanonical(localeID);
    case 2:
        return {localeID};
    default:
        return {""};
    }
}

void LocaleTest::TestCanonicalization()
{
    static const struct {
        const char *localeID;    /* input */
        const char *getNameID;   /* expected getName() result */
        const char *canonicalID; /* expected canonicalize() result */
    } testCases[] = {
        { "ca_ES-with-extra-stuff-that really doesn't make any sense-unless-you're trying to increase code coverage",
          "ca_ES_WITH_EXTRA_STUFF_THAT REALLY DOESN'T MAKE ANY SENSE_UNLESS_YOU'RE TRYING TO INCREASE CODE COVERAGE",
          "ca_ES_EXTRA_STUFF_THAT REALLY DOESN'T MAKE ANY SENSE_UNLESS_WITH_YOU'RE TRYING TO INCREASE CODE COVERAGE"},
        { "zh@collation=pinyin", "zh@collation=pinyin", "zh@collation=pinyin" },
        { "zh_CN@collation=pinyin", "zh_CN@collation=pinyin", "zh_CN@collation=pinyin" },
        { "zh_CN_CA@collation=pinyin", "zh_CN_CA@collation=pinyin", "zh_CN_CA@collation=pinyin" },
        { "en_US_POSIX", "en_US_POSIX", "en_US_POSIX" }, 
        { "hy_AM_REVISED", "hy_AM_REVISED", "hy_AM_REVISED" }, 
        { "no_NO_NY", "no_NO_NY", "no_NO_NY" /* not: "nn_NO" [alan ICU3.0] */ },
        { "no@ny", "no@ny", "no__NY" /* not: "nn" [alan ICU3.0] */ }, /* POSIX ID */
        { "no-no.utf32@B", "no_NO.utf32@B", "no_NO_B" }, /* POSIX ID */
        { "qz-qz@Euro", "qz_QZ@Euro", "qz_QZ_EURO" }, /* qz-qz uses private use iso codes */

        // A very long charset name in IANA charset
        { "ja_JP.Extended_UNIX_Code_Packed_Format_for_Japanese@B",
          "ja_JP.Extended_UNIX_Code_Packed_Format_for_Japanese@B", "ja_JP_B" }, /* POSIX ID */
        // A fake long charset name below the limitation
        { "ja_JP.1234567890123456789012345678901234567890123456789012345678901234@B",
          "ja_JP.1234567890123456789012345678901234567890123456789012345678901234@B",
          "ja_JP_B" }, /* POSIX ID */
        // A fake long charset name one char above the limitation
        { "ja_JP.12345678901234567890123456789012345678901234567890123456789012345@B",
          "BOGUS",
          "ja_JP_B" }, /* POSIX ID */
        // NOTE: uloc_getName() works on en-BOONT, but Locale() parser considers it BOGUS
        // TODO: unify this behavior
        { "en-BOONT", "en__BOONT", "en__BOONT" }, /* registered name */
        { "de-1901", "de__1901", "de__1901" }, /* registered name */
        { "de-1906", "de__1906", "de__1906" }, /* registered name */
        // New in CLDR 39 / ICU 69
        { "nb", "nb", "nb" },

        /* posix behavior that used to be performed by getName */
        { "mr.utf8", "mr.utf8", "mr" },
        { "de-tv.koi8r", "de_TV.koi8r", "de_TV" },
        { "x-piglatin_ML.MBE", "x-piglatin_ML.MBE", "x-piglatin_ML" },
        { "i-cherokee_US.utf7", "i-cherokee_US.utf7", "i-cherokee_US" },
        { "x-filfli_MT_FILFLA.gb-18030", "x-filfli_MT_FILFLA.gb-18030", "x-filfli_MT_FILFLA" },
        { "no-no-ny.utf8@B", "no_NO_NY.utf8@B", "no_NO@b=ny" /* not: "nn_NO" [alan ICU3.0] */ }, /* @ ignored unless variant is empty */

        /* fleshing out canonicalization */
        /* trim space and sort keywords, ';' is separator so not present at end in canonical form */
        { "en_Hant_IL_VALLEY_GIRL@ currency = EUR; calendar = Japanese ;",
          "en_Hant_IL_VALLEY_GIRL@calendar=Japanese;currency=EUR",
          "en_Hant_IL_GIRL_VALLEY@calendar=Japanese;currency=EUR" },
        /* already-canonical ids are not changed */
        { "en_Hant_IL_VALLEY_GIRL@calendar=Japanese;currency=EUR",
          "en_Hant_IL_VALLEY_GIRL@calendar=Japanese;currency=EUR",
          "en_Hant_IL_GIRL_VALLEY@calendar=Japanese;currency=EUR" },
        /* norwegian is just too weird, if we handle things in their full generality */
        { "no-Hant-GB_NY@currency=$$$", "no_Hant_GB_NY@currency=$$$", "no_Hant_GB_NY@currency=$$$" /* not: "nn_Hant_GB@currency=$$$" [alan ICU3.0] */ },

        /* test cases reflecting internal resource bundle usage */
        { "root@kw=foo", "root@kw=foo", "root@kw=foo" },
        { "@calendar=gregorian", "@calendar=gregorian", "@calendar=gregorian" },
        { "ja_JP@calendar=Japanese", "ja_JP@calendar=Japanese", "ja_JP@calendar=Japanese" },

        // Before ICU 64, ICU locale canonicalization had some additional mappings.
        // They were removed for ICU-20187 "drop support for long-obsolete locale ID variants".
        // The following now use standard canonicalization.
        { "", "", "" },
        { "C", "c", "c" },
        { "POSIX", "posix", "posix" },
        { "ca_ES_PREEURO", "ca_ES_PREEURO", "ca_ES_PREEURO" },
        { "de_AT_PREEURO", "de_AT_PREEURO", "de_AT_PREEURO" },
        { "de_DE_PREEURO", "de_DE_PREEURO", "de_DE_PREEURO" },
        { "de_LU_PREEURO", "de_LU_PREEURO", "de_LU_PREEURO" },
        { "el_GR_PREEURO", "el_GR_PREEURO", "el_GR_PREEURO" },
        { "en_BE_PREEURO", "en_BE_PREEURO", "en_BE_PREEURO" },
        { "en_IE_PREEURO", "en_IE_PREEURO", "en_IE_PREEURO" },
        { "es_ES_PREEURO", "es_ES_PREEURO", "es_ES_PREEURO" },
        { "eu_ES_PREEURO", "eu_ES_PREEURO", "eu_ES_PREEURO" },
        { "fi_FI_PREEURO", "fi_FI_PREEURO", "fi_FI_PREEURO" },
        { "fr_BE_PREEURO", "fr_BE_PREEURO", "fr_BE_PREEURO" },
        { "fr_FR_PREEURO", "fr_FR_PREEURO", "fr_FR_PREEURO" },
        { "fr_LU_PREEURO", "fr_LU_PREEURO", "fr_LU_PREEURO" },
        { "ga_IE_PREEURO", "ga_IE_PREEURO", "ga_IE_PREEURO" },
        { "gl_ES_PREEURO", "gl_ES_PREEURO", "gl_ES_PREEURO" },
        { "it_IT_PREEURO", "it_IT_PREEURO", "it_IT_PREEURO" },
        { "nl_BE_PREEURO", "nl_BE_PREEURO", "nl_BE_PREEURO" },
        { "nl_NL_PREEURO", "nl_NL_PREEURO", "nl_NL_PREEURO" },
        { "pt_PT_PREEURO", "pt_PT_PREEURO", "pt_PT_PREEURO" },
        { "de__PHONEBOOK", "de__PHONEBOOK", "de__PHONEBOOK" },
        { "en_GB_EURO", "en_GB_EURO", "en_GB_EURO" },
        { "en_GB@EURO", "en_GB@EURO", "en_GB_EURO" }, /* POSIX ID */
        { "es__TRADITIONAL", "es__TRADITIONAL", "es__TRADITIONAL" },
        { "hi__DIRECT", "hi__DIRECT", "hi__DIRECT" },
        { "ja_JP_TRADITIONAL", "ja_JP_TRADITIONAL", "ja_JP_TRADITIONAL" },
        { "th_TH_TRADITIONAL", "th_TH_TRADITIONAL", "th_TH_TRADITIONAL" },
        { "zh_TW_STROKE", "zh_TW_STROKE", "zh_TW_STROKE" },
        { "zh__PINYIN", "zh__PINYIN", "zh__PINYIN" },
        { "sr-SP-Cyrl", "sr_SP_CYRL", "sr_SP_CYRL" }, /* .NET name */
        { "sr-SP-Latn", "sr_SP_LATN", "sr_SP_LATN" }, /* .NET name */
        { "sr_YU_CYRILLIC", "sr_YU_CYRILLIC", "sr_RS_CYRILLIC" }, /* Linux name */
        { "uz-UZ-Cyrl", "uz_UZ_CYRL", "uz_UZ_CYRL" }, /* .NET name */
        { "uz-UZ-Latn", "uz_UZ_LATN", "uz_UZ_LATN" }, /* .NET name */
        { "zh-CHS", "zh_CHS", "zh_CHS" }, /* .NET name */
        { "zh-CHT", "zh_CHT", "zh_CHT" }, /* .NET name This may change back to zh_Hant */
        /* PRE_EURO and EURO conversions don't affect other keywords */
        { "es_ES_PREEURO@CALendar=Japanese", "es_ES_PREEURO@calendar=Japanese", "es_ES_PREEURO@calendar=Japanese" },
        { "es_ES_EURO@SHOUT=zipeedeedoodah", "es_ES_EURO@shout=zipeedeedoodah", "es_ES_EURO@shout=zipeedeedoodah" },
        /* currency keyword overrides PRE_EURO and EURO currency */
        { "es_ES_PREEURO@currency=EUR", "es_ES_PREEURO@currency=EUR", "es_ES_PREEURO@currency=EUR" },
        { "es_ES_EURO@currency=ESP", "es_ES_EURO@currency=ESP", "es_ES_EURO@currency=ESP" },
    };
    
    static const char* label[] = { "createFromName", "createCanonical", "Locale" };

    int32_t i, j;
    
    for (i=0; i < UPRV_LENGTHOF(testCases); i++) {
        for (j=0; j<3; ++j) {
            const char* expected = (j==1) ? testCases[i].canonicalID : testCases[i].getNameID;
            Locale loc = _canonicalize(j, testCases[i].localeID);
            const char* getName = loc.isBogus() ? "BOGUS" : loc.getName();
            if(uprv_strcmp(expected, getName) != 0) {
                errln("FAIL: %s(%s).getName() => \"%s\", expected \"%s\"",
                      label[j], testCases[i].localeID, getName, expected);
            } else {
                logln("Ok: %s(%s) => \"%s\"",
                      label[j], testCases[i].localeID, getName);
            }
        }
    }
}

void LocaleTest::TestCanonicalize()
{
    static const struct {
        const char *localeID;    /* input */
        const char *canonicalID; /* expected canonicalize() result */
    } testCases[] = {
        // language _ variant -> language
        { "no-BOKMAL", "nb" },
        // also test with script, country and extensions
        { "no-Cyrl-ID-BOKMAL-u-ca-japanese", "nb-Cyrl-ID-u-ca-japanese" },
        { "no-Cyrl-ID-1901-BOKMAL-xsistemo-u-ca-japanese", "nb-Cyrl-ID-1901-xsistemo-u-ca-japanese" },
        { "no-Cyrl-ID-1901-BOKMAL-u-ca-japanese", "nb-Cyrl-ID-1901-u-ca-japanese" },
        { "no-Cyrl-ID-BOKMAL-xsistemo-u-ca-japanese", "nb-Cyrl-ID-xsistemo-u-ca-japanese" },
        { "no-NYNORSK", "nn" },
        { "no-Cyrl-ID-NYNORSK-u-ca-japanese", "nn-Cyrl-ID-u-ca-japanese" },
        { "aa-SAAHO", "ssy" },
        // also test with script, country and extensions
        { "aa-Deva-IN-SAAHO-u-ca-japanese", "ssy-Deva-IN-u-ca-japanese" },

        // language -> language
        { "aam", "aas" },
        // also test with script, country, variants and extensions
        { "aam-Cyrl-ID-3456-u-ca-japanese", "aas-Cyrl-ID-3456-u-ca-japanese" },

        // language -> language _ Script
        { "sh", "sr-Latn" },
        // also test with script
        { "sh-Cyrl", "sr-Cyrl" },
        // also test with country, variants and extensions
        { "sh-ID-3456-u-ca-roc", "sr-Latn-ID-3456-u-ca-roc" },

        // language -> language _ country
        { "prs", "fa-AF" },
        // also test with country
        { "prs-RU", "fa-RU" },
        // also test with script, variants and extensions
        { "prs-Cyrl-1009-u-ca-roc", "fa-Cyrl-AF-1009-u-ca-roc" },

        { "pa-IN", "pa-IN" },
        // also test with script
        { "pa-Latn-IN", "pa-Latn-IN" },
        // also test with variants and extensions
        { "pa-IN-5678-u-ca-hindi", "pa-IN-5678-u-ca-hindi" },

        { "ky-Cyrl-KG", "ky-Cyrl-KG" },
        // also test with variants and extensions
        { "ky-Cyrl-KG-3456-u-ca-roc", "ky-Cyrl-KG-3456-u-ca-roc" },

        // Test replacement of scriptAlias
        { "en-Qaai", "en-Zinh" },

        // Test replacement of territoryAlias
        // 554 has one replacement
        { "en-554", "en-NZ" },
        { "en-554-u-nu-arab", "en-NZ-u-nu-arab" },
        // 172 has multiple replacements
        // also test with variants
        { "ru-172-1234", "ru-RU-1234" },
        // also test with extensions
        { "ru-172-1234-u-nu-latn", "ru-RU-1234-u-nu-latn" },
        // also test with scripts
        { "uz-172", "uz-UZ" },
        { "uz-Cyrl-172", "uz-Cyrl-UZ" },
        { "uz-Bopo-172", "uz-Bopo-UZ" },
        // also test with variants and scripts
        { "uz-Cyrl-172-5678-u-nu-latn", "uz-Cyrl-UZ-5678-u-nu-latn" },
        // a language not used in this region
        { "fr-172", "fr-RU" },

        // variant
        { "ja-Latn-hepburn-heploc", "ja-Latn-alalc97"},

        { "aaa-Fooo-SU", "aaa-Fooo-RU"},

        // ICU-21344
        { "ku-Arab-NT", "ku-Arab-IQ"},

        // ICU-21402
        { "und-u-rg-no23", "und-u-rg-no50"},
        { "und-u-rg-cn11", "und-u-rg-cnbj"},
        { "und-u-rg-cz10a", "und-u-rg-cz110"},
        { "und-u-rg-fra", "und-u-rg-frges"},
        { "und-u-rg-frg", "und-u-rg-frges"},
        { "und-u-rg-lud", "und-u-rg-lucl"},

        { "und-NO-u-sd-no23", "und-NO-u-sd-no50"},
        { "und-CN-u-sd-cn11", "und-CN-u-sd-cnbj"},
        { "und-CZ-u-sd-cz10a", "und-CZ-u-sd-cz110"},
        { "und-FR-u-sd-fra", "und-FR-u-sd-frges"},
        { "und-FR-u-sd-frg", "und-FR-u-sd-frges"},
        { "und-LU-u-sd-lud", "und-LU-u-sd-lucl"},

        // ICU-21550
        { "und-u-rg-fi01", "und-u-rg-axzzzz"},
        { "und-u-rg-frcp", "und-u-rg-cpzzzz"},
        { "und-u-rg-frpm", "und-u-rg-pmzzzz"},
        { "und-u-rg-usvi", "und-u-rg-vizzzz"},
        { "und-u-rg-cn91", "und-u-rg-hkzzzz"},
        { "und-u-rg-nlaw", "und-u-rg-awzzzz"},

        { "und-NO-u-sd-frre", "und-NO-u-sd-rezzzz"},
        { "und-CN-u-sd-nlcw", "und-CN-u-sd-cwzzzz"},
        { "und-CZ-u-sd-usgu", "und-CZ-u-sd-guzzzz"},
        { "und-FR-u-sd-shta", "und-FR-u-sd-tazzzz"},
        { "und-FR-u-sd-cn71", "und-FR-u-sd-twzzzz"},


        // ICU-21401
        { "cel-gaulish", "xtg"},

        // ICU-21406
        // Inside T extension
        //  Case of Script and Region
        { "ja-kana-jp-t-it-latn-it", "ja-Kana-JP-t-it-latn-it"},
        { "und-t-zh-hani-tw", "und-t-zh-hani-tw"},
        { "und-cyrl-t-und-Latn", "und-Cyrl-t-und-latn"},
        //  Order of singleton
        { "und-u-ca-roc-t-zh", "und-t-zh-u-ca-roc"},
        //  Variant subtags are alphabetically ordered.
        { "sl-t-sl-rozaj-biske-1994", "sl-t-sl-1994-biske-rozaj"},
        // tfield subtags are alphabetically ordered.
        // (Also tests subtag case normalisation.)
        { "DE-T-lv-M0-DIN", "de-t-lv-m0-din"},
        { "DE-T-M0-DIN-K0-QWERTZ", "de-t-k0-qwertz-m0-din"},
        { "DE-T-lv-M0-DIN-K0-QWERTZ", "de-t-lv-k0-qwertz-m0-din"},
        // "true" tvalue subtags aren't removed.
        // (UTS 35 version 36, §3.2.1 claims otherwise, but tkey must be followed by
        // tvalue, so that's likely a spec bug in UTS 35.)
        { "en-t-m0-true", "en-t-m0-true"},
        // tlang subtags are canonicalised.
        { "en-t-iw", "en-t-he"},
        { "en-t-hy-latn-SU", "en-t-hy-latn-am"},
        { "ru-t-ru-cyrl-SU", "ru-t-ru-cyrl-ru"},
        { "fr-t-fr-172", "fr-t-fr-ru"},
        { "und-t-no-latn-BOKMAL", "und-t-nb-latn" },
        { "und-t-sgn-qAAi-NL", "und-t-dse-zinh" },
        // alias of tvalue should be replaced
        { "en-t-m0-NaMeS", "en-t-m0-prprname" },
        { "en-t-s0-ascii-d0-NaMe", "en-t-d0-charname-s0-ascii" },
    };
    int32_t i;
    for (i=0; i < UPRV_LENGTHOF(testCases); i++) {
        UErrorCode status = U_ZERO_ERROR;
        std::string otag = testCases[i].localeID;
        Locale loc = Locale::forLanguageTag(otag.c_str(), status);
        loc.canonicalize(status);
        std::string tag = loc.toLanguageTag<std::string>(status);
        if (tag != testCases[i].canonicalID) {
            errcheckln(status, "FAIL: %s should be canonicalized to %s but got %s - %s",
                       otag.c_str(),
                       testCases[i].canonicalID,
                       tag.c_str(),
                       u_errorName(status));
        }
    }
}

void LocaleTest::TestCurrencyByDate()
{
#if !UCONFIG_NO_FORMATTING
    UErrorCode status = U_ZERO_ERROR;
    UDate date = uprv_getUTCtime();
	char16_t TMP[4] = {0, 0, 0, 0};
	int32_t index = 0;
	int32_t resLen = 0;
    UnicodeString tempStr, resultStr;

	// Cycle through historical currencies
    date = (UDate)-630720000000.0; // pre 1961 - no currency defined
    index = ucurr_countCurrencies("eo_AM", date, &status);
    if (index != 0)
	{
		errcheckln(status, "FAIL: didn't return 0 for eo_AM - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_AM", date, index, TMP, 4, &status);
    if (resLen != 0) {
		errcheckln(status, "FAIL: eo_AM didn't return nullptr - %s", u_errorName(status));
    }
    status = U_ZERO_ERROR;

    date = (UDate)0.0; // 1970 - one currency defined
    index = ucurr_countCurrencies("eo_AM", date, &status);
    if (index != 1)
	{
		errcheckln(status, "FAIL: didn't return 1 for eo_AM - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_AM", date, index, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("SUR");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return SUR for eo_AM - %s", u_errorName(status));
    }

    date = (UDate)693792000000.0; // 1992 - one currency defined
	index = ucurr_countCurrencies("eo_AM", date, &status);
    if (index != 1)
	{
		errcheckln(status, "FAIL: didn't return 1 for eo_AM - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_AM", date, index, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("RUR");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return RUR for eo_AM - %s", u_errorName(status));
    }

	date = (UDate)977616000000.0; // post 1993 - one currency defined
	index = ucurr_countCurrencies("eo_AM", date, &status);
    if (index != 1)
	{
		errcheckln(status, "FAIL: didn't return 1 for eo_AM - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_AM", date, index, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("AMD");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return AMD for eo_AM - %s", u_errorName(status));
    }

    // Locale AD has multiple currencies at once
	date = (UDate)977616000000.0; // year 2001
	index = ucurr_countCurrencies("eo_AD", date, &status);
    if (index != 4)
	{
		errcheckln(status, "FAIL: didn't return 4 for eo_AD - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_AD", date, 1, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("EUR");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return EUR for eo_AD - %s", u_errorName(status));
    }
    resLen = ucurr_forLocaleAndDate("eo_AD", date, 2, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("ESP");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return ESP for eo_AD - %s", u_errorName(status));
    }
    resLen = ucurr_forLocaleAndDate("eo_AD", date, 3, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("FRF");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return FRF for eo_AD - %s", u_errorName(status));
    }
    resLen = ucurr_forLocaleAndDate("eo_AD", date, 4, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("ADP");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return ADP for eo_AD - %s", u_errorName(status));
    }

	date = (UDate)0.0; // year 1970
	index = ucurr_countCurrencies("eo_AD", date, &status);
    if (index != 3)
	{
		errcheckln(status, "FAIL: didn't return 3 for eo_AD - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_AD", date, 1, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("ESP");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return ESP for eo_AD - %s", u_errorName(status));
    }
    resLen = ucurr_forLocaleAndDate("eo_AD", date, 2, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("FRF");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return FRF for eo_AD - %s", u_errorName(status));
    }
    resLen = ucurr_forLocaleAndDate("eo_AD", date, 3, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("ADP");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return ADP for eo_AD - %s", u_errorName(status));
    }

	date = (UDate)-630720000000.0; // year 1950
	index = ucurr_countCurrencies("eo_AD", date, &status);
    if (index != 2)
	{
		errcheckln(status, "FAIL: didn't return 2 for eo_AD - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_AD", date, 1, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("ESP");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return ESP for eo_AD - %s", u_errorName(status));
    }
    resLen = ucurr_forLocaleAndDate("eo_AD", date, 2, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("ADP");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return ADP for eo_AD - %s", u_errorName(status));
    }

	date = (UDate)-2207520000000.0; // year 1900
	index = ucurr_countCurrencies("eo_AD", date, &status);
    if (index != 1)
	{
		errcheckln(status, "FAIL: didn't return 1 for eo_AD - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_AD", date, 1, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("ESP");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return ESP for eo_AD - %s", u_errorName(status));
    }

	// Locale UA has gap between years 1994 - 1996
	date = (UDate)788400000000.0;
	index = ucurr_countCurrencies("eo_UA", date, &status);
    if (index != 0)
	{
		errcheckln(status, "FAIL: didn't return 0 for eo_UA - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_UA", date, index, TMP, 4, &status);
    if (resLen != 0) {
		errcheckln(status, "FAIL: eo_UA didn't return nullptr - %s", u_errorName(status));
    }
    status = U_ZERO_ERROR;

	// Test index bounds
    resLen = ucurr_forLocaleAndDate("eo_UA", date, 100, TMP, 4, &status);
    if (resLen != 0) {
		errcheckln(status, "FAIL: eo_UA didn't return nullptr - %s", u_errorName(status));
    }
    status = U_ZERO_ERROR;

    resLen = ucurr_forLocaleAndDate("eo_UA", date, 0, TMP, 4, &status);
    if (resLen != 0) {
		errcheckln(status, "FAIL: eo_UA didn't return nullptr - %s", u_errorName(status));
    }
    status = U_ZERO_ERROR;

	// Test for bogus locale
	index = ucurr_countCurrencies("eo_QQ", date, &status);
    if (index != 0)
	{
		errcheckln(status, "FAIL: didn't return 0 for eo_QQ - %s", u_errorName(status));
	}
    status = U_ZERO_ERROR;
    resLen = ucurr_forLocaleAndDate("eo_QQ", date, 1, TMP, 4, &status);
    if (resLen != 0) {
		errcheckln(status, "FAIL: eo_QQ didn't return nullptr - %s", u_errorName(status));
    }
    status = U_ZERO_ERROR;
    resLen = ucurr_forLocaleAndDate("eo_QQ", date, 0, TMP, 4, &status);
    if (resLen != 0) {
		errcheckln(status, "FAIL: eo_QQ didn't return nullptr - %s", u_errorName(status));
    }
    status = U_ZERO_ERROR;

    // Cycle through histrocial currencies
	date = (UDate)977616000000.0; // 2001 - one currency
	index = ucurr_countCurrencies("eo_AO", date, &status);
    if (index != 1)
	{
		errcheckln(status, "FAIL: didn't return 1 for eo_AO - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_AO", date, 1, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("AOA");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return AOA for eo_AO - %s", u_errorName(status));
    }

	date = (UDate)819936000000.0; // 1996 - 2 currencies
	index = ucurr_countCurrencies("eo_AO", date, &status);
    if (index != 2)
	{
		errcheckln(status, "FAIL: didn't return 1 for eo_AO - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_AO", date, 1, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("AOR");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return AOR for eo_AO - %s", u_errorName(status));
    }
    resLen = ucurr_forLocaleAndDate("eo_AO", date, 2, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("AON");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return AON for eo_AO - %s", u_errorName(status));
    }

	date = (UDate)662256000000.0; // 1991 - 2 currencies
	index = ucurr_countCurrencies("eo_AO", date, &status);
    if (index != 2)
	{
		errcheckln(status, "FAIL: didn't return 1 for eo_AO - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_AO", date, 1, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("AON");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return AON for eo_AO - %s", u_errorName(status));
    }
    resLen = ucurr_forLocaleAndDate("eo_AO", date, 2, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("AOK");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return AOK for eo_AO - %s", u_errorName(status));
    }

	date = (UDate)315360000000.0; // 1980 - one currency
	index = ucurr_countCurrencies("eo_AO", date, &status);
    if (index != 1)
	{
		errcheckln(status, "FAIL: didn't return 1 for eo_AO - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_AO", date, 1, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("AOK");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return AOK for eo_AO - %s", u_errorName(status));
    }

	date = (UDate)0.0; // 1970 - no currencies
	index = ucurr_countCurrencies("eo_AO", date, &status);
    if (index != 0)
	{
		errcheckln(status, "FAIL: didn't return 1 for eo_AO - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_AO", date, 1, TMP, 4, &status);
    if (resLen != 0) {
		errcheckln(status, "FAIL: eo_AO didn't return nullptr - %s", u_errorName(status));
    }
    status = U_ZERO_ERROR;

    // Test with currency keyword override
	date = (UDate)977616000000.0; // 2001 - two currencies
	index = ucurr_countCurrencies("eo_DE@currency=DEM", date, &status);
    if (index != 2)
	{
		errcheckln(status, "FAIL: didn't return 2 for eo_DE@currency=DEM - %s", u_errorName(status));
	}
    resLen = ucurr_forLocaleAndDate("eo_DE@currency=DEM", date, 1, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("EUR");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return EUR for eo_DE@currency=DEM - %s", u_errorName(status));
    }
    resLen = ucurr_forLocaleAndDate("eo_DE@currency=DEM", date, 2, TMP, 4, &status);
	tempStr.setTo(TMP);
    resultStr.setTo("DEM");
    if (resultStr != tempStr) {
        errcheckln(status, "FAIL: didn't return DEM for eo_DE@currency=DEM - %s", u_errorName(status));
    }

    // Test Euro Support
	status = U_ZERO_ERROR; // reset
    date = uprv_getUTCtime();

    char16_t USD[4];
    ucurr_forLocaleAndDate("en_US", date, 1, USD, 4, &status);
    
	char16_t YEN[4];
    ucurr_forLocaleAndDate("ja_JP", date, 1, YEN, 4, &status);

    ucurr_forLocaleAndDate("en_US", date, 1, TMP, 4, &status);
    if (u_strcmp(USD, TMP) != 0) {
        errcheckln(status, "Fail: en_US didn't return USD - %s", u_errorName(status));
    }
    ucurr_forLocaleAndDate("en_US_Q", date, 1, TMP, 4, &status);
    if (u_strcmp(USD, TMP) != 0) {
        errcheckln(status, "Fail: en_US_Q didn't fallback to en_US - %s", u_errorName(status));
    }
    status = U_ZERO_ERROR; // reset
#endif
}

void LocaleTest::TestGetVariantWithKeywords()
{
  Locale l("en_US_VALLEY@foo=value");
  const char *variant = l.getVariant();
  logln(variant);
  test_assert(strcmp("VALLEY", variant) == 0);

  UErrorCode status = U_ZERO_ERROR;
  char buffer[50];
  int32_t len = l.getKeywordValue("foo", buffer, 50, status);
  buffer[len] = '\0';
  test_assert(strcmp("value", buffer) == 0);
}

void LocaleTest::TestIsRightToLeft() {
    assertFalse("root LTR", Locale::getRoot().isRightToLeft());
    assertFalse("zh LTR", Locale::getChinese().isRightToLeft());
    assertTrue("ar RTL", Locale("ar").isRightToLeft());
    assertTrue("und-EG RTL", Locale("und-EG").isRightToLeft(), false, true);
    assertFalse("fa-Cyrl LTR", Locale("fa-Cyrl").isRightToLeft());
    assertTrue("en-Hebr RTL", Locale("en-Hebr").isRightToLeft());
    assertTrue("ckb RTL", Locale("ckb").isRightToLeft(), false, true);  // Sorani Kurdish
    assertFalse("fil LTR", Locale("fil").isRightToLeft());
    assertFalse("he-Zyxw LTR", Locale("he-Zyxw").isRightToLeft());
}

void LocaleTest::TestBug11421() {
    Locale::getDefault().getBaseName();
    int32_t numLocales;
    const Locale *localeList = Locale::getAvailableLocales(numLocales);
    for (int localeIndex = 0; localeIndex < numLocales; localeIndex++) {
        const Locale &loc = localeList[localeIndex];
        if (strncmp(loc.getName(), loc.getBaseName(), strlen(loc.getBaseName()))) {
            errln("%s:%d loc.getName=\"%s\"; loc.getBaseName=\"%s\"",
                __FILE__, __LINE__, loc.getName(), loc.getBaseName());
            break;
        }
    }
}

//  TestBug13277. The failure manifests as valgrind errors.
//                See the trac ticket for details.
//                

void LocaleTest::TestBug13277() {
    UErrorCode status = U_ZERO_ERROR;
    CharString name("en-us-x-foo", -1, status);
    while (name.length() < 152) {
        name.append("-x-foo", -1, status);
    }

    while (name.length() < 160) {
        name.append('z', status);
        Locale loc(name.data(), nullptr, nullptr, nullptr);
    }
}

// TestBug13554 Check for read past end of array in getPosixID().
//              The bug shows as an Address Sanitizer failure.

void LocaleTest::TestBug13554() {
    UErrorCode status = U_ZERO_ERROR;
    const int BUFFER_SIZE = 100;
    char  posixID[BUFFER_SIZE];

    for (uint32_t hostid = 0; hostid < 0x500; ++hostid) {
        status = U_ZERO_ERROR;
        uprv_convertToPosix(hostid, posixID, BUFFER_SIZE, &status);
    }
}

void LocaleTest::TestBug20410() {
    IcuTestErrorCode status(*this, "TestBug20410()");

    static const char tag1[] = "art-lojban-x-0";
    static const Locale expected1("jbo@x=0");
    Locale result1 = Locale::forLanguageTag(tag1, status);
    status.errIfFailureAndReset("\"%s\"", tag1);
    assertEquals(tag1, expected1.getName(), result1.getName());

    static const char tag2[] = "zh-xiang-u-nu-thai-x-0";
    static const Locale expected2("hsn@numbers=thai;x=0");
    Locale result2 = Locale::forLanguageTag(tag2, status);
    status.errIfFailureAndReset("\"%s\"", tag2);
    assertEquals(tag2, expected2.getName(), result2.getName());

    static const char locid3[] = "art__lojban@x=0";
    Locale result3 = Locale::createCanonical(locid3);
    static const Locale expected3("jbo@x=0");
    assertEquals(locid3, expected3.getName(), result3.getName());

    static const char locid4[] = "art-lojban-x-0";
    Locale result4 = Locale::createCanonical(locid4);
    static const Locale expected4("jbo@x=0");
    assertEquals(locid4, expected4.getName(), result4.getName());
}

void LocaleTest::TestBug20900() {
    static const struct {
        const char *localeID;    /* input */
        const char *canonicalID; /* expected canonicalize() result */
    } testCases[] = {
        { "art-lojban", "jbo" },
        { "zh-guoyu", "zh" },
        { "zh-hakka", "hak" },
        { "zh-xiang", "hsn" },
        { "zh-min-nan", "nan" },
        { "zh-gan", "gan" },
        { "zh-wuu", "wuu" },
        { "zh-yue", "yue" },
    };

    IcuTestErrorCode status(*this, "TestBug20900");
    for (int32_t i=0; i < UPRV_LENGTHOF(testCases); i++) {
        Locale loc = Locale::createCanonical(testCases[i].localeID);
        std::string result = loc.toLanguageTag<std::string>(status);
        const char* tag = loc.isBogus() ? "BOGUS" : result.c_str();
        status.errIfFailureAndReset("FAIL: createCanonical(%s).toLanguageTag() expected \"%s\"",
                    testCases[i].localeID, tag);
        assertEquals("createCanonical", testCases[i].canonicalID, tag);
    }
}

U_DEFINE_LOCAL_OPEN_POINTER(LocalStdioFilePointer, FILE, fclose);
void LocaleTest::TestLocaleCanonicalizationFromFile()
{
    IcuTestErrorCode status(*this, "TestLocaleCanonicalizationFromFile");
    const char *sourceTestDataPath=getSourceTestData(status);
    if(status.errIfFailureAndReset("unable to find the source/test/testdata "
                                      "folder (getSourceTestData())")) {
        return;
    }
    char testPath[400];
    char line[256];
    strcpy(testPath, sourceTestDataPath);
    strcat(testPath, "cldr/localeIdentifiers/localeCanonicalization.txt");
    LocalStdioFilePointer testFile(fopen(testPath, "r"));
    if(testFile.isNull()) {
        errln("unable to open %s", testPath);
        return;
    }
    // Format:
    // <source locale identifier>	;	<expected canonicalized locale identifier>
    while (fgets(line, (int)sizeof(line), testFile.getAlias())!=nullptr) {
        if (line[0] == '#') {
            // ignore any lines start with #
            continue;
        }
        char *semi = strchr(line, ';');
        if (semi == nullptr) {
            // ignore any lines without ;
            continue;
        }
        *semi = '\0'; // null terminiate on the spot of semi
        const char* from = u_skipWhitespace((const char*)line);
        u_rtrim((char*)from);
        const char* to = u_skipWhitespace((const char*)semi + 1);
        u_rtrim((char*)to);
        std::string expect(to);
        // Change the _ to -
        std::transform(expect.begin(), expect.end(), expect.begin(),
                       [](unsigned char c){ return c == '_' ? '-' : c; });

        Locale loc = Locale::createCanonical(from);
        std::string result = loc.toLanguageTag<std::string>(status);
        const char* tag = loc.isBogus() ? "BOGUS" : result.c_str();
        status.errIfFailureAndReset(
            "FAIL: createCanonical(%s).toLanguageTag() expected \"%s\" locale is %s",
            from, tag, loc.getName());
        std::string msg("createCanonical(");
        msg += from;
        msg += ") locale = ";
        msg += loc.getName();
        assertEquals(msg.c_str(), expect.c_str(), tag);
    }
}

std::string trim(const std::string &s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(*start)) {
        start++;
    }

    auto end = s.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));

    return std::string(start, end + 1);
}

// A testing helper class which favorScript when minimizeSubtags.
class FavorScriptLocale : public Locale {
public:
    FavorScriptLocale(const Locale& l) :Locale(l) { }
    void minimizeSubtags(UErrorCode& status) {
        Locale::minimizeSubtags(true, status);
    }
};

void U_CALLCONV
testLikelySubtagsLineFn(void *context,
               char *fields[][2], int32_t fieldCount,
               UErrorCode *pErrorCode) {
    if (U_FAILURE(*pErrorCode)) {
        return;
    }
    (void)fieldCount;
    LocaleTest* THIS = (LocaleTest*)context;
    std::string source(trim(std::string(fields[0][0], fields[0][1]-fields[0][0])));
    std::string addLikely(trim(std::string(fields[1][0], fields[1][1]-fields[1][0])));
    std::string removeFavorScript(trim(std::string(fields[2][0], fields[2][1]-fields[2][0])));
    if (removeFavorScript.length() == 0) {
        removeFavorScript = addLikely;
    }
    std::string removeFavorRegion(trim(std::string(fields[3][0], fields[3][1]-fields[3][0])));

    if (removeFavorRegion.length() == 0) {
        removeFavorRegion = removeFavorScript;
    }
    Locale l = Locale::forLanguageTag(source, *pErrorCode);
    if (U_FAILURE(*pErrorCode)) {
        THIS->errln("forLanguageTag(%s) return error %x %s", source.c_str(),
                    *pErrorCode, u_errorName(*pErrorCode));
        *pErrorCode = U_ZERO_ERROR;
        return;
    }

    Locale actualMax(l);
    actualMax.addLikelySubtags(*pErrorCode);
    if (addLikely == "FAIL") {
        if (uprv_strcmp(l.getName(), actualMax.getName()) != 0) {
            THIS->errln("addLikelySubtags('%s') return should return the same but return '%s'",
                        l.getName(), actualMax.getName());
        }
    } else {
        std::string max = actualMax.toLanguageTag<std::string>(*pErrorCode);
        if (U_FAILURE(*pErrorCode)) {
            THIS->errln("toLanguageTag(%s) return error %x %s", actualMax.getName(),
                        *pErrorCode, u_errorName(*pErrorCode));
            *pErrorCode = U_ZERO_ERROR;
        } else {
            if (max != addLikely) {
                THIS->errln("addLikelySubtags('%s') should return '%s' but got '%s'",
                            source.c_str(), addLikely.c_str(), max.c_str());
            }
        }
    }

    Locale actualMin(l);
    actualMin.minimizeSubtags(*pErrorCode);
    if (removeFavorRegion == "FAIL") {
        if (uprv_strcmp(l.getName(), actualMin.getName()) != 0) {
            THIS->errln("minimizeSubtags('%s') return should return the same but return '%s'",
                        l.getName(), actualMin.getName());
        }
    } else {
        std::string min = actualMin.toLanguageTag<std::string>(*pErrorCode);
        if (U_FAILURE(*pErrorCode)) {
            THIS->errln("toLanguageTag(%s) return error %x %s", actualMin.getName(),
                        *pErrorCode, u_errorName(*pErrorCode));
            *pErrorCode = U_ZERO_ERROR;
        } else {
            if (min != removeFavorRegion) {
                THIS->errln("minimizeSubtags('%s') should return '%s' but got '%s'",
                            source.c_str(), removeFavorRegion.c_str(), min.c_str());
            }
        }
    }

    FavorScriptLocale actualMinFavorScript(l);
    actualMinFavorScript.minimizeSubtags(*pErrorCode);
    if (removeFavorScript == "FAIL") {
        if (uprv_strcmp(l.getName(), actualMinFavorScript.getName()) != 0) {
            THIS->errln("minimizeSubtags('%s') return should return the same but return '%s'",
                        l.getName(), actualMinFavorScript.getName());
        }
    } else {
        std::string min = actualMinFavorScript.toLanguageTag<std::string>(*pErrorCode);
        if (U_FAILURE(*pErrorCode)) {
            THIS->errln("toLanguageTag(%s) favor script return error %x %s", actualMinFavorScript.getName(),
                        *pErrorCode, u_errorName(*pErrorCode));
            *pErrorCode = U_ZERO_ERROR;
        } else {
            if (min != removeFavorScript) {
                THIS->errln("minimizeSubtags('%s') favor script should return '%s' but got '%s'",
                            source.c_str(), removeFavorScript.c_str(), min.c_str());
            }
        }
    }
}

void
LocaleTest::TestDataDrivenLikelySubtags() {
    if (quick) {
        // This test is too slow to run. Only run in -e mode.
        return;
    }
    IcuTestErrorCode errorCode(*this, "TestDataDrivenLikelySubtags()");
    const char* name = "cldr/localeIdentifiers/likelySubtags.txt";
    const char *sourceTestDataPath = getSourceTestData(errorCode);
    if (errorCode.errIfFailureAndReset("unable to find the source/test/testdata "
                                       "folder (getSourceTestData())")) {
        return;
    }
    CharString path(sourceTestDataPath, errorCode);
    path.appendPathPart(name, errorCode);
    LocalStdioFilePointer testFile(fopen(path.data(), "r"));
    if (testFile.isNull()) {
        errln("unable to open %s", path.data());
        return;
    }

    // Columns (c1, c2,...) are separated by semicolons.
    // Leading and trailing spaces and tabs in each column are ignored.
    // Comments are indicated with hash marks.
    const int32_t kNumFields = 4;
    char *fields[kNumFields][2];

    u_parseDelimitedFile(path.data(), ';', fields, kNumFields, testLikelySubtagsLineFn,
                         this, errorCode);
    if (errorCode.errIfFailureAndReset("error parsing %s", name)) {
        return;
    }
}



void LocaleTest::TestKnownCanonicalizedListCorrect()
{
    IcuTestErrorCode status(*this, "TestKnownCanonicalizedListCorrect");
    int32_t numOfKnownCanonicalized;
    const char* const* knownCanonicalized =
        ulocimp_getKnownCanonicalizedLocaleForTest(numOfKnownCanonicalized);
    for (int32_t i = 0; i < numOfKnownCanonicalized; i++) {
        std::string msg("Known Canonicalized Locale is not canonicalized: ");
        assertTrue((msg + knownCanonicalized[i]).c_str(),
                   ulocimp_isCanonicalizedLocaleForTest(knownCanonicalized[i]));
    }
}

void LocaleTest::TestConstructorAcceptsBCP47() {
    IcuTestErrorCode status(*this, "TestConstructorAcceptsBCP47");

    Locale loc1("ar-EG-u-nu-latn");
    Locale loc2("ar-EG@numbers=latn");
    Locale loc3("ar-EG");
    std::string val;

    // Check getKeywordValue "numbers"
    val = loc1.getKeywordValue<std::string>("numbers", status);
    assertEquals("BCP47 syntax has ICU keyword value", "latn", val.c_str());

    val = loc2.getKeywordValue<std::string>("numbers", status);
    assertEquals("ICU syntax has ICU keyword value", "latn", val.c_str());

    val = loc3.getKeywordValue<std::string>("numbers", status);
    assertEquals("Default, ICU keyword", "", val.c_str());

    // Check getUnicodeKeywordValue "nu"
    val = loc1.getUnicodeKeywordValue<std::string>("nu", status);
    assertEquals("BCP47 syntax has short unicode keyword value", "latn", val.c_str());

    val = loc2.getUnicodeKeywordValue<std::string>("nu", status);
    assertEquals("ICU syntax has short unicode keyword value", "latn", val.c_str());

    val = loc3.getUnicodeKeywordValue<std::string>("nu", status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR, "Default, short unicode keyword");

    // Check getUnicodeKeywordValue "numbers"
    val = loc1.getUnicodeKeywordValue<std::string>("numbers", status);
    assertEquals("BCP47 syntax has long unicode keyword value", "latn", val.c_str());

    val = loc2.getUnicodeKeywordValue<std::string>("numbers", status);
    assertEquals("ICU syntax has long unicode keyword value", "latn", val.c_str());

    val = loc3.getUnicodeKeywordValue<std::string>("numbers", status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR, "Default, long unicode keyword");
}

void LocaleTest::TestForLanguageTag() {
    IcuTestErrorCode status(*this, "TestForLanguageTag()");

    static const char tag_en[] = "en-US";
    static const char tag_oed[] = "en-GB-oed";
    static const char tag_af[] = "af-t-ar-i0-handwrit-u-ca-coptic-x-foo";
    static const char tag_ill[] = "!";
    static const char tag_no_nul[] = { 'e', 'n', '-', 'G', 'B' };
    static const char tag_ext[] = "en-GB-1-abc-efg-a-xyz";
    static const char tag_var[] = "sl-rozaj-biske-1994";

    static const Locale loc_en("en_US");
    static const Locale loc_oed("en_GB_OXENDICT");
    static const Locale loc_af("af@calendar=coptic;t=ar-i0-handwrit;x=foo");
    static const Locale loc_null("");
    static const Locale loc_gb("en_GB");
    static const Locale loc_ext("en_GB@1=abc-efg;a=xyz");
    static const Locale loc_var("sl__1994_BISKE_ROZAJ");

    Locale result_en = Locale::forLanguageTag(tag_en, status);
    status.errIfFailureAndReset("\"%s\"", tag_en);
    assertEquals(tag_en, loc_en.getName(), result_en.getName());

    Locale result_oed = Locale::forLanguageTag(tag_oed, status);
    status.errIfFailureAndReset("\"%s\"", tag_oed);
    assertEquals(tag_oed, loc_oed.getName(), result_oed.getName());

    Locale result_af = Locale::forLanguageTag(tag_af, status);
    status.errIfFailureAndReset("\"%s\"", tag_af);
    assertEquals(tag_af, loc_af.getName(), result_af.getName());

    Locale result_var = Locale::forLanguageTag(tag_var, status);
    status.errIfFailureAndReset("\"%s\"", tag_var);
    assertEquals(tag_var, loc_var.getName(), result_var.getName());

    Locale result_ill = Locale::forLanguageTag(tag_ill, status);
    assertEquals(tag_ill, U_ILLEGAL_ARGUMENT_ERROR, status.reset());
    assertTrue(result_ill.getName(), result_ill.isBogus());

    Locale result_null = Locale::forLanguageTag(nullptr, status);
    status.errIfFailureAndReset("nullptr");
    assertEquals("nullptr", loc_null.getName(), result_null.getName());

    StringPiece sp_substr(tag_oed, 5);  // "en-GB", no NUL.
    Locale result_substr = Locale::forLanguageTag(sp_substr, status);
    status.errIfFailureAndReset("\"%.*s\"", sp_substr.size(), sp_substr.data());
    assertEquals(CharString(sp_substr, status).data(),
            loc_gb.getName(), result_substr.getName());

    StringPiece sp_no_nul(tag_no_nul, sizeof tag_no_nul);  // "en-GB", no NUL.
    Locale result_no_nul = Locale::forLanguageTag(sp_no_nul, status);
    status.errIfFailureAndReset("\"%.*s\"", sp_no_nul.size(), sp_no_nul.data());
    assertEquals(CharString(sp_no_nul, status).data(),
            loc_gb.getName(), result_no_nul.getName());

    Locale result_ext = Locale::forLanguageTag(tag_ext, status);
    status.errIfFailureAndReset("\"%s\"", tag_ext);
    assertEquals(tag_ext, loc_ext.getName(), result_ext.getName());

    static const struct {
        const char *inputTag;    /* input */
        const char *expectedID; /* expected forLanguageTag().getName() result */
    } testCases[] = {
      // ICU-21433
      {"und-1994-biske-rozaj", "__1994_BISKE_ROZAJ"},
      {"de-1994-biske-rozaj", "de__1994_BISKE_ROZAJ"},
      {"und-x-private", "@x=private"},
      {"de-1994-biske-rozaj-x-private", "de__1994_BISKE_ROZAJ@x=private"},
      {"und-1994-biske-rozaj-x-private", "__1994_BISKE_ROZAJ@x=private"},
    };
    int32_t i;
    for (i=0; i < UPRV_LENGTHOF(testCases); i++) {
        UErrorCode status = U_ZERO_ERROR;
        std::string otag = testCases[i].inputTag;
        std::string tag = Locale::forLanguageTag(otag.c_str(), status).getName();
        if (tag != testCases[i].expectedID) {
            errcheckln(status, "FAIL: %s should be toLanguageTag to %s but got %s - %s",
                       otag.c_str(),
                       testCases[i].expectedID,
                       tag.c_str(),
                       u_errorName(status));
        }
    }
}

void LocaleTest::TestForLanguageTagLegacyTagBug21676() {
    IcuTestErrorCode status(*this, "TestForLanguageTagLegacyTagBug21676()");
  std::string tag(
      "i-enochian-1nochian-129-515VNTR-64863775-X3il6-110Y101-29-515VNTR-"
      "64863775-153zu-u-Y4-H0-t6-X3-u6-110Y101-X");
  std::string input(tag);
  input += "EXTRA MEMORY AFTER NON-nullptr TERMINATED STRING";
  StringPiece stringp(input.c_str(), tag.length());
  std::string name = Locale::forLanguageTag(stringp, status).getName();
  std::string expected(
      "@x=i-enochian-1nochian-129-515vntr-64863775-x3il6-110y101-29-515vntr-"
      "64863775-153zu-u-y4-h0-t6-x3-u6-110y101-x");
  if (name != expected) {
      errcheckln(
          status,
          "FAIL: forLanguageTag('%s', \n%d).getName() should be \n'%s' but got %s",
          tag.c_str(), tag.length(), expected.c_str(), name.c_str());
  }
}

void LocaleTest::TestToLanguageTag() {
    IcuTestErrorCode status(*this, "TestToLanguageTag()");

    static const Locale loc_c("en_US_POSIX");
    static const Locale loc_en("en_US");
    static const Locale loc_af("af@calendar=coptic;t=ar-i0-handwrit;x=foo");
    static const Locale loc_ext("en@0=abc;a=xyz");
    static const Locale loc_empty("");
    static const Locale loc_ill("!");
    static const Locale loc_variant("sl__ROZAJ_BISKE_1994");

    static const char tag_c[] = "en-US-u-va-posix";
    static const char tag_en[] = "en-US";
    static const char tag_af[] = "af-t-ar-i0-handwrit-u-ca-coptic-x-foo";
    static const char tag_ext[] = "en-0-abc-a-xyz";
    static const char tag_und[] = "und";
    static const char tag_variant[] = "sl-1994-biske-rozaj";

    std::string result;
    StringByteSink<std::string> sink(&result);
    loc_c.toLanguageTag(sink, status);
    status.errIfFailureAndReset("\"%s\"", loc_c.getName());
    assertEquals(loc_c.getName(), tag_c, result.c_str());

    std::string result_c = loc_c.toLanguageTag<std::string>(status);
    status.errIfFailureAndReset("\"%s\"", loc_c.getName());
    assertEquals(loc_c.getName(), tag_c, result_c.c_str());

    std::string result_en = loc_en.toLanguageTag<std::string>(status);
    status.errIfFailureAndReset("\"%s\"", loc_en.getName());
    assertEquals(loc_en.getName(), tag_en, result_en.c_str());

    std::string result_af = loc_af.toLanguageTag<std::string>(status);
    status.errIfFailureAndReset("\"%s\"", loc_af.getName());
    assertEquals(loc_af.getName(), tag_af, result_af.c_str());

    std::string result_ext = loc_ext.toLanguageTag<std::string>(status);
    status.errIfFailureAndReset("\"%s\"", loc_ext.getName());
    assertEquals(loc_ext.getName(), tag_ext, result_ext.c_str());

    std::string result_empty = loc_empty.toLanguageTag<std::string>(status);
    status.errIfFailureAndReset("\"%s\"", loc_empty.getName());
    assertEquals(loc_empty.getName(), tag_und, result_empty.c_str());

    std::string result_ill = loc_ill.toLanguageTag<std::string>(status);
    status.errIfFailureAndReset("\"%s\"", loc_ill.getName());
    assertEquals(loc_ill.getName(), tag_und, result_ill.c_str());

    std::string result_variant = loc_variant.toLanguageTag<std::string>(status);
    status.errIfFailureAndReset("\"%s\"", loc_variant.getName());
    assertEquals(loc_variant.getName(), tag_variant, result_variant.c_str());

    Locale loc_bogus;
    loc_bogus.setToBogus();
    std::string result_bogus = loc_bogus.toLanguageTag<std::string>(status);
    assertEquals("bogus", U_ILLEGAL_ARGUMENT_ERROR, status.reset());
    assertTrue(result_bogus.c_str(), result_bogus.empty());

    static const struct {
        const char *localeID;    /* input */
        const char *expectedID; /* expected toLanguageTag() result */
    } testCases[] = {
      /* ICU-21414 */
      {"und-x-abc-private", "und-x-abc-private"},
      {"und-x-private", "und-x-private"},
      {"und-u-ca-roc-x-private", "und-u-ca-roc-x-private"},
      {"und-US-x-private", "und-US-x-private"},
      {"und-Latn-x-private", "und-Latn-x-private"},
      {"und-1994-biske-rozaj", "und-1994-biske-rozaj"},
      {"und-1994-biske-rozaj-x-private", "und-1994-biske-rozaj-x-private"},
      // ICU-22497
      {"-ins0-ins17Rz-yqyq-UWLF-uRyq-UWLF-uRRyq-UWLF-uR-UWLF-uRns0-ins17Rz-yq-UWLF-uRyq-UWLF-uRRyq-LF-uRyq-UWLF-uRRyq-UWLF-uRq-UWLF-uRyq-UWLF-uRRyq-UWLF-uR", ""},
      // ICU-22504
      {"@attribute=zzo9zzzzzzzs0zzzzzzzzzz55555555555555555555500000000000000000000fffffffffffffffffffffffffzzzzz2mfPAK", ""},
    };
    int32_t i;
    for (i=0; i < UPRV_LENGTHOF(testCases); i++) {
        UErrorCode status = U_ZERO_ERROR;
        std::string otag = testCases[i].localeID;
        std::string tag = Locale::forLanguageTag(otag.c_str(), status).toLanguageTag<std::string>(status);
        if (tag != testCases[i].expectedID) {
            errcheckln(status, "FAIL: %s should be toLanguageTag to %s but got %s - %s",
                       otag.c_str(),
                       testCases[i].expectedID,
                       tag.c_str(),
                       u_errorName(status));
        }
        // Test ICU-22497
        status = U_ZERO_ERROR;
        icu::Locale locale(otag.c_str());
        char buf[245];
        icu::CheckedArrayByteSink sink(buf, sizeof(buf));
        locale.toLanguageTag(sink, status);
    }
}

/* ICU-20310 */
void LocaleTest::TestToLanguageTagOmitTrue() {
    IcuTestErrorCode status(*this, "TestToLanguageTagOmitTrue()");
    assertEquals("en-u-kn should drop true",
                 "en-u-kn", Locale("en-u-kn-true").toLanguageTag<std::string>(status).c_str());
    status.errIfFailureAndReset();
    assertEquals("en-u-kn should drop true",
                 "en-u-kn", Locale("en-u-kn").toLanguageTag<std::string>(status).c_str());
    status.errIfFailureAndReset();

    assertEquals("de-u-co should drop true",
                 "de-u-co", Locale("de-u-co").toLanguageTag<std::string>(status).c_str());
    status.errIfFailureAndReset();
    assertEquals("de-u-co should drop true",
                 "de-u-co", Locale("de-u-co-yes").toLanguageTag<std::string>(status).c_str());
    status.errIfFailureAndReset();
    assertEquals("de@collation=yes should drop true",
                 "de-u-co", Locale("de@collation=yes").toLanguageTag<std::string>(status).c_str());
    status.errIfFailureAndReset();

    assertEquals("cmn-Hans-CN-t-ca-u-ca-x-t-u should drop true",
                 "cmn-Hans-CN-t-ca-u-ca-x-t-u",
                 Locale("cmn-hans-cn-u-ca-t-ca-x-t-u").toLanguageTag<std::string>(status).c_str());
    status.errIfFailureAndReset();
}

void LocaleTest::TestMoveAssign() {
    // ULOC_FULLNAME_CAPACITY == 157 (uloc.h)
    Locale l1("de@collation=phonebook;x="
              "aaaaabbbbbcccccdddddeeeeefffffggggghhhhh"
              "aaaaabbbbbcccccdddddeeeeefffffggggghhhhh"
              "aaaaabbbbbcccccdddddeeeeefffffggggghhhhh"
              "aaaaabbbbbzz");

    Locale l2;
    {
        Locale l3(l1);
        assertTrue("l1 == l3", l1 == l3);
        l2 = std::move(l3);
        assertTrue("l1 == l2", l1 == l2);
        assertTrue("l2 != l3", l2.getName() != l3.getName());
    }

    // This should remain true also after l3 has been destructed.
    assertTrue("l1 == l2, again", l1 == l2);

    Locale l4("de@collation=phonebook");

    Locale l5;
    {
        Locale l6(l4);
        assertTrue("l4 == l6", l4 == l6);
        l5 = std::move(l6);
        assertTrue("l4 == l5", l4 == l5);
        assertTrue("l5 != l6", l5.getName() != l6.getName());
    }

    // This should remain true also after l6 has been destructed.
    assertTrue("l4 == l5, again", l4 == l5);

    Locale l7("vo_Cyrl_AQ_EURO");

    Locale l8;
    {
        Locale l9(l7);
        assertTrue("l7 == l9", l7 == l9);
        l8 = std::move(l9);
        assertTrue("l7 == l8", l7 == l8);
        assertTrue("l8 != l9", l8.getName() != l9.getName());
    }

    // This should remain true also after l9 has been destructed.
    assertTrue("l7 == l8, again", l7 == l8);

    assertEquals("language", l7.getLanguage(), l8.getLanguage());
    assertEquals("script", l7.getScript(), l8.getScript());
    assertEquals("country", l7.getCountry(), l8.getCountry());
    assertEquals("variant", l7.getVariant(), l8.getVariant());
    assertEquals("bogus", l7.isBogus(), l8.isBogus());
}

void LocaleTest::TestMoveCtor() {
    // ULOC_FULLNAME_CAPACITY == 157 (uloc.h)
    Locale l1("de@collation=phonebook;x="
              "aaaaabbbbbcccccdddddeeeeefffffggggghhhhh"
              "aaaaabbbbbcccccdddddeeeeefffffggggghhhhh"
              "aaaaabbbbbcccccdddddeeeeefffffggggghhhhh"
              "aaaaabbbbbzz");

    Locale l3(l1);
    assertTrue("l1 == l3", l1 == l3);
    Locale l2(std::move(l3));
    assertTrue("l1 == l2", l1 == l2);
    assertTrue("l2 != l3", l2.getName() != l3.getName());

    Locale l4("de@collation=phonebook");

    Locale l6(l4);
    assertTrue("l4 == l6", l4 == l6);
    Locale l5(std::move(l6));
    assertTrue("l4 == l5", l4 == l5);
    assertTrue("l5 != l6", l5.getName() != l6.getName());

    Locale l7("vo_Cyrl_AQ_EURO");

    Locale l9(l7);
    assertTrue("l7 == l9", l7 == l9);
    Locale l8(std::move(l9));
    assertTrue("l7 == l8", l7 == l8);
    assertTrue("l8 != l9", l8.getName() != l9.getName());

    assertEquals("language", l7.getLanguage(), l8.getLanguage());
    assertEquals("script", l7.getScript(), l8.getScript());
    assertEquals("country", l7.getCountry(), l8.getCountry());
    assertEquals("variant", l7.getVariant(), l8.getVariant());
    assertEquals("bogus", l7.isBogus(), l8.isBogus());
}

void LocaleTest::TestBug20407iVariantPreferredValue() {
    IcuTestErrorCode status(*this, "TestBug20407iVariantPreferredValue()");

    Locale l = Locale::forLanguageTag("hy-arevela", status);
    status.errIfFailureAndReset("hy-arevela fail");
    assertTrue("!l.isBogus()", !l.isBogus());

    std::string result = l.toLanguageTag<std::string>(status);
    assertEquals(l.getName(), "hy", result.c_str());

    l = Locale::forLanguageTag("hy-arevmda", status);
    status.errIfFailureAndReset("hy-arevmda");
    assertTrue("!l.isBogus()", !l.isBogus());

    result = l.toLanguageTag<std::string>(status);
    assertEquals(l.getName(), "hyw", result.c_str());
}

void LocaleTest::TestBug13417VeryLongLanguageTag() {
    IcuTestErrorCode status(*this, "TestBug13417VeryLongLanguageTag()");

    static const char tag[] =
        "zh-x"
        "-foo-bar-baz-foo-bar-baz-foo-bar-baz-foo-bar-baz"
        "-foo-bar-baz-foo-bar-baz-foo-bar-baz-foo-bar-baz"
        "-foo-bar-baz-foo-bar-baz-foo-bar-baz-foo-bar-baz"
        "-foo-bar-baz-fxx"
    ;

    Locale l = Locale::forLanguageTag(tag, status);
    status.errIfFailureAndReset("\"%s\"", tag);
    assertTrue("!l.isBogus()", !l.isBogus());

    std::string result = l.toLanguageTag<std::string>(status);
    status.errIfFailureAndReset("\"%s\"", l.getName());
    assertEquals("equals", tag, result.c_str());
}

void LocaleTest::TestBug11053UnderlineTimeZone() {
    static const char* const tz_in_ext[] = {
        "etadd",
        "tzdar",
        "eheai",
        "sttms",
        "arirj",
        "arrgl",
        "aruaq",
        "arluq",
        "mxpvr",
        "brbvb",
        "arbue",
        "caycb",
        "brcgr",
        "cayzs",
        "crsjo",
        "caydq",
        "svsal",
        "cafne",
        "caglb",
        "cagoo",
        "tcgdt",
        "ustel",
        "bolpb",
        "uslax",
        "sxphi",
        "mxmex",
        "usnyc",
        "usxul",
        "usndcnt",
        "usndnsl",
        "ttpos",
        "brpvh",
        "prsju",
        "clpuq",
        "caffs",
        "cayek",
        "brrbr",
        "mxstis",
        "dosdq",
        "brsao",
        "gpsbh",
        "casjf",
        "knbas",
        "lccas",
        "vistt",
        "vcsvd",
        "cayyn",
        "cathu",
        "hkhkg",
        "mykul",
        "khpnh",
        "cvrai",
        "gsgrv",
        "shshn",
        "aubhq",
        "auldh",
        "imdgs",
        "smsai",
        "asppg",
        "pgpom",
    };
    static const char* const tzname_with_underline[] = {
        "America/Buenos_Aires",
        "America/Coral_Harbour",
        "America/Los_Angeles",
        "America/Mexico_City",
        "America/New_York",
        "America/Rio_Branco",
        "America/Sao_Paulo",
        "America/St_Johns",
        "America/St_Thomas",
        "Australia/Broken_Hill",
        "Australia/Lord_Howe",
        "Pacific/Pago_Pago",
    };
    std::string locale_str;
    for (int32_t i = 0; i < UPRV_LENGTHOF(tz_in_ext); i++) {
        locale_str = "en-u-tz-";
        locale_str += tz_in_ext[i];
        Locale l(locale_str.c_str());
        assertTrue((locale_str + " !l.isBogus()").c_str(), !l.isBogus());
    }
    for (int32_t i = 0; i < UPRV_LENGTHOF(tzname_with_underline); i++) {
        locale_str = "en@timezone=";
        locale_str +=  tzname_with_underline[i];
        Locale l(locale_str.c_str());
        assertTrue((locale_str + " !l.isBogus()").c_str(), !l.isBogus());
    }
    locale_str = "en_US@timezone=America/Coral_Harbour";
    Locale l2(locale_str.c_str());
    assertTrue((locale_str + " !l2.isBogus()").c_str(), !l2.isBogus());
    locale_str = "en_Latn@timezone=America/New_York";
    Locale l3(locale_str.c_str());
    assertTrue((locale_str + " !l3.isBogus()").c_str(), !l3.isBogus());
    locale_str = "en_Latn_US@timezone=Australia/Broken_Hill";
    Locale l4(locale_str.c_str());
    assertTrue((locale_str + " !l4.isBogus()").c_str(), !l4.isBogus());
    locale_str = "en-u-tz-ciabj";
    Locale l5(locale_str.c_str());
    assertTrue((locale_str + " !l5.isBogus()").c_str(), !l5.isBogus());
    locale_str = "en-US-u-tz-asppg";
    Locale l6(locale_str.c_str());
    assertTrue((locale_str + " !l6.isBogus()").c_str(), !l6.isBogus());
    locale_str = "fil-Latn-u-tz-cvrai";
    Locale l7(locale_str.c_str());
    assertTrue((locale_str + " !l7.isBogus()").c_str(), !l7.isBogus());
    locale_str = "fil-Latn-PH-u-tz-gsgrv";
    Locale l8(locale_str.c_str());
    assertTrue((locale_str + " !l8.isBogus()").c_str(), !l8.isBogus());
}

void LocaleTest::TestUnd() {
    IcuTestErrorCode status(*this, "TestUnd()");

    static const char empty[] = "";
    static const char root[] = "root";
    static const char und[] = "und";

    Locale empty_ctor(empty);
    Locale empty_tag = Locale::forLanguageTag(empty, status);
    status.errIfFailureAndReset("\"%s\"", empty);

    Locale root_ctor(root);
    Locale root_tag = Locale::forLanguageTag(root, status);
    Locale root_build = LocaleBuilder().setLanguageTag(root).build(status);
    status.errIfFailureAndReset("\"%s\"", root);

    Locale und_ctor(und);
    Locale und_tag = Locale::forLanguageTag(und, status);
    Locale und_build = LocaleBuilder().setLanguageTag(und).build(status);
    status.errIfFailureAndReset("\"%s\"", und);

    assertEquals("getName()", empty, empty_ctor.getName());
    assertEquals("getName()", empty, root_ctor.getName());
    assertEquals("getName()", empty, und_ctor.getName());

    assertEquals("getName()", empty, empty_tag.getName());
    assertEquals("getName()", empty, root_tag.getName());
    assertEquals("getName()", empty, und_tag.getName());

    assertEquals("getName()", empty, root_build.getName());
    assertEquals("getName()", empty, und_build.getName());

    assertEquals("toLanguageTag()", und, empty_ctor.toLanguageTag<std::string>(status).c_str());
    assertEquals("toLanguageTag()", und, root_ctor.toLanguageTag<std::string>(status).c_str());
    assertEquals("toLanguageTag()", und, und_ctor.toLanguageTag<std::string>(status).c_str());
    status.errIfFailureAndReset();

    assertEquals("toLanguageTag()", und, empty_tag.toLanguageTag<std::string>(status).c_str());
    assertEquals("toLanguageTag()", und, root_tag.toLanguageTag<std::string>(status).c_str());
    assertEquals("toLanguageTag()", und, und_tag.toLanguageTag<std::string>(status).c_str());
    status.errIfFailureAndReset();

    assertEquals("toLanguageTag()", und, root_build.toLanguageTag<std::string>(status).c_str());
    assertEquals("toLanguageTag()", und, und_build.toLanguageTag<std::string>(status).c_str());
    status.errIfFailureAndReset();

    assertTrue("empty_ctor == empty_tag", empty_ctor == empty_tag);

    assertTrue("root_ctor == root_tag", root_ctor == root_tag);
    assertTrue("root_ctor == root_build", root_ctor == root_build);
    assertTrue("root_tag == root_build", root_tag == root_build);

    assertTrue("und_ctor == und_tag", und_ctor == und_tag);
    assertTrue("und_ctor == und_build", und_ctor == und_build);
    assertTrue("und_tag == und_build", und_tag == und_build);

    assertTrue("empty_ctor == root_ctor", empty_ctor == root_ctor);
    assertTrue("empty_ctor == und_ctor", empty_ctor == und_ctor);
    assertTrue("root_ctor == und_ctor", root_ctor == und_ctor);

    assertTrue("empty_tag == root_tag", empty_tag == root_tag);
    assertTrue("empty_tag == und_tag", empty_tag == und_tag);
    assertTrue("root_tag == und_tag", root_tag == und_tag);

    assertTrue("root_build == und_build", root_build == und_build);

    static const Locale& displayLocale = Locale::getEnglish();
    static const UnicodeString displayName("Unknown language");
    UnicodeString tmp;

    assertEquals("getDisplayName()", displayName, empty_ctor.getDisplayName(displayLocale, tmp));
    assertEquals("getDisplayName()", displayName, root_ctor.getDisplayName(displayLocale, tmp));
    assertEquals("getDisplayName()", displayName, und_ctor.getDisplayName(displayLocale, tmp));

    assertEquals("getDisplayName()", displayName, empty_tag.getDisplayName(displayLocale, tmp));
    assertEquals("getDisplayName()", displayName, root_tag.getDisplayName(displayLocale, tmp));
    assertEquals("getDisplayName()", displayName, und_tag.getDisplayName(displayLocale, tmp));

    assertEquals("getDisplayName()", displayName, root_build.getDisplayName(displayLocale, tmp));
    assertEquals("getDisplayName()", displayName, und_build.getDisplayName(displayLocale, tmp));
}

void LocaleTest::TestUndScript() {
    IcuTestErrorCode status(*this, "TestUndScript()");

    static const char id[] = "_Cyrl";
    static const char tag[] = "und-Cyrl";
    static const char script[] = "Cyrl";

    Locale locale_ctor(id);
    Locale locale_legacy(tag);
    Locale locale_tag = Locale::forLanguageTag(tag, status);
    Locale locale_build = LocaleBuilder().setScript(script).build(status);
    status.errIfFailureAndReset("\"%s\"", tag);

    assertEquals("getName()", id, locale_ctor.getName());
    assertEquals("getName()", id, locale_legacy.getName());
    assertEquals("getName()", id, locale_tag.getName());
    assertEquals("getName()", id, locale_build.getName());

    assertEquals("toLanguageTag()", tag, locale_ctor.toLanguageTag<std::string>(status).c_str());
    assertEquals("toLanguageTag()", tag, locale_legacy.toLanguageTag<std::string>(status).c_str());
    assertEquals("toLanguageTag()", tag, locale_tag.toLanguageTag<std::string>(status).c_str());
    assertEquals("toLanguageTag()", tag, locale_build.toLanguageTag<std::string>(status).c_str());
    status.errIfFailureAndReset();

    static const Locale& displayLocale = Locale::getEnglish();
    static const UnicodeString displayName("Unknown language (Cyrillic)");
    UnicodeString tmp;

    assertEquals("getDisplayName()", displayName, locale_ctor.getDisplayName(displayLocale, tmp));
    assertEquals("getDisplayName()", displayName, locale_legacy.getDisplayName(displayLocale, tmp));
    assertEquals("getDisplayName()", displayName, locale_tag.getDisplayName(displayLocale, tmp));
    assertEquals("getDisplayName()", displayName, locale_build.getDisplayName(displayLocale, tmp));
}

void LocaleTest::TestUndRegion() {
    IcuTestErrorCode status(*this, "TestUndRegion()");

    static const char id[] = "_AQ";
    static const char tag[] = "und-AQ";
    static const char region[] = "AQ";

    Locale locale_ctor(id);
    Locale locale_legacy(tag);
    Locale locale_tag = Locale::forLanguageTag(tag, status);
    Locale locale_build = LocaleBuilder().setRegion(region).build(status);
    status.errIfFailureAndReset("\"%s\"", tag);

    assertEquals("getName()", id, locale_ctor.getName());
    assertEquals("getName()", id, locale_legacy.getName());
    assertEquals("getName()", id, locale_tag.getName());
    assertEquals("getName()", id, locale_build.getName());

    assertEquals("toLanguageTag()", tag, locale_ctor.toLanguageTag<std::string>(status).c_str());
    assertEquals("toLanguageTag()", tag, locale_legacy.toLanguageTag<std::string>(status).c_str());
    assertEquals("toLanguageTag()", tag, locale_tag.toLanguageTag<std::string>(status).c_str());
    assertEquals("toLanguageTag()", tag, locale_build.toLanguageTag<std::string>(status).c_str());
    status.errIfFailureAndReset();

    static const Locale& displayLocale = Locale::getEnglish();
    static const UnicodeString displayName("Unknown language (Antarctica)");
    UnicodeString tmp;

    assertEquals("getDisplayName()", displayName, locale_ctor.getDisplayName(displayLocale, tmp));
    assertEquals("getDisplayName()", displayName, locale_legacy.getDisplayName(displayLocale, tmp));
    assertEquals("getDisplayName()", displayName, locale_tag.getDisplayName(displayLocale, tmp));
    assertEquals("getDisplayName()", displayName, locale_build.getDisplayName(displayLocale, tmp));
}

void LocaleTest::TestUndCAPI() {
    IcuTestErrorCode status(*this, "TestUndCAPI()");

    static const char empty[] = "";
    static const char root[] = "root";
    static const char und[] = "und";

    static const char empty_script[] = "_Cyrl";
    static const char empty_region[] = "_AQ";

    static const char und_script[] = "und_Cyrl";
    static const char und_region[] = "und_AQ";

    char tmp[ULOC_FULLNAME_CAPACITY];
    int32_t reslen;

    // uloc_getName()

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getName(empty, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", empty);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getName()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getName(root, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", root);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getName()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getName(und, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", und);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getName()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getName(empty_script, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", empty_script);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getName()", empty_script, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getName(empty_region, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", empty_region);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getName()", empty_region, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getName(und_script, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", und_script);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getName()", empty_script, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getName(und_region, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", und_region);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getName()", empty_region, tmp);

    // uloc_getBaseName()

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getBaseName(empty, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", empty);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getBaseName()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getBaseName(root, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", root);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getBaseName()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getBaseName(und, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", und);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getBaseName()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getBaseName(empty_script, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", empty_script);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getBaseName()", empty_script, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getBaseName(empty_region, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", empty_region);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getBaseName()", empty_region, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getBaseName(und_script, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", und_script);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getBaseName()", empty_script, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getBaseName(und_region, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", und_region);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getBaseName()", empty_region, tmp);

    // uloc_getParent()

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getParent(empty, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", empty);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getParent()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getParent(root, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", root);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getParent()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getParent(und, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", und);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getParent()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getParent(empty_script, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", empty_script);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getParent()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getParent(empty_region, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", empty_region);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getParent()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getParent(und_script, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", und_script);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getParent()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getParent(und_region, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", und_region);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getParent()", empty, tmp);

    // uloc_getLanguage()

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getLanguage(empty, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", empty);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getLanguage()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getLanguage(root, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", root);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getLanguage()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getLanguage(und, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", und);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getLanguage()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getLanguage(empty_script, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", empty_script);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getLanguage()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getLanguage(empty_region, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", empty_region);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getLanguage()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getLanguage(und_script, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", und_script);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getLanguage()", empty, tmp);

    uprv_memset(tmp, '!', sizeof tmp);
    reslen = uloc_getLanguage(und_region, tmp, sizeof tmp, status);
    status.errIfFailureAndReset("\"%s\"", und_region);
    assertTrue("reslen >= 0", reslen >= 0);
    assertEquals("uloc_getLanguage()", empty, tmp);
}

#define ARRAY_RANGE(array) (array), ((array) + UPRV_LENGTHOF(array))

void LocaleTest::TestRangeIterator() {
    IcuTestErrorCode status(*this, "TestRangeIterator");
    Locale locales[] = { "fr", "en_GB", "en" };
    Locale::RangeIterator<Locale *> iter(ARRAY_RANGE(locales));

    assertTrue("0.hasNext()", iter.hasNext());
    const Locale &l0 = iter.next();
    assertEquals("0.next()", "fr", l0.getName());
    assertTrue("&0.next()", &l0 == &locales[0]);

    assertTrue("1.hasNext()", iter.hasNext());
    const Locale &l1 = iter.next();
    assertEquals("1.next()", "en_GB", l1.getName());
    assertTrue("&1.next()", &l1 == &locales[1]);

    assertTrue("2.hasNext()", iter.hasNext());
    const Locale &l2 = iter.next();
    assertEquals("2.next()", "en", l2.getName());
    assertTrue("&2.next()", &l2 == &locales[2]);

    assertFalse("3.hasNext()", iter.hasNext());
}

void LocaleTest::TestPointerConvertingIterator() {
    IcuTestErrorCode status(*this, "TestPointerConvertingIterator");
    Locale locales[] = { "fr", "en_GB", "en" };
    Locale *pointers[] = { locales, locales + 1, locales + 2 };
    // Lambda with explicit reference return type to prevent copy-constructing a temporary
    // which would be destructed right away.
    Locale::ConvertingIterator<Locale **, std::function<const Locale &(const Locale *)>> iter(
        ARRAY_RANGE(pointers), [](const Locale *p) -> const Locale & { return *p; });

    assertTrue("0.hasNext()", iter.hasNext());
    const Locale &l0 = iter.next();
    assertEquals("0.next()", "fr", l0.getName());
    assertTrue("&0.next()", &l0 == pointers[0]);

    assertTrue("1.hasNext()", iter.hasNext());
    const Locale &l1 = iter.next();
    assertEquals("1.next()", "en_GB", l1.getName());
    assertTrue("&1.next()", &l1 == pointers[1]);

    assertTrue("2.hasNext()", iter.hasNext());
    const Locale &l2 = iter.next();
    assertEquals("2.next()", "en", l2.getName());
    assertTrue("&2.next()", &l2 == pointers[2]);

    assertFalse("3.hasNext()", iter.hasNext());
}

namespace {

class LocaleFromTag {
public:
    LocaleFromTag() : locale(Locale::getRoot()) {}
    const Locale &operator()(const char *tag) { return locale = Locale(tag); }

private:
    // Store the locale in the converter, rather than return a reference to a temporary,
    // or a value which could go out of scope with the caller's reference to it.
    Locale locale;
};

}  // namespace

void LocaleTest::TestTagConvertingIterator() {
    IcuTestErrorCode status(*this, "TestTagConvertingIterator");
    const char *tags[] = { "fr", "en_GB", "en" };
    LocaleFromTag converter;
    Locale::ConvertingIterator<const char **, LocaleFromTag> iter(ARRAY_RANGE(tags), converter);

    assertTrue("0.hasNext()", iter.hasNext());
    const Locale &l0 = iter.next();
    assertEquals("0.next()", "fr", l0.getName());

    assertTrue("1.hasNext()", iter.hasNext());
    const Locale &l1 = iter.next();
    assertEquals("1.next()", "en_GB", l1.getName());

    assertTrue("2.hasNext()", iter.hasNext());
    const Locale &l2 = iter.next();
    assertEquals("2.next()", "en", l2.getName());

    assertFalse("3.hasNext()", iter.hasNext());
}

void LocaleTest::TestCapturingTagConvertingIterator() {
    IcuTestErrorCode status(*this, "TestCapturingTagConvertingIterator");
    const char *tags[] = { "fr", "en_GB", "en" };
    // Store the converted locale in a locale variable,
    // rather than return a reference to a temporary,
    // or a value which could go out of scope with the caller's reference to it.
    Locale locale;
    // Lambda with explicit reference return type to prevent copy-constructing a temporary
    // which would be destructed right away.
    Locale::ConvertingIterator<const char **, std::function<const Locale &(const char *)>> iter(
        ARRAY_RANGE(tags), [&](const char *tag) -> const Locale & { return locale = Locale(tag); });

    assertTrue("0.hasNext()", iter.hasNext());
    const Locale &l0 = iter.next();
    assertEquals("0.next()", "fr", l0.getName());

    assertTrue("1.hasNext()", iter.hasNext());
    const Locale &l1 = iter.next();
    assertEquals("1.next()", "en_GB", l1.getName());

    assertTrue("2.hasNext()", iter.hasNext());
    const Locale &l2 = iter.next();
    assertEquals("2.next()", "en", l2.getName());

    assertFalse("3.hasNext()", iter.hasNext());
}

void LocaleTest::TestSetUnicodeKeywordValueInLongLocale() {
    IcuTestErrorCode status(*this, "TestSetUnicodeKeywordValueInLongLocale");
    const char* value = "efghijkl";
    icu::Locale l("de");
    char keyword[3];
    CharString expected("de-u", status);
    keyword[2] = '\0';
    for (char i = 'a'; i < 's'; i++) {
        keyword[0] = keyword[1] = i;
        expected.append("-", status);
        expected.append(keyword, status);
        expected.append("-", status);
        expected.append(value, status);
        l.setUnicodeKeywordValue(keyword, value, status);
        if (status.errIfFailureAndReset(
            "setUnicodeKeywordValue(\"%s\", \"%s\") fail while locale is \"%s\"",
            keyword, value, l.getName())) {
            return;
        }
        std::string tag = l.toLanguageTag<std::string>(status);
        if (status.errIfFailureAndReset(
            "toLanguageTag fail on \"%s\"", l.getName())) {
            return;
        }
        if (tag != expected.data()) {
            errln("Expected to get \"%s\" bug got \"%s\"", tag.c_str(),
                  expected.data());
            return;
        }
    }
}

void LocaleTest::TestLongLocaleSetKeywordAssign() {
    IcuTestErrorCode status(*this, "TestLongLocaleSetKeywordAssign");
    // A long base name, with an illegal keyword and copy constructor
    icu::Locale l("de_AAAAAAA1_AAAAAAA2_AAAAAAA3_AAAAAAA4_AAAAAAA5_AAAAAAA6_"
                  "AAAAAAA7_AAAAAAA8_AAAAAAA9_AAAAAA10_AAAAAA11_AAAAAA12_"
                  "AAAAAA13_AAAAAA14_AAAAAA15_AAAAAA16_AAAAAA17_AAAAAA18");
    Locale l2;
    l.setUnicodeKeywordValue("co", "12", status); // Cause an error
    status.reset();
    l2 = l; // copy operator on such bogus locale.
}

void LocaleTest::TestLongLocaleSetKeywordMoveAssign() {
    IcuTestErrorCode status(*this, "TestLongLocaleSetKeywordMoveAssign");
    // A long base name, with an illegal keyword and copy constructor
    icu::Locale l("de_AAAAAAA1_AAAAAAA2_AAAAAAA3_AAAAAAA4_AAAAAAA5_AAAAAAA6_"
                  "AAAAAAA7_AAAAAAA8_AAAAAAA9_AAAAAA10_AAAAAA11_AAAAAA12_"
                  "AAAAAA13_AAAAAA14_AAAAAA15_AAAAAA16_AAAAAA17");
    Locale l2;
    l.setUnicodeKeywordValue("co", "12", status); // Cause an error
    status.reset();
    Locale l3 = std::move(l); // move assign
}

void LocaleTest::TestSetUnicodeKeywordValueNullInLongLocale() {
    IcuTestErrorCode status(*this, "TestSetUnicodeKeywordValueNullInLongLocale");
    const char *exts[] = {"cf", "cu", "em", "kk", "kr", "ks", "kv", "lb", "lw",
      "ms", "nu", "rg", "sd", "ss", "tz"};
    for (int32_t i = 0; i < UPRV_LENGTHOF(exts); i++) {
        CharString tag("de-u", status);
        for (int32_t j = 0; j <= i; j++) {
            tag.append("-", status).append(exts[j], status);
        }
        if (status.errIfFailureAndReset(
                "Cannot create tag \"%s\"", tag.data())) {
            continue;
        }
        Locale l = Locale::forLanguageTag(tag.data(), status);
        if (status.errIfFailureAndReset(
                "Locale::forLanguageTag(\"%s\") failed", tag.data())) {
            continue;
        }
        for (int32_t j = 0; j <= i; j++) {
            l.setUnicodeKeywordValue(exts[j], nullptr, status);
            if (status.errIfFailureAndReset(
                    "Locale(\"%s\").setUnicodeKeywordValue(\"%s\", nullptr) failed",
                    tag.data(), exts[j])) {
                 continue;
            }
        }
        if (strcmp("de", l.getName()) != 0) {
            errln("setUnicodeKeywordValue should remove all extensions from "
                  "\"%s\" and only have \"de\", but is \"%s\" instead.",
                  tag.data(), l.getName());
        }
    }
}

void LocaleTest::TestLeak21419() {
    IcuTestErrorCode status(*this, "TestLeak21419");
    Locale l = Locale("s-yU");
    l.canonicalize(status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
}

void LocaleTest::TestNullDereferenceWrite21597() {
    IcuTestErrorCode status(*this, "TestNullDereferenceWrite21597");
    Locale l = Locale("zu-t-q5-X1-vKf-KK-Ks-cO--Kc");
    l.canonicalize(status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
}
#if !UCONFIG_NO_FORMATTING
void LocaleTest::TestSierraLeoneCurrency21997() {
    // CLDR 41: Check that currency of Sierra Leone is SLL (which is legal tender)
    // and not the newer currency SLE (which is not legal tender), as of CLDR 41.
    // Test will fail once SLE is declared legal.
    // CLDR 42: Now check that currency of Sierra Leone is SLE (which is legal tender)
    UnicodeString sllStr("SLE", ""), resultStr;
    char16_t tmp[4];
    UErrorCode status = U_ZERO_ERROR;

    ucurr_forLocale("en_SL", tmp, 4, &status);
    resultStr.setTo(tmp);
    if (sllStr != resultStr) {
        errcheckln(status, "Fail: en_SL didn't return SLE - %s", u_errorName(status));
    }
}
#endif
