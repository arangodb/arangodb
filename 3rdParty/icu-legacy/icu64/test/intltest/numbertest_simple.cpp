// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include <cmath>

#include "numbertest.h"
#include "unicode/simplenumberformatter.h"


void SimpleNumberFormatterTest::runIndexedTest(int32_t index, UBool exec, const char*& name, char*) {
    if (exec) {
        logln("TestSuite SimpleNumberFormatterTest: ");
    }
    TESTCASE_AUTO_BEGIN;
        TESTCASE_AUTO(testBasic);
        TESTCASE_AUTO(testWithOptions);
        TESTCASE_AUTO(testDigits);
        TESTCASE_AUTO(testSymbols);
        TESTCASE_AUTO(testSign);
        TESTCASE_AUTO(testCopyMove);
        TESTCASE_AUTO(testCAPI);
    TESTCASE_AUTO_END;
}

void SimpleNumberFormatterTest::testBasic() {
    IcuTestErrorCode status(*this, "testBasic");

    SimpleNumberFormatter snf = SimpleNumberFormatter::forLocale("de-CH", status);
    FormattedNumber result = snf.formatInt64(-1000007, status);

    static const UFieldPosition expectedFieldPositions[] = {
        // field, begin index, end index
        {UNUM_SIGN_FIELD, 0, 1},
        {UNUM_GROUPING_SEPARATOR_FIELD, 2, 3},
        {UNUM_GROUPING_SEPARATOR_FIELD, 6, 7},
        {UNUM_INTEGER_FIELD, 1, 10},
    };
    checkFormattedValue(
        u"testBasic",
        result,
        u"-1’000’007",
        UFIELD_CATEGORY_NUMBER,
        expectedFieldPositions,
        UPRV_LENGTHOF(expectedFieldPositions));
}

void SimpleNumberFormatterTest::testWithOptions() {
    IcuTestErrorCode status(*this, "testWithOptions");

    SimpleNumber num = SimpleNumber::forInt64(1250000, status);
    num.setMaximumIntegerDigits(6, status);
    num.setMinimumIntegerDigits(6, status);
    num.setMinimumFractionDigits(2, status);
    num.multiplyByPowerOfTen(-2, status);
    num.roundTo(3, UNUM_ROUND_HALFUP, status);
    SimpleNumberFormatter snf = SimpleNumberFormatter::forLocale("bn", status);
    FormattedNumber result = snf.format(std::move(num), status);

    static const UFieldPosition expectedFieldPositions[] = {
        // field, begin index, end index
        {UNUM_GROUPING_SEPARATOR_FIELD, 1, 2},
        {UNUM_GROUPING_SEPARATOR_FIELD, 4, 5},
        {UNUM_INTEGER_FIELD, 0, 8},
        {UNUM_DECIMAL_SEPARATOR_FIELD, 8, 9},
        {UNUM_FRACTION_FIELD, 9, 11},
    };
    checkFormattedValue(
        u"testWithOptions",
        result,
        u"০,০৩,০০০.০০",
        UFIELD_CATEGORY_NUMBER,
        expectedFieldPositions,
        UPRV_LENGTHOF(expectedFieldPositions));
}

void SimpleNumberFormatterTest::testDigits() {
    IcuTestErrorCode status(*this, "testDigits");

    SimpleNumberFormatter snf = SimpleNumberFormatter::forLocaleAndGroupingStrategy(
        "en-US",
        UNUM_GROUPING_ON_ALIGNED,
        status
    );

    {
        SimpleNumber num = SimpleNumber::forInt64(918273645, status);
        num.multiplyByPowerOfTen(-4, status);
        auto result = snf.format(std::move(num), status);
        assertEquals("no digit options",
            u"91,827.3645",
            result.toTempString(status));
    }

    {
        SimpleNumber num = SimpleNumber::forInt64(918273645, status);
        num.multiplyByPowerOfTen(-4, status);
        num.setMaximumIntegerDigits(4, status);
        auto result = snf.format(std::move(num), status);
        assertEquals("setMaximumIntegerDigits",
            u"1,827.3645",
            result.toTempString(status));
    }

    {
        SimpleNumber num = SimpleNumber::forInt64(918273645, status);
        num.multiplyByPowerOfTen(-4, status);
        num.setMaximumIntegerDigits(8, status);
        auto result = snf.format(std::move(num), status);
        assertEquals("bigger setMaximumIntegerDigits",
            u"91,827.3645",
            result.toTempString(status));
    }

    {
        SimpleNumber num = SimpleNumber::forInt64(918273645, status);
        num.multiplyByPowerOfTen(-4, status);
        num.setMinimumIntegerDigits(6, status);
        auto result = snf.format(std::move(num), status);
        assertEquals("setMinimumIntegerDigits",
            u"091,827.3645",
            result.toTempString(status));
    }

    {
        SimpleNumber num = SimpleNumber::forInt64(918273645, status);
        num.multiplyByPowerOfTen(-4, status);
        num.setMinimumIntegerDigits(8, status);
        num.setMinimumIntegerDigits(6, status);
        auto result = snf.format(std::move(num), status);
        assertEquals("double setMinimumIntegerDigits",
            u"091,827.3645",
            result.toTempString(status));
    }

    {
        SimpleNumber num = SimpleNumber::forInt64(918273645, status);
        num.multiplyByPowerOfTen(-4, status);
        num.setMaximumIntegerDigits(4, status);
        num.setMinimumIntegerDigits(6, status);
        auto result = snf.format(std::move(num), status);
        assertEquals("setMaximumIntegerDigits setMinimumIntegerDigits",
            u"001,827.3645",
            result.toTempString(status));
    }

    {
        SimpleNumber num = SimpleNumber::forInt64(918273645, status);
        num.multiplyByPowerOfTen(-4, status);
        num.setMinimumIntegerDigits(6, status);
        num.setMaximumIntegerDigits(4, status);
        auto result = snf.format(std::move(num), status);
        assertEquals("setMinimumIntegerDigits setMaximumIntegerDigits",
            u"1,827.3645",
            result.toTempString(status));
    }

    {
        SimpleNumber num = SimpleNumber::forInt64(918273645, status);
        num.multiplyByPowerOfTen(-4, status);
        num.setMaximumIntegerDigits(8, status);
        num.setMinimumIntegerDigits(6, status);
        auto result = snf.format(std::move(num), status);
        assertEquals("bigger setMaximumIntegerDigits setMinimumIntegerDigits",
            u"091,827.3645",
            result.toTempString(status));
    }

    {
        SimpleNumber num = SimpleNumber::forInt64(918273645, status);
        num.multiplyByPowerOfTen(-4, status);
        num.roundTo(-1, UNUM_ROUND_HALFEVEN, status);
        auto result = snf.format(std::move(num), status);
        assertEquals("roundTo",
            u"91,827.4",
            result.toTempString(status));
    }

    {
        SimpleNumber num = SimpleNumber::forInt64(918273645, status);
        num.multiplyByPowerOfTen(-4, status);
        num.setMinimumFractionDigits(5, status);
        auto result = snf.format(std::move(num), status);
        assertEquals("setMinimumFractionDigits",
            u"91,827.36450",
            result.toTempString(status));
    }

    {
        SimpleNumber num = SimpleNumber::forInt64(918273645, status);
        num.multiplyByPowerOfTen(-4, status);
        num.setMinimumFractionDigits(6, status);
        num.setMinimumFractionDigits(5, status);
        auto result = snf.format(std::move(num), status);
        assertEquals("double setMinimumFractionDigits",
            u"91,827.36450",
            result.toTempString(status));
    }

    {
        SimpleNumber num = SimpleNumber::forInt64(918273645, status);
        num.multiplyByPowerOfTen(-4, status);
        num.roundTo(-1, UNUM_ROUND_HALFEVEN, status);
        num.setMinimumFractionDigits(5, status);
        auto result = snf.format(std::move(num), status);
        assertEquals("roundTo setMinimumFractionDigits",
            u"91,827.40000",
            result.toTempString(status));
    }

    {
        SimpleNumber num = SimpleNumber::forInt64(918273645, status);
        num.multiplyByPowerOfTen(-4, status);
        num.setMinimumFractionDigits(5, status);
        num.roundTo(-1, UNUM_ROUND_HALFEVEN, status);
        auto result = snf.format(std::move(num), status);
        assertEquals("setMinimumFractionDigits roundTo",
            u"91,827.40000",
            result.toTempString(status));
    }
}

void SimpleNumberFormatterTest::testSymbols() {
    IcuTestErrorCode status(*this, "testSymbols");

    LocalPointer<DecimalFormatSymbols> symbols(new DecimalFormatSymbols("bn", status), status);
    SimpleNumberFormatter snf = SimpleNumberFormatter::forLocaleAndSymbolsAndGroupingStrategy(
        "en-US",
        *symbols,
        UNUM_GROUPING_ON_ALIGNED,
        status
    );
    auto result = snf.formatInt64(987654321, status);

    assertEquals("bn symbols with en-US pattern",
        u"৯৮৭,৬৫৪,৩২১",
        result.toTempString(status));
}

void SimpleNumberFormatterTest::testSign() {
    IcuTestErrorCode status(*this, "testSign");

    SimpleNumberFormatter snf = SimpleNumberFormatter::forLocale("und", status);

    struct TestCase {
        int64_t input;
        USimpleNumberSign sign;
        const char16_t* expected;
    } cases[] = {
        { 1, UNUM_SIMPLE_NUMBER_NO_SIGN, u"1" },
        { 1, UNUM_SIMPLE_NUMBER_PLUS_SIGN, u"+1" },
        { 1, UNUM_SIMPLE_NUMBER_MINUS_SIGN, u"-1" },
        { 0, UNUM_SIMPLE_NUMBER_NO_SIGN, u"0" },
        { 0, UNUM_SIMPLE_NUMBER_PLUS_SIGN, u"+0" },
        { 0, UNUM_SIMPLE_NUMBER_MINUS_SIGN, u"-0" },
        { -1, UNUM_SIMPLE_NUMBER_NO_SIGN, u"1" },
        { -1, UNUM_SIMPLE_NUMBER_PLUS_SIGN, u"+1" },
        { -1, UNUM_SIMPLE_NUMBER_MINUS_SIGN, u"-1" },
    };
    for (auto& cas : cases) {
        SimpleNumber num = SimpleNumber::forInt64(cas.input, status);
        num.setSign(cas.sign, status);
        auto result = snf.format(std::move(num), status);
        assertEquals("", cas.expected, result.toTempString(status));
    }
}

void SimpleNumberFormatterTest::testCopyMove() {
    IcuTestErrorCode status(*this, "testCopyMove");

    SimpleNumberFormatter snf0 = SimpleNumberFormatter::forLocale("und", status);

    SimpleNumber sn0 = SimpleNumber::forInt64(55, status);
    SimpleNumber sn1 = std::move(sn0);

    snf0.format(std::move(sn0), status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR, "Use of moved number");

    assertEquals("Move number constructor",
        u"55",
        snf0.format(std::move(sn1), status).toTempString(status));

    SimpleNumber sn2;
    snf0.format(std::move(sn2), status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR, "Default constructed number");

    sn0 = SimpleNumber::forInt64(44, status);

    assertEquals("Move number assignment",
        u"44",
        snf0.format(std::move(sn0), status).toTempString(status));

    SimpleNumberFormatter snf1 = std::move(snf0);

    snf0.format(SimpleNumber::forInt64(22, status), status);
    status.expectErrorAndReset(U_INVALID_STATE_ERROR, "Use of moved formatter");

    assertEquals("Move formatter constructor",
        u"33",
        snf1.format(SimpleNumber::forInt64(33, status), status).toTempString(status));

    SimpleNumberFormatter snf2;
    snf2.format(SimpleNumber::forInt64(22, status), status);
    status.expectErrorAndReset(U_INVALID_STATE_ERROR, "Default constructed formatter");

    snf0 = std::move(snf1);

    assertEquals("Move formatter assignment",
        u"22",
        snf0.format(SimpleNumber::forInt64(22, status), status).toTempString(status));

    snf0 = SimpleNumberFormatter::forLocale("de", status);
    sn0 = SimpleNumber::forInt64(22, status);
    sn0 = SimpleNumber::forInt64(11, status);

    assertEquals("Move assignment with nonempty fields",
        u"11",
        snf0.format(std::move(sn0), status).toTempString(status));
}

void SimpleNumberFormatterTest::testCAPI() {
    IcuTestErrorCode status(*this, "testCAPI");

    LocalUSimpleNumberFormatterPointer uformatter(usnumf_openForLocale("de-CH", status));
    LocalUFormattedNumberPointer uresult(unumf_openResult(status));
    usnumf_formatInt64(uformatter.getAlias(), 55, uresult.getAlias(), status);
    assertEquals("",
        u"55",
        ufmtval_getString(unumf_resultAsValue(uresult.getAlias(), status), nullptr, status));

    LocalUSimpleNumberPointer unumber(usnum_openForInt64(44, status));
    usnumf_format(uformatter.getAlias(), unumber.getAlias(), uresult.getAlias(), status);
    assertEquals("",
        u"44",
        ufmtval_getString(unumf_resultAsValue(uresult.getAlias(), status), nullptr, status));

    // Can't re-use a number without setting it again
    usnumf_format(uformatter.getAlias(), unumber.getAlias(), uresult.getAlias(), status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
    usnum_multiplyByPowerOfTen(unumber.getAlias(), 0, status);
    status.expectErrorAndReset(U_INVALID_STATE_ERROR);

    usnum_setToInt64(unumber.getAlias(), 2335, status);
    usnum_multiplyByPowerOfTen(unumber.getAlias(), -2, status);
    usnum_roundTo(unumber.getAlias(), -1, UNUM_ROUND_HALFEVEN, status);
    usnum_setMaximumIntegerDigits(unumber.getAlias(), 1, status);
    usnum_setMinimumFractionDigits(unumber.getAlias(), 3, status);
    usnum_setMinimumIntegerDigits(unumber.getAlias(), 3, status);
    usnumf_format(uformatter.getAlias(), unumber.getAlias(), uresult.getAlias(), status);
    assertEquals("",
        u"003.400",
        ufmtval_getString(unumf_resultAsValue(uresult.getAlias(), status), nullptr, status));

    // Setting twice should overwrite the first value
    usnum_setToInt64(unumber.getAlias(), 1111, status);
    usnum_setToInt64(unumber.getAlias(), 2222, status);
    usnumf_format(uformatter.getAlias(), unumber.getAlias(), uresult.getAlias(), status);
    assertEquals("",
        u"2’222",
        ufmtval_getString(unumf_resultAsValue(uresult.getAlias(), status), nullptr, status));

    {
        // This is the exact example in usimplenumberformatter.h
        LocalUSimpleNumberFormatterPointer uformatter(usnumf_openForLocale("de-CH", status));
        LocalUFormattedNumberPointer uresult(unumf_openResult(status));
        usnumf_formatInt64(uformatter.getAlias(), 55, uresult.getAlias(), status);
        assertEquals("",
            u"55",
            ufmtval_getString(unumf_resultAsValue(uresult.getAlias(), status), nullptr, status));
    }
}


#endif
