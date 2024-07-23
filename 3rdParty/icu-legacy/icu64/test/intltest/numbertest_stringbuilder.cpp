// © 2017 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "putilimp.h"
#include "numbertest.h"

static const char16_t *EXAMPLE_STRINGS[] = {
        u"",
        u"xyz",
        u"The quick brown fox jumps over the lazy dog",
        u"😁",
        u"mixed 😇 and ASCII",
        u"with combining characters like 🇦🇧🇨🇩",
        u"A very very very very very very very very very very long string to force heap"};

void NumberStringBuilderTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite NumberStringBuilderTest: ");
    }
    TESTCASE_AUTO_BEGIN;
        TESTCASE_AUTO(testInsertAppendUnicodeString);
        TESTCASE_AUTO(testSplice);
        TESTCASE_AUTO(testInsertAppendCodePoint);
        TESTCASE_AUTO(testCopy);
        TESTCASE_AUTO(testFields);
        TESTCASE_AUTO(testUnlimitedCapacity);
        TESTCASE_AUTO(testCodePoints);
    TESTCASE_AUTO_END;
}

void NumberStringBuilderTest::testInsertAppendUnicodeString() {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString sb1;
    NumberStringBuilder sb2;
    for (const char16_t* strPtr : EXAMPLE_STRINGS) {
        UnicodeString str(strPtr);

        NumberStringBuilder sb3;
        sb1.append(str);
        // Note: UNUM_FIELD_COUNT is like passing null in Java
        sb2.append(str, UNUM_FIELD_COUNT, status);
        assertSuccess("Appending to sb2", status);
        sb3.append(str, UNUM_FIELD_COUNT, status);
        assertSuccess("Appending to sb3", status);
        assertEqualsImpl(sb1, sb2);
        assertEqualsImpl(str, sb3);

        UnicodeString sb4;
        NumberStringBuilder sb5;
        sb4.append(u"😇");
        sb4.append(str);
        sb4.append(u"xx");
        sb5.append(u"😇xx", UNUM_FIELD_COUNT, status);
        assertSuccess("Appending to sb5", status);
        sb5.insert(2, str, UNUM_FIELD_COUNT, status);
        assertSuccess("Inserting into sb5", status);
        assertEqualsImpl(sb4, sb5);

        int start = uprv_min(1, str.length());
        int end = uprv_min(10, str.length());
        sb4.insert(3, str, start, end - start); // UnicodeString uses length instead of end index
        sb5.insert(3, str, start, end, UNUM_FIELD_COUNT, status);
        assertSuccess("Inserting into sb5 again", status);
        assertEqualsImpl(sb4, sb5);

        UnicodeString sb4cp(sb4);
        NumberStringBuilder sb5cp(sb5);
        sb4.append(sb4cp);
        sb5.append(sb5cp, status);
        assertSuccess("Appending again to sb5", status);
        assertEqualsImpl(sb4, sb5);
    }
}

void NumberStringBuilderTest::testSplice() {
    static const struct TestCase {
        const char16_t* input;
        const int32_t startThis;
        const int32_t endThis;
    } cases[] = {
            { u"", 0, 0 },
            { u"abc", 0, 0 },
            { u"abc", 1, 1 },
            { u"abc", 1, 2 },
            { u"abc", 0, 2 },
            { u"abc", 0, 3 },
            { u"lorem ipsum dolor sit amet", 8, 8 },
            { u"lorem ipsum dolor sit amet", 8, 11 }, // 3 chars, equal to replacement "xyz"
            { u"lorem ipsum dolor sit amet", 8, 18 } }; // 10 chars, larger than several replacements

    UErrorCode status = U_ZERO_ERROR;
    UnicodeString sb1;
    NumberStringBuilder sb2;
    for (auto cas : cases) {
        for (const char16_t* replacementPtr : EXAMPLE_STRINGS) {
            UnicodeString replacement(replacementPtr);

            // Test replacement with full string
            sb1.remove();
            sb1.append(cas.input);
            sb1.replace(cas.startThis, cas.endThis - cas.startThis, replacement);
            sb2.clear();
            sb2.append(cas.input, UNUM_FIELD_COUNT, status);
            sb2.splice(cas.startThis, cas.endThis, replacement, 0, replacement.length(), UNUM_FIELD_COUNT, status);
            assertSuccess("Splicing into sb2 first time", status);
            assertEqualsImpl(sb1, sb2);

            // Test replacement with partial string
            if (replacement.length() <= 2) {
                continue;
            }
            sb1.remove();
            sb1.append(cas.input);
            sb1.replace(cas.startThis, cas.endThis - cas.startThis, UnicodeString(replacement, 1, 2));
            sb2.clear();
            sb2.append(cas.input, UNUM_FIELD_COUNT, status);
            sb2.splice(cas.startThis, cas.endThis, replacement, 1, 3, UNUM_FIELD_COUNT, status);
            assertSuccess("Splicing into sb2 second time", status);
            assertEqualsImpl(sb1, sb2);
        }
    }
}

void NumberStringBuilderTest::testInsertAppendCodePoint() {
    static const UChar32 cases[] = {
            0, 1, 60, 127, 128, 0x7fff, 0x8000, 0xffff, 0x10000, 0x1f000, 0x10ffff};
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString sb1;
    NumberStringBuilder sb2;
    for (UChar32 cas : cases) {
        NumberStringBuilder sb3;
        sb1.append(cas);
        sb2.appendCodePoint(cas, UNUM_FIELD_COUNT, status);
        assertSuccess("Appending to sb2", status);
        sb3.appendCodePoint(cas, UNUM_FIELD_COUNT, status);
        assertSuccess("Appending to sb3", status);
        assertEqualsImpl(sb1, sb2);
        assertEquals("Length of sb3", U16_LENGTH(cas), sb3.length());
        assertEquals("Code point count of sb3", 1, sb3.codePointCount());
        assertEquals(
                "First code unit in sb3",
                !U_IS_SUPPLEMENTARY(cas) ? (char16_t) cas : U16_LEAD(cas),
                sb3.charAt(0));

        UnicodeString sb4;
        NumberStringBuilder sb5;
        sb4.append(u"😇xx");
        sb4.insert(2, cas);
        sb5.append(u"😇xx", UNUM_FIELD_COUNT, status);
        assertSuccess("Appending to sb5", status);
        sb5.insertCodePoint(2, cas, UNUM_FIELD_COUNT, status);
        assertSuccess("Inserting into sb5", status);
        assertEqualsImpl(sb4, sb5);
    }
}

void NumberStringBuilderTest::testCopy() {
    UErrorCode status = U_ZERO_ERROR;
    for (UnicodeString str : EXAMPLE_STRINGS) {
        NumberStringBuilder sb1;
        sb1.append(str, UNUM_FIELD_COUNT, status);
        assertSuccess("Appending to sb1 first time", status);
        NumberStringBuilder sb2(sb1);
        assertTrue("Content should equal itself", sb1.contentEquals(sb2));

        sb1.append("12345", UNUM_FIELD_COUNT, status);
        assertSuccess("Appending to sb1 second time", status);
        assertFalse("Content should no longer equal itself", sb1.contentEquals(sb2));
    }
}

void NumberStringBuilderTest::testFields() {
    UErrorCode status = U_ZERO_ERROR;
    // Note: This is a C++11 for loop that calls the UnicodeString constructor on each iteration.
    for (UnicodeString str : EXAMPLE_STRINGS) {
        NumberStringBuilder sb;
        sb.append(str, UNUM_FIELD_COUNT, status);
        assertSuccess("Appending to sb", status);
        sb.append(str, UNUM_CURRENCY_FIELD, status);
        assertSuccess("Appending to sb", status);
        assertEquals("Reference string copied twice", str.length() * 2, sb.length());
        for (int32_t i = 0; i < str.length(); i++) {
            assertEquals("Null field first", (Field) UNUM_FIELD_COUNT, sb.fieldAt(i));
            assertEquals("Currency field second", (Field) UNUM_CURRENCY_FIELD, sb.fieldAt(i + str.length()));
        }

        // Very basic FieldPosition test. More robust tests happen in NumberFormatTest.
        // Let NumberFormatTest also take care of FieldPositionIterator material.
        FieldPosition fp(UNUM_CURRENCY_FIELD);
        sb.nextFieldPosition(fp, status);
        assertSuccess("Populating the FieldPosition", status);
        assertEquals("Currency start position", str.length(), fp.getBeginIndex());
        assertEquals("Currency end position", str.length() * 2, fp.getEndIndex());

        if (str.length() > 0) {
            sb.insertCodePoint(2, 100, UNUM_INTEGER_FIELD, status);
            assertSuccess("Inserting code point into sb", status);
            assertEquals("New length", str.length() * 2 + 1, sb.length());
            assertEquals("Integer field", (Field) UNUM_INTEGER_FIELD, sb.fieldAt(2));
        }

        NumberStringBuilder old(sb);
        sb.append(old, status);
        assertSuccess("Appending to myself", status);
        int32_t numNull = 0;
        int32_t numCurr = 0;
        int32_t numInt = 0;
        for (int32_t i = 0; i < sb.length(); i++) {
            Field field = sb.fieldAt(i);
            assertEquals("Field should equal location in old", old.fieldAt(i % old.length()), field);
            if (field == UNUM_FIELD_COUNT) {
                numNull++;
            } else if (field == UNUM_CURRENCY_FIELD) {
                numCurr++;
            } else if (field == UNUM_INTEGER_FIELD) {
                numInt++;
            } else {
                errln("Encountered unknown field");
            }
        }
        assertEquals("Number of null fields", str.length() * 2, numNull);
        assertEquals("Number of currency fields", numNull, numCurr);
        assertEquals("Number of integer fields", str.length() > 0 ? 2 : 0, numInt);
    }
}

void NumberStringBuilderTest::testUnlimitedCapacity() {
    UErrorCode status = U_ZERO_ERROR;
    NumberStringBuilder builder;
    // The builder should never fail upon repeated appends.
    for (int i = 0; i < 1000; i++) {
        UnicodeString message("Iteration #");
        message += Int64ToUnicodeString(i);
        assertEquals(message, builder.length(), i);
        builder.appendCodePoint(u'x', UNUM_FIELD_COUNT, status);
        assertSuccess(message, status);
        assertEquals(message, builder.length(), i + 1);
    }
}

void NumberStringBuilderTest::testCodePoints() {
    UErrorCode status = U_ZERO_ERROR;
    NumberStringBuilder nsb;
    assertEquals("First is -1 on empty string", -1, nsb.getFirstCodePoint());
    assertEquals("Last is -1 on empty string", -1, nsb.getLastCodePoint());
    assertEquals("Length is 0 on empty string", 0, nsb.codePointCount());

    nsb.append(u"q", UNUM_FIELD_COUNT, status);
    assertSuccess("Spot 1", status);
    assertEquals("First is q", u'q', nsb.getFirstCodePoint());
    assertEquals("Last is q", u'q', nsb.getLastCodePoint());
    assertEquals("0th is q", u'q', nsb.codePointAt(0));
    assertEquals("Before 1st is q", u'q', nsb.codePointBefore(1));
    assertEquals("Code point count is 1", 1, nsb.codePointCount());

    // 🚀 is two char16s
    nsb.append(u"🚀", UNUM_FIELD_COUNT, status);
    assertSuccess("Spot 2" ,status);
    assertEquals("First is still q", u'q', nsb.getFirstCodePoint());
    assertEquals("Last is space ship", 128640, nsb.getLastCodePoint());
    assertEquals("1st is space ship", 128640, nsb.codePointAt(1));
    assertEquals("Before 1st is q", u'q', nsb.codePointBefore(1));
    assertEquals("Before 3rd is space ship", 128640, nsb.codePointBefore(3));
    assertEquals("Code point count is 2", 2, nsb.codePointCount());
}

void NumberStringBuilderTest::assertEqualsImpl(const UnicodeString &a, const NumberStringBuilder &b) {
    // TODO: Why won't this compile without the IntlTest:: qualifier?
    IntlTest::assertEquals("Lengths should be the same", a.length(), b.length());
    IntlTest::assertEquals("Code point counts should be the same", a.countChar32(), b.codePointCount());

    if (a.length() != b.length()) {
        return;
    }

    for (int32_t i = 0; i < a.length(); i++) {
        IntlTest::assertEquals(
                UnicodeString(u"Char at position ") + Int64ToUnicodeString(i) +
                UnicodeString(u" in \"") + a + UnicodeString("\" versus \"") +
                b.toUnicodeString() + UnicodeString("\""), a.charAt(i), b.charAt(i));
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */
