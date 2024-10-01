/************************************************************************
 * © 2016 and later: Unicode, Inc. and others.
 * License & terms of use: http://www.unicode.org/copyright.html#License
 *
 *************************************************************************
 ********************************************************************
 * COPYRIGHT:
 * Copyright (C) 2008-2012 IBM, Inc.   All Rights Reserved.
 *
 ********************************************************************/
/** 
 * This program tests string search performance.
 * APIs tested: 
 * ICU4C 
 */

#include "strsrchperf.h"

StringSearchPerformanceTest::StringSearchPerformanceTest(int32_t argc, const char *argv[], UErrorCode &status)
:UPerfTest(argc,argv,status){
    int32_t start, end;
    srch = NULL;
    pttrn = NULL;
    if(status== U_ILLEGAL_ARGUMENT_ERROR || line_mode){
       fprintf(stderr,gUsageString, "strsrchperf");
       return;
    }
    /* Get the Text */
    src = getBuffer(srcLen, status);

#if 0
    /* Get a word to find. Do this by selecting a random word with a word breakiterator. */
    UBreakIterator* brk = ubrk_open(UBRK_WORD, locale, src, srcLen, &status);
    if(U_FAILURE(status)){
        fprintf(stderr, "FAILED to create pattern for searching. Error: %s\n", u_errorName(status));
        return;
    }
    start = ubrk_preceding(brk, 1000);
    end = ubrk_following(brk, start);
    pttrnLen = end - start;
    UChar* temp = (UChar*)malloc(sizeof(UChar)*(pttrnLen));
    for (int i = 0; i < pttrnLen; i++) {
        temp[i] = src[start++];
    }
    pttrn = temp; /* store word in pttrn */
    ubrk_close(brk);
#else
    /* The first line of the file contains the pattern */
    start = 0;

    for(end = start; ; end += 1) {
        UChar ch = src[end];

        if (ch == 0x000A || ch == 0x000D || ch == 0x2028) {
            break;
        }
    }

    pttrnLen = end - start;
    UChar* temp = (UChar*)malloc(sizeof(UChar)*(pttrnLen));
    for (int i = 0; i < pttrnLen; i++) {
        temp[i] = src[start++];
    }
    pttrn = temp; /* store word in pttrn */
#endif
    
    /* Create the StringSearch object to be use in performance test. */
    srch = usearch_open(pttrn, pttrnLen, src, srcLen, locale, NULL, &status);

    if(U_FAILURE(status)){
        fprintf(stderr, "FAILED to create UPerfTest object. Error: %s\n", u_errorName(status));
        return;
    }
    
}

StringSearchPerformanceTest::~StringSearchPerformanceTest() {
    if (pttrn != NULL) {
        free(pttrn);
    }
    if (srch != NULL) {
        usearch_close(srch);
    }
}

UPerfFunction* StringSearchPerformanceTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char *par) {
    switch (index) {
        TESTCASE(0,Test_ICU_Forward_Search);
        TESTCASE(1,Test_ICU_Backward_Search);

        default: 
            name = ""; 
            return NULL;
    }
    return NULL;
}

UPerfFunction* StringSearchPerformanceTest::Test_ICU_Forward_Search(){
    StringSearchPerfFunction* func = new StringSearchPerfFunction(ICUForwardSearch, srch, src, srcLen, pttrn, pttrnLen);
    return func;
}

UPerfFunction* StringSearchPerformanceTest::Test_ICU_Backward_Search(){
    StringSearchPerfFunction* func = new StringSearchPerfFunction(ICUBackwardSearch, srch, src, srcLen, pttrn, pttrnLen);
    return func;
}

int main (int argc, const char* argv[]) {
    UErrorCode status = U_ZERO_ERROR;
    StringSearchPerformanceTest test(argc, argv, status);
    if(U_FAILURE(status)){
        return status;
    }
    if(test.run()==FALSE){
        fprintf(stderr,"FAILED: Tests could not be run please check the arguments.\n");
        return -1;
    }
    return 0;
}
