// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include <iterator>
#include <set>
#include <utility>

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
    test_assert(    Test (should be TRUE)  )

   Example:
       test_assert(i==3);

   the macro is ugly but makes the tests pretty.
*/

#define test_assert(test) \
    { \
        if(!(test)) \
            errln("FAIL: " #test " was not true. In " __FILE__ " on line %d", __LINE__ ); \
        else \
            logln("PASS: asserted " #test); \
    }

/*
 Usage:
    test_assert_print(    Test (should be TRUE),  printable  )

   Example:
       test_assert(i==3, toString(i));

   the macro is ugly but makes the tests pretty.
*/

#define test_assert_print(test,print) \
    { \
        if(!(test)) \
            errln("FAIL: " #test " was not true. " + UnicodeString(print) ); \
        else \
            logln("PASS: asserted " #test "-> " + UnicodeString(print)); \
    }


#define test_dumpLocale(l) { logln(#l " = " + UnicodeString(l.getName(), "")); }

LocaleTest::LocaleTest()
: dataTable(NULL)
{
    setUpDataTable();
}

LocaleTest::~LocaleTest()
{
    if (dataTable != 0) {
        for (int32_t i = 0; i < 33; i++) {
            delete []dataTable[i];
        }
        delete []dataTable;
        dataTable = 0;
    }
}

void LocaleTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestBug11421);         // Must run early in list to trigger failure.
    TESTCASE_AUTO(TestBasicGetters);
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
#if !UCONFIG_NO_FORMATTING
    TESTCASE_AUTO(Test4105828);
#endif
    TESTCASE_AUTO(TestSetIsBogus);
    TESTCASE_AUTO(TestParallelAPIValues);
    TESTCASE_AUTO(TestAddLikelySubtags);
    TESTCASE_AUTO(TestMinimizeSubtags);
    TESTCASE_AUTO(TestAddLikelyAndMinimizeSubtags);
    TESTCASE_AUTO(TestKeywordVariants);
    TESTCASE_AUTO(TestCreateUnicodeKeywords);
    TESTCASE_AUTO(TestKeywordVariantParsing);
    TESTCASE_AUTO(TestCreateKeywordSet);
    TESTCASE_AUTO(TestCreateUnicodeKeywordSet);
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
    TESTCASE_AUTO(TestForLanguageTag);
    TESTCASE_AUTO(TestToLanguageTag);
    TESTCASE_AUTO(TestMoveAssign);
    TESTCASE_AUTO(TestMoveCtor);
    TESTCASE_AUTO(TestBug20407iVariantPreferredValue);
    TESTCASE_AUTO(TestBug13417VeryLongLanguageTag);
    TESTCASE_AUTO(TestBug11053UnderlineTimeZone);
    TESTCASE_AUTO(TestUnd);
    TESTCASE_AUTO(TestUndScript);
    TESTCASE_AUTO(TestUndRegion);
    TESTCASE_AUTO(TestUndCAPI);
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

    if (Locale::getDefault() != Locale::createFromName(NULL))
        errln("Locale::getDefault() == Locale::createFromName(NULL)");

    /*----------*/
    // NOTE: There used to be a special test for locale names that had language or
    // country codes that were longer than two letters.  The new version of Locale
    // doesn't support anything that isn't an officially recognized language or
    // country code, so we no longer support this feature.

    Locale bogusLang("THISISABOGUSLANGUAGE"); // Jitterbug 2864: language code too long
    if(!bogusLang.isBogus()) {
        errln("Locale(\"THISISABOGUSLANGUAGE\").isBogus()==FALSE");
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

        sprintf(temp2, "%x", (int)testLocale.getLCID());
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
    if (dataTable == 0) {
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
    if (testCount != 595)
        errln("Expected getISOLanguages() to return 595 languages; it returned %d", testCount);
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

    if (testCount != 249){
        errln("Expected getISOCountries to return 249 countries; it returned %d", testCount);
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
    DecimalFormat *thaiCurrency = (DecimalFormat*)NumberFormat::createCurrencyInstance(
                    Locale("th", "TH"), status);
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
    UChar euro = 0x20ac;
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
                            NULL
                        };
    const char** locales = localeArr;

    UErrorCode status = U_ZERO_ERROR;

    UnicodeString temp;

    for (;*locales!=NULL;locales++) {
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

    UnicodeString dollarStr("USD", ""), euroStr("EUR", ""), genericStr((UChar)0x00a4), resultStr;
    UChar tmp[4];
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
        errln("Fail: en_QQ didn't return NULL");
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
    UChar ocf = 0x00f4;
    UChar oda = 0x0151;
    if (str.indexOf(oda) < 0 || str.indexOf(ocf) >= 0) {
      /* If the default locale is "th" this test will fail because of the buddhist calendar. */
      if (strcmp(Locale::getDefault().getLanguage(), "th") != 0) {
        errln("Fail: Monday in Hungarian is wrong - oda's index is %d and ocf's is %d",
              str.indexOf(oda), str.indexOf(ocf));
      } else {
        logln(UnicodeString("An error is produce in buddhist calendar."));
      }
      logln(UnicodeString("String is: ") + str );
    }
}

UDate
LocaleTest::date(int32_t y, int32_t m, int32_t d, int32_t hr, int32_t min, int32_t sec)
{
    UErrorCode status = U_ZERO_ERROR;
    Calendar *cal = Calendar::createInstance(status);
    if (cal == 0)
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
    if((result==NULL)||(result[0] != 0))
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
    if((result==NULL)||(result[0] != 0))
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
    if(l.isBogus() != TRUE) {
        errln("After setting bogus, didn't return TRUE");
    }
    l = "en_US"; // This should reset bogus
    if(l.isBogus() != FALSE) {
        errln("After resetting bogus, didn't return FALSE");
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
            "und_AQ",
            "_Latn_AQ",
            "_AQ"
        }, {
            "und_Zzzz_AQ",
            "_Latn_AQ",
            "_AQ"
        }, {
            "und_Latn_AQ",
            "_Latn_AQ",
            "_AQ"
        }, {
            "und_Moon_AQ",
            "_Moon_AQ",
            "_Moon_AQ"
        },
    };

    for (const auto& item : full_data) {
        const char* const org = item.from;
        const char* const exp = item.add;
        Locale res(org);
        res.addLikelySubtags(status);
        status.errIfFailureAndReset("\"%s\"", org);
        assertEquals("addLikelySubtags", exp, res.getName());
    }

    for (const auto& item : full_data) {
        const char* const org = item.from;
        const char* const exp = item.remove;
        Locale res(org);
        res.minimizeSubtags(status);
        status.errIfFailureAndReset("\"%s\"", org);
        assertEquals("minimizeSubtags", exp, res.getName());
    }
}


void
LocaleTest::TestKeywordVariants(void) {
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
    const char *result = NULL;
    StringEnumeration *keywords;
    int32_t keyCount = 0;
    const char *keyword = NULL;
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
                        if((keyword = keywords->next(&keywordLen, status)) == NULL) {
                            break;
                        }
                        if(strcmp(keyword, testCases[i].expectedKeywords[j]) != 0) {
                            err("Expected to get keyword value %s, got %s\n", testCases[i].expectedKeywords[j], keyword);
                        }
                    } else {
                        if((keywordString = keywords->snext(status)) == NULL) {
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
                        if(k2 == NULL || keyCount != k2->count(status)) {
                            errln("KeywordEnumeration.clone() failed");
                        } else {
                            delete keywords;
                            keywords = k2;
                        }
                    }
                }
                keywords->reset(status); // Make sure that reset works.
                for(j = 0;;) {
                    if((keyword = keywords->next(&keywordLen, status)) == NULL) {
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
LocaleTest::TestCreateUnicodeKeywords(void) {
    IcuTestErrorCode status(*this, "TestCreateUnicodeKeywords()");

    static const Locale l("de@calendar=buddhist;collation=phonebook");

    LocalPointer<StringEnumeration> keys(l.createUnicodeKeywords(status));
    status.errIfFailureAndReset("\"%s\"", l.getName());

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
LocaleTest::TestKeywordVariantParsing(void) {
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
LocaleTest::TestCreateKeywordSet(void) {
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
LocaleTest::TestCreateUnicodeKeywordSet(void) {
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
}

void
LocaleTest::TestGetKeywordValueStdString(void) {
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
LocaleTest::TestGetUnicodeKeywordValueStdString(void) {
    IcuTestErrorCode status(*this, "TestGetUnicodeKeywordValueStdString()");

    static const char keyword[] = "co";
    static const char expected[] = "phonebk";

    static const Locale l("de@calendar=buddhist;collation=phonebook");

    std::string result = l.getUnicodeKeywordValue<std::string>(keyword, status);
    status.errIfFailureAndReset("\"%s\"", keyword);
    assertEquals(keyword, expected, result.c_str());
}

void
LocaleTest::TestSetKeywordValue(void) {
    static const struct {
        const char *keyword;
        const char *value;
    } testCases[] = {
        { "collation", "phonebook" },
        { "currency", "euro" },
        { "calendar", "buddhist" }
    };

    UErrorCode status = U_ZERO_ERROR;

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
}

void
LocaleTest::TestSetKeywordValueStringPiece(void) {
    IcuTestErrorCode status(*this, "TestSetKeywordValueStringPiece()");
    Locale l(Locale::getGerman());

    l.setKeywordValue(StringPiece("collation"), StringPiece("phonebook"), status);
    l.setKeywordValue(StringPiece("calendarxxx", 8), StringPiece("buddhistxxx", 8), status);

    static const char expected[] = "de@calendar=buddhist;collation=phonebook";
    assertEquals("", expected, l.getName());
}

void
LocaleTest::TestSetUnicodeKeywordValueStringPiece(void) {
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
LocaleTest::TestGetBaseName(void) {
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

void LocaleTest::TestGetLocale(void) {
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
            if (dec == NULL) {
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
            if (sym == NULL) {
                errln("FAIL: getDecimalFormatSymbols returned NULL");
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
        if (df == 0){
            dataerrln("Error calling DateFormat::createDateInstance()");
        } else {
            SimpleDateFormat* dat = dynamic_cast<SimpleDateFormat*>(df);
            if (dat == NULL) {
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
            if (sym == NULL) {
                errln("FAIL: getDateFormatSymbols returned NULL");
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
            brk = 0; // registerInstance adopts
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
                brk = 0;
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

        checkRegisteredCollators(NULL); // Don't expect any extras

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
            coll = 0; // registerInstance adopts
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
                coll = 0;
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

        checkRegisteredCollators(NULL); // extra should be gone again
    }
#endif
#endif
}

#if !UCONFIG_NO_COLLATION
/**
 * Compare Collator::getAvailableLocales(int) [ "old", returning an array ]
 *   with  Collator::getAvailableLocales()    [ "new", returning a StringEnumeration ]
 * These should be identical (check their API docs) EXCEPT that
 * if expectExtra is non-NULL, it will be in the "new" array but not "old".
 * Does not return any status but calls errln on error.
 * @param expectExtra an extra locale, will be in "new" but not "old". Or NULL.
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
    if(oldList == NULL) {
        dataerrln("Error: Collator::getAvailableLocales(count) returned NULL");
        return;
    }

    // the 'new' list (enumeration)
    LocalPointer<StringEnumeration> newEnum(Collator::getAvailableLocales());
    if(newEnum.isNull()) {
       errln("Error: collator::getAvailableLocales() returned NULL");
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
        if(expectExtra != NULL && !strcmp(expectExtra, oldList[i].getName())) {
            errln("Inexplicably, Collator::getAvailableCollators(count) had registered collator %s. This shouldn't happen, so I am going to consider it an error.\n", expectExtra);
        }
    }

    // 2. add all of NEW
    const UnicodeString *locStr;
    UBool foundExpected = FALSE;
    while((locStr = newEnum->snext(status)) && U_SUCCESS(status)) {
        count2++;

        if(expectExtra != NULL && expectStr == *locStr) {
            foundExpected = TRUE;
            logln(UnicodeString("Found expected registered collator: ","") + expectStr);
        }
        (void)foundExpected;    // Hush unused variable compiler warning.

        if( oldHash.geti(*locStr) == 0 ) {
            if(expectExtra != NULL && expectStr==*locStr) {
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
    if(expectExtra != NULL) {
        expectCount2 ++; // if an extra item registered, bump the expect count
    }

    assertEquals("Collator::getAvail() count", expectCount2, count2);
}
#endif



void LocaleTest::TestVariantWithOutCountry(void) {
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
        return Locale(localeID);
    default:
        return Locale("");
    }
}

void LocaleTest::TestCanonicalization(void)
{
    static const struct {
        const char *localeID;    /* input */
        const char *getNameID;   /* expected getName() result */
        const char *canonicalID; /* expected canonicalize() result */
    } testCases[] = {
        { "ca_ES-with-extra-stuff-that really doesn't make any sense-unless-you're trying to increase code coverage",
          "ca_ES_WITH_EXTRA_STUFF_THAT REALLY DOESN'T MAKE ANY SENSE_UNLESS_YOU'RE TRYING TO INCREASE CODE COVERAGE",
          "ca_ES_WITH_EXTRA_STUFF_THAT REALLY DOESN'T MAKE ANY SENSE_UNLESS_YOU'RE TRYING TO INCREASE CODE COVERAGE"},
        { "zh@collation=pinyin", "zh@collation=pinyin", "zh@collation=pinyin" },
        { "zh_CN@collation=pinyin", "zh_CN@collation=pinyin", "zh_CN@collation=pinyin" },
        { "zh_CN_CA@collation=pinyin", "zh_CN_CA@collation=pinyin", "zh_CN_CA@collation=pinyin" },
        { "en_US_POSIX", "en_US_POSIX", "en_US_POSIX" }, 
        { "hy_AM_REVISED", "hy_AM_REVISED", "hy_AM_REVISED" }, 
        { "no_NO_NY", "no_NO_NY", "no_NO_NY" /* not: "nn_NO" [alan ICU3.0] */ },
        { "no@ny", "no@ny", "no__NY" /* not: "nn" [alan ICU3.0] */ }, /* POSIX ID */
        { "no-no.utf32@B", "no_NO.utf32@B", "no_NO_B" /* not: "nb_NO_B" [alan ICU3.0] */ }, /* POSIX ID */
        { "qz-qz@Euro", "qz_QZ@Euro", "qz_QZ_EURO" }, /* qz-qz uses private use iso codes */
        // NOTE: uloc_getName() works on en-BOONT, but Locale() parser considers it BOGUS
        // TODO: unify this behavior
        { "en-BOONT", "en__BOONT", "en__BOONT" }, /* registered name */
        { "de-1901", "de__1901", "de__1901" }, /* registered name */
        { "de-1906", "de__1906", "de__1906" }, /* registered name */

        /* posix behavior that used to be performed by getName */
        { "mr.utf8", "mr.utf8", "mr" },
        { "de-tv.koi8r", "de_TV.koi8r", "de_TV" },
        { "x-piglatin_ML.MBE", "x-piglatin_ML.MBE", "x-piglatin_ML" },
        { "i-cherokee_US.utf7", "i-cherokee_US.utf7", "i-cherokee_US" },
        { "x-filfli_MT_FILFLA.gb-18030", "x-filfli_MT_FILFLA.gb-18030", "x-filfli_MT_FILFLA" },
        { "no-no-ny.utf8@B", "no_NO_NY.utf8@B", "no_NO_NY_B" /* not: "nn_NO" [alan ICU3.0] */ }, /* @ ignored unless variant is empty */

        /* fleshing out canonicalization */
        /* trim space and sort keywords, ';' is separator so not present at end in canonical form */
        { "en_Hant_IL_VALLEY_GIRL@ currency = EUR; calendar = Japanese ;", "en_Hant_IL_VALLEY_GIRL@calendar=Japanese;currency=EUR", "en_Hant_IL_VALLEY_GIRL@calendar=Japanese;currency=EUR" },
        /* already-canonical ids are not changed */
        { "en_Hant_IL_VALLEY_GIRL@calendar=Japanese;currency=EUR", "en_Hant_IL_VALLEY_GIRL@calendar=Japanese;currency=EUR", "en_Hant_IL_VALLEY_GIRL@calendar=Japanese;currency=EUR" },
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
        { "sr_YU_CYRILLIC", "sr_YU_CYRILLIC", "sr_YU_CYRILLIC" }, /* Linux name */
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

void LocaleTest::TestCurrencyByDate(void)
{
#if !UCONFIG_NO_FORMATTING
    UErrorCode status = U_ZERO_ERROR;
    UDate date = uprv_getUTCtime();
	UChar TMP[4];
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
		errcheckln(status, "FAIL: eo_AM didn't return NULL - %s", u_errorName(status));
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
		errcheckln(status, "FAIL: eo_UA didn't return NULL - %s", u_errorName(status));
    }
    status = U_ZERO_ERROR;

	// Test index bounds
    resLen = ucurr_forLocaleAndDate("eo_UA", date, 100, TMP, 4, &status);
    if (resLen != 0) {
		errcheckln(status, "FAIL: eo_UA didn't return NULL - %s", u_errorName(status));
    }
    status = U_ZERO_ERROR;

    resLen = ucurr_forLocaleAndDate("eo_UA", date, 0, TMP, 4, &status);
    if (resLen != 0) {
		errcheckln(status, "FAIL: eo_UA didn't return NULL - %s", u_errorName(status));
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
		errcheckln(status, "FAIL: eo_QQ didn't return NULL - %s", u_errorName(status));
    }
    status = U_ZERO_ERROR;
    resLen = ucurr_forLocaleAndDate("eo_QQ", date, 0, TMP, 4, &status);
    if (resLen != 0) {
		errcheckln(status, "FAIL: eo_QQ didn't return NULL - %s", u_errorName(status));
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
		errcheckln(status, "FAIL: eo_AO didn't return NULL - %s", u_errorName(status));
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

    UChar USD[4];
    ucurr_forLocaleAndDate("en_US", date, 1, USD, 4, &status);
    
	UChar YEN[4];
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

void LocaleTest::TestGetVariantWithKeywords(void)
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
    assertTrue("und-EG RTL", Locale("und-EG").isRightToLeft(), FALSE, TRUE);
    assertFalse("fa-Cyrl LTR", Locale("fa-Cyrl").isRightToLeft());
    assertTrue("en-Hebr RTL", Locale("en-Hebr").isRightToLeft());
    assertTrue("ckb RTL", Locale("ckb").isRightToLeft(), FALSE, TRUE);  // Sorani Kurdish
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
    static const Locale expected3("art__LOJBAN@x=0");
    assertEquals(locid3, expected3.getName(), result3.getName());

    static const char locid4[] = "art-lojban-x-0";
    Locale result4 = Locale::createCanonical(locid4);
    static const Locale expected4("jbo@x=0");
    assertEquals(locid4, expected4.getName(), result4.getName());
}

void LocaleTest::TestForLanguageTag() {
    IcuTestErrorCode status(*this, "TestForLanguageTag()");

    static const char tag_en[] = "en-US";
    static const char tag_oed[] = "en-GB-oed";
    static const char tag_af[] = "af-t-ar-i0-handwrit-u-ca-coptic-x-foo";
    static const char tag_ill[] = "!";
    static const char tag_no_nul[] = { 'e', 'n', '-', 'G', 'B' };
    static const char tag_ext[] = "en-GB-1-abc-efg-a-xyz";

    static const Locale loc_en("en_US");
    static const Locale loc_oed("en_GB_OXENDICT");
    static const Locale loc_af("af@calendar=coptic;t=ar-i0-handwrit;x=foo");
    static const Locale loc_null("");
    static const Locale loc_gb("en_GB");
    static const Locale loc_ext("en_GB@1=abc-efg;a=xyz");

    Locale result_en = Locale::forLanguageTag(tag_en, status);
    status.errIfFailureAndReset("\"%s\"", tag_en);
    assertEquals(tag_en, loc_en.getName(), result_en.getName());

    Locale result_oed = Locale::forLanguageTag(tag_oed, status);
    status.errIfFailureAndReset("\"%s\"", tag_oed);
    assertEquals(tag_oed, loc_oed.getName(), result_oed.getName());

    Locale result_af = Locale::forLanguageTag(tag_af, status);
    status.errIfFailureAndReset("\"%s\"", tag_af);
    assertEquals(tag_af, loc_af.getName(), result_af.getName());

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
}

void LocaleTest::TestToLanguageTag() {
    IcuTestErrorCode status(*this, "TestToLanguageTag()");

    static const Locale loc_c("en_US_POSIX");
    static const Locale loc_en("en_US");
    static const Locale loc_af("af@calendar=coptic;t=ar-i0-handwrit;x=foo");
    static const Locale loc_ext("en@0=abc;a=xyz");
    static const Locale loc_empty("");
    static const Locale loc_ill("!");

    static const char tag_c[] = "en-US-u-va-posix";
    static const char tag_en[] = "en-US";
    static const char tag_af[] = "af-t-ar-i0-handwrit-u-ca-coptic-x-foo";
    static const char tag_ext[] = "en-0-abc-a-xyz";
    static const char tag_und[] = "und";

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

    Locale loc_bogus;
    loc_bogus.setToBogus();
    std::string result_bogus = loc_bogus.toLanguageTag<std::string>(status);
    assertEquals("bogus", U_ILLEGAL_ARGUMENT_ERROR, status.reset());
    assertTrue(result_bogus.c_str(), result_bogus.empty());
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
