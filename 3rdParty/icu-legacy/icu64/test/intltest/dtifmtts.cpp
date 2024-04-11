// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * COPYRIGHT:
 * Copyright (c) 1997-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING


//TODO: define it in compiler flag
//#define DTIFMTTS_DEBUG 1


#ifdef DTIFMTTS_DEBUG
#include <iostream>
#endif

#include "dtifmtts.h"

#include "cmemory.h"
#include "cstr.h"
#include "cstring.h"
#include "simplethread.h"
#include "japancal.h"
#include "unicode/gregocal.h"
#include "unicode/dtintrv.h"
#include "unicode/dtitvinf.h"
#include "unicode/dtitvfmt.h"
#include "unicode/localpointer.h"
#include "unicode/timezone.h"



#ifdef DTIFMTTS_DEBUG
//#define PRINTMESG(msg) { std::cout << "(" << __FILE__ << ":" << __LINE__ << ") " << msg << "\n"; }
#define PRINTMESG(msg) { std::cout << msg; }
#endif

#include <stdio.h>


void DateIntervalFormatTest::runIndexedTest( int32_t index, UBool exec, const char* &name, char* /*par*/ ) {
    if (exec) logln("TestSuite DateIntervalFormat");
    switch (index) {
        TESTCASE(0, testAPI);
        TESTCASE(1, testFormat);
        TESTCASE(2, testFormatUserDII);
        TESTCASE(3, testSetIntervalPatternNoSideEffect);
        TESTCASE(4, testYearFormats);
        TESTCASE(5, testStress);
        TESTCASE(6, testTicket11583_2);
        TESTCASE(7, testTicket11985);
        TESTCASE(8, testTicket11669);
        TESTCASE(9, testTicket12065);
        TESTCASE(10, testFormattedDateInterval);
        default: name = ""; break;
    }
}

/**
 * Test various generic API methods of DateIntervalFormat for API coverage.
 */
void DateIntervalFormatTest::testAPI() {

    /* ====== Test create interval instance with default locale and skeleton
     */
    UErrorCode status = U_ZERO_ERROR;
    logln("Testing DateIntervalFormat create instance with default locale and skeleton");

    DateIntervalFormat* dtitvfmt = DateIntervalFormat::createInstance(UDAT_YEAR_MONTH_DAY, status);
    if(U_FAILURE(status)) {
        dataerrln("ERROR: Could not create DateIntervalFormat (skeleton + default locale) - exitting");
        return;
    } else {
        delete dtitvfmt;
    }


    /* ====== Test create interval instance with given locale and skeleton
     */
    status = U_ZERO_ERROR;
    logln("Testing DateIntervalFormat create instance with given locale and skeleton");

    dtitvfmt = DateIntervalFormat::createInstance(UDAT_YEAR_MONTH_DAY, Locale::getJapanese(), status);
    if(U_FAILURE(status)) {
        dataerrln("ERROR: Could not create DateIntervalFormat (skeleton + locale) - exitting");
        return;
    } else {
        delete dtitvfmt;
    }


    /* ====== Test create interval instance with dateIntervalInfo and skeleton
     */
    status = U_ZERO_ERROR;
    logln("Testing DateIntervalFormat create instance with dateIntervalInfo  and skeleton");

    DateIntervalInfo* dtitvinf = new DateIntervalInfo(Locale::getSimplifiedChinese(), status);

    dtitvfmt = DateIntervalFormat::createInstance("EEEdMMMyhms", *dtitvinf, status);
    delete dtitvinf;

    if(U_FAILURE(status)) {
        dataerrln("ERROR: Could not create DateIntervalFormat (skeleton + DateIntervalInfo + default locale) - exitting");
        return;
    } else {
        delete dtitvfmt;
    }


    /* ====== Test create interval instance with dateIntervalInfo and skeleton
     */
    status = U_ZERO_ERROR;
    logln("Testing DateIntervalFormat create instance with dateIntervalInfo  and skeleton");

    dtitvinf = new DateIntervalInfo(Locale::getSimplifiedChinese(), status);

    dtitvfmt = DateIntervalFormat::createInstance("EEEdMMMyhms", Locale::getSimplifiedChinese(), *dtitvinf, status);
    delete dtitvinf;
    if(U_FAILURE(status)) {
        dataerrln("ERROR: Could not create DateIntervalFormat (skeleton + DateIntervalInfo + locale) - exitting");
        return;
    }
    // not deleted, test clone


    // ====== Test clone()
    status = U_ZERO_ERROR;
    logln("Testing DateIntervalFormat clone");

    DateIntervalFormat* another = (DateIntervalFormat*)dtitvfmt->clone();
    if ( (*another) != (*dtitvfmt) ) {
        dataerrln("%s:%d ERROR: clone failed", __FILE__, __LINE__);
    }


    // ====== Test getDateIntervalInfo, setDateIntervalInfo, adoptDateIntervalInfo
    status = U_ZERO_ERROR;
    logln("Testing DateIntervalFormat getDateIntervalInfo");
    const DateIntervalInfo* inf = another->getDateIntervalInfo();
    dtitvfmt->setDateIntervalInfo(*inf, status);
    const DateIntervalInfo* anotherInf = dtitvfmt->getDateIntervalInfo();
    if ( (*inf) != (*anotherInf) || U_FAILURE(status) ) {
        dataerrln("ERROR: getDateIntervalInfo/setDateIntervalInfo failed");
    }

    {
        // We make sure that setDateIntervalInfo does not corrupt the cache. See ticket 9919.
        status = U_ZERO_ERROR;
        logln("Testing DateIntervalFormat setDateIntervalInfo");
        const Locale &enLocale = Locale::getEnglish();
        LocalPointer<DateIntervalFormat> dif(DateIntervalFormat::createInstance("yMd", enLocale, status));
        if (U_FAILURE(status)) {
            errln("Failure encountered: %s", u_errorName(status));
            return;
        }
        UnicodeString expected;
        LocalPointer<Calendar> fromTime(Calendar::createInstance(enLocale, status));
        LocalPointer<Calendar> toTime(Calendar::createInstance(enLocale, status));
        if (U_FAILURE(status)) {
            errln("Failure encountered: %s", u_errorName(status));
            return;
        }
        FieldPosition pos(FieldPosition::DONT_CARE);
        fromTime->set(2013, 3, 26);
        toTime->set(2013, 3, 28);
        dif->format(*fromTime, *toTime, expected, pos, status);
        if (U_FAILURE(status)) {
            errln("Failure encountered: %s", u_errorName(status));
            return;
        }
        LocalPointer<DateIntervalInfo> dii(new DateIntervalInfo(Locale::getEnglish(), status), status);
        if (U_FAILURE(status)) {
            errln("Failure encountered: %s", u_errorName(status));
            return;
        }
        dii->setIntervalPattern(ctou("yMd"), UCAL_DATE, ctou("M/d/y \\u2013 d"), status);
        dif->setDateIntervalInfo(*dii, status);
        if (U_FAILURE(status)) {
            errln("Failure encountered: %s", u_errorName(status));
            return;
        }
        dif.adoptInstead(DateIntervalFormat::createInstance("yMd", enLocale, status));
        if (U_FAILURE(status)) {
            errln("Failure encountered: %s", u_errorName(status));
            return;
        }
        UnicodeString actual;
        pos = 0;
        dif->format(*fromTime, *toTime, actual, pos, status);
        if (U_FAILURE(status)) {
            errln("Failure encountered: %s", u_errorName(status));
            return;
        }
        if (expected != actual) {
            errln("DateIntervalFormat.setIntervalInfo should have no side effects.");
        }
    }

    /*
    status = U_ZERO_ERROR;
    DateIntervalInfo* nonConstInf = inf->clone();
    dtitvfmt->adoptDateIntervalInfo(nonConstInf, status);
    anotherInf = dtitvfmt->getDateIntervalInfo();
    if ( (*inf) != (*anotherInf) || U_FAILURE(status) ) {
        dataerrln("ERROR: adoptDateIntervalInfo failed");
    }
    */

    // ====== Test getDateFormat, setDateFormat, adoptDateFormat

    status = U_ZERO_ERROR;
    logln("Testing DateIntervalFormat getDateFormat");
    /*
    const DateFormat* fmt = another->getDateFormat();
    dtitvfmt->setDateFormat(*fmt, status);
    const DateFormat* anotherFmt = dtitvfmt->getDateFormat();
    if ( (*fmt) != (*anotherFmt) || U_FAILURE(status) ) {
        dataerrln("ERROR: getDateFormat/setDateFormat failed");
    }

    status = U_ZERO_ERROR;
    DateFormat* nonConstFmt = (DateFormat*)fmt->clone();
    dtitvfmt->adoptDateFormat(nonConstFmt, status);
    anotherFmt = dtitvfmt->getDateFormat();
    if ( (*fmt) != (*anotherFmt) || U_FAILURE(status) ) {
        dataerrln("ERROR: adoptDateFormat failed");
    }
    delete fmt;
    */


    // ======= Test getStaticClassID()

    logln("Testing getStaticClassID()");


    if(dtitvfmt->getDynamicClassID() != DateIntervalFormat::getStaticClassID()) {
        errln("ERROR: getDynamicClassID() didn't return the expected value");
    }

    delete another;

    // ====== test constructor/copy constructor and assignment
    /* they are protected, no test
    logln("Testing DateIntervalFormat constructor and assigment operator");
    status = U_ZERO_ERROR;

    DateFormat* constFmt = (constFmt*)dtitvfmt->getDateFormat()->clone();
    inf = dtitvfmt->getDateIntervalInfo()->clone();


    DateIntervalFormat* dtifmt = new DateIntervalFormat(fmt, inf, status);
    if(U_FAILURE(status)) {
        dataerrln("ERROR: Could not create DateIntervalFormat (default) - exitting");
        return;
    }

    DateIntervalFormat* dtifmt2 = new(dtifmt);
    if ( (*dtifmt) != (*dtifmt2) ) {
        dataerrln("ERROR: Could not create DateIntervalFormat (default) - exitting");
        return;
    }

    DateIntervalFormat dtifmt3 = (*dtifmt);
    if ( (*dtifmt) != dtifmt3 ) {
        dataerrln("ERROR: Could not create DateIntervalFormat (default) - exitting");
        return;
    }

    delete dtifmt2;
    delete dtifmt3;
    delete dtifmt;
    */


    //===== test format and parse ==================
    Formattable formattable;
    formattable.setInt64(10);
    UnicodeString res;
    FieldPosition pos(FieldPosition::DONT_CARE);
    status = U_ZERO_ERROR;
    dtitvfmt->format(formattable, res, pos, status);
    if ( status != U_ILLEGAL_ARGUMENT_ERROR ) {
        dataerrln("ERROR: format non-date-interval object should set U_ILLEGAL_ARGUMENT_ERROR - exitting");
        return;
    }

    DateInterval* dtitv = new DateInterval(3600*24*365, 3600*24*366);
    formattable.adoptObject(dtitv);
    res.remove();
    pos = 0;
    status = U_ZERO_ERROR;
    dtitvfmt->format(formattable, res, pos, status);
    if ( U_FAILURE(status) ) {
        dataerrln("ERROR: format date interval failed - exitting");
        return;
    }

    const DateFormat* dfmt = dtitvfmt->getDateFormat();
    Calendar* fromCal = dfmt->getCalendar()->clone();
    Calendar* toCal = dfmt->getCalendar()->clone();
    res.remove();
    pos = 0;
    status = U_ZERO_ERROR;
    dtitvfmt->format(*fromCal, *toCal, res, pos, status);
    if ( U_FAILURE(status) ) {
        dataerrln("ERROR: format date interval failed - exitting");
        return;
    }
    delete fromCal;
    delete toCal;


    Formattable fmttable;
    status = U_ZERO_ERROR;
    // TODO: why do I need cast?
    ((Format*)dtitvfmt)->parseObject(res, fmttable, status);
    if ( status != U_INVALID_FORMAT_ERROR ) {
        dataerrln("ERROR: parse should set U_INVALID_FORMAT_ERROR - exitting");
        return;
    }

    delete dtitvfmt;

    //====== test setting time zone
    logln("Testing DateIntervalFormat set & format with different time zones, get time zone");
    status = U_ZERO_ERROR;
    dtitvfmt = DateIntervalFormat::createInstance("MMMdHHmm", Locale::getEnglish(), status);
    if ( U_SUCCESS(status) ) {
        UDate date1 = 1299090600000.0; // 2011-Mar-02 1030 in US/Pacific, 2011-Mar-03 0330 in Asia/Tokyo
        UDate date2 = 1299115800000.0; // 2011-Mar-02 1730 in US/Pacific, 2011-Mar-03 1030 in Asia/Tokyo

        DateInterval * dtitv12 = new DateInterval(date1, date2);
        TimeZone * tzCalif = TimeZone::createTimeZone("US/Pacific");
        TimeZone * tzTokyo = TimeZone::createTimeZone("Asia/Tokyo");
        UnicodeString fmtCalif = UnicodeString(ctou("Mar 2, 10:30 \\u2013 17:30"));
        UnicodeString fmtTokyo = UnicodeString(ctou("Mar 3, 03:30 \\u2013 10:30"));

        dtitvfmt->adoptTimeZone(tzCalif);
        res.remove();
        pos = 0;
        status = U_ZERO_ERROR;
        dtitvfmt->format(dtitv12, res, pos, status);
        if ( U_SUCCESS(status) ) {
            if ( res.compare(fmtCalif) != 0 ) {
                errln("ERROR: DateIntervalFormat::format for tzCalif, expect " + fmtCalif + ", get " + res);
            }
        } else {
            errln("ERROR: DateIntervalFormat::format for tzCalif, status %s", u_errorName(status));
        }

        dtitvfmt->setTimeZone(*tzTokyo);
        res.remove();
        pos = 0;
        status = U_ZERO_ERROR;
        dtitvfmt->format(dtitv12, res, pos, status);
        if ( U_SUCCESS(status) ) {
            if ( res.compare(fmtTokyo) != 0 ) {
                errln("ERROR: DateIntervalFormat::format for fmtTokyo, expect " + fmtTokyo + ", get " + res);
            }
        } else {
            errln("ERROR: DateIntervalFormat::format for tzTokyo, status %s", u_errorName(status));
        }

        if ( dtitvfmt->getTimeZone() != *tzTokyo ) {
            errln("ERROR: DateIntervalFormat::getTimeZone returns mismatch.");
        }

        delete tzTokyo; // tzCalif was owned by dtitvfmt which should have deleted it
        delete dtitv12;
        delete dtitvfmt;
    } else {
        errln("ERROR: DateIntervalFormat::createInstance(\"MdHH\", Locale::getEnglish(), ...), status %s", u_errorName(status));
    }
    //====== test format  in testFormat()

    //====== test DateInterval class (better coverage)
    DateInterval dtitv1(3600*24*365, 3600*24*366);
    DateInterval dtitv2(dtitv1);

    if (!(dtitv1 == dtitv2)) {
        errln("ERROR: Copy constructor failed for DateInterval.");
    }

    DateInterval dtitv3(3600*365, 3600*366);
    dtitv3 = dtitv1;
    if (!(dtitv3 == dtitv1)) {
        errln("ERROR: Equal operator failed for DateInterval.");
    }

    DateInterval *dtitv4 = dtitv1.clone();
    if (*dtitv4 != dtitv1) {
        errln("ERROR: Equal operator failed for DateInterval.");
    }
    delete dtitv4;
}


/**
 * Test format
 */
void DateIntervalFormatTest::testFormat() {
    // first item is date pattern
    // followed by a group of locale/from_data/to_data/skeleton/interval_data
    // Note that from_data/to_data are specified using era names from root, for the calendar specified by locale.
    const char* DATA[] = {
        "GGGGG y MM dd HH:mm:ss", // pattern for from_data/to_data
        // test root
        "root", "CE 2007 11 10 10:10:10", "CE 2007 12 10 10:10:10", "yM", "2007-11 \\u2013 2007-12",

        // test 'H' and 'h', using availableFormat in fallback
        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 10 15:10:10", "Hms", "10:10:10 \\u2013 15:10:10",
        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 10 15:10:10", "hms", "10:10:10 AM \\u2013 3:10:10 PM",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "MMMM", "October 2007 \\u2013 October 2008",
        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "MMM", "Oct 2007 \\u2013 Oct 2008",
        // test skeleton with both date and time
        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "dMMMyhm", "Nov 10, 2007, 10:10 AM \\u2013 Nov 20, 2007, 10:10 AM",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 10 11:10:10", "dMMMyhm", "Nov 10, 2007, 10:10 \\u2013 11:10 AM",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 10 11:10:10", "hms", "10:10:10 AM \\u2013 11:10:10 AM",
        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 10 11:10:10", "Hms", "10:10:10 \\u2013 11:10:10",
        "en", "CE 2007 11 10 20:10:10", "CE 2007 11 10 21:10:10", "Hms", "20:10:10 \\u2013 21:10:10",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "EEEEdMMMMy", "Wednesday, October 10, 2007 \\u2013 Friday, October 10, 2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "dMMMMy", "October 10, 2007 \\u2013 October 10, 2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "dMMMM", "October 10, 2007 \\u2013 October 10, 2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "MMMMy", "October 2007 \\u2013 October 2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "EEEEdMMMM", "Wednesday, October 10, 2007 \\u2013 Friday, October 10, 2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "EdMMMy", "Wed, Oct 10, 2007 \\u2013 Fri, Oct 10, 2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "dMMMy", "Oct 10, 2007 \\u2013 Oct 10, 2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "dMMM", "Oct 10, 2007 \\u2013 Oct 10, 2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "MMMy", "Oct 2007 \\u2013 Oct 2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "EdMMM", "Wed, Oct 10, 2007 \\u2013 Fri, Oct 10, 2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "EdMy", "Wed, 10/10/2007 \\u2013 Fri, 10/10/2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "dMy", "10/10/2007 \\u2013 10/10/2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "dM", "10/10/2007 \\u2013 10/10/2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "My", "10/2007 \\u2013 10/2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "EdM", "Wed, 10/10/2007 \\u2013 Fri, 10/10/2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "d", "10/10/2007 \\u2013 10/10/2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "Ed", "10 Wed \\u2013 10 Fri",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "y", "2007 \\u2013 2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "M", "10/2007 \\u2013 10/2008",



        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "hm", "10/10/2007, 10:10 AM \\u2013 10/10/2008, 10:10 AM",
        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "Hm", "10/10/2007, 10:10 \\u2013 10/10/2008, 10:10",
        "en", "CE 2007 10 10 20:10:10", "CE 2008 10 10 20:10:10", "Hm", "10/10/2007, 20:10 \\u2013 10/10/2008, 20:10",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "hmv", "10/10/2007, 10:10 AM PT \\u2013 10/10/2008, 10:10 AM PT",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "hmz", "10/10/2007, 10:10 AM PDT \\u2013 10/10/2008, 10:10 AM PDT",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "h", "10/10/2007, 10 AM \\u2013 10/10/2008, 10 AM",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "hv", "10/10/2007, 10 AM PT \\u2013 10/10/2008, 10 AM PT",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "hz", "10/10/2007, 10 AM PDT \\u2013 10/10/2008, 10 AM PDT",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "EEddMMyyyy", "Wed, 10/10/2007 \\u2013 Fri, 10/10/2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "EddMMy", "Wed, 10/10/2007 \\u2013 Fri, 10/10/2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "hhmm", "10/10/2007, 10:10 AM \\u2013 10/10/2008, 10:10 AM",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "hhmmzz", "10/10/2007, 10:10 AM PDT \\u2013 10/10/2008, 10:10 AM PDT",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "hms", "10/10/2007, 10:10:10 AM \\u2013 10/10/2008, 10:10:10 AM",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "dMMMMMy", "O 10, 2007 \\u2013 O 10, 2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "EEEEEdM", "W, 10/10/2007 \\u2013 F, 10/10/2008",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "EEEEdMMMMy", "Wednesday, October 10 \\u2013 Saturday, November 10, 2007",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "dMMMMy", "October 10 \\u2013 November 10, 2007",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "dMMMM", "October 10 \\u2013 November 10",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "MMMMy", "October \\u2013 November 2007",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "EEEEdMMMM", "Wednesday, October 10 \\u2013 Saturday, November 10",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "EdMMMy", "Wed, Oct 10 \\u2013 Sat, Nov 10, 2007",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "dMMMy", "Oct 10 \\u2013 Nov 10, 2007",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "dMMM", "Oct 10 \\u2013 Nov 10",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "MMMy", "Oct \\u2013 Nov 2007",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "EdMMM", "Wed, Oct 10 \\u2013 Sat, Nov 10",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "EdMy", "Wed, 10/10/2007 \\u2013 Sat, 11/10/2007",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "dMy", "10/10/2007 \\u2013 11/10/2007",


        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "My", "10/2007 \\u2013 11/2007",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "EdM", "Wed, 10/10 \\u2013 Sat, 11/10",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "d", "10/10 \\u2013 11/10",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "Ed", "10 Wed \\u2013 10 Sat",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "y", "2007",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "M", "10 \\u2013 11",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "MMM", "Oct \\u2013 Nov",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "MMMM", "October \\u2013 November",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "hm", "10/10/2007, 10:10 AM \\u2013 11/10/2007, 10:10 AM",
        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "Hm", "10/10/2007, 10:10 \\u2013 11/10/2007, 10:10",
        "en", "CE 2007 10 10 20:10:10", "CE 2007 11 10 20:10:10", "Hm", "10/10/2007, 20:10 \\u2013 11/10/2007, 20:10",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "hmv", "10/10/2007, 10:10 AM PT \\u2013 11/10/2007, 10:10 AM PT",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "hmz", "10/10/2007, 10:10 AM PDT \\u2013 11/10/2007, 10:10 AM PST",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "h", "10/10/2007, 10 AM \\u2013 11/10/2007, 10 AM",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "hv", "10/10/2007, 10 AM PT \\u2013 11/10/2007, 10 AM PT",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "hz", "10/10/2007, 10 AM PDT \\u2013 11/10/2007, 10 AM PST",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "EEddMMyyyy", "Wed, 10/10/2007 \\u2013 Sat, 11/10/2007",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "EddMMy", "Wed, 10/10/2007 \\u2013 Sat, 11/10/2007",


        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "hhmmzz", "10/10/2007, 10:10 AM PDT \\u2013 11/10/2007, 10:10 AM PST",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "hms", "10/10/2007, 10:10:10 AM \\u2013 11/10/2007, 10:10:10 AM",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "dMMMMMy", "O 10 \\u2013 N 10, 2007",

        "en", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "EEEEEdM", "W, 10/10 \\u2013 S, 11/10",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EEEEdMMMMy", "Saturday, November 10 \\u2013 Tuesday, November 20, 2007",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "dMMMMy", "November 10 \\u2013 20, 2007",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "dMMMM", "November 10 \\u2013 20",


        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EEEEdMMMM", "Saturday, November 10 \\u2013 Tuesday, November 20",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EdMMMy", "Sat, Nov 10 \\u2013 Tue, Nov 20, 2007",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "dMMMy", "Nov 10 \\u2013 20, 2007",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "dMMM", "Nov 10 \\u2013 20",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "MMMy", "Nov 2007",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EdMMM", "Sat, Nov 10 \\u2013 Tue, Nov 20",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EdMy", "Sat, 11/10/2007 \\u2013 Tue, 11/20/2007",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "dMy", "11/10/2007 \\u2013 11/20/2007",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "dM", "11/10 \\u2013 11/20",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "My", "11/2007",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EdM", "Sat, 11/10 \\u2013 Tue, 11/20",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "d", "10 \\u2013 20",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "Ed", "10 Sat \\u2013 20 Tue",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "y", "2007",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "M", "11",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "MMM", "Nov",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "MMMM", "November",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "hm", "11/10/2007, 10:10 AM \\u2013 11/20/2007, 10:10 AM",
        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "Hm", "11/10/2007, 10:10 \\u2013 11/20/2007, 10:10",
        "en", "CE 2007 11 10 20:10:10", "CE 2007 11 20 20:10:10", "Hm", "11/10/2007, 20:10 \\u2013 11/20/2007, 20:10",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "hmv", "11/10/2007, 10:10 AM PT \\u2013 11/20/2007, 10:10 AM PT",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "hmz", "11/10/2007, 10:10 AM PST \\u2013 11/20/2007, 10:10 AM PST",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "h", "11/10/2007, 10 AM \\u2013 11/20/2007, 10 AM",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "hv", "11/10/2007, 10 AM PT \\u2013 11/20/2007, 10 AM PT",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "hz", "11/10/2007, 10 AM PST \\u2013 11/20/2007, 10 AM PST",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EEddMMyyyy", "Sat, 11/10/2007 \\u2013 Tue, 11/20/2007",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EddMMy", "Sat, 11/10/2007 \\u2013 Tue, 11/20/2007",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "hhmm", "11/10/2007, 10:10 AM \\u2013 11/20/2007, 10:10 AM",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "hhmmzz", "11/10/2007, 10:10 AM PST \\u2013 11/20/2007, 10:10 AM PST",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "hms", "11/10/2007, 10:10:10 AM \\u2013 11/20/2007, 10:10:10 AM",
        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "Hms", "11/10/2007, 10:10:10 \\u2013 11/20/2007, 10:10:10",
        "en", "CE 2007 11 10 20:10:10", "CE 2007 11 20 20:10:10", "Hms", "11/10/2007, 20:10:10 \\u2013 11/20/2007, 20:10:10",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "dMMMMMy", "N 10 \\u2013 20, 2007",

        "en", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EEEEEdM", "S, 11/10 \\u2013 T, 11/20",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "EEEEdMMMMy", "Wednesday, January 10, 2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "dMMMMy", "January 10, 2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "dMMMM", "January 10",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "MMMMy", "January 2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "EEEEdMMMM", "Wednesday, January 10",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "EdMMMy", "Wed, Jan 10, 2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "dMMMy", "Jan 10, 2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "dMMM", "Jan 10",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "MMMy", "Jan 2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "EdMMM", "Wed, Jan 10",


        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "dMy", "1/10/2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "dM", "1/10",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "My", "1/2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "EdM", "Wed, 1/10",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "d", "10",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "Ed", "10 Wed",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "y", "2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "M", "1",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "MMM", "Jan",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "MMMM", "January",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "hm", "10:00 AM \\u2013 2:10 PM",
        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "Hm", "10:00 \\u2013 14:10",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "hmv", "10:00 AM \\u2013 2:10 PM PT",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "hmz", "10:00 AM \\u2013 2:10 PM PST",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "h", "10 AM \\u2013 2 PM",
        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "H", "10 \\u2013 14",


        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "hz", "10 AM \\u2013 2 PM PST",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "EEddMMyyyy", "Wed, 01/10/2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "EddMMy", "Wed, 01/10/2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "hhmm", "10:00 AM \\u2013 2:10 PM",
        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "HHmm", "10:00 \\u2013 14:10",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "hhmmzz", "10:00 AM \\u2013 2:10 PM PST",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "hms", "10:00:10 AM \\u2013 2:10:10 PM",
        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "Hms", "10:00:10 \\u2013 14:10:10",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "dMMMMMy", "J 10, 2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "EEEEEdM", "W, 1/10",
        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "dMMMMy", "January 10, 2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "dMMMM", "January 10",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "MMMMy", "January 2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "EEEEdMMMM", "Wednesday, January 10",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "EdMMMy", "Wed, Jan 10, 2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "dMMMy", "Jan 10, 2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "dMMM", "Jan 10",


        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "EdMMM", "Wed, Jan 10",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "EdMy", "Wed, 1/10/2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "dMy", "1/10/2007",


        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "My", "1/2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "EdM", "Wed, 1/10",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "d", "10",


        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "y", "2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "M", "1",



        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "hm", "10:00 \\u2013 10:20 AM",
        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "Hm", "10:00 \\u2013 10:20",


        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "hmz", "10:00 \\u2013 10:20 AM PST",


        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "hv", "10 AM PT",



        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "EddMMy", "Wed, 01/10/2007",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "hhmm", "10:00 \\u2013 10:20 AM",
        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "HHmm", "10:00 \\u2013 10:20",

        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "hhmmzz", "10:00 \\u2013 10:20 AM PST",


        "en", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "dMMMMMy", "J 10, 2007",


        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "EEEEdMMMMy", "Wednesday, January 10, 2007",

        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "dMMMMy", "January 10, 2007",


        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "MMMMy", "January 2007",

        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "EEEEdMMMM", "Wednesday, January 10",


        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "dMMMy", "Jan 10, 2007",

        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "dMMM", "Jan 10",


        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "EdMMM", "Wed, Jan 10",

        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "EdMy", "Wed, 1/10/2007",

        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "dMy", "1/10/2007",


        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "My", "1/2007",

        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "EdM", "Wed, 1/10",

        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "d", "10",


        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "y", "2007",

        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "M", "1",


        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "MMMM", "January",

        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "hm", "10:10 AM",
        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "Hm", "10:10",


        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "hmz", "10:10 AM PST",

        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "h", "10 AM",

        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "hv", "10 AM PT",


        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "EEddMMyyyy", "Wed, 01/10/2007",


        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "hhmm", "10:10 AM",
        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "HHmm", "10:10",

        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "hhmmzz", "10:10 AM PST",


        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "dMMMMMy", "J 10, 2007",

        "en", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "EEEEEdM", "W, 1/10",

        "zh", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "EEEEdMMMMy", "2007\\u5e7410\\u670810\\u65e5\\u661f\\u671f\\u4e09\\u81f32008\\u5e7410\\u670810\\u65e5\\u661f\\u671f\\u4e94",


        "zh", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "dMMMMy", "2007\\u5e7410\\u670810\\u65e5\\u81f311\\u670810\\u65e5",


        "zh", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "MMMMy", "2007\\u5e7410\\u6708\\u81f311\\u6708",


        "zh", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "hmv", "2007/10/10 \\u6D1B\\u6749\\u77F6\\u65F6\\u95F4 \\u4E0A\\u534810:10 \\u2013 2007/11/10 \\u6D1B\\u6749\\u77F6\\u65F6\\u95F4 \\u4E0A\\u534810:10",

        "zh", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EEEEdMMMMy", "2007\\u5e7411\\u670810\\u65e5\\u661f\\u671f\\u516d\\u81f320\\u65e5\\u661f\\u671f\\u4e8c",


        "zh", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "dMMMM", "11\\u670810\\u65e5\\u81f320\\u65e5",

        "zh", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "MMMMy", "2007\\u5E7411\\u6708", // (fixed expected result per ticket:6626:)

        "zh", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EEEEdMMMM", "11\\u670810\\u65e5\\u661f\\u671f\\u516d\\u81f320\\u65e5\\u661f\\u671f\\u4e8c",


        "zh", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EdMy", "2007/11/10\\u5468\\u516d\\u81f32007/11/20\\u5468\\u4e8c",


        "zh", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "dM", "11/10 \\u2013 11/20",

        "zh", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "My", "2007\\u5E7411\\u6708",

        "zh", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EdM", "11/10\\u5468\\u516d\\u81f311/20\\u5468\\u4e8c",


        "zh", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "y", "2007\\u5E74", // (fixed expected result per ticket:6626:)

        "zh", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "M", "11\\u6708",

        "zh", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "MMM", "11\\u6708", // (fixed expected result per ticket:6626: and others)


        "zh", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "hmz", "2007/11/10 GMT-8 \\u4e0a\\u534810:10 \\u2013 2007/11/20 GMT-8 \\u4e0a\\u534810:10",

        "zh", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "h", "2007/11/10 \\u4e0a\\u534810\\u65f6 \\u2013 2007/11/20 \\u4e0a\\u534810\\u65f6",

        "zh", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "EEEEdMMMMy", "2007\\u5e741\\u670810\\u65e5\\u661f\\u671f\\u4e09", // (fixed expected result per ticket:6626:)

        "zh", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "hm", "\\u4e0a\\u534810:00\\u81f3\\u4e0b\\u53482:10",


        "zh", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "hmz", "GMT-8\\u4e0a\\u534810:00\\u81f3\\u4e0b\\u53482:10",

        "zh", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "h", "\\u4e0a\\u534810\\u65F6\\u81f3\\u4e0b\\u53482\\u65f6",

        "zh", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "hv", "\\u6D1B\\u6749\\u77F6\\u65F6\\u95F4\\u4E0A\\u534810\\u65F6\\u81F3\\u4E0B\\u53482\\u65F6",

        "zh", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "hm", "\\u4e0a\\u534810:00\\u81f310:20",

        "zh", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "hmv", "\\u6D1B\\u6749\\u77F6\\u65F6\\u95F4\\u4E0A\\u534810:00\\u81F310:20",

        "zh", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "hz", "GMT-8\\u4e0a\\u534810\\u65f6",

        "zh", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "hm", "\\u4e0a\\u534810:10",

        "zh", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "h", "\\u4e0a\\u534810\\u65f6",

        "de", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "EEEEdMMMy", "Mittwoch, 10. Okt. 2007 \\u2013 Freitag, 10. Okt. 2008",


        "de", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "dMMM", "10. Okt. 2007 \\u2013 10. Okt. 2008",

        "de", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "MMMy", "Okt. 2007 \\u2013 Okt. 2008",


        "de", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "EdMy", "Mi., 10.10.2007 \\u2013 Fr., 10.10.2008",

        "de", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "dMy", "10.10.2007 \\u2013 10.10.2008",


        "de", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "My", "10.2007 \\u2013 10.2008",

        "de", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "EdM", "Mi., 10.10.2007 \\u2013 Fr., 10.10.2008",


        "de", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "y", "2007\\u20132008",

        "de", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "M", "10.2007 \\u2013 10.2008",


        "de", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "hm", "10.10.2007, 10:10 AM \\u2013 10.10.2008, 10:10 AM",
        "de", "CE 2007 10 10 10:10:10", "CE 2008 10 10 10:10:10", "Hm", "10.10.2007, 10:10 \\u2013 10.10.2008, 10:10",

        "de", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "EEEEdMMMy", "Mittwoch, 10. Okt. \\u2013 Samstag, 10. Nov. 2007",


        "de", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "dMMM", "10. Okt. \\u2013 10. Nov.",

        "de", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "MMMy", "Okt.\\u2013Nov. 2007",

        "de", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "EEEEdMMM", "Mittwoch, 10. Okt. \\u2013 Samstag, 10. Nov.",


        "de", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "dM", "10.10. \\u2013 10.11.",

        "de", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "My", "10.2007 \\u2013 11.2007",


        "de", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "d", "10.10. \\u2013 10.11.",

        "de", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "y", "2007",


        "de", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "MMM", "Okt.\\u2013Nov.",


        "de", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "hms", "10.10.2007, 10:10:10 AM \\u2013 10.11.2007, 10:10:10 AM",
        "de", "CE 2007 10 10 10:10:10", "CE 2007 11 10 10:10:10", "Hms", "10.10.2007, 10:10:10 \\u2013 10.11.2007, 10:10:10",

        "de", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EEEEdMMMy", "Samstag, 10. \\u2013 Dienstag, 20. Nov. 2007",

        "de", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "dMMMy", "10.\\u201320. Nov. 2007",


        "de", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "MMMy", "Nov. 2007",

        "de", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EEEEdMMM", "Samstag, 10. \\u2013 Dienstag, 20. Nov.",

        "de", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "EdMy", "Sa., 10. \\u2013 Di., 20.11.2007",


        "de", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "dM", "10.\\u201320.11.",

        "de", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "My", "11.2007",


        "de", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "d", "10.\\u201320.",

        "de", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "y", "2007",


        "de", "CE 2007 11 10 10:10:10", "CE 2007 11 20 10:10:10", "hmv", "10.11.2007, 10:10 AM Los Angeles Zeit \\u2013 20.11.2007, 10:10 AM Los Angeles Zeit",

        "de", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "EEEEdMMMy", "Mittwoch, 10. Jan. 2007",


        "de", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "dMMM", "10. Jan.",

        "de", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "MMMy", "Jan. 2007",

        "de", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "EEEEdMMM", "Mittwoch, 10. Jan.",

        /* Following is an important test, because the 'h' in 'Uhr' is interpreted as a pattern
           if not escaped properly. */
        "de", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "h", "10 Uhr AM \\u2013 2 Uhr PM",
        "de", "CE 2007 01 10 10:00:10", "CE 2007 01 10 14:10:10", "H", "10\\u201314 Uhr",

        "de", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "EEEEdMMM", "Mittwoch, 10. Jan.",


        "de", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "hmv", "10:00\\u201310:20 AM Los Angeles Zeit",

        "de", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "hmz", "10:00\\u201310:20 AM GMT-8",

        "de", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "h", "10 Uhr AM",
        "de", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "H", "10 Uhr",


        "de", "CE 2007 01 10 10:00:10", "CE 2007 01 10 10:20:10", "hz", "10 Uhr AM GMT-8",

        "de", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "EEEEdMMMy", "Mittwoch, 10. Jan. 2007",


        "de", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "hmv", "10:10 AM Los Angeles Zeit",

        "de", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "hmz", "10:10 AM GMT-8",


        "de", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "hv", "10 Uhr AM Los Angeles Zeit",

        "de", "CE 2007 01 10 10:10:10", "CE 2007 01 10 10:10:20", "hz", "10 Uhr AM GMT-8",

        // Thai (default calendar buddhist)

        "th", "BE 2550 10 10 10:10:10", "BE 2551 10 10 10:10:10", "EEEEdMMMy", "\\u0E27\\u0E31\\u0E19\\u0E1E\\u0E38\\u0E18\\u0E17\\u0E35\\u0E48 10 \\u0E15.\\u0E04. 2550 \\u2013 \\u0E27\\u0E31\\u0E19\\u0E28\\u0E38\\u0E01\\u0E23\\u0E4C\\u0E17\\u0E35\\u0E48 10 \\u0E15.\\u0E04. 2551",


        "th", "BE 2550 10 10 10:10:10", "BE 2551 10 10 10:10:10", "dMMM", "10 \\u0E15.\\u0E04. 2550 \\u2013 10 \\u0E15.\\u0E04. 2551",

        "th", "BE 2550 10 10 10:10:10", "BE 2551 10 10 10:10:10", "MMMy", "\\u0E15.\\u0E04. 2550 \\u2013 \\u0E15.\\u0E04. 2551",


        "th", "BE 2550 10 10 10:10:10", "BE 2551 10 10 10:10:10", "EdMy", "\\u0E1E. 10/10/2550 \\u2013 \\u0E28. 10/10/2551",

        "th", "BE 2550 10 10 10:10:10", "BE 2551 10 10 10:10:10", "dMy", "10/10/2550 \\u2013 10/10/2551",


        "th", "BE 2550 10 10 10:10:10", "BE 2551 10 10 10:10:10", "My", "10/2550 \\u2013 10/2551",

        "th", "BE 2550 10 10 10:10:10", "BE 2551 10 10 10:10:10", "EdM", "\\u0E1E. 10/10/2550 \\u2013 \\u0E28. 10/10/2551",


        "th", "BE 2550 10 10 10:10:10", "BE 2551 10 10 10:10:10", "y", "2550\\u20132551",

        "th", "BE 2550 10 10 10:10:10", "BE 2551 10 10 10:10:10", "M", "10/2550 \\u2013 10/2551",


        "th", "BE 2550 10 10 10:10:10", "BE 2550 11 10 10:10:10", "EEEEdMMMy", "\\u0E27\\u0E31\\u0E19\\u0E1E\\u0E38\\u0E18\\u0E17\\u0E35\\u0E48 10 \\u0E15.\\u0E04. \\u2013 \\u0E27\\u0E31\\u0E19\\u0E40\\u0E2A\\u0E32\\u0E23\\u0E4C\\u0E17\\u0E35\\u0E48 10 \\u0E1E.\\u0E22. 2550",


        "th", "BE 2550 10 10 10:10:10", "BE 2550 11 10 10:10:10", "dMMM", "10 \\u0E15.\\u0E04. \\u2013 10 \\u0E1E.\\u0E22.",

        "th", "BE 2550 10 10 10:10:10", "BE 2550 11 10 10:10:10", "MMMy", "\\u0E15.\\u0E04.\\u2013\\u0E1E.\\u0E22. 2550",

        "th", "2550 10 10 10:10:10", "2550 11 10 10:10:10", "dM", "10/10 \\u2013 10/11",

        "th", "BE 2550 10 10 10:10:10", "BE 2550 11 10 10:10:10", "My", "10/2550 \\u2013 11/2550",


        "th", "BE 2550 10 10 10:10:10", "BE 2550 11 10 10:10:10", "d", "10/10 \\u2013 10/11",

        "th", "BE 2550 10 10 10:10:10", "BE 2550 11 10 10:10:10", "y", "\\u0E1E.\\u0E28. 2550",


        "th", "BE 2550 10 10 10:10:10", "BE 2550 11 10 10:10:10", "MMM", "\\u0E15.\\u0E04.\\u2013\\u0E1E.\\u0E22.",

        // Tests for Japanese calendar with eras, including new era in 2019 (Heisei 31 through April 30, then new era)

        "en-u-ca-japanese", "H 31 03 15 09:00:00", "H 31 04 15 09:00:00", "GyMMMd", "Mar 15 \\u2013 Apr 15, 31 Heisei",

        "en-u-ca-japanese", "H 31 03 15 09:00:00", "H 31 04 15 09:00:00", "GGGGGyMd", "3/15/31 \\u2013 4/15/31 H",

        "en-u-ca-japanese", "S 64 01 05 09:00:00", "H 1 01 15 09:00:00",  "GyMMMd", "Jan 5, 64 Sh\\u014Dwa \\u2013 Jan 15, 1 Heisei",

        "en-u-ca-japanese", "S 64 01 05 09:00:00", "H 1 01 15 09:00:00",  "GGGGGyMd", "1/5/64 S \\u2013 1/15/1 H",
 
        "en-u-ca-japanese", "H 31 04 15 09:00:00", JP_ERA_2019_NARROW " 1 05 15 09:00:00",  "GyMMMd", "Apr 15, 31 Heisei \\u2013 May 15, 1 " JP_ERA_2019_ROOT,

        "en-u-ca-japanese", "H 31 04 15 09:00:00", JP_ERA_2019_NARROW " 1 05 15 09:00:00",  "GGGGGyMd", "4/15/31 H \\u2013 5/15/1 " JP_ERA_2019_NARROW,
 
 
        "ja-u-ca-japanese", "H 31 03 15 09:00:00", "H 31 04 15 09:00:00", "GyMMMd", "\\u5E73\\u621031\\u5E743\\u670815\\u65E5\\uFF5E4\\u670815\\u65E5",

        "ja-u-ca-japanese", "H 31 03 15 09:00:00", "H 31 04 15 09:00:00", "GGGGGyMd", "H31/03/15\\uFF5E31/04/15",

        "ja-u-ca-japanese", "S 64 01 05 09:00:00", "H 1 01 15 09:00:00",  "GyMMMd", "\\u662D\\u548C64\\u5E741\\u67085\\u65E5\\uFF5E\\u5E73\\u6210\\u5143\\u5E741\\u670815\\u65E5",

        "ja-u-ca-japanese", "S 64 01 05 09:00:00", "H 1 01 15 09:00:00",  "GGGGGyMd", "S64/01/05\\uFF5EH1/01/15",

        "ja-u-ca-japanese", "H 31 04 15 09:00:00", JP_ERA_2019_NARROW " 1 05 15 09:00:00", "GGGGGyMd", "H31/04/15\\uFF5E" JP_ERA_2019_NARROW "1/05/15",

    };
    expect(DATA, UPRV_LENGTHOF(DATA));
}


void DateIntervalFormatTest::expect(const char** data, int32_t data_length) {
    int32_t i = 0;
    UErrorCode ec = U_ZERO_ERROR;
    UnicodeString str, str2;
    const char* pattern = data[i++];

    while (i<data_length) {
        const char* locName = data[i++];
        const char* datestr = data[i++];
        const char* datestr_2 = data[i++];

        Locale loc(locName);
        LocalPointer<Calendar> defCal(Calendar::createInstance(loc, ec));
        if (U_FAILURE(ec)) {
            dataerrln("Calendar::createInstance fails for loc %s with: %s", locName, u_errorName(ec));
            return;
        }
        const char* calType = defCal->getType();
 
        Locale refLoc("root");
        if (calType) {
            refLoc.setKeywordValue("calendar", calType, ec);
        }
        SimpleDateFormat ref(pattern, refLoc, ec);
        logln( "case %d, locale: %s\n", (i-1)/5, locName);
        if (U_FAILURE(ec)) {
            dataerrln("contruct SimpleDateFormat in expect failed: %s", u_errorName(ec));
            return;
        }

        // 'f'
        logln("original date: %s - %s\n", datestr, datestr_2);
        UDate date = ref.parse(ctou(datestr), ec);
        if (!assertSuccess("parse 1st data in expect", ec)) return;
        UDate date_2 = ref.parse(ctou(datestr_2), ec);
        if (!assertSuccess("parse 2nd data in expect", ec)) return;
        DateInterval dtitv(date, date_2);

        const UnicodeString& oneSkeleton(ctou(data[i++]));

        DateIntervalFormat* dtitvfmt = DateIntervalFormat::createInstance(oneSkeleton, loc, ec);
        if (!assertSuccess("createInstance(skeleton) in expect", ec)) return;
        FieldPosition pos(FieldPosition::DONT_CARE);
        dtitvfmt->format(&dtitv, str.remove(), pos, ec);
        if (!assertSuccess("format in expect", ec)) return;
        assertEquals((UnicodeString)"\"" + locName + "\\" + oneSkeleton + "\\" + ctou(datestr) + "\\" + ctou(datestr_2) + "\"", ctou(data[i++]), str);

        logln("interval date:" + str + "\"" + locName + "\", "
                 + "\"" + datestr + "\", "
              + "\"" + datestr_2 + "\", " + oneSkeleton);
        delete dtitvfmt;
    }
}


/*
 * Test format using user defined DateIntervalInfo
 */
void DateIntervalFormatTest::testFormatUserDII() {
    // first item is date pattern
    const char* DATA[] = {
        "yyyy MM dd HH:mm:ss",
        "en", "2007 10 10 10:10:10", "2008 10 10 10:10:10", "Oct 10, 2007 --- Oct 10, 2008",

        "en", "2007 10 10 10:10:10", "2007 11 10 10:10:10", "2007 Oct 10 - Nov 2007",

        "en", "2007 11 10 10:10:10", "2007 11 20 10:10:10", "Nov 10, 2007 --- Nov 20, 2007",

        "en", "2007 01 10 10:00:10", "2007 01 10 14:10:10", "Jan 10, 2007",

        "en", "2007 01 10 10:00:10", "2007 01 10 10:20:10", "Jan 10, 2007",

        "en", "2007 01 10 10:10:10", "2007 01 10 10:10:20", "Jan 10, 2007",

        "zh", "2007 10 10 10:10:10", "2008 10 10 10:10:10", "2007\\u5e7410\\u670810\\u65e5 --- 2008\\u5e7410\\u670810\\u65e5",

        "zh", "2007 10 10 10:10:10", "2007 11 10 10:10:10", "2007 10\\u6708 10 - 11\\u6708 2007",

        "zh", "2007 11 10 10:10:10", "2007 11 20 10:10:10", "2007\\u5e7411\\u670810\\u65e5 --- 2007\\u5e7411\\u670820\\u65e5",

        "zh", "2007 01 10 10:00:10", "2007 01 10 14:10:10", "2007\\u5e741\\u670810\\u65e5", // (fixed expected result per ticket:6626:)

        "zh", "2007 01 10 10:00:10", "2007 01 10 10:20:10", "2007\\u5e741\\u670810\\u65e5", // (fixed expected result per ticket:6626:)

        "zh", "2007 01 10 10:10:10", "2007 01 10 10:10:20", "2007\\u5e741\\u670810\\u65e5", // (fixed expected result per ticket:6626:)

        "de", "2007 10 10 10:10:10", "2008 10 10 10:10:10", "10. Okt. 2007 --- 10. Okt. 2008",


        "de", "2007 11 10 10:10:10", "2007 11 20 10:10:10", "10. Nov. 2007 --- 20. Nov. 2007",

        "de", "2007 01 10 10:00:10", "2007 01 10 14:10:10", "10. Jan. 2007",

        "de", "2007 01 10 10:00:10", "2007 01 10 10:20:10", "10. Jan. 2007",


        "es", "2007 10 10 10:10:10", "2008 10 10 10:10:10", "10 oct. 2007 --- 10 oct. 2008",

        "es", "2007 10 10 10:10:10", "2007 11 10 10:10:10", "2007 oct. 10 - nov. 2007",

        "es", "2007 11 10 10:10:10", "2007 11 20 10:10:10", "10 nov. 2007 --- 20 nov. 2007",

        "es", "2007 01 10 10:00:10", "2007 01 10 14:10:10", "10 ene. 2007",

        "es", "2007 01 10 10:00:10", "2007 01 10 10:20:10", "10 ene. 2007",

        "es", "2007 01 10 10:10:10", "2007 01 10 10:10:20", "10 ene. 2007",
    };
    expectUserDII(DATA, UPRV_LENGTHOF(DATA));
}


void DateIntervalFormatTest::testSetIntervalPatternNoSideEffect() {
    UErrorCode ec = U_ZERO_ERROR;
    LocalPointer<DateIntervalInfo> dtitvinf(new DateIntervalInfo(ec), ec);
    if (U_FAILURE(ec)) {
        errln("Failure encountered: %s", u_errorName(ec));
        return;
    }
    UnicodeString expected;
    dtitvinf->getIntervalPattern(ctou("yMd"), UCAL_DATE, expected, ec);
    dtitvinf->setIntervalPattern(ctou("yMd"), UCAL_DATE, ctou("M/d/y \\u2013 d"), ec);
    if (U_FAILURE(ec)) {
        errln("Failure encountered: %s", u_errorName(ec));
        return;
    }
    dtitvinf.adoptInsteadAndCheckErrorCode(new DateIntervalInfo(ec), ec);
    if (U_FAILURE(ec)) {
        errln("Failure encountered: %s", u_errorName(ec));
        return;
    }
    UnicodeString actual;
    dtitvinf->getIntervalPattern(ctou("yMd"), UCAL_DATE, actual, ec);
    if (U_FAILURE(ec)) {
        errln("Failure encountered: %s", u_errorName(ec));
        return;
    }
    if (expected != actual) {
        errln("DateIntervalInfo.setIntervalPattern should have no side effects.");
    }
}

void DateIntervalFormatTest::testYearFormats() {
    const Locale &enLocale = Locale::getEnglish();
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<Calendar> fromTime(Calendar::createInstance(enLocale, status));
    LocalPointer<Calendar> toTime(Calendar::createInstance(enLocale, status));
    if (U_FAILURE(status)) {
        errln("Failure encountered: %s", u_errorName(status));
        return;
    }
    // April 26, 113. Three digit year so that we can test 2 digit years;
    // 4 digit years with padded 0's and full years.
    fromTime->set(113, 3, 26);
    // April 28, 113.
    toTime->set(113, 3, 28);
    {
        LocalPointer<DateIntervalFormat> dif(DateIntervalFormat::createInstance("yyyyMd", enLocale, status));
        if (U_FAILURE(status)) {
            dataerrln("Failure encountered: %s", u_errorName(status));
            return;
        }
        UnicodeString actual;
        UnicodeString expected(ctou("4/26/0113 \\u2013 4/28/0113"));
        FieldPosition pos;
        dif->format(*fromTime, *toTime, actual, pos, status);
        if (U_FAILURE(status)) {
            errln("Failure encountered: %s", u_errorName(status));
            return;
        }
        if (actual != expected) {
            errln("Expected " + expected + ", got: " + actual);
        }
    }
    {
        LocalPointer<DateIntervalFormat> dif(DateIntervalFormat::createInstance("yyMd", enLocale, status));
        if (U_FAILURE(status)) {
            errln("Failure encountered: %s", u_errorName(status));
            return;
        }
        UnicodeString actual;
        UnicodeString expected(ctou("4/26/13 \\u2013 4/28/13"));
        FieldPosition pos(FieldPosition::DONT_CARE);
        dif->format(*fromTime, *toTime, actual, pos, status);
        if (U_FAILURE(status)) {
            errln("Failure encountered: %s", u_errorName(status));
            return;
        }
        if (actual != expected) {
            errln("Expected " + expected + ", got: " + actual);
        }
    }
    {
        LocalPointer<DateIntervalFormat> dif(DateIntervalFormat::createInstance("yMd", enLocale, status));
        if (U_FAILURE(status)) {
            errln("Failure encountered: %s", u_errorName(status));
            return;
        }
        UnicodeString actual;
        UnicodeString expected(ctou("4/26/113 \\u2013 4/28/113"));
        FieldPosition pos(FieldPosition::DONT_CARE);
        dif->format(*fromTime, *toTime, actual, pos, status);
        if (U_FAILURE(status)) {
            errln("Failure encountered: %s", u_errorName(status));
            return;
        }
        if (actual != expected) {
            errln("Expected " + expected + ", got: " + actual);
        }
    }
}

void DateIntervalFormatTest::expectUserDII(const char** data,
                                           int32_t data_length) {
    int32_t i = 0;
    UnicodeString str;
    UErrorCode ec = U_ZERO_ERROR;
    const char* pattern = data[0];
    i++;

    while ( i < data_length ) {
        const char* locName = data[i++];
        Locale loc(locName);
        SimpleDateFormat ref(pattern, loc, ec);
        if (U_FAILURE(ec)) {
            dataerrln("contruct SimpleDateFormat in expectUserDII failed: %s", u_errorName(ec));
            return;
        }
        const char* datestr = data[i++];
        const char* datestr_2 = data[i++];
        UDate date = ref.parse(ctou(datestr), ec);
        if (!assertSuccess("parse in expectUserDII", ec)) return;
        UDate date_2 = ref.parse(ctou(datestr_2), ec);
        if (!assertSuccess("parse in expectUserDII", ec)) return;
        DateInterval dtitv(date, date_2);

        ec = U_ZERO_ERROR;
        // test user created DateIntervalInfo
        DateIntervalInfo* dtitvinf = new DateIntervalInfo(ec);
        dtitvinf->setFallbackIntervalPattern("{0} --- {1}", ec);
        dtitvinf->setIntervalPattern(UDAT_YEAR_ABBR_MONTH_DAY, UCAL_MONTH, "yyyy MMM d - MMM y",ec);
        if (!assertSuccess("DateIntervalInfo::setIntervalPattern", ec)) return;
        dtitvinf->setIntervalPattern(UDAT_YEAR_ABBR_MONTH_DAY, UCAL_HOUR_OF_DAY, "yyyy MMM d HH:mm - HH:mm", ec);
        if (!assertSuccess("DateIntervalInfo::setIntervalPattern", ec)) return;
        DateIntervalFormat* dtitvfmt = DateIntervalFormat::createInstance(UDAT_YEAR_ABBR_MONTH_DAY, loc, *dtitvinf, ec);
        delete dtitvinf;
        if (!assertSuccess("createInstance(skeleton,dtitvinf) in expectUserDII", ec)) return;
        FieldPosition pos(FieldPosition::DONT_CARE);
        dtitvfmt->format(&dtitv, str.remove(), pos, ec);
        if (!assertSuccess("format in expectUserDII", ec)) return;
        assertEquals((UnicodeString)"\"" + locName + "\\" + datestr + "\\" + datestr_2 + "\"", ctou(data[i++]), str);
#ifdef DTIFMTTS_DEBUG
        char result[1000];
        char mesg[1000];
        PRINTMESG("interval format using user defined DateIntervalInfo\n");
        str.extract(0,  str.length(), result, "UTF-8");
        sprintf(mesg, "interval date: %s\n", result);
        PRINTMESG(mesg);
#endif
        delete dtitvfmt;
    }
}


void DateIntervalFormatTest::testStress() {
    if(quick){
    	logln("Quick mode: Skipping test");
    	return;
    }
	const char* DATA[] = {
        "yyyy MM dd HH:mm:ss",
        "2007 10 10 10:10:10", "2008 10 10 10:10:10",
        "2007 10 10 10:10:10", "2007 11 10 10:10:10",
        "2007 11 10 10:10:10", "2007 11 20 10:10:10",
        "2007 01 10 10:00:10", "2007 01 10 14:10:10",
        "2007 01 10 10:00:10", "2007 01 10 10:20:10",
        "2007 01 10 10:10:10", "2007 01 10 10:10:20",
    };

    const char* testLocale[][3] = {
        //{"th", "", ""},
        {"en", "", ""},
        {"zh", "", ""},
        {"de", "", ""},
        {"ar", "", ""},
        {"en", "GB",  ""},
        {"fr", "", ""},
        {"it", "", ""},
        {"nl", "", ""},
        {"zh", "TW",  ""},
        {"ja", "", ""},
        {"pt", "BR", ""},
        {"ru", "", ""},
        {"pl", "", ""},
        {"tr", "", ""},
        {"es", "", ""},
        {"ko", "", ""},
        {"sv", "", ""},
        {"fi", "", ""},
        {"da", "", ""},
        {"pt", "PT", ""},
        {"ro", "", ""},
        {"hu", "", ""},
        {"he", "", ""},
        {"in", "", ""},
        {"cs", "", ""},
        {"el", "", ""},
        {"no", "", ""},
        {"vi", "", ""},
        {"bg", "", ""},
        {"hr", "", ""},
        {"lt", "", ""},
        {"sk", "", ""},
        {"sl", "", ""},
        {"sr", "", ""},
        {"ca", "", ""},
        {"lv", "", ""},
        {"uk", "", ""},
        {"hi", "", ""},
    };

    uint32_t localeIndex;
    for ( localeIndex = 0; localeIndex < UPRV_LENGTHOF(testLocale); ++localeIndex ) {
        char locName[32];
        uprv_strcpy(locName, testLocale[localeIndex][0]);
        uprv_strcat(locName, testLocale[localeIndex][1]);
        stress(DATA, UPRV_LENGTHOF(DATA), Locale(testLocale[localeIndex][0], testLocale[localeIndex][1], testLocale[localeIndex][2]), locName);
    }
}


void DateIntervalFormatTest::stress(const char** data, int32_t data_length,
                                    const Locale& loc, const char* locName) {
    UnicodeString skeleton[] = {
        "EEEEdMMMMy",
        "dMMMMy",
        "dMMMM",
        "MMMMy",
        "EEEEdMMMM",
        "EdMMMy",
        "dMMMy",
        "dMMM",
        "MMMy",
        "EdMMM",
        "EdMy",
        "dMy",
        "dM",
        "My",
        "EdM",
        "d",
        "Ed",
        "y",
        "M",
        "MMM",
        "MMMM",
        "hm",
        "hmv",
        "hmz",
        "h",
        "hv",
        "hz",
        "EEddMMyyyy", // following could be normalized
        "EddMMy",
        "hhmm",
        "hhmmzz",
        "hms",  // following could not be normalized
        "dMMMMMy",
        "EEEEEdM",
    };

    int32_t i = 0;
    UErrorCode ec = U_ZERO_ERROR;
    UnicodeString str, str2;
    SimpleDateFormat ref(data[i++], loc, ec);
    if (!assertSuccess("construct SimpleDateFormat", ec)) return;

#ifdef DTIFMTTS_DEBUG
    char result[1000];
    char mesg[1000];
    sprintf(mesg, "locale: %s\n", locName);
    PRINTMESG(mesg);
#endif

    while (i<data_length) {

        // 'f'
        const char* datestr = data[i++];
        const char* datestr_2 = data[i++];
#ifdef DTIFMTTS_DEBUG
        sprintf(mesg, "original date: %s - %s\n", datestr, datestr_2);
        PRINTMESG(mesg)
#endif
        UDate date = ref.parse(ctou(datestr), ec);
        if (!assertSuccess("parse", ec)) return;
        UDate date_2 = ref.parse(ctou(datestr_2), ec);
        if (!assertSuccess("parse", ec)) return;
        DateInterval dtitv(date, date_2);

        for ( uint32_t skeletonIndex = 0;
              skeletonIndex < UPRV_LENGTHOF(skeleton);
              ++skeletonIndex ) {
            const UnicodeString& oneSkeleton = skeleton[skeletonIndex];
            DateIntervalFormat* dtitvfmt = DateIntervalFormat::createInstance(oneSkeleton, loc, ec);
            if (!assertSuccess("createInstance(skeleton)", ec)) return;
            /*
            // reset the calendar to be Gregorian calendar for "th"
            if ( uprv_strcmp(locName, "th") == 0 ) {
                GregorianCalendar* gregCal = new GregorianCalendar(loc, ec);
                if (!assertSuccess("GregorianCalendar()", ec)) return;
                const DateFormat* dformat = dtitvfmt->getDateFormat();
                DateFormat* newOne = (DateFormat*)dformat->clone();
                newOne->adoptCalendar(gregCal);
                //dtitvfmt->adoptDateFormat(newOne, ec);
                dtitvfmt->setDateFormat(*newOne, ec);
                delete newOne;
                if (!assertSuccess("adoptDateFormat()", ec)) return;
            }
            */
            FieldPosition pos(FieldPosition::DONT_CARE);
            dtitvfmt->format(&dtitv, str.remove(), pos, ec);
            if (!assertSuccess("format", ec)) return;
#ifdef DTIFMTTS_DEBUG
            oneSkeleton.extract(0,  oneSkeleton.length(), result, "UTF-8");
            sprintf(mesg, "interval by skeleton: %s\n", result);
            PRINTMESG(mesg)
            str.extract(0,  str.length(), result, "UTF-8");
            sprintf(mesg, "interval date: %s\n", result);
            PRINTMESG(mesg)
#endif
            delete dtitvfmt;
        }

        // test user created DateIntervalInfo
        ec = U_ZERO_ERROR;
        DateIntervalInfo* dtitvinf = new DateIntervalInfo(ec);
        dtitvinf->setFallbackIntervalPattern("{0} --- {1}", ec);
        dtitvinf->setIntervalPattern(UDAT_YEAR_ABBR_MONTH_DAY, UCAL_MONTH, "yyyy MMM d - MMM y",ec);
        if (!assertSuccess("DateIntervalInfo::setIntervalPattern", ec)) return;
        dtitvinf->setIntervalPattern(UDAT_YEAR_ABBR_MONTH_DAY, UCAL_HOUR_OF_DAY, "yyyy MMM d HH:mm - HH:mm", ec);
        if (!assertSuccess("DateIntervalInfo::setIntervalPattern", ec)) return;
        DateIntervalFormat* dtitvfmt = DateIntervalFormat::createInstance(UDAT_YEAR_ABBR_MONTH_DAY, loc, *dtitvinf, ec);
        delete dtitvinf;
        if (!assertSuccess("createInstance(skeleton,dtitvinf)", ec)) return;
        FieldPosition pos(FieldPosition::DONT_CARE);
        dtitvfmt->format(&dtitv, str.remove(), pos, ec);
        if ( uprv_strcmp(locName, "th") ) {
            if (!assertSuccess("format", ec)) return;
#ifdef DTIFMTTS_DEBUG
            PRINTMESG("interval format using user defined DateIntervalInfo\n");
            str.extract(0,  str.length(), result, "UTF-8");
            sprintf(mesg, "interval date: %s\n", result);
            PRINTMESG(mesg)
#endif
        } else {
            // for "th", the default calendar is Budhist,
            // not Gregorian.
            assertTrue("Default calendar for \"th\" is Budhist", ec == U_ILLEGAL_ARGUMENT_ERROR);
            ec = U_ZERO_ERROR;
        }
        delete dtitvfmt;
    }
}

void DateIntervalFormatTest::testTicket11583_2() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DateIntervalFormat> fmt(
            DateIntervalFormat::createInstance("yMMM", "es-US", status));
    if (!assertSuccess("Error create format object", status)) {
        return;
    }
    DateInterval interval((UDate) 1232364615000.0, (UDate) 1328787015000.0);
    UnicodeString appendTo;
    FieldPosition fpos(FieldPosition::DONT_CARE);
    UnicodeString expected("ene. de 2009 \\u2013 feb. de 2012");
    assertEquals(
            "",
            expected.unescape(),
            fmt->format(&interval, appendTo, fpos, status));
    if (!assertSuccess("Error formatting", status)) {
        return;
    }
}


void DateIntervalFormatTest::testTicket11985() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DateIntervalFormat> fmt(
            DateIntervalFormat::createInstance(UDAT_HOUR_MINUTE, Locale::getEnglish(), status));
    if (!assertSuccess("createInstance", status)) {
        return;
    }
    UnicodeString pattern;
    static_cast<const SimpleDateFormat*>(fmt->getDateFormat())->toPattern(pattern);
    assertEquals("Format pattern", "h:mm a", pattern);
}

// Ticket 11669 - thread safety of DateIntervalFormat::format(). This test failed before
//                the implementation was fixed.

static const DateIntervalFormat *gIntervalFormatter = NULL;      // The Formatter to be used concurrently by test threads.
static const DateInterval *gInterval = NULL;                     // The date interval to be formatted concurrently.
static const UnicodeString *gExpectedResult = NULL;

void DateIntervalFormatTest::threadFunc11669(int32_t threadNum) {
    (void)threadNum;
    for (int loop=0; loop<1000; ++loop) {
        UErrorCode status = U_ZERO_ERROR;
        FieldPosition pos(FieldPosition::DONT_CARE);
        UnicodeString result;
        gIntervalFormatter->format(gInterval, result, pos, status);
        if (U_FAILURE(status)) {
            errln("%s:%d %s", __FILE__, __LINE__, u_errorName(status));
            return;
        }
        if (result != *gExpectedResult) {
            errln("%s:%d Expected \"%s\", got \"%s\"", __FILE__, __LINE__, CStr(*gExpectedResult)(), CStr(result)());
            return;
        }
    }
}

void DateIntervalFormatTest::testTicket11669() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DateIntervalFormat> formatter(DateIntervalFormat::createInstance(UDAT_YEAR_MONTH_DAY, Locale::getEnglish(), status), status);
    LocalPointer<TimeZone> tz(TimeZone::createTimeZone("America/Los_Angleles"), status);
    LocalPointer<Calendar> intervalStart(Calendar::createInstance(*tz, Locale::getEnglish(), status), status);
    LocalPointer<Calendar> intervalEnd(Calendar::createInstance(*tz, Locale::getEnglish(), status), status);
    if (U_FAILURE(status)) {
        errcheckln(status, "%s:%d %s", __FILE__, __LINE__, u_errorName(status));
        return;
    }

    intervalStart->set(2009, 6, 1, 14, 0);
    intervalEnd->set(2009, 6, 2, 14, 0);
    DateInterval interval(intervalStart->getTime(status), intervalEnd->getTime(status));
    FieldPosition pos(FieldPosition::DONT_CARE);
    UnicodeString expectedResult;
    formatter->format(&interval, expectedResult, pos, status);
    if (U_FAILURE(status)) {
        errln("%s:%d %s", __FILE__, __LINE__, u_errorName(status));
        return;
    }

    gInterval = &interval;
    gIntervalFormatter = formatter.getAlias();
    gExpectedResult = &expectedResult;

    ThreadPool<DateIntervalFormatTest> threads(this, 4, &DateIntervalFormatTest::threadFunc11669);
    threads.start();
    threads.join();

    gInterval = NULL;             // Don't leave dangling pointers lying around. Not strictly necessary.
    gIntervalFormatter = NULL;
    gExpectedResult = NULL;
}


// testTicket12065
//    Using a DateIntervalFormat to format shouldn't change its state in any way
//    that changes how the behavior of operator ==.
void DateIntervalFormatTest::testTicket12065() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<DateIntervalFormat> formatter(DateIntervalFormat::createInstance(UDAT_YEAR_MONTH_DAY, Locale::getEnglish(), status), status);
    if (formatter.isNull()) {
        dataerrln("FAIL: DateIntervalFormat::createInstance failed for Locale::getEnglish()");
        return;
    }
    LocalPointer<DateIntervalFormat> clone(dynamic_cast<DateIntervalFormat *>(formatter->clone()));
    if (*formatter != *clone) {
        errln("%s:%d DateIntervalFormat and clone are not equal.", __FILE__, __LINE__);
        return;
    }
    DateInterval interval((UDate) 1232364615000.0, (UDate) 1328787015000.0);
    UnicodeString appendTo;
    FieldPosition fpos(FieldPosition::DONT_CARE);
    formatter->format(&interval, appendTo, fpos, status);
    if (*formatter != *clone) {
        errln("%s:%d DateIntervalFormat and clone are not equal after formatting.", __FILE__, __LINE__);
        return;
    }
    if (U_FAILURE(status)) {
        errln("%s:%d %s", __FILE__, __LINE__, u_errorName(status));
    }
}


void DateIntervalFormatTest::testFormattedDateInterval() {
    IcuTestErrorCode status(*this, "testFormattedDateInterval");
    LocalPointer<DateIntervalFormat> fmt(DateIntervalFormat::createInstance(u"dMMMMy", "en-US", status), status);

    {
        const char16_t* message = u"FormattedDateInterval test 1";
        const char16_t* expectedString = u"July 20 \u2013 25, 2018";
        LocalPointer<Calendar> input1(Calendar::createInstance("en-GB", status));
        if (status.errIfFailureAndReset()) { return; }
        LocalPointer<Calendar> input2(Calendar::createInstance("en-GB", status));
        if (status.errIfFailureAndReset()) { return; }
        input1->set(2018, 6, 20);
        input2->set(2018, 6, 25);
        FormattedDateInterval result = fmt->formatToValue(*input1, *input2, status);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // field, begin index, end index
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 0, 4},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 0, 5, 7},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 5, 7},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 1, 10, 12},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 10, 12},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 14, 18}};
        checkMixedFormattedValue(
            message,
            result,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }

    // To test the fallback pattern behavior, make a custom DateIntervalInfo.
    DateIntervalInfo dtitvinf(status);
    dtitvinf.setFallbackIntervalPattern("<< {1} --- {0} >>", status);
    fmt.adoptInsteadAndCheckErrorCode(
        DateIntervalFormat::createInstance(u"dMMMMy", "en-US", dtitvinf, status),
        status);

    {
        const char16_t* message = u"FormattedDateInterval with fallback format test 1";
        const char16_t* expectedString = u"<< July 25, 2018 --- July 20, 2018 >>";
        LocalPointer<Calendar> input1(Calendar::createInstance("en-GB", status));
        if (status.errIfFailureAndReset()) { return; }
        LocalPointer<Calendar> input2(Calendar::createInstance("en-GB", status));
        if (status.errIfFailureAndReset()) { return; }
        input1->set(2018, 6, 20);
        input2->set(2018, 6, 25);
        FormattedDateInterval result = fmt->formatToValue(*input1, *input2, status);
        static const UFieldPositionWithCategory expectedFieldPositions[] = {
            // field, begin index, end index
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 1, 3, 16},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 3, 7},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 8, 10},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 12, 16},
            {UFIELD_CATEGORY_DATE_INTERVAL_SPAN, 0, 21, 34},
            {UFIELD_CATEGORY_DATE, UDAT_MONTH_FIELD, 21, 25},
            {UFIELD_CATEGORY_DATE, UDAT_DATE_FIELD, 26, 28},
            {UFIELD_CATEGORY_DATE, UDAT_YEAR_FIELD, 30, 34}};
        checkMixedFormattedValue(
            message,
            result,
            expectedString,
            expectedFieldPositions,
            UPRV_LENGTHOF(expectedFieldPositions));
    }
}


#endif /* #if !UCONFIG_NO_FORMATTING */
