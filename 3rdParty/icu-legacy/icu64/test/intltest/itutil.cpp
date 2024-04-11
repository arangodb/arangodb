// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include <utility>

/**
 * IntlTestUtilities is the medium level test class for everything in the directory "utility".
 */

#include "unicode/utypes.h"
#include "unicode/errorcode.h"
#include "unicode/localpointer.h"
#include "charstr.h"
#include "itutil.h"
#include "strtest.h"
#include "loctest.h"
#include "localebuildertest.h"
#include "citrtest.h"
#include "ustrtest.h"
#include "ucdtest.h"
#include "restest.h"
#include "restsnew.h"
#include "tsmthred.h"
#include "tsputil.h"
#include "uobjtest.h"
#include "utxttest.h"
#include "v32test.h"
#include "uvectest.h" 
#include "aliastst.h"
#include "usettest.h"

extern IntlTest *createBytesTrieTest();
static IntlTest *createLocalPointerTest();
extern IntlTest *createUCharsTrieTest();
static IntlTest *createEnumSetTest();
extern IntlTest *createSimpleFormatterTest();
extern IntlTest *createUnifiedCacheTest();
extern IntlTest *createQuantityFormatterTest();
extern IntlTest *createPluralMapTest();
#if !UCONFIG_NO_FORMATTING
extern IntlTest *createStaticUnicodeSetsTest();
#endif


#define CASE(id, test) case id:                               \
                          name = #test;                       \
                          if (exec) {                         \
                              logln(#test "---"); logln();    \
                              test t;                         \
                              callTest(t, par);               \
                          }                                   \
                          break

void IntlTestUtilities::runIndexedTest( int32_t index, UBool exec, const char* &name, char* par )
{
    if (exec) logln("TestSuite Utilities: ");
    switch (index) {
        CASE(0, MultithreadTest);
        CASE(1, StringTest);
        CASE(2, UnicodeStringTest);
        CASE(3, LocaleTest);
        CASE(4, CharIterTest);
        CASE(5, UObjectTest);
        CASE(6, UnicodeTest);
        CASE(7, ResourceBundleTest);
        CASE(8, NewResourceBundleTest);
        CASE(9, PUtilTest);
        CASE(10, UVector32Test);
        CASE(11, UVectorTest);
        CASE(12, UTextTest);
        CASE(13, LocaleAliasTest);
        CASE(14, UnicodeSetTest);
        CASE(15, ErrorCodeTest);
        case 16:
            name = "LocalPointerTest";
            if (exec) {
                logln("TestSuite LocalPointerTest---"); logln();
                LocalPointer<IntlTest> test(createLocalPointerTest());
                callTest(*test, par);
            }
            break;
        case 17:
            name = "BytesTrieTest";
            if (exec) {
                logln("TestSuite BytesTrieTest---"); logln();
                LocalPointer<IntlTest> test(createBytesTrieTest());
                callTest(*test, par);
            }
            break;
        case 18:
            name = "UCharsTrieTest";
            if (exec) {
                logln("TestSuite UCharsTrieTest---"); logln();
                LocalPointer<IntlTest> test(createUCharsTrieTest());
                callTest(*test, par);
            }
            break;
        case 19:
            name = "EnumSetTest";
            if (exec) {
                logln("TestSuite EnumSetTest---"); logln();
                LocalPointer<IntlTest> test(createEnumSetTest());
                callTest(*test, par);
            }
            break;
        case 20:
            name = "SimpleFormatterTest";
            if (exec) {
                logln("TestSuite SimpleFormatterTest---"); logln();
                LocalPointer<IntlTest> test(createSimpleFormatterTest());
                callTest(*test, par);
            }
            break;
        case 21:
            name = "UnifiedCacheTest";
            if (exec) {
                logln("TestSuite UnifiedCacheTest---"); logln();
                LocalPointer<IntlTest> test(createUnifiedCacheTest());
                callTest(*test, par);
            }
            break;
        case 22:
            name = "QuantityFormatterTest";
            if (exec) {
                logln("TestSuite QuantityFormatterTest---"); logln();
                LocalPointer<IntlTest> test(createQuantityFormatterTest());
                callTest(*test, par);
            }
            break;
        case 23: 
            name = "PluralMapTest"; 
            if (exec) { 
                logln("TestSuite PluralMapTest---"); logln(); 
                LocalPointer<IntlTest> test(createPluralMapTest()); 
                callTest(*test, par); 
            } 
            break;
        case 24:
            name = "StaticUnicodeSetsTest";
#if !UCONFIG_NO_FORMATTING
            if (exec) {
                logln("TestSuite StaticUnicodeSetsTest---"); logln();
                LocalPointer<IntlTest> test(createStaticUnicodeSetsTest());
                callTest(*test, par);
            }
#endif
            break;
        CASE(25, LocaleBuilderTest);
        default: name = ""; break; //needed to end loop
    }
}

void ErrorCodeTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/) {
    if (exec) logln("TestSuite Utilities: ");
    switch (index) {
        case 0: name = "TestErrorCode"; if (exec) TestErrorCode(); break;
        case 1: name = "TestSubclass"; if (exec) TestSubclass(); break;
        case 2: name = "TestIcuTestErrorCode"; if (exec) TestIcuTestErrorCode(); break;
        default: name = ""; break; //needed to end loop
    }
}

static void RefPlusOne(UErrorCode &code) { code=(UErrorCode)(code+1); }
static void PtrPlusTwo(UErrorCode *code) { *code=(UErrorCode)(*code+2); }

void ErrorCodeTest::TestErrorCode() {
    ErrorCode errorCode;
    if(errorCode.get()!=U_ZERO_ERROR || !errorCode.isSuccess() || errorCode.isFailure()) {
        errln("ErrorCode did not initialize properly");
        return;
    }
    errorCode.assertSuccess();
    if(errorCode.errorName()!=u_errorName(U_ZERO_ERROR)) {
        errln("ErrorCode did not format error message string properly");
    }
    RefPlusOne(errorCode);
    if(errorCode.get()!=U_ILLEGAL_ARGUMENT_ERROR || errorCode.isSuccess() || !errorCode.isFailure()) {
        errln("ErrorCode did not yield a writable reference");
    }
    PtrPlusTwo(errorCode);
    if(errorCode.get()!=U_INVALID_FORMAT_ERROR || errorCode.isSuccess() || !errorCode.isFailure()) {
        errln("ErrorCode did not yield a writable pointer");
    }
    errorCode.set(U_PARSE_ERROR);
    if(errorCode.get()!=U_PARSE_ERROR || errorCode.isSuccess() || !errorCode.isFailure()) {
        errln("ErrorCode.set() failed");
    }
    if( errorCode.reset()!=U_PARSE_ERROR || errorCode.get()!=U_ZERO_ERROR ||
        !errorCode.isSuccess() || errorCode.isFailure()
    ) {
        errln("ErrorCode did not reset properly");
    }
}

class MyErrorCode: public ErrorCode {
public:
    MyErrorCode(int32_t &countChecks, int32_t &countDests)
        : checks(countChecks), dests(countDests) {}
    ~MyErrorCode() {
        if(isFailure()) {
            ++dests;
        }
    }
private:
    virtual void handleFailure() const {
        ++checks;
    }
    int32_t &checks;
    int32_t &dests;
};

void ErrorCodeTest::TestSubclass() {
    int32_t countChecks=0;
    int32_t countDests=0;
    {
        MyErrorCode errorCode(countChecks, countDests);
        if( errorCode.get()!=U_ZERO_ERROR || !errorCode.isSuccess() || errorCode.isFailure() ||
            countChecks!=0 || countDests!=0
        ) {
            errln("ErrorCode did not initialize properly");
            return;
        }
        errorCode.assertSuccess();
        if(countChecks!=0) {
            errln("ErrorCode.assertSuccess() called handleFailure() despite success");
        }
        RefPlusOne(errorCode);
        if(errorCode.get()!=U_ILLEGAL_ARGUMENT_ERROR || errorCode.isSuccess() || !errorCode.isFailure()) {
            errln("ErrorCode did not yield a writable reference");
        }
        errorCode.assertSuccess();
        if(countChecks!=1) {
            errln("ErrorCode.assertSuccess() did not handleFailure()");
        }
        PtrPlusTwo(errorCode);
        if(errorCode.get()!=U_INVALID_FORMAT_ERROR || errorCode.isSuccess() || !errorCode.isFailure()) {
            errln("ErrorCode did not yield a writable pointer");
        }
        errorCode.assertSuccess();
        if(countChecks!=2) {
            errln("ErrorCode.assertSuccess() did not handleFailure()");
        }
        errorCode.set(U_PARSE_ERROR);
        if(errorCode.get()!=U_PARSE_ERROR || errorCode.isSuccess() || !errorCode.isFailure()) {
            errln("ErrorCode.set() failed");
        }
        if( errorCode.reset()!=U_PARSE_ERROR || errorCode.get()!=U_ZERO_ERROR ||
            !errorCode.isSuccess() || errorCode.isFailure()
        ) {
            errln("ErrorCode did not reset properly");
        }
        errorCode.assertSuccess();
        if(countChecks!=2) {
            errln("ErrorCode.assertSuccess() called handleFailure() despite success");
        }
    }
    if(countDests!=0) {
        errln("MyErrorCode destructor detected failure despite success");
    }
    countChecks=countDests=0;
    {
        MyErrorCode errorCode(countChecks, countDests);
        errorCode.set(U_PARSE_ERROR);
    }
    if(countDests!=1) {
        errln("MyErrorCode destructor failed to detect failure");
    }
}

class IcuTestErrorCodeTestHelper : public IntlTest {
  public:
    void errln( const UnicodeString &message ) U_OVERRIDE {
        test->assertFalse("Already saw an error", seenError);
        seenError = TRUE;
        test->assertEquals("Message for Error", expectedErrln, message);
        if (expectedDataErr) {
            test->errln("Got non-data error, but expected data error");
        }
    }

    void dataerrln( const UnicodeString &message ) U_OVERRIDE {
        test->assertFalse("Already saw an error", seenError);
        seenError = TRUE;
        test->assertEquals("Message for Error", expectedErrln, message);
        if (!expectedDataErr) {
            test->errln("Got data error, but expected non-data error");
        }
    }

    IntlTest* test;
    UBool expectedDataErr;
    UnicodeString expectedErrln;
    UBool seenError;
};

void ErrorCodeTest::TestIcuTestErrorCode() {
    IcuTestErrorCodeTestHelper helper;
    helper.test = this;

    // Test destructor message
    helper.expectedErrln = u"AAA destructor: expected success but got error: U_ILLEGAL_PAD_POSITION";
    helper.expectedDataErr = FALSE;
    helper.seenError = FALSE;
    {
        IcuTestErrorCode testStatus(helper, "AAA");
        testStatus.set(U_ILLEGAL_PAD_POSITION);
    }
    assertTrue("Should have seen an error", helper.seenError);

    // Test destructor message with scope
    helper.expectedErrln = u"BBB destructor: expected success but got error: U_ILLEGAL_PAD_POSITION scope: foo";
    helper.expectedDataErr = FALSE;
    helper.seenError = FALSE;
    {
        IcuTestErrorCode testStatus(helper, "BBB");
        testStatus.setScope("foo");
        testStatus.set(U_ILLEGAL_PAD_POSITION);
    }
    assertTrue("Should have seen an error", helper.seenError);

    // Check errIfFailure message with scope
    helper.expectedErrln = u"CCC expected success but got error: U_ILLEGAL_PAD_POSITION scope: foo";
    helper.expectedDataErr = FALSE;
    helper.seenError = FALSE;
    {
        IcuTestErrorCode testStatus(helper, "CCC");
        testStatus.setScope("foo");
        testStatus.set(U_ILLEGAL_PAD_POSITION);
        testStatus.errIfFailureAndReset();
        assertTrue("Should have seen an error", helper.seenError);
        helper.seenError = FALSE;
        helper.expectedErrln = u"CCC expected success but got error: U_ILLEGAL_CHAR_FOUND scope: foo - 5.4300";
        testStatus.set(U_ILLEGAL_CHAR_FOUND);
        testStatus.errIfFailureAndReset("%6.4f", 5.43);
        assertTrue("Should have seen an error", helper.seenError);
    }

    // Check errDataIfFailure message without scope
    helper.expectedErrln = u"DDD data: expected success but got error: U_ILLEGAL_PAD_POSITION";
    helper.expectedDataErr = TRUE;
    helper.seenError = FALSE;
    {
        IcuTestErrorCode testStatus(helper, "DDD");
        testStatus.set(U_ILLEGAL_PAD_POSITION);
        testStatus.errDataIfFailureAndReset();
        assertTrue("Should have seen an error", helper.seenError);
        helper.seenError = FALSE;
        helper.expectedErrln = u"DDD data: expected success but got error: U_ILLEGAL_CHAR_FOUND - 5.4300";
        testStatus.set(U_ILLEGAL_CHAR_FOUND);
        testStatus.errDataIfFailureAndReset("%6.4f", 5.43);
        assertTrue("Should have seen an error", helper.seenError);
    }

    // Check expectFailure
    helper.expectedErrln = u"EEE expected: U_ILLEGAL_CHAR_FOUND but got error: U_ILLEGAL_PAD_POSITION";
    helper.expectedDataErr = FALSE;
    helper.seenError = FALSE;
    {
        IcuTestErrorCode testStatus(helper, "EEE");
        testStatus.set(U_ILLEGAL_PAD_POSITION);
        testStatus.expectErrorAndReset(U_ILLEGAL_PAD_POSITION);
        assertFalse("Should NOT have seen an error", helper.seenError);
        testStatus.set(U_ILLEGAL_PAD_POSITION);
        testStatus.expectErrorAndReset(U_ILLEGAL_CHAR_FOUND);
        assertTrue("Should have seen an error", helper.seenError);
        helper.seenError = FALSE;
        helper.expectedErrln = u"EEE expected: U_ILLEGAL_CHAR_FOUND but got error: U_ZERO_ERROR scope: scopety scope - 5.4300";
        testStatus.setScope("scopety scope");
        testStatus.set(U_ILLEGAL_PAD_POSITION);
        testStatus.expectErrorAndReset(U_ILLEGAL_PAD_POSITION, "%6.4f", 5.43);
        assertFalse("Should NOT have seen an error", helper.seenError);
        testStatus.expectErrorAndReset(U_ILLEGAL_CHAR_FOUND, "%6.4f", 5.43);
        assertTrue("Should have seen an error", helper.seenError);
    }
}


class LocalPointerTest : public IntlTest {
public:
    LocalPointerTest() {}

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=NULL);

    void TestLocalPointer();
    void TestLocalPointerMoveSwap();
    void TestLocalPointerStdUniquePtr();
    void TestLocalArray();
    void TestLocalArrayMoveSwap();
    void TestLocalArrayStdUniquePtr();
    void TestLocalXyzPointer();
    void TestLocalXyzPointerMoveSwap();
    void TestLocalXyzPointerNull();
    void TestLocalXyzStdUniquePtr();
};

static IntlTest *createLocalPointerTest() {
    return new LocalPointerTest();
}

void LocalPointerTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) {
    if(exec) {
        logln("TestSuite LocalPointerTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestLocalPointer);
    TESTCASE_AUTO(TestLocalPointerMoveSwap);
    TESTCASE_AUTO(TestLocalPointerStdUniquePtr);
    TESTCASE_AUTO(TestLocalArray);
    TESTCASE_AUTO(TestLocalArrayMoveSwap);
    TESTCASE_AUTO(TestLocalArrayStdUniquePtr);
    TESTCASE_AUTO(TestLocalXyzPointer);
    TESTCASE_AUTO(TestLocalXyzPointerMoveSwap);
    TESTCASE_AUTO(TestLocalXyzPointerNull);
    TESTCASE_AUTO(TestLocalXyzStdUniquePtr);
    TESTCASE_AUTO_END;
}

// Exercise almost every LocalPointer and LocalPointerBase method.
void LocalPointerTest::TestLocalPointer() {
    // constructor
    LocalPointer<UnicodeString> s(new UnicodeString((UChar32)0x50005));
    // isNULL(), isValid(), operator==(), operator!=()
    if(s.isNull() || !s.isValid() || s==NULL || !(s!=NULL)) {
        errln("LocalPointer constructor or NULL test failure");
        return;
    }
    // getAlias(), operator->, operator*
    if(s.getAlias()->length()!=2 || s->length()!=2 || (*s).length()!=2) {
        errln("LocalPointer access failure");
    }
    // adoptInstead(), orphan()
    s.adoptInstead(new UnicodeString((UChar)0xfffc));
    if(s->length()!=1) {
        errln("LocalPointer adoptInstead(U+FFFC) failure");
    }
    UnicodeString *orphan=s.orphan();
    if(orphan==NULL || orphan->length()!=1 || s.isValid() || s!=NULL) {
        errln("LocalPointer orphan() failure");
    }
    delete orphan;
    s.adoptInstead(new UnicodeString());
    if(s->length()!=0) {
        errln("LocalPointer adoptInstead(empty) failure");
    }

    // LocalPointer(p, errorCode) sets U_MEMORY_ALLOCATION_ERROR if p==NULL.
    UErrorCode errorCode = U_ZERO_ERROR;
    LocalPointer<CharString> csx(new CharString("some chars", errorCode), errorCode);
    if(csx.isNull() && U_SUCCESS(errorCode)) {
        errln("LocalPointer(p, errorCode) failure");
        return;
    }
    errorCode = U_ZERO_ERROR;
    csx.adoptInsteadAndCheckErrorCode(new CharString("different chars", errorCode), errorCode);
    if(csx.isNull() && U_SUCCESS(errorCode)) {
        errln("adoptInsteadAndCheckErrorCode(p, errorCode) failure");
        return;
    }
    // Incoming failure: Keep the current object and delete the input object.
    errorCode = U_ILLEGAL_ARGUMENT_ERROR;
    csx.adoptInsteadAndCheckErrorCode(new CharString("unused", errorCode), errorCode);
    if(csx.isValid() && strcmp(csx->data(), "different chars") != 0) {
        errln("adoptInsteadAndCheckErrorCode(p, U_FAILURE) did not retain the old object");
        return;
    }
    errorCode = U_ZERO_ERROR;
    csx.adoptInsteadAndCheckErrorCode(NULL, errorCode);
    if(errorCode != U_MEMORY_ALLOCATION_ERROR) {
        errln("adoptInsteadAndCheckErrorCode(NULL, errorCode) did not set U_MEMORY_ALLOCATION_ERROR");
        return;
    }
    if(csx.isValid()) {
        errln("adoptInsteadAndCheckErrorCode(NULL, errorCode) kept the object");
        return;
    }
    errorCode = U_ZERO_ERROR;
    LocalPointer<CharString> null(NULL, errorCode);
    if(errorCode != U_MEMORY_ALLOCATION_ERROR) {
        errln("LocalPointer(NULL, errorCode) did not set U_MEMORY_ALLOCATION_ERROR");
        return;
    }

    // destructor
}

// Try to avoid clang -Wself-move warnings from s1 = std::move(s1);
template<typename T>
void moveFrom(T &dest, T &src) {
    dest = std::move(src);
}

void LocalPointerTest::TestLocalPointerMoveSwap() {
    UnicodeString *p1 = new UnicodeString((UChar)0x61);
    UnicodeString *p2 = new UnicodeString((UChar)0x62);
    LocalPointer<UnicodeString> s1(p1);
    LocalPointer<UnicodeString> s2(p2);
    s1.swap(s2);
    if(s1.getAlias() != p2 || s2.getAlias() != p1) {
        errln("LocalPointer.swap() did not swap");
    }
    swap(s1, s2);
    if(s1.getAlias() != p1 || s2.getAlias() != p2) {
        errln("swap(LocalPointer) did not swap back");
    }
    LocalPointer<UnicodeString> s3;
    s3 = std::move(s1);
    if(s3.getAlias() != p1 || s1.isValid()) {
        errln("LocalPointer = std::move() did not move");
    }
    infoln("TestLocalPointerMoveSwap() with rvalue references");
    s1 = static_cast<LocalPointer<UnicodeString> &&>(s3);
    if(s1.getAlias() != p1 || s3.isValid()) {
        errln("LocalPointer move assignment operator did not move");
    }
    LocalPointer<UnicodeString> s4(static_cast<LocalPointer<UnicodeString> &&>(s2));
    if(s4.getAlias() != p2 || s2.isValid()) {
        errln("LocalPointer move constructor did not move");
    }

    // Move self assignment leaves the object valid but in an undefined state.
    // Do it to make sure there is no crash,
    // but do not check for any particular resulting value.
    moveFrom(s1, s1);
    moveFrom(s3, s3);
}

void LocalPointerTest::TestLocalPointerStdUniquePtr() {
    auto* ptr = new UnicodeString((UChar32)0x50005);
    // Implicit conversion operator
    std::unique_ptr<UnicodeString> s = LocalPointer<UnicodeString>(ptr);
    // Explicit move constructor
    LocalPointer<UnicodeString> s2(std::move(s));
    // Conversion operator should also work with std::move
    s = std::move(s2);
    // Back again with move assignment
    s2 = std::move(s);
    assertTrue("Pointer should remain the same", ptr == s2.getAlias());
}

// Exercise almost every LocalArray method (but not LocalPointerBase).
void LocalPointerTest::TestLocalArray() {
    // constructor
    LocalArray<UnicodeString> a(new UnicodeString[2]);
    // operator[]()
    a[0].append((UChar)0x61);
    a[1].append((UChar32)0x60006);
    if(a[0].length()!=1 || a[1].length()!=2) {
        errln("LocalArray access failure");
    }
    // adoptInstead()
    a.adoptInstead(new UnicodeString[4]);
    a[3].append((UChar)0x62).append((UChar)0x63).reverse();
    if(a[3].length()!=2 || a[3][1]!=0x62) {
        errln("LocalArray adoptInstead() failure");
    }

    // LocalArray(p, errorCode) sets U_MEMORY_ALLOCATION_ERROR if p==NULL.
    UErrorCode errorCode = U_ZERO_ERROR;
    LocalArray<UnicodeString> ua(new UnicodeString[3], errorCode);
    if(ua.isNull() && U_SUCCESS(errorCode)) {
        errln("LocalArray(p, errorCode) failure");
        return;
    }
    errorCode = U_ZERO_ERROR;
    UnicodeString *u4 = new UnicodeString[4];
    ua.adoptInsteadAndCheckErrorCode(u4, errorCode);
    if(ua.isNull() && U_SUCCESS(errorCode)) {
        errln("adoptInsteadAndCheckErrorCode(p, errorCode) failure");
        return;
    }
    // Incoming failure: Keep the current object and delete the input object.
    errorCode = U_ILLEGAL_ARGUMENT_ERROR;
    ua.adoptInsteadAndCheckErrorCode(new UnicodeString[5], errorCode);
    if(ua.isValid() && ua.getAlias() != u4) {
        errln("adoptInsteadAndCheckErrorCode(p, U_FAILURE) did not retain the old array");
        return;
    }
    errorCode = U_ZERO_ERROR;
    ua.adoptInsteadAndCheckErrorCode(NULL, errorCode);
    if(errorCode != U_MEMORY_ALLOCATION_ERROR) {
        errln("adoptInsteadAndCheckErrorCode(NULL, errorCode) did not set U_MEMORY_ALLOCATION_ERROR");
        return;
    }
    if(ua.isValid()) {
        errln("adoptInsteadAndCheckErrorCode(NULL, errorCode) kept the array");
        return;
    }
    errorCode = U_ZERO_ERROR;
    LocalArray<UnicodeString> null(NULL, errorCode);
    if(errorCode != U_MEMORY_ALLOCATION_ERROR) {
        errln("LocalArray(NULL, errorCode) did not set U_MEMORY_ALLOCATION_ERROR");
        return;
    }

    // destructor
}

void LocalPointerTest::TestLocalArrayMoveSwap() {
    UnicodeString *p1 = new UnicodeString[2];
    UnicodeString *p2 = new UnicodeString[3];
    LocalArray<UnicodeString> a1(p1);
    LocalArray<UnicodeString> a2(p2);
    a1.swap(a2);
    if(a1.getAlias() != p2 || a2.getAlias() != p1) {
        errln("LocalArray.swap() did not swap");
    }
    swap(a1, a2);
    if(a1.getAlias() != p1 || a2.getAlias() != p2) {
        errln("swap(LocalArray) did not swap back");
    }
    LocalArray<UnicodeString> a3;
    a3 = std::move(a1);
    if(a3.getAlias() != p1 || a1.isValid()) {
        errln("LocalArray = std::move() did not move");
    }
    infoln("TestLocalArrayMoveSwap() with rvalue references");
    a1 = static_cast<LocalArray<UnicodeString> &&>(a3);
    if(a1.getAlias() != p1 || a3.isValid()) {
        errln("LocalArray move assignment operator did not move");
    }
    LocalArray<UnicodeString> a4(static_cast<LocalArray<UnicodeString> &&>(a2));
    if(a4.getAlias() != p2 || a2.isValid()) {
        errln("LocalArray move constructor did not move");
    }

    // Move self assignment leaves the object valid but in an undefined state.
    // Do it to make sure there is no crash,
    // but do not check for any particular resulting value.
    moveFrom(a1, a1);
    moveFrom(a3, a3);
}

void LocalPointerTest::TestLocalArrayStdUniquePtr() {
    auto* ptr = new UnicodeString[2];
    // Implicit conversion operator
    std::unique_ptr<UnicodeString[]> a = LocalArray<UnicodeString>(ptr);
    // Explicit move constructor
    LocalArray<UnicodeString> a2(std::move(a));
    // Conversion operator should also work with std::move
    a = std::move(a2);
    // Back again with move assignment
    a2 = std::move(a);
    assertTrue("Pointer should remain the same", ptr == a2.getAlias());
}

#include "unicode/ucnvsel.h"
#include "unicode/ucal.h"
#include "unicode/udatpg.h"
#include "unicode/uidna.h"
#include "unicode/uldnames.h"
#include "unicode/umsg.h"
#include "unicode/unorm2.h"
#include "unicode/uregex.h"
#include "unicode/utrans.h"
#include "unicode/uformattedvalue.h"

// Use LocalXyzPointer types that are not covered elsewhere in the intltest suite.
void LocalPointerTest::TestLocalXyzPointer() {
    IcuTestErrorCode errorCode(*this, "TestLocalXyzPointer");

    static const char *const encoding="ISO-8859-1";
    LocalUConverterSelectorPointer sel(
        ucnvsel_open(&encoding, 1, NULL, UCNV_ROUNDTRIP_SET, errorCode));
    if(errorCode.errIfFailureAndReset("ucnvsel_open()")) {
        return;
    }
    if(sel.isNull()) {
        errln("LocalUConverterSelectorPointer failure");
        return;
    }

#if !UCONFIG_NO_FORMATTING
    LocalUCalendarPointer cal(ucal_open(NULL, 0, "root", UCAL_GREGORIAN, errorCode));
    if(errorCode.errDataIfFailureAndReset("ucal_open()")) {
        return;
    }
    if(cal.isNull()) {
        errln("LocalUCalendarPointer failure");
        return;
    }

    LocalUDateTimePatternGeneratorPointer patgen(udatpg_open("root", errorCode));
    if(errorCode.errDataIfFailureAndReset("udatpg_open()")) {
        return;
    }
    if(patgen.isNull()) {
        errln("LocalUDateTimePatternGeneratorPointer failure");
        return;
    }

    LocalULocaleDisplayNamesPointer ldn(uldn_open("de-CH", ULDN_STANDARD_NAMES, errorCode));
    if(errorCode.errIfFailureAndReset("uldn_open()")) {
        return;
    }
    if(ldn.isNull()) {
        errln("LocalULocaleDisplayNamesPointer failure");
        return;
    }

    UnicodeString hello=UNICODE_STRING_SIMPLE("Hello {0}!");
    LocalUMessageFormatPointer msg(
        umsg_open(hello.getBuffer(), hello.length(), "root", NULL, errorCode));
    if(errorCode.errIfFailureAndReset("umsg_open()")) {
        return;
    }
    if(msg.isNull()) {
        errln("LocalUMessageFormatPointer failure");
        return;
    }
#endif  /* UCONFIG_NO_FORMATTING  */

#if !UCONFIG_NO_NORMALIZATION
    const UNormalizer2 *nfc=unorm2_getNFCInstance(errorCode);
    UnicodeSet emptySet;
    LocalUNormalizer2Pointer fn2(unorm2_openFiltered(nfc, emptySet.toUSet(), errorCode));
    if(errorCode.errIfFailureAndReset("unorm2_openFiltered()")) {
        return;
    }
    if(fn2.isNull()) {
        errln("LocalUNormalizer2Pointer failure");
        return;
    }
#endif /* !UCONFIG_NO_NORMALIZATION */

#if !UCONFIG_NO_IDNA
    LocalUIDNAPointer idna(uidna_openUTS46(0, errorCode));
    if(errorCode.errIfFailureAndReset("uidna_openUTS46()")) {
        return;
    }
    if(idna.isNull()) {
        errln("LocalUIDNAPointer failure");
        return;
    }
#endif  /* !UCONFIG_NO_IDNA */

#if !UCONFIG_NO_REGULAR_EXPRESSIONS
    UnicodeString pattern=UNICODE_STRING_SIMPLE("abc|xy+z");
    LocalURegularExpressionPointer regex(
        uregex_open(pattern.getBuffer(), pattern.length(), 0, NULL, errorCode));
    if(errorCode.errIfFailureAndReset("uregex_open()")) {
        return;
    }
    if(regex.isNull()) {
        errln("LocalURegularExpressionPointer failure");
        return;
    }
#endif /* UCONFIG_NO_REGULAR_EXPRESSIONS */

#if !UCONFIG_NO_TRANSLITERATION
    UnicodeString id=UNICODE_STRING_SIMPLE("Grek-Latn");
    LocalUTransliteratorPointer trans(
        utrans_openU(id.getBuffer(), id.length(), UTRANS_FORWARD, NULL, 0, NULL, errorCode));
    if(errorCode.errIfFailureAndReset("utrans_open()")) {
        return;
    }
    if(trans.isNull()) {
        errln("LocalUTransliteratorPointer failure");
        return;
    }
#endif /* !UCONFIG_NO_TRANSLITERATION */

    // destructors
}

void LocalPointerTest::TestLocalXyzPointerMoveSwap() {
#if !UCONFIG_NO_NORMALIZATION
    IcuTestErrorCode errorCode(*this, "TestLocalXyzPointerMoveSwap");
    const UNormalizer2 *nfc=unorm2_getNFCInstance(errorCode);
    const UNormalizer2 *nfd=unorm2_getNFDInstance(errorCode);
    if(errorCode.errIfFailureAndReset("unorm2_getNF[CD]Instance()")) {
        return;
    }
    UnicodeSet emptySet;
    UNormalizer2 *p1 = unorm2_openFiltered(nfc, emptySet.toUSet(), errorCode);
    UNormalizer2 *p2 = unorm2_openFiltered(nfd, emptySet.toUSet(), errorCode);
    LocalUNormalizer2Pointer f1(p1);
    LocalUNormalizer2Pointer f2(p2);
    if(errorCode.errIfFailureAndReset("unorm2_openFiltered()")) {
        return;
    }
    if(f1.isNull() || f2.isNull()) {
        errln("LocalUNormalizer2Pointer failure");
        return;
    }
    f1.swap(f2);
    if(f1.getAlias() != p2 || f2.getAlias() != p1) {
        errln("LocalUNormalizer2Pointer.swap() did not swap");
    }
    swap(f1, f2);
    if(f1.getAlias() != p1 || f2.getAlias() != p2) {
        errln("swap(LocalUNormalizer2Pointer) did not swap back");
    }
    LocalUNormalizer2Pointer f3;
    f3 = std::move(f1);
    if(f3.getAlias() != p1 || f1.isValid()) {
        errln("LocalUNormalizer2Pointer = std::move() did not move");
    }
    infoln("TestLocalXyzPointerMoveSwap() with rvalue references");
    f1 = static_cast<LocalUNormalizer2Pointer &&>(f3);
    if(f1.getAlias() != p1 || f3.isValid()) {
        errln("LocalUNormalizer2Pointer move assignment operator did not move");
    }
    LocalUNormalizer2Pointer f4(static_cast<LocalUNormalizer2Pointer &&>(f2));
    if(f4.getAlias() != p2 || f2.isValid()) {
        errln("LocalUNormalizer2Pointer move constructor did not move");
    }
    // Move self assignment leaves the object valid but in an undefined state.
    // Do it to make sure there is no crash,
    // but do not check for any particular resulting value.
    moveFrom(f1, f1);
    moveFrom(f3, f3);
#endif /* !UCONFIG_NO_NORMALIZATION */
}

// Try LocalXyzPointer types with NULL pointers.
void LocalPointerTest::TestLocalXyzPointerNull() {
    {
        IcuTestErrorCode errorCode(*this, "TestLocalXyzPointerNull/LocalUConverterSelectorPointer");
        static const char *const encoding="ISO-8859-1";
        LocalUConverterSelectorPointer null;
        LocalUConverterSelectorPointer sel(
            ucnvsel_open(&encoding, 1, NULL, UCNV_ROUNDTRIP_SET, errorCode));
        sel.adoptInstead(NULL);
    }
#if !UCONFIG_NO_FORMATTING
    {
        IcuTestErrorCode errorCode(*this, "TestLocalXyzPointerNull/LocalUCalendarPointer");
        LocalUCalendarPointer null;
        LocalUCalendarPointer cal(ucal_open(NULL, 0, "root", UCAL_GREGORIAN, errorCode));
        if(!errorCode.errDataIfFailureAndReset("ucal_open()")) {
            cal.adoptInstead(NULL);
        }
    }
    {
        IcuTestErrorCode errorCode(*this, "TestLocalXyzPointerNull/LocalUDateTimePatternGeneratorPointer");
        LocalUDateTimePatternGeneratorPointer null;
        LocalUDateTimePatternGeneratorPointer patgen(udatpg_open("root", errorCode));
        patgen.adoptInstead(NULL);
    }
    {
        IcuTestErrorCode errorCode(*this, "TestLocalXyzPointerNull/LocalUMessageFormatPointer");
        UnicodeString hello=UNICODE_STRING_SIMPLE("Hello {0}!");
        LocalUMessageFormatPointer null;
        LocalUMessageFormatPointer msg(
            umsg_open(hello.getBuffer(), hello.length(), "root", NULL, errorCode));
        msg.adoptInstead(NULL);
    }
#endif /* !UCONFIG_NO_FORMATTING */

#if !UCONFIG_NO_REGULAR_EXPRESSIONS
    {
        IcuTestErrorCode errorCode(*this, "TestLocalXyzPointerNull/LocalURegularExpressionPointer");
        UnicodeString pattern=UNICODE_STRING_SIMPLE("abc|xy+z");
        LocalURegularExpressionPointer null;
        LocalURegularExpressionPointer regex(
            uregex_open(pattern.getBuffer(), pattern.length(), 0, NULL, errorCode));
        if(!errorCode.errDataIfFailureAndReset("urege_open()")) {
            regex.adoptInstead(NULL);
        }
    }
#endif /* !UCONFIG_NO_REGULAR_EXPRESSIONS */

#if !UCONFIG_NO_TRANSLITERATION
    {
        IcuTestErrorCode errorCode(*this, "TestLocalXyzPointerNull/LocalUTransliteratorPointer");
        UnicodeString id=UNICODE_STRING_SIMPLE("Grek-Latn");
        LocalUTransliteratorPointer null;
        LocalUTransliteratorPointer trans(
            utrans_openU(id.getBuffer(), id.length(), UTRANS_FORWARD, NULL, 0, NULL, errorCode));
        if(!errorCode.errDataIfFailureAndReset("utrans_openU()")) {
            trans.adoptInstead(NULL);
        }
    }
#endif /* !UCONFIG_NO_TRANSLITERATION */

}

void LocalPointerTest::TestLocalXyzStdUniquePtr() {
    IcuTestErrorCode status(*this, "TestLocalXyzStdUniquePtr");
#if !UCONFIG_NO_FORMATTING
    auto* ptr = ucfpos_open(status);
    // Implicit conversion operator
    std::unique_ptr<UConstrainedFieldPosition, void(*)(UConstrainedFieldPosition*)> a =
        LocalUConstrainedFieldPositionPointer(ptr);
    // Explicit move constructor
    LocalUConstrainedFieldPositionPointer a2(std::move(a));
    // Conversion operator should also work with std::move
    a = std::move(a2);
    // Back again with move assignment
    a2 = std::move(a);
    assertTrue("Pointer should remain the same", ptr == a2.getAlias());
#endif // UCONFIG_NO_FORMATTING
}

/** EnumSet test **/
#include "unicode/enumset.h"

class EnumSetTest : public IntlTest {
public:
  EnumSetTest() {}
  virtual void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=NULL);
  void TestEnumSet();
};

static IntlTest *createEnumSetTest() {
    return new EnumSetTest();
}

void EnumSetTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) {
  TESTCASE_AUTO_BEGIN;
  TESTCASE_AUTO(TestEnumSet);
  TESTCASE_AUTO_END;
}
enum myEnum {
    MAX_NONBOOLEAN=-1,
    THING1,
    THING2,
    THING3,
    LIMIT_BOOLEAN
};

void EnumSetTest::TestEnumSet() {
    EnumSet<myEnum,
            MAX_NONBOOLEAN+1,
            LIMIT_BOOLEAN>
                            flags;

    logln("Enum is from [%d..%d]\n", MAX_NONBOOLEAN+1,
          LIMIT_BOOLEAN);

    assertFalse(WHERE, flags.get(THING1));
    assertFalse(WHERE, flags.get(THING2));
    assertFalse(WHERE, flags.get(THING3));

    logln("get(thing1)=%d, get(thing2)=%d, get(thing3)=%d\n",          flags.get(THING1),          flags.get(THING2),          flags.get(THING3));
    logln("Value now: %d\n", flags.getAll());
    flags.clear();
    logln("clear -Value now: %d\n", flags.getAll());
    logln("get(thing1)=%d, get(thing2)=%d, get(thing3)=%d\n",          flags.get(THING1),          flags.get(THING2),          flags.get(THING3));
    assertFalse(WHERE, flags.get(THING1));
    assertFalse(WHERE, flags.get(THING2));
    assertFalse(WHERE, flags.get(THING3));
    flags.add(THING1);
    logln("set THING1 -Value now: %d\n", flags.getAll());
    assertTrue(WHERE, flags.get(THING1));
    assertFalse(WHERE, flags.get(THING2));
    assertFalse(WHERE, flags.get(THING3));
    logln("get(thing1)=%d, get(thing2)=%d, get(thing3)=%d\n",          flags.get(THING1),          flags.get(THING2),          flags.get(THING3));
    flags.add(THING3);
    logln("set THING3 -Value now: %d\n", flags.getAll());
    assertTrue(WHERE, flags.get(THING1));
    assertFalse(WHERE, flags.get(THING2));
    assertTrue(WHERE, flags.get(THING3));
    logln("get(thing1)=%d, get(thing2)=%d, get(thing3)=%d\n",          flags.get(THING1),          flags.get(THING2),          flags.get(THING3));
    flags.remove(THING2);
    assertTrue(WHERE, flags.get(THING1));
    assertFalse(WHERE, flags.get(THING2));
    assertTrue(WHERE, flags.get(THING3));
    logln("remove THING2 -Value now: %d\n", flags.getAll());
    logln("get(thing1)=%d, get(thing2)=%d, get(thing3)=%d\n",          flags.get(THING1),          flags.get(THING2),          flags.get(THING3));
    flags.remove(THING1);
    assertFalse(WHERE, flags.get(THING1));
    assertFalse(WHERE, flags.get(THING2));
    assertTrue(WHERE, flags.get(THING3));
    logln("remove THING1 -Value now: %d\n", flags.getAll());
    logln("get(thing1)=%d, get(thing2)=%d, get(thing3)=%d\n",          flags.get(THING1),          flags.get(THING2),          flags.get(THING3));

    flags.clear();
    logln("clear -Value now: %d\n", flags.getAll());
    logln("get(thing1)=%d, get(thing2)=%d, get(thing3)=%d\n",          flags.get(THING1),          flags.get(THING2),          flags.get(THING3));
    assertFalse(WHERE, flags.get(THING1));
    assertFalse(WHERE, flags.get(THING2));
    assertFalse(WHERE, flags.get(THING3));
}
