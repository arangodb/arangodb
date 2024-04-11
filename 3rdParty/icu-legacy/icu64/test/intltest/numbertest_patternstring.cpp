// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "numbertest.h"
#include "number_patternstring.h"

void PatternStringTest::runIndexedTest(int32_t index, UBool exec, const char*& name, char*) {
    if (exec) {
        logln("TestSuite PatternStringTest: ");
    }
    TESTCASE_AUTO_BEGIN;
        TESTCASE_AUTO(testLocalized);
        TESTCASE_AUTO(testToPatternSimple);
        TESTCASE_AUTO(testExceptionOnInvalid);
        TESTCASE_AUTO(testBug13117);
    TESTCASE_AUTO_END;
}

void PatternStringTest::testLocalized() {
    IcuTestErrorCode status(*this, "testLocalized");
    DecimalFormatSymbols symbols(Locale::getEnglish(), status);
    if (status.isFailure()) { return; }
    symbols.setSymbol(DecimalFormatSymbols::kDecimalSeparatorSymbol, u"a", status);
    symbols.setSymbol(DecimalFormatSymbols::kPercentSymbol, u"b", status);
    symbols.setSymbol(DecimalFormatSymbols::kMinusSignSymbol, u".", status);
    symbols.setSymbol(DecimalFormatSymbols::kPlusSignSymbol, u"'", status);

    UnicodeString standard = u"+-abcb''a''#,##0.0%'a%'";
    UnicodeString localized = u"’.'ab'c'b''a'''#,##0a0b'a%'";
    UnicodeString toStandard = u"+-'ab'c'b''a'''#,##0.0%'a%'";

    assertEquals(
            "standard to localized",
            localized,
            PatternStringUtils::convertLocalized(standard, symbols, true, status));
    assertEquals(
            "localized to standard",
            toStandard,
            PatternStringUtils::convertLocalized(localized, symbols, false, status));
}

void PatternStringTest::testToPatternSimple() {
    const char16_t* cases[][2] = {{u"#", u"0"},
                                  {u"0", u"0"},
                                  {u"#0", u"0"},
                                  {u"###", u"0"},
                                  {u"0.##", u"0.##"},
                                  {u"0.00", u"0.00"},
                                  {u"0.00#", u"0.00#"},
                                  {u"0.05", u"0.05"},
                                  {u"#E0", u"#E0"},
                                  {u"0E0", u"0E0"},
                                  {u"#00E00", u"#00E00"},
                                  {u"#,##0", u"#,##0"},
                                  {u"#;#", u"0;0"},
            // ignore a negative prefix pattern of '-' since that is the default:
                                  {u"#;-#", u"0"},
                                  {u"pp#,000;(#)", u"pp#,000;(#,000)"},
                                  {u"**##0", u"**##0"},
                                  {u"*'x'##0", u"*x##0"},
                                  {u"a''b0", u"a''b0"},
                                  {u"*''##0", u"*''##0"},
                                  {u"*📺##0", u"*'📺'##0"},
                                  {u"*'நி'##0", u"*'நி'##0"},};

    UErrorCode status = U_ZERO_ERROR;
    for (const char16_t** cas : cases) {
        UnicodeString input(cas[0]);
        UnicodeString output(cas[1]);

        DecimalFormatProperties properties = PatternParser::parseToProperties(
                input, IGNORE_ROUNDING_NEVER, status);
        assertSuccess(input, status);
        UnicodeString actual = PatternStringUtils::propertiesToPatternString(properties, status);
        assertEquals(input, output, actual);
    }
}

void PatternStringTest::testExceptionOnInvalid() {
    static const char16_t* invalidPatterns[] = {
            u"#.#.#",
            u"0#",
            u"0#.",
            u".#0",
            u"0#.#0",
            u"@0",
            u"0@",
            u"0,",
            u"0,,",
            u"0,,0",
            u"0,,0,",
            u"#,##0E0"};

    for (auto pattern : invalidPatterns) {
        UErrorCode status = U_ZERO_ERROR;
        ParsedPatternInfo patternInfo;
        PatternParser::parseToPatternInfo(pattern, patternInfo, status);
        assertTrue(pattern, U_FAILURE(status));
    }
}

void PatternStringTest::testBug13117() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatProperties expected = PatternParser::parseToProperties(
            u"0", IGNORE_ROUNDING_NEVER, status);
    DecimalFormatProperties actual = PatternParser::parseToProperties(
            u"0;", IGNORE_ROUNDING_NEVER, status);
    assertSuccess("Spot 1", status);
    assertTrue("Should not consume negative subpattern", expected == actual);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
