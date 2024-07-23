// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/********************************************************************
 * Copyright (c) 2008-2016, International Business Machines Corporation and
 * others. All Rights Reserved.
 ********************************************************************/

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/decimfmt.h"
#include "unicode/tmunit.h"
#include "unicode/tmutamt.h"
#include "unicode/tmutfmt.h"
#include "unicode/ustring.h"
#include "cmemory.h"
#include "intltest.h"

//TODO: put as compilation flag
//#define TUFMTTS_DEBUG 1

#ifdef TUFMTTS_DEBUG
#include <iostream>
#endif

class TimeUnitTest : public IntlTest {
    void runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/ ) {
        if (exec) logln("TestSuite TimeUnitTest");
        TESTCASE_AUTO_BEGIN;
        TESTCASE_AUTO(testBasic);
        TESTCASE_AUTO(testAPI);
        TESTCASE_AUTO(testGreekWithFallback);
        TESTCASE_AUTO(testGreekWithSanitization);
        TESTCASE_AUTO(test10219Plurals);
        TESTCASE_AUTO(TestBritishShortHourFallback);
        TESTCASE_AUTO_END;
    }

public:
    /**
     * Performs basic tests
     **/
    void testBasic();

    /**
     * Performs API tests
     **/
    void testAPI();

    /**
     * Performs tests for Greek
     * This tests that requests for short unit names correctly fall back 
     * to long unit names for a locale where the locale data does not 
     * provide short unit names. As of CLDR 1.9, Greek is one such language.
     **/
    void testGreekWithFallback();

    /**
     * Performs tests for Greek
     * This tests that if the plural count listed in time unit format does not
     * match those in the plural rules for the locale, those plural count in
     * time unit format will be ingored and subsequently, fall back will kick in
     * which is tested above.
     * Without data sanitization, setNumberFormat() would crash.
     * As of CLDR shiped in ICU4.8, Greek is one such language.
     */
    void testGreekWithSanitization();

    /**
     * Performs unit test for ticket 10219 making sure that plurals work
     * correctly with rounding.
     */
    void test10219Plurals();

    void TestBritishShortHourFallback();
};

extern IntlTest *createTimeUnitTest() {
    return new TimeUnitTest();
}

// This function is more lenient than equals operator as it considers integer 3 hours and
// double 3.0 hours to be equal
static UBool tmaEqual(const TimeUnitAmount& left, const TimeUnitAmount& right) {
    if (left.getTimeUnitField() != right.getTimeUnitField()) {
        return FALSE;
    }
    UErrorCode status = U_ZERO_ERROR;
    if (!left.getNumber().isNumeric() || !right.getNumber().isNumeric()) {
        return FALSE;
    }
    UBool result = left.getNumber().getDouble(status) == right.getNumber().getDouble(status);
    if (U_FAILURE(status)) {
        return FALSE;
    }
    return result;
}

/**
 * Test basic
 */
void TimeUnitTest::testBasic() {
    const char* locales[] = {"en", "sl", "fr", "zh", "ar", "ru", "zh_Hant", "pa"};
    for ( unsigned int locIndex = 0; 
          locIndex < UPRV_LENGTHOF(locales); 
          ++locIndex ) {
        UErrorCode status = U_ZERO_ERROR;
        Locale loc(locales[locIndex]);
        TimeUnitFormat** formats = new TimeUnitFormat*[2];
        formats[UTMUTFMT_FULL_STYLE] = new TimeUnitFormat(loc, status);
        if (!assertSuccess("TimeUnitFormat(full)", status, TRUE)) return;
        formats[UTMUTFMT_ABBREVIATED_STYLE] = new TimeUnitFormat(loc, UTMUTFMT_ABBREVIATED_STYLE, status);
        if (!assertSuccess("TimeUnitFormat(short)", status)) return;
#ifdef TUFMTTS_DEBUG
        std::cout << "locale: " << locales[locIndex] << "\n";
#endif
        for (int style = UTMUTFMT_FULL_STYLE; 
             style <= UTMUTFMT_ABBREVIATED_STYLE;
             ++style) {
          for (TimeUnit::UTimeUnitFields j = TimeUnit::UTIMEUNIT_YEAR; 
             j < TimeUnit::UTIMEUNIT_FIELD_COUNT; 
             j = (TimeUnit::UTimeUnitFields)(j+1)) {
#ifdef TUFMTTS_DEBUG
            std::cout << "time unit: " << j << "\n";
#endif
            double tests[] = {0, 0.5, 1, 1.5, 2, 2.5, 3, 3.5, 5, 10, 100, 101.35};
            for (unsigned int i = 0; i < UPRV_LENGTHOF(tests); ++i) {
#ifdef TUFMTTS_DEBUG
                std::cout << "number: " << tests[i] << "\n";
#endif
                TimeUnitAmount* source = new TimeUnitAmount(tests[i], j, status);
                if (!assertSuccess("TimeUnitAmount()", status)) return;
                UnicodeString formatted;
                Formattable formattable;
                formattable.adoptObject(source);
                formatted = ((Format*)formats[style])->format(formattable, formatted, status);
                if (!assertSuccess("format()", status)) return;
#ifdef TUFMTTS_DEBUG
                char formatResult[1000];
                formatted.extract(0, formatted.length(), formatResult, "UTF-8");
                std::cout << "format result: " << formatResult << "\n";
#endif
                Formattable result;
                ((Format*)formats[style])->parseObject(formatted, result, status);
                if (!assertSuccess("parseObject()", status)) return;
                if (!tmaEqual(*((TimeUnitAmount *)result.getObject()), *((TimeUnitAmount *) formattable.getObject()))) {
                    dataerrln("No round trip: ");
                }
                // other style parsing
                Formattable result_1;
                ((Format*)formats[1-style])->parseObject(formatted, result_1, status);
                if (!assertSuccess("parseObject()", status)) return;
                if (!tmaEqual(*((TimeUnitAmount *)result_1.getObject()), *((TimeUnitAmount *) formattable.getObject()))) {
                    dataerrln("No round trip: ");
                }
            }
          }
        }
        delete formats[UTMUTFMT_FULL_STYLE];
        delete formats[UTMUTFMT_ABBREVIATED_STYLE];
        delete[] formats;
    }
}


void TimeUnitTest::testAPI() {
    //================= TimeUnit =================
    UErrorCode status = U_ZERO_ERROR;

    TimeUnit* tmunit = TimeUnit::createInstance(TimeUnit::UTIMEUNIT_YEAR, status);
    if (!assertSuccess("TimeUnit::createInstance", status)) return;

    TimeUnit* another = (TimeUnit*)tmunit->clone();
    TimeUnit third(*tmunit);
    TimeUnit fourth = third;

    assertTrue("orig and clone are equal", (*tmunit == *another));
    assertTrue("copied and assigned are equal", (third == fourth));

    TimeUnit* tmunit_m = TimeUnit::createInstance(TimeUnit::UTIMEUNIT_MONTH, status);
    assertTrue("year != month", (*tmunit != *tmunit_m));

    TimeUnit::UTimeUnitFields field = tmunit_m->getTimeUnitField();
    assertTrue("field of month time unit is month", (field == TimeUnit::UTIMEUNIT_MONTH));

    //===== Interoperability with MeasureUnit ======
    MeasureUnit **ptrs = new MeasureUnit *[TimeUnit::UTIMEUNIT_FIELD_COUNT];

    ptrs[TimeUnit::UTIMEUNIT_YEAR] = MeasureUnit::createYear(status);
    ptrs[TimeUnit::UTIMEUNIT_MONTH] = MeasureUnit::createMonth(status);
    ptrs[TimeUnit::UTIMEUNIT_DAY] = MeasureUnit::createDay(status);
    ptrs[TimeUnit::UTIMEUNIT_WEEK] = MeasureUnit::createWeek(status);
    ptrs[TimeUnit::UTIMEUNIT_HOUR] = MeasureUnit::createHour(status);
    ptrs[TimeUnit::UTIMEUNIT_MINUTE] = MeasureUnit::createMinute(status);
    ptrs[TimeUnit::UTIMEUNIT_SECOND] = MeasureUnit::createSecond(status);
    if (!assertSuccess("TimeUnit::createInstance", status)) return;

    for (TimeUnit::UTimeUnitFields j = TimeUnit::UTIMEUNIT_YEAR; 
            j < TimeUnit::UTIMEUNIT_FIELD_COUNT; 
            j = (TimeUnit::UTimeUnitFields)(j+1)) {
        MeasureUnit *ptr = TimeUnit::createInstance(j, status);
        if (!assertSuccess("TimeUnit::createInstance", status)) return;
        // We have to convert *ptr to a MeasureUnit or else == will fail over
        // differing types (TimeUnit vs. MeasureUnit).
        assertTrue(
                "Time unit should be equal to corresponding MeasureUnit",
                MeasureUnit(*ptr) == *ptrs[j]);
        delete ptr;
    }
    delete tmunit;
    delete another;
    delete tmunit_m;
    for (int i = 0; i < TimeUnit::UTIMEUNIT_FIELD_COUNT; ++i) {
        delete ptrs[i];
    }
    delete [] ptrs;

    //
    //================= TimeUnitAmount =================

    Formattable formattable((int32_t)2);
    TimeUnitAmount tma_long(formattable, TimeUnit::UTIMEUNIT_DAY, status);
    if (!assertSuccess("TimeUnitAmount(formattable...)", status)) return;

    formattable.setDouble(2);
    TimeUnitAmount tma_double(formattable, TimeUnit::UTIMEUNIT_DAY, status);
    if (!assertSuccess("TimeUnitAmount(formattable...)", status)) return;

    formattable.setDouble(3);
    TimeUnitAmount tma_double_3(formattable, TimeUnit::UTIMEUNIT_DAY, status);
    if (!assertSuccess("TimeUnitAmount(formattable...)", status)) return;

    TimeUnitAmount tma(2, TimeUnit::UTIMEUNIT_DAY, status);
    if (!assertSuccess("TimeUnitAmount(number...)", status)) return;

    TimeUnitAmount tma_h(2, TimeUnit::UTIMEUNIT_HOUR, status);
    if (!assertSuccess("TimeUnitAmount(number...)", status)) return;

    TimeUnitAmount second(tma);
    TimeUnitAmount third_tma = tma;
    TimeUnitAmount* fourth_tma = (TimeUnitAmount*)tma.clone();

    assertTrue("orig and copy are equal", (second == tma));
    assertTrue("clone and assigned are equal", (third_tma == *fourth_tma));
    assertTrue("different if number diff", (tma_double != tma_double_3));
    assertTrue("different if number type diff", (tma_double != tma_long));
    assertTrue("different if time unit diff", (tma != tma_h));
    assertTrue("same even different constructor", (tma_double == tma));

    assertTrue("getTimeUnitField", (tma.getTimeUnitField() == TimeUnit::UTIMEUNIT_DAY));
    delete fourth_tma;
    //
    //================= TimeUnitFormat =================
    //
    TimeUnitFormat* tmf_en = new TimeUnitFormat(Locale("en"), status);
    if (!assertSuccess("TimeUnitFormat(en...)", status, TRUE)) return;
    TimeUnitFormat tmf_fr(Locale("fr"), status);
    if (!assertSuccess("TimeUnitFormat(fr...)", status)) return;

    assertTrue("TimeUnitFormat: en and fr diff", (*tmf_en != tmf_fr));

    TimeUnitFormat tmf_assign = *tmf_en;
    assertTrue("TimeUnitFormat: orig and assign are equal", (*tmf_en == tmf_assign));

    TimeUnitFormat tmf_copy(tmf_fr);
    assertTrue("TimeUnitFormat: orig and copy are equal", (tmf_fr == tmf_copy));

    TimeUnitFormat* tmf_clone = (TimeUnitFormat*)tmf_en->clone();
    assertTrue("TimeUnitFormat: orig and clone are equal", (*tmf_en == *tmf_clone));
    delete tmf_clone;

    tmf_en->setLocale(Locale("fr"), status);
    if (!assertSuccess("setLocale(fr...)", status)) return;

    NumberFormat* numberFmt = NumberFormat::createInstance(
                                 Locale("fr"), status);
    if (!assertSuccess("NumberFormat::createInstance()", status)) return;
    tmf_en->setNumberFormat(*numberFmt, status);
    if (!assertSuccess("setNumberFormat(en...)", status)) return;
    assertTrue("TimeUnitFormat: setLocale", (*tmf_en == tmf_fr));

    delete tmf_en;

    TimeUnitFormat* en_long = new TimeUnitFormat(Locale("en"), UTMUTFMT_FULL_STYLE, status);
    if (!assertSuccess("TimeUnitFormat(en...)", status)) return;
    delete en_long;

    TimeUnitFormat* en_short = new TimeUnitFormat(Locale("en"), UTMUTFMT_ABBREVIATED_STYLE, status);
    if (!assertSuccess("TimeUnitFormat(en...)", status)) return;
    delete en_short;

    TimeUnitFormat* format = new TimeUnitFormat(status);
    format->setLocale(Locale("zh"), status);
    format->setNumberFormat(*numberFmt, status);
    if (!assertSuccess("TimeUnitFormat(en...)", status)) return;
    delete numberFmt;
    delete format;
}

/* @bug 7902
 * Tests for Greek Language.
 * This tests that requests for short unit names correctly fall back 
 * to long unit names for a locale where the locale data does not 
 * provide short unit names. As of CLDR 1.9, Greek is one such language.
 */
void TimeUnitTest::testGreekWithFallback() {
    UErrorCode status = U_ZERO_ERROR;

    const char* locales[] = {"el-GR", "el"};
    TimeUnit::UTimeUnitFields tunits[] = {TimeUnit::UTIMEUNIT_SECOND, TimeUnit::UTIMEUNIT_MINUTE, TimeUnit::UTIMEUNIT_HOUR, TimeUnit::UTIMEUNIT_DAY, TimeUnit::UTIMEUNIT_MONTH, TimeUnit::UTIMEUNIT_YEAR};
    UTimeUnitFormatStyle styles[] = {UTMUTFMT_FULL_STYLE, UTMUTFMT_ABBREVIATED_STYLE};
    const int numbers[] = {1, 7};

    const UChar oneSecond[] = {0x0031, 0x0020, 0x03b4, 0x03b5, 0x03c5, 0x03c4, 0x03b5, 0x03c1, 0x03cc, 0x03bb, 0x03b5, 0x03c0, 0x03c4, 0x03bf, 0};
    const UChar oneSecondShort[] = {0x0031, 0x0020, 0x03b4, 0x03b5, 0x03c5, 0x03c4, 0x002e, 0};
    const UChar oneMinute[] = {0x0031, 0x0020, 0x03bb, 0x03b5, 0x03c0, 0x03c4, 0x03cc, 0};
    const UChar oneMinuteShort[] = {0x0031, 0x0020, 0x03bb, 0x03b5, 0x03c0, 0x002e, 0};
    const UChar oneHour[] = {0x0031, 0x0020, 0x03ce, 0x03c1, 0x03b1, 0};
    const UChar oneDay[] = {0x0031, 0x0020, 0x03b7, 0x03bc, 0x03ad, 0x03c1, 0x03b1, 0};
    const UChar oneMonth[] = {0x0031, 0x0020, 0x03bc, 0x03ae, 0x03bd, 0x03b1, 0x03c2, 0};
    const UChar oneMonthShort[] = {0x0031, 0x0020, 0x03bc, 0x03ae, 0x03bd, 0x002e, 0};
    const UChar oneYear[] = {0x0031, 0x0020, 0x03ad, 0x03c4, 0x03bf, 0x03c2, 0};
    const UChar oneYearShort[] = {0x0031, 0x0020, 0x03ad, 0x03c4, 0x002e, 0};
    const UChar sevenSeconds[] = {0x0037, 0x0020, 0x03b4, 0x03b5, 0x03c5, 0x03c4, 0x03b5, 0x03c1, 0x03cc, 0x03bb, 0x03b5, 0x03c0, 0x03c4, 0x03b1, 0};
    const UChar sevenSecondsShort[] = {0x0037, 0x0020, 0x03b4, 0x03b5, 0x03c5, 0x03c4, 0x002e, 0};
    const UChar sevenMinutes[] = {0x0037, 0x0020, 0x03bb, 0x03b5, 0x03c0, 0x03c4, 0x03ac, 0};
    const UChar sevenMinutesShort[] = {0x0037, 0x0020, 0x03bb, 0x03b5, 0x03c0, 0x002e, 0};
    const UChar sevenHours[] = {0x0037, 0x0020, 0x03ce, 0x03c1, 0x03b5, 0x03c2, 0};
    const UChar sevenHoursShort[] = {0x0037, 0x0020, 0x03ce, 0x03c1, 0x002e, 0};
    const UChar sevenDays[] = {0x0037, 0x0020, 0x03b7, 0x03bc, 0x03ad, 0x03c1, 0x03b5, 0x03c2, 0};
    const UChar sevenMonths[] = {0x0037, 0x0020, 0x03bc, 0x03ae, 0x03bd, 0x03b5, 0x3c2, 0};
    const UChar sevenMonthsShort[] = {0x0037, 0x0020, 0x03bc, 0x03ae, 0x03bd, 0x002e, 0};
    const UChar sevenYears[] = {0x0037, 0x0020, 0x03ad, 0x03c4, 0x03b7, 0};
    const UChar sevenYearsShort[] = {0x0037, 0x0020, 0x03ad, 0x03c4, 0x002e, 0};

    const UnicodeString oneSecondStr(oneSecond);
    const UnicodeString oneSecondShortStr(oneSecondShort);
    const UnicodeString oneMinuteStr(oneMinute);
    const UnicodeString oneMinuteShortStr(oneMinuteShort);
    const UnicodeString oneHourStr(oneHour);
    const UnicodeString oneDayStr(oneDay);
    const UnicodeString oneMonthStr(oneMonth);
    const UnicodeString oneMonthShortStr(oneMonthShort);
    const UnicodeString oneYearStr(oneYear);
    const UnicodeString oneYearShortStr(oneYearShort);
    const UnicodeString sevenSecondsStr(sevenSeconds);
    const UnicodeString sevenSecondsShortStr(sevenSecondsShort);
    const UnicodeString sevenMinutesStr(sevenMinutes);
    const UnicodeString sevenMinutesShortStr(sevenMinutesShort);
    const UnicodeString sevenHoursStr(sevenHours);
    const UnicodeString sevenHoursShortStr(sevenHoursShort);
    const UnicodeString sevenDaysStr(sevenDays);
    const UnicodeString sevenMonthsStr(sevenMonths);
    const UnicodeString sevenMonthsShortStr(sevenMonthsShort);
    const UnicodeString sevenYearsStr(sevenYears);
    const UnicodeString sevenYearsShortStr(sevenYearsShort);

    const UnicodeString expected[] = {
            oneSecondStr, oneMinuteStr, oneHourStr, oneDayStr, oneMonthStr, oneYearStr,
            oneSecondShortStr, oneMinuteShortStr, oneHourStr, oneDayStr, oneMonthShortStr, oneYearShortStr,
            sevenSecondsStr, sevenMinutesStr, sevenHoursStr, sevenDaysStr, sevenMonthsStr, sevenYearsStr,
            sevenSecondsShortStr, sevenMinutesShortStr, sevenHoursShortStr, sevenDaysStr, sevenMonthsShortStr, sevenYearsShortStr,

            oneSecondStr, oneMinuteStr, oneHourStr, oneDayStr, oneMonthStr, oneYearStr,
            oneSecondShortStr, oneMinuteShortStr, oneHourStr, oneDayStr, oneMonthShortStr, oneYearShortStr,
            sevenSecondsStr, sevenMinutesStr, sevenHoursStr, sevenDaysStr, sevenMonthsStr, sevenYearsStr,
            sevenSecondsShortStr, sevenMinutesShortStr, sevenHoursShortStr, sevenDaysStr, sevenMonthsShortStr, sevenYearsShortStr};

    int counter = 0;
    for ( unsigned int locIndex = 0;
        locIndex < UPRV_LENGTHOF(locales);
        ++locIndex ) {

        Locale l = Locale::createFromName(locales[locIndex]);

        for ( unsigned int numberIndex = 0;
            numberIndex < UPRV_LENGTHOF(numbers);
            ++numberIndex ) {

            for ( unsigned int styleIndex = 0;
                styleIndex < UPRV_LENGTHOF(styles);
                ++styleIndex ) {

                for ( unsigned int unitIndex = 0;
                    unitIndex < UPRV_LENGTHOF(tunits);
                    ++unitIndex ) {

                    TimeUnitAmount *tamt = new TimeUnitAmount(numbers[numberIndex], tunits[unitIndex], status);
                    if (U_FAILURE(status)) {
                        dataerrln("generating TimeUnitAmount Object failed.");
#ifdef TUFMTTS_DEBUG
                        std::cout << "Failed to get TimeUnitAmount for " << tunits[unitIndex] << "\n";
#endif
                        return;
                    }

                    TimeUnitFormat *tfmt = new TimeUnitFormat(l, styles[styleIndex], status);
                    if (U_FAILURE(status)) {
                        dataerrln("generating TimeUnitAmount Object failed.");
#ifdef TUFMTTS_DEBUG
                       std::cout <<  "Failed to get TimeUnitFormat for " << locales[locIndex] << "\n";
#endif
                       return;
                    }

                    Formattable fmt;
                    UnicodeString str;

                    fmt.adoptObject(tamt);
                    str = ((Format *)tfmt)->format(fmt, str, status);
                    if (!assertSuccess("formatting relative time failed", status)) {
                        delete tfmt;
#ifdef TUFMTTS_DEBUG
                        std::cout <<  "Failed to format" << "\n";
#endif
                        return;
                    }

#ifdef TUFMTTS_DEBUG
                    char tmp[128];    //output
                    char tmp1[128];    //expected
                    int len = 0;
                    u_strToUTF8(tmp, 128, &len, str.getTerminatedBuffer(), str.length(), &status);
                    u_strToUTF8(tmp1, 128, &len, expected[counter].unescape().getTerminatedBuffer(), expected[counter].unescape().length(), &status);
                    std::cout <<  "Formatted string : " << tmp << " expected : " << tmp1 << "\n";
#endif
                    if (!assertEquals("formatted time string is not expected, locale: " + UnicodeString(locales[locIndex]) + " style: " + (int)styles[styleIndex] + " units: " + (int)tunits[unitIndex], expected[counter], str)) {
                        delete tfmt;
                        str.remove();
                        return;
                    }
                    delete tfmt;
                    str.remove();
                    ++counter;
                }
            }
        }
    }
}

// Test bug9042
void TimeUnitTest::testGreekWithSanitization() {
    
    UErrorCode status = U_ZERO_ERROR;
    Locale elLoc("el");
    NumberFormat* numberFmt = NumberFormat::createInstance(Locale("el"), status);
    if (!assertSuccess("NumberFormat::createInstance for el locale", status, TRUE)) return;
    numberFmt->setMaximumFractionDigits(1);

    TimeUnitFormat* timeUnitFormat = new TimeUnitFormat(elLoc, status);
    if (!assertSuccess("TimeUnitFormat::TimeUnitFormat for el locale", status)) return;

    timeUnitFormat->setNumberFormat(*numberFmt, status);

    delete numberFmt;
    delete timeUnitFormat;
}

void TimeUnitTest::test10219Plurals() {
    Locale usLocale("en_US");
    double values[2] = {1.588, 1.011};
    UnicodeString expected[2][3] = {
        {"1 minute", "1.5 minutes", "1.58 minutes"},
        {"1 minute", "1.0 minutes", "1.01 minutes"}
    };
    UErrorCode status = U_ZERO_ERROR;
    TimeUnitFormat tuf(usLocale, status);
    if (U_FAILURE(status)) {
        dataerrln("generating TimeUnitFormat Object failed: %s", u_errorName(status));
        return;
    }
    LocalPointer<DecimalFormat> nf((DecimalFormat *) NumberFormat::createInstance(usLocale, status));
    if (U_FAILURE(status)) {
        dataerrln("generating NumberFormat Object failed: %s", u_errorName(status));
        return;
    }
    for (int32_t j = 0; j < UPRV_LENGTHOF(values); ++j) {
        for (int32_t i = 0; i < UPRV_LENGTHOF(expected[j]); ++i) {
            nf->setMinimumFractionDigits(i);
            nf->setMaximumFractionDigits(i);
            nf->setRoundingMode(DecimalFormat::kRoundDown);
            tuf.setNumberFormat(*nf, status);
            if (U_FAILURE(status)) {
                dataerrln("setting NumberFormat failed: %s", u_errorName(status));
                return;
            }
            UnicodeString actual;
            Formattable fmt;
            LocalPointer<TimeUnitAmount> tamt(
                new TimeUnitAmount(values[j], TimeUnit::UTIMEUNIT_MINUTE, status), status);
            if (U_FAILURE(status)) {
                dataerrln("generating TimeUnitAmount Object failed: %s", u_errorName(status));
                return;
            }
            fmt.adoptObject(tamt.orphan());
            tuf.format(fmt, actual, status);
            if (U_FAILURE(status)) {
                dataerrln("Actual formatting failed: %s", u_errorName(status));
                return;
            }
            if (expected[j][i] != actual) {
                errln("Expected " + expected[j][i] + ", got " + actual);
            }
        }
    }

    // test parsing
    Formattable result;
    ParsePosition pos;
    UnicodeString formattedString = "1 minutes";
    tuf.parseObject(formattedString, result, pos);
    if (formattedString.length() != pos.getIndex()) {
        errln("Expect parsing to go all the way to the end of the string.");
    }
}

void TimeUnitTest::TestBritishShortHourFallback() {
    // See ticket #11986 "incomplete fallback in MeasureFormat".
    UErrorCode status = U_ZERO_ERROR;
    Formattable oneHour(new TimeUnitAmount(1, TimeUnit::UTIMEUNIT_HOUR, status));
    Locale en_GB("en_GB");
    TimeUnitFormat formatter(en_GB, UTMUTFMT_ABBREVIATED_STYLE, status);
    UnicodeString result;
    formatter.format(oneHour, result, status);
    assertSuccess("TestBritishShortHourFallback()", status);
    assertEquals("TestBritishShortHourFallback()", UNICODE_STRING_SIMPLE("1 hr"), result, TRUE);
}

#endif
