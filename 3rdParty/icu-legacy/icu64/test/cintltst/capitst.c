// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * Copyright (c) 1997-2016, International Business Machines
 * Corporation and others. All Rights Reserved.
 ********************************************************************/
/*****************************************************************************
*
* File CAPITEST.C
*
* Modification History:
*        Name                     Description
*     Madhu Katragadda             Ported for C API
*     Brian Rower                  Added TestOpenVsOpenRules
******************************************************************************
*//* C API TEST For COLLATOR */

#include "unicode/utypes.h"

#if !UCONFIG_NO_COLLATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "unicode/uloc.h"
#include "unicode/ulocdata.h"
#include "unicode/ustring.h"
#include "unicode/ures.h"
#include "unicode/ucoleitr.h"
#include "cintltst.h"
#include "capitst.h"
#include "ccolltst.h"
#include "putilimp.h"
#include "cmemory.h"
#include "cstring.h"
#include "ucol_imp.h"

static void TestAttribute(void);
static void TestDefault(void);
static void TestDefaultKeyword(void);
static void TestBengaliSortKey(void);


static char* U_EXPORT2 ucol_sortKeyToString(const UCollator *coll, const uint8_t *sortkey, char *buffer, uint32_t len) {
    uint32_t position = 0;
    uint8_t b;

    if (position + 1 < len)
        position += sprintf(buffer + position, "[");
    while ((b = *sortkey++) != 0) {
        if (b == 1 && position + 5 < len) {
            position += sprintf(buffer + position, "%02X . ", b);
        } else if (b != 1 && position + 3 < len) {
            position += sprintf(buffer + position, "%02X ", b);
        }
    }
    if (position + 3 < len)
        position += sprintf(buffer + position, "%02X]", b);
    return buffer;
}

void addCollAPITest(TestNode** root)
{
    /* WEIVTODO: return tests here */
    addTest(root, &TestProperty,      "tscoll/capitst/TestProperty");
    addTest(root, &TestRuleBasedColl, "tscoll/capitst/TestRuleBasedColl");
    addTest(root, &TestCompare,       "tscoll/capitst/TestCompare");
    addTest(root, &TestSortKey,       "tscoll/capitst/TestSortKey");
    addTest(root, &TestHashCode,      "tscoll/capitst/TestHashCode");
    addTest(root, &TestElemIter,      "tscoll/capitst/TestElemIter");
    addTest(root, &TestGetAll,        "tscoll/capitst/TestGetAll");
    /*addTest(root, &TestGetDefaultRules, "tscoll/capitst/TestGetDefaultRules");*/
    addTest(root, &TestDecomposition, "tscoll/capitst/TestDecomposition");
    addTest(root, &TestSafeClone, "tscoll/capitst/TestSafeClone");
    addTest(root, &TestCloneBinary, "tscoll/capitst/TestCloneBinary");
    addTest(root, &TestGetSetAttr, "tscoll/capitst/TestGetSetAttr");
    addTest(root, &TestBounds, "tscoll/capitst/TestBounds");
    addTest(root, &TestGetLocale, "tscoll/capitst/TestGetLocale");
    addTest(root, &TestSortKeyBufferOverrun, "tscoll/capitst/TestSortKeyBufferOverrun");
    addTest(root, &TestAttribute, "tscoll/capitst/TestAttribute");
    addTest(root, &TestGetTailoredSet, "tscoll/capitst/TestGetTailoredSet");
    addTest(root, &TestMergeSortKeys, "tscoll/capitst/TestMergeSortKeys");
    addTest(root, &TestShortString, "tscoll/capitst/TestShortString");
    addTest(root, &TestGetContractionsAndUnsafes, "tscoll/capitst/TestGetContractionsAndUnsafes");
    addTest(root, &TestOpenBinary, "tscoll/capitst/TestOpenBinary");
    addTest(root, &TestDefault, "tscoll/capitst/TestDefault");
    addTest(root, &TestDefaultKeyword, "tscoll/capitst/TestDefaultKeyword");
    addTest(root, &TestOpenVsOpenRules, "tscoll/capitst/TestOpenVsOpenRules");
    addTest(root, &TestBengaliSortKey, "tscoll/capitst/TestBengaliSortKey");
    addTest(root, &TestGetKeywordValuesForLocale, "tscoll/capitst/TestGetKeywordValuesForLocale");
    addTest(root, &TestStrcollNull, "tscoll/capitst/TestStrcollNull");
}

void TestGetSetAttr(void) {
  UErrorCode status = U_ZERO_ERROR;
  UCollator *coll = ucol_open(NULL, &status);
  struct attrTest {
    UColAttribute att;
    UColAttributeValue val[5];
    uint32_t valueSize;
    UColAttributeValue nonValue;
  } attrs[] = {
    {UCOL_FRENCH_COLLATION, {UCOL_ON, UCOL_OFF}, 2, UCOL_SHIFTED},
    {UCOL_ALTERNATE_HANDLING, {UCOL_NON_IGNORABLE, UCOL_SHIFTED}, 2, UCOL_OFF},/* attribute for handling variable elements*/
    {UCOL_CASE_FIRST, {UCOL_OFF, UCOL_LOWER_FIRST, UCOL_UPPER_FIRST}, 3, UCOL_SHIFTED},/* who goes first, lower case or uppercase */
    {UCOL_CASE_LEVEL, {UCOL_ON, UCOL_OFF}, 2, UCOL_SHIFTED},/* do we have an extra case level */
    {UCOL_NORMALIZATION_MODE, {UCOL_ON, UCOL_OFF}, 2, UCOL_SHIFTED},/* attribute for normalization */
    {UCOL_DECOMPOSITION_MODE, {UCOL_ON, UCOL_OFF}, 2, UCOL_SHIFTED},
    {UCOL_STRENGTH,         {UCOL_PRIMARY, UCOL_SECONDARY, UCOL_TERTIARY, UCOL_QUATERNARY, UCOL_IDENTICAL}, 5, UCOL_SHIFTED},/* attribute for strength */
    {UCOL_HIRAGANA_QUATERNARY_MODE, {UCOL_ON, UCOL_OFF}, 2, UCOL_SHIFTED},/* when turned on, this attribute */
  };
  UColAttribute currAttr;
  UColAttributeValue value;
  uint32_t i = 0, j = 0;

  if (coll == NULL) {
    log_err_status(status, "Unable to open collator. %s\n", u_errorName(status));
    return;
  } 
  for(i = 0; i<UPRV_LENGTHOF(attrs); i++) {
    currAttr = attrs[i].att;
    ucol_setAttribute(coll, currAttr, UCOL_DEFAULT, &status);
    if(U_FAILURE(status)) {
      log_err_status(status, "ucol_setAttribute with the default value returned error: %s\n", u_errorName(status));
      break;
    }
    value = ucol_getAttribute(coll, currAttr, &status);
    if(U_FAILURE(status)) {
      log_err("ucol_getAttribute returned error: %s\n", u_errorName(status));
      break;
    }
    for(j = 0; j<attrs[i].valueSize; j++) {
      ucol_setAttribute(coll, currAttr, attrs[i].val[j], &status);
      if(U_FAILURE(status)) {
        log_err("ucol_setAttribute with the value %i returned error: %s\n", attrs[i].val[j], u_errorName(status));
        break;
      }
    }
    status = U_ZERO_ERROR;
    ucol_setAttribute(coll, currAttr, attrs[i].nonValue, &status);
    if(U_SUCCESS(status)) {
      log_err("ucol_setAttribute with the bad value didn't return an error\n");
      break;
    }
    status = U_ZERO_ERROR;

    ucol_setAttribute(coll, currAttr, value, &status);
    if(U_FAILURE(status)) {
      log_err("ucol_setAttribute with the default valuereturned error: %s\n", u_errorName(status));
      break;
    }
  }
  status = U_ZERO_ERROR;
  value = ucol_getAttribute(coll, UCOL_ATTRIBUTE_COUNT, &status);
  if(U_SUCCESS(status)) {
    log_err("ucol_getAttribute for UCOL_ATTRIBUTE_COUNT didn't return an error\n");
  }
  status = U_ZERO_ERROR;
  ucol_setAttribute(coll, UCOL_ATTRIBUTE_COUNT, UCOL_DEFAULT, &status);
  if(U_SUCCESS(status)) {
    log_err("ucol_setAttribute for UCOL_ATTRIBUTE_COUNT didn't return an error\n");
  }
  status = U_ZERO_ERROR;
  ucol_close(coll);
}


static void doAssert(int condition, const char *message)
{
    if (condition==0) {
        log_err("ERROR :  %s\n", message);
    }
}

#define UTF8_BUF_SIZE 128

static void doStrcoll(const UCollator* coll, const UChar* src, int32_t srcLen, const UChar* tgt, int32_t tgtLen,
                    UCollationResult expected, const char *message) {
    UErrorCode err = U_ZERO_ERROR;
    char srcU8[UTF8_BUF_SIZE], tgtU8[UTF8_BUF_SIZE];
    int32_t srcU8Len = -1, tgtU8Len = -1;
    int32_t len = 0;

    if (ucol_strcoll(coll, src, srcLen, tgt, tgtLen) != expected) {
        log_err("ERROR :  %s\n", message);
    }

    u_strToUTF8(srcU8, UTF8_BUF_SIZE, &len, src, srcLen, &err);
    if (U_FAILURE(err) || len >= UTF8_BUF_SIZE) {
        log_err("ERROR : UTF-8 conversion error\n");
        return;
    }
    if (srcLen >= 0) {
        srcU8Len = len;
    }
    u_strToUTF8(tgtU8, UTF8_BUF_SIZE, &len, tgt, tgtLen, &err);
    if (U_FAILURE(err) || len >= UTF8_BUF_SIZE) {
        log_err("ERROR : UTF-8 conversion error\n");
        return;
    }
    if (tgtLen >= 0) {
        tgtU8Len = len;
    }

    if (ucol_strcollUTF8(coll, srcU8, srcU8Len, tgtU8, tgtU8Len, &err) != expected
        || U_FAILURE(err)) {
        log_err("ERROR: %s (strcollUTF8)\n", message);
    }
}

#if 0
/* We don't have default rules, at least not in the previous sense */
void TestGetDefaultRules(){
    uint32_t size=0;
    UErrorCode status=U_ZERO_ERROR;
    UCollator *coll=NULL;
    int32_t len1 = 0, len2=0;
    uint8_t *binColData = NULL;

    UResourceBundle *res = NULL;
    UResourceBundle *binColl = NULL;
    uint8_t *binResult = NULL;


    const UChar * defaultRulesArray=ucol_getDefaultRulesArray(&size);
    log_verbose("Test the function ucol_getDefaultRulesArray()\n");

    coll = ucol_openRules(defaultRulesArray, size, UCOL_ON, UCOL_PRIMARY, &status);
    if(U_SUCCESS(status) && coll !=NULL) {
        binColData = (uint8_t*)ucol_cloneRuleData(coll, &len1, &status);

    }


    status=U_ZERO_ERROR;
    res=ures_open(NULL, "root", &status);
    if(U_FAILURE(status)){
        log_err("ERROR: Failed to get resource for \"root Locale\" with %s", myErrorName(status));
        return;
    }
    binColl=ures_getByKey(res, "%%Collation", binColl, &status);
    if(U_SUCCESS(status)){
        binResult=(uint8_t*)ures_getBinary(binColl,  &len2, &status);
        if(U_FAILURE(status)){
            log_err("ERROR: ures_getBinary() failed\n");
        }
    }else{
        log_err("ERROR: ures_getByKey(locale(default), %%Collation) failed");
    }


    if(len1 != len2){
        log_err("Error: ucol_getDefaultRulesArray() failed to return the correct length.\n");
    }
    if(memcmp(binColData, binResult, len1) != 0){
        log_err("Error: ucol_getDefaultRulesArray() failed\n");
    }

    free(binColData);
    ures_close(binColl);
    ures_close(res);
    ucol_close(coll);

}
#endif

/* Collator Properties
 ucol_open, ucol_strcoll,  getStrength/setStrength
 getDecomposition/setDecomposition, getDisplayName*/
void TestProperty()
{
    UCollator *col, *ruled;
    const UChar *rules;
    UChar *disName;
    int32_t len = 0;
    UChar source[12], target[12];
    int32_t tempLength;
    UErrorCode status = U_ZERO_ERROR;
    /*
     * Expected version of the English collator.
     * Currently, the major/minor version numbers change when the builder code
     * changes,
     * number 2 is from the tailoring data version and
     * number 3 is the UCA version.
     * This changes with every UCA version change, and the expected value
     * needs to be adjusted.
     * Same in intltest/apicoll.cpp.
     */
    UVersionInfo currVersionArray = {0x31, 0xC0, 0x05, 0x2A};  /* from ICU 4.4/UCA 5.2 */
    UVersionInfo versionArray = {0, 0, 0, 0};
    UVersionInfo versionUCAArray = {0, 0, 0, 0};
    UVersionInfo versionUCDArray = {0, 0, 0, 0};

    log_verbose("The property tests begin : \n");
    log_verbose("Test ucol_strcoll : \n");
    col = ucol_open("en_US", &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "Default Collator creation failed.: %s\n", myErrorName(status));
        return;
    }

    ucol_getVersion(col, versionArray);
    /* Check for a version greater than some value rather than equality
     * so that we need not update the expected version each time. */
    if (uprv_memcmp(versionArray, currVersionArray, 4)<0) {
      log_err("Testing ucol_getVersion() - unexpected result: %02x.%02x.%02x.%02x\n",
              versionArray[0], versionArray[1], versionArray[2], versionArray[3]);
    } else {
      log_verbose("ucol_getVersion() result: %02x.%02x.%02x.%02x\n",
                  versionArray[0], versionArray[1], versionArray[2], versionArray[3]);
    }

    /* Assume that the UCD and UCA versions are the same,
     * rather than hardcoding (and updating each time) a particular UCA version. */
    u_getUnicodeVersion(versionUCDArray);
    ucol_getUCAVersion(col, versionUCAArray);
    if (0!=uprv_memcmp(versionUCAArray, versionUCDArray, 4)) {
      log_err("Testing ucol_getUCAVersion() - unexpected result: %hu.%hu.%hu.%hu\n",
              versionUCAArray[0], versionUCAArray[1], versionUCAArray[2], versionUCAArray[3]);
    }

    u_uastrcpy(source, "ab");
    u_uastrcpy(target, "abc");

    doStrcoll(col, source, u_strlen(source), target, u_strlen(target), UCOL_LESS, "ab < abc comparison failed");

    u_uastrcpy(source, "ab");
    u_uastrcpy(target, "AB");

    doStrcoll(col, source, u_strlen(source), target, u_strlen(target), UCOL_LESS, "ab < AB comparison failed");

    u_uastrcpy(source, "blackbird");
    u_uastrcpy(target, "black-bird");

    doStrcoll(col, source, u_strlen(source), target, u_strlen(target), UCOL_GREATER, "black-bird > blackbird comparison failed");

    u_uastrcpy(source, "black bird");
    u_uastrcpy(target, "black-bird");

    doStrcoll(col, source, u_strlen(source), target, u_strlen(target), UCOL_LESS, "black bird < black-bird comparison failed");

    u_uastrcpy(source, "Hello");
    u_uastrcpy(target, "hello");

    doStrcoll(col, source, u_strlen(source), target, u_strlen(target), UCOL_GREATER, "Hello > hello comparison failed");

    log_verbose("Test ucol_strcoll ends.\n");

    log_verbose("testing ucol_getStrength() method ...\n");
    doAssert( (ucol_getStrength(col) == UCOL_TERTIARY), "collation object has the wrong strength");
    doAssert( (ucol_getStrength(col) != UCOL_PRIMARY), "collation object's strength is primary difference");

    log_verbose("testing ucol_setStrength() method ...\n");
    ucol_setStrength(col, UCOL_SECONDARY);
    doAssert( (ucol_getStrength(col) != UCOL_TERTIARY), "collation object's strength is secondary difference");
    doAssert( (ucol_getStrength(col) != UCOL_PRIMARY), "collation object's strength is primary difference");
    doAssert( (ucol_getStrength(col) == UCOL_SECONDARY), "collation object has the wrong strength");


    log_verbose("Get display name for the default collation in German : \n");

    len=ucol_getDisplayName("en_US", "de_DE", NULL, 0,  &status);
    if(status==U_BUFFER_OVERFLOW_ERROR){
        status=U_ZERO_ERROR;
        disName=(UChar*)malloc(sizeof(UChar) * (len+1));
        ucol_getDisplayName("en_US", "de_DE", disName, len+1,  &status);
        log_verbose("the display name for default collation in german: %s\n", austrdup(disName) );
        free(disName);
    }
    if(U_FAILURE(status)){
        log_err("ERROR: in getDisplayName: %s\n", myErrorName(status));
        return;
    }
    log_verbose("Default collation getDisplayName ended.\n");

    ruled = ucol_open("da_DK", &status);
    if(U_FAILURE(status)) {
        log_data_err("ucol_open(\"da_DK\") failed - %s\n", u_errorName(status));
        ucol_close(col);
        return;
    }
    log_verbose("ucol_getRules() testing ...\n");
    rules = ucol_getRules(ruled, &tempLength);
    if(tempLength == 0) {
        log_data_err("missing da_DK tailoring rule string\n");
    } else {
        UChar aa[2] = { 0x61, 0x61 };
        doAssert(u_strFindFirst(rules, tempLength, aa, 2) != NULL,
                 "da_DK rules do not contain 'aa'");
    }
    log_verbose("getRules tests end.\n");
    {
        UChar *buffer = (UChar *)malloc(200000*sizeof(UChar));
        int32_t bufLen = 200000;
        buffer[0] = '\0';
        log_verbose("ucol_getRulesEx() testing ...\n");
        tempLength = ucol_getRulesEx(col,UCOL_TAILORING_ONLY,buffer,bufLen );
        doAssert( tempLength == 0x00, "getRulesEx() result incorrect" );
        log_verbose("getRules tests end.\n");

        log_verbose("ucol_getRulesEx() testing ...\n");
        tempLength=ucol_getRulesEx(col,UCOL_FULL_RULES,buffer,bufLen );
        if(tempLength == 0) {
            log_data_err("missing *full* rule string\n");
        }
        log_verbose("getRulesEx tests end.\n");
        free(buffer);
    }
    ucol_close(ruled);
    ucol_close(col);

    log_verbose("open an collator for french locale");
    col = ucol_open("fr_FR", &status);
    if (U_FAILURE(status)) {
       log_err("ERROR: Creating French collation failed.: %s\n", myErrorName(status));
        return;
    }
    ucol_setStrength(col, UCOL_PRIMARY);
    log_verbose("testing ucol_getStrength() method again ...\n");
    doAssert( (ucol_getStrength(col) != UCOL_TERTIARY), "collation object has the wrong strength");
    doAssert( (ucol_getStrength(col) == UCOL_PRIMARY), "collation object's strength is not primary difference");

    log_verbose("testing French ucol_setStrength() method ...\n");
    ucol_setStrength(col, UCOL_TERTIARY);
    doAssert( (ucol_getStrength(col) == UCOL_TERTIARY), "collation object's strength is not tertiary difference");
    doAssert( (ucol_getStrength(col) != UCOL_PRIMARY), "collation object's strength is primary difference");
    doAssert( (ucol_getStrength(col) != UCOL_SECONDARY), "collation object's strength is secondary difference");
    ucol_close(col);

    log_verbose("Get display name for the french collation in english : \n");
    len=ucol_getDisplayName("fr_FR", "en_US", NULL, 0,  &status);
    if(status==U_BUFFER_OVERFLOW_ERROR){
        status=U_ZERO_ERROR;
        disName=(UChar*)malloc(sizeof(UChar) * (len+1));
        ucol_getDisplayName("fr_FR", "en_US", disName, len+1,  &status);
        log_verbose("the display name for french collation in english: %s\n", austrdup(disName) );
        free(disName);
    }
    if(U_FAILURE(status)){
        log_err("ERROR: in getDisplayName: %s\n", myErrorName(status));
        return;
    }
    log_verbose("Default collation getDisplayName ended.\n");

}

/* Test RuleBasedCollator and getRules*/
void TestRuleBasedColl()
{
    UCollator *col1, *col2, *col3, *col4;
    UCollationElements *iter1, *iter2;
    UChar ruleset1[60];
    UChar ruleset2[50];
    UChar teststr[10];
    const UChar *rule1, *rule2, *rule3, *rule4;
    int32_t tempLength;
    UErrorCode status = U_ZERO_ERROR;
    u_uastrcpy(ruleset1, "&9 < a, A < b, B < c, C; ch, cH, Ch, CH < d, D, e, E");
    u_uastrcpy(ruleset2, "&9 < a, A < b, B < c, C < d, D, e, E");


    col1 = ucol_openRules(ruleset1, u_strlen(ruleset1), UCOL_DEFAULT, UCOL_DEFAULT_STRENGTH, NULL,&status);
    if (U_FAILURE(status)) {
        log_err_status(status, "RuleBased Collator creation failed.: %s\n", myErrorName(status));
        return;
    }
    else
        log_verbose("PASS: RuleBased Collator creation passed\n");

    status = U_ZERO_ERROR;
    col2 = ucol_openRules(ruleset2, u_strlen(ruleset2),  UCOL_DEFAULT, UCOL_DEFAULT_STRENGTH, NULL, &status);
    if (U_FAILURE(status)) {
        log_err("RuleBased Collator creation failed.: %s\n", myErrorName(status));
        return;
    }
    else
        log_verbose("PASS: RuleBased Collator creation passed\n");


    status = U_ZERO_ERROR;
    col3= ucol_open(NULL, &status);
    if (U_FAILURE(status)) {
        log_err("Default Collator creation failed.: %s\n", myErrorName(status));
        return;
    }
    else
        log_verbose("PASS: Default Collator creation passed\n");

    rule1 = ucol_getRules(col1, &tempLength);
    rule2 = ucol_getRules(col2, &tempLength);
    rule3 = ucol_getRules(col3, &tempLength);

    doAssert((u_strcmp(rule1, rule2) != 0), "Default collator getRules failed");
    doAssert((u_strcmp(rule2, rule3) != 0), "Default collator getRules failed");
    doAssert((u_strcmp(rule1, rule3) != 0), "Default collator getRules failed");

    col4=ucol_openRules(rule2, u_strlen(rule2), UCOL_DEFAULT, UCOL_DEFAULT_STRENGTH, NULL, &status);
    if (U_FAILURE(status)) {
        log_err("RuleBased Collator creation failed.: %s\n", myErrorName(status));
        return;
    }
    rule4= ucol_getRules(col4, &tempLength);
    doAssert((u_strcmp(rule2, rule4) == 0), "Default collator getRules failed");

    ucol_close(col1);
    ucol_close(col2);
    ucol_close(col3);
    ucol_close(col4);

    /* tests that modifier ! is always ignored */
    u_uastrcpy(ruleset1, "!&a<b");
    teststr[0] = 0x0e40;
    teststr[1] = 0x0e01;
    teststr[2] = 0x0e2d;
    col1 = ucol_openRules(ruleset1, u_strlen(ruleset1), UCOL_DEFAULT, UCOL_DEFAULT_STRENGTH, NULL, &status);
    if (U_FAILURE(status)) {
        log_err("RuleBased Collator creation failed.: %s\n", myErrorName(status));
        return;
    }
    col2 = ucol_open("en_US", &status);
    if (U_FAILURE(status)) {
        log_err("en_US Collator creation failed.: %s\n", myErrorName(status));
        return;
    }
    iter1 = ucol_openElements(col1, teststr, 3, &status);
    iter2 = ucol_openElements(col2, teststr, 3, &status);
    if(U_FAILURE(status)) {
        log_err("ERROR: CollationElement iterator creation failed.: %s\n", myErrorName(status));
        return;
    }
    while (TRUE) {
        /* testing with en since thai has its own tailoring */
        uint32_t ce = ucol_next(iter1, &status);
        uint32_t ce2 = ucol_next(iter2, &status);
        if(U_FAILURE(status)) {
            log_err("ERROR: CollationElement iterator creation failed.: %s\n", myErrorName(status));
            return;
        }
        if (ce2 != ce) {
             log_err("! modifier test failed");
        }
        if (ce == UCOL_NULLORDER) {
            break;
        }
    }
    ucol_closeElements(iter1);
    ucol_closeElements(iter2);
    ucol_close(col1);
    ucol_close(col2);
    /* CLDR 24+ requires a reset before the first relation */
    u_uastrcpy(ruleset1, "< z < a");
    col1 = ucol_openRules(ruleset1, u_strlen(ruleset1), UCOL_DEFAULT, UCOL_DEFAULT_STRENGTH, NULL, &status);
    if (status != U_PARSE_ERROR && status != U_INVALID_FORMAT_ERROR) {
        log_err("ucol_openRules(without initial reset: '< z < a') "
                "should fail with U_PARSE_ERROR or U_INVALID_FORMAT_ERROR but yielded %s\n",
                myErrorName(status));
    }
    ucol_close(col1);
}

void TestCompare()
{
    UErrorCode status = U_ZERO_ERROR;
    UCollator *col;
    UChar* test1;
    UChar* test2;

    log_verbose("The compare tests begin : \n");
    status=U_ZERO_ERROR;
    col = ucol_open("en_US", &status);
    if(U_FAILURE(status)) {
        log_err_status(status, "ucal_open() collation creation failed.: %s\n", myErrorName(status));
        return;
    }
    test1=(UChar*)malloc(sizeof(UChar) * 6);
    test2=(UChar*)malloc(sizeof(UChar) * 6);
    u_uastrcpy(test1, "Abcda");
    u_uastrcpy(test2, "abcda");

    log_verbose("Use tertiary comparison level testing ....\n");

    doAssert( (!ucol_equal(col, test1, u_strlen(test1), test2, u_strlen(test2))), "Result should be \"Abcda\" != \"abcda\" ");
    doAssert( (ucol_greater(col, test1, u_strlen(test1), test2, u_strlen(test2))), "Result should be \"Abcda\" >>> \"abcda\" ");
    doAssert( (ucol_greaterOrEqual(col, test1, u_strlen(test1), test2, u_strlen(test2))), "Result should be \"Abcda\" >>> \"abcda\"");

    ucol_setStrength(col, UCOL_SECONDARY);
    log_verbose("Use secondary comparison level testing ....\n");

    doAssert( (ucol_equal(col, test1, u_strlen(test1), test2, u_strlen(test2) )), "Result should be \"Abcda\" == \"abcda\"");
    doAssert( (!ucol_greater(col, test1, u_strlen(test1), test2, u_strlen(test2))), "Result should be \"Abcda\" == \"abcda\"");
    doAssert( (ucol_greaterOrEqual(col, test1, u_strlen(test1), test2, u_strlen(test2) )), "Result should be \"Abcda\" == \"abcda\"");

    ucol_setStrength(col, UCOL_PRIMARY);
    log_verbose("Use primary comparison level testing ....\n");

    doAssert( (ucol_equal(col, test1, u_strlen(test1), test2, u_strlen(test2))), "Result should be \"Abcda\" == \"abcda\"");
    doAssert( (!ucol_greater(col, test1, u_strlen(test1), test2, u_strlen(test2))), "Result should be \"Abcda\" == \"abcda\"");
    doAssert( (ucol_greaterOrEqual(col, test1, u_strlen(test1), test2, u_strlen(test2))), "Result should be \"Abcda\" == \"abcda\"");


    log_verbose("The compare tests end.\n");
    ucol_close(col);
    free(test1);
    free(test2);

}
/*
---------------------------------------------
 tests decomposition setting
*/
void TestDecomposition() {
    UErrorCode status = U_ZERO_ERROR;
    UCollator *en_US, *el_GR, *vi_VN;
    en_US = ucol_open("en_US", &status);
    el_GR = ucol_open("el_GR", &status);
    vi_VN = ucol_open("vi_VN", &status);

    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: collation creation failed.: %s\n", myErrorName(status));
        return;
    }

    if (ucol_getAttribute(vi_VN, UCOL_NORMALIZATION_MODE, &status) != UCOL_ON ||
        U_FAILURE(status))
    {
        log_err("ERROR: vi_VN collation did not have canonical decomposition for normalization!\n");
    }

    status = U_ZERO_ERROR;
    if (ucol_getAttribute(el_GR, UCOL_NORMALIZATION_MODE, &status) != UCOL_ON ||
        U_FAILURE(status))
    {
        log_err("ERROR: el_GR collation did not have canonical decomposition for normalization!\n");
    }

    status = U_ZERO_ERROR;
    if (ucol_getAttribute(en_US, UCOL_NORMALIZATION_MODE, &status) != UCOL_OFF ||
        U_FAILURE(status))
    {
        log_err("ERROR: en_US collation had canonical decomposition for normalization!\n");
    }

    ucol_close(en_US);
    ucol_close(el_GR);
    ucol_close(vi_VN);
}

#define CLONETEST_COLLATOR_COUNT 4

void TestSafeClone() {
    UChar test1[6];
    UChar test2[6];
    static const UChar umlautUStr[] = {0x00DC, 0};
    static const UChar oeStr[] = {0x0055, 0x0045, 0};
    UCollator * someCollators [CLONETEST_COLLATOR_COUNT];
    UCollator * someClonedCollators [CLONETEST_COLLATOR_COUNT];
    UCollator * col;
    UErrorCode err = U_ZERO_ERROR;
    int8_t idx = 6;    /* Leave this here to test buffer alingment in memory*/
    uint8_t buffer [CLONETEST_COLLATOR_COUNT] [U_COL_SAFECLONE_BUFFERSIZE];
    int32_t bufferSize = U_COL_SAFECLONE_BUFFERSIZE;
    const char sampleRuleChars[] = "&Z < CH";
    UChar sampleRule[sizeof(sampleRuleChars)];

    u_uastrcpy(test1, "abCda");
    u_uastrcpy(test2, "abcda");
    u_uastrcpy(sampleRule, sampleRuleChars);

    /* one default collator & two complex ones */
    someCollators[0] = ucol_open("en_US", &err);
    someCollators[1] = ucol_open("ko", &err);
    someCollators[2] = ucol_open("ja_JP", &err);
    someCollators[3] = ucol_openRules(sampleRule, -1, UCOL_ON, UCOL_TERTIARY, NULL, &err);
    if(U_FAILURE(err)) {
        for (idx = 0; idx < CLONETEST_COLLATOR_COUNT; idx++) {
            ucol_close(someCollators[idx]);
        }
        log_data_err("Couldn't open one or more collators\n");
        return;
    }

    /* Check the various error & informational states: */

    /* Null status - just returns NULL */
    if (NULL != ucol_safeClone(someCollators[0], buffer[0], &bufferSize, NULL))
    {
        log_err("FAIL: Cloned Collator failed to deal correctly with null status\n");
    }
    /* error status - should return 0 & keep error the same */
    err = U_MEMORY_ALLOCATION_ERROR;
    if (NULL != ucol_safeClone(someCollators[0], buffer[0], &bufferSize, &err) || err != U_MEMORY_ALLOCATION_ERROR)
    {
        log_err("FAIL: Cloned Collator failed to deal correctly with incoming error status\n");
    }
    err = U_ZERO_ERROR;

    /* Null buffer size pointer is ok */
    if (NULL == (col = ucol_safeClone(someCollators[0], buffer[0], NULL, &err)) || U_FAILURE(err))
    {
        log_err("FAIL: Cloned Collator failed to deal correctly with null bufferSize pointer\n");
    }
    ucol_close(col);
    err = U_ZERO_ERROR;

    /* buffer size pointer is 0 - fill in pbufferSize with a size */
    bufferSize = 0;
    if (NULL != ucol_safeClone(someCollators[0], buffer[0], &bufferSize, &err) ||
            U_FAILURE(err) || bufferSize <= 0)
    {
        log_err("FAIL: Cloned Collator failed a sizing request ('preflighting')\n");
    }
    /* Verify our define is large enough  */
    if (U_COL_SAFECLONE_BUFFERSIZE < bufferSize)
    {
        log_err("FAIL: Pre-calculated buffer size is too small\n");
    }
    /* Verify we can use this run-time calculated size */
    if (NULL == (col = ucol_safeClone(someCollators[0], buffer[0], &bufferSize, &err)) || U_FAILURE(err))
    {
        log_err("FAIL: Collator can't be cloned with run-time size\n");
    }
    if (col) ucol_close(col);
    /* size one byte too small - should allocate & let us know */
    if (bufferSize > 1) {
        --bufferSize;
    }
    if (NULL == (col = ucol_safeClone(someCollators[0], 0, &bufferSize, &err)) || err != U_SAFECLONE_ALLOCATED_WARNING)
    {
        log_err("FAIL: Cloned Collator failed to deal correctly with too-small buffer size\n");
    }
    if (col) ucol_close(col);
    err = U_ZERO_ERROR;
    bufferSize = U_COL_SAFECLONE_BUFFERSIZE;


    /* Null buffer pointer - return Collator & set error to U_SAFECLONE_ALLOCATED_ERROR */
    if (NULL == (col = ucol_safeClone(someCollators[0], 0, &bufferSize, &err)) || err != U_SAFECLONE_ALLOCATED_WARNING)
    {
        log_err("FAIL: Cloned Collator failed to deal correctly with null buffer pointer\n");
    }
    if (col) ucol_close(col);
    err = U_ZERO_ERROR;

    /* Null Collator - return NULL & set U_ILLEGAL_ARGUMENT_ERROR */
    if (NULL != ucol_safeClone(NULL, buffer[0], &bufferSize, &err) || err != U_ILLEGAL_ARGUMENT_ERROR)
    {
        log_err("FAIL: Cloned Collator failed to deal correctly with null Collator pointer\n");
    }

    err = U_ZERO_ERROR;

    /* Test that a cloned collator doesn't accidentally use UCA. */
    col=ucol_open("de@collation=phonebook", &err);
    bufferSize = U_COL_SAFECLONE_BUFFERSIZE;
    someClonedCollators[0] = ucol_safeClone(col, buffer[0], &bufferSize, &err);
    doAssert( (ucol_greater(col, umlautUStr, u_strlen(umlautUStr), oeStr, u_strlen(oeStr))), "Original German phonebook collation sorts differently than expected");
    doAssert( (ucol_greater(someClonedCollators[0], umlautUStr, u_strlen(umlautUStr), oeStr, u_strlen(oeStr))), "Cloned German phonebook collation sorts differently than expected");
    if (!ucol_equals(someClonedCollators[0], col)) {
        log_err("FAIL: Cloned German phonebook collator is not equal to original.\n");
    }
    ucol_close(col);
    ucol_close(someClonedCollators[0]);

    err = U_ZERO_ERROR;

    /* change orig & clone & make sure they are independent */

    for (idx = 0; idx < CLONETEST_COLLATOR_COUNT; idx++)
    {
        ucol_setStrength(someCollators[idx], UCOL_IDENTICAL);
        bufferSize = 1;
        err = U_ZERO_ERROR;
        ucol_close(ucol_safeClone(someCollators[idx], buffer[idx], &bufferSize, &err));
        if (err != U_SAFECLONE_ALLOCATED_WARNING) {
            log_err("FAIL: collator number %d was not allocated.\n", idx);
            log_err("FAIL: status of Collator[%d] is %d  (hex: %x).\n", idx, err, err);
        }

        bufferSize = U_COL_SAFECLONE_BUFFERSIZE;
        err = U_ZERO_ERROR;
        someClonedCollators[idx] = ucol_safeClone(someCollators[idx], buffer[idx], &bufferSize, &err);
        if (U_FAILURE(err)) {
            log_err("FAIL: Unable to clone collator %d - %s\n", idx, u_errorName(err));
            continue;
        }
        if (!ucol_equals(someClonedCollators[idx], someCollators[idx])) {
            log_err("FAIL: Cloned collator is not equal to original at index = %d.\n", idx);
        }

        /* Check the usability */
        ucol_setStrength(someCollators[idx], UCOL_PRIMARY);
        ucol_setAttribute(someCollators[idx], UCOL_CASE_LEVEL, UCOL_OFF, &err);

        doAssert( (ucol_equal(someCollators[idx], test1, u_strlen(test1), test2, u_strlen(test2))), "Result should be \"abcda\" == \"abCda\"");

        /* Close the original to make sure that the clone is usable. */
        ucol_close(someCollators[idx]);

        ucol_setStrength(someClonedCollators[idx], UCOL_TERTIARY);
        ucol_setAttribute(someClonedCollators[idx], UCOL_CASE_LEVEL, UCOL_OFF, &err);
        doAssert( (ucol_greater(someClonedCollators[idx], test1, u_strlen(test1), test2, u_strlen(test2))), "Result should be \"abCda\" >>> \"abcda\" ");

        ucol_close(someClonedCollators[idx]);
    }
}

void TestCloneBinary(){
    UErrorCode err = U_ZERO_ERROR;
    UCollator * col = ucol_open("en_US", &err);
    UCollator * c;
    int32_t size;
    uint8_t * buffer;

    if (U_FAILURE(err)) {
        log_data_err("Couldn't open collator. Error: %s\n", u_errorName(err));
        return;
    }

    size = ucol_cloneBinary(col, NULL, 0, &err);
    if(size==0 || err!=U_BUFFER_OVERFLOW_ERROR) {
        log_err("ucol_cloneBinary - couldn't check size. Error: %s\n", u_errorName(err));
        return;
    }
    err = U_ZERO_ERROR;

    buffer = (uint8_t *) malloc(size);
    ucol_cloneBinary(col, buffer, size, &err);
    if(U_FAILURE(err)) {
        log_err("ucol_cloneBinary - couldn't clone.. Error: %s\n", u_errorName(err));
        free(buffer);
        return;
    }

    /* how to check binary result ? */

    c = ucol_openBinary(buffer, size, col, &err);
    if(U_FAILURE(err)) {
        log_err("ucol_openBinary failed. Error: %s\n", u_errorName(err));
    } else {
        UChar t[] = {0x41, 0x42, 0x43, 0};  /* ABC */
        uint8_t  *k1, *k2;
        int l1, l2;
        l1 = ucol_getSortKey(col, t, -1, NULL,0);
        l2 = ucol_getSortKey(c, t, -1, NULL,0);
        k1 = (uint8_t *) malloc(sizeof(uint8_t) * l1);
        k2 = (uint8_t *) malloc(sizeof(uint8_t) * l2);
        ucol_getSortKey(col, t, -1, k1, l1);
        ucol_getSortKey(col, t, -1, k2, l2);
        if (strcmp((char *)k1,(char *)k2) != 0){
            log_err("ucol_openBinary - new collator should equal to old one\n");
        };
        free(k1);
        free(k2);
    }
    free(buffer);
    ucol_close(c);
    ucol_close(col);
}


static void TestBengaliSortKey(void)
{
  const char *curLoc = "bn";
  UChar str1[] = { 0x09BE, 0 };
  UChar str2[] = { 0x0B70, 0 };
  UCollator *c2 = NULL;
  const UChar *rules;
  int32_t rulesLength=-1;
  uint8_t *sortKey1;
  int32_t sortKeyLen1 = 0;
  uint8_t *sortKey2;
  int32_t sortKeyLen2 = 0;
  UErrorCode status = U_ZERO_ERROR;
  char sortKeyStr1[2048];
  uint32_t sortKeyStrLen1 = UPRV_LENGTHOF(sortKeyStr1);
  char sortKeyStr2[2048];
  uint32_t sortKeyStrLen2 = UPRV_LENGTHOF(sortKeyStr2);
  UCollationResult result;

  static UChar preRules[41] = { 0x26, 0x9fa, 0x3c, 0x98c, 0x3c, 0x9e1, 0x3c, 0x98f, 0x3c, 0x990, 0x3c, 0x993, 0x3c, 0x994, 0x3c, 0x9bc, 0x3c, 0x982, 0x3c, 0x983, 0x3c, 0x981, 0x3c, 0x9b0, 0x3c, 0x9b8, 0x3c, 0x9b9, 0x3c, 0x9bd, 0x3c, 0x9be, 0x3c, 0x9bf, 0x3c, 0x9c8, 0x3c, 0x9cb, 0x3d, 0x9cb , 0};

  rules = preRules;
  
  log_verbose("Rules: %s\n", aescstrdup(rules, rulesLength));

  c2 = ucol_openRules(rules, rulesLength, UCOL_DEFAULT, UCOL_DEFAULT_STRENGTH, NULL, &status);
  if (U_FAILURE(status)) {
    log_data_err("ERROR: Creating collator from rules failed with locale: %s : %s\n", curLoc, myErrorName(status));
    return;
  }
  
  sortKeyLen1 = ucol_getSortKey(c2, str1, -1, NULL, 0);
  sortKey1 = (uint8_t*)malloc(sortKeyLen1+1);
  ucol_getSortKey(c2,str1,-1,sortKey1, sortKeyLen1+1);
  ucol_sortKeyToString(c2, sortKey1, sortKeyStr1, sortKeyStrLen1);
  
  
  sortKeyLen2 = ucol_getSortKey(c2, str2, -1, NULL, 0);
  sortKey2 = (uint8_t*)malloc(sortKeyLen2+1);
  ucol_getSortKey(c2,str2,-1,sortKey2, sortKeyLen2+1);

  ucol_sortKeyToString(c2, sortKey2, sortKeyStr2, sortKeyStrLen2);



  result=ucol_strcoll(c2, str1, -1, str2, -1);
  if(result!=UCOL_LESS) {
    log_err("Error: %s was not less than %s: result=%d.\n", aescstrdup(str1,-1), aescstrdup(str2,-1), result);
    log_info("[%s] -> %s (%d, from rule)\n", aescstrdup(str1,-1), sortKeyStr1, sortKeyLen1);
    log_info("[%s] -> %s (%d, from rule)\n", aescstrdup(str2,-1), sortKeyStr2, sortKeyLen2);
  } else {
    log_verbose("OK: %s was  less than %s: result=%d.\n", aescstrdup(str1,-1), aescstrdup(str2,-1), result);
    log_verbose("[%s] -> %s (%d, from rule)\n", aescstrdup(str1,-1), sortKeyStr1, sortKeyLen1);
    log_verbose("[%s] -> %s (%d, from rule)\n", aescstrdup(str2,-1), sortKeyStr2, sortKeyLen2);
  }

  free(sortKey1);
  free(sortKey2);
  ucol_close(c2);

}

/*
    TestOpenVsOpenRules ensures that collators from ucol_open and ucol_openRules
    will generate identical sort keys
*/
void TestOpenVsOpenRules(){

    /* create an array of all the locales */
    int32_t numLocales = uloc_countAvailable();
    int32_t sizeOfStdSet;
    uint32_t adder;
    UChar str[41]; /* create an array of UChar of size maximum strSize + 1 */
    USet *stdSet;
    char* curLoc;
    UCollator * c1;
    UCollator * c2;
    const UChar* rules;
    int32_t rulesLength;
    int32_t sortKeyLen1, sortKeyLen2;
    uint8_t *sortKey1 = NULL, *sortKey2 = NULL;
    char sortKeyStr1[512], sortKeyStr2[512];
    uint32_t sortKeyStrLen1 = UPRV_LENGTHOF(sortKeyStr1),
             sortKeyStrLen2 = UPRV_LENGTHOF(sortKeyStr2);
    ULocaleData *uld;
    int32_t x, y, z;
    USet *eSet;
    int32_t eSize;
    int strSize;

    UErrorCode err = U_ZERO_ERROR;

    /* create a set of standard characters that aren't very interesting...
    and then we can find some interesting ones later */

    stdSet = uset_open(0x61, 0x7A);
    uset_addRange(stdSet, 0x41, 0x5A);
    uset_addRange(stdSet, 0x30, 0x39);
    sizeOfStdSet = uset_size(stdSet);
    (void)sizeOfStdSet;   /* Suppress set but not used warning. */

    adder = 1;
    if(getTestOption(QUICK_OPTION))
    {
        adder = 10;
    }

    for(x = 0; x < numLocales; x+=adder){
        curLoc = (char *)uloc_getAvailable(x);
        log_verbose("Processing %s\n", curLoc);

        /* create a collator the normal API way */
        c1 = ucol_open(curLoc, &err);
        if (U_FAILURE(err)) {
            log_err("ERROR: Normal collation creation failed with locale: %s : %s\n", curLoc, myErrorName(err));
            return;
        }

        /* grab the rules */
        rules = ucol_getRules(c1, &rulesLength);
        if (rulesLength == 0) {
            /* The optional tailoring rule string is either empty (boring) or missing. */
            ucol_close(c1);
            continue;
        }

        /* use those rules to create a collator from rules */
        c2 = ucol_openRules(rules, rulesLength, UCOL_DEFAULT, UCOL_DEFAULT_STRENGTH, NULL, &err);
        if (U_FAILURE(err)) {
            log_err("ERROR: Creating collator from rules failed with locale: %s : %s\n", curLoc, myErrorName(err));
            ucol_close(c1);
            continue;
        }

        uld = ulocdata_open(curLoc, &err);

        /*now that we have some collators, we get several strings */

        for(y = 0; y < 5; y++){

            /* get a set of ALL the characters in this locale */
            eSet =  ulocdata_getExemplarSet(uld, NULL, 0, ULOCDATA_ES_STANDARD, &err);
            eSize = uset_size(eSet);

            /* make a string with these characters in it */
            strSize = (rand()%40) + 1;

            for(z = 0; z < strSize; z++){
                str[z] = uset_charAt(eSet, rand()%eSize);
            }

            /* change the set to only include 'abnormal' characters (not A-Z, a-z, 0-9 */
            uset_removeAll(eSet, stdSet);
            eSize = uset_size(eSet);

            /* if there are some non-normal characters left, put a few into the string, just to make sure we have some */
            if(eSize > 0){
                str[2%strSize] = uset_charAt(eSet, rand()%eSize);
                str[3%strSize] = uset_charAt(eSet, rand()%eSize);
                str[5%strSize] = uset_charAt(eSet, rand()%eSize);
                str[10%strSize] = uset_charAt(eSet, rand()%eSize);
                str[13%strSize] = uset_charAt(eSet, rand()%eSize);
            }
            /* terminate the string */
            str[strSize-1] = '\0';
            log_verbose("String used: %S\n", str);

            /* get sort keys for both of them, and check that the keys are identicle */
            sortKeyLen1 = ucol_getSortKey(c1, str, u_strlen(str),  NULL, 0);
            sortKey1 = (uint8_t*)malloc(sizeof(uint8_t) * (sortKeyLen1 + 1));
            /*memset(sortKey1, 0xFE, sortKeyLen1);*/
            ucol_getSortKey(c1, str, u_strlen(str), sortKey1, sortKeyLen1 + 1);
            ucol_sortKeyToString(c1, sortKey1, sortKeyStr1, sortKeyStrLen1);

            sortKeyLen2 = ucol_getSortKey(c2, str, u_strlen(str),  NULL, 0);
            sortKey2 = (uint8_t*)malloc(sizeof(uint8_t) * (sortKeyLen2 + 1));
            /*memset(sortKey2, 0xFE, sortKeyLen2);*/
            ucol_getSortKey(c2, str, u_strlen(str), sortKey2, sortKeyLen2 + 1);
            ucol_sortKeyToString(c2, sortKey2, sortKeyStr2, sortKeyStrLen2);

            /* Check that the lengths are the same */
            if (sortKeyLen1 != sortKeyLen2) {
                log_err("ERROR : Sort key lengths %d and %d for text '%s' in locale '%s' do not match.\n",
                    sortKeyLen1, sortKeyLen2, str, curLoc);
            }

            /* check that the keys are the same */
            if (memcmp(sortKey1, sortKey2, sortKeyLen1) != 0) {
                log_err("ERROR : Sort keys '%s' and '%s' for text '%s' in locale '%s' are not equivalent.\n",
                    sortKeyStr1, sortKeyStr2, str, curLoc);
            }

            /* clean up after each string */
            free(sortKey1);
            free(sortKey2);
            uset_close(eSet);
        }
        /* clean up after each locale */
        ulocdata_close(uld);
        ucol_close(c1);
        ucol_close(c2);
    }
    /* final clean up */
    uset_close(stdSet);
}
/*
----------------------------------------------------------------------------
 ctor -- Tests the getSortKey
*/
void TestSortKey()
{
    uint8_t *sortk1 = NULL, *sortk2 = NULL, *sortk3 = NULL, *sortkEmpty = NULL;
    int32_t sortklen, osortklen;
    UCollator *col;
    UChar *test1, *test2, *test3;
    UErrorCode status = U_ZERO_ERROR;
    char toStringBuffer[256], *resultP;
    uint32_t toStringLen=UPRV_LENGTHOF(toStringBuffer);


    uint8_t s1[] = { 0x9f, 0x00 };
    uint8_t s2[] = { 0x61, 0x00 };
    int  strcmpResult;

    strcmpResult = strcmp((const char *)s1, (const char *)s2);
    log_verbose("strcmp(0x9f..., 0x61...) = %d\n", strcmpResult);

    if(strcmpResult <= 0) {
      log_err("ERR: expected strcmp(\"9f 00\", \"61 00\") to be >=0 (GREATER).. got %d. Calling strcmp() for sortkeys may not work! \n",
              strcmpResult);
    }


    log_verbose("testing SortKey begins...\n");
    /* this is supposed to open default date format, but later on it treats it like it is "en_US"
       - very bad if you try to run the tests on machine where default locale is NOT "en_US" */
    /* col = ucol_open(NULL, &status); */
    col = ucol_open("en_US", &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: Default collation creation failed.: %s\n", myErrorName(status));
        return;
    }


    if(ucol_getStrength(col) != UCOL_DEFAULT_STRENGTH)
    {
        log_err("ERROR: default collation did not have UCOL_DEFAULT_STRENGTH !\n");
    }
    /* Need to use identical strength */
    ucol_setAttribute(col, UCOL_STRENGTH, UCOL_IDENTICAL, &status);

    test1=(UChar*)malloc(sizeof(UChar) * 6);
    test2=(UChar*)malloc(sizeof(UChar) * 6);
    test3=(UChar*)malloc(sizeof(UChar) * 6);

    memset(test1,0xFE, sizeof(UChar)*6);
    memset(test2,0xFE, sizeof(UChar)*6);
    memset(test3,0xFE, sizeof(UChar)*6);


    u_uastrcpy(test1, "Abcda");
    u_uastrcpy(test2, "abcda");
    u_uastrcpy(test3, "abcda");

    log_verbose("Use tertiary comparison level testing ....\n");

    sortklen=ucol_getSortKey(col, test1, u_strlen(test1),  NULL, 0);
    sortk1=(uint8_t*)malloc(sizeof(uint8_t) * (sortklen+1));
    memset(sortk1,0xFE, sortklen);
    ucol_getSortKey(col, test1, u_strlen(test1), sortk1, sortklen+1);

    sortklen=ucol_getSortKey(col, test2, u_strlen(test2),  NULL, 0);
    sortk2=(uint8_t*)malloc(sizeof(uint8_t) * (sortklen+1));
    memset(sortk2,0xFE, sortklen);
    ucol_getSortKey(col, test2, u_strlen(test2), sortk2, sortklen+1);

    osortklen = sortklen;
    sortklen=ucol_getSortKey(col, test2, u_strlen(test3),  NULL, 0);
    sortk3=(uint8_t*)malloc(sizeof(uint8_t) * (sortklen+1));
    memset(sortk3,0xFE, sortklen);
    ucol_getSortKey(col, test2, u_strlen(test2), sortk3, sortklen+1);

    doAssert( (sortklen == osortklen), "Sortkey length should be the same (abcda, abcda)");

    doAssert( (memcmp(sortk1, sortk2, sortklen) > 0), "Result should be \"Abcda\" > \"abcda\"");
    doAssert( (memcmp(sortk2, sortk1, sortklen) < 0), "Result should be \"abcda\" < \"Abcda\"");
    doAssert( (memcmp(sortk2, sortk3, sortklen) == 0), "Result should be \"abcda\" ==  \"abcda\"");

    resultP = ucol_sortKeyToString(col, sortk3, toStringBuffer, toStringLen);
    doAssert( (resultP != 0), "sortKeyToString failed!");

#if 1 /* verobse log of sortkeys */
    {
      char junk2[1000];
      char junk3[1000];
      int i;

      strcpy(junk2, "abcda[2] ");
      strcpy(junk3, " abcda[3] ");

      for(i=0;i<sortklen;i++)
        {
          sprintf(junk2+strlen(junk2), "%02X ",(int)( 0xFF & sortk2[i]));
          sprintf(junk3+strlen(junk3), "%02X ",(int)( 0xFF & sortk3[i]));
        }

      log_verbose("%s\n", junk2);
      log_verbose("%s\n", junk3);
    }
#endif

    free(sortk1);
    free(sortk2);
    free(sortk3);

    log_verbose("Use secondary comparision level testing ...\n");
    ucol_setStrength(col, UCOL_SECONDARY);
    sortklen=ucol_getSortKey(col, test1, u_strlen(test1),  NULL, 0);
    sortk1=(uint8_t*)malloc(sizeof(uint8_t) * (sortklen+1));
    ucol_getSortKey(col, test1, u_strlen(test1), sortk1, sortklen+1);
    sortklen=ucol_getSortKey(col, test2, u_strlen(test2),  NULL, 0);
    sortk2=(uint8_t*)malloc(sizeof(uint8_t) * (sortklen+1));
    ucol_getSortKey(col, test2, u_strlen(test2), sortk2, sortklen+1);

    doAssert( !(memcmp(sortk1, sortk2, sortklen) > 0), "Result should be \"Abcda\" == \"abcda\"");
    doAssert( !(memcmp(sortk2, sortk1, sortklen) < 0), "Result should be \"abcda\" == \"Abcda\"");
    doAssert( (memcmp(sortk1, sortk2, sortklen) == 0), "Result should be \"abcda\" ==  \"abcda\"");

    log_verbose("getting sortkey for an empty string\n");
    ucol_setAttribute(col, UCOL_STRENGTH, UCOL_TERTIARY, &status);
    sortklen = ucol_getSortKey(col, test1, 0, NULL, 0);
    sortkEmpty = (uint8_t*)malloc(sizeof(uint8_t) * sortklen+1);
    sortklen = ucol_getSortKey(col, test1, 0, sortkEmpty, sortklen+1);
    if(sortklen != 3 || sortkEmpty[0] != 1 || sortkEmpty[0] != 1 || sortkEmpty[2] != 0) {
      log_err("Empty string generated wrong sortkey!\n");
    }
    free(sortkEmpty);

    log_verbose("testing passing invalid string\n");
    sortklen = ucol_getSortKey(col, NULL, 10, NULL, 0);
    if(sortklen != 0) {
      log_err("Invalid string didn't return sortkey size of 0\n");
    }


    log_verbose("testing sortkey ends...\n");
    ucol_close(col);
    free(test1);
    free(test2);
    free(test3);
    free(sortk1);
    free(sortk2);

}
void TestHashCode()
{
    uint8_t *sortk1, *sortk2, *sortk3;
    int32_t sortk1len, sortk2len, sortk3len;
    UCollator *col;
    UChar *test1, *test2, *test3;
    UErrorCode status = U_ZERO_ERROR;
    log_verbose("testing getHashCode begins...\n");
    col = ucol_open("en_US", &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: Default collation creation failed.: %s\n", myErrorName(status));
        return;
    }
    test1=(UChar*)malloc(sizeof(UChar) * 6);
    test2=(UChar*)malloc(sizeof(UChar) * 6);
    test3=(UChar*)malloc(sizeof(UChar) * 6);
    u_uastrcpy(test1, "Abcda");
    u_uastrcpy(test2, "abcda");
    u_uastrcpy(test3, "abcda");

    log_verbose("Use tertiary comparison level testing ....\n");
    sortk1len=ucol_getSortKey(col, test1, u_strlen(test1),  NULL, 0);
    sortk1=(uint8_t*)malloc(sizeof(uint8_t) * (sortk1len+1));
    ucol_getSortKey(col, test1, u_strlen(test1), sortk1, sortk1len+1);
    sortk2len=ucol_getSortKey(col, test2, u_strlen(test2),  NULL, 0);
    sortk2=(uint8_t*)malloc(sizeof(uint8_t) * (sortk2len+1));
    ucol_getSortKey(col, test2, u_strlen(test2), sortk2, sortk2len+1);
    sortk3len=ucol_getSortKey(col, test2, u_strlen(test3),  NULL, 0);
    sortk3=(uint8_t*)malloc(sizeof(uint8_t) * (sortk3len+1));
    ucol_getSortKey(col, test2, u_strlen(test2), sortk3, sortk3len+1);


    log_verbose("ucol_hashCode() testing ...\n");

    doAssert( ucol_keyHashCode(sortk1, sortk1len) != ucol_keyHashCode(sortk2, sortk2len), "Hash test1 result incorrect" );
    doAssert( !(ucol_keyHashCode(sortk1, sortk1len) == ucol_keyHashCode(sortk2, sortk2len)), "Hash test2 result incorrect" );
    doAssert( ucol_keyHashCode(sortk2, sortk2len) == ucol_keyHashCode(sortk3, sortk3len), "Hash result not equal" );

    log_verbose("hashCode tests end.\n");
    ucol_close(col);
    free(sortk1);
    free(sortk2);
    free(sortk3);
    free(test1);
    free(test2);
    free(test3);


}
/*
 *----------------------------------------------------------------------------
 * Tests the UCollatorElements API.
 *
 */
void TestElemIter()
{
    int32_t offset;
    int32_t order1, order2, order3;
    UChar *testString1, *testString2;
    UCollator *col;
    UCollationElements *iterator1, *iterator2, *iterator3;
    UErrorCode status = U_ZERO_ERROR;
    log_verbose("testing UCollatorElements begins...\n");
    col = ucol_open("en_US", &status);
    ucol_setAttribute(col, UCOL_NORMALIZATION_MODE, UCOL_OFF, &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "ERROR: Default collation creation failed.: %s\n", myErrorName(status));
        return;
    }

    testString1=(UChar*)malloc(sizeof(UChar) * 150);
    testString2=(UChar*)malloc(sizeof(UChar) * 150);
    u_uastrcpy(testString1, "XFILE What subset of all possible test cases has the highest probability of detecting the most errors?");
    u_uastrcpy(testString2, "Xf_ile What subset of all possible test cases has the lowest probability of detecting the least errors?");

    log_verbose("Constructors and comparison testing....\n");

    iterator1 = ucol_openElements(col, testString1, u_strlen(testString1), &status);
    if(U_FAILURE(status)) {
        log_err("ERROR: Default collationElement iterator creation failed.: %s\n", myErrorName(status));
        ucol_close(col);
        return;
    }
    else{ log_verbose("PASS: Default collationElement iterator1 creation passed\n");}

    iterator2 = ucol_openElements(col, testString1, u_strlen(testString1), &status);
    if(U_FAILURE(status)) {
        log_err("ERROR: Default collationElement iterator creation failed.: %s\n", myErrorName(status));
        ucol_close(col);
        return;
    }
    else{ log_verbose("PASS: Default collationElement iterator2 creation passed\n");}

    iterator3 = ucol_openElements(col, testString2, u_strlen(testString2), &status);
    if(U_FAILURE(status)) {
        log_err("ERROR: Default collationElement iterator creation failed.: %s\n", myErrorName(status));
        ucol_close(col);
        return;
    }
    else{ log_verbose("PASS: Default collationElement iterator3 creation passed\n");}

    offset=ucol_getOffset(iterator1);
    (void)offset;   /* Suppress set but not used warning. */
    ucol_setOffset(iterator1, 6, &status);
    if (U_FAILURE(status)) {
        log_err("Error in setOffset for UCollatorElements iterator.: %s\n", myErrorName(status));
        return;
    }
    if(ucol_getOffset(iterator1)==6)
        log_verbose("setOffset and getOffset working fine\n");
    else{
        log_err("error in set and get Offset got %d instead of 6\n", ucol_getOffset(iterator1));
    }

    ucol_setOffset(iterator1, 0, &status);
    order1 = ucol_next(iterator1, &status);
    if (U_FAILURE(status)) {
        log_err("Somehow ran out of memory stepping through the iterator1.: %s\n", myErrorName(status));
        return;
    }
    order2=ucol_getOffset(iterator2);
    doAssert((order1 != order2), "The first iterator advance failed");
    order2 = ucol_next(iterator2, &status);
    if (U_FAILURE(status)) {
        log_err("Somehow ran out of memory stepping through the iterator2.: %s\n", myErrorName(status));
        return;
    }
    order3 = ucol_next(iterator3, &status);
    if (U_FAILURE(status)) {
        log_err("Somehow ran out of memory stepping through the iterator3.: %s\n", myErrorName(status));
        return;
    }

    doAssert((order1 == order2), "The second iterator advance failed should be the same as first one");

doAssert( (ucol_primaryOrder(order1) == ucol_primaryOrder(order3)), "The primary orders should be identical");
doAssert( (ucol_secondaryOrder(order1) == ucol_secondaryOrder(order3)), "The secondary orders should be identical");
doAssert( (ucol_tertiaryOrder(order1) == ucol_tertiaryOrder(order3)), "The tertiary orders should be identical");

    order1=ucol_next(iterator1, &status);
    if (U_FAILURE(status)) {
        log_err("Somehow ran out of memory stepping through the iterator2.: %s\n", myErrorName(status));
        return;
    }
    order3=ucol_next(iterator3, &status);
    if (U_FAILURE(status)) {
        log_err("Somehow ran out of memory stepping through the iterator2.: %s\n", myErrorName(status));
        return;
    }
doAssert( (ucol_primaryOrder(order1) == ucol_primaryOrder(order3)), "The primary orders should be identical");
doAssert( (ucol_tertiaryOrder(order1) != ucol_tertiaryOrder(order3)), "The tertiary orders should be different");

    order1=ucol_next(iterator1, &status);
    if (U_FAILURE(status)) {
        log_err("Somehow ran out of memory stepping through the iterator2.: %s\n", myErrorName(status));
        return;
    }
    order3=ucol_next(iterator3, &status);
    if (U_FAILURE(status)) {
        log_err("Somehow ran out of memory stepping through the iterator2.: %s\n", myErrorName(status));
        return;
    }
    /* this here, my friends, is either pure lunacy or something so obsolete that even it's mother
     * doesn't care about it. Essentialy, this test complains if secondary values for 'I' and '_'
     * are the same. According to the UCA, this is not true. Therefore, remove the test.
     * Besides, if primary strengths for two code points are different, it doesn't matter one bit
     * what is the relation between secondary or any other strengths.
     * killed by weiv 06/11/2002.
     */
    /*
    doAssert( ((order1 & UCOL_SECONDARYMASK) != (order3 & UCOL_SECONDARYMASK)), "The secondary orders should be different");
    */
    doAssert( (order1 != UCOL_NULLORDER), "Unexpected end of iterator reached");

    free(testString1);
    free(testString2);
    ucol_closeElements(iterator1);
    ucol_closeElements(iterator2);
    ucol_closeElements(iterator3);
    ucol_close(col);

    log_verbose("testing CollationElementIterator ends...\n");
}

void TestGetLocale() {
  UErrorCode status = U_ZERO_ERROR;
  const char *rules = "&a<x<y<z";
  UChar rlz[256] = {0};
  uint32_t rlzLen = u_unescape(rules, rlz, 256);

  UCollator *coll = NULL;
  const char *locale = NULL;

  int32_t i = 0;

  static const struct {
    const char* requestedLocale;
    const char* validLocale;
    const char* actualLocale;
  } testStruct[] = {
    { "sr_RS", "sr_Cyrl_RS", "sr" },
    { "sh_YU", "sr_Latn_RS", "sr_Latn" }, /* was sh, then aliased to hr, now sr_Latn via import per cldrbug 5647: */
    { "en_BE_FOO", "en", "root" },
    { "sv_SE_NONEXISTANT", "sv", "sv" }
  };

  /* test opening collators for different locales */
  for(i = 0; i<UPRV_LENGTHOF(testStruct); i++) {
    status = U_ZERO_ERROR;
    coll = ucol_open(testStruct[i].requestedLocale, &status);
    if(U_FAILURE(status)) {
      log_err_status(status, "Failed to open collator for %s with %s\n", testStruct[i].requestedLocale, u_errorName(status));
      ucol_close(coll);
      continue;
    }
    /*
     * The requested locale may be the same as the valid locale,
     * or may not be supported at all. See ticket #10477.
     */
    locale = ucol_getLocaleByType(coll, ULOC_REQUESTED_LOCALE, &status);
    if(U_SUCCESS(status) &&
          strcmp(locale, testStruct[i].requestedLocale) != 0 && strcmp(locale, testStruct[i].validLocale) != 0) {
      log_err("[Coll %s]: Error in requested locale, expected %s, got %s\n", testStruct[i].requestedLocale, testStruct[i].requestedLocale, locale);
    }
    status = U_ZERO_ERROR;
    locale = ucol_getLocaleByType(coll, ULOC_VALID_LOCALE, &status);
    if(strcmp(locale, testStruct[i].validLocale) != 0) {
      log_err("[Coll %s]: Error in valid locale, expected %s, got %s\n", testStruct[i].requestedLocale, testStruct[i].validLocale, locale);
    }
    locale = ucol_getLocaleByType(coll, ULOC_ACTUAL_LOCALE, &status);
    if(strcmp(locale, testStruct[i].actualLocale) != 0) {
      log_err("[Coll %s]: Error in actual locale, expected %s, got %s\n", testStruct[i].requestedLocale, testStruct[i].actualLocale, locale);
    }
    ucol_close(coll);
  }

  /* completely non-existent locale for collator should get a root collator */
  {
    UCollator *defaultColl = ucol_open(NULL, &status);
    coll = ucol_open("blahaha", &status);
    if(U_SUCCESS(status)) {
      /* See comment above about ticket #10477.
      if(strcmp(ucol_getLocaleByType(coll, ULOC_REQUESTED_LOCALE, &status), "blahaha")) {
        log_err("Nonexisting locale didn't preserve the requested locale\n");
      } */
      const char *name = ucol_getLocaleByType(coll, ULOC_VALID_LOCALE, &status);
      if(*name != 0 && strcmp(name, "root") != 0) {
        log_err("Valid locale for nonexisting-locale collator is \"%s\" not root\n", name);
      }
      name = ucol_getLocaleByType(coll, ULOC_ACTUAL_LOCALE, &status);
      if(*name != 0 && strcmp(name, "root") != 0) {
        log_err("Actual locale for nonexisting-locale collator is \"%s\" not root\n", name);
      }
      ucol_close(coll);
      ucol_close(defaultColl);
    } else {
      log_data_err("Couldn't open collators\n");
    }
  }



  /* collator instantiated from rules should have all three locales NULL */
  coll = ucol_openRules(rlz, rlzLen, UCOL_DEFAULT, UCOL_DEFAULT, NULL, &status);
  if (coll != NULL) {
    locale = ucol_getLocaleByType(coll, ULOC_REQUESTED_LOCALE, &status);
    if(U_SUCCESS(status) && locale != NULL) {
      log_err("For collator instantiated from rules, requested locale returned %s instead of NULL\n", locale);
    }
    status = U_ZERO_ERROR;
    locale = ucol_getLocaleByType(coll, ULOC_VALID_LOCALE, &status);
    if(locale != NULL) {
      log_err("For collator instantiated from rules,  valid locale returned %s instead of NULL\n", locale);
    }
    locale = ucol_getLocaleByType(coll, ULOC_ACTUAL_LOCALE, &status);
    if(locale != NULL) {
      log_err("For collator instantiated from rules, actual locale returned %s instead of NULL\n", locale);
    }
    ucol_close(coll);
  } else {
    log_data_err("Couldn't get collator from ucol_openRules() - %s\n", u_errorName(status));
  }
}


void TestGetAll()
{
    int32_t i, count;
    count=ucol_countAvailable();
    /* use something sensible w/o hardcoding the count */
    if(count < 0){
        log_err("Error in countAvailable(), it returned %d\n", count);
    }
    else{
        log_verbose("PASS: countAvailable() successful, it returned %d\n", count);
    }
    for(i=0;i<count;i++)
        log_verbose("%s\n", ucol_getAvailable(i));


}


struct teststruct {
    const char *original;
    uint8_t key[256];
} ;

static int compare_teststruct(const void *string1, const void *string2) {
    return(strcmp((const char *)((struct teststruct *)string1)->key, (const char *)((struct teststruct *)string2)->key));
}

void TestBounds() {
    UErrorCode status = U_ZERO_ERROR;

    UCollator *coll = ucol_open("sh", &status);

    uint8_t sortkey[512], lower[512], upper[512];
    UChar buffer[512];

    static const char * const test[] = {
        "John Smith",
        "JOHN SMITH",
        "john SMITH",
        "j\\u00F6hn sm\\u00EFth",
        "J\\u00F6hn Sm\\u00EFth",
        "J\\u00D6HN SM\\u00CFTH",
        "john smithsonian",
        "John Smithsonian",
    };

    struct teststruct tests[] = {
        {"\\u010CAKI MIHALJ" } ,
        {"\\u010CAKI MIHALJ" } ,
        {"\\u010CAKI PIRO\\u0160KA" },
        {"\\u010CABAI ANDRIJA" } ,
        {"\\u010CABAI LAJO\\u0160" } ,
        {"\\u010CABAI MARIJA" } ,
        {"\\u010CABAI STEVAN" } ,
        {"\\u010CABAI STEVAN" } ,
        {"\\u010CABARKAPA BRANKO" } ,
        {"\\u010CABARKAPA MILENKO" } ,
        {"\\u010CABARKAPA MIROSLAV" } ,
        {"\\u010CABARKAPA SIMO" } ,
        {"\\u010CABARKAPA STANKO" } ,
        {"\\u010CABARKAPA TAMARA" } ,
        {"\\u010CABARKAPA TOMA\\u0160" } ,
        {"\\u010CABDARI\\u0106 NIKOLA" } ,
        {"\\u010CABDARI\\u0106 ZORICA" } ,
        {"\\u010CABI NANDOR" } ,
        {"\\u010CABOVI\\u0106 MILAN" } ,
        {"\\u010CABRADI AGNEZIJA" } ,
        {"\\u010CABRADI IVAN" } ,
        {"\\u010CABRADI JELENA" } ,
        {"\\u010CABRADI LJUBICA" } ,
        {"\\u010CABRADI STEVAN" } ,
        {"\\u010CABRDA MARTIN" } ,
        {"\\u010CABRILO BOGDAN" } ,
        {"\\u010CABRILO BRANISLAV" } ,
        {"\\u010CABRILO LAZAR" } ,
        {"\\u010CABRILO LJUBICA" } ,
        {"\\u010CABRILO SPASOJA" } ,
        {"\\u010CADE\\u0160 ZDENKA" } ,
        {"\\u010CADESKI BLAGOJE" } ,
        {"\\u010CADOVSKI VLADIMIR" } ,
        {"\\u010CAGLJEVI\\u0106 TOMA" } ,
        {"\\u010CAGOROVI\\u0106 VLADIMIR" } ,
        {"\\u010CAJA VANKA" } ,
        {"\\u010CAJI\\u0106 BOGOLJUB" } ,
        {"\\u010CAJI\\u0106 BORISLAV" } ,
        {"\\u010CAJI\\u0106 RADOSLAV" } ,
        {"\\u010CAK\\u0160IRAN MILADIN" } ,
        {"\\u010CAKAN EUGEN" } ,
        {"\\u010CAKAN EVGENIJE" } ,
        {"\\u010CAKAN IVAN" } ,
        {"\\u010CAKAN JULIJAN" } ,
        {"\\u010CAKAN MIHAJLO" } ,
        {"\\u010CAKAN STEVAN" } ,
        {"\\u010CAKAN VLADIMIR" } ,
        {"\\u010CAKAN VLADIMIR" } ,
        {"\\u010CAKAN VLADIMIR" } ,
        {"\\u010CAKARA ANA" } ,
        {"\\u010CAKAREVI\\u0106 MOMIR" } ,
        {"\\u010CAKAREVI\\u0106 NEDELJKO" } ,
        {"\\u010CAKI \\u0160ANDOR" } ,
        {"\\u010CAKI AMALIJA" } ,
        {"\\u010CAKI ANDRA\\u0160" } ,
        {"\\u010CAKI LADISLAV" } ,
        {"\\u010CAKI LAJO\\u0160" } ,
        {"\\u010CAKI LASLO" } ,
    };



    int32_t i = 0, j = 0, k = 0, buffSize = 0, skSize = 0, lowerSize = 0, upperSize = 0;
    int32_t arraySize = UPRV_LENGTHOF(tests);

    if(U_SUCCESS(status) && coll) {
        for(i = 0; i<arraySize; i++) {
            buffSize = u_unescape(tests[i].original, buffer, 512);
            skSize = ucol_getSortKey(coll, buffer, buffSize, tests[i].key, 512);
        }

        qsort(tests, arraySize, sizeof(struct teststruct), compare_teststruct);

        for(i = 0; i < arraySize-1; i++) {
            for(j = i+1; j < arraySize; j++) {
                lowerSize = ucol_getBound(tests[i].key, -1, UCOL_BOUND_LOWER, 1, lower, 512, &status);
                upperSize = ucol_getBound(tests[j].key, -1, UCOL_BOUND_UPPER, 1, upper, 512, &status);
                (void)lowerSize;    /* Suppress set but not used warning. */
                (void)upperSize;
                for(k = i; k <= j; k++) {
                    if(strcmp((const char *)lower, (const char *)tests[k].key) > 0) {
                        log_err("Problem with lower! j = %i (%s vs %s)\n", k, tests[k].original, tests[i].original);
                    }
                    if(strcmp((const char *)upper, (const char *)tests[k].key) <= 0) {
                        log_err("Problem with upper! j = %i (%s vs %s)\n", k, tests[k].original, tests[j].original);
                    }
                }
            }
        }


#if 0
        for(i = 0; i < 1000; i++) {
            lowerRND = (rand()/(RAND_MAX/arraySize));
            upperRND = lowerRND + (rand()/(RAND_MAX/(arraySize-lowerRND)));

            lowerSize = ucol_getBound(tests[lowerRND].key, -1, UCOL_BOUND_LOWER, 1, lower, 512, &status);
            upperSize = ucol_getBound(tests[upperRND].key, -1, UCOL_BOUND_UPPER_LONG, 1, upper, 512, &status);

            for(j = lowerRND; j<=upperRND; j++) {
                if(strcmp(lower, tests[j].key) > 0) {
                    log_err("Problem with lower! j = %i (%s vs %s)\n", j, tests[j].original, tests[lowerRND].original);
                }
                if(strcmp(upper, tests[j].key) <= 0) {
                    log_err("Problem with upper! j = %i (%s vs %s)\n", j, tests[j].original, tests[upperRND].original);
                }
            }
        }
#endif





        for(i = 0; i<UPRV_LENGTHOF(test); i++) {
            buffSize = u_unescape(test[i], buffer, 512);
            skSize = ucol_getSortKey(coll, buffer, buffSize, sortkey, 512);
            lowerSize = ucol_getBound(sortkey, skSize, UCOL_BOUND_LOWER, 1, lower, 512, &status);
            upperSize = ucol_getBound(sortkey, skSize, UCOL_BOUND_UPPER_LONG, 1, upper, 512, &status);
            for(j = i+1; j<UPRV_LENGTHOF(test); j++) {
                buffSize = u_unescape(test[j], buffer, 512);
                skSize = ucol_getSortKey(coll, buffer, buffSize, sortkey, 512);
                if(strcmp((const char *)lower, (const char *)sortkey) > 0) {
                    log_err("Problem with lower! i = %i, j = %i (%s vs %s)\n", i, j, test[i], test[j]);
                }
                if(strcmp((const char *)upper, (const char *)sortkey) <= 0) {
                    log_err("Problem with upper! i = %i, j = %i (%s vs %s)\n", i, j, test[i], test[j]);
                }
            }
        }
        ucol_close(coll);
    } else {
        log_data_err("Couldn't open collator\n");
    }

}

static void doOverrunTest(UCollator *coll, const UChar *uString, int32_t strLen) {
    int32_t skLen = 0, skLen2 = 0;
    uint8_t sortKey[256];
    int32_t i, j;
    uint8_t filler = 0xFF;

    skLen = ucol_getSortKey(coll, uString, strLen, NULL, 0);

    for(i = 0; i < skLen; i++) {
        memset(sortKey, filler, 256);
        skLen2 = ucol_getSortKey(coll, uString, strLen, sortKey, i);
        if(skLen != skLen2) {
            log_err("For buffer size %i, got different sortkey length. Expected %i got %i\n", i, skLen, skLen2);
        }
        for(j = i; j < 256; j++) {
            if(sortKey[j] != filler) {
                log_err("Something run over index %i\n", j);
                break;
            }
        }
    }
}

/* j1865 reports that if a shorter buffer is passed to
* to get sort key, a buffer overrun happens in some
* cases. This test tries to check this.
*/
void TestSortKeyBufferOverrun(void) {
    UErrorCode status = U_ZERO_ERROR;
    const char* cString = "A very Merry liTTle-lamB..";
    UChar uString[256];
    int32_t strLen = 0;
    UCollator *coll = ucol_open("root", &status);
    strLen = u_unescape(cString, uString, 256);

    if(U_SUCCESS(status)) {
        log_verbose("testing non ignorable\n");
        ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_NON_IGNORABLE, &status);
        doOverrunTest(coll, uString, strLen);

        log_verbose("testing shifted\n");
        ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, &status);
        doOverrunTest(coll, uString, strLen);

        log_verbose("testing shifted quaternary\n");
        ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_QUATERNARY, &status);
        doOverrunTest(coll, uString, strLen);

        log_verbose("testing with french secondaries\n");
        ucol_setAttribute(coll, UCOL_FRENCH_COLLATION, UCOL_ON, &status);
        ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_TERTIARY, &status);
        ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_NON_IGNORABLE, &status);
        doOverrunTest(coll, uString, strLen);

    }
    ucol_close(coll);
}

static void TestAttribute()
{
    UErrorCode error = U_ZERO_ERROR;
    UCollator *coll = ucol_open(NULL, &error);

    if (U_FAILURE(error)) {
        log_err_status(error, "Creation of default collator failed\n");
        return;
    }

    ucol_setAttribute(coll, UCOL_FRENCH_COLLATION, UCOL_OFF, &error);
    if (ucol_getAttribute(coll, UCOL_FRENCH_COLLATION, &error) != UCOL_OFF ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the french collation failed\n");
    }

    ucol_setAttribute(coll, UCOL_FRENCH_COLLATION, UCOL_ON, &error);
    if (ucol_getAttribute(coll, UCOL_FRENCH_COLLATION, &error) != UCOL_ON ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the french collation failed\n");
    }

    ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_SHIFTED, &error);
    if (ucol_getAttribute(coll, UCOL_ALTERNATE_HANDLING, &error) != UCOL_SHIFTED ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the alternate handling failed\n");
    }

    ucol_setAttribute(coll, UCOL_ALTERNATE_HANDLING, UCOL_NON_IGNORABLE, &error);
    if (ucol_getAttribute(coll, UCOL_ALTERNATE_HANDLING, &error) != UCOL_NON_IGNORABLE ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the alternate handling failed\n");
    }

    ucol_setAttribute(coll, UCOL_CASE_FIRST, UCOL_LOWER_FIRST, &error);
    if (ucol_getAttribute(coll, UCOL_CASE_FIRST, &error) != UCOL_LOWER_FIRST ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the case first attribute failed\n");
    }

    ucol_setAttribute(coll, UCOL_CASE_FIRST, UCOL_UPPER_FIRST, &error);
    if (ucol_getAttribute(coll, UCOL_CASE_FIRST, &error) != UCOL_UPPER_FIRST ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the case first attribute failed\n");
    }

    ucol_setAttribute(coll, UCOL_CASE_LEVEL, UCOL_ON, &error);
    if (ucol_getAttribute(coll, UCOL_CASE_LEVEL, &error) != UCOL_ON ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the case level attribute failed\n");
    }

    ucol_setAttribute(coll, UCOL_CASE_LEVEL, UCOL_OFF, &error);
    if (ucol_getAttribute(coll, UCOL_CASE_LEVEL, &error) != UCOL_OFF ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the case level attribute failed\n");
    }

    ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_ON, &error);
    if (ucol_getAttribute(coll, UCOL_NORMALIZATION_MODE, &error) != UCOL_ON ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the normalization on/off attribute failed\n");
    }

    ucol_setAttribute(coll, UCOL_NORMALIZATION_MODE, UCOL_OFF, &error);
    if (ucol_getAttribute(coll, UCOL_NORMALIZATION_MODE, &error) != UCOL_OFF ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the normalization on/off attribute failed\n");
    }

    ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_PRIMARY, &error);
    if (ucol_getAttribute(coll, UCOL_STRENGTH, &error) != UCOL_PRIMARY ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the collation strength failed\n");
    }

    ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_SECONDARY, &error);
    if (ucol_getAttribute(coll, UCOL_STRENGTH, &error) != UCOL_SECONDARY ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the collation strength failed\n");
    }

    ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_TERTIARY, &error);
    if (ucol_getAttribute(coll, UCOL_STRENGTH, &error) != UCOL_TERTIARY ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the collation strength failed\n");
    }

    ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_QUATERNARY, &error);
    if (ucol_getAttribute(coll, UCOL_STRENGTH, &error) != UCOL_QUATERNARY ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the collation strength failed\n");
    }

    ucol_setAttribute(coll, UCOL_STRENGTH, UCOL_IDENTICAL, &error);
    if (ucol_getAttribute(coll, UCOL_STRENGTH, &error) != UCOL_IDENTICAL ||
        U_FAILURE(error)) {
        log_err_status(error, "Setting and retrieving of the collation strength failed\n");
    }

    ucol_close(coll);
}

void TestGetTailoredSet() {
  struct {
    const char *rules;
    const char *tests[20];
    int32_t testsize;
  } setTest[] = {
    { "&a < \\u212b", { "\\u212b", "A\\u030a", "\\u00c5" }, 3},
    { "& S < \\u0161 <<< \\u0160", { "\\u0161", "s\\u030C", "\\u0160", "S\\u030C" }, 4}
  };

  int32_t i = 0, j = 0;
  UErrorCode status = U_ZERO_ERROR;
  UParseError pError;

  UCollator *coll = NULL;
  UChar buff[1024];
  int32_t buffLen = 0;
  USet *set = NULL;

  for(i = 0; i < UPRV_LENGTHOF(setTest); i++) {
    buffLen = u_unescape(setTest[i].rules, buff, 1024);
    coll = ucol_openRules(buff, buffLen, UCOL_DEFAULT, UCOL_DEFAULT, &pError, &status);
    if(U_SUCCESS(status)) {
      set = ucol_getTailoredSet(coll, &status);
      if(uset_size(set) < setTest[i].testsize) {
        log_err("Tailored set size smaller (%d) than expected (%d)\n", uset_size(set), setTest[i].testsize);
      }
      for(j = 0; j < setTest[i].testsize; j++) {
        buffLen = u_unescape(setTest[i].tests[j], buff, 1024);
        if(!uset_containsString(set, buff, buffLen)) {
          log_err("Tailored set doesn't contain %s... It should\n", setTest[i].tests[j]);
        }
      }
      uset_close(set);
    } else {
      log_err_status(status, "Couldn't open collator with rules %s\n", setTest[i].rules);
    }
    ucol_close(coll);
  }
}

static int tMemCmp(const uint8_t *first, const uint8_t *second) {
   int32_t firstLen = (int32_t)strlen((const char *)first);
   int32_t secondLen = (int32_t)strlen((const char *)second);
   return memcmp(first, second, uprv_min(firstLen, secondLen));
}
static const char * strengthsC[] = {
     "UCOL_PRIMARY",
     "UCOL_SECONDARY",
     "UCOL_TERTIARY",
     "UCOL_QUATERNARY",
     "UCOL_IDENTICAL"
};

void TestMergeSortKeys(void) {
   UErrorCode status = U_ZERO_ERROR;
   UCollator *coll = ucol_open("en", &status);
   if(U_SUCCESS(status)) {

     const char* cases[] = {
       "abc",
         "abcd",
         "abcde"
     };
     uint32_t casesSize = UPRV_LENGTHOF(cases);
     const char* prefix = "foo";
     const char* suffix = "egg";
     char outBuff1[256], outBuff2[256];

     uint8_t **sortkeys = (uint8_t **)malloc(casesSize*sizeof(uint8_t *));
     uint8_t **mergedPrefixkeys = (uint8_t **)malloc(casesSize*sizeof(uint8_t *));
     uint8_t **mergedSuffixkeys = (uint8_t **)malloc(casesSize*sizeof(uint8_t *));
     uint32_t *sortKeysLen = (uint32_t *)malloc(casesSize*sizeof(uint32_t));
     uint8_t prefixKey[256], suffixKey[256];
     uint32_t prefixKeyLen = 0, suffixKeyLen = 0, i = 0;
     UChar buffer[256];
     uint32_t unescapedLen = 0, l1 = 0, l2 = 0;
     UColAttributeValue strength;

     log_verbose("ucol_mergeSortkeys test\n");
     log_verbose("Testing order of the test cases\n");
     genericLocaleStarter("en", cases, casesSize);

     for(i = 0; i<casesSize; i++) {
       sortkeys[i] = (uint8_t *)malloc(256*sizeof(uint8_t));
       mergedPrefixkeys[i] = (uint8_t *)malloc(256*sizeof(uint8_t));
       mergedSuffixkeys[i] = (uint8_t *)malloc(256*sizeof(uint8_t));
     }

     unescapedLen = u_unescape(prefix, buffer, 256);
     prefixKeyLen = ucol_getSortKey(coll, buffer, unescapedLen, prefixKey, 256);

     unescapedLen = u_unescape(suffix, buffer, 256);
     suffixKeyLen = ucol_getSortKey(coll, buffer, unescapedLen, suffixKey, 256);

     log_verbose("Massaging data with prefixes and different strengths\n");
     strength = UCOL_PRIMARY;
     while(strength <= UCOL_IDENTICAL) {
       log_verbose("Strength %s\n", strengthsC[strength<=UCOL_QUATERNARY?strength:4]);
       ucol_setAttribute(coll, UCOL_STRENGTH, strength, &status);
       for(i = 0; i<casesSize; i++) {
         unescapedLen = u_unescape(cases[i], buffer, 256);
         sortKeysLen[i] = ucol_getSortKey(coll, buffer, unescapedLen, sortkeys[i], 256);
         ucol_mergeSortkeys(prefixKey, prefixKeyLen, sortkeys[i], sortKeysLen[i], mergedPrefixkeys[i], 256);
         ucol_mergeSortkeys(sortkeys[i], sortKeysLen[i], suffixKey, suffixKeyLen, mergedSuffixkeys[i], 256);
         if(i>0) {
           if(tMemCmp(mergedPrefixkeys[i-1], mergedPrefixkeys[i]) >= 0) {
             log_err("Error while comparing prefixed keys @ strength %s:\n", strengthsC[strength<=UCOL_QUATERNARY?strength:4]);
             log_err("%s\n%s\n",
                         ucol_sortKeyToString(coll, mergedPrefixkeys[i-1], outBuff1, l1),
                         ucol_sortKeyToString(coll, mergedPrefixkeys[i], outBuff2, l2));
           }
           if(tMemCmp(mergedSuffixkeys[i-1], mergedSuffixkeys[i]) >= 0) {
             log_err("Error while comparing suffixed keys @ strength %s:\n", strengthsC[strength<=UCOL_QUATERNARY?strength:4]);
             log_err("%s\n%s\n",
                         ucol_sortKeyToString(coll, mergedSuffixkeys[i-1], outBuff1, l1),
                         ucol_sortKeyToString(coll, mergedSuffixkeys[i], outBuff2, l2));
           }
         }
       }
       if(strength == UCOL_QUATERNARY) {
         strength = UCOL_IDENTICAL;
       } else {
         strength++;
       }
     }

     {
       uint8_t smallBuf[3];
       uint32_t reqLen = 0;
       log_verbose("testing buffer overflow\n");
       reqLen = ucol_mergeSortkeys(prefixKey, prefixKeyLen, suffixKey, suffixKeyLen, smallBuf, 3);
       if(reqLen != (prefixKeyLen+suffixKeyLen)) {
         log_err("Wrong preflight size for merged sortkey\n");
       }
     }

     {
       UChar empty = 0;
       uint8_t emptyKey[20], abcKey[50], mergedKey[100];
       int32_t emptyKeyLen = 0, abcKeyLen = 0, mergedKeyLen = 0;

       log_verbose("testing merging with sortkeys generated for empty strings\n");
       emptyKeyLen = ucol_getSortKey(coll, &empty, 0, emptyKey, 20);
       unescapedLen = u_unescape(cases[0], buffer, 256);
       abcKeyLen = ucol_getSortKey(coll, buffer, unescapedLen, abcKey, 50);
       mergedKeyLen = ucol_mergeSortkeys(emptyKey, emptyKeyLen, abcKey, abcKeyLen, mergedKey, 100);
       if(mergedKey[0] != 2) {
         log_err("Empty sortkey didn't produce a level separator\n");
       }
       /* try with zeros */
       mergedKeyLen = ucol_mergeSortkeys(emptyKey, 0, abcKey, abcKeyLen, mergedKey, 100);
       if(mergedKeyLen != 0 || mergedKey[0] != 0) {
         log_err("Empty key didn't produce null mergedKey\n");
       }
       mergedKeyLen = ucol_mergeSortkeys(abcKey, abcKeyLen, emptyKey, 0, mergedKey, 100);
       if(mergedKeyLen != 0 || mergedKey[0] != 0) {
         log_err("Empty key didn't produce null mergedKey\n");
       }

     }

     for(i = 0; i<casesSize; i++) {
       free(sortkeys[i]);
       free(mergedPrefixkeys[i]);
       free(mergedSuffixkeys[i]);
     }
     free(sortkeys);
     free(mergedPrefixkeys);
     free(mergedSuffixkeys);
     free(sortKeysLen);
     ucol_close(coll);
     /* need to finish this up */
   } else {
     log_data_err("Couldn't open collator");
   }
}
static void TestShortString(void)
{
    struct {
        const char *input;
        const char *expectedOutput;
        const char *locale;
        UErrorCode expectedStatus;
        int32_t    expectedOffset;
        uint32_t   expectedIdentifier;
    } testCases[] = {
        /*
         * Note: The first test case sets variableTop to the dollar sign '$'.
         * We have agreed to drop support for variableTop in ucol_getShortDefinitionString(),
         * related to ticket #10372 "deprecate collation APIs for short definition strings",
         * and because it did not work for most spaces/punctuation/symbols,
         * as documented in ticket #10386 "collation short definition strings issues":
         * The old code wrote only 3 hex digits for primary weights below 0x0FFF,
         * which is a syntax error, and then failed to normalize the result.
         *
         * The "B2700" was removed from the expected result ("B2700_KPHONEBOOK_LDE").
         *
         * Previously, this test had to be adjusted for root collator changes because the
         * primary weight of the variable top character naturally changed
         * but was baked into the expected result.
         */
        {"LDE_RDE_KPHONEBOOK_T0024_ZLATN","KPHONEBOOK_LDE", "de@collation=phonebook", U_USING_FALLBACK_WARNING, 0, 0 },

        {"LEN_RUS_NO_AS_S4","AS_LROOT_NO_S4", NULL, U_USING_DEFAULT_WARNING, 0, 0 },
        // uloc_canonicalize("de__PHONEBOOK") used to return "de@collation=phonebook"
        // and we got U_ZERO_ERROR.
        // Since ICU-20187 "drop support for long-obsolete locale ID variants..."
        // we actually load the "de__PHONEBOOK" bundle and fall back to "de".
        {"LDE_VPHONEBOOK_EO_SI","EO_KPHONEBOOK_LDE_SI", "de@collation=phonebook", U_USING_FALLBACK_WARNING, 0, 0 },
        {"LDE_Kphonebook","KPHONEBOOK_LDE", "de@collation=phonebook", U_ZERO_ERROR, 0, 0 },
        {"Xqde_DE@collation=phonebookq_S3_EX","KPHONEBOOK_LDE", "de@collation=phonebook", U_USING_FALLBACK_WARNING, 0, 0 },
        {"LFR_FO", "FO_LROOT", NULL, U_USING_DEFAULT_WARNING, 0, 0 },
        {"SO_LX_AS", "", NULL, U_ILLEGAL_ARGUMENT_ERROR, 8, 0 },
        {"S3_ASS_MMM", "", NULL, U_ILLEGAL_ARGUMENT_ERROR, 5, 0 }
    };

    int32_t i = 0;
    UCollator *coll = NULL, *fromNormalized = NULL;
    UParseError parseError;
    UErrorCode status = U_ZERO_ERROR;
    char fromShortBuffer[256], normalizedBuffer[256], fromNormalizedBuffer[256];
    const char* locale = NULL;


    for(i = 0; i < UPRV_LENGTHOF(testCases); i++) {
        status = U_ZERO_ERROR;
        if(testCases[i].locale) {
            locale = testCases[i].locale;
        } else {
            locale = NULL;
        }

        coll = ucol_openFromShortString(testCases[i].input, FALSE, &parseError, &status);
        if(status != testCases[i].expectedStatus) {
            log_err_status(status, "Got status '%s' that is different from expected '%s' for '%s'\n",
                u_errorName(status), u_errorName(testCases[i].expectedStatus), testCases[i].input);
            continue;
        }

        if(U_SUCCESS(status)) {
            ucol_getShortDefinitionString(coll, locale, fromShortBuffer, 256, &status);

            if(strcmp(fromShortBuffer, testCases[i].expectedOutput)) {
                log_err("Got short string '%s' from the collator. Expected '%s' for input '%s'\n",
                    fromShortBuffer, testCases[i].expectedOutput, testCases[i].input);
            }

            ucol_normalizeShortDefinitionString(testCases[i].input, normalizedBuffer, 256, &parseError, &status);
            fromNormalized = ucol_openFromShortString(normalizedBuffer, FALSE, &parseError, &status);
            ucol_getShortDefinitionString(fromNormalized, locale, fromNormalizedBuffer, 256, &status);

            if(strcmp(fromShortBuffer, fromNormalizedBuffer)) {
                log_err("Strings obtained from collators instantiated by short string ('%s') and from normalized string ('%s') differ\n",
                    fromShortBuffer, fromNormalizedBuffer);
            }


            if(!ucol_equals(coll, fromNormalized)) {
                log_err("Collator from short string ('%s') differs from one obtained through a normalized version ('%s')\n",
                    testCases[i].input, normalizedBuffer);
            }

            ucol_close(fromNormalized);
            ucol_close(coll);

        } else {
            if(parseError.offset != testCases[i].expectedOffset) {
                log_err("Got parse error offset %i, but expected %i instead for '%s'\n",
                    parseError.offset, testCases[i].expectedOffset, testCases[i].input);
            }
        }
    }

}

static void
doSetsTest(const char *locale, const USet *ref, USet *set, const char* inSet, const char* outSet, UErrorCode *status) {
    UChar buffer[65536];
    int32_t bufLen;

    uset_clear(set);
    bufLen = u_unescape(inSet, buffer, 512);
    uset_applyPattern(set, buffer, bufLen, 0, status);
    if(U_FAILURE(*status)) {
        log_err("%s: Failure setting pattern %s\n", locale, u_errorName(*status));
    }

    if(!uset_containsAll(ref, set)) {
        log_err("%s: Some stuff from %s is not present in the set\n", locale, inSet);
        uset_removeAll(set, ref);
        bufLen = uset_toPattern(set, buffer, UPRV_LENGTHOF(buffer), TRUE, status);
        log_info("    missing: %s\n", aescstrdup(buffer, bufLen));
        bufLen = uset_toPattern(ref, buffer, UPRV_LENGTHOF(buffer), TRUE, status);
        log_info("    total: size=%i  %s\n", uset_getItemCount(ref), aescstrdup(buffer, bufLen));
    }

    uset_clear(set);
    bufLen = u_unescape(outSet, buffer, 512);
    uset_applyPattern(set, buffer, bufLen, 0, status);
    if(U_FAILURE(*status)) {
        log_err("%s: Failure setting pattern %s\n", locale, u_errorName(*status));
    }

    if(!uset_containsNone(ref, set)) {
        log_err("%s: Some stuff from %s is present in the set\n", locale, outSet);
    }
}




static void
TestGetContractionsAndUnsafes(void)
{
    static struct {
        const char* locale;
        const char* inConts;
        const char* outConts;
        const char* inExp;
        const char* outExp;
        const char* unsafeCodeUnits;
        const char* safeCodeUnits;
    } tests[] = {
        { "ru",
            "[{\\u0418\\u0306}{\\u0438\\u0306}]",
            "[\\u0439\\u0457]",
            "[\\u00e6]",
            "[ae]",
            "[\\u0418\\u0438]",
            "[aAbB\\u0430\\u0410\\u0433\\u0413]"
        },
        { "uk",
            "[{\\u0406\\u0308}{\\u0456\\u0308}{\\u0418\\u0306}{\\u0438\\u0306}]",
            "[\\u0407\\u0419\\u0439\\u0457]",
            "[\\u00e6]",
            "[ae]",
            "[\\u0406\\u0456\\u0418\\u0438]",
            "[aAbBxv]",
        },
        { "sh",
            "[{C\\u0301}{C\\u030C}{C\\u0341}{DZ\\u030C}{Dz\\u030C}{D\\u017D}{D\\u017E}{lj}{nj}]",
            "[{\\u309d\\u3099}{\\u30fd\\u3099}]",
            "[\\u00e6]",
            "[a]",
            "[nlcdzNLCDZ]",
            "[jabv]"
        },
        { "ja",
          /*
           * The "collv2" builder omits mappings if the collator maps their
           * character sequences to the same CEs.
           * For example, it omits Japanese contractions for NFD forms
           * of the voiced iteration mark (U+309E = U+309D + U+3099), such as
           * {\\u3053\\u3099\\u309D\\u3099}{\\u3053\\u309D\\u3099}
           * {\\u30B3\\u3099\\u30FD\\u3099}{\\u30B3\\u30FD\\u3099}.
           * It does add mappings for the precomposed forms.
           */
          "[{\\u3053\\u3099\\u309D}{\\u3053\\u3099\\u309E}{\\u3053\\u3099\\u30FC}"
           "{\\u3053\\u309D}{\\u3053\\u309E}{\\u3053\\u30FC}"
           "{\\u30B3\\u3099\\u30FC}{\\u30B3\\u3099\\u30FD}{\\u30B3\\u3099\\u30FE}"
           "{\\u30B3\\u30FC}{\\u30B3\\u30FD}{\\u30B3\\u30FE}]",
          "[{\\u30FD\\u3099}{\\u309D\\u3099}{\\u3053\\u3099}{\\u30B3\\u3099}{lj}{nj}]",
            "[\\u30FE\\u00e6]",
            "[a]",
            "[\\u3099]",
            "[]"
        }
    };

    UErrorCode status = U_ZERO_ERROR;
    UCollator *coll = NULL;
    int32_t i = 0;
    int32_t noConts = 0;
    USet *conts = uset_open(0,0);
    USet *exp = uset_open(0, 0);
    USet *set  = uset_open(0,0);
    int32_t setBufferLen = 65536;
    UChar buffer[65536];
    int32_t setLen = 0;

    for(i = 0; i < UPRV_LENGTHOF(tests); i++) {
        log_verbose("Testing locale: %s\n", tests[i].locale);
        coll = ucol_open(tests[i].locale, &status);
        if (coll == NULL || U_FAILURE(status)) {
            log_err_status(status, "Unable to open collator for locale %s ==> %s\n", tests[i].locale, u_errorName(status));
            continue;
        }
        ucol_getContractionsAndExpansions(coll, conts, exp, TRUE, &status);
        doSetsTest(tests[i].locale, conts, set, tests[i].inConts, tests[i].outConts, &status);
        setLen = uset_toPattern(conts, buffer, setBufferLen, TRUE, &status);
        if(U_SUCCESS(status)) {
            /*log_verbose("Contractions %i: %s\n", uset_getItemCount(conts), aescstrdup(buffer, setLen));*/
        } else {
            log_err("error %s. %i\n", u_errorName(status), setLen);
            status = U_ZERO_ERROR;
        }
        doSetsTest(tests[i].locale, exp, set, tests[i].inExp, tests[i].outExp, &status);
        setLen = uset_toPattern(exp, buffer, setBufferLen, TRUE, &status);
        if(U_SUCCESS(status)) {
            /*log_verbose("Expansions %i: %s\n", uset_getItemCount(exp), aescstrdup(buffer, setLen));*/
        } else {
            log_err("error %s. %i\n", u_errorName(status), setLen);
            status = U_ZERO_ERROR;
        }

        noConts = ucol_getUnsafeSet(coll, conts, &status);
        (void)noConts;   /* Suppress set but not used warning */
        doSetsTest(tests[i].locale, conts, set, tests[i].unsafeCodeUnits, tests[i].safeCodeUnits, &status);
        setLen = uset_toPattern(conts, buffer, setBufferLen, TRUE, &status);
        if(U_SUCCESS(status)) {
            log_verbose("Unsafe %i: %s\n", uset_getItemCount(exp), aescstrdup(buffer, setLen));
        } else {
            log_err("error %s. %i\n", u_errorName(status), setLen);
            status = U_ZERO_ERROR;
        }

        ucol_close(coll);
    }


    uset_close(conts);
    uset_close(exp);
    uset_close(set);
}

static void
TestOpenBinary(void)
{
    /*
     * ucol_openBinary() documents:
     * "The API also takes a base collator which usually should be UCA."
     * and
     * "Currently it cannot be NULL."
     *
     * However, the check for NULL was commented out in ICU 3.4 (r18149).
     * Ticket #4355 requested "Make collation work with minimal data.
     * Optionally without UCA, with relevant parts of UCA copied into the tailoring table."
     *
     * The ICU team agreed with ticket #10517 "require base collator in ucol_openBinary() etc."
     * to require base!=NULL again.
     */
#define OPEN_BINARY_ACCEPTS_NULL_BASE 0
    UErrorCode status = U_ZERO_ERROR;
    /*
    char rule[] = "&h < d < c < b";
    char *wUCA[] = { "a", "h", "d", "c", "b", "i" };
    char *noUCA[] = {"d", "c", "b", "a", "h", "i" };
    */
    /* we have to use Cyrillic letters because latin-1 always gets copied */
    const char rule[] = "&\\u0452 < \\u0434 < \\u0433 < \\u0432"; /* &dje < d < g < v */
    const char *wUCA[] = { "\\u0430", "\\u0452", "\\u0434", "\\u0433", "\\u0432", "\\u0435" }; /* a, dje, d, g, v, e */
#if OPEN_BINARY_ACCEPTS_NULL_BASE
    const char *noUCA[] = {"\\u0434", "\\u0433", "\\u0432", "\\u0430", "\\u0435", "\\u0452" }; /* d, g, v, a, e, dje */
#endif

    UChar uRules[256];
    int32_t uRulesLen = u_unescape(rule, uRules, 256);

    UCollator *coll = ucol_openRules(uRules, uRulesLen, UCOL_DEFAULT, UCOL_DEFAULT, NULL, &status);
    UCollator *UCA = NULL;
    UCollator *cloneNOUCA = NULL, *cloneWUCA = NULL;

    uint8_t imageBuffer[32768];
    uint8_t *image = imageBuffer;
    int32_t imageBufferCapacity = 32768;

    int32_t imageSize;

    if((coll==NULL)||(U_FAILURE(status))) {
        log_data_err("could not load collators or error occured: %s\n",
            u_errorName(status));
        return;
    }
    UCA = ucol_open("root", &status);
    if((UCA==NULL)||(U_FAILURE(status))) {
        log_data_err("could not load UCA collator or error occured: %s\n",
            u_errorName(status));
        return;
    }
    imageSize = ucol_cloneBinary(coll, image, imageBufferCapacity, &status);
    if(U_FAILURE(status)) {
        image = (uint8_t *)malloc(imageSize*sizeof(uint8_t));
        status = U_ZERO_ERROR;
        imageSize = ucol_cloneBinary(coll, imageBuffer, imageSize, &status);
    }


    cloneWUCA = ucol_openBinary(image, imageSize, UCA, &status);
    cloneNOUCA = ucol_openBinary(image, imageSize, NULL, &status);
#if !OPEN_BINARY_ACCEPTS_NULL_BASE
    if(status != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("ucol_openBinary(base=NULL) unexpectedly did not fail - %s\n", u_errorName(status));
    }
#endif

    genericOrderingTest(coll, wUCA, UPRV_LENGTHOF(wUCA));

    genericOrderingTest(cloneWUCA, wUCA, UPRV_LENGTHOF(wUCA));
#if OPEN_BINARY_ACCEPTS_NULL_BASE
    genericOrderingTest(cloneNOUCA, noUCA, UPRV_LENGTHOF(noUCA));
#endif

    if(image != imageBuffer) {
        free(image);
    }
    ucol_close(coll);
    ucol_close(cloneNOUCA);
    ucol_close(cloneWUCA);
    ucol_close(UCA);
}

static void TestDefault(void) {
    /* Tests for code coverage. */
    UErrorCode status = U_ZERO_ERROR;
    UCollator *coll = ucol_open("es@collation=pinyin", &status);
    if (coll == NULL || status == U_FILE_ACCESS_ERROR) {
        log_data_err("Unable to open collator es@collation=pinyin\n");
        return;
    }
    if (status != U_USING_DEFAULT_WARNING) {
        /* What do you mean that you know about using pinyin collation in Spanish!? This should be in the zh locale. */
        log_err("es@collation=pinyin should return U_USING_DEFAULT_WARNING, but returned %s\n", u_errorName(status));
    }
    ucol_close(coll);
    if (ucol_getKeywordValues("funky", &status) != NULL) {
        log_err("Collators should not know about the funky keyword.\n");
    }
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("funky keyword didn't fail as expected %s\n", u_errorName(status));
    }
    if (ucol_getKeywordValues("collation", &status) != NULL) {
        log_err("ucol_getKeywordValues should not work when given a bad status.\n");
    }
}

static void TestDefaultKeyword(void) {
    /* Tests for code coverage. */
    UErrorCode status = U_ZERO_ERROR;
    const char *loc = "zh_TW@collation=default";
    UCollator *coll = ucol_open(loc, &status);
    if(U_FAILURE(status)) {
        log_info("Warning: ucol_open(%s, ...) returned %s, at least it didn't crash.\n", loc, u_errorName(status));
    } else if (status != U_USING_FALLBACK_WARNING) {
        /* Hmm, skip the following test for CLDR 1.9 data and/or ICU 4.6, no longer seems to apply */
        #if 0
        log_err("ucol_open(%s, ...) should return an error or some sort of U_USING_FALLBACK_WARNING, but returned %s\n", loc, u_errorName(status));
        #endif
    }
    ucol_close(coll);
}

static UBool uenum_contains(UEnumeration *e, const char *s, UErrorCode *status) {
    const char *t;
    uenum_reset(e, status);
    while(((t = uenum_next(e, NULL, status)) != NULL) && U_SUCCESS(*status)) {
        if(uprv_strcmp(s, t) == 0) {
            return TRUE;
        }
    }
    return FALSE;
}

static void TestGetKeywordValuesForLocale(void) {
#define MAX_NUMBER_OF_KEYWORDS 9
    const char *PREFERRED[][MAX_NUMBER_OF_KEYWORDS+1] = {
            { "und",            "standard", "eor", "search", NULL, NULL, NULL, NULL, NULL, NULL },
            { "en_US",          "standard", "eor", "search", NULL, NULL, NULL, NULL, NULL, NULL },
            { "en_029",         "standard", "eor", "search", NULL, NULL, NULL, NULL, NULL, NULL },
            { "de_DE",          "standard", "phonebook", "search", "eor", NULL, NULL, NULL, NULL, NULL },
            { "de_Latn_DE",     "standard", "phonebook", "search", "eor", NULL, NULL, NULL, NULL, NULL },
            { "zh",             "pinyin", "stroke", "eor", "search", "standard", NULL },
            { "zh_Hans",        "pinyin", "stroke", "eor", "search", "standard", NULL },
            { "zh_CN",          "pinyin", "stroke", "eor", "search", "standard", NULL },
            { "zh_Hant",        "stroke", "pinyin", "eor", "search", "standard", NULL },
            { "zh_TW",          "stroke", "pinyin", "eor", "search", "standard", NULL },
            { "zh__PINYIN",     "pinyin", "stroke", "eor", "search", "standard", NULL },
            { "es_ES",          "standard", "search", "traditional", "eor", NULL, NULL, NULL, NULL, NULL },
            { "es__TRADITIONAL","traditional", "search", "standard", "eor", NULL, NULL, NULL, NULL, NULL },
            { "und@collation=phonebook",    "standard", "eor", "search", NULL, NULL, NULL, NULL, NULL, NULL },
            { "de_DE@collation=pinyin",     "standard", "phonebook", "search", "eor", NULL, NULL, NULL, NULL, NULL },
            { "zzz@collation=xxx",          "standard", "eor", "search", NULL, NULL, NULL, NULL, NULL, NULL }
    };

    UErrorCode status = U_ZERO_ERROR;
    UEnumeration *keywordValues = NULL;
    int32_t i, n, size;
    const char *locale = NULL, *value = NULL;
    UBool errorOccurred = FALSE;

    for (i = 0; i < UPRV_LENGTHOF(PREFERRED) && !errorOccurred; i++) {
        locale = PREFERRED[i][0];
        value = NULL;
        size = 0;

        keywordValues = ucol_getKeywordValuesForLocale("collation", locale, TRUE, &status);
        if (keywordValues == NULL || U_FAILURE(status)) {
            log_err_status(status, "Error getting keyword values: %s\n", u_errorName(status));
            break;
        }
        size = uenum_count(keywordValues, &status);
        (void)size;

        for (n = 0; (value = PREFERRED[i][n+1]) != NULL; n++) {
            if (!uenum_contains(keywordValues, value, &status)) {
                if (U_SUCCESS(status)) {
                    log_err("Keyword value \"%s\" missing for locale: %s\n", value, locale);
                } else {
                    log_err("While getting keyword value from locale: %s got this error: %s\n", locale, u_errorName(status));
                    errorOccurred = TRUE;
                    break;
                }
            }
        }
        uenum_close(keywordValues);
        keywordValues = NULL;
    }
    uenum_close(keywordValues);
}

static void TestStrcollNull(void) {
    UErrorCode status = U_ZERO_ERROR;
    UCollator *coll;

    const UChar u16asc[] = {0x0049, 0x0042, 0x004D, 0};
    const int32_t u16ascLen = 3;

    const UChar u16han[] = {0x5c71, 0x5ddd, 0};
    const int32_t u16hanLen = 2;

    const char *u8asc = "\x49\x42\x4D";
    const int32_t u8ascLen = 3;

    const char *u8han = "\xE5\xB1\xB1\xE5\xB7\x9D";
    const int32_t u8hanLen = 6;

    coll = ucol_open(NULL, &status);
    if (U_FAILURE(status)) {
        log_err_status(status, "Default Collator creation failed.: %s\n", myErrorName(status));
        return;
    }

    /* UChar API */
    if (ucol_strcoll(coll, NULL, 0, NULL, 0) != 0) {
        log_err("ERROR : ucol_strcoll NULL/0 and NULL/0");
    }

    if (ucol_strcoll(coll, NULL, -1, NULL, 0) != 0) {
        /* No error arg, should return equal without crash */
        log_err("ERROR : ucol_strcoll NULL/-1 and NULL/0");
    }

    if (ucol_strcoll(coll, u16asc, -1, NULL, 10) != 0) {
        /* No error arg, should return equal without crash */
        log_err("ERROR : ucol_strcoll u16asc/u16ascLen and NULL/10");
    }

    if (ucol_strcoll(coll, u16asc, -1, NULL, 0) <= 0) {
        log_err("ERROR : ucol_strcoll u16asc/-1 and NULL/0");
    }
    if (ucol_strcoll(coll, NULL, 0, u16asc, -1) >= 0) {
        log_err("ERROR : ucol_strcoll NULL/0 and u16asc/-1");
    }
    if (ucol_strcoll(coll, u16asc, u16ascLen, NULL, 0) <= 0) {
        log_err("ERROR : ucol_strcoll u16asc/u16ascLen and NULL/0");
    }

    if (ucol_strcoll(coll, u16han, -1, NULL, 0) <= 0) {
        log_err("ERROR : ucol_strcoll u16han/-1 and NULL/0");
    }
    if (ucol_strcoll(coll, NULL, 0, u16han, -1) >= 0) {
        log_err("ERROR : ucol_strcoll NULL/0 and u16han/-1");
    }
    if (ucol_strcoll(coll, NULL, 0, u16han, u16hanLen) >= 0) {
        log_err("ERROR : ucol_strcoll NULL/0 and u16han/u16hanLen");
    }

    /* UTF-8 API */
    status = U_ZERO_ERROR;
    if (ucol_strcollUTF8(coll, NULL, 0, NULL, 0, &status) != 0 || U_FAILURE(status)) {
        log_err("ERROR : ucol_strcollUTF8 NULL/0 and NULL/0");
    }
    status = U_ZERO_ERROR;
    ucol_strcollUTF8(coll, NULL, -1, NULL, 0, &status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("ERROR: ucol_strcollUTF8 NULL/-1 and NULL/0, should return U_ILLEGAL_ARGUMENT_ERROR");
    }
    status = U_ZERO_ERROR;
    ucol_strcollUTF8(coll, u8asc, u8ascLen, NULL, 10, &status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        log_err("ERROR: ucol_strcollUTF8 u8asc/u8ascLen and NULL/10, should return U_ILLEGAL_ARGUMENT_ERROR");
    }

    status = U_ZERO_ERROR;
    if (ucol_strcollUTF8(coll, u8asc, -1, NULL, 0, &status) <= 0  || U_FAILURE(status)) {
        log_err("ERROR : ucol_strcollUTF8 u8asc/-1 and NULL/0");
    }
    status = U_ZERO_ERROR;
    if (ucol_strcollUTF8(coll, NULL, 0, u8asc, -1, &status) >= 0  || U_FAILURE(status)) {
        log_err("ERROR : ucol_strcollUTF8 NULL/0 and u8asc/-1");
    }
    status = U_ZERO_ERROR;
    if (ucol_strcollUTF8(coll, u8asc, u8ascLen, NULL, 0, &status) <= 0 || U_FAILURE(status)) {
        log_err("ERROR : ucol_strcollUTF8 u8asc/u8ascLen and NULL/0");
    }

    status = U_ZERO_ERROR;
    if (ucol_strcollUTF8(coll, u8han, -1, NULL, 0, &status) <= 0 || U_FAILURE(status)) {
        log_err("ERROR : ucol_strcollUTF8 u8han/-1 and NULL/0");
    }
    status = U_ZERO_ERROR;
    if (ucol_strcollUTF8(coll, NULL, 0, u8han, -1, &status) >= 0 || U_FAILURE(status)) {
        log_err("ERROR : ucol_strcollUTF8 NULL/0 and u8han/-1");
    }
    status = U_ZERO_ERROR;
    if (ucol_strcollUTF8(coll, NULL, 0, u8han, u8hanLen, &status) >= 0 || U_FAILURE(status)) {
        log_err("ERROR : ucol_strcollUTF8 NULL/0 and u8han/u8hanLen");
    }

    ucol_close(coll);
}

#endif /* #if !UCONFIG_NO_COLLATION */
