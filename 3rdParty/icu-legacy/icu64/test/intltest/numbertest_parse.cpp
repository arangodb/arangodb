// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "numbertest.h"
#include "numparse_impl.h"
#include "static_unicode_sets.h"
#include "unicode/dcfmtsym.h"
#include "unicode/testlog.h"

#include <cmath>
#include <numparse_affixes.h>

void NumberParserTest::runIndexedTest(int32_t index, UBool exec, const char*& name, char*) {
    if (exec) {
        logln("TestSuite NumberParserTest: ");
    }
    TESTCASE_AUTO_BEGIN;
        TESTCASE_AUTO(testBasic);
        TESTCASE_AUTO(testSeriesMatcher);
        TESTCASE_AUTO(testCombinedCurrencyMatcher);
        TESTCASE_AUTO(testAffixPatternMatcher);
        TESTCASE_AUTO(test20360_BidiOverflow);
        TESTCASE_AUTO(testInfiniteRecursion);
    TESTCASE_AUTO_END;
}

void NumberParserTest::testBasic() {
    IcuTestErrorCode status(*this, "testBasic");

    static const struct TestCase {
        int32_t flags;
        const char16_t* inputString;
        const char16_t* patternString;
        int32_t expectedCharsConsumed;
        double expectedResultDouble;
    } cases[] = {{3, u"51423", u"0", 5, 51423.},
                 {3, u"51423x", u"0", 5, 51423.},
                 {3, u" 51423", u"0", 6, 51423.},
                 {3, u"51423 ", u"0", 5, 51423.},
                 {3, u"𝟱𝟭𝟰𝟮𝟯", u"0", 10, 51423.},
                 {3, u"𝟱𝟭𝟰𝟮𝟯x", u"0", 10, 51423.},
                 {3, u" 𝟱𝟭𝟰𝟮𝟯", u"0", 11, 51423.},
                 {3, u"𝟱𝟭𝟰𝟮𝟯 ", u"0", 10, 51423.},
                 {7, u"51,423", u"#,##,##0", 6, 51423.},
                 {7, u" 51,423", u"#,##,##0", 7, 51423.},
                 {7, u"51,423 ", u"#,##,##0", 6, 51423.},
                 {7, u"51,423,", u"#,##,##0", 6, 51423.},
                 {7, u"51,423,,", u"#,##,##0", 6, 51423.},
                 {7, u"51,423.5", u"#,##,##0", 8, 51423.5},
                 {7, u"51,423.5,", u"#,##,##0", 8, 51423.5},
                 {7, u"51,423.5,,", u"#,##,##0", 8, 51423.5},
                 {7, u"51,423.5.", u"#,##,##0", 8, 51423.5},
                 {7, u"51,423.5..", u"#,##,##0", 8, 51423.5},
                 {7, u"𝟱𝟭,𝟰𝟮𝟯", u"#,##,##0", 11, 51423.},
                 {7, u"𝟳,𝟴𝟵,𝟱𝟭,𝟰𝟮𝟯", u"#,##,##0", 19, 78951423.},
                 {7, u"𝟳𝟴,𝟵𝟱𝟭.𝟰𝟮𝟯", u"#,##,##0", 18, 78951.423},
                 {7, u"𝟳𝟴,𝟬𝟬𝟬", u"#,##,##0", 11, 78000.},
                 {7, u"𝟳𝟴,𝟬𝟬𝟬.𝟬𝟬𝟬", u"#,##,##0", 18, 78000.},
                 {7, u"𝟳𝟴,𝟬𝟬𝟬.𝟬𝟮𝟯", u"#,##,##0", 18, 78000.023},
                 {7, u"𝟳𝟴.𝟬𝟬𝟬.𝟬𝟮𝟯", u"#,##,##0", 11, 78.},
                 {7, u"1,", u"#,##,##0", 1, 1.},
                 {7, u"1,,", u"#,##,##0", 1, 1.},
                 {7, u"1.,", u"#,##,##0", 2, 1.},
                 {3, u"1,.", u"#,##,##0", 3, 1.},
                 {7, u"1..", u"#,##,##0", 2, 1.},
                 {3, u",1", u"#,##,##0", 2, 1.},
                 {3, u"1,1", u"#,##,##0", 1, 1.},
                 {3, u"1,1,", u"#,##,##0", 1, 1.},
                 {3, u"1,1,,", u"#,##,##0", 1, 1.},
                 {3, u"1,1,1", u"#,##,##0", 1, 1.},
                 {3, u"1,1,1,", u"#,##,##0", 1, 1.},
                 {3, u"1,1,1,,", u"#,##,##0", 1, 1.},
                 {3, u"-51423", u"0", 6, -51423.},
                 {3, u"51423-", u"0", 5, 51423.}, // plus and minus sign by default do NOT match after
                 {3, u"+51423", u"0", 6, 51423.},
                 {3, u"51423+", u"0", 5, 51423.}, // plus and minus sign by default do NOT match after
                 {3, u"%51423", u"0", 6, 51423.},
                 {3, u"51423%", u"0", 6, 51423.},
                 {3, u"51423%%", u"0", 6, 51423.},
                 {3, u"‰51423", u"0", 6, 51423.},
                 {3, u"51423‰", u"0", 6, 51423.},
                 {3, u"51423‰‰", u"0", 6, 51423.},
                 {3, u"∞", u"0", 1, INFINITY},
                 {3, u"-∞", u"0", 2, -INFINITY},
                 {3, u"@@@123  @@", u"0", 6, 123.}, // TODO: Should padding be strong instead of weak?
                 {3, u"@@@123@@  ", u"0", 6, 123.}, // TODO: Should padding be strong instead of weak?
                 {3, u"a51423US dollars", u"a0¤¤¤", 16, 51423.},
                 {3, u"a 51423 US dollars", u"a0¤¤¤", 18, 51423.},
                 {3, u"514.23 USD", u"¤0", 10, 514.23},
                 {3, u"514.23 GBP", u"¤0", 10, 514.23},
                 {3, u"a 𝟱𝟭𝟰𝟮𝟯 b", u"a0b", 14, 51423.},
                 {3, u"-a 𝟱𝟭𝟰𝟮𝟯 b", u"a0b", 15, -51423.},
                 {3, u"a -𝟱𝟭𝟰𝟮𝟯 b", u"a0b", 15, -51423.},
                 {3, u"𝟱𝟭𝟰𝟮𝟯", u"[0];(0)", 10, 51423.},
                 {3, u"[𝟱𝟭𝟰𝟮𝟯", u"[0];(0)", 11, 51423.},
                 {3, u"𝟱𝟭𝟰𝟮𝟯]", u"[0];(0)", 11, 51423.},
                 {3, u"[𝟱𝟭𝟰𝟮𝟯]", u"[0];(0)", 12, 51423.},
                 {3, u"(𝟱𝟭𝟰𝟮𝟯", u"[0];(0)", 11, -51423.},
                 {3, u"𝟱𝟭𝟰𝟮𝟯)", u"[0];(0)", 11, -51423.},
                 {3, u"(𝟱𝟭𝟰𝟮𝟯)", u"[0];(0)", 12, -51423.},
                 {3, u"𝟱𝟭𝟰𝟮𝟯", u"{0};{0}", 10, 51423.},
                 {3, u"{𝟱𝟭𝟰𝟮𝟯", u"{0};{0}", 11, 51423.},
                 {3, u"𝟱𝟭𝟰𝟮𝟯}", u"{0};{0}", 11, 51423.},
                 {3, u"{𝟱𝟭𝟰𝟮𝟯}", u"{0};{0}", 12, 51423.},
                 {1, u"a40b", u"a0'0b'", 3, 40.}, // greedy code path thinks "40" is the number
                 {2, u"a40b", u"a0'0b'", 4, 4.}, // slow code path finds the suffix "0b"
                 {3, u"𝟱.𝟭𝟰𝟮E𝟯", u"0", 12, 5142.},
                 {3, u"𝟱.𝟭𝟰𝟮E-𝟯", u"0", 13, 0.005142},
                 {3, u"𝟱.𝟭𝟰𝟮e-𝟯", u"0", 13, 0.005142},
                 {7, u"5,142.50 Canadian dollars", u"#,##,##0 ¤¤¤", 25, 5142.5},
                 {3, u"a$ b5", u"a ¤ b0", 5, 5.0},
                 {3, u"📺1.23", u"📺0;📻0", 6, 1.23},
                 {3, u"📻1.23", u"📺0;📻0", 6, -1.23},
                 {3, u".00", u"0", 3, 0.0},
                 {3, u"                              1,234", u"a0", 35, 1234.}, // should not hang
                 {3, u"NaN", u"0", 3, NAN},
                 {3, u"NaN E5", u"0", 6, NAN},
                 {3, u"0", u"0", 1, 0.0}};

    parse_flags_t parseFlags = PARSE_FLAG_IGNORE_CASE | PARSE_FLAG_INCLUDE_UNPAIRED_AFFIXES;
    for (auto& cas : cases) {
        UnicodeString inputString(cas.inputString);
        UnicodeString patternString(cas.patternString);
        LocalPointer<const NumberParserImpl> parser(
                NumberParserImpl::createSimpleParser(
                        Locale("en"), patternString, parseFlags, status));
        if (status.errDataIfFailureAndReset("createSimpleParser() failed")) {
            continue;
        }
        UnicodeString message =
                UnicodeString("Input <") + inputString + UnicodeString("> Parser ") + parser->toString();

        if (0 != (cas.flags & 0x01)) {
            // Test greedy code path
            ParsedNumber resultObject;
            parser->parse(inputString, true, resultObject, status);
            assertTrue("Greedy Parse failed: " + message, resultObject.success());
            assertEquals("Greedy Parse failed: " + message,
                cas.expectedCharsConsumed, resultObject.charEnd);
            assertEquals("Greedy Parse failed: " + message,
                cas.expectedResultDouble, resultObject.getDouble(status));
        }

        if (0 != (cas.flags & 0x02)) {
            // Test slow code path
            ParsedNumber resultObject;
            parser->parse(inputString, false, resultObject, status);
            assertTrue("Non-Greedy Parse failed: " + message, resultObject.success());
            assertEquals(
                    "Non-Greedy Parse failed: " + message,
                    cas.expectedCharsConsumed,
                    resultObject.charEnd);
            assertEquals(
                    "Non-Greedy Parse failed: " + message,
                    cas.expectedResultDouble,
                    resultObject.getDouble(status));
        }

        if (0 != (cas.flags & 0x04)) {
            // Test with strict separators
            parser.adoptInstead(
                    NumberParserImpl::createSimpleParser(
                            Locale("en"),
                            patternString,
                            parseFlags | PARSE_FLAG_STRICT_GROUPING_SIZE,
                            status));
            ParsedNumber resultObject;
            parser->parse(inputString, true, resultObject, status);
            assertTrue("Strict Parse failed: " + message, resultObject.success());
            assertEquals("Strict Parse failed: " + message,
                cas.expectedCharsConsumed, resultObject.charEnd);
            assertEquals("Strict Parse failed: " + message,
                cas.expectedResultDouble, resultObject.getDouble(status));
        }
    }
}

void NumberParserTest::testSeriesMatcher() {
    IcuTestErrorCode status(*this, "testSeriesMatcher");

    DecimalFormatSymbols symbols("en", status);
    if (status.errDataIfFailureAndReset("Failure in DecimalFormtSymbols constructor")) {
        return;
    }
    PlusSignMatcher m0(symbols, false);
    MinusSignMatcher m1(symbols, false);
    IgnorablesMatcher m2(unisets::DEFAULT_IGNORABLES);
    PercentMatcher m3(symbols);
    IgnorablesMatcher m4(unisets::DEFAULT_IGNORABLES);

    ArraySeriesMatcher::MatcherArray matchers(5);
    matchers[0] = &m0;
    matchers[1] = &m1;
    matchers[2] = &m2;
    matchers[3] = &m3;
    matchers[4] = &m4;
    ArraySeriesMatcher series(matchers, 5);

    assertFalse("", series.smokeTest(StringSegment(u"x", false)));
    assertFalse("", series.smokeTest(StringSegment(u"-", false)));
    assertTrue("", series.smokeTest(StringSegment(u"+", false)));

    static const struct TestCase {
        const char16_t* input;
        int32_t expectedOffset;
        bool expectedMaybeMore;
    } cases[] = {{u"", 0, true},
                 {u" ", 0, false},
                 {u"$", 0, false},
                 {u"+", 0, true},
                 {u" +", 0, false},
                 {u"+-", 0, true},
                 {u"+ -", 0, false},
                 {u"+-  ", 0, true},
                 {u"+-  $", 0, false},
                 {u"+-%", 3, true},
                 {u"  +-  %  ", 0, false},
                 {u"+-  %  ", 7, true},
                 {u"+-%$", 3, false}};

    for (auto& cas : cases) {
        UnicodeString input(cas.input);

        StringSegment segment(input, false);
        ParsedNumber result;
        bool actualMaybeMore = series.match(segment, result, status);
        int actualOffset = segment.getOffset();

        assertEquals("'" + input + "'", cas.expectedOffset, actualOffset);
        assertEquals("'" + input + "'", cas.expectedMaybeMore, actualMaybeMore);
    }
}

void NumberParserTest::testCombinedCurrencyMatcher() {
    IcuTestErrorCode status(*this, "testCombinedCurrencyMatcher");

    IgnorablesMatcher ignorables(unisets::DEFAULT_IGNORABLES);
    Locale locale = Locale::getEnglish();

    DecimalFormatSymbols dfs(locale, status);
    if (status.errDataIfFailureAndReset("Failure in DecimalFormtSymbols constructor")) {
        return;
    }
    dfs.setSymbol(DecimalFormatSymbols::kCurrencySymbol, u"IU$", status);
    dfs.setSymbol(DecimalFormatSymbols::kIntlCurrencySymbol, u"ICU", status);
    CurrencySymbols currencySymbols({u"ICU", status}, locale, dfs, status);

    AffixTokenMatcherSetupData affixSetupData = {
            currencySymbols, {"en", status}, ignorables, "en", 0};
    AffixTokenMatcherWarehouse warehouse(&affixSetupData);
    NumberParseMatcher& matcher = warehouse.currency(status);
    affixSetupData.parseFlags = PARSE_FLAG_NO_FOREIGN_CURRENCY;
    AffixTokenMatcherWarehouse warehouseNoForeign(&affixSetupData);
    NumberParseMatcher& matcherNoForeign = warehouseNoForeign.currency(status);

    static const struct TestCase {
        const char16_t* input;
        const char16_t* expectedCurrencyCode;
        const char16_t* expectedNoForeignCurrencyCode;
    } cases[]{{u"", u"", u""},
              {u"FOO", u"", u""},
              {u"USD", u"USD", u""},
              {u"$", u"USD", u""},
              {u"US dollars", u"USD", u""},
              {u"eu", u"", u""},
              {u"euros", u"EUR", u""},
              {u"ICU", u"ICU", u"ICU"},
              {u"IU$", u"ICU", u"ICU"}};
    for (auto& cas : cases) {
        UnicodeString input(cas.input);

        {
            StringSegment segment(input, false);
            ParsedNumber result;
            matcher.match(segment, result, status);
            assertEquals(
                    "Parsing " + input,
                    cas.expectedCurrencyCode,
                    result.currencyCode);
            assertEquals(
                    "Whole string on " + input,
                    cas.expectedCurrencyCode[0] == 0 ? 0 : input.length(),
                    result.charEnd);
        }
        {
            StringSegment segment(input, false);
            ParsedNumber result;
            matcherNoForeign.match(segment, result, status);
            assertEquals(
                    "[no foreign] Parsing " + input,
                    cas.expectedNoForeignCurrencyCode,
                    result.currencyCode);
            assertEquals(
                    "[no foreign] Whole string on " + input,
                    cas.expectedNoForeignCurrencyCode[0] == 0 ? 0 : input.length(),
                    result.charEnd);
        }
    }
}

void NumberParserTest::testAffixPatternMatcher() {
    IcuTestErrorCode status(*this, "testAffixPatternMatcher");
    Locale locale = Locale::getEnglish();
    IgnorablesMatcher ignorables(unisets::DEFAULT_IGNORABLES);

    DecimalFormatSymbols dfs(locale, status);
    dfs.setSymbol(DecimalFormatSymbols::kCurrencySymbol, u"IU$", status);
    dfs.setSymbol(DecimalFormatSymbols::kIntlCurrencySymbol, u"ICU", status);
    CurrencySymbols currencySymbols({u"ICU", status}, locale, dfs, status);

    AffixTokenMatcherSetupData affixSetupData = {
            currencySymbols, {"en", status}, ignorables, "en", 0};
    AffixTokenMatcherWarehouse warehouse(&affixSetupData);

    static const struct TestCase {
        bool exactMatch;
        const char16_t* affixPattern;
        int32_t expectedMatcherLength;
        const char16_t* sampleParseableString;
    } cases[] = {{false, u"-", 1, u"-"},
                 {false, u"+-%", 5, u"+-%"},
                 {true, u"+-%", 3, u"+-%"},
                 {false, u"ab c", 5, u"a    bc"},
                 {true, u"abc", 3, u"abc"},
                 {false, u"hello-to+this%very¤long‰string", 59, u"hello-to+this%very USD long‰string"}};

    for (auto& cas : cases) {
        UnicodeString affixPattern(cas.affixPattern);
        UnicodeString sampleParseableString(cas.sampleParseableString);
        int parseFlags = cas.exactMatch ? PARSE_FLAG_EXACT_AFFIX : 0;

        bool success;
        AffixPatternMatcher matcher = AffixPatternMatcher::fromAffixPattern(
                affixPattern, warehouse, parseFlags, &success, status);
        if (!status.errDataIfFailureAndReset("Creation should be successful")) {

            // Check that the matcher has the expected number of children
            assertEquals(affixPattern + " " + cas.exactMatch, cas.expectedMatcherLength, matcher.length());

            // Check that the matcher works on a sample string
            StringSegment segment(sampleParseableString, false);
            ParsedNumber result;
            matcher.match(segment, result, status);
            assertEquals(affixPattern + " " + cas.exactMatch, sampleParseableString.length(), result.charEnd);
        }
    }
}

void NumberParserTest::test20360_BidiOverflow() {
    IcuTestErrorCode status(*this, "test20360_BidiOverflow");
    UnicodeString inputString;
    inputString.append(u'-');
    for (int32_t i=0; i<100000; i++) {
        inputString.append(u'\u061C');
    }
    inputString.append(u'5');

    LocalPointer<const NumberParserImpl> parser(NumberParserImpl::createSimpleParser("en", u"0", 0, status));
    if (status.errDataIfFailureAndReset("createSimpleParser() failed")) {
        return;
    }

    ParsedNumber resultObject;
    parser->parse(inputString, true, resultObject, status);
    assertTrue("Greedy Parse, success", resultObject.success());
    assertEquals("Greedy Parse, chars consumed", 100002, resultObject.charEnd);
    assertEquals("Greedy Parse, expected double", -5.0, resultObject.getDouble(status));

    resultObject.clear();
    parser->parse(inputString, false, resultObject, status);
    assertFalse("Non-Greedy Parse, success", resultObject.success());
    assertEquals("Non-Greedy Parse, chars consumed", 1, resultObject.charEnd);
}

void NumberParserTest::testInfiniteRecursion() {
    IcuTestErrorCode status(*this, "testInfiniteRecursion");
    UnicodeString inputString;
    inputString.append(u'-');
    for (int32_t i=0; i<200; i++) {
        inputString.append(u'\u061C');
    }
    inputString.append(u'5');

    LocalPointer<const NumberParserImpl> parser(NumberParserImpl::createSimpleParser("en", u"0", 0, status));
    if (status.errDataIfFailureAndReset("createSimpleParser() failed")) {
        return;
    }

    ParsedNumber resultObject;
    parser->parse(inputString, false, resultObject, status);
    assertFalse("Default recursion limit, success", resultObject.success());
    assertEquals("Default recursion limit, chars consumed", 1, resultObject.charEnd);

    parser.adoptInstead(NumberParserImpl::createSimpleParser(
        "en", u"0", PARSE_FLAG_ALLOW_INFINITE_RECURSION, status));
    resultObject.clear();
    parser->parse(inputString, false, resultObject, status);
    assertTrue("Unlimited recursion, success", resultObject.success());
    assertEquals("Unlimited recursion, chars consumed", 202, resultObject.charEnd);
    assertEquals("Unlimited recursion, expected double", -5.0, resultObject.getDouble(status));
}


#endif
