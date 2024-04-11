// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2010-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 *********************************************************************/

#include "locnmtst.h"
#include "unicode/ustring.h"
#include "cstring.h"

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

#define test_assert_equal(target,value) UPRV_BLOCK_MACRO_BEGIN { \
    if (UnicodeString(target)!=(value)) { \
        logln("unexpected value '" + (value) + "'"); \
        dataerrln("FAIL: " #target " == " #value " was not true. In " __FILE__ " on line %d", __LINE__); \
    } else { \
        logln("PASS: asserted " #target " == " #value); \
    } \
} UPRV_BLOCK_MACRO_END

#define test_dumpLocale(l) UPRV_BLOCK_MACRO_BEGIN { \
    logln(#l " = " + UnicodeString(l.getName(), "")); \
} UPRV_BLOCK_MACRO_END

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
        TESTCASE(8, TestCurrencyKeyword);
        TESTCASE(9, TestUnknownCurrencyKeyword);
        TESTCASE(10, TestUntranslatedKeywords);
        TESTCASE(11, TestPrivateUse);
        TESTCASE(12, TestUldnDisplayContext);
        TESTCASE(13, TestUldnWithGarbage);
        TESTCASE(14, TestSubstituteHandling);
        TESTCASE(15, TestNumericRegionID);
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

void LocaleDisplayNamesTest::TestCurrencyKeyword() {
  UnicodeString temp;
  LocaleDisplayNames *ldn = LocaleDisplayNames::createInstance(Locale::getUS());
  const char *locname = "ja@currency=JPY";
  const char *target = "Japanese (Japanese Yen)";
  ldn->localeDisplayName(locname, temp);
  delete ldn;
  test_assert_equal(target, temp);
}

void LocaleDisplayNamesTest::TestUnknownCurrencyKeyword() {
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
  char16_t result[150];
  ULocaleDisplayNames *ldn = uldn_open(Locale::getGermany().getName(), ULDN_STANDARD_NAMES, &status);
  int32_t len = uldn_localeDisplayName(ldn, "de_DE", result, kMaxResultSize, &status);
  uldn_close(ldn);
  test_assert(U_SUCCESS(status));

  UnicodeString str(result, len, kMaxResultSize);
  test_assert_equal("Deutsch (Deutschland)", str);

  // make sure that nullptr gives us the default locale as usual
  ldn = uldn_open(nullptr, ULDN_STANDARD_NAMES, &status);
  const char *locale = uldn_getLocale(ldn);
  if(0 != uprv_strcmp(uloc_getDefault(), locale)) {
    errln("uldn_getLocale(uldn_open(nullptr))=%s != default locale %s\n", locale, uloc_getDefault());
  }
  uldn_close(ldn);
  test_assert(U_SUCCESS(status));
}

void LocaleDisplayNamesTest::TestUldnOpenDialect() {
  UErrorCode status = U_ZERO_ERROR;
  const int32_t kMaxResultSize = 150;  // long enough
  char16_t result[150];
  ULocaleDisplayNames *ldn = uldn_open(Locale::getUS().getName(), ULDN_DIALECT_NAMES, &status);
  int32_t len = uldn_localeDisplayName(ldn, "en_GB", result, kMaxResultSize, &status);
  uldn_close(ldn);
  test_assert(U_SUCCESS(status));

  UnicodeString str(result, len, kMaxResultSize);
  test_assert_equal("British English", str);
}

void LocaleDisplayNamesTest::TestUldnWithGarbage() {
  UErrorCode status = U_ZERO_ERROR;
  const int32_t kMaxResultSize = 150;  // long enough
  char16_t result[150];
  ULocaleDisplayNames *ldn = uldn_open(Locale::getUS().getName(), ULDN_DIALECT_NAMES, &status);
  int32_t len = uldn_localeDisplayName(ldn, "english (United States) [w", result, kMaxResultSize, &status);
  uldn_close(ldn);
  test_assert(U_FAILURE(status) && len == 0);
}

void LocaleDisplayNamesTest::TestUldnWithKeywordsAndEverything() {
  UErrorCode status = U_ZERO_ERROR;
  const int32_t kMaxResultSize = 150;  // long enough
  char16_t result[150];
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
  char16_t result[150];

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
    UDisplayContext displayLength;
    const char * localeToBeNamed;
    const char16_t * result;
} LocNameDispContextItem;

static char en[] = "en";
static char en_cabud[] = "en@calendar=buddhist";
static char en_GB[] = "en_GB";
static char uz_Latn[] = "uz_Latn";

static char16_t daFor_en[]       = {0x65,0x6E,0x67,0x65,0x6C,0x73,0x6B,0}; //"engelsk"
static char16_t daFor_en_cabud[] = {0x65,0x6E,0x67,0x65,0x6C,0x73,0x6B,0x20,0x28,0x62,0x75,0x64,0x64,0x68,0x69,0x73,0x74,0x69,0x73,0x6B,0x20,
                                 0x6B,0x61,0x6C,0x65,0x6E,0x64,0x65,0x72,0x29,0}; //"engelsk (buddhistisk kalender)"
static char16_t daFor_en_GB[]    = {0x65,0x6E,0x67,0x65,0x6C,0x73,0x6B,0x20,0x28,0x53,0x74,0x6F,0x72,0x62,0x72,0x69,0x74,0x61,0x6E,0x6E,0x69,0x65,0x6E,0x29,0}; //"engelsk (Storbritannien)"
static char16_t daFor_en_GB_D[]  = {0x62,0x72,0x69,0x74,0x69,0x73,0x6B,0x20,0x65,0x6E,0x67,0x65,0x6C,0x73,0x6B,0}; //"britisk engelsk"
static char16_t esFor_en[]       = {0x69,0x6E,0x67,0x6C,0xE9,0x73,0}; //"ingles" with acute on the e
static char16_t esFor_en_GB[]    = {0x69,0x6E,0x67,0x6C,0xE9,0x73,0x20,0x28,0x52,0x65,0x69,0x6E,0x6F,0x20,0x55,0x6E,0x69,0x64,0x6F,0x29,0}; //"ingles (Reino Unido)" ...
static char16_t esFor_en_GB_S[]  = {0x69,0x6E,0x67,0x6C,0xE9,0x73,0x20,0x28,0x52,0x55,0x29,0}; //"ingles (RU)" ...
static char16_t esFor_en_GB_D[]  = {0x69,0x6E,0x67,0x6C,0xE9,0x73,0x20,0x62,0x72,0x69,0x74,0xE1,0x6E,0x69,0x63,0x6F,0}; //"ingles britanico" with acute on the e, a
static char16_t ruFor_uz_Latn[]  = {0x0443,0x0437,0x0431,0x0435,0x043A,0x0441,0x043A,0x0438,0x0439,0x20,0x28,0x043B,0x0430,0x0442,0x0438,0x043D,0x0438,0x0446,0x0430,0x29,0}; // all lowercase
#if !UCONFIG_NO_BREAK_ITERATION
static char16_t daFor_en_T[]     = {0x45,0x6E,0x67,0x65,0x6C,0x73,0x6B,0}; //"Engelsk"
static char16_t daFor_en_cabudT[]= {0x45,0x6E,0x67,0x65,0x6C,0x73,0x6B,0x20,0x28,0x62,0x75,0x64,0x64,0x68,0x69,0x73,0x74,0x69,0x73,0x6B,0x20,
                                 0x6B,0x61,0x6C,0x65,0x6E,0x64,0x65,0x72,0x29,0}; //"Engelsk (buddhistisk kalender)"
static char16_t daFor_en_GB_T[]  = {0x45,0x6E,0x67,0x65,0x6C,0x73,0x6B,0x20,0x28,0x53,0x74,0x6F,0x72,0x62,0x72,0x69,0x74,0x61,0x6E,0x6E,0x69,0x65,0x6E,0x29,0}; //"Engelsk (Storbritannien)"
static char16_t daFor_en_GB_DT[] = {0x42,0x72,0x69,0x74,0x69,0x73,0x6B,0x20,0x65,0x6E,0x67,0x65,0x6C,0x73,0x6B,0}; //"Britisk engelsk"
static char16_t esFor_en_T[]     = {0x49,0x6E,0x67,0x6C,0xE9,0x73,0}; //"Ingles" with acute on the e
static char16_t esFor_en_GB_T[]  = {0x49,0x6E,0x67,0x6C,0xE9,0x73,0x20,0x28,0x52,0x65,0x69,0x6E,0x6F,0x20,0x55,0x6E,0x69,0x64,0x6F,0x29,0}; //"Ingles (Reino Unido)" ...
static char16_t esFor_en_GB_ST[] = {0x49,0x6E,0x67,0x6C,0xE9,0x73,0x20,0x28,0x52,0x55,0x29,0}; //"Ingles (RU)" ...
static char16_t esFor_en_GB_DT[] = {0x49,0x6E,0x67,0x6C,0xE9,0x73,0x20,0x62,0x72,0x69,0x74,0xE1,0x6E,0x69,0x63,0x6F,0}; //"Ingles britanico" with acute on the e, a
static char16_t ruFor_uz_Latn_T[]= {0x0423,0x0437,0x0431,0x0435,0x043A,0x0441,0x043A,0x0438,0x0439,0x20,0x28,0x043B,0x0430,0x0442,0x0438,0x043D,0x0438,0x0446,0x0430,0x29,0}; // first char upper
#endif /* #if !UCONFIG_NO_BREAK_ITERATION */

static const LocNameDispContextItem ctxtItems[] = {
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_FULL,   en,    daFor_en },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_FULL,   en_cabud, daFor_en_cabud },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_FULL,   en_GB, daFor_en_GB },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_SHORT,  en_GB, daFor_en_GB },
    { "da", UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_FULL,   en_GB, daFor_en_GB_D },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_FULL,   en,    esFor_en },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_FULL,   en_GB, esFor_en_GB },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_SHORT,  en_GB, esFor_en_GB_S },
    { "es", UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_FULL,   en_GB, esFor_en_GB_D },
    { "ru", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_MIDDLE_OF_SENTENCE,    UDISPCTX_LENGTH_FULL,   uz_Latn, ruFor_uz_Latn },
#if !UCONFIG_NO_BREAK_ITERATION
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_LENGTH_FULL,   en,    daFor_en_T },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_LENGTH_FULL,   en_cabud, daFor_en_cabudT },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_LENGTH_FULL,   en_GB, daFor_en_GB_T },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_LENGTH_SHORT,  en_GB, daFor_en_GB_T },
    { "da", UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_LENGTH_FULL,   en_GB, daFor_en_GB_DT },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_LENGTH_FULL,   en,    esFor_en_T },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_LENGTH_FULL,   en_GB, esFor_en_GB_T },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_LENGTH_SHORT,  en_GB, esFor_en_GB_ST },
    { "es", UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_LENGTH_FULL,   en_GB, esFor_en_GB_DT },
    { "ru", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_BEGINNING_OF_SENTENCE, UDISPCTX_LENGTH_FULL,   uz_Latn, ruFor_uz_Latn_T },

    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_FULL,   en,    daFor_en_T },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_FULL,   en_cabud, daFor_en_cabudT },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_FULL,   en_GB, daFor_en_GB_T },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_SHORT,  en_GB, daFor_en_GB_T },
    { "da", UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_FULL,   en_GB, daFor_en_GB_DT },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_FULL,   en,    esFor_en_T },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_FULL,   en_GB, esFor_en_GB_T },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_SHORT,  en_GB, esFor_en_GB_ST },
    { "es", UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_FULL,   en_GB, esFor_en_GB_DT },
    { "ru", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_UI_LIST_OR_MENU,       UDISPCTX_LENGTH_FULL,   uz_Latn, ruFor_uz_Latn_T },

    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_STANDALONE,            UDISPCTX_LENGTH_FULL,   en,    daFor_en },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_STANDALONE,            UDISPCTX_LENGTH_FULL,   en_cabud, daFor_en_cabud },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_STANDALONE,            UDISPCTX_LENGTH_FULL,   en_GB, daFor_en_GB },
    { "da", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_STANDALONE,            UDISPCTX_LENGTH_SHORT,  en_GB, daFor_en_GB },
    { "da", UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_STANDALONE,            UDISPCTX_LENGTH_FULL,   en_GB, daFor_en_GB_D },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_STANDALONE,            UDISPCTX_LENGTH_FULL,   en,    esFor_en_T },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_STANDALONE,            UDISPCTX_LENGTH_FULL,   en_GB, esFor_en_GB_T },
    { "es", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_STANDALONE,            UDISPCTX_LENGTH_SHORT,  en_GB, esFor_en_GB_ST },
    { "es", UDISPCTX_DIALECT_NAMES,  UDISPCTX_CAPITALIZATION_FOR_STANDALONE,            UDISPCTX_LENGTH_FULL,   en_GB, esFor_en_GB_DT },
    { "ru", UDISPCTX_STANDARD_NAMES, UDISPCTX_CAPITALIZATION_FOR_STANDALONE,            UDISPCTX_LENGTH_FULL,   uz_Latn, ruFor_uz_Latn_T },
 #endif /* #if !UCONFIG_NO_BREAK_ITERATION */
    { nullptr, (UDisplayContext)0,      (UDisplayContext)0,                                (UDisplayContext)0,     nullptr,  nullptr }
};

void LocaleDisplayNamesTest::TestUldnDisplayContext() {
    const LocNameDispContextItem * ctxtItemPtr;
    for (ctxtItemPtr = ctxtItems; ctxtItemPtr->displayLocale != nullptr; ctxtItemPtr++) {
        UDisplayContext contexts[3] = {ctxtItemPtr->dialectHandling, ctxtItemPtr->capitalization, ctxtItemPtr->displayLength};
        UErrorCode status = U_ZERO_ERROR;
        ULocaleDisplayNames * uldn = uldn_openForContext(ctxtItemPtr->displayLocale, contexts, 3, &status);
        if (U_FAILURE(status)) {
            errln(UnicodeString("FAIL: uldn_openForContext failed for locale ") + ctxtItemPtr->displayLocale +
                  ", dialectHandling " + ctxtItemPtr->dialectHandling +
                  ", capitalization " +  ctxtItemPtr->capitalization +
                  ", displayLength " +  ctxtItemPtr->displayLength);
        } else {
            UDisplayContext dialectHandling = uldn_getContext(uldn, UDISPCTX_TYPE_DIALECT_HANDLING, &status);
            UDisplayContext capitalization = uldn_getContext(uldn, UDISPCTX_TYPE_CAPITALIZATION, &status);
            UDisplayContext displayLength = uldn_getContext(uldn, UDISPCTX_TYPE_DISPLAY_LENGTH, &status);
            if (U_FAILURE(status)) {
                errln(UnicodeString("FAIL: uldn_getContext status ") + (int)status);
            } else if (dialectHandling != ctxtItemPtr->dialectHandling ||
                       capitalization != ctxtItemPtr->capitalization ||
                       displayLength != ctxtItemPtr->displayLength) {
                errln("FAIL: uldn_getContext retrieved incorrect dialectHandling, capitalization, or displayLength");
            } else {
                char16_t nameBuf[ULOC_FULLNAME_CAPACITY];
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
  const char *target = "root (Phonebook Sort Order)";
  ldn->localeDisplayName(locname, temp);
  test_assert_equal(target, temp);

  ldn->languageDisplayName("root", temp);
  test_assert_equal("root", temp);

  ldn->languageDisplayName("en_GB", temp);
  test_assert_equal("en_GB", temp);

  delete ldn;
}

void LocaleDisplayNamesTest::TestNumericRegionID() {
    UErrorCode err = U_ZERO_ERROR;
    ULocaleDisplayNames* ldn = uldn_open("es_MX", ULDN_STANDARD_NAMES, &err);
    char16_t displayName[200];
    uldn_regionDisplayName(ldn, "019", displayName, 200, &err);
    test_assert(U_SUCCESS(err));
    test_assert_equal(UnicodeString(u"América"), UnicodeString(displayName));
    uldn_close(ldn);    

    err = U_ZERO_ERROR; // reset in case the test above returned an error code
    ldn = uldn_open("en_AU", ULDN_STANDARD_NAMES, &err);
    uldn_regionDisplayName(ldn, "002", displayName, 200, &err);
    test_assert(U_SUCCESS(err));
    test_assert_equal(UnicodeString(u"Africa"), UnicodeString(displayName));
    uldn_close(ldn);    
}

static const char unknown_region[] = "wx";
static const char unknown_lang[] = "xy";
static const char unknown_script[] = "wxyz";
static const char unknown_variant[] = "abc";
static const char unknown_key[] = "efg";
static const char unknown_ca_value[] = "ijk";
static const char known_lang_unknown_script[] = "en-wxyz";
static const char unknown_lang_unknown_script[] = "xy-wxyz";
static const char unknown_lang_known_script[] = "xy-Latn";
static const char unknown_lang_unknown_region[] = "xy-wx";
static const char known_lang_unknown_region[] = "en-wx";
static const char unknown_lang_known_region[] = "xy-US";
static const char unknown_lang_unknown_script_unknown_region[] = "xy-wxyz-wx";
static const char known_lang_unknown_script_unknown_region[] = "en-wxyz-wx";
static const char unknown_lang_known_script_unknown_region[] = "xy-Latn-wx";
static const char unknown_lang_known_script_known_region[] = "xy-wxyz-US";
static const char known_lang[] = "en";
static const char known_lang_known_script[] = "en-Latn";
static const char known_lang_known_region[] = "en-US";
static const char known_lang_known_script_known_region[] = "en-Latn-US";

void LocaleDisplayNamesTest::VerifySubstitute(LocaleDisplayNames* ldn) {
  UnicodeString temp;
  // Ensure the default is UDISPCTX_SUBSTITUTE
  UDisplayContext context = ldn->getContext(UDISPCTX_TYPE_SUBSTITUTE_HANDLING);
  test_assert(UDISPCTX_SUBSTITUTE == context);

  ldn->regionDisplayName(unknown_region, temp);
  test_assert_equal(unknown_region, temp);
  ldn->languageDisplayName(unknown_lang, temp);
  test_assert_equal(unknown_lang, temp);
  ldn->scriptDisplayName(unknown_script, temp);
  test_assert_equal(unknown_script, temp);
  ldn->variantDisplayName(unknown_variant, temp);
  test_assert_equal(unknown_variant, temp);
  ldn->keyDisplayName(unknown_key, temp);
  test_assert_equal(unknown_key, temp);
  ldn->keyValueDisplayName("ca", unknown_ca_value, temp);
  test_assert_equal(unknown_ca_value, temp);

  ldn->localeDisplayName(unknown_lang, temp);
  test_assert_equal(unknown_lang, temp);
  ldn->localeDisplayName(known_lang_unknown_script, temp);
  test_assert_equal("Englisch (Wxyz)", temp);
  ldn->localeDisplayName(unknown_lang_unknown_script, temp);
  test_assert_equal("xy (Wxyz)", temp);
  ldn->localeDisplayName(unknown_lang_known_script, temp);
  test_assert_equal("xy (Lateinisch)", temp);
  ldn->localeDisplayName(unknown_lang_unknown_region, temp);
  test_assert_equal("xy (WX)", temp);
  ldn->localeDisplayName(known_lang_unknown_region, temp);
  test_assert_equal("Englisch (WX)", temp);
  ldn->localeDisplayName(unknown_lang_known_region, temp);
  test_assert_equal("xy (Vereinigte Staaten)", temp);
  ldn->localeDisplayName(unknown_lang_unknown_script_unknown_region, temp);
  test_assert_equal("xy (Wxyz, WX)", temp);
  ldn->localeDisplayName(known_lang_unknown_script_unknown_region, temp);
  test_assert_equal("Englisch (Wxyz, WX)", temp);
  ldn->localeDisplayName(unknown_lang_known_script_unknown_region, temp);
  test_assert_equal("xy (Lateinisch, WX)", temp);
  ldn->localeDisplayName(unknown_lang_known_script_known_region, temp);
  test_assert_equal("xy (Wxyz, Vereinigte Staaten)", temp);

  ldn->localeDisplayName(known_lang, temp);
  test_assert_equal("Englisch", temp);
  ldn->localeDisplayName(known_lang_known_script, temp);
  test_assert_equal("Englisch (Lateinisch)", temp);
  ldn->localeDisplayName(known_lang_known_region, temp);
  test_assert_equal("Englisch (Vereinigte Staaten)", temp);
  ldn->localeDisplayName(known_lang_known_script_known_region, temp);
  test_assert_equal("Englisch (Lateinisch, Vereinigte Staaten)", temp);
}

void LocaleDisplayNamesTest::VerifyNoSubstitute(LocaleDisplayNames* ldn) {
  UnicodeString temp("");
  std::string utf8;
  // Ensure the default is UDISPCTX_SUBSTITUTE
  UDisplayContext context = ldn->getContext(UDISPCTX_TYPE_SUBSTITUTE_HANDLING);
  test_assert(UDISPCTX_NO_SUBSTITUTE == context);

  ldn->regionDisplayName(unknown_region, temp);
  test_assert(true == temp.isBogus());
  ldn->languageDisplayName(unknown_lang, temp);
  test_assert(true == temp.isBogus());
  ldn->scriptDisplayName(unknown_script, temp);
  test_assert(true == temp.isBogus());
  ldn->variantDisplayName(unknown_variant, temp);
  test_assert(true == temp.isBogus());
  ldn->keyDisplayName(unknown_key, temp);
  test_assert(true == temp.isBogus());
  ldn->keyValueDisplayName("ca", unknown_ca_value, temp);
  test_assert(true == temp.isBogus());

  ldn->localeDisplayName(unknown_lang, temp);
  test_assert(true == temp.isBogus());
  ldn->localeDisplayName(known_lang_unknown_script, temp);
  test_assert(true == temp.isBogus());
  ldn->localeDisplayName(unknown_lang_unknown_script, temp);
  test_assert(true == temp.isBogus());
  ldn->localeDisplayName(unknown_lang_known_script, temp);
  test_assert(true == temp.isBogus());
  ldn->localeDisplayName(unknown_lang_unknown_region, temp);
  test_assert(true == temp.isBogus());
  ldn->localeDisplayName(known_lang_unknown_region, temp);
  test_assert(true == temp.isBogus());
  ldn->localeDisplayName(unknown_lang_known_region, temp);
  test_assert(true == temp.isBogus());
  ldn->localeDisplayName(unknown_lang_unknown_script_unknown_region, temp);
  test_assert(true == temp.isBogus());
  ldn->localeDisplayName(known_lang_unknown_script_unknown_region, temp);
  test_assert(true == temp.isBogus());
  ldn->localeDisplayName(unknown_lang_known_script_unknown_region, temp);
  test_assert(true == temp.isBogus());
  ldn->localeDisplayName(unknown_lang_known_script_known_region, temp);
  test_assert(true == temp.isBogus());

  ldn->localeDisplayName(known_lang, temp);
  test_assert_equal("Englisch", temp);
  ldn->localeDisplayName(known_lang_known_script, temp);
  test_assert_equal("Englisch (Lateinisch)", temp);
  ldn->localeDisplayName(known_lang_known_region, temp);
  test_assert_equal("Englisch (Vereinigte Staaten)", temp);
  ldn->localeDisplayName(known_lang_known_script_known_region, temp);
  test_assert_equal("Englisch (Lateinisch, Vereinigte Staaten)", temp);
}

void LocaleDisplayNamesTest::TestSubstituteHandling() {
  // With substitute as default
  logln("Context: none\n");
  std::unique_ptr<LocaleDisplayNames> ldn(LocaleDisplayNames::createInstance(Locale::getGermany()));
  VerifySubstitute(ldn.get());

  // With explicit set substitute, and standard names
  logln("Context: UDISPCTX_SUBSTITUTE, UDISPCTX_STANDARD_NAMES\n");
  UDisplayContext context_1[] = { UDISPCTX_SUBSTITUTE, UDISPCTX_STANDARD_NAMES };
  ldn.reset(LocaleDisplayNames::createInstance(Locale::getGermany(), context_1, 2));
  VerifySubstitute(ldn.get());

  // With explicit set substitute and dialect names
  logln("Context: UDISPCTX_SUBSTITUTE, UDISPCTX_DIALECT_NAMES\n");
  UDisplayContext context_2[] = { UDISPCTX_SUBSTITUTE, UDISPCTX_DIALECT_NAMES };
  ldn.reset(LocaleDisplayNames::createInstance(Locale::getGermany(), context_2, 2));
  VerifySubstitute(ldn.get());

  // With explicit set no_substitute, and standard names
  logln("Context: UDISPCTX_NO_SUBSTITUTE, UDISPCTX_STANDARD_NAMES\n");
  UDisplayContext context_3[] = { UDISPCTX_NO_SUBSTITUTE, UDISPCTX_STANDARD_NAMES };
  ldn.reset(LocaleDisplayNames::createInstance(Locale::getGermany(), context_3, 2));
  VerifyNoSubstitute(ldn.get());

  // With explicit set no_substitute and dialect names
  logln("Context: UDISPCTX_NO_SUBSTITUTE, UDISPCTX_DIALECT_NAMES\n");
  UDisplayContext context_4[] = { UDISPCTX_NO_SUBSTITUTE, UDISPCTX_DIALECT_NAMES };
  ldn.reset(LocaleDisplayNames::createInstance(Locale::getGermany(), context_4, 2));
  VerifyNoSubstitute(ldn.get());
}

#endif   /*  UCONFIG_NO_FORMATTING */
