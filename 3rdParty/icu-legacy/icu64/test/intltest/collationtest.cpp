// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2012-2015, International Business Machines
* Corporation and others.  All Rights Reserved.
*******************************************************************************
* collationtest.cpp
*
* created on: 2012apr27
* created by: Markus W. Scherer
*/

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include "unicode/coll.h"
#include "unicode/errorcode.h"
#include "unicode/localpointer.h"
#include "unicode/normalizer2.h"
#include "unicode/sortkey.h"
#include "unicode/std_string.h"
#include "unicode/strenum.h"
#include "unicode/stringpiece.h"
#include "unicode/tblcoll.h"
#include "unicode/uiter.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "unicode/usetiter.h"
#include "unicode/ustring.h"
#include "charstr.h"
#include "cmemory.h"
#include "collation.h"
#include "collationdata.h"
#include "collationfcd.h"
#include "collationiterator.h"
#include "collationroot.h"
#include "collationrootelements.h"
#include "collationruleparser.h"
#include "collationweights.h"
#include "cstring.h"
#include "intltest.h"
#include "normalizer2impl.h"
#include "ucbuf.h"
#include "uhash.h"
#include "uitercollationiterator.h"
#include "utf16collationiterator.h"
#include "utf8collationiterator.h"
#include "uvectr32.h"
#include "uvectr64.h"
#include "writesrc.h"

class CodePointIterator;

// TODO: try to share code with IntlTestCollator; for example, prettify(CollationKey)

class CollationTest : public IntlTest {
public:
    CollationTest()
            : fcd(nullptr), nfd(nullptr),
              fileLineNumber(0),
              coll(nullptr) {}

    ~CollationTest() {
        delete coll;
    }

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=nullptr) override;

    void TestMinMax();
    void TestImplicits();
    void TestNulTerminated();
    void TestIllegalUTF8();
    void TestShortFCDData();
    void TestFCD();
    void TestCollationWeights();
    void TestRootElements();
    void TestTailoredElements();
    void TestDataDriven();
    void TestLongLocale();
    void TestBuilderContextsOverflow();
    void TestHang22414();

private:
    void checkFCD(const char *name, CollationIterator &ci, CodePointIterator &cpi);
    void checkAllocWeights(CollationWeights &cw,
                           uint32_t lowerLimit, uint32_t upperLimit, int32_t n,
                           int32_t someLength, int32_t minCount);

    static UnicodeString printSortKey(const uint8_t *p, int32_t length);
    static UnicodeString printCollationKey(const CollationKey &key);

    // Helpers & fields for data-driven test.
    static UBool isCROrLF(char16_t c) { return c == 0xa || c == 0xd; }
    static UBool isSpace(char16_t c) { return c == 9 || c == 0x20 || c == 0x3000; }
    static UBool isSectionStarter(char16_t c) { return c == 0x25 || c == 0x2a || c == 0x40; }  // %*@
    int32_t skipSpaces(int32_t i) {
        while(isSpace(fileLine[i])) { ++i; }
        return i;
    }

    UBool readNonEmptyLine(UCHARBUF *f, IcuTestErrorCode &errorCode);
    void parseString(int32_t &start, UnicodeString &prefix, UnicodeString &s, UErrorCode &errorCode);
    Collation::Level parseRelationAndString(UnicodeString &s, IcuTestErrorCode &errorCode);
    void parseAndSetAttribute(IcuTestErrorCode &errorCode);
    void parseAndSetReorderCodes(int32_t start, IcuTestErrorCode &errorCode);
    void buildTailoring(UCHARBUF *f, IcuTestErrorCode &errorCode);
    void setRootCollator(IcuTestErrorCode &errorCode);
    void setLocaleCollator(IcuTestErrorCode &errorCode);

    UBool needsNormalization(const UnicodeString &s, UErrorCode &errorCode) const;

    UBool getSortKeyParts(const char16_t *s, int32_t length,
                          CharString &dest, int32_t partSize,
                          IcuTestErrorCode &errorCode);
    UBool getCollationKey(const char *norm, const UnicodeString &line,
                          const char16_t *s, int32_t length,
                          CollationKey &key, IcuTestErrorCode &errorCode);
    UBool getMergedCollationKey(const char16_t *s, int32_t length,
                                CollationKey &key, IcuTestErrorCode &errorCode);
    UBool checkCompareTwo(const char *norm, const UnicodeString &prevFileLine,
                          const UnicodeString &prevString, const UnicodeString &s,
                          UCollationResult expectedOrder, Collation::Level expectedLevel,
                          IcuTestErrorCode &errorCode);
    void checkCompareStrings(UCHARBUF *f, IcuTestErrorCode &errorCode);

    const Normalizer2 *fcd, *nfd;
    UnicodeString fileLine;
    int32_t fileLineNumber;
    UnicodeString fileTestName;
    Collator *coll;
};

extern IntlTest *createCollationTest() {
    return new CollationTest();
}

void CollationTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) {
    if(exec) {
        logln("TestSuite CollationTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestMinMax);
    TESTCASE_AUTO(TestImplicits);
    TESTCASE_AUTO(TestNulTerminated);
    TESTCASE_AUTO(TestIllegalUTF8);
    TESTCASE_AUTO(TestShortFCDData);
    TESTCASE_AUTO(TestFCD);
    TESTCASE_AUTO(TestCollationWeights);
    TESTCASE_AUTO(TestRootElements);
    TESTCASE_AUTO(TestTailoredElements);
    TESTCASE_AUTO(TestDataDriven);
    TESTCASE_AUTO(TestLongLocale);
    TESTCASE_AUTO(TestBuilderContextsOverflow);
    TESTCASE_AUTO(TestHang22414);
    TESTCASE_AUTO_END;
}

void CollationTest::TestMinMax() {
    IcuTestErrorCode errorCode(*this, "TestMinMax");

    setRootCollator(errorCode);
    if(errorCode.isFailure()) {
        errorCode.reset();
        return;
    }
    RuleBasedCollator *rbc = dynamic_cast<RuleBasedCollator *>(coll);
    if(rbc == nullptr) {
        errln("the root collator is not a RuleBasedCollator");
        return;
    }

    static const char16_t s[2] = { 0xfffe, 0xffff };
    UVector64 ces(errorCode);
    rbc->internalGetCEs(UnicodeString(false, s, 2), ces, errorCode);
    errorCode.assertSuccess();
    if(ces.size() != 2) {
        errln("expected 2 CEs for <FFFE, FFFF>, got %d", (int)ces.size());
        return;
    }
    int64_t ce = ces.elementAti(0);
    int64_t expected = Collation::makeCE(Collation::MERGE_SEPARATOR_PRIMARY);
    if(ce != expected) {
        errln("CE(U+fffe)=%04lx != 02..", (long)ce);
    }

    ce = ces.elementAti(1);
    expected = Collation::makeCE(Collation::MAX_PRIMARY);
    if(ce != expected) {
        errln("CE(U+ffff)=%04lx != max..", (long)ce);
    }
}

void CollationTest::TestImplicits() {
    IcuTestErrorCode errorCode(*this, "TestImplicits");

    const CollationData *cd = CollationRoot::getData(errorCode);
    if(errorCode.errDataIfFailureAndReset("CollationRoot::getData()")) {
        return;
    }

    // Implicit primary weights should be assigned for the following sets,
    // and sort in ascending order by set and then code point.
    // See https://www.unicode.org/reports/tr10/#Implicit_Weights

    // core Han Unified Ideographs
    UnicodeSet coreHan("[\\p{unified_ideograph}&"
                            "[\\p{Block=CJK_Unified_Ideographs}"
                            "\\p{Block=CJK_Compatibility_Ideographs}]]",
                       errorCode);
    // all other Unified Han ideographs
    UnicodeSet otherHan("[\\p{unified ideograph}-"
                            "[\\p{Block=CJK_Unified_Ideographs}"
                            "\\p{Block=CJK_Compatibility_Ideographs}]]",
                        errorCode);
    UnicodeSet unassigned("[[:Cn:][:Cs:][:Co:]]", errorCode);
    unassigned.remove(0xfffe, 0xffff);  // These have special CLDR root mappings.

    // Starting with CLDR 26/ICU 54, the root Han order may instead be
    // the Unihan radical-stroke order.
    // The tests should pass either way, so we only test the order of a small set of Han characters
    // whose radical-stroke order is the same as their code point order.
    //
    // When the radical-stroke data (kRSUnicode) for one of these characters changes
    // such that it no longer sorts in code point order,
    // then we need to remove it from this set.
    // (These changes are easiest to see in the change history of the Unicode Tools file
    // unicodetools/data/ucd/dev/Unihan/kRSUnicode.txt.)
    // For example, in Unicode 15.1, U+503B has a kRSUnicode value of 9.9
    // while the neighboring characters still have 9.8. We remove the out-of-order U+503B.
    //
    // FYI: The Unicode Tools program GenerateUnihanCollators prints something like
    // hanInCPOrder = [一-世丘-丫中-丼举-么乊-习乣-亏...鼢-齡齣-龏龑-龥]
    // number of original-Unihan characters out of order: 318
    UnicodeSet someHanInCPOrder(
            u"[\u4E00-\u4E16\u4E18-\u4E2B\u4E2D-\u4E3C\u4E3E-\u4E48"
            u"\u4E4A-\u4E60\u4E63-\u4E8F\u4E91-\u4F63\u4F65-\u503A\u503C-\u50F1\u50F3-\u50F6]",
            errorCode);
    UnicodeSet inOrder(someHanInCPOrder);
    inOrder.addAll(unassigned).freeze();
    if(errorCode.errIfFailureAndReset("UnicodeSet")) {
        return;
    }
    const UnicodeSet *sets[] = { &coreHan, &otherHan, &unassigned };
    const char *const setNames[] = { "core Han", "Han extensions", "unassigned" };
    UChar32 prev = 0;
    uint32_t prevPrimary = 0;
    UTF16CollationIterator ci(cd, false, nullptr, nullptr, nullptr);
    for(int32_t i = 0; i < UPRV_LENGTHOF(sets); ++i) {
        const char *setName = setNames[i];
        LocalPointer<UnicodeSetIterator> iter(new UnicodeSetIterator(*sets[i]));
        while(iter->next()) {
            UChar32 c = iter->getCodepoint();
            UnicodeString s(c);
            ci.setText(s.getBuffer(), s.getBuffer() + s.length());
            int64_t ce = ci.nextCE(errorCode);
            int64_t ce2 = ci.nextCE(errorCode);
            if(errorCode.errIfFailureAndReset("CollationIterator.nextCE()")) {
                return;
            }
            if(ce == Collation::NO_CE || ce2 != Collation::NO_CE) {
                errln("%s: CollationIterator.nextCE(U+%04lx) did not yield exactly one CE",
                      setName, (long)c);
                continue;
            }
            if((ce & 0xffffffff) != Collation::COMMON_SEC_AND_TER_CE) {
                errln("%s: CollationIterator.nextCE(U+%04lx) has non-common sec/ter weights: %08lx",
                      setName, (long)c, (long)(ce & 0xffffffff));
                continue;
            }
            uint32_t primary = (uint32_t)(ce >> 32);
            if(!(primary > prevPrimary) && inOrder.contains(c) && inOrder.contains(prev)) {
                errln("%s: CE(U+%04lx)=%04lx.. not greater than CE(U+%04lx)=%04lx..",
                      setName, (long)c, (long)primary, (long)prev, (long)prevPrimary);
            }
            prev = c;
            prevPrimary = primary;
        }
    }
}

void CollationTest::TestNulTerminated() {
    IcuTestErrorCode errorCode(*this, "TestNulTerminated");
    const CollationData *data = CollationRoot::getData(errorCode);
    if(errorCode.errDataIfFailureAndReset("CollationRoot::getData()")) {
        return;
    }

    static const char16_t s[] = { 0x61, 0x62, 0x61, 0x62, 0 };

    UTF16CollationIterator ci1(data, false, s, s, s + 2);
    UTF16CollationIterator ci2(data, false, s + 2, s + 2, nullptr);
    for(int32_t i = 0;; ++i) {
        int64_t ce1 = ci1.nextCE(errorCode);
        int64_t ce2 = ci2.nextCE(errorCode);
        if(errorCode.errIfFailureAndReset("CollationIterator.nextCE()")) {
            return;
        }
        if(ce1 != ce2) {
            errln("CollationIterator.nextCE(with length) != nextCE(NUL-terminated) at CE %d", (int)i);
            break;
        }
        if(ce1 == Collation::NO_CE) { break; }
    }
}

void CollationTest::TestIllegalUTF8() {
    IcuTestErrorCode errorCode(*this, "TestIllegalUTF8");

    setRootCollator(errorCode);
    if(errorCode.isFailure()) {
        errorCode.reset();
        return;
    }
    coll->setAttribute(UCOL_STRENGTH, UCOL_IDENTICAL, errorCode);

    static const StringPiece strings[] = {
        // string with U+FFFD == illegal byte sequence
        u8"a\uFFFDz",                   "a\x80z",  // trail byte
        u8"a\uFFFD\uFFFDz",             "a\xc1\x81z",  // non-shortest form
        u8"a\uFFFD\uFFFD\uFFFDz",       "a\xe0\x82\x83z",  // non-shortest form
        u8"a\uFFFD\uFFFD\uFFFDz",       "a\xed\xa0\x80z",  // lead surrogate: would be U+D800
        u8"a\uFFFD\uFFFD\uFFFDz",       "a\xed\xbf\xbfz",  // trail surrogate: would be U+DFFF
        u8"a\uFFFD\uFFFD\uFFFD\uFFFDz", "a\xf0\x8f\xbf\xbfz",  // non-shortest form
        u8"a\uFFFD\uFFFD\uFFFD\uFFFDz", "a\xf4\x90\x80\x80z"  // out of range: would be U+110000
    };

    for(int32_t i = 0; i < UPRV_LENGTHOF(strings); i += 2) {
        StringPiece fffd(strings[i]);
        StringPiece illegal(strings[i + 1]);
        UCollationResult order = coll->compareUTF8(fffd, illegal, errorCode);
        if(order != UCOL_EQUAL) {
            errln("compareUTF8(pair %d: U+FFFD, illegal UTF-8)=%d != UCOL_EQUAL",
                  (int)i, order);
        }
    }
}

namespace {

void addLeadSurrogatesForSupplementary(const UnicodeSet &src, UnicodeSet &dest) {
    for(UChar32 c = 0x10000; c < 0x110000;) {
        UChar32 next = c + 0x400;
        if(src.containsSome(c, next - 1)) {
            dest.add(U16_LEAD(c));
        }
        c = next;
    }
}

}  // namespace

void CollationTest::TestShortFCDData() {
    // See CollationFCD class comments.
    IcuTestErrorCode errorCode(*this, "TestShortFCDData");
    UnicodeSet expectedLccc("[:^lccc=0:]", errorCode);
    errorCode.assertSuccess();
    expectedLccc.add(0xdc00, 0xdfff);  // add all trail surrogates
    addLeadSurrogatesForSupplementary(expectedLccc, expectedLccc);
    UnicodeSet lccc;  // actual
    for(UChar32 c = 0; c <= 0xffff; ++c) {
        if(CollationFCD::hasLccc(c)) { lccc.add(c); }
    }
    UnicodeSet diff(expectedLccc);
    diff.removeAll(lccc);
    diff.remove(0x10000, 0x10ffff);  // hasLccc() only works for the BMP
    UnicodeString empty("[]");
    UnicodeString diffString;
    diff.toPattern(diffString, true);
    assertEquals("CollationFCD::hasLccc() expected-actual", empty, diffString);
    diff = lccc;
    diff.removeAll(expectedLccc);
    diff.toPattern(diffString, true);
    assertEquals("CollationFCD::hasLccc() actual-expected", empty, diffString, true);

    UnicodeSet expectedTccc("[:^tccc=0:]", errorCode);
    if (errorCode.isSuccess()) {
        addLeadSurrogatesForSupplementary(expectedLccc, expectedTccc);
        addLeadSurrogatesForSupplementary(expectedTccc, expectedTccc);
        UnicodeSet tccc;  // actual
        for(UChar32 c = 0; c <= 0xffff; ++c) {
            if(CollationFCD::hasTccc(c)) { tccc.add(c); }
        }
        diff = expectedTccc;
        diff.removeAll(tccc);
        diff.remove(0x10000, 0x10ffff);  // hasTccc() only works for the BMP
        assertEquals("CollationFCD::hasTccc() expected-actual", empty, diffString);
        diff = tccc;
        diff.removeAll(expectedTccc);
        diff.toPattern(diffString, true);
        assertEquals("CollationFCD::hasTccc() actual-expected", empty, diffString);
    }
}

class CodePointIterator {
public:
    CodePointIterator(const UChar32 *cp, int32_t length) : cp(cp), length(length), pos(0) {}
    void resetToStart() { pos = 0; }
    UChar32 next() { return (pos < length) ? cp[pos++] : U_SENTINEL; }
    UChar32 previous() { return (pos > 0) ? cp[--pos] : U_SENTINEL; }
    int32_t getLength() const { return length; }
    int getIndex() const { return (int)pos; }
private:
    const UChar32 *cp;
    int32_t length;
    int32_t pos;
};

void CollationTest::checkFCD(const char *name,
                             CollationIterator &ci, CodePointIterator &cpi) {
    IcuTestErrorCode errorCode(*this, "checkFCD");

    // Iterate forward to the limit.
    for(;;) {
        UChar32 c1 = ci.nextCodePoint(errorCode);
        UChar32 c2 = cpi.next();
        if(c1 != c2) {
            errln("%s.nextCodePoint(to limit, 1st pass) = U+%04lx != U+%04lx at %d",
                  name, (long)c1, (long)c2, cpi.getIndex());
            return;
        }
        if(c1 < 0) { break; }
    }

    // Iterate backward most of the way.
    for(int32_t n = (cpi.getLength() * 2) / 3; n > 0; --n) {
        UChar32 c1 = ci.previousCodePoint(errorCode);
        UChar32 c2 = cpi.previous();
        if(c1 != c2) {
            errln("%s.previousCodePoint() = U+%04lx != U+%04lx at %d",
                  name, (long)c1, (long)c2, cpi.getIndex());
            return;
        }
    }

    // Forward again.
    for(;;) {
        UChar32 c1 = ci.nextCodePoint(errorCode);
        UChar32 c2 = cpi.next();
        if(c1 != c2) {
            errln("%s.nextCodePoint(to limit again) = U+%04lx != U+%04lx at %d",
                  name, (long)c1, (long)c2, cpi.getIndex());
            return;
        }
        if(c1 < 0) { break; }
    }

    // Iterate backward to the start.
    for(;;) {
        UChar32 c1 = ci.previousCodePoint(errorCode);
        UChar32 c2 = cpi.previous();
        if(c1 != c2) {
            errln("%s.previousCodePoint(to start) = U+%04lx != U+%04lx at %d",
                  name, (long)c1, (long)c2, cpi.getIndex());
            return;
        }
        if(c1 < 0) { break; }
    }
}

void CollationTest::TestFCD() {
    IcuTestErrorCode errorCode(*this, "TestFCD");
    const CollationData *data = CollationRoot::getData(errorCode);
    if(errorCode.errDataIfFailureAndReset("CollationRoot::getData()")) {
        return;
    }

    // Input string, not FCD, NUL-terminated.
    static const char16_t s[] = {
        0x308, 0xe1, 0x62, 0x301, 0x327, 0x430, 0x62,
        U16_LEAD(0x1D15F), U16_TRAIL(0x1D15F),  // MUSICAL SYMBOL QUARTER NOTE=1D158 1D165, ccc=0, 216
        0x327, 0x308,  // ccc=202, 230
        U16_LEAD(0x1D16D), U16_TRAIL(0x1D16D),  // MUSICAL SYMBOL COMBINING AUGMENTATION DOT, ccc=226
        U16_LEAD(0x1D15F), U16_TRAIL(0x1D15F),
        U16_LEAD(0x1D16D), U16_TRAIL(0x1D16D),
        0xac01,
        0xe7,  // Character with tccc!=0 decomposed together with mis-ordered sequence.
        U16_LEAD(0x1D16D), U16_TRAIL(0x1D16D), U16_LEAD(0x1D165), U16_TRAIL(0x1D165),
        0xe1,  // Character with tccc!=0 decomposed together with decomposed sequence.
        0xf73, 0xf75,  // Tibetan composite vowels must be decomposed.
        0x4e00, 0xf81,
        0
    };
    // Expected code points.
    static const UChar32 cp[] = {
        0x308, 0xe1, 0x62, 0x327, 0x301, 0x430, 0x62,
        0x1D158, 0x327, 0x1D165, 0x1D16D, 0x308,
        0x1D15F, 0x1D16D,
        0xac01,
        0x63, 0x327, 0x1D165, 0x1D16D,
        0x61,
        0xf71, 0xf71, 0xf72, 0xf74, 0x301,
        0x4e00, 0xf71, 0xf80
    };

    FCDUTF16CollationIterator u16ci(data, false, s, s, nullptr);
    if(errorCode.errIfFailureAndReset("FCDUTF16CollationIterator constructor")) {
        return;
    }
    CodePointIterator cpi(cp, UPRV_LENGTHOF(cp));
    checkFCD("FCDUTF16CollationIterator", u16ci, cpi);

    cpi.resetToStart();
    std::string utf8;
    UnicodeString(s).toUTF8String(utf8);
    FCDUTF8CollationIterator u8ci(data, false,
                                  reinterpret_cast<const uint8_t *>(utf8.c_str()), 0, -1);
    if(errorCode.errIfFailureAndReset("FCDUTF8CollationIterator constructor")) {
        return;
    }
    checkFCD("FCDUTF8CollationIterator", u8ci, cpi);

    cpi.resetToStart();
    UCharIterator iter;
    uiter_setString(&iter, s, UPRV_LENGTHOF(s) - 1);  // -1: without the terminating NUL
    FCDUIterCollationIterator uici(data, false, iter, 0);
    if(errorCode.errIfFailureAndReset("FCDUIterCollationIterator constructor")) {
        return;
    }
    checkFCD("FCDUIterCollationIterator", uici, cpi);
}

void CollationTest::checkAllocWeights(CollationWeights &cw,
                                      uint32_t lowerLimit, uint32_t upperLimit, int32_t n,
                                      int32_t someLength, int32_t minCount) {
    if(!cw.allocWeights(lowerLimit, upperLimit, n)) {
        errln("CollationWeights::allocWeights(%lx, %lx, %ld) = false",
              (long)lowerLimit, (long)upperLimit, (long)n);
        return;
    }
    uint32_t previous = lowerLimit;
    int32_t count = 0;  // number of weights that have someLength
    for(int32_t i = 0; i < n; ++i) {
        uint32_t w = cw.nextWeight();
        if(w == 0xffffffff) {
            errln("CollationWeights::allocWeights(%lx, %lx, %ld).nextWeight() "
                  "returns only %ld weights",
                  (long)lowerLimit, (long)upperLimit, (long)n, (long)i);
            return;
        }
        if(!(previous < w && w < upperLimit)) {
            errln("CollationWeights::allocWeights(%lx, %lx, %ld).nextWeight() "
                  "number %ld -> %lx not between %lx and %lx",
                  (long)lowerLimit, (long)upperLimit, (long)n,
                  (long)(i + 1), (long)w, (long)previous, (long)upperLimit);
            return;
        }
        if(CollationWeights::lengthOfWeight(w) == someLength) { ++count; }
    }
    if(count < minCount) {
        errln("CollationWeights::allocWeights(%lx, %lx, %ld).nextWeight() "
              "returns only %ld < %ld weights of length %d",
              (long)lowerLimit, (long)upperLimit, (long)n,
              (long)count, (long)minCount, (int)someLength);
    }
}

void CollationTest::TestCollationWeights() {
    CollationWeights cw;

    // Non-compressible primaries use 254 second bytes 02..FF.
    logln("CollationWeights.initForPrimary(non-compressible)");
    cw.initForPrimary(false);
    // Expect 1 weight 11 and 254 weights 12xx.
    checkAllocWeights(cw, 0x10000000, 0x13000000, 255, 1, 1);
    checkAllocWeights(cw, 0x10000000, 0x13000000, 255, 2, 254);
    // Expect 255 two-byte weights from the ranges 10ff, 11xx, 1202.
    checkAllocWeights(cw, 0x10fefe40, 0x12030300, 260, 2, 255);
    // Expect 254 two-byte weights from the ranges 10ff and 11xx.
    checkAllocWeights(cw, 0x10fefe40, 0x12030300, 600, 2, 254);
    // Expect 254^2=64516 three-byte weights.
    // During computation, there should be 3 three-byte ranges
    // 10ffff, 11xxxx, 120202.
    // The middle one should be split 64515:1,
    // and the newly-split-off range and the last ranged lengthened.
    checkAllocWeights(cw, 0x10fffe00, 0x12020300, 1 + 64516 + 254 + 1, 3, 64516);
    // Expect weights 1102 & 1103.
    checkAllocWeights(cw, 0x10ff0000, 0x11040000, 2, 2, 2);
    // Expect weights 102102 & 102103.
    checkAllocWeights(cw, 0x1020ff00, 0x10210400, 2, 3, 2);

    // Compressible primaries use 251 second bytes 04..FE.
    logln("CollationWeights.initForPrimary(compressible)");
    cw.initForPrimary(true);
    // Expect 1 weight 11 and 251 weights 12xx.
    checkAllocWeights(cw, 0x10000000, 0x13000000, 252, 1, 1);
    checkAllocWeights(cw, 0x10000000, 0x13000000, 252, 2, 251);
    // Expect 252 two-byte weights from the ranges 10fe, 11xx, 1204.
    checkAllocWeights(cw, 0x10fdfe40, 0x12050300, 260, 2, 252);
    // Expect weights 1104 & 1105.
    checkAllocWeights(cw, 0x10fe0000, 0x11060000, 2, 2, 2);
    // Expect weights 102102 & 102103.
    checkAllocWeights(cw, 0x1020ff00, 0x10210400, 2, 3, 2);

    // Secondary and tertiary weights use only bytes 3 & 4.
    logln("CollationWeights.initForSecondary()");
    cw.initForSecondary();
    // Expect weights fbxx and all four fc..ff.
    checkAllocWeights(cw, 0xfb20, 0x10000, 20, 3, 4);

    logln("CollationWeights.initForTertiary()");
    cw.initForTertiary();
    // Expect weights 3dxx and both 3e & 3f.
    checkAllocWeights(cw, 0x3d02, 0x4000, 10, 3, 2);
}

namespace {

UBool isValidCE(const CollationRootElements &re, const CollationData &data,
                uint32_t p, uint32_t s, uint32_t ctq) {
    uint32_t p1 = p >> 24;
    uint32_t p2 = (p >> 16) & 0xff;
    uint32_t p3 = (p >> 8) & 0xff;
    uint32_t p4 = p & 0xff;
    uint32_t s1 = s >> 8;
    uint32_t s2 = s & 0xff;
    // ctq = Case, Tertiary, Quaternary
    uint32_t c = (ctq & Collation::CASE_MASK) >> 14;
    uint32_t t = ctq & Collation::ONLY_TERTIARY_MASK;
    uint32_t t1 = t >> 8;
    uint32_t t2 = t & 0xff;
    uint32_t q = ctq & Collation::QUATERNARY_MASK;
    // No leading zero bytes.
    if((p != 0 && p1 == 0) || (s != 0 && s1 == 0) || (t != 0 && t1 == 0)) {
        return false;
    }
    // No intermediate zero bytes.
    if(p1 != 0 && p2 == 0 && (p & 0xffff) != 0) {
        return false;
    }
    if(p2 != 0 && p3 == 0 && p4 != 0) {
        return false;
    }
    // Minimum & maximum lead bytes.
    if((p1 != 0 && p1 <= Collation::MERGE_SEPARATOR_BYTE) ||
            s1 == Collation::LEVEL_SEPARATOR_BYTE ||
            t1 == Collation::LEVEL_SEPARATOR_BYTE || t1 > 0x3f) {
        return false;
    }
    if(c > 2) {
        return false;
    }
    // The valid byte range for the second primary byte depends on compressibility.
    if(p2 != 0) {
        if(data.isCompressibleLeadByte(p1)) {
            if(p2 <= Collation::PRIMARY_COMPRESSION_LOW_BYTE ||
                    Collation::PRIMARY_COMPRESSION_HIGH_BYTE <= p2) {
                return false;
            }
        } else {
            if(p2 <= Collation::LEVEL_SEPARATOR_BYTE) {
                return false;
            }
        }
    }
    // Other bytes just need to avoid the level separator.
    // Trailing zeros are ok.
    U_ASSERT(Collation::LEVEL_SEPARATOR_BYTE == 1);
    if(p3 == Collation::LEVEL_SEPARATOR_BYTE || p4 == Collation::LEVEL_SEPARATOR_BYTE ||
            s2 == Collation::LEVEL_SEPARATOR_BYTE || t2 == Collation::LEVEL_SEPARATOR_BYTE) {
        return false;
    }
    // Well-formed CEs.
    if(p == 0) {
        if(s == 0) {
            if(t == 0) {
                // Completely ignorable CE.
                // Quaternary CEs are not supported.
                if(c != 0 || q != 0) {
                    return false;
                }
            } else {
                // Tertiary CE.
                if(t < re.getTertiaryBoundary() || c != 2) {
                    return false;
                }
            }
        } else {
            // Secondary CE.
            if(s < re.getSecondaryBoundary() || t == 0 || t >= re.getTertiaryBoundary()) {
                return false;
            }
        }
    } else {
        // Primary CE.
        if(s == 0 || (Collation::COMMON_WEIGHT16 < s && s <= re.getLastCommonSecondary()) ||
                s >= re.getSecondaryBoundary()) {
            return false;
        }
        if(t == 0 || t >= re.getTertiaryBoundary()) {
            return false;
        }
    }
    return true;
}

UBool isValidCE(const CollationRootElements &re, const CollationData &data, int64_t ce) {
    uint32_t p = (uint32_t)(ce >> 32);
    uint32_t secTer = (uint32_t)ce;
    return isValidCE(re, data, p, secTer >> 16, secTer & 0xffff);
}

class RootElementsIterator {
public:
    RootElementsIterator(const CollationData &root)
            : data(root),
              elements(root.rootElements), length(root.rootElementsLength),
              pri(0), secTer(0),
              index((int32_t)elements[CollationRootElements::IX_FIRST_TERTIARY_INDEX]) {}

    UBool next() {
        if(index >= length) { return false; }
        uint32_t p = elements[index];
        if(p == CollationRootElements::PRIMARY_SENTINEL) { return false; }
        if((p & CollationRootElements::SEC_TER_DELTA_FLAG) != 0) {
            ++index;
            secTer = p & ~CollationRootElements::SEC_TER_DELTA_FLAG;
            return true;
        }
        if((p & CollationRootElements::PRIMARY_STEP_MASK) != 0) {
            // End of a range, enumerate the primaries in the range.
            int32_t step = (int32_t)p & CollationRootElements::PRIMARY_STEP_MASK;
            p &= 0xffffff00;
            if(pri == p) {
                // Finished the range, return the next CE after it.
                ++index;
                return next();
            }
            U_ASSERT(pri < p);
            // Return the next primary in this range.
            UBool isCompressible = data.isCompressiblePrimary(pri);
            if((pri & 0xffff) == 0) {
                pri = Collation::incTwoBytePrimaryByOffset(pri, isCompressible, step);
            } else {
                pri = Collation::incThreeBytePrimaryByOffset(pri, isCompressible, step);
            }
            return true;
        }
        // Simple primary CE.
        ++index;
        pri = p;
        // Does this have an explicit below-common sec/ter unit,
        // or does it imply a common one?
        if(index == length) {
            secTer = Collation::COMMON_SEC_AND_TER_CE;
        } else {
            secTer = elements[index];
            if((secTer & CollationRootElements::SEC_TER_DELTA_FLAG) == 0) {
                // No sec/ter delta.
                secTer = Collation::COMMON_SEC_AND_TER_CE;
            } else {
                secTer &= ~CollationRootElements::SEC_TER_DELTA_FLAG;
                if(secTer > Collation::COMMON_SEC_AND_TER_CE) {
                    // Implied sec/ter.
                    secTer = Collation::COMMON_SEC_AND_TER_CE;
                } else {
                    // Explicit sec/ter below common/common.
                    ++index;
                }
            }
        }
        return true;
    }

    uint32_t getPrimary() const { return pri; }
    uint32_t getSecTer() const { return secTer; }

private:
    const CollationData &data;
    const uint32_t *elements;
    int32_t length;

    uint32_t pri;
    uint32_t secTer;
    int32_t index;
};

}  // namespace

void CollationTest::TestRootElements() {
    IcuTestErrorCode errorCode(*this, "TestRootElements");
    const CollationData *root = CollationRoot::getData(errorCode);
    if(errorCode.errDataIfFailureAndReset("CollationRoot::getData()")) {
        return;
    }
    CollationRootElements rootElements(root->rootElements, root->rootElementsLength);
    RootElementsIterator iter(*root);

    // We check each root CE for validity,
    // and we also verify that there is a tailoring gap between each two CEs.
    CollationWeights cw1c;  // compressible primary weights
    CollationWeights cw1u;  // uncompressible primary weights
    CollationWeights cw2;
    CollationWeights cw3;

    cw1c.initForPrimary(true);
    cw1u.initForPrimary(false);
    cw2.initForSecondary();
    cw3.initForTertiary();

    // Note: The root elements do not include Han-implicit or unassigned-implicit CEs,
    // nor the special merge-separator CE for U+FFFE.
    uint32_t prevPri = 0;
    uint32_t prevSec = 0;
    uint32_t prevTer = 0;
    while(iter.next()) {
        uint32_t pri = iter.getPrimary();
        uint32_t secTer = iter.getSecTer();
        // CollationRootElements CEs must have 0 case and quaternary bits.
        if((secTer & Collation::CASE_AND_QUATERNARY_MASK) != 0) {
            errln("CollationRootElements CE has non-zero case and/or quaternary bits: %08lx %08lx",
                  (long)pri, (long)secTer);
        }
        uint32_t sec = secTer >> 16;
        uint32_t ter = secTer & Collation::ONLY_TERTIARY_MASK;
        uint32_t ctq = ter;
        if(pri == 0 && sec == 0 && ter != 0) {
            // Tertiary CEs must have uppercase bits,
            // but they are not stored in the CollationRootElements.
            ctq |= 0x8000;
        }
        if(!isValidCE(rootElements, *root, pri, sec, ctq)) {
            errln("invalid root CE %08lx %08lx", (long)pri, (long)secTer);
        } else {
            if(pri != prevPri) {
                uint32_t newWeight = 0;
                if(prevPri == 0 || prevPri >= Collation::FFFD_PRIMARY) {
                    // There is currently no tailoring gap after primary ignorables,
                    // and we forbid tailoring after U+FFFD and U+FFFF.
                } else if(root->isCompressiblePrimary(prevPri)) {
                    if(!cw1c.allocWeights(prevPri, pri, 1)) {
                        errln("no primary/compressible tailoring gap between %08lx and %08lx",
                              (long)prevPri, (long)pri);
                    } else {
                        newWeight = cw1c.nextWeight();
                    }
                } else {
                    if(!cw1u.allocWeights(prevPri, pri, 1)) {
                        errln("no primary/uncompressible tailoring gap between %08lx and %08lx",
                              (long)prevPri, (long)pri);
                    } else {
                        newWeight = cw1u.nextWeight();
                    }
                }
                if(newWeight != 0 && !(prevPri < newWeight && newWeight < pri)) {
                    errln("mis-allocated primary weight, should get %08lx < %08lx < %08lx",
                          (long)prevPri, (long)newWeight, (long)pri);
                }
            } else if(sec != prevSec) {
                uint32_t lowerLimit =
                    prevSec == 0 ? rootElements.getSecondaryBoundary() - 0x100 : prevSec;
                if(!cw2.allocWeights(lowerLimit, sec, 1)) {
                    errln("no secondary tailoring gap between %04x and %04x", lowerLimit, sec);
                } else {
                    uint32_t newWeight = cw2.nextWeight();
                    if(!(prevSec < newWeight && newWeight < sec)) {
                        errln("mis-allocated secondary weight, should get %04x < %04x < %04x",
                              (long)lowerLimit, (long)newWeight, (long)sec);
                    }
                }
            } else if(ter != prevTer) {
                uint32_t lowerLimit =
                    prevTer == 0 ? rootElements.getTertiaryBoundary() - 0x100 : prevTer;
                if(!cw3.allocWeights(lowerLimit, ter, 1)) {
                    errln("no teriary tailoring gap between %04x and %04x", lowerLimit, ter);
                } else {
                    uint32_t newWeight = cw3.nextWeight();
                    if(!(prevTer < newWeight && newWeight < ter)) {
                        errln("mis-allocated secondary weight, should get %04x < %04x < %04x",
                              (long)lowerLimit, (long)newWeight, (long)ter);
                    }
                }
            } else {
                errln("duplicate root CE %08lx %08lx", (long)pri, (long)secTer);
            }
        }
        prevPri = pri;
        prevSec = sec;
        prevTer = ter;
    }
}

void CollationTest::TestTailoredElements() {
    IcuTestErrorCode errorCode(*this, "TestTailoredElements");
    const CollationData *root = CollationRoot::getData(errorCode);
    if(errorCode.errDataIfFailureAndReset("CollationRoot::getData()")) {
        return;
    }
    CollationRootElements rootElements(root->rootElements, root->rootElementsLength);

    UHashtable *prevLocales = uhash_open(uhash_hashChars, uhash_compareChars, nullptr, errorCode);
    if(errorCode.errIfFailureAndReset("failed to create a hash table")) {
        return;
    }
    uhash_setKeyDeleter(prevLocales, uprv_free);
    // TestRootElements() tests the root collator which does not have tailorings.
    uhash_puti(prevLocales, uprv_strdup(""), 1, errorCode);
    uhash_puti(prevLocales, uprv_strdup("root"), 1, errorCode);
    uhash_puti(prevLocales, uprv_strdup("root@collation=standard"), 1, errorCode);

    UVector64 ces(errorCode);
    LocalPointer<StringEnumeration> locales(Collator::getAvailableLocales());
    U_ASSERT(locales.isValid());
    const char *localeID = "root";
    do {
        Locale locale(localeID);
        LocalPointer<StringEnumeration> types(
                Collator::getKeywordValuesForLocale("collation", locale, false, errorCode));
        errorCode.assertSuccess();
        const char *type;  // first: default type
        while((type = types->next(nullptr, errorCode)) != nullptr) {
            if(strncmp(type, "private-", 8) == 0) {
                errln("Collator::getKeywordValuesForLocale(%s) returns private collation keyword: %s",
                        localeID, type);
            }
            Locale localeWithType(locale);
            localeWithType.setKeywordValue("collation", type, errorCode);
            errorCode.assertSuccess();
            LocalPointer<Collator> coll(Collator::createInstance(localeWithType, errorCode));
            if(errorCode.errIfFailureAndReset("Collator::createInstance(%s)",
                                              localeWithType.getName())) {
                continue;
            }
            Locale actual = coll->getLocale(ULOC_ACTUAL_LOCALE, errorCode);
            if(uhash_geti(prevLocales, actual.getName()) != 0) {
                continue;
            }
            uhash_puti(prevLocales, uprv_strdup(actual.getName()), 1, errorCode);
            errorCode.assertSuccess();
            logln("TestTailoredElements(): requested %s -> actual %s",
                  localeWithType.getName(), actual.getName());
            RuleBasedCollator *rbc = dynamic_cast<RuleBasedCollator *>(coll.getAlias());
            if(rbc == nullptr) {
                continue;
            }
            // Note: It would be better to get tailored strings such that we can
            // identify the prefix, and only get the CEs for the prefix+string,
            // not also for the prefix.
            // There is currently no API for that.
            // It would help in an unusual case where a contraction starting in the prefix
            // extends past its end, and we do not see the intended mapping.
            // For example, for a mapping p|st, if there is also a contraction ps,
            // then we get CEs(ps)+CEs(t), rather than CEs(p|st).
            LocalPointer<UnicodeSet> tailored(coll->getTailoredSet(errorCode));
            errorCode.assertSuccess();
            UnicodeSetIterator iter(*tailored);
            while(iter.next()) {
                const UnicodeString &s = iter.getString();
                ces.removeAllElements();
                rbc->internalGetCEs(s, ces, errorCode);
                errorCode.assertSuccess();
                for(int32_t i = 0; i < ces.size(); ++i) {
                    int64_t ce = ces.elementAti(i);
                    if(!isValidCE(rootElements, *root, ce)) {
                        errln("invalid tailored CE %016llx at CE index %d from string:",
                              (long long)ce, (int)i);
                        infoln(prettify(s));
                    }
                }
            }
        }
    } while((localeID = locales->next(nullptr, errorCode)) != nullptr);
    uhash_close(prevLocales);
}

UnicodeString CollationTest::printSortKey(const uint8_t *p, int32_t length) {
    UnicodeString s;
    for(int32_t i = 0; i < length; ++i) {
        if(i > 0) { s.append((char16_t)0x20); }
        uint8_t b = p[i];
        if(b == 0) {
            s.append((char16_t)0x2e);  // period
        } else if(b == 1) {
            s.append((char16_t)0x7c);  // vertical bar
        } else {
            appendHex(b, 2, s);
        }
    }
    return s;
}

UnicodeString CollationTest::printCollationKey(const CollationKey &key) {
    int32_t length;
    const uint8_t *p = key.getByteArray(length);
    return printSortKey(p, length);
}

UBool CollationTest::readNonEmptyLine(UCHARBUF *f, IcuTestErrorCode &errorCode) {
    for(;;) {
        int32_t lineLength;
        const char16_t *line = ucbuf_readline(f, &lineLength, errorCode);
        if(line == nullptr || errorCode.isFailure()) {
            fileLine.remove();
            return false;
        }
        ++fileLineNumber;
        // Strip trailing CR/LF, comments, and spaces.
        const char16_t *comment = u_memchr(line, 0x23, lineLength);  // '#'
        if(comment != nullptr) {
            lineLength = (int32_t)(comment - line);
        } else {
            while(lineLength > 0 && isCROrLF(line[lineLength - 1])) { --lineLength; }
        }
        while(lineLength > 0 && isSpace(line[lineLength - 1])) { --lineLength; }
        if(lineLength != 0) {
            fileLine.setTo(false, line, lineLength);
            return true;
        }
        // Empty line, continue.
    }
}

void CollationTest::parseString(int32_t &start, UnicodeString &prefix, UnicodeString &s,
                                UErrorCode &errorCode) {
    int32_t length = fileLine.length();
    int32_t i;
    for(i = start; i < length && !isSpace(fileLine[i]); ++i) {}
    int32_t pipeIndex = fileLine.indexOf((char16_t)0x7c, start, i - start);  // '|'
    if(pipeIndex >= 0) {
        prefix = fileLine.tempSubStringBetween(start, pipeIndex).unescape();
        if(prefix.isEmpty()) {
            errln("empty prefix on line %d", (int)fileLineNumber);
            infoln(fileLine);
            errorCode = U_PARSE_ERROR;
            return;
        }
        start = pipeIndex + 1;
    } else {
        prefix.remove();
    }
    s = fileLine.tempSubStringBetween(start, i).unescape();
    if(s.isEmpty()) {
        errln("empty string on line %d", (int)fileLineNumber);
        infoln(fileLine);
        errorCode = U_PARSE_ERROR;
        return;
    }
    start = i;
}

Collation::Level CollationTest::parseRelationAndString(UnicodeString &s, IcuTestErrorCode &errorCode) {
    Collation::Level relation;
    int32_t start;
    if(fileLine[0] == 0x3c) {  // <
        char16_t second = fileLine[1];
        start = 2;
        switch(second) {
        case 0x31:  // <1
            relation = Collation::PRIMARY_LEVEL;
            break;
        case 0x32:  // <2
            relation = Collation::SECONDARY_LEVEL;
            break;
        case 0x33:  // <3
            relation = Collation::TERTIARY_LEVEL;
            break;
        case 0x34:  // <4
            relation = Collation::QUATERNARY_LEVEL;
            break;
        case 0x63:  // <c
            relation = Collation::CASE_LEVEL;
            break;
        case 0x69:  // <i
            relation = Collation::IDENTICAL_LEVEL;
            break;
        default:  // just <
            relation = Collation::NO_LEVEL;
            start = 1;
            break;
        }
    } else if(fileLine[0] == 0x3d) {  // =
        relation = Collation::ZERO_LEVEL;
        start = 1;
    } else {
        start = 0;
    }
    if(start == 0 || !isSpace(fileLine[start])) {
        errln("no relation (= < <1 <2 <c <3 <4 <i) at beginning of line %d", (int)fileLineNumber);
        infoln(fileLine);
        errorCode.set(U_PARSE_ERROR);
        return Collation::NO_LEVEL;
    }
    start = skipSpaces(start);
    UnicodeString prefix;
    parseString(start, prefix, s, errorCode);
    if(errorCode.isSuccess() && !prefix.isEmpty()) {
        errln("prefix string not allowed for test string: on line %d", (int)fileLineNumber);
        infoln(fileLine);
        errorCode.set(U_PARSE_ERROR);
        return Collation::NO_LEVEL;
    }
    if(start < fileLine.length()) {
        errln("unexpected line contents after test string on line %d", (int)fileLineNumber);
        infoln(fileLine);
        errorCode.set(U_PARSE_ERROR);
        return Collation::NO_LEVEL;
    }
    return relation;
}

static const struct {
    const char *name;
    UColAttribute attr;
} attributes[] = {
    { "backwards", UCOL_FRENCH_COLLATION },
    { "alternate", UCOL_ALTERNATE_HANDLING },
    { "caseFirst", UCOL_CASE_FIRST },
    { "caseLevel", UCOL_CASE_LEVEL },
    // UCOL_NORMALIZATION_MODE is turned on and off automatically.
    { "strength", UCOL_STRENGTH },
    // UCOL_HIRAGANA_QUATERNARY_MODE is deprecated.
    { "numeric", UCOL_NUMERIC_COLLATION }
};

static const struct {
    const char *name;
    UColAttributeValue value;
} attributeValues[] = {
    { "default", UCOL_DEFAULT },
    { "primary", UCOL_PRIMARY },
    { "secondary", UCOL_SECONDARY },
    { "tertiary", UCOL_TERTIARY },
    { "quaternary", UCOL_QUATERNARY },
    { "identical", UCOL_IDENTICAL },
    { "off", UCOL_OFF },
    { "on", UCOL_ON },
    { "shifted", UCOL_SHIFTED },
    { "non-ignorable", UCOL_NON_IGNORABLE },
    { "lower", UCOL_LOWER_FIRST },
    { "upper", UCOL_UPPER_FIRST }
};

void CollationTest::parseAndSetAttribute(IcuTestErrorCode &errorCode) {
    // Parse attributes even if the Collator could not be created,
    // in order to report syntax errors.
    int32_t start = skipSpaces(1);
    int32_t equalPos = fileLine.indexOf((char16_t)0x3d);
    if(equalPos < 0) {
        if(fileLine.compare(start, 7, UNICODE_STRING("reorder", 7)) == 0) {
            parseAndSetReorderCodes(start + 7, errorCode);
            return;
        }
        errln("missing '=' on line %d", (int)fileLineNumber);
        infoln(fileLine);
        errorCode.set(U_PARSE_ERROR);
        return;
    }

    UnicodeString attrString = fileLine.tempSubStringBetween(start, equalPos);
    UnicodeString valueString = fileLine.tempSubString(equalPos+1);
    if(attrString == UNICODE_STRING("maxVariable", 11)) {
        UColReorderCode max;
        if(valueString == UNICODE_STRING("space", 5)) {
            max = UCOL_REORDER_CODE_SPACE;
        } else if(valueString == UNICODE_STRING("punct", 5)) {
            max = UCOL_REORDER_CODE_PUNCTUATION;
        } else if(valueString == UNICODE_STRING("symbol", 6)) {
            max = UCOL_REORDER_CODE_SYMBOL;
        } else if(valueString == UNICODE_STRING("currency", 8)) {
            max = UCOL_REORDER_CODE_CURRENCY;
        } else {
            errln("invalid attribute value name on line %d", (int)fileLineNumber);
            infoln(fileLine);
            errorCode.set(U_PARSE_ERROR);
            return;
        }
        if(coll != nullptr) {
            coll->setMaxVariable(max, errorCode);
            if(errorCode.isFailure()) {
                errln("setMaxVariable() failed on line %d: %s",
                      (int)fileLineNumber, errorCode.errorName());
                infoln(fileLine);
                return;
            }
        }
        fileLine.remove();
        return;
    }

    UColAttribute attr;
    for(int32_t i = 0;; ++i) {
        if(i == UPRV_LENGTHOF(attributes)) {
            errln("invalid attribute name on line %d", (int)fileLineNumber);
            infoln(fileLine);
            errorCode.set(U_PARSE_ERROR);
            return;
        }
        if(attrString == UnicodeString(attributes[i].name, -1, US_INV)) {
            attr = attributes[i].attr;
            break;
        }
    }

    UColAttributeValue value;
    for(int32_t i = 0;; ++i) {
        if(i == UPRV_LENGTHOF(attributeValues)) {
            errln("invalid attribute value name on line %d", (int)fileLineNumber);
            infoln(fileLine);
            errorCode.set(U_PARSE_ERROR);
            return;
        }
        if(valueString == UnicodeString(attributeValues[i].name, -1, US_INV)) {
            value = attributeValues[i].value;
            break;
        }
    }

    if(coll != nullptr) {
        coll->setAttribute(attr, value, errorCode);
        if(errorCode.isFailure()) {
            errln("illegal attribute=value combination on line %d: %s",
                  (int)fileLineNumber, errorCode.errorName());
            infoln(fileLine);
            return;
        }
    }
    fileLine.remove();
}

void CollationTest::parseAndSetReorderCodes(int32_t start, IcuTestErrorCode &errorCode) {
    UVector32 reorderCodes(errorCode);
    while(start < fileLine.length()) {
        start = skipSpaces(start);
        int32_t limit = start;
        while(limit < fileLine.length() && !isSpace(fileLine[limit])) { ++limit; }
        CharString name;
        name.appendInvariantChars(fileLine.tempSubStringBetween(start, limit), errorCode);
        int32_t code = CollationRuleParser::getReorderCode(name.data());
        if(code < 0) {
            if(uprv_stricmp(name.data(), "default") == 0) {
                code = UCOL_REORDER_CODE_DEFAULT;  // -1
            } else {
                errln("invalid reorder code '%s' on line %d", name.data(), (int)fileLineNumber);
                infoln(fileLine);
                errorCode.set(U_PARSE_ERROR);
                return;
            }
        }
        reorderCodes.addElement(code, errorCode);
        start = limit;
    }
    if(coll != nullptr) {
        coll->setReorderCodes(reorderCodes.getBuffer(), reorderCodes.size(), errorCode);
        if(errorCode.isFailure()) {
            errln("setReorderCodes() failed on line %d: %s",
                  (int)fileLineNumber, errorCode.errorName());
            infoln(fileLine);
            return;
        }
    }
    fileLine.remove();
}

void CollationTest::buildTailoring(UCHARBUF *f, IcuTestErrorCode &errorCode) {
    UnicodeString rules;
    while(readNonEmptyLine(f, errorCode) && !isSectionStarter(fileLine[0])) {
        rules.append(fileLine.unescape());
    }
    if(errorCode.isFailure()) { return; }
    logln(rules);

    UParseError parseError;
    UnicodeString reason;
    delete coll;
    coll = new RuleBasedCollator(rules, parseError, reason, errorCode);
    if(coll == nullptr) {
        errln("unable to allocate a new collator");
        errorCode.set(U_MEMORY_ALLOCATION_ERROR);
        return;
    }
    if(errorCode.isFailure()) {
        dataerrln("RuleBasedCollator(rules) failed - %s", errorCode.errorName());
        infoln(UnicodeString("  reason: ") + reason);
        if(parseError.offset >= 0) { infoln("  rules offset: %d", (int)parseError.offset); }
        if(parseError.preContext[0] != 0 || parseError.postContext[0] != 0) {
            infoln(UnicodeString("  snippet: ...") +
                parseError.preContext + "(!)" + parseError.postContext + "...");
        }
        delete coll;
        coll = nullptr;
        errorCode.reset();
    } else {
        assertEquals("no error reason when RuleBasedCollator(rules) succeeds",
                     UnicodeString(), reason);
    }
}

void CollationTest::setRootCollator(IcuTestErrorCode &errorCode) {
    if(errorCode.isFailure()) { return; }
    delete coll;
    coll = Collator::createInstance(Locale::getRoot(), errorCode);
    if(errorCode.isFailure()) {
        dataerrln("unable to create a root collator");
        return;
    }
}

void CollationTest::setLocaleCollator(IcuTestErrorCode &errorCode) {
    if(errorCode.isFailure()) { return; }
    delete coll;
    coll = nullptr;
    int32_t at = fileLine.indexOf((char16_t)0x40, 9);  // @ is not invariant
    if(at >= 0) {
        fileLine.setCharAt(at, (char16_t)0x2a);  // *
    }
    CharString localeID;
    localeID.appendInvariantChars(fileLine.tempSubString(9), errorCode);
    if(at >= 0) {
        localeID.data()[at - 9] = '@';
    }
    Locale locale(localeID.data());
    if(fileLine.length() == 9 || errorCode.isFailure() || locale.isBogus()) {
        errln("invalid language tag on line %d", (int)fileLineNumber);
        infoln(fileLine);
        if(errorCode.isSuccess()) { errorCode.set(U_PARSE_ERROR); }
        return;
    }

    logln("creating a collator for locale ID %s", locale.getName());
    coll = Collator::createInstance(locale, errorCode);
    if(errorCode.isFailure()) {
        dataerrln("unable to create a collator for locale %s on line %d",
                  locale.getName(), (int)fileLineNumber);
        infoln(fileLine);
        delete coll;
        coll = nullptr;
        errorCode.reset();
    }
}

UBool CollationTest::needsNormalization(const UnicodeString &s, UErrorCode &errorCode) const {
    if(U_FAILURE(errorCode) || !fcd->isNormalized(s, errorCode)) { return true; }
    // In some sequences with Tibetan composite vowel signs,
    // even if the string passes the FCD check,
    // those composites must be decomposed.
    // Check if s contains 0F71 immediately followed by 0F73 or 0F75 or 0F81.
    int32_t index = 0;
    while((index = s.indexOf((char16_t)0xf71, index)) >= 0) {
        if(++index < s.length()) {
            char16_t c = s[index];
            if(c == 0xf73 || c == 0xf75 || c == 0xf81) { return true; }
        }
    }
    return false;
}

UBool CollationTest::getSortKeyParts(const char16_t *s, int32_t length,
                                     CharString &dest, int32_t partSize,
                                     IcuTestErrorCode &errorCode) {
    if(errorCode.isFailure()) { return false; }
    uint8_t part[32];
    U_ASSERT(partSize <= UPRV_LENGTHOF(part));
    UCharIterator iter;
    uiter_setString(&iter, s, length);
    uint32_t state[2] = { 0, 0 };
    for(;;) {
        int32_t partLength = coll->internalNextSortKeyPart(&iter, state, part, partSize, errorCode);
        UBool done = partLength < partSize;
        if(done) {
            // At the end, append the next byte as well which should be 00.
            ++partLength;
        }
        dest.append(reinterpret_cast<char *>(part), partLength, errorCode);
        if(done) {
            return errorCode.isSuccess();
        }
    }
}

UBool CollationTest::getCollationKey(const char *norm, const UnicodeString &line,
                                     const char16_t *s, int32_t length,
                                     CollationKey &key, IcuTestErrorCode &errorCode) {
    if(errorCode.isFailure()) { return false; }
    coll->getCollationKey(s, length, key, errorCode);
    if(errorCode.isFailure()) {
        infoln(fileTestName);
        errln("Collator(%s).getCollationKey() failed: %s",
              norm, errorCode.errorName());
        infoln(line);
        return false;
    }
    int32_t keyLength;
    const uint8_t *keyBytes = key.getByteArray(keyLength);
    if(keyLength == 0 || keyBytes[keyLength - 1] != 0) {
        infoln(fileTestName);
        errln("Collator(%s).getCollationKey() wrote an empty or unterminated key",
              norm);
        infoln(line);
        infoln(printCollationKey(key));
        return false;
    }

    int32_t numLevels = coll->getAttribute(UCOL_STRENGTH, errorCode);
    if(numLevels < UCOL_IDENTICAL) {
        ++numLevels;
    } else {
        numLevels = 5;
    }
    if(coll->getAttribute(UCOL_CASE_LEVEL, errorCode) == UCOL_ON) {
        ++numLevels;
    }
    errorCode.assertSuccess();
    int32_t numLevelSeparators = 0;
    for(int32_t i = 0; i < (keyLength - 1); ++i) {
        uint8_t b = keyBytes[i];
        if(b == 0) {
            infoln(fileTestName);
            errln("Collator(%s).getCollationKey() contains a 00 byte", norm);
            infoln(line);
            infoln(printCollationKey(key));
            return false;
        }
        if(b == 1) { ++numLevelSeparators; }
    }
    if(numLevelSeparators != (numLevels - 1)) {
        infoln(fileTestName);
        errln("Collator(%s).getCollationKey() has %d level separators for %d levels",
              norm, (int)numLevelSeparators, (int)numLevels);
        infoln(line);
        infoln(printCollationKey(key));
        return false;
    }

    // Check that internalNextSortKeyPart() makes the same key, with several part sizes.
    static const int32_t partSizes[] = { 32, 3, 1 };
    for(int32_t psi = 0; psi < UPRV_LENGTHOF(partSizes); ++psi) {
        int32_t partSize = partSizes[psi];
        CharString parts;
        if(!getSortKeyParts(s, length, parts, 32, errorCode)) {
            infoln(fileTestName);
            errln("Collator(%s).internalNextSortKeyPart(%d) failed: %s",
                  norm, (int)partSize, errorCode.errorName());
            infoln(line);
            return false;
        }
        if(keyLength != parts.length() || uprv_memcmp(keyBytes, parts.data(), keyLength) != 0) {
            infoln(fileTestName);
            errln("Collator(%s).getCollationKey() != internalNextSortKeyPart(%d)",
                  norm, (int)partSize);
            infoln(line);
            infoln(printCollationKey(key));
            infoln(printSortKey(reinterpret_cast<uint8_t *>(parts.data()), parts.length()));
            return false;
        }
    }
    return true;
}

/**
 * Changes the key to the merged segments of the U+FFFE-separated substrings of s.
 * Leaves key unchanged if s does not contain U+FFFE.
 * @return true if the key was successfully changed
 */
UBool CollationTest::getMergedCollationKey(const char16_t *s, int32_t length,
                                           CollationKey &key, IcuTestErrorCode &errorCode) {
    if(errorCode.isFailure()) { return false; }
    LocalMemory<uint8_t> mergedKey;
    int32_t mergedKeyLength = 0;
    int32_t mergedKeyCapacity = 0;
    int32_t sLength = (length >= 0) ? length : u_strlen(s);
    int32_t segmentStart = 0;
    for(int32_t i = 0;;) {
        if(i == sLength) {
            if(segmentStart == 0) {
                // s does not contain any U+FFFE.
                return false;
            }
        } else if(s[i] != 0xfffe) {
            ++i;
            continue;
        }
        // Get the sort key for another segment and merge it into mergedKey.
        CollationKey key1(mergedKey.getAlias(), mergedKeyLength);  // copies the bytes
        CollationKey key2;
        coll->getCollationKey(s + segmentStart, i - segmentStart, key2, errorCode);
        int32_t key1Length, key2Length;
        const uint8_t *key1Bytes = key1.getByteArray(key1Length);
        const uint8_t *key2Bytes = key2.getByteArray(key2Length);
        uint8_t *dest;
        int32_t minCapacity = key1Length + key2Length;
        if(key1Length > 0) { --minCapacity; }
        if(minCapacity <= mergedKeyCapacity) {
            dest = mergedKey.getAlias();
        } else {
            if(minCapacity <= 200) {
                mergedKeyCapacity = 200;
            } else if(minCapacity <= 2 * mergedKeyCapacity) {
                mergedKeyCapacity *= 2;
            } else {
                mergedKeyCapacity = minCapacity;
            }
            dest = mergedKey.allocateInsteadAndReset(mergedKeyCapacity);
        }
        U_ASSERT(dest != nullptr || mergedKeyCapacity == 0);
        if(key1Length == 0) {
            // key2 is the sort key for the first segment.
            uprv_memcpy(dest, key2Bytes, key2Length);
            mergedKeyLength = key2Length;
        } else {
            mergedKeyLength =
                ucol_mergeSortkeys(key1Bytes, key1Length, key2Bytes, key2Length,
                                   dest, mergedKeyCapacity);
        }
        if(i == sLength) { break; }
        segmentStart = ++i;
    }
    key = CollationKey(mergedKey.getAlias(), mergedKeyLength);
    return true;
}

namespace {

/**
 * Replaces unpaired surrogates with U+FFFD.
 * Returns s if no replacement was made, otherwise buffer.
 */
const UnicodeString &surrogatesToFFFD(const UnicodeString &s, UnicodeString &buffer) {
    int32_t i = 0;
    while(i < s.length()) {
        UChar32 c = s.char32At(i);
        if(U_IS_SURROGATE(c)) {
            if(buffer.length() < i) {
                buffer.append(s, buffer.length(), i - buffer.length());
            }
            buffer.append((char16_t)0xfffd);
        }
        i += U16_LENGTH(c);
    }
    if(buffer.isEmpty()) {
        return s;
    }
    if(buffer.length() < i) {
        buffer.append(s, buffer.length(), i - buffer.length());
    }
    return buffer;
}

int32_t getDifferenceLevel(const CollationKey &prevKey, const CollationKey &key,
                           UCollationResult order, UBool collHasCaseLevel) {
    if(order == UCOL_EQUAL) {
        return Collation::NO_LEVEL;
    }
    int32_t prevKeyLength;
    const uint8_t *prevBytes = prevKey.getByteArray(prevKeyLength);
    int32_t keyLength;
    const uint8_t *bytes = key.getByteArray(keyLength);
    int32_t level = Collation::PRIMARY_LEVEL;
    for(int32_t i = 0;; ++i) {
        uint8_t b = prevBytes[i];
        if(b != bytes[i]) { break; }
        if(b == Collation::LEVEL_SEPARATOR_BYTE) {
            ++level;
            if(level == Collation::CASE_LEVEL && !collHasCaseLevel) {
                ++level;
            }
        }
    }
    return level;
}

}

UBool CollationTest::checkCompareTwo(const char *norm, const UnicodeString &prevFileLine,
                                     const UnicodeString &prevString, const UnicodeString &s,
                                     UCollationResult expectedOrder, Collation::Level expectedLevel,
                                     IcuTestErrorCode &errorCode) {
    if(errorCode.isFailure()) { return false; }

    // Get the sort keys first, for error debug output.
    CollationKey prevKey;
    if(!getCollationKey(norm, prevFileLine, prevString.getBuffer(), prevString.length(),
                        prevKey, errorCode)) {
        return false;
    }
    CollationKey key;
    if(!getCollationKey(norm, fileLine, s.getBuffer(), s.length(), key, errorCode)) { return false; }

    UCollationResult order = coll->compare(prevString, s, errorCode);
    if(order != expectedOrder || errorCode.isFailure()) {
        infoln(fileTestName);
        errln("line %d Collator(%s).compare(previous, current) wrong order: %d != %d (%s)",
              (int)fileLineNumber, norm, order, expectedOrder, errorCode.errorName());
        infoln(prevFileLine);
        infoln(fileLine);
        infoln(printCollationKey(prevKey));
        infoln(printCollationKey(key));
        return false;
    }
    order = coll->compare(s, prevString, errorCode);
    if(order != -expectedOrder || errorCode.isFailure()) {
        infoln(fileTestName);
        errln("line %d Collator(%s).compare(current, previous) wrong order: %d != %d (%s)",
              (int)fileLineNumber, norm, order, -expectedOrder, errorCode.errorName());
        infoln(prevFileLine);
        infoln(fileLine);
        infoln(printCollationKey(prevKey));
        infoln(printCollationKey(key));
        return false;
    }
    // Test NUL-termination if the strings do not contain NUL characters.
    UBool containNUL = prevString.indexOf((char16_t)0) >= 0 || s.indexOf((char16_t)0) >= 0;
    if(!containNUL) {
        order = coll->compare(prevString.getBuffer(), -1, s.getBuffer(), -1, errorCode);
        if(order != expectedOrder || errorCode.isFailure()) {
            infoln(fileTestName);
            errln("line %d Collator(%s).compare(previous-NUL, current-NUL) wrong order: %d != %d (%s)",
                  (int)fileLineNumber, norm, order, expectedOrder, errorCode.errorName());
            infoln(prevFileLine);
            infoln(fileLine);
            infoln(printCollationKey(prevKey));
            infoln(printCollationKey(key));
            return false;
        }
        order = coll->compare(s.getBuffer(), -1, prevString.getBuffer(), -1, errorCode);
        if(order != -expectedOrder || errorCode.isFailure()) {
            infoln(fileTestName);
            errln("line %d Collator(%s).compare(current-NUL, previous-NUL) wrong order: %d != %d (%s)",
                  (int)fileLineNumber, norm, order, -expectedOrder, errorCode.errorName());
            infoln(prevFileLine);
            infoln(fileLine);
            infoln(printCollationKey(prevKey));
            infoln(printCollationKey(key));
            return false;
        }
    }

    // compare(UTF-16) treats unpaired surrogates like unassigned code points.
    // Unpaired surrogates cannot be converted to UTF-8.
    // Create valid UTF-16 strings if necessary, and use those for
    // both the expected compare() result and for the input to compare(UTF-8).
    UnicodeString prevBuffer, sBuffer;
    const UnicodeString &prevValid = surrogatesToFFFD(prevString, prevBuffer);
    const UnicodeString &sValid = surrogatesToFFFD(s, sBuffer);
    std::string prevUTF8, sUTF8;
    UnicodeString(prevValid).toUTF8String(prevUTF8);
    UnicodeString(sValid).toUTF8String(sUTF8);
    UCollationResult expectedUTF8Order;
    if(&prevValid == &prevString && &sValid == &s) {
        expectedUTF8Order = expectedOrder;
    } else {
        expectedUTF8Order = coll->compare(prevValid, sValid, errorCode);
    }

    order = coll->compareUTF8(prevUTF8, sUTF8, errorCode);
    if(order != expectedUTF8Order || errorCode.isFailure()) {
        infoln(fileTestName);
        errln("line %d Collator(%s).compareUTF8(previous, current) wrong order: %d != %d (%s)",
              (int)fileLineNumber, norm, order, expectedUTF8Order, errorCode.errorName());
        infoln(prevFileLine);
        infoln(fileLine);
        infoln(printCollationKey(prevKey));
        infoln(printCollationKey(key));
        return false;
    }
    order = coll->compareUTF8(sUTF8, prevUTF8, errorCode);
    if(order != -expectedUTF8Order || errorCode.isFailure()) {
        infoln(fileTestName);
        errln("line %d Collator(%s).compareUTF8(current, previous) wrong order: %d != %d (%s)",
              (int)fileLineNumber, norm, order, -expectedUTF8Order, errorCode.errorName());
        infoln(prevFileLine);
        infoln(fileLine);
        infoln(printCollationKey(prevKey));
        infoln(printCollationKey(key));
        return false;
    }
    // Test NUL-termination if the strings do not contain NUL characters.
    if(!containNUL) {
        order = coll->internalCompareUTF8(prevUTF8.c_str(), -1, sUTF8.c_str(), -1, errorCode);
        if(order != expectedUTF8Order || errorCode.isFailure()) {
            infoln(fileTestName);
            errln("line %d Collator(%s).internalCompareUTF8(previous-NUL, current-NUL) wrong order: %d != %d (%s)",
                  (int)fileLineNumber, norm, order, expectedUTF8Order, errorCode.errorName());
            infoln(prevFileLine);
            infoln(fileLine);
            infoln(printCollationKey(prevKey));
            infoln(printCollationKey(key));
            return false;
        }
        order = coll->internalCompareUTF8(sUTF8.c_str(), -1, prevUTF8.c_str(), -1, errorCode);
        if(order != -expectedUTF8Order || errorCode.isFailure()) {
            infoln(fileTestName);
            errln("line %d Collator(%s).internalCompareUTF8(current-NUL, previous-NUL) wrong order: %d != %d (%s)",
                  (int)fileLineNumber, norm, order, -expectedUTF8Order, errorCode.errorName());
            infoln(prevFileLine);
            infoln(fileLine);
            infoln(printCollationKey(prevKey));
            infoln(printCollationKey(key));
            return false;
        }
    }

    UCharIterator leftIter;
    UCharIterator rightIter;
    uiter_setString(&leftIter, prevString.getBuffer(), prevString.length());
    uiter_setString(&rightIter, s.getBuffer(), s.length());
    order = coll->compare(leftIter, rightIter, errorCode);
    if(order != expectedOrder || errorCode.isFailure()) {
        infoln(fileTestName);
        errln("line %d Collator(%s).compare(UCharIterator: previous, current) "
              "wrong order: %d != %d (%s)",
              (int)fileLineNumber, norm, order, expectedOrder, errorCode.errorName());
        infoln(prevFileLine);
        infoln(fileLine);
        infoln(printCollationKey(prevKey));
        infoln(printCollationKey(key));
        return false;
    }

    order = prevKey.compareTo(key, errorCode);
    if(order != expectedOrder || errorCode.isFailure()) {
        infoln(fileTestName);
        errln("line %d Collator(%s).getCollationKey(previous, current).compareTo() wrong order: %d != %d (%s)",
              (int)fileLineNumber, norm, order, expectedOrder, errorCode.errorName());
        infoln(prevFileLine);
        infoln(fileLine);
        infoln(printCollationKey(prevKey));
        infoln(printCollationKey(key));
        return false;
    }
    UBool collHasCaseLevel = coll->getAttribute(UCOL_CASE_LEVEL, errorCode) == UCOL_ON;
    int32_t level = getDifferenceLevel(prevKey, key, order, collHasCaseLevel);
    if(order != UCOL_EQUAL && expectedLevel != Collation::NO_LEVEL) {
        if(level != expectedLevel) {
            infoln(fileTestName);
            errln("line %d Collator(%s).getCollationKey(previous, current).compareTo()=%d wrong level: %d != %d",
                  (int)fileLineNumber, norm, order, level, expectedLevel);
            infoln(prevFileLine);
            infoln(fileLine);
            infoln(printCollationKey(prevKey));
            infoln(printCollationKey(key));
            return false;
        }
    }

    // If either string contains U+FFFE, then their sort keys must compare the same as
    // the merged sort keys of each string's between-FFFE segments.
    //
    // It is not required that
    //   sortkey(str1 + "\uFFFE" + str2) == mergeSortkeys(sortkey(str1), sortkey(str2))
    // only that those two methods yield the same order.
    //
    // Use bit-wise OR so that getMergedCollationKey() is always called for both strings.
    if((getMergedCollationKey(prevString.getBuffer(), prevString.length(), prevKey, errorCode) |
                getMergedCollationKey(s.getBuffer(), s.length(), key, errorCode)) ||
            errorCode.isFailure()) {
        order = prevKey.compareTo(key, errorCode);
        if(order != expectedOrder || errorCode.isFailure()) {
            infoln(fileTestName);
            errln("line %d ucol_mergeSortkeys(Collator(%s).getCollationKey"
                "(previous, current segments between U+FFFE)).compareTo() wrong order: %d != %d (%s)",
                (int)fileLineNumber, norm, order, expectedOrder, errorCode.errorName());
            infoln(prevFileLine);
            infoln(fileLine);
            infoln(printCollationKey(prevKey));
            infoln(printCollationKey(key));
            return false;
        }
        int32_t mergedLevel = getDifferenceLevel(prevKey, key, order, collHasCaseLevel);
        if(order != UCOL_EQUAL && expectedLevel != Collation::NO_LEVEL) {
            if(mergedLevel != level) {
                infoln(fileTestName);
                errln("line %d ucol_mergeSortkeys(Collator(%s).getCollationKey"
                    "(previous, current segments between U+FFFE)).compareTo()=%d wrong level: %d != %d",
                    (int)fileLineNumber, norm, order, mergedLevel, level);
                infoln(prevFileLine);
                infoln(fileLine);
                infoln(printCollationKey(prevKey));
                infoln(printCollationKey(key));
                return false;
            }
        }
    }
    return true;
}

void CollationTest::checkCompareStrings(UCHARBUF *f, IcuTestErrorCode &errorCode) {
    if(errorCode.isFailure()) { return; }
    UnicodeString prevFileLine = UNICODE_STRING("(none)", 6);
    UnicodeString prevString, s;
    prevString.getTerminatedBuffer();  // Ensure NUL-termination.
    while(readNonEmptyLine(f, errorCode) && !isSectionStarter(fileLine[0])) {
        // Parse the line even if it will be ignored (when we do not have a Collator)
        // in order to report syntax issues.
        Collation::Level relation = parseRelationAndString(s, errorCode);
        if(errorCode.isFailure()) {
            errorCode.reset();
            break;
        }
        if(coll == nullptr) {
            // We were unable to create the Collator but continue with tests.
            // Ignore test data for this Collator.
            // The next Collator creation might work.
            continue;
        }
        UCollationResult expectedOrder = (relation == Collation::ZERO_LEVEL) ? UCOL_EQUAL : UCOL_LESS;
        Collation::Level expectedLevel = relation;
        s.getTerminatedBuffer();  // Ensure NUL-termination.
        UBool isOk = true;
        if(!needsNormalization(prevString, errorCode) && !needsNormalization(s, errorCode)) {
            coll->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_OFF, errorCode);
            isOk = checkCompareTwo("normalization=on", prevFileLine, prevString, s,
                                   expectedOrder, expectedLevel, errorCode);
        }
        if(isOk) {
            coll->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_ON, errorCode);
            isOk = checkCompareTwo("normalization=off", prevFileLine, prevString, s,
                                   expectedOrder, expectedLevel, errorCode);
        }
        if(isOk && (!nfd->isNormalized(prevString, errorCode) || !nfd->isNormalized(s, errorCode))) {
            UnicodeString pn = nfd->normalize(prevString, errorCode);
            UnicodeString n = nfd->normalize(s, errorCode);
            pn.getTerminatedBuffer();
            n.getTerminatedBuffer();
            errorCode.assertSuccess();
            isOk = checkCompareTwo("NFD input", prevFileLine, pn, n,
                                   expectedOrder, expectedLevel, errorCode);
        }
        if(!isOk) {
            errorCode.reset();  // already reported
        }
        prevFileLine = fileLine;
        prevString = s;
        prevString.getTerminatedBuffer();  // Ensure NUL-termination.
    }
}

void CollationTest::TestDataDriven() {
    IcuTestErrorCode errorCode(*this, "TestDataDriven");

    fcd = Normalizer2Factory::getFCDInstance(errorCode);
    nfd = Normalizer2::getNFDInstance(errorCode);
    if(errorCode.errDataIfFailureAndReset("Normalizer2Factory::getFCDInstance() or getNFDInstance()")) {
        return;
    }

    CharString path(getSourceTestData(errorCode), errorCode);
    path.appendPathPart("collationtest.txt", errorCode);
    const char *codePage = "UTF-8";
    LocalUCHARBUFPointer f(ucbuf_open(path.data(), &codePage, true, false, errorCode));
    if(errorCode.errIfFailureAndReset("ucbuf_open(collationtest.txt)")) {
        return;
    }
    // Read a new line if necessary.
    // Sub-parsers leave the first line set that they do not handle.
    while(errorCode.isSuccess() && (!fileLine.isEmpty() || readNonEmptyLine(f.getAlias(), errorCode))) {
        if(!isSectionStarter(fileLine[0])) {
            errln("syntax error on line %d", (int)fileLineNumber);
            infoln(fileLine);
            return;
        }
        if(fileLine.startsWith(UNICODE_STRING("** test: ", 9))) {
            fileTestName = fileLine;
            logln(fileLine);
            fileLine.remove();
        } else if(fileLine == UNICODE_STRING("@ root", 6)) {
            setRootCollator(errorCode);
            fileLine.remove();
        } else if(fileLine.startsWith(UNICODE_STRING("@ locale ", 9))) {
            setLocaleCollator(errorCode);
            fileLine.remove();
        } else if(fileLine == UNICODE_STRING("@ rules", 7)) {
            buildTailoring(f.getAlias(), errorCode);
        } else if(fileLine[0] == 0x25 && isSpace(fileLine[1])) {  // %
            parseAndSetAttribute(errorCode);
        } else if(fileLine == UNICODE_STRING("* compare", 9)) {
            checkCompareStrings(f.getAlias(), errorCode);
        } else {
            errln("syntax error on line %d", (int)fileLineNumber);
            infoln(fileLine);
            return;
        }
    }
}

void CollationTest::TestLongLocale() {
    IcuTestErrorCode errorCode(*this, "TestLongLocale");
    Locale longLocale("sie__1G_C_CEIE_CEZCX_CSUE_E_EIESZNI2_GB_LM_LMCSUE_LMCSX_"
                      "LVARIANT_MMCSIE_STEU_SU1GCEIE_SU6G_SU6SU6G_U_UBGE_UC_"
                      "UCEZCSI_UCIE_UZSIU_VARIANT_X@collation=bcs-ukvsz");
    LocalPointer<Collator> coll(Collator::createInstance(longLocale, errorCode));
}

void CollationTest::TestHang22414() {
    IcuTestErrorCode errorCode(*this, "TestHang22414");
    const char* cases[] = {
        "en", // just make sure the code work.
        // The following hang before fixing ICU-22414
        "sr-Latn-TH-t-su-BM-u-co-private-unihan-x-lvariant-zxsuhc-vss-vjf-0-kn-"
        "uaktmtca-uce66u-vtcb1ik-ubsuuuk8-u3iucls-ue38925l-vau30i-u6uccttg-"
        "u1iuylik-u-ueein-zzzz",
    };
    for(int32_t i = 0; i < UPRV_LENGTHOF(cases); i ++) {
        icu::Locale l = icu::Locale::forLanguageTag(cases[i], errorCode);
        // Make sure the following won't hang.
        LocalPointer<Collator> coll(Collator::createInstance(l, errorCode));
        errorCode.reset();
    }
}
void CollationTest::TestBuilderContextsOverflow() {
    IcuTestErrorCode errorCode(*this, "TestBuilderContextsOverflow");
    // ICU-20715: Bad memory access in what looks like a bogus CharsTrie after
    // intermediate contextual-mappings data overflowed.
    // Caused by the CollationDataBuilder using some outdated values when building
    // contextual mappings with both prefix and contraction matching.
    // Fixed by resetting those outdated values before code looks at them.
    char16_t rules[] = {
        u'&', 0x10, 0x2ff, 0x503c, 0x4617,
        u'=', 0x80, 0x4f7f, 0xff, 0x3c3d, 0x1c4f, 0x3c3c,
        u'<', 0, 0, 0, 0, u'|', 0, 0, 0, 0, 0, 0xf400, 0x30ff, 0, 0, 0x4f7f, 0xff,
        u'=', 0, u'|', 0, 0, 0, 0, 0, 0, 0x1f00, 0xe30,
        0x3035, 0, 0, 0xd200, 0, 0x7f00, 0xff4f, 0x3d00, 0, 0x7c00,
        0, 0, 0, 0, 0, 0, 0, 0x301f, 0x350e, 0x30,
        0, 0, 0xd2, 0x7c00, 0, 0, 0, 0, 0, 0,
        0, 0x301f, 0x350e, 0x30, 0, 0, 0x52d2, 0x2f3c, 0x5552, 0x493c,
        0x1f10, 0x1f50, 0x300, 0, 0, 0xf400, 0x30ff, 0, 0, 0x4f7f,
        0xff,
        u'=', 0, u'|', 0, 0, 0, 0, 0x5000, 0x4617,
        u'=', 0x80, 0x4f7f, 0, 0, 0xd200, 0
    };
    UnicodeString s(false, rules, UPRV_LENGTHOF(rules));
    LocalPointer<Collator> coll(new RuleBasedCollator(s, errorCode), errorCode);
    if(errorCode.isSuccess()) {
        logln("successfully built the Collator");
    }
}

#endif  // !UCONFIG_NO_COLLATION
