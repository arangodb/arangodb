// © 2024 and later: Unicode, Inc. and others.

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "unicode/gregocal.h"
#include "messageformat2test.h"

using namespace icu::message2;
using namespace data_model;

/*
  Tests based on ICU4J's MessageFormat2Test.java
and Mf2FeaturesTest.java
*/

/*
  TODO: Tests need to be unified in a single format that
  both ICU4C and ICU4J can use, rather than being embedded in code.
*/

/*
Tests reflect the syntax specified in

  https://github.com/unicode-org/message-format-wg/commits/main/spec/message.abnf

as of the following commit from 2023-05-09:
  https://github.com/unicode-org/message-format-wg/commit/194f6efcec5bf396df36a19bd6fa78d1fa2e0867

*/

void TestMessageFormat2::testEmptyMessage(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    TestUtils::runTestCase(*this, testBuilder.setPattern("")
                           .setExpected("")
                           .build(), errorCode);
}

void TestMessageFormat2::testPlainText(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    TestUtils::runTestCase(*this, testBuilder.setPattern("Hello World!")
                           .setExpected("Hello World!")
                           .build(), errorCode);
}

void TestMessageFormat2::testPlaceholders(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    TestUtils::runTestCase(*this, testBuilder.setPattern("Hello, {$userName}!")
                                .setExpected("Hello, John!")
                                .setArgument("userName", "John")
                                .build(), errorCode);
}

void TestMessageFormat2::testArgumentMissing(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    UnicodeString message = "Hello {$name}, today is {$today :date style=long}.";
    LocalPointer<Calendar> cal(Calendar::createInstance(errorCode));
    CHECK_ERROR(errorCode);

    // November 23, 2022 at 7:42:37.123 PM
    cal->set(2022, Calendar::NOVEMBER, 23, 19, 42, 37);
    UDate TEST_DATE = cal->getTime(errorCode);
    CHECK_ERROR(errorCode);

    TestCase test = testBuilder.setPattern(message)
        .clearArguments()
        .setArgument("name", "John")
        .setDateArgument("today", TEST_DATE)
        .setExpected("Hello John, today is November 23, 2022.")
        .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Missing date argument
    test = testBuilder.setPattern(message)
                                .clearArguments()
                                .setArgument("name", "John")
                                .setExpected("Hello John, today is {$today}.")
                                .setExpectedError(U_MF_UNRESOLVED_VARIABLE_ERROR)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setPattern(message)
                                .clearArguments()
                                .setDateArgument("today", TEST_DATE)
                                .setExpectedError(U_MF_UNRESOLVED_VARIABLE_ERROR)
                                .setExpected("Hello {$name}, today is November 23, 2022.")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Both arguments missing
    test = testBuilder.setPattern(message)
                                .clearArguments()
                                .setExpectedError(U_MF_UNRESOLVED_VARIABLE_ERROR)
                                .setExpected("Hello {$name}, today is {$today}.")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

void TestMessageFormat2::testDefaultLocale(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    LocalPointer<Calendar> cal(Calendar::createInstance(errorCode));
    CHECK_ERROR(errorCode);
    // November 23, 2022 at 7:42:37.123 PM
    cal->set(2022, Calendar::NOVEMBER, 23, 19, 42, 37);
    UDate TEST_DATE = cal->getTime(errorCode);
    CHECK_ERROR(errorCode);

    UnicodeString message = "Date: {$date :date style=long}.";
    UnicodeString expectedEn = "Date: November 23, 2022.";
    UnicodeString expectedRo = "Date: 23 noiembrie 2022.";

    testBuilder.setPattern(message);

    TestCase test = testBuilder.clearArguments()
        .setDateArgument("date", TEST_DATE)
        .setExpected(expectedEn)
        .setExpectSuccess()
        .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setExpected(expectedRo)
                                .setLocale(Locale("ro"))
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    Locale originalLocale = Locale::getDefault();
    Locale::setDefault(Locale::forLanguageTag("ro", errorCode), errorCode);
    CHECK_ERROR(errorCode);

    test = testBuilder.setExpected(expectedEn)
                                .setLocale(Locale("en", "US"))
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setExpected(expectedRo)
                                .setLocale(Locale::forLanguageTag("ro", errorCode))
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    Locale::setDefault(originalLocale, errorCode);
    CHECK_ERROR(errorCode);
}

void TestMessageFormat2::testSpecialPluralWithDecimals(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    UnicodeString message;

    message = ".local $amount = {$count :number}\n\
                .match {$amount :number}\n\
                  1 {{I have {$amount} dollar.}}\n\
                  * {{I have {$amount} dollars.}}";

    TestCase test = testBuilder.setPattern(message)
        .clearArguments()
        .setArgument("count", (int64_t) 1)
        .setExpected("I have 1 dollar.")
        .setLocale(Locale("en", "US"))
        .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

void TestMessageFormat2::testDefaultFunctionAndOptions(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    LocalPointer<Calendar> cal(Calendar::createInstance(errorCode));
    CHECK_ERROR(errorCode);
    // November 23, 2022 at 7:42:37.123 PM
    cal->set(2022, Calendar::NOVEMBER, 23, 19, 42, 37);
    UDate TEST_DATE = cal->getTime(errorCode);
    CHECK_ERROR(errorCode);

    TestCase test = testBuilder.setPattern("Testing date formatting: {$date}.")
        .clearArguments()
        .setDateArgument("date", TEST_DATE)
        .setExpected("Testing date formatting: 23.11.2022, 19:42.")
        .setLocale(Locale("ro"))
        .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setPattern("Testing date formatting: {$date :datetime}.")
                                .setExpected("Testing date formatting: 23.11.2022, 19:42.")
                                .setLocale(Locale("ro"))
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

void TestMessageFormat2::testSimpleSelection(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    (void) testBuilder;
    (void) errorCode;

    /* Covered by testPlural */
}

void TestMessageFormat2::testComplexSelection(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    UnicodeString message = ".match {$photoCount :number} {$userGender :string}\n\
                  1 masculine {{{$userName} added a new photo to his album.}}\n\
                  1 feminine {{{$userName} added a new photo to her album.}}\n\
                  1 * {{{$userName} added a new photo to their album.}}\n\
                  * masculine {{{$userName} added {$photoCount} photos to his album.}}\n\
                  * feminine {{{$userName} added {$photoCount} photos to her album.}}\n\
                  * * {{{$userName} added {$photoCount} photos to their album.}}";
    testBuilder.setPattern(message);

    int64_t count = 1;
    TestCase test = testBuilder.clearArguments().setArgument("photoCount", count)
        .setArgument("userGender", "masculine")
        .setArgument("userName", "John")
        .setExpected("John added a new photo to his album.")
        .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setArgument("userGender", "feminine")
                      .setArgument("userName", "Anna")
                      .setExpected("Anna added a new photo to her album.")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setArgument("userGender", "unknown")
                      .setArgument("userName", "Anonymous")
                      .setExpected("Anonymous added a new photo to their album.")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    count = 13;
    test = testBuilder.clearArguments().setArgument("photoCount", count)
                                .setArgument("userGender", "masculine")
                                .setArgument("userName", "John")
                                .setExpected("John added 13 photos to his album.")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setArgument("userGender", "feminine")
                      .setArgument("userName", "Anna")
                      .setExpected("Anna added 13 photos to her album.")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setArgument("userGender", "unknown")
                      .setArgument("userName", "Anonymous")
                      .setExpected("Anonymous added 13 photos to their album.")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

void TestMessageFormat2::testSimpleLocalVariable(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    LocalPointer<Calendar> cal(Calendar::createInstance(errorCode));
    CHECK_ERROR(errorCode);
    // November 23, 2022 at 7:42:37.123 PM
    cal->set(2022, Calendar::NOVEMBER, 23, 19, 42, 37);
    UDate TEST_DATE = cal->getTime(errorCode);
    CHECK_ERROR(errorCode);

    testBuilder.setPattern(".input {$expDate :date style=medium}\n\
                            {{Your tickets expire on {$expDate}.}}");

    int64_t count = 1;
    TestUtils::runTestCase(*this, testBuilder.clearArguments().setArgument("count", count)
                      .setLocale(Locale("en"))
                      .setDateArgument("expDate", TEST_DATE)
                      .setExpected("Your tickets expire on Nov 23, 2022.")
                      .build(), errorCode);
}

void TestMessageFormat2::testLocalVariableWithSelect(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    LocalPointer<Calendar> cal(Calendar::createInstance(errorCode));
    CHECK_ERROR(errorCode);
    // November 23, 2022 at 7:42:37.123 PM
    cal->set(2022, Calendar::NOVEMBER, 23, 19, 42, 37);
    UDate TEST_DATE = cal->getTime(errorCode);
    CHECK_ERROR(errorCode);

    testBuilder.setPattern(".input {$expDate :date style=medium}\n\
                .match {$count :number}\n\
                 1 {{Your ticket expires on {$expDate}.}}\n\
                 * {{Your {$count} tickets expire on {$expDate}.}}");

    int64_t count = 1;
    TestCase test = testBuilder.clearArguments().setArgument("count", count)
                      .setLocale(Locale("en"))
                      .setDateArgument("expDate", TEST_DATE)
                      .setExpected("Your ticket expires on Nov 23, 2022.")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);
    count = 3;
    test = testBuilder.setArgument("count", count)
                      .setExpected("Your 3 tickets expire on Nov 23, 2022.")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

void TestMessageFormat2::testDateFormat(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    LocalPointer<Calendar> cal(Calendar::createInstance(errorCode));
    CHECK_ERROR(errorCode);

    cal->set(2022, Calendar::OCTOBER, 27, 0, 0, 0);
    UDate expiration = cal->getTime(errorCode);
    CHECK_ERROR(errorCode);

    TestCase test = testBuilder.clearArguments().setPattern("Your card expires on {$exp :date style=medium}!")
                                .setLocale(Locale("en"))
                                .setExpected("Your card expires on Oct 27, 2022!")
                                .setDateArgument("exp", expiration)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.clearArguments().setPattern("Your card expires on {$exp :date style=full}!")
                      .setExpected("Your card expires on Thursday, October 27, 2022!")
                      .setDateArgument("exp", expiration)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.clearArguments().setPattern("Your card expires on {$exp :date style=long}!")
                      .setExpected("Your card expires on October 27, 2022!")
                      .setDateArgument("exp", expiration)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.clearArguments().setPattern("Your card expires on {$exp :date style=medium}!")
                      .setExpected("Your card expires on Oct 27, 2022!")
                      .setDateArgument("exp", expiration)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.clearArguments().setPattern("Your card expires on {$exp :date style=short}!")
                      .setExpected("Your card expires on 10/27/22!")
                      .setDateArgument("exp", expiration)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

/*
  This test would require the calendar to be passed as a UObject* with the datetime formatter
  doing an RTTI check -- however, that would be awkward, since it would have to check the tag for each
  possible subclass of `Calendar`. datetime currently has no support for formatting any object argument

    cal.adoptInstead(new GregorianCalendar(2022, Calendar::OCTOBER, 27, errorCode));
    if (cal.isValid()) {
        test = testBuilder.setPattern("Your card expires on {$exp :datetime skeleton=yMMMdE}!")
                          .setExpected("Your card expires on Thu, Oct 27, 2022!")
                          .setArgument("exp", cal.orphan(), errorCode)
                          .build();
        TestUtils::runTestCase(*this, test, errorCode);
    }
*/

    // Implied function based on type of the object to format
    test = testBuilder.clearArguments().setPattern("Your card expires on {$exp}!")
                      .setExpected(CharsToUnicodeString("Your card expires on 10/27/22, 12:00\\u202FAM!"))
                      .setDateArgument("exp", expiration)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

void TestMessageFormat2::testPlural(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    UnicodeString message = ".match {$count :number}\n\
                 1 {{You have one notification.}}\n           \
                 * {{You have {$count} notifications.}}";

    int64_t count = 1;
    TestCase test = testBuilder.clearArguments().setPattern(message)
                                .setExpected("You have one notification.")
                                .setArgument("count", count)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    count = 42;
    test = testBuilder.clearArguments().setExpected("You have 42 notifications.")
                      .setArgument("count", count)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    count = 1;
    test = testBuilder.clearArguments().setPattern(message)
                      .setExpected("You have one notification.")
                      .setArgument("count", "1")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    count = 42;
    test = testBuilder.clearArguments().setExpected("You have 42 notifications.")
                      .setArgument("count", "42")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

void TestMessageFormat2::testPluralOrdinal(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    UnicodeString message =  ".match {$place :number select=ordinal}\n\
                 1 {{You got the gold medal}}\n            \
                 2 {{You got the silver medal}}\n          \
                 3 {{You got the bronze medal}}\n\
                 one {{You got in the {$place}st place}}\n\
                 two {{You got in the {$place}nd place}}\n \
                 few {{You got in the {$place}rd place}}\n \
                 * {{You got in the {$place}th place}}";

    TestCase test = testBuilder.clearArguments().setPattern(message)
                                .setExpected("You got the gold medal")
                                .setArgument("place", "1")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.clearArguments().setExpected("You got the silver medal")
                          .setArgument("place", "2")
                          .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.clearArguments().setExpected("You got the bronze medal")
                          .setArgument("place", "3")
                          .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.clearArguments().setExpected("You got in the 21st place")
                          .setArgument("place", "21")
                          .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.clearArguments().setExpected("You got in the 32nd place")
                          .setArgument("place", "32")
                          .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.clearArguments().setExpected("You got in the 23rd place")
                          .setArgument("place", "23")
                          .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.clearArguments().setExpected("You got in the 15th place")
                          .setArgument("place", "15")
                          .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

void TestMessageFormat2::testDeclareBeforeUse(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {

    UnicodeString message = ".local $foo = {$baz :number}\n\
                 .local $bar = {$foo}\n                    \
                 .local $baz = {$bar}\n                    \
                 {{The message uses {$baz} and works}}";
    testBuilder.setPattern(message);
    testBuilder.setName("declare-before-use");

    TestCase test = testBuilder.clearArguments().setExpected("The message uses {$baz} and works")
                                .setExpectedError(U_MF_DUPLICATE_DECLARATION_ERROR)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
}


void TestMessageFormat2::featureTests() {
    IcuTestErrorCode errorCode(*this, "featureTests");

    TestCase::Builder testBuilder;
    testBuilder.setName("featureTests");

    testEmptyMessage(testBuilder, errorCode);
    testPlainText(testBuilder, errorCode);
    testPlaceholders(testBuilder, errorCode);
    testArgumentMissing(testBuilder, errorCode);
    testDefaultLocale(testBuilder, errorCode);
    testSpecialPluralWithDecimals(testBuilder, errorCode);
    testDefaultFunctionAndOptions(testBuilder, errorCode);
    testSimpleSelection(testBuilder, errorCode);
    testComplexSelection(testBuilder, errorCode);
    testSimpleLocalVariable(testBuilder, errorCode);
    testLocalVariableWithSelect(testBuilder, errorCode);

    testDateFormat(testBuilder, errorCode);
    testPlural(testBuilder, errorCode);
    testPluralOrdinal(testBuilder, errorCode);
    testDeclareBeforeUse(testBuilder, errorCode);
}

TestCase::~TestCase() {}
TestCase::Builder::~Builder() {}

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */
