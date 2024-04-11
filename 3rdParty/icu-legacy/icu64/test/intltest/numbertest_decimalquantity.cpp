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
        TESTCASE_AUTO(testSwitchStorage);;
        TESTCASE_AUTO(testCopyMove);
        TESTCASE_AUTO(testAppend);
        if (!quick) {
            // Slow test: run in exhaustive mode only
            TESTCASE_AUTO(testConvertToAccurateDouble);
        }
        TESTCASE_AUTO(testUseApproximateDoubleWhenAble);
        TESTCASE_AUTO(testHardDoubleConversion);
        TESTCASE_AUTO(testToDouble);
        TESTCASE_AUTO(testMaxDigits);
        TESTCASE_AUTO(testNickelRounding);
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
    fq.setMinInteger(2);
    fq.applyMaxInteger(5);
    assertToStringAndHealth(fq, u"<DecimalQuantity 2:0 long 9E3>");
    fq.setMinFraction(3);
    assertToStringAndHealth(fq, u"<DecimalQuantity 2:-3 long 9E3>");

    fq.setToDouble(987.654321);
    assertToStringAndHealth(fq, u"<DecimalQuantity 2:-3 long 987654321E-6>");
    fq.roundToInfinity();
    assertToStringAndHealth(fq, u"<DecimalQuantity 2:-3 long 987654321E-6>");
    fq.roundToIncrement(0.005, RoundingMode::UNUM_ROUND_HALFEVEN, status);
    assertSuccess("Rounding to increment", status);
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
            -5074790912492772E-327,
            83602530019752571E-327,
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

    static double integerDoubles[] = {
            51423,
            51423e10,
            4.503599627370496E15,
            6.789512076111555E15,
            9.007199254740991E15,
            9.007199254740992E15};

    for (double d : hardDoubles) {
        checkDoubleBehavior(d, true);
    }

    for (double d : integerDoubles) {
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
    for (TestCase cas : cases) {
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

    for (auto& cas : cases) {
        DecimalQuantity q;
        q.setToDouble(cas.input);
        q.roundToInfinity();
        UnicodeString actualOutput = q.toPlainString();
        assertEquals("", cas.expectedOutput, actualOutput);
    }
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

    for (auto& cas : cases) {
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
    dq.setMinInteger(0);
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
        {1.025, -2, UNUM_ROUND_HALFUP,   u"1.05"},
        {1.026, -2, UNUM_ROUND_HALFEVEN, u"1.05"},
        {1.030, -2, UNUM_ROUND_HALFEVEN, u"1.05"},
        {1.040, -2, UNUM_ROUND_HALFEVEN, u"1.05"},
        {1.050, -2, UNUM_ROUND_HALFEVEN, u"1.05"},
        {1.060, -2, UNUM_ROUND_HALFEVEN, u"1.05"},
        {1.070, -2, UNUM_ROUND_HALFEVEN, u"1.05"},
        {1.074, -2, UNUM_ROUND_HALFEVEN, u"1.05"},
        {1.075, -2, UNUM_ROUND_HALFDOWN, u"1.05"},
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

#endif /* #if !UCONFIG_NO_FORMATTING */
