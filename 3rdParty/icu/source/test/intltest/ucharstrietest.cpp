/*
*******************************************************************************
*   Copyright (C) 2010-2011, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  ucharstrietest.cpp
*   encoding:   US-ASCII
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010nov16
*   created by: Markus W. Scherer
*/

#include <string.h>

#include "unicode/utypes.h"
#include "unicode/appendable.h"
#include "unicode/localpointer.h"
#include "unicode/ucharstrie.h"
#include "unicode/ucharstriebuilder.h"
#include "unicode/uniset.h"
#include "unicode/unistr.h"
#include "intltest.h"

#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))

struct StringAndValue {
    const char *s;
    int32_t value;
};

class UCharsTrieTest : public IntlTest {
public:
    UCharsTrieTest();
    virtual ~UCharsTrieTest();

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=NULL);
    void TestBuilder();
    void TestEmpty();
    void Test_a();
    void Test_a_ab();
    void TestShortestBranch();
    void TestBranches();
    void TestLongSequence();
    void TestLongBranch();
    void TestValuesForState();
    void TestCompact();
    void TestFirstForCodePoint();
    void TestNextForCodePoint();

    UCharsTrie *buildLargeTrie(int32_t numUniqueFirst);
    void TestLargeTrie();

    UCharsTrie *buildMonthsTrie(UStringTrieBuildOption buildOption);
    void TestHasUniqueValue();
    void TestGetNextUChars();
    void TestIteratorFromBranch();
    void TestIteratorFromLinearMatch();
    void TestTruncatingIteratorFromRoot();
    void TestTruncatingIteratorFromLinearMatchShort();
    void TestTruncatingIteratorFromLinearMatchLong();
    void TestIteratorFromUChars();

    void checkData(const StringAndValue data[], int32_t dataLength);
    void checkData(const StringAndValue data[], int32_t dataLength, UStringTrieBuildOption buildOption);
    UCharsTrie *buildTrie(const StringAndValue data[], int32_t dataLength,
                          UStringTrieBuildOption buildOption);
    void checkFirst(UCharsTrie &trie, const StringAndValue data[], int32_t dataLength);
    void checkNext(UCharsTrie &trie, const StringAndValue data[], int32_t dataLength);
    void checkNextWithState(UCharsTrie &trie, const StringAndValue data[], int32_t dataLength);
    void checkNextString(UCharsTrie &trie, const StringAndValue data[], int32_t dataLength);
    void checkIterator(UCharsTrie &trie, const StringAndValue data[], int32_t dataLength);
    void checkIterator(UCharsTrie::Iterator &iter, const StringAndValue data[], int32_t dataLength);

private:
    UCharsTrieBuilder *builder_;
};

extern IntlTest *createUCharsTrieTest() {
    return new UCharsTrieTest();
}

UCharsTrieTest::UCharsTrieTest() : builder_(NULL) {
    IcuTestErrorCode errorCode(*this, "UCharsTrieTest()");
    builder_=new UCharsTrieBuilder(errorCode);
}

UCharsTrieTest::~UCharsTrieTest() {
    delete builder_;
}

void UCharsTrieTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) {
    if(exec) {
        logln("TestSuite UCharsTrieTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestBuilder);
    TESTCASE_AUTO(TestEmpty);
    TESTCASE_AUTO(Test_a);
    TESTCASE_AUTO(Test_a_ab);
    TESTCASE_AUTO(TestShortestBranch);
    TESTCASE_AUTO(TestBranches);
    TESTCASE_AUTO(TestLongSequence);
    TESTCASE_AUTO(TestLongBranch);
    TESTCASE_AUTO(TestValuesForState);
    TESTCASE_AUTO(TestCompact);
    TESTCASE_AUTO(TestFirstForCodePoint);
    TESTCASE_AUTO(TestNextForCodePoint);
    TESTCASE_AUTO(TestLargeTrie);
    TESTCASE_AUTO(TestHasUniqueValue);
    TESTCASE_AUTO(TestGetNextUChars);
    TESTCASE_AUTO(TestIteratorFromBranch);
    TESTCASE_AUTO(TestIteratorFromLinearMatch);
    TESTCASE_AUTO(TestTruncatingIteratorFromRoot);
    TESTCASE_AUTO(TestTruncatingIteratorFromLinearMatchShort);
    TESTCASE_AUTO(TestTruncatingIteratorFromLinearMatchLong);
    TESTCASE_AUTO(TestIteratorFromUChars);
    TESTCASE_AUTO_END;
}

void UCharsTrieTest::TestBuilder() {
    IcuTestErrorCode errorCode(*this, "TestBuilder()");
    delete builder_->build(USTRINGTRIE_BUILD_FAST, errorCode);
    if(errorCode.reset()!=U_INDEX_OUTOFBOUNDS_ERROR) {
        errln("UCharsTrieBuilder().build() did not set U_INDEX_OUTOFBOUNDS_ERROR");
        return;
    }
    // TODO: remove .build(...) once add() checks for duplicates.
    builder_->add("=", 0, errorCode).add("=", 1, errorCode).build(USTRINGTRIE_BUILD_FAST, errorCode);
    if(errorCode.reset()!=U_ILLEGAL_ARGUMENT_ERROR) {
        errln("UCharsTrieBuilder.add() did not detect duplicates");
        return;
    }
}

void UCharsTrieTest::TestEmpty() {
    static const StringAndValue data[]={
        { "", 0 }
    };
    checkData(data, LENGTHOF(data));
}

void UCharsTrieTest::Test_a() {
    static const StringAndValue data[]={
        { "a", 1 }
    };
    checkData(data, LENGTHOF(data));
}

void UCharsTrieTest::Test_a_ab() {
    static const StringAndValue data[]={
        { "a", 1 },
        { "ab", 100 }
    };
    checkData(data, LENGTHOF(data));
}

void UCharsTrieTest::TestShortestBranch() {
    static const StringAndValue data[]={
        { "a", 1000 },
        { "b", 2000 }
    };
    checkData(data, LENGTHOF(data));
}

void UCharsTrieTest::TestBranches() {
    static const StringAndValue data[]={
        { "a", 0x10 },
        { "cc", 0x40 },
        { "e", 0x100 },
        { "ggg", 0x400 },
        { "i", 0x1000 },
        { "kkkk", 0x4000 },
        { "n", 0x10000 },
        { "ppppp", 0x40000 },
        { "r", 0x100000 },
        { "sss", 0x200000 },
        { "t", 0x400000 },
        { "uu", 0x800000 },
        { "vv", 0x7fffffff },
        { "zz", 0x80000000 }
    };
    for(int32_t length=2; length<=LENGTHOF(data); ++length) {
        infoln("TestBranches length=%d", (int)length);
        checkData(data, length);
    }
}

void UCharsTrieTest::TestLongSequence() {
    static const StringAndValue data[]={
        { "a", -1 },
        // sequence of linear-match nodes
        { "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", -2 },
        // more than 256 units
        { "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", -3 }
    };
    checkData(data, LENGTHOF(data));
}

void UCharsTrieTest::TestLongBranch() {
    // Split-branch and interesting compact-integer values.
    static const StringAndValue data[]={
        { "a", -2 },
        { "b", -1 },
        { "c", 0 },
        { "d2", 1 },
        { "f", 0x3f },
        { "g", 0x40 },
        { "h", 0x41 },
        { "j23", 0x1900 },
        { "j24", 0x19ff },
        { "j25", 0x1a00 },
        { "k2", 0x1a80 },
        { "k3", 0x1aff },
        { "l234567890", 0x1b00 },
        { "l234567890123", 0x1b01 },
        { "nnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnnn", 0x10ffff },
        { "oooooooooooooooooooooooooooooooooooooooooooooooooooooo", 0x110000 },
        { "pppppppppppppppppppppppppppppppppppppppppppppppppppppp", 0x120000 },
        { "r", 0x333333 },
        { "s2345", 0x4444444 },
        { "t234567890", 0x77777777 },
        { "z", 0x80000001 }
    };
    checkData(data, LENGTHOF(data));
}

void UCharsTrieTest::TestValuesForState() {
    // Check that saveState() and resetToState() interact properly
    // with next() and current().
    static const StringAndValue data[]={
        { "a", -1 },
        { "ab", -2 },
        { "abc", -3 },
        { "abcd", -4 },
        { "abcde", -5 },
        { "abcdef", -6 }
    };
    checkData(data, LENGTHOF(data));
}

void UCharsTrieTest::TestCompact() {
    // Duplicate trailing strings and values provide opportunities for compacting.
    static const StringAndValue data[]={
        { "+", 0 },
        { "+august", 8 },
        { "+december", 12 },
        { "+july", 7 },
        { "+june", 6 },
        { "+november", 11 },
        { "+october", 10 },
        { "+september", 9 },
        { "-", 0 },
        { "-august", 8 },
        { "-december", 12 },
        { "-july", 7 },
        { "-june", 6 },
        { "-november", 11 },
        { "-october", 10 },
        { "-september", 9 },
        // The l+n branch (with its sub-nodes) is a duplicate but will be written
        // both times because each time it follows a different linear-match node.
        { "xjuly", 7 },
        { "xjune", 6 }
    };
    checkData(data, LENGTHOF(data));
}

void UCharsTrieTest::TestFirstForCodePoint() {
    static const StringAndValue data[]={
        { "a", 1 },
        { "a\\ud800", 2 },
        { "a\\U00010000", 3 },
        { "\\ud840", 4 },
        { "\\U00020000\\udbff", 5 },
        { "\\U00020000\\U0010ffff", 6 },
        { "\\U00020000\\U0010ffffz", 7 },
        { "\\U00050000xy", 8 },
        { "\\U00050000xyz", 9 }
    };
    checkData(data, LENGTHOF(data));
}

void UCharsTrieTest::TestNextForCodePoint() {
    static const StringAndValue data[]={
        { "\\u4dff\\U00010000\\u9999\\U00020000\\udfff\\U0010ffff", 2000000000 },
        { "\\u4dff\\U00010000\\u9999\\U00020002", 44444 },
        { "\\u4dff\\U000103ff", 99999 }
    };
    LocalPointer<UCharsTrie> trie(buildTrie(data, LENGTHOF(data), USTRINGTRIE_BUILD_FAST));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    UStringTrieResult result;
    if( (result=trie->nextForCodePoint(0x4dff))!=USTRINGTRIE_NO_VALUE || result!=trie->current() ||
        (result=trie->nextForCodePoint(0x10000))!=USTRINGTRIE_NO_VALUE || result!=trie->current() ||
        (result=trie->nextForCodePoint(0x9999))!=USTRINGTRIE_NO_VALUE || result!=trie->current() ||
        (result=trie->nextForCodePoint(0x20000))!=USTRINGTRIE_NO_VALUE || result!=trie->current() ||
        (result=trie->nextForCodePoint(0xdfff))!=USTRINGTRIE_NO_VALUE || result!=trie->current() ||
        (result=trie->nextForCodePoint(0x10ffff))!=USTRINGTRIE_FINAL_VALUE || result!=trie->current() ||
        trie->getValue()!=2000000000
    ) {
        errln("UCharsTrie.nextForCodePoint() fails for %s", data[0].s);
    }
    if( (result=trie->firstForCodePoint(0x4dff))!=USTRINGTRIE_NO_VALUE || result!=trie->current() ||
        (result=trie->nextForCodePoint(0x10000))!=USTRINGTRIE_NO_VALUE || result!=trie->current() ||
        (result=trie->nextForCodePoint(0x9999))!=USTRINGTRIE_NO_VALUE || result!=trie->current() ||
        (result=trie->nextForCodePoint(0x20002))!=USTRINGTRIE_FINAL_VALUE || result!=trie->current() ||
        trie->getValue()!=44444
    ) {
        errln("UCharsTrie.nextForCodePoint() fails for %s", data[1].s);
    }
    if( (result=trie->reset().nextForCodePoint(0x4dff))!=USTRINGTRIE_NO_VALUE || result!=trie->current() ||
        (result=trie->nextForCodePoint(0x10000))!=USTRINGTRIE_NO_VALUE || result!=trie->current() ||
        (result=trie->nextForCodePoint(0x9999))!=USTRINGTRIE_NO_VALUE || result!=trie->current() ||
        (result=trie->nextForCodePoint(0x20222))!=USTRINGTRIE_NO_MATCH || result!=trie->current()  // no match for trail surrogate
    ) {
        errln("UCharsTrie.nextForCodePoint() fails for \\u4dff\\U00010000\\u9999\\U00020222");
    }
    if( (result=trie->reset().nextForCodePoint(0x4dff))!=USTRINGTRIE_NO_VALUE || result!=trie->current() ||
        (result=trie->nextForCodePoint(0x103ff))!=USTRINGTRIE_FINAL_VALUE || result!=trie->current() ||
        trie->getValue()!=99999
    ) {
        errln("UCharsTrie.nextForCodePoint() fails for %s", data[2].s);
    }
}

// Definitions in the anonymous namespace are invisible outside this file.
namespace {

// Generate (string, value) pairs.
// The first string (before next()) will be empty.
class Generator {
public:
    Generator() : value(4711), num(0) {}
    void next() {
        UChar c;
        s.truncate(0);
        s.append(c=(UChar)(value>>16));
        s.append((UChar)(value>>4));
        if(value&1) {
            s.append((UChar)value);
        }
        set.add(c);
        value+=((value>>5)&0x7ff)*3+1;
        ++num;
    }
    const UnicodeString &getString() const { return s; }
    int32_t getValue() const { return value; }
    int32_t countUniqueFirstChars() const { return set.size(); }
    int32_t getIndex() const { return num; }

private:
    UnicodeString s;
    UnicodeSet set;
    int32_t value;
    int32_t num;
};

}  // end namespace

UCharsTrie *UCharsTrieTest::buildLargeTrie(int32_t numUniqueFirst) {
    IcuTestErrorCode errorCode(*this, "buildLargeTrie()");
    Generator gen;
    builder_->clear();
    while(gen.countUniqueFirstChars()<numUniqueFirst) {
        builder_->add(gen.getString(), gen.getValue(), errorCode);
        gen.next();
    }
    infoln("buildLargeTrie(%ld) added %ld strings", (long)numUniqueFirst, (long)gen.getIndex());
    UnicodeString trieUChars;
    builder_->buildUnicodeString(USTRINGTRIE_BUILD_FAST, trieUChars, errorCode);
    logln("serialized trie size: %ld UChars\n", (long)trieUChars.length());
    return new UCharsTrie(trieUChars.getBuffer());
}

// Exercise a large branch node.
void UCharsTrieTest::TestLargeTrie() {
    LocalPointer<UCharsTrie> trie(buildLargeTrie(1111));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    Generator gen;
    while(gen.countUniqueFirstChars()<1111) {
        UnicodeString x(gen.getString());
        int32_t value=gen.getValue();
        if(!x.isEmpty()) {
            if(trie->first(x[0])==USTRINGTRIE_NO_MATCH) {
                errln("first(first char U+%04X)=USTRINGTRIE_NO_MATCH for string %ld\n",
                      x[0], (long)gen.getIndex());
                break;
            }
            x.remove(0, 1);
        }
        UStringTrieResult result=trie->next(x.getBuffer(), x.length());
        if(!USTRINGTRIE_HAS_VALUE(result) || result!=trie->current() || value!=trie->getValue()) {
            errln("next(%d chars U+%04X U+%04X)!=hasValue or "
                  "next()!=current() or getValue() wrong "
                  "for string %ld\n", (int)x.length(), x[0], x[1], (long)gen.getIndex());
            break;
        }
        gen.next();
    }
}

enum {
    u_a=0x61,
    u_b=0x62,
    u_c=0x63,
    u_j=0x6a,
    u_n=0x6e,
    u_r=0x72,
    u_u=0x75,
    u_y=0x79
};

UCharsTrie *UCharsTrieTest::buildMonthsTrie(UStringTrieBuildOption buildOption) {
    // All types of nodes leading to the same value,
    // for code coverage of recursive functions.
    // In particular, we need a lot of branches on some single level
    // to exercise a split-branch node.
    static const StringAndValue data[]={
        { "august", 8 },
        { "jan", 1 },
        { "jan.", 1 },
        { "jana", 1 },
        { "janbb", 1 },
        { "janc", 1 },
        { "janddd", 1 },
        { "janee", 1 },
        { "janef", 1 },
        { "janf", 1 },
        { "jangg", 1 },
        { "janh", 1 },
        { "janiiii", 1 },
        { "janj", 1 },
        { "jankk", 1 },
        { "jankl", 1 },
        { "jankmm", 1 },
        { "janl", 1 },
        { "janm", 1 },
        { "jannnnnnnnnnnnnnnnnnnnnnnnnnnnn", 1 },
        { "jano", 1 },
        { "janpp", 1 },
        { "janqqq", 1 },
        { "janr", 1 },
        { "januar", 1 },
        { "january", 1 },
        { "july", 7 },
        { "jun", 6 },
        { "jun.", 6 },
        { "june", 6 }
    };
    return buildTrie(data, LENGTHOF(data), buildOption);
}

void UCharsTrieTest::TestHasUniqueValue() {
    LocalPointer<UCharsTrie> trie(buildMonthsTrie(USTRINGTRIE_BUILD_FAST));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    int32_t uniqueValue;
    if(trie->hasUniqueValue(uniqueValue)) {
        errln("unique value at root");
    }
    trie->next(u_j);
    trie->next(u_a);
    trie->next(u_n);
    // hasUniqueValue() directly after next()
    if(!trie->hasUniqueValue(uniqueValue) || uniqueValue!=1) {
        errln("not unique value 1 after \"jan\"");
    }
    trie->first(u_j);
    trie->next(u_u);
    if(trie->hasUniqueValue(uniqueValue)) {
        errln("unique value after \"ju\"");
    }
    if(trie->next(u_n)!=USTRINGTRIE_INTERMEDIATE_VALUE || 6!=trie->getValue()) {
        errln("not normal value 6 after \"jun\"");
    }
    // hasUniqueValue() after getValue()
    if(!trie->hasUniqueValue(uniqueValue) || uniqueValue!=6) {
        errln("not unique value 6 after \"jun\"");
    }
    // hasUniqueValue() from within a linear-match node
    trie->first(u_a);
    trie->next(u_u);
    if(!trie->hasUniqueValue(uniqueValue) || uniqueValue!=8) {
        errln("not unique value 8 after \"au\"");
    }
}

void UCharsTrieTest::TestGetNextUChars() {
    LocalPointer<UCharsTrie> trie(buildMonthsTrie(USTRINGTRIE_BUILD_SMALL));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    UnicodeString buffer;
    UnicodeStringAppendable app(buffer);
    int32_t count=trie->getNextUChars(app);
    if(count!=2 || buffer.length()!=2 || buffer[0]!=u_a || buffer[1]!=u_j) {
        errln("months getNextUChars()!=[aj] at root");
    }
    trie->next(u_j);
    trie->next(u_a);
    trie->next(u_n);
    // getNextUChars() directly after next()
    buffer.remove();
    count=trie->getNextUChars(app);
    if(count!=20 || buffer!=UNICODE_STRING_SIMPLE(".abcdefghijklmnopqru")) {
        errln("months getNextUChars()!=[.abcdefghijklmnopqru] after \"jan\"");
    }
    // getNextUChars() after getValue()
    trie->getValue();  // next() had returned USTRINGTRIE_INTERMEDIATE_VALUE.
    buffer.remove();
    count=trie->getNextUChars(app);
    if(count!=20 || buffer!=UNICODE_STRING_SIMPLE(".abcdefghijklmnopqru")) {
        errln("months getNextUChars()!=[.abcdefghijklmnopqru] after \"jan\"+getValue()");
    }
    // getNextUChars() from a linear-match node
    trie->next(u_u);
    buffer.remove();
    count=trie->getNextUChars(app);
    if(count!=1 || buffer.length()!=1 || buffer[0]!=u_a) {
        errln("months getNextUChars()!=[a] after \"janu\"");
    }
    trie->next(u_a);
    buffer.remove();
    count=trie->getNextUChars(app);
    if(count!=1 || buffer.length()!=1 || buffer[0]!=u_r) {
        errln("months getNextUChars()!=[r] after \"janua\"");
    }
    trie->next(u_r);
    trie->next(u_y);
    // getNextUChars() after a final match
    buffer.remove();
    count=trie->getNextUChars(app);
    if(count!=0 || buffer.length()!=0) {
        errln("months getNextUChars()!=[] after \"january\"");
    }
}

void UCharsTrieTest::TestIteratorFromBranch() {
    LocalPointer<UCharsTrie> trie(buildMonthsTrie(USTRINGTRIE_BUILD_FAST));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    // Go to a branch node.
    trie->next(u_j);
    trie->next(u_a);
    trie->next(u_n);
    IcuTestErrorCode errorCode(*this, "TestIteratorFromBranch()");
    UCharsTrie::Iterator iter(*trie, 0, errorCode);
    if(errorCode.logIfFailureAndReset("UCharsTrie::Iterator(trie) constructor")) {
        return;
    }
    // Expected data: Same as in buildMonthsTrie(), except only the suffixes
    // following "jan".
    static const StringAndValue data[]={
        { "", 1 },
        { ".", 1 },
        { "a", 1 },
        { "bb", 1 },
        { "c", 1 },
        { "ddd", 1 },
        { "ee", 1 },
        { "ef", 1 },
        { "f", 1 },
        { "gg", 1 },
        { "h", 1 },
        { "iiii", 1 },
        { "j", 1 },
        { "kk", 1 },
        { "kl", 1 },
        { "kmm", 1 },
        { "l", 1 },
        { "m", 1 },
        { "nnnnnnnnnnnnnnnnnnnnnnnnnnnn", 1 },
        { "o", 1 },
        { "pp", 1 },
        { "qqq", 1 },
        { "r", 1 },
        { "uar", 1 },
        { "uary", 1 }
    };
    checkIterator(iter, data, LENGTHOF(data));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), data, LENGTHOF(data));
}

void UCharsTrieTest::TestIteratorFromLinearMatch() {
    LocalPointer<UCharsTrie> trie(buildMonthsTrie(USTRINGTRIE_BUILD_SMALL));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    // Go into a linear-match node.
    trie->next(u_j);
    trie->next(u_a);
    trie->next(u_n);
    trie->next(u_u);
    trie->next(u_a);
    IcuTestErrorCode errorCode(*this, "TestIteratorFromLinearMatch()");
    UCharsTrie::Iterator iter(*trie, 0, errorCode);
    if(errorCode.logIfFailureAndReset("UCharsTrie::Iterator(trie) constructor")) {
        return;
    }
    // Expected data: Same as in buildMonthsTrie(), except only the suffixes
    // following "janua".
    static const StringAndValue data[]={
        { "r", 1 },
        { "ry", 1 }
    };
    checkIterator(iter, data, LENGTHOF(data));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), data, LENGTHOF(data));
}

void UCharsTrieTest::TestTruncatingIteratorFromRoot() {
    LocalPointer<UCharsTrie> trie(buildMonthsTrie(USTRINGTRIE_BUILD_FAST));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    IcuTestErrorCode errorCode(*this, "TestTruncatingIteratorFromRoot()");
    UCharsTrie::Iterator iter(*trie, 4, errorCode);
    if(errorCode.logIfFailureAndReset("UCharsTrie::Iterator(trie) constructor")) {
        return;
    }
    // Expected data: Same as in buildMonthsTrie(), except only the first 4 characters
    // of each string, and no string duplicates from the truncation.
    static const StringAndValue data[]={
        { "augu", -1 },
        { "jan", 1 },
        { "jan.", 1 },
        { "jana", 1 },
        { "janb", -1 },
        { "janc", 1 },
        { "jand", -1 },
        { "jane", -1 },
        { "janf", 1 },
        { "jang", -1 },
        { "janh", 1 },
        { "jani", -1 },
        { "janj", 1 },
        { "jank", -1 },
        { "janl", 1 },
        { "janm", 1 },
        { "jann", -1 },
        { "jano", 1 },
        { "janp", -1 },
        { "janq", -1 },
        { "janr", 1 },
        { "janu", -1 },
        { "july", 7 },
        { "jun", 6 },
        { "jun.", 6 },
        { "june", 6 }
    };
    checkIterator(iter, data, LENGTHOF(data));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), data, LENGTHOF(data));
}

void UCharsTrieTest::TestTruncatingIteratorFromLinearMatchShort() {
    static const StringAndValue data[]={
        { "abcdef", 10 },
        { "abcdepq", 200 },
        { "abcdeyz", 3000 }
    };
    LocalPointer<UCharsTrie> trie(buildTrie(data, LENGTHOF(data), USTRINGTRIE_BUILD_FAST));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    // Go into a linear-match node.
    trie->next(u_a);
    trie->next(u_b);
    IcuTestErrorCode errorCode(*this, "TestTruncatingIteratorFromLinearMatchShort()");
    // Truncate within the linear-match node.
    UCharsTrie::Iterator iter(*trie, 2, errorCode);
    if(errorCode.logIfFailureAndReset("UCharsTrie::Iterator(trie) constructor")) {
        return;
    }
    static const StringAndValue expected[]={
        { "cd", -1 }
    };
    checkIterator(iter, expected, LENGTHOF(expected));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), expected, LENGTHOF(expected));
}

void UCharsTrieTest::TestTruncatingIteratorFromLinearMatchLong() {
    static const StringAndValue data[]={
        { "abcdef", 10 },
        { "abcdepq", 200 },
        { "abcdeyz", 3000 }
    };
    LocalPointer<UCharsTrie> trie(buildTrie(data, LENGTHOF(data), USTRINGTRIE_BUILD_FAST));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    // Go into a linear-match node.
    trie->next(u_a);
    trie->next(u_b);
    trie->next(u_c);
    IcuTestErrorCode errorCode(*this, "TestTruncatingIteratorFromLinearMatchLong()");
    // Truncate after the linear-match node.
    UCharsTrie::Iterator iter(*trie, 3, errorCode);
    if(errorCode.logIfFailureAndReset("UCharsTrie::Iterator(trie) constructor")) {
        return;
    }
    static const StringAndValue expected[]={
        { "def", 10 },
        { "dep", -1 },
        { "dey", -1 }
    };
    checkIterator(iter, expected, LENGTHOF(expected));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), expected, LENGTHOF(expected));
}

void UCharsTrieTest::TestIteratorFromUChars() {
    static const StringAndValue data[]={
        { "mm", 3 },
        { "mmm", 33 },
        { "mmnop", 333 }
    };
    builder_->clear();
    IcuTestErrorCode errorCode(*this, "TestIteratorFromUChars()");
    for(int32_t i=0; i<LENGTHOF(data); ++i) {
        builder_->add(data[i].s, data[i].value, errorCode);
    }
    UnicodeString trieUChars;
    builder_->buildUnicodeString(USTRINGTRIE_BUILD_FAST, trieUChars, errorCode);
    UCharsTrie::Iterator iter(trieUChars.getBuffer(), 0, errorCode);
    checkIterator(iter, data, LENGTHOF(data));
}

void UCharsTrieTest::checkData(const StringAndValue data[], int32_t dataLength) {
    logln("checkData(dataLength=%d, fast)", (int)dataLength);
    checkData(data, dataLength, USTRINGTRIE_BUILD_FAST);
    logln("checkData(dataLength=%d, small)", (int)dataLength);
    checkData(data, dataLength, USTRINGTRIE_BUILD_SMALL);
}

void UCharsTrieTest::checkData(const StringAndValue data[], int32_t dataLength, UStringTrieBuildOption buildOption) {
    LocalPointer<UCharsTrie> trie(buildTrie(data, dataLength, buildOption));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    checkFirst(*trie, data, dataLength);
    checkNext(*trie, data, dataLength);
    checkNextWithState(*trie, data, dataLength);
    checkNextString(*trie, data, dataLength);
    checkIterator(*trie, data, dataLength);
}

UCharsTrie *UCharsTrieTest::buildTrie(const StringAndValue data[], int32_t dataLength,
                                      UStringTrieBuildOption buildOption) {
    IcuTestErrorCode errorCode(*this, "buildTrie()");
    // Add the items to the trie builder in an interesting (not trivial, not random) order.
    int32_t index, step;
    if(dataLength&1) {
        // Odd number of items.
        index=dataLength/2;
        step=2;
    } else if((dataLength%3)!=0) {
        // Not a multiple of 3.
        index=dataLength/5;
        step=3;
    } else {
        index=dataLength-1;
        step=-1;
    }
    builder_->clear();
    for(int32_t i=0; i<dataLength; ++i) {
        builder_->add(UnicodeString(data[index].s, -1, US_INV).unescape(),
                      data[index].value, errorCode);
        index=(index+step)%dataLength;
    }
    UnicodeString trieUChars;
    builder_->buildUnicodeString(buildOption, trieUChars, errorCode);
    LocalPointer<UCharsTrie> trie(builder_->build(buildOption, errorCode));
    if(!errorCode.logIfFailureAndReset("add()/build()")) {
        builder_->add("zzz", 999, errorCode);
        if(errorCode.reset()!=U_NO_WRITE_PERMISSION) {
            errln("builder.build().add(zzz) did not set U_NO_WRITE_PERMISSION");
        }
    }
    logln("serialized trie size: %ld UChars\n", (long)trieUChars.length());
    UnicodeString trieUChars2;
    builder_->buildUnicodeString(buildOption, trieUChars2, errorCode);
    if(trieUChars.getBuffer()==trieUChars2.getBuffer()) {
        errln("builder.buildUnicodeString() before & after build() returned same array");
    }
    if(errorCode.isFailure()) {
        return NULL;
    }
    // Tries from either build() method should be identical but
    // UCharsTrie does not implement equals().
    // We just return either one.
    if((dataLength&1)!=0) {
        return trie.orphan();
    } else {
        return new UCharsTrie(trieUChars2.getBuffer());
    }
}

void UCharsTrieTest::checkFirst(UCharsTrie &trie,
                                const StringAndValue data[], int32_t dataLength) {
    for(int32_t i=0; i<dataLength; ++i) {
        if(*data[i].s==0) {
            continue;  // skip empty string
        }
        UnicodeString expectedString=UnicodeString(data[i].s, -1, US_INV).unescape();
        UChar32 c=expectedString[0];
        UChar32 nextCp=expectedString.length()>1 ? expectedString[1] : 0;
        UStringTrieResult firstResult=trie.first(c);
        int32_t firstValue=USTRINGTRIE_HAS_VALUE(firstResult) ? trie.getValue() : -1;
        UStringTrieResult nextResult=trie.next(nextCp);
        if(firstResult!=trie.reset().next(c) ||
           firstResult!=trie.current() ||
           firstValue!=(USTRINGTRIE_HAS_VALUE(firstResult) ? trie.getValue() : -1) ||
           nextResult!=trie.next(nextCp)
        ) {
            errln("trie.first(U+%04X)!=trie.reset().next(same) for %s",
                  c, data[i].s);
        }
        c=expectedString.char32At(0);
        int32_t cLength=U16_LENGTH(c);
        nextCp=expectedString.length()>cLength ? expectedString.char32At(cLength) : 0;
        firstResult=trie.firstForCodePoint(c);
        firstValue=USTRINGTRIE_HAS_VALUE(firstResult) ? trie.getValue() : -1;
        nextResult=trie.nextForCodePoint(nextCp);
        if(firstResult!=trie.reset().nextForCodePoint(c) ||
           firstResult!=trie.current() ||
           firstValue!=(USTRINGTRIE_HAS_VALUE(firstResult) ? trie.getValue() : -1) ||
           nextResult!=trie.nextForCodePoint(nextCp)
        ) {
            errln("trie.firstForCodePoint(U+%04X)!=trie.reset().nextForCodePoint(same) for %s",
                  c, data[i].s);
        }
    }
    trie.reset();
}

void UCharsTrieTest::checkNext(UCharsTrie &trie,
                               const StringAndValue data[], int32_t dataLength) {
    UCharsTrie::State state; 
    for(int32_t i=0; i<dataLength; ++i) {
        UnicodeString expectedString=UnicodeString(data[i].s, -1, US_INV).unescape();
        int32_t stringLength= (i&1) ? -1 : expectedString.length();
        UStringTrieResult result;
        if( !USTRINGTRIE_HAS_VALUE(
                result=trie.next(expectedString.getTerminatedBuffer(), stringLength)) ||
            result!=trie.current()
        ) {
            errln("trie does not seem to contain %s", data[i].s);
        } else if(trie.getValue()!=data[i].value) {
            errln("trie value for %s is %ld=0x%lx instead of expected %ld=0x%lx",
                  data[i].s,
                  (long)trie.getValue(), (long)trie.getValue(),
                  (long)data[i].value, (long)data[i].value);
        } else if(result!=trie.current() || trie.getValue()!=data[i].value) {
            errln("trie value for %s changes when repeating current()/getValue()", data[i].s);
        }
        trie.reset();
        stringLength=expectedString.length();
        result=trie.current();
        for(int32_t j=0; j<stringLength; ++j) {
            if(!USTRINGTRIE_HAS_NEXT(result)) {
                errln("trie.current()!=hasNext before end of %s (at index %d)", data[i].s, j);
                break;
            }
            if(result==USTRINGTRIE_INTERMEDIATE_VALUE) {
                trie.getValue();
                if(trie.current()!=USTRINGTRIE_INTERMEDIATE_VALUE) {
                    errln("trie.getValue().current()!=USTRINGTRIE_INTERMEDIATE_VALUE before end of %s (at index %d)", data[i].s, j);
                    break;
                }
            }
            result=trie.next(expectedString[j]);
            if(!USTRINGTRIE_MATCHES(result)) {
                errln("trie.next()=USTRINGTRIE_NO_MATCH before end of %s (at index %d)", data[i].s, j);
                break;
            }
            if(result!=trie.current()) {
                errln("trie.next()!=following current() before end of %s (at index %d)", data[i].s, j);
                break;
            }
        }
        if(!USTRINGTRIE_HAS_VALUE(result)) {
            errln("trie.next()!=hasValue at the end of %s", data[i].s);
            continue;
        }
        trie.getValue();
        if(result!=trie.current()) {
            errln("trie.current() != current()+getValue()+current() after end of %s",
                  data[i].s);
        }
        // Compare the final current() with whether next() can actually continue.
        trie.saveState(state);
        UBool nextContinues=FALSE;
        for(int32_t c=0x20; c<0xe000; ++c) {
            if(c==0x80) {
                c=0xd800;  // Check for ASCII and surrogates but not all of the BMP.
            }
            if(trie.resetToState(state).next(c)) {
                nextContinues=TRUE;
                break;
            }
        }
        if((result==USTRINGTRIE_INTERMEDIATE_VALUE)!=nextContinues) {
            errln("(trie.current()==USTRINGTRIE_INTERMEDIATE_VALUE) contradicts "
                  "(trie.next(some UChar)!=USTRINGTRIE_NO_MATCH) after end of %s", data[i].s);
        }
        trie.reset();
    }
}

void UCharsTrieTest::checkNextWithState(UCharsTrie &trie,
                                        const StringAndValue data[], int32_t dataLength) {
    UCharsTrie::State noState, state; 
    for(int32_t i=0; i<dataLength; ++i) {
        if((i&1)==0) {
            // This should have no effect.
            trie.resetToState(noState);
        }
        UnicodeString expectedString=UnicodeString(data[i].s, -1, US_INV).unescape();
        int32_t stringLength=expectedString.length();
        int32_t partialLength=stringLength/3;
        for(int32_t j=0; j<partialLength; ++j) {
            if(!USTRINGTRIE_MATCHES(trie.next(expectedString[j]))) {
                errln("trie.next()=USTRINGTRIE_NO_MATCH for a prefix of %s", data[i].s);
                return;
            }
        }
        trie.saveState(state);
        UStringTrieResult resultAtState=trie.current();
        UStringTrieResult result;
        int32_t valueAtState=-99;
        if(USTRINGTRIE_HAS_VALUE(resultAtState)) {
            valueAtState=trie.getValue();
        }
        result=trie.next(0);  // mismatch
        if(result!=USTRINGTRIE_NO_MATCH || result!=trie.current()) {
            errln("trie.next(0) matched after part of %s", data[i].s);
        }
        if( resultAtState!=trie.resetToState(state).current() ||
            (USTRINGTRIE_HAS_VALUE(resultAtState) && valueAtState!=trie.getValue())
        ) {
            errln("trie.next(part of %s) changes current()/getValue() after "
                  "saveState/next(0)/resetToState",
                  data[i].s);
        } else if(!USTRINGTRIE_HAS_VALUE(
                      result=trie.next(expectedString.getTerminatedBuffer()+partialLength,
                                       stringLength-partialLength)) ||
                  result!=trie.current()) {
            errln("trie.next(rest of %s) does not seem to contain %s after "
                  "saveState/next(0)/resetToState",
                  data[i].s, data[i].s);
        } else if(!USTRINGTRIE_HAS_VALUE(
                      result=trie.resetToState(state).
                                  next(expectedString.getTerminatedBuffer()+partialLength,
                                       stringLength-partialLength)) ||
                  result!=trie.current()) {
            errln("trie does not seem to contain %s after saveState/next(rest)/resetToState",
                  data[i].s);
        } else if(trie.getValue()!=data[i].value) {
            errln("trie value for %s is %ld=0x%lx instead of expected %ld=0x%lx",
                  data[i].s,
                  (long)trie.getValue(), (long)trie.getValue(),
                  (long)data[i].value, (long)data[i].value);
        }
        trie.reset();
    }
}

// next(string) is also tested in other functions,
// but here we try to go partway through the string, and then beyond it.
void UCharsTrieTest::checkNextString(UCharsTrie &trie,
                                     const StringAndValue data[], int32_t dataLength) {
    for(int32_t i=0; i<dataLength; ++i) {
        UnicodeString expectedString=UnicodeString(data[i].s, -1, US_INV).unescape();
        int32_t stringLength=expectedString.length();
        if(!trie.next(expectedString.getTerminatedBuffer(), stringLength/2)) {
            errln("trie.next(up to middle of string)=USTRINGTRIE_NO_MATCH for %s", data[i].s);
            continue;
        }
        // Test that we stop properly at the end of the string.
        if(trie.next(expectedString.getTerminatedBuffer()+stringLength/2,
                     stringLength+1-stringLength/2)) {
            errln("trie.next(string+NUL)!=USTRINGTRIE_NO_MATCH for %s", data[i].s);
        }
        trie.reset();
    }
}

void UCharsTrieTest::checkIterator(UCharsTrie &trie,
                                   const StringAndValue data[], int32_t dataLength) {
    IcuTestErrorCode errorCode(*this, "checkIterator()");
    UCharsTrie::Iterator iter(trie, 0, errorCode);
    if(errorCode.logIfFailureAndReset("UCharsTrie::Iterator(trieUChars) constructor")) {
        return;
    }
    checkIterator(iter, data, dataLength);
}

void UCharsTrieTest::checkIterator(UCharsTrie::Iterator &iter,
                                   const StringAndValue data[], int32_t dataLength) {
    IcuTestErrorCode errorCode(*this, "checkIterator()");
    for(int32_t i=0; i<dataLength; ++i) {
        if(!iter.hasNext()) {
            errln("trie iterator hasNext()=FALSE for item %d: %s", (int)i, data[i].s);
            break;
        }
        UBool hasNext=iter.next(errorCode);
        if(errorCode.logIfFailureAndReset("trie iterator next() for item %d: %s", (int)i, data[i].s)) {
            break;
        }
        if(!hasNext) {
            errln("trie iterator next()=FALSE for item %d: %s", (int)i, data[i].s);
            break;
        }
        UnicodeString expectedString=UnicodeString(data[i].s, -1, US_INV).unescape();
        if(iter.getString()!=expectedString) {
            char buffer[1000];
            UnicodeString invString(prettify(iter.getString()));
            invString.extract(0, invString.length(), buffer, LENGTHOF(buffer), US_INV);
            errln("trie iterator next().getString()=%s but expected %s for item %d",
                  buffer, data[i].s, (int)i);
        }
        if(iter.getValue()!=data[i].value) {
            errln("trie iterator next().getValue()=%ld=0x%lx but expected %ld=0x%lx for item %d: %s",
                  (long)iter.getValue(), (long)iter.getValue(),
                  (long)data[i].value, (long)data[i].value,
                  (int)i, data[i].s);
        }
    }
    if(iter.hasNext()) {
        errln("trie iterator hasNext()=TRUE after all items");
    }
    UBool hasNext=iter.next(errorCode);
    errorCode.logIfFailureAndReset("trie iterator next() after all items");
    if(hasNext) {
        errln("trie iterator next()=TRUE after all items");
    }
}
