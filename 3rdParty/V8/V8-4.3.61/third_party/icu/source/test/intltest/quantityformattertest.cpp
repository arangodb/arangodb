/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File QUANTITYFORMATTERTEST.CPP
*
********************************************************************************
*/
#include "cstring.h"
#include "intltest.h"
#include "quantityformatter.h"
#include "simplepatternformatter.h"
#include "unicode/numfmt.h"
#include "unicode/plurrule.h"


class QuantityFormatterTest : public IntlTest {
public:
    QuantityFormatterTest() {
    }
    void TestBasic();
    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
};

void QuantityFormatterTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/) {
  TESTCASE_AUTO_BEGIN;
  TESTCASE_AUTO(TestBasic);
  TESTCASE_AUTO_END;
}

void QuantityFormatterTest::TestBasic() {
    UErrorCode status = U_ZERO_ERROR;
#if !UCONFIG_NO_FORMATTING
    QuantityFormatter fmt;
    assertFalse(
            "adding bad variant",
            fmt.add("a bad variant", "{0} pounds", status));
    assertEquals("adding bad variant status", U_ILLEGAL_ARGUMENT_ERROR, status);
    status = U_ZERO_ERROR;
    assertFalse(
            "Adding bad pattern",
            fmt.add("other", "{0} {1} too many placeholders", status));
    assertEquals("adding bad pattern status", U_ILLEGAL_ARGUMENT_ERROR, status);
    status = U_ZERO_ERROR;
    assertFalse("isValid with no patterns", fmt.isValid());
    assertTrue(
            "Adding good pattern with no placeholders",
            fmt.add("other", "no placeholder", status));
    assertTrue(
            "Adding good pattern",
            fmt.add("other", "{0} pounds", status));
    assertTrue("isValid with other", fmt.isValid());
    assertTrue(
            "Adding good pattern",
            fmt.add("one", "{0} pound", status));

    assertEquals(
            "getByVariant",
            fmt.getByVariant("bad variant")->getPatternWithNoPlaceholders(),
            " pounds");
    assertEquals(
            "getByVariant",
            fmt.getByVariant("other")->getPatternWithNoPlaceholders(),
            " pounds");
    assertEquals(
            "getByVariant",
            fmt.getByVariant("one")->getPatternWithNoPlaceholders(),
            " pound");
    assertEquals(
            "getByVariant",
            fmt.getByVariant("few")->getPatternWithNoPlaceholders(),
            " pounds");

    // Test copy constructor
    {
        QuantityFormatter copied(fmt);
        assertEquals(
                "copied getByVariant",
                copied.getByVariant("other")->getPatternWithNoPlaceholders(),
                " pounds");
        assertEquals(
                "copied getByVariant",
                copied.getByVariant("one")->getPatternWithNoPlaceholders(),
                " pound");
        assertEquals(
                "copied getByVariant",
                copied.getByVariant("few")->getPatternWithNoPlaceholders(),
                " pounds");
    }
        
    // Test assignment
    {
        QuantityFormatter assigned;
        assigned = fmt;
        assertEquals(
                "assigned getByVariant",
                assigned.getByVariant("other")->getPatternWithNoPlaceholders(),
                " pounds");
        assertEquals(
                "assigned getByVariant",
                assigned.getByVariant("one")->getPatternWithNoPlaceholders(),
                " pound");
        assertEquals(
                "assigned getByVariant",
                assigned.getByVariant("few")->getPatternWithNoPlaceholders(),
                " pounds");
    }

    // Test format.
    {
        LocalPointer<NumberFormat> numfmt(
                NumberFormat::createInstance(Locale::getEnglish(), status));
        LocalPointer<PluralRules> plurrule(
                PluralRules::forLocale("en", status));
        FieldPosition pos(FieldPosition::DONT_CARE);
        UnicodeString appendTo;
        assertEquals(
                "format singular",
                "1 pound",
                fmt.format(
                        1,
                        *numfmt,
                        *plurrule,
                        appendTo,
                        pos,
                        status), TRUE);
        appendTo.remove();
        assertEquals(
                "format plural",
                "2 pounds",
                fmt.format(
                        2,
                        *numfmt,
                        *plurrule,
                        appendTo,
                        pos,
                        status), TRUE);
    }
    fmt.reset();
    assertFalse("isValid after reset", fmt.isValid());
#endif
    assertSuccess("", status);
}

extern IntlTest *createQuantityFormatterTest() {
    return new QuantityFormatterTest();
}
