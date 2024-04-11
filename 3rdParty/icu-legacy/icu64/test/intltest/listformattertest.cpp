// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2012-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  listformattertest.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2012aug27
*   created by: Umesh P. Nair
*/

#include "listformattertest.h"
#include "unicode/ulistformatter.h"
#include "cmemory.h"
#include <string.h>

#if !UCONFIG_NO_FORMATTING

void ListFormatterTest::runIndexedTest(int32_t index, UBool exec,
                                       const char* &name, char* /*par */) {
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestRoot);
    TESTCASE_AUTO(TestBogus);
    TESTCASE_AUTO(TestEnglish);
    TESTCASE_AUTO(TestEnglishUS);
    TESTCASE_AUTO(TestRussian);
    TESTCASE_AUTO(TestMalayalam);
    TESTCASE_AUTO(TestZulu);
    TESTCASE_AUTO(TestOutOfOrderPatterns);
    TESTCASE_AUTO(Test9946);
    TESTCASE_AUTO(TestEnglishGB);
    TESTCASE_AUTO(TestNynorsk);
    TESTCASE_AUTO(TestChineseTradHK);
    TESTCASE_AUTO(TestFieldPositionIteratorWith1Item);
    TESTCASE_AUTO(TestFieldPositionIteratorWith2Items);
    TESTCASE_AUTO(TestFieldPositionIteratorWith2ItemsPatternShift);
    TESTCASE_AUTO(TestFieldPositionIteratorWith3Items);
    TESTCASE_AUTO(TestFieldPositionIteratorWith3ItemsPatternShift);
    TESTCASE_AUTO(TestFormattedValue);
    TESTCASE_AUTO(TestDifferentStyles);
    TESTCASE_AUTO(TestCreateStyled);
    TESTCASE_AUTO(TestContextual);
    TESTCASE_AUTO(TestNextPosition);
    TESTCASE_AUTO(TestInt32Overflow);
    TESTCASE_AUTO(Test21871);
    TESTCASE_AUTO_END;
}

namespace {
const char* attrString(int32_t attrId) {
  switch (attrId) {
    case ULISTFMT_LITERAL_FIELD: return "literal";
    case ULISTFMT_ELEMENT_FIELD: return "element";
    default: return "xxx";
  }
}
}  // namespace

void ListFormatterTest::ExpectPositions(
        const FormattedList& iter,
        int32_t *values,
        int32_t tupleCount,
        UErrorCode& status) {
    UBool found[10];
    ConstrainedFieldPosition cfp;
    cfp.constrainCategory(UFIELD_CATEGORY_LIST);
    if (tupleCount > 10) {
      assertTrue("internal error, tupleCount too large", false);
    } else {
        for (int i = 0; i < tupleCount; ++i) {
            found[i] = false;
        }
    }
    while (iter.nextPosition(cfp, status)) {
        UBool ok = false;
        int32_t id = cfp.getField();
        int32_t start = cfp.getStart();
        int32_t limit = cfp.getLimit();
        char buf[128];
        snprintf(buf, sizeof(buf), "%24s %3d %3d %3d", attrString(id), id, start, limit);
        logln(buf);
        for (int i = 0; i < tupleCount; ++i) {
            if (found[i]) {
                continue;
            }
            if (values[i*3] == id && values[i*3+1] == start && values[i*3+2] == limit) {
                found[i] = ok = true;
                break;
            }
        }
        assertTrue((UnicodeString)"found [" + attrString(id) + "," + start + "," + limit + "]", ok);
    }
    // check that all were found
    UBool ok = true;
    for (int i = 0; i < tupleCount; ++i) {
        if (!found[i]) {
            ok = false;
            assertTrue((UnicodeString) "missing [" + attrString(values[i*3]) + "," + values[i*3+1] +
                       "," + values[i*3+2] + "]", found[i]);
        }
    }
    assertTrue("no expected values were missing", ok);
}

ListFormatterTest::ListFormatterTest() :
        prefix("Prefix: ", -1, US_INV),
        one("Alice", -1, US_INV), two("Bob", -1, US_INV),
        three("Charlie", -1, US_INV), four("Delta", -1, US_INV) {
}

void ListFormatterTest::CheckFormatting(const ListFormatter* formatter, UnicodeString data[], int32_t dataSize,
                                        const UnicodeString& expected_result, const char* testName) {
    UnicodeString actualResult(prefix);
    IcuTestErrorCode errorCode(*this, testName);
    formatter->format(data, dataSize, actualResult, errorCode);
    UnicodeString expectedStringWithPrefix = prefix + expected_result;
    if (expectedStringWithPrefix != actualResult) {
        errln(UnicodeString("Expected: |") + expectedStringWithPrefix +  "|, Actual: |" + actualResult + "|");
    }
}

void ListFormatterTest::CheckFourCases(const char* locale_string, UnicodeString one, UnicodeString two,
        UnicodeString three, UnicodeString four, UnicodeString results[4], const char* testName) {
    IcuTestErrorCode errorCode(*this, testName);
    LocalPointer<ListFormatter> formatter(ListFormatter::createInstance(Locale(locale_string), errorCode));
    if (U_FAILURE(errorCode)) {
        dataerrln("ListFormatter::createInstance(Locale(\"%s\"), errorCode) failed in CheckFourCases: %s", locale_string, u_errorName(errorCode));
        return;
    }
    UnicodeString input1[] = {one};
    CheckFormatting(formatter.getAlias(), input1, 1, results[0], testName);

    UnicodeString input2[] = {one, two};
    CheckFormatting(formatter.getAlias(), input2, 2, results[1], testName);

    UnicodeString input3[] = {one, two, three};
    CheckFormatting(formatter.getAlias(), input3, 3, results[2], testName);

    UnicodeString input4[] = {one, two, three, four};
    CheckFormatting(formatter.getAlias(), input4, 4, results[3], testName);
}

UBool ListFormatterTest::RecordFourCases(const Locale& locale, UnicodeString one, UnicodeString two,
        UnicodeString three, UnicodeString four, UnicodeString results[4], const char* testName)  {
    IcuTestErrorCode errorCode(*this, testName);
    LocalPointer<ListFormatter> formatter(ListFormatter::createInstance(locale, errorCode));
    if (U_FAILURE(errorCode)) {
        dataerrln("ListFormatter::createInstance(\"%s\", errorCode) failed in RecordFourCases: %s", locale.getName(), u_errorName(errorCode));
        return false;
    }
    UnicodeString input1[] = {one};
    formatter->format(input1, 1, results[0], errorCode);
    UnicodeString input2[] = {one, two};
    formatter->format(input2, 2, results[1], errorCode);
    UnicodeString input3[] = {one, two, three};
    formatter->format(input3, 3, results[2], errorCode);
    UnicodeString input4[] = {one, two, three, four};
    formatter->format(input4, 4, results[3], errorCode);
    if (U_FAILURE(errorCode)) {
        errln("RecordFourCases failed: %s", u_errorName(errorCode));
        return false;
    }
    return true;
}

void ListFormatterTest::TestRoot() {
    UnicodeString results[4] = {
        one,
        one + ", " + two,
        one + ", " + two + ", " + three,
        one + ", " + two + ", " + three + ", " + four
    };

    CheckFourCases("", one, two, three, four, results, "TestRoot()");
}

// Bogus locale should fallback to root.
void ListFormatterTest::TestBogus() {
    UnicodeString results[4];
    if (RecordFourCases(Locale::getDefault(), one, two, three, four, results, "TestBogus()")) {
      CheckFourCases("ex_PY", one, two, three, four, results, "TestBogus()");
    }
}

// Formatting in English.
// "and" is used before the last element, and all elements up to (and including) the penultimate are followed by a comma.
void ListFormatterTest::TestEnglish() {
    UnicodeString results[4] = {
        one,
        one + " and " + two,
        one + ", " + two + ", and " + three,
        one + ", " + two + ", " + three + ", and " + four
    };

    CheckFourCases("en", one, two, three, four, results, "TestEnglish()");
}

void ListFormatterTest::Test9946() {
    IcuTestErrorCode errorCode(*this, "Test9946()");
    LocalPointer<ListFormatter> formatter(ListFormatter::createInstance(Locale("en"), errorCode));
    if (U_FAILURE(errorCode)) {
        dataerrln(
            "ListFormatter::createInstance(Locale(\"en\"), errorCode) failed in Test9946: %s",
            u_errorName(errorCode));
        return;
    }
    UnicodeString data[3] = {"{0}", "{1}", "{2}"};
    UnicodeString actualResult;
    formatter->format(data, 3, actualResult, errorCode);
    if (U_FAILURE(errorCode)) {
        dataerrln(
            "ListFormatter::createInstance(Locale(\"en\"), errorCode) failed in Test9946: %s",
            u_errorName(errorCode));
        return;
    }
    UnicodeString expected("{0}, {1}, and {2}");
    if (expected != actualResult) {
        errln("Expected " + expected + ", got " + actualResult);
    }
}

void ListFormatterTest::TestEnglishUS() {
    UnicodeString results[4] = {
        one,
        one + " and " + two,
        one + ", " + two + ", and " + three,
        one + ", " + two + ", " + three + ", and " + four
    };

    CheckFourCases("en_US", one, two, three, four, results, "TestEnglishUS()");
}

// Tests resource loading and inheritance when region sublocale
// has only partial data for the listPattern element (overriding
// some of the parent data). #12994
void ListFormatterTest::TestEnglishGB() {
    UnicodeString results[4] = {
        one,
        one + " and " + two,
        one + ", " + two + " and " + three,
        one + ", " + two + ", " + three + " and " + four
    };

    CheckFourCases("en_GB", one, two, three, four, results, "TestEnglishGB()");
}

void ListFormatterTest::RunTestFieldPositionIteratorWithFormatter(
        ListFormatter* formatter,
        UnicodeString data[], int32_t n, int32_t expected[], int32_t tupleCount,
        const char16_t *expectedFormatted,
        const char* testName) {
    IcuTestErrorCode errorCode(*this, testName);
    FormattedList fl = formatter->formatStringsToValue(data, n, errorCode);
    UnicodeString actual = fl.toString(errorCode);
    if (U_FAILURE(errorCode)) {
        dataerrln(
            "ListFormatter::format(data, %d, &iter, errorCode) "
            "failed in %s: %s", n, testName, u_errorName(errorCode));
        return;
    }
    if (actual != expectedFormatted) {
        errln(UnicodeString("Expected: |") + expectedFormatted +  "|, Actual: |" + actual + "|");
    }
    ExpectPositions(fl, expected, tupleCount, errorCode);
}

void ListFormatterTest::RunTestFieldPositionIteratorWithNItemsPatternShift(
        UnicodeString data[], int32_t n, int32_t expected[], int32_t tupleCount,
        const char16_t *expectedFormatted,
        const char* testName) {
    IcuTestErrorCode errorCode(*this, testName);
    LocalPointer<ListFormatter> formatter(ListFormatter::createInstance(
        Locale("ur", "IN"), // in CLDR 41 alpha1 the "backwards order" patterns in this and other locales were removed.
        ULISTFMT_TYPE_UNITS,
        ULISTFMT_WIDTH_NARROW,
        errorCode));
    if (U_FAILURE(errorCode)) {
        dataerrln(
            "ListFormatter::createInstance(Locale(\"ur\", \"IN\"), \"unit-narrow\", errorCode) failed in "
            "%s: %s", testName, u_errorName(errorCode));
        return;
    }
    RunTestFieldPositionIteratorWithFormatter(
        formatter.getAlias(),
        data, n, expected, tupleCount, expectedFormatted, testName);
}

void ListFormatterTest::RunTestFieldPositionIteratorWithNItems(
        UnicodeString data[], int32_t n, int32_t expected[], int32_t tupleCount,
        const char16_t *expectedFormatted,
        const char* testName) {
    IcuTestErrorCode errorCode(*this, testName);
    LocalPointer<ListFormatter> formatter(
        ListFormatter::createInstance(Locale("en"), errorCode));
    if (U_FAILURE(errorCode)) {
        dataerrln(
            "ListFormatter::createInstance(Locale(\"en\"), errorCode) failed in "
            "%s: %s", testName, u_errorName(errorCode));
        return;
    }
    RunTestFieldPositionIteratorWithFormatter(
        formatter.getAlias(),
        data, n, expected, tupleCount, expectedFormatted, testName);
}

void ListFormatterTest::TestFieldPositionIteratorWith3Items() {
    //  0         1
    //  012345678901234
    // "a, bbb, and cc"
    UnicodeString data[3] = {"a", "bbb", "cc"};
    int32_t expected[] = {
        ULISTFMT_ELEMENT_FIELD, 0, 1,
        ULISTFMT_LITERAL_FIELD, 1, 3,
        ULISTFMT_ELEMENT_FIELD, 3, 6,
        ULISTFMT_LITERAL_FIELD, 6, 12,
        ULISTFMT_ELEMENT_FIELD, 12, 14
    };
    int32_t tupleCount = sizeof(expected)/(3 * sizeof(*expected));
    RunTestFieldPositionIteratorWithNItems(
        data, 3, expected, tupleCount,
        u"a, bbb, and cc",
        "TestFieldPositionIteratorWith3Items");
}

void ListFormatterTest::TestFieldPositionIteratorWith3ItemsPatternShift() {
    // Note: In CLDR 41 alpha1 the "backwards order" patterns in ur_IN (and one or two
    // other locales) were removed, ur_IN now just inherits list patterns from ur.
    // So this test may no longer be interesting.
    UnicodeString data[3] = {"a", "bbb", "cc"};
    int32_t expected[] = {
        ULISTFMT_ELEMENT_FIELD, 0, 1,
        ULISTFMT_LITERAL_FIELD, 1, 3,
        ULISTFMT_ELEMENT_FIELD, 3, 6,
        ULISTFMT_LITERAL_FIELD, 6, 12,
        ULISTFMT_ELEMENT_FIELD, 12, 14
    };
    int32_t tupleCount = sizeof(expected)/(3 * sizeof(*expected));
    RunTestFieldPositionIteratorWithNItemsPatternShift(
        data, 3, expected, tupleCount,
        u"a، bbb، اور cc",
        "TestFieldPositionIteratorWith3ItemsPatternShift");
}

void ListFormatterTest::TestFieldPositionIteratorWith2Items() {
    //  0         1
    //  01234567890
    // "bbb and cc"
    UnicodeString data[2] = {"bbb", "cc"};
    int32_t expected[] = {
        ULISTFMT_ELEMENT_FIELD, 0, 3,
        ULISTFMT_LITERAL_FIELD, 3, 8,
        ULISTFMT_ELEMENT_FIELD, 8, 10
    };
    int32_t tupleCount = sizeof(expected)/(3 * sizeof(*expected));
    RunTestFieldPositionIteratorWithNItems(
        data, 2, expected, tupleCount,
        u"bbb and cc",
        "TestFieldPositionIteratorWith2Items");
}

void ListFormatterTest::TestFieldPositionIteratorWith2ItemsPatternShift() {
    // Note: In CLDR 41 alpha1 the "backwards order" patterns in ur_IN (and one or two
    // other locales) were removed, ur_IN now just inherits list patterns from ur.
    // So this test may no longer be interesting.
    UnicodeString data[2] = {"bbb", "cc"};
    int32_t expected[] = {
        ULISTFMT_ELEMENT_FIELD, 0, 3,
        ULISTFMT_LITERAL_FIELD, 3, 8,
        ULISTFMT_ELEMENT_FIELD, 8, 10
    };
    int32_t tupleCount = sizeof(expected)/(3 * sizeof(*expected));
    RunTestFieldPositionIteratorWithNItemsPatternShift(
        data, 2, expected, tupleCount,
        u"bbb اور cc",
        "TestFieldPositionIteratorWith2ItemsPatternShift");
}

void ListFormatterTest::TestFieldPositionIteratorWith1Item() {
    //  012
    // "cc"
    UnicodeString data[1] = {"cc"};
    int32_t expected[] = {
        ULISTFMT_ELEMENT_FIELD, 0, 2
    };
    int32_t tupleCount = sizeof(expected)/(3 * sizeof(*expected));
    RunTestFieldPositionIteratorWithNItems(
        data, 1, expected, tupleCount,
        u"cc",
        "TestFieldPositionIteratorWith1Item");
}

// Tests resource loading and inheritance when region sublocale
// has only partial data for the listPattern element (overriding
// some of the parent data). #12994
void ListFormatterTest::TestNynorsk() {
    UnicodeString results[4] = {
        one,
        one + " og " + two,
        one + ", " + two + " og " + three,
        one + ", " + two + ", " + three + " og " + four
    };

    CheckFourCases("nn", one, two, three, four, results, "TestNynorsk()");
}

// Tests resource loading and inheritance when region sublocale
// has only partial data for the listPattern element (overriding
// some of the parent data). #12994
void ListFormatterTest::TestChineseTradHK() {
    UnicodeString and_string = UnicodeString("\\u53CA", -1, US_INV).unescape();
    UnicodeString comma_string = UnicodeString("\\u3001", -1, US_INV).unescape();
    UnicodeString results[4] = {
        one,
        one + and_string + two,
        one + comma_string + two + and_string + three,
        one + comma_string + two + comma_string + three + and_string + four
    };

    CheckFourCases("zh_Hant_HK", one, two, three, four, results, "TestChineseTradHK()");
}

// Formatting in Russian.
// "\\u0438" is used before the last element, and all elements up to (but not including) the penultimate are followed by a comma.
void ListFormatterTest::TestRussian() {
    UnicodeString and_string = UnicodeString(" \\u0438 ", -1, US_INV).unescape();
    UnicodeString results[4] = {
        one,
        one + and_string + two,
        one + ", " + two + and_string + three,
        one + ", " + two + ", " + three + and_string + four
    };

    CheckFourCases("ru", one, two, three, four, results, "TestRussian()");
}

// Formatting in Malayalam.
// For two elements, "\\u0d15\\u0d42\\u0d1f\\u0d3e\\u0d24\\u0d46" is inserted in between.
// For more than two elements, comma is inserted between all elements up to (and including) the penultimate,
// and the word \\u0d0e\\u0d28\\u0d4d\\u0d28\\u0d3f\\u0d35 is inserted in the end.
void ListFormatterTest::TestMalayalam() {
    UnicodeString pair_string = UnicodeString(" \\u0d15\\u0d42\\u0d1f\\u0d3e\\u0d24\\u0d46 ", -1, US_INV).unescape();
    UnicodeString total_string = UnicodeString(" \\u0d0e\\u0d28\\u0d4d\\u0d28\\u0d3f\\u0d35", -1, US_INV).unescape();
    UnicodeString results[4] = {
        one,
        one + pair_string + two,
        one + ", " + two + ", " + three + total_string,
        one + ", " + two + ", " + three + ", " + four + total_string
    };

    CheckFourCases("ml", one, two, three, four, results, "TestMalayalam()");
}

// Formatting in Zulu.
// "and" is used before the last element, and all elements up to (and including) the penultimate are followed by a comma.
void ListFormatterTest::TestZulu() {
    UnicodeString results[4] = {
        one,
        one + " ne-" + two,
        one + ", " + two + ", ne-" + three,
        one + ", " + two + ", " + three + ", ne-" + four
    };

    CheckFourCases("zu", one, two, three, four, results, "TestZulu()");
}

void ListFormatterTest::TestOutOfOrderPatterns() {
    UnicodeString results[4] = {
        one,
        two + " after " + one,
        three + " in the last after " + two + " after the first " + one,
        four + " in the last after " + three + " after " + two + " after the first " + one
    };

    IcuTestErrorCode errorCode(*this, "TestOutOfOrderPatterns()");
    Locale locale("en");
    ListFormatData data("{1} after {0}", "{1} after the first {0}",
                        "{1} after {0}", "{1} in the last after {0}", locale);
    ListFormatter formatter(data, errorCode);

    UnicodeString input1[] = {one};
    CheckFormatting(&formatter, input1, 1, results[0], "TestOutOfOrderPatterns()");

    UnicodeString input2[] = {one, two};
    CheckFormatting(&formatter, input2, 2, results[1], "TestOutOfOrderPatterns()");

    UnicodeString input3[] = {one, two, three};
    CheckFormatting(&formatter, input3, 3, results[2], "TestOutOfOrderPatterns()");

    UnicodeString input4[] = {one, two, three, four};
    CheckFormatting(&formatter, input4, 4, results[3], "TestOutOfOrderPatterns()");
}

void ListFormatterTest::TestFormattedValue() {
    IcuTestErrorCode status(*this, "TestFormattedValue");

    {
        LocalPointer<ListFormatter> fmt(ListFormatter::createInstance("en", status));
        if (status.errIfFailureAndReset()) { return; }
        const char16_t* message = u"Field position test 1";
        const char16_t* expectedString = u"hello, wonderful, and world";
        const UnicodeString inputs[] = {
            u"hello",
            u"wonderful",
            u"world"
        };
        FormattedList result = fmt->formatStringsToValue(inputs, UPRV_LENGTHOF(inputs), status);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // field, begin index, end index
            {UFIELD_CATEGORY_LIST_SPAN, 0, 0, 5},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 0, 5},
            {UFIELD_CATEGORY_LIST, ULISTFMT_LITERAL_FIELD, 5, 7},
            {UFIELD_CATEGORY_LIST_SPAN, 1, 7, 16},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 7, 16},
            {UFIELD_CATEGORY_LIST, ULISTFMT_LITERAL_FIELD, 16, 22},
            {UFIELD_CATEGORY_LIST_SPAN, 2, 22, 27},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 22, 27}};
        checkMixedFormattedValue(
            message,
            result,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        LocalPointer<ListFormatter> fmt(ListFormatter::createInstance("zh", ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_SHORT, status));
        if (status.errIfFailureAndReset()) { return; }
        const char16_t* message = u"Field position test 2 (ICU-21340)";
        const char16_t* expectedString = u"aabbbbbbbccc";
        const UnicodeString inputs[] = {
            u"aa",
            u"bbbbbbb",
            u"ccc"
        };
        FormattedList result = fmt->formatStringsToValue(inputs, UPRV_LENGTHOF(inputs), status);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // field, begin index, end index
            {UFIELD_CATEGORY_LIST_SPAN, 0, 0, 2},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 0, 2},
            {UFIELD_CATEGORY_LIST_SPAN, 1, 2, 9},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 2, 9},
            {UFIELD_CATEGORY_LIST_SPAN, 2, 9, 12},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 9, 12}};
        checkMixedFormattedValue(
            message,
            result,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }

    {
        LocalPointer<ListFormatter> fmt(ListFormatter::createInstance("en", ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_SHORT, status));
        if (status.errIfFailureAndReset()) { return; }
        const char16_t* message = u"ICU-21383 Long list";
        const char16_t* expectedString = u"a, b, c, d, e, f, g, h, i";
        const UnicodeString inputs[] = {
            u"a",
            u"b",
            u"c",
            u"d",
            u"e",
            u"f",
            u"g",
            u"h",
            u"i",
        };
        FormattedList result = fmt->formatStringsToValue(inputs, UPRV_LENGTHOF(inputs), status);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // field, begin index, end index
            {UFIELD_CATEGORY_LIST_SPAN, 0, 0, 1},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 0, 1},
            {UFIELD_CATEGORY_LIST, ULISTFMT_LITERAL_FIELD, 1, 3},
            {UFIELD_CATEGORY_LIST_SPAN, 1, 3, 4},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 3, 4},
            {UFIELD_CATEGORY_LIST, ULISTFMT_LITERAL_FIELD, 4, 6},
            {UFIELD_CATEGORY_LIST_SPAN, 2, 6, 7},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 6, 7},
            {UFIELD_CATEGORY_LIST, ULISTFMT_LITERAL_FIELD, 7, 9},
            {UFIELD_CATEGORY_LIST_SPAN, 3, 9, 10},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 9, 10},
            {UFIELD_CATEGORY_LIST, ULISTFMT_LITERAL_FIELD, 10, 12},
            {UFIELD_CATEGORY_LIST_SPAN, 4, 12, 13},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 12, 13},
            {UFIELD_CATEGORY_LIST, ULISTFMT_LITERAL_FIELD, 13, 15},
            {UFIELD_CATEGORY_LIST_SPAN, 5, 15, 16},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 15, 16},
            {UFIELD_CATEGORY_LIST, ULISTFMT_LITERAL_FIELD, 16, 18},
            {UFIELD_CATEGORY_LIST_SPAN, 6, 18, 19},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 18, 19},
            {UFIELD_CATEGORY_LIST, ULISTFMT_LITERAL_FIELD, 19, 21},
            {UFIELD_CATEGORY_LIST_SPAN, 7, 21, 22},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 21, 22},
            {UFIELD_CATEGORY_LIST, ULISTFMT_LITERAL_FIELD, 22, 24},
            {UFIELD_CATEGORY_LIST_SPAN, 8, 24, 25},
            {UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD, 24, 25},
            };
        checkMixedFormattedValue(
            message,
            result,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
}

void ListFormatterTest::DoTheRealListStyleTesting(
        Locale locale,
        UnicodeString items[],
        int itemCount,
        UListFormatterType type,
        UListFormatterWidth width,
        const char* expected,
        IcuTestErrorCode status) {

    LocalPointer<ListFormatter> formatter(
            ListFormatter::createInstance(locale, type, width, status));

    UnicodeString actualResult;
    formatter->format(items, itemCount, actualResult, status);
    assertEquals(Int64ToUnicodeString(type) + "-" + Int64ToUnicodeString(width),
        UnicodeString(expected), actualResult);
}

void ListFormatterTest::TestDifferentStyles() {
    Locale locale("fr");
    UnicodeString input[4] = { u"rouge", u"jaune", u"bleu", u"vert" };
    IcuTestErrorCode status(*this, "TestDifferentStyles()");

    DoTheRealListStyleTesting(locale, input, 4, ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, "rouge, jaune, bleu et vert", status);
    DoTheRealListStyleTesting(locale, input, 4, ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_WIDE, "rouge, jaune, bleu ou vert", status);
    DoTheRealListStyleTesting(locale, input, 4, ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_WIDE, "rouge, jaune, bleu et vert", status);
    DoTheRealListStyleTesting(locale, input, 4, ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_NARROW, "rouge jaune bleu vert", status);
    DoTheRealListStyleTesting(locale, input, 4, ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_SHORT, "rouge, jaune, bleu et vert", status);
}

void ListFormatterTest::TestCreateStyled() {
    IcuTestErrorCode status(*this, "TestCreateStyled");
    // Locale en has interesting data
    struct TestCase {
        const char* locale;
        UListFormatterType type;
        UListFormatterWidth width;
        const char16_t* expected3;
        const char16_t* expected2;
        const char16_t* expected1;
    } cases[] = {
        { "pt", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, u"A, B e C", u"A e B", u"A" },
        { "pt", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_SHORT, u"A, B e C", u"A e B", u"A" },
        { "pt", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_NARROW, u"A, B, C", u"A, B", u"A" },
        { "pt", ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_WIDE, u"A, B ou C", u"A ou B", u"A" },
        { "pt", ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_SHORT, u"A, B ou C", u"A ou B", u"A" },
        { "pt", ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_NARROW, u"A, B ou C", u"A ou B", u"A" },
        { "pt", ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_WIDE, u"A, B e C", u"A e B", u"A" },
        { "pt", ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_SHORT, u"A, B e C", u"A e B", u"A" },
        { "pt", ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_NARROW, u"A B C", u"A B", u"A" },
        { "en", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_WIDE, u"A, B, and C", u"A and B", u"A" },
        { "en", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_SHORT, u"A, B, & C", u"A & B", u"A" },
        { "en", ULISTFMT_TYPE_AND, ULISTFMT_WIDTH_NARROW, u"A, B, C", u"A, B", u"A" },
        { "en", ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_WIDE, u"A, B, or C", u"A or B", u"A" },
        { "en", ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_SHORT, u"A, B, or C", u"A or B", u"A" },
        { "en", ULISTFMT_TYPE_OR, ULISTFMT_WIDTH_NARROW, u"A, B, or C", u"A or B", u"A" },
        { "en", ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_WIDE, u"A, B, C", u"A, B", u"A" },
        { "en", ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_SHORT, u"A, B, C", u"A, B", u"A" },
        { "en", ULISTFMT_TYPE_UNITS, ULISTFMT_WIDTH_NARROW, u"A B C", u"A B", u"A" },
    };
    for (auto cas : cases) {
        LocalPointer<ListFormatter> fmt(
            ListFormatter::createInstance(cas.locale, cas.type, cas.width, status),
            status);
        if (status.errIfFailureAndReset()) {
            continue;
        }
        UnicodeString message = UnicodeString(u"TestCreateStyled loc=")
            + cas.locale + u" type="
            + Int64ToUnicodeString(cas.type) + u" width="
            + Int64ToUnicodeString(cas.width);
        const UnicodeString inputs3[] = {
            u"A",
            u"B",
            u"C"
        };
        FormattedList result = fmt->formatStringsToValue(inputs3, UPRV_LENGTHOF(inputs3), status);
        assertEquals(message, cas.expected3, result.toTempString(status));
        const UnicodeString inputs2[] = {
            u"A",
            u"B"
        };
        result = fmt->formatStringsToValue(inputs2, UPRV_LENGTHOF(inputs2), status);
        assertEquals(message, cas.expected2, result.toTempString(status));
        const UnicodeString inputs1[] = {
            u"A"
        };
        result = fmt->formatStringsToValue(inputs1, UPRV_LENGTHOF(inputs1), status);
        assertEquals(message, cas.expected1, result.toTempString(status));
    }
}

void ListFormatterTest::TestContextual() {
    IcuTestErrorCode status(*this, "TestContextual");
    std::vector<std::string> es = { "es", "es_419" , "es_PY", "es_DO" };
    std::vector<std::string> he = { "he", "he_IL", "iw", "iw_IL" };
    UListFormatterWidth widths [] = {
        ULISTFMT_WIDTH_WIDE, ULISTFMT_WIDTH_SHORT, ULISTFMT_WIDTH_NARROW
    };
    struct TestCase {
        std::vector<std::string> locales;
        UListFormatterType type;
        const char16_t* expected;
        const char16_t* data1;
        const char16_t* data2;
        const char16_t* data3;
    } cases[] = {
        { es, ULISTFMT_TYPE_AND, u"fascinante e increíblemente",
          u"fascinante",                     u"increíblemente",       nullptr },
        { es, ULISTFMT_TYPE_AND, u"Comunicaciones Industriales e IIoT",
          u"Comunicaciones Industriales",    u"IIoT",                 nullptr },
        { es, ULISTFMT_TYPE_AND, u"España e Italia",         u"España",   u"Italia",      nullptr },
        { es, ULISTFMT_TYPE_AND, u"hijas intrépidas e hijos solidarios",
          u"hijas intrépidas",               u"hijos solidarios",     nullptr },
        { es, ULISTFMT_TYPE_AND, u"a un hombre e hirieron a otro",
          u"a un hombre",                    u"hirieron a otro",      nullptr },
        { es, ULISTFMT_TYPE_AND, u"hija e hijo",             u"hija",     u"hijo",        nullptr },
        { es, ULISTFMT_TYPE_AND, u"esposa, hija e hijo",     u"esposa",   u"hija",        u"hijo" },
        // For 'y' exception
        { es, ULISTFMT_TYPE_AND, u"oro y hierro",            u"oro",      u"hierro",      nullptr },
        { es, ULISTFMT_TYPE_AND, u"agua y hielo",            u"agua",     u"hielo",       nullptr },
        { es, ULISTFMT_TYPE_AND, u"colágeno y hialurónico",  u"colágeno", u"hialurónico", nullptr },

        { es, ULISTFMT_TYPE_OR, u"desierto u oasis",         u"desierto", u"oasis",       nullptr },
        { es, ULISTFMT_TYPE_OR, u"oasis, desierto u océano", u"oasis",    u"desierto",    u"océano" },
        { es, ULISTFMT_TYPE_OR, u"7 u 8",                    u"7",        u"8",           nullptr },
        { es, ULISTFMT_TYPE_OR, u"7 u 80",                   u"7",        u"80",          nullptr },
        { es, ULISTFMT_TYPE_OR, u"7 u 800",                  u"7",        u"800",         nullptr },
        { es, ULISTFMT_TYPE_OR, u"6, 7 u 8",                 u"6",        u"7",           u"8" },
        { es, ULISTFMT_TYPE_OR, u"10 u 11",                  u"10",       u"11",          nullptr },
        { es, ULISTFMT_TYPE_OR, u"10 o 111",                 u"10",       u"111",         nullptr },
        { es, ULISTFMT_TYPE_OR, u"10 o 11.2",                u"10",       u"11.2",        nullptr },
        { es, ULISTFMT_TYPE_OR, u"9, 10 u 11",               u"9",        u"10",          u"11" },

        { he, ULISTFMT_TYPE_AND, u"a, b ו-c",               u"a",      u"b",      u"c" },
        { he, ULISTFMT_TYPE_AND, u"a ו-b",                  u"a",      u"b",      nullptr },
        { he, ULISTFMT_TYPE_AND, u"1, 2 ו-3",               u"1",      u"2",      u"3" },
        { he, ULISTFMT_TYPE_AND, u"1 ו-2",                  u"1",      u"2",      nullptr },
        { he, ULISTFMT_TYPE_AND, u"אהבה ומקווה",            u"אהבה",   u"מקווה",  nullptr },
        { he, ULISTFMT_TYPE_AND, u"אהבה, מקווה ואמונה",     u"אהבה",   u"מקווה",  u"אמונה" },
    };
    for (auto width : widths) {
        for (auto cas : cases) {
            for (auto locale : cas.locales) {
                LocalPointer<ListFormatter> fmt(
                    ListFormatter::createInstance(locale.c_str(), cas.type, width, status),
                    status);
                if (status.errIfFailureAndReset()) {
                    continue;
                }
                UnicodeString message = UnicodeString(u"TestContextual loc=")
                    + locale.c_str() + u" type="
                    + Int64ToUnicodeString(cas.type) + u" width="
                    + Int64ToUnicodeString(width);
                if (cas.data3 == nullptr) {
                    const UnicodeString inputs2[] = { cas.data1, cas.data2 };
                    FormattedList result = fmt->formatStringsToValue(inputs2, UPRV_LENGTHOF(inputs2), status);
                    assertEquals(message, cas.expected, result.toTempString(status));
                } else {
                    const UnicodeString inputs3[] = { cas.data1, cas.data2, cas.data3 };
                    FormattedList result = fmt->formatStringsToValue(inputs3, UPRV_LENGTHOF(inputs3), status);
                    assertEquals(message, cas.expected, result.toTempString(status));
                }
            }
        }
    }
}

void ListFormatterTest::TestNextPosition() {
    IcuTestErrorCode status(*this, "TestNextPosition");
    std::vector<std::string> locales = { "en", "es", "zh", "ja" };
    UListFormatterWidth widths [] = {
        ULISTFMT_WIDTH_WIDE, ULISTFMT_WIDTH_SHORT, ULISTFMT_WIDTH_NARROW
    };
    const char* widthStr [] = {"wide", "short", "narrow"};
    UListFormatterType types [] = {
        ULISTFMT_TYPE_AND, ULISTFMT_TYPE_OR, ULISTFMT_TYPE_UNITS
    };
    const char* typeStr [] = {"and", "or", "units"};
    const UnicodeString inputs[] = { u"A1", u"B2", u"C3", u"D4" };
    for (auto width : widths) {
        for (auto type : types) {
            for (auto locale : locales) {
                LocalPointer<ListFormatter> fmt(
                        ListFormatter::createInstance(locale.c_str(), type, width, status),
                    status);
                if (status.errIfFailureAndReset()) {
                    continue;
                }
                for (int32_t n = 1; n <= UPRV_LENGTHOF(inputs); n++) {
                    FormattedList result = fmt->formatStringsToValue(
                        inputs, n, status);
                    int32_t elements = 0;
                    icu::ConstrainedFieldPosition cfpos;
                    cfpos.constrainCategory(UFIELD_CATEGORY_LIST);
                    while (result.nextPosition(cfpos, status) && U_SUCCESS(status)) {
                        if (cfpos.getField() == ULISTFMT_ELEMENT_FIELD) {
                            elements++;
                        }
                    }
                    std::string msg = locale;
                    // Test that if there are n elements (n=1..4) in the input, then the
                    // nextPosition() should iterate through exactly n times
                    // with field == ULISTFMT_ELEMENT_FIELD.
                    assertEquals((msg
                                  .append(" w=").append(widthStr[width])
                                  .append(" t=").append(typeStr[type])).c_str(),
                                 n, elements);
                }
            }
        }
    }
}

void ListFormatterTest::TestInt32Overflow() {
    if (quick) {
        return;
    }
    IcuTestErrorCode status(*this, "TestInt32Overflow");
    LocalPointer<ListFormatter> fmt(ListFormatter::createInstance("en", status), status);
    std::vector<UnicodeString> inputs;
    UnicodeString input(0xAAAFF00, 0x00000042, 0xAAAFF00);
    for (int32_t i = 0; i < 16; i++) {
      inputs.push_back(input);
    }
    FormattedList result = fmt->formatStringsToValue(
        inputs.data(), static_cast<int32_t>(inputs.size()), status);
    status.expectErrorAndReset(U_INPUT_TOO_LONG_ERROR);
}


void ListFormatterTest::Test21871() {
    IcuTestErrorCode status(*this, "Test21871");
    LocalPointer<ListFormatter> fmt(ListFormatter::createInstance("en", status), status);
    {
        UnicodeString strings[] = {{u"A"}, {u""}};
        FormattedList result = fmt->formatStringsToValue(strings, 2, status);
        ConstrainedFieldPosition cfp;
        cfp.constrainField(UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD);
        assertTrue("nextPosition 1", result.nextPosition(cfp, status));
        assertEquals("start", cfp.getStart(), 0);
        assertEquals("limit", cfp.getLimit(), 1);
        assertFalse("nextPosition 2", result.nextPosition(cfp, status));
    }
    {
        UnicodeString strings[] = {{u""}, {u"B"}};
        FormattedList result = fmt->formatStringsToValue(strings, 2, status);
        ConstrainedFieldPosition cfp;
        cfp.constrainField(UFIELD_CATEGORY_LIST, ULISTFMT_ELEMENT_FIELD);
        assertTrue("nextPosition 1", result.nextPosition(cfp, status));
        assertEquals("start", cfp.getStart(), 5);
        assertEquals("limit", cfp.getLimit(), 6);
        assertFalse("nextPosition 2", result.nextPosition(cfp, status));
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
