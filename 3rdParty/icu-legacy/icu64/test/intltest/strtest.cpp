// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2015, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
/*   file name:  strtest.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 1999nov22
*   created by: Markus W. Scherer
*/

#include <string_view>
#include <cstddef>
#include <string.h>
#include <limits>

#include "unicode/utypes.h"
#include "unicode/putil.h"
#include "unicode/std_string.h"
#include "unicode/stringpiece.h"
#include "unicode/unistr.h"
#include "unicode/ustring.h"
#include "unicode/utf_old.h"    // for UTF8_COUNT_TRAIL_BYTES
#include "unicode/utf8.h"
#include "charstr.h"
#include "cstr.h"
#include "intltest.h"
#include "strtest.h"
#include "uinvchar.h"

StringTest::~StringTest() {}

void StringTest::TestEndian() {
    union {
        uint8_t byte;
        uint16_t word;
    } u;
    u.word=0x0100;
    if(U_IS_BIG_ENDIAN!=u.byte) {
        errln("TestEndian: U_IS_BIG_ENDIAN needs to be fixed in platform.h");
    }
}

void StringTest::TestSizeofTypes() {
    if(U_SIZEOF_WCHAR_T!=sizeof(wchar_t)) {
        errln("TestSizeofWCharT: U_SIZEOF_WCHAR_T!=sizeof(wchar_t) - U_SIZEOF_WCHAR_T needs to be fixed in platform.h");
    }
#ifdef U_INT64_T_UNAVAILABLE
    errln("int64_t and uint64_t are undefined.");
#else
    if(8!=sizeof(int64_t)) {
        errln("TestSizeofTypes: 8!=sizeof(int64_t) - int64_t needs to be fixed in platform.h");
    }
    if(8!=sizeof(uint64_t)) {
        errln("TestSizeofTypes: 8!=sizeof(uint64_t) - uint64_t needs to be fixed in platform.h");
    }
#endif
    if(8!=sizeof(double)) {
        errln("8!=sizeof(double) - putil.c code may not work");
    }
    if(4!=sizeof(int32_t)) {
        errln("4!=sizeof(int32_t)");
    }
    if(4!=sizeof(uint32_t)) {
        errln("4!=sizeof(uint32_t)");
    }
    if(2!=sizeof(int16_t)) {
        errln("2!=sizeof(int16_t)");
    }
    if(2!=sizeof(uint16_t)) {
        errln("2!=sizeof(uint16_t)");
    }
    if(2!=sizeof(char16_t)) {
        errln("2!=sizeof(char16_t)");
    }
    if(1!=sizeof(int8_t)) {
        errln("1!=sizeof(int8_t)");
    }
    if(1!=sizeof(uint8_t)) {
        errln("1!=sizeof(uint8_t)");
    }
    if(1!=sizeof(UBool)) {
        errln("1!=sizeof(UBool)");
    }
}

void StringTest::TestCharsetFamily() {
    unsigned char c='A';
    if( (U_CHARSET_FAMILY==U_ASCII_FAMILY && c!=0x41) ||
        (U_CHARSET_FAMILY==U_EBCDIC_FAMILY && c!=0xc1)
    ) {
        errln("TestCharsetFamily: U_CHARSET_FAMILY needs to be fixed in platform.h");
    }
}

U_STRING_DECL(ustringVar, "aZ0 -", 5);

void
StringTest::Test_U_STRING() {
    U_STRING_INIT(ustringVar, "aZ0 -", 5);
    if( u_strlen(ustringVar)!=5 ||
        ustringVar[0]!=0x61 ||
        ustringVar[1]!=0x5a ||
        ustringVar[2]!=0x30 ||
        ustringVar[3]!=0x20 ||
        ustringVar[4]!=0x2d ||
        ustringVar[5]!=0
    ) {
        errln("Test_U_STRING: U_STRING_DECL with U_STRING_INIT does not work right! "
              "See putil.h and utypes.h with platform.h.");
    }
}

void
StringTest::Test_UNICODE_STRING() {
    UnicodeString ustringVar=UNICODE_STRING("aZ0 -", 5);
    if( ustringVar.length()!=5 ||
        ustringVar[0]!=0x61 ||
        ustringVar[1]!=0x5a ||
        ustringVar[2]!=0x30 ||
        ustringVar[3]!=0x20 ||
        ustringVar[4]!=0x2d
    ) {
        errln("Test_UNICODE_STRING: UNICODE_STRING does not work right! "
              "See unistr.h and utypes.h with platform.h.");
    }
}

void
StringTest::Test_UNICODE_STRING_SIMPLE() {
    UnicodeString ustringVar=UNICODE_STRING_SIMPLE("aZ0 -");
    if( ustringVar.length()!=5 ||
        ustringVar[0]!=0x61 ||
        ustringVar[1]!=0x5a ||
        ustringVar[2]!=0x30 ||
        ustringVar[3]!=0x20 ||
        ustringVar[4]!=0x2d
    ) {
        errln("Test_UNICODE_STRING_SIMPLE: UNICODE_STRING_SIMPLE does not work right! "
              "See unistr.h and utypes.h with platform.h.");
    }
}

namespace {

// See U_CHARSET_FAMILY in unicode/platform.h.
const char *nativeInvChars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789 \"%&'()*+,-./:;<=>?_";
const char16_t *asciiInvChars =
    u"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    u"abcdefghijklmnopqrstuvwxyz"
    u"0123456789 \"%&'()*+,-./:;<=>?_";

}  // namespace

void
StringTest::TestUpperOrdinal() {
    for (int32_t i = 0;; ++i) {
        char ic = nativeInvChars[i];
        uint8_t ac = static_cast<uint8_t>(asciiInvChars[i]);
        int32_t expected = ac - 'A';
        int32_t actual = uprv_upperOrdinal(ic);
        if (0 <= expected && expected <= 25) {
            if (actual != expected) {
                errln("uprv_upperOrdinal('%c')=%d != expected %d",
                      ic, (int)actual, (int)expected);
            }
        } else {
            if (0 <= actual && actual <= 25) {
                errln("uprv_upperOrdinal('%c')=%d should have been outside 0..25",
                      ic, (int)actual);
            }
        }
        if (ic == 0) { break; }
    }
}

void
StringTest::TestLowerOrdinal() {
    for (int32_t i = 0;; ++i) {
        char ic = nativeInvChars[i];
        uint8_t ac = static_cast<uint8_t>(asciiInvChars[i]);
        int32_t expected = ac - 'a';
        int32_t actual = uprv_lowerOrdinal(ic);
        if (0 <= expected && expected <= 25) {
            if (actual != expected) {
                errln("uprv_lowerOrdinal('%c')=%d != expected %d",
                      ic, (int)actual, (int)expected);
            }
        } else {
            if (0 <= actual && actual <= 25) {
                errln("uprv_lowerOrdinal('%c')=%d should have been outside 0..25",
                      ic, (int)actual);
            }
        }
        if (ic == 0) { break; }
    }
}

void
StringTest::Test_UTF8_COUNT_TRAIL_BYTES() {
#if !U_HIDE_OBSOLETE_UTF_OLD_H
    if(UTF8_COUNT_TRAIL_BYTES(0x7F) != 0
            || UTF8_COUNT_TRAIL_BYTES(0xC2) != 1
            || UTF8_COUNT_TRAIL_BYTES(0xE0) != 2
            || UTF8_COUNT_TRAIL_BYTES(0xF0) != 3) {
        errln("UTF8_COUNT_TRAIL_BYTES does not work right! See utf_old.h.");
    }
#endif
    // Note: U8_COUNT_TRAIL_BYTES (current) and UTF8_COUNT_TRAIL_BYTES (deprecated)
    //       have completely different implementations.
    if (U8_COUNT_TRAIL_BYTES(0x7F) != 0
            || U8_COUNT_TRAIL_BYTES(0xC2) != 1
            || U8_COUNT_TRAIL_BYTES(0xE0) != 2
            || U8_COUNT_TRAIL_BYTES(0xF0) != 3) {
        errln("U8_COUNT_TRAIL_BYTES does not work right! See utf8.h.");
    }
}

void StringTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) {
    if(exec) {
        logln("TestSuite Character and String Test: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestEndian);
    TESTCASE_AUTO(TestSizeofTypes);
    TESTCASE_AUTO(TestCharsetFamily);
    TESTCASE_AUTO(Test_U_STRING);
    TESTCASE_AUTO(Test_UNICODE_STRING);
    TESTCASE_AUTO(Test_UNICODE_STRING_SIMPLE);
    TESTCASE_AUTO(TestUpperOrdinal);
    TESTCASE_AUTO(TestLowerOrdinal);
    TESTCASE_AUTO(Test_UTF8_COUNT_TRAIL_BYTES);
    TESTCASE_AUTO(TestSTLCompatibility);
    TESTCASE_AUTO(TestStringPiece);
    TESTCASE_AUTO(TestStringPieceComparisons);
    TESTCASE_AUTO(TestStringPieceFind);
    TESTCASE_AUTO(TestStringPieceOther);
    TESTCASE_AUTO(TestStringPieceStringView);
    TESTCASE_AUTO(TestStringPieceU8);
    TESTCASE_AUTO(TestByteSink);
    TESTCASE_AUTO(TestCheckedArrayByteSink);
    TESTCASE_AUTO(TestStringByteSink);
    TESTCASE_AUTO(TestStringByteSinkAppendU8);
    TESTCASE_AUTO(TestCharString);
    TESTCASE_AUTO(TestCStr);
    TESTCASE_AUTO(TestCharStrAppendNumber);
    TESTCASE_AUTO(Testctou);
    TESTCASE_AUTO_END;
}

void
StringTest::TestStringPiece() {
    // Default constructor.
    StringPiece empty;
    if(!empty.empty() || empty.data()!=nullptr || empty.length()!=0 || empty.size()!=0) {
        errln("StringPiece() failed");
    }
    // Construct from nullptr const char * pointer.
    StringPiece null((const char *)nullptr);
    if(!null.empty() || null.data()!=nullptr || null.length()!=0 || null.size()!=0) {
        errln("StringPiece(nullptr) failed");
    }
    // Construct from const char * pointer.
    static const char *abc_chars="abc";
    StringPiece abc(abc_chars);
    if(abc.empty() || abc.data()!=abc_chars || abc.length()!=3 || abc.size()!=3) {
        errln("StringPiece(abc_chars) failed");
    }
    // Construct from const char * pointer and length.
    static const char *abcdefg_chars="abcdefg";
    StringPiece abcd(abcdefg_chars, 4);
    if(abcd.empty() || abcd.data()!=abcdefg_chars || abcd.length()!=4 || abcd.size()!=4) {
        errln("StringPiece(abcdefg_chars, 4) failed");
    }
    // Construct from std::string.
    std::string uvwxyz_string("uvwxyz");
    StringPiece uvwxyz(uvwxyz_string);
    if(uvwxyz.empty() || uvwxyz.data()!=uvwxyz_string.data() || uvwxyz.length()!=6 || uvwxyz.size()!=6) {
        errln("StringPiece(uvwxyz_string) failed");
    }
    // Substring constructor with pos.
    StringPiece sp(abcd, -1);
    if(sp.empty() || sp.data()!=abcdefg_chars || sp.length()!=4 || sp.size()!=4) {
        errln("StringPiece(abcd, -1) failed");
    }
    sp=StringPiece(abcd, 5);
    if(!sp.empty() || sp.length()!=0 || sp.size()!=0) {
        errln("StringPiece(abcd, 5) failed");
    }
    sp=StringPiece(abcd, 2);
    if(sp.empty() || sp.data()!=abcdefg_chars+2 || sp.length()!=2 || sp.size()!=2) {
        errln("StringPiece(abcd, -1) failed");
    }
    // Substring constructor with pos and len.
    sp=StringPiece(abcd, -1, 8);
    if(sp.empty() || sp.data()!=abcdefg_chars || sp.length()!=4 || sp.size()!=4) {
        errln("StringPiece(abcd, -1, 8) failed");
    }
    sp=StringPiece(abcd, 5, 8);
    if(!sp.empty() || sp.length()!=0 || sp.size()!=0) {
        errln("StringPiece(abcd, 5, 8) failed");
    }
    sp=StringPiece(abcd, 2, 8);
    if(sp.empty() || sp.data()!=abcdefg_chars+2 || sp.length()!=2 || sp.size()!=2) {
        errln("StringPiece(abcd, -1) failed");
    }
    sp=StringPiece(abcd, 2, -1);
    if(!sp.empty() || sp.length()!=0 || sp.size()!=0) {
        errln("StringPiece(abcd, 5, -1) failed");
    }
    // static const npos
    const int32_t *ptr_npos=&StringPiece::npos;
    if(StringPiece::npos!=0x7fffffff || *ptr_npos!=0x7fffffff) {
        errln("StringPiece::npos!=0x7fffffff");
    }
    // substr() method with pos, using len=npos.
    sp=abcd.substr(-1);
    if(sp.empty() || sp.data()!=abcdefg_chars || sp.length()!=4 || sp.size()!=4) {
        errln("abcd.substr(-1) failed");
    }
    sp=abcd.substr(5);
    if(!sp.empty() || sp.length()!=0 || sp.size()!=0) {
        errln("abcd.substr(5) failed");
    }
    sp=abcd.substr(2);
    if(sp.empty() || sp.data()!=abcdefg_chars+2 || sp.length()!=2 || sp.size()!=2) {
        errln("abcd.substr(-1) failed");
    }
    // substr() method with pos and len.
    sp=abcd.substr(-1, 8);
    if(sp.empty() || sp.data()!=abcdefg_chars || sp.length()!=4 || sp.size()!=4) {
        errln("abcd.substr(-1, 8) failed");
    }
    sp=abcd.substr(5, 8);
    if(!sp.empty() || sp.length()!=0 || sp.size()!=0) {
        errln("abcd.substr(5, 8) failed");
    }
    sp=abcd.substr(2, 8);
    if(sp.empty() || sp.data()!=abcdefg_chars+2 || sp.length()!=2 || sp.size()!=2) {
        errln("abcd.substr(-1) failed");
    }
    sp=abcd.substr(2, -1);
    if(!sp.empty() || sp.length()!=0 || sp.size()!=0) {
        errln("abcd.substr(5, -1) failed");
    }
    // clear()
    sp=abcd;
    sp.clear();
    if(!sp.empty() || sp.data()!=nullptr || sp.length()!=0 || sp.size()!=0) {
        errln("abcd.clear() failed");
    }
    // remove_prefix()
    sp=abcd;
    sp.remove_prefix(-1);
    if(sp.empty() || sp.data()!=abcdefg_chars || sp.length()!=4 || sp.size()!=4) {
        errln("abcd.remove_prefix(-1) failed");
    }
    sp=abcd;
    sp.remove_prefix(2);
    if(sp.empty() || sp.data()!=abcdefg_chars+2 || sp.length()!=2 || sp.size()!=2) {
        errln("abcd.remove_prefix(2) failed");
    }
    sp=abcd;
    sp.remove_prefix(5);
    if(!sp.empty() || sp.length()!=0 || sp.size()!=0) {
        errln("abcd.remove_prefix(5) failed");
    }
    // remove_suffix()
    sp=abcd;
    sp.remove_suffix(-1);
    if(sp.empty() || sp.data()!=abcdefg_chars || sp.length()!=4 || sp.size()!=4) {
        errln("abcd.remove_suffix(-1) failed");
    }
    sp=abcd;
    sp.remove_suffix(2);
    if(sp.empty() || sp.data()!=abcdefg_chars || sp.length()!=2 || sp.size()!=2) {
        errln("abcd.remove_suffix(2) failed");
    }
    sp=abcd;
    sp.remove_suffix(5);
    if(!sp.empty() || sp.length()!=0 || sp.size()!=0) {
        errln("abcd.remove_suffix(5) failed");
    }
}

void
StringTest::TestStringPieceComparisons() {
    StringPiece empty;
    StringPiece null(nullptr);
    StringPiece abc("abc");
    StringPiece abcd("abcdefg", 4);
    StringPiece abx("abx");
    if(empty!=null) {
        errln("empty!=null");
    }
    if(empty==abc) {
        errln("empty==abc");
    }
    if(abc==abcd) {
        errln("abc==abcd");
    }

    assertTrue("null<abc", null.compare(abc) < 0);
    assertTrue("abc>null", abc.compare(null) > 0);
    assertTrue("abc<abcd", abc.compare(abcd) < 0);
    assertTrue("abcd>abc", abcd.compare(abc) > 0);
    assertTrue("abc<abx", abc.compare(abx) < 0);
    assertTrue("abx>abc", abx.compare(abc) > 0);
    assertTrue("abx>abcd", abx.compare(abcd) > 0);
    assertTrue("abcd<abx", abcd.compare(abx) < 0);
    assertTrue("abx==abx", abx.compare(abx) == 0);

    // Behavior should be the same as std::string::compare
    {
        std::string null("");
        std::string abc("abc");
        std::string abcd("abcdefg", 4);
        std::string abx("abx");

        assertTrue("std: null<abc", null.compare(abc) < 0);
        assertTrue("std: abc>null", abc.compare(null) > 0);
        assertTrue("std: abc<abcd", abc.compare(abcd) < 0);
        assertTrue("std: abcd>abc", abcd.compare(abc) > 0);
        assertTrue("std: abc<abx", abc.compare(abx) < 0);
        assertTrue("std: abx>abc", abx.compare(abc) > 0);
        assertTrue("std: abx>abcd", abx.compare(abcd) > 0);
        assertTrue("std: abcd<abx", abcd.compare(abx) < 0);
        assertTrue("std: abx==abx", abx.compare(abx) == 0);
    }

    abcd.remove_suffix(1);
    if(abc!=abcd) {
        errln("abc!=abcd.remove_suffix(1)");
    }
    if(abc==abx) {
        errln("abc==abx");
    }
}

void
StringTest::TestStringPieceFind() {
    struct TestCase {
        const char* haystack;
        const char* needle;
        int32_t expected;
    } cases[] = {
        { "", "", 0 },
        { "", "x", -1 },
        { "x", "", 0 },
        { "x", "x", 0 },
        { "xy", "x", 0 },
        { "xy", "y", 1 },
        { "xy", "xy", 0 },
        { "xy", "xyz", -1 },
        { "qwerty", "qqw", -1 },
        { "qwerty", "qw", 0 },
        { "qwerty", "er", 2 },
        { "qwerty", "err", -1 },
        { "qwerty", "ert", 2 },
        { "qwerty", "ty", 4 },
        { "qwerty", "tyy", -1 },
        { "qwerty", "a", -1 },
        { "qwerty", "abc", -1 }
    };
    int32_t caseNumber = 0;
    for (auto& cas : cases) {
        StringPiece haystack(cas.haystack);
        StringPiece needle(cas.needle);
        assertEquals(Int64ToUnicodeString(caseNumber),
            cas.expected, haystack.find(needle, 0));
        // Should be same as std::string::find
        std::string stdhaystack(cas.haystack);
        std::string stdneedle(cas.needle);
        assertEquals(Int64ToUnicodeString(caseNumber) + u" (std)",
            cas.expected, static_cast<int32_t>(stdhaystack.find(stdneedle, 0)));
        // Test offsets against std::string::find
        for (int32_t offset = 0; offset < haystack.length(); offset++) {
            assertEquals(Int64ToUnicodeString(caseNumber) + "u @ " + Int64ToUnicodeString(offset),
                static_cast<int32_t>(stdhaystack.find(stdneedle, offset)), haystack.find(needle, offset));
        }
        caseNumber++;
    }
}

void
StringTest::TestStringPieceOther() {
    static constexpr char msg[] = "Kapow!";

    // Another string piece implementation.
    struct Other {
        const char* data() { return msg; }
        size_t size() { return sizeof msg - 1; }
    };

    Other other;
    StringPiece piece(other);

    assertEquals("size()", piece.size(), static_cast<int32_t>(other.size()));
    assertEquals("data()", piece.data(), other.data());
}

void
StringTest::TestStringPieceStringView() {
    static constexpr char msg[] = "Kapow!";

    std::string_view view(msg);  // C++17
    StringPiece piece(view);

    assertEquals("size()", piece.size(), view.size());
    assertEquals("data()", piece.data(), view.data());
}

void
StringTest::TestStringPieceU8() {
    // ICU-20984 "mitigate some C++20 char8_t breakages"
    // For the following APIs there are overloads for both
    // const char * and const char8_t *.
    // A u8"string literal" has one type or the other
    // depending on C++ version and compiler settings.
    StringPiece abc(u8"abc");
    assertEquals("abc.length", 3, abc.length());
    assertEquals("abc", "\x61\x62\x63", abc.data());

    StringPiece abc3(u8"abcdef", 3);
    assertEquals("abc3.length", 3, abc3.length());
    assertEquals("abc3[0]", 0x61, abc3.data()[0]);
    assertEquals("abc3[1]", 0x62, abc3.data()[1]);
    assertEquals("abc3[2]", 0x63, abc3.data()[2]);

    StringPiece uvw("q");
    uvw.set(u8"uvw");
    assertEquals("uvw.length", 3, uvw.length());
    assertEquals("uvw", "\x75\x76\x77", uvw.data());

    StringPiece xyz("r");
    xyz.set(u8"xyzXYZ", 3);
    assertEquals("xyz.length", 3, xyz.length());
    assertEquals("xyz[0]", 0x78, xyz.data()[0]);
    assertEquals("xyz[1]", 0x79, xyz.data()[1]);
    assertEquals("xyz[2]", 0x7a, xyz.data()[2]);

    StringPiece null(nullptr);
    assertTrue("null is empty", null.empty());
    assertTrue("null is null", null.data() == nullptr);

#ifdef __cpp_lib_char8_t
    std::u8string_view u8sv(u8"sv");  // C++20
    StringPiece u8svsp(u8sv);
    assertEquals("u8svsp.length", 2, u8svsp.length());
    assertEquals("u8svsp", "\x73\x76", u8svsp.data());

    std::u8string u8str(u8"str");  // C++20
    StringPiece u8strsp(u8str);
    assertEquals("u8strsp.length", 3, u8strsp.length());
    assertEquals("u8strsp", "\x73\x74\x72", u8strsp.data());
#endif  // __cpp_lib_char8_t
}

// Verify that ByteSink is subclassable and Flush() overridable.
class SimpleByteSink : public ByteSink {
public:
    SimpleByteSink(char *outbuf) : fOutbuf(outbuf), fLength(0) {}
    virtual void Append(const char *bytes, int32_t n) override {
        if(fOutbuf != bytes) {
            memcpy(fOutbuf, bytes, n);
        }
        fOutbuf += n;
        fLength += n;
    }
    virtual void Flush() override { Append("z", 1); }
    int32_t length() { return fLength; }
private:
    char *fOutbuf;
    int32_t fLength;
};

// Test the ByteSink base class.
void
StringTest::TestByteSink() {
    char buffer[20];
    buffer[4] = '!';
    SimpleByteSink sink(buffer);
    sink.Append("abc", 3);
    sink.Flush();
    if(!(sink.length() == 4 && 0 == memcmp("abcz", buffer, 4) && buffer[4] == '!')) {
        errln("ByteSink (SimpleByteSink) did not Append() or Flush() as expected");
        return;
    }
    char scratch[20];
    int32_t capacity = -1;
    char *dest = sink.GetAppendBuffer(0, 50, scratch, (int32_t)sizeof(scratch), &capacity);
    if(dest != nullptr || capacity != 0) {
        errln("ByteSink.GetAppendBuffer(min_capacity<1) did not properly return nullptr[0]");
        return;
    }
    dest = sink.GetAppendBuffer(10, 50, scratch, 9, &capacity);
    if(dest != nullptr || capacity != 0) {
        errln("ByteSink.GetAppendBuffer(scratch_capacity<min_capacity) did not properly return nullptr[0]");
        return;
    }
    dest = sink.GetAppendBuffer(5, 50, scratch, (int32_t)sizeof(scratch), &capacity);
    if(dest != scratch || capacity != (int32_t)sizeof(scratch)) {
        errln("ByteSink.GetAppendBuffer() did not properly return the scratch buffer");
    }
}

void
StringTest::TestCheckedArrayByteSink() {
    char buffer[20];  // < 26 for the test code to work
    buffer[3] = '!';
    CheckedArrayByteSink sink(buffer, (int32_t)sizeof(buffer));
    sink.Append("abc", 3);
    if(!(sink.NumberOfBytesAppended() == 3 && sink.NumberOfBytesWritten() == 3 &&
         0 == memcmp("abc", buffer, 3) && buffer[3] == '!') &&
         !sink.Overflowed()
    ) {
        errln("CheckedArrayByteSink did not Append() as expected");
        return;
    }
    char scratch[10];
    int32_t capacity = -1;
    char *dest = sink.GetAppendBuffer(0, 50, scratch, (int32_t)sizeof(scratch), &capacity);
    if(dest != nullptr || capacity != 0) {
        errln("CheckedArrayByteSink.GetAppendBuffer(min_capacity<1) did not properly return nullptr[0]");
        return;
    }
    dest = sink.GetAppendBuffer(10, 50, scratch, 9, &capacity);
    if(dest != nullptr || capacity != 0) {
        errln("CheckedArrayByteSink.GetAppendBuffer(scratch_capacity<min_capacity) did not properly return nullptr[0]");
        return;
    }
    dest = sink.GetAppendBuffer(10, 50, scratch, (int32_t)sizeof(scratch), &capacity);
    if(dest != buffer + 3 || capacity != (int32_t)sizeof(buffer) - 3) {
        errln("CheckedArrayByteSink.GetAppendBuffer() did not properly return its own buffer");
        return;
    }
    memcpy(dest, "defghijklm", 10);
    sink.Append(dest, 10);
    if(!(sink.NumberOfBytesAppended() == 13 && sink.NumberOfBytesWritten() == 13 &&
         0 == memcmp("abcdefghijklm", buffer, 13) &&
         !sink.Overflowed())
    ) {
        errln("CheckedArrayByteSink did not Append(its own buffer) as expected");
        return;
    }
    dest = sink.GetAppendBuffer(10, 50, scratch, (int32_t)sizeof(scratch), &capacity);
    if(dest != scratch || capacity != (int32_t)sizeof(scratch)) {
        errln("CheckedArrayByteSink.GetAppendBuffer() did not properly return the scratch buffer");
    }
    memcpy(dest, "nopqrstuvw", 10);
    sink.Append(dest, 10);
    if(!(sink.NumberOfBytesAppended() == 23 &&
         sink.NumberOfBytesWritten() == (int32_t)sizeof(buffer) &&
         0 == memcmp("abcdefghijklmnopqrstuvwxyz", buffer, (int32_t)sizeof(buffer)) &&
         sink.Overflowed())
    ) {
        errln("CheckedArrayByteSink did not Append(scratch buffer) as expected");
        return;
    }
    sink.Reset().Append("123", 3);
    if(!(sink.NumberOfBytesAppended() == 3 && sink.NumberOfBytesWritten() == 3 &&
         0 == memcmp("123defghijklmnopqrstuvwxyz", buffer, (int32_t)sizeof(buffer)) &&
         !sink.Overflowed())
    ) {
        errln("CheckedArrayByteSink did not Reset().Append() as expected");
        return;
    }
}

void
StringTest::TestStringByteSink() {
    // Not much to test because only the constructors and Append()
    // are implemented, and trivially so.
    std::string result("abc");  // std::string
    StringByteSink<std::string> sink(&result);
    sink.Append("def", 3);
    if(result != "abcdef") {
        errln("StringByteSink did not Append() as expected");
    }
    StringByteSink<std::string> sink2(&result, 20);
    if(result.capacity() < (result.length() + 20)) {
        errln("StringByteSink should have 20 append capacity, has only %d",
              (int)(result.capacity() - result.length()));
    }
    sink.Append("ghi", 3);
    if(result != "abcdefghi") {
        errln("StringByteSink did not Append() as expected");
    }
}

void
StringTest::TestStringByteSinkAppendU8() {
    // ICU-20984 "mitigate some C++20 char8_t breakages"
    // For the following APIs there are overloads for both
    // const char * and const char8_t *.
    // A u8"string literal" has one type or the other
    // depending on C++ version and compiler settings.
    std::string result("abc");
    StringByteSink<std::string> sink(&result);
    sink.AppendU8("def", 3);
    sink.AppendU8(u8"ghijkl", 4);
    assertEquals("abcdefghij", "abcdef\x67\x68\x69\x6a", result.c_str());
}

#if defined(_MSC_VER)
#include <vector>
#endif

void
StringTest::TestSTLCompatibility() {
#if defined(_MSC_VER)
    /* Just make sure that it compiles with STL's placement new usage. */
    std::vector<UnicodeString> myvect;
    myvect.push_back(UnicodeString("blah"));
#endif
}

void
StringTest::TestCharString() {
    IcuTestErrorCode errorCode(*this, "TestCharString()");
    char expected[400];
    static const char longStr[] =
        "This is a long string that is meant to cause reallocation of the internal buffer of CharString.";
    CharString chStr(longStr, errorCode);
    if (0 != strcmp(longStr, chStr.data()) || (int32_t)strlen(longStr) != chStr.length()) {
        errln("CharString(longStr) failed.");
    }
    CharString test("Test", errorCode);
    CharString copy(test,errorCode);
    copy.copyFrom(chStr, errorCode);
    if (0 != strcmp(longStr, copy.data()) || (int32_t)strlen(longStr) != copy.length()) {
        errln("CharString.copyFrom() failed.");
    }
    StringPiece sp(chStr.toStringPiece());
    sp.remove_prefix(4);
    chStr.append(sp, errorCode).append(chStr, errorCode);
    strcpy(expected, longStr);
    strcat(expected, longStr+4);
    strcat(expected, longStr);
    strcat(expected, longStr+4);
    if (0 != strcmp(expected, chStr.data()) || (int32_t)strlen(expected) != chStr.length()) {
        errln("CharString(longStr).append(substring of self).append(self) failed.");
    }
    chStr.clear().append("abc", errorCode).append("defghij", 3, errorCode);
    if (0 != strcmp("abcdef", chStr.data()) || 6 != chStr.length()) {
        errln("CharString.clear().append(abc).append(defghij, 3) failed.");
    }
    chStr.appendInvariantChars(UNICODE_STRING_SIMPLE(
        "This is a long string that is meant to cause reallocation of the internal buffer of CharString."),
        errorCode);
    strcpy(expected, "abcdef");
    strcat(expected, longStr);
    if (0 != strcmp(expected, chStr.data()) || (int32_t)strlen(expected) != chStr.length()) {
        errln("CharString.appendInvariantChars(longStr) failed.");
    }
    int32_t appendCapacity = 0;
    char *buffer = chStr.getAppendBuffer(5, 10, appendCapacity, errorCode);
    if (errorCode.isFailure()) {
        return;
    }
    memcpy(buffer, "*****", 5);
    chStr.append(buffer, 5, errorCode);
    chStr.truncate(chStr.length()-3);
    strcat(expected, "**");
    if (0 != strcmp(expected, chStr.data()) || (int32_t)strlen(expected) != chStr.length()) {
        errln("CharString.getAppendBuffer().append(**) failed.");
    }

    UErrorCode ec = U_ZERO_ERROR;
    chStr.clear();
    chStr.appendInvariantChars(UnicodeString("The '@' character is not invariant."), ec);
    if (ec != U_INVARIANT_CONVERSION_ERROR) {
        errln("%s:%d expected U_INVARIANT_CONVERSION_ERROR, got %s", __FILE__, __LINE__, u_errorName(ec));
    }
    if (chStr.length() != 0) {
        errln("%s:%d expected length() = 0, got %d", __FILE__, __LINE__, chStr.length());
    }

    {
        CharString s1("Short string", errorCode);
        CharString s2(std::move(s1));
        assertEquals("s2 should have content of s1", "Short string", s2.data());
        CharString s3("Dummy", errorCode);
        s3 = std::move(s2);
        assertEquals("s3 should have content of s2", "Short string", s3.data());
    }

    {
        CharString s1("Long string over 40 characters to trigger heap allocation", errorCode);
        CharString s2(std::move(s1));
        assertEquals("s2 should have content of s1",
                "Long string over 40 characters to trigger heap allocation",
                s2.data());
        CharString s3("Dummy string with over 40 characters to trigger heap allocation", errorCode);
        s3 = std::move(s2);
        assertEquals("s3 should have content of s2",
                "Long string over 40 characters to trigger heap allocation",
                s3.data());
    }

    {
        // extract()
        errorCode.reset();
        CharString s("abc", errorCode);
        char buffer[10];

        s.extract(buffer, 10, errorCode);
        assertEquals("abc.extract(10) success", U_ZERO_ERROR, errorCode.get());
        assertEquals("abc.extract(10) output", "abc", buffer);

        strcpy(buffer, "012345");
        s.extract(buffer, 3, errorCode);
        assertEquals("abc.extract(3) not terminated",
                     U_STRING_NOT_TERMINATED_WARNING, errorCode.reset());
        assertEquals("abc.extract(3) output", "abc345", buffer);

        strcpy(buffer, "012345");
        s.extract(buffer, 2, errorCode);
        assertEquals("abc.extract(2) overflow", U_BUFFER_OVERFLOW_ERROR, errorCode.reset());
    }
}

void
StringTest::TestCStr() {
    const char *cs = "This is a test string.";
    UnicodeString us(cs);
    if (0 != strcmp(CStr(us)(), cs)) {
        errln("%s:%d CStr(s)() failed. Expected \"%s\", got \"%s\"", __FILE__, __LINE__, cs, CStr(us)());
    }
}

void StringTest::TestCharStrAppendNumber() {
    IcuTestErrorCode errorCode(*this, "TestCharStrAppendNumber()");

    CharString testString;
    testString.appendNumber(1, errorCode);
    assertEquals("TestAppendNumber 1", "1", testString.data());

    testString.clear();
    testString.appendNumber(-1, errorCode);
    assertEquals("TestAppendNumber -1", "-1", testString.data());

    testString.clear();
    testString.appendNumber(12345, errorCode);
    assertEquals("TestAppendNumber 12345", "12345", testString.data());
    testString.appendNumber(123, errorCode);
    assertEquals("TestAppendNumber 12345 and then 123", "12345123", testString.data());

    testString.clear();
    testString.appendNumber(std::numeric_limits<int32_t>::max(), errorCode);
    assertEquals("TestAppendNumber when appending the biggest int32", "2147483647", testString.data());

    testString.clear();
    testString.appendNumber(std::numeric_limits<int32_t>::min(), errorCode);
    assertEquals("TestAppendNumber when appending the smallest int32", "-2147483648", testString.data());

    testString.clear();
    testString.appendNumber(0, errorCode);
    assertEquals("TestAppendNumber when appending zero", "0", testString.data());
}

void
StringTest::Testctou() {
  const char *cs = "Fa\\u0127mu";
  UnicodeString u = ctou(cs);
  assertEquals("Testing unescape@0", (int32_t)0x0046, u.charAt(0));
  assertEquals("Testing unescape@2", (int32_t)295, u.charAt(2));
}
