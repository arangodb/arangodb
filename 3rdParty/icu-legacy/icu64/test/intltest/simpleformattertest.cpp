// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2014-2016, International Business Machines Corporation and
* others. All Rights Reserved.
*******************************************************************************
*
* simpleformattertest.cpp
*
********************************************************************************
*/

#include "unicode/msgfmt.h"
#include "unicode/unistr.h"
#include "cstring.h"
#include "intltest.h"
#include "unicode/simpleformatter.h"

class SimpleFormatterTest : public IntlTest {
public:
    SimpleFormatterTest() {
    }
    void TestNoArguments();
    void TestSyntaxErrors();
    void TestOneArgument();
    void TestBigArgument();
    void TestManyArguments();
    void TestTooFewArgumentValues();
    void TestBadArguments();
    void TestTextWithNoArguments();
    void TestFormatReplaceNoOptimization();
    void TestFormatReplaceNoOptimizationLeadingText();
    void TestFormatReplaceOptimization();
    void TestFormatReplaceNoOptimizationLeadingArgumentUsedTwice();
    void TestFormatReplaceOptimizationNoOffsets();
    void TestFormatReplaceNoOptimizationNoOffsets();
    void TestQuotingLikeMessageFormat();
    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void verifyOffsets(
            const int32_t *expected,
            const int32_t *actual,
            int32_t count);
};

void SimpleFormatterTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/) {
  TESTCASE_AUTO_BEGIN;
  TESTCASE_AUTO(TestNoArguments);
  TESTCASE_AUTO(TestSyntaxErrors);
  TESTCASE_AUTO(TestOneArgument);
  TESTCASE_AUTO(TestBigArgument);
  TESTCASE_AUTO(TestManyArguments);
  TESTCASE_AUTO(TestTooFewArgumentValues);
  TESTCASE_AUTO(TestBadArguments);
  TESTCASE_AUTO(TestTextWithNoArguments);
  TESTCASE_AUTO(TestFormatReplaceNoOptimization);
  TESTCASE_AUTO(TestFormatReplaceNoOptimizationLeadingText);
  TESTCASE_AUTO(TestFormatReplaceOptimization);
  TESTCASE_AUTO(TestFormatReplaceNoOptimizationLeadingArgumentUsedTwice);
  TESTCASE_AUTO(TestFormatReplaceOptimizationNoOffsets);
  TESTCASE_AUTO(TestFormatReplaceNoOptimizationNoOffsets);
  TESTCASE_AUTO(TestQuotingLikeMessageFormat);
  TESTCASE_AUTO_END;
}

void SimpleFormatterTest::TestNoArguments() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleFormatter fmt("This doesn''t have templates '{0}", status);
    assertEquals("getArgumentLimit", 0, fmt.getArgumentLimit());
    UnicodeString appendTo;
    assertEquals(
            "format",
            "This doesn't have templates {0}", 
            fmt.format("unused", appendTo, status));
    appendTo.remove();
    int32_t offsets[] = { 0 };
    assertEquals(
            "formatAndAppend",
            "This doesn't have templates {0}", 
            fmt.formatAndAppend(NULL, 0, appendTo, offsets, 1, status));
    assertEquals("formatAndAppend offsets[0]", -1, offsets[0]);
    assertEquals(
            "formatAndReplace",
            "This doesn't have templates {0}", 
            fmt.formatAndReplace(NULL, 0, appendTo, NULL, 0, status));
    assertSuccess("Status", status);
}

void SimpleFormatterTest::TestSyntaxErrors() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleFormatter fmt("{}", status);
    assertEquals("syntax error {}", (int32_t)U_ILLEGAL_ARGUMENT_ERROR, status);
    status = U_ZERO_ERROR;
    fmt.applyPattern("{12d", status);
    assertEquals("syntax error {12d", (int32_t)U_ILLEGAL_ARGUMENT_ERROR, status);
}

void SimpleFormatterTest::TestOneArgument() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleFormatter fmt;
    fmt.applyPattern("{0} meter", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    assertEquals("getArgumentLimit", 1, fmt.getArgumentLimit());
    UnicodeString appendTo;
    assertEquals(
            "format",
            "1 meter",
            fmt.format("1", appendTo, status));

    // assignment
    SimpleFormatter s;
    s = fmt;
    appendTo.remove();
    assertEquals(
            "Assignment",
            "1 meter",
            s.format("1", appendTo, status));

    // Copy constructor
    SimpleFormatter r(fmt);
    appendTo.remove();
    assertEquals(
            "Copy constructor",
            "1 meter",
            r.format("1", appendTo, status));
    assertSuccess("Status", status);
}

void SimpleFormatterTest::TestBigArgument() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleFormatter fmt("a{20}c", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    assertEquals("{20} count", 21, fmt.getArgumentLimit());
    UnicodeString b("b");
    UnicodeString *values[] = {
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
        &b
    };
    UnicodeString result;
    assertEquals("{20}=b", "abc", fmt.formatAndAppend(values, 21, result, NULL, 0, status));
    assertSuccess("Status", status);
}

void SimpleFormatterTest::TestManyArguments() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleFormatter fmt;
    fmt.applyPattern(
            "Templates {2}{1}{5} and {4} are out of order.", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    assertEquals("getArgumentLimit", 6, fmt.getArgumentLimit());
    UnicodeString values[] = {
            "freddy", "tommy", "frog", "billy", "leg", "{0}"};
    UnicodeString *params[] = {
           &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]}; 
    int32_t offsets[6];
    int32_t expectedOffsets[6] = {-1, 22, 18, -1, 35, 27};
    UnicodeString appendTo("Prefix: ");
    assertEquals(
            "format",
            "Prefix: Templates frogtommy{0} and leg are out of order.",
            fmt.formatAndAppend(
                    params,
                    UPRV_LENGTHOF(params),
                    appendTo,
                    offsets,
                    UPRV_LENGTHOF(offsets),
                    status));
    if (!assertSuccess("Status", status)) {
        return;
    }
    verifyOffsets(expectedOffsets, offsets, UPRV_LENGTHOF(expectedOffsets));
    appendTo.remove();

    // Ensure we don't write to offsets array beyond its length.
    status = U_ZERO_ERROR;
    offsets[UPRV_LENGTHOF(offsets) - 1] = 289;
    appendTo.remove();
    fmt.formatAndAppend(
            params,
            UPRV_LENGTHOF(params),
            appendTo,
            offsets,
            UPRV_LENGTHOF(offsets) - 1,
            status);
    assertEquals("Offsets buffer length", 289, offsets[UPRV_LENGTHOF(offsets) - 1]);

    // Test assignment
    SimpleFormatter s;
    s = fmt;
    appendTo.remove();
    assertEquals(
            "Assignment",
            "Templates frogtommy{0} and leg are out of order.",
            s.formatAndAppend(
                    params,
                    UPRV_LENGTHOF(params),
                    appendTo,
                    NULL,
                    0,
                    status));

    // Copy constructor
    SimpleFormatter r(fmt);
    appendTo.remove();
    assertEquals(
            "Copy constructor",
            "Templates frogtommy{0} and leg are out of order.",
            r.formatAndAppend(
                    params,
                    UPRV_LENGTHOF(params),
                    appendTo,
                    NULL,
                    0,
                    status));
    r.applyPattern("{0} meter", status);
    assertEquals("getArgumentLimit", 1, r.getArgumentLimit());
    appendTo.remove();
    assertEquals(
            "Replace with new applyPattern",
            "freddy meter",
            r.format("freddy", appendTo, status));
    r.applyPattern("{0}, {1}", status);
    assertEquals("getArgumentLimit", 2, r.getArgumentLimit());
    appendTo.remove();
    assertEquals(
            "2 arg",
            "foo, bar",
            r.format("foo", "bar", appendTo, status));
    r.applyPattern("{0}, {1} and {2}", status);
    assertEquals("getArgumentLimit", 3, r.getArgumentLimit());
    appendTo.remove();
    assertEquals(
            "3 arg",
            "foo, bar and baz",
            r.format("foo", "bar", "baz", appendTo, status));
    assertSuccess("Status", status);
}

void SimpleFormatterTest::TestTooFewArgumentValues() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleFormatter fmt("{0} and {1}", status);
    UnicodeString appendTo;
    UnicodeString firstValue;
    UnicodeString *params[] = {&firstValue};

    fmt.format(
            firstValue, appendTo, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR");
    }

    status = U_ZERO_ERROR;
    fmt.formatAndAppend(
            params, UPRV_LENGTHOF(params), appendTo, NULL, 0, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR");
    }

    status = U_ZERO_ERROR;
    fmt.formatAndReplace(
            params, UPRV_LENGTHOF(params), appendTo, NULL, 0, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR");
    }
}

void SimpleFormatterTest::TestBadArguments() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleFormatter fmt("pickle", status);
    UnicodeString appendTo;

    // These succeed
    fmt.formatAndAppend(
            NULL, 0, appendTo, NULL, 0, status);
    fmt.formatAndReplace(
            NULL, 0, appendTo, NULL, 0, status);
    assertSuccess("", status);
    status = U_ZERO_ERROR;

    // fails
    fmt.formatAndAppend(
            NULL, 1, appendTo, NULL, 0, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR: formatAndAppend() values=NULL but length=1");
    }
    status = U_ZERO_ERROR;
   
    // fails
    fmt.formatAndAppend(
            NULL, 0, appendTo, NULL, 1, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR: formatAndAppend() offsets=NULL but length=1");
    }
    status = U_ZERO_ERROR;

    // fails because appendTo used as a parameter value
    SimpleFormatter fmt2("Arguments {0} and {1}", status);
    UnicodeString frog("frog");
    const UnicodeString *params[] = { &appendTo, &frog };
    fmt2.formatAndAppend(params, 2, appendTo, NULL, 0, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR: formatAndAppend() value=appendTo");
    }
    status = U_ZERO_ERROR;

   
    // fails
    fmt.formatAndReplace(
            NULL, 1, appendTo, NULL, 0, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR: formatAndReplace() values=NULL but length=1");
    }
    status = U_ZERO_ERROR;
   
    // fails
    fmt.formatAndReplace(
            NULL, 0, appendTo, NULL, 1, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR: formatAndReplace() offsets=NULL but length=1");
    }
}

void SimpleFormatterTest::TestTextWithNoArguments() {
    IcuTestErrorCode status(*this, "TestTextWithNoArguments");
    SimpleFormatter fmt("{0} has no {1} arguments.", status);
    assertEquals("String output 1",
        " has no  arguments.", fmt.getTextWithNoArguments());

    // Test offset positions
    int32_t offsets[3];
    assertEquals("String output 2",
        u" has no  arguments.", fmt.getTextWithNoArguments(offsets, 3));
    assertEquals("Offset at 0",
        0, offsets[0]);
    assertEquals("Offset at 1",
        8, offsets[1]);
    assertEquals("Offset at 2",
        -1, offsets[2]);
}

void SimpleFormatterTest::TestFormatReplaceNoOptimization() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleFormatter fmt;
    fmt.applyPattern("{2}, {0}, {1} and {3}", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    UnicodeString result("original");
    int32_t offsets[4];
    UnicodeString freddy("freddy");
    UnicodeString frog("frog");
    UnicodeString by("by");
    const UnicodeString *params[] = {&result, &freddy, &frog, &by};
    assertEquals(
            "",
            "frog, original, freddy and by",
            fmt.formatAndReplace(
                    params,
                    UPRV_LENGTHOF(params),
                    result,
                    offsets,
                    UPRV_LENGTHOF(offsets),
                    status));
    if (!assertSuccess("Status", status)) {
        return;
    }
    int32_t expectedOffsets[] = {6, 16, 0, 27};
    verifyOffsets(expectedOffsets, offsets, UPRV_LENGTHOF(expectedOffsets));
}

void SimpleFormatterTest::TestFormatReplaceNoOptimizationLeadingText() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleFormatter fmt;
    fmt.applyPattern("boo {2}, {0}, {1} and {3}", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    UnicodeString result("original");
    int32_t offsets[4];
    UnicodeString freddy("freddy");
    UnicodeString frog("frog");
    UnicodeString by("by");
    const UnicodeString *params[] = {&freddy, &frog, &result, &by};
    assertEquals(
            "",
            "boo original, freddy, frog and by",
            fmt.formatAndReplace(
                    params,
                    UPRV_LENGTHOF(params),
                    result,
                    offsets,
                    UPRV_LENGTHOF(offsets),
                    status));
    if (!assertSuccess("Status", status)) {
        return;
    }
    int32_t expectedOffsets[] = {14, 22, 4, 31};
    verifyOffsets(expectedOffsets, offsets, UPRV_LENGTHOF(expectedOffsets));
}

void SimpleFormatterTest::TestFormatReplaceOptimization() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleFormatter fmt;
    fmt.applyPattern("{2}, {0}, {1} and {3}", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    UnicodeString result("original");
    int32_t offsets[4];
    UnicodeString freddy("freddy");
    UnicodeString frog("frog");
    UnicodeString by("by");
    const UnicodeString *params[] = {&freddy, &frog, &result, &by};
    assertEquals(
            "",
            "original, freddy, frog and by",
            fmt.formatAndReplace(
                    params,
                    UPRV_LENGTHOF(params),
                    result,
                    offsets,
                    UPRV_LENGTHOF(offsets),
                    status));
    if (!assertSuccess("Status", status)) {
        return;
    }
    int32_t expectedOffsets[] = {10, 18, 0, 27};
    verifyOffsets(expectedOffsets, offsets, UPRV_LENGTHOF(expectedOffsets));
}

void SimpleFormatterTest::TestFormatReplaceNoOptimizationLeadingArgumentUsedTwice() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleFormatter fmt;
    fmt.applyPattern("{2}, {0}, {1} and {3} {2}", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    UnicodeString result("original");
    int32_t offsets[4];
    UnicodeString freddy("freddy");
    UnicodeString frog("frog");
    UnicodeString by("by");
    const UnicodeString *params[] = {&freddy, &frog, &result, &by};
    assertEquals(
            "",
            "original, freddy, frog and by original",
            fmt.formatAndReplace(
                    params,
                    UPRV_LENGTHOF(params),
                    result,
                    offsets,
                    UPRV_LENGTHOF(offsets),
                    status));
    if (!assertSuccess("Status", status)) {
        return;
    }
    int32_t expectedOffsets[] = {10, 18, 30, 27};
    verifyOffsets(expectedOffsets, offsets, UPRV_LENGTHOF(expectedOffsets));
}

void SimpleFormatterTest::TestFormatReplaceOptimizationNoOffsets() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleFormatter fmt;
    fmt.applyPattern("{2}, {0}, {1} and {3}", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    UnicodeString result("original");
    UnicodeString freddy("freddy");
    UnicodeString frog("frog");
    UnicodeString by("by");
    const UnicodeString *params[] = {&freddy, &frog, &result, &by};
    assertEquals(
            "",
            "original, freddy, frog and by",
            fmt.formatAndReplace(
                    params,
                    UPRV_LENGTHOF(params),
                    result,
                    NULL,
                    0,
                    status));
    assertSuccess("Status", status);
}

void SimpleFormatterTest::TestFormatReplaceNoOptimizationNoOffsets() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleFormatter fmt("Arguments {0} and {1}", status);
    UnicodeString result("previous:");
    UnicodeString frog("frog");
    const UnicodeString *params[] = {&result, &frog};
    assertEquals(
            "",
            "Arguments previous: and frog",
            fmt.formatAndReplace(
                    params,
                    UPRV_LENGTHOF(params),
                    result,
                    NULL,
                    0,
                    status));
    assertSuccess("Status", status);
}

void SimpleFormatterTest::TestQuotingLikeMessageFormat() {
#if !UCONFIG_NO_FORMATTING
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString pattern = "{0} don't can''t '{5}''}{a' again '}'{1} to the '{end";
    SimpleFormatter spf(pattern, status);
    MessageFormat mf(pattern, Locale::getRoot(), status);
    UnicodeString expected = "X don't can't {5}'}{a again }Y to the {end";
    UnicodeString x("X"), y("Y");
    Formattable values[] = { x, y };
    UnicodeString result;
    FieldPosition ignore(FieldPosition::DONT_CARE);
    assertEquals("MessageFormat", expected, mf.format(values, 2, result, ignore, status));
    assertEquals("SimpleFormatter", expected, spf.format(x, y, result.remove(), status));
#endif /* !UCONFIG_NO_FORMATTING */
}

void SimpleFormatterTest::verifyOffsets(
        const int32_t *expected, const int32_t *actual, int32_t count) {
    for (int32_t i = 0; i < count; ++i) {
        if (expected[i] != actual[i]) {
            errln("Expected %d, got %d", expected[i], actual[i]);
        }
    }
}

extern IntlTest *createSimpleFormatterTest() {
    return new SimpleFormatterTest();
}
