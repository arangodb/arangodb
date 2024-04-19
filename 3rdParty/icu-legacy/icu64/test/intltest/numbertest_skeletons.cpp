// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/dcfmtsym.h"

#include "cstr.h"
#include "numbertest.h"
#include "number_utils.h"
#include "number_skeletons.h"
#include "putilimp.h"

using namespace icu::number::impl;


void NumberSkeletonTest::runIndexedTest(int32_t index, UBool exec, const char*& name, char*) {
    if (exec) {
        logln("TestSuite AffixUtilsTest: ");
    }
    TESTCASE_AUTO_BEGIN;
        TESTCASE_AUTO(validTokens);
        TESTCASE_AUTO(invalidTokens);
        TESTCASE_AUTO(unknownTokens);
        TESTCASE_AUTO(unexpectedTokens);
        TESTCASE_AUTO(duplicateValues);
        TESTCASE_AUTO(stemsRequiringOption);
        TESTCASE_AUTO(defaultTokens);
        TESTCASE_AUTO(flexibleSeparators);
    TESTCASE_AUTO_END;
}

void NumberSkeletonTest::validTokens() {
    IcuTestErrorCode status(*this, "validTokens");

    // This tests only if the tokens are valid, not their behavior.
    // Most of these are from the design doc.
    static const char16_t* cases[] = {
            u"precision-integer",
            u"precision-unlimited",
            u"@@@##",
            u"@@+",
            u".000##",
            u".00+",
            u".",
            u".+",
            u".######",
            u".00/@@+",
            u".00/@##",
            u"precision-increment/3.14",
            u"precision-currency-standard",
            u"precision-integer rounding-mode-half-up",
            u".00# rounding-mode-ceiling",
            u".00/@@+ rounding-mode-floor",
            u"scientific",
            u"scientific/+ee",
            u"scientific/sign-always",
            u"scientific/+ee/sign-always",
            u"scientific/sign-always/+ee",
            u"scientific/sign-except-zero",
            u"engineering",
            u"engineering/+eee",
            u"compact-short",
            u"compact-long",
            u"notation-simple",
            u"percent",
            u"permille",
            u"measure-unit/length-meter",
            u"measure-unit/area-square-meter",
            u"measure-unit/energy-joule per-measure-unit/length-meter",
            u"currency/XXX",
            u"currency/ZZZ",
            u"currency/usd",
            u"group-off",
            u"group-min2",
            u"group-auto",
            u"group-on-aligned",
            u"group-thousands",
            u"integer-width/00",
            u"integer-width/#0",
            u"integer-width/+00",
            u"sign-always",
            u"sign-auto",
            u"sign-never",
            u"sign-accounting",
            u"sign-accounting-always",
            u"sign-except-zero",
            u"sign-accounting-except-zero",
            u"unit-width-narrow",
            u"unit-width-short",
            u"unit-width-iso-code",
            u"unit-width-full-name",
            u"unit-width-hidden",
            u"decimal-auto",
            u"decimal-always",
            u"scale/5.2",
            u"scale/-5.2",
            u"scale/100",
            u"scale/1E2",
            u"scale/1",
            u"latin",
            u"numbering-system/arab",
            u"numbering-system/latn",
            u"precision-integer/@##",
            u"precision-integer rounding-mode-ceiling",
            u"precision-currency-cash rounding-mode-ceiling"};

    for (auto& cas : cases) {
        UnicodeString skeletonString(cas);
        status.setScope(skeletonString);
        UParseError perror;
        NumberFormatter::forSkeleton(skeletonString, perror, status);
        assertSuccess(CStr(skeletonString)(), status, true);
        assertEquals(skeletonString, -1, perror.offset);
        status.errIfFailureAndReset();
    }
}

void NumberSkeletonTest::invalidTokens() {
    static const char16_t* cases[] = {
            u".00x",
            u".00##0",
            u".##+",
            u".00##+",
            u".0#+",
            u"@@x",
            u"@@##0",
            u"@#+",
            u".00/@",
            u".00/@@",
            u".00/@@x",
            u".00/@@#",
            u".00/@@#+",
            u".00/floor/@@+", // wrong order
            u"precision-increment/français", // non-invariant characters for C++
            u"scientific/ee",
            u"precision-increment/xxx",
            u"precision-increment/NaN",
            u"precision-increment/0.1.2",
            u"scale/xxx",
            u"scale/NaN",
            u"scale/0.1.2",
            u"scale/français", // non-invariant characters for C++
            u"currency/dummy",
            u"currency/ççç", // three characters but not ASCII
            u"measure-unit/foo",
            u"integer-width/xxx",
            u"integer-width/0+",
            u"integer-width/+0#",
            u"scientific/foo"};

    expectedErrorSkeleton(cases, UPRV_LENGTHOF(cases));
}

void NumberSkeletonTest::unknownTokens() {
    static const char16_t* cases[] = {
            u"maesure-unit",
            u"measure-unit/foo-bar",
            u"numbering-system/dummy",
            u"français",
            u"measure-unit/français-français", // non-invariant characters for C++
            u"numbering-system/français", // non-invariant characters for C++
            u"currency-USD"};

    expectedErrorSkeleton(cases, UPRV_LENGTHOF(cases));
}

void NumberSkeletonTest::unexpectedTokens() {
    static const char16_t* cases[] = {
            u"group-thousands/foo",
            u"precision-integer//@## group-off",
            u"precision-integer//@##  group-off",
            u"precision-integer/ group-off",
            u"precision-integer// group-off"};

    expectedErrorSkeleton(cases, UPRV_LENGTHOF(cases));
}

void NumberSkeletonTest::duplicateValues() {
    static const char16_t* cases[] = {
            u"precision-integer precision-integer",
            u"precision-integer .00+",
            u"precision-integer precision-unlimited",
            u"precision-integer @@@",
            u"scientific engineering",
            u"engineering compact-long",
            u"sign-auto sign-always"};

    expectedErrorSkeleton(cases, UPRV_LENGTHOF(cases));
}

void NumberSkeletonTest::stemsRequiringOption() {
    static const char16_t* stems[] = {
            u"precision-increment",
            u"measure-unit",
            u"per-measure-unit",
            u"currency",
            u"integer-width",
            u"numbering-system",
            u"scale"};
    static const char16_t* suffixes[] = {u"", u"/@##", u" scientific", u"/@## scientific"};

    for (auto& stem : stems) {
        for (auto& suffix : suffixes) {
            UnicodeString skeletonString = UnicodeString(stem) + suffix;
            UErrorCode status = U_ZERO_ERROR;
            UParseError perror;
            NumberFormatter::forSkeleton(skeletonString, perror, status);
            assertEquals(skeletonString, U_NUMBER_SKELETON_SYNTAX_ERROR, status);

            // Check the UParseError for integrity.
            // If an option is present, the option is wrong; error offset is at the start of the option
            // If an option is not present, the error offset is at the token separator (end of stem)
            int32_t expectedOffset = u_strlen(stem) + ((suffix[0] == u'/') ? 1 : 0);
            assertEquals(skeletonString, expectedOffset, perror.offset);
            UnicodeString expectedPreContext = skeletonString.tempSubString(0, expectedOffset);
            if (expectedPreContext.length() >= U_PARSE_CONTEXT_LEN - 1) {
                expectedPreContext = expectedPreContext.tempSubString(expectedOffset - U_PARSE_CONTEXT_LEN + 1);
            }
            assertEquals(skeletonString, expectedPreContext, perror.preContext);
            UnicodeString expectedPostContext = skeletonString.tempSubString(expectedOffset);
            // None of the postContext strings in this test exceed U_PARSE_CONTEXT_LEN
            assertEquals(skeletonString, expectedPostContext, perror.postContext);
        }
    }
}

void NumberSkeletonTest::defaultTokens() {
    IcuTestErrorCode status(*this, "defaultTokens");

    static const char16_t* cases[] = {
            u"notation-simple",
            u"base-unit",
            u"group-auto",
            u"integer-width/+0",
            u"sign-auto",
            u"unit-width-short",
            u"decimal-auto"};

    for (auto& cas : cases) {
        UnicodeString skeletonString(cas);
        status.setScope(skeletonString);
        UnicodeString normalized = NumberFormatter::forSkeleton(
                skeletonString, status).toSkeleton(status);
        // Skeleton should become empty when normalized
        assertEquals(skeletonString, u"", normalized);
        status.errIfFailureAndReset();
    }
}

void NumberSkeletonTest::flexibleSeparators() {
    IcuTestErrorCode status(*this, "flexibleSeparators");

    static struct TestCase {
        const char16_t* skeleton;
        const char16_t* expected;
    } cases[] = {{u"precision-integer group-off", u"5142"},
                 {u"precision-integer  group-off", u"5142"},
                 {u"precision-integer/@## group-off", u"5140"},
                 {u"precision-integer/@##  group-off", u"5140"}};

    for (auto& cas : cases) {
        UnicodeString skeletonString(cas.skeleton);
        UnicodeString expected(cas.expected);
        status.setScope(skeletonString);
        UnicodeString actual = NumberFormatter::forSkeleton(skeletonString, status).locale("en")
                               .formatDouble(5142.3, status)
                               .toString(status);
        if (!status.errDataIfFailureAndReset()) {
            assertEquals(skeletonString, expected, actual);
        }
        status.errIfFailureAndReset();
    }
}

// In C++, there is no distinguishing between "invalid", "unknown", and "unexpected" tokens.
void NumberSkeletonTest::expectedErrorSkeleton(const char16_t** cases, int32_t casesLen) {
    for (int32_t i = 0; i < casesLen; i++) {
        UnicodeString skeletonString(cases[i]);
        UErrorCode status = U_ZERO_ERROR;
        NumberFormatter::forSkeleton(skeletonString, status);
        assertEquals(skeletonString, U_NUMBER_SKELETON_SYNTAX_ERROR, status);
    }
}


#endif /* #if !UCONFIG_NO_FORMATTING */
