// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
**********************************************************************
*   Copyright (C) 2002-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
**********************************************************************
*   file name:  iotest.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2002feb21
*   created by: George Rhoten
*/


#include "unicode/ustdio.h"
#include "unicode/uclean.h"

#include "unicode/ucnv.h"
#include "unicode/uchar.h"
#include "unicode/unistr.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "ustr_cnv.h"
#include "iotest.h"
#include "unicode/tstdtmod.h"
#include "putilimp.h"

#include <string.h>
#include <stdlib.h>

class DataDrivenLogger : public TestLog {
    static const char* fgDataDir;
    static char *fgTestDataPath;

public:
    static void cleanUp() {
        if (fgTestDataPath) {
            free(fgTestDataPath);
            fgTestDataPath = nullptr;
        }
    }
    virtual void errln( const UnicodeString &message ) override {
        char buffer[4000];
        message.extract(0, message.length(), buffer, sizeof(buffer));
        buffer[3999] = 0; /* NUL terminate */
        log_err(buffer);
    }

    virtual void logln( const UnicodeString &message ) override {
        char buffer[4000];
        message.extract(0, message.length(), buffer, sizeof(buffer));
        buffer[3999] = 0; /* NUL terminate */
        log_info(buffer);
    }

    virtual void dataerrln( const UnicodeString &message ) override {
        char buffer[4000];
        message.extract(0, message.length(), buffer, sizeof(buffer));
        buffer[3999] = 0; /* NUL terminate */
        log_data_err(buffer);
    }

    static const char * pathToDataDirectory()
    {

        if(fgDataDir != nullptr) {
            return fgDataDir;
        }

        /* U_TOPSRCDIR is set by the makefiles on UNIXes when building cintltst and intltst
        //              to point to the top of the build hierarchy, which may or
        //              may not be the same as the source directory, depending on
        //              the configure options used.  At any rate,
        //              set the data path to the built data from this directory.
        //              The value is complete with quotes, so it can be used
        //              as-is as a string constant.
        */
    #if defined (U_TOPSRCDIR)
        {
            fgDataDir = U_TOPSRCDIR  U_FILE_SEP_STRING "data" U_FILE_SEP_STRING;
        }
    #else

        /* On Windows, the file name obtained from __FILE__ includes a full path.
        *             This file is "wherever\icu\source\test\cintltst\cintltst.c"
        *             Change to    "wherever\icu\source\data"
        */
        {
            static char p[sizeof(__FILE__) + 10];
            char *pBackSlash;
            int i;

            strcpy(p, __FILE__);
            /* We want to back over three '\' chars.                            */
            /*   Only Windows should end up here, so looking for '\' is safe.   */
            for (i=1; i<=3; i++) {
                pBackSlash = strrchr(p, U_FILE_SEP_CHAR);
                if (pBackSlash != nullptr) {
                    *pBackSlash = 0;        /* Truncate the string at the '\'   */
                }
            }

            if (pBackSlash != nullptr) {
                /* We found and truncated three names from the path.
                *  Now append "source\data" and set the environment
                */
                strcpy(pBackSlash, U_FILE_SEP_STRING "data" U_FILE_SEP_STRING );
                fgDataDir = p;
            }
            else {
                /* __FILE__ on MSVC7 does not contain the directory */
                FILE *file = fopen(".." U_FILE_SEP_STRING".." U_FILE_SEP_STRING "data" U_FILE_SEP_STRING "Makefile.in", "r");
                if (file) {
                    fclose(file);
                    fgDataDir = ".." U_FILE_SEP_STRING".." U_FILE_SEP_STRING "data" U_FILE_SEP_STRING;
                }
                else {
                    fgDataDir = ".." U_FILE_SEP_STRING".." U_FILE_SEP_STRING".." U_FILE_SEP_STRING".." U_FILE_SEP_STRING "data" U_FILE_SEP_STRING;
                }
            }
        }
    #endif

        return fgDataDir;

    }

    static const char* loadTestData(UErrorCode& err){
        if( fgTestDataPath == nullptr){
            const char*      directory=nullptr;
            UResourceBundle* test =nullptr;
            char* tdpath=nullptr;
            const char* tdrelativepath;

#if defined (U_TOPBUILDDIR)
            tdrelativepath = "test" U_FILE_SEP_STRING "testdata" U_FILE_SEP_STRING "out" U_FILE_SEP_STRING;
            directory = U_TOPBUILDDIR;
#else
            tdrelativepath = ".." U_FILE_SEP_STRING "test" U_FILE_SEP_STRING "testdata" U_FILE_SEP_STRING "out" U_FILE_SEP_STRING;
            directory = pathToDataDirectory();
#endif

            tdpath = (char*) malloc(sizeof(char) *(( strlen(directory) * strlen(tdrelativepath)) + 100));


            /* u_getDataDirectory shoul return \source\data ... set the
            * directory to ..\source\data\..\test\testdata\out\testdata
            */
            strcpy(tdpath, directory);
            strcat(tdpath, tdrelativepath);
            strcat(tdpath,"testdata");

            test=ures_open(tdpath, "testtypes", &err);

            if(U_FAILURE(err)){
                err = U_FILE_ACCESS_ERROR;
                log_data_err("Could not load testtypes.res in testdata bundle with path %s - %s\n", tdpath, u_errorName(err));
                return "";
            }
            ures_close(test);
            fgTestDataPath = tdpath;
        }
        return fgTestDataPath;
    }

    virtual const char* getTestDataPath(UErrorCode& err) override {
        return loadTestData(err);
    }
};

const char* DataDrivenLogger::fgDataDir = nullptr;
char* DataDrivenLogger::fgTestDataPath = nullptr;

#if !UCONFIG_NO_FORMATTING && !UCONFIG_NO_FILE_IO
static int64_t
uto64(const char16_t  *buffer)
{
    uint64_t result = 0;
    /* iterate through buffer */
    while(*buffer) {
        /* read the next digit */
        result *= 16u;
        if (!u_isxdigit(*buffer)) {
            log_err("\\u%04X is not a valid hex digit for this test\n", (char16_t)*buffer);
        }
        result += *buffer - 0x0030 - (*buffer >= 0x0041 ? (*buffer >= 0x0061 ? 39 : 7) : 0);
        buffer++;
    }
    return (int64_t)result;
}
#endif

U_CDECL_BEGIN
static void U_CALLCONV DataDrivenPrintf()
{
#if !UCONFIG_NO_FORMATTING && !UCONFIG_NO_FILE_IO
    UErrorCode errorCode;
    TestDataModule *dataModule;
    TestData *testData;
    const DataMap *testCase;
    DataDrivenLogger logger;
    char16_t uBuffer[512];
    char cBuffer[512];
    char cFormat[sizeof(cBuffer)];
    char cExpected[sizeof(cBuffer)];
    UnicodeString tempStr;
    char16_t format[512];
    char16_t expectedResult[512];
    char16_t argument[512];
    int32_t i;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    double dbl;
    int32_t uBufferLenReturned;

    const char *fileLocale = "en_US_POSIX";
    int32_t uFileBufferLenReturned;
    LocalUFILEPointer testFile;

    errorCode=U_ZERO_ERROR;
    dataModule=TestDataModule::getTestDataModule("icuio", logger, errorCode);
    if(U_SUCCESS(errorCode)) {
        testData=dataModule->createTestData("printf", errorCode);
        if(U_SUCCESS(errorCode)) {
            for(i=0; testData->nextCase(testCase, errorCode); ++i) {
                if(U_FAILURE(errorCode)) {
                    log_err("error retrieving icuio/printf test case %d - %s\n",
                            i, u_errorName(errorCode));
                    errorCode=U_ZERO_ERROR;
                    continue;
                }
                testFile.adoptInstead(u_fopen(STANDARD_TEST_FILE, "w", fileLocale, "UTF-8"));
                if (testFile.isNull()) {
                    log_err("Can't open test file - %s\n",
                            STANDARD_TEST_FILE);
                    continue;
                }
                u_memset(uBuffer, 0x2A, UPRV_LENGTHOF(uBuffer));
                uBuffer[UPRV_LENGTHOF(uBuffer)-1] = 0;
                tempStr=testCase->getString("format", errorCode);
                tempStr.extract(format, UPRV_LENGTHOF(format), errorCode);
                tempStr=testCase->getString("result", errorCode);
                tempStr.extract(expectedResult, UPRV_LENGTHOF(expectedResult), errorCode);
                tempStr=testCase->getString("argument", errorCode);
                tempStr.extract(argument, UPRV_LENGTHOF(argument), errorCode);
                u_austrncpy(cBuffer, format, sizeof(cBuffer));
                if(U_FAILURE(errorCode)) {
                    log_err("error retrieving icuio/printf test case %d - %s\n",
                            i, u_errorName(errorCode));
                    errorCode=U_ZERO_ERROR;
                    continue;
                }
                log_verbose("Test %d: format=\"%s\"\n", i, cBuffer);
                switch (testCase->getString("argumentType", errorCode)[0]) {
                case 0x64:  // 'd' double
                    dbl = atof(u_austrcpy(cBuffer, argument));
                    uBufferLenReturned = u_sprintf_u(uBuffer, format, dbl);
                    uFileBufferLenReturned = u_fprintf_u(testFile.getAlias(), format, dbl);
                    break;
                case 0x31:  // '1' int8_t
                    i8 = (int8_t)uto64(argument);
                    uBufferLenReturned = u_sprintf_u(uBuffer, format, i8);
                    uFileBufferLenReturned = u_fprintf_u(testFile.getAlias(), format, i8);
                    break;
                case 0x32:  // '2' int16_t
                    i16 = (int16_t)uto64(argument);
                    uBufferLenReturned = u_sprintf_u(uBuffer, format, i16);
                    uFileBufferLenReturned = u_fprintf_u(testFile.getAlias(), format, i16);
                    break;
                case 0x34:  // '4' int32_t
                    i32 = (int32_t)uto64(argument);
                    uBufferLenReturned = u_sprintf_u(uBuffer, format, i32);
                    uFileBufferLenReturned = u_fprintf_u(testFile.getAlias(), format, i32);
                    break;
                case 0x38:  // '8' int64_t
                    i64 = uto64(argument);
                    uBufferLenReturned = u_sprintf_u(uBuffer, format, i64);
                    uFileBufferLenReturned = u_fprintf_u(testFile.getAlias(), format, i64);
                    break;
                case 0x73:  // 's' char *
                    u_austrncpy(cBuffer, argument, sizeof(cBuffer));
                    uBufferLenReturned = u_sprintf_u(uBuffer, format, cBuffer);
                    uFileBufferLenReturned = u_fprintf_u(testFile.getAlias(), format, cBuffer);
                    break;
                case 0x53:  // 'S' char16_t *
                    uBufferLenReturned = u_sprintf_u(uBuffer, format, argument);
                    uFileBufferLenReturned = u_fprintf_u(testFile.getAlias(), format, argument);
                    break;
                default:
                    uBufferLenReturned = 0;
                    uFileBufferLenReturned = 0;
                    log_err("Unknown type %c for test %d\n", testCase->getString("argumentType", errorCode)[0], i);
                }
                if (u_strcmp(uBuffer, expectedResult) != 0) {
                    u_austrncpy(cBuffer, uBuffer, sizeof(cBuffer));
                    u_austrncpy(cFormat, format, sizeof(cFormat));
                    u_austrncpy(cExpected, expectedResult, sizeof(cExpected));
                    cBuffer[sizeof(cBuffer)-1] = 0;
                    log_err("FAILURE string test case %d \"%s\" - Got: \"%s\" Expected: \"%s\"\n",
                            i, cFormat, cBuffer, cExpected);
                }
                if (uBufferLenReturned <= 0) {
                    log_err("FAILURE test case %d - \"%s\" is an empty string.\n",
                            i, cBuffer);
                }
                else if (uBuffer[uBufferLenReturned-1] == 0
                    || uBuffer[uBufferLenReturned] != 0
                    || uBuffer[uBufferLenReturned+1] != 0x2A
                    || uBuffer[uBufferLenReturned+2] != 0x2A)
                {
                    u_austrncpy(cBuffer, uBuffer, sizeof(cBuffer));
                    cBuffer[sizeof(cBuffer)-1] = 0;
                    log_err("FAILURE test case %d - \"%s\" wrong amount of characters was written. Got %d.\n",
                            i, cBuffer, uBufferLenReturned);
                }
                testFile.adoptInstead(u_fopen(STANDARD_TEST_FILE, "r", fileLocale, "UTF-8"));
                if (testFile.isNull()) {
                    log_err("Can't open test file - %s\n",
                            STANDARD_TEST_FILE);
                }
                uBuffer[0]=0;
                u_fgets(uBuffer, UPRV_LENGTHOF(uBuffer), testFile.getAlias());
                if (u_strcmp(uBuffer, expectedResult) != 0) {
                    u_austrncpy(cBuffer, uBuffer, sizeof(cBuffer));
                    u_austrncpy(cFormat, format, sizeof(cFormat));
                    u_austrncpy(cExpected, expectedResult, sizeof(cExpected));
                    cBuffer[sizeof(cBuffer)-1] = 0;
                    log_err("FAILURE file test case %d \"%s\" - Got: \"%s\" Expected: \"%s\"\n",
                            i, cFormat, cBuffer, cExpected);
                }
                if (uFileBufferLenReturned != uBufferLenReturned)
                {
                    u_austrncpy(cBuffer, uBuffer, sizeof(cBuffer));
                    cBuffer[sizeof(cBuffer)-1] = 0;
                    log_err("FAILURE uFileBufferLenReturned(%d) != uBufferLenReturned(%d)\n",
                            uFileBufferLenReturned, uBufferLenReturned);
                }

                if(U_FAILURE(errorCode)) {
                    log_err("error running icuio/printf test case %d - %s\n",
                            i, u_errorName(errorCode));
                    errorCode=U_ZERO_ERROR;
                    continue;
                }
            }
            delete testData;
        }
        delete dataModule;
    }
    else {
        log_data_err("Failed: could not load test icuio data\n");
    }
#endif
}
U_CDECL_END

U_CDECL_BEGIN
static void U_CALLCONV ScanfMultipleIntegers()
{
#if !UCONFIG_NO_FORMATTING && !UCONFIG_NO_FILE_IO
    UnicodeString input = UNICODE_STRING("[1.2.3]", 7);
    UnicodeString fmt = UNICODE_STRING("[%d.%d.%d]", 10);
    DataDrivenLogger logger;

    const int32_t expectedFirst = 1;
    const int32_t expectedSecond = 2;
    const int32_t expectedThird = 3;
    const int32_t expectedResult = 3;
    int32_t first = 0;
    int32_t second = 0;
    int32_t third = 0;
    int32_t result = u_sscanf_u(input.getBuffer(), fmt.getBuffer(), &first, &second, &third);

    if(first != expectedFirst){
        log_err("error in scanfmultipleintegers test 'first' Got: %d Exp: %d\n", 
                first, expectedFirst);
    }
    if(second != expectedSecond){
        log_err("error in scanfmultipleintegers test 'second' Got: %d Exp: %d\n",
                second, expectedSecond);
    }
    if(third != expectedThird){
        log_err("error in scanfmultipleintegers test 'third' Got: %d Exp: %d\n",
                third, expectedThird);
    }
    if(result != expectedResult){
        log_err("error in scanfmultipleintegers test 'result'  Got: %d Exp: %d\n",
                result, expectedResult);
    }
#endif
}
U_CDECL_END

U_CDECL_BEGIN
static void U_CALLCONV DataDrivenScanf()
{
#if !UCONFIG_NO_FORMATTING && !UCONFIG_NO_FILE_IO
    UErrorCode errorCode;
    TestDataModule *dataModule;
    TestData *testData;
    const DataMap *testCase;
    DataDrivenLogger logger;
    char16_t uBuffer[512];
    char cBuffer[512];
    char cExpected[sizeof(cBuffer)];
    UnicodeString tempStr;
    char16_t format[512];
    char16_t expectedResult[512];
    char16_t argument[512];
    int32_t i;
    int8_t i8, expected8;
    int16_t i16, expected16;
    int32_t i32, expected32;
    int64_t i64, expected64;
    double dbl, expectedDbl;
    volatile float flt, expectedFlt; // Use volatile in order to get around an Intel compiler issue.
    int32_t uBufferLenReturned;

    //const char *fileLocale = "en_US_POSIX";
    //int32_t uFileBufferLenReturned;
    //UFILE *testFile;

    errorCode=U_ZERO_ERROR;
    dataModule=TestDataModule::getTestDataModule("icuio", logger, errorCode);
    if(U_SUCCESS(errorCode)) {
        testData=dataModule->createTestData("scanf", errorCode);
        if(U_SUCCESS(errorCode)) {
            for(i=0; testData->nextCase(testCase, errorCode); ++i) {
                if(U_FAILURE(errorCode)) {
                    log_err("error retrieving icuio/printf test case %d - %s\n",
                            i, u_errorName(errorCode));
                    errorCode=U_ZERO_ERROR;
                    continue;
                }
/*                testFile = u_fopen(STANDARD_TEST_FILE, "w", fileLocale, "UTF-8");
                if (!testFile) {
                    log_err("Can't open test file - %s\n",
                            STANDARD_TEST_FILE);
                }*/
                u_memset(uBuffer, 0x2A, UPRV_LENGTHOF(uBuffer));
                uBuffer[UPRV_LENGTHOF(uBuffer)-1] = 0;
                tempStr=testCase->getString("format", errorCode);
                tempStr.extract(format, UPRV_LENGTHOF(format), errorCode);
                tempStr=testCase->getString("result", errorCode);
                tempStr.extract(expectedResult, UPRV_LENGTHOF(expectedResult), errorCode);
                tempStr=testCase->getString("argument", errorCode);
                tempStr.extract(argument, UPRV_LENGTHOF(argument), errorCode);
                u_austrncpy(cBuffer, format, sizeof(cBuffer));
                if(U_FAILURE(errorCode)) {
                    log_err("error retrieving icuio/printf test case %d - %s\n",
                            i, u_errorName(errorCode));
                    errorCode=U_ZERO_ERROR;
                    continue;
                }
                log_verbose("Test %d: format=\"%s\"\n", i, cBuffer);
                switch (testCase->getString("argumentType", errorCode)[0]) {
                case 0x64:  // 'd' double
                    expectedDbl = atof(u_austrcpy(cBuffer, expectedResult));
                    uBufferLenReturned = u_sscanf_u(argument, format, &dbl);
                    //uFileBufferLenReturned = u_fscanf_u(testFile, format, dbl);
                    if (dbl != expectedDbl) {
                        log_err("error in scanf test case[%d] Got: %f Exp: %f\n",
                                i, dbl, expectedDbl);
                    }
                    break;
                case 0x66:  // 'f' float
                    expectedFlt = (float)atof(u_austrcpy(cBuffer, expectedResult));
                    uBufferLenReturned = u_sscanf_u(argument, format, &flt);
                    //uFileBufferLenReturned = u_fscanf_u(testFile, format, flt);
                    if (flt != expectedFlt) {
                        log_err("error in scanf test case[%d] Got: %f Exp: %f\n",
                                i, flt, expectedFlt);
                    }
                    break;
                case 0x31:  // '1' int8_t
                    expected8 = (int8_t)uto64(expectedResult);
                    uBufferLenReturned = u_sscanf_u(argument, format, &i8);
                    //uFileBufferLenReturned = u_fscanf_u(testFile, format, i8);
                    if (i8 != expected8) {
                        log_err("error in scanf test case[%d] Got: %02X Exp: %02X\n",
                                i, i8, expected8);
                    }
                    break;
                case 0x32:  // '2' int16_t
                    expected16 = (int16_t)uto64(expectedResult);
                    uBufferLenReturned = u_sscanf_u(argument, format, &i16);
                    //uFileBufferLenReturned = u_fscanf_u(testFile, format, i16);
                    if (i16 != expected16) {
                        log_err("error in scanf test case[%d] Got: %04X Exp: %04X\n",
                                i, i16, expected16);
                    }
                    break;
                case 0x34:  // '4' int32_t
                    expected32 = (int32_t)uto64(expectedResult);
                    uBufferLenReturned = u_sscanf_u(argument, format, &i32);
                    //uFileBufferLenReturned = u_fscanf_u(testFile, format, i32);
                    if (i32 != expected32) {
                        log_err("error in scanf test case[%d] Got: %08X Exp: %08X\n",
                                i, i32, expected32);
                    }
                    break;
                case 0x38:  // '8' int64_t
                    expected64 = uto64(expectedResult);
                    uBufferLenReturned = u_sscanf_u(argument, format, &i64);
                    //uFileBufferLenReturned = u_fscanf_u(testFile, format, i64);
                    if (i64 != expected64) {
                        log_err("error in scanf 64-bit. Test case = %d\n", i);
                    }
                    break;
                case 0x73:  // 's' char *
                    u_austrcpy(cExpected, expectedResult);
                    uBufferLenReturned = u_sscanf_u(argument, format, cBuffer);
                    //uFileBufferLenReturned = u_fscanf_u(testFile, format, cBuffer);
                    if (strcmp(cBuffer, cExpected) != 0) {
                        log_err("error in scanf char * string. Got \"%s\" Expected \"%s\". Test case = %d\n", cBuffer, cExpected, i);
                    }
                    break;
                case 0x53:  // 'S' char16_t *
                    uBufferLenReturned = u_sscanf_u(argument, format, uBuffer);
                    //uFileBufferLenReturned = u_fscanf_u(testFile, format, argument);
                    if (u_strcmp(uBuffer, expectedResult) != 0) {
                        u_austrcpy(cExpected, format);
                        u_austrcpy(cBuffer, uBuffer);
                        log_err("error in scanf char16_t * string %s Got: \"%s\". Test case = %d\n", cExpected, cBuffer, i);
                    }
                    break;
                default:
                    uBufferLenReturned = 0;
                    //uFileBufferLenReturned = 0;
                    log_err("Unknown type %c for test %d\n", testCase->getString("argumentType", errorCode)[0], i);
                }
                if (uBufferLenReturned != 1) {
                    log_err("error scanf converted %d arguments. Test case = %d\n", uBufferLenReturned, i);
                }
/*                if (u_strcmp(uBuffer, expectedResult) != 0) {
                    u_austrncpy(cBuffer, uBuffer, sizeof(cBuffer));
                    u_austrncpy(cFormat, format, sizeof(cFormat));
                    u_austrncpy(cExpected, expectedResult, sizeof(cExpected));
                    cBuffer[sizeof(cBuffer)-1] = 0;
                    log_err("FAILURE string test case %d \"%s\" - Got: \"%s\" Expected: \"%s\"\n",
                            i, cFormat, cBuffer, cExpected);
                }
                if (uBuffer[uBufferLenReturned-1] == 0
                    || uBuffer[uBufferLenReturned] != 0
                    || uBuffer[uBufferLenReturned+1] != 0x2A
                    || uBuffer[uBufferLenReturned+2] != 0x2A)
                {
                    u_austrncpy(cBuffer, uBuffer, sizeof(cBuffer));
                    cBuffer[sizeof(cBuffer)-1] = 0;
                    log_err("FAILURE test case %d - \"%s\" wrong amount of characters was written. Got %d.\n",
                            i, cBuffer, uBufferLenReturned);
                }*/
/*                u_fclose(testFile);
                testFile = u_fopen(STANDARD_TEST_FILE, "r", fileLocale, "UTF-8");
                if (!testFile) {
                    log_err("Can't open test file - %s\n",
                            STANDARD_TEST_FILE);
                }
                uBuffer[0];
                u_fgets(uBuffer, UPRV_LENGTHOF(uBuffer), testFile);
                if (u_strcmp(uBuffer, expectedResult) != 0) {
                    u_austrncpy(cBuffer, uBuffer, sizeof(cBuffer));
                    u_austrncpy(cFormat, format, sizeof(cFormat));
                    u_austrncpy(cExpected, expectedResult, sizeof(cExpected));
                    cBuffer[sizeof(cBuffer)-1] = 0;
                    log_err("FAILURE file test case %d \"%s\" - Got: \"%s\" Expected: \"%s\"\n",
                            i, cFormat, cBuffer, cExpected);
                }
                if (uFileBufferLenReturned != uBufferLenReturned)
                {
                    u_austrncpy(cBuffer, uBuffer, sizeof(cBuffer));
                    cBuffer[sizeof(cBuffer)-1] = 0;
                    log_err("FAILURE uFileBufferLenReturned(%d) != uBufferLenReturned(%d)\n",
                            uFileBufferLenReturned, uBufferLenReturned);
                }
*/
                if(U_FAILURE(errorCode)) {
                    log_err("error running icuio/printf test case %d - %s\n",
                            i, u_errorName(errorCode));
                    errorCode=U_ZERO_ERROR;
                    continue;
                }
//                u_fclose(testFile);
            }
            delete testData;
        }
        delete dataModule;
    }
    else {
        log_data_err("Failed: could not load test icuio data\n");
    }
#endif
}
U_CDECL_END

U_CDECL_BEGIN
static void U_CALLCONV DataDrivenPrintfPrecision()
{
#if !UCONFIG_NO_FORMATTING && !UCONFIG_NO_FILE_IO
    UErrorCode errorCode;
    TestDataModule *dataModule;
    TestData *testData;
    const DataMap *testCase;
    DataDrivenLogger logger;
    char16_t uBuffer[512];
    char cBuffer[512];
    char cFormat[sizeof(cBuffer)];
    char cExpected[sizeof(cBuffer)];
    UnicodeString tempStr;
    char16_t format[512];
    char16_t expectedResult[512];
    char16_t argument[512];
    int32_t precision;
    int32_t i;
    int8_t i8;
    int16_t i16;
    int32_t i32;
    int64_t i64;
    double dbl;
    int32_t uBufferLenReturned;

    errorCode=U_ZERO_ERROR;
    dataModule=TestDataModule::getTestDataModule("icuio", logger, errorCode);
    if(U_SUCCESS(errorCode)) {
        testData=dataModule->createTestData("printfPrecision", errorCode);
        if(U_SUCCESS(errorCode)) {
            for(i=0; testData->nextCase(testCase, errorCode); ++i) {
                if(U_FAILURE(errorCode)) {
                    log_err("error retrieving icuio/printf test case %d - %s\n",
                            i, u_errorName(errorCode));
                    errorCode=U_ZERO_ERROR;
                    continue;
                }
                u_memset(uBuffer, 0x2A, UPRV_LENGTHOF(uBuffer));
                uBuffer[UPRV_LENGTHOF(uBuffer)-1] = 0;
                tempStr=testCase->getString("format", errorCode);
                tempStr.extract(format, UPRV_LENGTHOF(format), errorCode);
                tempStr=testCase->getString("result", errorCode);
                tempStr.extract(expectedResult, UPRV_LENGTHOF(expectedResult), errorCode);
                tempStr=testCase->getString("argument", errorCode);
                tempStr.extract(argument, UPRV_LENGTHOF(argument), errorCode);
                precision=testCase->getInt28("precision", errorCode);
                u_austrncpy(cBuffer, format, sizeof(cBuffer));
                if(U_FAILURE(errorCode)) {
                    log_err("error retrieving icuio/printf test case %d - %s\n",
                            i, u_errorName(errorCode));
                    errorCode=U_ZERO_ERROR;
                    continue;
                }
                log_verbose("Test %d: format=\"%s\"\n", i, cBuffer);
                switch (testCase->getString("argumentType", errorCode)[0]) {
                case 0x64:  // 'd' double
                    dbl = atof(u_austrcpy(cBuffer, argument));
                    uBufferLenReturned = u_sprintf_u(uBuffer, format, precision, dbl);
                    break;
                case 0x31:  // '1' int8_t
                    i8 = (int8_t)uto64(argument);
                    uBufferLenReturned = u_sprintf_u(uBuffer, format, precision, i8);
                    break;
                case 0x32:  // '2' int16_t
                    i16 = (int16_t)uto64(argument);
                    uBufferLenReturned = u_sprintf_u(uBuffer, format, precision, i16);
                    break;
                case 0x34:  // '4' int32_t
                    i32 = (int32_t)uto64(argument);
                    uBufferLenReturned = u_sprintf_u(uBuffer, format, precision, i32);
                    break;
                case 0x38:  // '8' int64_t
                    i64 = uto64(argument);
                    uBufferLenReturned = u_sprintf_u(uBuffer, format, precision, i64);
                    break;
                case 0x73:  // 's' char *
                    u_austrncpy(cBuffer, uBuffer, sizeof(cBuffer));
                    uBufferLenReturned = u_sprintf_u(uBuffer, format, precision, cBuffer);
                    break;
                case 0x53:  // 'S' char16_t *
                    uBufferLenReturned = u_sprintf_u(uBuffer, format, precision, argument);
                    break;
                default:
                    uBufferLenReturned = 0;
                    log_err("Unknown type %c for test %d\n", testCase->getString("argumentType", errorCode)[0], i);
                }
                if (u_strcmp(uBuffer, expectedResult) != 0) {
                    u_austrncpy(cBuffer, uBuffer, sizeof(cBuffer));
                    u_austrncpy(cFormat, format, sizeof(cFormat));
                    u_austrncpy(cExpected, expectedResult, sizeof(cExpected));
                    cBuffer[sizeof(cBuffer)-1] = 0;
                    log_err("FAILURE test case %d \"%s\" - Got: \"%s\" Expected: \"%s\"\n",
                            i, cFormat, cBuffer, cExpected);
                }
                if (uBufferLenReturned <= 0) {
                    log_err("FAILURE test case %d - \"%s\" is an empty string.\n",
                            i, cBuffer);
                }
                else if (uBuffer[uBufferLenReturned-1] == 0
                    || uBuffer[uBufferLenReturned] != 0
                    || uBuffer[uBufferLenReturned+1] != 0x2A
                    || uBuffer[uBufferLenReturned+2] != 0x2A)
                {
                    u_austrncpy(cBuffer, uBuffer, sizeof(cBuffer));
                    cBuffer[sizeof(cBuffer)-1] = 0;
                    log_err("FAILURE test case %d - \"%s\" wrong amount of characters was written. Got %d.\n",
                            i, cBuffer, uBufferLenReturned);
                }
                if(U_FAILURE(errorCode)) {
                    log_err("error running icuio/printf test case %d - %s\n",
                            i, u_errorName(errorCode));
                    errorCode=U_ZERO_ERROR;
                    continue;
                }
            }
            delete testData;
        }
        delete dataModule;
    }
    else {
        log_data_err("Failed: could not load test icuio data\n");
    }
#endif
}
U_CDECL_END

static void addAllTests(TestNode** root) {
    addFileTest(root);
    addStringTest(root);
    addTranslitTest(root);

#if !UCONFIG_NO_FORMATTING && !UCONFIG_NO_LEGACY_CONVERSION
    addTest(root, &DataDrivenPrintf, "datadriv/DataDrivenPrintf");
    addTest(root, &DataDrivenPrintfPrecision, "datadriv/DataDrivenPrintfPrecision");
    addTest(root, &DataDrivenScanf, "datadriv/DataDrivenScanf");
    addTest(root, &ScanfMultipleIntegers, "ScanfMultipleIntegers");
#endif
    addStreamTests(root);
}

/* returns the path to icu/source/data/out */
static const char *ctest_dataOutDir()
{
    static const char *dataOutDir = nullptr;

    if(dataOutDir) {
        return dataOutDir;
    }

    /* U_TOPBUILDDIR is set by the makefiles on UNIXes when building cintltst and intltst
    //              to point to the top of the build hierarchy, which may or
    //              may not be the same as the source directory, depending on
    //              the configure options used.  At any rate,
    //              set the data path to the built data from this directory.
    //              The value is complete with quotes, so it can be used
    //              as-is as a string constant.
    */
#if defined (U_TOPBUILDDIR)
    {
        dataOutDir = U_TOPBUILDDIR "data" U_FILE_SEP_STRING "out" U_FILE_SEP_STRING;
    }
#else

    /* On Windows, the file name obtained from __FILE__ includes a full path.
     *             This file is "wherever\icu\source\test\cintltst\cintltst.c"
     *             Change to    "wherever\icu\source\data"
     */
    {
        static char p[sizeof(__FILE__) + 20];
        char *pBackSlash;
        int i;

        strcpy(p, __FILE__);
        /* We want to back over three '\' chars.                            */
        /*   Only Windows should end up here, so looking for '\' is safe.   */
        for (i=1; i<=3; i++) {
            pBackSlash = strrchr(p, U_FILE_SEP_CHAR);
            if (pBackSlash != nullptr) {
                *pBackSlash = 0;        /* Truncate the string at the '\'   */
            }
        }

        if (pBackSlash != nullptr) {
            /* We found and truncated three names from the path.
             *  Now append "source\data" and set the environment
             */
            strcpy(pBackSlash, U_FILE_SEP_STRING "data" U_FILE_SEP_STRING "out" U_FILE_SEP_STRING);
            dataOutDir = p;
        }
        else {
            /* __FILE__ on MSVC7 does not contain the directory */
            FILE *file = fopen(".." U_FILE_SEP_STRING ".." U_FILE_SEP_STRING "data" U_FILE_SEP_STRING "Makefile.in", "r");
            if (file) {
                fclose(file);
                dataOutDir = ".." U_FILE_SEP_STRING ".." U_FILE_SEP_STRING "data" U_FILE_SEP_STRING "out" U_FILE_SEP_STRING;
            }
            else {
                dataOutDir = ".." U_FILE_SEP_STRING ".." U_FILE_SEP_STRING ".." U_FILE_SEP_STRING "data" U_FILE_SEP_STRING "out" U_FILE_SEP_STRING;
            }
        }
    }
#endif

    return dataOutDir;
}

/*  ctest_setICU_DATA  - if the ICU_DATA environment variable is not already
 *                       set, try to deduce the directory in which ICU was built,
 *                       and set ICU_DATA to "icu/source/data" in that location.
 *                       The intent is to allow the tests to have a good chance
 *                       of running without requiring that the user manually set
 *                       ICU_DATA.  Common data isn't a problem, since it is
 *                       picked up via a static (build time) reference, but the
 *                       tests dynamically load some data.
 */
static void ctest_setICU_DATA() {

    /* No location for the data dir was identifiable.
     *   Add other fallbacks for the test data location here if the need arises
     */
    if (getenv("ICU_DATA") == nullptr) {
        /* If ICU_DATA isn't set, set it to the usual location */
        u_setDataDirectory(ctest_dataOutDir());
    }
}

U_CDECL_BEGIN
/*
 * Note: this assumes that context is a pointer to STANDARD_TEST_FILE. It would be
 * cleaner to define an actual context with a string pointer in it and set STANDARD_TEST_FILE
 * after the call to initArgs()...
 */
static int U_CALLCONV argHandler(int arg, int /*argc*/, const char * const argv[], void *context)
{
    const char **str = (const char **) context;

    if (argv[arg][0] != '/' && argv[arg][0] != '-') {
        *str = argv[arg];
        return 1;
    }

    return 0;
}
U_CDECL_END

int main(int argc, char* argv[])
{
    int32_t nerrors = 0;
    TestNode *root = nullptr;
    UErrorCode errorCode = U_ZERO_ERROR;
    UDate startTime, endTime;
    int32_t diffTime;

    startTime = uprv_getRawUTCtime();

    /* Check whether ICU will initialize without forcing the build data directory into
    *  the ICU_DATA path.  Success here means either the data dll contains data, or that
    *  this test program was run with ICU_DATA set externally.  Failure of this check
    *  is normal when ICU data is not packaged into a shared library.
    *
    *  Whether or not this test succeeds, we want to cleanup and reinitialize
    *  with a data path so that data loading from individual files can be tested.
    */
    u_init(&errorCode);
    if (U_FAILURE(errorCode)) {
        fprintf(stderr,
            "#### Note:  ICU Init without build-specific setDataDirectory() failed.\n");
    }
    u_cleanup();
    errorCode = U_ZERO_ERROR;
    if (!initArgs(argc, argv, argHandler, (void *) &STANDARD_TEST_FILE)) {
        /* Error already displayed. */
        return -1;
    }

    /* Initialize ICU */
    ctest_setICU_DATA();    /* u_setDataDirectory() must happen Before u_init() */
    u_init(&errorCode);
    if (U_FAILURE(errorCode)) {
        fprintf(stderr,
            "#### ERROR! %s: u_init() failed with status = \"%s\".\n"
            "*** Check the ICU_DATA environment variable and \n"
            "*** check that the data files are present.\n", argv[0], u_errorName(errorCode));
        return 1;
    }

    fprintf(stdout, "Default charset for this run is %s\n", ucnv_getDefaultName());

    addAllTests(&root);
    nerrors = runTestRequest(root, argc, argv);

#if 1
    static const char* filenamesToRemove[] = { STANDARD_TEST_FILE, MEDIUMNAME_TEST_FILE, LONGNAME_TEST_FILE, nullptr };
    const char** filenamesToRemovePtr = filenamesToRemove;
    const char* filenameToRemove;
    while ((filenameToRemove = *filenamesToRemovePtr++) != nullptr)
    {

        FILE* fileToRemove = fopen(filenameToRemove, "r");
        /* This should delete any temporary files. */
        if (fileToRemove) {
            fclose(fileToRemove);
            log_verbose("Deleting: %s\n", filenameToRemove);
            if (remove(filenameToRemove) != 0) {
                /* Maybe someone didn't close the file correctly. */
                fprintf(stderr, "FAIL: Could not delete %s\n", filenameToRemove);
                nerrors += 1;
            }
        }
    }
#endif

    cleanUpTestTree(root);
    DataDrivenLogger::cleanUp();
    u_cleanup();

    endTime = uprv_getRawUTCtime();
    diffTime = (int32_t)(endTime - startTime);
    printf("Elapsed Time: %02d:%02d:%02d.%03d\n",
        (int)((diffTime%U_MILLIS_PER_DAY)/U_MILLIS_PER_HOUR),
        (int)((diffTime%U_MILLIS_PER_HOUR)/U_MILLIS_PER_MINUTE),
        (int)((diffTime%U_MILLIS_PER_MINUTE)/U_MILLIS_PER_SECOND),
        (int)(diffTime%U_MILLIS_PER_SECOND));

    return nerrors;
}
