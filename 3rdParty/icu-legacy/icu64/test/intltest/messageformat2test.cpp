// © 2024 and later: Unicode, Inc. and others.

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "unicode/calendar.h"
#include "messageformat2test.h"

using namespace icu::message2;

/*
  TODO: Tests need to be unified in a single format that
  both ICU4C and ICU4J can use, rather than being embedded in code.

  Tests are included in their current state to give a sense of
  how much test coverage has been achieved. Most of the testing is
  of the parser/serializer; the formatter needs to be tested more
  thoroughly.
*/

/*
Tests reflect the syntax specified in

  https://github.com/unicode-org/message-format-wg/commits/main/spec/message.abnf

as of the following commit from 2023-05-09:
  https://github.com/unicode-org/message-format-wg/commit/194f6efcec5bf396df36a19bd6fa78d1fa2e0867

*/

static const int32_t numValidTestCases = 55;
TestResult validTestCases[] = {
    {"hello {|4.2| :number}", "hello 4.2"},
    {"hello {|4.2| :number minimumFractionDigits=2}", "hello 4.20"},
    {"hello {|4.2| :number minimumFractionDigits = 2}", "hello 4.20"},
    {"hello {|4.2| :number minimumFractionDigits= 2}", "hello 4.20"},
    {"hello {|4.2| :number minimumFractionDigits =2}", "hello 4.20"},
    {"hello {|4.2| :number minimumFractionDigits=2  }", "hello 4.20"},
    {"hello {|4.2| :number minimumFractionDigits=2 bar=3}", "hello 4.20"},
    {"hello {|4.2| :number minimumFractionDigits=2 bar=3  }", "hello 4.20"},
    {"hello {|4.2| :number minimumFractionDigits=|2|}", "hello 4.20"},
    {"content -tag", "content -tag"},
    {"", ""},
    // tests for escape sequences in literals
    {"{|hel\\\\lo|}", "hel\\lo"},
    {"{|hel\\|lo|}", "hel|lo"},
    {"{|hel\\|\\\\lo|}", "hel|\\lo"},
    // tests for text escape sequences
    {"hel\\{lo", "hel{lo"},
    {"hel\\}lo", "hel}lo"},
    {"hel\\\\lo", "hel\\lo"},
    {"hel\\{\\\\lo", "hel{\\lo"},
    {"hel\\{\\}lo", "hel{}lo"},
    // tests for newlines in literals and text
    {"hello {|wo\nrld|}", "hello wo\nrld"},
    {"hello wo\nrld", "hello wo\nrld"},
    // Markup is ignored when formatting to string
    {"{#tag/} content", " content"},
    {"{#tag} content", " content"},
    {"{#tag/} {|content|}", " content"},
    {"{#tag} {|content|}", " content"},
    {"{|content|} {#tag/}", "content "},
    {"{|content|} {#tag}", "content "},
    {"{/tag} {|content|}", " content"},
    {"{|content|} {/tag}", "content "},
    {"{#tag} {|content|} {/tag}", " content "},
    {"{/tag} {|content|} {#tag}", " content "},
    {"{#tag/} {|content|} {#tag}", " content "},
    {"{#tag/} {|content|} {/tag}", " content "},
    {"{#tag foo=bar/} {|content|}", " content"},
    {"{#tag foo=bar} {|content|}", " content"},
    {"{/tag foo=bar} {|content|}", " content"},
    {"{#tag foo=bar} {|content|} {/tag foo=bar}", " content "},
    {"{/tag foo=bar} {|content|} {#tag foo=bar}", " content "},
    {"{#tag foo=bar /} {|content|} {#tag foo=bar}", " content "},
    {"{#tag foo=bar/} {|content|} {/tag foo=bar}", " content "},
    // Attributes are ignored
    {"The value is {horse @horse}.", "The value is horse."},
    {"hello {|4.2| @number}", "hello 4.2"},
    {"The value is {horse @horse=cool}.", "The value is horse."},
    {"hello {|4.2| @number=5}", "hello 4.2"},
    // Number literals
    {"{-1}", "-1"},
    {"{0}", "0"},
    {"{0.0123}", "0.0123"},
    {"{1.234e5}", "1.234e5"},
    {"{1.234E5}", "1.234E5"},
    {"{1.234E+5}", "1.234E+5"},
    {"{1.234e-5}", "1.234e-5"},
    {"{42e5}", "42e5"},
    {"{42e0}", "42e0"},
    {"{42e000}", "42e000"},
    {"{42e369}", "42e369"},
};


static const int32_t numResolutionErrors = 3;
TestResultError jsonTestCasesResolutionError[] = {
    {".local $foo = {$bar} .match {$foo :number}  one {{one}}  * {{other}}", "other", U_MF_UNRESOLVED_VARIABLE_ERROR},
    {".local $foo = {$bar} .match {$foo :number}  one {{one}}  * {{other}}", "other", U_MF_UNRESOLVED_VARIABLE_ERROR},
    {".local $bar = {$none :number} .match {$foo :string}  one {{one}}  * {{{$bar}}}", "{$none}", U_MF_UNRESOLVED_VARIABLE_ERROR}
};

static const int32_t numReservedErrors = 34;
UnicodeString reservedErrors[] = {
    // tests for reserved syntax
    "hello {|4.2| %number}",
    "hello {|4.2| %n|um|ber}",
    "{+42}",
    // Private use -- n.b. this implementation doesn't support
    // any private-use annotations, so it's treated like reserved
    "hello {|4.2| &num|be|r}",
    "hello {|4.2| ^num|be|r}",
    "hello {|4.2| +num|be|r}",
    "hello {|4.2| ?num|be||r|s}",
    "hello {|foo| !number}",
    "hello {|foo| *number}",
    "hello {#number}",
    "{<tag}",
    ".local $bar = {$none ~plural} .match {$foo :string}  * {{{$bar}}}",
    // tests for reserved syntax with escaped chars
    "hello {|4.2| %num\\\\ber}",
    "hello {|4.2| %num\\{be\\|r}",
    "hello {|4.2| %num\\\\\\}ber}",
    // tests for reserved syntax
    "hello {|4.2| !}",
    "hello {|4.2| %}",
    "hello {|4.2| *}",
    "hello {|4.2| ^abc|123||5|\\\\}",
    "hello {|4.2| ^ abc|123||5|\\\\}",
    "hello {|4.2| ^ abc|123||5|\\\\ \\|def |3.14||2|}",
    // tests for reserved syntax with trailing whitespace
    "hello {|4.2| ? }",
    "hello {|4.2| %xyzz }",
    "hello {|4.2| >xyzz   }",
    "hello {$foo ~xyzz }",
    "hello {$x   <xyzz   }",
    "{>xyzz }",
    "{  !xyzz   }",
    "{~xyzz }",
    "{ <xyzz   }",
    // tests for reserved syntax with space-separated sequences
    "hello {|4.2| !xy z z }",
    "hello {|4.2| *num \\\\ b er}",
    "hello {|4.2| %num \\\\ b |3.14| r    }",
    "hello {|4.2|    +num xx \\\\ b |3.14| r  }",
    "hello {$foo    +num x \\\\ abcde |3.14| r  }",
    "hello {$foo    >num x \\\\ abcde |aaa||3.14||42| r  }",
    "hello {$foo    >num x \\\\ abcde |aaa||3.14| |42| r  }",
    0
};

static const int32_t numMatches = 15;
UnicodeString matches[] = {
    // multiple scrutinees, with or without whitespace
    "match {$foo :string} {$bar :string} when one * {one} when * * {other}",
    "match {$foo :string} {$bar :string}when one * {one} when * * {other}",
    "match {$foo :string}{$bar :string} when one * {one} when * * {other}",
    "match {$foo :string}{$bar :string}when one * {one} when * * {other}",
    "match{$foo :string} {$bar :string} when one * {one} when * * {other}",
    "match{$foo :string} {$bar :string}when one * {one} when * * {other}",
    "match{$foo :string}{$bar :string} when one * {one} when * * {other}",
    "match{$foo :string}{$bar :string}when one * {one} when * * {other}",
    // multiple variants, with or without whitespace
    "match {$foo :string} {$bar :string} when one * {one} when * * {other}",
    "match {$foo :string} {$bar :string} when one * {one}when * * {other}",
    "match {$foo :string} {$bar :string}when one * {one} when * * {other}",
    "match {$foo :string} {$bar :string}when one * {one}when * * {other}",
    // one or multiple keys, with or without whitespace before pattern
    "match {$foo :string} {$bar :string} when one *{one} when * * {foo}",
    "match {$foo :string} {$bar :string} when one * {one} when * * {foo}",
    "match {$foo :string} {$bar :string} when one *  {one} when * * {foo}",
    0
};

static const int32_t numSyntaxTests = 19;
// These patterns are tested to ensure they parse without a syntax error
UnicodeString syntaxTests[] = {
    "hello {|foo| :number   }",
    // zero, one or multiple options, with or without whitespace before '}'
    "{:foo}",
    "{:foo }",
    "{:foo   }",
    "{:foo k=v}",
    "{:foo k=v   }",
    "{:foo k1=v1   k2=v2}",
    "{:foo k1=v1   k2=v2   }",
    // literals or variables followed by space, with or without an annotation following
    "{|3.14| }",
    "{|3.14|    }",
    "{|3.14|    :foo}",
    "{|3.14|    :foo   }",
    "{$bar }",
    "{$bar    }",
    "{$bar    :foo}",
    "{$bar    :foo   }",
    // Variable names can contain '-'
    "{$bar-foo}",
    // Not a syntax error (is a semantic error)
    ".local $foo = {|hello|} .local $foo = {$foo} {{{$foo}}}",
    // Unquoted literal -- should work
    "good {placeholder}",
    0
};

void
TestMessageFormat2::runIndexedTest(int32_t index, UBool exec,
                                  const char* &name, char* /*par*/) {
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(testAPICustomFunctions);
    TESTCASE_AUTO(messageFormat1Tests);
    TESTCASE_AUTO(featureTests);
    TESTCASE_AUTO(testCustomFunctions);
    TESTCASE_AUTO(testBuiltInFunctions);
    TESTCASE_AUTO(testDataModelErrors);
    TESTCASE_AUTO(testResolutionErrors);
    TESTCASE_AUTO(testAPI);
    TESTCASE_AUTO(testAPISimple);
    TESTCASE_AUTO(testDataModelAPI);
    TESTCASE_AUTO(testVariousPatterns);
    TESTCASE_AUTO(testInvalidPatterns);
    TESTCASE_AUTO(specTests);
    TESTCASE_AUTO_END;
}

// Needs more tests
void TestMessageFormat2::testDataModelAPI() {
    IcuTestErrorCode errorCode1(*this, "testAPI");
    UErrorCode errorCode = (UErrorCode) errorCode1;

    using Pattern = data_model::Pattern;

    Pattern::Builder builder(errorCode);

    builder.add("a", errorCode);
    builder.add("b", errorCode);
    builder.add("c", errorCode);

    Pattern p = builder.build(errorCode);
    int32_t i = 0;
    for (auto iter = p.begin(); iter != p.end(); ++iter) {
        std::variant<UnicodeString, Expression, Markup> part = *iter;
        UnicodeString val = *std::get_if<UnicodeString>(&part);
        if (i == 0) {
            assertEquals("testDataModelAPI", val, "a");
        } else if (i == 1) {
            assertEquals("testDataModelAPI", val, "b");
        } else if (i == 2) {
            assertEquals("testDataModelAPI", val, "c");
        }
        i++;
    }
    assertEquals("testDataModelAPI", i, 3);
}

// Example for design doc -- version without null and error checks
void TestMessageFormat2::testAPISimple() {
    IcuTestErrorCode errorCode1(*this, "testAPI");
    UErrorCode errorCode = (UErrorCode) errorCode1;
    UParseError parseError;
    Locale locale = "en_US";

    // Since this is the example used in the
    // design doc, it elides null checks and error checks.
    // To be used in the test suite, it should include those checks
    // Null checks and error checks elided
    MessageFormatter::Builder builder(errorCode);
    MessageFormatter mf = builder.setPattern(u"Hello, {$userName}!", parseError, errorCode)
        .build(errorCode);

    std::map<UnicodeString, message2::Formattable> argsBuilder;
    argsBuilder["userName"] = message2::Formattable("John");
    MessageArguments args(argsBuilder, errorCode);

    UnicodeString result;
    result = mf.formatToString(args, errorCode);
    assertEquals("testAPI", result, "Hello, John!");

    mf = builder.setPattern("Today is {$today :date style=full}.", parseError, errorCode)
        .setLocale(locale)
        .build(errorCode);

    Calendar* cal(Calendar::createInstance(errorCode));
    // Sunday, October 28, 2136 8:39:12 AM PST
    cal->set(2136, Calendar::OCTOBER, 28, 8, 39, 12);
    UDate date = cal->getTime(errorCode);

    argsBuilder.clear();
    argsBuilder["today"] = message2::Formattable::forDate(date);
    args = MessageArguments(argsBuilder, errorCode);
    result = mf.formatToString(args, errorCode);
    assertEquals("testAPI", "Today is Sunday, October 28, 2136.", result);

    argsBuilder.clear();
    argsBuilder["photoCount"] = message2::Formattable((int64_t) 12);
    argsBuilder["userGender"] = message2::Formattable("feminine");
    argsBuilder["userName"] = message2::Formattable("Maria");
    args = MessageArguments(argsBuilder, errorCode);

    mf = builder.setPattern(".match {$photoCount :number} {$userGender :string}\n\
                      1 masculine {{{$userName} added a new photo to his album.}}\n \
                      1 feminine {{{$userName} added a new photo to her album.}}\n \
                      1 * {{{$userName} added a new photo to their album.}}\n \
                      * masculine {{{$userName} added {$photoCount} photos to his album.}}\n \
                      * feminine {{{$userName} added {$photoCount} photos to her album.}}\n \
                      * * {{{$userName} added {$photoCount} photos to their album.}}", parseError, errorCode)
        .setLocale(locale)
        .build(errorCode);
    result = mf.formatToString(args, errorCode);
    assertEquals("testAPI", "Maria added 12 photos to her album.", result);

    delete cal;
}

// Design doc example, with more details
void TestMessageFormat2::testAPI() {
    IcuTestErrorCode errorCode(*this, "testAPI");
    TestCase::Builder testBuilder;

    // Pattern: "Hello, {$userName}!"
    TestCase test(testBuilder.setName("testAPI")
                  .setPattern("Hello, {$userName}!")
                  .setArgument("userName", "John")
                  .setExpected("Hello, John!")
                  .setLocale("en_US")
                  .build());
    TestUtils::runTestCase(*this, test, errorCode);

    // Pattern: "{Today is {$today ..."
    LocalPointer<Calendar> cal(Calendar::createInstance(errorCode));
    // Sunday, October 28, 2136 8:39:12 AM PST
    cal->set(2136, Calendar::OCTOBER, 28, 8, 39, 12);
    UDate date = cal->getTime(errorCode);

    test = testBuilder.setName("testAPI")
        .setPattern("Today is {$today :date style=full}.")
        .setDateArgument("today", date)
        .setExpected("Today is Sunday, October 28, 2136.")
        .setLocale("en_US")
        .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Pattern matching - plural
    UnicodeString pattern = ".match {$photoCount :string} {$userGender :string}\n\
                      1 masculine {{{$userName} added a new photo to his album.}}\n \
                      1 feminine {{{$userName} added a new photo to her album.}}\n \
                      1 * {{{$userName} added a new photo to their album.}}\n \
                      * masculine {{{$userName} added {$photoCount} photos to his album.}}\n \
                      * feminine {{{$userName} added {$photoCount} photos to her album.}}\n \
                      * * {{{$userName} added {$photoCount} photos to their album.}}";


    int64_t photoCount = 12;
    test = testBuilder.setName("testAPI")
        .setPattern(pattern)
        .setArgument("photoCount", photoCount)
        .setArgument("userGender", "feminine")
        .setArgument("userName", "Maria")
        .setExpected("Maria added 12 photos to her album.")
        .setLocale("en_US")
        .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Built-in functions
    pattern = ".match {$photoCount :number} {$userGender :string}\n\
                      1 masculine {{{$userName} added a new photo to his album.}}\n \
                      1 feminine {{{$userName} added a new photo to her album.}}\n \
                      1 * {{{$userName} added a new photo to their album.}}\n \
                      * masculine {{{$userName} added {$photoCount} photos to his album.}}\n \
                      * feminine {{{$userName} added {$photoCount} photos to her album.}}\n \
                      * * {{{$userName} added {$photoCount} photos to their album.}}";

    photoCount = 1;
    test = testBuilder.setName("testAPI")
        .setPattern(pattern)
        .setArgument("photoCount", photoCount)
        .setArgument("userGender", "feminine")
        .setArgument("userName", "Maria")
        .setExpected("Maria added a new photo to her album.")
        .setLocale("en_US")
        .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

// Custom functions example from the ICU4C API design doc
// Note: error/null checks are omitted
void TestMessageFormat2::testAPICustomFunctions() {
    IcuTestErrorCode errorCode1(*this, "testAPICustomFunctions");
    UErrorCode errorCode = (UErrorCode) errorCode1;
    UParseError parseError;
    Locale locale = "en_US";

    // Set up custom function registry
    MFFunctionRegistry::Builder builder(errorCode);
    MFFunctionRegistry functionRegistry =
        builder.adoptFormatter(data_model::FunctionName("person"), new PersonNameFormatterFactory(), errorCode)
               .build();

    Person* person = new Person(UnicodeString("Mr."), UnicodeString("John"), UnicodeString("Doe"));

    std::map<UnicodeString, message2::Formattable> argsBuilder;
    argsBuilder["name"] = message2::Formattable(person);
    MessageArguments arguments(argsBuilder, errorCode);

    MessageFormatter::Builder mfBuilder(errorCode);
    UnicodeString result;
    // This fails, because we did not provide a function registry:
    MessageFormatter mf = mfBuilder.setPattern("Hello {$name :person formality=informal}", parseError, errorCode)
                                    .setLocale(locale)
                                    .build(errorCode);
    result = mf.formatToString(arguments, errorCode);
    assertEquals("testAPICustomFunctions", U_MF_UNKNOWN_FUNCTION_ERROR, errorCode);

    errorCode = U_ZERO_ERROR;
    mfBuilder.setFunctionRegistry(functionRegistry).setLocale(locale);

    mf = mfBuilder.setPattern("Hello {$name :person formality=informal}", parseError, errorCode)
                    .build(errorCode);
    result = mf.formatToString(arguments, errorCode);
    assertEquals("testAPICustomFunctions", "Hello John", result);

    mf = mfBuilder.setPattern("Hello {$name :person formality=formal}", parseError, errorCode)
                    .build(errorCode);
    result = mf.formatToString(arguments, errorCode);
    assertEquals("testAPICustomFunctions", "Hello Mr. Doe", result);

    mf = mfBuilder.setPattern("Hello {$name :person formality=formal length=long}", parseError, errorCode)
                    .build(errorCode);
    result = mf.formatToString(arguments, errorCode);
    assertEquals("testAPICustomFunctions", "Hello Mr. John Doe", result);

    // By type
    MFFunctionRegistry::Builder builderByType(errorCode);
    FunctionName personFormatterName("person");
    MFFunctionRegistry functionRegistryByType =
        builderByType.adoptFormatter(personFormatterName,
                                   new PersonNameFormatterFactory(),
                                   errorCode)
                     .setDefaultFormatterNameByType("person",
                                                    personFormatterName,
                                                    errorCode)
                     .build();
    mfBuilder.setFunctionRegistry(functionRegistryByType);
    mf = mfBuilder.setPattern("Hello {$name}", parseError, errorCode)
        .setLocale(locale)
        .build(errorCode);
    result = mf.formatToString(arguments, errorCode);
    assertEquals("testAPICustomFunctions", U_ZERO_ERROR, errorCode);
    // Expect "Hello John" because in the custom function we registered,
    // "informal" is the default formality and "length" is the default length
    assertEquals("testAPICustomFunctions", "Hello John", result);
    delete person;
}

void TestMessageFormat2::testValidPatterns(const TestResult* patterns, int32_t len, IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    TestCase::Builder testBuilder;
    testBuilder.setName("testOtherJsonPatterns");

    for (int32_t i = 0; i < len - 1; i++) {
        TestUtils::runTestCase(*this, testBuilder.setPattern(patterns[i].pattern)
                               .setExpected(patterns[i].output)
                               .setExpectSuccess()
                               .build(), errorCode);
    }
}

void TestMessageFormat2::testResolutionErrors(IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    TestCase::Builder testBuilder;
    testBuilder.setName("testResolutionErrorPattern");

    for (int32_t i = 0; i < numResolutionErrors - 1; i++) {
        TestUtils::runTestCase(*this, testBuilder.setPattern(jsonTestCasesResolutionError[i].pattern)
                          .setExpected(jsonTestCasesResolutionError[i].output)
                          .setExpectedError(jsonTestCasesResolutionError[i].expected)
                          .build(), errorCode);
    }
}

void TestMessageFormat2::testNoSyntaxErrors(const UnicodeString* patterns, int32_t len, IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    TestCase::Builder testBuilder;
    testBuilder.setName("testNoSyntaxErrors");

    for (int32_t i = 0; i < len - 1; i++) {
        TestUtils::runTestCase(*this, testBuilder.setPattern(patterns[i])
                          .setNoSyntaxError()
                          .build(), errorCode);
    }
}

void TestMessageFormat2::testVariousPatterns() {
    IcuTestErrorCode errorCode(*this, "jsonTests");

    jsonTests(errorCode);
    testValidPatterns(validTestCases, numValidTestCases, errorCode);
    testResolutionErrors(errorCode);
    testNoSyntaxErrors(reservedErrors, numReservedErrors, errorCode);
    testNoSyntaxErrors(matches, numMatches, errorCode);
    testNoSyntaxErrors(syntaxTests, numSyntaxTests, errorCode);
}

void TestMessageFormat2::specTests() {
    IcuTestErrorCode errorCode(*this, "specTests");

    runSpecTests(errorCode);
}

/*
 Tests a single pattern, which is expected to be invalid.

 `testNum`: Test number (only used for diagnostic output)
 `s`: The pattern string.

 The error is assumed to be on line 0, offset `s.length()`.
*/
void TestMessageFormat2::testInvalidPattern(uint32_t testNum, const UnicodeString& s) {
    testInvalidPattern(testNum, s, s.length(), 0);
}

/*
 Tests a single pattern, which is expected to be invalid.

 `testNum`: Test number (only used for diagnostic output)
 `s`: The pattern string.

 The error is assumed to be on line 0, offset `expectedErrorOffset`.
*/
void TestMessageFormat2::testInvalidPattern(uint32_t testNum, const UnicodeString& s, uint32_t expectedErrorOffset) {
    testInvalidPattern(testNum, s, expectedErrorOffset, 0);
}

/*
 Tests a single pattern, which is expected to be invalid.

 `testNum`: Test number (only used for diagnostic output)
 `s`: The pattern string.
 `expectedErrorOffset`: The expected character offset for the parse error.

 The error is assumed to be on line `expectedErrorLine`, offset `expectedErrorOffset`.
*/
void TestMessageFormat2::testInvalidPattern(uint32_t testNum, const UnicodeString& s, uint32_t expectedErrorOffset, uint32_t expectedErrorLine) {
    IcuTestErrorCode errorCode(*this, "testInvalidPattern");
    char testName[50];
    snprintf(testName, sizeof(testName), "testInvalidPattern: %d", testNum);

    TestCase::Builder testBuilder;
    testBuilder.setName("testName");

    TestUtils::runTestCase(*this, testBuilder.setPattern(s)
                           .setExpectedError(U_MF_SYNTAX_ERROR)
                           .setExpectedLineNumberAndOffset(expectedErrorLine, expectedErrorOffset)
                           .build(), errorCode);
}

/*
 Tests a single pattern, which is expected to cause the parser to
 emit a data model error

 `testNum`: Test number (only used for diagnostic output)
 `s`: The pattern string.
 `expectedErrorCode`: the error code expected to be returned by the formatter

  For now, the line and character numbers are not checked
*/
void TestMessageFormat2::testSemanticallyInvalidPattern(uint32_t testNum, const UnicodeString& s, UErrorCode expectedErrorCode) {
    IcuTestErrorCode errorCode(*this, "testInvalidPattern");

    char testName[50];
    snprintf(testName, sizeof(testName), "testSemanticallyInvalidPattern: %d", testNum);

    TestCase::Builder testBuilder;
    testBuilder.setName("testName").setPattern(s);
    testBuilder.setExpectedError(expectedErrorCode);

    TestUtils::runTestCase(*this, testBuilder.build(), errorCode);
}

/*
 Tests a single pattern, which is expected to cause the formatter
 to emit a resolution error, selection error, or
 formatting error

 `testNum`: Test number (only used for diagnostic output)
 `s`: The pattern string.
 `expectedErrorCode`: the error code expected to be returned by the formatter

 For now, the line and character numbers are not checked
*/
void TestMessageFormat2::testRuntimeErrorPattern(uint32_t testNum, const UnicodeString& s, UErrorCode expectedErrorCode) {
    IcuTestErrorCode errorCode(*this, "testInvalidPattern");
    char testName[50];
    snprintf(testName, sizeof(testName), "testInvalidPattern (errors): %u", testNum);

    TestCase::Builder testBuilder;
    TestUtils::runTestCase(*this, testBuilder.setName(testName)
                           .setPattern(s)
                           .setExpectedError(expectedErrorCode)
                           .build(), errorCode);
}

/*
 Tests a single pattern, which is expected to cause the formatter
 to emit a resolution error, selection error, or
 formatting error

 `testNum`: Test number (only used for diagnostic output)
 `s`: The pattern string.
 `expectedErrorCode`: the error code expected to be returned by the formatter

 For now, the line and character numbers are not checked
*/
void TestMessageFormat2::testRuntimeWarningPattern(uint32_t testNum, const UnicodeString& s, const UnicodeString& expectedResult, UErrorCode expectedErrorCode) {
    IcuTestErrorCode errorCode(*this, "testInvalidPattern");
    char testName[50];
    snprintf(testName, sizeof(testName), "testInvalidPattern (warnings): %u", testNum);

    TestCase::Builder testBuilder;
    TestUtils::runTestCase(*this, testBuilder.setName(testName)
                                .setPattern(s)
                                .setExpected(expectedResult)
                                .setExpectedError(expectedErrorCode)
                                .build(), errorCode);
}

void TestMessageFormat2::testDataModelErrors() {
    uint32_t i = 0;
    IcuTestErrorCode errorCode(*this, "testDataModelErrors");

    // The following tests are syntactically valid but should trigger a data model error

    // Examples taken from https://github.com/unicode-org/message-format-wg/blob/main/spec/formatting.md

    // Variant key mismatch
    testSemanticallyInvalidPattern(++i, ".match {$foo :number} {$bar :number}  one{{one}}", U_MF_VARIANT_KEY_MISMATCH_ERROR);
    testSemanticallyInvalidPattern(++i, ".match {$foo :number} {$bar :number}  one {{one}}", U_MF_VARIANT_KEY_MISMATCH_ERROR);
    testSemanticallyInvalidPattern(++i, ".match {$foo :number} {$bar :number}  one  {{one}}", U_MF_VARIANT_KEY_MISMATCH_ERROR);

    testSemanticallyInvalidPattern(++i, ".match {$foo :number}  * * {{foo}}", U_MF_VARIANT_KEY_MISMATCH_ERROR);
    testSemanticallyInvalidPattern(++i, ".match {$one :number}\n\
                              1 2 {{Too many}}\n\
                              * {{Otherwise}}", U_MF_VARIANT_KEY_MISMATCH_ERROR);
    testSemanticallyInvalidPattern(++i, ".match {$one :number} {$two :number}\n\
                              1 2 {{Two keys}}\n\
                              * {{Missing a key}}\n\
                              * * {{Otherwise}}", U_MF_VARIANT_KEY_MISMATCH_ERROR);
    testSemanticallyInvalidPattern(++i, ".match {$foo :x} {$bar :x} * {{foo}}", U_MF_VARIANT_KEY_MISMATCH_ERROR);

    // Non-exhaustive patterns
    testSemanticallyInvalidPattern(++i, ".match {$one :number}\n\
                                          1 {{Value is one}}\n\
                                          2 {{Value is two}}", U_MF_NONEXHAUSTIVE_PATTERN_ERROR);
    testSemanticallyInvalidPattern(++i, ".match {$one :number} {$two :number}\n\
                                          1 * {{First is one}}\n\
                                          * 1 {{Second is one}}", U_MF_NONEXHAUSTIVE_PATTERN_ERROR);
    testSemanticallyInvalidPattern(++i, ".match {:foo} 1 {{_}}", U_MF_NONEXHAUSTIVE_PATTERN_ERROR);
    testSemanticallyInvalidPattern(++i, ".match {:foo} other {{_}}", U_MF_NONEXHAUSTIVE_PATTERN_ERROR);

    // Duplicate option names
    testSemanticallyInvalidPattern(++i, "{:foo a=1 b=2 a=1}", U_MF_DUPLICATE_OPTION_NAME_ERROR);
    testSemanticallyInvalidPattern(++i, "{:foo a=1 a=1}", U_MF_DUPLICATE_OPTION_NAME_ERROR);
    testSemanticallyInvalidPattern(++i, "{:foo a=1 a=2}", U_MF_DUPLICATE_OPTION_NAME_ERROR);
    testSemanticallyInvalidPattern(++i, "{|x| :foo a=1 a=2}", U_MF_DUPLICATE_OPTION_NAME_ERROR);
    testSemanticallyInvalidPattern(++i, "bad {:placeholder option=x option=x}", U_MF_DUPLICATE_OPTION_NAME_ERROR);
    testSemanticallyInvalidPattern(++i, "bad {:placeholder ns:option=x ns:option=y}", U_MF_DUPLICATE_OPTION_NAME_ERROR);

    // Missing selector annotation
    testSemanticallyInvalidPattern(++i, ".match {$one}\n\
                                          1 {{Value is one}}\n\
                                          * {{Value is not one}}", U_MF_MISSING_SELECTOR_ANNOTATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".local $one = {|The one|}\n\
                                         .match {$one}\n\
                                          1 {{Value is one}}\n\
                                          * {{Value is not one}}", U_MF_MISSING_SELECTOR_ANNOTATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".match {|horse| ^private}\n\
                                          1 {{The value is one.}}\n          \
                                          * {{The value is not one.}}", U_MF_MISSING_SELECTOR_ANNOTATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".match {$foo !select}  |1| {{one}}  * {{other}}",
                                   U_MF_MISSING_SELECTOR_ANNOTATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".match {$foo ^select}  |1| {{one}}  * {{other}}",
                                   U_MF_MISSING_SELECTOR_ANNOTATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".input {$foo} .match {$foo} one {{one}} * {{other}}", U_MF_MISSING_SELECTOR_ANNOTATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".local $foo = {$bar} .match {$foo} one {{one}} * {{other}}", U_MF_MISSING_SELECTOR_ANNOTATION_ERROR);

    // Duplicate declaration errors
    testSemanticallyInvalidPattern(++i, ".local $x = {|1|} .input {$x :number} {{{$x}}}",
                                   U_MF_DUPLICATE_DECLARATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".input {$x :number} .input {$x :string} {{{$x}}}",
                                   U_MF_DUPLICATE_DECLARATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".input {$foo} .input {$foo} {{_}}", U_MF_DUPLICATE_DECLARATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".input {$foo} .local $foo = {42} {{_}}", U_MF_DUPLICATE_DECLARATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".local $foo = {42} .input {$foo} {{_}}", U_MF_DUPLICATE_DECLARATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".local $foo = {:unknown} .local $foo = {42} {{_}}", U_MF_DUPLICATE_DECLARATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".local $foo = {$bar} .local $bar = {42} {{_}}", U_MF_DUPLICATE_DECLARATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".local $foo = {$foo} {{_}}", U_MF_DUPLICATE_DECLARATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".local $foo = {$bar} .local $bar = {$baz} {{_}}", U_MF_DUPLICATE_DECLARATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".local $foo = {$bar :func} .local $bar = {$baz} {{_}}", U_MF_DUPLICATE_DECLARATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".local $foo = {42 :func opt=$foo} {{_}}", U_MF_DUPLICATE_DECLARATION_ERROR);
    testSemanticallyInvalidPattern(++i, ".local $foo = {42 :func opt=$bar} .local $bar = {42} {{_}}", U_MF_DUPLICATE_DECLARATION_ERROR);

    // Disambiguating unsupported statements from match
    testSemanticallyInvalidPattern(++i, ".matc {-1} {{hello}}", U_MF_UNSUPPORTED_STATEMENT_ERROR);
    testSemanticallyInvalidPattern(++i, ".m {-1} {{hello}}", U_MF_UNSUPPORTED_STATEMENT_ERROR);

    TestCase::Builder testBuilder;
    testBuilder.setName("testDataModelErrors");

    // This should *not* trigger a "missing selector annotation" error
    TestCase test = testBuilder.setPattern(".local $one = {|The one| :string}\n\
                 .match {$one}\n\
                  1 {{Value is one}}\n\
                  * {{Value is not one}}")
                          .setExpected("Value is not one")
                          .setExpectSuccess()
                          .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern(".local $one = {|The one| :string}\n\
                 .local $two = {$one}\n\
                 .match {$two}\n\
                  1 {{Value is one}}\n\
                  * {{Value is not one}}")
                          .setExpected("Value is not one")
                          .setExpectSuccess()
                          .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

void TestMessageFormat2::testResolutionErrors() {
    uint32_t i = 0;

    // The following tests are syntactically valid and free of data model errors,
    // but should trigger a resolution error

    // Unresolved variable
    testRuntimeWarningPattern(++i, "{$oops}", "{$oops}", U_MF_UNRESOLVED_VARIABLE_ERROR);
    // .input of $x but $x is not supplied as an argument -- also unresolved variable
    testRuntimeWarningPattern(++i, ".input {$x :number} {{{$x}}}", "{$x}", U_MF_UNRESOLVED_VARIABLE_ERROR);

    // Unknown function
    testRuntimeWarningPattern(++i, "The value is {horse :func}.", "The value is {|horse|}.", U_MF_UNKNOWN_FUNCTION_ERROR);
    testRuntimeWarningPattern(++i, ".match {|horse| :func}\n\
                                          1 {{The value is one.}}\n\
                                          * {{The value is not one.}}",
                              "The value is not one.", U_MF_UNKNOWN_FUNCTION_ERROR);
    // Using formatter as selector
    // The fallback string will match the '*' variant
    testRuntimeWarningPattern(++i, ".match {|horse| :number}\n\
                                          1 {{The value is one.}}\n\
                                          * {{The value is not one.}}", "The value is not one.", U_MF_SELECTOR_ERROR);

    // Using selector as formatter
    testRuntimeWarningPattern(++i, ".match {|horse| :string}\n\
                                          1 {{The value is one.}}\n   \
                                          * {{{|horse| :string}}}",
                              "{|horse|}", U_MF_FORMATTING_ERROR);

    // Unsupported expressions
    testRuntimeErrorPattern(++i, "hello {|4.2| !number}", U_MF_UNSUPPORTED_EXPRESSION_ERROR);
    testRuntimeErrorPattern(++i, "{<tag}", U_MF_UNSUPPORTED_EXPRESSION_ERROR);
    testRuntimeErrorPattern(++i, ".local $bar = {|42| ~plural} .match {|horse| :string}  * {{{$bar}}}",
                            U_MF_UNSUPPORTED_EXPRESSION_ERROR);

    // Selector error
    // Here, the plural selector returns "no match" so the * variant matches
    testRuntimeWarningPattern(++i, ".match {|horse| :number}\n\
                                   1 {{The value is one.}}\n\
                                   * {{The value is not one.}}", "The value is not one.", U_MF_SELECTOR_ERROR);
    testRuntimeWarningPattern(++i, ".local $sel = {|horse| :number}\n\
                                  .match {$sel}\n\
                                   1 {{The value is one.}}\n\
                                   * {{The value is not one.}}", "The value is not one.", U_MF_SELECTOR_ERROR);
}

void TestMessageFormat2::testInvalidPatterns() {
/*
  These tests are mostly from the test suite created for the JavaScript implementation of MessageFormat v2:
  <p>Original JSON file
  <a href="https://github.com/messageformat/messageformat/blob/master/packages/mf2-messageformat/src/__fixtures/test-messages.json">here</a>.</p>
  Some have been modified or added to reflect syntax changes that post-date the JSON file.

 */
    uint32_t i = 0;

    // Unexpected end of input
    testInvalidPattern(++i, ".local    ");
    testInvalidPattern(++i, ".lo");
    testInvalidPattern(++i, ".local $foo");
    testInvalidPattern(++i, ".local $foo =    ");
    testInvalidPattern(++i, "{:fszzz");
    testInvalidPattern(++i, "{:fszzz   ");
    testInvalidPattern(++i, ".match {$foo}  |xyz");
    testInvalidPattern(++i, "{:f aaa");
    testInvalidPattern(++i, "{{missing end brace");
    testInvalidPattern(++i, "{{missing end brace}");
    testInvalidPattern(++i, "{{missing end {$brace");
    testInvalidPattern(++i, "{{missing end {$brace}");
    testInvalidPattern(++i, "{{missing end {$brace}}");

    // Error should be reported at character 0, not end of input
    testInvalidPattern(++i, "}{|xyz|", 0);
    testInvalidPattern(++i, "}", 0);

    // %xyz is a valid annotation (`reserved`) so the error should be at the end of input
    testInvalidPattern(++i, "{{%xyz");
    // Backslash followed by non-backslash followed by a '{' -- this should be an error
    // immediately after the first backslash
    testInvalidPattern(++i, "{{{%\\y{}}", 5);

    // Reserved chars followed by a '|' that doesn't begin a valid literal -- this should be
    // an error at the first invalid char in the literal
    testInvalidPattern(++i, "{%abc|\\z}}", 7);

    // Same pattern, but with a valid reserved-char following the erroneous reserved-escape
    // -- the offset should be the same as with the previous one
    testInvalidPattern(++i, "{%\\y{p}}", 3);
    // Erroneous literal inside a reserved string -- the error should be at the first
    // erroneous literal char
    testInvalidPattern(++i, "{{{%ab|\\z|cd}}", 8);

    // tests for reserved syntax with bad escaped chars
    // Single backslash - not allowed
    testInvalidPattern(++i, "hello {|4.2| %num\\ber}}", 18);
    // Unescaped '{' -- not allowed
    testInvalidPattern(++i, "hello {|4.2| %num{be\\|r}}", 17);
    // Unescaped '}' -- will be interpreted as the end of the reserved
    // string, and the error will be reported at the index of '|', which is
    // when the parser determines that "\|" isn't a valid text-escape
    testInvalidPattern(++i, "hello {|4.2| %num}be\\|r}}", 21);
    // Unescaped '|' -- will be interpreted as the beginning of a literal
    // Error at end of input
    testInvalidPattern(++i, "hello {|4.2| %num\\{be|r}}", 25);

    // Invalid escape sequence in a `text` -- the error should be at the character
    // following the backslash
    testInvalidPattern(++i, "a\\qbc", 2);

    // No spaces are required here. The error should be
    // in the pattern, not before
    testInvalidPattern(++i, ".match{|y|}|y|{{{|||}}}", 19);

    // Missing spaces betwen keys
    testInvalidPattern(++i, ".match {|y|}|foo|bar {{{a}}}", 17);
    testInvalidPattern(++i, ".match {|y|} |quux| |foo|bar {{{a}}}", 25);
    testInvalidPattern(++i, ".match {|y|}  |quux| |foo||bar| {{{a}}}", 26);

    // Error parsing the first key -- the error should be there, not in the
    // also-erroneous third key
    testInvalidPattern(++i, ".match {|y|}  |\\q| * %{! {z}", 16);

    // Error parsing the second key -- the error should be there, not in the
    // also-erroneous third key
    testInvalidPattern(++i, ".match {|y|}  * %{! {z} |\\q|", 16);

    // Error parsing the last key -- the error should be there, not in the erroneous
    // pattern
    testInvalidPattern(++i, ".match {|y|}  * |\\q| {\\z}", 18);

    // Non-expression as scrutinee in pattern -- error should be at the first
    // non-expression, not the later non-expression
    testInvalidPattern(++i, ".match {|y|} {\\|} {@}  * * * {{a}}", 14);

    // Non-key in variant -- error should be there, not in the next erroneous
    // variant
    testInvalidPattern(++i, ".match {|y|}  $foo * {{a}} when * :bar {{b}}", 14);


    // Error should be within the first erroneous `text` or expression
    testInvalidPattern(++i, "{{ foo {|bar|} \\q baz  ", 16);

    // ':' has to be followed by a function name -- the error should be at the first
    // whitespace character
    testInvalidPattern(++i, "{{{:    }}}", 4);

    // Expression not starting with a '{'
    testInvalidPattern(++i, ".local $x = }|foo|}", 12);

    // Error should be at the first declaration not starting with a `.local`
    testInvalidPattern(++i, ".local $x = {|foo|} .l $y = {|bar|} .local $z {|quux|}", 22);

    // Missing '=' in `.local` declaration
    testInvalidPattern(++i, ".local $bar {|foo|} {{$bar}}", 12);

    // LHS of declaration doesn't start with a '$'
    testInvalidPattern(++i, ".local bar = {|foo|} {{$bar}}", 7);

    // `.local` RHS isn't an expression
    testInvalidPattern(++i, ".local $bar = |foo| {{$bar}}", 14);

    // Trailing characters that are not whitespace
    testInvalidPattern(++i, "{{extra}}content", 9);
    testInvalidPattern(++i, ".match {|x|}  * {{foo}}extra", 28);

    // Trailing whitespace at end of message should not be accepted either
    UnicodeString longMsg(".match {$foo :string} {$bar :string}  one * {{one}}  * * {{other}}   ");
    testInvalidPattern(++i, longMsg, longMsg.length() - 3);
    testInvalidPattern(++i, "{{hi}} ", 6);

    // Empty expression
    testInvalidPattern(++i, "empty { }", 8);
    testInvalidPattern(++i, ".match {}  * {{foo}}", 8);

    // ':' not preceding a function name
    testInvalidPattern(++i, "bad {:}", 6);

    // Missing '=' after option name
    testInvalidPattern(++i, "{{no-equal {|42| :number m }}}", 27);
    testInvalidPattern(++i, "{{no-equal {|42| :number minimumFractionDigits 2}}}", 47);
    testInvalidPattern(++i, "bad {:placeholder option value}", 25);

    // Extra '=' after option value
    testInvalidPattern(++i, "hello {|4.2| :number min=2=3}", 26),
    testInvalidPattern(++i, "hello {|4.2| :number min=2max=3}", 26),
    // Missing whitespace between valid options
    testInvalidPattern(++i, "hello {|4.2| :number min=|a|max=3}", 28),
    // Ill-formed RHS of option -- the error should be within the RHS,
    // not after parsing options
    testInvalidPattern(++i, "hello {|4.2| :number min=|\\a|}", 27),


    // Junk after annotation
    testInvalidPattern(++i, "no-equal {|42| :number   {}", 25);

    // Missing RHS of option
    testInvalidPattern(++i, "bad {:placeholder option=}", 25);
    testInvalidPattern(++i, "bad {:placeholder option}", 24);

    // Annotation is not a function or reserved text
    testInvalidPattern(++i, "bad {$placeholder option}", 18);
    testInvalidPattern(++i, "no {$placeholder end", 17);

    // Missing expression in selectors
    testInvalidPattern(++i, ".match  * {{foo}}", 8);
    // Non-expression in selectors
    testInvalidPattern(++i, ".match |x|  * {{foo}}", 7);

    // Missing RHS in variant
    testInvalidPattern(++i, ".match {|x|}  * foo");

    // Text may include newlines; check that the missing closing '}' is
    // reported on the correct line
    testInvalidPattern(++i, "{{hello wo\nrld", 3, 1);
    testInvalidPattern(++i, "{{hello wo\nr\nl\ndddd", 4, 3);
    // Offset for end-of-input should be 0 here because the line begins
    // after the '\n', but there is no character after the '\n'
    testInvalidPattern(++i, "{{hello wo\nr\nl\n", 0, 3);

    // Quoted literals may include newlines; check that the missing closing '|' is
    // reported on the correct line
    testInvalidPattern(++i, "hello {|wo\nrld", 3, 1);
    testInvalidPattern(++i, "hello {|wo\nr\nl\ndddd", 4, 3);
    // Offset for end-of-input should be 0 here because the line begins
    // after the '\n', but there is no character after the '\n'
    testInvalidPattern(++i, "hello {|wo\nr\nl\n", 0, 3);

    // Variable names can't start with a : or -
    testInvalidPattern(++i, "{$:abc}", 2);
    testInvalidPattern(++i, "{$-abc}", 2);

    // Missing space before annotation
    // Note that {{$bar:foo}} and {{$bar-foo}} are valid,
    // because variable names can contain a ':' or a '-'
    testInvalidPattern(++i, "{$bar+foo}", 5);
    testInvalidPattern(++i, "{|3.14|:foo}", 7);
    testInvalidPattern(++i, "{|3.14|-foo}", 7);
    testInvalidPattern(++i, "{|3.14|+foo}", 7);

    // Unquoted literals can't begin with a ':'
    testInvalidPattern(++i, ".local $foo = {$bar} .match {$foo}  :one {one} * {other}", 36);
    testInvalidPattern(++i, ".local $foo = {$bar :fun option=:a} {{bar {$foo}}}", 32);

    // Markup in wrong place
    testInvalidPattern(++i, "{|foo| {#markup}}", 7);
    testInvalidPattern(++i, "{|foo| #markup}", 7);
    testInvalidPattern(++i, "{|foo| {#markup/}}", 7);
    testInvalidPattern(++i, "{|foo| {/markup}}", 7);

    // .input with non-variable-expression
    testInvalidPattern(++i, ".input $x = {|1|} {{{$x}}}", 7);
    testInvalidPattern(++i, ".input $x = {:number} {{{$x}}}", 7);
    testInvalidPattern(++i, ".input {|1| :number} {{{$x}}}", 7);
    testInvalidPattern(++i, ".input {:number} {{{$x}}}", 7);
    testInvalidPattern(++i, ".input {|1|} {{{$x}}}", 7);

    // invalid number literals
    testInvalidPattern(++i, "{00}", 2);
    testInvalidPattern(++i, "{042}", 2);
    testInvalidPattern(++i, "{1.}", 3);
    testInvalidPattern(++i, "{1e}", 3);
    testInvalidPattern(++i, "{1E}", 3);
    testInvalidPattern(++i, "{1.e}", 3);
    testInvalidPattern(++i, "{1.2e}", 5);
    testInvalidPattern(++i, "{1.e3}", 3);
    testInvalidPattern(++i, "{1e+}", 4);
    testInvalidPattern(++i, "{1e-}", 4);
    testInvalidPattern(++i, "{1.0e2.0}", 6);

    // The following are from https://github.com/unicode-org/message-format-wg/blob/main/test/syntax-errors.json
    testInvalidPattern(++i,".", 1);
    testInvalidPattern(++i, "{", 1);
    testInvalidPattern(++i, "}", 0);
    testInvalidPattern(++i, "{}", 1);
    testInvalidPattern(++i, "{{", 2);
    testInvalidPattern(++i, "{{}", 3);
    testInvalidPattern(++i, "{{}}}", 4);
    testInvalidPattern(++i, "{|foo| #markup}", 7);
    testInvalidPattern(++i, "{{missing end brace}", 20);
    testInvalidPattern(++i, "{{missing end braces", 20);
    testInvalidPattern(++i, "{{missing end {$braces", 22);
    testInvalidPattern(++i, "{{extra}} content", 9);
    testInvalidPattern(++i, "empty { } placeholder", 8);
    testInvalidPattern(++i, "missing space {42:func}", 17);
    testInvalidPattern(++i, "missing space {|foo|:func}", 20);
    testInvalidPattern(++i, "missing space {|foo|@bar}", 20);
    testInvalidPattern(++i, "missing space {:func@bar}", 20);
    testInvalidPattern(++i, "{:func @bar@baz}", 11);
    testInvalidPattern(++i, "{:func @bar=42@baz}", 14);
    testInvalidPattern(++i, "{+reserved@bar}", 10);
    testInvalidPattern(++i, "{&private@bar}", 9);
    testInvalidPattern(++i, "bad {:} placeholder", 6);
    testInvalidPattern(++i, "bad {\\u0000placeholder}", 5);
    testInvalidPattern(++i, "no-equal {|42| :number minimumFractionDigits 2}", 45);
    testInvalidPattern(++i, "bad {:placeholder option=}", 25);
    testInvalidPattern(++i, "bad {:placeholder option value}", 25);
    testInvalidPattern(++i, "bad {:placeholder option:value}", 30);
    testInvalidPattern(++i, "bad {:placeholder option}", 24);
    testInvalidPattern(++i, "bad {:placeholder:}", 18);
    testInvalidPattern(++i, "bad {::placeholder}", 6);
    testInvalidPattern(++i, "bad {:placeholder::foo}", 18);
    testInvalidPattern(++i, "bad {:placeholder option:=x}", 25);
    testInvalidPattern(++i, "bad {:placeholder :option=x}", 18);
    testInvalidPattern(++i, "bad {:placeholder option::x=y}", 25);
    testInvalidPattern(++i, "bad {$placeholder option}", 18);
    testInvalidPattern(++i, "bad {:placeholder @attribute=}", 29);
    testInvalidPattern(++i, "bad {:placeholder @attribute=@foo}", 29);
    testInvalidPattern(++i, "no {placeholder end", 16);
    testInvalidPattern(++i, "no {$placeholder end", 17);
    testInvalidPattern(++i, "no {:placeholder end", 20);
    testInvalidPattern(++i, "no {|placeholder| end", 18);
    testInvalidPattern(++i, "no {|literal} end", 17);
    testInvalidPattern(++i, "no {|literal or placeholder end", 31);
    testInvalidPattern(++i, ".local bar = {|foo|} {{_}}", 7);
    testInvalidPattern(++i, ".local #bar = {|foo|} {{_}}", 7);
    testInvalidPattern(++i, ".local $bar {|foo|} {{_}}", 12);
    testInvalidPattern(++i, ".local $bar = |foo| {{_}}", 14);
    testInvalidPattern(++i, ".match {#foo} * {{foo}}", 8);
    testInvalidPattern(++i, ".match {} * {{foo}}", 8);
    testInvalidPattern(++i, ".match {|foo| :x} {|bar| :x} ** {{foo}}", 30);
    testInvalidPattern(++i, ".match * {{foo}}", 7);
    testInvalidPattern(++i, ".match {|x| :x} * foo", 21);
    testInvalidPattern(++i, ".match {|x| :x} * {{foo}} extra", 31);
    testInvalidPattern(++i, ".match |x| * {{foo}}", 7);

    // tests for ':' in unquoted literals (not allowed)
    testInvalidPattern(++i, ".match {|foo| :string} o:ne {{one}}  * {{other}}", 24);
    testInvalidPattern(++i, ".match {|foo| :string} one: {{one}}  * {{other}}", 26);
    testInvalidPattern(++i, ".local $foo = {|42| :number option=a:b} {{bar {$foo}}}", 36);
    testInvalidPattern(++i, ".local $foo = {|42| :number option=a:b:c} {{bar {$foo}}}", 36);
    testInvalidPattern(++i, "{$bar:foo}", 5);

    // Disambiguating a wrong .match from an unsupported statement
    testInvalidPattern(++i, ".match {1} {{_}}", 12);
}

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

