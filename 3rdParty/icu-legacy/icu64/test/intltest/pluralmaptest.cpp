// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2014-2015, International Business Machines Corporation and    *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File PLURALMAPTEST.CPP
*
********************************************************************************
*/
#include "unicode/unistr.h"

#include "intltest.h"
#include "pluralmap.h"

class PluralMapForPluralMapTest : public PluralMap<UnicodeString> {
public:
    UBool operator==(const PluralMapForPluralMapTest &other) {
        return equals(other, strEqual);
    }
private:
    static UBool strEqual(const UnicodeString &lhs, const UnicodeString &rhs) {
        return lhs == rhs;
    }
};

class PluralMapTest : public IntlTest {
public:
    PluralMapTest() {
    }
    void TestToCategory();
    void TestGetCategoryName();
    void TestGet();
    void TestIterate();
    void TestEqual();
    void TestCopyAndAssign();
    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
    void addVariant(
            PluralMapBase::Category v,
            const UnicodeString &value,
            PluralMapForPluralMapTest &map);
private:
};

void PluralMapTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/) {
  TESTCASE_AUTO_BEGIN;
  TESTCASE_AUTO(TestToCategory);
  TESTCASE_AUTO(TestGetCategoryName);
  TESTCASE_AUTO(TestGet);
  TESTCASE_AUTO(TestIterate);
  TESTCASE_AUTO(TestEqual);
  TESTCASE_AUTO(TestCopyAndAssign);
  TESTCASE_AUTO_END;
}

void PluralMapTest::TestToCategory() {
    assertEquals("", (int32_t)PluralMapBase::OTHER, PluralMapBase::toCategory("other"));
    assertEquals("", (int32_t)PluralMapBase::ZERO, PluralMapBase::toCategory("zero"));
    assertEquals("", (int32_t)PluralMapBase::ONE, PluralMapBase::toCategory("one"));
    assertEquals("", (int32_t)PluralMapBase::TWO, PluralMapBase::toCategory("two"));
    assertEquals("", (int32_t)PluralMapBase::FEW, PluralMapBase::toCategory("few"));
    assertEquals("", (int32_t)PluralMapBase::MANY, PluralMapBase::toCategory("many"));
    assertEquals("", (int32_t)PluralMapBase::NONE, PluralMapBase::toCategory("Many"));
    assertEquals(
            "",
            (int32_t)PluralMapBase::FEW,
            PluralMapBase::toCategory(UnicodeString("few")));
    assertEquals(
            "",
            (int32_t)PluralMapBase::MANY,
            PluralMapBase::toCategory(UnicodeString("many")));
    assertEquals(
            "",
            (int32_t)PluralMapBase::NONE,
            PluralMapBase::toCategory(UnicodeString("Many")));
}

void PluralMapTest::TestGetCategoryName() {
    assertTrue("", PluralMapBase::getCategoryName(PluralMapBase::NONE) == NULL);
    assertTrue("", PluralMapBase::getCategoryName(PluralMapBase::CATEGORY_COUNT) == NULL);
    assertEquals("", "other", PluralMapBase::getCategoryName(PluralMapBase::OTHER));
    assertEquals("", "zero", PluralMapBase::getCategoryName(PluralMapBase::ZERO));
    assertEquals("", "one", PluralMapBase::getCategoryName(PluralMapBase::ONE));
    assertEquals("", "two", PluralMapBase::getCategoryName(PluralMapBase::TWO));
    assertEquals("", "few", PluralMapBase::getCategoryName(PluralMapBase::FEW));
    assertEquals("", "many", PluralMapBase::getCategoryName(PluralMapBase::MANY));
}

void PluralMapTest::TestGet() {
    PluralMapForPluralMapTest map;
    addVariant(PluralMapBase::OTHER, "pickles", map);
    addVariant(PluralMapBase::ONE, "pickle", map);
    addVariant(PluralMapBase::FEW, "picklefew", map);
    assertEquals("", "pickles", map.get(PluralMapBase::OTHER));
    assertEquals("", "pickle", map.get(PluralMapBase::ONE));
    assertEquals("", "picklefew", map.get(PluralMapBase::FEW));
    assertEquals("", "pickles", map.get(PluralMapBase::MANY));
    assertEquals("", "pickles", map.get(PluralMapBase::NONE));
    assertEquals("", "pickles", map.get(PluralMapBase::CATEGORY_COUNT));
    assertEquals("", "picklefew", map.get("few"));
    assertEquals("", "pickles", map.get("many"));
    assertEquals("", "pickles", map.get("somebadform"));
    assertEquals("", "pickle", map.get(UnicodeString("one")));
    assertEquals("", "pickles", map.get(UnicodeString("many")));
    assertEquals("", "pickles", map.get(UnicodeString("somebadform")));
    assertEquals("", "pickles", map.getOther());
}

void PluralMapTest::TestIterate() {
    PluralMapForPluralMapTest map;
    addVariant(PluralMapBase::OTHER, "pickles", map);
    addVariant(PluralMapBase::ONE, "pickle", map);
    addVariant(PluralMapBase::FEW, "pickleops", map);
    addVariant(PluralMapBase::FEW, "picklefew", map);
    PluralMapBase::Category index = PluralMapBase::NONE;
    const UnicodeString *current = map.next(index);
    assertEquals("", "pickles", *current);
    assertEquals("", (int32_t)PluralMapBase::OTHER, index);
    current = map.next(index);
    assertEquals("", "pickle", *current);
    assertEquals("", (int32_t)PluralMapBase::ONE, index);
    current = map.next(index);
    assertEquals("", "picklefew", *current);
    assertEquals("", (int32_t)PluralMapBase::FEW, index);
    current = map.next(index);
    assertEquals("", (int32_t)PluralMapBase::CATEGORY_COUNT, index);
    assertTrue("", current == NULL);

    PluralMapForPluralMapTest map2;
    index = PluralMapBase::NONE;
    current = map2.next(index);
    assertEquals("", "", *current);
    assertEquals("", (int32_t)PluralMapBase::OTHER, index);
    current = map2.next(index);
    assertEquals("", (int32_t)PluralMapBase::CATEGORY_COUNT, index);
    assertTrue("", current == NULL);
}

void PluralMapTest::TestEqual() {
    PluralMapForPluralMapTest control;
    addVariant(PluralMapBase::OTHER, "pickles", control);
    addVariant(PluralMapBase::ONE, "pickle", control);
    addVariant(PluralMapBase::FEW, "picklefew", control);

    {
        PluralMapForPluralMapTest map;
        addVariant(PluralMapBase::FEW, "picklefew", map);
        addVariant(PluralMapBase::OTHER, "pickles", map);
        addVariant(PluralMapBase::ONE, "pickle", map);
        assertTrue("", control == map);
        addVariant(PluralMapBase::ONE, "pickl", map);
        assertFalse("", control == map);
    }
    {
        PluralMapForPluralMapTest map;
        addVariant(PluralMapBase::MANY, "picklemany", map);
        addVariant(PluralMapBase::OTHER, "pickles", map);
        addVariant(PluralMapBase::ONE, "pickle", map);
        assertFalse("", control == map);
    }
}

void PluralMapTest::TestCopyAndAssign() {
    PluralMapForPluralMapTest control;
    addVariant(PluralMapBase::OTHER, "pickles", control);
    addVariant(PluralMapBase::ONE, "pickle", control);
    addVariant(PluralMapBase::FEW, "picklefew", control);
    {
        PluralMapForPluralMapTest *rhs = new PluralMapForPluralMapTest();
        if (rhs == NULL) {
            errln("Memory allocation error.");
            return;
        }
        addVariant(PluralMapBase::OTHER, "pickles", *rhs);
        addVariant(PluralMapBase::ONE, "pickle", *rhs);
        addVariant(PluralMapBase::FEW, "picklefew", *rhs);
        PluralMapForPluralMapTest lhs(*rhs);
        delete rhs;
        assertTrue("", lhs == control);
    }
    {
        PluralMapForPluralMapTest *rhs = new PluralMapForPluralMapTest();
        if (rhs == NULL) {
            errln("Memory allocation error.");
            return;
        }
        addVariant(PluralMapBase::OTHER, "pickles", *rhs);
        addVariant(PluralMapBase::ONE, "pickle", *rhs);
        addVariant(PluralMapBase::FEW, "picklefew", *rhs);
        PluralMapForPluralMapTest lhs;
        addVariant(PluralMapBase::OTHER, "pickles", lhs);
        addVariant(PluralMapBase::TWO, "pickletwo", lhs);
        addVariant(PluralMapBase::MANY, "picklemany", lhs);
        addVariant(PluralMapBase::FEW, "picklefew", lhs);
        lhs = *rhs;
        delete rhs;
        assertTrue("", lhs == control);
    }

}



void PluralMapTest::addVariant(
        PluralMapBase::Category v,
        const UnicodeString &value,
        PluralMapForPluralMapTest &map) {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString *current = map.getMutable(v, status);
    if (!assertSuccess("", status)) {
        return;
    }
    (*current) = value;
}

extern IntlTest *createPluralMapTest() {
    return new PluralMapTest();
}
