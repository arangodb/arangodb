// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*   Copyright (C) 2010-2014, International Business Machines
*   Corporation and others.  All Rights Reserved.
*******************************************************************************
*   file name:  bytetrietest.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2010nov16
*   created by: Markus W. Scherer
*/

#include <string.h>

#include "unicode/utypes.h"
#include "unicode/bytestrie.h"
#include "unicode/bytestriebuilder.h"
#include "unicode/localpointer.h"
#include "unicode/stringpiece.h"
#include "intltest.h"
#include "cmemory.h"

struct StringAndValue {
    const char *s;
    int32_t value;
};

class BytesTrieTest : public IntlTest {
public:
    BytesTrieTest();
    virtual ~BytesTrieTest();

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

    BytesTrie *buildMonthsTrie(UStringTrieBuildOption buildOption);
    void TestHasUniqueValue();
    void TestGetNextBytes();
    void TestIteratorFromBranch();
    void TestIteratorFromLinearMatch();
    void TestTruncatingIteratorFromRoot();
    void TestTruncatingIteratorFromLinearMatchShort();
    void TestTruncatingIteratorFromLinearMatchLong();
    void TestIteratorFromBytes();
    void TestFailedIterator();

    void checkData(const StringAndValue data[], int32_t dataLength);
    void checkData(const StringAndValue data[], int32_t dataLength, UStringTrieBuildOption buildOption);
    BytesTrie *buildTrie(const StringAndValue data[], int32_t dataLength,
                         UStringTrieBuildOption buildOption);
    void checkFirst(BytesTrie &trie, const StringAndValue data[], int32_t dataLength);
    void checkNext(BytesTrie &trie, const StringAndValue data[], int32_t dataLength);
    void checkNextWithState(BytesTrie &trie, const StringAndValue data[], int32_t dataLength);
    void checkNextString(BytesTrie &trie, const StringAndValue data[], int32_t dataLength);
    void checkIterator(const BytesTrie &trie, const StringAndValue data[], int32_t dataLength);
    void checkIterator(BytesTrie::Iterator &iter, const StringAndValue data[], int32_t dataLength);

private:
    BytesTrieBuilder *builder_;
};

extern IntlTest *createBytesTrieTest() {
    return new BytesTrieTest();
}

BytesTrieTest::BytesTrieTest() : builder_(NULL) {
    IcuTestErrorCode errorCode(*this, "BytesTrieTest()");
    builder_=new BytesTrieBuilder(errorCode);
}

BytesTrieTest::~BytesTrieTest() {
    delete builder_;
}

void BytesTrieTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) {
    if(exec) {
        logln("TestSuite BytesTrieTest: ");
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
    TESTCASE_AUTO(TestHasUniqueValue);
    TESTCASE_AUTO(TestGetNextBytes);
    TESTCASE_AUTO(TestIteratorFromBranch);
    TESTCASE_AUTO(TestIteratorFromLinearMatch);
    TESTCASE_AUTO(TestTruncatingIteratorFromRoot);
    TESTCASE_AUTO(TestTruncatingIteratorFromLinearMatchShort);
    TESTCASE_AUTO(TestTruncatingIteratorFromLinearMatchLong);
    TESTCASE_AUTO(TestIteratorFromBytes);
    TESTCASE_AUTO(TestFailedIterator);
    TESTCASE_AUTO_END;
}

void BytesTrieTest::TestBuilder() {
    IcuTestErrorCode errorCode(*this, "TestBuilder()");
    builder_->clear();
    delete builder_->build(USTRINGTRIE_BUILD_FAST, errorCode);
    if(errorCode.reset()!=U_INDEX_OUTOFBOUNDS_ERROR) {
        errln("BytesTrieBuilder().build() did not set U_INDEX_OUTOFBOUNDS_ERROR");
        return;
    }
    // TODO: remove .build(...) once add() checks for duplicates.
    builder_->add("=", 0, errorCode).add("=", 1, errorCode).build(USTRINGTRIE_BUILD_FAST, errorCode);
    if(errorCode.reset()!=U_ILLEGAL_ARGUMENT_ERROR) {
        errln("BytesTrieBuilder.add() did not detect duplicates");
        return;
    }
}

void BytesTrieTest::TestEmpty() {
    static const StringAndValue data[]={
        { "", 0 }
    };
    checkData(data, UPRV_LENGTHOF(data));
}

void BytesTrieTest::Test_a() {
    static const StringAndValue data[]={
        { "a", 1 }
    };
    checkData(data, UPRV_LENGTHOF(data));
}

void BytesTrieTest::Test_a_ab() {
    static const StringAndValue data[]={
        { "a", 1 },
        { "ab", 100 }
    };
    checkData(data, UPRV_LENGTHOF(data));
}

void BytesTrieTest::TestShortestBranch() {
    static const StringAndValue data[]={
        { "a", 1000 },
        { "b", 2000 }
    };
    checkData(data, UPRV_LENGTHOF(data));
}

void BytesTrieTest::TestBranches() {
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
        { "zz", (int32_t)0x80000000 }
    };
    for(int32_t length=2; length<=UPRV_LENGTHOF(data); ++length) {
        logln("TestBranches length=%d", (int)length);
        checkData(data, length);
    }
}

void BytesTrieTest::TestLongSequence() {
    static const StringAndValue data[]={
        { "a", -1 },
        // sequence of linear-match nodes
        { "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", -2 },
        // more than 256 bytes
        { "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ", -3 }
    };
    checkData(data, UPRV_LENGTHOF(data));
}

void BytesTrieTest::TestLongBranch() {
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
        { "z", (int32_t)0x80000001 }
    };
    checkData(data, UPRV_LENGTHOF(data));
}

void BytesTrieTest::TestValuesForState() {
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
    checkData(data, UPRV_LENGTHOF(data));
}

void BytesTrieTest::TestCompact() {
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
    checkData(data, UPRV_LENGTHOF(data));
}

BytesTrie *BytesTrieTest::buildMonthsTrie(UStringTrieBuildOption buildOption) {
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
    return buildTrie(data, UPRV_LENGTHOF(data), buildOption);
}

void BytesTrieTest::TestHasUniqueValue() {
    LocalPointer<BytesTrie> trie(buildMonthsTrie(USTRINGTRIE_BUILD_FAST));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    int32_t uniqueValue;
    if(trie->hasUniqueValue(uniqueValue)) {
        errln("unique value at root");
    }
    trie->next('j');
    trie->next('a');
    trie->next('n');
    // hasUniqueValue() directly after next()
    if(!trie->hasUniqueValue(uniqueValue) || uniqueValue!=1) {
        errln("not unique value 1 after \"jan\"");
    }
    trie->first('j');
    trie->next('u');
    if(trie->hasUniqueValue(uniqueValue)) {
        errln("unique value after \"ju\"");
    }
    if(trie->next('n')!=USTRINGTRIE_INTERMEDIATE_VALUE || 6!=trie->getValue()) {
        errln("not normal value 6 after \"jun\"");
    }
    // hasUniqueValue() after getValue()
    if(!trie->hasUniqueValue(uniqueValue) || uniqueValue!=6) {
        errln("not unique value 6 after \"jun\"");
    }
    // hasUniqueValue() from within a linear-match node
    trie->first('a');
    trie->next('u');
    if(!trie->hasUniqueValue(uniqueValue) || uniqueValue!=8) {
        errln("not unique value 8 after \"au\"");
    }
}

void BytesTrieTest::TestGetNextBytes() {
    LocalPointer<BytesTrie> trie(buildMonthsTrie(USTRINGTRIE_BUILD_SMALL));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    char buffer[40];
    CheckedArrayByteSink sink(buffer, UPRV_LENGTHOF(buffer));
    int32_t count=trie->getNextBytes(sink);
    if(count!=2 || sink.NumberOfBytesAppended()!=2 || buffer[0]!='a' || buffer[1]!='j') {
        errln("months getNextBytes()!=[aj] at root");
    }
    trie->next('j');
    trie->next('a');
    trie->next('n');
    // getNextBytes() directly after next()
    count=trie->getNextBytes(sink.Reset());
    buffer[count]=0;
    if(count!=20 || sink.NumberOfBytesAppended()!=20 || 0!=strcmp(buffer, ".abcdefghijklmnopqru")) {
        errln("months getNextBytes()!=[.abcdefghijklmnopqru] after \"jan\"");
    }
    // getNextBytes() after getValue()
    trie->getValue();  // next() had returned USTRINGTRIE_INTERMEDIATE_VALUE.
    memset(buffer, 0, sizeof(buffer));
    count=trie->getNextBytes(sink.Reset());
    if(count!=20 || sink.NumberOfBytesAppended()!=20 || 0!=strcmp(buffer, ".abcdefghijklmnopqru")) {
        errln("months getNextBytes()!=[.abcdefghijklmnopqru] after \"jan\"+getValue()");
    }
    // getNextBytes() from a linear-match node
    trie->next('u');
    memset(buffer, 0, sizeof(buffer));
    count=trie->getNextBytes(sink.Reset());
    if(count!=1 || sink.NumberOfBytesAppended()!=1 || buffer[0]!='a') {
        errln("months getNextBytes()!=[a] after \"janu\"");
    }
    trie->next('a');
    memset(buffer, 0, sizeof(buffer));
    count=trie->getNextBytes(sink.Reset());
    if(count!=1 || sink.NumberOfBytesAppended()!=1 || buffer[0]!='r') {
        errln("months getNextBytes()!=[r] after \"janua\"");
    }
    trie->next('r');
    trie->next('y');
    // getNextBytes() after a final match
    count=trie->getNextBytes(sink.Reset());
    if(count!=0 || sink.NumberOfBytesAppended()!=0) {
        errln("months getNextBytes()!=[] after \"january\"");
    }
}

void BytesTrieTest::TestIteratorFromBranch() {
    LocalPointer<BytesTrie> trie(buildMonthsTrie(USTRINGTRIE_BUILD_FAST));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    // Go to a branch node.
    trie->next('j');
    trie->next('a');
    trie->next('n');
    IcuTestErrorCode errorCode(*this, "TestIteratorFromBranch()");
    BytesTrie::Iterator iter(*trie, 0, errorCode);
    if(errorCode.errIfFailureAndReset("BytesTrie::Iterator(trie) constructor")) {
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
    checkIterator(iter, data, UPRV_LENGTHOF(data));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), data, UPRV_LENGTHOF(data));
}

void BytesTrieTest::TestIteratorFromLinearMatch() {
    LocalPointer<BytesTrie> trie(buildMonthsTrie(USTRINGTRIE_BUILD_SMALL));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    // Go into a linear-match node.
    trie->next('j');
    trie->next('a');
    trie->next('n');
    trie->next('u');
    trie->next('a');
    IcuTestErrorCode errorCode(*this, "TestIteratorFromLinearMatch()");
    BytesTrie::Iterator iter(*trie, 0, errorCode);
    if(errorCode.errIfFailureAndReset("BytesTrie::Iterator(trie) constructor")) {
        return;
    }
    // Expected data: Same as in buildMonthsTrie(), except only the suffixes
    // following "janua".
    static const StringAndValue data[]={
        { "r", 1 },
        { "ry", 1 }
    };
    checkIterator(iter, data, UPRV_LENGTHOF(data));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), data, UPRV_LENGTHOF(data));
}

void BytesTrieTest::TestTruncatingIteratorFromRoot() {
    LocalPointer<BytesTrie> trie(buildMonthsTrie(USTRINGTRIE_BUILD_FAST));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    IcuTestErrorCode errorCode(*this, "TestTruncatingIteratorFromRoot()");
    BytesTrie::Iterator iter(*trie, 4, errorCode);
    if(errorCode.errIfFailureAndReset("BytesTrie::Iterator(trie) constructor")) {
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
    checkIterator(iter, data, UPRV_LENGTHOF(data));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), data, UPRV_LENGTHOF(data));
}

void BytesTrieTest::TestTruncatingIteratorFromLinearMatchShort() {
    static const StringAndValue data[]={
        { "abcdef", 10 },
        { "abcdepq", 200 },
        { "abcdeyz", 3000 }
    };
    LocalPointer<BytesTrie> trie(buildTrie(data, UPRV_LENGTHOF(data), USTRINGTRIE_BUILD_FAST));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    // Go into a linear-match node.
    trie->next('a');
    trie->next('b');
    IcuTestErrorCode errorCode(*this, "TestTruncatingIteratorFromLinearMatchShort()");
    // Truncate within the linear-match node.
    BytesTrie::Iterator iter(*trie, 2, errorCode);
    if(errorCode.errIfFailureAndReset("BytesTrie::Iterator(trie) constructor")) {
        return;
    }
    static const StringAndValue expected[]={
        { "cd", -1 }
    };
    checkIterator(iter, expected, UPRV_LENGTHOF(expected));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), expected, UPRV_LENGTHOF(expected));
}

void BytesTrieTest::TestTruncatingIteratorFromLinearMatchLong() {
    static const StringAndValue data[]={
        { "abcdef", 10 },
        { "abcdepq", 200 },
        { "abcdeyz", 3000 }
    };
    LocalPointer<BytesTrie> trie(buildTrie(data, UPRV_LENGTHOF(data), USTRINGTRIE_BUILD_FAST));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    // Go into a linear-match node.
    trie->next('a');
    trie->next('b');
    trie->next('c');
    IcuTestErrorCode errorCode(*this, "TestTruncatingIteratorFromLinearMatchLong()");
    // Truncate after the linear-match node.
    BytesTrie::Iterator iter(*trie, 3, errorCode);
    if(errorCode.errIfFailureAndReset("BytesTrie::Iterator(trie) constructor")) {
        return;
    }
    static const StringAndValue expected[]={
        { "def", 10 },
        { "dep", -1 },
        { "dey", -1 }
    };
    checkIterator(iter, expected, UPRV_LENGTHOF(expected));
    // Reset, and we should get the same result.
    logln("after iter.reset()");
    checkIterator(iter.reset(), expected, UPRV_LENGTHOF(expected));
}

void BytesTrieTest::TestIteratorFromBytes() {
    static const StringAndValue data[]={
        { "mm", 3 },
        { "mmm", 33 },
        { "mmnop", 333 }
    };
    builder_->clear();
    IcuTestErrorCode errorCode(*this, "TestIteratorFromBytes()");
    for(int32_t i=0; i<UPRV_LENGTHOF(data); ++i) {
        builder_->add(data[i].s, data[i].value, errorCode);
    }
    StringPiece trieBytes=builder_->buildStringPiece(USTRINGTRIE_BUILD_FAST, errorCode);
    BytesTrie::Iterator iter(trieBytes.data(), 0, errorCode);
    checkIterator(iter, data, UPRV_LENGTHOF(data));
}

void BytesTrieTest::TestFailedIterator() {
    UErrorCode failure = U_ILLEGAL_ARGUMENT_ERROR;
    BytesTrie::Iterator iter(NULL, 0, failure);
    StringPiece sp = iter.getString();
    if (!sp.empty()) {
        errln("failed iterator returned garbage data");
    }
}

void BytesTrieTest::checkData(const StringAndValue data[], int32_t dataLength) {
    logln("checkData(dataLength=%d, fast)", (int)dataLength);
    checkData(data, dataLength, USTRINGTRIE_BUILD_FAST);
    logln("checkData(dataLength=%d, small)", (int)dataLength);
    checkData(data, dataLength, USTRINGTRIE_BUILD_SMALL);
}

void BytesTrieTest::checkData(const StringAndValue data[], int32_t dataLength, UStringTrieBuildOption buildOption) {
    LocalPointer<BytesTrie> trie(buildTrie(data, dataLength, buildOption));
    if(trie.isNull()) {
        return;  // buildTrie() reported an error
    }
    checkFirst(*trie, data, dataLength);
    checkNext(*trie, data, dataLength);
    checkNextWithState(*trie, data, dataLength);
    checkNextString(*trie, data, dataLength);
    checkIterator(*trie, data, dataLength);
}

BytesTrie *BytesTrieTest::buildTrie(const StringAndValue data[], int32_t dataLength,
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
        builder_->add(data[index].s, data[index].value, errorCode);
        index=(index+step)%dataLength;
    }
    StringPiece sp=builder_->buildStringPiece(buildOption, errorCode);
    LocalPointer<BytesTrie> trie(builder_->build(buildOption, errorCode));
    if(!errorCode.errIfFailureAndReset("add()/build()")) {
        builder_->add("zzz", 999, errorCode);
        if(errorCode.reset()!=U_NO_WRITE_PERMISSION) {
            errln("builder.build().add(zzz) did not set U_NO_WRITE_PERMISSION");
        }
    }
    logln("serialized trie size: %ld bytes\n", (long)sp.length());
    StringPiece sp2=builder_->buildStringPiece(buildOption, errorCode);
    if(sp.data()==sp2.data()) {
        errln("builder.buildStringPiece() before & after build() returned same array");
    }
    if(errorCode.isFailure()) {
        return NULL;
    }
    // Tries from either build() method should be identical but
    // BytesTrie does not implement equals().
    // We just return either one.
    if((dataLength&1)!=0) {
        return trie.orphan();
    } else {
        return new BytesTrie(sp2.data());
    }
}

void BytesTrieTest::checkFirst(BytesTrie &trie,
                               const StringAndValue data[], int32_t dataLength) {
    for(int32_t i=0; i<dataLength; ++i) {
        int c=*data[i].s;
        if(c==0) {
            continue;  // skip empty string
        }
        UStringTrieResult firstResult=trie.first(c);
        int32_t firstValue=USTRINGTRIE_HAS_VALUE(firstResult) ? trie.getValue() : -1;
        UStringTrieResult nextResult=trie.next(data[i].s[1]);
        if(firstResult!=trie.reset().next(c) ||
           firstResult!=trie.current() ||
           firstValue!=(USTRINGTRIE_HAS_VALUE(firstResult) ? trie.getValue() : -1) ||
           nextResult!=trie.next(data[i].s[1])
        ) {
            errln("trie.first(%c)!=trie.reset().next(same) for %s",
                  c, data[i].s);
        }
    }
    trie.reset();
}

void BytesTrieTest::checkNext(BytesTrie &trie,
                              const StringAndValue data[], int32_t dataLength) {
    BytesTrie::State state;
    for(int32_t i=0; i<dataLength; ++i) {
        int32_t stringLength= (i&1) ? -1 : static_cast<int32_t>(strlen(data[i].s));
        UStringTrieResult result;
        if( !USTRINGTRIE_HAS_VALUE(result=trie.next(data[i].s, stringLength)) ||
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
        stringLength = static_cast<int32_t>(strlen(data[i].s));
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
            result=trie.next(data[i].s[j]);
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
        // Try all graphic characters; we only use those in test strings in this file.
#if U_CHARSET_FAMILY==U_ASCII_FAMILY
        const int32_t minChar=0x20;
        const int32_t maxChar=0x7e;
#elif U_CHARSET_FAMILY==U_EBCDIC_FAMILY
        const int32_t minChar=0x40;
        const int32_t maxChar=0xfe;
#else
        const int32_t minChar=0;
        const int32_t maxChar=0xff;
#endif
        for(int32_t c=minChar; c<=maxChar; ++c) {
            if(trie.resetToState(state).next(c)) {
                nextContinues=TRUE;
                break;
            }
        }
        if((result==USTRINGTRIE_INTERMEDIATE_VALUE)!=nextContinues) {
            errln("(trie.current()==USTRINGTRIE_INTERMEDIATE_VALUE) contradicts "
                  "(trie.next(some byte)!=USTRINGTRIE_NO_MATCH) after end of %s", data[i].s);
        }
        trie.reset();
    }
}

void BytesTrieTest::checkNextWithState(BytesTrie &trie,
                                       const StringAndValue data[], int32_t dataLength) {
    BytesTrie::State noState, state;
    for(int32_t i=0; i<dataLength; ++i) {
        if((i&1)==0) {
            // This should have no effect.
            trie.resetToState(noState);
        }
        const char *expectedString=data[i].s;
        int32_t stringLength= static_cast<int32_t>(strlen(expectedString));
        int32_t partialLength = stringLength / 3;
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
                      result=trie.next(expectedString+partialLength,
                                       stringLength-partialLength)) ||
                  result!=trie.current()) {
            errln("trie.next(rest of %s) does not seem to contain %s after "
                  "saveState/next(0)/resetToState",
                  data[i].s, data[i].s);
        } else if(!USTRINGTRIE_HAS_VALUE(
                      result=trie.resetToState(state).
                                  next(expectedString+partialLength,
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
void BytesTrieTest::checkNextString(BytesTrie &trie,
                                    const StringAndValue data[], int32_t dataLength) {
    for(int32_t i=0; i<dataLength; ++i) {
        const char *expectedString=data[i].s;
        int32_t stringLength = static_cast<int32_t>(strlen(expectedString));
        if(!trie.next(expectedString, stringLength/2)) {
            errln("trie.next(up to middle of string)=USTRINGTRIE_NO_MATCH for %s", data[i].s);
            continue;
        }
        // Test that we stop properly at the end of the string.
        if(trie.next(expectedString+stringLength/2, stringLength+1-stringLength/2)) {
            errln("trie.next(string+NUL)!=USTRINGTRIE_NO_MATCH for %s", data[i].s);
        }
        trie.reset();
    }
}

void BytesTrieTest::checkIterator(const BytesTrie &trie,
                                  const StringAndValue data[], int32_t dataLength) {
    IcuTestErrorCode errorCode(*this, "checkIterator()");
    BytesTrie::Iterator iter(trie, 0, errorCode);
    if(errorCode.errIfFailureAndReset("BytesTrie::Iterator(trie) constructor")) {
        return;
    }
    checkIterator(iter, data, dataLength);
}

void BytesTrieTest::checkIterator(BytesTrie::Iterator &iter,
                                  const StringAndValue data[], int32_t dataLength) {
    IcuTestErrorCode errorCode(*this, "checkIterator()");
    for(int32_t i=0; i<dataLength; ++i) {
        if(!iter.hasNext()) {
            errln("trie iterator hasNext()=FALSE for item %d: %s", (int)i, data[i].s);
            break;
        }
        UBool hasNext=iter.next(errorCode);
        if(errorCode.errIfFailureAndReset("trie iterator next() for item %d: %s", (int)i, data[i].s)) {
            break;
        }
        if(!hasNext) {
            errln("trie iterator next()=FALSE for item %d: %s", (int)i, data[i].s);
            break;
        }
        if(iter.getString()!=StringPiece(data[i].s)) {
            errln("trie iterator next().getString()=%s but expected %s for item %d",
                  iter.getString().data(), data[i].s, (int)i);
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
    errorCode.errIfFailureAndReset("trie iterator next() after all items");
    if(hasNext) {
        errln("trie iterator next()=TRUE after all items");
    }
}
