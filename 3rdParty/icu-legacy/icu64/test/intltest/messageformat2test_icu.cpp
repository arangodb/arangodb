// © 2024 and later: Unicode, Inc. and others.

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "unicode/gregocal.h"
#include "unicode/msgfmt.h"
#include "messageformat2test.h"

using namespace icu::message2;

/*
  Tests based on ICU4J's Mf2IcuTest.java
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

void TestMessageFormat2::testSample(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    TestUtils::runTestCase(*this, testBuilder.setPattern("There are {$count} files on {$where}")
                                .setArgument("count", "abc")
                                .setArgument("where", "def")
                                .setExpected("There are abc files on def")
                                .build(), errorCode);
}

void TestMessageFormat2::testStaticFormat(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    TestUtils::runTestCase(*this, testBuilder.setPattern("At {$when :time style=medium} on {$when :date style=medium}, \
there was {$what} on planet {$planet :integer}.")
                                .setArgument("planet", (int64_t) 7)
                                .setDateArgument("when", (UDate) 871068000000)
                                .setArgument("what", "a disturbance in the Force")
                                .setExpected(CharsToUnicodeString("At 12:20:00\\u202FPM on Aug 8, 1997, there was a disturbance in the Force on planet 7."))
                                .build(), errorCode);
}

void TestMessageFormat2::testSimpleFormat(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    testBuilder.setPattern("The disk \"{$diskName}\" contains {$fileCount} file(s).");
    testBuilder.setArgument("diskName", "MyDisk");


    TestCase test = testBuilder.setArgument("fileCount", (int64_t) 0)
        .setExpected("The disk \"MyDisk\" contains 0 file(s).")
        .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setArgument("fileCount", (int64_t) 1)
                      .setExpected("The disk \"MyDisk\" contains 1 file(s).")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setArgument("fileCount", (int64_t) 12)
                      .setExpected("The disk \"MyDisk\" contains 12 file(s).")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

void TestMessageFormat2::testSelectFormatToPattern(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    UnicodeString pattern = CharsToUnicodeString(".match {$userGender :string}\n\
                 female {{{$userName} est all\\u00E9e \\u00E0 Paris.}}\n\
                 *     {{{$userName} est all\\u00E9 \\u00E0 Paris.}}");

    testBuilder.setPattern(pattern);

    TestCase test = testBuilder.setArgument("userName", "Charlotte")
                                .setArgument("userGender", "female")
                                .setExpected(CharsToUnicodeString("Charlotte est all\\u00e9e \\u00e0 Paris."))
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setArgument("userName", "Guillaume")
                                .setArgument("userGender", "male")
                                .setExpected(CharsToUnicodeString("Guillaume est all\\u00e9 \\u00e0 Paris."))
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setArgument("userName", "Dominique")
                                .setArgument("userGender", "unknown")
                                .setExpected(CharsToUnicodeString("Dominique est all\\u00e9 \\u00e0 Paris."))
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

void TestMessageFormat2::testMf1Behavior(TestCase::Builder& testBuilder, IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    UDate testDate = UDate(1671782400000); // 2022-12-23
    UnicodeString user = "John";
    UnicodeString badArgumentsNames[] = {
        "userX", "todayX"
    };
    UnicodeString goodArgumentsNames[] = {
        "user", "today"
    };
    icu::Formattable oldArgumentsValues[] = {
        icu::Formattable(user), icu::Formattable(testDate, icu::Formattable::kIsDate)
    };
    UnicodeString expectedGood = "Hello John, today is December 23, 2022.";

    LocalPointer<MessageFormat> mf1(new MessageFormat("Hello {user}, today is {today,date,long}.", errorCode));
    CHECK_ERROR(errorCode);

    UnicodeString result;
    mf1->format(badArgumentsNames, oldArgumentsValues, 2, result, errorCode);
    assertEquals("testMf1Behavior", (UBool) true, U_SUCCESS(errorCode));
    assertEquals("old icu test", "Hello {user}, today is {today}.", result);
    result.remove();
    mf1->format(goodArgumentsNames, oldArgumentsValues, 2, result, errorCode);
    assertEquals("testMf1Behavior", (UBool) true, U_SUCCESS(errorCode));
    assertEquals("old icu test", expectedGood, result);

    TestCase test = testBuilder.setPattern("Hello {$user}, today is {$today :date style=long}.")
                                .setArgument(badArgumentsNames[0], user)
                                .setDateArgument(badArgumentsNames[1], testDate)
                                .setExpected("Hello {$user}, today is {$today}.")
                                .setExpectedError(U_MF_UNRESOLVED_VARIABLE_ERROR)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.clearArguments()
                      .setExpectSuccess()
                      .setArgument(goodArgumentsNames[0], user)
                      .setDateArgument(goodArgumentsNames[1], testDate)
                      .setExpected(expectedGood)
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

void TestMessageFormat2::messageFormat1Tests() {
    IcuTestErrorCode errorCode(*this, "featureTests");

    TestCase::Builder testBuilder;
    testBuilder.setName("messageFormat1Tests");

    testSample(testBuilder, errorCode);
    testStaticFormat(testBuilder, errorCode);
    testSimpleFormat(testBuilder, errorCode);
    testSelectFormatToPattern(testBuilder, errorCode);
    testMf1Behavior(testBuilder, errorCode);
}

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */
