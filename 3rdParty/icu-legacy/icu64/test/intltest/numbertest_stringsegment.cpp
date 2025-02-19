// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "numbertest.h"
#include "numparse_stringsegment.h"

static const char16_t* SAMPLE_STRING = u"📻 radio 📻";

void StringSegmentTest::runIndexedTest(int32_t index, UBool exec, const char*&name, char*) {
    if (exec) {
        logln("TestSuite StringSegmentTest: ");
    }
    TESTCASE_AUTO_BEGIN;
        TESTCASE_AUTO(testOffset);
        TESTCASE_AUTO(testLength);
        TESTCASE_AUTO(testCharAt);
        TESTCASE_AUTO(testGetCodePoint);
        TESTCASE_AUTO(testCommonPrefixLength);
    TESTCASE_AUTO_END;
}

void StringSegmentTest::testOffset() {
    // Note: sampleString needs function scope so it is valid while the StringSegment is valid
    UnicodeString sampleString(SAMPLE_STRING);
    StringSegment segment(sampleString, false);
    assertEquals("Initial Offset", 0, segment.getOffset());
    segment.adjustOffset(3);
    assertEquals("Adjust A", 3, segment.getOffset());
    segment.adjustOffset(2);
    assertEquals("Adjust B", 5, segment.getOffset());
    segment.setOffset(4);
    assertEquals("Set Offset", 4, segment.getOffset());
}

void StringSegmentTest::testLength() {
    // Note: sampleString needs function scope so it is valid while the StringSegment is valid
    UnicodeString sampleString(SAMPLE_STRING);
    StringSegment segment(sampleString, false);
    assertEquals("Initial length", 11, segment.length());
    segment.adjustOffset(3);
    assertEquals("Adjust", 8, segment.length());
    segment.setLength(4);
    assertEquals("Set Length", 4, segment.length());
    segment.setOffset(5);
    assertEquals("After adjust offset", 2, segment.length());
    segment.resetLength();
    assertEquals("After reset length", 6, segment.length());
}

void StringSegmentTest::testCharAt() {
    // Note: sampleString needs function scope so it is valid while the StringSegment is valid
    UnicodeString sampleString(SAMPLE_STRING);
    StringSegment segment(sampleString, false);
    assertEquals("Initial", SAMPLE_STRING, segment.toUnicodeString());
    assertEquals("Initial", SAMPLE_STRING, segment.toTempUnicodeString());
    segment.adjustOffset(3);
    assertEquals("After adjust-offset", UnicodeString(u"radio 📻"), segment.toUnicodeString());
    assertEquals("After adjust-offset", UnicodeString(u"radio 📻"), segment.toTempUnicodeString());
    segment.setLength(5);
    assertEquals("After adjust-length", UnicodeString(u"radio"), segment.toUnicodeString());
    assertEquals("After adjust-length", UnicodeString(u"radio"), segment.toTempUnicodeString());
}

void StringSegmentTest::testGetCodePoint() {
    // Note: sampleString needs function scope so it is valid while the StringSegment is valid
    UnicodeString sampleString(SAMPLE_STRING);
    StringSegment segment(sampleString, false);
    assertEquals("Double-width code point", 0x1F4FB, segment.getCodePoint());
    segment.setLength(1);
    assertEquals("Inalid A", -1, segment.getCodePoint());
    segment.resetLength();
    segment.adjustOffset(1);
    assertEquals("Invalid B", -1, segment.getCodePoint());
    segment.adjustOffset(1);
    assertEquals("Valid again", 0x20, segment.getCodePoint());
}

void StringSegmentTest::testCommonPrefixLength() {
    // Note: sampleString needs function scope so it is valid while the StringSegment is valid
    UnicodeString sampleString(SAMPLE_STRING);
    StringSegment segment(sampleString, false);
    assertEquals("", 11, segment.getCommonPrefixLength(SAMPLE_STRING));
    assertEquals("", 4, segment.getCommonPrefixLength(u"📻 r"));
    assertEquals("", 3, segment.getCommonPrefixLength(u"📻 x"));
    assertEquals("", 0, segment.getCommonPrefixLength(u"x"));
    segment.adjustOffset(3);
    assertEquals("", 0, segment.getCommonPrefixLength(u"RADiO"));
    assertEquals("", 5, segment.getCommonPrefixLength(u"radio"));
    assertEquals("", 2, segment.getCommonPrefixLength(u"rafio"));
    assertEquals("", 0, segment.getCommonPrefixLength(u"fadio"));
    segment.setLength(3);
    assertEquals("", 3, segment.getCommonPrefixLength(u"radio"));
    assertEquals("", 2, segment.getCommonPrefixLength(u"rafio"));
    assertEquals("", 0, segment.getCommonPrefixLength(u"fadio"));
    segment.resetLength();
    segment.setOffset(11); // end of string
    assertEquals("", 0, segment.getCommonPrefixLength(u"foo"));
}

#endif
