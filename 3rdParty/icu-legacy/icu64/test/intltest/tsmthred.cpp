// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1999-2015, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include "simplethread.h"

#include "unicode/utypes.h"
#include "unicode/ustring.h"
#include "umutex.h"
#include "cmemory.h"
#include "cstring.h"
#include "indiancal.h"
#include "uparse.h"
#include "unicode/localpointer.h"
#include "unicode/resbund.h"
#include "unicode/udata.h"
#include "unicode/uloc.h"
#include "unicode/locid.h"
#include "putilimp.h"
#include "intltest.h"
#include "tsmthred.h"
#include "unicode/ushape.h"
#include "unicode/translit.h"
#include "sharedobject.h"
#include "unifiedcache.h"
#include "uassert.h"


MultithreadTest::MultithreadTest()
{
}

MultithreadTest::~MultithreadTest()
{
}

#include <stdio.h>
#include <string.h>
#include <ctype.h>    // tolower, toupper
#include <memory>

#include "unicode/putil.h"

// for mthreadtest
#include "unicode/numfmt.h"
#include "unicode/choicfmt.h"
#include "unicode/msgfmt.h"
#include "unicode/locid.h"
#include "unicode/coll.h"
#include "unicode/calendar.h"
#include "ucaconf.h"


void MultithreadTest::runIndexedTest( int32_t index, UBool exec,
                const char* &name, char* /*par*/ ) {
    if (exec)
        logln("TestSuite MultithreadTest: ");

    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestThreads);
    TESTCASE_AUTO(TestMutex);
#if !UCONFIG_NO_FORMATTING
    TESTCASE_AUTO(TestThreadedIntl);
#endif
#if !UCONFIG_NO_COLLATION
    TESTCASE_AUTO(TestCollators);
#endif /* #if !UCONFIG_NO_COLLATION */
    TESTCASE_AUTO(TestString);
    TESTCASE_AUTO(TestArabicShapingThreads);
    TESTCASE_AUTO(TestAnyTranslit);
    TESTCASE_AUTO(TestConditionVariables);
    TESTCASE_AUTO(TestUnifiedCache);
#if !UCONFIG_NO_TRANSLITERATION
    TESTCASE_AUTO(TestBreakTranslit);
    TESTCASE_AUTO(TestIncDec);
#if !UCONFIG_NO_FORMATTING
    TESTCASE_AUTO(Test20104);
#endif /* #if !UCONFIG_NO_FORMATTING */
#endif /* #if !UCONFIG_NO_TRANSLITERATION */
    TESTCASE_AUTO_END
}


//-----------------------------------------------------------------------------------
//
//   TestThreads -- see if threads really work at all.
//
//   Set up N threads pointing at N chars. When they are started, they will
//   set their chars. At the end we make sure they are all set.
//
//-----------------------------------------------------------------------------------

class TestThreadsThread : public SimpleThread
{
public:
    TestThreadsThread(char* whatToChange) { fWhatToChange = whatToChange; }
    virtual void run() { Mutex m;
                         *fWhatToChange = '*';
    }
private:
    char *fWhatToChange;
};


void MultithreadTest::TestThreads()
{
    static const int32_t THREADTEST_NRTHREADS = 8;
    char threadTestChars[THREADTEST_NRTHREADS + 1];
    SimpleThread *threads[THREADTEST_NRTHREADS];
    int32_t numThreadsStarted = 0;

    int32_t i;
    for(i=0;i<THREADTEST_NRTHREADS;i++)
    {
        threadTestChars[i] = ' ';
        threads[i] = new TestThreadsThread(&threadTestChars[i]);
    }
    threadTestChars[THREADTEST_NRTHREADS] = '\0';

    logln("->" + UnicodeString(threadTestChars) + "<- Firing off threads.. ");
    for(i=0;i<THREADTEST_NRTHREADS;i++)
    {
        if (threads[i]->start() != 0) {
            errln("Error starting thread %d", i);
        }
        else {
            numThreadsStarted++;
        }
        logln(" Subthread started.");
    }

    assertTrue(WHERE, THREADTEST_NRTHREADS == numThreadsStarted);

    logln("Waiting for threads to be set..");
    for(i=0; i<THREADTEST_NRTHREADS; i++) {
        threads[i]->join();
        if (threadTestChars[i] != '*') {
            errln("%s:%d Thread %d failed.", __FILE__, __LINE__, i);
        }
        delete threads[i];
    }
}


//-----------------------------------------------------------------------------------
//
//   TestArabicShapeThreads -- see if calls to u_shapeArabic in many threads works successfully
//
//   Set up N threads pointing at N chars. When they are started, they will make calls to doTailTest which tests
//   u_shapeArabic, if the calls are successful it will the set * chars.
//   At the end we make sure all threads managed to run u_shapeArabic successfully.
//   This is a unit test for ticket 9473
//
//-----------------------------------------------------------------------------------

class TestArabicShapeThreads : public SimpleThread
{
public:
    TestArabicShapeThreads() {};
    virtual void run() { doTailTest(); };
private:
	void doTailTest();
};


void TestArabicShapeThreads::doTailTest(void) {
    static const UChar src[] = { 0x0020, 0x0633, 0 };
    static const UChar dst_old[] = { 0xFEB1, 0x200B,0 };
    static const UChar dst_new[] = { 0xFEB1, 0xFE73,0 };
    UChar dst[3] = { 0x0000, 0x0000,0 };
    int32_t length;
    UErrorCode status;

    for (int32_t loopCount = 0; loopCount < 100; loopCount++) {
        status = U_ZERO_ERROR;
        length = u_shapeArabic(src, -1, dst, UPRV_LENGTHOF(dst),
                U_SHAPE_LETTERS_SHAPE|U_SHAPE_SEEN_TWOCELL_NEAR, &status);
        if(U_FAILURE(status)) {
            IntlTest::gTest->errln("Fail: status %s\n", u_errorName(status));
            return;
        } else if(length!=2) {
            IntlTest::gTest->errln("Fail: len %d expected 3\n", length);
            return;
        } else if(u_strncmp(dst,dst_old,UPRV_LENGTHOF(dst))) {
            IntlTest::gTest->errln("Fail: got U+%04X U+%04X expected U+%04X U+%04X\n",
                    dst[0],dst[1],dst_old[0],dst_old[1]);
            return;
        }


        // Trying new tail
        status = U_ZERO_ERROR;
        length = u_shapeArabic(src, -1, dst, UPRV_LENGTHOF(dst),
                U_SHAPE_LETTERS_SHAPE|U_SHAPE_SEEN_TWOCELL_NEAR|U_SHAPE_TAIL_NEW_UNICODE, &status);
        if(U_FAILURE(status)) {
            IntlTest::gTest->errln("Fail: status %s\n", u_errorName(status));
            return;
        } else if(length!=2) {
            IntlTest::gTest->errln("Fail: len %d expected 3\n", length);
            return;
        } else if(u_strncmp(dst,dst_new,UPRV_LENGTHOF(dst))) {
            IntlTest::gTest->errln("Fail: got U+%04X U+%04X expected U+%04X U+%04X\n",
                    dst[0],dst[1],dst_new[0],dst_new[1]);
            return;
        }
    }
    return;
}
	

void MultithreadTest::TestArabicShapingThreads()
{
    TestArabicShapeThreads threads[30];

    int32_t i;

    logln("-> do TestArabicShapingThreads <- Firing off threads.. ");
    for(i=0; i < UPRV_LENGTHOF(threads); i++) {
        if (threads[i].start() != 0) {
            errln("Error starting thread %d", i);
        }
    }

    for(i=0; i < UPRV_LENGTHOF(threads); i++) {
        threads[i].join();
    }
    logln("->TestArabicShapingThreads <- Got all threads! cya");
}


//-----------------------------------------------------------------------
//
//  TestMutex  - a simple (non-stress) test to verify that ICU mutexes
//               and condition variables are functioning.  Does not test the use of
//               mutexes within ICU services, but rather that the
//               platform's mutex support is at least superficially there.
//
//----------------------------------------------------------------------
static UMutex *gTestMutexA() {
    static UMutex m = U_MUTEX_INITIALIZER;
    return &m;
}
static UConditionVar *gThreadsCountChanged() {
    static UConditionVar cv = U_CONDITION_INITIALIZER;
    return &cv;
}

static int     gThreadsStarted = 0;
static int     gThreadsInMiddle = 0;
static int     gThreadsDone = 0;

static const int TESTMUTEX_THREAD_COUNT = 40;

class TestMutexThread : public SimpleThread
{
public:
    virtual void run() {
        // This is the code that each of the spawned threads runs.
        // All threads move together throught the started - middle - done sequence together,
        // waiting for all other threads to reach each point before advancing.
        umtx_lock(gTestMutexA());
        gThreadsStarted += 1;
        umtx_condBroadcast(gThreadsCountChanged());
        while (gThreadsStarted < TESTMUTEX_THREAD_COUNT) {
            if (gThreadsInMiddle != 0) {
                IntlTest::gTest->errln(
                    "%s:%d gThreadsInMiddle = %d. Expected 0.", __FILE__, __LINE__, gThreadsInMiddle);
                return;
            }
            umtx_condWait(gThreadsCountChanged(), gTestMutexA());
        }

        gThreadsInMiddle += 1;
        umtx_condBroadcast(gThreadsCountChanged());
        while (gThreadsInMiddle < TESTMUTEX_THREAD_COUNT) {
            if (gThreadsDone != 0) {
                IntlTest::gTest->errln(
                    "%s:%d gThreadsDone = %d. Expected 0.", __FILE__, __LINE__, gThreadsDone);
                return;
            }
            umtx_condWait(gThreadsCountChanged(), gTestMutexA());
        }

        gThreadsDone += 1;
        umtx_condBroadcast(gThreadsCountChanged());
        while (gThreadsDone < TESTMUTEX_THREAD_COUNT) {
            umtx_condWait(gThreadsCountChanged(), gTestMutexA());
        }
        umtx_unlock(gTestMutexA());
    }
};

void MultithreadTest::TestMutex()
{
    gThreadsStarted = 0;
    gThreadsInMiddle = 0;
    gThreadsDone = 0;
    int32_t i = 0;
    TestMutexThread  threads[TESTMUTEX_THREAD_COUNT];
    umtx_lock(gTestMutexA());
    for (i=0; i<TESTMUTEX_THREAD_COUNT; i++) {
        if (threads[i].start() != 0) {
            errln("%s:%d Error starting thread %d", __FILE__, __LINE__, i);
            return;
        }
    }

    // Because we are holding gTestMutexA, all of the threads should be blocked
    // at the start of their run() function.
    if (gThreadsStarted != 0) {
        errln("%s:%d gThreadsStarted=%d. Expected 0.", __FILE__, __LINE__, gThreadsStarted);
        return;
    }

    while (gThreadsInMiddle < TESTMUTEX_THREAD_COUNT) {
        if (gThreadsDone != 0) {
            errln("%s:%d gThreadsDone=%d. Expected 0.", __FILE__, __LINE__, gThreadsStarted);
            return;
        }
        umtx_condWait(gThreadsCountChanged(), gTestMutexA());
    }

    while (gThreadsDone < TESTMUTEX_THREAD_COUNT) {
        umtx_condWait(gThreadsCountChanged(), gTestMutexA());
    }
    umtx_unlock(gTestMutexA());

    for (i=0; i<TESTMUTEX_THREAD_COUNT; i++) {
        threads[i].join();
    }
}


//-------------------------------------------------------------------------------------------
//
//   TestMultithreadedIntl.  Test ICU Formatting in a multi-threaded environment
//
//-------------------------------------------------------------------------------------------


// * Show exactly where the string's differences lie.
UnicodeString showDifference(const UnicodeString& expected, const UnicodeString& result)
{
    UnicodeString res;
    res = expected + u"<Expected\n";
    if(expected.length() != result.length())
        res += u" [ Different lengths ] \n";
    else
    {
        for(int32_t i=0;i<expected.length();i++)
        {
            if(expected[i] == result[i])
            {
                res += u" ";
            }
            else
            {
                res += u"|";
            }
        }
        res += u"<Differences";
        res += u"\n";
    }
    res += result + u"<Result\n";

    return res;
}


//-------------------------------------------------------------------------------------------
//
//   FormatThreadTest - a thread that tests performing a number of numberformats.
//
//-------------------------------------------------------------------------------------------

const int kFormatThreadIterations = 100;  // # of iterations per thread
const int kFormatThreadThreads    = 10;  // # of threads to spawn

#if !UCONFIG_NO_FORMATTING



struct FormatThreadTestData
{
    double number;
    UnicodeString string;
    FormatThreadTestData(double a, const UnicodeString& b) : number(a),string(b) {}
} ;


// "Someone from {2} is receiving a #{0} error - {1}. Their telephone call is costing {3 number,currency}."

static void formatErrorMessage(UErrorCode &realStatus, const UnicodeString& pattern, const Locale& theLocale,
                     UErrorCode inStatus0,                       // statusString 1
                     const Locale &inCountry2, double currency3, // these numbers are the message arguments.
                     UnicodeString &result)
{
    if(U_FAILURE(realStatus))
        return; // you messed up

    UnicodeString errString1(u_errorName(inStatus0));

    UnicodeString countryName2;
    inCountry2.getDisplayCountry(theLocale,countryName2);

    Formattable myArgs[] = {
        Formattable((int32_t)inStatus0),   // inStatus0      {0}
        Formattable(errString1), // statusString1 {1}
        Formattable(countryName2),  // inCountry2 {2}
        Formattable(currency3)// currency3  {3,number,currency}
    };

    LocalPointer<MessageFormat> fmt(new MessageFormat(u"MessageFormat's API is broken!!!!!!!!!!!",realStatus), realStatus);
    if (U_FAILURE(realStatus)) {
        return;
    }
    fmt->setLocale(theLocale);
    fmt->applyPattern(pattern, realStatus);

    FieldPosition ignore = 0;
    fmt->format(myArgs,4,result,ignore,realStatus);
}

/**
  * Shared formatters &  data used by instances of ThreadSafeFormat.
  * Exactly one instance of this class is created, and it is then shared concurrently
  * by the multiple instances of ThreadSafeFormat.
  */
class ThreadSafeFormatSharedData {
  public:
    ThreadSafeFormatSharedData(UErrorCode &status);
    ~ThreadSafeFormatSharedData();
    LocalPointer<NumberFormat>  fFormat;
    Formattable    fYDDThing;
    Formattable    fBBDThing;
    UnicodeString  fYDDStr;
    UnicodeString  fBBDStr;
};

const ThreadSafeFormatSharedData *gSharedData = NULL;

ThreadSafeFormatSharedData::ThreadSafeFormatSharedData(UErrorCode &status) {
    fFormat.adoptInstead(NumberFormat::createCurrencyInstance(Locale::getUS(), status));
    static const UChar *kYDD = u"YDD";
    static const UChar *kBBD = u"BBD";
    fYDDThing.adoptObject(new CurrencyAmount(123.456, kYDD, status));
    fBBDThing.adoptObject(new CurrencyAmount(987.654, kBBD, status));
    if (U_FAILURE(status)) {
        return;
    }
    fFormat->format(fYDDThing, fYDDStr, NULL, status);
    fFormat->format(fBBDThing, fBBDStr, NULL, status);
    gSharedData = this;
}

ThreadSafeFormatSharedData::~ThreadSafeFormatSharedData() {
    gSharedData = NULL;
}

/**
 * Class for thread-safe testing of format.
 *   Instances of this class appear as members of class FormatThreadTest.
 *   Multiple instances of FormatThreadTest coexist.
 *   ThreadSafeFormat::doStuff() is called concurrently to test the thread safety of
 *   various shared format operations.
 */
class ThreadSafeFormat {
public:
  /* give a unique offset to each thread */
  ThreadSafeFormat(UErrorCode &status);
  UBool doStuff(int32_t offset, UnicodeString &appendErr, UErrorCode &status) const;
private:
  LocalPointer<NumberFormat> fFormat; // formatter - en_US constructed currency
};


ThreadSafeFormat::ThreadSafeFormat(UErrorCode &status) {
  fFormat.adoptInstead(NumberFormat::createCurrencyInstance(Locale::getUS(), status));
}

static const UChar *kUSD = u"USD";

UBool ThreadSafeFormat::doStuff(int32_t offset, UnicodeString &appendErr, UErrorCode &status) const {
  UBool okay = TRUE;

  if(u_strcmp(fFormat->getCurrency(), kUSD)) {
    appendErr.append(u"fFormat currency != ")
      .append(kUSD)
      .append(u", =")
      .append(fFormat->getCurrency())
      .append(u"! ");
    okay = FALSE;
  }

  if(u_strcmp(gSharedData->fFormat->getCurrency(), kUSD)) {
    appendErr.append(u"gFormat currency != ")
      .append(kUSD)
      .append(u", =")
      .append(gSharedData->fFormat->getCurrency())
      .append(u"! ");
    okay = FALSE;
  }
  UnicodeString str;
  const UnicodeString *o=NULL;
  Formattable f;
  const NumberFormat *nf = NULL; // only operate on it as const.
  switch(offset%4) {
  case 0:  f = gSharedData->fYDDThing;  o = &gSharedData->fYDDStr;  nf = gSharedData->fFormat.getAlias();  break;
  case 1:  f = gSharedData->fBBDThing;  o = &gSharedData->fBBDStr;  nf = gSharedData->fFormat.getAlias();  break;
  case 2:  f = gSharedData->fYDDThing;  o = &gSharedData->fYDDStr;  nf = fFormat.getAlias();  break;
  case 3:  f = gSharedData->fBBDThing;  o = &gSharedData->fBBDStr;  nf = fFormat.getAlias();  break;
  }
  nf->format(f, str, NULL, status);

  if(*o != str) {
    appendErr.append(showDifference(*o, str));
    okay = FALSE;
  }
  return okay;
}

UBool U_CALLCONV isAcceptable(void *, const char *, const char *, const UDataInfo *) {
    return TRUE;
}

//static UMTX debugMutex = NULL;
//static UMTX gDebugMutex;


class FormatThreadTest : public SimpleThread
{
public:
    int     fNum;
    int     fTraceInfo;

    LocalPointer<ThreadSafeFormat> fTSF;

    FormatThreadTest() // constructor is NOT multithread safe.
        : SimpleThread(),
        fNum(0),
        fTraceInfo(0),
        fTSF(NULL),
        fOffset(0)
        // the locale to use
    {
        UErrorCode status = U_ZERO_ERROR;      // TODO: rearrange code to allow checking of status.
        fTSF.adoptInstead(new ThreadSafeFormat(status));
        static int32_t fgOffset = 0;
        fgOffset += 3;
        fOffset = fgOffset;
    }


    virtual void run()
    {
        fTraceInfo                     = 1;
        LocalPointer<NumberFormat> percentFormatter;
        UErrorCode status = U_ZERO_ERROR;

#if 0
        // debugging code,
        for (int i=0; i<4000; i++) {
            status = U_ZERO_ERROR;
            UDataMemory *data1 = udata_openChoice(0, "res", "en_US", isAcceptable, 0, &status);
            UDataMemory *data2 = udata_openChoice(0, "res", "fr", isAcceptable, 0, &status);
            udata_close(data1);
            udata_close(data2);
            if (U_FAILURE(status)) {
                error("udata_openChoice failed.\n");
                break;
            }
        }
        return;
#endif

#if 0
        // debugging code,
        int m;
        for (m=0; m<4000; m++) {
            status         = U_ZERO_ERROR;
            UResourceBundle *res   = NULL;
            const char *localeName = NULL;

            Locale  loc = Locale::getEnglish();

            localeName = loc.getName();
            // localeName = "en";

            // ResourceBundle bund = ResourceBundle(0, loc, status);
            //umtx_lock(&gDebugMutex);
            res = ures_open(NULL, localeName, &status);
            //umtx_unlock(&gDebugMutex);

            //umtx_lock(&gDebugMutex);
            ures_close(res);
            //umtx_unlock(&gDebugMutex);

            if (U_FAILURE(status)) {
                error("Resource bundle construction failed.\n");
                break;
            }
        }
        return;
#endif

        // Keep this data here to avoid static initialization.
        FormatThreadTestData kNumberFormatTestData[] =
        {
            FormatThreadTestData((double)5.0, UnicodeString(u"5")),
                FormatThreadTestData( 6.0, UnicodeString(u"6")),
                FormatThreadTestData( 20.0, UnicodeString(u"20")),
                FormatThreadTestData( 8.0, UnicodeString(u"8")),
                FormatThreadTestData( 8.3, UnicodeString(u"8.3")),
                FormatThreadTestData( 12345, UnicodeString(u"12,345")),
                FormatThreadTestData( 81890.23, UnicodeString(u"81,890.23")),
        };
        int32_t kNumberFormatTestDataLength = UPRV_LENGTHOF(kNumberFormatTestData);

        // Keep this data here to avoid static initialization.
        FormatThreadTestData kPercentFormatTestData[] =
        {
            FormatThreadTestData((double)5.0, CharsToUnicodeString("500\\u00a0%")),
                FormatThreadTestData( 1.0, CharsToUnicodeString("100\\u00a0%")),
                FormatThreadTestData( 0.26, CharsToUnicodeString("26\\u00a0%")),
                FormatThreadTestData(
                   16384.99, CharsToUnicodeString("1\\u202F638\\u202F499\\u00a0%")), // U+202F = NNBSP
                FormatThreadTestData(
                    81890.23, CharsToUnicodeString("8\\u202F189\\u202F023\\u00a0%")),
        };
        int32_t kPercentFormatTestDataLength = UPRV_LENGTHOF(kPercentFormatTestData);
        int32_t iteration;

        status = U_ZERO_ERROR;
        LocalPointer<NumberFormat> formatter(NumberFormat::createInstance(Locale::getEnglish(),status));
        if(U_FAILURE(status)) {
            IntlTest::gTest->dataerrln("%s:%d Error %s on NumberFormat::createInstance().",
                    __FILE__, __LINE__, u_errorName(status));
            goto cleanupAndReturn;
        }

        percentFormatter.adoptInstead(NumberFormat::createPercentInstance(Locale::getFrench(),status));
        if(U_FAILURE(status))             {
            IntlTest::gTest->errln("%s:%d Error %s on NumberFormat::createPercentInstance().",
                    __FILE__, __LINE__, u_errorName(status));
            goto cleanupAndReturn;
        }

        for(iteration = 0;!IntlTest::gTest->getErrors() && iteration<kFormatThreadIterations;iteration++)
        {

            int32_t whichLine = (iteration + fOffset)%kNumberFormatTestDataLength;

            UnicodeString  output;

            formatter->format(kNumberFormatTestData[whichLine].number, output);

            if(0 != output.compare(kNumberFormatTestData[whichLine].string)) {
                IntlTest::gTest->errln("format().. expected " + kNumberFormatTestData[whichLine].string
                        + " got " + output);
                goto cleanupAndReturn;
            }

            // Now check percent.
            output.remove();
            whichLine = (iteration + fOffset)%kPercentFormatTestDataLength;

            percentFormatter->format(kPercentFormatTestData[whichLine].number, output);
            if(0 != output.compare(kPercentFormatTestData[whichLine].string))
            {
                IntlTest::gTest->errln("percent format().. \n" +
                        showDifference(kPercentFormatTestData[whichLine].string,output));
                goto cleanupAndReturn;
            }

            // Test message error
            const int       kNumberOfMessageTests = 3;
            UErrorCode      statusToCheck;
            UnicodeString   patternToCheck;
            Locale          messageLocale;
            Locale          countryToCheck;
            double          currencyToCheck;

            UnicodeString   expected;

            // load the cases.
            switch((iteration+fOffset) % kNumberOfMessageTests)
            {
            default:
            case 0:
                statusToCheck=                      U_FILE_ACCESS_ERROR;
                patternToCheck=        u"0:Someone from {2} is receiving a #{0}"
                                        " error - {1}. Their telephone call is costing "
                                        "{3,number,currency}."; // number,currency
                messageLocale=                      Locale("en","US");
                countryToCheck=                     Locale("","HR");
                currencyToCheck=                    8192.77;
                expected=  u"0:Someone from Croatia is receiving a #4 error - "
                            "U_FILE_ACCESS_ERROR. Their telephone call is costing $8,192.77.";
                break;
            case 1:
                statusToCheck=                      U_INDEX_OUTOFBOUNDS_ERROR;
                patternToCheck=                     u"1:A customer in {2} is receiving a #{0} error - {1}. "
                                                     "Their telephone call is costing {3,number,currency}."; // number,currency
                messageLocale=                      Locale("de","DE@currency=DEM");
                countryToCheck=                     Locale("","BF");
                currencyToCheck=                    2.32;
                expected=                           u"1:A customer in Burkina Faso is receiving a #8 error - U_INDEX_OUTOFBOUNDS_ERROR. "
                                                    u"Their telephone call is costing 2,32\u00A0DM.";
                break;
            case 2:
                statusToCheck=                      U_MEMORY_ALLOCATION_ERROR;
                patternToCheck=   u"2:user in {2} is receiving a #{0} error - {1}. "
                                  "They insist they just spent {3,number,currency} "
                                  "on memory."; // number,currency
                messageLocale=                      Locale("de","AT@currency=ATS"); // Austrian German
                countryToCheck=                     Locale("","US"); // hmm
                currencyToCheck=                    40193.12;
                expected=       u"2:user in Vereinigte Staaten is receiving a #7 error"
                                u" - U_MEMORY_ALLOCATION_ERROR. They insist they just spent"
                                u" \u00f6S\u00A040.193,12 on memory.";
                break;
            }

            UnicodeString result;
            UErrorCode status = U_ZERO_ERROR;
            formatErrorMessage(status,patternToCheck,messageLocale,statusToCheck,
                                countryToCheck,currencyToCheck,result);
            if(U_FAILURE(status))
            {
                UnicodeString tmp(u_errorName(status));
                IntlTest::gTest->errln(u"Failure on message format, pattern=" + patternToCheck +
                        ", error = " + tmp);
                goto cleanupAndReturn;
            }

            if(result != expected)
            {
                IntlTest::gTest->errln(u"PatternFormat: \n" + showDifference(expected,result));
                goto cleanupAndReturn;
            }
            // test the Thread Safe Format
            UnicodeString appendErr;
            if(!fTSF->doStuff(fNum, appendErr, status)) {
              IntlTest::gTest->errln(appendErr);
              goto cleanupAndReturn;
            }
        }   /*  end of for loop */



cleanupAndReturn:
        fTraceInfo = 2;
    }

private:
    int32_t fOffset; // where we are testing from.
};

// ** The actual test function.

void MultithreadTest::TestThreadedIntl()
{
    UnicodeString theErr;

    UErrorCode threadSafeErr = U_ZERO_ERROR;

    ThreadSafeFormatSharedData sharedData(threadSafeErr);
    assertSuccess(WHERE, threadSafeErr, TRUE);

    //
    //  Create and start the test threads
    //
    logln("Spawning: %d threads * %d iterations each.",
                kFormatThreadThreads, kFormatThreadIterations);
    FormatThreadTest tests[kFormatThreadThreads];
    int32_t j;
    for(j = 0; j < UPRV_LENGTHOF(tests); j++) {
        tests[j].fNum = j;
        int32_t threadStatus = tests[j].start();
        if (threadStatus != 0) {
            errln("%s:%d System Error %d starting thread number %d.",
                    __FILE__, __LINE__, threadStatus, j);
            return;
        }
    }


    for (j=0; j<UPRV_LENGTHOF(tests); j++) {
        tests[j].join();
        logln("Thread # %d is complete..", j);
    }
}
#endif /* #if !UCONFIG_NO_FORMATTING */





//-------------------------------------------------------------------------------------------
//
// Collation threading test
//
//-------------------------------------------------------------------------------------------
#if !UCONFIG_NO_COLLATION

#define kCollatorThreadThreads   10  // # of threads to spawn
#define kCollatorThreadPatience kCollatorThreadThreads*30

struct Line {
    UChar buff[25];
    int32_t buflen;
} ;


static UCollationResult
normalizeResult(int32_t result) {
    return result<0 ? UCOL_LESS : result==0 ? UCOL_EQUAL : UCOL_GREATER;
}

class CollatorThreadTest : public SimpleThread
{
private:
    const Collator *coll;
    const Line *lines;
    int32_t noLines;
    UBool isAtLeastUCA62;
public:
    CollatorThreadTest()  : SimpleThread(),
        coll(NULL),
        lines(NULL),
        noLines(0),
        isAtLeastUCA62(TRUE)
    {
    };
    void setCollator(Collator *c, Line *l, int32_t nl, UBool atLeastUCA62)
    {
        coll = c;
        lines = l;
        noLines = nl;
        isAtLeastUCA62 = atLeastUCA62;
    }
    virtual void run() {
        uint8_t sk1[1024], sk2[1024];
        uint8_t *oldSk = NULL, *newSk = sk1;
        int32_t oldLen = 0;
        int32_t prev = 0;
        int32_t i = 0;

        for(i = 0; i < noLines; i++) {
            if(lines[i].buflen == 0) { continue; }

            int32_t resLen = coll->getSortKey(lines[i].buff, lines[i].buflen, newSk, 1024);

            if(oldSk != NULL) {
                int32_t skres = strcmp((char *)oldSk, (char *)newSk);
                int32_t cmpres = coll->compare(lines[prev].buff, lines[prev].buflen, lines[i].buff, lines[i].buflen);
                int32_t cmpres2 = coll->compare(lines[i].buff, lines[i].buflen, lines[prev].buff, lines[prev].buflen);

                if(cmpres != -cmpres2) {
                    IntlTest::gTest->errln(UnicodeString(u"Compare result not symmetrical on line ") + (i + 1));
                    break;
                }

                if(cmpres != normalizeResult(skres)) {
                    IntlTest::gTest->errln(UnicodeString(u"Difference between coll->compare and sortkey compare on line ") + (i + 1));
                    break;
                }

                int32_t res = cmpres;
                if(res == 0 && !isAtLeastUCA62) {
                    // Up to UCA 6.1, the collation test files use a custom tie-breaker,
                    // comparing the raw input strings.
                    res = u_strcmpCodePointOrder(lines[prev].buff, lines[i].buff);
                    // Starting with UCA 6.2, the collation test files use the standard UCA tie-breaker,
                    // comparing the NFD versions of the input strings,
                    // which we do via setting strength=identical.
                }
                if(res > 0) {
                    IntlTest::gTest->errln(UnicodeString(u"Line is not greater or equal than previous line, for line ") + (i + 1));
                    break;
                }
            }

            oldSk = newSk;
            oldLen = resLen;
            (void)oldLen;   // Suppress set but not used warning.
            prev = i;

            newSk = (newSk == sk1)?sk2:sk1;
        }
    }
};

void MultithreadTest::TestCollators()
{

    UErrorCode status = U_ZERO_ERROR;
    FILE *testFile = NULL;
    char testDataPath[1024];
    strcpy(testDataPath, IntlTest::getSourceTestData(status));
    if (U_FAILURE(status)) {
        errln("ERROR: could not open test data %s", u_errorName(status));
        return;
    }
    strcat(testDataPath, "CollationTest_");

    const char* type = "NON_IGNORABLE";

    const char *ext = ".txt";
    if(testFile) {
        fclose(testFile);
    }
    char buffer[1024];
    strcpy(buffer, testDataPath);
    strcat(buffer, type);
    size_t bufLen = strlen(buffer);

    // we try to open 3 files:
    // path/CollationTest_type.txt
    // path/CollationTest_type_SHORT.txt
    // path/CollationTest_type_STUB.txt
    // we are going to test with the first one that we manage to open.

    strcpy(buffer+bufLen, ext);

    testFile = fopen(buffer, "rb");

    if(testFile == 0) {
        strcpy(buffer+bufLen, "_SHORT");
        strcat(buffer, ext);
        testFile = fopen(buffer, "rb");

        if(testFile == 0) {
            strcpy(buffer+bufLen, "_STUB");
            strcat(buffer, ext);
            testFile = fopen(buffer, "rb");

            if (testFile == 0) {
                *(buffer+bufLen) = 0;
                dataerrln("could not open any of the conformance test files, tried opening base %s", buffer);
                return;
            } else {
                infoln(
                    "INFO: Working with the stub file.\n"
                    "If you need the full conformance test, please\n"
                    "download the appropriate data files from:\n"
                    "http://source.icu-project.org/repos/icu/tools/trunk/unicodetools/com/ibm/text/data/");
            }
        }
    }

    LocalArray<Line> lines(new Line[200000]);
    memset(lines.getAlias(), 0, sizeof(Line)*200000);
    int32_t lineNum = 0;

    UChar bufferU[1024];
    uint32_t first = 0;

    while (fgets(buffer, 1024, testFile) != NULL) {
        if(*buffer == 0 || buffer[0] == '#') {
            // Store empty and comment lines so that errors are reported
            // for the real test file lines.
            lines[lineNum].buflen = 0;
            lines[lineNum].buff[0] = 0;
        } else {
            int32_t buflen = u_parseString(buffer, bufferU, 1024, &first, &status);
            lines[lineNum].buflen = buflen;
            u_memcpy(lines[lineNum].buff, bufferU, buflen);
            lines[lineNum].buff[buflen] = 0;
        }
        lineNum++;
    }
    fclose(testFile);
    if(U_FAILURE(status)) {
      dataerrln("Couldn't read the test file!");
      return;
    }

    UVersionInfo uniVersion;
    static const UVersionInfo v62 = { 6, 2, 0, 0 };
    u_getUnicodeVersion(uniVersion);
    UBool isAtLeastUCA62 = uprv_memcmp(uniVersion, v62, 4) >= 0;

    LocalPointer<Collator> coll(Collator::createInstance(Locale::getRoot(), status));
    if(U_FAILURE(status)) {
        errcheckln(status, "Couldn't open UCA collator");
        return;
    }
    coll->setAttribute(UCOL_NORMALIZATION_MODE, UCOL_ON, status);
    coll->setAttribute(UCOL_CASE_FIRST, UCOL_OFF, status);
    coll->setAttribute(UCOL_CASE_LEVEL, UCOL_OFF, status);
    coll->setAttribute(UCOL_STRENGTH, isAtLeastUCA62 ? UCOL_IDENTICAL : UCOL_TERTIARY, status);
    coll->setAttribute(UCOL_ALTERNATE_HANDLING, UCOL_NON_IGNORABLE, status);

    int32_t spawnResult = 0;
    LocalArray<CollatorThreadTest> tests(new CollatorThreadTest[kCollatorThreadThreads]);

    logln(UnicodeString(u"Spawning: ") + kCollatorThreadThreads + u" threads * " + kFormatThreadIterations + u" iterations each.");
    int32_t j = 0;
    for(j = 0; j < kCollatorThreadThreads; j++) {
        //logln("Setting collator %i", j);
        tests[j].setCollator(coll.getAlias(), lines.getAlias(), lineNum, isAtLeastUCA62);
    }
    for(j = 0; j < kCollatorThreadThreads; j++) {
        log("%i ", j);
        spawnResult = tests[j].start();
        if(spawnResult != 0) {
            errln("%s:%d THREAD INFO: thread %d failed to start with status %d", __FILE__, __LINE__, j, spawnResult);
            return;
        }
    }
    logln("Spawned all");

    for(int32_t i=0;i<kCollatorThreadThreads;i++) {
        tests[i].join();
        //logln(UnicodeString("Test #") + i + " is complete.. ");
    }
}

#endif /* #if !UCONFIG_NO_COLLATION */




//-------------------------------------------------------------------------------------------
//
//   StringThreadTest2
//
//-------------------------------------------------------------------------------------------

const int kStringThreadIterations = 2500;// # of iterations per thread
const int kStringThreadThreads    = 10;  // # of threads to spawn


class StringThreadTest2 : public SimpleThread
{
public:
    int                 fNum;
    int                 fTraceInfo;
    static const UnicodeString *gSharedString;

    StringThreadTest2() // constructor is NOT multithread safe.
        : SimpleThread(),
        fTraceInfo(0)
    {
    };


    virtual void run()
    {
        fTraceInfo    = 1;
        int loopCount = 0;

        for (loopCount = 0; loopCount < kStringThreadIterations; loopCount++) {
            if (*gSharedString != u"This is the original test string.") {
                IntlTest::gTest->errln("%s:%d Original string is corrupt.", __FILE__, __LINE__);
                break;
            }
            UnicodeString s1 = *gSharedString;
            s1 += u"cat this";
            UnicodeString s2(s1);
            UnicodeString s3 = *gSharedString;
            s2 = s3;
            s3.truncate(12);
            s2.truncate(0);
        }

        fTraceInfo = 2;
    }

};

const UnicodeString *StringThreadTest2::gSharedString = NULL;

// ** The actual test function.


void MultithreadTest::TestString()
{
    int     j;
    StringThreadTest2::gSharedString = new UnicodeString(u"This is the original test string.");
    StringThreadTest2  tests[kStringThreadThreads];

    logln(UnicodeString(u"Spawning: ") + kStringThreadThreads + u" threads * " + kStringThreadIterations + u" iterations each.");
    for(j = 0; j < kStringThreadThreads; j++) {
        int32_t threadStatus = tests[j].start();
        if (threadStatus != 0) {
            errln("%s:%d System Error %d starting thread number %d.", __FILE__, __LINE__, threadStatus, j);
        }
    }

    // Force a failure, to verify test is functioning and can report errors.
    // const_cast<UnicodeString *>(StringThreadTest2::gSharedString)->setCharAt(5, 'x');

    for(j=0; j<kStringThreadThreads; j++) {
        tests[j].join();
        logln(UnicodeString(u"Test #") + j + u" is complete.. ");
    }

    delete StringThreadTest2::gSharedString;
    StringThreadTest2::gSharedString = NULL;
}


//
// Test for ticket #10673, race in cache code in AnyTransliterator.
// It's difficult to make the original unsafe code actually fail, but
// this test will fairly reliably take the code path for races in
// populating the cache.
//

#if !UCONFIG_NO_TRANSLITERATION
Transliterator *gSharedTranslit = NULL;
class TxThread: public SimpleThread {
  public:
    TxThread() {};
    ~TxThread();
    void run();
};

TxThread::~TxThread() {}
void TxThread::run() {
    UnicodeString greekString(u"διαφορετικούς");
    gSharedTranslit->transliterate(greekString);
    IntlTest::gTest->assertEquals(WHERE, UnicodeString(u"diaphoretikoús"), greekString);
}
#endif


void MultithreadTest::TestAnyTranslit() {
#if !UCONFIG_NO_TRANSLITERATION
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Transliterator> tx(Transliterator::createInstance("Any-Latin", UTRANS_FORWARD, status));
    if (!assertSuccess(WHERE, status, true)) { return; }

    gSharedTranslit = tx.getAlias();
    TxThread  threads[4];
    int32_t i;
    for (i=0; i<UPRV_LENGTHOF(threads); i++) {
        threads[i].start();
    }

    for (i=0; i<UPRV_LENGTHOF(threads); i++) {
        threads[i].join();
    }
    gSharedTranslit = NULL;
#endif  // !UCONFIG_NO_TRANSLITERATION
}


//
// Condition Variables Test
//   Create a swarm of threads.
//   Using a mutex and a condition variables each thread
//     Increments a global count of started threads.
//     Broadcasts that it has started.
//     Waits on the condition that all threads have started.
//     Increments a global count of finished threads.
//     Waits on the condition that all threads have finished.
//     Exits.
//

class CondThread: public SimpleThread {
  public:
    CondThread() :fFinished(false)  {};
    ~CondThread() {};
    void run();
    bool  fFinished;
};

static UMutex *gCTMutex() {
    static UMutex m = U_MUTEX_INITIALIZER;
    return &m;
}
static UConditionVar *gCTConditionVar() {
    static UConditionVar cv = U_CONDITION_INITIALIZER;
    return &cv;
}
int gConditionTestOne = 1;   // Value one. Non-const, extern linkage to inhibit
                             //   compiler assuming a known value.
int gStartedThreads;
int gFinishedThreads;
static const int NUMTHREADS = 10;


// Worker thread function.
void CondThread::run() {
    umtx_lock(gCTMutex());
    gStartedThreads += gConditionTestOne;
    umtx_condBroadcast(gCTConditionVar());

    while (gStartedThreads < NUMTHREADS) {
        if (gFinishedThreads != 0) {
            IntlTest::gTest->errln("File %s, Line %d: Error, gStartedThreads = %d, gFinishedThreads = %d",
                             __FILE__, __LINE__, gStartedThreads, gFinishedThreads);
        }
        umtx_condWait(gCTConditionVar(), gCTMutex());
    }

    gFinishedThreads += gConditionTestOne;
    fFinished = true;
    umtx_condBroadcast(gCTConditionVar());

    while (gFinishedThreads < NUMTHREADS) {
        umtx_condWait(gCTConditionVar(), gCTMutex());
    }
    umtx_unlock(gCTMutex());
}

void MultithreadTest::TestConditionVariables() {
    gStartedThreads = 0;
    gFinishedThreads = 0;
    int i;

    umtx_lock(gCTMutex());
    CondThread *threads[NUMTHREADS];
    for (i=0; i<NUMTHREADS; ++i) {
        threads[i] = new CondThread;
        threads[i]->start();
    }

    while (gStartedThreads < NUMTHREADS) {
        umtx_condWait(gCTConditionVar(), gCTMutex());
    }

    while (gFinishedThreads < NUMTHREADS) {
        umtx_condWait(gCTConditionVar(), gCTMutex());
    }

    umtx_unlock(gCTMutex());

    for (i=0; i<NUMTHREADS; ++i) {
        assertTrue(WHERE, threads[i]->fFinished);
    }

    for (i=0; i<NUMTHREADS; ++i) {
        threads[i]->join();
        delete threads[i];
    }
}


//
// Unified Cache Test
//

// Each thread fetches a pair of objects. There are 8 distinct pairs:
// ("en_US", "bs"), ("en_GB", "ca"), ("fr_FR", "ca_AD") etc.
// These pairs represent 8 distinct languages

// Note that only one value per language gets created in the cache.
// In particular each cached value can have multiple keys.
static const char *gCacheLocales[] = {
    "en_US", "en_GB", "fr_FR", "fr",
    "de", "sr_ME", "sr_BA", "sr_CS"};
static const char *gCacheLocales2[] = {
    "bs", "ca", "ca_AD", "ca_ES",
    "en_US", "fi", "ff_CM", "ff_GN"};

static int32_t gObjectsCreated = 0;  // protected by gCTMutex
static const int32_t CACHE_LOAD = 3;

class UCTMultiThreadItem : public SharedObject {
  public:
    char *value;
    UCTMultiThreadItem(const char *x) : value(NULL) {
        value = uprv_strdup(x);
    }
    virtual ~UCTMultiThreadItem() {
        uprv_free(value);
    }
};

U_NAMESPACE_BEGIN

template<> U_EXPORT
const UCTMultiThreadItem *LocaleCacheKey<UCTMultiThreadItem>::createObject(
        const void *context, UErrorCode &status) const {
    const UnifiedCache *cacheContext = (const UnifiedCache *) context;

    if (uprv_strcmp(fLoc.getLanguage(), fLoc.getName()) != 0) {
        const UCTMultiThreadItem *result = NULL;
        if (cacheContext == NULL) {
            UnifiedCache::getByLocale(fLoc.getLanguage(), result, status);
            return result;
        }
        cacheContext->get(LocaleCacheKey<UCTMultiThreadItem>(fLoc.getLanguage()), result, status);
        return result;
    }

    umtx_lock(gCTMutex());
    bool firstObject = (gObjectsCreated == 0);
    if (firstObject) {
        // Force the first object creation that comes through to wait
        // until other have completed. Verifies that cache doesn't
        // deadlock when a creation is slow.

        // Note that gObjectsCreated needs to be incremeneted from 0 to 1
        // early, to keep subsequent threads from entering this path.
        gObjectsCreated = 1;
        while (gObjectsCreated < 3) {
            umtx_condWait(gCTConditionVar(), gCTMutex());
        }
    }
    umtx_unlock(gCTMutex());

    const UCTMultiThreadItem *result =
        new UCTMultiThreadItem(fLoc.getLanguage());
    if (result == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    } else {
        result->addRef();
    }
 
    // Log that we created an object. The first object was already counted,
    //    don't do it again.
    umtx_lock(gCTMutex());
    if (!firstObject) {
        gObjectsCreated += 1;
    }
    umtx_condBroadcast(gCTConditionVar());
    umtx_unlock(gCTMutex());

    return result;
}

U_NAMESPACE_END

class UnifiedCacheThread: public SimpleThread {
  public:
    UnifiedCacheThread(
            const UnifiedCache *cache,
            const char *loc,
            const char *loc2) : fCache(cache), fLoc(loc), fLoc2(loc2) {};
    ~UnifiedCacheThread() {};
    void run();
    void exerciseByLocale(const Locale &);
    const UnifiedCache *fCache;
    Locale fLoc;
    Locale fLoc2;
};

void UnifiedCacheThread::exerciseByLocale(const Locale &locale) {
    UErrorCode status = U_ZERO_ERROR;
    const UCTMultiThreadItem *origItem = NULL;
    fCache->get(
            LocaleCacheKey<UCTMultiThreadItem>(locale), fCache, origItem, status);
    U_ASSERT(U_SUCCESS(status));
    IntlTest::gTest->assertEquals(WHERE, locale.getLanguage(), origItem->value);

    // Fetch the same item again many times. We should always get the same
    // pointer since this client is already holding onto it
    for (int32_t i = 0; i < 1000; ++i) {
        const UCTMultiThreadItem *item = NULL;
        fCache->get(
                LocaleCacheKey<UCTMultiThreadItem>(locale), fCache, item, status);
        IntlTest::gTest->assertTrue(WHERE, item == origItem);
        if (item != NULL) {
            item->removeRef();
        }
    }
    origItem->removeRef();
}

void UnifiedCacheThread::run() {
    // Run the exercise with 2 different locales so that we can exercise
    // eviction more. If each thread exercises just one locale, then
    // eviction can't start until the threads end.
    exerciseByLocale(fLoc);
    exerciseByLocale(fLoc2);
}

void MultithreadTest::TestUnifiedCache() {

    // Start with our own local cache so that we have complete control
    // and set the eviction policy to evict starting with 2 unused
    // values
    UErrorCode status = U_ZERO_ERROR;
    UnifiedCache::getInstance(status);
    UnifiedCache cache(status);
    cache.setEvictionPolicy(2, 0, status);
    U_ASSERT(U_SUCCESS(status));

    gFinishedThreads = 0;
    gObjectsCreated = 0;

    UnifiedCacheThread *threads[CACHE_LOAD][UPRV_LENGTHOF(gCacheLocales)];
    for (int32_t i=0; i<CACHE_LOAD; ++i) {
        for (int32_t j=0; j<UPRV_LENGTHOF(gCacheLocales); ++j) {
            // Each thread works with a pair of locales.
            threads[i][j] = new UnifiedCacheThread(
                    &cache, gCacheLocales[j], gCacheLocales2[j]);
            threads[i][j]->start();
        }
    }

    for (int32_t i=0; i<CACHE_LOAD; ++i) {
        for (int32_t j=0; j<UPRV_LENGTHOF(gCacheLocales); ++j) {
            threads[i][j]->join();
        }
    }
    // Because of cache eviction, we can't assert exactly how many
    // distinct objects get created over the course of this run.
    // However we know that at least 8 objects get created because that
    // is how many distinct languages we have in our test.
    if (gObjectsCreated < 8) {
        errln("%s:%d Too few objects created.", __FILE__, __LINE__);
    }
    // We know that each thread cannot create more than 2 objects in
    // the cache, and there are UPRV_LENGTHOF(gCacheLocales) pairs of
    // objects fetched from the cache. If the threads run in series because
    // of eviction, at worst case each thread creates two objects.
    if (gObjectsCreated > 2 * CACHE_LOAD * UPRV_LENGTHOF(gCacheLocales)) {
        errln("%s:%d Too many objects created, got %d, expected %d", __FILE__, __LINE__, gObjectsCreated, 2 * CACHE_LOAD * UPRV_LENGTHOF(gCacheLocales));

    }

    assertEquals(WHERE, 2, cache.unusedCount());

    // clean up threads
    for (int32_t i=0; i<CACHE_LOAD; ++i) {
        for (int32_t j=0; j<UPRV_LENGTHOF(gCacheLocales); ++j) {
            delete threads[i][j];
        }
    }
}

#if !UCONFIG_NO_TRANSLITERATION
//
//  BreakTransliterator Threading Test
//     This is a test for bug #11603. Test verified to fail prior to fix.
//

static const Transliterator *gSharedTransliterator;
static const UnicodeString *gTranslitInput;
static const UnicodeString *gTranslitExpected;

class BreakTranslitThread: public SimpleThread {
  public:
    BreakTranslitThread() {};
    ~BreakTranslitThread() {};
    void run();
};

void BreakTranslitThread::run() {
    for (int i=0; i<10; i++) {
        icu::UnicodeString s(*gTranslitInput);
        gSharedTransliterator->transliterate(s);
        if (*gTranslitExpected != s) {
            IntlTest::gTest->errln("%s:%d Transliteration threading failure.", __FILE__, __LINE__);
            break;
        }
    }
}

void MultithreadTest::TestBreakTranslit() {
    UErrorCode status = U_ZERO_ERROR;
    UnicodeString input(
        u"\u0E42\u0E14\u0E22\u0E1E\u0E37\u0E49\u0E19\u0E10\u0E32\u0E19\u0E41\u0E25\u0E49\u0E27,");
        // Thai script, โดยพื้นฐานแล้ว
    gTranslitInput = &input;

    gSharedTransliterator = Transliterator::createInstance(
        UnicodeString(u"Any-Latin; Lower; NFD; [:Diacritic:]Remove; NFC; Latin-ASCII;"), UTRANS_FORWARD, status);
    assertSuccess(WHERE, status);
    if (!assertTrue(WHERE, gSharedTransliterator != nullptr)) {
        return;
    }

    UnicodeString expected(*gTranslitInput);
    gSharedTransliterator->transliterate(expected);
    gTranslitExpected = &expected;

    BreakTranslitThread threads[4];
    for (int i=0; i<UPRV_LENGTHOF(threads); ++i) {
        threads[i].start();
    }
    for (int i=0; i<UPRV_LENGTHOF(threads); ++i) {
        threads[i].join();
    }

    delete gSharedTransliterator;
    gTranslitInput = NULL;
    gTranslitExpected = NULL;
}


class TestIncDecThread : public SimpleThread {
public:
    TestIncDecThread() { };
    virtual void run();
};

static u_atomic_int32_t gIncDecCounter;

void TestIncDecThread::run() {
    umtx_atomic_inc(&gIncDecCounter);
    for (int32_t i=0; i<5000000; ++i) {
        umtx_atomic_inc(&gIncDecCounter);
        umtx_atomic_dec(&gIncDecCounter);
    }
}

void MultithreadTest::TestIncDec()
{
    static constexpr int NUM_THREADS = 4;
    gIncDecCounter = 0;
    TestIncDecThread threads[NUM_THREADS];
    for (auto &thread:threads) {
        thread.start();
    }
    for (auto &thread:threads) {
        thread.join();
    }
    assertEquals(WHERE, NUM_THREADS, gIncDecCounter);
}

#if !UCONFIG_NO_FORMATTING
static Calendar  *gSharedCalendar = {};

class Test20104Thread : public SimpleThread {
public:
    Test20104Thread() { };
    virtual void run();
};

void Test20104Thread::run() {
    gSharedCalendar->defaultCenturyStartYear();
}

void MultithreadTest::Test20104() {
    UErrorCode status = U_ZERO_ERROR;
    Locale loc("hi_IN");
    gSharedCalendar = new IndianCalendar(loc, status);
    assertSuccess(WHERE, status);

    static constexpr int NUM_THREADS = 4;
    Test20104Thread threads[NUM_THREADS];
    for (auto &thread:threads) {
        thread.start();
    }
    for (auto &thread:threads) {
        thread.join();
    }
    delete gSharedCalendar;
    // Note: failure is reported by Thread Sanitizer. Test itself succeeds.
}
#endif /* !UCONFIG_NO_FORMATTING */

#endif /* !UCONFIG_NO_TRANSLITERATION */
