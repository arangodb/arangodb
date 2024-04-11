// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "numbertest.h"
#include "number_microprops.h"
#include "number_patternmodifier.h"

void PatternModifierTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite PatternModifierTest: ");
    }
    TESTCASE_AUTO_BEGIN;
        TESTCASE_AUTO(testBasic);
        TESTCASE_AUTO(testPatternWithNoPlaceholder);
        TESTCASE_AUTO(testMutableEqualsImmutable);
    TESTCASE_AUTO_END;
}

void PatternModifierTest::testBasic() {
    UErrorCode status = U_ZERO_ERROR;
    MutablePatternModifier mod(false);
    ParsedPatternInfo patternInfo;
    PatternParser::parseToPatternInfo(u"a0b", patternInfo, status);
    assertSuccess("Spot 1", status);
    mod.setPatternInfo(&patternInfo, kUndefinedField);
    mod.setPatternAttributes(UNUM_SIGN_AUTO, false, false);
    DecimalFormatSymbols symbols(Locale::getEnglish(), status);
    mod.setSymbols(&symbols, {u"USD", status}, UNUM_UNIT_WIDTH_SHORT, nullptr, status);
    if (!assertSuccess("Spot 2", status, true)) {
        return;
    }

    mod.setNumberProperties(SIGNUM_POS, StandardPlural::Form::COUNT);
    assertEquals("Pattern a0b", u"a", getPrefix(mod, status));
    assertEquals("Pattern a0b", u"b", getSuffix(mod, status));
    mod.setPatternAttributes(UNUM_SIGN_ALWAYS, false, false);
    assertEquals("Pattern a0b", u"+a", getPrefix(mod, status));
    assertEquals("Pattern a0b", u"b", getSuffix(mod, status));
    mod.setNumberProperties(SIGNUM_NEG_ZERO, StandardPlural::Form::COUNT);
    assertEquals("Pattern a0b", u"-a", getPrefix(mod, status));
    assertEquals("Pattern a0b", u"b", getSuffix(mod, status));
    mod.setNumberProperties(SIGNUM_POS_ZERO, StandardPlural::Form::COUNT);
    assertEquals("Pattern a0b", u"+a", getPrefix(mod, status));
    assertEquals("Pattern a0b", u"b", getSuffix(mod, status));
    mod.setPatternAttributes(UNUM_SIGN_EXCEPT_ZERO, false, false);
    assertEquals("Pattern a0b", u"a", getPrefix(mod, status));
    assertEquals("Pattern a0b", u"b", getSuffix(mod, status));
    mod.setNumberProperties(SIGNUM_NEG, StandardPlural::Form::COUNT);
    assertEquals("Pattern a0b", u"-a", getPrefix(mod, status));
    assertEquals("Pattern a0b", u"b", getSuffix(mod, status));
    mod.setPatternAttributes(UNUM_SIGN_NEVER, false, false);
    assertEquals("Pattern a0b", u"a", getPrefix(mod, status));
    assertEquals("Pattern a0b", u"b", getSuffix(mod, status));
    assertSuccess("Spot 3", status);

    mod.setPatternAttributes(UNUM_SIGN_AUTO, false, true);
    mod.setNumberProperties(SIGNUM_POS, StandardPlural::Form::COUNT);
    assertEquals("Pattern a0b", u"~a", getPrefix(mod, status));
    assertEquals("Pattern a0b", u"b", getSuffix(mod, status));
    mod.setNumberProperties(SIGNUM_NEG, StandardPlural::Form::COUNT);
    assertEquals("Pattern a0b", u"~-a", getPrefix(mod, status));
    assertEquals("Pattern a0b", u"b", getSuffix(mod, status));
    mod.setPatternAttributes(UNUM_SIGN_ALWAYS, false, true);
    mod.setNumberProperties(SIGNUM_POS, StandardPlural::Form::COUNT);
    assertEquals("Pattern a0b", u"~+a", getPrefix(mod, status));
    assertEquals("Pattern a0b", u"b", getSuffix(mod, status));
    assertSuccess("Spot 3.5", status);

    ParsedPatternInfo patternInfo2;
    PatternParser::parseToPatternInfo(u"a0b;c-0d", patternInfo2, status);
    assertSuccess("Spot 4", status);
    mod.setPatternInfo(&patternInfo2, kUndefinedField);
    mod.setPatternAttributes(UNUM_SIGN_AUTO, false, false);
    mod.setNumberProperties(SIGNUM_POS, StandardPlural::Form::COUNT);
    assertEquals("Pattern a0b;c-0d", u"a", getPrefix(mod, status));
    assertEquals("Pattern a0b;c-0d", u"b", getSuffix(mod, status));
    mod.setPatternAttributes(UNUM_SIGN_ALWAYS, false, false);
    assertEquals("Pattern a0b;c-0d", u"c+", getPrefix(mod, status));
    assertEquals("Pattern a0b;c-0d", u"d", getSuffix(mod, status));
    mod.setNumberProperties(SIGNUM_NEG_ZERO, StandardPlural::Form::COUNT);
    assertEquals("Pattern a0b;c-0d", u"c-", getPrefix(mod, status));
    assertEquals("Pattern a0b;c-0d", u"d", getSuffix(mod, status));
    mod.setNumberProperties(SIGNUM_POS_ZERO, StandardPlural::Form::COUNT);
    assertEquals("Pattern a0b;c-0d", u"c+", getPrefix(mod, status));
    assertEquals("Pattern a0b;c-0d", u"d", getSuffix(mod, status));
    mod.setPatternAttributes(UNUM_SIGN_EXCEPT_ZERO, false, false);
    assertEquals("Pattern a0b;c-0d", u"a", getPrefix(mod, status));
    assertEquals("Pattern a0b;c-0d", u"b", getSuffix(mod, status));
    mod.setNumberProperties(SIGNUM_NEG, StandardPlural::Form::COUNT);
    assertEquals("Pattern a0b;c-0d", u"c-", getPrefix(mod, status));
    assertEquals("Pattern a0b;c-0d", u"d", getSuffix(mod, status));
    mod.setPatternAttributes(UNUM_SIGN_NEVER, false, false);
    assertEquals("Pattern a0b;c-0d", u"a", getPrefix(mod, status));
    assertEquals("Pattern a0b;c-0d", u"b", getSuffix(mod, status));
    assertSuccess("Spot 5", status);

    mod.setPatternAttributes(UNUM_SIGN_AUTO, false, true);
    mod.setNumberProperties(SIGNUM_POS, StandardPlural::Form::COUNT);
    assertEquals("Pattern a0b;c-0d", u"c~", getPrefix(mod, status));
    assertEquals("Pattern a0b;c-0d", u"d", getSuffix(mod, status));
    mod.setNumberProperties(SIGNUM_NEG, StandardPlural::Form::COUNT);
    assertEquals("Pattern a0b;c-0d", u"c~-", getPrefix(mod, status));
    assertEquals("Pattern a0b;c-0d", u"d", getSuffix(mod, status));
    mod.setPatternAttributes(UNUM_SIGN_ALWAYS, false, true);
    mod.setNumberProperties(SIGNUM_POS, StandardPlural::Form::COUNT);
    assertEquals("Pattern a0b;c-0d", u"c~+", getPrefix(mod, status));
    assertEquals("Pattern a0b;c-0d", u"d", getSuffix(mod, status));
    assertSuccess("Spot 5.5", status);
}

void PatternModifierTest::testPatternWithNoPlaceholder() {
    UErrorCode status = U_ZERO_ERROR;
    MutablePatternModifier mod(false);
    ParsedPatternInfo patternInfo;
    PatternParser::parseToPatternInfo(u"abc", patternInfo, status);
    assertSuccess("Spot 1", status);
    mod.setPatternInfo(&patternInfo, kUndefinedField);
    mod.setPatternAttributes(UNUM_SIGN_AUTO, false, false);
    DecimalFormatSymbols symbols(Locale::getEnglish(), status);
    mod.setSymbols(&symbols, {u"USD", status}, UNUM_UNIT_WIDTH_SHORT, nullptr, status);
    if (!assertSuccess("Spot 2", status, true)) {
        return;
    }
    mod.setNumberProperties(SIGNUM_POS, StandardPlural::Form::COUNT);

    // Unsafe Code Path
    FormattedStringBuilder nsb;
    nsb.append(u"x123y", kUndefinedField, status);
    assertSuccess("Spot 3", status);
    mod.apply(nsb, 1, 4, status);
    assertSuccess("Spot 4", status);
    assertEquals("Unsafe Path", u"xabcy", nsb.toUnicodeString());

    // Safe Code Path
    nsb.clear();
    nsb.append(u"x123y", kUndefinedField, status);
    assertSuccess("Spot 5", status);
    MicroProps micros;
    LocalPointer<ImmutablePatternModifier> imod(mod.createImmutable(status), status);
    if (U_FAILURE(status)) {
      dataerrln("%s %d  Error in ImmutablePatternModifier creation",
                __FILE__, __LINE__);
      assertSuccess("Spot 6", status);
      return;
    }
    DecimalQuantity quantity;
    imod->applyToMicros(micros, quantity, status);
    micros.modMiddle->apply(nsb, 1, 4, status);
    assertSuccess("Spot 7", status);
    assertEquals("Safe Path", u"xabcy", nsb.toUnicodeString());
}

void PatternModifierTest::testMutableEqualsImmutable() {
    UErrorCode status = U_ZERO_ERROR;
    MutablePatternModifier mod(false);
    ParsedPatternInfo patternInfo;
    PatternParser::parseToPatternInfo("a0b;c-0d", patternInfo, status);
    assertSuccess("Spot 1", status);
    mod.setPatternInfo(&patternInfo, kUndefinedField);
    mod.setPatternAttributes(UNUM_SIGN_AUTO, false, false);
    DecimalFormatSymbols symbols(Locale::getEnglish(), status);
    mod.setSymbols(&symbols, {u"USD", status}, UNUM_UNIT_WIDTH_SHORT, nullptr, status);
    assertSuccess("Spot 2", status);
    if (U_FAILURE(status)) { return; }
    DecimalQuantity fq;
    fq.setToInt(1);

    FormattedStringBuilder nsb1;
    MicroProps micros1;
    mod.addToChain(&micros1);
    mod.processQuantity(fq, micros1, status);
    micros1.modMiddle->apply(nsb1, 0, 0, status);
    assertSuccess("Spot 3", status);

    FormattedStringBuilder nsb2;
    MicroProps micros2;
    LocalPointer<ImmutablePatternModifier> immutable(mod.createImmutable(status));
    immutable->applyToMicros(micros2, fq, status);
    micros2.modMiddle->apply(nsb2, 0, 0, status);
    assertSuccess("Spot 4", status);

    FormattedStringBuilder nsb3;
    MicroProps micros3;
    mod.addToChain(&micros3);
    mod.setPatternAttributes(UNUM_SIGN_ALWAYS, false, false);
    mod.processQuantity(fq, micros3, status);
    micros3.modMiddle->apply(nsb3, 0, 0, status);
    assertSuccess("Spot 5", status);

    assertTrue(nsb1.toUnicodeString() + " vs " + nsb2.toUnicodeString(), nsb1.contentEquals(nsb2));
    assertFalse(nsb1.toUnicodeString() + " vs " + nsb3.toUnicodeString(), nsb1.contentEquals(nsb3));
}

UnicodeString PatternModifierTest::getPrefix(const MutablePatternModifier &mod, UErrorCode &status) {
    FormattedStringBuilder nsb;
    mod.apply(nsb, 0, 0, status);
    int32_t prefixLength = mod.getPrefixLength();
    return UnicodeString(nsb.toUnicodeString(), 0, prefixLength);
}

UnicodeString PatternModifierTest::getSuffix(const MutablePatternModifier &mod, UErrorCode &status) {
    FormattedStringBuilder nsb;
    mod.apply(nsb, 0, 0, status);
    int32_t prefixLength = mod.getPrefixLength();
    return UnicodeString(nsb.toUnicodeString(), prefixLength, nsb.length() - prefixLength);
}

#endif /* #if !UCONFIG_NO_FORMATTING */
