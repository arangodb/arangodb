// © 2022 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "intltest.h"
#include "unicode/displayoptions.h"
#include "unicode/udisplayoptions.h"

class DisplayOptionsTest : public IntlTest {
  public:
    void testDisplayOptionsDefault();
    void testDisplayOptionsEachElement();
    void testDisplayOptionsUpdating();
    void testDisplayOptionsGetIdentifier();
    void testDisplayOptionsFromIdentifier();

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par = nullptr) override;
};

void DisplayOptionsTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln(u"TestSuite DisplayOptionsTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(testDisplayOptionsDefault);
    TESTCASE_AUTO(testDisplayOptionsEachElement);
    TESTCASE_AUTO(testDisplayOptionsUpdating);
    TESTCASE_AUTO(testDisplayOptionsGetIdentifier);
    TESTCASE_AUTO(testDisplayOptionsFromIdentifier);
    TESTCASE_AUTO_END;
}

void DisplayOptionsTest::testDisplayOptionsDefault() {
    icu::DisplayOptions displayOptions = icu::DisplayOptions::builder().build();
    assertEquals(u"Test setting parameters", UDISPOPT_GRAMMATICAL_CASE_UNDEFINED,
                 displayOptions.getGrammaticalCase());
    assertEquals(u"Test default values: ", UDISPOPT_NOUN_CLASS_UNDEFINED,
                 displayOptions.getNounClass());
    assertEquals(u"Test default values: ", UDISPOPT_PLURAL_CATEGORY_UNDEFINED,
                 displayOptions.getPluralCategory());
    assertEquals(u"Test default values: ", UDISPOPT_CAPITALIZATION_UNDEFINED,
                 displayOptions.getCapitalization());
    assertEquals(u"Test default values: ", UDISPOPT_NAME_STYLE_UNDEFINED,
                 displayOptions.getNameStyle());
    assertEquals(u"Test default values: ", UDISPOPT_DISPLAY_LENGTH_UNDEFINED,
                 displayOptions.getDisplayLength());
    assertEquals(u"Test default values: ", UDISPOPT_SUBSTITUTE_HANDLING_UNDEFINED,
                 displayOptions.getSubstituteHandling());
}

void DisplayOptionsTest::testDisplayOptionsEachElement() {
    icu::DisplayOptions displayOptions =
        icu::DisplayOptions::builder()
            .setGrammaticalCase(UDISPOPT_GRAMMATICAL_CASE_ABLATIVE)
            .build();
    assertEquals(u"Test setting parameters: ", UDISPOPT_GRAMMATICAL_CASE_ABLATIVE,
                 displayOptions.getGrammaticalCase());

    displayOptions =
        icu::DisplayOptions::builder().setNounClass(UDISPOPT_NOUN_CLASS_PERSONAL).build();
    assertEquals(u"Test setting parameters: ", UDISPOPT_NOUN_CLASS_PERSONAL,
                 displayOptions.getNounClass());

    displayOptions =
        icu::DisplayOptions::builder().setPluralCategory(UDISPOPT_PLURAL_CATEGORY_FEW).build();
    assertEquals(u"Test setting parameters: ", UDISPOPT_PLURAL_CATEGORY_FEW,
                 displayOptions.getPluralCategory());

    displayOptions = icu::DisplayOptions::builder()
                         .setCapitalization(UDISPOPT_CAPITALIZATION_BEGINNING_OF_SENTENCE)
                         .build();
    assertEquals(u"Test setting parameters: ", UDISPOPT_CAPITALIZATION_BEGINNING_OF_SENTENCE,
                 displayOptions.getCapitalization());

    displayOptions = icu::DisplayOptions::builder()
                         .setNameStyle(UDISPOPT_NAME_STYLE_STANDARD_NAMES)
                         .build();
    assertEquals(u"Test setting parameters: ", UDISPOPT_NAME_STYLE_STANDARD_NAMES,
                 displayOptions.getNameStyle());

    displayOptions = icu::DisplayOptions::builder()
                         .setDisplayLength(UDISPOPT_DISPLAY_LENGTH_FULL)
                         .build();
    assertEquals(u"Test setting parameters: ", UDISPOPT_DISPLAY_LENGTH_FULL,
                 displayOptions.getDisplayLength());

    displayOptions = icu::DisplayOptions::builder()
                         .setSubstituteHandling(UDISPOPT_SUBSTITUTE_HANDLING_NO_SUBSTITUTE)
                         .build();
    assertEquals(u"Test setting parameters: ", UDISPOPT_SUBSTITUTE_HANDLING_NO_SUBSTITUTE,
                 displayOptions.getSubstituteHandling());
}

void DisplayOptionsTest::testDisplayOptionsUpdating() {
    DisplayOptions displayOptions = DisplayOptions::builder()
                                        .setGrammaticalCase(UDISPOPT_GRAMMATICAL_CASE_ABLATIVE)
                                        .build();
    assertEquals(u"Test updating parameters: ", UDISPOPT_GRAMMATICAL_CASE_ABLATIVE,
                 displayOptions.getGrammaticalCase());
    assertEquals(u"Test updating parameters: ", UDISPOPT_NOUN_CLASS_UNDEFINED,
                 displayOptions.getNounClass());
    assertEquals(u"Test updating parameters: ", UDISPOPT_PLURAL_CATEGORY_UNDEFINED,
                 displayOptions.getPluralCategory());
    assertEquals(u"Test updating parameters: ", UDISPOPT_CAPITALIZATION_UNDEFINED,
                 displayOptions.getCapitalization());
    assertEquals(u"Test updating parameters: ", UDISPOPT_NAME_STYLE_UNDEFINED,
                 displayOptions.getNameStyle());
    assertEquals(u"Test updating parameters: ", UDISPOPT_DISPLAY_LENGTH_UNDEFINED,
                 displayOptions.getDisplayLength());
    assertEquals(u"Test updating parameters: ", UDISPOPT_SUBSTITUTE_HANDLING_UNDEFINED,
                 displayOptions.getSubstituteHandling());

    displayOptions =
        displayOptions.copyToBuilder().setNounClass(UDISPOPT_NOUN_CLASS_PERSONAL).build();
    assertEquals(u"Test updating parameters: ", UDISPOPT_GRAMMATICAL_CASE_ABLATIVE,
                 displayOptions.getGrammaticalCase());
    assertEquals(u"Test updating parameters: ", UDISPOPT_NOUN_CLASS_PERSONAL,
                 displayOptions.getNounClass());
    assertEquals(u"Test updating parameters: ", UDISPOPT_PLURAL_CATEGORY_UNDEFINED,
                 displayOptions.getPluralCategory());
    assertEquals(u"Test updating parameters: ", UDISPOPT_CAPITALIZATION_UNDEFINED,
                 displayOptions.getCapitalization());
    assertEquals(u"Test updating parameters: ", UDISPOPT_NAME_STYLE_UNDEFINED,
                 displayOptions.getNameStyle());
    assertEquals(u"Test updating parameters: ", UDISPOPT_DISPLAY_LENGTH_UNDEFINED,
                 displayOptions.getDisplayLength());
    assertEquals(u"Test updating parameters: ", UDISPOPT_SUBSTITUTE_HANDLING_UNDEFINED,
                 displayOptions.getSubstituteHandling());

    displayOptions =
        displayOptions.copyToBuilder().setPluralCategory(UDISPOPT_PLURAL_CATEGORY_FEW).build();
    assertEquals(u"Test updating parameters: ", UDISPOPT_GRAMMATICAL_CASE_ABLATIVE,
                 displayOptions.getGrammaticalCase());
    assertEquals(u"Test updating parameters: ", UDISPOPT_NOUN_CLASS_PERSONAL,
                 displayOptions.getNounClass());
    assertEquals(u"Test updating parameters: ", UDISPOPT_PLURAL_CATEGORY_FEW,
                 displayOptions.getPluralCategory());
    assertEquals(u"Test updating parameters: ", UDISPOPT_CAPITALIZATION_UNDEFINED,
                 displayOptions.getCapitalization());
    assertEquals(u"Test updating parameters: ", UDISPOPT_NAME_STYLE_UNDEFINED,
                 displayOptions.getNameStyle());
    assertEquals(u"Test updating parameters: ", UDISPOPT_DISPLAY_LENGTH_UNDEFINED,
                 displayOptions.getDisplayLength());
    assertEquals(u"Test updating parameters: ", UDISPOPT_SUBSTITUTE_HANDLING_UNDEFINED,
                 displayOptions.getSubstituteHandling());

    displayOptions = displayOptions.copyToBuilder()
                         .setCapitalization(UDISPOPT_CAPITALIZATION_BEGINNING_OF_SENTENCE)
                         .build();
    assertEquals(u"Test updating parameters: ", UDISPOPT_GRAMMATICAL_CASE_ABLATIVE,
                 displayOptions.getGrammaticalCase());
    assertEquals(u"Test updating parameters: ", UDISPOPT_NOUN_CLASS_PERSONAL,
                 displayOptions.getNounClass());
    assertEquals(u"Test updating parameters: ", UDISPOPT_PLURAL_CATEGORY_FEW,
                 displayOptions.getPluralCategory());
    assertEquals(u"Test updating parameters: ", UDISPOPT_CAPITALIZATION_BEGINNING_OF_SENTENCE,
                 displayOptions.getCapitalization());
    assertEquals(u"Test updating parameters: ", UDISPOPT_NAME_STYLE_UNDEFINED,
                 displayOptions.getNameStyle());
    assertEquals(u"Test updating parameters: ", UDISPOPT_DISPLAY_LENGTH_UNDEFINED,
                 displayOptions.getDisplayLength());
    assertEquals(u"Test updating parameters: ", UDISPOPT_SUBSTITUTE_HANDLING_UNDEFINED,
                 displayOptions.getSubstituteHandling());

    displayOptions = displayOptions.copyToBuilder()
                         .setNameStyle(UDISPOPT_NAME_STYLE_STANDARD_NAMES)
                         .build();
    assertEquals(u"Test updating parameters: ", UDISPOPT_GRAMMATICAL_CASE_ABLATIVE,
                 displayOptions.getGrammaticalCase());
    assertEquals(u"Test updating parameters: ", UDISPOPT_NOUN_CLASS_PERSONAL,
                 displayOptions.getNounClass());
    assertEquals(u"Test updating parameters: ", UDISPOPT_PLURAL_CATEGORY_FEW,
                 displayOptions.getPluralCategory());
    assertEquals(u"Test updating parameters: ", UDISPOPT_CAPITALIZATION_BEGINNING_OF_SENTENCE,
                 displayOptions.getCapitalization());
    assertEquals(u"Test updating parameters: ", UDISPOPT_NAME_STYLE_STANDARD_NAMES,
                 displayOptions.getNameStyle());
    assertEquals(u"Test updating parameters: ", UDISPOPT_DISPLAY_LENGTH_UNDEFINED,
                 displayOptions.getDisplayLength());
    assertEquals(u"Test updating parameters: ", UDISPOPT_SUBSTITUTE_HANDLING_UNDEFINED,
                 displayOptions.getSubstituteHandling());

    displayOptions = displayOptions.copyToBuilder()
                         .setDisplayLength(UDISPOPT_DISPLAY_LENGTH_FULL)
                         .build();
    assertEquals(u"Test updating parameters: ", UDISPOPT_GRAMMATICAL_CASE_ABLATIVE,
                 displayOptions.getGrammaticalCase());
    assertEquals(u"Test updating parameters: ", UDISPOPT_NOUN_CLASS_PERSONAL,
                 displayOptions.getNounClass());
    assertEquals(u"Test updating parameters: ", UDISPOPT_PLURAL_CATEGORY_FEW,
                 displayOptions.getPluralCategory());
    assertEquals(u"Test updating parameters: ", UDISPOPT_CAPITALIZATION_BEGINNING_OF_SENTENCE,
                 displayOptions.getCapitalization());
    assertEquals(u"Test updating parameters: ", UDISPOPT_NAME_STYLE_STANDARD_NAMES,
                 displayOptions.getNameStyle());
    assertEquals(u"Test updating parameters: ", UDISPOPT_DISPLAY_LENGTH_FULL,
                 displayOptions.getDisplayLength());
    assertEquals(u"Test updating parameters: ", UDISPOPT_SUBSTITUTE_HANDLING_UNDEFINED,
                 displayOptions.getSubstituteHandling());

    displayOptions = displayOptions.copyToBuilder()
                         .setSubstituteHandling(UDISPOPT_SUBSTITUTE_HANDLING_NO_SUBSTITUTE)
                         .build();
    assertEquals(u"Test updating parameters: ", UDISPOPT_GRAMMATICAL_CASE_ABLATIVE,
                 displayOptions.getGrammaticalCase());
    assertEquals(u"Test updating parameters: ", UDISPOPT_NOUN_CLASS_PERSONAL,
                 displayOptions.getNounClass());
    assertEquals(u"Test updating parameters: ", UDISPOPT_PLURAL_CATEGORY_FEW,
                 displayOptions.getPluralCategory());
    assertEquals(u"Test updating parameters: ", UDISPOPT_CAPITALIZATION_BEGINNING_OF_SENTENCE,
                 displayOptions.getCapitalization());
    assertEquals(u"Test updating parameters: ", UDISPOPT_NAME_STYLE_STANDARD_NAMES,
                 displayOptions.getNameStyle());
    assertEquals(u"Test updating parameters: ", UDISPOPT_DISPLAY_LENGTH_FULL,
                 displayOptions.getDisplayLength());
    assertEquals(u"Test updating parameters: ", UDISPOPT_SUBSTITUTE_HANDLING_NO_SUBSTITUTE,
                 displayOptions.getSubstituteHandling());
}

void DisplayOptionsTest::testDisplayOptionsGetIdentifier() {

    assertEquals(u"test get identifier: ", "undefined",
                 udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_UNDEFINED));
    assertEquals(u"test get identifier: ", "ablative",
                 udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_ABLATIVE));
    assertEquals(u"test get identifier: ", "accusative",
                 udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_ACCUSATIVE));
    assertEquals(u"test get identifier: ", "comitative",
                 udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_COMITATIVE));
    assertEquals(u"test get identifier: ", "dative",
                 udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_DATIVE));
    assertEquals(u"test get identifier: ", "ergative",
                 udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_ERGATIVE));
    assertEquals(u"test get identifier: ", "genitive",
                 udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_GENITIVE));
    assertEquals(
        u"test get identifier: ", "instrumental",
        udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_INSTRUMENTAL));
    assertEquals(u"test get identifier: ", "locative",
                 udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_LOCATIVE));
    assertEquals(
        u"test get identifier: ", "locative_copulative",
        udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_LOCATIVE_COPULATIVE));
    assertEquals(u"test get identifier: ", "nominative",
                 udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_NOMINATIVE));
    assertEquals(u"test get identifier: ", "oblique",
                 udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_OBLIQUE));
    assertEquals(
        u"test get identifier: ", "prepositional",
        udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_PREPOSITIONAL));
    assertEquals(u"test get identifier: ", "sociative",
                 udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_SOCIATIVE));
    assertEquals(u"test get identifier: ", "vocative",
                 udispopt_getGrammaticalCaseIdentifier(UDISPOPT_GRAMMATICAL_CASE_VOCATIVE));

    assertEquals(u"test get identifier: ", "undefined",
                 udispopt_getPluralCategoryIdentifier(UDISPOPT_PLURAL_CATEGORY_UNDEFINED));
    assertEquals(u"test get identifier: ", "zero",
                 udispopt_getPluralCategoryIdentifier(UDISPOPT_PLURAL_CATEGORY_ZERO));
    assertEquals(u"test get identifier: ", "one",
                 udispopt_getPluralCategoryIdentifier(UDISPOPT_PLURAL_CATEGORY_ONE));
    assertEquals(u"test get identifier: ", "two",
                 udispopt_getPluralCategoryIdentifier(UDISPOPT_PLURAL_CATEGORY_TWO));
    assertEquals(u"test get identifier: ", "few",
                 udispopt_getPluralCategoryIdentifier(UDISPOPT_PLURAL_CATEGORY_FEW));
    assertEquals(u"test get identifier: ", "many",
                 udispopt_getPluralCategoryIdentifier(UDISPOPT_PLURAL_CATEGORY_MANY));
    assertEquals(u"test get identifier: ", "other",
                 udispopt_getPluralCategoryIdentifier(UDISPOPT_PLURAL_CATEGORY_OTHER));

    assertEquals(u"test get identifier: ", "undefined",
                 udispopt_getNounClassIdentifier(UDISPOPT_NOUN_CLASS_UNDEFINED));
    assertEquals(u"test get identifier: ", "other",
                 udispopt_getNounClassIdentifier(UDISPOPT_NOUN_CLASS_OTHER));
    assertEquals(u"test get identifier: ", "neuter",
                 udispopt_getNounClassIdentifier(UDISPOPT_NOUN_CLASS_NEUTER));
    assertEquals(u"test get identifier: ", "feminine",
                 udispopt_getNounClassIdentifier(UDISPOPT_NOUN_CLASS_FEMININE));
    assertEquals(u"test get identifier: ", "masculine",
                 udispopt_getNounClassIdentifier(UDISPOPT_NOUN_CLASS_MASCULINE));
    assertEquals(u"test get identifier: ", "animate",
                 udispopt_getNounClassIdentifier(UDISPOPT_NOUN_CLASS_ANIMATE));
    assertEquals(u"test get identifier: ", "inanimate",
                 udispopt_getNounClassIdentifier(UDISPOPT_NOUN_CLASS_INANIMATE));
    assertEquals(u"test get identifier: ", "personal",
                 udispopt_getNounClassIdentifier(UDISPOPT_NOUN_CLASS_PERSONAL));
    assertEquals(u"test get identifier: ", "common",
                 udispopt_getNounClassIdentifier(UDISPOPT_NOUN_CLASS_COMMON));
}

void DisplayOptionsTest::testDisplayOptionsFromIdentifier() {

    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_UNDEFINED,
                 udispopt_fromGrammaticalCaseIdentifier(""));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_UNDEFINED,
                 udispopt_fromGrammaticalCaseIdentifier("undefined"));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_ABLATIVE,
                 udispopt_fromGrammaticalCaseIdentifier("ablative"));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_ACCUSATIVE,
                 udispopt_fromGrammaticalCaseIdentifier("accusative"));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_COMITATIVE,
                 udispopt_fromGrammaticalCaseIdentifier("comitative"));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_DATIVE,
                 udispopt_fromGrammaticalCaseIdentifier("dative"));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_ERGATIVE,
                 udispopt_fromGrammaticalCaseIdentifier("ergative"));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_GENITIVE,
                 udispopt_fromGrammaticalCaseIdentifier("genitive"));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_INSTRUMENTAL,
                 udispopt_fromGrammaticalCaseIdentifier("instrumental"));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_LOCATIVE,
                 udispopt_fromGrammaticalCaseIdentifier("locative"));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_LOCATIVE_COPULATIVE,
                 udispopt_fromGrammaticalCaseIdentifier("locative_copulative"));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_NOMINATIVE,
                 udispopt_fromGrammaticalCaseIdentifier("nominative"));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_OBLIQUE,
                 udispopt_fromGrammaticalCaseIdentifier("oblique"));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_PREPOSITIONAL,
                 udispopt_fromGrammaticalCaseIdentifier("prepositional"));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_SOCIATIVE,
                 udispopt_fromGrammaticalCaseIdentifier("sociative"));
    assertEquals(u"test from identifier: ", UDISPOPT_GRAMMATICAL_CASE_VOCATIVE,
                 udispopt_fromGrammaticalCaseIdentifier("vocative"));

    assertEquals(u"test from identifier: ", UDISPOPT_PLURAL_CATEGORY_UNDEFINED,
                 udispopt_fromPluralCategoryIdentifier(""));
    assertEquals(u"test from identifier: ", UDISPOPT_PLURAL_CATEGORY_UNDEFINED,
                 udispopt_fromPluralCategoryIdentifier("undefined"));
    assertEquals(u"test from identifier: ", UDISPOPT_PLURAL_CATEGORY_ZERO,
                 udispopt_fromPluralCategoryIdentifier("zero"));
    assertEquals(u"test from identifier: ", UDISPOPT_PLURAL_CATEGORY_ONE,
                 udispopt_fromPluralCategoryIdentifier("one"));
    assertEquals(u"test from identifier: ", UDISPOPT_PLURAL_CATEGORY_TWO,
                 udispopt_fromPluralCategoryIdentifier("two"));
    assertEquals(u"test from identifier: ", UDISPOPT_PLURAL_CATEGORY_FEW,
                 udispopt_fromPluralCategoryIdentifier("few"));
    assertEquals(u"test from identifier: ", UDISPOPT_PLURAL_CATEGORY_MANY,
                 udispopt_fromPluralCategoryIdentifier("many"));
    assertEquals(u"test from identifier: ", UDISPOPT_PLURAL_CATEGORY_OTHER,
                 udispopt_fromPluralCategoryIdentifier("other"));

    assertEquals(u"test from identifier: ", UDISPOPT_NOUN_CLASS_UNDEFINED,
                 udispopt_fromNounClassIdentifier(""));
    assertEquals(u"test from identifier: ", UDISPOPT_NOUN_CLASS_UNDEFINED,
                 udispopt_fromNounClassIdentifier("undefined"));
    assertEquals(u"test from identifier: ", UDISPOPT_NOUN_CLASS_OTHER,
                 udispopt_fromNounClassIdentifier("other"));
    assertEquals(u"test from identifier: ", UDISPOPT_NOUN_CLASS_NEUTER,
                 udispopt_fromNounClassIdentifier("neuter"));
    assertEquals(u"test from identifier: ", UDISPOPT_NOUN_CLASS_FEMININE,
                 udispopt_fromNounClassIdentifier("feminine"));
    assertEquals(u"test from identifier: ", UDISPOPT_NOUN_CLASS_MASCULINE,
                 udispopt_fromNounClassIdentifier("masculine"));
    assertEquals(u"test from identifier: ", UDISPOPT_NOUN_CLASS_ANIMATE,
                 udispopt_fromNounClassIdentifier("animate"));
    assertEquals(u"test from identifier: ", UDISPOPT_NOUN_CLASS_INANIMATE,
                 udispopt_fromNounClassIdentifier("inanimate"));
    assertEquals(u"test from identifier: ", UDISPOPT_NOUN_CLASS_PERSONAL,
                 udispopt_fromNounClassIdentifier("personal"));
    assertEquals(u"test from identifier: ", UDISPOPT_NOUN_CLASS_COMMON,
                 udispopt_fromNounClassIdentifier("common"));
}

extern IntlTest *createDisplayOptionsTest() { return new DisplayOptionsTest(); }

#endif /* #if !UCONFIG_NO_FORMATTING */
