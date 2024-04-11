// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "putilimp.h"
#include "intltest.h"
#include "number_stringbuilder.h"
#include "number_modifiers.h"
#include "numbertest.h"

void ModifiersTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite ModifiersTest: ");
    }
    TESTCASE_AUTO_BEGIN;
        TESTCASE_AUTO(testConstantAffixModifier);
        TESTCASE_AUTO(testConstantMultiFieldModifier);
        TESTCASE_AUTO(testSimpleModifier);
        TESTCASE_AUTO(testCurrencySpacingEnabledModifier);
    TESTCASE_AUTO_END;
}

void ModifiersTest::testConstantAffixModifier() {
    UErrorCode status = U_ZERO_ERROR;
    ConstantAffixModifier mod0(u"", u"", UNUM_PERCENT_FIELD, true);
    assertModifierEquals(mod0, 0, true, u"|", u"n", status);
    assertSuccess("Spot 1", status);

    ConstantAffixModifier mod1(u"a📻", u"b", UNUM_PERCENT_FIELD, true);
    assertModifierEquals(mod1, 3, true, u"a📻|b", u"%%%n%", status);
    assertSuccess("Spot 2", status);
}

void ModifiersTest::testConstantMultiFieldModifier() {
    UErrorCode status = U_ZERO_ERROR;
    NumberStringBuilder prefix;
    NumberStringBuilder suffix;
    ConstantMultiFieldModifier mod1(prefix, suffix, false, true);
    assertModifierEquals(mod1, 0, true, u"|", u"n", status);
    assertSuccess("Spot 1", status);

    prefix.append(u"a📻", UNUM_PERCENT_FIELD, status);
    suffix.append(u"b", UNUM_CURRENCY_FIELD, status);
    ConstantMultiFieldModifier mod2(prefix, suffix, false, true);
    assertModifierEquals(mod2, 3, true, u"a📻|b", u"%%%n$", status);
    assertSuccess("Spot 2", status);

    // Make sure the first modifier is still the same (that it stayed constant)
    assertModifierEquals(mod1, 0, true, u"|", u"n", status);
    assertSuccess("Spot 3", status);
}

void ModifiersTest::testSimpleModifier() {
    static const int32_t NUM_CASES = 5;
    static const int32_t NUM_OUTPUTS = 4;
    static const char16_t *patterns[] = {u"{0}", u"X{0}Y", u"XX{0}YYY", u"{0}YY", u"XX📺XX{0}"};
    static const struct {
        const char16_t *baseString;
        int32_t leftIndex;
        int32_t rightIndex;
    } outputs[NUM_OUTPUTS] = {{u"", 0, 0}, {u"a📻bcde", 0, 0}, {u"a📻bcde", 4, 4}, {u"a📻bcde", 3, 5}};
    static const int32_t prefixLens[] = {0, 1, 2, 0, 6};
    static const char16_t *expectedCharFields[][2] = {{u"|", u"n"},
                                                      {u"X|Y", u"%n%"},
                                                      {u"XX|YYY", u"%%n%%%"},
                                                      {u"|YY", u"n%%"},
                                                      {u"XX📺XX|", u"%%%%%%n"}};
    static const char16_t *expecteds[][NUM_CASES] = // force auto-format line break
            {{
                     u"", u"XY", u"XXYYY", u"YY", u"XX📺XX"}, {
                     u"a📻bcde", u"XYa📻bcde", u"XXYYYa📻bcde", u"YYa📻bcde", u"XX📺XXa📻bcde"}, {
                     u"a📻bcde", u"a📻bXYcde", u"a📻bXXYYYcde", u"a📻bYYcde", u"a📻bXX📺XXcde"}, {
                     u"a📻bcde", u"a📻XbcYde", u"a📻XXbcYYYde", u"a📻bcYYde", u"a📻XX📺XXbcde"}};

    UErrorCode status = U_ZERO_ERROR;
    for (int32_t i = 0; i < NUM_CASES; i++) {
        const UnicodeString pattern(patterns[i]);
        SimpleFormatter compiledFormatter(pattern, 1, 1, status);
        assertSuccess("Spot 1", status);
        SimpleModifier mod(compiledFormatter, UNUM_PERCENT_FIELD, false);
        assertModifierEquals(
                mod, prefixLens[i], false, expectedCharFields[i][0], expectedCharFields[i][1], status);
        assertSuccess("Spot 2", status);

        // Test strange insertion positions
        for (int32_t j = 0; j < NUM_OUTPUTS; j++) {
            NumberStringBuilder output;
            output.append(outputs[j].baseString, UNUM_FIELD_COUNT, status);
            mod.apply(output, outputs[j].leftIndex, outputs[j].rightIndex, status);
            UnicodeString expected = expecteds[j][i];
            UnicodeString actual = output.toUnicodeString();
            assertEquals("Strange insertion position", expected, actual);
            assertSuccess("Spot 3", status);
        }
    }
}

void ModifiersTest::testCurrencySpacingEnabledModifier() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalFormatSymbols symbols(Locale("en"), status);
    if (!assertSuccess("Spot 1", status, true)) {
        return;
    }

    NumberStringBuilder prefix;
    NumberStringBuilder suffix;
    CurrencySpacingEnabledModifier mod1(prefix, suffix, false, true, symbols, status);
    assertSuccess("Spot 2", status);
    assertModifierEquals(mod1, 0, true, u"|", u"n", status);
    assertSuccess("Spot 3", status);

    prefix.append(u"USD", UNUM_CURRENCY_FIELD, status);
    assertSuccess("Spot 4", status);
    CurrencySpacingEnabledModifier mod2(prefix, suffix, false, true, symbols, status);
    assertSuccess("Spot 5", status);
    assertModifierEquals(mod2, 3, true, u"USD|", u"$$$n", status);
    assertSuccess("Spot 6", status);

    // Test the default currency spacing rules
    NumberStringBuilder sb;
    sb.append("123", UNUM_INTEGER_FIELD, status);
    assertSuccess("Spot 7", status);
    NumberStringBuilder sb1(sb);
    assertModifierEquals(mod2, sb1, 3, true, u"USD\u00A0123", u"$$$niii", status);
    assertSuccess("Spot 8", status);

    // Compare with the unsafe code path
    NumberStringBuilder sb2(sb);
    sb2.insert(0, "USD", UNUM_CURRENCY_FIELD, status);
    assertSuccess("Spot 9", status);
    CurrencySpacingEnabledModifier::applyCurrencySpacing(sb2, 0, 3, 6, 0, symbols, status);
    assertSuccess("Spot 10", status);
    assertTrue(sb1.toDebugString() + " vs " + sb2.toDebugString(), sb1.contentEquals(sb2));

    // Test custom patterns
    // The following line means that the last char of the number should be a | (rather than a digit)
    symbols.setPatternForCurrencySpacing(UNUM_CURRENCY_SURROUNDING_MATCH, true, u"[|]");
    suffix.append("XYZ", UNUM_CURRENCY_FIELD, status);
    assertSuccess("Spot 11", status);
    CurrencySpacingEnabledModifier mod3(prefix, suffix, false, true, symbols, status);
    assertSuccess("Spot 12", status);
    assertModifierEquals(mod3, 3, true, u"USD|\u00A0XYZ", u"$$$nn$$$", status);
    assertSuccess("Spot 13", status);
}

void ModifiersTest::assertModifierEquals(const Modifier &mod, int32_t expectedPrefixLength,
                                         bool expectedStrong, UnicodeString expectedChars,
                                         UnicodeString expectedFields, UErrorCode &status) {
    NumberStringBuilder sb;
    sb.appendCodePoint('|', UNUM_FIELD_COUNT, status);
    assertModifierEquals(
            mod, sb, expectedPrefixLength, expectedStrong, expectedChars, expectedFields, status);

}

void ModifiersTest::assertModifierEquals(const Modifier &mod, NumberStringBuilder &sb,
                                         int32_t expectedPrefixLength, bool expectedStrong,
                                         UnicodeString expectedChars, UnicodeString expectedFields,
                                         UErrorCode &status) {
    int32_t oldCount = sb.codePointCount();
    mod.apply(sb, 0, sb.length(), status);
    assertEquals("Prefix length", expectedPrefixLength, mod.getPrefixLength());
    assertEquals("Strong", expectedStrong, mod.isStrong());
    if (dynamic_cast<const CurrencySpacingEnabledModifier*>(&mod) == nullptr) {
        // i.e., if mod is not a CurrencySpacingEnabledModifier
        assertEquals("Code point count equals actual code point count",
                sb.codePointCount() - oldCount, mod.getCodePointCount());
    }

    UnicodeString debugString;
    debugString.append(u"<NumberStringBuilder [");
    debugString.append(expectedChars);
    debugString.append(u"] [");
    debugString.append(expectedFields);
    debugString.append(u"]>");
    assertEquals("Debug string", debugString, sb.toDebugString());
}

#endif /* #if !UCONFIG_NO_FORMATTING */
