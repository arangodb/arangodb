/*********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2010-2013, International Business Machines Corporation and
 * others. All Rights Reserved.
 *********************************************************************/

#include "locnmtst.h"
#include "cstring.h"

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

#define test_assert_equal(target,value) \
  { \
    if (UnicodeString(target)!=(value)) { \
      logln("unexpected value '" + (value) + "'"); \
      dataerrln("FAIL: " #target " == " #value " was not true. In " __FILE__ " on line %d", __LINE__); \
    } else { \
      logln("PASS: asserted " #target " == " #value); \
    } \
  }

#define test_dumpLocale(l) { logln(#l " = " + UnicodeString(l.getName(), "")); }

LocaleDisplayNamesTest::LocaleDisplayNamesTest() {
}

LocaleDisplayNamesTest::~LocaleDisplayNamesTest() {
}

void LocaleDisplayNamesTest::runIndexedTest(int32_t index, UBool exec, const char* &name, 
                        char* /*par*/) {
    switch (index) {
#if !UCONFIG_NO_FORMATTING
        TESTCASE(0, TestCreate);
        TESTCASE(1, TestCreateDialect);
        TESTCASE(2, TestWithKeywordsAndEverything);
        TESTCASE(3, TestUldnOpen);
        TESTCASE(4, TestUldnOpenDialect);
        TESTCASE(5, TestUldnWithKeywordsAndEverything);
        TESTCASE(6, TestUldnComponents);
        TESTCASE(7, TestRootEtc);
        TESTCASE(8, TestKeywords);
        TESTCASE(9, TestUntranslatedKeywords);
        TESTCASE(10, TestPrivateUse);
        TESTCASE(11, TestUldnDisplayContext);
#endif
        default:
            name = "";
            break;
    }
}

#if !UCONFIG_NO_FORMATTING
void LocaleDisplayNamesTest::TestCreate() {
  UnicodeString temp;
  LocaleDisplayNames *ldn = LocaleDisplayNames::createInstance(Locale::getGermany());
  ldn->localeDisplayName("de_DE", temp);
  delete ldn;
  test_assert_equal("Deutsch (Deutschland)", temp);
}

void LocaleDisplayNamesTest::TestCreateDialect() {
  UnicodeString temp;
  LocaleDisplayNames *ldn = LocaleDisplayNames::createInstance(Locale::getUS(), ULDN_DIALECT_NAMES);
  ldn->localeDisplayName("en_GB", temp);
  delete ldn;
  test_assert_equal("British English", temp);
}

void LocaleDisplayNamesTest::TestWithKeywordsAndEverything() {
  UnicodeString temp;
  LocaleDisplayNames *ldn = LocaleDisplayNames::createInstance(Locale::getUS());
  const char *locname = "en_Hant_US_VALLEY@calendar=gregorian;collation=phonebook";
  const char *target = "English (Traditional, United States, VALLEY, "
    "Gregorian Calendar, Phonebook Sort Order)";
  ldn->localeDisplayName(locname, temp);
  delete ldn;
  test_assert_equal(target, temp);
}

void LocaleDisplayNamesTest::TestKeywords() {
  UnicodeString temp;
  LocaleDisplayNames *ldn = LocaleDisplayNames::createInstance(Locale::getUS());
  const char *locname = "de@currency=XYZ";
  const char *target = "German (Currency: XYZ)";
  ldn->localeDisplayName(locname, temp);
  delete ldn;
  test_assert_equal(target, temp);
}

void LocaleDisplayNamesTest::TestUntranslatedKeywords() {
  UnicodeString temp;
  LocaleDisplayNames *ldn = LocaleDisplayNames::createInstance(Locale::getUS());
  const char *locname = "de@foo=bar";
  const char *target = "German (foo=bar)";
  ldn->localeDisplayName(locname, temp);
  delete ldn;
  test_assert_equal(target, temp);
}

void LocaleDisplayNamesTest::TestPrivateUse() {
  UnicodeString temp;
  LocaleDisplayNames *ldn = LocaleDisplayNames::createInstance(Locale::getUS());
  const char *locname = "de@x=foobar";
  const char *target = "German (Private-Use: foobar)";
  ldn->localeDisplayName(locname, temp);
  delete ldn;
  test_assert_equal(target, temp);
}

void LocaleDisplayNamesTest::TestUldnOpen() {
  UErrorCode status = U_ZERO_ERROR;
  const int32_t kMaxResultSize = 150;  // long enough
  UChar result[150];
  ULocaleDisplayNames *ldn = uldn_open(Locale::getGermany().getName(), ULDN_STANDARD_NAMES, &status);
  int32_t len = uldn_localeDisplayName(ldn, "de_DE", result, kMaxResultSize, &status);
  uldn_close(ldn);
  test_assert(U_SUCCESS(status));

  UnicodeString str(result, len, kMaxResultSize);
  test_assert_equal("Deutsch (Deutschland)", str);

  // make sure that NULL gives us the default locale as usual
  ldn = uldn_open(NULL, ULDN_STANDARD_NAMES, &status);
  const char *locale = uldn_getLocale(ldn);
  if(0 != uprv_strcmp(uloc_getDefault(), locale)) {
    errln("uldn_getLocale(uldn_open(NULL))=%s != default locale %s\n", locale, uloc_getDefault());
  }
  uldn_close(ldn);
  test_assert(U_SUCCESS(status));
}

void LocaleDisplayNamesTest::TestUldnOpenDialect() {
  UErrorCode status = U_ZERO_ERROR;
  const int32_t kMaxResultSize = 150;  // long enough
  UChar result[150];
  ULocaleDisplayNames *ldn = uldn_open(Locale::getUS().getName(), ULDN_DIALECT_NAMES, &status);
  int32_t len = uldn_localeDisplayName(ldn, "en_GB", result, kMaxResultSize, &status);
  uldn_close(ldn);
  test_assert(U_SUCCESS(status));

  UnicodeString str(result, len, kMaxResultSize);
  test_assert_equal("British English", str);
}

void LocaleDisplayNamesTest::TestUldnWithKeywordsAndEverything() {
  UErrorCode status = U_ZERO_ERROR;
  const int32_t kMaxResultSize = 150;  // long enough
  UChar result[150];
  const char *locname = "en_Hant_US_VALLEY@calendar=gregorian;collation=phonebook";
  const char *target = "English (Traditional, United States, VALLEY, "
    "Gregorian Calendar, Phonebook Sort Order)";
  ULocaleDisplayNames *ldn = uldn_open(Locale::getUS().getName(), ULDN_STANDARD_NAMES, &status);
  int32_t len = uldn_localeDisplayName(ldn, locname, result, kMaxResultSize, &status);
  uldn_close(ldn);
  test_assert(U_SUCCESS(status));

  UnicodeString str(result, len, kMaxResultSize);
  test_assert_equal(target, str);
}

void LocaleDisplayNamesTest::TestUldnComponents() {
  UErrorCode status = U_ZERO_ERROR;
  const int32_t kMaxResultSize = 150;  // long enough
  UChar result[150];

  ULocaleDisplayNames *ldn = uldn_open(Locale::getGermany().getName(), ULDN_STANDARD_NAMES, &status);
  test_assert(U_SUCCESS(status));
  if (U_FAILURE(status)) {
    return;
  }

  // "en_Hant_US_PRE_EURO@calendar=gregorian";

  {
    int32_t len = uldn_languageDisplayName(ldn, "en", result, kMaxResultSize, &status);
    UnicodeString str(result, len, kMaxResultSize);
    test_assert_equal("Englisch", str);
  }


  {
    int32_t len = uldn_scriptDisplayName(ldn, "Hant", result, kMaxResultSize, &status);
    UnicodeString str(result, len, kMaxResultSize);
    test_assert_equal("Traditionell", str);
  }

  {
    int32_t len = uldn_scriptCodeDisplayName(ldn, USCRIPT_TRADITIONAL_HAN, result, kMaxResultSize, 
                         &status);
    UnicodeString str(result, len, kMaxResultSize);
    test_assert_equal("Traditionell", str);
  }

  {
    int32_t len = uldn_regionDisplayName(ldn, "US", result, kMaxResultSize, &status);
    UnicodeString str(result, len, kMaxResultSize);
    test_assert_equal("Vereinigte Staaten", str);
  }

  {
    int32_t len = uldn_variantDisplayName(ldn, "PRE_EURO", result, kMaxResultSize, &status);
    UnicodeString str(result, len, kMaxResultSize);
    test_assert_equal("PRE_EURO", str);
  }

  {
    int32_t len = uldn_keyDisplayName(ldn, "calendar", result, kMaxResultSize, &status);
    UnicodeString str(result, len, kMaxResultSize);
    test_assert_equal("Kalender", str);
  }

  {
    int32_t len = uldn_keyValueDisplayName(ldn, "calendar", "gregorian", result, 
                       kMaxResultSize, &status);
    UnicodeString str(result, len, kMaxResultSize);
    test_assert_equal("Gregorianischer Kalender", str);
  }

  uldn_close(ldn);
}


typedef struct {
    const char * displayLocale;
    UDisplayContext dialectHandling;
    UDisplayContext capitalization;
    const char * localeToBeNamed;
    const UChar * result;
} LocNameDispContextItem;

static char en[]    = "en";
static char en_US[] = "en_US";

static UChar daFor_en[]       = {0x65,0x6E,0x67,0x65,0x6C,0x73,0x6B,0}; //"engelsk"
static UChar daFor_en_US[]    = {0x65,0x6E,0x67,0x65,0x6C,0x73,0x6B,0x20,0x28,0x55,0x53,0x41,0x29,0}; //"engelsk (USA)"
static UChar daFor_en_US_D[]  = {0x61,0x6D,0x65,0x72,0x69,0x6B,0x61,0x6E,0x73,0x6B,0x20,0x65,0x6E,0x67,0x65,0x6C,0x73,0x6B,0}; //"amerikansk engelsk"
static UChar esFor_en[]       = {0x69,0x6E,0x67,0x6C,0xE9,0x73,0}; //"ingles" with acute on e
static UChar esFor_en_US[]    = {0x69,0x6E,0x67,0x6C,0xE9,0x73,0x20,0x28,0x45,0x73,0x74,0x61,0x64,0x6F,0x73,0x20,0x55,0x6E,0x69,0x64,0x6F,0x73,0x29,0}; //"ingles (Estados Unidos)" ...
static UChar esFor_en_US_D[]  = {0x69,0x6E,0x67,0x6C,0xE9,0x73,0x20,0x65,0x73,0x74,0x61,0x64,0x6F,0x75,0x6E,0x69,0x64,0x65,0x6E,0x73,0x65,0}; //"ingles estadounidense" ...
#if !UCONFIG_NO_BREAK_ITERATION
static UChar daFor_en_T[]     = {0x45,0x6E,0x67,0x65,0x6C,0x73,0x6B,0}; //"Engelsk"
static UChar daFor_en_US_T[]  = {0x45,0x6E,0x67,0x65,0x6C,0x73,0x6B,0x20,0x28,0x55,0x53,0x41,0x29,0}; //"Engelsk (USA)"
static UChar daFor_en_US_DT[] = {0x41,0x6D,0x65,0x72,0x69,0x6B,0x61,0x6E,0x73,0x6B,0x20,0x65,0x6E,0x67,0x65,0x6C,0x73,0x6B,0}; //"Amerikansk engelsk"
static UChar esFor_en_T[]     = {0x49,0x6E,0x67,0x6C,0xE9,0x73,0}; //"Ingles" ...
static UChar esFor_en_US_T[]  = {0x49,0x6E,0x67,0x6C,0xE9,0x73,0x20,0x28,0x45,0x73,0x74,0x61,0x64,0x6F,0x73,0x20,0x55,0x6E,0x69,0x64,0x6F,0x73,0x29,0}; //"Ingles (Estados Unidos)" ...
static UChar esFor_en_US_DT[] = {0x49,0x6E,0x67,0x6C,0xE9,0x73,0x20,0x65,0x73,0x74,0x61,0x64,0x6F,0x75,0x6E,0x69,0x64,0x65,0x6E,0x73,0x65,0}; //"Ingles estadounidense" ...
#endif /* #if !UCONFIG_NO_BREAK_ITERATION */

static const LocNameDispContextItem ctxtItems[] = {
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    en,    daFor_en   },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    en_US, daFor_en_US   },
    { "da", UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    en_US, daFor_en_US_D  },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    en,    esFor_en   },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    en_US, esFor_en_US   },
    { "es", UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    en_US, esFor_en_US_D  },
#if !UCONFIG_NO_BREAK_ITERATION
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, en,    daFor_en_T },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, en_US, daFor_en_US_T },
    { "da", UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, en_US, daFor_en_US_DT },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, en,    esFor_en_T },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, en_US, esFor_en_US_T },
    { "es", UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       en_US, esFor_en_US_DT },

    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       en,    daFor_en_T },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       en_US, daFor_en_US_T },
    { "da", UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       en_US, daFor_en_US_DT },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       en,    esFor_en_T },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       en_US, esFor_en_US_T },
    { "es", UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, en_US, esFor_en_US_DT },
 #endif /* #if !UCONFIG_NO_BREAK_ITERATION */
    { NULL, (UDisplayContext)0,      (UDisplayContext)0,                                NULL,  NULL }
};

void LocaleDisplayNamesTest::TestUldnDisplayContext() {
    const LocNameDispContextItem * ctxtItemPtr;
    for (ctxtItemPtr = ctxtItems; ctxtItemPtr->displayLocale != NULL; ctxtItemPtr++) {
        UDisplayContext contexts[2] = {ctxtItemPtr->dialectHandling, ctxtItemPtr->capitalization};
        UErrorCode status = U_ZERO_ERROR;
        ULocaleDisplayNames * uldn = uldn_openForContext(ctxtItemPtr->displayLocale, contexts, 2, &status);
        if (U_FAILURE(status)) {
            errln(UnicodeString("FAIL: uldn_openForContext failed for locale ") + ctxtItemPtr->displayLocale +
                  ", dialectHandling " + ctxtItemPtr->dialectHandling + ", capitalization " +  ctxtItemPtr->capitalization);
        } else {
            UDisplayContext dialectHandling = uldn_getContext(uldn, UDISPCTX_TYPE_DIALECT_HANDLING, &status);
            UDisplayContext capitalization = uldn_getContext(uldn, UDISPCTX_TYPE_CAPITALIZATION, &status);
            if (U_FAILURE(status)) {
                errln(UnicodeString("FAIL: uldn_getContext status ") + (int)status);
            } else if (dialectHandling != ctxtItemPtr->dialectHandling || capitalization != ctxtItemPtr->capitalization) {
                errln("FAIL: uldn_getContext retrieved incorrect dialectHandling or capitalization");
            } else {
                UChar nameBuf[ULOC_FULLNAME_CAPACITY];
                int32_t len = uldn_localeDisplayName(uldn, ctxtItemPtr->localeToBeNamed, nameBuf, ULOC_FULLNAME_CAPACITY, &status);
                if (U_FAILURE(status)) {
                    dataerrln(UnicodeString("FAIL: uldn_localeDisplayName status: ") + u_errorName(status));
                } else if (u_strcmp(ctxtItemPtr->result, nameBuf) != 0) {
                    UnicodeString exp(ctxtItemPtr->result, u_strlen(ctxtItemPtr->result));
                    UnicodeString got(nameBuf, len);
                    dataerrln(UnicodeString("FAIL: uldn_localeDisplayName, capitalization ") + ctxtItemPtr->capitalization +
                          ", expected " + exp + ", got " + got );
                }
            }
            uldn_close(uldn);
        }
    }
}

void LocaleDisplayNamesTest::TestRootEtc() {
  UnicodeString temp;
  LocaleDisplayNames *ldn = LocaleDisplayNames::createInstance(Locale::getUS());
  const char *locname = "@collation=phonebook";
  const char *target = "Root (Phonebook Sort Order)";
  ldn->localeDisplayName(locname, temp);
  test_assert_equal(target, temp);

  ldn->languageDisplayName("root", temp);
  test_assert_equal("root", temp);

  ldn->languageDisplayName("en_GB", temp);
  test_assert_equal("en_GB", temp);

  delete ldn;
}

#endif   /*  UCONFIG_NO_FORMATTING */
