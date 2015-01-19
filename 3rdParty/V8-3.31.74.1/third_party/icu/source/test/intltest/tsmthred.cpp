/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1999-2013, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#if defined(hpux)
# ifndef _INCLUDE_POSIX_SOURCE
#  define _INCLUDE_POSIX_SOURCE
# endif
#endif

#include "simplethread.h"

#include "unicode/utypes.h"
#include "unicode/ustring.h"
#include "umutex.h"
#include "cmemory.h"
#include "cstring.h"
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


#if U_PLATFORM_USES_ONLY_WIN32_API
    /* Prefer native Windows APIs even if POSIX is implemented (i.e., on Cygwin). */
#   undef POSIX
#elif U_PLATFORM_IMPLEMENTS_POSIX
#   define POSIX
#else
#   undef POSIX
#endif


#define LENGTHOF(array) (int32_t)(sizeof(array)/sizeof((array)[0]))
/* Needed by z/OS to get usleep */
#if U_PLATFORM == U_PF_OS390
#define __DOT1 1
#define __UU
#ifndef _XPG4_2
#define _XPG4_2
#endif
#include <unistd.h>
#endif
#if defined(POSIX)

#define HAVE_IMP

#if (ICU_USE_THREADS == 1)
#include <pthread.h>
#endif

#if defined(__hpux) && defined(HPUX_CMA)
# if defined(read)  // read being defined as cma_read causes trouble with iostream::read
#  undef read
# endif
#endif

/* Define __EXTENSIONS__ for Solaris and old friends in strict mode. */
#ifndef __EXTENSIONS__
#define __EXTENSIONS__
#endif

#if U_PLATFORM == U_PF_OS390
#include <sys/types.h>
#endif

#if U_PLATFORM != U_PF_OS390
#include <signal.h>
#endif

/* Define _XPG4_2 for Solaris and friends. */
#ifndef _XPG4_2
#define _XPG4_2
#endif

/* Define __USE_XOPEN_EXTENDED for Linux and glibc. */
#ifndef __USE_XOPEN_EXTENDED
#define __USE_XOPEN_EXTENDED 
#endif

/* Define _INCLUDE_XOPEN_SOURCE_EXTENDED for HP/UX (11?). */
#ifndef _INCLUDE_XOPEN_SOURCE_EXTENDED
#define _INCLUDE_XOPEN_SOURCE_EXTENDED
#endif

#include <unistd.h>

#endif
/* HPUX */
#ifdef sleep
#undef sleep
#endif

#define TSMTHREAD_FAIL(msg) errln("%s at file %s, line %d", msg, __FILE__, __LINE__)
#define TSMTHREAD_ASSERT(expr) {if (!(expr)) {TSMTHREAD_FAIL("Fail");}}

MultithreadTest::MultithreadTest()
{
}

MultithreadTest::~MultithreadTest()
{
}



#if (ICU_USE_THREADS==0)
void MultithreadTest::runIndexedTest( int32_t index, UBool exec, 
                const char* &name, char* /*par*/ ) {
  if (exec) logln("TestSuite MultithreadTest: ");

  if(index == 0)
      name = "NO_THREADED_TESTS";
  else
      name = "";

  if(exec) { logln("MultithreadTest - test DISABLED.  ICU_USE_THREADS set to 0, check your configuration if this is a problem..");
  }
}
#else

#include <stdio.h>
#include <string.h>
#include <ctype.h>    // tolower, toupper

#include "unicode/putil.h"

// for mthreadtest
#include "unicode/numfmt.h"
#include "unicode/choicfmt.h"
#include "unicode/msgfmt.h"
#include "unicode/locid.h"
#include "unicode/coll.h"
#include "unicode/calendar.h"
#include "ucaconf.h"

void SimpleThread::errorFunc() {
    // *(char *)0 = 3;            // Force entry into a debugger via a crash;
}

void MultithreadTest::runIndexedTest( int32_t index, UBool exec, 
                const char* &name, char* /*par*/ ) {
    if (exec)
        logln("TestSuite MultithreadTest: ");
    switch (index) {
    case 0:
        name = "TestThreads";
        if (exec)
            TestThreads();
        break;

    case 1:
        name = "TestMutex";
        if (exec)
            TestMutex();
        break;

    case 2:
        name = "TestThreadedIntl";
#if !UCONFIG_NO_FORMATTING
        if (exec) {
            TestThreadedIntl();
        }
#endif
        break;

    case 3:
      name = "TestCollators";
#if !UCONFIG_NO_COLLATION
      if (exec) {
            TestCollators();
      }
#endif /* #if !UCONFIG_NO_COLLATION */
      break;

    case 4:
        name = "TestString"; 
        if (exec) {
            TestString();
        }
        break;

	case 5:
        name = "TestArabicShapingThreads"; 
        if (exec) {
            TestArabicShapingThreads();
        }
        break;
		

    default:
        name = "";
        break; //needed to end loop
    }
}


//-----------------------------------------------------------------------------------
//
//   TestThreads -- see if threads really work at all.
//
//   Set up N threads pointing at N chars. When they are started, they will
//   each sleep 1 second and then set their chars. At the end we make sure they
//   are all set.
//
//-----------------------------------------------------------------------------------
#define THREADTEST_NRTHREADS 8
#define ARABICSHAPE_THREADTEST 30

class TestThreadsThread : public SimpleThread
{
public:
    TestThreadsThread(char* whatToChange) { fWhatToChange = whatToChange; }
    virtual void run() { SimpleThread::sleep(1000); 
                         Mutex m;
                         *fWhatToChange = '*'; 
    }
private:
    char *fWhatToChange;
};
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
    TestArabicShapeThreads(char* whatToChange) { fWhatToChange = whatToChange;}
    virtual void run() { 
	    if(doTailTest()==TRUE)
			*fWhatToChange = '*';             
    }
private:
    char *fWhatToChange;
	
	UBool doTailTest(void) {
  static const UChar src[] = { 0x0020, 0x0633, 0 };
  static const UChar dst_old[] = { 0xFEB1, 0x200B,0 };
  static const UChar dst_new[] = { 0xFEB1, 0xFE73,0 };
  UChar dst[3] = { 0x0000, 0x0000,0 };
  int32_t length;
  UErrorCode status;
  IntlTest inteltst =  IntlTest();
 
  status = U_ZERO_ERROR;
  length = u_shapeArabic(src, -1, dst, LENGTHOF(dst),
                         U_SHAPE_LETTERS_SHAPE|U_SHAPE_SEEN_TWOCELL_NEAR, &status);
  if(U_FAILURE(status)) {
	   inteltst.errln("Fail: status %s\n", u_errorName(status)); 
	return FALSE;
  } else if(length!=2) {
    inteltst.errln("Fail: len %d expected 3\n", length);
	return FALSE;
  } else if(u_strncmp(dst,dst_old,LENGTHOF(dst))) {
    inteltst.errln("Fail: got U+%04X U+%04X expected U+%04X U+%04X\n",
            dst[0],dst[1],dst_old[0],dst_old[1]);
	return FALSE;
  }


  //"Trying new tail
  status = U_ZERO_ERROR;
  length = u_shapeArabic(src, -1, dst, LENGTHOF(dst),
                         U_SHAPE_LETTERS_SHAPE|U_SHAPE_SEEN_TWOCELL_NEAR|U_SHAPE_TAIL_NEW_UNICODE, &status);
  if(U_FAILURE(status)) {
    inteltst.errln("Fail: status %s\n", u_errorName(status)); 
	return FALSE;
  } else if(length!=2) {
    inteltst.errln("Fail: len %d expected 3\n", length);
	return FALSE;
  } else if(u_strncmp(dst,dst_new,LENGTHOF(dst))) {
    inteltst.errln("Fail: got U+%04X U+%04X expected U+%04X U+%04X\n",
            dst[0],dst[1],dst_new[0],dst_new[1]);
	return FALSE;
  } 
  
  
  return TRUE;
  
}
	

};

void MultithreadTest::TestThreads()
{
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
        SimpleThread::sleep(100);
        logln(" Subthread started.");
    }

    logln("Waiting for threads to be set..");
    if (numThreadsStarted == 0) {
        errln("No threads could be started for testing!");
        return;
    }

    int32_t patience = 40; // seconds to wait

    while(patience--)
    {
        int32_t count = 0;
        umtx_lock(NULL);
        for(i=0;i<THREADTEST_NRTHREADS;i++)
        {
            if(threadTestChars[i] == '*')
            {
                count++;
            }
        }
        umtx_unlock(NULL);
        
        if(count == THREADTEST_NRTHREADS)
        {
            logln("->" + UnicodeString(threadTestChars) + "<- Got all threads! cya");
            for(i=0;i<THREADTEST_NRTHREADS;i++)
            {
                delete threads[i];
            }
            return;
        }

        logln("->" + UnicodeString(threadTestChars) + "<- Waiting..");
        SimpleThread::sleep(500);
    }

    errln("->" + UnicodeString(threadTestChars) + "<- PATIENCE EXCEEDED!! Still missing some.");
    for(i=0;i<THREADTEST_NRTHREADS;i++)
    {
        delete threads[i];
    }
}


void MultithreadTest::TestArabicShapingThreads()
{
    char threadTestChars[ARABICSHAPE_THREADTEST + 1];
    SimpleThread *threads[ARABICSHAPE_THREADTEST];
    int32_t numThreadsStarted = 0;

    int32_t i;
    
    for(i=0;i<ARABICSHAPE_THREADTEST;i++)
    {
        threadTestChars[i] = ' ';
        threads[i] = new TestArabicShapeThreads(&threadTestChars[i]);
    }
    threadTestChars[ARABICSHAPE_THREADTEST] = '\0';

    logln("-> do TestArabicShapingThreads <- Firing off threads.. ");
    for(i=0;i<ARABICSHAPE_THREADTEST;i++)
    {
        if (threads[i]->start() != 0) {
            errln("Error starting thread %d", i);
        }
        else {
            numThreadsStarted++;
        }
        //SimpleThread::sleep(100);
        logln(" Subthread started.");
    }

    logln("Waiting for threads to be set..");
    if (numThreadsStarted == 0) {
        errln("No threads could be started for testing!");
        return;
    }

    int32_t patience = 100; // seconds to wait

    while(patience--)
    {
        int32_t count = 0;
        umtx_lock(NULL);
        for(i=0;i<ARABICSHAPE_THREADTEST;i++)
        {
            if(threadTestChars[i] == '*')
            {
                count++;
            }
        }
        umtx_unlock(NULL);
        
        if(count == ARABICSHAPE_THREADTEST)
        {
            logln("->TestArabicShapingThreads <- Got all threads! cya");
            for(i=0;i<ARABICSHAPE_THREADTEST;i++)
            {
                delete threads[i];
            }
            return;
        }

        logln("-> TestArabicShapingThreads <- Waiting..");
        SimpleThread::sleep(500);
    }

    errln("-> TestArabicShapingThreads <- PATIENCE EXCEEDED!! Still missing some.");
    for(i=0;i<ARABICSHAPE_THREADTEST;i++)
    {
        delete threads[i];
    }
	
}

 
//-----------------------------------------------------------------------
//
//  TestMutex  - a simple (non-stress) test to verify that ICU mutexes
//               are actually mutexing.  Does not test the use of
//               mutexes within ICU services, but rather that the
//               platform's mutex support is at least superficially there.
//
//----------------------------------------------------------------------
static UMutex    gTestMutexA = U_MUTEX_INITIALIZER;
static UMutex    gTestMutexB = U_MUTEX_INITIALIZER;

static int     gThreadsStarted = 0; 
static int     gThreadsInMiddle = 0;
static int     gThreadsDone = 0;

static const int TESTMUTEX_THREAD_COUNT = 4;

static int safeIncr(int &var, int amt) {
    // Thread safe (using global mutex) increment of a variable.
    // Return the updated value.
    // Can also be used as a safe load of a variable by incrementing it by 0.
    Mutex m;
    var += amt;
    return var;
}

class TestMutexThread : public SimpleThread
{
public:
    virtual void run()
    {
        // This is the code that each of the spawned threads runs.
        // All of the spawned threads bunch up together at each of the two mutexes
        // because the main holds the mutexes until they do.
        //
        safeIncr(gThreadsStarted, 1);
        umtx_lock(&gTestMutexA);
        umtx_unlock(&gTestMutexA);
        safeIncr(gThreadsInMiddle, 1);
        umtx_lock(&gTestMutexB);
        umtx_unlock(&gTestMutexB);
        safeIncr(gThreadsDone, 1);
    }
};

void MultithreadTest::TestMutex()
{
    // Start up the test threads.  They should all pile up waiting on
    // gTestMutexA, which we (the main thread) hold until the test threads
    //   all get there.
    gThreadsStarted = 0;
    gThreadsInMiddle = 0;
    gThreadsDone = 0;
    umtx_lock(&gTestMutexA);
    TestMutexThread  *threads[TESTMUTEX_THREAD_COUNT];
    int i;
    int32_t numThreadsStarted = 0;
    for (i=0; i<TESTMUTEX_THREAD_COUNT; i++) {
        threads[i] = new TestMutexThread;
        if (threads[i]->start() != 0) {
            errln("Error starting thread %d", i);
        }
        else {
            numThreadsStarted++;
        }
    }
    if (numThreadsStarted == 0) {
        errln("No threads could be started for testing!");
        return;
    }

    int patience = 0;
    while (safeIncr(gThreadsStarted, 0) != TESTMUTEX_THREAD_COUNT) {
        if (patience++ > 24) {
            TSMTHREAD_FAIL("Patience Exceeded");
            return;
        }
        SimpleThread::sleep(500);
    }
    // None of the test threads should have advanced past the first mutex.
    TSMTHREAD_ASSERT(gThreadsInMiddle==0);
    TSMTHREAD_ASSERT(gThreadsDone==0);

    //  All of the test threads have made it to the first mutex.
    //  We (the main thread) now let them advance to the second mutex,
    //   where they should all pile up again.
    umtx_lock(&gTestMutexB);
    umtx_unlock(&gTestMutexA);

    patience = 0;
    while (safeIncr(gThreadsInMiddle, 0) != TESTMUTEX_THREAD_COUNT) {
        if (patience++ > 24) {
            TSMTHREAD_FAIL("Patience Exceeded");
            return;
        }
        SimpleThread::sleep(500);
    }
    TSMTHREAD_ASSERT(gThreadsDone==0);

    //  All test threads made it to the second mutex.
    //   Now let them proceed from there.  They will all terminate.
    umtx_unlock(&gTestMutexB);    
    patience = 0;
    while (safeIncr(gThreadsDone, 0) != TESTMUTEX_THREAD_COUNT) {
        if (patience++ > 24) {
            TSMTHREAD_FAIL("Patience Exceeded");
            return;
        }
        SimpleThread::sleep(500);
    }

    // All threads made it by both mutexes.

    for (i=0; i<TESTMUTEX_THREAD_COUNT; i++) {
        delete threads[i];
    }

}


//-------------------------------------------------------------------------------------------
//
// class ThreadWithStatus - a thread that we can check the status and error condition of
//
//-------------------------------------------------------------------------------------------
class ThreadWithStatus : public SimpleThread
{
public:
    UBool  getError() { return (fErrors > 0); } 
    UBool  getError(UnicodeString& fillinError) { fillinError = fErrorString; return (fErrors > 0); } 
    virtual ~ThreadWithStatus(){}
protected:
    ThreadWithStatus() :  fErrors(0) {}
    void error(const UnicodeString &error) { 
        fErrors++; fErrorString = error; 
        SimpleThread::errorFunc();  
    }
    void error() { error("An error occured."); }
private:
    int32_t fErrors;
    UnicodeString fErrorString;
};



//-------------------------------------------------------------------------------------------
//
//   TestMultithreadedIntl.  Test ICU Formatting n a multi-threaded environment 
//
//-------------------------------------------------------------------------------------------


// * Show exactly where the string's differences lie.
UnicodeString showDifference(const UnicodeString& expected, const UnicodeString& result)
{
    UnicodeString res;
    res = expected + "<Expected\n";
    if(expected.length() != result.length())
        res += " [ Different lengths ] \n";
    else
    {
        for(int32_t i=0;i<expected.length();i++)
        {
            if(expected[i] == result[i])
            {
                res += " ";
            }
            else
            {
                res += "|";
            }
        }
        res += "<Differences";
        res += "\n";
    }
    res += result + "<Result\n";

    return res;
}




//-------------------------------------------------------------------------------------------
//
//   FormatThreadTest - a thread that tests performing a number of numberformats.
//
//-------------------------------------------------------------------------------------------

const int kFormatThreadIterations = 100;  // # of iterations per thread
const int kFormatThreadThreads    = 10;  // # of threads to spawn   
const int kFormatThreadPatience   = 60;  // time in seconds to wait for all threads

#if !UCONFIG_NO_FORMATTING



struct FormatThreadTestData
{
    double number;
    UnicodeString string;
    FormatThreadTestData(double a, const UnicodeString& b) : number(a),string(b) {}
} ;


// "Someone from {2} is receiving a #{0} error - {1}. Their telephone call is costing {3 number,currency}."

static void formatErrorMessage(UErrorCode &realStatus, const UnicodeString& pattern, const Locale& theLocale,
                     UErrorCode inStatus0, /* statusString 1 */ const Locale &inCountry2, double currency3, // these numbers are the message arguments.
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

    MessageFormat *fmt = new MessageFormat("MessageFormat's API is broken!!!!!!!!!!!",realStatus);
    fmt->setLocale(theLocale);
    fmt->applyPattern(pattern, realStatus);
    
    if (U_FAILURE(realStatus)) {
        delete fmt;
        return;
    }

    FieldPosition ignore = 0;                      
    fmt->format(myArgs,4,result,ignore,realStatus);

    delete fmt;
}

/**
 * Class for thread-safe (theoretically) format.
 * 
 *
 * Its constructor, destructor, and init/fini are NOT thread safe.
 */
class ThreadSafeFormat {
public:
  /* give a unique offset to each thread */
  ThreadSafeFormat();
  UBool doStuff(int32_t offset, UnicodeString &appendErr, UErrorCode &status);
private:
  LocalPointer<NumberFormat> fFormat; // formtter - default constructed currency
  Formattable  fYDDThing;    // Formattable currency - YDD
  Formattable  fBBDThing;   // Formattable currency - BBD

  // statics
private:
  static LocalPointer<NumberFormat> gFormat;
  static NumberFormat *createFormat(UErrorCode &status);
  static Formattable gYDDThing, gBBDThing;
public:
  static void init(UErrorCode &status); // avoid static init.
  static void fini(UErrorCode &status); // avoid static fini
};

LocalPointer<NumberFormat> ThreadSafeFormat::gFormat;
Formattable ThreadSafeFormat::gYDDThing;
Formattable ThreadSafeFormat::gBBDThing;
UnicodeString gYDDStr, gBBDStr;
NumberFormat *ThreadSafeFormat::createFormat(UErrorCode &status) {
  LocalPointer<NumberFormat> fmt(NumberFormat::createCurrencyInstance(Locale::getUS(), status));
  return fmt.orphan();
}


static const UChar kYDD[] = { 0x59, 0x44, 0x44, 0x00 };
static const UChar kBBD[] = { 0x42, 0x42, 0x44, 0x00 };
static const UChar kUSD[] = { 0x55, 0x53, 0x44, 0x00 };

void ThreadSafeFormat::init(UErrorCode &status) {
  gFormat.adoptInstead(createFormat(status));
  gYDDThing.adoptObject(new CurrencyAmount(123.456, kYDD, status));
  gBBDThing.adoptObject(new CurrencyAmount(987.654, kBBD, status));
  if (U_FAILURE(status)) {
      return;
  }
  gFormat->format(gYDDThing, gYDDStr, NULL, status);
  gFormat->format(gBBDThing, gBBDStr, NULL, status);
}

void ThreadSafeFormat::fini(UErrorCode &/*status*/) {
  gFormat.adoptInstead(NULL);
}

ThreadSafeFormat::ThreadSafeFormat() {
}

UBool ThreadSafeFormat::doStuff(int32_t offset, UnicodeString &appendErr, UErrorCode &status) {
  UBool okay = TRUE;
  if(fFormat.isNull()) {
    fFormat.adoptInstead(createFormat(status));
  }

  if(u_strcmp(fFormat->getCurrency(), kUSD)) {
    appendErr.append("fFormat currency != ")
      .append(kUSD)
      .append(", =")
      .append(fFormat->getCurrency())
      .append("! ");
    okay = FALSE;
  }

  if(u_strcmp(gFormat->getCurrency(), kUSD)) {
    appendErr.append("gFormat currency != ")
      .append(kUSD)
      .append(", =")
      .append(gFormat->getCurrency())
      .append("! ");
    okay = FALSE;
  }
  UnicodeString str;
  const UnicodeString *o=NULL;
  Formattable f;
  const NumberFormat *nf = NULL; // only operate on it as const.
  switch(offset%4) {
  case 0:  f = gYDDThing;  o = &gYDDStr;  nf = gFormat.getAlias();  break;
  case 1:  f = gBBDThing;  o = &gBBDStr;  nf = gFormat.getAlias();  break;
  case 2:  f = gYDDThing;  o = &gYDDStr;  nf = fFormat.getAlias();  break;
  case 3:  f = gBBDThing;  o = &gBBDStr;  nf = fFormat.getAlias();  break;
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


class FormatThreadTest : public ThreadWithStatus
{
public:
    int     fNum;
    int     fTraceInfo;

    ThreadSafeFormat fTSF;

    FormatThreadTest() // constructor is NOT multithread safe.
        : ThreadWithStatus(),
        fNum(0),
        fTraceInfo(0),
        fOffset(0)
        // the locale to use
    {
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
            FormatThreadTestData((double)5.0, UnicodeString("5", "")),
                FormatThreadTestData( 6.0, UnicodeString("6", "")),
                FormatThreadTestData( 20.0, UnicodeString("20", "")),
                FormatThreadTestData( 8.0, UnicodeString("8", "")),
                FormatThreadTestData( 8.3, UnicodeString("8.3", "")),
                FormatThreadTestData( 12345, UnicodeString("12,345", "")),
                FormatThreadTestData( 81890.23, UnicodeString("81,890.23", "")),
        };
        int32_t kNumberFormatTestDataLength = (int32_t)(sizeof(kNumberFormatTestData) / 
                                                        sizeof(kNumberFormatTestData[0]));
        
        // Keep this data here to avoid static initialization.
        FormatThreadTestData kPercentFormatTestData[] = 
        {
            FormatThreadTestData((double)5.0, CharsToUnicodeString("500\\u00a0%")),
                FormatThreadTestData( 1.0, CharsToUnicodeString("100\\u00a0%")),
                FormatThreadTestData( 0.26, CharsToUnicodeString("26\\u00a0%")),
                FormatThreadTestData( 
                   16384.99, CharsToUnicodeString("1\\u00a0638\\u00a0499\\u00a0%")), // U+00a0 = NBSP
                FormatThreadTestData( 
                    81890.23, CharsToUnicodeString("8\\u00a0189\\u00a0023\\u00a0%")),
        };
        int32_t kPercentFormatTestDataLength = 
                (int32_t)(sizeof(kPercentFormatTestData) / sizeof(kPercentFormatTestData[0]));
        int32_t iteration;
        
        status = U_ZERO_ERROR;
        LocalPointer<NumberFormat> formatter(NumberFormat::createInstance(Locale::getEnglish(),status));
        if(U_FAILURE(status)) {
            error("Error on NumberFormat::createInstance().");
            goto cleanupAndReturn;
        }
        
        percentFormatter.adoptInstead(NumberFormat::createPercentInstance(Locale::getFrench(),status));
        if(U_FAILURE(status))             {
            error("Error on NumberFormat::createPercentInstance().");
            goto cleanupAndReturn;
        }
        
        for(iteration = 0;!getError() && iteration<kFormatThreadIterations;iteration++)
        {
            
            int32_t whichLine = (iteration + fOffset)%kNumberFormatTestDataLength;
            
            UnicodeString  output;
            
            formatter->format(kNumberFormatTestData[whichLine].number, output);
            
            if(0 != output.compare(kNumberFormatTestData[whichLine].string)) {
                error("format().. expected " + kNumberFormatTestData[whichLine].string 
                        + " got " + output);
                goto cleanupAndReturn;
            }
            
            // Now check percent.
            output.remove();
            whichLine = (iteration + fOffset)%kPercentFormatTestDataLength;
            
            percentFormatter->format(kPercentFormatTestData[whichLine].number, output);
            if(0 != output.compare(kPercentFormatTestData[whichLine].string))
            {
                error("percent format().. \n" + 
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
                patternToCheck=        "0:Someone from {2} is receiving a #{0}"
                                       " error - {1}. Their telephone call is costing "
                                       "{3,number,currency}."; // number,currency
                messageLocale=                      Locale("en","US");
                countryToCheck=                     Locale("","HR");
                currencyToCheck=                    8192.77;
                expected=  "0:Someone from Croatia is receiving a #4 error - "
                            "U_FILE_ACCESS_ERROR. Their telephone call is costing $8,192.77.";
                break;
            case 1:
                statusToCheck=                      U_INDEX_OUTOFBOUNDS_ERROR;
                patternToCheck=                     "1:A customer in {2} is receiving a #{0} error - {1}. Their telephone call is costing {3,number,currency}."; // number,currency
                messageLocale=                      Locale("de","DE@currency=DEM");
                countryToCheck=                     Locale("","BF");
                currencyToCheck=                    2.32;
                expected=                           CharsToUnicodeString(
                                                    "1:A customer in Burkina Faso is receiving a #8 error - U_INDEX_OUTOFBOUNDS_ERROR. Their telephone call is costing 2,32\\u00A0DEM.");
                break;
            case 2:
                statusToCheck=                      U_MEMORY_ALLOCATION_ERROR;
                patternToCheck=   "2:user in {2} is receiving a #{0} error - {1}. "
                                  "They insist they just spent {3,number,currency} "
                                  "on memory."; // number,currency
                messageLocale=                      Locale("de","AT@currency=ATS"); // Austrian German
                countryToCheck=                     Locale("","US"); // hmm
                currencyToCheck=                    40193.12;
                expected=       CharsToUnicodeString(
                            "2:user in Vereinigte Staaten is receiving a #7 error"
                            " - U_MEMORY_ALLOCATION_ERROR. They insist they just spent"
                            " \\u00f6S\\u00A040.193,12 on memory.");
                break;
            }
            
            UnicodeString result;
            UErrorCode status = U_ZERO_ERROR;
            formatErrorMessage(status,patternToCheck,messageLocale,statusToCheck,
                                countryToCheck,currencyToCheck,result);
            if(U_FAILURE(status))
            {
                UnicodeString tmp(u_errorName(status));
                error("Failure on message format, pattern=" + patternToCheck +
                        ", error = " + tmp);
                goto cleanupAndReturn;
            }
            
            if(result != expected)
            {
                error("PatternFormat: \n" + showDifference(expected,result));
                goto cleanupAndReturn;
            }
            // test the Thread Safe Format
            UnicodeString appendErr;
            if(!fTSF.doStuff(fNum, appendErr, status)) {
              error(appendErr);
              goto cleanupAndReturn;
            }
        }   /*  end of for loop */



cleanupAndReturn:
        //  while (fNum == 4) {SimpleThread::sleep(10000);}   // Force a failure by preventing thread from finishing
        fTraceInfo = 2;
    }
    
private:
    int32_t fOffset; // where we are testing from.
};

// ** The actual test function.

void MultithreadTest::TestThreadedIntl()
{
    int i;
    UnicodeString theErr;
    UBool   haveDisplayedInfo[kFormatThreadThreads];
    static const int32_t PATIENCE_SECONDS = 45;

    UErrorCode threadSafeErr = U_ZERO_ERROR;

    ThreadSafeFormat::init(threadSafeErr);
    assertSuccess("initializing ThreadSafeFormat", threadSafeErr, TRUE);

    //
    //  Create and start the test threads
    //
    logln("Spawning: %d threads * %d iterations each.",
                kFormatThreadThreads, kFormatThreadIterations);
    LocalArray<FormatThreadTest> tests(new FormatThreadTest[kFormatThreadThreads]);
    for(int32_t j = 0; j < kFormatThreadThreads; j++) {
        tests[j].fNum = j;
        int32_t threadStatus = tests[j].start();
        if (threadStatus != 0) {
            errln("System Error %d starting thread number %d.", threadStatus, j);
            SimpleThread::errorFunc();
            return;
        }
        haveDisplayedInfo[j] = FALSE;
    }


    // Spin, waiting for the test threads to finish.
    UBool   stillRunning;
    UDate startTime, endTime;
    startTime = Calendar::getNow();
    double lastComplaint = 0;
    do {
        /*  Spin until the test threads  complete. */
        stillRunning = FALSE;
        endTime = Calendar::getNow();
        double elapsedSeconds =  ((int32_t)(endTime - startTime)/U_MILLIS_PER_SECOND);
        if (elapsedSeconds > PATIENCE_SECONDS) {
            errln("Patience exceeded. Test is taking too long.");
            return;
        } else if((elapsedSeconds-lastComplaint) > 2.0) {
            infoln("%.1f seconds elapsed (still waiting..)", elapsedSeconds);
            lastComplaint = elapsedSeconds;
        }
        /*
         The following sleep must be here because the *BSD operating systems
         have a brain dead thread scheduler. They starve the child threads from
         CPU time.
        */
        SimpleThread::sleep(1); // yield
        for(i=0;i<kFormatThreadThreads;i++) {
            if (tests[i].isRunning()) {
                stillRunning = TRUE;
            } else if (haveDisplayedInfo[i] == FALSE) {
                logln("Thread # %d is complete..", i);
                if(tests[i].getError(theErr)) {
                    dataerrln(UnicodeString("#") + i + ": " + theErr);
                    SimpleThread::errorFunc();
                }
                haveDisplayedInfo[i] = TRUE;
            }
        }
    } while (stillRunning);

    //
    //  All threads have finished.
    //
    ThreadSafeFormat::fini(threadSafeErr);
    assertSuccess("finalizing ThreadSafeFormat", threadSafeErr, TRUE);
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

static UBool
skipLineBecauseOfBug(const UChar *s, int32_t length) {
    // TODO: Fix ICU ticket #8052
    if(length >= 3 &&
            (s[0] == 0xfb2 || s[0] == 0xfb3) &&
            s[1] == 0x334 &&
            (s[2] == 0xf73 || s[2] == 0xf75 || s[2] == 0xf81)) {
        return TRUE;
    }
    return FALSE;
}

static UCollationResult
normalizeResult(int32_t result) {
    return result<0 ? UCOL_LESS : result==0 ? UCOL_EQUAL : UCOL_GREATER;
}

class CollatorThreadTest : public ThreadWithStatus
{
private: 
    const Collator *coll;
    const Line *lines;
    int32_t noLines;
    UBool isAtLeastUCA62;
public:
    CollatorThreadTest()  : ThreadWithStatus(),
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

            if(skipLineBecauseOfBug(lines[i].buff, lines[i].buflen)) { continue; }

            int32_t resLen = coll->getSortKey(lines[i].buff, lines[i].buflen, newSk, 1024);

            if(oldSk != NULL) {
                int32_t skres = strcmp((char *)oldSk, (char *)newSk);
                int32_t cmpres = coll->compare(lines[prev].buff, lines[prev].buflen, lines[i].buff, lines[i].buflen);
                int32_t cmpres2 = coll->compare(lines[i].buff, lines[i].buflen, lines[prev].buff, lines[prev].buflen);

                if(cmpres != -cmpres2) {
                    error(UnicodeString("Compare result not symmetrical on line ") + (i + 1));
                    break;
                }

                if(cmpres != normalizeResult(skres)) {
                    error(UnicodeString("Difference between coll->compare and sortkey compare on line ") + (i + 1));
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
                    error(UnicodeString("Line is not greater or equal than previous line, for line ") + (i + 1));
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

    int32_t noSpawned = 0;
    int32_t spawnResult = 0;
    LocalArray<CollatorThreadTest> tests(new CollatorThreadTest[kCollatorThreadThreads]);

    logln(UnicodeString("Spawning: ") + kCollatorThreadThreads + " threads * " + kFormatThreadIterations + " iterations each.");
    int32_t j = 0;
    for(j = 0; j < kCollatorThreadThreads; j++) {
        //logln("Setting collator %i", j);
        tests[j].setCollator(coll.getAlias(), lines.getAlias(), lineNum, isAtLeastUCA62);
    }
    for(j = 0; j < kCollatorThreadThreads; j++) {
        log("%i ", j);
        spawnResult = tests[j].start();
        if(spawnResult != 0) {
            infoln("THREAD INFO: Couldn't spawn more than %i threads", noSpawned);
            break;
        }
        noSpawned++;
    }
    logln("Spawned all");
    if (noSpawned == 0) {
        errln("No threads could be spawned.");
        return;
    }

    for(int32_t patience = kCollatorThreadPatience;patience > 0; patience --)
    {
        logln("Waiting...");

        int32_t i;
        int32_t terrs = 0;
        int32_t completed =0;

        for(i=0;i<kCollatorThreadThreads;i++)
        {
            if (tests[i].isRunning() == FALSE)
            {
                completed++;

                //logln(UnicodeString("Test #") + i + " is complete.. ");

                UnicodeString theErr;
                if(tests[i].getError(theErr))
                {
                    terrs++;
                    errln(UnicodeString("#") + i + ": " + theErr);
                }
                // print out the error, too, if any.
            }
        }
        logln("Completed %i tests", completed);

        if(completed == noSpawned)
        {
            logln("Done! All %i tests are finished", noSpawned);

            if(terrs)
            {
                errln("There were errors.");
                SimpleThread::errorFunc();
            }
            return;
        }

        SimpleThread::sleep(900);
    }
    errln("patience exceeded. ");
    SimpleThread::errorFunc();
}

#endif /* #if !UCONFIG_NO_COLLATION */




//-------------------------------------------------------------------------------------------
//
//   StringThreadTest2 
//
//-------------------------------------------------------------------------------------------

const int kStringThreadIterations = 2500;// # of iterations per thread
const int kStringThreadThreads    = 10;  // # of threads to spawn
const int kStringThreadPatience   = 120; // time in seconds to wait for all threads


class StringThreadTest2 : public ThreadWithStatus
{
public:
    int                 fNum;
    int                 fTraceInfo;
    const UnicodeString *fSharedString;

    StringThreadTest2(const UnicodeString *sharedString, int num) // constructor is NOT multithread safe.
        : ThreadWithStatus(),
        fNum(num),
        fTraceInfo(0),
        fSharedString(sharedString)
    {
    };


    virtual void run()
    {
        fTraceInfo    = 1;
        int loopCount = 0;

        for (loopCount = 0; loopCount < kStringThreadIterations; loopCount++) {
            if (*fSharedString != "This is the original test string.") {
                error("Original string is corrupt.");
                break;
            }
            UnicodeString s1 = *fSharedString;
            s1 += "cat this";
            UnicodeString s2(s1);
            UnicodeString s3 = *fSharedString;
            s2 = s3;
            s3.truncate(12);
            s2.truncate(0);
        }

        //  while (fNum == 4) {SimpleThread::sleep(10000);}   // Force a failure by preventing thread from finishing
        fTraceInfo = 2;
    }
    
};

// ** The actual test function.

void MultithreadTest::TestString()
{
    int     patience;
    int     terrs = 0;
    int     j;

    UnicodeString *testString = new UnicodeString("This is the original test string.");

    // Not using LocalArray<StringThreadTest2> tests[kStringThreadThreads];
    // because we don't always want to delete them.
    // See the comments below the cleanupAndReturn label.
    StringThreadTest2  *tests[kStringThreadThreads];
    for(j = 0; j < kStringThreadThreads; j++) {
        tests[j] = new StringThreadTest2(testString, j);
    }
 
    logln(UnicodeString("Spawning: ") + kStringThreadThreads + " threads * " + kStringThreadIterations + " iterations each.");
    for(j = 0; j < kStringThreadThreads; j++) {
        int32_t threadStatus = tests[j]->start();
        if (threadStatus != 0) {
            errln("System Error %d starting thread number %d.", threadStatus, j);
            SimpleThread::errorFunc();  
            goto cleanupAndReturn;
        }
    }

    for(patience = kStringThreadPatience;patience > 0; patience --)
    {
        logln("Waiting...");

        int32_t i;
        terrs = 0;
        int32_t completed =0;

        for(i=0;i<kStringThreadThreads;i++) {
            if (tests[i]->isRunning() == FALSE)
            {
                completed++;
                
                logln(UnicodeString("Test #") + i + " is complete.. ");
                
                UnicodeString theErr;
                if(tests[i]->getError(theErr))
                {
                    terrs++;
                    errln(UnicodeString("#") + i + ": " + theErr);
                }
                // print out the error, too, if any.
            }
        }
        
        if(completed == kStringThreadThreads)
        {
            logln("Done!");
            if(terrs) {
                errln("There were errors.");
            }
            break;
        }

        SimpleThread::sleep(900);
    }

    if (patience <= 0) {
        errln("patience exceeded. ");
        // while (TRUE) {SimpleThread::sleep(10000);}   // TODO:   for debugging.  Sleep forever on failure.
        terrs++;
    }

    if (terrs > 0) {
        SimpleThread::errorFunc();  
    }

cleanupAndReturn:
    if (terrs == 0) {
        /*
        Don't clean up if there are errors. This prevents crashes if the
        threads are still running and using this data. This will only happen
        if there is an error with the test, ICU, or the machine is too slow.
        It's better to leak than crash.
        */
        for(j = 0; j < kStringThreadThreads; j++) {
            delete tests[j];
        }
        delete testString;
    }
}

#endif // ICU_USE_THREADS
