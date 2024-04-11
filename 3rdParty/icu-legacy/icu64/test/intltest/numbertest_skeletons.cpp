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
        TESTCASE_AUTO(wildcardCharacters);
        TESTCASE_AUTO(perUnitInArabic);
        TESTCASE_AUTO(perUnitToSkeleton);
        TESTCASE_AUTO(measurementSystemOverride);
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
            u"@@*",
            u"@@+",
            u"@@+/w",
            u".000##",
            u".00*",
            u".00+",
            u".",
            u"./w",
            u".*",
            u".+",
            u".+/w",
            u".######",
            u".00/@@*",
            u".00/@@+",
            u".00/@##",
            u".00/@##/w",
            u".00/@",
            u".00/@r",
            u".00/@@s",
            u".00/@@#r",
            u"precision-increment/3.14",
            u"precision-increment/3.14/w",
            u"precision-currency-standard",
            u"precision-currency-standard/w",
            u"precision-integer rounding-mode-half-up",
            u".00# rounding-mode-ceiling",
            u".00/@@* rounding-mode-floor",
            u".00/@@+ rounding-mode-floor",
            u"scientific",
            u"scientific/*ee",
            u"scientific/+ee",
            u"scientific/sign-always",
            u"scientific/*ee/sign-always",
            u"scientific/+ee/sign-always",
            u"scientific/sign-always/*ee",
            u"scientific/sign-always/+ee",
            u"scientific/sign-except-zero",
            u"engineering",
            u"engineering/*eee",
            u"engineering/+eee",
            u"compact-short",
            u"compact-long",
            u"notation-simple",
            u"percent",
            u"permille",
            u"measure-unit/length-meter",
            u"measure-unit/area-square-meter",
            u"measure-unit/energy-joule per-measure-unit/length-meter",
            u"unit/square-meter-per-square-meter",
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
            u"integer-width/*00",
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
            u"precision-currency-cash rounding-mode-ceiling",
            u"0",
            u"00",
            u"000",
            u"E0",
            u"E00",
            u"E000",
            u"EE0",
            u"EE00",
            u"EE+?0",
            u"EE+?00",
            u"EE+!0",
            u"EE+!00",
    };

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
            u".00i",
            u".00/x",
            u".00/ww",
            u".00##0",
            u".##*",
            u".00##*",
            u".0#*",
            u"@#*",
            u".##+",
            u".00##+",
            u".0#+",
            u"@#+",
            u"@@x",
            u"@@##0",
            u".00/@@",
            u".00/@@x",
            u".00/@@#",
            u".00/@@#*",
            u".00/floor/@@*", // wrong order
            u".00/@@#+",
            u".00/@@@+r",
            u".00/floor/@@+", // wrong order
            u"precision-increment/français", // non-invariant characters for C++
            u"scientific/ee",
            u"precision-increment/xxx",
            u"precision-increment/NaN",
            u"precision-increment/Infinity",
            u"precision-increment/0.1.2",
            u"scale/xxx",
            u"scale/NaN",
            u"scale/Infinity",
            u"scale/0.1.2",
            u"scale/français", // non-invariant characters for C++
            u"currency/dummy",
            u"currency/ççç", // three characters but not ASCII
            u"measure-unit/foo",
            u"integer-width/xxx",
            u"integer-width/0*",
            u"integer-width/*0#",
            u"integer-width/*#",
            u"integer-width/*#0",
            u"integer-width/0+",
            u"integer-width/+0#",
            u"integer-width/+#",
            u"integer-width/+#0",
            u"scientific/foo",
            u"E",
            u"E1",
            u"E+",
            u"E+?",
            u"E+!",
            u"E+0",
            u"EE",
            u"EE+",
            u"EEE",
            u"EEE0",
            u"001",
            u"00*",
            u"00+",
    };

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
            u".00/w/w",
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

void NumberSkeletonTest::wildcardCharacters() {
    IcuTestErrorCode status(*this, "wildcardCharacters");

    struct TestCase {
        const char16_t* star;
        const char16_t* plus;
    } cases[] = {
        { u".00*", u".00+" },
        { u"@@*", u"@@+" },
        { u"scientific/*ee", u"scientific/+ee" },
        { u"integer-width/*00", u"integer-width/+00" },
    };

    for (const auto& cas : cases) {
        UnicodeString star(cas.star);
        UnicodeString plus(cas.plus);
        status.setScope(star);

        UnicodeString normalized = NumberFormatter::forSkeleton(plus, status)
            .toSkeleton(status);
        assertEquals("Plus should normalize to star", star, normalized);
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

void NumberSkeletonTest::perUnitInArabic() {
    IcuTestErrorCode status(*this, "perUnitInArabic");

    struct TestCase {
        const char16_t* type;
        const char16_t* subtype;
    } cases[] = {
        {u"area", u"acre"},
        {u"digital", u"bit"},
        {u"digital", u"byte"},
        {u"temperature", u"celsius"},
        {u"length", u"centimeter"},
        {u"duration", u"day"},
        {u"angle", u"degree"},
        {u"temperature", u"fahrenheit"},
        {u"volume", u"fluid-ounce"},
        {u"length", u"foot"},
        {u"volume", u"gallon"},
        {u"digital", u"gigabit"},
        {u"digital", u"gigabyte"},
        {u"mass", u"gram"},
        {u"area", u"hectare"},
        {u"duration", u"hour"},
        {u"length", u"inch"},
        {u"digital", u"kilobit"},
        {u"digital", u"kilobyte"},
        {u"mass", u"kilogram"},
        {u"length", u"kilometer"},
        {u"volume", u"liter"},
        {u"digital", u"megabit"},
        {u"digital", u"megabyte"},
        {u"length", u"meter"},
        {u"length", u"mile"},
        {u"length", u"mile-scandinavian"},
        {u"volume", u"milliliter"},
        {u"length", u"millimeter"},
        {u"duration", u"millisecond"},
        {u"duration", u"minute"},
        {u"duration", u"month"},
        {u"mass", u"ounce"},
        {u"concentr", u"percent"},
        {u"digital", u"petabyte"},
        {u"mass", u"pound"},
        {u"duration", u"second"},
        {u"mass", u"stone"},
        {u"digital", u"terabit"},
        {u"digital", u"terabyte"},
        {u"duration", u"week"},
        {u"length", u"yard"},
        {u"duration", u"year"},
    };

    for (const auto& cas1 : cases) {
        for (const auto& cas2 : cases) {
            UnicodeString skeleton(u"measure-unit/");
            skeleton += cas1.type;
            skeleton += u"-";
            skeleton += cas1.subtype;
            skeleton += u" ";
            skeleton += u"per-measure-unit/";
            skeleton += cas2.type;
            skeleton += u"-";
            skeleton += cas2.subtype;

            status.setScope(skeleton);
            UnicodeString actual = NumberFormatter::forSkeleton(skeleton, status).locale("ar")
                                   .formatDouble(5142.3, status)
                                   .toString(status);
            status.errIfFailureAndReset();
        }
    }
}

void NumberSkeletonTest::perUnitToSkeleton() {
    IcuTestErrorCode status(*this, "perUnitToSkeleton");
    struct TestCase {
        const char16_t* type;
        const char16_t* subtype;
    } cases[] = {
        {u"area", u"acre"},
        {u"concentr", u"percent"},
        {u"concentr", u"permille"},
        {u"concentr", u"permillion"},
        {u"concentr", u"permyriad"},
        {u"digital", u"bit"},
        {u"length", u"yard"},
    };

    for (const auto& cas1 : cases) {
        for (const auto& cas2 : cases) {
            UnicodeString skeleton(u"measure-unit/");
            skeleton += cas1.type;
            skeleton += u"-";
            skeleton += cas1.subtype;
            skeleton += u" ";
            skeleton += u"per-measure-unit/";
            skeleton += cas2.type;
            skeleton += u"-";
            skeleton += cas2.subtype;

            status.setScope(skeleton);
            if (cas1.type != cas2.type && cas1.subtype != cas2.subtype) {
                UnicodeString toSkeleton = NumberFormatter::forSkeleton(
                    skeleton, status).toSkeleton(status);
                if (status.errIfFailureAndReset()) {
                    continue;
                }
                // Ensure both subtype are in the toSkeleton.
                UnicodeString msg;
                msg.append(toSkeleton)
                    .append(" should contain '")
                    .append(UnicodeString(cas1.subtype))
                    .append("' when constructed from ")
                    .append(skeleton);
                assertTrue(msg, toSkeleton.indexOf(cas1.subtype) >= 0);

                msg.remove();
                msg.append(toSkeleton)
                    .append(" should contain '")
                    .append(UnicodeString(cas2.subtype))
                    .append("' when constructed from ")
                    .append(skeleton);
                assertTrue(msg, toSkeleton.indexOf(cas2.subtype) >= 0);
            }
        }
    }
}

void NumberSkeletonTest::measurementSystemOverride() {
    // NOTE TO REVIEWERS: When the appropriate changes are made on the CLDR side, do we want to keep this
    // test or rely on additions the CLDR project makes to unitPreferencesTest.txt? --rtg 8/29/23
    IcuTestErrorCode status(*this, "measurementSystemOverride");
    struct TestCase {
        const char* locale;
        const char16_t* skeleton;
        const char16_t* expectedResult;
    } testCases[] = {
        // Norway uses m/s for wind speed and should with or without the "ms-metric" subtag in the locale,
        // but it uses km/h for other speeds.  France uses km/h for all speeds.  And in both places, if
        // you say "ms-ussystem", you should get mph.  In the US, we use mph for all speeds, but should
        // use km/h if the locale has "ms-metric" in it.
        { "nn_NO",                  u"unit/kilometer-per-hour usage/wind",    u"0,34 m/s" },
        { "nn_NO@measure=metric",   u"unit/kilometer-per-hour usage/wind",    u"0,34 m/s" },
        { "nn_NO@measure=ussystem", u"unit/kilometer-per-hour usage/wind",    u"0,76 mile/t" },
        { "fr_FR",                  u"unit/kilometer-per-hour usage/wind",    u"1,2\u202Fkm/h" },
        { "fr_FR@measure=metric",   u"unit/kilometer-per-hour usage/wind",    u"1,2\u202Fkm/h" },
        { "fr_FR@measure=ussystem", u"unit/kilometer-per-hour usage/wind",    u"0,76\u202Fmi/h" },
        { "en_US",                  u"unit/kilometer-per-hour usage/wind",    u"0.76 mph" },
        { "en_US@measure=metric",   u"unit/kilometer-per-hour usage/wind",    u"1.2 km/h" },
        { "en_US@measure=ussystem", u"unit/kilometer-per-hour usage/wind",    u"0.76 mph" },

        { "nn_NO",                  u"unit/kilometer-per-hour usage/default", u"1,2 km/t" },
        { "nn_NO@measure=metric",   u"unit/kilometer-per-hour usage/default", u"1,2 km/t" },
        { "nn_NO@measure=ussystem", u"unit/kilometer-per-hour usage/default", u"0,76 mile/t" },
        { "fr_FR",                  u"unit/kilometer-per-hour usage/default", u"1,2\u202Fkm/h" },
        { "fr_FR@measure=metric",   u"unit/kilometer-per-hour usage/default", u"1,2\u202Fkm/h" },
        { "fr_FR@measure=ussystem", u"unit/kilometer-per-hour usage/default", u"0,76\u202Fmi/h" },
        { "en_US",                  u"unit/kilometer-per-hour usage/default", u"0.76 mph" },
        { "en_US@measure=metric",   u"unit/kilometer-per-hour usage/default", u"1.2 km/h" },
        { "en_US@measure=ussystem", u"unit/kilometer-per-hour usage/default", u"0.76 mph" },
    };
    
    for (const auto& testCase : testCases) {
        UErrorCode err = U_ZERO_ERROR;
        LocalizedNumberFormatter nf = NumberFormatter::forSkeleton(testCase.skeleton, err).locale(testCase.locale);
        UnicodeString actualResult = nf.formatDouble(1.23, err).toString(err);
        
        UnicodeString errorMessage = ": ";
        errorMessage += testCase.locale;
        errorMessage += "/";
        errorMessage += testCase.skeleton;
        if (assertSuccess(u"Formatting error" + errorMessage, err)) {
            assertEquals(u"Wrong result" + errorMessage, testCase.expectedResult, actualResult);
        }
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
