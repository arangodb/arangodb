// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1996-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

/* Test CalendarAstronomer for C++ */

#include "unicode/utypes.h"
#include "string.h"
#include "unicode/locid.h"

#if !UCONFIG_NO_FORMATTING

#include "astro.h"
#include "astrotst.h"
#include "cmemory.h"
#include "gregoimp.h" // for Math
#include "unicode/simpletz.h"


#define CASE(id,test) case id: name = #test; if (exec) { logln(#test "---"); logln((UnicodeString)""); test(); } break

AstroTest::AstroTest(): gc(nullptr) {
}

void AstroTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite AstroTest");
    switch (index) {
      // CASE(0,FooTest);
      CASE(0,TestSolarLongitude);
      CASE(1,TestLunarPosition);
      CASE(2,TestCoordinates);
      CASE(3,TestCoverage);
      CASE(4,TestBasics);
      CASE(5,TestMoonAge);
    default: name = ""; break;
    }
}

#undef CASE

#define ASSERT_OK(x) UPRV_BLOCK_MACRO_BEGIN { \
    if(U_FAILURE(x)) { \
        dataerrln("%s:%d: %s\n", __FILE__, __LINE__, u_errorName(x)); \
        return; \
    } \
} UPRV_BLOCK_MACRO_END


void AstroTest::init(UErrorCode &status) {
  if(U_FAILURE(status)) return;

  if(gc != nullptr) {
    dataerrln("Err: init() called twice!");
    close(status);
    if(U_SUCCESS(status)) {
      status = U_INTERNAL_PROGRAM_ERROR;
    }
  }

  if(U_FAILURE(status)) return;

  gc = Calendar::createInstance(TimeZone::getGMT()->clone(), status);
}

void AstroTest::close(UErrorCode &/*status*/) {
  if(gc != nullptr) {
    delete gc;
    gc = nullptr;
  }
}

void AstroTest::TestSolarLongitude() {
  UErrorCode status = U_ZERO_ERROR;
  init(status);
  ASSERT_OK(status);

  struct {
    int32_t d[5]; double f ;
  } tests[] = {
    { { 1980, 7, 27, 0, 00 },  124.114347 },
    { { 1988, 7, 27, 00, 00 },  124.187732 }
  };

  logln("");
  for (uint32_t i = 0; i < UPRV_LENGTHOF(tests); i++) {
    gc->clear();
    gc->set(tests[i].d[0], tests[i].d[1]-1, tests[i].d[2], tests[i].d[3], tests[i].d[4]);

    CalendarAstronomer astro(gc->getTime(status));

    astro.getSunLongitude();
  }
  close(status);
  ASSERT_OK(status);
}



void AstroTest::TestLunarPosition() {
  UErrorCode status = U_ZERO_ERROR;
  init(status);
  ASSERT_OK(status);

  static const double tests[][7] = {
    { 1979, 2, 26, 16, 00,  0, 0 }
  };
  logln("");

  for (int32_t i = 0; i < UPRV_LENGTHOF(tests); i++) {
    gc->clear();
    gc->set((int32_t)tests[i][0], (int32_t)tests[i][1]-1, (int32_t)tests[i][2], (int32_t)tests[i][3], (int32_t)tests[i][4]);
    CalendarAstronomer astro(gc->getTime(status));

    const CalendarAstronomer::Equatorial& result = astro.getMoonPosition();
    logln((UnicodeString)"Moon position is " + result.toString() + (UnicodeString)";  " /* + result->toHmsString()*/);
  }

  close(status);
  ASSERT_OK(status);
}



void AstroTest::TestCoordinates() {
  UErrorCode status = U_ZERO_ERROR;
  init(status);
  ASSERT_OK(status);

  CalendarAstronomer::Equatorial result;
  CalendarAstronomer astro;
  astro.eclipticToEquatorial(result, 139.686111 * CalendarAstronomer::PI / 180.0, 4.875278* CalendarAstronomer::PI / 180.0);
  logln((UnicodeString)"result is " + result.toString() + (UnicodeString)";  " /* + result.toHmsString()*/ );
  close(status);
  ASSERT_OK(status);
}



void AstroTest::TestCoverage() {
  UErrorCode status = U_ZERO_ERROR;
  init(status);
  ASSERT_OK(status);
  GregorianCalendar *cal = new GregorianCalendar(1958, UCAL_AUGUST, 15,status);
  UDate then = cal->getTime(status);
  CalendarAstronomer *myastro = new CalendarAstronomer(then);
  ASSERT_OK(status);

  //Latitude:  34 degrees 05' North
  //Longitude:  118 degrees 22' West
  double laLat = 34 + 5./60, laLong = 360 - (118 + 22./60);

  double eclLat = laLat * CalendarAstronomer::PI / 360;
  double eclLong = laLong * CalendarAstronomer::PI / 360;

  CalendarAstronomer::Equatorial eq;

  CalendarAstronomer *astronomers[] = {
    myastro, myastro, myastro // check cache
  };

  for (uint32_t i = 0; i < UPRV_LENGTHOF(astronomers); ++i) {
    CalendarAstronomer *anAstro = astronomers[i];

    //logln("astro: " + astro);
    logln((UnicodeString)"   date: " + anAstro->getTime());
    logln((UnicodeString)"   equ ecl: " + (anAstro->eclipticToEquatorial(eq,eclLat,eclLong)).toString());
  }

  delete myastro;
  delete cal;

  close(status);
  ASSERT_OK(status);
}

void AstroTest::TestBasics() {
  UErrorCode status = U_ZERO_ERROR;
  init(status);
  if (U_FAILURE(status)) {
    dataerrln("Got error: %s", u_errorName(status));
    return;
  }

  // Check that our JD computation is the same as the book's (p. 88)
  GregorianCalendar cal3(TimeZone::getGMT()->clone(), Locale::getUS(), status);
  LocalPointer<DateFormat> d3(DateFormat::createDateTimeInstance(DateFormat::MEDIUM,DateFormat::MEDIUM,Locale::getUS()));
  if (d3.isNull()) {
      dataerrln("Got error: %s", u_errorName(status));
      close(status);
      return;
  }
  d3->setTimeZone(*TimeZone::getGMT());
  cal3.clear();
  cal3.set(UCAL_YEAR, 1980);
  cal3.set(UCAL_MONTH, UCAL_JULY);
  cal3.set(UCAL_DATE, 2);
  logln("cal3[a]=%.1lf, d=%d\n", cal3.getTime(status), cal3.get(UCAL_JULIAN_DAY,status));
  {
    UnicodeString s;
    logln(UnicodeString("cal3[a] = ") + d3->format(cal3.getTime(status),s));
  }
  cal3.clear();
  cal3.set(UCAL_YEAR, 1980);
  cal3.set(UCAL_MONTH, UCAL_JULY);
  cal3.set(UCAL_DATE, 27);
  logln("cal3=%.1lf, d=%d\n", cal3.getTime(status), cal3.get(UCAL_JULIAN_DAY,status));

  ASSERT_OK(status);
  {
    UnicodeString s;
    logln(UnicodeString("cal3 = ") + d3->format(cal3.getTime(status),s));
  }
  CalendarAstronomer astro(cal3.getTime(status));
  double jd = astro.getJulianDay() - 2447891.5;
  double exp = -3444.;
  if (jd == exp) {
    UnicodeString s;
    logln(d3->format(cal3.getTime(status),s) + " => " + jd);
  } else {
    UnicodeString s;
    errln("FAIL: " + d3->format(cal3.getTime(status), s) + " => " + jd +
          ", expected " + exp);
  }

  //        cal3.clear();
  //        cal3.set(cal3.YEAR, 1990);
  //        cal3.set(cal3.MONTH, Calendar.JANUARY);
  //        cal3.set(cal3.DATE, 1);
  //        cal3.add(cal3.DATE, -1);
  //        astro.setDate(cal3.getTime());
  //        astro.foo();

  ASSERT_OK(status);
  close(status);
  ASSERT_OK(status);

}

void AstroTest::TestMoonAge(){
	UErrorCode status = U_ZERO_ERROR;
	init(status);
	ASSERT_OK(status);
	
	// more testcases are around the date 05/20/2012
	//ticket#3785  UDate ud0 = 1337557623000.0;
	static const double testcase[][10] = {{2012, 5, 20 , 16 , 48, 59},
	                {2012, 5, 20 , 16 , 47, 34},
	                {2012, 5, 21, 00, 00, 00},
	                {2012, 5, 20, 14, 55, 59},
	                {2012, 5, 21, 7, 40, 40},
	                {2023, 9, 25, 10,00, 00},
	                {2008, 7, 7, 15, 00, 33}, 
	                {1832, 9, 24, 2, 33, 41 },
	                {2016, 1, 31, 23, 59, 59},
	                {2099, 5, 20, 14, 55, 59}
	        };
	// Moon phase angle - Got from http://www.moonsystem.to/checkupe.htm
	static const double angle[] = {356.8493418421329, 356.8386760059673, 0.09625415252237701, 355.9986960782416, 3.5714026601303317, 124.26906744384183, 59.80247650195558,
									357.54163205513123, 268.41779281511094, 4.82340276581624};
	static const double precision = CalendarAstronomer::PI/32;
	for (int32_t i = 0; i < UPRV_LENGTHOF(testcase); i++) {
		gc->clear();
		logln((UnicodeString)"CASE["+i+"]: Year "+(int32_t)testcase[i][0]+" Month "+(int32_t)testcase[i][1]+" Day "+
		                                    (int32_t)testcase[i][2]+" Hour "+(int32_t)testcase[i][3]+" Minutes "+(int32_t)testcase[i][4]+
		                                    " Seconds "+(int32_t)testcase[i][5]);
		gc->set((int32_t)testcase[i][0], (int32_t)testcase[i][1]-1, (int32_t)testcase[i][2], (int32_t)testcase[i][3], (int32_t)testcase[i][4], (int32_t)testcase[i][5]);
                CalendarAstronomer astro(gc->getTime(status));
		double expectedAge = (angle[i]*CalendarAstronomer::PI)/180;
		double got = astro.getMoonAge();
		//logln(testString);
		if(!(got>expectedAge-precision && got<expectedAge+precision)){
			errln((UnicodeString)"FAIL: expected " + expectedAge +
					" got " + got);
		}else{
			logln((UnicodeString)"PASS: expected " + expectedAge +
					" got " + got);
		}
	}
	close(status);
	ASSERT_OK(status);
}


// TODO: try finding next new moon after  07/28/1984 16:00 GMT


#endif



