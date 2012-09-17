/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2012, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/
//
//   file:  alphaindex.cpp
//          Alphabetic Index Tests.
//
#include "intltest.h"
#include "alphaindextst.h"

#include "unicode/alphaindex.h"
#include "unicode/coll.h"
#include "unicode/tblcoll.h"
#include "unicode/uniset.h"

#if !UCONFIG_NO_COLLATION && !UCONFIG_NO_NORMALIZATION

// #include <string>
// #include <iostream>

AlphabeticIndexTest::AlphabeticIndexTest() {
}

AlphabeticIndexTest::~AlphabeticIndexTest() {
}

void AlphabeticIndexTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite AlphabeticIndex: ");
    switch (index) {

        case 0: name = "APITest";
            if (exec) APITest();
            break;

        case 1: name = "ManyLocales";
            if (exec) ManyLocalesTest();
            break;

        case 2: name = "HackPinyinTest";
            if (exec) HackPinyinTest();
            break;

        case 3: name = "TestBug9009";
            if (exec) TestBug9009();
            break;

        default: name = "";
            break; //needed to end loop
    }
}

#define TEST_CHECK_STATUS {if (U_FAILURE(status)) {dataerrln("%s:%d: Test failure.  status=%s", \
                                                              __FILE__, __LINE__, u_errorName(status)); return;}}

#define TEST_ASSERT(expr) {if ((expr)==FALSE) {errln("%s:%d: Test failure \n", __FILE__, __LINE__);};}

//
//  APITest.   Invoke every function at least once, and check that it does something.
//             Does not attempt to check complete functionality.
//
void AlphabeticIndexTest::APITest() {
    //
    //  Simple constructor and destructor,  getBucketCount()
    //
    UErrorCode status = U_ZERO_ERROR;
    int32_t lc = 0;
    int32_t i  = 0;
    AlphabeticIndex *index = new AlphabeticIndex(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
    lc = index->getBucketCount(status);
    TEST_CHECK_STATUS;
    TEST_ASSERT(28 == lc);    // 26 letters plus two under/overflow labels.
    //printf("getBucketCount() == %d\n", lc);
    delete index;

    // addLabels()

    status = U_ZERO_ERROR;
    index = new AlphabeticIndex(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
    UnicodeSet additions;
    additions.add((UChar32)0x410).add((UChar32)0x415);   // A couple of Cyrillic letters
    index->addLabels(additions, status);
    TEST_CHECK_STATUS;
    lc = index->getBucketCount(status);
    TEST_CHECK_STATUS;
    // TODO:  should get 31.  Java also gives 30.  Needs fixing
    TEST_ASSERT(30 == lc);    // 26 Latin letters plus
    // TEST_ASSERT(31 == lc);    // 26 Latin letters plus
                              //  2 Cyrillic letters plus
                              //  1 inflow label plus
                              //  two under/overflow labels.
    // std::cout << lc << std::endl;
    delete index;


    // addLabels(Locale)

    status = U_ZERO_ERROR;
    index = new AlphabeticIndex(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
    AlphabeticIndex &aip = index->addLabels(Locale::getJapanese(), status);
    TEST_ASSERT(&aip == index);
    TEST_CHECK_STATUS;
    lc = index->getBucketCount(status);
    TEST_CHECK_STATUS;
    TEST_ASSERT(35 < lc);  // Japanese should add a bunch.  Don't rely on the exact value.
    delete index;

    // GetCollator(),  Get under/in/over flow labels

    status = U_ZERO_ERROR;
    index = new AlphabeticIndex(Locale::getGerman(), status);
    TEST_CHECK_STATUS;
    Collator *germanCol = Collator::createInstance(Locale::getGerman(), status);
    TEST_CHECK_STATUS;
    const RuleBasedCollator &indexCol = index->getCollator();
    TEST_ASSERT(*germanCol == indexCol);
    delete germanCol;

    UnicodeString ELLIPSIS;  ELLIPSIS.append((UChar32)0x2026);
    UnicodeString s = index->getUnderflowLabel();
    TEST_ASSERT(ELLIPSIS == s);
    s = index->getOverflowLabel();
    TEST_ASSERT(ELLIPSIS == s);
    s = index->getInflowLabel();
    TEST_ASSERT(ELLIPSIS == s);
    index->setOverflowLabel(UNICODE_STRING_SIMPLE("O"), status);
    index->setUnderflowLabel(UNICODE_STRING_SIMPLE("U"), status).setInflowLabel(UNICODE_STRING_SIMPLE("I"), status);
    s = index->getUnderflowLabel();
    TEST_ASSERT(UNICODE_STRING_SIMPLE("U") == s);
    s = index->getOverflowLabel();
    TEST_ASSERT(UNICODE_STRING_SIMPLE("O") == s);
    s = index->getInflowLabel();
    TEST_ASSERT(UNICODE_STRING_SIMPLE("I") == s);




    delete index;



    const UnicodeString adam = UNICODE_STRING_SIMPLE("Adam");
    const UnicodeString baker = UNICODE_STRING_SIMPLE("Baker");
    const UnicodeString charlie = UNICODE_STRING_SIMPLE("Charlie");
    const UnicodeString chad = UNICODE_STRING_SIMPLE("Chad");
    const UnicodeString zed  = UNICODE_STRING_SIMPLE("Zed");
    const UnicodeString Cyrillic = UNICODE_STRING_SIMPLE("\\u0410\\u0443\\u0435").unescape();

    // addRecord(), verify that it comes back out.
    //
    status = U_ZERO_ERROR;
    index = new AlphabeticIndex(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
    index->addRecord(UnicodeString("Adam"), this, status);
    UBool   b;
    TEST_CHECK_STATUS;
    index->resetBucketIterator(status);
    TEST_CHECK_STATUS;
    index->nextBucket(status);  // Move to underflow label
    index->nextBucket(status);  // Move to "A"
    TEST_CHECK_STATUS;
    const UnicodeString &label2 = index->getBucketLabel();
    UnicodeString A_STR = UNICODE_STRING_SIMPLE("A");
    TEST_ASSERT(A_STR == label2);

    b = index->nextRecord(status);
    TEST_CHECK_STATUS;
    TEST_ASSERT(b);
    const UnicodeString &itemName = index->getRecordName();
    TEST_ASSERT(adam == itemName);

    const void *itemContext = index->getRecordData();
    TEST_ASSERT(itemContext == this);

    delete index;

    // clearRecords, addRecord(), Iteration

    status = U_ZERO_ERROR;
    index = new AlphabeticIndex(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
    while (index->nextBucket(status)) {
        TEST_CHECK_STATUS;
        while (index->nextRecord(status)) {
            TEST_CHECK_STATUS;
            TEST_ASSERT(FALSE);   // No items have been added.
        }
        TEST_CHECK_STATUS;
    }

    index->addRecord(adam, NULL, status);
    index->addRecord(baker, NULL, status);
    index->addRecord(charlie, NULL, status);
    index->addRecord(chad, NULL, status);
    TEST_CHECK_STATUS;
    int itemCount = 0;
    index->resetBucketIterator(status);
    while (index->nextBucket(status)) {
        TEST_CHECK_STATUS;
        while (index->nextRecord(status)) {
            TEST_CHECK_STATUS;
            ++itemCount;
        }
    }
    TEST_CHECK_STATUS;
    TEST_ASSERT(itemCount == 4);

    TEST_ASSERT(index->nextBucket(status) == FALSE);
    index->resetBucketIterator(status);
    TEST_CHECK_STATUS;
    TEST_ASSERT(index->nextBucket(status) == TRUE);

    index->clearRecords(status);
    TEST_CHECK_STATUS;
    index->resetBucketIterator(status);
    while (index->nextBucket(status)) {
        TEST_CHECK_STATUS;
        while (index->nextRecord(status)) {
            TEST_ASSERT(FALSE);   // No items have been added.
        }
    }
    TEST_CHECK_STATUS;
    delete index;

    // getBucketLabel(), getBucketType()

    status = U_ZERO_ERROR;
    index = new AlphabeticIndex(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
    index->setUnderflowLabel(adam, status).setOverflowLabel(charlie, status);
    TEST_CHECK_STATUS;
    for (i=0; index->nextBucket(status); i++) {
        TEST_CHECK_STATUS;
        UnicodeString label = index->getBucketLabel();
        UAlphabeticIndexLabelType type = index->getBucketLabelType();
        if (i == 0) {
            TEST_ASSERT(type == U_ALPHAINDEX_UNDERFLOW);
            TEST_ASSERT(label == adam);
        } else if (i <= 26) {
            // Labels A - Z for English locale
            TEST_ASSERT(type == U_ALPHAINDEX_NORMAL);
            UnicodeString expectedLabel((UChar)(0x40 + i));
            TEST_ASSERT(expectedLabel == label);
        } else if (i == 27) {
            TEST_ASSERT(type == U_ALPHAINDEX_OVERFLOW);
            TEST_ASSERT(label == charlie);
        } else {
            TEST_ASSERT(FALSE);
        }
    }
    TEST_ASSERT(i==28);
    delete index;

    // getBucketIndex()

    status = U_ZERO_ERROR;
    index = new AlphabeticIndex(Locale::getEnglish(), status);
    TEST_CHECK_STATUS;
    int32_t n = index->getBucketIndex(adam, status);
    TEST_CHECK_STATUS;
    TEST_ASSERT(n == 1);    /*  Label #0 is underflow, 1 is A, etc. */
    n = index->getBucketIndex(baker, status);
    TEST_ASSERT(n == 2);
    n = index->getBucketIndex(Cyrillic, status);
    TEST_ASSERT(n == 27);   // Overflow label
    n = index->getBucketIndex(zed, status);
    TEST_ASSERT(n == 26);

    for (i=0; index->nextBucket(status); i++) {
        n = index->getBucketIndex();
        TEST_ASSERT(n == i);
        UnicodeString label = index->getBucketLabel();
        TEST_ASSERT(n == i);
    }
    TEST_ASSERT(i == 28);

    delete index;
    index = new AlphabeticIndex(Locale::createFromName("ru"), status);
        //Locale loc = Locale::createFromName(localeName);
    TEST_CHECK_STATUS;
    n = index->getBucketIndex(adam, status);
    TEST_CHECK_STATUS;
    TEST_ASSERT(n == 0);    //  Label #0 is underflow
    n = index->getBucketIndex(baker, status);
    TEST_ASSERT(n == 0);
    n = index->getBucketIndex(Cyrillic, status);
    TEST_ASSERT(n == 1);   // Overflow label
    n = index->getBucketIndex(zed, status);
    TEST_ASSERT(n == 0);

    delete index;

}


static const char * KEY_LOCALES[] = {
            "en", "es", "de", "fr", "ja", "it", "tr", "pt", "zh", "nl",
            "pl", "ar", "ru", "zh_Hant", "ko", "th", "sv", "fi", "da",
            "he", "nb", "el", "hr", "bg", "sk", "lt", "vi", "lv", "sr",
            "pt_PT", "ro", "hu", "cs", "id", "sl", "fil", "fa", "uk",
            "ca", "hi", "et", "eu", "is", "sw", "ms", "bn", "am", "ta",
            "te", "mr", "ur", "ml", "kn", "gu", "or", ""};


void AlphabeticIndexTest::ManyLocalesTest() {
    UErrorCode status = U_ZERO_ERROR;
    int32_t  lc = 0;
    AlphabeticIndex *index = NULL;

    for (int i=0; ; ++i) {
        status = U_ZERO_ERROR;
        const char *localeName = KEY_LOCALES[i];
        if (localeName[0] == 0) {
            break;
        }
        // std::cout <<  localeName << "  ";
        Locale loc = Locale::createFromName(localeName);
        index = new AlphabeticIndex(loc, status);
        TEST_CHECK_STATUS;
        lc = index->getBucketCount(status);
        TEST_CHECK_STATUS;
        // std::cout << "getBucketCount() == " << lc << std::endl;

        while (index->nextBucket(status)) {
            TEST_CHECK_STATUS;
            const UnicodeString &label = index->getBucketLabel();
            TEST_ASSERT(label.length()>0);
            // std::string ss;
            // std::cout << ":" << label.toUTF8String(ss);
        }
        // std::cout << ":" << std::endl;


        delete index;
    }
}


// Test data for Pinyin based indexes.
//  The Chinese characters should be distributed under latin labels in
//  an index.

static const char *pinyinTestData[] = { 
        "\\u0101", "\\u5416", "\\u58ba", //
        "b", "\\u516b", "\\u62d4", "\\u8500", //
        "c", "\\u5693", "\\u7938", "\\u9e7e", //
        "d", "\\u5491", "\\u8fcf", "\\u964a", //
        "\\u0113","\\u59b8", "\\u92e8", "\\u834b", //
        "f", "\\u53d1", "\\u9197", "\\u99a5", //
        "g", "\\u7324", "\\u91d3", "\\u8142", //
        "h", "\\u598e", "\\u927f", "\\u593b", //
        "j", "\\u4e0c", "\\u6785", "\\u9d58", //
        "k", "\\u5494", "\\u958b", "\\u7a52", //
        "l", "\\u5783", "\\u62c9", "\\u9ba5", //
        "m", "\\u5638", "\\u9ebb", "\\u65c0", //
        "n", "\\u62ff", "\\u80ad", "\\u685b", //
        "\\u014D", "\\u5662", "\\u6bee", "\\u8bb4", //
        "p", "\\u5991", "\\u8019", "\\u8c31", //
        "q", "\\u4e03", "\\u6053", "\\u7f56", //
        "r", "\\u5465", "\\u72aa", "\\u6e03", //
        "s", "\\u4ee8", "\\u9491", "\\u93c1", //
        "t", "\\u4ed6", "\\u9248", "\\u67dd", //
        "w", "\\u5c72", "\\u5558", "\\u5a7a", //
        "x", "\\u5915", "\\u5438", "\\u6bbe", //
        "y", "\\u4e2b", "\\u82bd", "\\u8574", //
        "z", "\\u5e00", "\\u707d", "\\u5c0a",
        NULL
    };

void AlphabeticIndexTest::HackPinyinTest() {
    UErrorCode status = U_ZERO_ERROR;
    AlphabeticIndex aindex(Locale::createFromName("zh"), status);
    TEST_CHECK_STATUS; 

    UnicodeString names[sizeof(pinyinTestData) / sizeof(pinyinTestData[0])];
    int32_t  nameCount;
    for (nameCount=0; pinyinTestData[nameCount] != NULL; nameCount++) {
        names[nameCount] = UnicodeString(pinyinTestData[nameCount], -1, UnicodeString::kInvariant).unescape();
        aindex.addRecord(names[nameCount], &names[nameCount], status);
        TEST_CHECK_STATUS; 
        if (U_FAILURE(status)) {
            return;
        }
    }
    TEST_ASSERT(nameCount == aindex.getRecordCount(status));
    
    // Weak checking:  make sure that none of the Chinese names landed in the overflow bucket
    //   of the index, and that the names are distributed among several buckets.
    //   (Exact expected data would be subject to change with evolution of the collation rules.)

    int32_t bucketCount = 0;
    int32_t filledBucketCount = 0;
    while (aindex.nextBucket(status)) {
        bucketCount++;
        UnicodeString label = aindex.getBucketLabel();
        // std::string s;
        // std::cout << label.toUTF8String(s) << ":  ";

        UBool  bucketHasContents = FALSE;
        while (aindex.nextRecord(status)) {
            bucketHasContents = TRUE;
            UnicodeString name = aindex.getRecordName();
            if (aindex.getBucketLabelType() != U_ALPHAINDEX_NORMAL) {
                errln("File %s, Line %d, Name \"\\u%x\" is in an under or overflow bucket.",
                    __FILE__, __LINE__, name.char32At(0));
            }
            // s.clear();
            // std::cout << aindex.getRecordName().toUTF8String(s) << " ";
        }
        if (bucketHasContents) {
            filledBucketCount++;
        }
        // std::cout << std::endl;
    }
    TEST_ASSERT(bucketCount > 25);
    TEST_ASSERT(filledBucketCount > 15);
}


void AlphabeticIndexTest::TestBug9009() {
    UErrorCode status = U_ZERO_ERROR;
    Locale loc("root");
    AlphabeticIndex aindex(loc, status);
    TEST_CHECK_STATUS; 
    aindex.nextBucket(status);  // Crash here before bug was fixed.
    TEST_CHECK_STATUS; 
}
    

#endif
