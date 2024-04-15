// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
 *******************************************************************************
 *   Copyright (C) 1997-2016 International Business Machines
 *   Corporation and others.  All Rights Reserved.
 *******************************************************************************
 *   Date        Name        Description
 *   06/23/00    aliu        Creation.
 *******************************************************************************
 */

#include "unicode/utypes.h"

#if !UCONFIG_NO_TRANSLITERATION

#include <stdlib.h>
#include <string.h>
#include "unicode/utrans.h"
#include "unicode/ustring.h"
#include "unicode/uset.h"
#include "cintltst.h"
#include "cmemory.h"

#define TEST(x) addTest(root, &x, "utrans/" # x)

static void TestAPI(void);
static void TestSimpleRules(void);
static void TestFilter(void);
static void TestOpenInverse(void);
static void TestClone(void);
static void TestRegisterUnregister(void);
static void TestExtractBetween(void);
static void TestUnicodeIDs(void);
static void TestGetRulesAndSourceSet(void);
static void TestDataVariantsCompounds(void);

static void _expectRules(const char*, const char*, const char*);
static void _expect(const UTransliterator* trans, const char* cfrom, const char* cto);

void addUTransTest(TestNode** root);


void
addUTransTest(TestNode** root) {
    TEST(TestAPI);
    TEST(TestSimpleRules);
    TEST(TestFilter);
    TEST(TestOpenInverse);
    TEST(TestClone);
    TEST(TestRegisterUnregister);
    TEST(TestExtractBetween);
    TEST(TestUnicodeIDs);
    TEST(TestGetRulesAndSourceSet);
    TEST(TestDataVariantsCompounds);
}

/*------------------------------------------------------------------
 * Replaceable glue
 *
 * To test the Replaceable glue we have to dummy up a C-based
 * Replaceable callback.  This code is for testing purposes only.
 *------------------------------------------------------------------*/

typedef struct XReplaceable {
    UChar* text;    /* MUST BE null-terminated */
} XReplaceable;

static void InitXReplaceable(XReplaceable* rep, const char* cstring) {
    rep->text = malloc(sizeof(UChar) * (strlen(cstring)+1));
    u_uastrcpy(rep->text, cstring);
}

static void FreeXReplaceable(XReplaceable* rep) {
    if (rep->text != NULL) {
        free(rep->text);
        rep->text = NULL;
    }
}

/* UReplaceableCallbacks callback */
static int32_t Xlength(const UReplaceable* rep) {
    const XReplaceable* x = (const XReplaceable*)rep;
    return u_strlen(x->text);
}

/* UReplaceableCallbacks callback */
static UChar XcharAt(const UReplaceable* rep, int32_t offset) {
    const XReplaceable* x = (const XReplaceable*)rep;
    return x->text[offset];
}

/* UReplaceableCallbacks callback */
static UChar32 Xchar32At(const UReplaceable* rep, int32_t offset) {
    const XReplaceable* x = (const XReplaceable*)rep;
    return x->text[offset];
}

/* UReplaceableCallbacks callback */
static void Xreplace(UReplaceable* rep, int32_t start, int32_t limit,
              const UChar* text, int32_t textLength) {
    XReplaceable* x = (XReplaceable*)rep;
    int32_t newLen = Xlength(rep) + limit - start + textLength;
    UChar* newText = (UChar*) malloc(sizeof(UChar) * (newLen+1));
    u_strncpy(newText, x->text, start);
    u_strncpy(newText + start, text, textLength);
    u_strcpy(newText + start + textLength, x->text + limit);
    free(x->text);
    x->text = newText;
}

/* UReplaceableCallbacks callback */
static void Xcopy(UReplaceable* rep, int32_t start, int32_t limit, int32_t dest) {
    XReplaceable* x = (XReplaceable*)rep;
    int32_t newLen = Xlength(rep) + limit - start;
    UChar* newText = (UChar*) malloc(sizeof(UChar) * (newLen+1));
    u_strncpy(newText, x->text, dest);
    u_strncpy(newText + dest, x->text + start, limit - start);
    u_strcpy(newText + dest + limit - start, x->text + dest);
    free(x->text);
    x->text = newText;
}

/* UReplaceableCallbacks callback */
static void Xextract(UReplaceable* rep, int32_t start, int32_t limit, UChar* dst) {
    XReplaceable* x = (XReplaceable*)rep;
    int32_t len = limit - start;
    u_strncpy(dst, x->text, len);
}

static void InitXReplaceableCallbacks(UReplaceableCallbacks* callbacks) {
    callbacks->length = Xlength;
    callbacks->charAt = XcharAt;
    callbacks->char32At = Xchar32At;
    callbacks->replace = Xreplace;
    callbacks->extract = Xextract;
    callbacks->copy = Xcopy;
}

/*------------------------------------------------------------------
 * Tests
 *------------------------------------------------------------------*/

static void TestAPI() {
    enum { BUF_CAP = 128 };
    char buf[BUF_CAP], buf2[BUF_CAP];
    UErrorCode status = U_ZERO_ERROR;
    UTransliterator* trans = NULL;
    int32_t i, n;
    
    /* Test getAvailableIDs */
    n = utrans_countAvailableIDs();
    if (n < 1) {
        log_err("FAIL: utrans_countAvailableIDs() returned %d\n", n);
    } else {
        log_verbose("System ID count: %d\n", n);
    }
    for (i=0; i<n; ++i) {
        utrans_getAvailableID(i, buf, BUF_CAP);
        if (*buf == 0) {
            log_err("FAIL: System transliterator %d: \"\"\n", i);
        } else {
            log_verbose("System transliterator %d: \"%s\"\n", i, buf);
        }
    }

    /* Test open */
    utrans_getAvailableID(0, buf, BUF_CAP);
    trans = utrans_open(buf, UTRANS_FORWARD,NULL,0,NULL, &status);
    if (U_FAILURE(status)) {
        log_err("FAIL: utrans_open(%s) failed, error=%s\n",
                buf, u_errorName(status));
    }

    else {
        /* Test getID */
        utrans_getID(trans, buf2, BUF_CAP);
        if (0 != strcmp(buf, buf2)) {
            log_err("FAIL: utrans_getID(%s) returned %s\n",
                    buf, buf2);
        }
        utrans_close(trans);
    }
}

static void TestUnicodeIDs() {
    UEnumeration *uenum;
    UTransliterator *utrans;
    const UChar *id, *id2;
    int32_t idLength, id2Length, count, count2;

    UErrorCode errorCode;

    errorCode=U_ZERO_ERROR;
    uenum=utrans_openIDs(&errorCode);
    if(U_FAILURE(errorCode)) {
        log_err("utrans_openIDs() failed - %s\n", u_errorName(errorCode));
        return;
    }

    count=uenum_count(uenum, &errorCode);
    if(U_FAILURE(errorCode) || count<1) {
        log_err("uenum_count(transliterator IDs)=%d - %s\n", count, u_errorName(errorCode));
    }

    count=0;
    for(;;) {
        id=uenum_unext(uenum, &idLength, &errorCode);
        if(U_FAILURE(errorCode)) {
            log_err("uenum_unext(transliterator ID %d) failed - %s\n", count, u_errorName(errorCode));
            break;
        }
        if(id==NULL) {
            break;
        }

        if(++count>10) {
            /* try to actually open only a few transliterators */
            continue;
        }

        utrans=utrans_openU(id, idLength, UTRANS_FORWARD, NULL, 0, NULL, &errorCode);
        if(U_FAILURE(errorCode)) {
            log_err("utrans_openU(%s) failed - %s\n", aescstrdup(id, idLength), u_errorName(errorCode));
            continue;
        }

        id2=utrans_getUnicodeID(utrans, &id2Length);
        if(idLength!=id2Length || 0!=u_memcmp(id, id2, idLength)) {
            log_err("utrans_getUnicodeID(%s) does not match the original ID\n", aescstrdup(id, idLength));
        }

        utrans_close(utrans);
    }

    uenum_reset(uenum, &errorCode);
    if(U_FAILURE(errorCode) || count<1) {
        log_err("uenum_reset(transliterator IDs) failed - %s\n", u_errorName(errorCode));
    } else {
        count2=uenum_count(uenum, &errorCode);
        if(U_FAILURE(errorCode) || count<1) {
            log_err("2nd uenum_count(transliterator IDs)=%d - %s\n", count2, u_errorName(errorCode));
        } else if(count!=count2) {
            log_err("uenum_unext(transliterator IDs) returned %d IDs but uenum_count() after uenum_reset() claims there are %d\n", count, count2);
        }
    }

    uenum_close(uenum);
}

static void TestOpenInverse(){
    UErrorCode status=U_ZERO_ERROR;
    UTransliterator* t1=NULL;
    UTransliterator* inverse1=NULL;
    enum { BUF_CAP = 128 };
    char buf1[BUF_CAP];
    int32_t i=0;

    const char TransID[][25]={
           "Halfwidth-Fullwidth",
           "Fullwidth-Halfwidth",
           "Greek-Latin" ,
           "Latin-Greek", 
           /*"Arabic-Latin", // Removed in 2.0*/
           /*"Latin-Arabic", // Removed in 2.0*/
           "Katakana-Latin",
           "Latin-Katakana",
           /*"Hebrew-Latin", // Removed in 2.0*/
           /*"Latin-Hebrew", // Removed in 2.0*/
           "Cyrillic-Latin", 
           "Latin-Cyrillic", 
           "Devanagari-Latin", 
           "Latin-Devanagari", 
           "Any-Hex",
           "Hex-Any"
         };
     
    for(i=0; i<UPRV_LENGTHOF(TransID); i=i+2){
        status = U_ZERO_ERROR;
        t1=utrans_open(TransID[i], UTRANS_FORWARD,NULL,0,NULL, &status);
        if(t1 == NULL || U_FAILURE(status)){
            log_data_err("FAIL: in instantiation for id=%s -> %s (Are you missing data?)\n", TransID[i], u_errorName(status));
            continue;
        }
        inverse1=utrans_openInverse(t1, &status);
        if(U_FAILURE(status)){
            log_err("FAIL: utrans_openInverse() failed for id=%s. Error=%s\n", TransID[i], myErrorName(status));
            continue;
        }
        utrans_getID(inverse1, buf1, BUF_CAP);
        if(strcmp(buf1, TransID[i+1]) != 0){
            log_err("FAIL :openInverse() for %s returned %s instead of %s\n", TransID[i], buf1, TransID[i+1]);
        }
        utrans_close(t1);
        utrans_close(inverse1);
   }
}

static void TestClone(){
    UErrorCode status=U_ZERO_ERROR;
    UTransliterator* t1=NULL;
    UTransliterator* t2=NULL;
    UTransliterator* t3=NULL;
    UTransliterator* t4=NULL;
    enum { BUF_CAP = 128 };
    char buf1[BUF_CAP], buf2[BUF_CAP], buf3[BUF_CAP];
   
    t1=utrans_open("Latin-Devanagari", UTRANS_FORWARD, NULL,0,NULL,&status);
    if(U_FAILURE(status)){
        log_data_err("FAIL: construction -> %s (Are you missing data?)\n", u_errorName(status));
        return;
    }
    t2=utrans_open("Latin-Greek", UTRANS_FORWARD, NULL,0,NULL,&status);
    if(U_FAILURE(status)){
        log_err("FAIL: construction\n");
        utrans_close(t1);
        return;
    }

    t3=utrans_clone(t1, &status);
    t4=utrans_clone(t2, &status);

    utrans_getID(t1, buf1, BUF_CAP);
    utrans_getID(t2, buf2, BUF_CAP);
    utrans_getID(t3, buf3, BUF_CAP);

    if(strcmp(buf1, buf3) != 0 ||
        strcmp(buf1, buf2) == 0) {
        log_err("FAIL: utrans_clone() failed\n");
    }

    utrans_getID(t4, buf3, BUF_CAP);

    if(strcmp(buf2, buf3) != 0 ||
        strcmp(buf1, buf3) == 0) {
        log_err("FAIL: utrans_clone() failed\n");
    }

    utrans_close(t1);
    utrans_close(t2);
    utrans_close(t3);
    utrans_close(t4);

}

static void TestRegisterUnregister(){
    UErrorCode status=U_ZERO_ERROR;
    UTransliterator* t1=NULL;
    UTransliterator* rules=NULL, *rules2;
    UTransliterator* inverse1=NULL;
    UChar rule[]={ 0x0061, 0x003c, 0x003e, 0x0063}; /*a<>b*/

    U_STRING_DECL(ID, "TestA-TestB", 11);
    U_STRING_INIT(ID, "TestA-TestB", 11);

    /* Make sure it doesn't exist */
    t1=utrans_open("TestA-TestB", UTRANS_FORWARD,NULL,0,NULL, &status);
    if(t1 != NULL || U_SUCCESS(status)) {
        log_err("FAIL: TestA-TestB already registered\n");
        return;
    }
    status=U_ZERO_ERROR;
    /* Check inverse too */
    inverse1=utrans_open("TestA-TestB", UTRANS_REVERSE, NULL,0,NULL,&status);
    if(inverse1 != NULL || U_SUCCESS(status)) {
        log_err("FAIL: TestA-TestB already registered\n");
        return;
    }
    status=U_ZERO_ERROR;
    /* Create it */
    rules=utrans_open("TestA-TestB",UTRANS_FORWARD, rule, 4, NULL, &status);
    if(U_FAILURE(status)){
        log_err("FAIL: utrans_openRules(a<>B) failed with error=%s\n", myErrorName(status));
        return;
    }

    /* clone it so we can register it a second time */
    rules2=utrans_clone(rules, &status);
    if(U_FAILURE(status)) {
        log_err("FAIL: utrans_clone(a<>B) failed with error=%s\n", myErrorName(status));
        return;
    }

    status=U_ZERO_ERROR;
    /* Register it */
    utrans_register(rules, &status);
    if(U_FAILURE(status)){
        log_err("FAIL: utrans_register failed with error=%s\n", myErrorName(status));
        return;
    }
    status=U_ZERO_ERROR;
    /* Now check again -- should exist now*/
    t1= utrans_open("TestA-TestB", UTRANS_FORWARD, NULL,0,NULL,&status);
    if(U_FAILURE(status) || t1 == NULL){
        log_err("FAIL: TestA-TestB not registered\n");
        return;
    }
    utrans_close(t1);
   
    /*unregister the instance*/
    status=U_ZERO_ERROR;
    utrans_unregister("TestA-TestB");
    /* now Make sure it doesn't exist */
    t1=utrans_open("TestA-TestB", UTRANS_FORWARD,NULL,0,NULL, &status);
    if(U_SUCCESS(status) || t1 != NULL) {
        log_err("FAIL: TestA-TestB isn't unregistered\n");
        return;
    }
    utrans_close(t1);

    /* now with utrans_unregisterID(const UChar *) */
    status=U_ZERO_ERROR;
    utrans_register(rules2, &status);
    if(U_FAILURE(status)){
        log_err("FAIL: 2nd utrans_register failed with error=%s\n", myErrorName(status));
        return;
    }
    status=U_ZERO_ERROR;
    /* Now check again -- should exist now*/
    t1= utrans_open("TestA-TestB", UTRANS_FORWARD, NULL,0,NULL,&status);
    if(U_FAILURE(status) || t1 == NULL){
        log_err("FAIL: 2nd TestA-TestB not registered\n");
        return;
    }
    utrans_close(t1);
   
    /*unregister the instance*/
    status=U_ZERO_ERROR;
    utrans_unregisterID(ID, -1);
    /* now Make sure it doesn't exist */
    t1=utrans_openU(ID, -1, UTRANS_FORWARD,NULL,0,NULL, &status);
    if(U_SUCCESS(status) || t1 != NULL) {
        log_err("FAIL: 2nd TestA-TestB isn't unregistered\n");
        return;
    }

    utrans_close(t1);
    utrans_close(inverse1);
}

static void TestSimpleRules() {
    /* Test rules */
    /* Example: rules 1. ab>x|y
     *                2. yc>z
     *
     * []|eabcd  start - no match, copy e to tranlated buffer
     * [e]|abcd  match rule 1 - copy output & adjust cursor
     * [ex|y]cd  match rule 2 - copy output & adjust cursor
     * [exz]|d   no match, copy d to transliterated buffer
     * [exzd]|   done
     */
    _expectRules("ab>x|y;"
                 "yc>z",
                 "eabcd", "exzd");

    /* Another set of rules:
     *    1. ab>x|yzacw
     *    2. za>q
     *    3. qc>r
     *    4. cw>n
     *
     * []|ab       Rule 1
     * [x|yzacw]   No match
     * [xy|zacw]   Rule 2
     * [xyq|cw]    Rule 4
     * [xyqn]|     Done
     */
    _expectRules("ab>x|yzacw;"
                 "za>q;"
                 "qc>r;"
                 "cw>n",
                 "ab", "xyqn");

    /* Test categories
     */
    _expectRules("$dummy=" "\\uE100" ";" /* careful here with E100 */
                 "$vowel=[aeiouAEIOU];"
                 "$lu=[:Lu:];"
                 "$vowel } $lu > '!';"
                 "$vowel > '&';"
                 "'!' { $lu > '^';"
                 "$lu > '*';"
                 "a > ERROR",
                 "abcdefgABCDEFGU", "&bcd&fg!^**!^*&");

    /* Test multiple passes 
    */ 
    _expectRules("abc > xy;"
                 "::Null;"
                 "aba > z;",
                 "abc ababc aba", "xy abxy z"); 
}

static void TestFilter() {
    UErrorCode status = U_ZERO_ERROR;
    UChar filt[128];
    UChar buf[128];
    UChar exp[128];
    char *cbuf;
    int32_t limit;
    const char* DATA[] = {
        "[^c]", /* Filter out 'c' */
        "abcde",
        "\\u0061\\u0062c\\u0064\\u0065",

        "", /* No filter */
        "abcde",
        "\\u0061\\u0062\\u0063\\u0064\\u0065"
    };
    int32_t DATA_length = UPRV_LENGTHOF(DATA);
    int32_t i;

    UTransliterator* hex = utrans_open("Any-Hex", UTRANS_FORWARD, NULL,0,NULL,&status);

    if (hex == 0 || U_FAILURE(status)) {
        log_err("FAIL: utrans_open(Unicode-Hex) failed, error=%s\n",
                u_errorName(status)); 
        goto exit;
    }

    for (i=0; i<DATA_length; i+=3) {
        /*u_uastrcpy(filt, DATA[i]);*/
        u_charsToUChars(DATA[i], filt, (int32_t)strlen(DATA[i])+1);
        utrans_setFilter(hex, filt, -1, &status);

        if (U_FAILURE(status)) {
            log_err("FAIL: utrans_setFilter() failed, error=%s\n",
                    u_errorName(status));
            goto exit;
        }
        
        /*u_uastrcpy(buf, DATA[i+1]);*/
        u_charsToUChars(DATA[i+1], buf, (int32_t)strlen(DATA[i+1])+1);
        limit = 5;
        utrans_transUChars(hex, buf, NULL, 128, 0, &limit, &status);
        
        if (U_FAILURE(status)) {
            log_err("FAIL: utrans_transUChars() failed, error=%s\n",
                    u_errorName(status));
            goto exit;
        }
        
        cbuf=aescstrdup(buf, -1);
        u_charsToUChars(DATA[i+2], exp, (int32_t)strlen(DATA[i+2])+1);
        if (0 == u_strcmp(buf, exp)) {
            log_verbose("Ok: %s | %s -> %s\n", DATA[i+1], DATA[i], cbuf);
        } else {
            log_err("FAIL: %s | %s -> %s, expected %s\n", DATA[i+1], DATA[i], cbuf, DATA[i+2]);
        }
    }

 exit:
    utrans_close(hex);
}

/**
 * Test the UReplaceableCallback extractBetween support.  We use a
 * transliterator known to rely on this call.
 */
static void TestExtractBetween() {

    UTransliterator *trans;
    UErrorCode status = U_ZERO_ERROR;
    UParseError parseErr;

    trans = utrans_open("Lower", UTRANS_FORWARD, NULL, -1,
                        &parseErr, &status);

    if (U_FAILURE(status)) {
        log_err("FAIL: utrans_open(Lower) failed, error=%s\n",
                u_errorName(status));
    } else {
        _expect(trans, "ABC", "abc");

        utrans_close(trans);
    }
}

/**
 * Test utrans_toRules, utrans_getSourceSet
 */

/* A simple transform with a small filter & source set: rules 50-100 chars unescaped, 100-200 chars escaped,
   filter & source set 4-20 chars */
static const UChar transSimpleID[] = { 0x79,0x6F,0x2D,0x79,0x6F,0x5F,0x42,0x4A,0 }; /* "yo-yo_BJ" */
static const char* transSimpleCName = "yo-yo_BJ";

enum { kUBufMax = 256 };
static void TestGetRulesAndSourceSet() {
    UErrorCode status = U_ZERO_ERROR;
    UTransliterator *utrans = utrans_openU(transSimpleID, -1, UTRANS_FORWARD, NULL, 0, NULL, &status);
    if ( U_SUCCESS(status) ) {
        USet* uset;
        UChar ubuf[kUBufMax];
        int32_t ulen;

        status = U_ZERO_ERROR;
        ulen = utrans_toRules(utrans, FALSE, ubuf, kUBufMax, &status);
        if ( U_FAILURE(status) || ulen <= 50 || ulen >= 100) {
            log_err("FAIL: utrans_toRules unescaped, expected noErr and len 50-100, got error=%s and len=%d\n",
                    u_errorName(status), ulen);
        }

        status = U_ZERO_ERROR;
        ulen = utrans_toRules(utrans, FALSE, NULL, 0, &status);
        if ( status != U_BUFFER_OVERFLOW_ERROR || ulen <= 50 || ulen >= 100) {
            log_err("FAIL: utrans_toRules unescaped, expected U_BUFFER_OVERFLOW_ERROR and len 50-100, got error=%s and len=%d\n",
                    u_errorName(status), ulen);
        }

        status = U_ZERO_ERROR;
        ulen = utrans_toRules(utrans, TRUE, ubuf, kUBufMax, &status);
        if ( U_FAILURE(status) || ulen <= 100 || ulen >= 200) {
            log_err("FAIL: utrans_toRules escaped, expected noErr and len 100-200, got error=%s and len=%d\n",
                    u_errorName(status), ulen);
        }

        status = U_ZERO_ERROR;
        uset = utrans_getSourceSet(utrans, FALSE, NULL, &status);
        ulen = uset_toPattern(uset, ubuf, kUBufMax, FALSE, &status);
        uset_close(uset);
        if ( U_FAILURE(status) || ulen <= 4 || ulen >= 20) {
            log_err("FAIL: utrans_getSourceSet useFilter, expected noErr and len 4-20, got error=%s and len=%d\n",
                    u_errorName(status), ulen);
        }

        status = U_ZERO_ERROR;
        uset = utrans_getSourceSet(utrans, TRUE, NULL, &status);
        ulen = uset_toPattern(uset, ubuf, kUBufMax, FALSE, &status);
        uset_close(uset);
        if ( U_FAILURE(status) || ulen <= 4 || ulen >= 20) {
            log_err("FAIL: utrans_getSourceSet ignoreFilter, expected noErr and len 4-20, got error=%s and len=%d\n",
                    u_errorName(status), ulen);
        }

        utrans_close(utrans);
    } else {
        log_data_err("FAIL: utrans_openRules(%s) failed, error=%s (Are you missing data?)\n",
                transSimpleCName, u_errorName(status));
    }
}

typedef struct {
    const char * transID;
    const char * sourceText;
    const char * targetText;
} TransIDSourceTarg;

static const TransIDSourceTarg dataVarCompItems[] = {
    { "Simplified-Traditional",
       "\\u4E0B\\u9762\\u662F\\u4E00\\u4E9B\\u4ECE\\u7B80\\u4F53\\u8F6C\\u6362\\u4E3A\\u7E41\\u4F53\\u5B57\\u793A\\u4F8B\\u6587\\u672C\\u3002",
       "\\u4E0B\\u9762\\u662F\\u4E00\\u4E9B\\u5F9E\\u7C21\\u9AD4\\u8F49\\u63DB\\u70BA\\u7E41\\u9AD4\\u5B57\\u793A\\u4F8B\\u6587\\u672C\\u3002" },
    { "Halfwidth-Fullwidth",
      "Sample text, \\uFF7B\\uFF9D\\uFF8C\\uFF9F\\uFF99\\uFF83\\uFF77\\uFF7D\\uFF84.",
      "\\uFF33\\uFF41\\uFF4D\\uFF50\\uFF4C\\uFF45\\u3000\\uFF54\\uFF45\\uFF58\\uFF54\\uFF0C\\u3000\\u30B5\\u30F3\\u30D7\\u30EB\\u30C6\\u30AD\\u30B9\\u30C8\\uFF0E" },
    { "Han-Latin/Names; Latin-Bopomofo",
       "\\u4E07\\u4FDF\\u919C\\u5974\\u3001\\u533A\\u695A\\u826F\\u3001\\u4EFB\\u70E8\\u3001\\u5CB3\\u98DB",
       "\\u3107\\u311B\\u02CB \\u3111\\u3127\\u02CA \\u3114\\u3121\\u02C7 \\u310B\\u3128\\u02CA\\u3001 \\u3121 \\u3114\\u3128\\u02C7 \\u310C\\u3127\\u3124\\u02CA\\u3001 \\u3116\\u3123\\u02CA \\u3127\\u311D\\u02CB\\u3001 \\u3129\\u311D\\u02CB \\u3108\\u311F" },
    { "Greek-Latin",
      "\\u1F08 \\u1FBC \\u1F89 \\u1FEC",
      "A \\u0100I H\\u0100I RH" },
/* The following transform is provisional and not present in ICU 60
    { "Greek-Latin/BGN",
      "\\u1F08 \\u1FBC \\u1F89 \\u1FEC",
      "A\\u0313 A\\u0345 A\\u0314\\u0345 \\u1FEC" },
*/
    { "Greek-Latin/UNGEGN",
      "\\u1F08 \\u1FBC \\u1F89 \\u1FEC",
      "A A A R" },
    { NULL, NULL, NULL }
};

enum { kBBufMax = 384 };
static void TestDataVariantsCompounds() {
    const TransIDSourceTarg* itemsPtr;
    for (itemsPtr = dataVarCompItems; itemsPtr->transID != NULL; itemsPtr++) {
        UErrorCode status = U_ZERO_ERROR;
        UChar utrid[kUBufMax];
        int32_t utridlen = u_unescape(itemsPtr->transID, utrid, kUBufMax);
        UTransliterator* utrans = utrans_openU(utrid, utridlen, UTRANS_FORWARD, NULL, 0, NULL, &status);
        if (U_FAILURE(status)) {
            log_data_err("FAIL: utrans_openRules(%s) failed, error=%s (Are you missing data?)\n", itemsPtr->transID, u_errorName(status));
            continue;
        }
        UChar text[kUBufMax];
        int32_t textLen =  u_unescape(itemsPtr->sourceText, text, kUBufMax);
        int32_t textLim = textLen;
        utrans_transUChars(utrans, text, &textLen, kUBufMax, 0, &textLim, &status);
        if (U_FAILURE(status)) {
            log_err("FAIL: utrans_transUChars(%s) failed, error=%s\n", itemsPtr->transID, u_errorName(status));
        } else {
            UChar expect[kUBufMax];
            int32_t expectLen =  u_unescape(itemsPtr->targetText, expect, kUBufMax);
            if (textLen != expectLen || u_strncmp(text, expect, textLen) != 0) {
                char btext[kBBufMax], bexpect[kBBufMax];
                u_austrncpy(btext, text, textLen);
                u_austrncpy(bexpect, expect, expectLen);
                log_err("FAIL: utrans_transUChars(%s),\n       expect %s\n       get    %s\n", itemsPtr->transID, bexpect, btext);
            }
        }
        utrans_close(utrans);
    }
}

static void _expectRules(const char* crules,
                  const char* cfrom,
                  const char* cto) {
    /* u_uastrcpy has no capacity param for the buffer -- so just
     * make all buffers way too big */
    enum { CAP = 256 };
    UChar rules[CAP];
    UTransliterator *trans;
    UErrorCode status = U_ZERO_ERROR;
    UParseError parseErr;

    u_uastrcpy(rules, crules);

    trans = utrans_open(crules /*use rules as ID*/, UTRANS_FORWARD, rules, -1, 
                             &parseErr, &status);
    if (U_FAILURE(status)) {
        utrans_close(trans);
        log_data_err("FAIL: utrans_openRules(%s) failed, error=%s (Are you missing data?)\n",
                crules, u_errorName(status));
        return;
    }

    _expect(trans, cfrom, cto);

    utrans_close(trans);
}

static void _expect(const UTransliterator* trans,
             const char* cfrom,
             const char* cto) {
    /* u_uastrcpy has no capacity param for the buffer -- so just
     * make all buffers way too big */
    enum { CAP = 256 };
    UChar from[CAP];
    UChar to[CAP];
    UChar buf[CAP];
    const UChar *ID;
    int32_t IDLength;
    const char *id;

    UErrorCode status = U_ZERO_ERROR;
    int32_t limit;
    UTransPosition pos;
    XReplaceable xrep;
    XReplaceable *xrepPtr = &xrep;
    UReplaceableCallbacks xrepVtable;

    u_uastrcpy(from, cfrom);
    u_uastrcpy(to, cto);

    ID = utrans_getUnicodeID(trans, &IDLength);
    id = aescstrdup(ID, IDLength);

    /* utrans_transUChars() */
    u_strcpy(buf, from);
    limit = u_strlen(buf);
    utrans_transUChars(trans, buf, NULL, CAP, 0, &limit, &status);
    if (U_FAILURE(status)) {
        log_err("FAIL: utrans_transUChars() failed, error=%s\n",
                u_errorName(status));
        return;
    }

    if (0 == u_strcmp(buf, to)) {
        log_verbose("Ok: utrans_transUChars(%s) x %s -> %s\n",
                    id, cfrom, cto);
    } else {
        char actual[CAP];
        u_austrcpy(actual, buf);
        log_err("FAIL: utrans_transUChars(%s) x %s -> %s, expected %s\n",
                id, cfrom, actual, cto);
    }

    /* utrans_transIncrementalUChars() */
    u_strcpy(buf, from);
    pos.start = pos.contextStart = 0;
    pos.limit = pos.contextLimit = u_strlen(buf);
    utrans_transIncrementalUChars(trans, buf, NULL, CAP, &pos, &status);
    utrans_transUChars(trans, buf, NULL, CAP, pos.start, &pos.limit, &status);
    if (U_FAILURE(status)) {
        log_err("FAIL: utrans_transIncrementalUChars() failed, error=%s\n",
                u_errorName(status));
        return;
    }

    if (0 == u_strcmp(buf, to)) {
        log_verbose("Ok: utrans_transIncrementalUChars(%s) x %s -> %s\n",
                    id, cfrom, cto);
    } else {
        char actual[CAP];
        u_austrcpy(actual, buf);
        log_err("FAIL: utrans_transIncrementalUChars(%s) x %s -> %s, expected %s\n",
                id, cfrom, actual, cto);
    }

    /* utrans_trans() */
    InitXReplaceableCallbacks(&xrepVtable);
    InitXReplaceable(&xrep, cfrom);
    limit = u_strlen(from);
    utrans_trans(trans, (UReplaceable*)xrepPtr, &xrepVtable, 0, &limit, &status);
    if (U_FAILURE(status)) {
        log_err("FAIL: utrans_trans() failed, error=%s\n",
                u_errorName(status));
        FreeXReplaceable(&xrep);
        return;
    }

    if (0 == u_strcmp(xrep.text, to)) {
        log_verbose("Ok: utrans_trans(%s) x %s -> %s\n",
                    id, cfrom, cto);
    } else {
        char actual[CAP];
        u_austrcpy(actual, xrep.text);
        log_err("FAIL: utrans_trans(%s) x %s -> %s, expected %s\n",
                id, cfrom, actual, cto);
    }
    FreeXReplaceable(&xrep);

    /* utrans_transIncremental() */
    InitXReplaceable(&xrep, cfrom);
    pos.start = pos.contextStart = 0;
    pos.limit = pos.contextLimit = u_strlen(from);
    utrans_transIncremental(trans, (UReplaceable*)xrepPtr, &xrepVtable, &pos, &status);
    utrans_trans(trans, (UReplaceable*)xrepPtr, &xrepVtable, pos.start, &pos.limit, &status);
    if (U_FAILURE(status)) {
        log_err("FAIL: utrans_transIncremental() failed, error=%s\n",
                u_errorName(status));
        FreeXReplaceable(&xrep);
        return;
    }

    if (0 == u_strcmp(xrep.text, to)) {
        log_verbose("Ok: utrans_transIncremental(%s) x %s -> %s\n",
                    id, cfrom, cto);
    } else {
        char actual[CAP];
        u_austrcpy(actual, xrep.text);
        log_err("FAIL: utrans_transIncremental(%s) x %s -> %s, expected %s\n",
                id, cfrom, actual, cto);
    }
    FreeXReplaceable(&xrep);
}

#endif /* #if !UCONFIG_NO_TRANSLITERATION */
