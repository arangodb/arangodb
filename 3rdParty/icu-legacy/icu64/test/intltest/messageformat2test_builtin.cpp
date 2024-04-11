// © 2024 and later: Unicode, Inc. and others.

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "unicode/calendar.h"
#include "messageformat2test.h"

using namespace icu::message2;

/*
Tests reflect the syntax specified in

  https://github.com/unicode-org/message-format-wg/commits/main/spec/message.abnf

  release LDML45-alpha:

  https://github.com/unicode-org/message-format-wg/releases/tag/LDML45-alpha
*/

void TestMessageFormat2::testDateTime(IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    LocalPointer<Calendar> cal(Calendar::createInstance(errorCode));
    TestCase::Builder testBuilder;

    testBuilder.setName("testDateTime");
    // November 23, 2022 at 7:42:37.123 PM
    cal->set(2022, Calendar::NOVEMBER, 23, 19, 42, 37);
    UDate TEST_DATE = cal->getTime(errorCode);
    UnicodeString date = "date";
    testBuilder.setLocale(Locale("ro"));

    TestCase test = testBuilder.setPattern("Testing date formatting: {$date :datetime}.")
                                .setExpected("Testing date formatting: 23.11.2022, 19:42.")
                                .setDateArgument(date, TEST_DATE)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Formatted string as argument -- `:date` should format the source Formattable
    test = testBuilder.setPattern(".local $dateStr = {$date :datetime}\n\
                                               {{Testing date formatting: {$dateStr :datetime}}}")
                                .setExpected("Testing date formatting: 23.11.2022, 19:42.")
                                .setExpectSuccess()
                                .setDateArgument(date, TEST_DATE)
                                .build();
   // Style

    testBuilder.setLocale(Locale("en", "US"));

    test = testBuilder.setPattern("Testing date formatting: {$date :date style=long}.")
                                .setExpected("Testing date formatting: November 23, 2022.")
                                .setDateArgument(date, TEST_DATE)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("Testing date formatting: {$date :date style=medium}.")
                                .setExpected("Testing date formatting: Nov 23, 2022.")
                                .setDateArgument(date, TEST_DATE)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("Testing date formatting: {$date :date style=short}.")
                                .setExpected("Testing date formatting: 11/23/22.")
                                .setDateArgument(date, TEST_DATE)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("Testing date formatting: {$date :time style=long}.")
                                .setExpected(CharsToUnicodeString("Testing date formatting: 7:42:37\\u202FPM PST."))
                                .setDateArgument(date, TEST_DATE)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("Testing date formatting: {$date :time style=medium}.")
                                .setExpected(CharsToUnicodeString("Testing date formatting: 7:42:37\\u202FPM."))
                                .setDateArgument(date, TEST_DATE)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("Testing date formatting: {$date :time style=short}.")
                                .setExpected(CharsToUnicodeString("Testing date formatting: 7:42\\u202FPM."))
                                .setDateArgument(date, TEST_DATE)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Error cases
    // Number as argument
    test = testBuilder.setPattern(".local $num = {|42| :number}\n\
                                              {{Testing date formatting: {$num :datetime}}}")
                                .clearArguments()
                                .setExpected("Testing date formatting: {|42|}")
                                .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    // Literal string as argument
    test = testBuilder.setPattern("Testing date formatting: {|horse| :datetime}")
                                .setExpected("Testing date formatting: {|horse|}")
                                .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                .build();

    TestUtils::runTestCase(*this, test, errorCode);

}

void TestMessageFormat2::testNumbers(IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    double value = 1234567890.97531;
    UnicodeString val = "val";

    TestCase::Builder testBuilder;
    testBuilder.setName("testNumbers");

    // Literals
    TestCase test = testBuilder.setPattern("From literal: {123456789 :number}!")
        .setArgument(val, value)
        .setExpected("From literal: 123.456.789!")
        .setLocale(Locale("ro"))
        .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("From literal: {|123456789.531| :number}!")
                                .setArgument(val, value)
                                .setExpected("From literal: 123.456.789,531!")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // This should fail, because number literals are not treated
    // as localized numbers
    test = testBuilder.setPattern("From literal: {|123456789,531| :number}!")
                                .setArgument(val, value)
                                .setExpectedError(U_MF_OPERAND_MISMATCH_ERROR)
                                .setExpected("From literal: {|123456789,531|}!")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("From literal: {|123456789.531| :number}!")
                                .setArgument(val, value)
                                .setExpectSuccess()
                                .setExpected(CharsToUnicodeString("From literal: \\u1041\\u1042\\u1043,\\u1044\\u1045\\u1046,\\u1047\\u1048\\u1049.\\u1045\\u1043\\u1041!"))
                                .setLocale(Locale("my"))
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);


    // Testing that the detection works for various types (without specifying :number)
    test = testBuilder.setPattern("Default double: {$val}!")
                                .setLocale(Locale("en", "IN"))
                                .setArgument(val, value)
                                .setExpected("Default double: 1,23,45,67,890.97531!")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setPattern("Default double: {$val}!")
                                .setLocale(Locale("ro"))
                                .setArgument(val, value)
                                .setExpected("Default double: 1.234.567.890,97531!")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setPattern("Default float: {$val}!")
                                .setLocale(Locale("ro"))
                                .setArgument(val, 3.1415926535)
                                .setExpected("Default float: 3,141593!")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setPattern("Default int64: {$val}!")
                                .setLocale(Locale("ro"))
                                .setArgument(val, (int64_t) 1234567890123456789)
                                .setExpected("Default int64: 1.234.567.890.123.456.789!")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setPattern("Default number: {$val}!")
                                .setLocale(Locale("ro"))
                                .setDecimalArgument(val, "1234567890123456789.987654321", errorCode)
                                .setExpected("Default number: 1.234.567.890.123.456.789,987654!")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    // Omitted CurrencyAmount test from ICU4J since it's not supported by Formattable

}

void TestMessageFormat2::testBuiltInFunctions() {
  IcuTestErrorCode errorCode(*this, "testBuiltInFunctions");

  testDateTime(errorCode);
  testNumbers(errorCode);
}

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */
