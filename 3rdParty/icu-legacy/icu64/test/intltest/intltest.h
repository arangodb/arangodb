// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/


/**
 * IntlTest is a base class for tests.  */

#ifndef _INTLTEST
#define _INTLTEST

// The following includes utypes.h, uobject.h and unistr.h
#include "unicode/fmtable.h"
#include "unicode/testlog.h"
#include "unicode/uniset.h"

U_NAMESPACE_USE

#if U_PLATFORM == U_PF_OS390
// avoid collision with math.h/log()
// this must be after including utypes.h so that U_PLATFORM is actually defined
#pragma map(IntlTest::log( const UnicodeString &message ),"logos390")
#endif

//-----------------------------------------------------------------------------
//convenience classes to ease porting code that uses the Java
//string-concatenation operator (moved from findword test by rtg)
UnicodeString UCharToUnicodeString(UChar c);
UnicodeString Int64ToUnicodeString(int64_t num);
UnicodeString DoubleToUnicodeString(double num);
//UnicodeString operator+(const UnicodeString& left, int64_t num); // Some compilers don't allow this because of the long type.
UnicodeString operator+(const UnicodeString& left, long num);
UnicodeString operator+(const UnicodeString& left, unsigned long num);
UnicodeString operator+(const UnicodeString& left, double num);
UnicodeString operator+(const UnicodeString& left, char num);
UnicodeString operator+(const UnicodeString& left, short num);
UnicodeString operator+(const UnicodeString& left, int num);
UnicodeString operator+(const UnicodeString& left, unsigned char num);
UnicodeString operator+(const UnicodeString& left, unsigned short num);
UnicodeString operator+(const UnicodeString& left, unsigned int num);
UnicodeString operator+(const UnicodeString& left, float num);
#if !UCONFIG_NO_FORMATTING
UnicodeString toString(const Formattable& f); // liu
UnicodeString toString(int32_t n);
#endif
UnicodeString toString(UBool b);

//-----------------------------------------------------------------------------

// Use the TESTCASE macro in subclasses of IntlTest.  Define the
// runIndexedTest method in this fashion:
//
//| void MyTest::runIndexedTest(int32_t index, UBool exec,
//|                             const char* &name, char* /*par*/) {
//|     switch (index) {
//|         TESTCASE(0,TestSomething);
//|         TESTCASE(1,TestSomethingElse);
//|         TESTCASE(2,TestAnotherThing);
//|         default: name = ""; break;
//|     }
//| }
#define TESTCASE(id,test)             \
    case id:                          \
        name = #test;                 \
        if (exec) {                   \
            logln(#test "---");       \
            logln();                  \
            test();                   \
        }                             \
        break

// More convenient macros. These allow easy reordering of the test cases.
//
//| void MyTest::runIndexedTest(int32_t index, UBool exec,
//|                             const char* &name, char* /*par*/) {
//|     TESTCASE_AUTO_BEGIN;
//|     TESTCASE_AUTO(TestSomething);
//|     TESTCASE_AUTO(TestSomethingElse);
//|     TESTCASE_AUTO(TestAnotherThing);
//|     TESTCASE_AUTO_END;
//| }
#define TESTCASE_AUTO_BEGIN \
    for(;;) { \
        int32_t testCaseAutoNumber = 0

#define TESTCASE_AUTO(test) \
        if (index == testCaseAutoNumber++) { \
            name = #test; \
            if (exec) { \
                logln(#test "---"); \
                logln(); \
                test(); \
            } \
            break; \
        }

#define TESTCASE_AUTO_CLASS(TestClass) \
        if (index == testCaseAutoNumber++) { \
            name = #TestClass; \
            if (exec) { \
                logln(#TestClass "---"); \
                logln(); \
                TestClass test; \
                callTest(test, par); \
            } \
            break; \
        }

#define TESTCASE_AUTO_CREATE_CLASS(TestClass) \
        if (index == testCaseAutoNumber++) { \
            name = #TestClass; \
            if (exec) { \
                logln(#TestClass "---"); \
                logln(); \
                LocalPointer<IntlTest> test(create##TestClass()); \
                callTest(*test, par); \
            } \
            break; \
        }

#define TESTCASE_AUTO_END \
        name = ""; \
        break; \
    }


// WHERE Macro yields a literal string of the form "source_file_name:line number "
#define WHERE __FILE__ ":" XLINE(__LINE__) " "
#define XLINE(s) LINE(s)
#define LINE(s) #s

class IntlTest : public TestLog {
public:

    IntlTest();
    // TestLog has a virtual destructor.

    virtual UBool runTest( char* name = NULL, char* par = NULL, char *baseName = NULL); // not to be overidden

    virtual UBool setVerbose( UBool verbose = TRUE );
    virtual UBool setNoErrMsg( UBool no_err_msg = TRUE );
    virtual UBool setQuick( UBool quick = TRUE );
    virtual UBool setLeaks( UBool leaks = TRUE );
    virtual UBool setNotime( UBool no_time = TRUE );
    virtual UBool setWarnOnMissingData( UBool warn_on_missing_data = TRUE );
    virtual int32_t setThreadCount( int32_t count = 1);

    virtual int32_t getErrors( void );
    virtual int32_t getDataErrors (void );

    virtual void setCaller( IntlTest* callingTest ); // for internal use only
    virtual void setPath( char* path ); // for internal use only

    virtual void log( const UnicodeString &message );

    virtual void logln( const UnicodeString &message );

    virtual void logln( void );

    /**
     * Replaces isICUVersionAtLeast and isICUVersionBefore
     * log that an issue is known.
     * Usually used this way:
     * <code>if( ... && logKnownIssue("12345", "some bug")) continue; </code>
     * @param ticket ticket string, "12345" or "cldrbug:1234"
     * @param message optional message string
     * @return true if test should be skipped
     */
    UBool logKnownIssue( const char *ticket, const UnicodeString &message );
    /**
     * Replaces isICUVersionAtLeast and isICUVersionBefore
     * log that an issue is known.
     * Usually used this way:
     * <code>if( ... && logKnownIssue("12345", "some bug")) continue; </code>
     * @param ticket ticket string, "12345" or "cldrbug:1234"
     * @return true if test should be skipped
     */
    UBool logKnownIssue( const char *ticket );
    /**
     * Replaces isICUVersionAtLeast and isICUVersionBefore
     * log that an issue is known.
     * Usually used this way:
     * <code>if( ... && logKnownIssue("12345", "some bug")) continue; </code>
     * @param ticket ticket string, "12345" or "cldrbug:1234"
     * @param message optional message string
     * @return true if test should be skipped
     */
    UBool logKnownIssue( const char *ticket, const char *fmt, ...);

    virtual void info( const UnicodeString &message );

    virtual void infoln( const UnicodeString &message );

    virtual void infoln( void );

    virtual void err(void);

    virtual void err( const UnicodeString &message );

    virtual void errln( const UnicodeString &message );

    virtual void dataerr( const UnicodeString &message );

    virtual void dataerrln( const UnicodeString &message );

    void errcheckln(UErrorCode status, const UnicodeString &message );

    // convenience functions: sprintf() + errln() etc.
    void log(const char *fmt, ...);
    void logln(const char *fmt, ...);
    void info(const char *fmt, ...);
    void infoln(const char *fmt, ...);
    void err(const char *fmt, ...);
    void errln(const char *fmt, ...);
    void dataerr(const char *fmt, ...);
    void dataerrln(const char *fmt, ...);

    /**
     * logs an error (even if status==U_ZERO_ERROR), but
     * calls dataerrln() or errln() depending on the type of error.
     * Does not report the status code.
     * @param status parameter for selecting whether errln or dataerrln is called.
     */
    void errcheckln(UErrorCode status, const char *fmt, ...);

    // Print ALL named errors encountered so far
    void printErrors();

    // print known issues. return TRUE if there were any.
    UBool printKnownIssues();

    virtual void usage( void ) ;

    /**
     * Returns a uniform random value x, with 0.0 <= x < 1.0.  Use
     * with care: Does not return all possible values; returns one of
     * 714,025 values, uniformly spaced.  However, the period is
     * effectively infinite.  See: Numerical Recipes, section 7.1.
     *
     * @param seedp pointer to seed. Set *seedp to any negative value
     * to restart the sequence.
     */
    static float random(int32_t* seedp);

    /**
     * Convenience method using a global seed.
     */
    static float random();


    /**
     *   Integer random numbers, similar to C++ std::minstd_rand, with the same algorithm
     *   and constants.  Allow additional access to internal state, for use by monkey tests,
     *   which need to recreate previous random sequences beginning near a failure point.
     */
    class icu_rand {
      public:
        icu_rand(uint32_t seed = 1);
        ~icu_rand();
        void seed(uint32_t seed);
        uint32_t operator()();
        /**
          * Get a seed corresponding to the current state of the generator.
          * Seeding any generator with this value will cause it to produce the
          * same sequence as this one will from this point forward.
          */
        uint32_t getSeed();
      private:
        uint32_t fLast;
    };



    enum { kMaxProps = 16 };

    virtual void setProperty(const char* propline);
    virtual const char* getProperty(const char* prop);

    /* JUnit-like assertions. Each returns TRUE if it succeeds. */
    UBool assertTrue(const char* message, UBool condition, UBool quiet=FALSE, UBool possibleDataError=FALSE, const char *file=NULL, int line=0);
    UBool assertFalse(const char* message, UBool condition, UBool quiet=FALSE, UBool possibleDataError=FALSE);
    /**
     * @param possibleDataError - if TRUE, use dataerrln instead of errcheckln on failure
     * @return TRUE on success, FALSE on failure.
     */
    UBool assertSuccess(const char* message, UErrorCode ec, UBool possibleDataError=FALSE, const char *file=NULL, int line=0);
    UBool assertEquals(const char* message, const UnicodeString& expected,
                       const UnicodeString& actual, UBool possibleDataError=FALSE);
    UBool assertEquals(const char* message, const char* expected, const char* actual);
    UBool assertEquals(const char* message, UBool expected, UBool actual);
    UBool assertEquals(const char* message, int32_t expected, int32_t actual);
    UBool assertEquals(const char* message, int64_t expected, int64_t actual);
    UBool assertEquals(const char* message, double expected, double actual);
    UBool assertEquals(const char* message, UErrorCode expected, UErrorCode actual);
    UBool assertEquals(const char* message, const UnicodeSet& expected, const UnicodeSet& actual);
#if !UCONFIG_NO_FORMATTING
    UBool assertEquals(const char* message, const Formattable& expected,
                       const Formattable& actual, UBool possibleDataError=FALSE);
    UBool assertEquals(const UnicodeString& message, const Formattable& expected,
                       const Formattable& actual);
#endif
    UBool assertTrue(const UnicodeString& message, UBool condition, UBool quiet=FALSE, UBool possibleDataError=FALSE);
    UBool assertFalse(const UnicodeString& message, UBool condition, UBool quiet=FALSE, UBool possibleDataError=FALSE);
    UBool assertSuccess(const UnicodeString& message, UErrorCode ec);
    UBool assertEquals(const UnicodeString& message, const UnicodeString& expected,
                       const UnicodeString& actual, UBool possibleDataError=FALSE);
    UBool assertEquals(const UnicodeString& message, const char* expected, const char* actual);
    UBool assertEquals(const UnicodeString& message, UBool expected, UBool actual);
    UBool assertEquals(const UnicodeString& message, int32_t expected, int32_t actual);
    UBool assertEquals(const UnicodeString& message, int64_t expected, int64_t actual);
    UBool assertEquals(const UnicodeString& message, double expected, double actual);
    UBool assertEquals(const UnicodeString& message, UErrorCode expected, UErrorCode actual);
    UBool assertEquals(const UnicodeString& message, const UnicodeSet& expected, const UnicodeSet& actual);

    virtual void runIndexedTest( int32_t index, UBool exec, const char* &name, char* par = NULL ); // overide !

    virtual UBool runTestLoop( char* testname, char* par, char *baseName );

    virtual int32_t IncErrorCount( void );

    virtual int32_t IncDataErrorCount( void );

    virtual UBool callTest( IntlTest& testToBeCalled, char* par );


    UBool       verbose;
    UBool       no_err_msg;
    UBool       quick;
    UBool       leaks;
    UBool       warn_on_missing_data;
    UBool       no_time;
    int32_t     threadCount;

private:
    UBool       LL_linestart;
    int32_t     LL_indentlevel;

    int32_t     errorCount;
    int32_t     dataErrorCount;
    IntlTest*   caller;
    char*       testPath;           // specifies subtests

    char basePath[1024];
    char currName[1024]; // current test name

    //FILE *testoutfp;
    void *testoutfp;

    const char* proplines[kMaxProps];
    int32_t     numProps;

protected:

    virtual void LL_message( UnicodeString message, UBool newline );

    // used for collation result reporting, defined here for convenience

    static UnicodeString &prettify(const UnicodeString &source, UnicodeString &target);
    static UnicodeString prettify(const UnicodeString &source, UBool parseBackslash=FALSE);
    // digits=-1 determines the number of digits automatically
    static UnicodeString &appendHex(uint32_t number, int32_t digits, UnicodeString &target);
    static UnicodeString toHex(uint32_t number, int32_t digits=-1);
    static inline UnicodeString toHex(int32_t number, int32_t digits=-1) {
        return toHex((uint32_t)number, digits);
    }

public:
    static void setICU_DATA();       // Set up ICU_DATA if necessary.

    static const char* pathToDataDirectory();

public:
    UBool run_phase2( char* name, char* par ); // internally, supports reporting memory leaks
    static const char* loadTestData(UErrorCode& err);
    virtual const char* getTestDataPath(UErrorCode& err);
    static const char* getSourceTestData(UErrorCode& err);
    static char *getUnidataPath(char path[]);

// static members
public:
    static IntlTest* gTest;
    static const char* fgDataDir;

};

void it_log( UnicodeString message );
void it_logln( UnicodeString message );
void it_logln( void );
void it_info( UnicodeString message );
void it_infoln( UnicodeString message );
void it_infoln( void );
void it_err(void);
void it_err( UnicodeString message );
void it_errln( UnicodeString message );
void it_dataerr( UnicodeString message );
void it_dataerrln( UnicodeString message );

/**
 * This is a variant of cintltst/ccolltst.c:CharsToUChars().
 * It converts a character string into a UnicodeString, with
 * unescaping \u sequences.
 */
extern UnicodeString CharsToUnicodeString(const char* chars);

/* alias for CharsToUnicodeString */
extern UnicodeString ctou(const char* chars);

#endif // _INTLTEST
