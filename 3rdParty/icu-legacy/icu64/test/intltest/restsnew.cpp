// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * Copyright (c) 1997-2016, International Business Machines
 * Corporation and others. All Rights Reserved.
 ********************************************************************/

#include "unicode/utypes.h"

#include "charstr.h"
#include "cmemory.h"
#include "cstring.h"
#include "unicode/unistr.h"
#include "unicode/resbund.h"
#include "unicode/brkiter.h"
#include "unicode/utrace.h"
#include "unicode/ucurr.h"
#include "uresimp.h"
#include "restsnew.h"

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <limits.h>
#include <vector>
#include <string>

//***************************************************************************************

void NewResourceBundleTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite ResourceBundleTest: ");
    TESTCASE_AUTO_BEGIN;

#if !UCONFIG_NO_FILE_IO && !UCONFIG_NO_LEGACY_CONVERSION
    TESTCASE_AUTO(TestResourceBundles);
    TESTCASE_AUTO(TestConstruction);
    TESTCASE_AUTO(TestIteration);
    TESTCASE_AUTO(TestOtherAPI);
    TESTCASE_AUTO(TestNewTypes);
#endif

    TESTCASE_AUTO(TestGetByFallback);
    TESTCASE_AUTO(TestFilter);
    TESTCASE_AUTO(TestIntervalAliasFallbacks);

#if U_ENABLE_TRACING
    TESTCASE_AUTO(TestTrace);
#endif

    TESTCASE_AUTO(TestOpenDirectFillIn);
    TESTCASE_AUTO(TestStackReuse);
    TESTCASE_AUTO_END;
}

//***************************************************************************************

static const char16_t kErrorUChars[] = { 0x45, 0x52, 0x52, 0x4f, 0x52, 0 };
static const int32_t kErrorLength = 5;
static const int32_t kERROR_COUNT = -1234567;

//***************************************************************************************

enum E_Where
{
    e_Root,
    e_te,
    e_te_IN,
    e_Where_count
};

//***************************************************************************************

#define CONFIRM_EQ(actual,expected) UPRV_BLOCK_MACRO_BEGIN { \
    if ((expected)==(actual)) { \
        record_pass(); \
    } else { \
        record_fail(); \
        errln(action + (UnicodeString)" returned " + (actual) + (UnicodeString)" instead of " + (expected)); \
    } \
} UPRV_BLOCK_MACRO_END
#define CONFIRM_GE(actual,expected) UPRV_BLOCK_MACRO_BEGIN { \
    if ((actual)>=(expected)) { \
        record_pass(); \
    } else { \
        record_fail(); \
        errln(action + (UnicodeString)" returned " + (actual) + (UnicodeString)" instead of x >= " + (expected)); \
    } \
} UPRV_BLOCK_MACRO_END
#define CONFIRM_NE(actual,expected) UPRV_BLOCK_MACRO_BEGIN { \
    if ((expected)!=(actual)) { \
        record_pass(); \
    } else { \
        record_fail(); \
        errln(action + (UnicodeString)" returned " + (actual) + (UnicodeString)" instead of x != " + (expected)); \
    } \
} UPRV_BLOCK_MACRO_END

#define CONFIRM_UErrorCode(actual,expected) UPRV_BLOCK_MACRO_BEGIN { \
    if ((expected)==(actual)) { \
        record_pass(); \
    } else { \
        record_fail(); \
        errln(action + (UnicodeString)" returned " + (UnicodeString)u_errorName(actual) + (UnicodeString)" instead of " + (UnicodeString)u_errorName(expected)); \
    } \
} UPRV_BLOCK_MACRO_END

//***************************************************************************************

/**
 * Convert an integer, positive or negative, to a character string radix 10.
 */
static char*
itoa(int32_t i, char* buf)
{
    char* result = buf;

    // Handle negative
    if (i < 0)
    {
        *buf++ = '-';
        i = -i;
    }

    // Output digits in reverse order
    char* p = buf;
    do
    {
        *p++ = (char)('0' + (i % 10));
        i /= 10;
    }
    while (i);
    *p-- = 0;

    // Reverse the string
    while (buf < p)
    {
        char c = *buf;
        *buf++ = *p;
        *p-- = c;
    }

    return result;
}



//***************************************************************************************

// Array of our test objects

static struct
{
    const char* name;
    Locale *locale;
    UErrorCode expected_constructor_status;
    E_Where where;
    UBool like[e_Where_count];
    UBool inherits[e_Where_count];
}
param[] =
{
    // "te" means test
    // "IN" means inherits
    // "NE" or "ne" means "does not exist"

    { "root",       nullptr,   U_ZERO_ERROR,             e_Root,      { true, false, false }, { true, false, false } },
    { "te",         nullptr,   U_ZERO_ERROR,             e_te,        { false, true, false }, { true, true, false  } },
    { "te_IN",      nullptr,   U_ZERO_ERROR,             e_te_IN,     { false, false, true }, { true, true, true   } },
    { "te_NE",      nullptr,   U_USING_FALLBACK_WARNING, e_te,        { false, true, false }, { true, true, false  } },
    { "te_IN_NE",   nullptr,   U_USING_FALLBACK_WARNING, e_te_IN,     { false, false, true }, { true, true, true   } },
    { "ne",         nullptr,   U_USING_DEFAULT_WARNING,  e_Root,      { true, false, false }, { true, false, false } }
};

static int32_t bundles_count = UPRV_LENGTHOF(param);

//***************************************************************************************

/**
 * Return a random unsigned long l where 0N <= l <= ULONG_MAX.
 */

static uint32_t
randul()
{
    static UBool initialized = false;
    if (!initialized)
    {
        srand((unsigned)time(nullptr));
        initialized = true;
    }
    // Assume rand has at least 12 bits of precision
    uint32_t l = 0;
    for (uint32_t i=0; i<sizeof(l); ++i)
        ((char*)&l)[i] = (char)((rand() & 0x0FF0) >> 4);
    return l;
}

/**
 * Return a random double x where 0.0 <= x < 1.0.
 */
static double
randd()
{
    return (double)(randul() / ULONG_MAX);
}

/**
 * Return a random integer i where 0 <= i < n.
 */
static int32_t randi(int32_t n)
{
    return (int32_t)(randd() * n);
}

//***************************************************************************************

/*
 Don't use more than one of these at a time because of the Locale names
*/
NewResourceBundleTest::NewResourceBundleTest()
: pass(0),
  fail(0)
{
    if (param[5].locale == nullptr) {
        param[0].locale = new Locale("root");
        param[1].locale = new Locale("te");
        param[2].locale = new Locale("te", "IN");
        param[3].locale = new Locale("te", "NE");
        param[4].locale = new Locale("te", "IN", "NE");
        param[5].locale = new Locale("ne");
    }
}

NewResourceBundleTest::~NewResourceBundleTest()
{
    if (param[5].locale) {
        int idx;
        for (idx = 0; idx < UPRV_LENGTHOF(param); idx++) {
            delete param[idx].locale;
            param[idx].locale = nullptr;
        }
    }
}

//***************************************************************************************

void
NewResourceBundleTest::TestResourceBundles()
{
    UErrorCode status = U_ZERO_ERROR;
    loadTestData(status);
    if(U_FAILURE(status))
    {
        dataerrln("Could not load testdata.dat %s " + UnicodeString(u_errorName(status)));
        return;
    }

    /* Make sure that users using te_IN for the default locale don't get test failures. */
    Locale originalDefault;
    if (Locale::getDefault() == Locale("te_IN")) {
        Locale::setDefault(Locale("en_US"), status);
    }

    testTag("only_in_Root", true, false, false);
    testTag("only_in_te", false, true, false);
    testTag("only_in_te_IN", false, false, true);
    testTag("in_Root_te", true, true, false);
    testTag("in_Root_te_te_IN", true, true, true);
    testTag("in_Root_te_IN", true, false, true);
    testTag("in_te_te_IN", false, true, true);
    testTag("nonexistent", false, false, false);
    logln("Passed: %d\nFailed: %d", pass, fail);

    /* Restore the default locale for the other tests. */
    Locale::setDefault(originalDefault, status);
}

void
NewResourceBundleTest::TestConstruction()
{
    UErrorCode   err = U_ZERO_ERROR;
    Locale       locale("te", "IN");

    const char* testdatapath;
    testdatapath=loadTestData(err);
    if(U_FAILURE(err))
    {
        dataerrln("Could not load testdata.dat %s " + UnicodeString(u_errorName(err)));
        return;
    }

    /* Make sure that users using te_IN for the default locale don't get test failures. */
    Locale originalDefault;
    if (Locale::getDefault() == Locale("te_IN")) {
        Locale::setDefault(Locale("en_US"), err);
    }

    ResourceBundle  test1((UnicodeString)testdatapath, err);
    ResourceBundle  test2(testdatapath, locale, err);
    
    UnicodeString   result1;
    UnicodeString   result2;

    result1 = test1.getStringEx("string_in_Root_te_te_IN", err);
    result2 = test2.getStringEx("string_in_Root_te_te_IN", err);
    if (U_FAILURE(err)) {
        errln("Something threw an error in TestConstruction()");
        return;
    }

    logln("for string_in_Root_te_te_IN, root.txt had " + result1);
    logln("for string_in_Root_te_te_IN, te_IN.txt had " + result2);

    if (result1 != "ROOT" || result2 != "TE_IN") {
        errln("Construction test failed; run verbose for more information");
    }

    const char* version1;
    const char* version2;

    version1 = test1.getVersionNumber();
    version2 = test2.getVersionNumber();

    char *versionID1 = new char[1 + strlen(U_ICU_VERSION) + strlen(version1)]; // + 1 for zero byte
    char *versionID2 = new char[1 + strlen(U_ICU_VERSION) + strlen(version2)]; // + 1 for zero byte

    strcpy(versionID1, "45.0");  // hardcoded, please change if the default.txt file or ResourceBundle::kVersionSeparater is changed.

    strcpy(versionID2, "55.0");  // hardcoded, please change if the te_IN.txt file or ResourceBundle::kVersionSeparater is changed.

    logln(UnicodeString("getVersionNumber on default.txt returned ") + version1 + UnicodeString(" Expect: " ) + versionID1);
    logln(UnicodeString("getVersionNumber on te_IN.txt returned ") + version2 + UnicodeString(" Expect: " ) + versionID2);

    if (strcmp(version1, versionID1) != 0) {
        errln("getVersionNumber(version1) failed. %s != %s", version1, versionID1);
    }
    if (strcmp(version2, versionID2) != 0) {
        errln("getVersionNumber(version2) failed. %s != %s", version2, versionID2);
    }
    delete[] versionID1;
    delete[] versionID2;

    /* Restore the default locale for the other tests. */
    Locale::setDefault(originalDefault, err);
}

void
NewResourceBundleTest::TestIteration()
{
    UErrorCode   err = U_ZERO_ERROR;
    const char* testdatapath;
    const char* data[]={
        "string_in_Root_te_te_IN",   "1",
        "array_in_Root_te_te_IN",    "5",
        "array_2d_in_Root_te_te_IN", "4",
    };

    Locale       *locale=new Locale("te_IN");

    testdatapath=loadTestData(err);
    if(U_FAILURE(err))
    {
        dataerrln("Could not load testdata.dat %s " + UnicodeString(u_errorName(err)));
        return;
    }

    ResourceBundle  test1(testdatapath, *locale, err);
    if(U_FAILURE(err)){
        errln("Construction failed");
    }
    uint32_t i;
    int32_t count, row=0, col=0;
    char buf[5];
    UnicodeString expected;
    UnicodeString element("TE_IN");
    UnicodeString action;


    for(i=0; i<UPRV_LENGTHOF(data); i=i+2){
        action = "te_IN";
        action +=".get(";
        action += data[i];
        action +=", err)";
        err=U_ZERO_ERROR;
        ResourceBundle bundle = test1.get(data[i], err); 
        if(!U_FAILURE(err)){
            action = "te_IN";
            action +=".getKey()";

            CONFIRM_EQ((UnicodeString)bundle.getKey(), (UnicodeString)data[i]);

            count=0;
            row=0;
            while(bundle.hasNext()){
                action = data[i];
                action +=".getNextString(err)";
                row=count;   
                UnicodeString got=bundle.getNextString(err);
                if(U_SUCCESS(err)){
                    expected=element;
                    if(bundle.getSize() > 1){
                        CONFIRM_EQ(bundle.getType(), URES_ARRAY);
                        expected+=itoa(row, buf);
                        ResourceBundle rowbundle=bundle.get(row, err);
                        if(!U_FAILURE(err) && rowbundle.getSize()>1){
                            col=0;
                            while(rowbundle.hasNext()){
                                expected=element;
                                got=rowbundle.getNextString(err);
                                if(!U_FAILURE(err)){
                                    expected+=itoa(row, buf);
                                    expected+=itoa(col, buf);
                                    col++;
                                    CONFIRM_EQ(got, expected);
                                }
                            }
                            CONFIRM_EQ(col, rowbundle.getSize());
                        }
                    }
                    else{
                        CONFIRM_EQ(bundle.getType(), (int32_t)URES_STRING);
                    }
                }
                CONFIRM_EQ(got, expected);
                count++;
            }
            action = data[i];
            action +=".getSize()";
            CONFIRM_EQ(bundle.getSize(), count);
            CONFIRM_EQ(count, atoi(data[i+1]));
            //after reaching the end
            err=U_ZERO_ERROR;
            ResourceBundle errbundle=bundle.getNext(err);
            action = "After reaching the end of the Iterator:-  ";
            action +=data[i];
            action +=".getNext()";
            CONFIRM_NE(err, (int32_t)U_ZERO_ERROR);
            CONFIRM_EQ(u_errorName(err), u_errorName(U_INDEX_OUTOFBOUNDS_ERROR));
            //reset the iterator
            err = U_ZERO_ERROR;
            bundle.resetIterator();
         /*  The following code is causing a crash
         ****CRASH******
         */

            bundle.getNext(err);
            if(U_FAILURE(err)){
                errln("ERROR: getNext()  throw an error");
            }
        }
    }
    delete locale;
}

// TODO: add operator== and != to ResourceBundle
static UBool
equalRB(ResourceBundle &a, ResourceBundle &b) {
    UResType type;
    UErrorCode status;

    type=a.getType();
    status=U_ZERO_ERROR;
    return
        type==b.getType() &&
        a.getLocale()==b.getLocale() &&
        0==strcmp(a.getName(), b.getName()) &&
        type==URES_STRING ?
            a.getString(status)==b.getString(status) :
            type==URES_INT ?
                a.getInt(status)==b.getInt(status) :
                true;
}

void
NewResourceBundleTest::TestOtherAPI(){
    UErrorCode   err = U_ZERO_ERROR;
    const char* testdatapath=loadTestData(err);
    UnicodeString tDataPathUS = UnicodeString(testdatapath, "");

    if(U_FAILURE(err))
    {
        dataerrln("Could not load testdata.dat %s " + UnicodeString(u_errorName(err)));
        return;
    }

    /* Make sure that users using te_IN for the default locale don't get test failures. */
    Locale originalDefault;
    if (Locale::getDefault() == Locale("te_IN")) {
        Locale::setDefault(Locale("en_US"), err);
    }

    Locale       *locale=new Locale("te_IN");

    ResourceBundle test0(tDataPathUS, *locale, err);
    if(U_FAILURE(err)){
        errln("Construction failed");
        return;
    }

    ResourceBundle  test1(testdatapath, *locale, err);
    if(U_FAILURE(err)){
        errln("Construction failed");
        return;
    }

    logln("Testing getLocale()\n");
    if(strcmp(test1.getLocale().getName(), locale->getName()) !=0 ){
        errln("FAIL: ResourceBundle::getLocale() failed\n");
    }

    delete locale;

    logln("Testing ResourceBundle(UErrorCode)\n");
    ResourceBundle defaultresource(err);
    ResourceBundle explicitdefaultresource(nullptr, Locale::getDefault(), err);
    if(U_FAILURE(err)){
        errcheckln(err, "Construction of default resourcebundle failed - %s", u_errorName(err));
        return;
    }
    // You can't compare the default locale to the resolved locale in the
    // resource bundle due to aliasing, keywords in the default locale
    // or the chance that the machine running these tests is using a locale
    // that isn't available in ICU.
    if(strcmp(defaultresource.getLocale().getName(), explicitdefaultresource.getLocale().getName()) != 0){
        errln("Construction of default resourcebundle didn't take the defaultlocale. Expected %s Got %s err=%s\n",
            explicitdefaultresource.getLocale().getName(), defaultresource.getLocale().getName(), u_errorName(err));
    }
    

    ResourceBundle copyRes(defaultresource);
    if(strcmp(copyRes.getName(), defaultresource.getName() ) !=0  ||
        strcmp(test1.getName(), defaultresource.getName() ) ==0 ||
        strcmp(copyRes.getLocale().getName(), defaultresource.getLocale().getName() ) !=0  ||
        strcmp(test1.getLocale().getName(), defaultresource.getLocale().getName() ) ==0 )
    {
        errln("copy construction failed\n");
    }

    {
        LocalPointer<ResourceBundle> p(defaultresource.clone());
        if(p.getAlias() == &defaultresource || !equalRB(*p, defaultresource)) {
            errln("ResourceBundle.clone() failed");
        }
    }

    // The following tests involving defaultSub may no longer be exercised if
    // defaultresource is for a locale like en_US with an empty resource bundle.
    // (Before ICU-21028 such a bundle would have contained at least a Version string.)
    if(defaultresource.getSize() != 0) {
        ResourceBundle defaultSub = defaultresource.get((int32_t)0, err);
        ResourceBundle defSubCopy(defaultSub);
        if(strcmp(defSubCopy.getName(), defaultSub.getName()) != 0 ||
                strcmp(defSubCopy.getLocale().getName(), defaultSub.getLocale().getName() ) != 0) {
            errln("copy construction for subresource failed\n");
        }
        LocalPointer<ResourceBundle> p(defaultSub.clone());
        if(p.getAlias() == &defaultSub || !equalRB(*p, defaultSub)) {
            errln("2nd ResourceBundle.clone() failed");
        }
    }

    UVersionInfo ver;
    copyRes.getVersion(ver);

    logln("Version returned: [%d.%d.%d.%d]\n", ver[0], ver[1], ver[2], ver[3]);

    logln("Testing C like UnicodeString APIs\n");

    UResourceBundle *testCAPI = nullptr, *bundle = nullptr, *rowbundle = nullptr, *temp = nullptr;
    err = U_ZERO_ERROR;
    const char* data[]={
        "string_in_Root_te_te_IN",   "1",
        "array_in_Root_te_te_IN",    "5",
        "array_2d_in_Root_te_te_IN", "4",
    };


    testCAPI = ures_open(testdatapath, "te_IN", &err);

    if(U_SUCCESS(err)) {
        // Do the testing
        // first iteration

        uint32_t i;
        int32_t count, row=0, col=0;
        char buf[5];
        UnicodeString expected;
        UnicodeString element("TE_IN");
        UnicodeString action;


        for(i=0; i<UPRV_LENGTHOF(data); i=i+2){
            action = "te_IN";
            action +=".get(";
            action += data[i];
            action +=", err)";
            err=U_ZERO_ERROR;
            bundle = ures_getByKey(testCAPI, data[i], bundle, &err); 
            if(!U_FAILURE(err)){
                const char* key = nullptr;
                action = "te_IN";
                action +=".getKey()";

                CONFIRM_EQ((UnicodeString)ures_getKey(bundle), (UnicodeString)data[i]);

                count=0;
                row=0;
                while(ures_hasNext(bundle)){
                    action = data[i];
                    action +=".getNextString(err)";
                    row=count;   
                    UnicodeString got=ures_getNextUnicodeString(bundle, &key, &err);
                    if(U_SUCCESS(err)){
                        expected=element;
                        if(ures_getSize(bundle) > 1){
                            CONFIRM_EQ(ures_getType(bundle), URES_ARRAY);
                            expected+=itoa(row, buf);
                            rowbundle=ures_getByIndex(bundle, row, rowbundle, &err);
                            if(!U_FAILURE(err) && ures_getSize(rowbundle)>1){
                                col=0;
                                while(ures_hasNext(rowbundle)){
                                    expected=element;
                                    got=ures_getNextUnicodeString(rowbundle, &key, &err);
                                    temp = ures_getByIndex(rowbundle, col, temp, &err);
                                    UnicodeString bla = ures_getUnicodeString(temp, &err);
                                    UnicodeString bla2 = ures_getUnicodeStringByIndex(rowbundle, col, &err);
                                    if(!U_FAILURE(err)){
                                        expected+=itoa(row, buf);
                                        expected+=itoa(col, buf);
                                        col++;
                                        CONFIRM_EQ(got, expected);
                                        CONFIRM_EQ(bla, expected);
                                        CONFIRM_EQ(bla2, expected);
                                    }
                                }
                                CONFIRM_EQ(col, ures_getSize(rowbundle));
                            }
                        }
                        else{
                            CONFIRM_EQ(ures_getType(bundle), (int32_t)URES_STRING);
                        }
                    }
                    CONFIRM_EQ(got, expected);
                    count++;
                }
            }
        }

        // Check that ures_getUnicodeString() & variants return a bogus string if failure.
        // Same relevant code path whether the failure code is passed in
        // or comes from a lookup error.
        UErrorCode failure = U_INTERNAL_PROGRAM_ERROR;
        assertTrue("ures_getUnicodeString(failure).isBogus()",
                   ures_getUnicodeString(testCAPI, &failure).isBogus());
        assertTrue("ures_getNextUnicodeString(failure).isBogus()",
                   ures_getNextUnicodeString(testCAPI, nullptr, &failure).isBogus());
        assertTrue("ures_getUnicodeStringByIndex(failure).isBogus()",
                   ures_getUnicodeStringByIndex(testCAPI, 999, &failure).isBogus());
        assertTrue("ures_getUnicodeStringByKey(failure).isBogus()",
                   ures_getUnicodeStringByKey(testCAPI, "bogus key", &failure).isBogus());

        ures_close(temp);
        ures_close(rowbundle);
        ures_close(bundle);
        ures_close(testCAPI);
    } else {
        errln("failed to open a resource bundle\n");
    }

    /* Restore the default locale for the other tests. */
    Locale::setDefault(originalDefault, err);
}




//***************************************************************************************

UBool
NewResourceBundleTest::testTag(const char* frag,
                            UBool in_Root,
                            UBool in_te,
                            UBool in_te_IN)
{
    int32_t failOrig = fail;

    // Make array from input params

    UBool is_in[] = { in_Root, in_te, in_te_IN };

    const char* NAME[] = { "ROOT", "TE", "TE_IN" };

    // Now try to load the desired items

    char tag[100];
    UnicodeString action;

    int32_t i,j,row,col, actual_bundle;
    int32_t index;
    const char* testdatapath;

    UErrorCode status = U_ZERO_ERROR;
    testdatapath=loadTestData(status);
    if(U_FAILURE(status))
    {
        dataerrln("Could not load testdata.dat %s " + UnicodeString(u_errorName(status)));
        return false;
    }

    for (i=0; i<bundles_count; ++i)
    {
        action = "Constructor for ";
        action += param[i].name;

        status = U_ZERO_ERROR;
        ResourceBundle theBundle( testdatapath, *param[i].locale, status);
        //ResourceBundle theBundle( "c:\\icu\\icu\\source\\test\\testdata\\testdata", *param[i].locale, status);
        CONFIRM_UErrorCode(status,param[i].expected_constructor_status);

        if(i == 5)
          actual_bundle = 0; /* ne -> default */
        else if(i == 3)
          actual_bundle = 1; /* te_NE -> te */
        else if(i == 4)
          actual_bundle = 2; /* te_IN_NE -> te_IN */
        else
          actual_bundle = i;


        UErrorCode expected_resource_status = U_MISSING_RESOURCE_ERROR;
        for (j=e_te_IN; j>=e_Root; --j)
        {
            if (is_in[j] && param[i].inherits[j])
              {
                if(j == actual_bundle) /* it's in the same bundle OR it's a nonexistent=default bundle (5) */
                  expected_resource_status = U_ZERO_ERROR;
                else if(j == 0)
                  expected_resource_status = U_USING_DEFAULT_WARNING;
                else
                  expected_resource_status = U_USING_FALLBACK_WARNING;
                
                break;
            }
        }

        UErrorCode expected_status;

        UnicodeString base;
        for (j=param[i].where; j>=0; --j)
        {
            if (is_in[j])
            {
                base = NAME[j];
                break;
            }
        }

        //--------------------------------------------------------------------------
        // string

        uprv_strcpy(tag, "string_");
        uprv_strcat(tag, frag);

        action = param[i].name;
        action += ".getStringEx(";
        action += tag;
        action += ")";


        status = U_ZERO_ERROR;
        UnicodeString string = theBundle.getStringEx(tag, status);
        if(U_FAILURE(status)) {
            string.setTo(true, kErrorUChars, kErrorLength);
        }

        CONFIRM_UErrorCode(status, expected_resource_status);

        UnicodeString expected_string(kErrorUChars);
        if (U_SUCCESS(status)) {
            expected_string = base;
        }

        CONFIRM_EQ(string, expected_string);

        //--------------------------------------------------------------------------
        // array   ResourceBundle using the key

        uprv_strcpy(tag, "array_");
        uprv_strcat(tag, frag);

        action = param[i].name;
        action += ".get(";
        action += tag;
        action += ")";

        int32_t count = kERROR_COUNT;
        status = U_ZERO_ERROR;
        ResourceBundle array = theBundle.get(tag, status);
        CONFIRM_UErrorCode(status,expected_resource_status);


        if (U_SUCCESS(status))
        {
            //confirm the resource type is an array
            UResType bundleType=array.getType();
            CONFIRM_EQ(bundleType, URES_ARRAY);

            count=array.getSize();
            CONFIRM_GE(count,1);
           
            for (j=0; j<count; ++j)
            {
                char buf[32];
                expected_string = base;
                expected_string += itoa(j,buf);
                CONFIRM_EQ(array.getNextString(status),expected_string);
            }

        }
        else
        {
            CONFIRM_EQ(count,kERROR_COUNT);
            //       CONFIRM_EQ((int32_t)(unsigned long)array,(int32_t)0);
            count = 0;
        }

        //--------------------------------------------------------------------------
        // arrayItem ResourceBundle using the index


        for (j=0; j<100; ++j)
        {
            index = count ? (randi(count * 3) - count) : (randi(200) - 100);
            status = U_ZERO_ERROR;
            string = kErrorUChars;
            ResourceBundle array = theBundle.get(tag, status);
            if(!U_FAILURE(status)){
                UnicodeString t = array.getStringEx(index, status);
                if(!U_FAILURE(status)) {
                   string=t;
                }
            }

            expected_status = (index >= 0 && index < count) ? expected_resource_status : U_MISSING_RESOURCE_ERROR;
            CONFIRM_UErrorCode(status,expected_status);

            if (U_SUCCESS(status)){
                char buf[32];
                expected_string = base;
                expected_string += itoa(index,buf);
            } else {
                expected_string = kErrorUChars;
            }
               CONFIRM_EQ(string,expected_string);

        }

        //--------------------------------------------------------------------------
        // 2dArray

        uprv_strcpy(tag, "array_2d_");
        uprv_strcat(tag, frag);

        action = param[i].name;
        action += ".get(";
        action += tag;
        action += ")";


        int32_t row_count = kERROR_COUNT, column_count = kERROR_COUNT;
        status = U_ZERO_ERROR;
        ResourceBundle array2d=theBundle.get(tag, status);

        //const UnicodeString** array2d = theBundle.get2dArray(tag, row_count, column_count, status);
        CONFIRM_UErrorCode(status,expected_resource_status);

        if (U_SUCCESS(status))
        {
            //confirm the resource type is an 2darray
            UResType bundleType=array2d.getType();
            CONFIRM_EQ(bundleType, URES_ARRAY);

            row_count=array2d.getSize();
            CONFIRM_GE(row_count,1);

            for(row=0; row<row_count; ++row){
                ResourceBundle tablerow=array2d.get(row, status);
                CONFIRM_UErrorCode(status, expected_resource_status);
                if(U_SUCCESS(status)){
                    //confirm the resourcetype of each table row is an array
                    UResType rowType=tablerow.getType();
                    CONFIRM_EQ(rowType, URES_ARRAY);

                    column_count=tablerow.getSize();
                    CONFIRM_GE(column_count,1);

                    for (col=0; j<column_count; ++j) {
                           char buf[32];
                           expected_string = base;
                           expected_string += itoa(row,buf);
                           expected_string += itoa(col,buf);
                           CONFIRM_EQ(tablerow.getNextString(status),expected_string);
                    }
                }
            }
        }else{
            CONFIRM_EQ(row_count,kERROR_COUNT);
            CONFIRM_EQ(column_count,kERROR_COUNT);
             row_count=column_count=0;
        }




       //--------------------------------------------------------------------------
       // 2dArrayItem
       for (j=0; j<200; ++j)
       {
           row = row_count ? (randi(row_count * 3) - row_count) : (randi(200) - 100);
           col = column_count ? (randi(column_count * 3) - column_count) : (randi(200) - 100);
           status = U_ZERO_ERROR;
           string = kErrorUChars;
           ResourceBundle array2d=theBundle.get(tag, status);
           if(U_SUCCESS(status)){
                ResourceBundle tablerow=array2d.get(row, status);
                if(U_SUCCESS(status)) {
                    UnicodeString t=tablerow.getStringEx(col, status);
                    if(U_SUCCESS(status)){
                       string=t;
                    }
                }
           }
           expected_status = (row >= 0 && row < row_count && col >= 0 && col < column_count) ?
                                  expected_resource_status: U_MISSING_RESOURCE_ERROR;
           CONFIRM_UErrorCode(status,expected_status);

           if (U_SUCCESS(status)){
               char buf[32];
               expected_string = base;
               expected_string += itoa(row,buf);
               expected_string += itoa(col,buf);
           } else {
               expected_string = kErrorUChars;
           }
               CONFIRM_EQ(string,expected_string);

        }

        //--------------------------------------------------------------------------
        // taggedArray

        uprv_strcpy(tag, "tagged_array_");
        uprv_strcat(tag, frag);

        action = param[i].name;
        action += ".get(";
        action += tag;
        action += ")";

        int32_t         tag_count;
        status = U_ZERO_ERROR;

        ResourceBundle tags=theBundle.get(tag, status);
        CONFIRM_UErrorCode(status, expected_resource_status);

        if (U_SUCCESS(status)) {
            UResType bundleType=tags.getType();
            CONFIRM_EQ(bundleType, URES_TABLE);

            tag_count=tags.getSize();
            CONFIRM_GE((int32_t)tag_count, (int32_t)0); 

            for(index=0; index <tag_count; index++){
                ResourceBundle tagelement=tags.get(index, status);
                UnicodeString key=tagelement.getKey();
                UnicodeString value=tagelement.getNextString(status);
                logln("tag = " + key + ", value = " + value );
                if(key.startsWith("tag") && value.startsWith(base)){
                    record_pass();
                }else{
                    record_fail();
                }

            }

            for(index=0; index <tag_count; index++){
                ResourceBundle tagelement=tags.get(index, status);
                const char *tkey=nullptr;
                UnicodeString value=tagelement.getNextString(&tkey, status);
                UnicodeString key(tkey);
                logln("tag = " + key + ", value = " + value );
                if(value.startsWith(base)){
                    record_pass();
                }else{
                    record_fail();
                }
            }

        }else{
            tag_count=0;
        }




        //--------------------------------------------------------------------------
        // taggedArrayItem

        action = param[i].name;
        action += ".get(";
        action += tag;
        action += ")";

        count = 0;
        for (index=-20; index<20; ++index)
        {
            char buf[32];
            status = U_ZERO_ERROR;
            string = kErrorUChars;
            char item_tag[8];
            uprv_strcpy(item_tag, "tag");
            uprv_strcat(item_tag, itoa(index,buf));
            ResourceBundle tags=theBundle.get(tag, status);
            if(U_SUCCESS(status)){
                ResourceBundle tagelement=tags.get(item_tag, status);
                if(!U_FAILURE(status)){
                    UResType elementType=tagelement.getType();
                    CONFIRM_EQ(elementType, (int32_t)URES_STRING);
                    const char* key=tagelement.getKey();
                    CONFIRM_EQ((UnicodeString)key, (UnicodeString)item_tag);
                    UnicodeString t=tagelement.getString(status);
                    if(!U_FAILURE(status)){
                        string=t;
                    }
                }
                if (index < 0) {
                    CONFIRM_UErrorCode(status,U_MISSING_RESOURCE_ERROR);
                }
                else{
                   if (status != U_MISSING_RESOURCE_ERROR) {
                       count++;
                       expected_string = base;
                       expected_string += buf;
                       CONFIRM_EQ(string,expected_string);
                   }
                }
            }

        }
        CONFIRM_EQ(count, tag_count);

    }
    return (UBool)(failOrig == fail);
}

void
NewResourceBundleTest::record_pass()
{
  ++pass;
}
void
NewResourceBundleTest::record_fail()
{
  err();
  ++fail;
}


void
NewResourceBundleTest::TestNewTypes() {
    char action[256];
    const char* testdatapath;
    UErrorCode status = U_ZERO_ERROR;
    uint8_t *binResult = nullptr;
    int32_t len = 0;
    int32_t i = 0;
    int32_t intResult = 0;
    uint32_t uintResult = 0;
    char16_t expected[] = { 'a','b','c','\0','d','e','f' };
    const char* expect ="tab:\t cr:\r ff:\f newline:\n backslash:\\\\ quote=\\\' doubleQuote=\\\" singlequoutes=''";
    char16_t uExpect[200];

    testdatapath=loadTestData(status);

    if(U_FAILURE(status))
    {
        dataerrln("Could not load testdata.dat %s \n",u_errorName(status));
        return;
    }

    ResourceBundle theBundle(testdatapath, "testtypes", status);
    ResourceBundle bundle(testdatapath, Locale("te_IN"),status);

    UnicodeString emptyStr = theBundle.getStringEx("emptystring", status);
    if(emptyStr.length() != 0) {
      logln("Empty string returned invalid value\n");
    }

    CONFIRM_UErrorCode(status, U_ZERO_ERROR);

    /* This test reads the string "abc\u0000def" from the bundle   */
    /* if everything is working correctly, the size of this string */
    /* should be 7. Everything else is a wrong answer, esp. 3 and 6*/

    strcpy(action, "getting and testing of string with embedded zero");
    ResourceBundle res = theBundle.get("zerotest", status);
    CONFIRM_UErrorCode(status, U_ZERO_ERROR);
    CONFIRM_EQ(res.getType(), URES_STRING);
    UnicodeString zeroString=res.getString(status);
    len = zeroString.length();
    if(U_SUCCESS(status)){
        CONFIRM_UErrorCode(status, U_ZERO_ERROR);
        CONFIRM_EQ(len, 7);
        CONFIRM_NE(len, 3);
    }
    for(i=0;i<len;i++){
        if(zeroString[i]!= expected[i]){
            logln("Output didnot match Expected: \\u%4X Got: \\u%4X", expected[i], zeroString[i]);
        }
    }

    strcpy(action, "getting and testing of binary type");
    res = theBundle.get("binarytest", status);
    CONFIRM_UErrorCode(status, U_ZERO_ERROR);
    CONFIRM_EQ(res.getType(), URES_BINARY);
    binResult=(uint8_t*)res.getBinary(len, status);
    if(U_SUCCESS(status)){
        CONFIRM_UErrorCode(status, U_ZERO_ERROR);
        CONFIRM_EQ(len, 15);
        for(i = 0; i<15; i++) {
            CONFIRM_EQ(binResult[i], i);
        }
    }

    strcpy(action, "getting and testing of imported binary type");
    res = theBundle.get("importtest",status);
    CONFIRM_UErrorCode(status, U_ZERO_ERROR);
    CONFIRM_EQ(res.getType(), URES_BINARY);
    binResult=(uint8_t*)res.getBinary(len, status);
    if(U_SUCCESS(status)){
        CONFIRM_UErrorCode(status, U_ZERO_ERROR);
        CONFIRM_EQ(len, 15);
        for(i = 0; i<15; i++) {
            CONFIRM_EQ(binResult[i], i);
        }
    }

    strcpy(action, "getting and testing of integer types");
    res = theBundle.get("one",  status);
    CONFIRM_UErrorCode(status, U_ZERO_ERROR);
    CONFIRM_EQ(res.getType(), URES_INT);
    intResult=res.getInt(status);
    uintResult = res.getUInt(status);
    if(U_SUCCESS(status)){
        CONFIRM_UErrorCode(status, U_ZERO_ERROR);
        CONFIRM_EQ(uintResult, (uint32_t)intResult);
        CONFIRM_EQ(intResult, 1);
    }

    strcpy(action, "getting minusone");
    res = theBundle.get((const char*)"minusone", status);
    CONFIRM_UErrorCode(status, U_ZERO_ERROR);
    CONFIRM_EQ(res.getType(), URES_INT);
    intResult=res.getInt(status);
    uintResult = res.getUInt(status);
    if(U_SUCCESS(status)){
        CONFIRM_UErrorCode(status, U_ZERO_ERROR);
        CONFIRM_EQ(uintResult, 0x0FFFFFFF); /* a 28 bit integer */
        CONFIRM_EQ(intResult, -1);
        CONFIRM_NE(uintResult, (uint32_t)intResult);
    }

    strcpy(action, "getting plusone");
    res = theBundle.get("plusone",status);
    CONFIRM_UErrorCode(status, U_ZERO_ERROR);
    CONFIRM_EQ(res.getType(), URES_INT);
    intResult=res.getInt(status);
    uintResult = res.getUInt(status);
    if(U_SUCCESS(status)){
        CONFIRM_UErrorCode(status, U_ZERO_ERROR);
        CONFIRM_EQ(uintResult, (uint32_t)intResult);
        CONFIRM_EQ(intResult, 1);
    }

    res = theBundle.get("onehundredtwentythree",status);
    CONFIRM_UErrorCode(status, U_ZERO_ERROR);
    CONFIRM_EQ(res.getType(), URES_INT);
    intResult=res.getInt(status);
    if(U_SUCCESS(status)){
        CONFIRM_UErrorCode(status, U_ZERO_ERROR);
        CONFIRM_EQ(intResult, 123);
    }

    /* this tests if escapes are preserved or not */
    {
        UnicodeString str = theBundle.getStringEx("testescape",status);
        CONFIRM_UErrorCode(status, U_ZERO_ERROR);
        if(U_SUCCESS(status)){
            u_charsToUChars(expect,uExpect,(int32_t)uprv_strlen(expect)+1);
            if(str.compare(uExpect)!=0){
                errln("Did not get the expected string for testescape expected. Expected : " 
                    +UnicodeString(uExpect )+ " Got: " + str);
            }
        }
    }
    /* test for jitterbug#1435 */
    {
        UnicodeString str = theBundle.getStringEx("test_underscores",status);
        expect ="test message ....";
        CONFIRM_UErrorCode(status, U_ZERO_ERROR);
        u_charsToUChars(expect,uExpect,(int32_t)uprv_strlen(expect)+1);
        if(str.compare(uExpect)!=0){
            errln("Did not get the expected string for test_underscores.\n");
        }
    }


}

void
NewResourceBundleTest::TestGetByFallback() {
    UErrorCode status = U_ZERO_ERROR;

    ResourceBundle heRes(nullptr, "he", status);

    heRes.getWithFallback("calendar", status).getWithFallback("islamic-civil", status).getWithFallback("DateTime", status);
    if(U_SUCCESS(status)) {
        errln("he locale's Islamic-civil DateTime resource exists. How did it get here?\n");
    }
    status = U_ZERO_ERROR;

    heRes.getWithFallback("calendar", status).getWithFallback("islamic-civil", status).getWithFallback("eras", status);
    if(U_FAILURE(status)) {
        dataerrln("Didn't get Islamic Eras. I know they are there! - %s", u_errorName(status));
    }
    status = U_ZERO_ERROR;

    ResourceBundle rootRes(nullptr, "root", status);
    rootRes.getWithFallback("calendar", status).getWithFallback("islamic-civil", status).getWithFallback("DateTime", status);
    if(U_SUCCESS(status)) {
        errln("Root's Islamic-civil's DateTime resource exists. How did it get here?\n");
    }
    status = U_ZERO_ERROR;

}


#define REQUIRE_SUCCESS(status) UPRV_BLOCK_MACRO_BEGIN { \
    if (status.errIfFailureAndReset("line %d", __LINE__)) { \
        return; \
    } \
} UPRV_BLOCK_MACRO_END

#define REQUIRE_ERROR(expected, status) UPRV_BLOCK_MACRO_BEGIN { \
    if (!status.expectErrorAndReset(expected, "line %d", __LINE__)) { \
        return; \
    } \
} UPRV_BLOCK_MACRO_END

/**
 * Tests the --filterDir option in genrb.
 *
 * Input resource text file: test/testdata/filtertest.txt
 * Input filter rule file: test/testdata/filters/filtertest.txt
 *
 * The resource bundle should contain no keys matched by the filter
 * and should contain all other keys.
 */
void NewResourceBundleTest::TestFilter() {
    IcuTestErrorCode status(*this, "TestFilter");

    ResourceBundle rb(loadTestData(status), "filtertest", status);
    REQUIRE_SUCCESS(status);
    assertEquals("rb", rb.getType(), URES_TABLE);

    ResourceBundle alabama = rb.get("alabama", status);
    REQUIRE_SUCCESS(status);
    assertEquals("alabama", alabama.getType(), URES_TABLE);

    {
        ResourceBundle alaska = alabama.get("alaska", status);
        REQUIRE_SUCCESS(status);
        assertEquals("alaska", alaska.getType(), URES_TABLE);

        {
            ResourceBundle arizona = alaska.get("arizona", status);
            REQUIRE_SUCCESS(status);
            assertEquals("arizona", arizona.getType(), URES_STRING);
            assertEquals("arizona", u"arkansas", arizona.getString(status));
            REQUIRE_SUCCESS(status);

            // Filter: california should not be included
            ResourceBundle california = alaska.get("california", status);
            REQUIRE_ERROR(U_MISSING_RESOURCE_ERROR, status);
        }

        // Filter: connecticut should not be included
        ResourceBundle connecticut = alabama.get("connecticut", status);
        REQUIRE_ERROR(U_MISSING_RESOURCE_ERROR, status);
    }

    ResourceBundle fornia = rb.get("fornia", status);
    REQUIRE_SUCCESS(status);
    assertEquals("fornia", fornia.getType(), URES_TABLE);

    {
        // Filter: hawaii should not be included based on parent inheritance
        ResourceBundle hawaii = fornia.get("hawaii", status);
        REQUIRE_ERROR(U_MISSING_RESOURCE_ERROR, status);

        // Filter: illinois should not be included based on direct rule
        ResourceBundle illinois = fornia.get("illinois", status);
        REQUIRE_ERROR(U_MISSING_RESOURCE_ERROR, status);
    }

    ResourceBundle mississippi = rb.get("mississippi", status);
    REQUIRE_SUCCESS(status);
    assertEquals("mississippi", mississippi.getType(), URES_TABLE);

    {
        ResourceBundle louisiana = mississippi.get("louisiana", status);
        REQUIRE_SUCCESS(status);
        assertEquals("louisiana", louisiana.getType(), URES_TABLE);

        {
            ResourceBundle maine = louisiana.get("maine", status);
            REQUIRE_SUCCESS(status);
            assertEquals("maine", maine.getType(), URES_STRING);
            assertEquals("maine", u"maryland", maine.getString(status));
            REQUIRE_SUCCESS(status);

            ResourceBundle iowa = louisiana.get("iowa", status);
            REQUIRE_SUCCESS(status);
            assertEquals("iowa", iowa.getType(), URES_STRING);
            assertEquals("iowa", u"kansas", iowa.getString(status));
            REQUIRE_SUCCESS(status);

            // Filter: missouri should not be included
            ResourceBundle missouri = louisiana.get("missouri", status);
            REQUIRE_ERROR(U_MISSING_RESOURCE_ERROR, status);
        }

        ResourceBundle michigan = mississippi.get("michigan", status);
        REQUIRE_SUCCESS(status);
        assertEquals("michigan", michigan.getType(), URES_TABLE);

        {
            ResourceBundle maine = michigan.get("maine", status);
            REQUIRE_SUCCESS(status);
            assertEquals("maine", maine.getType(), URES_STRING);
            assertEquals("maine", u"minnesota", maine.getString(status));
            REQUIRE_SUCCESS(status);

            // Filter: iowa should not be included
            ResourceBundle iowa = michigan.get("iowa", status);
            REQUIRE_ERROR(U_MISSING_RESOURCE_ERROR, status);

            ResourceBundle missouri = michigan.get("missouri", status);
            REQUIRE_SUCCESS(status);
            assertEquals("missouri", missouri.getType(), URES_STRING);
            assertEquals("missouri", u"nebraska", missouri.getString(status));
            REQUIRE_SUCCESS(status);
        }

        ResourceBundle nevada = mississippi.get("nevada", status);
        REQUIRE_SUCCESS(status);
        assertEquals("nevada", nevada.getType(), URES_TABLE);

        {
            ResourceBundle maine = nevada.get("maine", status);
            REQUIRE_SUCCESS(status);
            assertEquals("maine", maine.getType(), URES_STRING);
            assertEquals("maine", u"new-hampshire", maine.getString(status));
            REQUIRE_SUCCESS(status);

            // Filter: iowa should not be included
            ResourceBundle iowa = nevada.get("iowa", status);
            REQUIRE_ERROR(U_MISSING_RESOURCE_ERROR, status);

            // Filter: missouri should not be included
            ResourceBundle missouri = nevada.get("missouri", status);
            REQUIRE_ERROR(U_MISSING_RESOURCE_ERROR, status);
        }
    }

    // Filter: northCarolina should be included based on direct rule,
    // and so should its child, northDakota
    ResourceBundle northCarolina = rb.get("northCarolina", status);
    REQUIRE_SUCCESS(status);
    assertEquals("northCarolina", northCarolina.getType(), URES_TABLE);

    {
        ResourceBundle northDakota = northCarolina.get("northDakota", status);
        REQUIRE_SUCCESS(status);
        assertEquals("northDakota", northDakota.getType(), URES_STRING);
        assertEquals("northDakota", u"west-virginia", northDakota.getString(status));
        REQUIRE_SUCCESS(status);
    }
}

void NewResourceBundleTest::TestIntervalAliasFallbacks() {
    const char* locales[] = {
        "en",
        "ja",
        "fr_CA",
        "en_150",
        "es_419",
        "id",
        "in",
        "pl",
        "pt_PT",
        "sr_ME",
        "zh_Hant",
        "zh_Hant_TW",
        "zh_TW",
    };
    const char* calendars[] = {
        "gregorian",
        "chinese",
        "islamic",
        "islamic-civil",
        "islamic-tbla",
        "islamic-umalqura",
        "ethiopic-amete-alem",
        "islamic-rgsa",
        "japanese",
        "roc",
    };

    for (int lidx = 0; lidx < UPRV_LENGTHOF(locales); lidx++) {
        UErrorCode status = U_ZERO_ERROR;
        UResourceBundle *rb = ures_open(nullptr, locales[lidx], &status);
        if (U_FAILURE(status)) {
            errln("Cannot open bundle for locale %s", locales[lidx]);
            break;
        }
        for (int cidx = 0; cidx < UPRV_LENGTHOF(calendars); cidx++) {
            CharString key;
            key.append("calendar/", status);
            key.append(calendars[cidx], status);
            key.append("/intervalFormats/fallback", status);
            int32_t resStrLen = 0;
            ures_getStringByKeyWithFallback(rb, key.data(), &resStrLen, &status);
            if (U_FAILURE(status)) {
                errln("Cannot ures_getStringByKeyWithFallback('%s') on locale %s",
                      key.data(), locales[lidx]);
                break;
            }
        }
        ures_close(rb);
    }
}

#if U_ENABLE_TRACING

static std::vector<std::string> gResourcePathsTraced;
static std::vector<std::string> gDataFilesTraced;
static std::vector<std::string> gBundlesTraced;

static void U_CALLCONV traceData(
        const void*,
        int32_t fnNumber,
        int32_t,
        const char *,
        va_list args) {

    // NOTE: Whether this test is run in isolation affects whether or not
    // *.res files are opened. For stability, ignore *.res file opens.

    if (fnNumber == UTRACE_UDATA_RESOURCE) {
        va_arg(args, const char*); // type
        va_arg(args, const char*); // file
        const char* resourcePath = va_arg(args, const char*);
        gResourcePathsTraced.push_back(resourcePath);
    } else if (fnNumber == UTRACE_UDATA_BUNDLE) {
        const char* filePath = va_arg(args, const char*);
        gBundlesTraced.push_back(filePath);
    } else if (fnNumber == UTRACE_UDATA_DATA_FILE) {
        const char* filePath = va_arg(args, const char*);
        gDataFilesTraced.push_back(filePath);
    } else if (fnNumber == UTRACE_UDATA_RES_FILE) {
        // ignore
    }
}

void NewResourceBundleTest::TestTrace() {
    IcuTestErrorCode status(*this, "TestTrace");

    assertEquals("Start position stability coverage", 0x3000, UTRACE_UDATA_START);

    const void* context;
    utrace_setFunctions(context, nullptr, nullptr, traceData);
    utrace_setLevel(UTRACE_VERBOSE);

    {
        LocalPointer<BreakIterator> brkitr(BreakIterator::createWordInstance("zh-CN", status));

        assertEquals("Should touch expected resource paths",
            { "/boundaries", "/boundaries/word", "/boundaries/word" },
            gResourcePathsTraced);
        assertEquals("Should touch expected resource bundles",
            { U_ICUDATA_NAME "-brkitr/zh.res" },
            gBundlesTraced);
        assertEquals("Should touch expected data files",
            { U_ICUDATA_NAME "-brkitr/word.brk" },
            gDataFilesTraced);
        gResourcePathsTraced.clear();
        gDataFilesTraced.clear();
        gBundlesTraced.clear();
    }

    {
        ucurr_getDefaultFractionDigits(u"USD", status);

        assertEquals("Should touch expected resource paths",
            { "/CurrencyMeta", "/CurrencyMeta/DEFAULT", "/CurrencyMeta/DEFAULT" },
            gResourcePathsTraced);
        assertEquals("Should touch expected resource bundles",
            { U_ICUDATA_NAME "-curr/supplementalData.res" },
            gBundlesTraced);
        assertEquals("Should touch expected data files",
            { },
            gDataFilesTraced);
        gResourcePathsTraced.clear();
        gDataFilesTraced.clear();
        gBundlesTraced.clear();
    }

    utrace_setFunctions(context, nullptr, nullptr, nullptr);
}

#endif

void NewResourceBundleTest::TestOpenDirectFillIn() {
    // Test that ures_openDirectFillIn() opens a stack allocated resource bundle, similar to ures_open().
    // Since ures_openDirectFillIn is just a wrapper function, this is just a very basic test copied from
    // the crestst.c/TestOpenDirect test.
    // ICU-20769: This test was moved to C++ intltest while
    // turning UResourceBundle from a C struct into a C++ class.
    IcuTestErrorCode errorCode(*this, "TestOpenDirectFillIn");
    UResourceBundle *item;
    UResourceBundle idna_rules;
    ures_initStackObject(&idna_rules);

    ures_openDirectFillIn(&idna_rules, loadTestData(errorCode), "idna_rules", errorCode);
    if(errorCode.errDataIfFailureAndReset("ures_openDirectFillIn(\"idna_rules\") failed\n")) {
        return;
    }

    if(0!=uprv_strcmp("idna_rules", ures_getLocale(&idna_rules, errorCode))) {
        errln("ures_openDirectFillIn(\"idna_rules\").getLocale()!=idna_rules");
    }
    errorCode.reset();

    /* try an item in idna_rules, must work */
    item=ures_getByKey(&idna_rules, "UnassignedSet", nullptr, errorCode);
    if(errorCode.errDataIfFailureAndReset("translit_index.getByKey(local key) failed\n")) {
        // pass
    } else {
        ures_close(item);
    }

    /* try an item in root, must fail */
    item=ures_getByKey(&idna_rules, "ShortLanguage", nullptr, errorCode);
    if(errorCode.isFailure()) {
        errorCode.reset();
    } else {
        errln("idna_rules.getByKey(root key) succeeded but should have failed!");
        ures_close(item);
    }
    ures_close(&idna_rules);
}

void NewResourceBundleTest::TestStackReuse() {
    // This test will crash if this doesn't work. Results don't need testing.
    // ICU-20769: This test was moved to C++ intltest while
    // turning UResourceBundle from a C struct into a C++ class.
    IcuTestErrorCode errorCode(*this, "TestStackReuse");
    UResourceBundle table;
    UResourceBundle *rb = ures_open(nullptr, "en_US", errorCode);

    if(errorCode.errDataIfFailureAndReset("Could not load en_US locale.\n")) {
        return;
    }
    ures_initStackObject(&table);
    ures_getByKeyWithFallback(rb, "Types", &table, errorCode);
    ures_getByKeyWithFallback(&table, "collation", &table, errorCode);
    ures_close(rb);
    ures_close(&table);
    errorCode.reset();  // ignore U_MISSING_RESOURCE_ERROR etc.
}
