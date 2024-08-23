// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * Copyright (c) 1999-2016, International Business Machines
 * Corporation and others. All Rights Reserved.
 ********************************************************************
 *   Date        Name        Description
 *   12/14/99    Madhu        Creation.
 *   01/12/2000  Madhu        updated for changed API
 ********************************************************************/

#include "unicode/utypes.h"

#if !UCONFIG_NO_BREAK_ITERATION

#include "unicode/uchar.h"
#include "intltest.h"
#include "unicode/rbbi.h"
#include "unicode/schriter.h"
#include "rbbiapts.h"
#include "rbbidata.h"
#include "cstring.h"
#include "ubrkimpl.h"
#include "unicode/locid.h"
#include "unicode/ustring.h"
#include "unicode/utext.h"
#include "cmemory.h"
#if !UCONFIG_NO_BREAK_ITERATION
#include "unicode/filteredbrk.h"
#include <stdio.h> // for sprintf
#endif
/**
 * API Test the RuleBasedBreakIterator class
 */


#define TEST_ASSERT_SUCCESS(status) {if (U_FAILURE(status)) {\
dataerrln("Failure at file %s, line %d, error = %s", __FILE__, __LINE__, u_errorName(status));}}

#define TEST_ASSERT(expr) {if ((expr) == FALSE) { \
    errln("Test Failure at file %s, line %d: \"%s\" is false.\n", __FILE__, __LINE__, #expr);};}

void RBBIAPITest::TestCloneEquals()
{

    UErrorCode status=U_ZERO_ERROR;
    RuleBasedBreakIterator* bi1     = (RuleBasedBreakIterator*)RuleBasedBreakIterator::createCharacterInstance(Locale::getDefault(), status);
    RuleBasedBreakIterator* biequal = (RuleBasedBreakIterator*)RuleBasedBreakIterator::createCharacterInstance(Locale::getDefault(), status);
    RuleBasedBreakIterator* bi3     = (RuleBasedBreakIterator*)RuleBasedBreakIterator::createCharacterInstance(Locale::getDefault(), status);
    RuleBasedBreakIterator* bi2     = (RuleBasedBreakIterator*)RuleBasedBreakIterator::createWordInstance(Locale::getDefault(), status);
    if(U_FAILURE(status)){
        errcheckln(status, "Fail : in construction - %s", u_errorName(status));
        return;
    }


    UnicodeString testString="Testing word break iterators's clone() and equals()";
    bi1->setText(testString);
    bi2->setText(testString);
    biequal->setText(testString);

    bi3->setText("hello");

    logln((UnicodeString)"Testing equals()");

    logln((UnicodeString)"Testing == and !=");
    UBool b = (*bi1 != *biequal);
    b |= *bi1 == *bi2;
    b |= *bi1 == *bi3;
    if (b) {
        errln("%s:%d ERROR:1 RBBI's == and != operator failed.", __FILE__, __LINE__);
    }

    if(*bi2 == *biequal || *bi2 == *bi1  || *biequal == *bi3)
        errln("%s:%d ERROR:2 RBBI's == and != operator  failed.", __FILE__, __LINE__);


    // Quick test of RulesBasedBreakIterator assignment -
    // Check that
    //    two different iterators are !=
    //    they are == after assignment
    //    source and dest iterator produce the same next() after assignment.
    //    deleting one doesn't disable the other.
    logln("Testing assignment");
    RuleBasedBreakIterator *bix = (RuleBasedBreakIterator *)BreakIterator::createLineInstance(Locale::getDefault(), status);
    if(U_FAILURE(status)){
        errcheckln(status, "Fail : in construction - %s", u_errorName(status));
        return;
    }

    RuleBasedBreakIterator biDefault, biDefault2;
    if(U_FAILURE(status)){
        errln("%s:%d FAIL : in construction of default iterator", __FILE__, __LINE__);
        return;
    }
    if (biDefault == *bix) {
        errln("%s:%d ERROR: iterators should not compare ==", __FILE__, __LINE__);
        return;
    }
    if (biDefault != biDefault2) {
        errln("%s:%d ERROR: iterators should compare ==", __FILE__, __LINE__);
        return;
    }


    UnicodeString   HelloString("Hello Kitty");
    bix->setText(HelloString);
    if (*bix == *bi2) {
        errln("%s:%d ERROR: strings should not be equal before assignment.", __FILE__, __LINE__);
    }
    *bix = *bi2;
    if (*bix != *bi2) {
        errln("%s:%d ERROR: strings should be equal before assignment.", __FILE__, __LINE__);
    }

    int bixnext = bix->next();
    int bi2next = bi2->next();
    if (! (bixnext == bi2next && bixnext == 7)) {
        errln("%s:%d ERROR: iterators behaved differently after assignment.", __FILE__, __LINE__);
    }
    delete bix;
    if (bi2->next() != 8) {
        errln("%s:%d ERROR: iterator.next() failed after deleting copy.", __FILE__, __LINE__);
    }



    logln((UnicodeString)"Testing clone()");
    RuleBasedBreakIterator* bi1clone = dynamic_cast<RuleBasedBreakIterator *>(bi1->clone());
    RuleBasedBreakIterator* bi2clone = dynamic_cast<RuleBasedBreakIterator *>(bi2->clone());

    if(*bi1clone != *bi1 || *bi1clone  != *biequal  ||
      *bi1clone == *bi3 || *bi1clone == *bi2)
        errln("%s:%d ERROR:1 RBBI's clone() method failed", __FILE__, __LINE__);

    if(*bi2clone == *bi1 || *bi2clone == *biequal ||
       *bi2clone == *bi3 || *bi2clone != *bi2)
        errln("%s:%d ERROR:2 RBBI's clone() method failed", __FILE__, __LINE__);

    if(bi1->getText() != bi1clone->getText()   ||
       bi2clone->getText() != bi2->getText()   ||
       *bi2clone == *bi1clone )
        errln("%s:%d ERROR: RBBI's clone() method failed", __FILE__, __LINE__);

    delete bi1clone;
    delete bi2clone;
    delete bi1;
    delete bi3;
    delete bi2;
    delete biequal;
}

void RBBIAPITest::TestBoilerPlate()
{
    UErrorCode status = U_ZERO_ERROR;
    BreakIterator* a = BreakIterator::createWordInstance(Locale("hi"), status);
    BreakIterator* b = BreakIterator::createWordInstance(Locale("hi_IN"),status);
    if (U_FAILURE(status)) {
        errcheckln(status, "Creation of break iterator failed %s", u_errorName(status));
        return;
    }
    if(*a!=*b){
        errln("Failed: boilerplate method operator!= does not return correct results");
    }
    // Japanese word break iterators are identical to root with
    // a dictionary-based break iterator
    BreakIterator* c = BreakIterator::createCharacterInstance(Locale("ja"),status);
    BreakIterator* d = BreakIterator::createCharacterInstance(Locale("root"),status);
    if(c && d){
        if(*c!=*d){
            errln("Failed: boilerplate method operator== does not return correct results");
        }
    }else{
        errln("creation of break iterator failed");
    }
    delete a;
    delete b;
    delete c;
    delete d;
}

void RBBIAPITest::TestgetRules()
{
    UErrorCode status=U_ZERO_ERROR;

    LocalPointer<RuleBasedBreakIterator> bi1(
            (RuleBasedBreakIterator*)RuleBasedBreakIterator::createCharacterInstance(Locale::getDefault(), status), status);
    LocalPointer<RuleBasedBreakIterator> bi2(
            (RuleBasedBreakIterator*)RuleBasedBreakIterator::createWordInstance(Locale::getDefault(), status), status);
    if(U_FAILURE(status)){
        errcheckln(status, "%s:%d, FAIL: in construction - %s", __FILE__, __LINE__, u_errorName(status));
        return;
    }

    logln((UnicodeString)"Testing getRules()");

    UnicodeString text(u"Hello there");
    bi1->setText(text);

    LocalPointer <RuleBasedBreakIterator> bi3((RuleBasedBreakIterator*)bi1->clone());

    UnicodeString temp=bi1->getRules();
    UnicodeString temp2=bi2->getRules();
    UnicodeString temp3=bi3->getRules();
    if( temp2.compare(temp3) ==0 || temp.compare(temp2) == 0 || temp.compare(temp3) != 0)
        errln("%s:%d ERROR: error in getRules() method", __FILE__, __LINE__);

    RuleBasedBreakIterator bi4;   // Default RuleBasedBreakIterator constructor gives empty shell with empty rules.
    if (!bi4.getRules().isEmpty()) {
        errln("%s:%d Empty string expected.", __FILE__, __LINE__);
    }
}

void RBBIAPITest::TestHashCode()
{
    UErrorCode status=U_ZERO_ERROR;
    RuleBasedBreakIterator* bi1     = (RuleBasedBreakIterator*)RuleBasedBreakIterator::createCharacterInstance(Locale::getDefault(), status);
    RuleBasedBreakIterator* bi3     = (RuleBasedBreakIterator*)RuleBasedBreakIterator::createCharacterInstance(Locale::getDefault(), status);
    RuleBasedBreakIterator* bi2     = (RuleBasedBreakIterator*)RuleBasedBreakIterator::createWordInstance(Locale::getDefault(), status);
    if(U_FAILURE(status)){
        errcheckln(status, "Fail : in construction - %s", u_errorName(status));
        delete bi1;
        delete bi2;
        delete bi3;
        return;
    }


    logln((UnicodeString)"Testing hashCode()");

    bi1->setText((UnicodeString)"Hash code");
    bi2->setText((UnicodeString)"Hash code");
    bi3->setText((UnicodeString)"Hash code");

    RuleBasedBreakIterator* bi1clone= (RuleBasedBreakIterator*)bi1->clone();
    RuleBasedBreakIterator* bi2clone= (RuleBasedBreakIterator*)bi2->clone();

    if(bi1->hashCode() != bi1clone->hashCode() ||  bi1->hashCode() != bi3->hashCode() ||
        bi1clone->hashCode() != bi3->hashCode() || bi2->hashCode() != bi2clone->hashCode())
        errln((UnicodeString)"ERROR: identical objects have different hashcodes");

    if(bi1->hashCode() == bi2->hashCode() ||  bi2->hashCode() == bi3->hashCode() ||
        bi1clone->hashCode() == bi2clone->hashCode() || bi1clone->hashCode() == bi2->hashCode())
        errln((UnicodeString)"ERROR: different objects have same hashcodes");

    delete bi1clone;
    delete bi2clone;
    delete bi1;
    delete bi2;
    delete bi3;

}
void RBBIAPITest::TestGetSetAdoptText()
{
    logln((UnicodeString)"Testing getText setText ");
    IcuTestErrorCode status(*this, "TestGetSetAdoptText");
    UnicodeString str1="first string.";
    UnicodeString str2="Second string.";
    LocalPointer<RuleBasedBreakIterator> charIter1((RuleBasedBreakIterator*)RuleBasedBreakIterator::createCharacterInstance(Locale::getDefault(), status));
    LocalPointer<RuleBasedBreakIterator> wordIter1((RuleBasedBreakIterator*)RuleBasedBreakIterator::createWordInstance(Locale::getDefault(), status));
    if(status.isFailure()){
        errcheckln(status, "Fail : in construction - %s", status.errorName());
            return;
    }


    CharacterIterator* text1= new StringCharacterIterator(str1);
    CharacterIterator* text1Clone = text1->clone();
    CharacterIterator* text2= new StringCharacterIterator(str2);
    CharacterIterator* text3= new StringCharacterIterator(str2, 3, 10, 3); //  "ond str"

    wordIter1->setText(str1);
    CharacterIterator *tci = &wordIter1->getText();
    UnicodeString      tstr;
    tci->getText(tstr);
    TEST_ASSERT(tstr == str1);
    if(wordIter1->current() != 0)
        errln((UnicodeString)"ERROR:1 setText did not set the iteration position to the beginning of the text, it is" + wordIter1->current() + (UnicodeString)"\n");

    wordIter1->next(2);

    wordIter1->setText(str2);
    if(wordIter1->current() != 0)
        errln((UnicodeString)"ERROR:2 setText did not reset the iteration position to the beginning of the text, it is" + wordIter1->current() + (UnicodeString)"\n");


    charIter1->adoptText(text1Clone);
    TEST_ASSERT(wordIter1->getText() != charIter1->getText());
    tci = &wordIter1->getText();
    tci->getText(tstr);
    TEST_ASSERT(tstr == str2);
    tci = &charIter1->getText();
    tci->getText(tstr);
    TEST_ASSERT(tstr == str1);


    LocalPointer<RuleBasedBreakIterator> rb((RuleBasedBreakIterator*)wordIter1->clone());
    rb->adoptText(text1);
    if(rb->getText() != *text1)
        errln((UnicodeString)"ERROR:1 error in adoptText ");
    rb->adoptText(text2);
    if(rb->getText() != *text2)
        errln((UnicodeString)"ERROR:2 error in adoptText ");

    // Adopt where iterator range is less than the entire orignal source string.
    //   (With the change of the break engine to working with UText internally,
    //    CharacterIterators starting at positions other than zero are not supported)
    rb->adoptText(text3);
    TEST_ASSERT(rb->preceding(2) == 0);
    TEST_ASSERT(rb->following(11) == BreakIterator::DONE);
    //if(rb->preceding(2) != 3) {
    //    errln((UnicodeString)"ERROR:3 error in adoptText ");
    //}
    //if(rb->following(11) != BreakIterator::DONE) {
    //    errln((UnicodeString)"ERROR:4 error in adoptText ");
    //}

    // UText API
    //
    //   Quick test to see if UText is working at all.
    //
    const char *s1 = "\x68\x65\x6C\x6C\x6F\x20\x77\x6F\x72\x6C\x64"; /* "hello world" in UTF-8 */
    const char *s2 = "\x73\x65\x65\x20\x79\x61"; /* "see ya" in UTF-8 */
    //                012345678901

    status.reset();
    LocalUTextPointer ut(utext_openUTF8(NULL, s1, -1, status));
    wordIter1->setText(ut.getAlias(), status);
    TEST_ASSERT_SUCCESS(status);

    int32_t pos;
    pos = wordIter1->first();
    TEST_ASSERT(pos==0);
    pos = wordIter1->next();
    TEST_ASSERT(pos==5);
    pos = wordIter1->next();
    TEST_ASSERT(pos==6);
    pos = wordIter1->next();
    TEST_ASSERT(pos==11);
    pos = wordIter1->next();
    TEST_ASSERT(pos==UBRK_DONE);

    status.reset();
    LocalUTextPointer ut2(utext_openUTF8(NULL, s2, -1, status));
    TEST_ASSERT_SUCCESS(status);
    wordIter1->setText(ut2.getAlias(), status);
    TEST_ASSERT_SUCCESS(status);

    pos = wordIter1->first();
    TEST_ASSERT(pos==0);
    pos = wordIter1->next();
    TEST_ASSERT(pos==3);
    pos = wordIter1->next();
    TEST_ASSERT(pos==4);

    pos = wordIter1->last();
    TEST_ASSERT(pos==6);
    pos = wordIter1->previous();
    TEST_ASSERT(pos==4);
    pos = wordIter1->previous();
    TEST_ASSERT(pos==3);
    pos = wordIter1->previous();
    TEST_ASSERT(pos==0);
    pos = wordIter1->previous();
    TEST_ASSERT(pos==UBRK_DONE);

    status.reset();
    UnicodeString sEmpty;
    LocalUTextPointer gut2(utext_openUnicodeString(NULL, &sEmpty, status));
    wordIter1->getUText(gut2.getAlias(), status);
    TEST_ASSERT_SUCCESS(status);
    status.reset();
}


void RBBIAPITest::TestIteration()
{
    // This test just verifies that the API is present.
    // Testing for correct operation of the break rules happens elsewhere.

    UErrorCode status=U_ZERO_ERROR;
    RuleBasedBreakIterator* bi  = (RuleBasedBreakIterator*)RuleBasedBreakIterator::createCharacterInstance(Locale::getDefault(), status);
    if (U_FAILURE(status) || bi == NULL)  {
        errcheckln(status, "Failure creating character break iterator.  Status = %s", u_errorName(status));
    }
    delete bi;

    status=U_ZERO_ERROR;
    bi  = (RuleBasedBreakIterator*)RuleBasedBreakIterator::createWordInstance(Locale::getDefault(), status);
    if (U_FAILURE(status) || bi == NULL)  {
        errcheckln(status, "Failure creating Word break iterator.  Status = %s", u_errorName(status));
    }
    delete bi;

    status=U_ZERO_ERROR;
    bi  = (RuleBasedBreakIterator*)RuleBasedBreakIterator::createLineInstance(Locale::getDefault(), status);
    if (U_FAILURE(status) || bi == NULL)  {
        errcheckln(status, "Failure creating Line break iterator.  Status = %s", u_errorName(status));
    }
    delete bi;

    status=U_ZERO_ERROR;
    bi  = (RuleBasedBreakIterator*)RuleBasedBreakIterator::createSentenceInstance(Locale::getDefault(), status);
    if (U_FAILURE(status) || bi == NULL)  {
        errcheckln(status, "Failure creating Sentence break iterator.  Status = %s", u_errorName(status));
    }
    delete bi;

    status=U_ZERO_ERROR;
    bi  = (RuleBasedBreakIterator*)RuleBasedBreakIterator::createTitleInstance(Locale::getDefault(), status);
    if (U_FAILURE(status) || bi == NULL)  {
        errcheckln(status, "Failure creating Title break iterator.  Status = %s", u_errorName(status));
    }
    delete bi;

    status=U_ZERO_ERROR;
    bi  = (RuleBasedBreakIterator*)RuleBasedBreakIterator::createCharacterInstance(Locale::getDefault(), status);
    if (U_FAILURE(status) || bi == NULL)  {
        errcheckln(status, "Failure creating character break iterator.  Status = %s", u_errorName(status));
        return;   // Skip the rest of these tests.
    }


    UnicodeString testString="0123456789";
    bi->setText(testString);

    int32_t i;
    i = bi->first();
    if (i != 0) {
        errln("%s:%d Incorrect value from bi->first().  Expected 0, got %d.", __FILE__, __LINE__, i);
    }

    i = bi->last();
    if (i != 10) {
        errln("%s:%d Incorrect value from bi->last().  Expected 10, got %d", __FILE__, __LINE__, i);
    }

    //
    // Previous
    //
    bi->last();
    i = bi->previous();
    if (i != 9) {
        errln("%s:%d Incorrect value from bi->last().  Expected 9, got %d", __FILE__, __LINE__, i);
    }


    bi->first();
    i = bi->previous();
    if (i != BreakIterator::DONE) {
        errln("%s:%d Incorrect value from bi->previous().  Expected DONE, got %d", __FILE__, __LINE__, i);
    }

    //
    // next()
    //
    bi->first();
    i = bi->next();
    if (i != 1) {
        errln("%s:%d Incorrect value from bi->next().  Expected 1, got %d", __FILE__, __LINE__, i);
    }

    bi->last();
    i = bi->next();
    if (i != BreakIterator::DONE) {
        errln("%s:%d Incorrect value from bi->next().  Expected DONE, got %d", __FILE__, __LINE__, i);
    }


    //
    //  current()
    //
    bi->first();
    i = bi->current();
    if (i != 0) {
        errln("%s:%d Incorrect value from bi->current().  Expected 0, got %d", __FILE__, __LINE__, i);
    }

    bi->next();
    i = bi->current();
    if (i != 1) {
        errln("%s:%d Incorrect value from bi->current().  Expected 1, got %d", __FILE__, __LINE__, i);
    }

    bi->last();
    bi->next();
    i = bi->current();
    if (i != 10) {
        errln("%s:%d Incorrect value from bi->current().  Expected 10, got %d", __FILE__, __LINE__, i);
    }

    bi->first();
    bi->previous();
    i = bi->current();
    if (i != 0) {
        errln("%s:%d Incorrect value from bi->current().  Expected 0, got %d", __FILE__, __LINE__, i);
    }


    //
    // Following()
    //
    i = bi->following(4);
    if (i != 5) {
        errln("%s:%d Incorrect value from bi->following().  Expected 5, got %d", __FILE__, __LINE__, i);
    }

    i = bi->following(9);
    if (i != 10) {
        errln("%s:%d Incorrect value from bi->following().  Expected 10, got %d", __FILE__, __LINE__, i);
    }

    i = bi->following(10);
    if (i != BreakIterator::DONE) {
        errln("%s:%d Incorrect value from bi->following().  Expected DONE, got %d", __FILE__, __LINE__, i);
    }


    //
    // Preceding
    //
    i = bi->preceding(4);
    if (i != 3) {
        errln("%s:%d Incorrect value from bi->preceding().  Expected 3, got %d", __FILE__, __LINE__, i);
    }

    i = bi->preceding(10);
    if (i != 9) {
        errln("%s:%d Incorrect value from bi->preceding().  Expected 9, got %d", __FILE__, __LINE__, i);
    }

    i = bi->preceding(1);
    if (i != 0) {
        errln("%s:%d Incorrect value from bi->preceding().  Expected 0, got %d", __FILE__, __LINE__, i);
    }

    i = bi->preceding(0);
    if (i != BreakIterator::DONE) {
        errln("%s:%d Incorrect value from bi->preceding().  Expected DONE, got %d", __FILE__, __LINE__, i);
    }


    //
    // isBoundary()
    //
    bi->first();
    if (bi->isBoundary(3) != TRUE) {
        errln("%s:%d Incorrect value from bi->isBoudary().  Expected TRUE, got FALSE", __FILE__, __LINE__, i);
    }
    i = bi->current();
    if (i != 3) {
        errln("%s:%d Incorrect value from bi->current().  Expected 3, got %d", __FILE__, __LINE__, i);
    }


    if (bi->isBoundary(11) != FALSE) {
        errln("%s:%d Incorrect value from bi->isBoudary().  Expected FALSE, got TRUE", __FILE__, __LINE__, i);
    }
    i = bi->current();
    if (i != 10) {
        errln("%s:%d Incorrect value from bi->current().  Expected 10, got %d", __FILE__, __LINE__, i);
    }

    //
    // next(n)
    //
    bi->first();
    i = bi->next(4);
    if (i != 4) {
        errln("%s:%d Incorrect value from bi->next().  Expected 4, got %d", __FILE__, __LINE__, i);
    }

    i = bi->next(6);
    if (i != 10) {
        errln("%s:%d Incorrect value from bi->next().  Expected 10, got %d", __FILE__, __LINE__, i);
    }

    bi->first();
    i = bi->next(11);
    if (i != BreakIterator::DONE) {
        errln("%s:%d Incorrect value from bi->next().  Expected BreakIterator::DONE, got %d", __FILE__, __LINE__, i);
    }

    delete bi;

}






void RBBIAPITest::TestBuilder() {
     UnicodeString rulesString1 = "$Letters = [:L:];\n"
                                  "$Numbers = [:N:];\n"
                                  "$Letters+;\n"
                                  "$Numbers+;\n"
                                  "[^$Letters $Numbers];\n"
                                  "!.*;\n";
     UnicodeString testString1  = "abc123..abc";
                                // 01234567890
     int32_t bounds1[] = {0, 3, 6, 7, 8, 11};
     UErrorCode status=U_ZERO_ERROR;
     UParseError    parseError;

     RuleBasedBreakIterator *bi = new RuleBasedBreakIterator(rulesString1, parseError, status);
     if(U_FAILURE(status)) {
         dataerrln("Fail : in construction - %s", u_errorName(status));
     } else {
         bi->setText(testString1);
         doBoundaryTest(*bi, testString1, bounds1);
     }
     delete bi;
}


//
//  TestQuoteGrouping
//       Single quotes within rules imply a grouping, so that a modifier
//       following the quoted text (* or +) applies to all of the quoted chars.
//
void RBBIAPITest::TestQuoteGrouping() {
     UnicodeString rulesString1 = "#Here comes the rule...\n"
                                  "'$@!'*;\n"   //  (\$\@\!)*
                                  ".;\n";

     UnicodeString testString1  = "$@!$@!X$@!!X";
                                // 0123456789012
     int32_t bounds1[] = {0, 6, 7, 10, 11, 12};
     UErrorCode status=U_ZERO_ERROR;
     UParseError    parseError;

     RuleBasedBreakIterator *bi = new RuleBasedBreakIterator(rulesString1, parseError, status);
     if(U_FAILURE(status)) {
         dataerrln("Fail : in construction - %s", u_errorName(status));
     } else {
         bi->setText(testString1);
         doBoundaryTest(*bi, testString1, bounds1);
     }
     delete bi;
}

//
//  TestRuleStatus
//      Test word break rule status constants.
//
void RBBIAPITest::TestRuleStatus() {
     UChar str[30];
     //no longer test Han or hiragana breaking here: ruleStatusVec would return nothing
     // changed UBRK_WORD_KANA to UBRK_WORD_IDEO
     u_unescape("plain word 123.45 \\u30a1\\u30a2 ",
              // 012345678901234567  8      9    0
              //                     Katakana
                str, 30);
     UnicodeString testString1(str);
     int32_t bounds1[] = {0, 5, 6, 10, 11, 17, 18, 20, 21};
     int32_t tag_lo[]  = {UBRK_WORD_NONE,     UBRK_WORD_LETTER, UBRK_WORD_NONE,    UBRK_WORD_LETTER,
                          UBRK_WORD_NONE,     UBRK_WORD_NUMBER, UBRK_WORD_NONE,
                          UBRK_WORD_IDEO,     UBRK_WORD_NONE};

     int32_t tag_hi[]  = {UBRK_WORD_NONE_LIMIT, UBRK_WORD_LETTER_LIMIT, UBRK_WORD_NONE_LIMIT, UBRK_WORD_LETTER_LIMIT,
                          UBRK_WORD_NONE_LIMIT, UBRK_WORD_NUMBER_LIMIT, UBRK_WORD_NONE_LIMIT,
                          UBRK_WORD_IDEO_LIMIT, UBRK_WORD_NONE_LIMIT};

     UErrorCode status=U_ZERO_ERROR;

     BreakIterator *bi = BreakIterator::createWordInstance(Locale::getEnglish(), status);
     if(U_FAILURE(status)) {
         errcheckln(status, "%s:%d Fail in construction - %s", __FILE__, __LINE__, u_errorName(status));
     } else {
         bi->setText(testString1);
         // First test that the breaks are in the right spots.
         doBoundaryTest(*bi, testString1, bounds1);

         // Then go back and check tag values
         int32_t i = 0;
         int32_t pos, tag;
         for (pos = bi->first(); pos != BreakIterator::DONE; pos = bi->next(), i++) {
             if (pos != bounds1[i]) {
                 errln("%s:%d FAIL: unexpected word break at postion %d", __FILE__, __LINE__, pos);
                 break;
             }
             tag = bi->getRuleStatus();
             if (tag < tag_lo[i] || tag >= tag_hi[i]) {
                 errln("%s:%d FAIL: incorrect tag value %d at position %d", __FILE__, __LINE__, tag, pos);
                 break;
             }

             // Check that we get the same tag values from getRuleStatusVec()
             int32_t vec[10];
             int t = bi->getRuleStatusVec(vec, 10, status);
             TEST_ASSERT_SUCCESS(status);
             TEST_ASSERT(t==1);
             TEST_ASSERT(vec[0] == tag);
         }
     }
     delete bi;

     // Now test line break status.  This test mostly is to confirm that the status constants
     //                              are correctly declared in the header.
     testString1 =   "test line. \n";
     // break type    s    s     h

     bi = BreakIterator::createLineInstance(Locale::getEnglish(), status);
     if(U_FAILURE(status)) {
         errcheckln(status, "%s:%d failed to create line break iterator. - %s", __FILE__, __LINE__, u_errorName(status));
     } else {
         int32_t i = 0;
         int32_t pos, tag;
         UBool   success;

         bi->setText(testString1);
         pos = bi->current();
         tag = bi->getRuleStatus();
         for (i=0; i<3; i++) {
             switch (i) {
             case 0:
                 success = pos==0  && tag==UBRK_LINE_SOFT; break;
             case 1:
                 success = pos==5  && tag==UBRK_LINE_SOFT; break;
             case 2:
                 success = pos==12 && tag==UBRK_LINE_HARD; break;
             default:
                 success = FALSE; break;
             }
             if (success == FALSE) {
                 errln("%s:%d: incorrect line break status or position.  i=%d, pos=%d, tag=%d",
                     __FILE__, __LINE__, i, pos, tag);
                 break;
             }
             pos = bi->next();
             tag = bi->getRuleStatus();
         }
         if (UBRK_LINE_SOFT >= UBRK_LINE_SOFT_LIMIT ||
             UBRK_LINE_HARD >= UBRK_LINE_HARD_LIMIT ||
             (UBRK_LINE_HARD > UBRK_LINE_SOFT && UBRK_LINE_HARD < UBRK_LINE_SOFT_LIMIT)) {
             errln("%s:%d UBRK_LINE_* constants from header are inconsistent.", __FILE__, __LINE__);
         }
     }
     delete bi;

}


//
//  TestRuleStatusVec
//      Test the vector form of  break rule status.
//
void RBBIAPITest::TestRuleStatusVec() {
    UnicodeString rulesString(   "[A-N]{100}; \n"
                                 "[a-w]{200}; \n"
                                 "[\\p{L}]{300}; \n"
                                 "[\\p{N}]{400}; \n"
                                 "[0-5]{500}; \n"
                                  "!.*;\n", -1, US_INV);
     UnicodeString testString1  = "Aapz5?";
     int32_t  statusVals[10];
     int32_t  numStatuses;
     int32_t  pos;

     UErrorCode status=U_ZERO_ERROR;
     UParseError    parseError;

     RuleBasedBreakIterator *bi = new RuleBasedBreakIterator(rulesString, parseError, status);
     if (U_FAILURE(status)) {
         dataerrln("Failure at file %s, line %d, error = %s", __FILE__, __LINE__, u_errorName(status));
     } else {
         bi->setText(testString1);

         // A
         pos = bi->next();
         TEST_ASSERT(pos==1);
         numStatuses = bi->getRuleStatusVec(statusVals, 10, status);
         TEST_ASSERT_SUCCESS(status);
         TEST_ASSERT(numStatuses == 2);
         TEST_ASSERT(statusVals[0] == 100);
         TEST_ASSERT(statusVals[1] == 300);

         // a
         pos = bi->next();
         TEST_ASSERT(pos==2);
         numStatuses = bi->getRuleStatusVec(statusVals, 10, status);
         TEST_ASSERT_SUCCESS(status);
         TEST_ASSERT(numStatuses == 2);
         TEST_ASSERT(statusVals[0] == 200);
         TEST_ASSERT(statusVals[1] == 300);

         // p
         pos = bi->next();
         TEST_ASSERT(pos==3);
         numStatuses = bi->getRuleStatusVec(statusVals, 10, status);
         TEST_ASSERT_SUCCESS(status);
         TEST_ASSERT(numStatuses == 2);
         TEST_ASSERT(statusVals[0] == 200);
         TEST_ASSERT(statusVals[1] == 300);

         // z
         pos = bi->next();
         TEST_ASSERT(pos==4);
         numStatuses = bi->getRuleStatusVec(statusVals, 10, status);
         TEST_ASSERT_SUCCESS(status);
         TEST_ASSERT(numStatuses == 1);
         TEST_ASSERT(statusVals[0] == 300);

         // 5
         pos = bi->next();
         TEST_ASSERT(pos==5);
         numStatuses = bi->getRuleStatusVec(statusVals, 10, status);
         TEST_ASSERT_SUCCESS(status);
         TEST_ASSERT(numStatuses == 2);
         TEST_ASSERT(statusVals[0] == 400);
         TEST_ASSERT(statusVals[1] == 500);

         // ?
         pos = bi->next();
         TEST_ASSERT(pos==6);
         numStatuses = bi->getRuleStatusVec(statusVals, 10, status);
         TEST_ASSERT_SUCCESS(status);
         TEST_ASSERT(numStatuses == 1);
         TEST_ASSERT(statusVals[0] == 0);

         //
         //  Check buffer overflow error handling.   Char == A
         //
         bi->first();
         pos = bi->next();
         TEST_ASSERT(pos==1);
         memset(statusVals, -1, sizeof(statusVals));
         numStatuses = bi->getRuleStatusVec(statusVals, 0, status);
         TEST_ASSERT(status == U_BUFFER_OVERFLOW_ERROR);
         TEST_ASSERT(numStatuses == 2);
         TEST_ASSERT(statusVals[0] == -1);

         status = U_ZERO_ERROR;
         memset(statusVals, -1, sizeof(statusVals));
         numStatuses = bi->getRuleStatusVec(statusVals, 1, status);
         TEST_ASSERT(status == U_BUFFER_OVERFLOW_ERROR);
         TEST_ASSERT(numStatuses == 2);
         TEST_ASSERT(statusVals[0] == 100);
         TEST_ASSERT(statusVals[1] == -1);

         status = U_ZERO_ERROR;
         memset(statusVals, -1, sizeof(statusVals));
         numStatuses = bi->getRuleStatusVec(statusVals, 2, status);
         TEST_ASSERT_SUCCESS(status);
         TEST_ASSERT(numStatuses == 2);
         TEST_ASSERT(statusVals[0] == 100);
         TEST_ASSERT(statusVals[1] == 300);
         TEST_ASSERT(statusVals[2] == -1);
     }
     delete bi;

}

//
//   Bug 2190 Regression test.   Builder crash on rule consisting of only a
//                               $variable reference
void RBBIAPITest::TestBug2190() {
     UnicodeString rulesString1 = "$aaa = abcd;\n"
                                  "$bbb = $aaa;\n"
                                  "$bbb;\n";
     UnicodeString testString1  = "abcdabcd";
                                // 01234567890
     int32_t bounds1[] = {0, 4, 8};
     UErrorCode status=U_ZERO_ERROR;
     UParseError    parseError;

     RuleBasedBreakIterator *bi = new RuleBasedBreakIterator(rulesString1, parseError, status);
     if(U_FAILURE(status)) {
         dataerrln("Fail : in construction - %s", u_errorName(status));
     } else {
         bi->setText(testString1);
         doBoundaryTest(*bi, testString1, bounds1);
     }
     delete bi;
}


void RBBIAPITest::TestRegistration() {
#if !UCONFIG_NO_SERVICE
    UErrorCode status = U_ZERO_ERROR;
    BreakIterator* ja_word = BreakIterator::createWordInstance("ja_JP", status);
    // ok to not delete these if we exit because of error?
    BreakIterator* ja_char = BreakIterator::createCharacterInstance("ja_JP", status);
    BreakIterator* root_word = BreakIterator::createWordInstance("", status);
    BreakIterator* root_char = BreakIterator::createCharacterInstance("", status);

    if (status == U_MISSING_RESOURCE_ERROR || status == U_FILE_ACCESS_ERROR) {
        dataerrln("Error creating instances of break interactors - %s", u_errorName(status));

        delete ja_word;
        delete ja_char;
        delete root_word;
        delete root_char;

        return;
    }

    URegistryKey key = BreakIterator::registerInstance(ja_word, "xx", UBRK_WORD, status);
    {
#if 0 // With a dictionary based word breaking, ja_word is identical to root.
        if (ja_word && *ja_word == *root_word) {
            errln("japan not different from root");
        }
#endif
    }

    {
        BreakIterator* result = BreakIterator::createWordInstance("xx_XX", status);
        UBool fail = TRUE;
        if(result){
            fail = *result != *ja_word;
        }
        delete result;
        if (fail) {
            errln("bad result for xx_XX/word");
        }
    }

    {
        BreakIterator* result = BreakIterator::createCharacterInstance("ja_JP", status);
        UBool fail = TRUE;
        if(result){
            fail = *result != *ja_char;
        }
        delete result;
        if (fail) {
            errln("bad result for ja_JP/char");
        }
    }

    {
        BreakIterator* result = BreakIterator::createCharacterInstance("xx_XX", status);
        UBool fail = TRUE;
        if(result){
            fail = *result != *root_char;
        }
        delete result;
        if (fail) {
            errln("bad result for xx_XX/char");
        }
    }

    {
        StringEnumeration* avail = BreakIterator::getAvailableLocales();
        UBool found = FALSE;
        const UnicodeString* p;
        while ((p = avail->snext(status))) {
            if (p->compare("xx") == 0) {
                found = TRUE;
                break;
            }
        }
        delete avail;
        if (!found) {
            errln("did not find test locale");
        }
    }

    {
        UBool unreg = BreakIterator::unregister(key, status);
        if (!unreg) {
            errln("unable to unregister");
        }
    }

    {
        BreakIterator* result = BreakIterator::createWordInstance("en_US", status);
        BreakIterator* root = BreakIterator::createWordInstance("", status);
        UBool fail = TRUE;
        if(root){
          fail = *root != *result;
        }
        delete root;
        delete result;
        if (fail) {
            errln("did not get root break");
        }
    }

    {
        StringEnumeration* avail = BreakIterator::getAvailableLocales();
        UBool found = FALSE;
        const UnicodeString* p;
        while ((p = avail->snext(status))) {
            if (p->compare("xx") == 0) {
                found = TRUE;
                break;
            }
        }
        delete avail;
        if (found) {
            errln("found test locale");
        }
    }

    {
        int32_t count;
        UBool   foundLocale = FALSE;
        const Locale *avail = BreakIterator::getAvailableLocales(count);
        for (int i=0; i<count; i++) {
            if (avail[i] == Locale::getEnglish()) {
                foundLocale = TRUE;
                break;
            }
        }
        if (foundLocale == FALSE) {
            errln("BreakIterator::getAvailableLocales(&count), failed to find EN.");
        }
    }


    // ja_word was adopted by factory
    delete ja_char;
    delete root_word;
    delete root_char;
#endif
}

void RBBIAPITest::RoundtripRule(const char *dataFile) {
    UErrorCode status = U_ZERO_ERROR;
    UParseError parseError;
    parseError.line = 0;
    parseError.offset = 0;
    LocalUDataMemoryPointer data(udata_open(U_ICUDATA_BRKITR, "brk", dataFile, &status));
    uint32_t length;
    const UChar *builtSource;
    const uint8_t *rbbiRules;
    const uint8_t *builtRules;

    if (U_FAILURE(status)) {
        errcheckln(status, "%s:%d Can't open \"%s\" - %s", __FILE__, __LINE__, dataFile, u_errorName(status));
        return;
    }

    builtRules = (const uint8_t *)udata_getMemory(data.getAlias());
    builtSource = (const UChar *)(builtRules + ((RBBIDataHeader*)builtRules)->fRuleSource);
    LocalPointer<RuleBasedBreakIterator> brkItr (new RuleBasedBreakIterator(builtSource, parseError, status));
    if (U_FAILURE(status)) {
        errln("%s:%d createRuleBasedBreakIterator: ICU Error \"%s\"  at line %d, column %d\n",
                __FILE__, __LINE__, u_errorName(status), parseError.line, parseError.offset);
        errln(UnicodeString(builtSource));
        return;
    };
    rbbiRules = brkItr->getBinaryRules(length);
    logln("Comparing \"%s\" len=%d", dataFile, length);
    if (memcmp(builtRules, rbbiRules, (int32_t)length) != 0) {
        errln("%s:%d Built rules and rebuilt rules are different %s", __FILE__, __LINE__, dataFile);
        return;
    }
}

void RBBIAPITest::TestRoundtripRules() {
    RoundtripRule("word");
    RoundtripRule("title");
    RoundtripRule("sent");
    RoundtripRule("line");
    RoundtripRule("char");
    if (!quick) {
        RoundtripRule("word_POSIX");
    }
}


// Check getBinaryRules() and construction of a break iterator from those rules.

void RBBIAPITest::TestGetBinaryRules() {
    UErrorCode status=U_ZERO_ERROR;
    LocalPointer<BreakIterator> bi(BreakIterator::createLineInstance(Locale::getEnglish(), status));
    if (U_FAILURE(status)) {
        dataerrln("FAIL: BreakIterator::createLineInstance for Locale::getEnglish(): %s", u_errorName(status));
        return;
    }
    RuleBasedBreakIterator *rbbi = dynamic_cast<RuleBasedBreakIterator *>(bi.getAlias());
    if (rbbi == NULL) {
        dataerrln("FAIL: RuleBasedBreakIterator is NULL");
        return;
    }

    // Check that the new line break iterator is nominally functional.
    UnicodeString helloWorld("Hello, World!");
    rbbi->setText(helloWorld);
    int n = 0;
    while (bi->next() != UBRK_DONE) {
        ++n;
    }
    TEST_ASSERT(n == 2);

    // Extract the binary rules as a uint8_t blob.
    uint32_t ruleLength;
    const uint8_t *binRules = rbbi->getBinaryRules(ruleLength);
    TEST_ASSERT(ruleLength > 0);
    TEST_ASSERT(binRules != NULL);

    // Clone the binary rules, and create a break iterator from that.
    // The break iterator does not adopt the rules; we must delete when we are finished with the iterator.
    uint8_t *clonedRules = new uint8_t[ruleLength];
    memcpy(clonedRules, binRules, ruleLength);
    RuleBasedBreakIterator clonedBI(clonedRules, ruleLength, status);
    TEST_ASSERT_SUCCESS(status);
    
    // Check that the cloned line break iterator is nominally alive.
    clonedBI.setText(helloWorld);
    n = 0;
    while (clonedBI.next() != UBRK_DONE) {
        ++n;
    }
    TEST_ASSERT(n == 2);

    delete[] clonedRules;
}


void RBBIAPITest::TestRefreshInputText() {
    /*
     *  RefreshInput changes out the input of a Break Iterator without
     *    changing anything else in the iterator's state.  Used with Java JNI,
     *    when Java moves the underlying string storage.   This test
     *    runs BreakIterator::next() repeatedly, moving the text in the middle of the sequence.
     *    The right set of boundaries should still be found.
     */
    UChar testStr[]  = {0x20, 0x41, 0x20, 0x42, 0x20, 0x43, 0x20, 0x44, 0x0};  /* = " A B C D"  */
    UChar movedStr[] = {0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,  0};
    UErrorCode status = U_ZERO_ERROR;
    UText ut1 = UTEXT_INITIALIZER;
    UText ut2 = UTEXT_INITIALIZER;
    RuleBasedBreakIterator *bi = (RuleBasedBreakIterator *)BreakIterator::createLineInstance(Locale::getEnglish(), status);
    TEST_ASSERT_SUCCESS(status);

    utext_openUChars(&ut1, testStr, -1, &status);
    TEST_ASSERT_SUCCESS(status);

    if (U_SUCCESS(status)) {
        bi->setText(&ut1, status);
        TEST_ASSERT_SUCCESS(status);

        /* Line boundaries will occur before each letter in the original string */
        TEST_ASSERT(1 == bi->next());
        TEST_ASSERT(3 == bi->next());

        /* Move the string, kill the original string.  */
        u_strcpy(movedStr, testStr);
        u_memset(testStr, 0x20, u_strlen(testStr));
        utext_openUChars(&ut2, movedStr, -1, &status);
        TEST_ASSERT_SUCCESS(status);
        RuleBasedBreakIterator *returnedBI = &bi->refreshInputText(&ut2, status);
        TEST_ASSERT_SUCCESS(status);
        TEST_ASSERT(bi == returnedBI);

        /* Find the following matches, now working in the moved string. */
        TEST_ASSERT(5 == bi->next());
        TEST_ASSERT(7 == bi->next());
        TEST_ASSERT(8 == bi->next());
        TEST_ASSERT(UBRK_DONE == bi->next());

        utext_close(&ut1);
        utext_close(&ut2);
    }
    delete bi;

}

#if !UCONFIG_NO_BREAK_ITERATION && !UCONFIG_NO_FILTERED_BREAK_ITERATION
static void prtbrks(BreakIterator* brk, const UnicodeString &ustr, IntlTest &it) {
  static const UChar PILCROW=0x00B6, CHSTR=0x3010, CHEND=0x3011; // lenticular brackets
  it.logln(UnicodeString("String:'")+ustr+UnicodeString("'"));

  int32_t *pos = new int32_t[ustr.length()];
  int32_t posCount = 0;

  // calculate breaks up front, so we can print out
  // sans any debugging
  for(int32_t n = 0; (n=brk->next())!=UBRK_DONE; ) {
    pos[posCount++] = n;
    if(posCount>=ustr.length()) {
      it.errln("brk count exceeds string length!");
      return;
    }
  }
  UnicodeString out;
  out.append((UChar)CHSTR);
  int32_t prev = 0;
  for(int32_t i=0;i<posCount;i++) {
    int32_t n=pos[i];
    out.append(ustr.tempSubString(prev,n-prev));
    out.append((UChar)PILCROW);
    prev=n;
  }
  out.append(ustr.tempSubString(prev,ustr.length()-prev));
  out.append((UChar)CHEND);
  it.logln(out);

  out.remove();
  for(int32_t i=0;i<posCount;i++) {
    char tmp[100];
    sprintf(tmp,"%d ",pos[i]);
    out.append(UnicodeString(tmp));
  }
  it.logln(out);
  delete [] pos;
}
#endif

void RBBIAPITest::TestFilteredBreakIteratorBuilder() {
#if !UCONFIG_NO_BREAK_ITERATION && !UCONFIG_NO_FILTERED_BREAK_ITERATION
  UErrorCode status = U_ZERO_ERROR;
  LocalPointer<FilteredBreakIteratorBuilder> builder;
  LocalPointer<BreakIterator> baseBI;
  LocalPointer<BreakIterator> filteredBI;
  LocalPointer<BreakIterator> frenchBI;

  const UnicodeString text("In the meantime Mr. Weston arrived with his small ship, which he had now recovered. Capt. Gorges, who informed the Sgt. here that one purpose of his going east was to meet with Mr. Weston, took this opportunity to call him to account for some abuses he had to lay to his charge."); // (William Bradford, public domain. http://catalog.hathitrust.org/Record/008651224 ) - edited.
  const UnicodeString ABBR_MR("Mr.");
  const UnicodeString ABBR_CAPT("Capt.");

  {
    logln("Constructing empty builder\n");
    builder.adoptInstead(FilteredBreakIteratorBuilder::createInstance(status));
    TEST_ASSERT_SUCCESS(status);

    logln("Constructing base BI\n");
    baseBI.adoptInstead(BreakIterator::createSentenceInstance(Locale::getEnglish(), status));
    TEST_ASSERT_SUCCESS(status);

	logln("Building new BI\n");
    filteredBI.adoptInstead(builder->build(baseBI.orphan(), status));
    TEST_ASSERT_SUCCESS(status);

	if (U_SUCCESS(status)) {
        logln("Testing:");
        filteredBI->setText(text);
        TEST_ASSERT(20 == filteredBI->next()); // Mr.
        TEST_ASSERT(84 == filteredBI->next()); // recovered.
        TEST_ASSERT(90 == filteredBI->next()); // Capt.
        TEST_ASSERT(181 == filteredBI->next()); // Mr.
        TEST_ASSERT(278 == filteredBI->next()); // charge.
        filteredBI->first();
        prtbrks(filteredBI.getAlias(), text, *this);
    }
  }

  {
    logln("Constructing empty builder\n");
    builder.adoptInstead(FilteredBreakIteratorBuilder::createInstance(status));
    TEST_ASSERT_SUCCESS(status);

    if (U_SUCCESS(status)) {
        logln("Adding Mr. as an exception\n");
        TEST_ASSERT(TRUE == builder->suppressBreakAfter(ABBR_MR, status));
        TEST_ASSERT(FALSE == builder->suppressBreakAfter(ABBR_MR, status)); // already have it
        TEST_ASSERT(TRUE == builder->unsuppressBreakAfter(ABBR_MR, status));
        TEST_ASSERT(FALSE == builder->unsuppressBreakAfter(ABBR_MR, status)); // already removed it
        TEST_ASSERT(TRUE == builder->suppressBreakAfter(ABBR_MR, status));
        TEST_ASSERT_SUCCESS(status);

        logln("Constructing base BI\n");
        baseBI.adoptInstead(BreakIterator::createSentenceInstance(Locale::getEnglish(), status));
        TEST_ASSERT_SUCCESS(status);

        logln("Building new BI\n");
        filteredBI.adoptInstead(builder->build(baseBI.orphan(), status));
        TEST_ASSERT_SUCCESS(status);

        logln("Testing:");
        filteredBI->setText(text);
        TEST_ASSERT(84 == filteredBI->next());
        TEST_ASSERT(90 == filteredBI->next());// Capt.
        TEST_ASSERT(278 == filteredBI->next());
        filteredBI->first();
        prtbrks(filteredBI.getAlias(), text, *this);
    }
  }


  {
    logln("Constructing empty builder\n");
    builder.adoptInstead(FilteredBreakIteratorBuilder::createInstance(status));
    TEST_ASSERT_SUCCESS(status);

    if (U_SUCCESS(status)) {
        logln("Adding Mr. and Capt as an exception\n");
        TEST_ASSERT(TRUE == builder->suppressBreakAfter(ABBR_MR, status));
        TEST_ASSERT(TRUE == builder->suppressBreakAfter(ABBR_CAPT, status));
        TEST_ASSERT_SUCCESS(status);

        logln("Constructing base BI\n");
        baseBI.adoptInstead(BreakIterator::createSentenceInstance(Locale::getEnglish(), status));
        TEST_ASSERT_SUCCESS(status);

        logln("Building new BI\n");
        filteredBI.adoptInstead(builder->build(baseBI.orphan(), status));
        TEST_ASSERT_SUCCESS(status);

        logln("Testing:");
        filteredBI->setText(text);
        TEST_ASSERT(84 == filteredBI->next());
        TEST_ASSERT(278 == filteredBI->next());
        filteredBI->first();
        prtbrks(filteredBI.getAlias(), text, *this);
    }
  }


  {
    logln("Constructing English builder\n");
    builder.adoptInstead(FilteredBreakIteratorBuilder::createInstance(Locale::getEnglish(), status));
    TEST_ASSERT_SUCCESS(status);

    logln("Constructing base BI\n");
    baseBI.adoptInstead(BreakIterator::createSentenceInstance(Locale::getEnglish(), status));
    TEST_ASSERT_SUCCESS(status);

    if (U_SUCCESS(status)) {
        logln("unsuppressing 'Capt'");
        TEST_ASSERT(TRUE == builder->unsuppressBreakAfter(ABBR_CAPT, status));

        logln("Building new BI\n");
        filteredBI.adoptInstead(builder->build(baseBI.orphan(), status));
        TEST_ASSERT_SUCCESS(status);

        if(filteredBI.isValid()) {
          logln("Testing:");
          filteredBI->setText(text);
          TEST_ASSERT(84 == filteredBI->next());
          TEST_ASSERT(90 == filteredBI->next());
          TEST_ASSERT(278 == filteredBI->next());
          filteredBI->first();
          prtbrks(filteredBI.getAlias(), text, *this);
        }
    }
  }


  {
    logln("Constructing English builder\n");
    builder.adoptInstead(FilteredBreakIteratorBuilder::createInstance(Locale::getEnglish(), status));
    TEST_ASSERT_SUCCESS(status);

    logln("Constructing base BI\n");
    baseBI.adoptInstead(BreakIterator::createSentenceInstance(Locale::getEnglish(), status));
    TEST_ASSERT_SUCCESS(status);

    if (U_SUCCESS(status)) {
        logln("Building new BI\n");
        filteredBI.adoptInstead(builder->build(baseBI.orphan(), status));
        TEST_ASSERT_SUCCESS(status);

        if(filteredBI.isValid()) {
          logln("Testing:");
          filteredBI->setText(text);
          TEST_ASSERT(84 == filteredBI->next());
          TEST_ASSERT(278 == filteredBI->next());
          filteredBI->first();
          prtbrks(filteredBI.getAlias(), text, *this);
        }
    }
  }

  // reenable once french is in
  {
    logln("Constructing French builder");
    builder.adoptInstead(FilteredBreakIteratorBuilder::createInstance(Locale::getFrench(), status));
    TEST_ASSERT_SUCCESS(status);

    logln("Constructing base BI\n");
    baseBI.adoptInstead(BreakIterator::createSentenceInstance(Locale::getFrench(), status));
    TEST_ASSERT_SUCCESS(status);

    if (U_SUCCESS(status)) {
        logln("Building new BI\n");
        frenchBI.adoptInstead(builder->build(baseBI.orphan(), status));
        TEST_ASSERT_SUCCESS(status);
    }

    if(frenchBI.isValid()) {
      logln("Testing:");
      UnicodeString frText("C'est MM. Duval.");
      frenchBI->setText(frText);
      TEST_ASSERT(16 == frenchBI->next());
      TEST_ASSERT(BreakIterator::DONE == frenchBI->next());
      frenchBI->first();
      prtbrks(frenchBI.getAlias(), frText, *this);
      logln("Testing against English:");
      filteredBI->setText(frText);
      TEST_ASSERT(10 == filteredBI->next()); // wrong for french, but filterBI is english.
      TEST_ASSERT(16 == filteredBI->next());
      TEST_ASSERT(BreakIterator::DONE == filteredBI->next());
      filteredBI->first();
      prtbrks(filteredBI.getAlias(), frText, *this);

      // Verify ==
      assertTrue(WHERE, *frenchBI   == *frenchBI);
      assertTrue(WHERE, *filteredBI != *frenchBI);
      assertTrue(WHERE, *frenchBI   != *filteredBI);
    } else {
      dataerrln("French BI: not valid.");
	}
  }

#else
  logln("Skipped- not: !UCONFIG_NO_BREAK_ITERATION && !UCONFIG_NO_FILTERED_BREAK_ITERATION");
#endif
}

//---------------------------------------------
// runIndexedTest
//---------------------------------------------

void RBBIAPITest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln((UnicodeString)"TestSuite RuleBasedBreakIterator API ");
    TESTCASE_AUTO_BEGIN;
#if !UCONFIG_NO_FILE_IO
    TESTCASE_AUTO(TestCloneEquals);
    TESTCASE_AUTO(TestgetRules);
    TESTCASE_AUTO(TestHashCode);
    TESTCASE_AUTO(TestGetSetAdoptText);
    TESTCASE_AUTO(TestIteration);
#endif
    TESTCASE_AUTO(TestBuilder);
    TESTCASE_AUTO(TestQuoteGrouping);
    TESTCASE_AUTO(TestRuleStatusVec);
    TESTCASE_AUTO(TestBug2190);
#if !UCONFIG_NO_FILE_IO
    TESTCASE_AUTO(TestRegistration);
    TESTCASE_AUTO(TestBoilerPlate);
    TESTCASE_AUTO(TestRuleStatus);
    TESTCASE_AUTO(TestRoundtripRules);
    TESTCASE_AUTO(TestGetBinaryRules);
#endif
    TESTCASE_AUTO(TestRefreshInputText);
#if !UCONFIG_NO_BREAK_ITERATION
    TESTCASE_AUTO(TestFilteredBreakIteratorBuilder);
#endif
    TESTCASE_AUTO_END;
}


//---------------------------------------------
//Internal subroutines
//---------------------------------------------

void RBBIAPITest::doBoundaryTest(BreakIterator& bi, UnicodeString& text, int32_t *boundaries){
     logln((UnicodeString)"testIsBoundary():");
        int32_t p = 0;
        UBool isB;
        for (int32_t i = 0; i < text.length(); i++) {
            isB = bi.isBoundary(i);
            logln((UnicodeString)"bi.isBoundary(" + i + ") -> " + isB);

            if (i == boundaries[p]) {
                if (!isB)
                    errln((UnicodeString)"Wrong result from isBoundary() for " + i + (UnicodeString)": expected true, got false");
                p++;
            }
            else {
                if (isB)
                    errln((UnicodeString)"Wrong result from isBoundary() for " + i + (UnicodeString)": expected false, got true");
            }
        }
}
void RBBIAPITest::doTest(UnicodeString& testString, int32_t start, int32_t gotoffset, int32_t expectedOffset, const char* expectedString){
    UnicodeString selected;
    UnicodeString expected=CharsToUnicodeString(expectedString);

    if(gotoffset != expectedOffset)
         errln((UnicodeString)"ERROR:****returned #" + gotoffset + (UnicodeString)" instead of #" + expectedOffset);
    if(start <= gotoffset){
        testString.extractBetween(start, gotoffset, selected);
    }
    else{
        testString.extractBetween(gotoffset, start, selected);
    }
    if(selected.compare(expected) != 0)
         errln(prettify((UnicodeString)"ERROR:****selected \"" + selected + "\" instead of \"" + expected + "\""));
    else
        logln(prettify("****selected \"" + selected + "\""));
}

#endif /* #if !UCONFIG_NO_BREAK_ITERATION */
