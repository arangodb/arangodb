/*
*******************************************************************************
*
*   Copyright (C) 2012-2013, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  listformattertest.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2012aug27
*   created by: Umesh P. Nair
*/

#include "listformattertest.h"
#include <string.h>

ListFormatterTest::ListFormatterTest() :
        prefix("Prefix: ", -1, US_INV),
        one("Alice", -1, US_INV), two("Bob", -1, US_INV),
        three("Charlie", -1, US_INV), four("Delta", -1, US_INV) {
}

void ListFormatterTest::CheckFormatting(const ListFormatter* formatter, UnicodeString data[], int32_t dataSize,
                                        const UnicodeString& expected_result) {
    UnicodeString actualResult(prefix);
    UErrorCode errorCode = U_ZERO_ERROR;
    formatter->format(data, dataSize, actualResult, errorCode);
    UnicodeString expectedStringWithPrefix = prefix + expected_result;
    if (expectedStringWithPrefix != actualResult) {
        errln(UnicodeString("Expected: |") + expectedStringWithPrefix +  "|, Actual: |" + actualResult + "|");
    }
}

void ListFormatterTest::CheckFourCases(const char* locale_string, UnicodeString one, UnicodeString two,
        UnicodeString three, UnicodeString four, UnicodeString results[4]) {
    UErrorCode errorCode = U_ZERO_ERROR;
    LocalPointer<ListFormatter> formatter(ListFormatter::createInstance(Locale(locale_string), errorCode));
    if (U_FAILURE(errorCode)) {
        dataerrln("ListFormatter::createInstance(Locale(\"%s\"), errorCode) failed in CheckFourCases: %s", locale_string, u_errorName(errorCode));
        return;
    }
    UnicodeString input1[] = {one};
    CheckFormatting(formatter.getAlias(), input1, 1, results[0]);

    UnicodeString input2[] = {one, two};
    CheckFormatting(formatter.getAlias(), input2, 2, results[1]);

    UnicodeString input3[] = {one, two, three};
    CheckFormatting(formatter.getAlias(), input3, 3, results[2]);

    UnicodeString input4[] = {one, two, three, four};
    CheckFormatting(formatter.getAlias(), input4, 4, results[3]);
}

UBool ListFormatterTest::RecordFourCases(const Locale& locale, UnicodeString one, UnicodeString two,
        UnicodeString three, UnicodeString four, UnicodeString results[4])  {
    UErrorCode errorCode = U_ZERO_ERROR;
    LocalPointer<ListFormatter> formatter(ListFormatter::createInstance(locale, errorCode));
    if (U_FAILURE(errorCode)) {
        dataerrln("ListFormatter::createInstance(\"%s\", errorCode) failed in RecordFourCases: %s", locale.getName(), u_errorName(errorCode));
        return FALSE;
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
        return FALSE;
    }
    return TRUE;
}

void ListFormatterTest::TestRoot() {
    UnicodeString results[4] = {
        one,
        one + ", " + two,
        one + ", " + two + ", " + three,
        one + ", " + two + ", " + three + ", " + four
    };

    CheckFourCases("", one, two, three, four, results);
}

// Bogus locale should fallback to root.
void ListFormatterTest::TestBogus() {
    UnicodeString results[4];
    if (RecordFourCases(Locale::getDefault(), one, two, three, four, results)) {
      CheckFourCases("ex_PY", one, two, three, four, results);
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

    CheckFourCases("en", one, two, three, four, results);
}

void ListFormatterTest::Test9946() {
    UErrorCode errorCode = U_ZERO_ERROR;
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

    CheckFourCases("en_US", one, two, three, four, results);
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

    CheckFourCases("ru", one, two, three, four, results);
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

    CheckFourCases("ml", one, two, three, four, results);
}

// Formatting in Zulu.
// "and" is used before the last element, and all elements up to (and including) the penultimate are followed by a comma.
void ListFormatterTest::TestZulu() {
    UnicodeString results[4] = {
        one,
        "I-" + one + " ne-" + two,
        one + ", " + two + ", no-" + three,
        one + ", " + two + ", " + three + ", no-" + four
    };

    CheckFourCases("zu", one, two, three, four, results);
}

void ListFormatterTest::TestOutOfOrderPatterns() {
    UnicodeString results[4] = {
        one,
        two + " after " + one,
        three + " in the last after " + two + " after the first " + one,
        four + " in the last after " + three + " after " + two + " after the first " + one
    };

    ListFormatData data("{1} after {0}", "{1} after the first {0}",
                        "{1} after {0}", "{1} in the last after {0}");
    ListFormatter formatter(&data);

    UnicodeString input1[] = {one};
    CheckFormatting(&formatter, input1, 1, results[0]);

    UnicodeString input2[] = {one, two};
    CheckFormatting(&formatter, input2, 2, results[1]);

    UnicodeString input3[] = {one, two, three};
    CheckFormatting(&formatter, input3, 3, results[2]);

    UnicodeString input4[] = {one, two, three, four};
    CheckFormatting(&formatter, input4, 4, results[3]);
}

void ListFormatterTest::runIndexedTest(int32_t index, UBool exec,
                                       const char* &name, char* /*par */) {
    switch(index) {
        case 0: name = "TestRoot"; if (exec) TestRoot(); break;
        case 1: name = "TestBogus"; if (exec) TestBogus(); break;
        case 2: name = "TestEnglish"; if (exec) TestEnglish(); break;
        case 3: name = "TestEnglishUS"; if (exec) TestEnglishUS(); break;
        case 4: name = "TestRussian"; if (exec) TestRussian(); break;
        case 5: name = "TestMalayalam"; if (exec) TestMalayalam(); break;
        case 6: name = "TestZulu"; if (exec) TestZulu(); break;
        case 7: name = "TestOutOfOrderPatterns"; if (exec) TestOutOfOrderPatterns(); break;
        case 8: name = "Test9946"; if (exec) Test9946(); break;

        default: name = ""; break;
    }
}
