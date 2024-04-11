// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "number_decimalquantity.h"
#include "number_decnum.h"
#include "math.h"
#include <cmath>
#include "number_utils.h"
#include "numbertest.h"

void DecimalQuantityTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite DecimalQuantityTest: ");
    }
    TESTCASE_AUTO_BEGIN;
        TESTCASE_AUTO(testDecimalQuantityBehaviorStandalone);
        TESTCASE_AUTO(testSwitchStorage);
        TESTCASE_AUTO(testCopyMove);
        TESTCASE_AUTO(testAppend);
        if (!quick) {
            // Slow test: run in exhaustive mode only
            TESTCASE_AUTO(testConvertToAccurateDouble);
        }
        TESTCASE_AUTO(testUseApproximateDoubleWhenAble);
        TESTCASE_AUTO(testHardDoubleConversion);
        TESTCASE_AUTO(testFitsInLong);
        TESTCASE_AUTO(testToDouble);
        TESTCASE_AUTO(testMaxDigits);
        TESTCASE_AUTO(testNickelRounding);
        TESTCASE_AUTO(testScientificAndCompactSuppressedExponent);
        TESTCASE_AUTO(testSuppressedExponentUnchangedByInitialScaling);
        TESTCASE_AUTO(testDecimalQuantityParseFormatRoundTrip);
    TESTCASE_AUTO_END;
}

void DecimalQuantityTest::assertDoubleEquals(UnicodeString message, double a, double b) {
    if (a == b) {
        return;
    }

    double diff = a - b;
    diff = diff < 0 ? -diff : diff;
    double bound = a < 0 ? -a * 1e-6 : a * 1e-6;
    if (diff > bound) {
        errln(message + u": " + DoubleToUnicodeString(a) + u" vs " + DoubleToUnicodeString(b) + u" differ by " + DoubleToUnicodeString(diff));
    }
}

void DecimalQuantityTest::assertHealth(const DecimalQuantity &fq) {
    const char16_t* health = fq.checkHealth();
    if (health != nullptr) {
        errln(UnicodeString(u"HEALTH FAILURE: ") + UnicodeString(health) + u": " + fq.toString());
    }
}

void
DecimalQuantityTest::assertToStringAndHealth(const DecimalQuantity &fq, const UnicodeString &expected) {
    UnicodeString actual = fq.toString();
    assertEquals("DecimalQuantity toString failed", expected, actual);
    assertHealth(fq);
}

void DecimalQuantityTest::checkDoubleBehavior(double d, bool explicitRequired) {
    DecimalQuantity fq;
    fq.setToDouble(d);
    if (explicitRequired) {
        assertTrue("Should be using approximate double", !fq.isExplicitExactDouble());
    }
    UnicodeString baseStr = fq.toString();
    fq.roundToInfinity();
    UnicodeString newStr = fq.toString();
    if (explicitRequired) {
        assertTrue("Should not be using approximate double", fq.isExplicitExactDouble());
    }
    assertDoubleEquals(
        UnicodeString(u"After conversion to exact BCD (double): ") + baseStr + u" vs " + newStr,
        d, fq.toDouble());
}

void DecimalQuantityTest::testDecimalQuantityBehaviorStandalone() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalQuantity fq;
    assertToStringAndHealth(fq, u"<DecimalQuantity 0:0 long 0E0>");
    fq.setToInt(51423);
    assertToStringAndHealth(fq, u"<DecimalQuantity 0:0 long 51423E0>");
    fq.adjustMagnitude(-3);
    assertToStringAndHealth(fq, u"<DecimalQuantity 0:0 long 51423E-3>");

    fq.setToLong(90909090909000L);
    assertToStringAndHealth(fq, u"<DecimalQuantity 0:0 long 90909090909E3>");
    fq.increaseMinIntegerTo(2);
    fq.applyMaxInteger(5);
    assertToStringAndHealth(fq, u"<DecimalQuantity 2:0 long 9E3>");
    fq.setMinFraction(3);
    assertToStringAndHealth(fq, u"<DecimalQuantity 2:-3 long 9E3>");

    fq.setToDouble(987.654321);
    assertToStringAndHealth(fq, u"<DecimalQuantity 2:-3 long 987654321E-6>");
    fq.roundToInfinity();
    assertToStringAndHealth(fq, u"<DecimalQuantity 2:-3 long 987654321E-6>");
    fq.roundToIncrement(4, -3, RoundingMode::UNUM_ROUND_HALFEVEN, status);
    assertSuccess("Rounding to increment", status);
    assertToStringAndHealth(fq, u"<DecimalQuantity 2:-3 long 987656E-3>");
    fq.roundToNickel(-3, RoundingMode::UNUM_ROUND_HALFEVEN, status);
    assertSuccess("Rounding to nickel", status);
    assertToStringAndHealth(fq, u"<DecimalQuantity 2:-3 long 987655E-3>");
    fq.roundToMagnitude(-2, RoundingMode::UNUM_ROUND_HALFEVEN, status);
    assertSuccess("Rounding to magnitude", status);
    assertToStringAndHealth(fq, u"<DecimalQuantity 2:-3 long 98766E-2>");
}

void DecimalQuantityTest::testSwitchStorage() {
    UErrorCode status = U_ZERO_ERROR;
    DecimalQuantity fq;

    fq.setToLong(1234123412341234L);
    assertFalse("Should not be using byte array", fq.isUsingBytes());
    assertEquals("Failed on initialize", u"1.234123412341234E+15", fq.toScientificString());
    assertHealth(fq);
    // Long -> Bytes
    fq.appendDigit(5, 0, true);
    assertTrue("Should be using byte array", fq.isUsingBytes());
    assertEquals("Failed on multiply", u"1.2341234123412345E+16", fq.toScientificString());
    assertHealth(fq);
    // Bytes -> Long
    fq.roundToMagnitude(5, RoundingMode::UNUM_ROUND_HALFEVEN, status);
    assertSuccess("Rounding to magnitude", status);
    assertFalse("Should not be using byte array", fq.isUsingBytes());
    assertEquals("Failed on round", u"1.23412341234E+16", fq.toScientificString());
    assertHealth(fq);
    // Bytes with popFromLeft
    fq.setToDecNumber({"999999999999999999"}, status);
    assertToStringAndHealth(fq, u"<DecimalQuantity 0:0 bytes 999999999999999999E0>");
    fq.applyMaxInteger(17);
    assertToStringAndHealth(fq, u"<DecimalQuantity 0:0 bytes 99999999999999999E0>");
    fq.applyMaxInteger(16);
    assertToStringAndHealth(fq, u"<DecimalQuantity 0:0 long 9999999999999999E0>");
    fq.applyMaxInteger(15);
    assertToStringAndHealth(fq, u"<DecimalQuantity 0:0 long 999999999999999E0>");
}

void DecimalQuantityTest::testCopyMove() {
    // Small numbers (fits in BCD long)
    {
        DecimalQuantity a;
        a.setToLong(1234123412341234L);
        DecimalQuantity b = a; // copy constructor
        assertToStringAndHealth(a, u"<DecimalQuantity 0:0 long 1234123412341234E0>");
        assertToStringAndHealth(b, u"<DecimalQuantity 0:0 long 1234123412341234E0>");
        DecimalQuantity c(std::move(a)); // move constructor
        assertToStringAndHealth(c, u"<DecimalQuantity 0:0 long 1234123412341234E0>");
        c.setToLong(54321L);
        assertToStringAndHealth(c, u"<DecimalQuantity 0:0 long 54321E0>");
        c = b; // copy assignment
        assertToStringAndHealth(b, u"<DecimalQuantity 0:0 long 1234123412341234E0>");
        assertToStringAndHealth(c, u"<DecimalQuantity 0:0 long 1234123412341234E0>");
        b.setToLong(45678);
        c.setToLong(56789);
        c = std::move(b); // move assignment
        assertToStringAndHealth(c, u"<DecimalQuantity 0:0 long 45678E0>");
        a = std::move(c); // move assignment to a defunct object
        assertToStringAndHealth(a, u"<DecimalQuantity 0:0 long 45678E0>");
    }

    // Large numbers (requires byte allocation)
    {
        IcuTestErrorCode status(*this, "testCopyMove");
        DecimalQuantity a;
        a.setToDecNumber({"1234567890123456789", -1}, status);
        DecimalQuantity b = a; // copy constructor
        assertToStringAndHealth(a, u"<DecimalQuantity 0:0 bytes 1234567890123456789E0>");
        assertToStringAndHealth(b, u"<DecimalQuantity 0:0 bytes 1234567890123456789E0>");
        DecimalQuantity c(std::move(a)); // move constructor
        assertToStringAndHealth(c, u"<DecimalQuantity 0:0 bytes 1234567890123456789E0>");
        c.setToDecNumber({"9876543210987654321", -1}, status);
        assertToStringAndHealth(c, u"<DecimalQuantity 0:0 bytes 9876543210987654321E0>");
        c = b; // copy assignment
        assertToStringAndHealth(b, u"<DecimalQuantity 0:0 bytes 1234567890123456789E0>");
        assertToStringAndHealth(c, u"<DecimalQuantity 0:0 bytes 1234567890123456789E0>");
        b.setToDecNumber({"876543210987654321", -1}, status);
        c.setToDecNumber({"987654321098765432", -1}, status);
        c = std::move(b); // move assignment
        assertToStringAndHealth(c, u"<DecimalQuantity 0:0 bytes 876543210987654321E0>");
        a = std::move(c); // move assignment to a defunct object
        assertToStringAndHealth(a, u"<DecimalQuantity 0:0 bytes 876543210987654321E0>");
    }
}

void DecimalQuantityTest::testAppend() {
    DecimalQuantity fq;
    fq.appendDigit(1, 0, true);
    assertEquals("Failed on append", u"1E+0", fq.toScientificString());
    assertHealth(fq);
    fq.appendDigit(2, 0, true);
    assertEquals("Failed on append", u"1.2E+1", fq.toScientificString());
    assertHealth(fq);
    fq.appendDigit(3, 1, true);
    assertEquals("Failed on append", u"1.203E+3", fq.toScientificString());
    assertHealth(fq);
    fq.appendDigit(0, 1, true);
    assertEquals("Failed on append", u"1.203E+5", fq.toScientificString());
    assertHealth(fq);
    fq.appendDigit(4, 0, true);
    assertEquals("Failed on append", u"1.203004E+6", fq.toScientificString());
    assertHealth(fq);
    fq.appendDigit(0, 0, true);
    assertEquals("Failed on append", u"1.203004E+7", fq.toScientificString());
    assertHealth(fq);
    fq.appendDigit(5, 0, false);
    assertEquals("Failed on append", u"1.20300405E+7", fq.toScientificString());
    assertHealth(fq);
    fq.appendDigit(6, 0, false);
    assertEquals("Failed on append", u"1.203004056E+7", fq.toScientificString());
    assertHealth(fq);
    fq.appendDigit(7, 3, false);
    assertEquals("Failed on append", u"1.2030040560007E+7", fq.toScientificString());
    assertHealth(fq);
    UnicodeString baseExpected(u"1.2030040560007");
    for (int i = 0; i < 10; i++) {
        fq.appendDigit(8, 0, false);
        baseExpected.append(u'8');
        UnicodeString expected(baseExpected);
        expected.append(u"E+7");
        assertEquals("Failed on append", expected, fq.toScientificString());
        assertHealth(fq);
    }
    fq.appendDigit(9, 2, false);
    baseExpected.append(u"009");
    UnicodeString expected(baseExpected);
    expected.append(u"E+7");
    assertEquals("Failed on append", expected, fq.toScientificString());
    assertHealth(fq);
}

void DecimalQuantityTest::testConvertToAccurateDouble() {
    // based on https://github.com/google/double-conversion/issues/28
    static double hardDoubles[] = {
            1651087494906221570.0,
            2.207817077636718750000000000000,
            1.818351745605468750000000000000,
            3.941719055175781250000000000000,
            3.738609313964843750000000000000,
            3.967735290527343750000000000000,
            1.328025817871093750000000000000,
            3.920967102050781250000000000000,
            1.015235900878906250000000000000,
            1.335227966308593750000000000000,
            1.344520568847656250000000000000,
            2.879127502441406250000000000000,
            3.695838928222656250000000000000,
            1.845344543457031250000000000000,
            3.793952941894531250000000000000,
            3.211402893066406250000000000000,
            2.565971374511718750000000000000,
            0.965156555175781250000000000000,
            2.700004577636718750000000000000,
            0.767097473144531250000000000000,
            1.780448913574218750000000000000,
            2.624839782714843750000000000000,
            1.305290222167968750000000000000,
            3.834922790527343750000000000000,};

    static double exactDoubles[] = {
            51423,
            51423e10,
            -5074790912492772E-327,
            83602530019752571E-327,
            4.503599627370496E15,
            6.789512076111555E15,
            9.007199254740991E15,
            9.007199254740992E15};

    for (double d : hardDoubles) {
        checkDoubleBehavior(d, true);
    }

    for (double d : exactDoubles) {
        checkDoubleBehavior(d, false);
    }

    assertDoubleEquals(u"NaN check failed", NAN, DecimalQuantity().setToDouble(NAN).toDouble());
    assertDoubleEquals(
            u"Inf check failed", INFINITY, DecimalQuantity().setToDouble(INFINITY).toDouble());
    assertDoubleEquals(
            u"-Inf check failed", -INFINITY, DecimalQuantity().setToDouble(-INFINITY).toDouble());

    // Generate random doubles
    for (int32_t i = 0; i < 10000; i++) {
        uint8_t bytes[8];
        for (int32_t j = 0; j < 8; j++) {
            bytes[j] = static_cast<uint8_t>(rand() % 256);
        }
        double d;
        uprv_memcpy(&d, bytes, 8);
        if (std::isnan(d) || !std::isfinite(d)) { continue; }
        checkDoubleBehavior(d, false);
    }
}

void DecimalQuantityTest::testUseApproximateDoubleWhenAble() {
    static const struct TestCase {
        double d;
        int32_t maxFrac;
        RoundingMode roundingMode;
        bool usesExact;
    } cases[] = {{1.2345678, 1, RoundingMode::UNUM_ROUND_HALFEVEN, false},
                 {1.2345678, 7, RoundingMode::UNUM_ROUND_HALFEVEN, false},
                 {1.2345678, 12, RoundingMode::UNUM_ROUND_HALFEVEN, false},
                 {1.2345678, 13, RoundingMode::UNUM_ROUND_HALFEVEN, true},
                 {1.235, 1, RoundingMode::UNUM_ROUND_HALFEVEN, false},
                 {1.235, 2, RoundingMode::UNUM_ROUND_HALFEVEN, true},
                 {1.235, 3, RoundingMode::UNUM_ROUND_HALFEVEN, false},
                 {1.000000000000001, 0, RoundingMode::UNUM_ROUND_HALFEVEN, false},
                 {1.000000000000001, 0, RoundingMode::UNUM_ROUND_CEILING, true},
                 {1.235, 1, RoundingMode::UNUM_ROUND_CEILING, false},
                 {1.235, 2, RoundingMode::UNUM_ROUND_CEILING, false},
                 {1.235, 3, RoundingMode::UNUM_ROUND_CEILING, true}};

    UErrorCode status = U_ZERO_ERROR;
    for (const TestCase& cas : cases) {
        DecimalQuantity fq;
        fq.setToDouble(cas.d);
        assertTrue("Should be using approximate double", !fq.isExplicitExactDouble());
        fq.roundToMagnitude(-cas.maxFrac, cas.roundingMode, status);
        assertSuccess("Rounding to magnitude", status);
        if (cas.usesExact != fq.isExplicitExactDouble()) {
            errln(UnicodeString(u"Using approximate double after rounding: ") + fq.toString());
        }
    }
}

void DecimalQuantityTest::testHardDoubleConversion() {
    static const struct TestCase {
        double input;
        const char16_t* expectedOutput;
    } cases[] = {
            { 512.0000000000017, u"512.0000000000017" },
            { 4095.9999999999977, u"4095.9999999999977" },
            { 4095.999999999998, u"4095.999999999998" },
            { 4095.9999999999986, u"4095.9999999999986" },
            { 4095.999999999999, u"4095.999999999999" },
            { 4095.9999999999995, u"4095.9999999999995" },
            { 4096.000000000001, u"4096.000000000001" },
            { 4096.000000000002, u"4096.000000000002" },
            { 4096.000000000003, u"4096.000000000003" },
            { 4096.000000000004, u"4096.000000000004" },
            { 4096.000000000005, u"4096.000000000005" },
            { 4096.0000000000055, u"4096.0000000000055" },
            { 4096.000000000006, u"4096.000000000006" },
            { 4096.000000000007, u"4096.000000000007" } };

    for (const auto& cas : cases) {
        DecimalQuantity q;
        q.setToDouble(cas.input);
        q.roundToInfinity();
        UnicodeString actualOutput = q.toPlainString();
        assertEquals("", cas.expectedOutput, actualOutput);
    }
}

void DecimalQuantityTest::testFitsInLong() {
    IcuTestErrorCode status(*this, "testFitsInLong");
    DecimalQuantity quantity;
    quantity.setToInt(0);
    assertTrue("Zero should fit", quantity.fitsInLong());
    quantity.setToInt(42);
    assertTrue("Small int should fit", quantity.fitsInLong());
    quantity.setToDouble(0.1);
    assertFalse("Fraction should not fit", quantity.fitsInLong());
    quantity.setToDouble(42.1);
    assertFalse("Fraction should not fit", quantity.fitsInLong());
    quantity.setToLong(1000000);
    assertTrue("Large low-precision int should fit", quantity.fitsInLong());
    quantity.setToLong(1000000000000000000L);
    assertTrue("10^19 should fit", quantity.fitsInLong());
    quantity.setToLong(1234567890123456789L);
    assertTrue("A number between 10^19 and max long should fit", quantity.fitsInLong());
    quantity.setToLong(1234567890000000000L);
    assertTrue("A number with trailing zeros less than max long should fit", quantity.fitsInLong());
    quantity.setToLong(9223372026854775808L);
    assertTrue("A number less than max long but with similar digits should fit",
            quantity.fitsInLong());
    quantity.setToLong(9223372036854775806L);
    assertTrue("One less than max long should fit", quantity.fitsInLong());
    quantity.setToLong(9223372036854775807L);
    assertTrue("Max long should fit", quantity.fitsInLong());
    assertEquals("Max long should equal toLong", 9223372036854775807L, quantity.toLong(false));
    quantity.setToDecNumber("9223372036854775808", status);
    assertFalse("One greater than max long should not fit", quantity.fitsInLong());
    assertEquals("toLong(true) should truncate", 223372036854775808L, quantity.toLong(true));
    quantity.setToDecNumber("9223372046854775806", status);
    assertFalse("A number between max long and 10^20 should not fit", quantity.fitsInLong());
    quantity.setToDecNumber("9223372046800000000", status);
    assertFalse("A large 10^19 number with trailing zeros should not fit", quantity.fitsInLong());
    quantity.setToDecNumber("10000000000000000000", status);
    assertFalse("10^20 should not fit", quantity.fitsInLong());
}

void DecimalQuantityTest::testToDouble() {
    IcuTestErrorCode status(*this, "testToDouble");
    static const struct TestCase {
        const char* input; // char* for the decNumber constructor
        double expected;
    } cases[] = {
            { "0", 0.0 },
            { "514.23", 514.23 },
            { "-3.142E-271", -3.142e-271 } };

    for (const auto& cas : cases) {
        status.setScope(cas.input);
        DecimalQuantity q;
        q.setToDecNumber({cas.input, -1}, status);
        double actual = q.toDouble();
        assertEquals("Doubles should exactly equal", cas.expected, actual);
    }
}

void DecimalQuantityTest::testMaxDigits() {
    IcuTestErrorCode status(*this, "testMaxDigits");
    DecimalQuantity dq;
    dq.setToDouble(876.543);
    dq.roundToInfinity();
    dq.increaseMinIntegerTo(0);
    dq.applyMaxInteger(2);
    dq.setMinFraction(0);
    dq.roundToMagnitude(-2, UNUM_ROUND_FLOOR, status);
    assertEquals("Should trim, toPlainString", "76.54", dq.toPlainString());
    assertEquals("Should trim, toScientificString", "7.654E+1", dq.toScientificString());
    assertEquals("Should trim, toLong", 76LL, dq.toLong(true));
    assertEquals("Should trim, toFractionLong", (int64_t) 54, (int64_t) dq.toFractionLong(false));
    assertEquals("Should trim, toDouble", 76.54, dq.toDouble());
    // To test DecNum output, check the round-trip.
    DecNum dn;
    dq.toDecNum(dn, status);
    DecimalQuantity copy;
    copy.setToDecNum(dn, status);
    assertEquals("Should trim, toDecNum", "76.54", copy.toPlainString());
}

void DecimalQuantityTest::testNickelRounding() {
    IcuTestErrorCode status(*this, "testNickelRounding");
    struct TestCase {
        double input;
        int32_t magnitude;
        UNumberFormatRoundingMode roundingMode;
        const char16_t* expected;
    } cases[] = {
        {1.000, -2, UNUM_ROUND_HALFEVEN, u"1"},
        {1.001, -2, UNUM_ROUND_HALFEVEN, u"1"},
        {1.010, -2, UNUM_ROUND_HALFEVEN, u"1"},
        {1.020, -2, UNUM_ROUND_HALFEVEN, u"1"},
        {1.024, -2, UNUM_ROUND_HALFEVEN, u"1"},
        {1.025, -2, UNUM_ROUND_HALFEVEN, u"1"},
        {1.025, -2, UNUM_ROUND_HALFDOWN, u"1"},
        {1.025, -2, UNUM_ROUND_HALF_ODD, u"1.05"},
        {1.025, -2, UNUM_ROUND_HALF_CEILING, u"1.05"},
        {1.025, -2, UNUM_ROUND_HALF_FLOOR, u"1"},
        {1.025, -2, UNUM_ROUND_HALFUP,   u"1.05"},
        {1.026, -2, UNUM_ROUND_HALFEVEN, u"1.05"},
        {1.030, -2, UNUM_ROUND_HALFEVEN, u"1.05"},
        {1.040, -2, UNUM_ROUND_HALFEVEN, u"1.05"},
        {1.050, -2, UNUM_ROUND_HALFEVEN, u"1.05"},
        {1.060, -2, UNUM_ROUND_HALFEVEN, u"1.05"},
        {1.070, -2, UNUM_ROUND_HALFEVEN, u"1.05"},
        {1.074, -2, UNUM_ROUND_HALFEVEN, u"1.05"},
        {1.075, -2, UNUM_ROUND_HALFDOWN, u"1.05"},
        {1.075, -2, UNUM_ROUND_HALF_ODD, u"1.05"},
        {1.075, -2, UNUM_ROUND_HALF_CEILING, u"1.1"},
        {1.075, -2, UNUM_ROUND_HALF_FLOOR, u"1.05"},
        {1.075, -2, UNUM_ROUND_HALFUP,   u"1.1"},
        {1.075, -2, UNUM_ROUND_HALFEVEN, u"1.1"},
        {1.076, -2, UNUM_ROUND_HALFEVEN, u"1.1"},
        {1.080, -2, UNUM_ROUND_HALFEVEN, u"1.1"},
        {1.090, -2, UNUM_ROUND_HALFEVEN, u"1.1"},
        {1.099, -2, UNUM_ROUND_HALFEVEN, u"1.1"},
        {1.999, -2, UNUM_ROUND_HALFEVEN, u"2"},
        {2.25, -1, UNUM_ROUND_HALFEVEN, u"2"},
        {2.25, -1, UNUM_ROUND_HALFUP,   u"2.5"},
        {2.75, -1, UNUM_ROUND_HALFDOWN, u"2.5"},
        {2.75, -1, UNUM_ROUND_HALF_ODD, u"2.5"},
        {2.75, -1, UNUM_ROUND_HALF_CEILING, u"3"},
        {2.75, -1, UNUM_ROUND_HALF_FLOOR, u"2.5"},
        {2.75, -1, UNUM_ROUND_HALFEVEN, u"3"},
        {3.00, -1, UNUM_ROUND_CEILING, u"3"},
        {3.25, -1, UNUM_ROUND_CEILING, u"3.5"},
        {3.50, -1, UNUM_ROUND_CEILING, u"3.5"},
        {3.75, -1, UNUM_ROUND_CEILING, u"4"},
        {4.00, -1, UNUM_ROUND_FLOOR, u"4"},
        {4.25, -1, UNUM_ROUND_FLOOR, u"4"},
        {4.50, -1, UNUM_ROUND_FLOOR, u"4.5"},
        {4.75, -1, UNUM_ROUND_FLOOR, u"4.5"},
        {5.00, -1, UNUM_ROUND_UP, u"5"},
        {5.25, -1, UNUM_ROUND_UP, u"5.5"},
        {5.50, -1, UNUM_ROUND_UP, u"5.5"},
        {5.75, -1, UNUM_ROUND_UP, u"6"},
        {6.00, -1, UNUM_ROUND_DOWN, u"6"},
        {6.25, -1, UNUM_ROUND_DOWN, u"6"},
        {6.50, -1, UNUM_ROUND_DOWN, u"6.5"},
        {6.75, -1, UNUM_ROUND_DOWN, u"6.5"},
        {7.00, -1, UNUM_ROUND_UNNECESSARY, u"7"},
        {7.50, -1, UNUM_ROUND_UNNECESSARY, u"7.5"},
    };
    for (const auto& cas : cases) {
        UnicodeString message = DoubleToUnicodeString(cas.input) + u" @ " + Int64ToUnicodeString(cas.magnitude) + u" / " + Int64ToUnicodeString(cas.roundingMode);
        status.setScope(message);
        DecimalQuantity dq;
        dq.setToDouble(cas.input);
        dq.roundToNickel(cas.magnitude, cas.roundingMode, status);
        status.errIfFailureAndReset();
        UnicodeString actual = dq.toPlainString();
        assertEquals(message, cas.expected, actual);
    }
    status.setScope("");
    DecimalQuantity dq;
    dq.setToDouble(7.1);
    dq.roundToNickel(-1, UNUM_ROUND_UNNECESSARY, status);
    status.expectErrorAndReset(U_FORMAT_INEXACT_ERROR);
}

void DecimalQuantityTest::testScientificAndCompactSuppressedExponent() {
    IcuTestErrorCode status(*this, "testScientificAndCompactSuppressedExponent");
    Locale ulocale("fr-FR");

    struct TestCase {
        UnicodeString skeleton;
        double input;
        const char16_t* expectedString;
        int64_t expectedLong;
        double expectedDouble;
        const char16_t* expectedPlainString;
        int32_t expectedSuppressedScientificExponent;
        int32_t expectedSuppressedCompactExponent;
    } cases[] = {
        // unlocalized formatter skeleton, input, string output, long output,
        // double output, BigDecimal output, plain string,
        // suppressed scientific exponent, suppressed compact exponent
        {u"",              123456789, u"123 456 789",  123456789L, 123456789.0, u"123456789", 0, 0},
        {u"compact-long",  123456789, u"123 millions", 123000000L, 123000000.0, u"123000000", 6, 6},
        {u"compact-short", 123456789, u"123 M",        123000000L, 123000000.0, u"123000000", 6, 6},
        {u"scientific",    123456789, u"1,234568E8",   123456800L, 123456800.0, u"123456800", 8, 8},

        {u"",              1234567, u"1 234 567",   1234567L, 1234567.0, u"1234567", 0, 0},
        {u"compact-long",  1234567, u"1,2 million", 1200000L, 1200000.0, u"1200000", 6, 6},
        {u"compact-short", 1234567, u"1,2 M",       1200000L, 1200000.0, u"1200000", 6, 6},
        {u"scientific",    1234567, u"1,234567E6",  1234567L, 1234567.0, u"1234567", 6, 6},

        {u"",              123456, u"123 456",   123456L, 123456.0, u"123456", 0, 0},
        {u"compact-long",  123456, u"123 mille", 123000L, 123000.0, u"123000", 3, 3},
        {u"compact-short", 123456, u"123 k",     123000L, 123000.0, u"123000", 3, 3},
        {u"scientific",    123456, u"1,23456E5", 123456L, 123456.0, u"123456", 5, 5},

        {u"",              123, u"123",    123L, 123.0, u"123", 0, 0},
        {u"compact-long",  123, u"123",    123L, 123.0, u"123", 0, 0},
        {u"compact-short", 123, u"123",    123L, 123.0, u"123", 0, 0},
        {u"scientific",    123, u"1,23E2", 123L, 123.0, u"123", 2, 2},

        {u"",              1.2, u"1,2",   1L, 1.2, u"1.2", 0, 0},
        {u"compact-long",  1.2, u"1,2",   1L, 1.2, u"1.2", 0, 0},
        {u"compact-short", 1.2, u"1,2",   1L, 1.2, u"1.2", 0, 0},
        {u"scientific",    1.2, u"1,2E0", 1L, 1.2, u"1.2", 0, 0},

        {u"",              0.12, u"0,12",   0L, 0.12, u"0.12",  0,  0},
        {u"compact-long",  0.12, u"0,12",   0L, 0.12, u"0.12",  0,  0},
        {u"compact-short", 0.12, u"0,12",   0L, 0.12, u"0.12",  0,  0},
        {u"scientific",    0.12, u"1,2E-1", 0L, 0.12, u"0.12", -1, -1},

        {u"",              0.012, u"0,012",   0L, 0.012, u"0.012",  0,  0},
        {u"compact-long",  0.012, u"0,012",   0L, 0.012, u"0.012",  0,  0},
        {u"compact-short", 0.012, u"0,012",   0L, 0.012, u"0.012",  0,  0},
        {u"scientific",    0.012, u"1,2E-2",  0L, 0.012, u"0.012", -2, -2},

        {u"",              999.9, u"999,9",     999L,  999.9,  u"999.9", 0, 0},
        {u"compact-long",  999.9, u"mille",     1000L, 1000.0, u"1000",  3, 3},
        {u"compact-short", 999.9, u"1 k",       1000L, 1000.0, u"1000",  3, 3},
        {u"scientific",    999.9, u"9,999E2",   999L,  999.9,  u"999.9", 2, 2},

        {u"",              1000.0, u"1 000",     1000L, 1000.0, u"1000", 0, 0},
        {u"compact-long",  1000.0, u"mille",     1000L, 1000.0, u"1000", 3, 3},
        {u"compact-short", 1000.0, u"1 k",       1000L, 1000.0, u"1000", 3, 3},
        {u"scientific",    1000.0, u"1E3",       1000L, 1000.0, u"1000", 3, 3},
    };
    for (const auto& cas : cases) {
        // test the helper methods used to compute plural operand values

        LocalizedNumberFormatter formatter =
            NumberFormatter::forSkeleton(cas.skeleton, status)
              .locale(ulocale);
        FormattedNumber fn = formatter.formatDouble(cas.input, status);
        DecimalQuantity dq;
        fn.getDecimalQuantity(dq, status);
        UnicodeString actualString = fn.toString(status);
        int64_t actualLong = dq.toLong();
        double actualDouble = dq.toDouble();
        UnicodeString actualPlainString = dq.toPlainString();
        int32_t actualSuppressedScientificExponent = dq.getExponent();
        int32_t actualSuppressedCompactExponent = dq.getExponent();

        assertEquals(
                u"formatted number " + cas.skeleton + u" toString: " + cas.input,
                cas.expectedString,
                actualString);
        assertEquals(
                u"formatted number " + cas.skeleton + u" toLong: " + cas.input,
                cas.expectedLong,
                actualLong);
        assertDoubleEquals(
                u"formatted number " + cas.skeleton + u" toDouble: " + cas.input,
                cas.expectedDouble,
                actualDouble);
        assertEquals(
                u"formatted number " + cas.skeleton + u" toPlainString: " + cas.input,
                cas.expectedPlainString,
                actualPlainString);
        assertEquals(
                u"formatted number " + cas.skeleton + u" suppressed scientific exponent: " + cas.input,
                cas.expectedSuppressedScientificExponent,
                actualSuppressedScientificExponent);
        assertEquals(
                u"formatted number " + cas.skeleton + u" suppressed compact exponent: " + cas.input,
                cas.expectedSuppressedCompactExponent,
                actualSuppressedCompactExponent);

        // test the actual computed values of the plural operands

        double expectedNOperand = cas.expectedDouble;
        double expectedIOperand = static_cast<double>(cas.expectedLong);
        double expectedEOperand = cas.expectedSuppressedScientificExponent;
        double expectedCOperand = cas.expectedSuppressedCompactExponent;
        double actualNOperand = dq.getPluralOperand(PLURAL_OPERAND_N);
        double actualIOperand = dq.getPluralOperand(PLURAL_OPERAND_I);
        double actualEOperand = dq.getPluralOperand(PLURAL_OPERAND_E);
        double actualCOperand = dq.getPluralOperand(PLURAL_OPERAND_C);

        assertDoubleEquals(
                u"formatted number " + cas.skeleton + u" n operand: " + cas.input,
                expectedNOperand,
                actualNOperand);
        assertDoubleEquals(
                u"formatted number " + cas.skeleton + u" i operand: " + cas.input,
                expectedIOperand,
                actualIOperand);
        assertDoubleEquals(
                u"formatted number " + cas.skeleton + " e operand: " + cas.input,
                expectedEOperand,
                actualEOperand);
        assertDoubleEquals(
                u"formatted number " + cas.skeleton + " c operand: " + cas.input,
                expectedCOperand,
                actualCOperand);
    }
}

void DecimalQuantityTest::testSuppressedExponentUnchangedByInitialScaling() {
    IcuTestErrorCode status(*this, "testSuppressedExponentUnchangedByInitialScaling");
    Locale ulocale("fr-FR");
    LocalizedNumberFormatter withLocale = NumberFormatter::withLocale(ulocale);
    LocalizedNumberFormatter compactLong =
        withLocale.notation(Notation::compactLong());
    LocalizedNumberFormatter compactScaled =
        compactLong.scale(Scale::powerOfTen(3));
    
    struct TestCase {
        int32_t input;
        UnicodeString expectedString;
        double expectedNOperand;
        double expectedIOperand;
        double expectedEOperand;
        double expectedCOperand;
    } cases[] = {
        // input, compact long string output,
        // compact n operand, compact i operand, compact e operand,
        // compact c operand
        {123456789, "123 millions", 123000000.0, 123000000.0, 6.0, 6.0},
        {1234567,   "1,2 million",  1200000.0,   1200000.0,   6.0, 6.0},
        {123456,    "123 mille",    123000.0,    123000.0,    3.0, 3.0},
        {123,       "123",          123.0,       123.0,       0.0, 0.0},
    };

    for (const auto& cas : cases) {
        FormattedNumber fnCompactScaled = compactScaled.formatInt(cas.input, status);
        DecimalQuantity dqCompactScaled;
        fnCompactScaled.getDecimalQuantity(dqCompactScaled, status);
        double compactScaledCOperand = dqCompactScaled.getPluralOperand(PLURAL_OPERAND_C);

        FormattedNumber fnCompact = compactLong.formatInt(cas.input, status);
        DecimalQuantity dqCompact;
        fnCompact.getDecimalQuantity(dqCompact, status);
        UnicodeString actualString = fnCompact.toString(status);
        double compactNOperand = dqCompact.getPluralOperand(PLURAL_OPERAND_N);
        double compactIOperand = dqCompact.getPluralOperand(PLURAL_OPERAND_I);
        double compactEOperand = dqCompact.getPluralOperand(PLURAL_OPERAND_E);
        double compactCOperand = dqCompact.getPluralOperand(PLURAL_OPERAND_C);
        assertEquals(
                u"formatted number " + Int64ToUnicodeString(cas.input) + " compactLong toString: ",
                cas.expectedString,
                actualString);
        assertDoubleEquals(
                u"compact decimal " + DoubleToUnicodeString(cas.input) + ", n operand vs. expected",
                cas.expectedNOperand,
                compactNOperand);
        assertDoubleEquals(
                u"compact decimal " + DoubleToUnicodeString(cas.input) + ", i operand vs. expected",
                cas.expectedIOperand,
                compactIOperand);
        assertDoubleEquals(
                u"compact decimal " + DoubleToUnicodeString(cas.input) + ", e operand vs. expected",
                cas.expectedEOperand,
                compactEOperand);
        assertDoubleEquals(
                u"compact decimal " + DoubleToUnicodeString(cas.input) + ", c operand vs. expected",
                cas.expectedCOperand,
                compactCOperand);

        // By scaling by 10^3 in a locale that has words / compact notation
        // based on powers of 10^3, we guarantee that the suppressed
        // exponent will differ by 3.
        assertDoubleEquals(
                u"decimal " + DoubleToUnicodeString(cas.input) + ", c operand for compact vs. compact scaled",
                compactCOperand + 3,
                compactScaledCOperand);
    }
}


void DecimalQuantityTest::testDecimalQuantityParseFormatRoundTrip() {
    IcuTestErrorCode status(*this, "testDecimalQuantityParseFormatRoundTrip");
    
    struct TestCase {
        UnicodeString numberString;
    } cases[] = {
        // number string
        { u"0" },
        { u"1" },
        { u"1.0" },
        { u"1.00" },
        { u"1.1" },
        { u"1.10" },
        { u"-1.10" },
        { u"0.0" },
        { u"1c5" },
        { u"1.0c5" },
        { u"1.1c5" },
        { u"1.10c5" },
        { u"0.00" },
        { u"0.1" },
        { u"1c-1" },
        { u"1.0c-1" }
    };

    for (const auto& cas : cases) {
        UnicodeString numberString = cas.numberString;

        DecimalQuantity dq = DecimalQuantity::fromExponentString(numberString, status);
        UnicodeString roundTrip = dq.toExponentString();

        assertEquals("DecimalQuantity format(parse(s)) should equal original s", numberString, roundTrip);
    }

    DecimalQuantity dq = DecimalQuantity::fromExponentString(u"1c0", status);
    assertEquals("Zero ignored for visible exponent",
                u"1",
                dq.toExponentString());

    dq.clear();
    dq = DecimalQuantity::fromExponentString(u"1.0c0", status);
    assertEquals("Zero ignored for visible exponent",
                u"1.0",
                dq.toExponentString());

}

#endif /* #if !UCONFIG_NO_FORMATTING */
