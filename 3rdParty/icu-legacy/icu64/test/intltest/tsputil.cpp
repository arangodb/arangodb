// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2011, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include "tsputil.h"

#include <float.h> // DBL_MAX, DBL_MIN
#include "putilimp.h"

#define CASE(id,test) case id: name = #test; if (exec) { logln(#test "---"); logln((UnicodeString)""); test(); } break;

void 
PUtilTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    //if (exec) logln("TestSuite PUtilTest: ");
    switch (index) {
        CASE(0, testMaxMin)
        CASE(1, testNaN)
        CASE(2, testPositiveInfinity)
        CASE(3, testNegativeInfinity)
        CASE(4, testZero)
//        CASE(, testIEEEremainder)

        default: name = ""; break; //needed to end loop
    }
}

#if 0
void
PUtilTest::testIEEEremainder()
{
    double    pinf  = uprv_getInfinity();
    double    ninf  = -uprv_getInfinity();
    double    nan   = uprv_getNaN();
    double    pzero = 0.0;
    double    nzero = 0.0;

    nzero *= -1;

    // simple remainder checks
    remainderTest(7.0, 2.5, -0.5);
    remainderTest(7.0, -2.5, -0.5);
#if U_PLATFORM != U_PF_OS390
    // ### TODO:
    // The following tests fails on S/390 with IEEE support in release builds;
    // debug builds work.
    // The functioning of ChoiceFormat is not affected by this bug.
    remainderTest(-7.0, 2.5, 0.5);
    remainderTest(-7.0, -2.5, 0.5);
#endif
    remainderTest(5.0, 3.0, -1.0);
    
    // this should work
    //remainderTest(43.7, 2.5, 1.25);

    /*

    // infinity and real
    remainderTest(pinf, 1.0, 1.25);
    remainderTest(1.0, pinf, 1.0);
    remainderTest(ninf, 1.0, 1.25);
    remainderTest(1.0, ninf, 1.0);

    // test infinity and nan
    remainderTest(ninf, pinf, 1.25);
    remainderTest(ninf, nan, 1.25);
    remainderTest(pinf, nan, 1.25);

    // test infinity and zero
    remainderTest(pinf, pzero, 1.25);
    remainderTest(pinf, nzero, 1.25);
    remainderTest(ninf, pzero, 1.25);
    remainderTest(ninf, nzero, 1.25);
*/
}

void
PUtilTest::remainderTest(double x, double y, double exp)
{
    double result = uprv_IEEEremainder(x,y);

    if(        uprv_isNaN(result) && 
        ! ( uprv_isNaN(x) || uprv_isNaN(y))) {
        errln(UnicodeString("FAIL: got NaN as result without NaN as argument"));
        errln(UnicodeString("      IEEEremainder(") + x + ", " + y + ") is " + result + ", expected " + exp);
    }
    else if(result != exp)
        errln(UnicodeString("FAIL: IEEEremainder(") + x + ", " + y + ") is " + result + ", expected " + exp);
    else
        logln(UnicodeString("OK: IEEEremainder(") + x + ", " + y + ") is " + result);

}
#endif

void
PUtilTest::testMaxMin()
{
    double    pinf        = uprv_getInfinity();
    double    ninf        = -uprv_getInfinity();
    double    nan        = uprv_getNaN();
    double    pzero        = 0.0;
    double    nzero        = 0.0;

    nzero *= -1;

    // +Inf with -Inf
    maxMinTest(pinf, ninf, pinf, true);
    maxMinTest(pinf, ninf, ninf, false);

    // +Inf with +0 and -0
    maxMinTest(pinf, pzero, pinf, true);
    maxMinTest(pinf, pzero, pzero, false);
    maxMinTest(pinf, nzero, pinf, true);
    maxMinTest(pinf, nzero, nzero, false);

    // -Inf with +0 and -0
    maxMinTest(ninf, pzero, pzero, true);
    maxMinTest(ninf, pzero, ninf, false);
    maxMinTest(ninf, nzero, nzero, true);
    maxMinTest(ninf, nzero, ninf, false);

    // NaN with +Inf and -Inf
    maxMinTest(pinf, nan, nan, true);
    maxMinTest(pinf, nan, nan, false);
    maxMinTest(ninf, nan, nan, true);
    maxMinTest(ninf, nan, nan, false);

    // NaN with NaN
    maxMinTest(nan, nan, nan, true);
    maxMinTest(nan, nan, nan, false);

    // NaN with +0 and -0
    maxMinTest(nan, pzero, nan, true);
    maxMinTest(nan, pzero, nan, false);
    maxMinTest(nan, nzero, nan, true);
    maxMinTest(nan, nzero, nan, false);

    // +Inf with DBL_MAX and DBL_MIN
    maxMinTest(pinf, DBL_MAX, pinf, true);
    maxMinTest(pinf, -DBL_MAX, pinf, true);
    maxMinTest(pinf, DBL_MIN, pinf, true);
    maxMinTest(pinf, -DBL_MIN, pinf, true);
    maxMinTest(pinf, DBL_MIN, DBL_MIN, false);
    maxMinTest(pinf, -DBL_MIN, -DBL_MIN, false);
    maxMinTest(pinf, DBL_MAX, DBL_MAX, false);
    maxMinTest(pinf, -DBL_MAX, -DBL_MAX, false);

    // -Inf with DBL_MAX and DBL_MIN
    maxMinTest(ninf, DBL_MAX, DBL_MAX, true);
    maxMinTest(ninf, -DBL_MAX, -DBL_MAX, true);
    maxMinTest(ninf, DBL_MIN, DBL_MIN, true);
    maxMinTest(ninf, -DBL_MIN, -DBL_MIN, true);
    maxMinTest(ninf, DBL_MIN, ninf, false);
    maxMinTest(ninf, -DBL_MIN, ninf, false);
    maxMinTest(ninf, DBL_MAX, ninf, false);
    maxMinTest(ninf, -DBL_MAX, ninf, false);

    // +0 with DBL_MAX and DBL_MIN
    maxMinTest(pzero, DBL_MAX, DBL_MAX, true);
    maxMinTest(pzero, -DBL_MAX, pzero, true);
    maxMinTest(pzero, DBL_MIN, DBL_MIN, true);
    maxMinTest(pzero, -DBL_MIN, pzero, true);
    maxMinTest(pzero, DBL_MIN, pzero, false);
    maxMinTest(pzero, -DBL_MIN, -DBL_MIN, false);
    maxMinTest(pzero, DBL_MAX, pzero, false);
    maxMinTest(pzero, -DBL_MAX, -DBL_MAX, false);

    // -0 with DBL_MAX and DBL_MIN
    maxMinTest(nzero, DBL_MAX, DBL_MAX, true);
    maxMinTest(nzero, -DBL_MAX, nzero, true);
    maxMinTest(nzero, DBL_MIN, DBL_MIN, true);
    maxMinTest(nzero, -DBL_MIN, nzero, true);
    maxMinTest(nzero, DBL_MIN, nzero, false);
    maxMinTest(nzero, -DBL_MIN, -DBL_MIN, false);
    maxMinTest(nzero, DBL_MAX, nzero, false);
    maxMinTest(nzero, -DBL_MAX, -DBL_MAX, false);
}

void
PUtilTest::maxMinTest(double a, double b, double exp, UBool max)
{
    double result = 0.0;

    if(max)
        result = uprv_fmax(a, b);
    else
        result = uprv_fmin(a, b);

    UBool nanResultOK = (uprv_isNaN(a) || uprv_isNaN(b));

    if(uprv_isNaN(result) && ! nanResultOK) {
        errln(UnicodeString("FAIL: got NaN as result without NaN as argument"));
        if(max)
            errln(UnicodeString("      max(") + a + ", " + b + ") is " + result + ", expected " + exp);
        else
            errln(UnicodeString("      min(") + a + ", " + b + ") is " + result + ", expected " + exp);
    }
    else if(result != exp && ! (uprv_isNaN(result) || uprv_isNaN(exp)))
        if(max)
            errln(UnicodeString("FAIL: max(") + a + ", " + b + ") is " + result + ", expected " + exp);
        else
            errln(UnicodeString("FAIL: min(") + a + ", " + b + ") is " + result + ", expected " + exp);
    else {
        if (verbose) {
            if(max)
                logln(UnicodeString("OK: max(") + a + ", " + b + ") is " + result);
            else
                logln(UnicodeString("OK: min(") + a + ", " + b + ") is " + result);
        }
    }
}
//==============================

// NaN is weird- comparisons with NaN _always_ return false, with the
// exception of !=, which _always_ returns true
void
PUtilTest::testNaN()
{
    logln("NaN tests may show that the expected NaN!=NaN etc. is not true on some");
    logln("platforms; however, ICU does not rely on them because it defines");
    logln("and uses uprv_isNaN(). Therefore, most failing NaN tests only report warnings.");

    PUtilTest::testIsNaN();
    PUtilTest::NaNGT();
    PUtilTest::NaNLT();
    PUtilTest::NaNGTE();
    PUtilTest::NaNLTE();
    PUtilTest::NaNE();
    PUtilTest::NaNNE();

    logln("End of NaN tests.");
}

//==============================

void 
PUtilTest::testPositiveInfinity()
{
    double  pinf    = uprv_getInfinity();
    double  ninf    = -uprv_getInfinity();
    double  ten     = 10.0;

    if(uprv_isInfinite(pinf) != true) {
        errln("FAIL: isInfinite(+Infinity) returned false, should be true.");
    }

    if(uprv_isPositiveInfinity(pinf) != true) {
        errln("FAIL: isPositiveInfinity(+Infinity) returned false, should be true.");
    }

    if(uprv_isNegativeInfinity(pinf) != false) {
        errln("FAIL: isNegativeInfinity(+Infinity) returned true, should be false.");
    }

    if((pinf > DBL_MAX) != true) {
        errln("FAIL: +Infinity > DBL_MAX returned false, should be true.");
    }

    if((pinf > DBL_MIN) != true) {
        errln("FAIL: +Infinity > DBL_MIN returned false, should be true.");
    }

    if((pinf > ninf) != true) {
        errln("FAIL: +Infinity > -Infinity returned false, should be true.");
    }

    if((pinf > ten) != true) {
        errln("FAIL: +Infinity > 10.0 returned false, should be true.");
    }
}

//==============================

void           
PUtilTest::testNegativeInfinity()
{
    double  pinf    = uprv_getInfinity();
    double  ninf    = -uprv_getInfinity();
    double  ten     = 10.0;

    if(uprv_isInfinite(ninf) != true) {
        errln("FAIL: isInfinite(-Infinity) returned false, should be true.");
    }

    if(uprv_isNegativeInfinity(ninf) != true) {
        errln("FAIL: isNegativeInfinity(-Infinity) returned false, should be true.");
    }

    if(uprv_isPositiveInfinity(ninf) != false) {
        errln("FAIL: isPositiveInfinity(-Infinity) returned true, should be false.");
    }

    if((ninf < DBL_MAX) != true) {
        errln("FAIL: -Infinity < DBL_MAX returned false, should be true.");
    }

    if((ninf < DBL_MIN) != true) {
        errln("FAIL: -Infinity < DBL_MIN returned false, should be true.");
    }

    if((ninf < pinf) != true) {
        errln("FAIL: -Infinity < +Infinity returned false, should be true.");
    }

    if((ninf < ten) != true) {
        errln("FAIL: -Infinity < 10.0 returned false, should be true.");
    }
}

//==============================

// notes about zero:
// -0.0 == 0.0 == true
// -0.0 <  0.0 == false
// generating -0.0 must be done at runtime.  compiler apparently ignores sign?
void           
PUtilTest::testZero()
{
    // volatile is used to fake out the compiler optimizer.  We really want to divide by 0.
    volatile double pzero   = 0.0;
    volatile double nzero   = 0.0;

    nzero = nzero * -1;

    if((pzero == nzero) != true) {
        errln("FAIL: 0.0 == -0.0 returned false, should be true.");
    }

    if((pzero > nzero) != false) {
        errln("FAIL: 0.0 > -0.0 returned true, should be false.");
    }

    if((pzero >= nzero) != true) {
        errln("FAIL: 0.0 >= -0.0 returned false, should be true.");
    }

    if((pzero < nzero) != false) {
        errln("FAIL: 0.0 < -0.0 returned true, should be false.");
    }

    if((pzero <= nzero) != true) {
        errln("FAIL: 0.0 <= -0.0 returned false, should be true.");
    }
#if U_PLATFORM != U_PF_OS400 /* OS/400 will generate divide by zero exception MCH1214 */
    if(uprv_isInfinite(1/pzero) != true) {
        errln("FAIL: isInfinite(1/0.0) returned false, should be true.");
    }

    if(uprv_isInfinite(1/nzero) != true) {
        errln("FAIL: isInfinite(1/-0.0) returned false, should be true.");
    }

    if(uprv_isPositiveInfinity(1/pzero) != true) {
        errln("FAIL: isPositiveInfinity(1/0.0) returned false, should be true.");
    }

    if(uprv_isNegativeInfinity(1/nzero) != true) {
        errln("FAIL: isNegativeInfinity(1/-0.0) returned false, should be true.");
    }
#endif
}

//==============================

void
PUtilTest::testIsNaN()
{
    double  pinf    = uprv_getInfinity();
    double  ninf    = -uprv_getInfinity();
    double  nan     = uprv_getNaN();
    double  ten     = 10.0;

    if(uprv_isNaN(nan) == false) {
        errln("FAIL: isNaN() returned false for NaN.");
    }

    if(uprv_isNaN(pinf) == true) {
        errln("FAIL: isNaN() returned true for +Infinity.");
    }

    if(uprv_isNaN(ninf) == true) {
        errln("FAIL: isNaN() returned true for -Infinity.");
    }

    if(uprv_isNaN(ten) == true) {
        errln("FAIL: isNaN() returned true for 10.0.");
    }
}

//==============================

void
PUtilTest::NaNGT()
{
    double  pinf    = uprv_getInfinity();
    double  ninf    = -uprv_getInfinity();
    double  nan     = uprv_getNaN();
    double  ten     = 10.0;

    if((nan > nan) != false) {
        logln("WARNING: NaN > NaN returned true, should be false");
    }

    if((nan > pinf) != false) {
        logln("WARNING: NaN > +Infinity returned true, should be false");
    }

    if((nan > ninf) != false) {
        logln("WARNING: NaN > -Infinity returned true, should be false");
    }

    if((nan > ten) != false) {
        logln("WARNING: NaN > 10.0 returned true, should be false");
    }
}

//==============================

void 
PUtilTest::NaNLT()
{
    double  pinf    = uprv_getInfinity();
    double  ninf    = -uprv_getInfinity();
    double  nan     = uprv_getNaN();
    double  ten     = 10.0;

    if((nan < nan) != false) {
        logln("WARNING: NaN < NaN returned true, should be false");
    }

    if((nan < pinf) != false) {
        logln("WARNING: NaN < +Infinity returned true, should be false");
    }

    if((nan < ninf) != false) {
        logln("WARNING: NaN < -Infinity returned true, should be false");
    }

    if((nan < ten) != false) {
        logln("WARNING: NaN < 10.0 returned true, should be false");
    }
}

//==============================

void                   
PUtilTest::NaNGTE()
{
    double  pinf    = uprv_getInfinity();
    double  ninf    = -uprv_getInfinity();
    double  nan     = uprv_getNaN();
    double  ten     = 10.0;

    if((nan >= nan) != false) {
        logln("WARNING: NaN >= NaN returned true, should be false");
    }

    if((nan >= pinf) != false) {
        logln("WARNING: NaN >= +Infinity returned true, should be false");
    }

    if((nan >= ninf) != false) {
        logln("WARNING: NaN >= -Infinity returned true, should be false");
    }

    if((nan >= ten) != false) {
        logln("WARNING: NaN >= 10.0 returned true, should be false");
    }
}

//==============================

void                   
PUtilTest::NaNLTE()
{
    double  pinf    = uprv_getInfinity();
    double  ninf    = -uprv_getInfinity();
    double  nan     = uprv_getNaN();
    double  ten     = 10.0;

    if((nan <= nan) != false) {
        logln("WARNING: NaN <= NaN returned true, should be false");
    }

    if((nan <= pinf) != false) {
        logln("WARNING: NaN <= +Infinity returned true, should be false");
    }

    if((nan <= ninf) != false) {
        logln("WARNING: NaN <= -Infinity returned true, should be false");
    }

    if((nan <= ten) != false) {
        logln("WARNING: NaN <= 10.0 returned true, should be false");
    }
}

//==============================

void                   
PUtilTest::NaNE()
{
    double  pinf    = uprv_getInfinity();
    double  ninf    = -uprv_getInfinity();
    double  nan     = uprv_getNaN();
    double  ten     = 10.0;

    if((nan == nan) != false) {
        logln("WARNING: NaN == NaN returned true, should be false");
    }

    if((nan == pinf) != false) {
        logln("WARNING: NaN == +Infinity returned true, should be false");
    }

    if((nan == ninf) != false) {
        logln("WARNING: NaN == -Infinity returned true, should be false");
    }

    if((nan == ten) != false) {
        logln("WARNING: NaN == 10.0 returned true, should be false");
    }
}

//==============================

void 
PUtilTest::NaNNE()
{
    double  pinf    = uprv_getInfinity();
    double  ninf    = -uprv_getInfinity();
    double  nan     = uprv_getNaN();
    double  ten     = 10.0;

    if((nan != nan) != true) {
        logln("WARNING: NaN != NaN returned false, should be true");
    }

    if((nan != pinf) != true) {
        logln("WARNING: NaN != +Infinity returned false, should be true");
    }

    if((nan != ninf) != true) {
        logln("WARNING: NaN != -Infinity returned false, should be true");
    }

    if((nan != ten) != true) {
        logln("WARNING: NaN != 10.0 returned false, should be true");
    }
}
