// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/***********************************************************************
 * COPYRIGHT: 
 * Copyright (c) 1997-2014, International Business Machines Corporation
 * and others. All Rights Reserved.
 ***********************************************************************/

/* Test Internationalized Calendars for C++ */

#include "unicode/utypes.h"
#include "string.h"
#include "unicode/locid.h"
#include "japancal.h"
#include "unicode/localpointer.h"
#include "unicode/datefmt.h"
#include "unicode/smpdtfmt.h"
#include "unicode/dtptngen.h"

#if !UCONFIG_NO_FORMATTING

#include <stdio.h>
#include "caltest.h"

#define CHECK(status, msg) \
    if (U_FAILURE(status)) { \
      dataerrln((UnicodeString(u_errorName(status)) + UnicodeString(" : " ) )+ msg); \
        return; \
    }


static UnicodeString escape( const UnicodeString&src)
{
  UnicodeString dst;
    dst.remove();
    for (int32_t i = 0; i < src.length(); ++i) {
        UChar c = src[i];
        if(c < 0x0080) 
            dst += c;
        else {
            dst += UnicodeString("[");
            char buf [8];
            sprintf(buf, "%#x", c);
            dst += UnicodeString(buf);
            dst += UnicodeString("]");
        }
    }

    return dst;
}


#include "incaltst.h"
#include "unicode/gregocal.h"
#include "unicode/smpdtfmt.h"
#include "unicode/simpletz.h"
 
// *****************************************************************************
// class IntlCalendarTest
// *****************************************************************************
//--- move to CalendarTest?

// Turn this on to dump the calendar fields 
#define U_DEBUG_DUMPCALS  


#define CASE(id,test) case id: name = #test; if (exec) { logln(#test "---"); logln((UnicodeString)""); test(); } break


void IntlCalendarTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ )
{
    if (exec) logln("TestSuite IntlCalendarTest");
    switch (index) {
    CASE(0,TestTypes);
    CASE(1,TestGregorian);
    CASE(2,TestBuddhist);
    CASE(3,TestJapanese);
    CASE(4,TestBuddhistFormat);
    CASE(5,TestJapaneseFormat);
    CASE(6,TestJapanese3860);
    CASE(7,TestForceGannenNumbering);
    CASE(8,TestPersian);
    CASE(9,TestPersianFormat);
    CASE(10,TestTaiwan);
    default: name = ""; break;
    }
}

#undef CASE

// ---------------------------------------------------------------------------------


/**
 * Test various API methods for API completeness.
 */
void
IntlCalendarTest::TestTypes()
{
  Calendar *c = NULL;
  UErrorCode status = U_ZERO_ERROR;
  int j;
  const char *locs [40] = { "en_US_VALLEYGIRL",     
                            "en_US_VALLEYGIRL@collation=phonebook;calendar=japanese",
                            "en_US_VALLEYGIRL@collation=phonebook;calendar=gregorian",
                            "ja_JP@calendar=japanese",   
                            "th_TH@calendar=buddhist", 
                            "th_TH_TRADITIONAL", 
                            "th_TH_TRADITIONAL@calendar=gregorian", 
                            "en_US",
                            "th_TH",    // Default calendar for th_TH is buddhist
                            "th",       // th's default region is TH and buddhist is used as default for TH
                            "en_TH",    // Default calendar for any locales with region TH is buddhist
                            "en-TH-u-ca-gregory",
                            NULL };
  const char *types[40] = { "gregorian", 
                            "japanese",
                            "gregorian",
                            "japanese",
                            "buddhist",
                            "buddhist",           
                            "gregorian",
                            "gregorian",
                            "buddhist",           
                            "buddhist",           
                            "buddhist",           
                            "gregorian",
                            NULL };

  for(j=0;locs[j];j++) {
    logln(UnicodeString("Creating calendar of locale ")  + locs[j]);
    status = U_ZERO_ERROR;
    c = Calendar::createInstance(locs[j], status);
    CHECK(status, "creating '" + UnicodeString(locs[j]) + "' calendar");
    if(U_SUCCESS(status)) {
      logln(UnicodeString(" type is ") + c->getType());
      if(strcmp(c->getType(), types[j])) {
        dataerrln(UnicodeString(locs[j]) + UnicodeString("Calendar type ") + c->getType() + " instead of " + types[j]);
      }
    }
    delete c;
  }
}



/**
 * Run a test of a quasi-Gregorian calendar.  This is a calendar
 * that behaves like a Gregorian but has different year/era mappings.
 * The int[] data array should have the format:
 * 
 * { era, year, gregorianYear, month, dayOfMonth, ...  ... , -1 }
 */
void IntlCalendarTest::quasiGregorianTest(Calendar& cal, const Locale& gcl, const int32_t *data) {
  UErrorCode status = U_ZERO_ERROR;
  // As of JDK 1.4.1_01, using the Sun JDK GregorianCalendar as
  // a reference throws us off by one hour.  This is most likely
  // due to the JDK 1.4 incorporation of historical time zones.
  //java.util.Calendar grego = java.util.Calendar.getInstance();
  Calendar *grego = Calendar::createInstance(gcl, status);
  if (U_FAILURE(status)) {
    dataerrln("Error calling Calendar::createInstance"); 
    return;
  }

  int32_t tz1 = cal.get(UCAL_ZONE_OFFSET,status);
  int32_t tz2 = grego -> get (UCAL_ZONE_OFFSET, status);
  if(tz1 != tz2) { 
    errln((UnicodeString)"cal's tz " + tz1 + " != grego's tz " + tz2);
  }

  for (int32_t i=0; data[i]!=-1; ) {
    int32_t era = data[i++];
    int32_t year = data[i++];
    int32_t gregorianYear = data[i++];
    int32_t month = data[i++];
    int32_t dayOfMonth = data[i++];
    
    grego->clear();
    grego->set(gregorianYear, month, dayOfMonth);
    UDate D = grego->getTime(status);
    
    cal.clear();
    cal.set(UCAL_ERA, era);
    cal.set(year, month, dayOfMonth);
    UDate d = cal.getTime(status);
#ifdef U_DEBUG_DUMPCALS
    logln((UnicodeString)"cal  : " + CalendarTest::calToStr(cal));
    logln((UnicodeString)"grego: " + CalendarTest::calToStr(*grego));
#endif
    if (d == D) {
      logln(UnicodeString("OK: ") + era + ":" + year + "/" + (month+1) + "/" + dayOfMonth +
            " => " + d + " (" + UnicodeString(cal.getType()) + ")");
    } else {
      errln(UnicodeString("Fail: (fields to millis)") + era + ":" + year + "/" + (month+1) + "/" + dayOfMonth +
            " => " + d + ", expected " + D + " (" + UnicodeString(cal.getType()) + "Off by: " + (d-D));
    }
    
    // Now, set the gregorian millis on the other calendar
    cal.clear();
    cal.setTime(D, status);
    int e = cal.get(UCAL_ERA, status);
    int y = cal.get(UCAL_YEAR, status);
#ifdef U_DEBUG_DUMPCALS
    logln((UnicodeString)"cal  : " + CalendarTest::calToStr(cal));
    logln((UnicodeString)"grego: " + CalendarTest::calToStr(*grego));
#endif
    if (y == year && e == era) {
      logln((UnicodeString)"OK: " + D + " => " + cal.get(UCAL_ERA, status) + ":" +
            cal.get(UCAL_YEAR, status) + "/" +
            (cal.get(UCAL_MONTH, status)+1) + "/" + cal.get(UCAL_DATE, status) +  " (" + UnicodeString(cal.getType()) + ")");
    } else {
      errln((UnicodeString)"Fail: (millis to fields)" + D + " => " + cal.get(UCAL_ERA, status) + ":" +
            cal.get(UCAL_YEAR, status) + "/" +
            (cal.get(UCAL_MONTH, status)+1) + "/" + cal.get(UCAL_DATE, status) +
            ", expected " + era + ":" + year + "/" + (month+1) + "/" +
            dayOfMonth +  " (" + UnicodeString(cal.getType()));
    }
  }
  delete grego;
  CHECK(status, "err during quasiGregorianTest()");
}

// Verify that Gregorian works like Gregorian
void IntlCalendarTest::TestGregorian() { 
    UDate timeA = Calendar::getNow();
    int32_t data[] = { 
        GregorianCalendar::AD, 1868, 1868, UCAL_SEPTEMBER, 8,
        GregorianCalendar::AD, 1868, 1868, UCAL_SEPTEMBER, 9,
        GregorianCalendar::AD, 1869, 1869, UCAL_JUNE, 4,
        GregorianCalendar::AD, 1912, 1912, UCAL_JULY, 29,
        GregorianCalendar::AD, 1912, 1912, UCAL_JULY, 30,
        GregorianCalendar::AD, 1912, 1912, UCAL_AUGUST, 1,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    
    Calendar *cal;
    UErrorCode status = U_ZERO_ERROR;
    cal = Calendar::createInstance(/*"de_DE", */ status);
    CHECK(status, UnicodeString("Creating de_CH calendar"));
    // Sanity check the calendar 
    UDate timeB = Calendar::getNow();
    UDate timeCal = cal->getTime(status);

    if(!(timeA <= timeCal) || !(timeCal <= timeB)) {
      errln((UnicodeString)"Error: Calendar time " + timeCal +
            " is not within sampled times [" + timeA + " to " + timeB + "]!");
    }
    // end sanity check

    // Note, the following is a good way to test the sanity of the constructed calendars,
    // using Collation as a delay-loop: 
    //
    // $ intltest  format/IntlCalendarTest  collate/G7CollationTest format/IntlCalendarTest

    quasiGregorianTest(*cal,Locale("fr_FR"),data);
    delete cal;
}

/**
 * Verify that BuddhistCalendar shifts years to Buddhist Era but otherwise
 * behaves like GregorianCalendar.
 */
void IntlCalendarTest::TestBuddhist() {
    // BE 2542 == 1999 CE
    UDate timeA = Calendar::getNow();

    int32_t data[] = {
        0,           // B. era   [928479600000]
        2542,        // B. year
        1999,        // G. year
        UCAL_JUNE,   // month
        4,           // day

        0,           // B. era   [-79204842000000]
        3,           // B. year
        -540,        // G. year
        UCAL_FEBRUARY, // month
        12,          // day

        0,           // test month calculation:  4795 BE = 4252 AD is a leap year, but 4795 AD is not.
        4795,        // BE [72018057600000]
        4252,        // AD
        UCAL_FEBRUARY,
        29,

        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    Calendar *cal;
    UErrorCode status = U_ZERO_ERROR;
    cal = Calendar::createInstance("th_TH@calendar=buddhist", status);
    CHECK(status, UnicodeString("Creating th_TH@calendar=buddhist calendar"));

    // Sanity check the calendar 
    UDate timeB = Calendar::getNow();
    UDate timeCal = cal->getTime(status);

    if(!(timeA <= timeCal) || !(timeCal <= timeB)) {
      errln((UnicodeString)"Error: Calendar time " + timeCal +
            " is not within sampled times [" + timeA + " to " + timeB + "]!");
    }
    // end sanity check


    quasiGregorianTest(*cal,Locale("th_TH@calendar=gregorian"),data);
    delete cal;
}


/**
 * Verify that TaiWanCalendar shifts years to Minguo Era but otherwise
 * behaves like GregorianCalendar.
 */
void IntlCalendarTest::TestTaiwan() {
    // MG 1 == 1912 AD
    UDate timeA = Calendar::getNow();
    
    // TODO port these to the data items
    int32_t data[] = {
        1,           // B. era   [928479600000]
        1,        // B. year
        1912,        // G. year
        UCAL_JUNE,   // month
        4,           // day

        1,           // B. era   [-79204842000000]
        3,           // B. year
        1914,        // G. year
        UCAL_FEBRUARY, // month
        12,          // day

        1,           // B. era   [-79204842000000]
        96,           // B. year
        2007,        // G. year
        UCAL_FEBRUARY, // month
        12,          // day

        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    Calendar *cal;
    UErrorCode status = U_ZERO_ERROR;
    cal = Calendar::createInstance("en_US@calendar=roc", status);
    CHECK(status, UnicodeString("Creating en_US@calendar=roc calendar"));

    // Sanity check the calendar 
    UDate timeB = Calendar::getNow();
    UDate timeCal = cal->getTime(status);

    if(!(timeA <= timeCal) || !(timeCal <= timeB)) {
      errln((UnicodeString)"Error: Calendar time " + timeCal +
            " is not within sampled times [" + timeA + " to " + timeB + "]!");
    }
    // end sanity check


    quasiGregorianTest(*cal,Locale("en_US"),data);
    delete cal;
}



/**
 * Verify that JapaneseCalendar shifts years to Japanese Eras but otherwise
 * behaves like GregorianCalendar.
 */
void IntlCalendarTest::TestJapanese() {
    UDate timeA = Calendar::getNow();
    
    /* Sorry.. japancal.h is private! */
#define JapaneseCalendar_MEIJI  232
#define JapaneseCalendar_TAISHO 233
#define JapaneseCalendar_SHOWA  234
#define JapaneseCalendar_HEISEI 235
    
    // BE 2542 == 1999 CE
    int32_t data[] = { 
        //       Jera         Jyr  Gyear   m             d
        JapaneseCalendar_MEIJI, 1, 1868, UCAL_SEPTEMBER, 8,
        JapaneseCalendar_MEIJI, 1, 1868, UCAL_SEPTEMBER, 9,
        JapaneseCalendar_MEIJI, 2, 1869, UCAL_JUNE, 4,
        JapaneseCalendar_MEIJI, 45, 1912, UCAL_JULY, 29,
        JapaneseCalendar_TAISHO, 1, 1912, UCAL_JULY, 30,
        JapaneseCalendar_TAISHO, 1, 1912, UCAL_AUGUST, 1,
        
        // new tests (not in java)
        JapaneseCalendar_SHOWA,     64,   1989,  UCAL_JANUARY, 7,  // Test current era transition (different code path than others)
        JapaneseCalendar_HEISEI,    1,   1989,  UCAL_JANUARY, 8,   
        JapaneseCalendar_HEISEI,    1,   1989,  UCAL_JANUARY, 9,
        JapaneseCalendar_HEISEI,    1,   1989,  UCAL_DECEMBER, 20,
        JapaneseCalendar_HEISEI,  15,  2003,  UCAL_MAY, 22,
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };
    
    Calendar *cal;
    UErrorCode status = U_ZERO_ERROR;
    cal = Calendar::createInstance("ja_JP@calendar=japanese", status);
    CHECK(status, UnicodeString("Creating ja_JP@calendar=japanese calendar"));
    // Sanity check the calendar 
    UDate timeB = Calendar::getNow();
    UDate timeCal = cal->getTime(status);

    if(!(timeA <= timeCal) || !(timeCal <= timeB)) {
      errln((UnicodeString)"Error: Calendar time " + timeCal +
            " is not within sampled times [" + timeA + " to " + timeB + "]!");
    }
    // end sanity check
    quasiGregorianTest(*cal,Locale("ja_JP"),data);
    delete cal;
}



void IntlCalendarTest::TestBuddhistFormat() {
    UErrorCode status = U_ZERO_ERROR;
    
    // Test simple parse/format with adopt
    
    // First, a contrived English test..
    UDate aDate = 999932400000.0; 
    SimpleDateFormat *fmt = new SimpleDateFormat(UnicodeString("MMMM d, yyyy G"), Locale("en_US@calendar=buddhist"), status);
    CHECK(status, "creating date format instance");
    SimpleDateFormat *fmt2 = new SimpleDateFormat(UnicodeString("MMMM d, yyyy G"), Locale("en_US@calendar=gregorian"), status);
    CHECK(status, "creating gregorian date format instance");
    if(!fmt) { 
        errln("Couldn't create en_US instance");
    } else {
        UnicodeString str;
        fmt2->format(aDate, str);
        logln(UnicodeString() + "Test Date: " + str);
        str.remove();
        fmt->format(aDate, str);
        logln(UnicodeString() + "as Buddhist Calendar: " + escape(str));
        UnicodeString expected("September 8, 2544 BE");
        if(str != expected) {
            errln("Expected " + escape(expected) + " but got " + escape(str));
        }
        UDate otherDate = fmt->parse(expected, status);
        if(otherDate != aDate) { 
            UnicodeString str3;
            fmt->format(otherDate, str3);
            errln("Parse incorrect of " + escape(expected) + " - wanted " + aDate + " but got " +  otherDate + ", " + escape(str3));
        } else {
            logln("Parsed OK: " + expected);
        }
        delete fmt;
    }
    delete fmt2;
    
    CHECK(status, "Error occurred testing Buddhist Calendar in English ");
    
    status = U_ZERO_ERROR;
    // Now, try in Thai
    {
        UnicodeString expect = CharsToUnicodeString("\\u0E27\\u0E31\\u0E19\\u0E40\\u0E2A\\u0E32\\u0E23\\u0E4C\\u0E17\\u0E35\\u0E48"
            " 8 \\u0E01\\u0E31\\u0e19\\u0e22\\u0e32\\u0e22\\u0e19 \\u0e1e.\\u0e28. 2544");
        UDate         expectDate = 999932400000.0;
        Locale        loc("th_TH_TRADITIONAL"); // legacy
        
        simpleTest(loc, expect, expectDate, status);
    }
    status = U_ZERO_ERROR;
    {
        UnicodeString expect = CharsToUnicodeString("\\u0E27\\u0E31\\u0E19\\u0E40\\u0E2A\\u0E32\\u0E23\\u0E4C\\u0E17\\u0E35\\u0E48"
            " 8 \\u0E01\\u0E31\\u0e19\\u0e22\\u0e32\\u0e22\\u0e19 \\u0e1e.\\u0e28. 2544");
        UDate         expectDate = 999932400000.0;
        Locale        loc("th_TH@calendar=buddhist");
        
        simpleTest(loc, expect, expectDate, status);
    }
    status = U_ZERO_ERROR;
    {
        UnicodeString expect = CharsToUnicodeString("\\u0E27\\u0E31\\u0E19\\u0E40\\u0E2A\\u0E32\\u0E23\\u0E4C\\u0E17\\u0E35\\u0E48"
            " 8 \\u0E01\\u0E31\\u0e19\\u0e22\\u0e32\\u0e22\\u0e19 \\u0e04.\\u0e28. 2001");
        UDate         expectDate = 999932400000.0;
        Locale        loc("th_TH@calendar=gregorian");
        
        simpleTest(loc, expect, expectDate, status);
    }
    status = U_ZERO_ERROR;
    {
        UnicodeString expect = CharsToUnicodeString("\\u0E27\\u0E31\\u0E19\\u0E40\\u0E2A\\u0E32\\u0E23\\u0E4C\\u0E17\\u0E35\\u0E48"
            " 8 \\u0E01\\u0E31\\u0e19\\u0e22\\u0e32\\u0e22\\u0e19 \\u0e04.\\u0e28. 2001");
        UDate         expectDate = 999932400000.0;
        Locale        loc("th_TH_TRADITIONAL@calendar=gregorian");
        
        simpleTest(loc, expect, expectDate, status);
    }
}

// TaiwanFormat has been moved to testdata/format.txt


void IntlCalendarTest::TestJapaneseFormat() {
    Calendar *cal;
    UErrorCode status = U_ZERO_ERROR;
    cal = Calendar::createInstance("ja_JP@calendar=japanese", status);
    CHECK(status, UnicodeString("Creating ja_JP@calendar=japanese calendar"));
    
    Calendar *cal2 = cal->clone();
    delete cal;
    cal = NULL;
    
    // Test simple parse/format with adopt
    
    UDate aDate = 999932400000.0; 
    SimpleDateFormat *fmt = new SimpleDateFormat(UnicodeString("MMMM d, yy G"), Locale("en_US@calendar=japanese"), status);
    SimpleDateFormat *fmt2 = new SimpleDateFormat(UnicodeString("MMMM d, yyyy G"), Locale("en_US@calendar=gregorian"), status);
    CHECK(status, "creating date format instance");
    if(!fmt) { 
        errln("Couldn't create en_US instance");
    } else {
        UnicodeString str;
        fmt2->format(aDate, str);
        logln(UnicodeString() + "Test Date: " + str);
        str.remove();
        fmt->format(aDate, str);
        logln(UnicodeString() + "as Japanese Calendar: " + str);
        UnicodeString expected("September 8, 13 Heisei");
        if(str != expected) {
            errln("Expected " + expected + " but got " + str);
        }
        UDate otherDate = fmt->parse(expected, status);
        if(otherDate != aDate) { 
            UnicodeString str3;
            ParsePosition pp;
            fmt->parse(expected, *cal2, pp);
            fmt->format(otherDate, str3);
            errln("Parse incorrect of " + expected + " - wanted " + aDate + " but got " +  " = " +   otherDate + ", " + str3 + " = " + CalendarTest::calToStr(*cal2) );
            
        } else {
            logln("Parsed OK: " + expected);
        }
        delete fmt;
    }

    // Test parse with incomplete information
    fmt = new SimpleDateFormat(UnicodeString("G y"), Locale("en_US@calendar=japanese"), status);
    aDate = -3197117222000.0;
    CHECK(status, "creating date format instance");
    if(!fmt) { 
        errln("Coudln't create en_US instance");
    } else {
        UnicodeString str;
        fmt2->format(aDate, str);
        logln(UnicodeString() + "Test Date: " + str);
        str.remove();
        fmt->format(aDate, str);
        logln(UnicodeString() + "as Japanese Calendar: " + str);
        UnicodeString expected("Meiji 1");
        if(str != expected) {
            errln("Expected " + expected + " but got " + str);
        }
        UDate otherDate = fmt->parse(expected, status);
        if(otherDate != aDate) { 
            UnicodeString str3;
            ParsePosition pp;
            fmt->parse(expected, *cal2, pp);
            fmt->format(otherDate, str3);
            errln("Parse incorrect of " + expected + " - wanted " + aDate + " but got " +  " = " +
                otherDate + ", " + str3 + " = " + CalendarTest::calToStr(*cal2) );
        } else {
            logln("Parsed OK: " + expected);
        }
        delete fmt;
    }

    delete cal2;
    delete fmt2;
    CHECK(status, "Error occurred");
    
    // Now, try in Japanese
    {
        UnicodeString expect = CharsToUnicodeString("\\u5e73\\u621013\\u5e749\\u67088\\u65e5\\u571f\\u66dc\\u65e5");
        UDate         expectDate = 999932400000.0; // Testing a recent date
        Locale        loc("ja_JP@calendar=japanese");
        
        status = U_ZERO_ERROR;
        simpleTest(loc, expect, expectDate, status);
    }
    {
        UnicodeString expect = CharsToUnicodeString("\\u5e73\\u621013\\u5e749\\u67088\\u65e5\\u571f\\u66dc\\u65e5");
        UDate         expectDate = 999932400000.0; // Testing a recent date
        Locale        loc("ja_JP@calendar=japanese");
        
        status = U_ZERO_ERROR;
        simpleTest(loc, expect, expectDate, status);
    }
    {
        UnicodeString expect = CharsToUnicodeString("\\u5b89\\u6c385\\u5e747\\u67084\\u65e5\\u6728\\u66dc\\u65e5");
        UDate         expectDate = -6106032422000.0; // 1776-07-04T00:00:00Z-075258
        Locale        loc("ja_JP@calendar=japanese");
        
        status = U_ZERO_ERROR;
        simpleTest(loc, expect, expectDate, status);    
        
    }
    {   // Jitterbug 1869 - this is an ambiguous era. (Showa 64 = Jan 6 1989, but Showa could be 2 other eras) )
        UnicodeString expect = CharsToUnicodeString("\\u662d\\u548c64\\u5e741\\u67086\\u65e5\\u91d1\\u66dc\\u65e5");
        UDate         expectDate = 600076800000.0;
        Locale        loc("ja_JP@calendar=japanese");
        
        status = U_ZERO_ERROR;
        simpleTest(loc, expect, expectDate, status);    
        
    }
    {   // 1989 Jan 9 Monday = Heisei 1; full is Gy年M月d日EEEE => 平成元年1月9日月曜日
        UnicodeString expect = CharsToUnicodeString("\\u5E73\\u6210\\u5143\\u5E741\\u67089\\u65E5\\u6708\\u66DC\\u65E5");
        UDate         expectDate = 600336000000.0;
        Locale        loc("ja_JP@calendar=japanese");
        
        status = U_ZERO_ERROR;
        simpleTest(loc, expect, expectDate, status);    
        
    }
    {   // This Feb 29th falls on a leap year by gregorian year, but not by Japanese year.
        UnicodeString expect = CharsToUnicodeString("\\u5EB7\\u6B632\\u5e742\\u670829\\u65e5\\u65e5\\u66dc\\u65e5");
        UDate         expectDate =  -16214400422000.0;  // 1456-03-09T00:00Z-075258
        Locale        loc("ja_JP@calendar=japanese");
        
        status = U_ZERO_ERROR;
        simpleTest(loc, expect, expectDate, status);    
        
    }
}

void IntlCalendarTest::TestJapanese3860()
{
    Calendar *cal;
    UErrorCode status = U_ZERO_ERROR;
    cal = Calendar::createInstance("ja_JP@calendar=japanese", status);
    CHECK(status, UnicodeString("Creating ja_JP@calendar=japanese calendar"));
    Calendar *cal2 = cal->clone();
    SimpleDateFormat *fmt2 = new SimpleDateFormat(UnicodeString("HH:mm:ss.S MMMM d, yyyy G"), Locale("en_US@calendar=gregorian"), status);
    UnicodeString str;

    
    {
        // Test simple parse/format with adopt
        UDate aDate = 0; 
        
        // Test parse with missing era (should default to current era, heisei)
        // Test parse with incomplete information
        logln("Testing parse w/ missing era...");
        SimpleDateFormat *fmt = new SimpleDateFormat(UnicodeString("y/M/d"), Locale("ja_JP@calendar=japanese"), status);
        CHECK(status, "creating date format instance");
        if(!fmt) { 
            errln("Couldn't create en_US instance");
        } else {
            UErrorCode s2 = U_ZERO_ERROR;
            cal2->clear();
            UnicodeString samplestr("1/5/9");
            logln(UnicodeString() + "Test Year: " + samplestr);
            aDate = fmt->parse(samplestr, s2);
            ParsePosition pp=0;
            fmt->parse(samplestr, *cal2, pp);
            CHECK(s2, "parsing the 1/5/9 string");
            logln("*cal2 after 159 parse:");
            str.remove();
            fmt2->format(aDate, str);
            logln(UnicodeString() + "as Gregorian Calendar: " + str);

            cal2->setTime(aDate, s2);
            int32_t gotYear = cal2->get(UCAL_YEAR, s2);
            int32_t gotEra = cal2->get(UCAL_ERA, s2);
            int32_t expectYear = 1;
            int32_t expectEra = JapaneseCalendar::getCurrentEra();
            if((gotYear!=1) || (gotEra != expectEra)) {
                errln(UnicodeString("parse "+samplestr+" of 'y/M/d' as Japanese Calendar, expected year ") + expectYear + 
                    UnicodeString(" and era ") + expectEra +", but got year " + gotYear + " and era " + gotEra + " (Gregorian:" + str +")");
            } else {            
                logln(UnicodeString() + " year: " + gotYear + ", era: " + gotEra);
            }
            delete fmt;
        }
    }
    
    {
        // Test simple parse/format with adopt
        UDate aDate = 0; 
        
        // Test parse with missing era (should default to current era, heisei)
        // Test parse with incomplete information
        logln("Testing parse w/ just year...");
        SimpleDateFormat *fmt = new SimpleDateFormat(UnicodeString("y"), Locale("ja_JP@calendar=japanese"), status);
        CHECK(status, "creating date format instance");
        if(!fmt) { 
            errln("Couldn't create en_US instance");
        } else {
            UErrorCode s2 = U_ZERO_ERROR;
            cal2->clear();
            UnicodeString samplestr("1");
            logln(UnicodeString() + "Test Year: " + samplestr);
            aDate = fmt->parse(samplestr, s2);
            ParsePosition pp=0;
            fmt->parse(samplestr, *cal2, pp);
            CHECK(s2, "parsing the 1 string");
            logln("*cal2 after 1 parse:");
            str.remove();
            fmt2->format(aDate, str);
            logln(UnicodeString() + "as Gregorian Calendar: " + str);

            cal2->setTime(aDate, s2);
            int32_t gotYear = cal2->get(UCAL_YEAR, s2);
            int32_t gotEra = cal2->get(UCAL_ERA, s2);
            int32_t expectYear = 1;
            int32_t expectEra = JapaneseCalendar::getCurrentEra();
            if((gotYear!=1) || (gotEra != expectEra)) {
                errln(UnicodeString("parse "+samplestr+" of 'y' as Japanese Calendar, expected year ") + expectYear + 
                    UnicodeString(" and era ") + expectEra +", but got year " + gotYear + " and era " + gotEra + " (Gregorian:" + str +")");
            } else {            
                logln(UnicodeString() + " year: " + gotYear + ", era: " + gotEra);
            }
            delete fmt;
        }
    }    

    delete cal2;
    delete cal;
    delete fmt2;
}

void IntlCalendarTest::TestForceGannenNumbering()
{
    UErrorCode status;
    const char* locID = "ja_JP@calendar=japanese";
    Locale loc(locID);
    UDate refDate = 600336000000.0; // 1989 Jan 9 Monday = Heisei 1
    UnicodeString patText(u"Gy年M月d日",-1);
    UnicodeString patNumr(u"GGGGGy/MM/dd",-1);
    UnicodeString skelText(u"yMMMM",-1);

    // Test Gannen year forcing
    status = U_ZERO_ERROR;
    LocalPointer<SimpleDateFormat> testFmt1(new SimpleDateFormat(patText, loc, status));
    LocalPointer<SimpleDateFormat> testFmt2(new SimpleDateFormat(patNumr, loc, status));
    if (U_FAILURE(status)) {
        dataerrln("Fail in new SimpleDateFormat locale %s: %s", locID, u_errorName(status));
    } else {
        UnicodeString testString1, testString2;
        testString1 = testFmt1->format(refDate, testString1);
        if (testString1.length() < 3 || testString1.charAt(2) != 0x5143) {
            errln(UnicodeString("Formatting year 1 in created text style, got " + testString1 + " but expected 3rd char to be 0x5143"));
        }
        testString2 = testFmt2->format(refDate, testString2);
        if (testString2.length() < 2 || testString2.charAt(1) != 0x0031) {
            errln(UnicodeString("Formatting year 1 in created numeric style, got " + testString2 + " but expected 2nd char to be 1"));
        }
        // Now switch the patterns and verify that Gannen use follows the pattern
        testFmt1->applyPattern(patNumr);
        testString1.remove();
        testString1 = testFmt1->format(refDate, testString1);
        if (testString1.length() < 2 || testString1.charAt(1) != 0x0031) {
            errln(UnicodeString("Formatting year 1 in applied numeric style, got " + testString1 + " but expected 2nd char to be 1"));
        }
        testFmt2->applyPattern(patText);
        testString2.remove();
        testString2 = testFmt2->format(refDate, testString2);
        if (testString2.length() < 3 || testString2.charAt(2) != 0x5143) {
            errln(UnicodeString("Formatting year 1 in applied text style, got " + testString2 + " but expected 3rd char to be 0x5143"));
        }
    }

    // Test disabling of Gannen year forcing
    status = U_ZERO_ERROR;
    LocalPointer<DateTimePatternGenerator> dtpgen(DateTimePatternGenerator::createInstance(loc, status));
    if (U_FAILURE(status)) {
        dataerrln("Fail in DateTimePatternGenerator::createInstance locale %s: %s", locID, u_errorName(status));
    } else {
        UnicodeString pattern = dtpgen->getBestPattern(skelText, status);
        if (U_FAILURE(status)) {
            dataerrln("Fail in DateTimePatternGenerator::getBestPattern locale %s: %s", locID, u_errorName(status));
        } else  {
            // Use override string of ""
            LocalPointer<SimpleDateFormat> testFmt3(new SimpleDateFormat(pattern, UnicodeString(""), loc, status));
            if (U_FAILURE(status)) {
                dataerrln("Fail in new SimpleDateFormat locale %s: %s", locID, u_errorName(status));
            } else {
                UnicodeString testString3;
                testString3 = testFmt3->format(refDate, testString3);
                if (testString3.length() < 3 || testString3.charAt(2) != 0x0031) {
                    errln(UnicodeString("Formatting year 1 with Gannen disabled, got " + testString3 + " but expected 3rd char to be 1"));
                }
            }
        }
    }
}

/**
 * Verify the Persian Calendar.
 */
void IntlCalendarTest::TestPersian() {
    UDate timeA = Calendar::getNow();
    
    Calendar *cal;
    UErrorCode status = U_ZERO_ERROR;
    cal = Calendar::createInstance("fa_IR@calendar=persian", status);
    CHECK(status, UnicodeString("Creating fa_IR@calendar=persian calendar"));
    // Sanity check the calendar 
    UDate timeB = Calendar::getNow();
    UDate timeCal = cal->getTime(status);

    if(!(timeA <= timeCal) || !(timeCal <= timeB)) {
      errln((UnicodeString)"Error: Calendar time " + timeCal +
            " is not within sampled times [" + timeA + " to " + timeB + "]!");
    }
    // end sanity check

    // Test various dates to be sure of validity
    int32_t data[] = { 
        1925, 4, 24, 1304, 2, 4,
        2011, 1, 11, 1389, 10, 21,
        1986, 2, 25, 1364, 12, 6, 
        1934, 3, 14, 1312, 12, 23,

        2090, 3, 19, 1468, 12, 29,
        2007, 2, 22, 1385, 12, 3,
        1969, 12, 31, 1348, 10, 10,
        1945, 11, 12, 1324, 8, 21,
        1925, 3, 31, 1304, 1, 11,

        1996, 3, 19, 1374, 12, 29,
        1996, 3, 20, 1375, 1, 1,
        1997, 3, 20, 1375, 12, 30,
        1997, 3, 21, 1376, 1, 1,

        2008, 3, 19, 1386, 12, 29,
        2008, 3, 20, 1387, 1, 1,
        2004, 3, 19, 1382, 12, 29,
        2004, 3, 20, 1383, 1, 1,

        2006, 3, 20, 1384, 12, 29,
        2006, 3, 21, 1385, 1, 1,

        2005, 4, 20, 1384, 1, 31,
        2005, 4, 21, 1384, 2, 1,
        2005, 5, 21, 1384, 2, 31,
        2005, 5, 22, 1384, 3, 1,
        2005, 6, 21, 1384, 3, 31,
        2005, 6, 22, 1384, 4, 1,
        2005, 7, 22, 1384, 4, 31,
        2005, 7, 23, 1384, 5, 1,
        2005, 8, 22, 1384, 5, 31,
        2005, 8, 23, 1384, 6, 1,
        2005, 9, 22, 1384, 6, 31,
        2005, 9, 23, 1384, 7, 1,
        2005, 10, 22, 1384, 7, 30,
        2005, 10, 23, 1384, 8, 1,
        2005, 11, 21, 1384, 8, 30,
        2005, 11, 22, 1384, 9, 1,
        2005, 12, 21, 1384, 9, 30,
        2005, 12, 22, 1384, 10, 1,
        2006, 1, 20, 1384, 10, 30,
        2006, 1, 21, 1384, 11, 1,
        2006, 2, 19, 1384, 11, 30,
        2006, 2, 20, 1384, 12, 1,
        2006, 3, 20, 1384, 12, 29,
        2006, 3, 21, 1385, 1, 1,

        // The 2820-year cycle arithmetical algorithm would fail this one.
        2025, 3, 21, 1404, 1, 1,
        
        -1,-1,-1,-1,-1,-1,-1,-1,-1,-1
    };

    Calendar *grego = Calendar::createInstance("fa_IR@calendar=gregorian", status);
    for (int32_t i=0; data[i]!=-1; ) {
        int32_t gregYear = data[i++];
        int32_t gregMonth = data[i++]-1;
        int32_t gregDay = data[i++];
        int32_t persYear = data[i++];
        int32_t persMonth = data[i++]-1;
        int32_t persDay = data[i++];
        
        // Test conversion from Persian dates
        grego->clear();
        grego->set(gregYear, gregMonth, gregDay);

        cal->clear();
        cal->set(persYear, persMonth, persDay);

        UDate persTime = cal->getTime(status);
        UDate gregTime = grego->getTime(status);

        if (persTime != gregTime) {
          errln(UnicodeString("Expected ") + gregTime + " but got " + persTime);
        }

        // Test conversion to Persian dates
        cal->clear();
        cal->setTime(gregTime, status);

        int32_t computedYear = cal->get(UCAL_YEAR, status);
        int32_t computedMonth = cal->get(UCAL_MONTH, status);
        int32_t computedDay = cal->get(UCAL_DATE, status);

        if ((persYear != computedYear) ||
            (persMonth != computedMonth) ||
            (persDay != computedDay)) {
          errln(UnicodeString("Expected ") + persYear + "/" + (persMonth+1) + "/" + persDay +
                " but got " +  computedYear + "/" + (computedMonth+1) + "/" + computedDay);
        }

    }

    delete cal;
    delete grego;
}

void IntlCalendarTest::TestPersianFormat() {
    UErrorCode status = U_ZERO_ERROR;
    SimpleDateFormat *fmt = new SimpleDateFormat(UnicodeString("MMMM d, yyyy G"), Locale(" en_US@calendar=persian"), status);
    CHECK(status, "creating date format instance");
    SimpleDateFormat *fmt2 = new SimpleDateFormat(UnicodeString("MMMM d, yyyy G"), Locale("en_US@calendar=gregorian"), status);
    CHECK(status, "creating gregorian date format instance");
    UnicodeString gregorianDate("January 18, 2007 AD");
    UDate aDate = fmt2->parse(gregorianDate, status); 
    if(!fmt) { 
        errln("Couldn't create en_US instance");
    } else {
        UnicodeString str;
        fmt->format(aDate, str);
        logln(UnicodeString() + "as Persian Calendar: " + escape(str));
        UnicodeString expected("Dey 28, 1385 AP");
        if(str != expected) {
            errln("Expected " + escape(expected) + " but got " + escape(str));
        }
        UDate otherDate = fmt->parse(expected, status); 
        if(otherDate != aDate) { 
            UnicodeString str3;
            fmt->format(otherDate, str3);
            errln("Parse incorrect of " + escape(expected) + " - wanted " + aDate + " but got " +  otherDate + ", " + escape(str3)); 
        } else {
            logln("Parsed OK: " + expected);
        }
        // Two digit year parsing problem #4732
        fmt->applyPattern("yy-MM-dd");
        str.remove();
        fmt->format(aDate, str);
        expected.setTo("85-10-28");
        if(str != expected) {
            errln("Expected " + escape(expected) + " but got " + escape(str));
        }
        otherDate = fmt->parse(expected, status);
        if (otherDate != aDate) {
            errln("Parse incorrect of " + escape(expected) + " - wanted " + aDate + " but got " + otherDate); 
        } else {
            logln("Parsed OK: " + expected);
        }
        delete fmt;
    }
    delete fmt2;
    
    CHECK(status, "Error occured testing Persian Calendar in English "); 
}


void IntlCalendarTest::simpleTest(const Locale& loc, const UnicodeString& expect, UDate expectDate, UErrorCode& status)
{
    UnicodeString tmp;
    UDate         d;
    DateFormat *fmt0 = DateFormat::createDateTimeInstance(DateFormat::kFull, DateFormat::kFull);

    logln("Try format/parse of " + (UnicodeString)loc.getName());
    DateFormat *fmt2 = DateFormat::createDateInstance(DateFormat::kFull, loc);
    if(fmt2) { 
        fmt2->format(expectDate, tmp);
        logln(escape(tmp) + " ( in locale " + loc.getName() + ")");
        if(tmp != expect) {
            errln(UnicodeString("Failed to format " ) + loc.getName() + " expected " + escape(expect) + " got " + escape(tmp) );
        }

        d = fmt2->parse(expect,status);
        CHECK(status, "Error occurred parsing " + UnicodeString(loc.getName()));
        if(d != expectDate) {
            fmt2->format(d,tmp);
            errln(UnicodeString("Failed to parse " ) + escape(expect) + ", " + loc.getName() + " expect " + (double)expectDate + " got " + (double)d  + " " + escape(tmp));
            logln( "wanted " + escape(fmt0->format(expectDate,tmp.remove())) + " but got " + escape(fmt0->format(d,tmp.remove())));
        }
        delete fmt2;
    } else {
        errln((UnicodeString)"Can't create " + loc.getName() + " date instance");
    }
    delete fmt0;
}

#undef CHECK

#endif /* #if !UCONFIG_NO_FORMATTING */

//eof
