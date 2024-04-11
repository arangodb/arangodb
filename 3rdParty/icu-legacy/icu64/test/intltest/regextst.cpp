// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 2002-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

//
//   regextst.cpp
//
//      ICU Regular Expressions test, part of intltest.
//

/*
     NOTE!!

     PLEASE be careful about ASCII assumptions in this test.
     This test is one of the worst repeat offenders.
     If you have questions, contact someone on the ICU PMC
     who has access to an EBCDIC system.

 */

#include "intltest.h"
#if !UCONFIG_NO_REGULAR_EXPRESSIONS

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "unicode/localpointer.h"
#include "unicode/regex.h"
#include "unicode/stringpiece.h"
#include "unicode/uchar.h"
#include "unicode/ucnv.h"
#include "unicode/uniset.h"
#include "unicode/uregex.h"
#include "unicode/usetiter.h"
#include "unicode/ustring.h"
#include "unicode/utext.h"
#include "unicode/utf16.h"
#include "cstr.h"
#include "regextst.h"
#include "regexcmp.h"
#include "uvector.h"
#include "util.h"
#include "cmemory.h"
#include "cstring.h"
#include "uinvchar.h"

#define SUPPORT_MUTATING_INPUT_STRING   0

//---------------------------------------------------------------------------
//
//  Test class boilerplate
//
//---------------------------------------------------------------------------
RegexTest::RegexTest()
{
}


RegexTest::~RegexTest()
{
}



void RegexTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite RegexTest: ");
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(Basic);
    TESTCASE_AUTO(API_Match);
    TESTCASE_AUTO(API_Replace);
    TESTCASE_AUTO(API_Pattern);
#if !UCONFIG_NO_FILE_IO
    TESTCASE_AUTO(Extended);
#endif
    TESTCASE_AUTO(Errors);
    TESTCASE_AUTO(PerlTests);
    TESTCASE_AUTO(Callbacks);
    TESTCASE_AUTO(FindProgressCallbacks);
    TESTCASE_AUTO(Bug6149);
    TESTCASE_AUTO(UTextBasic);
    TESTCASE_AUTO(API_Match_UTF8);
    TESTCASE_AUTO(API_Replace_UTF8);
    TESTCASE_AUTO(API_Pattern_UTF8);
    TESTCASE_AUTO(PerlTestsUTF8);
    TESTCASE_AUTO(PreAllocatedUTextCAPI);
    TESTCASE_AUTO(Bug7651);
    TESTCASE_AUTO(Bug7740);
    TESTCASE_AUTO(Bug8479);
    TESTCASE_AUTO(Bug7029);
    TESTCASE_AUTO(CheckInvBufSize);
    TESTCASE_AUTO(Bug9283);
    TESTCASE_AUTO(Bug10459);
    TESTCASE_AUTO(TestCaseInsensitiveStarters);
    TESTCASE_AUTO(TestBug11049);
    TESTCASE_AUTO(TestBug11371);
    TESTCASE_AUTO(TestBug11480);
    TESTCASE_AUTO(NamedCapture);
    TESTCASE_AUTO(NamedCaptureLimits);
    TESTCASE_AUTO(TestBug12884);
    TESTCASE_AUTO(TestBug13631);
    TESTCASE_AUTO(TestBug13632);
    TESTCASE_AUTO(TestBug20359);
    TESTCASE_AUTO(TestBug20863);
    TESTCASE_AUTO_END;
}


/**
 * Calls utext_openUTF8 after, potentially, converting invariant text from the compilation codepage
 * into ASCII.
 * @see utext_openUTF8
 */
static UText* regextst_openUTF8FromInvariant(UText* ut, const char *inv, int64_t length, UErrorCode *status);

//---------------------------------------------------------------------------
//
//   Error Checking / Reporting macros used in all of the tests.
//
//---------------------------------------------------------------------------

static void utextToPrintable(char *buf, int32_t bufLen, UText *text) {
  int64_t oldIndex = utext_getNativeIndex(text);
  utext_setNativeIndex(text, 0);
  char *bufPtr = buf;
  UChar32 c = utext_next32From(text, 0);
  while ((c != U_SENTINEL) && (bufPtr < buf+bufLen)) {
    if (0x000020<=c && c<0x00007e) {
      *bufPtr = c;
    } else {
#if 0
      snprintf(bufPtr, bufLen - (bufPtr-buf), "U+%04X", c);
      bufPtr+= strlen(bufPtr)-1;
#else
      *bufPtr = '%';
#endif
    }
    bufPtr++;
    c = UTEXT_NEXT32(text);
  }
  *bufPtr = 0;
#if (U_CHARSET_FAMILY==U_EBCDIC_FAMILY)
  char *ebuf = (char*)malloc(bufLen);
  uprv_eastrncpy((unsigned char*)ebuf, (const unsigned char*)buf, bufLen);
  uprv_strncpy(buf, ebuf, bufLen);
  free((void*)ebuf);
#endif
  utext_setNativeIndex(text, oldIndex);
}


static char ASSERT_BUF[1024];

const char* RegexTest::extractToAssertBuf(const UnicodeString& message) {
  if(message.length()==0) {
    strcpy(ASSERT_BUF, "[[empty UnicodeString]]");
  } else {
    UnicodeString buf;
    IntlTest::prettify(message,buf);
    if(buf.length()==0) {
      strcpy(ASSERT_BUF, "[[escape() returned 0 chars]]");
    } else {
      buf.extract(0, 0x7FFFFFFF, ASSERT_BUF, sizeof(ASSERT_BUF)-1);
      if(ASSERT_BUF[0]==0) {
        ASSERT_BUF[0]=0;
        for(int32_t i=0;i<buf.length();i++) {
          char16_t ch = buf[i];
          snprintf(ASSERT_BUF+strlen(ASSERT_BUF), sizeof(ASSERT_BUF) - strlen(ASSERT_BUF), "\\u%02x",ch);
        }
      }
    }
  }
  ASSERT_BUF[sizeof(ASSERT_BUF)-1] = 0;
  return ASSERT_BUF;
}

#define REGEX_VERBOSE_TEXT(text) UPRV_BLOCK_MACRO_BEGIN { \
    char buf[200]; \
    utextToPrintable(buf,UPRV_LENGTHOF(buf),text); \
    logln("%s:%d: UText %s=\"%s\"", __FILE__, __LINE__, #text, buf); \
} UPRV_BLOCK_MACRO_END

#define REGEX_CHECK_STATUS UPRV_BLOCK_MACRO_BEGIN { \
    if (U_FAILURE(status)) { \
        dataerrln("%s:%d: RegexTest failure.  status=%s", \
                  __FILE__, __LINE__, u_errorName(status)); \
        return; \
    } \
} UPRV_BLOCK_MACRO_END

#define REGEX_ASSERT(expr) UPRV_BLOCK_MACRO_BEGIN { \
    if ((expr)==false) { \
        errln("%s:%d: RegexTest failure: REGEX_ASSERT(%s) failed \n", __FILE__, __LINE__, #expr); \
    } \
} UPRV_BLOCK_MACRO_END

#define REGEX_ASSERT_FAIL(expr, errcode) UPRV_BLOCK_MACRO_BEGIN { \
    UErrorCode status=U_ZERO_ERROR; \
    (expr); \
    if (status!=errcode) { \
        dataerrln("RegexTest failure at line %d.  Expected status=%s, got %s", \
                  __LINE__, u_errorName(errcode), u_errorName(status)); \
    } \
} UPRV_BLOCK_MACRO_END

#define REGEX_CHECK_STATUS_L(line) UPRV_BLOCK_MACRO_BEGIN { \
    if (U_FAILURE(status)) { \
        errln("RegexTest failure at line %d, from %d.  status=%d\n",__LINE__, (line), status); \
    } \
} UPRV_BLOCK_MACRO_END

#define REGEX_ASSERT_L(expr, line) UPRV_BLOCK_MACRO_BEGIN { \
    if ((expr)==false) { \
        errln("RegexTest failure at line %d, from %d.", __LINE__, (line)); \
        return; \
    } \
} UPRV_BLOCK_MACRO_END

// expected: const char * , restricted to invariant characters.
// actual: const UnicodeString &
#define REGEX_ASSERT_UNISTR(expected, actual) UPRV_BLOCK_MACRO_BEGIN { \
    if (UnicodeString(expected, -1, US_INV) != (actual)) { \
        errln("%s:%d: RegexTest failure: REGEX_ASSERT_UNISTR(%s, %s) failed \n",  \
              __FILE__, __LINE__, expected, extractToAssertBuf(actual)); \
    } \
} UPRV_BLOCK_MACRO_END


static UBool testUTextEqual(UText *uta, UText *utb) {
    UChar32 ca = 0;
    UChar32 cb = 0;
    utext_setNativeIndex(uta, 0);
    utext_setNativeIndex(utb, 0);
    do {
        ca = utext_next32(uta);
        cb = utext_next32(utb);
        if (ca != cb) {
            break;
        }
    } while (ca != U_SENTINEL);
    return ca == cb;
}


/**
 * @param expected expected text in UTF-8 (not platform) codepage
 */
void RegexTest::assertUText(const char *expected, UText *actual, const char *file, int line) {
    UErrorCode status = U_ZERO_ERROR;
    UText expectedText = UTEXT_INITIALIZER;
    utext_openUTF8(&expectedText, expected, -1, &status);
    if(U_FAILURE(status)) {
      errln("%s:%d: assertUText: error %s calling utext_openUTF8(expected: %d chars)\n", file, line, u_errorName(status), strlen(expected));
      return;
    }
    if(utext_nativeLength(&expectedText)==0 && (strlen(expected)!=0)) {
      errln("%s:%d: assertUText:  expected is %d utf-8 bytes, but utext_nativeLength(expectedText) returned 0.", file, line, strlen(expected));
      return;
    }
    utext_setNativeIndex(actual, 0);
    if (!testUTextEqual(&expectedText, actual)) {
        char buf[201 /*21*/];
        char expectedBuf[201];
        utextToPrintable(buf, UPRV_LENGTHOF(buf), actual);
        utextToPrintable(expectedBuf, UPRV_LENGTHOF(expectedBuf), &expectedText);
        errln("%s:%d: assertUText: Failure: expected \"%s\" (%d chars), got \"%s\" (%d chars)", file, line, expectedBuf, (int)utext_nativeLength(&expectedText), buf, (int)utext_nativeLength(actual));
    }
    utext_close(&expectedText);
}
/**
 * @param expected invariant (platform local text) input
 */

void RegexTest::assertUTextInvariant(const char *expected, UText *actual, const char *file, int line) {
    UErrorCode status = U_ZERO_ERROR;
    UText expectedText = UTEXT_INITIALIZER;
    regextst_openUTF8FromInvariant(&expectedText, expected, -1, &status);
    if(U_FAILURE(status)) {
      errln("%s:%d: assertUTextInvariant: error %s calling regextst_openUTF8FromInvariant(expected: %d chars)\n", file, line, u_errorName(status), strlen(expected));
      return;
    }
    utext_setNativeIndex(actual, 0);
    if (!testUTextEqual(&expectedText, actual)) {
        char buf[201 /*21*/];
        char expectedBuf[201];
        utextToPrintable(buf, UPRV_LENGTHOF(buf), actual);
        utextToPrintable(expectedBuf, UPRV_LENGTHOF(expectedBuf), &expectedText);
        errln("%s:%d: assertUTextInvariant: Failure: expected \"%s\" (%d uchars), got \"%s\" (%d chars)", file, line, expectedBuf, (int)utext_nativeLength(&expectedText), buf, (int)utext_nativeLength(actual));
    }
    utext_close(&expectedText);
}

/**
 * Assumes utf-8 input
 */
#define REGEX_ASSERT_UTEXT_UTF8(expected, actual) assertUText((expected), (actual), __FILE__, __LINE__)
/**
 * Assumes Invariant input
 */
#define REGEX_ASSERT_UTEXT_INVARIANT(expected, actual) assertUTextInvariant((expected), (actual), __FILE__, __LINE__)

/**
 * This buffer ( inv_buf ) is used to hold the UTF-8 strings
 * passed into utext_openUTF8. An error will be given if
 * INV_BUFSIZ is too small.  It's only used on EBCDIC systems.
 */

#define INV_BUFSIZ 2048 /* increase this if too small */

static int64_t inv_next=0;

#if U_CHARSET_FAMILY!=U_ASCII_FAMILY
static char inv_buf[INV_BUFSIZ];
#endif

static UText* regextst_openUTF8FromInvariant(UText *ut, const char *inv, int64_t length, UErrorCode *status) {
  if(length==-1) length=strlen(inv);
#if U_CHARSET_FAMILY==U_ASCII_FAMILY
  inv_next+=length;
  return utext_openUTF8(ut, inv, length, status);
#else
  if(inv_next+length+1>INV_BUFSIZ) {
    fprintf(stderr, "%s:%d Error: INV_BUFSIZ #defined to be %d but needs to be at least %d.\n",
            __FILE__, __LINE__, INV_BUFSIZ, (inv_next+length+1));
    *status = U_MEMORY_ALLOCATION_ERROR;
    return nullptr;
  }

  unsigned char *buf = (unsigned char*)inv_buf+inv_next;
  uprv_aestrncpy(buf, (const uint8_t*)inv, length);
  inv_next+=length;

#if 0
  fprintf(stderr, " Note: INV_BUFSIZ at %d, used=%d\n", INV_BUFSIZ, inv_next);
#endif

  return utext_openUTF8(ut, (const char*)buf, length, status);
#endif
}


//---------------------------------------------------------------------------
//
//    REGEX_TESTLM       Macro + invocation function to simplify writing quick tests
//                       for the LookingAt() and  Match() functions.
//
//       usage:
//          REGEX_TESTLM("pattern",  "input text",  lookingAt expected, matches expected);
//
//          The expected results are UBool - true or false.
//          The input text is unescaped.  The pattern is not.
//
//
//---------------------------------------------------------------------------

#define REGEX_TESTLM(pat, text, looking, match) UPRV_BLOCK_MACRO_BEGIN { \
    doRegexLMTest(pat, text, looking, match, __LINE__); \
    doRegexLMTestUTF8(pat, text, looking, match, __LINE__); \
} UPRV_BLOCK_MACRO_END

UBool RegexTest::doRegexLMTest(const char *pat, const char *text, UBool looking, UBool match, int32_t line) {
    const UnicodeString pattern(pat, -1, US_INV);
    const UnicodeString inputText(text, -1, US_INV);
    UErrorCode          status  = U_ZERO_ERROR;
    UParseError         pe;
    RegexPattern        *REPattern = nullptr;
    RegexMatcher        *REMatcher = nullptr;
    UBool               retVal     = true;

    UnicodeString patString(pat, -1, US_INV);
    REPattern = RegexPattern::compile(patString, 0, pe, status);
    if (U_FAILURE(status)) {
        dataerrln("RegexTest failure in RegexPattern::compile() at line %d.  Status = %s",
            line, u_errorName(status));
        return false;
    }
    if (line==376) { REPattern->dumpPattern();}

    UnicodeString inputString(inputText);
    UnicodeString unEscapedInput = inputString.unescape();
    REMatcher = REPattern->matcher(unEscapedInput, status);
    if (U_FAILURE(status)) {
        errln("RegexTest failure in REPattern::matcher() at line %d.  Status = %s\n",
            line, u_errorName(status));
        return false;
    }

    UBool actualmatch;
    actualmatch = REMatcher->lookingAt(status);
    if (U_FAILURE(status)) {
        errln("RegexTest failure in lookingAt() at line %d.  Status = %s\n",
            line, u_errorName(status));
        retVal =  false;
    }
    if (actualmatch != looking) {
        errln("RegexTest: wrong return from lookingAt() at line %d.\n", line);
        retVal = false;
    }

    status = U_ZERO_ERROR;
    actualmatch = REMatcher->matches(status);
    if (U_FAILURE(status)) {
        errln("RegexTest failure in matches() at line %d.  Status = %s\n",
            line, u_errorName(status));
        retVal = false;
    }
    if (actualmatch != match) {
        errln("RegexTest: wrong return from matches() at line %d.\n", line);
        retVal = false;
    }

    if (retVal == false) {
        REPattern->dumpPattern();
    }

    delete REPattern;
    delete REMatcher;
    return retVal;
}


UBool RegexTest::doRegexLMTestUTF8(const char *pat, const char *text, UBool looking, UBool match, int32_t line) {
    UText               pattern    = UTEXT_INITIALIZER;
    int32_t             inputUTF8Length;
    char                *textChars = nullptr;
    UText               inputText  = UTEXT_INITIALIZER;
    UErrorCode          status     = U_ZERO_ERROR;
    UParseError         pe;
    RegexPattern        *REPattern = nullptr;
    RegexMatcher        *REMatcher = nullptr;
    UBool               retVal     = true;

    regextst_openUTF8FromInvariant(&pattern, pat, -1, &status);
    REPattern = RegexPattern::compile(&pattern, 0, pe, status);
    if (U_FAILURE(status)) {
        dataerrln("RegexTest failure in RegexPattern::compile() at line %d (UTF8).  Status = %s\n",
            line, u_errorName(status));
        return false;
    }

    UnicodeString inputString(text, -1, US_INV);
    UnicodeString unEscapedInput = inputString.unescape();
    LocalUConverterPointer UTF8Converter(ucnv_open("UTF8", &status));
    ucnv_setFromUCallBack(UTF8Converter.getAlias(), UCNV_FROM_U_CALLBACK_STOP, nullptr, nullptr, nullptr, &status);

    inputUTF8Length = unEscapedInput.extract(nullptr, 0, UTF8Converter.getAlias(), status);
    if (U_FAILURE(status) && status != U_BUFFER_OVERFLOW_ERROR) {
        // UTF-8 does not allow unpaired surrogates, so this could actually happen
        logln("RegexTest unable to convert input to UTF8 at line %d.  Status = %s\n", line, u_errorName(status));
        return true; // not a failure of the Regex engine
    }
    status = U_ZERO_ERROR; // buffer overflow
    textChars = new char[inputUTF8Length+1];
    unEscapedInput.extract(textChars, inputUTF8Length+1, UTF8Converter.getAlias(), status);
    utext_openUTF8(&inputText, textChars, inputUTF8Length, &status);

    REMatcher = &REPattern->matcher(status)->reset(&inputText);
    if (U_FAILURE(status)) {
        errln("RegexTest failure in REPattern::matcher() at line %d (UTF8).  Status = %s\n",
            line, u_errorName(status));
        return false;
    }

    UBool actualmatch;
    actualmatch = REMatcher->lookingAt(status);
    if (U_FAILURE(status)) {
        errln("RegexTest failure in lookingAt() at line %d (UTF8).  Status = %s\n",
            line, u_errorName(status));
        retVal =  false;
    }
    if (actualmatch != looking) {
        errln("RegexTest: wrong return from lookingAt() at line %d (UTF8).\n", line);
        retVal = false;
    }

    status = U_ZERO_ERROR;
    actualmatch = REMatcher->matches(status);
    if (U_FAILURE(status)) {
        errln("RegexTest failure in matches() at line %d (UTF8).  Status = %s\n",
            line, u_errorName(status));
        retVal = false;
    }
    if (actualmatch != match) {
        errln("RegexTest: wrong return from matches() at line %d (UTF8).\n", line);
        retVal = false;
    }

    if (retVal == false) {
        REPattern->dumpPattern();
    }

    delete REPattern;
    delete REMatcher;
    utext_close(&inputText);
    utext_close(&pattern);
    delete[] textChars;
    return retVal;
}



//---------------------------------------------------------------------------
//
//    REGEX_ERR       Macro + invocation function to simplify writing tests
//                       regex tests for incorrect patterns
//
//       usage:
//          REGEX_ERR("pattern",   expected error line, column, expected status);
//
//---------------------------------------------------------------------------
#define REGEX_ERR(pat, line, col, status) regex_err(pat, line, col, status, __LINE__)

void RegexTest::regex_err(const char *pat, int32_t errLine, int32_t errCol,
                          UErrorCode expectedStatus, int32_t line) {
    UnicodeString       pattern(pat);

    UErrorCode          status         = U_ZERO_ERROR;
    UParseError         pe;
    RegexPattern        *callerPattern = nullptr;

    //
    //  Compile the caller's pattern
    //
    UnicodeString patString(pat);
    callerPattern = RegexPattern::compile(patString, 0, pe, status);
    if (status != expectedStatus) {
        dataerrln("Line %d: unexpected error %s compiling pattern.", line, u_errorName(status));
    } else {
        if (status != U_ZERO_ERROR) {
            if (pe.line != errLine || pe.offset != errCol) {
                errln("Line %d: incorrect line/offset from UParseError.  Expected %d/%d; got %d/%d.\n",
                    line, errLine, errCol, pe.line, pe.offset);
            }
        }
    }

    delete callerPattern;

    //
    //  Compile again, using a UTF-8-based UText
    //
    UText patternText = UTEXT_INITIALIZER;
    regextst_openUTF8FromInvariant(&patternText, pat, -1, &status);
    callerPattern = RegexPattern::compile(&patternText, 0, pe, status);
    if (status != expectedStatus) {
        dataerrln("Line %d: unexpected error %s compiling pattern.", line, u_errorName(status));
    } else {
        if (status != U_ZERO_ERROR) {
            if (pe.line != errLine || pe.offset != errCol) {
                errln("Line %d: incorrect line/offset from UParseError.  Expected %d/%d; got %d/%d.\n",
                    line, errLine, errCol, pe.line, pe.offset);
            }
        }
    }

    delete callerPattern;
    utext_close(&patternText);
}



//---------------------------------------------------------------------------
//
//      Basic      Check for basic functionality of regex pattern matching.
//                 Avoid the use of REGEX_FIND test macro, which has
//                 substantial dependencies on basic Regex functionality.
//
//---------------------------------------------------------------------------
void RegexTest::Basic() {


//
// Debug - slide failing test cases early
//
#if 0
    {
        // REGEX_TESTLM("a\N{LATIN SMALL LETTER B}c", "abc", false, false);
        UParseError pe;
        UErrorCode  status = U_ZERO_ERROR;
        RegexPattern *pattern;
        pattern = RegexPattern::compile(UNICODE_STRING_SIMPLE("a\\u00dfx").unescape(), UREGEX_CASE_INSENSITIVE, pe, status);
        pattern->dumpPattern();
        RegexMatcher *m = pattern->matcher(UNICODE_STRING_SIMPLE("a\\u00dfxzzz").unescape(), status);
        UBool result = m->find();
        printf("result = %d\n", result);
        // REGEX_FIND("", "<0>ab<1>cc</1><2>ccc</2></0>ddd");
        // REGEX_FIND("(X([abc=X]+)+X)|(y[abc=]+)", "=XX====================");
    }
    exit(1);
#endif


    //
    // Pattern with parentheses
    //
    REGEX_TESTLM("st(abc)ring", "stabcring thing", true,  false);
    REGEX_TESTLM("st(abc)ring", "stabcring",       true,  true);
    REGEX_TESTLM("st(abc)ring", "stabcrung",       false, false);

    //
    // Patterns with *
    //
    REGEX_TESTLM("st(abc)*ring", "string", true, true);
    REGEX_TESTLM("st(abc)*ring", "stabcring", true, true);
    REGEX_TESTLM("st(abc)*ring", "stabcabcring", true, true);
    REGEX_TESTLM("st(abc)*ring", "stabcabcdring", false, false);
    REGEX_TESTLM("st(abc)*ring", "stabcabcabcring etc.", true, false);

    REGEX_TESTLM("a*", "",  true, true);
    REGEX_TESTLM("a*", "b", true, false);


    //
    //  Patterns with "."
    //
    REGEX_TESTLM(".", "abc", true, false);
    REGEX_TESTLM("...", "abc", true, true);
    REGEX_TESTLM("....", "abc", false, false);
    REGEX_TESTLM(".*", "abcxyz123", true, true);
    REGEX_TESTLM("ab.*xyz", "abcdefghij", false, false);
    REGEX_TESTLM("ab.*xyz", "abcdefg...wxyz", true, true);
    REGEX_TESTLM("ab.*xyz", "abcde...wxyz...abc..xyz", true, true);
    REGEX_TESTLM("ab.*xyz", "abcde...wxyz...abc..xyz...", true, false);

    //
    //  Patterns with * applied to chars at end of literal string
    //
    REGEX_TESTLM("abc*", "ab", true, true);
    REGEX_TESTLM("abc*", "abccccc", true, true);

    //
    //  Supplemental chars match as single chars, not a pair of surrogates.
    //
    REGEX_TESTLM(".", "\\U00011000", true, true);
    REGEX_TESTLM("...", "\\U00011000x\\U00012002", true, true);
    REGEX_TESTLM("...", "\\U00011000x\\U00012002y", true, false);


    //
    //  UnicodeSets in the pattern
    //
    REGEX_TESTLM("[1-6]", "1", true, true);
    REGEX_TESTLM("[1-6]", "3", true, true);
    REGEX_TESTLM("[1-6]", "7", false, false);
    REGEX_TESTLM("a[1-6]", "a3", true, true);
    REGEX_TESTLM("a[1-6]", "a3", true, true);
    REGEX_TESTLM("a[1-6]b", "a3b", true, true);

    REGEX_TESTLM("a[0-9]*b", "a123b", true, true);
    REGEX_TESTLM("a[0-9]*b", "abc", true, false);
    REGEX_TESTLM("[\\p{Nd}]*", "123456", true, true);
    REGEX_TESTLM("[\\p{Nd}]*", "a123456", true, false);   // note that * matches 0 occurrences.
    REGEX_TESTLM("[a][b][[:Zs:]]*", "ab   ", true, true);

    //
    //   OR operator in patterns
    //
    REGEX_TESTLM("(a|b)", "a", true, true);
    REGEX_TESTLM("(a|b)", "b", true, true);
    REGEX_TESTLM("(a|b)", "c", false, false);
    REGEX_TESTLM("a|b", "b", true, true);

    REGEX_TESTLM("(a|b|c)*", "aabcaaccbcabc", true, true);
    REGEX_TESTLM("(a|b|c)*", "aabcaaccbcabdc", true, false);
    REGEX_TESTLM("(a(b|c|d)(x|y|z)*|123)", "ac", true, true);
    REGEX_TESTLM("(a(b|c|d)(x|y|z)*|123)", "123", true, true);
    REGEX_TESTLM("(a|(1|2)*)(b|c|d)(x|y|z)*|123", "123", true, true);
    REGEX_TESTLM("(a|(1|2)*)(b|c|d)(x|y|z)*|123", "222211111czzzzw", true, false);

    //
    //  +
    //
    REGEX_TESTLM("ab+", "abbc", true, false);
    REGEX_TESTLM("ab+c", "ac", false, false);
    REGEX_TESTLM("b+", "", false, false);
    REGEX_TESTLM("(abc|def)+", "defabc", true, true);
    REGEX_TESTLM(".+y", "zippity dooy dah ", true, false);
    REGEX_TESTLM(".+y", "zippity dooy", true, true);

    //
    //   ?
    //
    REGEX_TESTLM("ab?", "ab", true, true);
    REGEX_TESTLM("ab?", "a", true, true);
    REGEX_TESTLM("ab?", "ac", true, false);
    REGEX_TESTLM("ab?", "abb", true, false);
    REGEX_TESTLM("a(b|c)?d", "abd", true, true);
    REGEX_TESTLM("a(b|c)?d", "acd", true, true);
    REGEX_TESTLM("a(b|c)?d", "ad", true, true);
    REGEX_TESTLM("a(b|c)?d", "abcd", false, false);
    REGEX_TESTLM("a(b|c)?d", "ab", false, false);

    //
    //  Escape sequences that become single literal chars, handled internally
    //   by ICU's Unescape.
    //

    // REGEX_TESTLM("\101\142", "Ab", true, true);      // Octal     TODO: not implemented yet.
    REGEX_TESTLM("\\a", "\\u0007", true, true);        // BEL
    REGEX_TESTLM("\\cL", "\\u000c", true, true);       // Control-L
    REGEX_TESTLM("\\e", "\\u001b", true, true);        // Escape
    REGEX_TESTLM("\\f", "\\u000c", true, true);        // Form Feed
    REGEX_TESTLM("\\n", "\\u000a", true, true);        // new line
    REGEX_TESTLM("\\r", "\\u000d", true, true);        //  CR
    REGEX_TESTLM("\\t", "\\u0009", true, true);        // Tab
    REGEX_TESTLM("\\u1234", "\\u1234", true, true);
    REGEX_TESTLM("\\U00001234", "\\u1234", true, true);

    REGEX_TESTLM(".*\\Ax", "xyz", true, false);  //  \A matches only at the beginning of input
    REGEX_TESTLM(".*\\Ax", " xyz", false, false);  //  \A matches only at the beginning of input

    // Escape of special chars in patterns
    REGEX_TESTLM("\\\\\\|\\(\\)\\[\\{\\~\\$\\*\\+\\?\\.", "\\\\|()[{~$*+?.", true, true);
}


//---------------------------------------------------------------------------
//
//    UTextBasic   Check for quirks that are specific to the UText
//                 implementation.
//
//---------------------------------------------------------------------------
void RegexTest::UTextBasic() {
    const char str_abc[] = { 0x61, 0x62, 0x63, 0x00 }; /* abc */
    UErrorCode status = U_ZERO_ERROR;
    UText pattern = UTEXT_INITIALIZER;
    utext_openUTF8(&pattern, str_abc, -1, &status);
    RegexMatcher matcher(&pattern, 0, status);
    REGEX_CHECK_STATUS;

    UText input = UTEXT_INITIALIZER;
    utext_openUTF8(&input, str_abc, -1, &status);
    REGEX_CHECK_STATUS;
    matcher.reset(&input);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT_UTEXT_UTF8(str_abc, matcher.inputText());

    matcher.reset(matcher.inputText());
    REGEX_CHECK_STATUS;
    REGEX_ASSERT_UTEXT_UTF8(str_abc, matcher.inputText());

    utext_close(&pattern);
    utext_close(&input);
}


//---------------------------------------------------------------------------
//
//      API_Match   Test that the API for class RegexMatcher
//                  is present and nominally working, but excluding functions
//                  implementing replace operations.
//
//---------------------------------------------------------------------------
void RegexTest::API_Match() {
    UParseError         pe;
    UErrorCode          status=U_ZERO_ERROR;
    int32_t             flags = 0;

    //
    // Debug - slide failing test cases early
    //
#if 0
    {
    }
    return;
#endif

    //
    // Simple pattern compilation
    //
    {
        UnicodeString       re("abc");
        RegexPattern        *pat2;
        pat2 = RegexPattern::compile(re, flags, pe, status);
        REGEX_CHECK_STATUS;

        UnicodeString inStr1 = "abcdef this is a test";
        UnicodeString instr2 = "not abc";
        UnicodeString empty  = "";


        //
        // Matcher creation and reset.
        //
        RegexMatcher *m1 = pat2->matcher(inStr1, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(m1->lookingAt(status) == true);
        REGEX_ASSERT(m1->input() == inStr1);
        m1->reset(instr2);
        REGEX_ASSERT(m1->lookingAt(status) == false);
        REGEX_ASSERT(m1->input() == instr2);
        m1->reset(inStr1);
        REGEX_ASSERT(m1->input() == inStr1);
        REGEX_ASSERT(m1->lookingAt(status) == true);
        m1->reset(empty);
        REGEX_ASSERT(m1->lookingAt(status) == false);
        REGEX_ASSERT(m1->input() == empty);
        REGEX_ASSERT(&m1->pattern() == pat2);

        //
        //  reset(pos, status)
        //
        m1->reset(inStr1);
        m1->reset(4, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(m1->input() == inStr1);
        REGEX_ASSERT(m1->lookingAt(status) == true);

        m1->reset(-1, status);
        REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);
        status = U_ZERO_ERROR;

        m1->reset(0, status);
        REGEX_CHECK_STATUS;
        status = U_ZERO_ERROR;

        int32_t len = m1->input().length();
        m1->reset(len-1, status);
        REGEX_CHECK_STATUS;
        status = U_ZERO_ERROR;

        m1->reset(len, status);
        REGEX_CHECK_STATUS;
        status = U_ZERO_ERROR;

        m1->reset(len+1, status);
        REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);
        status = U_ZERO_ERROR;

        //
        // match(pos, status)
        //
        m1->reset(instr2);
        REGEX_ASSERT(m1->matches(4, status) == true);
        m1->reset();
        REGEX_ASSERT(m1->matches(3, status) == false);
        m1->reset();
        REGEX_ASSERT(m1->matches(5, status) == false);
        REGEX_ASSERT(m1->matches(4, status) == true);
        REGEX_ASSERT(m1->matches(-1, status) == false);
        REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);

        // Match() at end of string should fail, but should not
        //  be an error.
        status = U_ZERO_ERROR;
        len = m1->input().length();
        REGEX_ASSERT(m1->matches(len, status) == false);
        REGEX_CHECK_STATUS;

        // Match beyond end of string should fail with an error.
        status = U_ZERO_ERROR;
        REGEX_ASSERT(m1->matches(len+1, status) == false);
        REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);

        // Successful match at end of string.
        {
            status = U_ZERO_ERROR;
            RegexMatcher m("A?", 0, status);  // will match zero length string.
            REGEX_CHECK_STATUS;
            m.reset(inStr1);
            len = inStr1.length();
            REGEX_ASSERT(m.matches(len, status) == true);
            REGEX_CHECK_STATUS;
            m.reset(empty);
            REGEX_ASSERT(m.matches(0, status) == true);
            REGEX_CHECK_STATUS;
        }


        //
        // lookingAt(pos, status)
        //
        status = U_ZERO_ERROR;
        m1->reset(instr2);  // "not abc"
        REGEX_ASSERT(m1->lookingAt(4, status) == true);
        REGEX_ASSERT(m1->lookingAt(5, status) == false);
        REGEX_ASSERT(m1->lookingAt(3, status) == false);
        REGEX_ASSERT(m1->lookingAt(4, status) == true);
        REGEX_ASSERT(m1->lookingAt(-1, status) == false);
        REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);
        status = U_ZERO_ERROR;
        len = m1->input().length();
        REGEX_ASSERT(m1->lookingAt(len, status) == false);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(m1->lookingAt(len+1, status) == false);
        REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);

        delete m1;
        delete pat2;
    }


    //
    // Capture Group.
    //     RegexMatcher::start();
    //     RegexMatcher::end();
    //     RegexMatcher::groupCount();
    //
    {
        int32_t             flags=0;
        UParseError         pe;
        UErrorCode          status=U_ZERO_ERROR;

        UnicodeString       re("01(23(45)67)(.*)");
        RegexPattern *pat = RegexPattern::compile(re, flags, pe, status);
        REGEX_CHECK_STATUS;
        UnicodeString data = "0123456789";

        RegexMatcher *matcher = pat->matcher(data, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(matcher->lookingAt(status) == true);
        static const int32_t matchStarts[] = {0,  2, 4, 8};
        static const int32_t matchEnds[]   = {10, 8, 6, 10};
        int32_t i;
        for (i=0; i<4; i++) {
            int32_t actualStart = matcher->start(i, status);
            REGEX_CHECK_STATUS;
            if (actualStart != matchStarts[i]) {
                errln("RegexTest failure at line %d, index %d.  Expected %d, got %d\n",
                    __LINE__, i, matchStarts[i], actualStart);
            }
            int32_t actualEnd = matcher->end(i, status);
            REGEX_CHECK_STATUS;
            if (actualEnd != matchEnds[i]) {
                errln("RegexTest failure at line %d index %d.  Expected %d, got %d\n",
                    __LINE__, i, matchEnds[i], actualEnd);
            }
        }

        REGEX_ASSERT(matcher->start(0, status) == matcher->start(status));
        REGEX_ASSERT(matcher->end(0, status) == matcher->end(status));

        REGEX_ASSERT_FAIL(matcher->start(-1, status), U_INDEX_OUTOFBOUNDS_ERROR);
        REGEX_ASSERT_FAIL(matcher->start( 4, status), U_INDEX_OUTOFBOUNDS_ERROR);
        matcher->reset();
        REGEX_ASSERT_FAIL(matcher->start( 0, status), U_REGEX_INVALID_STATE);

        matcher->lookingAt(status);
        REGEX_ASSERT(matcher->group(status)    == "0123456789");
        REGEX_ASSERT(matcher->group(0, status) == "0123456789");
        REGEX_ASSERT(matcher->group(1, status) == "234567"    );
        REGEX_ASSERT(matcher->group(2, status) == "45"        );
        REGEX_ASSERT(matcher->group(3, status) == "89"        );
        REGEX_CHECK_STATUS;
        REGEX_ASSERT_FAIL(matcher->group(-1, status), U_INDEX_OUTOFBOUNDS_ERROR);
        REGEX_ASSERT_FAIL(matcher->group( 4, status), U_INDEX_OUTOFBOUNDS_ERROR);
        matcher->reset();
        REGEX_ASSERT_FAIL(matcher->group( 0, status), U_REGEX_INVALID_STATE);

        delete matcher;
        delete pat;

    }

    //
    //  find
    //
    {
        int32_t             flags=0;
        UParseError         pe;
        UErrorCode          status=U_ZERO_ERROR;

        UnicodeString       re("abc");
        RegexPattern *pat = RegexPattern::compile(re, flags, pe, status);
        REGEX_CHECK_STATUS;
        UnicodeString data = ".abc..abc...abc..";
        //                    012345678901234567

        RegexMatcher *matcher = pat->matcher(data, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(matcher->find());
        REGEX_ASSERT(matcher->start(status) == 1);
        REGEX_ASSERT(matcher->find());
        REGEX_ASSERT(matcher->start(status) == 6);
        REGEX_ASSERT(matcher->find());
        REGEX_ASSERT(matcher->start(status) == 12);
        REGEX_ASSERT(matcher->find() == false);
        REGEX_ASSERT(matcher->find() == false);

        matcher->reset();
        REGEX_ASSERT(matcher->find());
        REGEX_ASSERT(matcher->start(status) == 1);

        REGEX_ASSERT(matcher->find(0, status));
        REGEX_ASSERT(matcher->start(status) == 1);
        REGEX_ASSERT(matcher->find(1, status));
        REGEX_ASSERT(matcher->start(status) == 1);
        REGEX_ASSERT(matcher->find(2, status));
        REGEX_ASSERT(matcher->start(status) == 6);
        REGEX_ASSERT(matcher->find(12, status));
        REGEX_ASSERT(matcher->start(status) == 12);
        REGEX_ASSERT(matcher->find(13, status) == false);
        REGEX_ASSERT(matcher->find(16, status) == false);
        REGEX_ASSERT(matcher->find(17, status) == false);
        REGEX_ASSERT_FAIL(matcher->start(status), U_REGEX_INVALID_STATE);

        status = U_ZERO_ERROR;
        REGEX_ASSERT_FAIL(matcher->find(-1, status), U_INDEX_OUTOFBOUNDS_ERROR);
        status = U_ZERO_ERROR;
        REGEX_ASSERT_FAIL(matcher->find(18, status), U_INDEX_OUTOFBOUNDS_ERROR);

        REGEX_ASSERT(matcher->groupCount() == 0);

        delete matcher;
        delete pat;
    }


    //
    //  find, with \G in pattern (true if at the end of a previous match).
    //
    {
        int32_t             flags=0;
        UParseError         pe;
        UErrorCode          status=U_ZERO_ERROR;

        UnicodeString       re(".*?(?:(\\Gabc)|(abc))", -1, US_INV);
        RegexPattern *pat = RegexPattern::compile(re, flags, pe, status);
        REGEX_CHECK_STATUS;
        UnicodeString data = ".abcabc.abc..";
        //                    012345678901234567

        RegexMatcher *matcher = pat->matcher(data, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(matcher->find());
        REGEX_ASSERT(matcher->start(status) == 0);
        REGEX_ASSERT(matcher->start(1, status) == -1);
        REGEX_ASSERT(matcher->start(2, status) == 1);

        REGEX_ASSERT(matcher->find());
        REGEX_ASSERT(matcher->start(status) == 4);
        REGEX_ASSERT(matcher->start(1, status) == 4);
        REGEX_ASSERT(matcher->start(2, status) == -1);
        REGEX_CHECK_STATUS;

        delete matcher;
        delete pat;
    }

    //
    //   find with zero length matches, match position should bump ahead
    //     to prevent loops.
    //
    {
        int32_t                 i;
        UErrorCode          status=U_ZERO_ERROR;
        RegexMatcher        m("(?= ?)", 0, status);   // This pattern will zero-length matches anywhere,
                                                      //   using an always-true look-ahead.
        REGEX_CHECK_STATUS;
        UnicodeString s("    ");
        m.reset(s);
        for (i=0; ; i++) {
            if (m.find() == false) {
                break;
            }
            REGEX_ASSERT(m.start(status) == i);
            REGEX_ASSERT(m.end(status) == i);
        }
        REGEX_ASSERT(i==5);

        // Check that the bump goes over surrogate pairs OK
        s = UNICODE_STRING_SIMPLE("\\U00010001\\U00010002\\U00010003\\U00010004");
        s = s.unescape();
        m.reset(s);
        for (i=0; ; i+=2) {
            if (m.find() == false) {
                break;
            }
            REGEX_ASSERT(m.start(status) == i);
            REGEX_ASSERT(m.end(status) == i);
        }
        REGEX_ASSERT(i==10);
    }
    {
        // find() loop breaking test.
        //        with pattern of /.?/, should see a series of one char matches, then a single
        //        match of zero length at the end of the input string.
        int32_t                 i;
        UErrorCode          status=U_ZERO_ERROR;
        RegexMatcher        m(".?", 0, status);
        REGEX_CHECK_STATUS;
        UnicodeString s("    ");
        m.reset(s);
        for (i=0; ; i++) {
            if (m.find() == false) {
                break;
            }
            REGEX_ASSERT(m.start(status) == i);
            REGEX_ASSERT(m.end(status) == (i<4 ? i+1 : i));
        }
        REGEX_ASSERT(i==5);
    }


    //
    // Matchers with no input string behave as if they had an empty input string.
    //

    {
        UErrorCode status = U_ZERO_ERROR;
        RegexMatcher  m(".?", 0, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(m.find());
        REGEX_ASSERT(m.start(status) == 0);
        REGEX_ASSERT(m.input() == "");
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        RegexPattern  *p = RegexPattern::compile(".", 0, status);
        RegexMatcher  *m = p->matcher(status);
        REGEX_CHECK_STATUS;

        REGEX_ASSERT(m->find() == false);
        REGEX_ASSERT(m->input() == "");
        delete m;
        delete p;
    }

    //
    // Regions
    //
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString testString("This is test data");
        RegexMatcher m(".*", testString,  0, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(m.regionStart() == 0);
        REGEX_ASSERT(m.regionEnd() == testString.length());
        REGEX_ASSERT(m.hasTransparentBounds() == false);
        REGEX_ASSERT(m.hasAnchoringBounds() == true);

        m.region(2,4, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(m.matches(status));
        REGEX_ASSERT(m.start(status)==2);
        REGEX_ASSERT(m.end(status)==4);
        REGEX_CHECK_STATUS;

        m.reset();
        REGEX_ASSERT(m.regionStart() == 0);
        REGEX_ASSERT(m.regionEnd() == testString.length());

        UnicodeString shorterString("short");
        m.reset(shorterString);
        REGEX_ASSERT(m.regionStart() == 0);
        REGEX_ASSERT(m.regionEnd() == shorterString.length());

        REGEX_ASSERT(m.hasAnchoringBounds() == true);
        REGEX_ASSERT(&m == &m.useAnchoringBounds(false));
        REGEX_ASSERT(m.hasAnchoringBounds() == false);
        REGEX_ASSERT(&m == &m.reset());
        REGEX_ASSERT(m.hasAnchoringBounds() == false);

        REGEX_ASSERT(&m == &m.useAnchoringBounds(true));
        REGEX_ASSERT(m.hasAnchoringBounds() == true);
        REGEX_ASSERT(&m == &m.reset());
        REGEX_ASSERT(m.hasAnchoringBounds() == true);

        REGEX_ASSERT(m.hasTransparentBounds() == false);
        REGEX_ASSERT(&m == &m.useTransparentBounds(true));
        REGEX_ASSERT(m.hasTransparentBounds() == true);
        REGEX_ASSERT(&m == &m.reset());
        REGEX_ASSERT(m.hasTransparentBounds() == true);

        REGEX_ASSERT(&m == &m.useTransparentBounds(false));
        REGEX_ASSERT(m.hasTransparentBounds() == false);
        REGEX_ASSERT(&m == &m.reset());
        REGEX_ASSERT(m.hasTransparentBounds() == false);

    }

    //
    // hitEnd() and requireEnd()
    //
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString testString("aabb");
        RegexMatcher m1(".*", testString,  0, status);
        REGEX_ASSERT(m1.lookingAt(status) == true);
        REGEX_ASSERT(m1.hitEnd() == true);
        REGEX_ASSERT(m1.requireEnd() == false);
        REGEX_CHECK_STATUS;

        status = U_ZERO_ERROR;
        RegexMatcher m2("a*", testString, 0, status);
        REGEX_ASSERT(m2.lookingAt(status) == true);
        REGEX_ASSERT(m2.hitEnd() == false);
        REGEX_ASSERT(m2.requireEnd() == false);
        REGEX_CHECK_STATUS;

        status = U_ZERO_ERROR;
        RegexMatcher m3(".*$", testString, 0, status);
        REGEX_ASSERT(m3.lookingAt(status) == true);
        REGEX_ASSERT(m3.hitEnd() == true);
        REGEX_ASSERT(m3.requireEnd() == true);
        REGEX_CHECK_STATUS;
    }


    //
    // Compilation error on reset with char16_t *
    //   These were a hazard that people were stumbling over with runtime errors.
    //   Changed them to compiler errors by adding private methods that more closely
    //   matched the incorrect use of the functions.
    //
#if 0
    {
        UErrorCode status = U_ZERO_ERROR;
        char16_t ucharString[20];
        RegexMatcher m(".", 0, status);
        m.reset(ucharString);  // should not compile.

        RegexPattern *p = RegexPattern::compile(".", 0, status);
        RegexMatcher *m2 = p->matcher(ucharString, status);    //  should not compile.

        RegexMatcher m3(".", ucharString, 0, status);  //  Should not compile
    }
#endif

    //
    //  Time Outs.
    //       Note:  These tests will need to be changed when the regexp engine is
    //              able to detect and cut short the exponential time behavior on
    //              this type of match.
    //
    {
        UErrorCode status = U_ZERO_ERROR;
        //    Enough 'a's in the string to cause the match to time out.
        //       (Each on additional 'a' doubles the time)
        UnicodeString testString("aaaaaaaaaaaaaaaaaaaaa");
        RegexMatcher matcher("(a+)+b", testString, 0, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(matcher.getTimeLimit() == 0);
        matcher.setTimeLimit(100, status);
        REGEX_ASSERT(matcher.getTimeLimit() == 100);
        REGEX_ASSERT(matcher.lookingAt(status) == false);
        REGEX_ASSERT(status == U_REGEX_TIME_OUT);
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        //   Few enough 'a's to slip in under the time limit.
        UnicodeString testString("aaaaaaaaaaaaaaaaaa");
        RegexMatcher matcher("(a+)+b", testString, 0, status);
        REGEX_CHECK_STATUS;
        matcher.setTimeLimit(100, status);
        REGEX_ASSERT(matcher.lookingAt(status) == false);
        REGEX_CHECK_STATUS;
    }

    //
    //  Stack Limits
    //
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString testString(1000000, 0x41, 1000000);  // Length 1,000,000, filled with 'A'

        // Adding the capturing parentheses to the pattern "(A)+A$" inhibits optimizations
        //   of the '+', and makes the stack frames larger.
        RegexMatcher matcher("(A)+A$", testString, 0, status);

        // With the default stack, this match should fail to run
        REGEX_ASSERT(matcher.lookingAt(status) == false);
        REGEX_ASSERT(status == U_REGEX_STACK_OVERFLOW);

        // With unlimited stack, it should run
        status = U_ZERO_ERROR;
        matcher.setStackLimit(0, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(matcher.lookingAt(status) == true);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(matcher.getStackLimit() == 0);

        // With a limited stack, it the match should fail
        status = U_ZERO_ERROR;
        matcher.setStackLimit(10000, status);
        REGEX_ASSERT(matcher.lookingAt(status) == false);
        REGEX_ASSERT(status == U_REGEX_STACK_OVERFLOW);
        REGEX_ASSERT(matcher.getStackLimit() == 10000);
    }

        // A pattern that doesn't save state should work with
        //   a minimal sized stack
    {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString testString = "abc";
        RegexMatcher matcher("abc", testString, 0, status);
        REGEX_CHECK_STATUS;
        matcher.setStackLimit(30, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(matcher.matches(status) == true);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(matcher.getStackLimit() == 30);

        // Negative stack sizes should fail
        status = U_ZERO_ERROR;
        matcher.setStackLimit(1000, status);
        REGEX_CHECK_STATUS;
        matcher.setStackLimit(-1, status);
        REGEX_ASSERT(status == U_ILLEGAL_ARGUMENT_ERROR);
        REGEX_ASSERT(matcher.getStackLimit() == 1000);
    }


}






//---------------------------------------------------------------------------
//
//      API_Replace        API test for class RegexMatcher, testing the
//                         Replace family of functions.
//
//---------------------------------------------------------------------------
void RegexTest::API_Replace() {
    //
    //  Replace
    //
    int32_t             flags=0;
    UParseError         pe;
    UErrorCode          status=U_ZERO_ERROR;

    UnicodeString       re("abc");
    RegexPattern *pat = RegexPattern::compile(re, flags, pe, status);
    REGEX_CHECK_STATUS;
    UnicodeString data = ".abc..abc...abc..";
    //                    012345678901234567
    RegexMatcher *matcher = pat->matcher(data, status);

    //
    //  Plain vanilla matches.
    //
    UnicodeString  dest;
    dest = matcher->replaceFirst("yz", status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(dest == ".yz..abc...abc..");

    dest = matcher->replaceAll("yz", status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(dest == ".yz..yz...yz..");

    //
    //  Plain vanilla non-matches.
    //
    UnicodeString d2 = ".abx..abx...abx..";
    matcher->reset(d2);
    dest = matcher->replaceFirst("yz", status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(dest == ".abx..abx...abx..");

    dest = matcher->replaceAll("yz", status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(dest == ".abx..abx...abx..");

    //
    // Empty source string
    //
    UnicodeString d3 = "";
    matcher->reset(d3);
    dest = matcher->replaceFirst("yz", status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(dest == "");

    dest = matcher->replaceAll("yz", status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(dest == "");

    //
    // Empty substitution string
    //
    matcher->reset(data);              // ".abc..abc...abc.."
    dest = matcher->replaceFirst("", status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(dest == "...abc...abc..");

    dest = matcher->replaceAll("", status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(dest == "........");

    //
    // match whole string
    //
    UnicodeString d4 = "abc";
    matcher->reset(d4);
    dest = matcher->replaceFirst("xyz", status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(dest == "xyz");

    dest = matcher->replaceAll("xyz", status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(dest == "xyz");

    //
    // Capture Group, simple case
    //
    UnicodeString       re2("a(..)");
    RegexPattern *pat2 = RegexPattern::compile(re2, flags, pe, status);
    REGEX_CHECK_STATUS;
    UnicodeString d5 = "abcdefg";
    RegexMatcher *matcher2 = pat2->matcher(d5, status);
    REGEX_CHECK_STATUS;
    dest = matcher2->replaceFirst("$1$1", status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(dest == "bcbcdefg");

    dest = matcher2->replaceFirst(UNICODE_STRING_SIMPLE("The value of \\$1 is $1."), status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(dest == "The value of $1 is bc.defg");

    dest = matcher2->replaceFirst("$ by itself, no group number $$$", status);
    REGEX_ASSERT(U_FAILURE(status));
    status = U_ZERO_ERROR;

    UnicodeString replacement = UNICODE_STRING_SIMPLE("Supplemental Digit 1 $\\U0001D7CF.");
    replacement = replacement.unescape();
    dest = matcher2->replaceFirst(replacement, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(dest == "Supplemental Digit 1 bc.defg");

    REGEX_ASSERT_FAIL(matcher2->replaceFirst("bad capture group number $5...",status), U_INDEX_OUTOFBOUNDS_ERROR);


    //
    // Replacement String with \u hex escapes
    //
    {
        UnicodeString  src = "abc 1 abc 2 abc 3";
        UnicodeString  substitute = UNICODE_STRING_SIMPLE("--\\u0043--");
        matcher->reset(src);
        UnicodeString  result = matcher->replaceAll(substitute, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == "--C-- 1 --C-- 2 --C-- 3");
    }
    {
        UnicodeString  src = "abc !";
        UnicodeString  substitute = UNICODE_STRING_SIMPLE("--\\U00010000--");
        matcher->reset(src);
        UnicodeString  result = matcher->replaceAll(substitute, status);
        REGEX_CHECK_STATUS;
        UnicodeString expected = UnicodeString("--");
        expected.append((UChar32)0x10000);
        expected.append("-- !");
        REGEX_ASSERT(result == expected);
    }
    // TODO:  need more through testing of capture substitutions.

    // Bug 4057
    //
    {
        status = U_ZERO_ERROR;
        UnicodeString s = "The matches start with ss and end with ee ss stuff ee fin";
        RegexMatcher m("ss(.*?)ee", 0, status);
        REGEX_CHECK_STATUS;
        UnicodeString result;

        // Multiple finds do NOT bump up the previous appendReplacement position.
        m.reset(s);
        m.find();
        m.find();
        m.appendReplacement(result, "ooh", status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == "The matches start with ss and end with ee ooh");

        // After a reset into the interior of a string, appendReplacemnt still starts at beginning.
        status = U_ZERO_ERROR;
        result.truncate(0);
        m.reset(10, status);
        m.find();
        m.find();
        m.appendReplacement(result, "ooh", status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == "The matches start with ss and end with ee ooh");

        // find() at interior of string, appendReplacemnt still starts at beginning.
        status = U_ZERO_ERROR;
        result.truncate(0);
        m.reset();
        m.find(10, status);
        m.find();
        m.appendReplacement(result, "ooh", status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == "The matches start with ss and end with ee ooh");

        m.appendTail(result);
        REGEX_ASSERT(result == "The matches start with ss and end with ee ooh fin");

    }

    delete matcher2;
    delete pat2;
    delete matcher;
    delete pat;
}


//---------------------------------------------------------------------------
//
//      API_Pattern       Test that the API for class RegexPattern is
//                        present and nominally working.
//
//---------------------------------------------------------------------------
void RegexTest::API_Pattern() {
    RegexPattern        pata;    // Test default constructor to not crash.
    RegexPattern        patb;

    REGEX_ASSERT(pata == patb);
    REGEX_ASSERT(pata == pata);

    UnicodeString re1("abc[a-l][m-z]");
    UnicodeString re2("def");
    UErrorCode    status = U_ZERO_ERROR;
    UParseError   pe;

    RegexPattern        *pat1 = RegexPattern::compile(re1, 0, pe, status);
    RegexPattern        *pat2 = RegexPattern::compile(re2, 0, pe, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(*pat1 == *pat1);
    REGEX_ASSERT(*pat1 != pata);

    // Assign
    patb = *pat1;
    REGEX_ASSERT(patb == *pat1);

    // Copy Construct
    RegexPattern patc(*pat1);
    REGEX_ASSERT(patc == *pat1);
    REGEX_ASSERT(patb == patc);
    REGEX_ASSERT(pat1 != pat2);
    patb = *pat2;
    REGEX_ASSERT(patb != patc);
    REGEX_ASSERT(patb == *pat2);

    // Compile with no flags.
    RegexPattern         *pat1a = RegexPattern::compile(re1, pe, status);
    REGEX_ASSERT(*pat1a == *pat1);

    REGEX_ASSERT(pat1a->flags() == 0);

    // Compile with different flags should be not equal
    RegexPattern        *pat1b = RegexPattern::compile(re1, UREGEX_CASE_INSENSITIVE, pe, status);
    REGEX_CHECK_STATUS;

    REGEX_ASSERT(*pat1b != *pat1a);
    REGEX_ASSERT(pat1b->flags() == UREGEX_CASE_INSENSITIVE);
    REGEX_ASSERT(pat1a->flags() == 0);
    delete pat1b;

    // clone
    RegexPattern *pat1c = pat1->clone();
    REGEX_ASSERT(*pat1c == *pat1);
    REGEX_ASSERT(*pat1c != *pat2);

    delete pat1c;
    delete pat1a;
    delete pat1;
    delete pat2;


    //
    //   Verify that a matcher created from a cloned pattern works.
    //     (Jitterbug 3423)
    //
    {
        UErrorCode     status     = U_ZERO_ERROR;
        RegexPattern  *pSource    = RegexPattern::compile(UNICODE_STRING_SIMPLE("\\p{L}+"), 0, status);
        RegexPattern  *pClone     = pSource->clone();
        delete         pSource;
        RegexMatcher  *mFromClone = pClone->matcher(status);
        REGEX_CHECK_STATUS;
        UnicodeString s = "Hello World";
        mFromClone->reset(s);
        REGEX_ASSERT(mFromClone->find() == true);
        REGEX_ASSERT(mFromClone->group(status) == "Hello");
        REGEX_ASSERT(mFromClone->find() == true);
        REGEX_ASSERT(mFromClone->group(status) == "World");
        REGEX_ASSERT(mFromClone->find() == false);
        delete mFromClone;
        delete pClone;
    }

    //
    //   matches convenience API
    //
    REGEX_ASSERT(RegexPattern::matches(".*", "random input", pe, status) == true);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(RegexPattern::matches("abc", "random input", pe, status) == false);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(RegexPattern::matches(".*nput", "random input", pe, status) == true);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(RegexPattern::matches("random input", "random input", pe, status) == true);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(RegexPattern::matches(".*u", "random input", pe, status) == false);
    REGEX_CHECK_STATUS;
    status = U_INDEX_OUTOFBOUNDS_ERROR;
    REGEX_ASSERT(RegexPattern::matches("abc", "abc", pe, status) == false);
    REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);


    //
    // Split()
    //
    status = U_ZERO_ERROR;
    pat1 = RegexPattern::compile(" +",  pe, status);
    REGEX_CHECK_STATUS;
    UnicodeString  fields[10];

    int32_t n;
    n = pat1->split("Now is the time", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==4);
    REGEX_ASSERT(fields[0]=="Now");
    REGEX_ASSERT(fields[1]=="is");
    REGEX_ASSERT(fields[2]=="the");
    REGEX_ASSERT(fields[3]=="time");
    REGEX_ASSERT(fields[4]=="");

    n = pat1->split("Now is the time", fields, 2, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==2);
    REGEX_ASSERT(fields[0]=="Now");
    REGEX_ASSERT(fields[1]=="is the time");
    REGEX_ASSERT(fields[2]=="the");   // left over from previous test

    fields[1] = "*";
    status = U_ZERO_ERROR;
    n = pat1->split("Now is the time", fields, 1, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==1);
    REGEX_ASSERT(fields[0]=="Now is the time");
    REGEX_ASSERT(fields[1]=="*");
    status = U_ZERO_ERROR;

    n = pat1->split("    Now       is the time   ", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==6);
    REGEX_ASSERT(fields[0]=="");
    REGEX_ASSERT(fields[1]=="Now");
    REGEX_ASSERT(fields[2]=="is");
    REGEX_ASSERT(fields[3]=="the");
    REGEX_ASSERT(fields[4]=="time");
    REGEX_ASSERT(fields[5]=="");

    n = pat1->split("     ", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==2);
    REGEX_ASSERT(fields[0]=="");
    REGEX_ASSERT(fields[1]=="");

    fields[0] = "foo";
    n = pat1->split("", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==0);
    REGEX_ASSERT(fields[0]=="foo");

    delete pat1;

    //  split, with a pattern with (capture)
    pat1 = RegexPattern::compile(UNICODE_STRING_SIMPLE("<(\\w*)>"),  pe, status);
    REGEX_CHECK_STATUS;

    status = U_ZERO_ERROR;
    n = pat1->split("<a>Now is <b>the time<c>", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==7);
    REGEX_ASSERT(fields[0]=="");
    REGEX_ASSERT(fields[1]=="a");
    REGEX_ASSERT(fields[2]=="Now is ");
    REGEX_ASSERT(fields[3]=="b");
    REGEX_ASSERT(fields[4]=="the time");
    REGEX_ASSERT(fields[5]=="c");
    REGEX_ASSERT(fields[6]=="");
    REGEX_ASSERT(status==U_ZERO_ERROR);

    n = pat1->split("  <a>Now is <b>the time<c>", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==7);
    REGEX_ASSERT(fields[0]=="  ");
    REGEX_ASSERT(fields[1]=="a");
    REGEX_ASSERT(fields[2]=="Now is ");
    REGEX_ASSERT(fields[3]=="b");
    REGEX_ASSERT(fields[4]=="the time");
    REGEX_ASSERT(fields[5]=="c");
    REGEX_ASSERT(fields[6]=="");

    status = U_ZERO_ERROR;
    fields[6] = "foo";
    n = pat1->split("  <a>Now is <b>the time<c>", fields, 6, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==6);
    REGEX_ASSERT(fields[0]=="  ");
    REGEX_ASSERT(fields[1]=="a");
    REGEX_ASSERT(fields[2]=="Now is ");
    REGEX_ASSERT(fields[3]=="b");
    REGEX_ASSERT(fields[4]=="the time");
    REGEX_ASSERT(fields[5]=="");  // All text following "<c>" field delimiter.
    REGEX_ASSERT(fields[6]=="foo");

    status = U_ZERO_ERROR;
    fields[5] = "foo";
    n = pat1->split("  <a>Now is <b>the time<c>", fields, 5, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==5);
    REGEX_ASSERT(fields[0]=="  ");
    REGEX_ASSERT(fields[1]=="a");
    REGEX_ASSERT(fields[2]=="Now is ");
    REGEX_ASSERT(fields[3]=="b");
    REGEX_ASSERT(fields[4]=="the time<c>");
    REGEX_ASSERT(fields[5]=="foo");

    status = U_ZERO_ERROR;
    fields[5] = "foo";
    n = pat1->split("  <a>Now is <b>the time", fields, 5, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==5);
    REGEX_ASSERT(fields[0]=="  ");
    REGEX_ASSERT(fields[1]=="a");
    REGEX_ASSERT(fields[2]=="Now is ");
    REGEX_ASSERT(fields[3]=="b");
    REGEX_ASSERT(fields[4]=="the time");
    REGEX_ASSERT(fields[5]=="foo");

    status = U_ZERO_ERROR;
    n = pat1->split("  <a>Now is <b>the time<c>", fields, 4, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==4);
    REGEX_ASSERT(fields[0]=="  ");
    REGEX_ASSERT(fields[1]=="a");
    REGEX_ASSERT(fields[2]=="Now is ");
    REGEX_ASSERT(fields[3]=="the time<c>");
    status = U_ZERO_ERROR;
    delete pat1;

    pat1 = RegexPattern::compile("([-,])",  pe, status);
    REGEX_CHECK_STATUS;
    n = pat1->split("1-10,20", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==5);
    REGEX_ASSERT(fields[0]=="1");
    REGEX_ASSERT(fields[1]=="-");
    REGEX_ASSERT(fields[2]=="10");
    REGEX_ASSERT(fields[3]==",");
    REGEX_ASSERT(fields[4]=="20");
    delete pat1;

    // Test split of string with empty trailing fields
    pat1 = RegexPattern::compile(",", pe, status);
    REGEX_CHECK_STATUS;
    n = pat1->split("a,b,c,", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==4);
    REGEX_ASSERT(fields[0]=="a");
    REGEX_ASSERT(fields[1]=="b");
    REGEX_ASSERT(fields[2]=="c");
    REGEX_ASSERT(fields[3]=="");

    n = pat1->split("a,,,", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==4);
    REGEX_ASSERT(fields[0]=="a");
    REGEX_ASSERT(fields[1]=="");
    REGEX_ASSERT(fields[2]=="");
    REGEX_ASSERT(fields[3]=="");
    delete pat1;

    // Split Separator with zero length match.
    pat1 = RegexPattern::compile(":?", pe, status);
    REGEX_CHECK_STATUS;
    n = pat1->split("abc", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==5);
    REGEX_ASSERT(fields[0]=="");
    REGEX_ASSERT(fields[1]=="a");
    REGEX_ASSERT(fields[2]=="b");
    REGEX_ASSERT(fields[3]=="c");
    REGEX_ASSERT(fields[4]=="");

    delete pat1;

    //
    // RegexPattern::pattern()
    //
    pat1 = new RegexPattern();
    REGEX_ASSERT(pat1->pattern() == "");
    delete pat1;

    pat1 = RegexPattern::compile("(Hello, world)*",  pe, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(pat1->pattern() == "(Hello, world)*");
    delete pat1;


    //
    // classID functions
    //
    pat1 = RegexPattern::compile("(Hello, world)*",  pe, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(pat1->getDynamicClassID() == RegexPattern::getStaticClassID());
    REGEX_ASSERT(pat1->getDynamicClassID() != nullptr);
    UnicodeString Hello("Hello, world.");
    RegexMatcher *m = pat1->matcher(Hello, status);
    REGEX_ASSERT(pat1->getDynamicClassID() != m->getDynamicClassID());
    REGEX_ASSERT(m->getDynamicClassID() == RegexMatcher::getStaticClassID());
    REGEX_ASSERT(m->getDynamicClassID() != nullptr);
    delete m;
    delete pat1;

}

//---------------------------------------------------------------------------
//
//      API_Match_UTF8   Test that the alternate engine for class RegexMatcher
//                       is present and working, but excluding functions
//                       implementing replace operations.
//
//---------------------------------------------------------------------------
void RegexTest::API_Match_UTF8() {
    UParseError         pe;
    UErrorCode          status=U_ZERO_ERROR;
    int32_t             flags = 0;

    //
    // Debug - slide failing test cases early
    //
#if 0
    {
    }
    return;
#endif

    //
    // Simple pattern compilation
    //
    {
        UText               re = UTEXT_INITIALIZER;
        regextst_openUTF8FromInvariant(&re, "abc", -1, &status);
        REGEX_VERBOSE_TEXT(&re);
        RegexPattern        *pat2;
        pat2 = RegexPattern::compile(&re, flags, pe, status);
        REGEX_CHECK_STATUS;

        UText input1 = UTEXT_INITIALIZER;
        UText input2 = UTEXT_INITIALIZER;
        UText empty  = UTEXT_INITIALIZER;
        regextst_openUTF8FromInvariant(&input1, "abcdef this is a test", -1, &status);
        REGEX_VERBOSE_TEXT(&input1);
        regextst_openUTF8FromInvariant(&input2, "not abc", -1, &status);
        REGEX_VERBOSE_TEXT(&input2);
        utext_openUChars(&empty, nullptr, 0, &status);

        int32_t input1Len = static_cast<int32_t>(strlen("abcdef this is a test")); /* TODO: why not nativelen (input1) ? */
        int32_t input2Len = static_cast<int32_t>(strlen("not abc"));


        //
        // Matcher creation and reset.
        //
        RegexMatcher *m1 = &pat2->matcher(status)->reset(&input1);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(m1->lookingAt(status) == true);
        const char str_abcdefthisisatest[] = { 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x20, 0x74, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x20, 0x74, 0x65, 0x73, 0x74, 0x00 }; /* abcdef this is a test */
        REGEX_ASSERT_UTEXT_UTF8(str_abcdefthisisatest, m1->inputText());
        m1->reset(&input2);
        REGEX_ASSERT(m1->lookingAt(status) == false);
        const char str_notabc[] = { 0x6e, 0x6f, 0x74, 0x20, 0x61, 0x62, 0x63, 0x00 }; /* not abc */
        REGEX_ASSERT_UTEXT_UTF8(str_notabc, m1->inputText());
        m1->reset(&input1);
        REGEX_ASSERT_UTEXT_UTF8(str_abcdefthisisatest, m1->inputText());
        REGEX_ASSERT(m1->lookingAt(status) == true);
        m1->reset(&empty);
        REGEX_ASSERT(m1->lookingAt(status) == false);
        REGEX_ASSERT(utext_nativeLength(&empty) == 0);

        //
        //  reset(pos, status)
        //
        m1->reset(&input1);
        m1->reset(4, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT_UTEXT_UTF8(str_abcdefthisisatest, m1->inputText());
        REGEX_ASSERT(m1->lookingAt(status) == true);

        m1->reset(-1, status);
        REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);
        status = U_ZERO_ERROR;

        m1->reset(0, status);
        REGEX_CHECK_STATUS;
        status = U_ZERO_ERROR;

        m1->reset(input1Len-1, status);
        REGEX_CHECK_STATUS;
        status = U_ZERO_ERROR;

        m1->reset(input1Len, status);
        REGEX_CHECK_STATUS;
        status = U_ZERO_ERROR;

        m1->reset(input1Len+1, status);
        REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);
        status = U_ZERO_ERROR;

        //
        // match(pos, status)
        //
        m1->reset(&input2);
        REGEX_ASSERT(m1->matches(4, status) == true);
        m1->reset();
        REGEX_ASSERT(m1->matches(3, status) == false);
        m1->reset();
        REGEX_ASSERT(m1->matches(5, status) == false);
        REGEX_ASSERT(m1->matches(4, status) == true);
        REGEX_ASSERT(m1->matches(-1, status) == false);
        REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);

        // Match() at end of string should fail, but should not
        //  be an error.
        status = U_ZERO_ERROR;
        REGEX_ASSERT(m1->matches(input2Len, status) == false);
        REGEX_CHECK_STATUS;

        // Match beyond end of string should fail with an error.
        status = U_ZERO_ERROR;
        REGEX_ASSERT(m1->matches(input2Len+1, status) == false);
        REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);

        // Successful match at end of string.
        {
            status = U_ZERO_ERROR;
            RegexMatcher m("A?", 0, status);  // will match zero length string.
            REGEX_CHECK_STATUS;
            m.reset(&input1);
            REGEX_ASSERT(m.matches(input1Len, status) == true);
            REGEX_CHECK_STATUS;
            m.reset(&empty);
            REGEX_ASSERT(m.matches(0, status) == true);
            REGEX_CHECK_STATUS;
        }


        //
        // lookingAt(pos, status)
        //
        status = U_ZERO_ERROR;
        m1->reset(&input2);  // "not abc"
        REGEX_ASSERT(m1->lookingAt(4, status) == true);
        REGEX_ASSERT(m1->lookingAt(5, status) == false);
        REGEX_ASSERT(m1->lookingAt(3, status) == false);
        REGEX_ASSERT(m1->lookingAt(4, status) == true);
        REGEX_ASSERT(m1->lookingAt(-1, status) == false);
        REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);
        status = U_ZERO_ERROR;
        REGEX_ASSERT(m1->lookingAt(input2Len, status) == false);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(m1->lookingAt(input2Len+1, status) == false);
        REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);

        delete m1;
        delete pat2;

        utext_close(&re);
        utext_close(&input1);
        utext_close(&input2);
        utext_close(&empty);
    }


    //
    // Capture Group.
    //     RegexMatcher::start();
    //     RegexMatcher::end();
    //     RegexMatcher::groupCount();
    //
    {
        int32_t             flags=0;
        UParseError         pe;
        UErrorCode          status=U_ZERO_ERROR;
        UText               re=UTEXT_INITIALIZER;
        const char str_01234567_pat[] = { 0x30, 0x31, 0x28, 0x32, 0x33, 0x28, 0x34, 0x35, 0x29, 0x36, 0x37, 0x29, 0x28, 0x2e, 0x2a, 0x29, 0x00 }; /* 01(23(45)67)(.*) */
        utext_openUTF8(&re, str_01234567_pat, -1, &status);

        RegexPattern *pat = RegexPattern::compile(&re, flags, pe, status);
        REGEX_CHECK_STATUS;

        UText input = UTEXT_INITIALIZER;
        const char str_0123456789[] = { 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00 }; /* 0123456789 */
        utext_openUTF8(&input, str_0123456789, -1, &status);

        RegexMatcher *matcher = &pat->matcher(status)->reset(&input);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(matcher->lookingAt(status) == true);
        static const int32_t matchStarts[] = {0,  2, 4, 8};
        static const int32_t matchEnds[]   = {10, 8, 6, 10};
        int32_t i;
        for (i=0; i<4; i++) {
            int32_t actualStart = matcher->start(i, status);
            REGEX_CHECK_STATUS;
            if (actualStart != matchStarts[i]) {
                errln("RegexTest failure at %s:%d, index %d.  Expected %d, got %d\n",
                      __FILE__, __LINE__, i, matchStarts[i], actualStart);
            }
            int32_t actualEnd = matcher->end(i, status);
            REGEX_CHECK_STATUS;
            if (actualEnd != matchEnds[i]) {
                errln("RegexTest failure at %s:%d index %d.  Expected %d, got %d\n",
                      __FILE__, __LINE__, i, matchEnds[i], actualEnd);
            }
        }

        REGEX_ASSERT(matcher->start(0, status) == matcher->start(status));
        REGEX_ASSERT(matcher->end(0, status) == matcher->end(status));

        REGEX_ASSERT_FAIL(matcher->start(-1, status), U_INDEX_OUTOFBOUNDS_ERROR);
        REGEX_ASSERT_FAIL(matcher->start( 4, status), U_INDEX_OUTOFBOUNDS_ERROR);
        matcher->reset();
        REGEX_ASSERT_FAIL(matcher->start( 0, status), U_REGEX_INVALID_STATE);

        matcher->lookingAt(status);

        UnicodeString dest;
        UText destText = UTEXT_INITIALIZER;
        utext_openUnicodeString(&destText, &dest, &status);
        UText *result;
        //const char str_0123456789[] = { 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x00 }; /* 0123456789 */
        //  Test shallow-clone API
        int64_t   group_len;
        result = matcher->group(nullptr, group_len, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT_UTEXT_UTF8(str_0123456789, result);
        utext_close(result);
        result = matcher->group(0, &destText, group_len, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == &destText);
        REGEX_ASSERT_UTEXT_UTF8(str_0123456789, result);
        //  destText is now immutable, reopen it
        utext_close(&destText);
        utext_openUnicodeString(&destText, &dest, &status);

        int64_t length;
        result = matcher->group(0, nullptr, length, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT_UTEXT_UTF8(str_0123456789, result);
        utext_close(result);
        result = matcher->group(0, &destText, length, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == &destText);
        REGEX_ASSERT(utext_getNativeIndex(result) == 0);
        REGEX_ASSERT(length == 10);
        REGEX_ASSERT_UTEXT_INVARIANT("0123456789", result);

        // Capture Group 1 == "234567"
        result = matcher->group(1, nullptr, length, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(utext_getNativeIndex(result) == 2);
        REGEX_ASSERT(length == 6);
        REGEX_ASSERT_UTEXT_INVARIANT("0123456789", result);
        utext_close(result);

        result = matcher->group(1, &destText, length, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == &destText);
        REGEX_ASSERT(utext_getNativeIndex(result) == 2);
        REGEX_ASSERT(length == 6);
        REGEX_ASSERT_UTEXT_INVARIANT("0123456789", result);
        utext_close(result);

        // Capture Group 2 == "45"
        result = matcher->group(2, nullptr, length, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(utext_getNativeIndex(result) == 4);
        REGEX_ASSERT(length == 2);
        REGEX_ASSERT_UTEXT_INVARIANT("0123456789", result);
        utext_close(result);

        result = matcher->group(2, &destText, length, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == &destText);
        REGEX_ASSERT(utext_getNativeIndex(result) == 4);
        REGEX_ASSERT(length == 2);
        REGEX_ASSERT_UTEXT_INVARIANT("0123456789", result);
        utext_close(result);

        // Capture Group 3 == "89"
        result = matcher->group(3, nullptr, length, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(utext_getNativeIndex(result) == 8);
        REGEX_ASSERT(length == 2);
        REGEX_ASSERT_UTEXT_INVARIANT("0123456789", result);
        utext_close(result);

        result = matcher->group(3, &destText, length, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == &destText);
        REGEX_ASSERT(utext_getNativeIndex(result) == 8);
        REGEX_ASSERT(length == 2);
        REGEX_ASSERT_UTEXT_INVARIANT("0123456789", result);
        utext_close(result);

        // Capture Group number out of range.
        status = U_ZERO_ERROR;
        REGEX_ASSERT_FAIL(matcher->group(-1, status), U_INDEX_OUTOFBOUNDS_ERROR);
        status = U_ZERO_ERROR;
        REGEX_ASSERT_FAIL(matcher->group( 4, status), U_INDEX_OUTOFBOUNDS_ERROR);
        status = U_ZERO_ERROR;
        matcher->reset();
        REGEX_ASSERT_FAIL(matcher->group( 0, status), U_REGEX_INVALID_STATE);

        delete matcher;
        delete pat;

        utext_close(&destText);
        utext_close(&input);
        utext_close(&re);
    }

    //
    //  find
    //
    {
        int32_t             flags=0;
        UParseError         pe;
        UErrorCode          status=U_ZERO_ERROR;
        UText               re=UTEXT_INITIALIZER;
        const char str_abc[] = { 0x61, 0x62, 0x63, 0x00 }; /* abc */
        utext_openUTF8(&re, str_abc, -1, &status);

        RegexPattern *pat = RegexPattern::compile(&re, flags, pe, status);
        REGEX_CHECK_STATUS;
        UText input = UTEXT_INITIALIZER;
        const char str_abcabcabc[] = { 0x2e, 0x61, 0x62, 0x63, 0x2e, 0x2e, 0x61, 0x62, 0x63, 0x2e, 0x2e, 0x2e, 0x61, 0x62, 0x63, 0x2e, 0x2e, 0x00 }; /* .abc..abc...abc.. */
        utext_openUTF8(&input, str_abcabcabc, -1, &status);
        //                      012345678901234567

        RegexMatcher *matcher = &pat->matcher(status)->reset(&input);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(matcher->find());
        REGEX_ASSERT(matcher->start(status) == 1);
        REGEX_ASSERT(matcher->find());
        REGEX_ASSERT(matcher->start(status) == 6);
        REGEX_ASSERT(matcher->find());
        REGEX_ASSERT(matcher->start(status) == 12);
        REGEX_ASSERT(matcher->find() == false);
        REGEX_ASSERT(matcher->find() == false);

        matcher->reset();
        REGEX_ASSERT(matcher->find());
        REGEX_ASSERT(matcher->start(status) == 1);

        REGEX_ASSERT(matcher->find(0, status));
        REGEX_ASSERT(matcher->start(status) == 1);
        REGEX_ASSERT(matcher->find(1, status));
        REGEX_ASSERT(matcher->start(status) == 1);
        REGEX_ASSERT(matcher->find(2, status));
        REGEX_ASSERT(matcher->start(status) == 6);
        REGEX_ASSERT(matcher->find(12, status));
        REGEX_ASSERT(matcher->start(status) == 12);
        REGEX_ASSERT(matcher->find(13, status) == false);
        REGEX_ASSERT(matcher->find(16, status) == false);
        REGEX_ASSERT(matcher->find(17, status) == false);
        REGEX_ASSERT_FAIL(matcher->start(status), U_REGEX_INVALID_STATE);

        status = U_ZERO_ERROR;
        REGEX_ASSERT_FAIL(matcher->find(-1, status), U_INDEX_OUTOFBOUNDS_ERROR);
        status = U_ZERO_ERROR;
        REGEX_ASSERT_FAIL(matcher->find(18, status), U_INDEX_OUTOFBOUNDS_ERROR);

        REGEX_ASSERT(matcher->groupCount() == 0);

        delete matcher;
        delete pat;

        utext_close(&input);
        utext_close(&re);
    }


    //
    //  find, with \G in pattern (true if at the end of a previous match).
    //
    {
        int32_t             flags=0;
        UParseError         pe;
        UErrorCode          status=U_ZERO_ERROR;
        UText               re=UTEXT_INITIALIZER;
        const char str_Gabcabc[] = { 0x2e, 0x2a, 0x3f, 0x28, 0x3f, 0x3a, 0x28, 0x5c, 0x47, 0x61, 0x62, 0x63, 0x29, 0x7c, 0x28, 0x61, 0x62, 0x63, 0x29, 0x29, 0x00 }; /* .*?(?:(\\Gabc)|(abc)) */
        utext_openUTF8(&re, str_Gabcabc, -1, &status);

        RegexPattern *pat = RegexPattern::compile(&re, flags, pe, status);

        REGEX_CHECK_STATUS;
        UText input = UTEXT_INITIALIZER;
        const char str_abcabcabc[] = { 0x2e, 0x61, 0x62, 0x63, 0x61, 0x62, 0x63, 0x2e, 0x61, 0x62, 0x63, 0x2e, 0x2e, 0x00 }; /* .abcabc.abc.. */
        utext_openUTF8(&input, str_abcabcabc, -1, &status);
        //                      012345678901234567

        RegexMatcher *matcher = &pat->matcher(status)->reset(&input);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(matcher->find());
        REGEX_ASSERT(matcher->start(status) == 0);
        REGEX_ASSERT(matcher->start(1, status) == -1);
        REGEX_ASSERT(matcher->start(2, status) == 1);

        REGEX_ASSERT(matcher->find());
        REGEX_ASSERT(matcher->start(status) == 4);
        REGEX_ASSERT(matcher->start(1, status) == 4);
        REGEX_ASSERT(matcher->start(2, status) == -1);
        REGEX_CHECK_STATUS;

        delete matcher;
        delete pat;

        utext_close(&input);
        utext_close(&re);
    }

    //
    //   find with zero length matches, match position should bump ahead
    //     to prevent loops.
    //
    {
        int32_t                 i;
        UErrorCode          status=U_ZERO_ERROR;
        RegexMatcher        m("(?= ?)", 0, status);   // This pattern will zero-length matches anywhere,
                                                      //   using an always-true look-ahead.
        REGEX_CHECK_STATUS;
        UText s = UTEXT_INITIALIZER;
        utext_openUTF8(&s, "    ", -1, &status);
        m.reset(&s);
        for (i=0; ; i++) {
            if (m.find() == false) {
                break;
            }
            REGEX_ASSERT(m.start(status) == i);
            REGEX_ASSERT(m.end(status) == i);
        }
        REGEX_ASSERT(i==5);

        // Check that the bump goes over characters outside the BMP OK
        // "\\U00010001\\U00010002\\U00010003\\U00010004".unescape()...in UTF-8
        unsigned char aboveBMP[] = {0xF0, 0x90, 0x80, 0x81, 0xF0, 0x90, 0x80, 0x82, 0xF0, 0x90, 0x80, 0x83, 0xF0, 0x90, 0x80, 0x84, 0x00};
        utext_openUTF8(&s, (char *)aboveBMP, -1, &status);
        m.reset(&s);
        for (i=0; ; i+=4) {
            if (m.find() == false) {
                break;
            }
            REGEX_ASSERT(m.start(status) == i);
            REGEX_ASSERT(m.end(status) == i);
        }
        REGEX_ASSERT(i==20);

        utext_close(&s);
    }
    {
        // find() loop breaking test.
        //        with pattern of /.?/, should see a series of one char matches, then a single
        //        match of zero length at the end of the input string.
        int32_t                 i;
        UErrorCode          status=U_ZERO_ERROR;
        RegexMatcher        m(".?", 0, status);
        REGEX_CHECK_STATUS;
        UText s = UTEXT_INITIALIZER;
        utext_openUTF8(&s, "    ", -1, &status);
        m.reset(&s);
        for (i=0; ; i++) {
            if (m.find() == false) {
                break;
            }
            REGEX_ASSERT(m.start(status) == i);
            REGEX_ASSERT(m.end(status) == (i<4 ? i+1 : i));
        }
        REGEX_ASSERT(i==5);

        utext_close(&s);
    }


    //
    // Matchers with no input string behave as if they had an empty input string.
    //

    {
        UErrorCode status = U_ZERO_ERROR;
        RegexMatcher  m(".?", 0, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(m.find());
        REGEX_ASSERT(m.start(status) == 0);
        REGEX_ASSERT(m.input() == "");
    }
    {
        UErrorCode status = U_ZERO_ERROR;
        RegexPattern  *p = RegexPattern::compile(".", 0, status);
        RegexMatcher  *m = p->matcher(status);
        REGEX_CHECK_STATUS;

        REGEX_ASSERT(m->find() == false);
        REGEX_ASSERT(utext_nativeLength(m->inputText()) == 0);
        delete m;
        delete p;
    }

    //
    // Regions
    //
    {
        UErrorCode status = U_ZERO_ERROR;
        UText testPattern = UTEXT_INITIALIZER;
        UText testText    = UTEXT_INITIALIZER;
        regextst_openUTF8FromInvariant(&testPattern, ".*", -1, &status);
        REGEX_VERBOSE_TEXT(&testPattern);
        regextst_openUTF8FromInvariant(&testText, "This is test data", -1, &status);
        REGEX_VERBOSE_TEXT(&testText);

        RegexMatcher m(&testPattern, &testText, 0, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(m.regionStart() == 0);
        REGEX_ASSERT(m.regionEnd() == (int32_t)strlen("This is test data"));
        REGEX_ASSERT(m.hasTransparentBounds() == false);
        REGEX_ASSERT(m.hasAnchoringBounds() == true);

        m.region(2,4, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(m.matches(status));
        REGEX_ASSERT(m.start(status)==2);
        REGEX_ASSERT(m.end(status)==4);
        REGEX_CHECK_STATUS;

        m.reset();
        REGEX_ASSERT(m.regionStart() == 0);
        REGEX_ASSERT(m.regionEnd() == (int32_t)strlen("This is test data"));

        regextst_openUTF8FromInvariant(&testText, "short", -1, &status);
        REGEX_VERBOSE_TEXT(&testText);
        m.reset(&testText);
        REGEX_ASSERT(m.regionStart() == 0);
        REGEX_ASSERT(m.regionEnd() == (int32_t)strlen("short"));

        REGEX_ASSERT(m.hasAnchoringBounds() == true);
        REGEX_ASSERT(&m == &m.useAnchoringBounds(false));
        REGEX_ASSERT(m.hasAnchoringBounds() == false);
        REGEX_ASSERT(&m == &m.reset());
        REGEX_ASSERT(m.hasAnchoringBounds() == false);

        REGEX_ASSERT(&m == &m.useAnchoringBounds(true));
        REGEX_ASSERT(m.hasAnchoringBounds() == true);
        REGEX_ASSERT(&m == &m.reset());
        REGEX_ASSERT(m.hasAnchoringBounds() == true);

        REGEX_ASSERT(m.hasTransparentBounds() == false);
        REGEX_ASSERT(&m == &m.useTransparentBounds(true));
        REGEX_ASSERT(m.hasTransparentBounds() == true);
        REGEX_ASSERT(&m == &m.reset());
        REGEX_ASSERT(m.hasTransparentBounds() == true);

        REGEX_ASSERT(&m == &m.useTransparentBounds(false));
        REGEX_ASSERT(m.hasTransparentBounds() == false);
        REGEX_ASSERT(&m == &m.reset());
        REGEX_ASSERT(m.hasTransparentBounds() == false);

        utext_close(&testText);
        utext_close(&testPattern);
    }

    //
    // hitEnd() and requireEnd()
    //
    {
        UErrorCode status = U_ZERO_ERROR;
        UText testPattern = UTEXT_INITIALIZER;
        UText testText    = UTEXT_INITIALIZER;
        const char str_[] = { 0x2e, 0x2a, 0x00 }; /* .* */
        const char str_aabb[] = { 0x61, 0x61, 0x62, 0x62, 0x00 }; /* aabb */
        utext_openUTF8(&testPattern, str_, -1, &status);
        utext_openUTF8(&testText, str_aabb, -1, &status);

        RegexMatcher m1(&testPattern, &testText,  0, status);
        REGEX_ASSERT(m1.lookingAt(status) == true);
        REGEX_ASSERT(m1.hitEnd() == true);
        REGEX_ASSERT(m1.requireEnd() == false);
        REGEX_CHECK_STATUS;

        status = U_ZERO_ERROR;
        const char str_a[] = { 0x61, 0x2a, 0x00 }; /* a* */
        utext_openUTF8(&testPattern, str_a, -1, &status);
        RegexMatcher m2(&testPattern, &testText, 0, status);
        REGEX_ASSERT(m2.lookingAt(status) == true);
        REGEX_ASSERT(m2.hitEnd() == false);
        REGEX_ASSERT(m2.requireEnd() == false);
        REGEX_CHECK_STATUS;

        status = U_ZERO_ERROR;
        const char str_dotstardollar[] = { 0x2e, 0x2a, 0x24, 0x00 }; /* .*$ */
        utext_openUTF8(&testPattern, str_dotstardollar, -1, &status);
        RegexMatcher m3(&testPattern, &testText, 0, status);
        REGEX_ASSERT(m3.lookingAt(status) == true);
        REGEX_ASSERT(m3.hitEnd() == true);
        REGEX_ASSERT(m3.requireEnd() == true);
        REGEX_CHECK_STATUS;

        utext_close(&testText);
        utext_close(&testPattern);
    }
}


//---------------------------------------------------------------------------
//
//      API_Replace_UTF8   API test for class RegexMatcher, testing the
//                         Replace family of functions.
//
//---------------------------------------------------------------------------
void RegexTest::API_Replace_UTF8() {
    //
    //  Replace
    //
    int32_t             flags=0;
    UParseError         pe;
    UErrorCode          status=U_ZERO_ERROR;

    UText               re=UTEXT_INITIALIZER;
    regextst_openUTF8FromInvariant(&re, "abc", -1, &status);
    REGEX_VERBOSE_TEXT(&re);
    RegexPattern *pat = RegexPattern::compile(&re, flags, pe, status);
    REGEX_CHECK_STATUS;

    char data[] = { 0x2e, 0x61, 0x62, 0x63, 0x2e, 0x2e, 0x61, 0x62, 0x63, 0x2e, 0x2e, 0x2e, 0x61, 0x62, 0x63, 0x2e, 0x2e, 0x00 }; /* .abc..abc...abc.. */
    //             012345678901234567
    UText dataText = UTEXT_INITIALIZER;
    utext_openUTF8(&dataText, data, -1, &status);
    REGEX_CHECK_STATUS;
    REGEX_VERBOSE_TEXT(&dataText);
    RegexMatcher *matcher = &pat->matcher(status)->reset(&dataText);

    //
    //  Plain vanilla matches.
    //
    UnicodeString  dest;
    UText destText = UTEXT_INITIALIZER;
    utext_openUnicodeString(&destText, &dest, &status);
    UText *result;

    UText replText = UTEXT_INITIALIZER;

    const char str_yz[] = { 0x79, 0x7a, 0x00 }; /* yz */
    utext_openUTF8(&replText, str_yz, -1, &status);
    REGEX_VERBOSE_TEXT(&replText);
    result = matcher->replaceFirst(&replText, nullptr, status);
    REGEX_CHECK_STATUS;
    const char str_yzabcabc[] = { 0x2e, 0x79, 0x7a, 0x2e, 0x2e, 0x61, 0x62, 0x63, 0x2e, 0x2e, 0x2e, 0x61, 0x62, 0x63, 0x2e, 0x2e, 0x00 }; /* .yz..abc...abc.. */
    REGEX_ASSERT_UTEXT_UTF8(str_yzabcabc, result);
    utext_close(result);
    result = matcher->replaceFirst(&replText, &destText, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(result == &destText);
    REGEX_ASSERT_UTEXT_UTF8(str_yzabcabc, result);

    result = matcher->replaceAll(&replText, nullptr, status);
    REGEX_CHECK_STATUS;
    const char str_yzyzyz[] = { 0x2e, 0x79, 0x7a, 0x2e, 0x2e, 0x79, 0x7a, 0x2e, 0x2e, 0x2e, 0x79, 0x7a, 0x2e, 0x2e, 0x00 }; /* .yz..yz...yz.. */
    REGEX_ASSERT_UTEXT_UTF8(str_yzyzyz, result);
    utext_close(result);

    utext_replace(&destText, 0, utext_nativeLength(&destText), nullptr, 0, &status);
    result = matcher->replaceAll(&replText, &destText, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(result == &destText);
    REGEX_ASSERT_UTEXT_UTF8(str_yzyzyz, result);

    //
    //  Plain vanilla non-matches.
    //
    const char str_abxabxabx[] = { 0x2e, 0x61, 0x62, 0x78, 0x2e, 0x2e, 0x61, 0x62, 0x78, 0x2e, 0x2e, 0x2e, 0x61, 0x62, 0x78, 0x2e, 0x2e, 0x00 }; /* .abx..abx...abx.. */
    utext_openUTF8(&dataText, str_abxabxabx, -1, &status);
    matcher->reset(&dataText);

    result = matcher->replaceFirst(&replText, nullptr, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT_UTEXT_UTF8(str_abxabxabx, result);
    utext_close(result);
    result = matcher->replaceFirst(&replText, &destText, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(result == &destText);
    REGEX_ASSERT_UTEXT_UTF8(str_abxabxabx, result);

    result = matcher->replaceAll(&replText, nullptr, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT_UTEXT_UTF8(str_abxabxabx, result);
    utext_close(result);
    utext_replace(&destText, 0, utext_nativeLength(&destText), nullptr, 0, &status);
    result = matcher->replaceAll(&replText, &destText, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(result == &destText);
    REGEX_ASSERT_UTEXT_UTF8(str_abxabxabx, result);

    //
    // Empty source string
    //
    utext_openUTF8(&dataText, nullptr, 0, &status);
    matcher->reset(&dataText);

    result = matcher->replaceFirst(&replText, nullptr, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT_UTEXT_UTF8("", result);
    utext_close(result);
    result = matcher->replaceFirst(&replText, &destText, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(result == &destText);
    REGEX_ASSERT_UTEXT_UTF8("", result);

    result = matcher->replaceAll(&replText, nullptr, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT_UTEXT_UTF8("", result);
    utext_close(result);
    result = matcher->replaceAll(&replText, &destText, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(result == &destText);
    REGEX_ASSERT_UTEXT_UTF8("", result);

    //
    // Empty substitution string
    //
    utext_openUTF8(&dataText, data, -1, &status); // ".abc..abc...abc.."
    matcher->reset(&dataText);

    utext_openUTF8(&replText, nullptr, 0, &status);
    result = matcher->replaceFirst(&replText, nullptr, status);
    REGEX_CHECK_STATUS;
    const char str_abcabc[] = { 0x2e, 0x2e, 0x2e, 0x61, 0x62, 0x63, 0x2e, 0x2e, 0x2e, 0x61, 0x62, 0x63, 0x2e, 0x2e, 0x00 }; /* ...abc...abc.. */
    REGEX_ASSERT_UTEXT_UTF8(str_abcabc, result);
    utext_close(result);
    result = matcher->replaceFirst(&replText, &destText, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(result == &destText);
    REGEX_ASSERT_UTEXT_UTF8(str_abcabc, result);

    result = matcher->replaceAll(&replText, nullptr, status);
    REGEX_CHECK_STATUS;
    const char str_dots[] = { 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x2e, 0x00 }; /* ........ */
    REGEX_ASSERT_UTEXT_UTF8(str_dots, result);
    utext_close(result);
    utext_replace(&destText, 0, utext_nativeLength(&destText), nullptr, 0, &status);
    result = matcher->replaceAll(&replText, &destText, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(result == &destText);
    REGEX_ASSERT_UTEXT_UTF8(str_dots, result);

    //
    // match whole string
    //
    const char str_abc[] = { 0x61, 0x62, 0x63, 0x00 }; /* abc */
    utext_openUTF8(&dataText, str_abc, -1, &status);
    matcher->reset(&dataText);

    const char str_xyz[] = { 0x78, 0x79, 0x7a, 0x00 }; /* xyz */
    utext_openUTF8(&replText, str_xyz, -1, &status);
    result = matcher->replaceFirst(&replText, nullptr, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT_UTEXT_UTF8(str_xyz, result);
    utext_close(result);
    utext_replace(&destText, 0, utext_nativeLength(&destText), nullptr, 0, &status);
    result = matcher->replaceFirst(&replText, &destText, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(result == &destText);
    REGEX_ASSERT_UTEXT_UTF8(str_xyz, result);

    result = matcher->replaceAll(&replText, nullptr, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT_UTEXT_UTF8(str_xyz, result);
    utext_close(result);
    utext_replace(&destText, 0, utext_nativeLength(&destText), nullptr, 0, &status);
    result = matcher->replaceAll(&replText, &destText, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(result == &destText);
    REGEX_ASSERT_UTEXT_UTF8(str_xyz, result);

    //
    // Capture Group, simple case
    //
    const char str_add[] = { 0x61, 0x28, 0x2e, 0x2e, 0x29, 0x00 }; /* a(..) */
    utext_openUTF8(&re, str_add, -1, &status);
    RegexPattern *pat2 = RegexPattern::compile(&re, flags, pe, status);
    REGEX_CHECK_STATUS;

    const char str_abcdefg[] = { 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x00 }; /* abcdefg */
    utext_openUTF8(&dataText, str_abcdefg, -1, &status);
    RegexMatcher *matcher2 = &pat2->matcher(status)->reset(&dataText);
    REGEX_CHECK_STATUS;

    const char str_11[] = { 0x24, 0x31, 0x24, 0x31, 0x00 }; /* $1$1 */
    utext_openUTF8(&replText, str_11, -1, &status);
    result = matcher2->replaceFirst(&replText, nullptr, status);
    REGEX_CHECK_STATUS;
    const char str_bcbcdefg[] = { 0x62, 0x63, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x00 }; /* bcbcdefg */
    REGEX_ASSERT_UTEXT_UTF8(str_bcbcdefg, result);
    utext_close(result);
    utext_replace(&destText, 0, utext_nativeLength(&destText), nullptr, 0, &status);
    result = matcher2->replaceFirst(&replText, &destText, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(result == &destText);
    REGEX_ASSERT_UTEXT_UTF8(str_bcbcdefg, result);

    const char str_v[24] = { 0x54, 0x68, 0x65, 0x20, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x20, 0x6f, 0x66, 0x20, 0x5c, 0x24, 0x31, 0x20, 0x69, 0x73, 0x20, 0x24, 0x31, 0x2e, 0x00 }; /* The value of \$1 is $1. */
    utext_openUTF8(&replText, str_v, -1, &status);
    REGEX_VERBOSE_TEXT(&replText);
    result = matcher2->replaceFirst(&replText, nullptr, status);
    REGEX_CHECK_STATUS;
    const char str_Thevalueof1isbcdefg[] = { 0x54, 0x68, 0x65, 0x20, 0x76, 0x61, 0x6c, 0x75, 0x65, 0x20, 0x6f, 0x66, 0x20, 0x24, 0x31, 0x20, 0x69, 0x73, 0x20, 0x62, 0x63, 0x2e, 0x64, 0x65, 0x66, 0x67, 0x00 }; /* The value of $1 is bc.defg */
    REGEX_ASSERT_UTEXT_UTF8(str_Thevalueof1isbcdefg, result);
    utext_close(result);
    utext_replace(&destText, 0, utext_nativeLength(&destText), nullptr, 0, &status);
    result = matcher2->replaceFirst(&replText, &destText, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(result == &destText);
    REGEX_ASSERT_UTEXT_UTF8(str_Thevalueof1isbcdefg, result);

    const char str_byitselfnogroupnumber[] = { 0x5c, 0x24, 0x20, 0x62, 0x79, 0x20, 0x69, 0x74, 0x73, 0x65, 0x6c,
               0x66, 0x2c, 0x20, 0x6e, 0x6f, 0x20, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x20, 0x6e, 0x75, 0x6d, 0x62,
               0x65, 0x72, 0x20, 0x5c, 0x24, 0x5c, 0x24, 0x5c, 0x24, 0x00 }; /* \$ by itself, no group number \$\$\$ */
    utext_openUTF8(&replText, str_byitselfnogroupnumber, -1, &status);
    result = matcher2->replaceFirst(&replText, nullptr, status);
    REGEX_CHECK_STATUS;
    const char str_byitselfnogroupnumberdefg[] = { 0x24, 0x20, 0x62, 0x79, 0x20, 0x69, 0x74, 0x73, 0x65, 0x6c, 0x66, 0x2c, 0x20, 0x6e, 0x6f, 0x20, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x20, 0x6e, 0x75, 0x6d, 0x62, 0x65, 0x72, 0x20, 0x24, 0x24, 0x24, 0x64, 0x65, 0x66, 0x67, 0x00 }; /* $ by itself, no group number $$$defg */
    REGEX_ASSERT_UTEXT_UTF8(str_byitselfnogroupnumberdefg, result);
    utext_close(result);
    utext_replace(&destText, 0, utext_nativeLength(&destText), nullptr, 0, &status);
    result = matcher2->replaceFirst(&replText, &destText, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(result == &destText);
    REGEX_ASSERT_UTEXT_UTF8(str_byitselfnogroupnumberdefg, result);

    unsigned char supplDigitChars[] = { 0x53, 0x75, 0x70, 0x70, 0x6c, 0x65, 0x6d, 0x65, 0x6e, 0x74, 0x61, 0x6c, 0x20, 0x44, 0x69, 0x67, 0x69, 0x74, 0x20, 0x31, 0x20, 0x24, 0x78, 0x78, 0x78, 0x78, 0x2e, 0x00 }; /* Supplemental Digit 1 $xxxx. */
    //unsigned char supplDigitChars[] = "Supplemental Digit 1 $xxxx."; // \U0001D7CF, MATHEMATICAL BOLD DIGIT ONE
    //                                 012345678901234567890123456
    supplDigitChars[22] = 0xF0;
    supplDigitChars[23] = 0x9D;
    supplDigitChars[24] = 0x9F;
    supplDigitChars[25] = 0x8F;
    utext_openUTF8(&replText, (char *)supplDigitChars, -1, &status);

    result = matcher2->replaceFirst(&replText, nullptr, status);
    REGEX_CHECK_STATUS;
    const char str_SupplementalDigit1bcdefg[] = { 0x53, 0x75, 0x70, 0x70, 0x6c, 0x65, 0x6d, 0x65, 0x6e, 0x74, 0x61, 0x6c, 0x20, 0x44, 0x69, 0x67, 0x69, 0x74, 0x20, 0x31, 0x20, 0x62, 0x63, 0x2e, 0x64, 0x65, 0x66, 0x67, 0x00 }; /* Supplemental Digit 1 bc.defg */
    REGEX_ASSERT_UTEXT_UTF8(str_SupplementalDigit1bcdefg, result);
    utext_close(result);
    utext_replace(&destText, 0, utext_nativeLength(&destText), nullptr, 0, &status);
    result = matcher2->replaceFirst(&replText, &destText, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(result == &destText);
    REGEX_ASSERT_UTEXT_UTF8(str_SupplementalDigit1bcdefg, result);
    const char str_badcapturegroupnumber5[] = { 0x62, 0x61, 0x64, 0x20, 0x63, 0x61, 0x70, 0x74, 0x75, 0x72, 0x65, 0x20, 0x67, 0x72, 0x6f, 0x75, 0x70, 0x20, 0x6e, 0x75, 0x6d, 0x62, 0x65, 0x72, 0x20, 0x24, 0x35, 0x2e, 0x2e, 0x2e,  0x00 }; /* bad capture group number $5..." */
    utext_openUTF8(&replText, str_badcapturegroupnumber5, -1, &status);
    REGEX_ASSERT_FAIL((result = matcher2->replaceFirst(&replText, nullptr, status)), U_INDEX_OUTOFBOUNDS_ERROR);
//    REGEX_ASSERT_UTEXT_UTF8("abcdefg", result);
    utext_close(result);
    utext_replace(&destText, 0, utext_nativeLength(&destText), nullptr, 0, &status);
    REGEX_ASSERT_FAIL((result = matcher2->replaceFirst(&replText, &destText, status)), U_INDEX_OUTOFBOUNDS_ERROR);
    REGEX_ASSERT(result == &destText);
//    REGEX_ASSERT_UTEXT_UTF8("abcdefg", result);

    //
    // Replacement String with \u hex escapes
    //
    {
      const char str_abc1abc2abc3[] = { 0x61, 0x62, 0x63, 0x20, 0x31, 0x20, 0x61, 0x62, 0x63, 0x20, 0x32, 0x20, 0x61, 0x62, 0x63, 0x20, 0x33, 0x00 }; /* abc 1 abc 2 abc 3 */
      const char str_u0043[] = { 0x2d, 0x2d, 0x5c, 0x75, 0x30, 0x30, 0x34, 0x33, 0x2d, 0x2d, 0x00 }; /* --\u0043-- */
        utext_openUTF8(&dataText, str_abc1abc2abc3, -1, &status);
        utext_openUTF8(&replText, str_u0043, -1, &status);
        matcher->reset(&dataText);

        result = matcher->replaceAll(&replText, nullptr, status);
        REGEX_CHECK_STATUS;
        const char str_C1C2C3[] = { 0x2d, 0x2d, 0x43, 0x2d, 0x2d, 0x20, 0x31, 0x20, 0x2d, 0x2d, 0x43, 0x2d, 0x2d, 0x20, 0x32, 0x20, 0x2d, 0x2d, 0x43, 0x2d, 0x2d, 0x20, 0x33, 0x00 }; /* --C-- 1 --C-- 2 --C-- 3 */
        REGEX_ASSERT_UTEXT_UTF8(str_C1C2C3, result);
        utext_close(result);
        utext_replace(&destText, 0, utext_nativeLength(&destText), nullptr, 0, &status);
        result = matcher->replaceAll(&replText, &destText, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == &destText);
        REGEX_ASSERT_UTEXT_UTF8(str_C1C2C3, result);
    }
    {
      const char str_abc[] = { 0x61, 0x62, 0x63, 0x20, 0x21, 0x00 }; /* abc ! */
        utext_openUTF8(&dataText, str_abc, -1, &status);
        const char str_U00010000[] = { 0x2d, 0x2d, 0x5c, 0x55, 0x30, 0x30, 0x30, 0x31, 0x30, 0x30, 0x30, 0x30, 0x2d, 0x2d, 0x00 }; /* --\U00010000-- */
        utext_openUTF8(&replText, str_U00010000, -1, &status);
        matcher->reset(&dataText);

        unsigned char expected[] = { 0x2d, 0x2d, 0x78, 0x78, 0x78, 0x78, 0x2d, 0x2d, 0x20, 0x21, 0x00 }; /* --xxxx-- ! */ // \U00010000, "LINEAR B SYLLABLE B008 A"
        //                          0123456789
        expected[2] = 0xF0;
        expected[3] = 0x90;
        expected[4] = 0x80;
        expected[5] = 0x80;

        result = matcher->replaceAll(&replText, nullptr, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT_UTEXT_UTF8((char *)expected, result);
        utext_close(result);
        utext_replace(&destText, 0, utext_nativeLength(&destText), nullptr, 0, &status);
        result = matcher->replaceAll(&replText, &destText, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == &destText);
        REGEX_ASSERT_UTEXT_UTF8((char *)expected, result);
    }
    // TODO:  need more through testing of capture substitutions.

    // Bug 4057
    //
    {
        status = U_ZERO_ERROR;
const char str_ssee[] = { 0x73, 0x73, 0x28, 0x2e, 0x2a, 0x3f, 0x29, 0x65, 0x65, 0x00 }; /* ss(.*?)ee */
const char str_blah[] = { 0x54, 0x68, 0x65, 0x20, 0x6d, 0x61, 0x74, 0x63, 0x68, 0x65, 0x73, 0x20, 0x73, 0x74, 0x61, 0x72, 0x74, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x73, 0x73, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x65, 0x6e, 0x64, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x65, 0x65, 0x20, 0x73, 0x73, 0x20, 0x73, 0x74, 0x75, 0x66, 0x66, 0x20, 0x65, 0x65, 0x20, 0x66, 0x69, 0x6e, 0x00 }; /* The matches start with ss and end with ee ss stuff ee fin */
const char str_ooh[] = { 0x6f, 0x6f, 0x68, 0x00 }; /* ooh */
        utext_openUTF8(&re, str_ssee, -1, &status);
        utext_openUTF8(&dataText, str_blah, -1, &status);
        utext_openUTF8(&replText, str_ooh, -1, &status);

        RegexMatcher m(&re, 0, status);
        REGEX_CHECK_STATUS;

        UnicodeString result;
        UText resultText = UTEXT_INITIALIZER;
        utext_openUnicodeString(&resultText, &result, &status);

        // Multiple finds do NOT bump up the previous appendReplacement position.
        m.reset(&dataText);
        m.find();
        m.find();
        m.appendReplacement(&resultText, &replText, status);
        REGEX_CHECK_STATUS;
        const char str_blah2[] = { 0x54, 0x68, 0x65, 0x20, 0x6d, 0x61, 0x74, 0x63, 0x68, 0x65, 0x73, 0x20, 0x73, 0x74, 0x61, 0x72, 0x74, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x73, 0x73, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x65, 0x6e, 0x64, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x65, 0x65, 0x20, 0x6f, 0x6f, 0x68, 0x00 }; /* The matches start with ss and end with ee ooh */
        REGEX_ASSERT_UTEXT_UTF8(str_blah2, &resultText);

        // After a reset into the interior of a string, appendReplacement still starts at beginning.
        status = U_ZERO_ERROR;
        result.truncate(0);
        utext_openUnicodeString(&resultText, &result, &status);
        m.reset(10, status);
        m.find();
        m.find();
        m.appendReplacement(&resultText, &replText, status);
        REGEX_CHECK_STATUS;
        const char str_blah3[] = { 0x54, 0x68, 0x65, 0x20, 0x6d, 0x61, 0x74, 0x63, 0x68, 0x65, 0x73, 0x20, 0x73, 0x74, 0x61, 0x72, 0x74, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x73, 0x73, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x65, 0x6e, 0x64, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x65, 0x65, 0x20, 0x6f, 0x6f, 0x68, 0x00 }; /* The matches start with ss and end with ee ooh */
        REGEX_ASSERT_UTEXT_UTF8(str_blah3, &resultText);

        // find() at interior of string, appendReplacement still starts at beginning.
        status = U_ZERO_ERROR;
        result.truncate(0);
        utext_openUnicodeString(&resultText, &result, &status);
        m.reset();
        m.find(10, status);
        m.find();
        m.appendReplacement(&resultText, &replText, status);
        REGEX_CHECK_STATUS;
        const char str_blah8[] = { 0x54, 0x68, 0x65, 0x20, 0x6d, 0x61, 0x74, 0x63, 0x68, 0x65, 0x73, 0x20, 0x73, 0x74, 0x61, 0x72, 0x74, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x73, 0x73, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x65, 0x6e, 0x64, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x65, 0x65, 0x20, 0x6f, 0x6f, 0x68, 0x00 }; /* The matches start with ss and end with ee ooh */
        REGEX_ASSERT_UTEXT_UTF8(str_blah8, &resultText);

        m.appendTail(&resultText, status);
        const char str_blah9[] = { 0x54, 0x68, 0x65, 0x20, 0x6d, 0x61, 0x74, 0x63, 0x68, 0x65, 0x73, 0x20, 0x73, 0x74, 0x61, 0x72, 0x74, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x73, 0x73, 0x20, 0x61, 0x6e, 0x64, 0x20, 0x65, 0x6e, 0x64, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x65, 0x65, 0x20, 0x6f, 0x6f, 0x68, 0x20, 0x66, 0x69, 0x6e, 0x00 }; /* The matches start with ss and end with ee ooh fin */
        REGEX_ASSERT_UTEXT_UTF8(str_blah9, &resultText);

        utext_close(&resultText);
    }

    delete matcher2;
    delete pat2;
    delete matcher;
    delete pat;

    utext_close(&dataText);
    utext_close(&replText);
    utext_close(&destText);
    utext_close(&re);
}


//---------------------------------------------------------------------------
//
//      API_Pattern_UTF8  Test that the API for class RegexPattern is
//                        present and nominally working.
//
//---------------------------------------------------------------------------
void RegexTest::API_Pattern_UTF8() {
    RegexPattern        pata;    // Test default constructor to not crash.
    RegexPattern        patb;

    REGEX_ASSERT(pata == patb);
    REGEX_ASSERT(pata == pata);

    UText         re1 = UTEXT_INITIALIZER;
    UText         re2 = UTEXT_INITIALIZER;
    UErrorCode    status = U_ZERO_ERROR;
    UParseError   pe;

    const char str_abcalmz[] = { 0x61, 0x62, 0x63, 0x5b, 0x61, 0x2d, 0x6c, 0x5d, 0x5b, 0x6d, 0x2d, 0x7a, 0x5d, 0x00 }; /* abc[a-l][m-z] */
    const char str_def[] = { 0x64, 0x65, 0x66, 0x00 }; /* def */
    utext_openUTF8(&re1, str_abcalmz, -1, &status);
    utext_openUTF8(&re2, str_def, -1, &status);

    RegexPattern        *pat1 = RegexPattern::compile(&re1, 0, pe, status);
    RegexPattern        *pat2 = RegexPattern::compile(&re2, 0, pe, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(*pat1 == *pat1);
    REGEX_ASSERT(*pat1 != pata);

    // Assign
    patb = *pat1;
    REGEX_ASSERT(patb == *pat1);

    // Copy Construct
    RegexPattern patc(*pat1);
    REGEX_ASSERT(patc == *pat1);
    REGEX_ASSERT(patb == patc);
    REGEX_ASSERT(pat1 != pat2);
    patb = *pat2;
    REGEX_ASSERT(patb != patc);
    REGEX_ASSERT(patb == *pat2);

    // Compile with no flags.
    RegexPattern         *pat1a = RegexPattern::compile(&re1, pe, status);
    REGEX_ASSERT(*pat1a == *pat1);

    REGEX_ASSERT(pat1a->flags() == 0);

    // Compile with different flags should be not equal
    RegexPattern        *pat1b = RegexPattern::compile(&re1, UREGEX_CASE_INSENSITIVE, pe, status);
    REGEX_CHECK_STATUS;

    REGEX_ASSERT(*pat1b != *pat1a);
    REGEX_ASSERT(pat1b->flags() == UREGEX_CASE_INSENSITIVE);
    REGEX_ASSERT(pat1a->flags() == 0);
    delete pat1b;

    // clone
    RegexPattern *pat1c = pat1->clone();
    REGEX_ASSERT(*pat1c == *pat1);
    REGEX_ASSERT(*pat1c != *pat2);

    delete pat1c;
    delete pat1a;
    delete pat1;
    delete pat2;

    utext_close(&re1);
    utext_close(&re2);


    //
    //   Verify that a matcher created from a cloned pattern works.
    //     (Jitterbug 3423)
    //
    {
        UErrorCode     status     = U_ZERO_ERROR;
        UText          pattern    = UTEXT_INITIALIZER;
        const char str_pL[] = { 0x5c, 0x70, 0x7b, 0x4c, 0x7d, 0x2b, 0x00 }; /* \p{L}+ */
        utext_openUTF8(&pattern, str_pL, -1, &status);

        RegexPattern  *pSource    = RegexPattern::compile(&pattern, 0, status);
        RegexPattern  *pClone     = pSource->clone();
        delete         pSource;
        RegexMatcher  *mFromClone = pClone->matcher(status);
        REGEX_CHECK_STATUS;

        UText          input      = UTEXT_INITIALIZER;
        const char str_HelloWorld[] = { 0x48, 0x65, 0x6c, 0x6c, 0x6f, 0x20, 0x57, 0x6f, 0x72, 0x6c, 0x64, 0x00 }; /* Hello World */
        utext_openUTF8(&input, str_HelloWorld, -1, &status);
        mFromClone->reset(&input);
        REGEX_ASSERT(mFromClone->find() == true);
        REGEX_ASSERT(mFromClone->group(status) == "Hello");
        REGEX_ASSERT(mFromClone->find() == true);
        REGEX_ASSERT(mFromClone->group(status) == "World");
        REGEX_ASSERT(mFromClone->find() == false);
        delete mFromClone;
        delete pClone;

        utext_close(&input);
        utext_close(&pattern);
    }

    //
    //   matches convenience API
    //
    {
        UErrorCode status  = U_ZERO_ERROR;
        UText      pattern = UTEXT_INITIALIZER;
        UText      input   = UTEXT_INITIALIZER;

        const char str_randominput[] = { 0x72, 0x61, 0x6e, 0x64, 0x6f, 0x6d, 0x20, 0x69, 0x6e, 0x70, 0x75, 0x74, 0x00 }; /* random input */
        utext_openUTF8(&input, str_randominput, -1, &status);

        const char str_dotstar[] = { 0x2e, 0x2a, 0x00 }; /* .* */
        utext_openUTF8(&pattern, str_dotstar, -1, &status);
        REGEX_ASSERT(RegexPattern::matches(&pattern, &input, pe, status) == true);
        REGEX_CHECK_STATUS;

        const char str_abc[] = { 0x61, 0x62, 0x63, 0x00 }; /* abc */
        utext_openUTF8(&pattern, str_abc, -1, &status);
        REGEX_ASSERT(RegexPattern::matches("abc", "random input", pe, status) == false);
        REGEX_CHECK_STATUS;

        const char str_nput[] = { 0x2e, 0x2a, 0x6e, 0x70, 0x75, 0x74, 0x00 }; /* .*nput */
        utext_openUTF8(&pattern, str_nput, -1, &status);
        REGEX_ASSERT(RegexPattern::matches(".*nput", "random input", pe, status) == true);
        REGEX_CHECK_STATUS;

        utext_openUTF8(&pattern, str_randominput, -1, &status);
        REGEX_ASSERT(RegexPattern::matches("random input", "random input", pe, status) == true);
        REGEX_CHECK_STATUS;

        const char str_u[] = { 0x2e, 0x2a, 0x75, 0x00 }; /* .*u */
        utext_openUTF8(&pattern, str_u, -1, &status);
        REGEX_ASSERT(RegexPattern::matches(".*u", "random input", pe, status) == false);
        REGEX_CHECK_STATUS;

        utext_openUTF8(&input, str_abc, -1, &status);
        utext_openUTF8(&pattern, str_abc, -1, &status);
        status = U_INDEX_OUTOFBOUNDS_ERROR;
        REGEX_ASSERT(RegexPattern::matches("abc", "abc", pe, status) == false);
        REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);

        utext_close(&input);
        utext_close(&pattern);
    }


    //
    // Split()
    //
    status = U_ZERO_ERROR;
    const char str_spaceplus[] = { 0x20, 0x2b, 0x00 }; /*  + */
    utext_openUTF8(&re1, str_spaceplus, -1, &status);
    pat1 = RegexPattern::compile(&re1, pe, status);
    REGEX_CHECK_STATUS;
    UnicodeString  fields[10];

    int32_t n;
    n = pat1->split("Now is the time", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==4);
    REGEX_ASSERT(fields[0]=="Now");
    REGEX_ASSERT(fields[1]=="is");
    REGEX_ASSERT(fields[2]=="the");
    REGEX_ASSERT(fields[3]=="time");
    REGEX_ASSERT(fields[4]=="");

    n = pat1->split("Now is the time", fields, 2, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==2);
    REGEX_ASSERT(fields[0]=="Now");
    REGEX_ASSERT(fields[1]=="is the time");
    REGEX_ASSERT(fields[2]=="the");   // left over from previous test

    fields[1] = "*";
    status = U_ZERO_ERROR;
    n = pat1->split("Now is the time", fields, 1, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==1);
    REGEX_ASSERT(fields[0]=="Now is the time");
    REGEX_ASSERT(fields[1]=="*");
    status = U_ZERO_ERROR;

    n = pat1->split("    Now       is the time   ", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==6);
    REGEX_ASSERT(fields[0]=="");
    REGEX_ASSERT(fields[1]=="Now");
    REGEX_ASSERT(fields[2]=="is");
    REGEX_ASSERT(fields[3]=="the");
    REGEX_ASSERT(fields[4]=="time");
    REGEX_ASSERT(fields[5]=="");
    REGEX_ASSERT(fields[6]=="");

    fields[2] = "*";
    n = pat1->split("     ", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==2);
    REGEX_ASSERT(fields[0]=="");
    REGEX_ASSERT(fields[1]=="");
    REGEX_ASSERT(fields[2]=="*");

    fields[0] = "foo";
    n = pat1->split("", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==0);
    REGEX_ASSERT(fields[0]=="foo");

    delete pat1;

    //  split, with a pattern with (capture)
    regextst_openUTF8FromInvariant(&re1, "<(\\w*)>", -1, &status);
    pat1 = RegexPattern::compile(&re1,  pe, status);
    REGEX_CHECK_STATUS;

    status = U_ZERO_ERROR;
    fields[6] = fields[7] = "*";
    n = pat1->split("<a>Now is <b>the time<c>", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==7);
    REGEX_ASSERT(fields[0]=="");
    REGEX_ASSERT(fields[1]=="a");
    REGEX_ASSERT(fields[2]=="Now is ");
    REGEX_ASSERT(fields[3]=="b");
    REGEX_ASSERT(fields[4]=="the time");
    REGEX_ASSERT(fields[5]=="c");
    REGEX_ASSERT(fields[6]=="");
    REGEX_ASSERT(fields[7]=="*");
    REGEX_ASSERT(status==U_ZERO_ERROR);

    fields[6] = fields[7] = "*";
    n = pat1->split("  <a>Now is <b>the time<c>", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==7);
    REGEX_ASSERT(fields[0]=="  ");
    REGEX_ASSERT(fields[1]=="a");
    REGEX_ASSERT(fields[2]=="Now is ");
    REGEX_ASSERT(fields[3]=="b");
    REGEX_ASSERT(fields[4]=="the time");
    REGEX_ASSERT(fields[5]=="c");
    REGEX_ASSERT(fields[6]=="");
    REGEX_ASSERT(fields[7]=="*");

    status = U_ZERO_ERROR;
    fields[6] = "foo";
    n = pat1->split("  <a>Now is <b>the time<c> ", fields, 6, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==6);
    REGEX_ASSERT(fields[0]=="  ");
    REGEX_ASSERT(fields[1]=="a");
    REGEX_ASSERT(fields[2]=="Now is ");
    REGEX_ASSERT(fields[3]=="b");
    REGEX_ASSERT(fields[4]=="the time");
    REGEX_ASSERT(fields[5]==" ");
    REGEX_ASSERT(fields[6]=="foo");

    status = U_ZERO_ERROR;
    fields[5] = "foo";
    n = pat1->split("  <a>Now is <b>the time<c>", fields, 5, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==5);
    REGEX_ASSERT(fields[0]=="  ");
    REGEX_ASSERT(fields[1]=="a");
    REGEX_ASSERT(fields[2]=="Now is ");
    REGEX_ASSERT(fields[3]=="b");
    REGEX_ASSERT(fields[4]=="the time<c>");
    REGEX_ASSERT(fields[5]=="foo");

    status = U_ZERO_ERROR;
    fields[5] = "foo";
    n = pat1->split("  <a>Now is <b>the time", fields, 5, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==5);
    REGEX_ASSERT(fields[0]=="  ");
    REGEX_ASSERT(fields[1]=="a");
    REGEX_ASSERT(fields[2]=="Now is ");
    REGEX_ASSERT(fields[3]=="b");
    REGEX_ASSERT(fields[4]=="the time");
    REGEX_ASSERT(fields[5]=="foo");

    status = U_ZERO_ERROR;
    n = pat1->split("  <a>Now is <b>the time<c>", fields, 4, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==4);
    REGEX_ASSERT(fields[0]=="  ");
    REGEX_ASSERT(fields[1]=="a");
    REGEX_ASSERT(fields[2]=="Now is ");
    REGEX_ASSERT(fields[3]=="the time<c>");
    status = U_ZERO_ERROR;
    delete pat1;

    regextst_openUTF8FromInvariant(&re1, "([-,])", -1, &status);
    pat1 = RegexPattern::compile(&re1, pe, status);
    REGEX_CHECK_STATUS;
    n = pat1->split("1-10,20", fields, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(n==5);
    REGEX_ASSERT(fields[0]=="1");
    REGEX_ASSERT(fields[1]=="-");
    REGEX_ASSERT(fields[2]=="10");
    REGEX_ASSERT(fields[3]==",");
    REGEX_ASSERT(fields[4]=="20");
    delete pat1;


    //
    // split of a UText based string, with library allocating output UTexts.
    //
    {
        status = U_ZERO_ERROR;
        RegexMatcher matcher(UnicodeString("(:)"), 0, status);
        UnicodeString stringToSplit("first:second:third");
        UText *textToSplit = utext_openUnicodeString(nullptr, &stringToSplit, &status);
        REGEX_CHECK_STATUS;

        UText *splits[10] = {nullptr};
        int32_t numFields = matcher.split(textToSplit, splits, UPRV_LENGTHOF(splits), status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(numFields == 5);
        REGEX_ASSERT_UTEXT_INVARIANT("first", splits[0]);
        REGEX_ASSERT_UTEXT_INVARIANT(":", splits[1]);
        REGEX_ASSERT_UTEXT_INVARIANT("second", splits[2]);
        REGEX_ASSERT_UTEXT_INVARIANT(":", splits[3]);
        REGEX_ASSERT_UTEXT_INVARIANT("third", splits[4]);
        REGEX_ASSERT(splits[5] == nullptr);

        for (int i=0; i<UPRV_LENGTHOF(splits); i++) {
            if (splits[i]) {
                utext_close(splits[i]);
                splits[i] = nullptr;
            }
        }
        utext_close(textToSplit);
    }


    //
    // RegexPattern::pattern() and patternText()
    //
    pat1 = new RegexPattern();
    REGEX_ASSERT(pat1->pattern() == "");
    REGEX_ASSERT_UTEXT_UTF8("", pat1->patternText(status));
    delete pat1;
    const char *helloWorldInvariant = "(Hello, world)*";
    regextst_openUTF8FromInvariant(&re1, helloWorldInvariant, -1, &status);
    pat1 = RegexPattern::compile(&re1, pe, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT_UNISTR("(Hello, world)*", pat1->pattern());
    REGEX_ASSERT_UTEXT_INVARIANT("(Hello, world)*", pat1->patternText(status));
    delete pat1;

    utext_close(&re1);
}


//---------------------------------------------------------------------------
//
//      Extended       A more thorough check for features of regex patterns
//                     The test cases are in a separate data file,
//                       source/tests/testdata/regextst.txt
//                     A description of the test data format is included in that file.
//
//---------------------------------------------------------------------------

const char *
RegexTest::getPath(char buffer[2048], const char *filename) {
    UErrorCode status=U_ZERO_ERROR;
    const char *testDataDirectory = IntlTest::getSourceTestData(status);
    if (U_FAILURE(status)) {
        errln("ERROR: loadTestData() failed - %s", u_errorName(status));
        return nullptr;
    }

    strcpy(buffer, testDataDirectory);
    strcat(buffer, filename);
    return buffer;
}

void RegexTest::Extended() {
    char tdd[2048];
    const char *srcPath;
    UErrorCode  status  = U_ZERO_ERROR;
    int32_t     lineNum = 0;

    //
    //  Open and read the test data file.
    //
    srcPath=getPath(tdd, "regextst.txt");
    if(srcPath==nullptr) {
        return; /* something went wrong, error already output */
    }

    int32_t    len;
    char16_t *testData = ReadAndConvertFile(srcPath, len, "utf-8", status);
    if (U_FAILURE(status)) {
        return; /* something went wrong, error already output */
    }

    //
    //  Put the test data into a UnicodeString
    //
    UnicodeString testString(false, testData, len);

    RegexMatcher    quotedStuffMat(UNICODE_STRING_SIMPLE("\\s*([\\'\\\"/])(.*?)\\1"), 0, status);
    RegexMatcher    commentMat    (UNICODE_STRING_SIMPLE("\\s*(#.*)?$"), 0, status);
    RegexMatcher    flagsMat      (UNICODE_STRING_SIMPLE("\\s*([ixsmdteDEGLMQvabtyYzZ2-9]*)([:letter:]*)"), 0, status);

    RegexMatcher    lineMat(UNICODE_STRING_SIMPLE("(.*?)\\r?\\n"), testString, 0, status);
    UnicodeString   testPattern;   // The pattern for test from the test file.
    UnicodeString   testFlags;     // the flags   for a test.
    UnicodeString   matchString;   // The marked up string to be used as input

    if (U_FAILURE(status)){
        dataerrln("Construct RegexMatcher() error - %s", u_errorName(status));
        delete [] testData;
        return;
    }

    //
    //  Loop over the test data file, once per line.
    //
    while (lineMat.find()) {
        lineNum++;
        if (U_FAILURE(status)) {
          errln("%s:%d: ICU Error \"%s\"", srcPath, lineNum, u_errorName(status));
        }

        status = U_ZERO_ERROR;
        UnicodeString testLine = lineMat.group(1, status);
        if (testLine.length() == 0) {
            continue;
        }

        //
        // Parse the test line.  Skip blank and comment only lines.
        // Separate out the three main fields - pattern, flags, target.
        //

        commentMat.reset(testLine);
        if (commentMat.lookingAt(status)) {
            // This line is a comment, or blank.
            continue;
        }

        //
        //  Pull out the pattern field, remove it from the test file line.
        //
        quotedStuffMat.reset(testLine);
        if (quotedStuffMat.lookingAt(status)) {
            testPattern = quotedStuffMat.group(2, status);
            testLine.remove(0, quotedStuffMat.end(0, status));
        } else {
            errln("Bad pattern (missing quotes?) at %s:%d", srcPath, lineNum);
            continue;
        }


        //
        //  Pull out the flags from the test file line.
        //
        flagsMat.reset(testLine);
        flagsMat.lookingAt(status);                  // Will always match, possibly an empty string.
        testFlags = flagsMat.group(1, status);
        if (flagsMat.group(2, status).length() > 0) {
            errln("Bad Match flag at line %d. Scanning %c\n",
                lineNum, flagsMat.group(2, status).charAt(0));
            continue;
        }
        testLine.remove(0, flagsMat.end(0, status));

        //
        //  Pull out the match string, as a whole.
        //    We'll process the <tags> later.
        //
        quotedStuffMat.reset(testLine);
        if (quotedStuffMat.lookingAt(status)) {
            matchString = quotedStuffMat.group(2, status);
            testLine.remove(0, quotedStuffMat.end(0, status));
        } else {
            errln("Bad match string at test file line %d", lineNum);
            continue;
        }

        //
        //  The only thing left from the input line should be an optional trailing comment.
        //
        commentMat.reset(testLine);
        if (commentMat.lookingAt(status) == false) {
            errln("Line %d: unexpected characters at end of test line.", lineNum);
            continue;
        }

        //
        //  Run the test
        //
        regex_find(testPattern, testFlags, matchString, srcPath, lineNum);
    }

    delete [] testData;

}



//---------------------------------------------------------------------------
//
//    regex_find(pattern, flags, inputString, lineNumber)
//
//         Function to run a single test from the Extended (data driven) tests.
//         See file test/testdata/regextst.txt for a description of the
//         pattern and inputString fields, and the allowed flags.
//         lineNumber is the source line in regextst.txt of the test.
//
//---------------------------------------------------------------------------


//  Set a value into a UVector at position specified by a decimal number in
//   a UnicodeString.   This is a utility function needed by the actual test function,
//   which follows.
static void set(UVector &vec, int32_t val, UnicodeString index) {
    UErrorCode  status=U_ZERO_ERROR;
    int32_t  idx = 0;
    for (int32_t i=0; i<index.length(); i++) {
        int32_t d=u_charDigitValue(index.charAt(i));
        if (d<0) {return;}
        idx = idx*10 + d;
    }
    while (vec.size()<idx+1) {vec.addElement(-1, status);}
    vec.setElementAt(val, idx);
}

static void setInt(UVector &vec, int32_t val, int32_t idx) {
    UErrorCode  status=U_ZERO_ERROR;
    while (vec.size()<idx+1) {vec.addElement(-1, status);}
    vec.setElementAt(val, idx);
}

static UBool utextOffsetToNative(UText *utext, int32_t unistrOffset, int32_t& nativeIndex)
{
    UBool couldFind = true;
    UTEXT_SETNATIVEINDEX(utext, 0);
    int32_t i = 0;
    while (i < unistrOffset) {
        UChar32 c = UTEXT_NEXT32(utext);
        if (c != U_SENTINEL) {
            i += U16_LENGTH(c);
        } else {
            couldFind = false;
            break;
        }
    }
    nativeIndex = (int32_t)UTEXT_GETNATIVEINDEX(utext);
    return couldFind;
}


void RegexTest::regex_find(const UnicodeString &pattern,
                           const UnicodeString &flags,
                           const UnicodeString &inputString,
                           const char *srcPath,
                           int32_t line) {
    UnicodeString       unEscapedInput;
    UnicodeString       deTaggedInput;

    int32_t             patternUTF8Length,      inputUTF8Length;
    char                *patternChars  = nullptr, *inputChars = nullptr;
    UText               patternText    = UTEXT_INITIALIZER;
    UText               inputText      = UTEXT_INITIALIZER;
    UConverter          *UTF8Converter = nullptr;

    UErrorCode          status         = U_ZERO_ERROR;
    UParseError         pe;
    RegexPattern        *parsePat      = nullptr;
    RegexMatcher        *parseMatcher  = nullptr;
    RegexPattern        *callerPattern = nullptr, *UTF8Pattern = nullptr;
    RegexMatcher        *matcher       = nullptr, *UTF8Matcher = nullptr;
    UVector             groupStarts(status);
    UVector             groupEnds(status);
    UVector             groupStartsUTF8(status);
    UVector             groupEndsUTF8(status);
    UBool               isMatch        = false, isUTF8Match = false;
    UBool               failed         = false;
    int32_t             numFinds;
    int32_t             i;
    UBool               useMatchesFunc   = false;
    UBool               useLookingAtFunc = false;
    int32_t             regionStart      = -1;
    int32_t             regionEnd        = -1;
    int32_t             regionStartUTF8  = -1;
    int32_t             regionEndUTF8    = -1;


    //
    //  Compile the caller's pattern
    //
    uint32_t bflags = 0;
    if (flags.indexOf((char16_t)0x69) >= 0)  { // 'i' flag
        bflags |= UREGEX_CASE_INSENSITIVE;
    }
    if (flags.indexOf((char16_t)0x78) >= 0)  { // 'x' flag
        bflags |= UREGEX_COMMENTS;
    }
    if (flags.indexOf((char16_t)0x73) >= 0)  { // 's' flag
        bflags |= UREGEX_DOTALL;
    }
    if (flags.indexOf((char16_t)0x6d) >= 0)  { // 'm' flag
        bflags |= UREGEX_MULTILINE;
    }

    if (flags.indexOf((char16_t)0x65) >= 0) { // 'e' flag
        bflags |= UREGEX_ERROR_ON_UNKNOWN_ESCAPES;
    }
    if (flags.indexOf((char16_t)0x44) >= 0) { // 'D' flag
        bflags |= UREGEX_UNIX_LINES;
    }
    if (flags.indexOf((char16_t)0x51) >= 0) { // 'Q' flag
        bflags |= UREGEX_LITERAL;
    }


    callerPattern = RegexPattern::compile(pattern, bflags, pe, status);
    if (status != U_ZERO_ERROR) {
        #if UCONFIG_NO_BREAK_ITERATION==1
        // 'v' test flag means that the test pattern should not compile if ICU was configured
        //     to not include break iteration.  RBBI is needed for Unicode word boundaries.
        if (flags.indexOf((char16_t)0x76) >= 0 /*'v'*/ && status == U_UNSUPPORTED_ERROR) {
            goto cleanupAndReturn;
        }
        #endif
        if (flags.indexOf((char16_t)0x45) >= 0) {  //  flags contain 'E'
            // Expected pattern compilation error.
            if (flags.indexOf((char16_t)0x64) >= 0) {   // flags contain 'd'
                logln("Pattern Compile returns \"%s\"", u_errorName(status));
            }
            goto cleanupAndReturn;
        } else {
            // Unexpected pattern compilation error.
            dataerrln("Line %d: error %s compiling pattern.", line, u_errorName(status));
            goto cleanupAndReturn;
        }
    }

    UTF8Converter = ucnv_open("UTF8", &status);
    ucnv_setFromUCallBack(UTF8Converter, UCNV_FROM_U_CALLBACK_STOP, nullptr, nullptr, nullptr, &status);

    patternUTF8Length = pattern.extract(nullptr, 0, UTF8Converter, status);
    status = U_ZERO_ERROR; // buffer overflow
    patternChars = new char[patternUTF8Length+1];
    pattern.extract(patternChars, patternUTF8Length+1, UTF8Converter, status);
    utext_openUTF8(&patternText, patternChars, patternUTF8Length, &status);

    if (status == U_ZERO_ERROR) {
        UTF8Pattern = RegexPattern::compile(&patternText, bflags, pe, status);

        if (status != U_ZERO_ERROR) {
#if UCONFIG_NO_BREAK_ITERATION==1
            // 'v' test flag means that the test pattern should not compile if ICU was configured
            //     to not include break iteration.  RBBI is needed for Unicode word boundaries.
            if (flags.indexOf((char16_t)0x76) >= 0 /*'v'*/ && status == U_UNSUPPORTED_ERROR) {
                goto cleanupAndReturn;
            }
#endif
            if (flags.indexOf((char16_t)0x45) >= 0) {  //  flags contain 'E'
                // Expected pattern compilation error.
                if (flags.indexOf((char16_t)0x64) >= 0) {   // flags contain 'd'
                    logln("Pattern Compile returns \"%s\" (UTF8)", u_errorName(status));
                }
                goto cleanupAndReturn;
            } else {
                // Unexpected pattern compilation error.
                errln("Line %d: error %s compiling pattern. (UTF8)", line, u_errorName(status));
                goto cleanupAndReturn;
            }
        }
    }

    if (UTF8Pattern == nullptr) {
        // UTF-8 does not allow unpaired surrogates, so this could actually happen without being a failure of the engine
        logln("Unable to create UTF-8 pattern, skipping UTF-8 tests for %s:%d", srcPath, line);
        status = U_ZERO_ERROR;
    }

    if (flags.indexOf((char16_t)0x64) >= 0) {  // 'd' flag
        callerPattern->dumpPattern();
    }

    if (flags.indexOf((char16_t)0x45) >= 0) {  // 'E' flag
        errln("%s, Line %d: Expected, but did not get, a pattern compilation error.", srcPath, line);
        goto cleanupAndReturn;
    }


    //
    // Number of times find() should be called on the test string, default to 1
    //
    numFinds = 1;
    for (i=2; i<=9; i++) {
        if (flags.indexOf((char16_t)(0x30 + i)) >= 0) {   // digit flag
            if (numFinds != 1) {
                errln("Line %d: more than one digit flag.  Scanning %d.", line, i);
                goto cleanupAndReturn;
            }
            numFinds = i;
        }
    }

    // 'M' flag.  Use matches() instead of find()
    if (flags.indexOf((char16_t)0x4d) >= 0) {
        useMatchesFunc = true;
    }
    if (flags.indexOf((char16_t)0x4c) >= 0) {
        useLookingAtFunc = true;
    }

    //
    //  Find the tags in the input data, remove them, and record the group boundary
    //    positions.
    //
    parsePat = RegexPattern::compile("<(/?)(r|[0-9]+)>", 0, pe, status);
    if (!assertSuccess(WHERE, status) ) {
        goto cleanupAndReturn;
    }

    unEscapedInput = inputString.unescape();
    parseMatcher = parsePat->matcher(unEscapedInput, status);
    if (!assertSuccess(WHERE, status) ) {
        goto cleanupAndReturn;
    }
    while(parseMatcher->find()) {
        parseMatcher->appendReplacement(deTaggedInput, "", status);
        REGEX_CHECK_STATUS;
        UnicodeString groupNum = parseMatcher->group(2, status);
        if (groupNum == "r") {
            // <r> or </r>, a region specification within the string
            if (parseMatcher->group(1, status) == "/") {
                regionEnd = deTaggedInput.length();
            } else {
                regionStart = deTaggedInput.length();
            }
        } else {
            // <digits> or </digits>, a group match boundary tag.
            if (parseMatcher->group(1, status) == "/") {
                set(groupEnds, deTaggedInput.length(), groupNum);
            } else {
                set(groupStarts, deTaggedInput.length(), groupNum);
            }
        }
    }
    parseMatcher->appendTail(deTaggedInput);

    if (groupStarts.size() != groupEnds.size()) {
        errln("Error at line %d: mismatched <n> group tags in expected results.", line);
        failed = true;
        goto cleanupAndReturn;
    }
    if ((regionStart>=0 || regionEnd>=0) && (regionStart<0 || regionStart>regionEnd)) {
        errln("mismatched <r> tags");
        failed = true;
        goto cleanupAndReturn;
    }

    //
    //  Configure the matcher according to the flags specified with this test.
    //
    matcher = callerPattern->matcher(deTaggedInput, status);
    REGEX_CHECK_STATUS_L(line);
    if (flags.indexOf((char16_t)0x74) >= 0) {   //  't' trace flag
        matcher->setTrace(true);
    }

    if (UTF8Pattern != nullptr) {
        inputUTF8Length = deTaggedInput.extract(nullptr, 0, UTF8Converter, status);
        status = U_ZERO_ERROR; // buffer overflow
        inputChars = new char[inputUTF8Length+1];
        deTaggedInput.extract(inputChars, inputUTF8Length+1, UTF8Converter, status);
        utext_openUTF8(&inputText, inputChars, inputUTF8Length, &status);

        if (status == U_ZERO_ERROR) {
            UTF8Matcher = &UTF8Pattern->matcher(status)->reset(&inputText);
            REGEX_CHECK_STATUS_L(line);
        }

        if (UTF8Matcher == nullptr) {
            // UTF-8 does not allow unpaired surrogates, so this could actually happen without being a failure of the engine
            logln("Unable to create UTF-8 matcher, skipping UTF-8 tests for %s:%d", srcPath, line);
            status = U_ZERO_ERROR;
        }
    }

    //
    //  Generate native indices for UTF8 versions of region and capture group info
    //
    if (UTF8Matcher != nullptr) {
        if (flags.indexOf((char16_t)0x74) >= 0) {   //  't' trace flag
            UTF8Matcher->setTrace(true);
        }
        if (regionStart>=0)    (void) utextOffsetToNative(&inputText, regionStart, regionStartUTF8);
        if (regionEnd>=0)      (void) utextOffsetToNative(&inputText, regionEnd, regionEndUTF8);

        //  Fill out the native index UVector info.
        //  Only need 1 loop, from above we know groupStarts.size() = groupEnds.size()
        for (i=0; i<groupStarts.size(); i++) {
            int32_t  start = groupStarts.elementAti(i);
            //  -1 means there was no UVector slot and we won't be requesting that capture group for this test, don't bother inserting
            if (start >= 0) {
                int32_t  startUTF8;
                if (!utextOffsetToNative(&inputText, start, startUTF8)) {
                    errln("Error at line %d: could not find native index for group start %d.  UTF16 index %d", line, i, start);
                    failed = true;
                    goto cleanupAndReturn;  // Good chance of subsequent bogus errors.  Stop now.
                }
                setInt(groupStartsUTF8, startUTF8, i);
            }

            int32_t  end = groupEnds.elementAti(i);
            //  -1 means there was no UVector slot and we won't be requesting that capture group for this test, don't bother inserting
            if (end >= 0) {
                int32_t  endUTF8;
                if (!utextOffsetToNative(&inputText, end, endUTF8)) {
                    errln("Error at line %d: could not find native index for group end %d.  UTF16 index %d", line, i, end);
                    failed = true;
                    goto cleanupAndReturn;  // Good chance of subsequent bogus errors.  Stop now.
                }
                setInt(groupEndsUTF8, endUTF8, i);
            }
        }
    }

    if (regionStart>=0) {
       matcher->region(regionStart, regionEnd, status);
       REGEX_CHECK_STATUS_L(line);
       if (UTF8Matcher != nullptr) {
           UTF8Matcher->region(regionStartUTF8, regionEndUTF8, status);
           REGEX_CHECK_STATUS_L(line);
       }
    }
    if (flags.indexOf((char16_t)0x61) >= 0) {   //  'a' anchoring bounds flag
        matcher->useAnchoringBounds(false);
        if (UTF8Matcher != nullptr) {
            UTF8Matcher->useAnchoringBounds(false);
        }
    }
    if (flags.indexOf((char16_t)0x62) >= 0) {   //  'b' transparent bounds flag
        matcher->useTransparentBounds(true);
        if (UTF8Matcher != nullptr) {
            UTF8Matcher->useTransparentBounds(true);
        }
    }



    //
    // Do a find on the de-tagged input using the caller's pattern
    //     TODO: error on count>1 and not find().
    //           error on both matches() and lookingAt().
    //
    for (i=0; i<numFinds; i++) {
        if (useMatchesFunc) {
            isMatch = matcher->matches(status);
            if (UTF8Matcher != nullptr) {
               isUTF8Match = UTF8Matcher->matches(status);
            }
        } else  if (useLookingAtFunc) {
            isMatch = matcher->lookingAt(status);
            if (UTF8Matcher != nullptr) {
                isUTF8Match = UTF8Matcher->lookingAt(status);
            }
        } else {
            isMatch = matcher->find();
            if (UTF8Matcher != nullptr) {
                isUTF8Match = UTF8Matcher->find();
            }
        }
    }
    matcher->setTrace(false);
    if (UTF8Matcher) {
        UTF8Matcher->setTrace(false);
    }
    if (U_FAILURE(status)) {
        errln("Error at line %d. ICU ErrorCode is %s", u_errorName(status));
    }

    //
    // Match up the groups from the find() with the groups from the tags
    //

    // number of tags should match number of groups from find operation.
    // matcher->groupCount does not include group 0, the entire match, hence the +1.
    //   G option in test means that capture group data is not available in the
    //     expected results, so the check needs to be suppressed.
    if (isMatch == false && groupStarts.size() != 0) {
        dataerrln("Error at line %d:  Match expected, but none found.", line);
        failed = true;
        goto cleanupAndReturn;
    } else if (UTF8Matcher != nullptr && isUTF8Match == false && groupStarts.size() != 0) {
        errln("Error at line %d:  Match expected, but none found. (UTF8)", line);
        failed = true;
        goto cleanupAndReturn;
    }
    if (isMatch && groupStarts.size() == 0) {
        errln("Error at line %d: No match expected, but one found at position %d.", line, matcher->start(status));
        failed = true;
    }
    if (UTF8Matcher && isUTF8Match && groupStarts.size() == 0) {
        errln("Error at line %d: No match expected, but one found at position %d (UTF-8).", line, UTF8Matcher->start(status));
        failed = true;
    }

    if (flags.indexOf((char16_t)0x47 /*G*/) >= 0) {
        // Only check for match / no match.  Don't check capture groups.
        goto cleanupAndReturn;
    }

    REGEX_CHECK_STATUS_L(line);
    for (i=0; i<=matcher->groupCount(); i++) {
        int32_t  expectedStart = (i >= groupStarts.size()? -1 : groupStarts.elementAti(i));
        int32_t  expectedStartUTF8 = (i >= groupStartsUTF8.size()? -1 : groupStartsUTF8.elementAti(i));
        if (matcher->start(i, status) != expectedStart) {
            errln("Error at line %d: incorrect start position for group %d.  Expected %d, got %d",
                line, i, expectedStart, matcher->start(i, status));
            failed = true;
            goto cleanupAndReturn;  // Good chance of subsequent bogus errors.  Stop now.
        } else if (UTF8Matcher != nullptr && UTF8Matcher->start(i, status) != expectedStartUTF8) {
            errln("Error at line %d: incorrect start position for group %d.  Expected %d, got %d (UTF8)",
                  line, i, expectedStartUTF8, UTF8Matcher->start(i, status));
            failed = true;
            goto cleanupAndReturn;  // Good chance of subsequent bogus errors.  Stop now.
        }

        int32_t  expectedEnd = (i >= groupEnds.size()? -1 : groupEnds.elementAti(i));
        int32_t  expectedEndUTF8 = (i >= groupEndsUTF8.size()? -1 : groupEndsUTF8.elementAti(i));
        if (matcher->end(i, status) != expectedEnd) {
            errln("Error at line %d: incorrect end position for group %d.  Expected %d, got %d",
                line, i, expectedEnd, matcher->end(i, status));
            failed = true;
            // Error on end position;  keep going; real error is probably yet to come as group
            //   end positions work from end of the input data towards the front.
        } else if (UTF8Matcher != nullptr && UTF8Matcher->end(i, status) != expectedEndUTF8) {
            errln("Error at line %d: incorrect end position for group %d.  Expected %d, got %d (UTF8)",
                  line, i, expectedEndUTF8, UTF8Matcher->end(i, status));
            failed = true;
            // Error on end position;  keep going; real error is probably yet to come as group
            //   end positions work from end of the input data towards the front.
        }
    }
    if ( matcher->groupCount()+1 < groupStarts.size()) {
        errln("Error at line %d: Expected %d capture groups, found %d.",
            line, groupStarts.size()-1, matcher->groupCount());
        failed = true;
        }
    else if (UTF8Matcher != nullptr && UTF8Matcher->groupCount()+1 < groupStarts.size()) {
        errln("Error at line %d: Expected %d capture groups, found %d. (UTF8)",
              line, groupStarts.size()-1, UTF8Matcher->groupCount());
        failed = true;
    }

    if ((flags.indexOf((char16_t)0x59) >= 0) &&   //  'Y' flag:  RequireEnd() == false
        matcher->requireEnd() == true) {
        errln("Error at line %d: requireEnd() returned true.  Expected false", line);
        failed = true;
    } else if (UTF8Matcher != nullptr && (flags.indexOf((char16_t)0x59) >= 0) &&   //  'Y' flag:  RequireEnd() == false
        UTF8Matcher->requireEnd() == true) {
        errln("Error at line %d: requireEnd() returned true.  Expected false (UTF8)", line);
        failed = true;
    }

    if ((flags.indexOf((char16_t)0x79) >= 0) &&   //  'y' flag:  RequireEnd() == true
        matcher->requireEnd() == false) {
        errln("Error at line %d: requireEnd() returned false.  Expected true", line);
        failed = true;
    } else if (UTF8Matcher != nullptr && (flags.indexOf((char16_t)0x79) >= 0) &&   //  'Y' flag:  RequireEnd() == false
        UTF8Matcher->requireEnd() == false) {
        errln("Error at line %d: requireEnd() returned false.  Expected true (UTF8)", line);
        failed = true;
    }

    if ((flags.indexOf((char16_t)0x5A) >= 0) &&   //  'Z' flag:  hitEnd() == false
        matcher->hitEnd() == true) {
        errln("Error at line %d: hitEnd() returned true.  Expected false", line);
        failed = true;
    } else if (UTF8Matcher != nullptr && (flags.indexOf((char16_t)0x5A) >= 0) &&   //  'Z' flag:  hitEnd() == false
               UTF8Matcher->hitEnd() == true) {
        errln("Error at line %d: hitEnd() returned true.  Expected false (UTF8)", line);
        failed = true;
    }

    if ((flags.indexOf((char16_t)0x7A) >= 0) &&   //  'z' flag:  hitEnd() == true
        matcher->hitEnd() == false) {
        errln("Error at line %d: hitEnd() returned false.  Expected true", line);
        failed = true;
    } else if (UTF8Matcher != nullptr && (flags.indexOf((char16_t)0x7A) >= 0) &&   //  'z' flag:  hitEnd() == true
               UTF8Matcher->hitEnd() == false) {
        errln("Error at line %d: hitEnd() returned false.  Expected true (UTF8)", line);
        failed = true;
    }


cleanupAndReturn:
    if (failed) {
        infoln((UnicodeString)"\""+pattern+(UnicodeString)"\"  "
            +flags+(UnicodeString)"  \""+inputString+(UnicodeString)"\"");
        // callerPattern->dump();
    }
    delete parseMatcher;
    delete parsePat;
    delete UTF8Matcher;
    delete UTF8Pattern;
    delete matcher;
    delete callerPattern;

    utext_close(&inputText);
    delete[] inputChars;
    utext_close(&patternText);
    delete[] patternChars;
    ucnv_close(UTF8Converter);
}




//---------------------------------------------------------------------------
//
//      Errors     Check for error handling in patterns.
//
//---------------------------------------------------------------------------
void RegexTest::Errors() {
    // \escape sequences that aren't implemented yet.
    //REGEX_ERR("hex format \\x{abcd} not implemented", 1, 13, U_REGEX_UNIMPLEMENTED);

    // Missing close parentheses
    REGEX_ERR("Comment (?# with no close", 1, 25, U_REGEX_MISMATCHED_PAREN);
    REGEX_ERR("Capturing Parenthesis(...", 1, 25, U_REGEX_MISMATCHED_PAREN);
    REGEX_ERR("Grouping only parens (?: blah blah", 1, 34, U_REGEX_MISMATCHED_PAREN);

    // Extra close paren
    REGEX_ERR("Grouping only parens (?: blah)) blah", 1, 31, U_REGEX_MISMATCHED_PAREN);
    REGEX_ERR(")))))))", 1, 1, U_REGEX_MISMATCHED_PAREN);
    REGEX_ERR("(((((((", 1, 7, U_REGEX_MISMATCHED_PAREN);

    // Look-ahead, Look-behind
    //  TODO:  add tests for unbounded length look-behinds.
    REGEX_ERR("abc(?<@xyz).*", 1, 7, U_REGEX_RULE_SYNTAX);       // illegal construct

    // Attempt to use non-default flags
    {
        UParseError   pe;
        UErrorCode    status = U_ZERO_ERROR;
        int32_t       flags  = UREGEX_CANON_EQ |
                               UREGEX_COMMENTS         | UREGEX_DOTALL   |
                               UREGEX_MULTILINE;
        RegexPattern *pat1= RegexPattern::compile(".*", flags, pe, status);
        REGEX_ASSERT(status == U_REGEX_UNIMPLEMENTED);
        delete pat1;
    }


    // Quantifiers are allowed only after something that can be quantified.
    REGEX_ERR("+", 1, 1, U_REGEX_RULE_SYNTAX);
    REGEX_ERR("abc\ndef(*2)", 2, 5, U_REGEX_RULE_SYNTAX);
    REGEX_ERR("abc**", 1, 5, U_REGEX_RULE_SYNTAX);

    // Mal-formed {min,max} quantifiers
    REGEX_ERR("abc{a,2}",1,5, U_REGEX_BAD_INTERVAL);
    REGEX_ERR("abc{4,2}",1,8, U_REGEX_MAX_LT_MIN);
    REGEX_ERR("abc{1,b}",1,7, U_REGEX_BAD_INTERVAL);
    REGEX_ERR("abc{1,,2}",1,7, U_REGEX_BAD_INTERVAL);
    REGEX_ERR("abc{1,2a}",1,8, U_REGEX_BAD_INTERVAL);
    REGEX_ERR("abc{222222222222222222222}",1,14, U_REGEX_NUMBER_TOO_BIG);
    REGEX_ERR("abc{5,50000000000}", 1, 16, U_REGEX_NUMBER_TOO_BIG);        // Overflows int during scan
    REGEX_ERR("abc{5,687865858}", 1, 16, U_REGEX_NUMBER_TOO_BIG);          // Overflows regex binary format
    REGEX_ERR("abc{687865858,687865859}", 1, 24, U_REGEX_NUMBER_TOO_BIG);

    // Ticket 5389
    REGEX_ERR("*c", 1, 1, U_REGEX_RULE_SYNTAX);

    // Invalid Back Reference \0
    //    For ICU 3.8 and earlier
    //    For ICU versions newer than 3.8, \0 introduces an octal escape.
    //
    REGEX_ERR("(ab)\\0", 1, 6, U_REGEX_BAD_ESCAPE_SEQUENCE);

}

//-------------------------------------------------------------------------------
//
//   PerlTests  - Run Perl's regular expression tests
//                The input file for this test is re_tests, the standard regular
//                expression test data distributed with the Perl source code.
//
//                Here is Perl's description of the test data file:
//
//        # The tests are in a separate file 't/op/re_tests'.
//        # Each line in that file is a separate test.
//        # There are five columns, separated by tabs.
//        #
//        # Column 1 contains the pattern, optionally enclosed in C<''>.
//        # Modifiers can be put after the closing C<'>.
//        #
//        # Column 2 contains the string to be matched.
//        #
//        # Column 3 contains the expected result:
//        #     y   expect a match
//        #     n   expect no match
//        #     c   expect an error
//        # B   test exposes a known bug in Perl, should be skipped
//        # b   test exposes a known bug in Perl, should be skipped if noamp
//        #
//        # Columns 4 and 5 are used only if column 3 contains C<y> or C<c>.
//        #
//        # Column 4 contains a string, usually C<$&>.
//        #
//        # Column 5 contains the expected result of double-quote
//        # interpolating that string after the match, or start of error message.
//        #
//        # Column 6, if present, contains a reason why the test is skipped.
//        # This is printed with "skipped", for harness to pick up.
//        #
//        # \n in the tests are interpolated, as are variables of the form ${\w+}.
//        #
//        # If you want to add a regular expression test that can't be expressed
//        # in this format, don't add it here: put it in op/pat.t instead.
//
//        For ICU, if field 3 contains an 'i', the test will be skipped.
//        The test exposes is some known incompatibility between ICU and Perl regexps.
//        (The i is in addition to whatever was there before.)
//
//-------------------------------------------------------------------------------
void RegexTest::PerlTests() {
    char tdd[2048];
    const char *srcPath;
    UErrorCode  status = U_ZERO_ERROR;
    UParseError pe;

    //
    //  Open and read the test data file.
    //
    srcPath=getPath(tdd, "re_tests.txt");
    if(srcPath==nullptr) {
        return; /* something went wrong, error already output */
    }

    int32_t    len;
    char16_t *testData = ReadAndConvertFile(srcPath, len, "iso-8859-1", status);
    if (U_FAILURE(status)) {
        return; /* something went wrong, error already output */
    }

    //
    //  Put the test data into a UnicodeString
    //
    UnicodeString testDataString(false, testData, len);

    //
    //  Regex to break the input file into lines, and strip the new lines.
    //     One line per match, capture group one is the desired data.
    //
    RegexPattern* linePat = RegexPattern::compile(UNICODE_STRING_SIMPLE("(.+?)[\\r\\n]+"), 0, pe, status);
    if (U_FAILURE(status)) {
        dataerrln("RegexPattern::compile() error");
        return;
    }
    RegexMatcher* lineMat = linePat->matcher(testDataString, status);

    //
    //  Regex to split a test file line into fields.
    //    There are six fields, separated by tabs.
    //
    RegexPattern* fieldPat = RegexPattern::compile(UNICODE_STRING_SIMPLE("\\t"), 0, pe, status);

    //
    //  Regex to identify test patterns with flag settings, and to separate them.
    //    Test patterns with flags look like 'pattern'i
    //    Test patterns without flags are not quoted:   pattern
    //   Coming out, capture group 2 is the pattern, capture group 3 is the flags.
    //
    RegexPattern *flagPat = RegexPattern::compile(UNICODE_STRING_SIMPLE("('?)(.*)\\1(.*)"), 0, pe, status);
    RegexMatcher* flagMat = flagPat->matcher(status);

    //
    // The Perl tests reference several perl-isms, which are evaluated/substituted
    //   in the test data.  Not being perl, this must be done explicitly.  Here
    //   are string constants and REs for these constructs.
    //
    UnicodeString nulnulSrc("${nulnul}");
    UnicodeString nulnul("\\u0000\\u0000", -1, US_INV);
    nulnul = nulnul.unescape();

    UnicodeString ffffSrc("${ffff}");
    UnicodeString ffff("\\uffff", -1, US_INV);
    ffff = ffff.unescape();

    //  regexp for $-[0], $+[2], etc.
    RegexPattern *groupsPat = RegexPattern::compile(UNICODE_STRING_SIMPLE("\\$([+\\-])\\[(\\d+)\\]"), 0, pe, status);
    RegexMatcher *groupsMat = groupsPat->matcher(status);

    //  regexp for $0, $1, $2, etc.
    RegexPattern *cgPat = RegexPattern::compile(UNICODE_STRING_SIMPLE("\\$(\\d+)"), 0, pe, status);
    RegexMatcher *cgMat = cgPat->matcher(status);


    //
    // Main Loop for the Perl Tests, runs once per line from the
    //   test data file.
    //
    int32_t  lineNum = 0;
    int32_t  skippedUnimplementedCount = 0;
    while (lineMat->find()) {
        lineNum++;

        //
        //  Get a line, break it into its fields, do the Perl
        //    variable substitutions.
        //
        UnicodeString line = lineMat->group(1, status);
        UnicodeString fields[7];
        fieldPat->split(line, fields, 7, status);

        flagMat->reset(fields[0]);
        flagMat->matches(status);
        UnicodeString pattern  = flagMat->group(2, status);
        pattern.findAndReplace("${bang}", "!");
        pattern.findAndReplace(nulnulSrc, UNICODE_STRING_SIMPLE("\\u0000\\u0000"));
        pattern.findAndReplace(ffffSrc, ffff);

        //
        //  Identify patterns that include match flag settings,
        //    split off the flags, remove the extra quotes.
        //
        UnicodeString flagStr = flagMat->group(3, status);
        if (U_FAILURE(status)) {
            errln("ucnv_toUChars: ICU Error \"%s\"\n", u_errorName(status));
            return;
        }
        int32_t flags = 0;
        const char16_t UChar_c = 0x63;  // Char constants for the flag letters.
        const char16_t UChar_i = 0x69;  //   (Damn the lack of Unicode support in C)
        const char16_t UChar_m = 0x6d;
        const char16_t UChar_x = 0x78;
        const char16_t UChar_y = 0x79;
        if (flagStr.indexOf(UChar_i) != -1) {
            flags |= UREGEX_CASE_INSENSITIVE;
        }
        if (flagStr.indexOf(UChar_m) != -1) {
            flags |= UREGEX_MULTILINE;
        }
        if (flagStr.indexOf(UChar_x) != -1) {
            flags |= UREGEX_COMMENTS;
        }

        //
        // Compile the test pattern.
        //
        status = U_ZERO_ERROR;
        RegexPattern *testPat = RegexPattern::compile(pattern, flags, pe, status);
        if (status == U_REGEX_UNIMPLEMENTED) {
            //
            // Test of a feature that is planned for ICU, but not yet implemented.
            //   skip the test.
            skippedUnimplementedCount++;
            delete testPat;
            status = U_ZERO_ERROR;
            continue;
        }

        if (U_FAILURE(status)) {
            // Some tests are supposed to generate errors.
            //   Only report an error for tests that are supposed to succeed.
            if (fields[2].indexOf(UChar_c) == -1  &&  // Compilation is not supposed to fail AND
                fields[2].indexOf(UChar_i) == -1)     //   it's not an accepted ICU incompatibility
            {
                errln("line %d: ICU Error \"%s\"\n", lineNum, u_errorName(status));
            }
            status = U_ZERO_ERROR;
            delete testPat;
            continue;
        }

        if (fields[2].indexOf(UChar_i) >= 0) {
            // ICU should skip this test.
            delete testPat;
            continue;
        }

        if (fields[2].indexOf(UChar_c) >= 0) {
            // This pattern should have caused a compilation error, but didn't/
            errln("line %d: Expected a pattern compile error, got success.", lineNum);
            delete testPat;
            continue;
        }

        //
        // replace the Perl variables that appear in some of the
        //   match data strings.
        //
        UnicodeString matchString = fields[1];
        matchString.findAndReplace(nulnulSrc, nulnul);
        matchString.findAndReplace(ffffSrc,   ffff);

        // Replace any \n in the match string with an actual new-line char.
        //  Don't do full unescape, as this unescapes more than Perl does, which
        //  causes other spurious failures in the tests.
        matchString.findAndReplace(UNICODE_STRING_SIMPLE("\\n"), "\n");



        //
        // Run the test, check for expected match/don't match result.
        //
        RegexMatcher *testMat = testPat->matcher(matchString, status);
        UBool found = testMat->find();
        UBool expected = false;
        if (fields[2].indexOf(UChar_y) >=0) {
            expected = true;
        }
        if (expected != found) {
            errln("line %d: Expected %smatch, got %smatch",
                lineNum, expected?"":"no ", found?"":"no " );
            delete testMat;
            delete testPat;
            continue;
        }

        // Don't try to check expected results if there is no match.
        //   (Some have stuff in the expected fields)
        if (!found) {
            delete testMat;
            delete testPat;
            continue;
        }

        //
        // Interpret the Perl expression from the fourth field of the data file,
        // building up an ICU string from the results of the ICU match.
        //   The Perl expression will contain references to the results of
        //     a regex match, including the matched string, capture group strings,
        //     group starting and ending indices, etc.
        //
        UnicodeString resultString;
        UnicodeString perlExpr = fields[3];
#if SUPPORT_MUTATING_INPUT_STRING
        groupsMat->reset(perlExpr);
        cgMat->reset(perlExpr);
#endif

        while (perlExpr.length() > 0) {
#if !SUPPORT_MUTATING_INPUT_STRING
            //  Preferred usage.  Reset after any modification to input string.
            groupsMat->reset(perlExpr);
            cgMat->reset(perlExpr);
#endif

            if (perlExpr.startsWith("$&")) {
                resultString.append(testMat->group(status));
                perlExpr.remove(0, 2);
            }

            else if (groupsMat->lookingAt(status)) {
                // $-[0]   $+[2]  etc.
                UnicodeString digitString = groupsMat->group(2, status);
                int32_t t = 0;
                int32_t groupNum = ICU_Utility::parseNumber(digitString, t, 10);
                UnicodeString plusOrMinus = groupsMat->group(1, status);
                int32_t matchPosition;
                if (plusOrMinus.compare("+") == 0) {
                    matchPosition = testMat->end(groupNum, status);
                } else {
                    matchPosition = testMat->start(groupNum, status);
                }
                if (matchPosition != -1) {
                    ICU_Utility::appendNumber(resultString, matchPosition);
                }
                perlExpr.remove(0, groupsMat->end(status));
            }

            else if (cgMat->lookingAt(status)) {
                // $1, $2, $3, etc.
                UnicodeString digitString = cgMat->group(1, status);
                int32_t t = 0;
                int32_t groupNum = ICU_Utility::parseNumber(digitString, t, 10);
                if (U_SUCCESS(status)) {
                    resultString.append(testMat->group(groupNum, status));
                    status = U_ZERO_ERROR;
                }
                perlExpr.remove(0, cgMat->end(status));
            }

            else if (perlExpr.startsWith("@-")) {
                int32_t i;
                for (i=0; i<=testMat->groupCount(); i++) {
                    if (i>0) {
                        resultString.append(" ");
                    }
                    ICU_Utility::appendNumber(resultString, testMat->start(i, status));
                }
                perlExpr.remove(0, 2);
            }

            else if (perlExpr.startsWith("@+")) {
                int32_t i;
                for (i=0; i<=testMat->groupCount(); i++) {
                    if (i>0) {
                        resultString.append(" ");
                    }
                    ICU_Utility::appendNumber(resultString, testMat->end(i, status));
                }
                perlExpr.remove(0, 2);
            }

            else if (perlExpr.startsWith(UNICODE_STRING_SIMPLE("\\"))) {    // \Escape.  Take following char as a literal.
                                                     //           or as an escaped sequence (e.g. \n)
                if (perlExpr.length() > 1) {
                    perlExpr.remove(0, 1);  // Remove the '\', but only if not last char.
                }
                char16_t c = perlExpr.charAt(0);
                switch (c) {
                case 'n':   c = '\n'; break;
                // add any other escape sequences that show up in the test expected results.
                }
                resultString.append(c);
                perlExpr.remove(0, 1);
            }

            else  {
                // Any characters from the perl expression that we don't explicitly
                //  recognize before here are assumed to be literals and copied
                //  as-is to the expected results.
                resultString.append(perlExpr.charAt(0));
                perlExpr.remove(0, 1);
            }

            if (U_FAILURE(status)) {
                errln("Line %d: ICU Error \"%s\"", lineNum, u_errorName(status));
                break;
            }
        }

        //
        // Expected Results Compare
        //
        UnicodeString expectedS(fields[4]);
        expectedS.findAndReplace(nulnulSrc, nulnul);
        expectedS.findAndReplace(ffffSrc,   ffff);
        expectedS.findAndReplace(UNICODE_STRING_SIMPLE("\\n"), "\n");


        if (expectedS.compare(resultString) != 0) {
            err("Line %d: Incorrect perl expression results.", lineNum);
            infoln((UnicodeString)"Expected \""+expectedS+(UnicodeString)"\"; got \""+resultString+(UnicodeString)"\"");
        }

        delete testMat;
        delete testPat;
    }

    //
    // All done.  Clean up allocated stuff.
    //
    delete cgMat;
    delete cgPat;

    delete groupsMat;
    delete groupsPat;

    delete flagMat;
    delete flagPat;

    delete lineMat;
    delete linePat;

    delete fieldPat;
    delete [] testData;


    logln("%d tests skipped because of unimplemented regexp features.", skippedUnimplementedCount);

}


//-------------------------------------------------------------------------------
//
//   PerlTestsUTF8  Run Perl's regular expression tests on UTF-8-based UTexts
//                  (instead of using UnicodeStrings) to test the alternate engine.
//                  The input file for this test is re_tests, the standard regular
//                  expression test data distributed with the Perl source code.
//                  See PerlTests() for more information.
//
//-------------------------------------------------------------------------------
void RegexTest::PerlTestsUTF8() {
    char tdd[2048];
    const char *srcPath;
    UErrorCode  status = U_ZERO_ERROR;
    UParseError pe;
    LocalUConverterPointer UTF8Converter(ucnv_open("UTF-8", &status));
    UText       patternText = UTEXT_INITIALIZER;
    char       *patternChars = nullptr;
    int32_t     patternLength;
    int32_t     patternCapacity = 0;
    UText       inputText = UTEXT_INITIALIZER;
    char       *inputChars = nullptr;
    int32_t     inputLength;
    int32_t     inputCapacity = 0;

    ucnv_setFromUCallBack(UTF8Converter.getAlias(), UCNV_FROM_U_CALLBACK_STOP, nullptr, nullptr, nullptr, &status);

    //
    //  Open and read the test data file.
    //
    srcPath=getPath(tdd, "re_tests.txt");
    if(srcPath==nullptr) {
        return; /* something went wrong, error already output */
    }

    int32_t    len;
    char16_t *testData = ReadAndConvertFile(srcPath, len, "iso-8859-1", status);
    if (U_FAILURE(status)) {
        return; /* something went wrong, error already output */
    }

    //
    //  Put the test data into a UnicodeString
    //
    UnicodeString testDataString(false, testData, len);

    //
    //  Regex to break the input file into lines, and strip the new lines.
    //     One line per match, capture group one is the desired data.
    //
    RegexPattern* linePat = RegexPattern::compile(UNICODE_STRING_SIMPLE("(.+?)[\\r\\n]+"), 0, pe, status);
    if (U_FAILURE(status)) {
        dataerrln("RegexPattern::compile() error");
        return;
    }
    RegexMatcher* lineMat = linePat->matcher(testDataString, status);

    //
    //  Regex to split a test file line into fields.
    //    There are six fields, separated by tabs.
    //
    RegexPattern* fieldPat = RegexPattern::compile(UNICODE_STRING_SIMPLE("\\t"), 0, pe, status);

    //
    //  Regex to identify test patterns with flag settings, and to separate them.
    //    Test patterns with flags look like 'pattern'i
    //    Test patterns without flags are not quoted:   pattern
    //   Coming out, capture group 2 is the pattern, capture group 3 is the flags.
    //
    RegexPattern *flagPat = RegexPattern::compile(UNICODE_STRING_SIMPLE("('?)(.*)\\1(.*)"), 0, pe, status);
    RegexMatcher* flagMat = flagPat->matcher(status);

    //
    // The Perl tests reference several perl-isms, which are evaluated/substituted
    //   in the test data.  Not being perl, this must be done explicitly.  Here
    //   are string constants and REs for these constructs.
    //
    UnicodeString nulnulSrc("${nulnul}");
    UnicodeString nulnul("\\u0000\\u0000", -1, US_INV);
    nulnul = nulnul.unescape();

    UnicodeString ffffSrc("${ffff}");
    UnicodeString ffff("\\uffff", -1, US_INV);
    ffff = ffff.unescape();

    //  regexp for $-[0], $+[2], etc.
    RegexPattern *groupsPat = RegexPattern::compile(UNICODE_STRING_SIMPLE("\\$([+\\-])\\[(\\d+)\\]"), 0, pe, status);
    RegexMatcher *groupsMat = groupsPat->matcher(status);

    //  regexp for $0, $1, $2, etc.
    RegexPattern *cgPat = RegexPattern::compile(UNICODE_STRING_SIMPLE("\\$(\\d+)"), 0, pe, status);
    RegexMatcher *cgMat = cgPat->matcher(status);


    //
    // Main Loop for the Perl Tests, runs once per line from the
    //   test data file.
    //
    int32_t  lineNum = 0;
    int32_t  skippedUnimplementedCount = 0;
    while (lineMat->find()) {
        lineNum++;

        //
        //  Get a line, break it into its fields, do the Perl
        //    variable substitutions.
        //
        UnicodeString line = lineMat->group(1, status);
        UnicodeString fields[7];
        fieldPat->split(line, fields, 7, status);

        flagMat->reset(fields[0]);
        flagMat->matches(status);
        UnicodeString pattern  = flagMat->group(2, status);
        pattern.findAndReplace("${bang}", "!");
        pattern.findAndReplace(nulnulSrc, UNICODE_STRING_SIMPLE("\\u0000\\u0000"));
        pattern.findAndReplace(ffffSrc, ffff);

        //
        //  Identify patterns that include match flag settings,
        //    split off the flags, remove the extra quotes.
        //
        UnicodeString flagStr = flagMat->group(3, status);
        if (U_FAILURE(status)) {
            errln("ucnv_toUChars: ICU Error \"%s\"\n", u_errorName(status));
            return;
        }
        int32_t flags = 0;
        const char16_t UChar_c = 0x63;  // Char constants for the flag letters.
        const char16_t UChar_i = 0x69;  //   (Damn the lack of Unicode support in C)
        const char16_t UChar_m = 0x6d;
        const char16_t UChar_x = 0x78;
        const char16_t UChar_y = 0x79;
        if (flagStr.indexOf(UChar_i) != -1) {
            flags |= UREGEX_CASE_INSENSITIVE;
        }
        if (flagStr.indexOf(UChar_m) != -1) {
            flags |= UREGEX_MULTILINE;
        }
        if (flagStr.indexOf(UChar_x) != -1) {
            flags |= UREGEX_COMMENTS;
        }

        //
        // Put the pattern in a UTF-8 UText
        //
        status = U_ZERO_ERROR;
        patternLength = pattern.extract(patternChars, patternCapacity, UTF8Converter.getAlias(), status);
        if (status == U_BUFFER_OVERFLOW_ERROR) {
            status = U_ZERO_ERROR;
            delete[] patternChars;
            patternCapacity = patternLength + 1;
            patternChars = new char[patternCapacity];
            pattern.extract(patternChars, patternCapacity, UTF8Converter.getAlias(), status);
        }
        utext_openUTF8(&patternText, patternChars, patternLength, &status);

        //
        // Compile the test pattern.
        //
        RegexPattern *testPat = RegexPattern::compile(&patternText, flags, pe, status);
        if (status == U_REGEX_UNIMPLEMENTED) {
            //
            // Test of a feature that is planned for ICU, but not yet implemented.
            //   skip the test.
            skippedUnimplementedCount++;
            delete testPat;
            status = U_ZERO_ERROR;
            continue;
        }

        if (U_FAILURE(status)) {
            // Some tests are supposed to generate errors.
            //   Only report an error for tests that are supposed to succeed.
            if (fields[2].indexOf(UChar_c) == -1  &&  // Compilation is not supposed to fail AND
                fields[2].indexOf(UChar_i) == -1)     //   it's not an accepted ICU incompatibility
            {
                errln("line %d: ICU Error \"%s\"\n", lineNum, u_errorName(status));
            }
            status = U_ZERO_ERROR;
            delete testPat;
            continue;
        }

        if (fields[2].indexOf(UChar_i) >= 0) {
            // ICU should skip this test.
            delete testPat;
            continue;
        }

        if (fields[2].indexOf(UChar_c) >= 0) {
            // This pattern should have caused a compilation error, but didn't/
            errln("line %d: Expected a pattern compile error, got success.", lineNum);
            delete testPat;
            continue;
        }


        //
        // replace the Perl variables that appear in some of the
        //   match data strings.
        //
        UnicodeString matchString = fields[1];
        matchString.findAndReplace(nulnulSrc, nulnul);
        matchString.findAndReplace(ffffSrc,   ffff);

        // Replace any \n in the match string with an actual new-line char.
        //  Don't do full unescape, as this unescapes more than Perl does, which
        //  causes other spurious failures in the tests.
        matchString.findAndReplace(UNICODE_STRING_SIMPLE("\\n"), "\n");

        //
        // Put the input in a UTF-8 UText
        //
        status = U_ZERO_ERROR;
        inputLength = matchString.extract(inputChars, inputCapacity, UTF8Converter.getAlias(), status);
        if (status == U_BUFFER_OVERFLOW_ERROR) {
            status = U_ZERO_ERROR;
            delete[] inputChars;
            inputCapacity = inputLength + 1;
            inputChars = new char[inputCapacity];
            matchString.extract(inputChars, inputCapacity, UTF8Converter.getAlias(), status);
        }
        utext_openUTF8(&inputText, inputChars, inputLength, &status);

        //
        // Run the test, check for expected match/don't match result.
        //
        RegexMatcher *testMat = &testPat->matcher(status)->reset(&inputText);
        UBool found = testMat->find();
        UBool expected = false;
        if (fields[2].indexOf(UChar_y) >=0) {
            expected = true;
        }
        if (expected != found) {
            errln("line %d: Expected %smatch, got %smatch",
                lineNum, expected?"":"no ", found?"":"no " );
            delete testMat;
            delete testPat;
            continue;
        }

        // Don't try to check expected results if there is no match.
        //   (Some have stuff in the expected fields)
        if (!found) {
            delete testMat;
            delete testPat;
            continue;
        }

        //
        // Interpret the Perl expression from the fourth field of the data file,
        // building up an ICU string from the results of the ICU match.
        //   The Perl expression will contain references to the results of
        //     a regex match, including the matched string, capture group strings,
        //     group starting and ending indices, etc.
        //
        UnicodeString resultString;
        UnicodeString perlExpr = fields[3];

        while (perlExpr.length() > 0) {
            groupsMat->reset(perlExpr);
            cgMat->reset(perlExpr);

            if (perlExpr.startsWith("$&")) {
                resultString.append(testMat->group(status));
                perlExpr.remove(0, 2);
            }

            else if (groupsMat->lookingAt(status)) {
                // $-[0]   $+[2]  etc.
                UnicodeString digitString = groupsMat->group(2, status);
                int32_t t = 0;
                int32_t groupNum = ICU_Utility::parseNumber(digitString, t, 10);
                UnicodeString plusOrMinus = groupsMat->group(1, status);
                int32_t matchPosition;
                if (plusOrMinus.compare("+") == 0) {
                    matchPosition = testMat->end(groupNum, status);
                } else {
                    matchPosition = testMat->start(groupNum, status);
                }
                if (matchPosition != -1) {
                    ICU_Utility::appendNumber(resultString, matchPosition);
                }
                perlExpr.remove(0, groupsMat->end(status));
            }

            else if (cgMat->lookingAt(status)) {
                // $1, $2, $3, etc.
                UnicodeString digitString = cgMat->group(1, status);
                int32_t t = 0;
                int32_t groupNum = ICU_Utility::parseNumber(digitString, t, 10);
                if (U_SUCCESS(status)) {
                    resultString.append(testMat->group(groupNum, status));
                    status = U_ZERO_ERROR;
                }
                perlExpr.remove(0, cgMat->end(status));
            }

            else if (perlExpr.startsWith("@-")) {
                int32_t i;
                for (i=0; i<=testMat->groupCount(); i++) {
                    if (i>0) {
                        resultString.append(" ");
                    }
                    ICU_Utility::appendNumber(resultString, testMat->start(i, status));
                }
                perlExpr.remove(0, 2);
            }

            else if (perlExpr.startsWith("@+")) {
                int32_t i;
                for (i=0; i<=testMat->groupCount(); i++) {
                    if (i>0) {
                        resultString.append(" ");
                    }
                    ICU_Utility::appendNumber(resultString, testMat->end(i, status));
                }
                perlExpr.remove(0, 2);
            }

            else if (perlExpr.startsWith(UNICODE_STRING_SIMPLE("\\"))) {    // \Escape.  Take following char as a literal.
                                                     //           or as an escaped sequence (e.g. \n)
                if (perlExpr.length() > 1) {
                    perlExpr.remove(0, 1);  // Remove the '\', but only if not last char.
                }
                char16_t c = perlExpr.charAt(0);
                switch (c) {
                case 'n':   c = '\n'; break;
                // add any other escape sequences that show up in the test expected results.
                }
                resultString.append(c);
                perlExpr.remove(0, 1);
            }

            else  {
                // Any characters from the perl expression that we don't explicitly
                //  recognize before here are assumed to be literals and copied
                //  as-is to the expected results.
                resultString.append(perlExpr.charAt(0));
                perlExpr.remove(0, 1);
            }

            if (U_FAILURE(status)) {
                errln("Line %d: ICU Error \"%s\"", lineNum, u_errorName(status));
                break;
            }
        }

        //
        // Expected Results Compare
        //
        UnicodeString expectedS(fields[4]);
        expectedS.findAndReplace(nulnulSrc, nulnul);
        expectedS.findAndReplace(ffffSrc,   ffff);
        expectedS.findAndReplace(UNICODE_STRING_SIMPLE("\\n"), "\n");


        if (expectedS.compare(resultString) != 0) {
            err("Line %d: Incorrect perl expression results.", lineNum);
            infoln((UnicodeString)"Expected \""+expectedS+(UnicodeString)"\"; got \""+resultString+(UnicodeString)"\"");
        }

        delete testMat;
        delete testPat;
    }

    //
    // All done.  Clean up allocated stuff.
    //
    delete cgMat;
    delete cgPat;

    delete groupsMat;
    delete groupsPat;

    delete flagMat;
    delete flagPat;

    delete lineMat;
    delete linePat;

    delete fieldPat;
    delete [] testData;

    utext_close(&patternText);
    utext_close(&inputText);

    delete [] patternChars;
    delete [] inputChars;


    logln("%d tests skipped because of unimplemented regexp features.", skippedUnimplementedCount);

}


//--------------------------------------------------------------
//
//  Bug6149   Verify limits to heap expansion for backtrack stack.
//             Use this pattern,
//                 "(a?){1,8000000}"
//             Note: was an unbounded upperbounds, but that now has loop-breaking enabled.
//                   This test is likely to be fragile, as further optimizations stop
//                   more cases of pointless looping in the match engine.
//
//---------------------------------------------------------------
void RegexTest::Bug6149() {
    UnicodeString pattern("(a?){1,8000000}");
    UnicodeString s("xyz");
    uint32_t flags = 0;
    UErrorCode status = U_ZERO_ERROR;

    RegexMatcher  matcher(pattern, s, flags, status);
    UBool result = false;
    REGEX_ASSERT_FAIL(result=matcher.matches(status), U_REGEX_STACK_OVERFLOW);
    REGEX_ASSERT(result == false);
 }


//
//   Callbacks()    Test the callback function.
//                  When set, callbacks occur periodically during matching operations,
//                  giving the application code the ability to abort the operation
//                  before it's normal completion.
//

struct callBackContext {
    RegexTest        *test;
    int32_t          maxCalls;
    int32_t          numCalls;
    int32_t          lastSteps;
    void reset(int32_t max) {maxCalls=max; numCalls=0; lastSteps=0;}
};

U_CDECL_BEGIN
static UBool U_CALLCONV
testCallBackFn(const void *context, int32_t steps) {
    callBackContext  *info = static_cast<callBackContext *>(const_cast<void*>(context));
    if (info->lastSteps+1 != steps) {
        info->test->errln("incorrect steps in callback.  Expected %d, got %d\n", info->lastSteps+1, steps);
    }
    info->lastSteps = steps;
    info->numCalls++;
    return (info->numCalls < info->maxCalls);
}
U_CDECL_END

void RegexTest::Callbacks() {
   {
        // Getter returns NULLs if no callback has been set

        //   The variables that the getter will fill in.
        //   Init to non-null values so that the action of the getter can be seen.
        const void          *returnedContext = &returnedContext;
        URegexMatchCallback *returnedFn = &testCallBackFn;

        UErrorCode status = U_ZERO_ERROR;
        RegexMatcher matcher("x", 0, status);
        REGEX_CHECK_STATUS;
        matcher.getMatchCallback(returnedFn, returnedContext, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(returnedFn == nullptr);
        REGEX_ASSERT(returnedContext == nullptr);
    }

   {
        // Set and Get work
        callBackContext cbInfo = {this, 0, 0, 0};
        const void          *returnedContext;
        URegexMatchCallback *returnedFn;
        UErrorCode status = U_ZERO_ERROR;
        RegexMatcher matcher(UNICODE_STRING_SIMPLE("((.)+\\2)+x"), 0, status);  // A pattern that can run long.
        REGEX_CHECK_STATUS;
        matcher.setMatchCallback(testCallBackFn, &cbInfo, status);
        REGEX_CHECK_STATUS;
        matcher.getMatchCallback(returnedFn, returnedContext, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(returnedFn == testCallBackFn);
        REGEX_ASSERT(returnedContext == &cbInfo);

        // A short-running match shouldn't invoke the callback
        status = U_ZERO_ERROR;
        cbInfo.reset(1);
        UnicodeString s = "xxx";
        matcher.reset(s);
        REGEX_ASSERT(matcher.matches(status));
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(cbInfo.numCalls == 0);

        // A medium-length match that runs long enough to invoke the
        //   callback, but not so long that the callback aborts it.
        status = U_ZERO_ERROR;
        cbInfo.reset(4);
        s = "aaaaaaaaaaaaaaaaaaab";
        matcher.reset(s);
        REGEX_ASSERT(matcher.matches(status)==false);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(cbInfo.numCalls > 0);

        // A longer running match that the callback function will abort.
        status = U_ZERO_ERROR;
        cbInfo.reset(4);
        s = "aaaaaaaaaaaaaaaaaaaaaaab";
        matcher.reset(s);
        REGEX_ASSERT(matcher.matches(status)==false);
        REGEX_ASSERT(status == U_REGEX_STOPPED_BY_CALLER);
        REGEX_ASSERT(cbInfo.numCalls == 4);

        // A longer running find that the callback function will abort.
        status = U_ZERO_ERROR;
        cbInfo.reset(4);
        s = "aaaaaaaaaaaaaaaaaaaaaaab";
        matcher.reset(s);
        REGEX_ASSERT(matcher.find(status)==false);
        REGEX_ASSERT(status == U_REGEX_STOPPED_BY_CALLER);
        REGEX_ASSERT(cbInfo.numCalls == 4);
    }


}


//
//   FindProgressCallbacks()    Test the find "progress" callback function.
//                  When set, the find progress callback will be invoked during a find operations
//                  after each return from a match attempt, giving the application the opportunity
//                  to terminate a long-running find operation before it's normal completion.
//

struct progressCallBackContext {
    RegexTest        *test;
    int64_t          lastIndex;
    int32_t          maxCalls;
    int32_t          numCalls;
    void reset(int32_t max) {maxCalls=max; numCalls=0;lastIndex=0;}
};

// call-back function for find().
// Return true to continue the find().
// Return false to stop the find().
U_CDECL_BEGIN
static UBool U_CALLCONV
testProgressCallBackFn(const void *context, int64_t matchIndex) {
    progressCallBackContext  *info = (progressCallBackContext *)context;
    info->numCalls++;
    info->lastIndex = matchIndex;
//    info->test->infoln("ProgressCallback - matchIndex = %d, numCalls = %d\n", matchIndex, info->numCalls);
    return (info->numCalls < info->maxCalls);
}
U_CDECL_END

void RegexTest::FindProgressCallbacks() {
   {
        // Getter returns NULLs if no callback has been set

        //   The variables that the getter will fill in.
        //   Init to non-null values so that the action of the getter can be seen.
        const void                  *returnedContext = &returnedContext;
        URegexFindProgressCallback  *returnedFn = &testProgressCallBackFn;

        UErrorCode status = U_ZERO_ERROR;
        RegexMatcher matcher("x", 0, status);
        REGEX_CHECK_STATUS;
        matcher.getFindProgressCallback(returnedFn, returnedContext, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(returnedFn == nullptr);
        REGEX_ASSERT(returnedContext == nullptr);
    }

   {
        // Set and Get work
        progressCallBackContext cbInfo = {this, 0, 0, 0};
        const void                  *returnedContext;
        URegexFindProgressCallback  *returnedFn;
        UErrorCode status = U_ZERO_ERROR;
        RegexMatcher matcher(UNICODE_STRING_SIMPLE("((.)\\2)x"), 0, status);
        REGEX_CHECK_STATUS;
        matcher.setFindProgressCallback(testProgressCallBackFn, &cbInfo, status);
        REGEX_CHECK_STATUS;
        matcher.getFindProgressCallback(returnedFn, returnedContext, status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(returnedFn == testProgressCallBackFn);
        REGEX_ASSERT(returnedContext == &cbInfo);

        // A find that matches on the initial position does NOT invoke the callback.
        status = U_ZERO_ERROR;
        cbInfo.reset(100);
        UnicodeString s = "aaxxx";
        matcher.reset(s);
#if 0
        matcher.setTrace(true);
#endif
        REGEX_ASSERT(matcher.find(0, status));
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(cbInfo.numCalls == 0);

        // A medium running find() that causes matcher.find() to invoke our callback for each index,
        //   but not so many times that we interrupt the operation.
        status = U_ZERO_ERROR;
        s = "aaaaaaaaaaaaaaaaaaab";
        cbInfo.reset(s.length()); //  Some upper limit for number of calls that is greater than size of our input string
        matcher.reset(s);
        REGEX_ASSERT(matcher.find(0, status)==false);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(cbInfo.numCalls > 0 && cbInfo.numCalls < 25);

        // A longer running match that causes matcher.find() to invoke our callback which we cancel/interrupt at some point.
        status = U_ZERO_ERROR;
        UnicodeString s1 = "aaaaaaaaaaaaaaaaaaaaaaab";
        cbInfo.reset(s1.length() - 5); //  Bail early somewhere near the end of input string
        matcher.reset(s1);
        REGEX_ASSERT(matcher.find(0, status)==false);
        REGEX_ASSERT(status == U_REGEX_STOPPED_BY_CALLER);
        REGEX_ASSERT(cbInfo.numCalls == s1.length() - 5);

        // Now a match that will succeed, but after an interruption
        status = U_ZERO_ERROR;
        UnicodeString s2 = "aaaaaaaaaaaaaa aaaaaaaaab xxx";
        cbInfo.reset(s2.length() - 10); //  Bail early somewhere near the end of input string
        matcher.reset(s2);
        REGEX_ASSERT(matcher.find(0, status)==false);
        REGEX_ASSERT(status == U_REGEX_STOPPED_BY_CALLER);
        // Now retry the match from where left off
        cbInfo.maxCalls = 100; //  No callback limit
        status = U_ZERO_ERROR;
        REGEX_ASSERT(matcher.find(cbInfo.lastIndex, status));
        REGEX_CHECK_STATUS;
    }


}


//---------------------------------------------------------------------------
//
//    PreAllocatedUTextCAPI    Check the C API with pre-allocated mutable
//                             UTexts. The pure-C implementation of UText
//                             has no mutable backing stores, but we can
//                             use UnicodeString here to test the functionality.
//
//---------------------------------------------------------------------------
void RegexTest::PreAllocatedUTextCAPI () {
    UErrorCode           status = U_ZERO_ERROR;
    URegularExpression  *re;
    UText                patternText = UTEXT_INITIALIZER;
    UnicodeString        buffer;
    UText                bufferText = UTEXT_INITIALIZER;

    utext_openUnicodeString(&bufferText, &buffer, &status);

    /*
     *  getText() and getUText()
     */
    {
        UText  text1 = UTEXT_INITIALIZER;
        UText  text2 = UTEXT_INITIALIZER;
        char16_t  text2Chars[20];
        UText  *resultText;

        status = U_ZERO_ERROR;
        regextst_openUTF8FromInvariant(&text1, "abcccd", -1, &status);
        regextst_openUTF8FromInvariant(&text2, "abcccxd", -1, &status);
        u_uastrncpy(text2Chars, "abcccxd", sizeof(text2)/2);
        utext_openUChars(&text2, text2Chars, -1, &status);

        regextst_openUTF8FromInvariant(&patternText, "abc*d", -1, &status);
        re = uregex_openUText(&patternText, 0, nullptr, &status);

        /* First set a UText */
        uregex_setUText(re, &text1, &status);
        resultText = uregex_getUText(re, &bufferText, &status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(resultText == &bufferText);
        utext_setNativeIndex(resultText, 0);
        utext_setNativeIndex(&text1, 0);
        REGEX_ASSERT(testUTextEqual(resultText, &text1));

        resultText = uregex_getUText(re, &bufferText, &status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(resultText == &bufferText);
        utext_setNativeIndex(resultText, 0);
        utext_setNativeIndex(&text1, 0);
        REGEX_ASSERT(testUTextEqual(resultText, &text1));

        /* Then set a char16_t * */
        uregex_setText(re, text2Chars, 7, &status);
        resultText = uregex_getUText(re, &bufferText, &status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(resultText == &bufferText);
        utext_setNativeIndex(resultText, 0);
        utext_setNativeIndex(&text2, 0);
        REGEX_ASSERT(testUTextEqual(resultText, &text2));

        uregex_close(re);
        utext_close(&text1);
        utext_close(&text2);
    }

    /*
     *  group()
     */
    {
        char16_t text1[80];
        UText   *actual;
        UBool    result;
        int64_t  length = 0;

        u_uastrncpy(text1, "noise abc interior def, and this is off the end",  UPRV_LENGTHOF(text1));
        //                  012345678901234567890123456789012345678901234567
        //                  0         1         2         3         4

        status = U_ZERO_ERROR;
        re = uregex_openC("abc(.*?)def", 0, nullptr, &status);
        REGEX_CHECK_STATUS;

        uregex_setText(re, text1, -1, &status);
        result = uregex_find(re, 0, &status);
        REGEX_ASSERT(result==true);

        /*  Capture Group 0, the full match.  Should succeed. "abc interior def" */
        status = U_ZERO_ERROR;
        actual = uregex_groupUText(re, 0, &bufferText, &length, &status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(actual == &bufferText);
        REGEX_ASSERT(utext_getNativeIndex(actual) == 6);
        REGEX_ASSERT(length == 16);
        REGEX_ASSERT(utext_nativeLength(actual) == 47);

        /*  Capture group #1.  Should succeed, matching " interior ". */
        status = U_ZERO_ERROR;
        actual = uregex_groupUText(re, 1, &bufferText, &length, &status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(actual == &bufferText);
        REGEX_ASSERT(utext_getNativeIndex(actual) == 9);   // position of " interior "
        REGEX_ASSERT(length == 10);
        REGEX_ASSERT(utext_nativeLength(actual) == 47);

        /*  Capture group out of range.  Error. */
        status = U_ZERO_ERROR;
        actual = uregex_groupUText(re, 2, &bufferText, &length, &status);
        REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);
        REGEX_ASSERT(actual == &bufferText);
        uregex_close(re);

    }

    /*
     *  replaceFirst()
     */
    {
        char16_t text1[80];
        char16_t text2[80];
        UText    replText = UTEXT_INITIALIZER;
        UText   *result;
        status = U_ZERO_ERROR;
        utext_openUnicodeString(&bufferText, &buffer, &status);

        status = U_ZERO_ERROR;
        u_uastrncpy(text1, "Replace xaax x1x x...x.",  UPRV_LENGTHOF(text1));
        u_uastrncpy(text2, "No match here.",  UPRV_LENGTHOF(text2)/2);
        regextst_openUTF8FromInvariant(&replText, "<$1>", -1, &status);

        re = uregex_openC("x(.*?)x", 0, nullptr, &status);
        REGEX_CHECK_STATUS;

        /*  Normal case, with match */
        uregex_setText(re, text1, -1, &status);
        REGEX_CHECK_STATUS;
        utext_replace(&bufferText, 0, utext_nativeLength(&bufferText), nullptr, 0, &status);
        REGEX_CHECK_STATUS;
        result = uregex_replaceFirstUText(re, &replText, &bufferText, &status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == &bufferText);
        REGEX_ASSERT_UTEXT_INVARIANT("Replace <aa> x1x x...x.", result);

        /* No match.  Text should copy to output with no changes.  */
        uregex_setText(re, text2, -1, &status);
        utext_replace(&bufferText, 0, utext_nativeLength(&bufferText), nullptr, 0, &status);
        result = uregex_replaceFirstUText(re, &replText, &bufferText, &status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == &bufferText);
        REGEX_ASSERT_UTEXT_INVARIANT("No match here.", result);

        /* Unicode escapes */
        uregex_setText(re, text1, -1, &status);
        regextst_openUTF8FromInvariant(&replText, "\\\\\\u0041$1\\U00000042\\$\\a", -1, &status);
        utext_replace(&bufferText, 0, utext_nativeLength(&bufferText), nullptr, 0, &status);
        result = uregex_replaceFirstUText(re, &replText, &bufferText, &status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == &bufferText);
        REGEX_ASSERT_UTEXT_INVARIANT("Replace \\AaaB$a x1x x...x.", result);

        uregex_close(re);
        utext_close(&replText);
    }


    /*
     *  replaceAll()
     */
    {
        char16_t text1[80];
        char16_t text2[80];
        UText    replText = UTEXT_INITIALIZER;
        UText   *result;

        status = U_ZERO_ERROR;
        u_uastrncpy(text1, "Replace xaax x1x x...x.",  sizeof(text1)/2);
        u_uastrncpy(text2, "No match here.",  sizeof(text2)/2);
        regextst_openUTF8FromInvariant(&replText, "<$1>", -1, &status);

        re = uregex_openC("x(.*?)x", 0, nullptr, &status);
        REGEX_CHECK_STATUS;

        /*  Normal case, with match */
        uregex_setText(re, text1, -1, &status);
        utext_replace(&bufferText, 0, utext_nativeLength(&bufferText), nullptr, 0, &status);
        result = uregex_replaceAllUText(re, &replText, &bufferText, &status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == &bufferText);
        REGEX_ASSERT_UTEXT_INVARIANT("Replace <aa> <1> <...>.", result);

        /* No match.  Text should copy to output with no changes.  */
        uregex_setText(re, text2, -1, &status);
        utext_replace(&bufferText, 0, utext_nativeLength(&bufferText), nullptr, 0, &status);
        result = uregex_replaceAllUText(re, &replText, &bufferText, &status);
        REGEX_CHECK_STATUS;
        REGEX_ASSERT(result == &bufferText);
        REGEX_ASSERT_UTEXT_INVARIANT("No match here.", result);

        uregex_close(re);
        utext_close(&replText);
    }


    /*
     *  splitUText() uses the C++ API directly, and the UnicodeString version uses mutable UTexts,
     *   so we don't need to test it here.
     */

    utext_close(&bufferText);
    utext_close(&patternText);
}


//--------------------------------------------------------------
//
//  NamedCapture   Check basic named capture group functionality
//
//--------------------------------------------------------------
void RegexTest::NamedCapture() {
    UErrorCode status = U_ZERO_ERROR;
    RegexPattern *pat = RegexPattern::compile(UnicodeString(
            "abc()()(?<three>xyz)(de)(?<five>hmm)(?<six>oh)f\\k<five>"), 0, status);
    REGEX_CHECK_STATUS;
    int32_t group = pat->groupNumberFromName("five", -1, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(5 == group);
    group = pat->groupNumberFromName("three", -1, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(3 == group);

    status = U_ZERO_ERROR;
    group = pat->groupNumberFromName(UnicodeString("six"), status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(6 == group);

    status = U_ZERO_ERROR;
    group = pat->groupNumberFromName(UnicodeString("nosuch"), status);
    U_ASSERT(status == U_REGEX_INVALID_CAPTURE_GROUP_NAME);

    status = U_ZERO_ERROR;

    // After copying a pattern, named capture should still work in the copy.
    RegexPattern *copiedPat = new RegexPattern(*pat);
    REGEX_ASSERT(*copiedPat == *pat);
    delete pat; pat = nullptr;  // Delete original, copy should have no references back to it.

    group = copiedPat->groupNumberFromName("five", -1, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(5 == group);
    group = copiedPat->groupNumberFromName("three", -1, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(3 == group);
    delete copiedPat;

    // ReplaceAll with named capture group.
    status = U_ZERO_ERROR;
    UnicodeString text("Substitution of <<quotes>> for <<double brackets>>");
    RegexMatcher *m = new RegexMatcher(UnicodeString("<<(?<mid>.+?)>>"), text, 0, status);
    REGEX_CHECK_STATUS;
    // m.pattern().dumpPattern();
    UnicodeString replacedText = m->replaceAll("'${mid}'", status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("Substitution of 'quotes' for 'double brackets'") == replacedText);
    delete m;

    // ReplaceAll, allowed capture group numbers.
    text = UnicodeString("abcmxyz");
    m = new RegexMatcher(UnicodeString("..(?<one>m)(.)(.)"), text, 0, status);
    REGEX_CHECK_STATUS;

    status = U_ZERO_ERROR;
    replacedText  = m->replaceAll(UnicodeString("<$0>"), status);   // group 0, full match, is allowed.
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("a<bcmxy>z") == replacedText);

    status = U_ZERO_ERROR;
    replacedText  = m->replaceAll(UnicodeString("<$1>"), status);      // group 1 by number.
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("a<m>z") == replacedText);

    status = U_ZERO_ERROR;
    replacedText  = m->replaceAll(UnicodeString("<${one}>"), status);   // group 1 by name.
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("a<m>z") == replacedText);

    status = U_ZERO_ERROR;
    replacedText  = m->replaceAll(UnicodeString("<$2>"), status);   // group 2.
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("a<x>z") == replacedText);

    status = U_ZERO_ERROR;
    replacedText  = m->replaceAll(UnicodeString("<$3>"), status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("a<y>z") == replacedText);

    status = U_ZERO_ERROR;
    replacedText  = m->replaceAll(UnicodeString("<$4>"), status);
    REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);

    status = U_ZERO_ERROR;
    replacedText  = m->replaceAll(UnicodeString("<$04>"), status);      // group 0, leading 0,
    REGEX_CHECK_STATUS;                                                 //    trailing out-of-range 4 passes through.
    REGEX_ASSERT(UnicodeString("a<bcmxy4>z") == replacedText);

    status = U_ZERO_ERROR;
    replacedText  = m->replaceAll(UnicodeString("<$000016>"), status);  // Consume leading zeroes. Don't consume digits
    REGEX_CHECK_STATUS;                                                 //   that push group num out of range.
    REGEX_ASSERT(UnicodeString("a<m6>z") == replacedText);              //   This is group 1.

    status = U_ZERO_ERROR;
    replacedText  = m->replaceAll(UnicodeString("<$3$2$1${one}>"), status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("a<yxmm>z") == replacedText);

    status = U_ZERO_ERROR;
    replacedText  = m->replaceAll(UnicodeString("$3$2$1${one}"), status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("ayxmmz") == replacedText);

    status = U_ZERO_ERROR;
    replacedText  = m->replaceAll(UnicodeString("<${noSuchName}>"), status);
    REGEX_ASSERT(status == U_REGEX_INVALID_CAPTURE_GROUP_NAME);

    status = U_ZERO_ERROR;
    replacedText  = m->replaceAll(UnicodeString("<${invalid-name}>"), status);
    REGEX_ASSERT(status == U_REGEX_INVALID_CAPTURE_GROUP_NAME);

    status = U_ZERO_ERROR;
    replacedText  = m->replaceAll(UnicodeString("<${one"), status);
    REGEX_ASSERT(status == U_REGEX_INVALID_CAPTURE_GROUP_NAME);

    status = U_ZERO_ERROR;
    replacedText  = m->replaceAll(UnicodeString("$not a capture group"), status);
    REGEX_ASSERT(status == U_REGEX_INVALID_CAPTURE_GROUP_NAME);

    delete m;

    // Repeat the above replaceAll() tests using the plain C API, which
    //  has a separate implementation internally.
    //  TODO: factor out the test data.

    status = U_ZERO_ERROR;
    URegularExpression *re = uregex_openC("..(?<one>m)(.)(.)", 0, nullptr, &status);
    REGEX_CHECK_STATUS;
    text = UnicodeString("abcmxyz");
    uregex_setText(re, text.getBuffer(), text.length(), &status);
    REGEX_CHECK_STATUS;

    char16_t resultBuf[100];
    int32_t resultLength;
    UnicodeString repl;

    status = U_ZERO_ERROR;
    repl = UnicodeString("<$0>");
    resultLength = uregex_replaceAll(re, repl.getBuffer(), repl.length(), resultBuf, UPRV_LENGTHOF(resultBuf), &status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("a<bcmxy>z") == UnicodeString(resultBuf, resultLength));

    status = U_ZERO_ERROR;
    repl = UnicodeString("<$1>");
    resultLength = uregex_replaceAll(re, repl.getBuffer(), repl.length(), resultBuf, UPRV_LENGTHOF(resultBuf), &status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("a<m>z") == UnicodeString(resultBuf, resultLength));

    status = U_ZERO_ERROR;
    repl = UnicodeString("<${one}>");
    resultLength = uregex_replaceAll(re, repl.getBuffer(), repl.length(), resultBuf, UPRV_LENGTHOF(resultBuf), &status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("a<m>z") == UnicodeString(resultBuf, resultLength));

    status = U_ZERO_ERROR;
    repl = UnicodeString("<$2>");
    resultLength = uregex_replaceAll(re, repl.getBuffer(), repl.length(), resultBuf, UPRV_LENGTHOF(resultBuf), &status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("a<x>z") == UnicodeString(resultBuf, resultLength));

    status = U_ZERO_ERROR;
    repl = UnicodeString("<$3>");
    resultLength = uregex_replaceAll(re, repl.getBuffer(), repl.length(), resultBuf, UPRV_LENGTHOF(resultBuf), &status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("a<y>z") == UnicodeString(resultBuf, resultLength));

    status = U_ZERO_ERROR;
    repl = UnicodeString("<$4>");
    resultLength = uregex_replaceAll(re, repl.getBuffer(), repl.length(), resultBuf, UPRV_LENGTHOF(resultBuf), &status);
    REGEX_ASSERT(status == U_INDEX_OUTOFBOUNDS_ERROR);

    status = U_ZERO_ERROR;
    repl = UnicodeString("<$04>");
    resultLength = uregex_replaceAll(re, repl.getBuffer(), repl.length(), resultBuf, UPRV_LENGTHOF(resultBuf), &status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("a<bcmxy4>z") == UnicodeString(resultBuf, resultLength));

    status = U_ZERO_ERROR;
    repl = UnicodeString("<$000016>");
    resultLength = uregex_replaceAll(re, repl.getBuffer(), repl.length(), resultBuf, UPRV_LENGTHOF(resultBuf), &status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("a<m6>z") == UnicodeString(resultBuf, resultLength));

    status = U_ZERO_ERROR;
    repl = UnicodeString("<$3$2$1${one}>");
    resultLength = uregex_replaceAll(re, repl.getBuffer(), repl.length(), resultBuf, UPRV_LENGTHOF(resultBuf), &status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("a<yxmm>z") == UnicodeString(resultBuf, resultLength));

    status = U_ZERO_ERROR;
    repl = UnicodeString("$3$2$1${one}");
    resultLength = uregex_replaceAll(re, repl.getBuffer(), repl.length(), resultBuf, UPRV_LENGTHOF(resultBuf), &status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(UnicodeString("ayxmmz") == UnicodeString(resultBuf, resultLength));

    status = U_ZERO_ERROR;
    repl = UnicodeString("<${noSuchName}>");
    resultLength = uregex_replaceAll(re, repl.getBuffer(), repl.length(), resultBuf, UPRV_LENGTHOF(resultBuf), &status);
    REGEX_ASSERT(status == U_REGEX_INVALID_CAPTURE_GROUP_NAME);

    status = U_ZERO_ERROR;
    repl = UnicodeString("<${invalid-name}>");
    resultLength = uregex_replaceAll(re, repl.getBuffer(), repl.length(), resultBuf, UPRV_LENGTHOF(resultBuf), &status);
    REGEX_ASSERT(status == U_REGEX_INVALID_CAPTURE_GROUP_NAME);

    status = U_ZERO_ERROR;
    repl = UnicodeString("<${one");
    resultLength = uregex_replaceAll(re, repl.getBuffer(), repl.length(), resultBuf, UPRV_LENGTHOF(resultBuf), &status);
    REGEX_ASSERT(status == U_REGEX_INVALID_CAPTURE_GROUP_NAME);

    status = U_ZERO_ERROR;
    repl = UnicodeString("$not a capture group");
    resultLength = uregex_replaceAll(re, repl.getBuffer(), repl.length(), resultBuf, UPRV_LENGTHOF(resultBuf), &status);
    REGEX_ASSERT(status == U_REGEX_INVALID_CAPTURE_GROUP_NAME);

    uregex_close(re);
}

//--------------------------------------------------------------
//
//  NamedCaptureLimits   Patterns with huge numbers of named capture groups.
//                       The point is not so much what the exact limit is,
//                       but that a largish number doesn't hit bad non-linear performance,
//                       and that exceeding the limit fails cleanly.
//
//--------------------------------------------------------------
void RegexTest::NamedCaptureLimits() {
    if (quick) {
        logln("Skipping test. Runs in exhuastive mode only.");
        return;
    }
    const int32_t goodLimit = 1000000;     // Pattern w this many groups builds successfully.
    const int32_t failLimit = 10000000;    // Pattern exceeds internal limits, fails to compile.
    char nnbuf[100];
    UnicodeString pattern;
    int32_t nn;

    for (nn=1; nn<goodLimit; nn++) {
        snprintf(nnbuf, sizeof(nnbuf), "(?<nn%d>)", nn);
        pattern.append(UnicodeString(nnbuf, -1, US_INV));
    }
    UErrorCode status = U_ZERO_ERROR;
    RegexPattern *pat = RegexPattern::compile(pattern, 0, status);
    REGEX_CHECK_STATUS;
    for (nn=1; nn<goodLimit; nn++) {
        snprintf(nnbuf, sizeof(nnbuf), "nn%d", nn);
        int32_t groupNum = pat->groupNumberFromName(nnbuf, -1, status);
        REGEX_ASSERT(nn == groupNum);
        if (nn != groupNum) {
            break;
        }
    }
    delete pat;

    pattern.remove();
    for (nn=1; nn<failLimit; nn++) {
        snprintf(nnbuf, sizeof(nnbuf), "(?<nn%d>)", nn);
        pattern.append(UnicodeString(nnbuf, -1, US_INV));
    }
    status = U_ZERO_ERROR;
    pat = RegexPattern::compile(pattern, 0, status);
    REGEX_ASSERT(status == U_REGEX_PATTERN_TOO_BIG);
    delete pat;
}


//--------------------------------------------------------------
//
//  Bug7651   Regex pattern that exceeds default operator stack depth in matcher.
//
//---------------------------------------------------------------
void RegexTest::Bug7651() {
    UnicodeString pattern1("((?<![A-Za-z0-9])[#\\uff03][A-Za-z0-9_][A-Za-z0-9_\\u00c0-\\u00d6\\u00c8-\\u00f6\\u00f8-\\u00ff]*|(?<![A-Za-z0-9_])[@\\uff20][A-Za-z0-9_]+(?:\\/[\\w-]+)?|(https?\\:\\/\\/|www\\.)\\S+(?<![\\!\\),\\.:;\\]\\u0080-\\uFFFF])|\\$[A-Za-z]+)");
    //  The following should exceed the default operator stack depth in the matcher, i.e. force the matcher to malloc instead of using fSmallData.
    //  It will cause a segfault if RegexMatcher tries to use fSmallData instead of malloc'ing the memory needed (see init2) for the pattern operator stack allocation.
    UnicodeString pattern2("((https?\\:\\/\\/|www\\.)\\S+(?<![\\!\\),\\.:;\\]\\u0080-\\uFFFF])|(?<![A-Za-z0-9_])[\\@\\uff20][A-Za-z0-9_]+(?:\\/[\\w\\-]+)?|(?<![A-Za-z0-9])[\\#\\uff03][A-Za-z0-9_][A-Za-z0-9_\\u00c0-\\u00d6\\u00c8-\\u00f6\\u00f8-\\u00ff]*|\\$[A-Za-z]+)");
    UnicodeString s("#ff @abcd This is test");
    RegexPattern  *REPattern = nullptr;
    RegexMatcher  *REMatcher = nullptr;
    UErrorCode status = U_ZERO_ERROR;
    UParseError pe;

    REPattern = RegexPattern::compile(pattern1, 0, pe, status);
    REGEX_CHECK_STATUS;
    REMatcher = REPattern->matcher(s, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(REMatcher->find());
    REGEX_ASSERT(REMatcher->start(status) == 0);
    delete REPattern;
    delete REMatcher;
    status = U_ZERO_ERROR;

    REPattern = RegexPattern::compile(pattern2, 0, pe, status);
    REGEX_CHECK_STATUS;
    REMatcher = REPattern->matcher(s, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(REMatcher->find());
    REGEX_ASSERT(REMatcher->start(status) == 0);
    delete REPattern;
    delete REMatcher;
    status = U_ZERO_ERROR;
 }

void RegexTest::Bug7740() {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString pattern = "(a)";
    UnicodeString text = "abcdef";
    RegexMatcher *m = new RegexMatcher(pattern, text, 0, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(m->lookingAt(status));
    REGEX_CHECK_STATUS;
    status = U_ILLEGAL_ARGUMENT_ERROR;
    UnicodeString s = m->group(1, status);    // Bug 7740: segfault here.
    REGEX_ASSERT(status == U_ILLEGAL_ARGUMENT_ERROR);
    REGEX_ASSERT(s == "");
    delete m;
}

// Bug 8479:  was crashing whith a Bogus UnicodeString as input.

void RegexTest::Bug8479() {
    UErrorCode status = U_ZERO_ERROR;

    RegexMatcher* const pMatcher = new RegexMatcher("\\Aboo\\z", UREGEX_DOTALL|UREGEX_CASE_INSENSITIVE, status);
    REGEX_CHECK_STATUS;
    if (U_SUCCESS(status))
    {
        UnicodeString str;
        str.setToBogus();
        pMatcher->reset(str);
        status = U_ZERO_ERROR;
        pMatcher->matches(status);
        REGEX_ASSERT(status == U_ILLEGAL_ARGUMENT_ERROR);
        delete pMatcher;
    }
}


// Bug 7029
void RegexTest::Bug7029() {
    UErrorCode status = U_ZERO_ERROR;

    RegexMatcher* const pMatcher = new RegexMatcher(".", 0, status);
    UnicodeString text = "abc.def";
    UnicodeString splits[10];
    REGEX_CHECK_STATUS;
    int32_t numFields = pMatcher->split(text, splits, 10, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(numFields == 8);
    delete pMatcher;
}

// Bug 9283
//   This test is checking for the existence of any supplemental characters that case-fold
//   to a bmp character.
//
//   At the time of this writing there are none. If any should appear in a subsequent release
//   of Unicode, the code in regular expressions compilation that determines the longest
//   possible match for a literal string  will need to be enhanced.
//
//   See file regexcmp.cpp, case URX_STRING_I in RegexCompile::maxMatchLength()
//   for details on what to do in case of a failure of this test.
//
void RegexTest::Bug9283() {
#if !UCONFIG_NO_NORMALIZATION
    UErrorCode status = U_ZERO_ERROR;
    UnicodeSet supplementalsWithCaseFolding("[[:CWCF:]&[\\U00010000-\\U0010FFFF]]", status);
    REGEX_CHECK_STATUS;
    int32_t index;
    UChar32 c;
    for (index=0; ; index++) {
        c = supplementalsWithCaseFolding.charAt(index);
        if (c == -1) {
            break;
        }
        UnicodeString cf = UnicodeString(c).foldCase();
        REGEX_ASSERT(cf.length() >= 2);
    }
#endif /* #if !UCONFIG_NO_NORMALIZATION */
}


void RegexTest::CheckInvBufSize() {
  if(inv_next>=INV_BUFSIZ) {
    errln("%s: increase #define of INV_BUFSIZ ( is %d but needs to be at least %d )\n",
          __FILE__, INV_BUFSIZ, inv_next);
  } else {
    logln("%s: INV_BUFSIZ is %d, usage %d\n", __FILE__, INV_BUFSIZ, inv_next);
  }
}


void RegexTest::Bug10459() {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString patternString("(txt)");
    UnicodeString txtString("txt");

    UText *utext_pat = utext_openUnicodeString(nullptr, &patternString, &status);
    REGEX_CHECK_STATUS;
    UText *utext_txt = utext_openUnicodeString(nullptr, &txtString, &status);
    REGEX_CHECK_STATUS;

    URegularExpression *icu_re = uregex_openUText(utext_pat, 0, nullptr, &status);
    REGEX_CHECK_STATUS;

    uregex_setUText(icu_re, utext_txt, &status);
    REGEX_CHECK_STATUS;

    // The bug was that calling uregex_group() before doing a matching operation
    //   was causing a segfault. Only for Regular Expressions created from UText.
    //   It should set an U_REGEX_INVALID_STATE.

    char16_t buf[100];
    int32_t len = uregex_group(icu_re, 0, buf, UPRV_LENGTHOF(buf), &status);
    REGEX_ASSERT(status == U_REGEX_INVALID_STATE);
    REGEX_ASSERT(len == 0);

    uregex_close(icu_re);
    utext_close(utext_pat);
    utext_close(utext_txt);
}

void RegexTest::TestCaseInsensitiveStarters() {
    // Test that the data used by RegexCompile::findCaseInsensitiveStarters() hasn't
    //  become stale because of new Unicode characters.
    // If it is stale, rerun the generation tool
    //    https://github.com/unicode-org/icu/tree/main/tools/unicode/c/genregexcasing
    // and replace the embedded data in i18n/regexcmp.cpp

    for (UChar32 cp=0; cp<=0x10ffff; cp++) {
        if (!u_hasBinaryProperty(cp, UCHAR_CASE_SENSITIVE)) {
            continue;
        }
        UnicodeSet s(cp, cp);
        s.closeOver(USET_CASE_INSENSITIVE);
        UnicodeSetIterator setIter(s);
        while (setIter.next()) {
            if (!setIter.isString()) {
                continue;
            }
            const UnicodeString &str = setIter.getString();
            UChar32 firstChar = str.char32At(0);
            UnicodeSet starters;
            RegexCompile::findCaseInsensitiveStarters(firstChar, &starters);
            if (!starters.contains(cp)) {
                errln("CaseInsensitiveStarters for \\u%x is missing character \\u%x.", cp, firstChar);
                return;
            }
        }
    }
}


void RegexTest::TestBug11049() {
    // Original bug report: pattern with match start consisting of one of several individual characters,
    //  and the text being matched ending with a supplementary character. find() would read past the
    //  end of the input text when searching for potential match starting points.

    // To see the problem, the text must exactly fill an allocated buffer, so that valgrind will
    // detect the bad read.

    TestCase11049("A|B|C", "a string \\ud800\\udc00", false, __LINE__);
    TestCase11049("A|B|C", "string matches at end C", true, __LINE__);

    // Test again with a pattern starting with a single character,
    // which takes a different code path than starting with an OR expression,
    // but with similar logic.
    TestCase11049("C", "a string \\ud800\\udc00", false, __LINE__);
    TestCase11049("C", "string matches at end C", true, __LINE__);
}

// Run a single test case from TestBug11049(). Internal function.
void RegexTest::TestCase11049(const char *pattern, const char *data, UBool expectMatch, int32_t lineNumber) {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString patternString = UnicodeString(pattern).unescape();
    LocalPointer<RegexPattern> compiledPat(RegexPattern::compile(patternString, 0, status));

    UnicodeString dataString = UnicodeString(data).unescape();
    char16_t *exactBuffer = new char16_t[dataString.length()];
    dataString.extract(exactBuffer, dataString.length(), status);
    UText *ut = utext_openUChars(nullptr, exactBuffer, dataString.length(), &status);

    LocalPointer<RegexMatcher> matcher(compiledPat->matcher(status));
    REGEX_CHECK_STATUS;
    matcher->reset(ut);
    UBool result = matcher->find();
    if (result != expectMatch) {
        errln("File %s, line %d: expected %d, got %d. Pattern = \"%s\", text = \"%s\"",
              __FILE__, lineNumber, expectMatch, result, pattern, data);
    }

    // Rerun test with UTF-8 input text. Won't see buffer overreads, but could see
    //   off-by-one on find() with match at the last code point.
    //   Size of the original char * data (invariant charset) will be <= than the equivalent UTF-8
    //   because string.unescape() will only shrink it.
    char * utf8Buffer = new char[uprv_strlen(data)+1];
    u_strToUTF8(utf8Buffer, static_cast<int32_t>(uprv_strlen(data)+1), nullptr, dataString.getBuffer(), dataString.length(), &status);
    REGEX_CHECK_STATUS;
    ut = utext_openUTF8(ut, utf8Buffer, -1, &status);
    REGEX_CHECK_STATUS;
    matcher->reset(ut);
    result = matcher->find();
    if (result != expectMatch) {
        errln("File %s, line %d (UTF-8 check): expected %d, got %d. Pattern = \"%s\", text = \"%s\"",
              __FILE__, lineNumber, expectMatch, result, pattern, data);
    }
    delete [] utf8Buffer;

    utext_close(ut);
    delete [] exactBuffer;
}


void RegexTest::TestBug11371() {
    if (quick) {
        logln("Skipping test. Runs in exhuastive mode only.");
        return;
    }
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString patternString;

    for (int i=0; i<8000000; i++) {
        patternString.append(UnicodeString("()"));
    }
    LocalPointer<RegexPattern> compiledPat(RegexPattern::compile(patternString, 0, status));
    if (status != U_REGEX_PATTERN_TOO_BIG) {
        errln("File %s, line %d expected status=U_REGEX_PATTERN_TOO_BIG; got %s.",
              __FILE__, __LINE__, u_errorName(status));
    }

    status = U_ZERO_ERROR;
    patternString = "(";
    for (int i=0; i<20000000; i++) {
        patternString.append(UnicodeString("A++"));
    }
    patternString.append(UnicodeString("){0}B++"));
    LocalPointer<RegexPattern> compiledPat2(RegexPattern::compile(patternString, 0, status));
    if (status != U_REGEX_PATTERN_TOO_BIG) {
        errln("File %s, line %d expected status=U_REGEX_PATTERN_TOO_BIG; got %s.",
              __FILE__, __LINE__, u_errorName(status));
    }

    // Pattern with too much string data, such that string indexes overflow operand data field size
    // in compiled instruction.
    status = U_ZERO_ERROR;
    patternString = "";
    while (patternString.length() < 0x00ffffff) {
        patternString.append(UnicodeString("stuff and things dont you know, these are a few of my favorite strings\n"));
    }
    patternString.append(UnicodeString("X? trailing string"));
    LocalPointer<RegexPattern> compiledPat3(RegexPattern::compile(patternString, 0, status));
    if (status != U_REGEX_PATTERN_TOO_BIG) {
        errln("File %s, line %d expected status=U_REGEX_PATTERN_TOO_BIG; got %s.",
              __FILE__, __LINE__, u_errorName(status));
    }
}

void RegexTest::TestBug11480() {
    // C API, get capture group of a group that does not participate in the match.
    //        (Returns a zero length string, with nul termination,
    //         indistinguishable from a group with a zero length match.)

    UErrorCode status = U_ZERO_ERROR;
    URegularExpression *re = uregex_openC("(A)|(B)", 0, nullptr, &status);
    REGEX_CHECK_STATUS;
    UnicodeString text = UNICODE_STRING_SIMPLE("A");
    uregex_setText(re, text.getBuffer(), text.length(), &status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(uregex_lookingAt(re, 0, &status));
    char16_t buf[10] = {(char16_t)13, (char16_t)13, (char16_t)13, (char16_t)13};
    int32_t length = uregex_group(re, 2, buf+1, UPRV_LENGTHOF(buf)-1, &status);
    REGEX_ASSERT(length == 0);
    REGEX_ASSERT(buf[0] == 13);
    REGEX_ASSERT(buf[1] == 0);
    REGEX_ASSERT(buf[2] == 13);
    uregex_close(re);

    // UText C++ API, length of match is 0 for non-participating matches.
    UText ut = UTEXT_INITIALIZER;
    utext_openUnicodeString(&ut, &text, &status);
    RegexMatcher matcher(UnicodeString("(A)|(B)"), 0, status);
    REGEX_CHECK_STATUS;
    matcher.reset(&ut);
    REGEX_ASSERT(matcher.lookingAt(0, status));

    // UText C++ API, Capture group 1 matches "A", position 0, length 1.
    int64_t groupLen = -666;
    UText group = UTEXT_INITIALIZER;
    matcher.group(1, &group, groupLen, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(groupLen == 1);
    REGEX_ASSERT(utext_getNativeIndex(&group) == 0);

    // Capture group 2, the (B), does not participate in the match.
    matcher.group(2, &group, groupLen, status);
    REGEX_CHECK_STATUS;
    REGEX_ASSERT(groupLen == 0);
    REGEX_ASSERT(matcher.start(2, status) == -1);
    REGEX_CHECK_STATUS;
}

void RegexTest::TestBug12884() {
    // setTimeLimit() was not effective for empty sub-patterns with large {minimum counts}
    UnicodeString pattern(u"(((((((){120}){11}){11}){11}){80}){11}){4}");
    UnicodeString text(u"hello");
    UErrorCode status = U_ZERO_ERROR;
    RegexMatcher m(pattern, text, 0, status);
    REGEX_CHECK_STATUS;
    m.setTimeLimit(5, status);
    m.find(status);
    REGEX_ASSERT(status == U_REGEX_TIME_OUT);

    // Non-greedy loops. They take a different code path during matching.
    UnicodeString ngPattern(u"(((((((){120}?){11}?){11}?){11}?){80}?){11}?){4}?");
    status = U_ZERO_ERROR;
    RegexMatcher ngM(ngPattern, text, 0, status);
    REGEX_CHECK_STATUS;
    ngM.setTimeLimit(5, status);
    ngM.find(status);
    REGEX_ASSERT(status == U_REGEX_TIME_OUT);

    // UText, wrapping non-UTF-16 text, also takes a different execution path.
    StringPiece text8(u8"¿Qué es Unicode?  Unicode proporciona un número único para cada"
                          "carácter, sin importar la plataforma, sin importar el programa,"
                          "sin importar el idioma.");
    status = U_ZERO_ERROR;
    LocalUTextPointer ut(utext_openUTF8(nullptr, text8.data(), text8.length(), &status));
    REGEX_CHECK_STATUS;
    m.reset(ut.getAlias());
    m.find(status);
    REGEX_ASSERT(status == U_REGEX_TIME_OUT);

    status = U_ZERO_ERROR;
    ngM.reset(ut.getAlias());
    ngM.find(status);
    REGEX_ASSERT(status == U_REGEX_TIME_OUT);
}

// Bug 13631. A find() of a pattern with a zero length look-behind assertions
//            can cause a read past the end of the input text.
//            The failure is seen when running this test with Clang's Address Sanitizer.

void RegexTest::TestBug13631() {
    const char16_t *pats[] = { u"(?<!^)",
                            u"(?<=^)",
                            nullptr
                          };
    for (const char16_t **pat=pats; *pat; ++pat) {
        UErrorCode status = U_ZERO_ERROR;
        UnicodeString upat(*pat);
        RegexMatcher matcher(upat, 0, status);
        const char16_t s =u'a';
        UText *ut = utext_openUChars(nullptr, &s, 1, &status);
        REGEX_CHECK_STATUS;
        matcher.reset(ut);
        while (matcher.find()) {
        }
        utext_close(ut);
    }
}

// Bug 13632 Out of bounds memory reference if a replacement string ends with a '$',
//           where a following group specification would be expected.
//           Failure shows when running the test under Clang's Address Sanitizer.

void RegexTest::TestBug13632() {
    UErrorCode status = U_ZERO_ERROR;
    URegularExpression *re = uregex_openC(" ", 0, nullptr, &status);
    const char16_t *sourceString = u"Hello, world.";
    uregex_setText(re, sourceString, u_strlen(sourceString), &status);

    const int32_t destCap = 20;
    char16_t dest[destCap] = {};
    const char16_t replacement[] = {u'x', u'$'};    // Not nul terminated string.
    uregex_replaceAll(re, replacement, 2, dest, destCap, &status);

    assertEquals("", U_REGEX_INVALID_CAPTURE_GROUP_NAME, status);
    uregex_close(re);
}

void RegexTest::TestBug20359() {
    // The bug was stack overflow while parsing a pattern with a huge number of adjacent \Q\E
    // pairs. (Enter and exit pattern literal quote mode). Logic was correct.
    // Changed implementation to loop instead of recursing.

    UnicodeString pattern;
    for (int i=0; i<50000; ++i) {
        pattern += u"\\Q\\E";
    }
    pattern += u"x";

    UErrorCode status = U_ZERO_ERROR;
    LocalURegularExpressionPointer re(uregex_open(pattern.getBuffer(), pattern.length(),
                                       0, nullptr, &status));
    assertSuccess(WHERE, status);

    // We have passed the point where the bug crashed. The following is a small sanity
    // check that the pattern works, that all the \Q\E\Q\E... didn't cause other problems.

    uregex_setText(re.getAlias(), u"abcxyz", -1, &status);
    assertSuccess(WHERE, status);
    assertTrue(WHERE, uregex_find(re.getAlias(), 0, &status));
    assertEquals(WHERE, 3, uregex_start(re.getAlias(), 0, &status));
    assertSuccess(WHERE, status);
}


void RegexTest::TestBug20863() {
    // Test that patterns with a large number of named capture groups work correctly.
    //
    // The ticket was not for a bug per se, but to reduce memory usage by using lazy
    // construction of the map from capture names to numbers, and decreasing the
    // default size of the map.

    constexpr int GROUP_COUNT = 2000;
    std::vector<UnicodeString> groupNames;
    for (int32_t i=0; i<GROUP_COUNT; ++i) {
        UnicodeString name;
        name.append(u"name");
        name.append(Int64ToUnicodeString(i));
        groupNames.push_back(name);
    }

    UnicodeString patternString;
    for (UnicodeString name: groupNames) {
        patternString.append(u"(?<");
        patternString.append(name);
        patternString.append(u">.)");
    }

    UErrorCode status = U_ZERO_ERROR;
    UParseError pe;
    LocalPointer<RegexPattern> pattern(RegexPattern::compile(patternString, pe, status), status);
    if (!assertSuccess(WHERE, status)) {
        return;
    }

    for (int32_t i=0; i<GROUP_COUNT; ++i) {
        int32_t group = pattern->groupNumberFromName(groupNames[i], status);
        if (!assertSuccess(WHERE, status)) {
            return;
        }
        assertEquals(WHERE, i+1, group);
        // Note: group 0 is the overall match; group 1 is the first separate capture group.
    }

    // Verify that assignment of patterns with various combinations of named capture work.
    // Lazy creation of the internal named capture map changed the implementation logic here.
    {
        LocalPointer<RegexPattern> pat1(RegexPattern::compile(u"abc", pe, status), status);
        LocalPointer<RegexPattern> pat2(RegexPattern::compile(u"a(?<name>b)c", pe, status), status);
        assertSuccess(WHERE, status);
        assertFalse(WHERE, *pat1 == *pat2);
        *pat1 = *pat2;
        assertTrue(WHERE, *pat1 == *pat2);
        assertEquals(WHERE, 1, pat1->groupNumberFromName(u"name", status));
        assertEquals(WHERE, 1, pat2->groupNumberFromName(u"name", status));
        assertSuccess(WHERE, status);
    }

    {
        LocalPointer<RegexPattern> pat1(RegexPattern::compile(u"abc", pe, status), status);
        LocalPointer<RegexPattern> pat2(RegexPattern::compile(u"a(?<name>b)c", pe, status), status);
        assertSuccess(WHERE, status);
        assertFalse(WHERE, *pat1 == *pat2);
        *pat2 = *pat1;
        assertTrue(WHERE, *pat1 == *pat2);
        assertEquals(WHERE, 0, pat1->groupNumberFromName(u"name", status));
        assertEquals(WHERE, U_REGEX_INVALID_CAPTURE_GROUP_NAME, status);
        status = U_ZERO_ERROR;
        assertEquals(WHERE, 0, pat2->groupNumberFromName(u"name", status));
        assertEquals(WHERE, U_REGEX_INVALID_CAPTURE_GROUP_NAME, status);
        status = U_ZERO_ERROR;
    }

    {
        LocalPointer<RegexPattern> pat1(RegexPattern::compile(u"a(?<name1>b)c", pe, status), status);
        LocalPointer<RegexPattern> pat2(RegexPattern::compile(u"a(?<name2>b)c", pe, status), status);
        assertSuccess(WHERE, status);
        assertFalse(WHERE, *pat1 == *pat2);
        *pat2 = *pat1;
        assertTrue(WHERE, *pat1 == *pat2);
        assertEquals(WHERE, 1, pat1->groupNumberFromName(u"name1", status));
        assertSuccess(WHERE, status);
        assertEquals(WHERE, 1, pat2->groupNumberFromName(u"name1", status));
        assertSuccess(WHERE, status);
        assertEquals(WHERE, 0, pat1->groupNumberFromName(u"name2", status));
        assertEquals(WHERE, U_REGEX_INVALID_CAPTURE_GROUP_NAME, status);
        status = U_ZERO_ERROR;
        assertEquals(WHERE, 0, pat2->groupNumberFromName(u"name2", status));
        assertEquals(WHERE, U_REGEX_INVALID_CAPTURE_GROUP_NAME, status);
        status = U_ZERO_ERROR;
    }

}


#endif  /* !UCONFIG_NO_REGULAR_EXPRESSIONS  */
