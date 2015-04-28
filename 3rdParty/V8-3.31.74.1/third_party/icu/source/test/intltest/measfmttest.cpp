/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File MEASFMTTEST.CPP
*
*******************************************************************************
*/
#include <stdio.h>
#include <stdlib.h>

#include "intltest.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/decimfmt.h"
#include "unicode/measfmt.h"
#include "unicode/measure.h"
#include "unicode/measunit.h"
#include "unicode/tmunit.h"
#include "unicode/plurrule.h"
#include "charstr.h"
#include "unicode/reldatefmt.h"

struct ExpectedResult {
    const Measure *measures;
    int32_t count;
    const char *expected;
};

class MeasureFormatTest : public IntlTest {
public:
    MeasureFormatTest() {
    }

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void TestBasic();
    void TestCompatible53_1();
    void TestCompatible54_1();
    void TestGetAvailable();
    void TestExamplesInDocs();
    void TestFormatPeriodEn();
    void Test10219FractionalPlurals();
    void TestGreek();
    void TestFormatSingleArg();
    void TestFormatMeasuresZeroArg();
    void TestMultiplesWithPer();
    void TestSimplePer();
    void TestNumeratorPlurals();
    void TestMultiples();
    void TestGram();
    void TestCurrencies();
    void TestFieldPosition();
    void TestFieldPositionMultiple();
    void TestFieldPositionMultipleWithPer();
    void TestBadArg();
    void TestEquality();
    void TestGroupingSeparator();
    void TestDoubleZero();
    void verifyFormat(
        const char *description,
        const MeasureFormat &fmt,
        const Measure *measures,
        int32_t measureCount,
        const char *expected);
    void verifyFormatWithPrefix(
        const char *description,
        const MeasureFormat &fmt,
        const UnicodeString &prefix,
        const Measure *measures,
        int32_t measureCount,
        const char *expected);
    void verifyFormat(
        const char *description,
        const MeasureFormat &fmt,
        const ExpectedResult *expectedResults,
        int32_t count);
    void helperTestSimplePer(
        const Locale &locale,
        UMeasureFormatWidth width,
        double value,
        const MeasureUnit &unit,
        const MeasureUnit &perUnit,
        const char *expected);
    void helperTestMultiplesWithPer(
        const Locale &locale,
        UMeasureFormatWidth width,
        const MeasureUnit &unit,
        const char *expected);
    void helperTestMultiples(
        const Locale &locale,
        UMeasureFormatWidth width,
        const char *expected);
    void verifyFieldPosition(
        const char *description,
        const MeasureFormat &fmt,
        const UnicodeString &prefix,
        const Measure *measures,
        int32_t measureCount,
        NumberFormat::EAlignmentFields field,
        int32_t start,
        int32_t end);
    void verifyFieldPositionWithPer(
        const char *description,
        const MeasureFormat &fmt,
        const UnicodeString &prefix,
        const Measure *measures,
        int32_t measureCount,
        const MeasureUnit &perUnit,
        NumberFormat::EAlignmentFields field,
        int32_t start,
        int32_t end);
};

void MeasureFormatTest::runIndexedTest(
        int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite MeasureFormatTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestBasic);
    TESTCASE_AUTO(TestCompatible53_1);
    TESTCASE_AUTO(TestCompatible54_1);
    TESTCASE_AUTO(TestGetAvailable);
    TESTCASE_AUTO(TestExamplesInDocs);
    TESTCASE_AUTO(TestFormatPeriodEn);
    TESTCASE_AUTO(Test10219FractionalPlurals);
    TESTCASE_AUTO(TestGreek);
    TESTCASE_AUTO(TestFormatSingleArg);
    TESTCASE_AUTO(TestFormatMeasuresZeroArg);
    TESTCASE_AUTO(TestMultiplesWithPer);
    TESTCASE_AUTO(TestSimplePer);
    TESTCASE_AUTO(TestNumeratorPlurals);
    TESTCASE_AUTO(TestMultiples);
    TESTCASE_AUTO(TestGram);
    TESTCASE_AUTO(TestCurrencies);
    TESTCASE_AUTO(TestFieldPosition);
    TESTCASE_AUTO(TestFieldPositionMultiple);
    TESTCASE_AUTO(TestFieldPositionMultipleWithPer);
    TESTCASE_AUTO(TestBadArg);
    TESTCASE_AUTO(TestEquality);
    TESTCASE_AUTO(TestGroupingSeparator);
    TESTCASE_AUTO(TestDoubleZero);
    TESTCASE_AUTO_END;
}

void MeasureFormatTest::TestCompatible53_1() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnit.adoptInstead(MeasureUnit::createDay(status));
    measureUnit.adoptInstead(MeasureUnit::createHour(status));
    measureUnit.adoptInstead(MeasureUnit::createMillisecond(status));
    measureUnit.adoptInstead(MeasureUnit::createMinute(status));
    measureUnit.adoptInstead(MeasureUnit::createMonth(status));
    measureUnit.adoptInstead(MeasureUnit::createSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createWeek(status));
    measureUnit.adoptInstead(MeasureUnit::createYear(status));
    measureUnit.adoptInstead(MeasureUnit::createCentimeter(status));
    measureUnit.adoptInstead(MeasureUnit::createFoot(status));
    measureUnit.adoptInstead(MeasureUnit::createInch(status));
    measureUnit.adoptInstead(MeasureUnit::createKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createLightYear(status));
    measureUnit.adoptInstead(MeasureUnit::createMeter(status));
    measureUnit.adoptInstead(MeasureUnit::createMile(status));
    measureUnit.adoptInstead(MeasureUnit::createMillimeter(status));
    measureUnit.adoptInstead(MeasureUnit::createPicometer(status));
    measureUnit.adoptInstead(MeasureUnit::createYard(status));
    measureUnit.adoptInstead(MeasureUnit::createGram(status));
    measureUnit.adoptInstead(MeasureUnit::createKilogram(status));
    measureUnit.adoptInstead(MeasureUnit::createOunce(status));
    measureUnit.adoptInstead(MeasureUnit::createPound(status));
    measureUnit.adoptInstead(MeasureUnit::createHorsepower(status));
    measureUnit.adoptInstead(MeasureUnit::createKilowatt(status));
    measureUnit.adoptInstead(MeasureUnit::createWatt(status));
    measureUnit.adoptInstead(MeasureUnit::createHectopascal(status));
    measureUnit.adoptInstead(MeasureUnit::createInchHg(status));
    measureUnit.adoptInstead(MeasureUnit::createMillibar(status));
    measureUnit.adoptInstead(MeasureUnit::createKilometerPerHour(status));
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnit.adoptInstead(MeasureUnit::createCubicKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createCubicMile(status));
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    assertSuccess("", status);
}

void MeasureFormatTest::TestCompatible54_1() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecondSquared(status));
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnit.adoptInstead(MeasureUnit::createRadian(status));
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareCentimeter(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareInch(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareYard(status));
    measureUnit.adoptInstead(MeasureUnit::createLiterPerKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallon(status));
    measureUnit.adoptInstead(MeasureUnit::createBit(status));
    measureUnit.adoptInstead(MeasureUnit::createByte(status));
    measureUnit.adoptInstead(MeasureUnit::createGigabit(status));
    measureUnit.adoptInstead(MeasureUnit::createGigabyte(status));
    measureUnit.adoptInstead(MeasureUnit::createKilobit(status));
    measureUnit.adoptInstead(MeasureUnit::createKilobyte(status));
    measureUnit.adoptInstead(MeasureUnit::createMegabit(status));
    measureUnit.adoptInstead(MeasureUnit::createMegabyte(status));
    measureUnit.adoptInstead(MeasureUnit::createTerabit(status));
    measureUnit.adoptInstead(MeasureUnit::createTerabyte(status));
    measureUnit.adoptInstead(MeasureUnit::createDay(status));
    measureUnit.adoptInstead(MeasureUnit::createHour(status));
    measureUnit.adoptInstead(MeasureUnit::createMicrosecond(status));
    measureUnit.adoptInstead(MeasureUnit::createMillisecond(status));
    measureUnit.adoptInstead(MeasureUnit::createMinute(status));
    measureUnit.adoptInstead(MeasureUnit::createMonth(status));
    measureUnit.adoptInstead(MeasureUnit::createNanosecond(status));
    measureUnit.adoptInstead(MeasureUnit::createSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createWeek(status));
    measureUnit.adoptInstead(MeasureUnit::createYear(status));
    measureUnit.adoptInstead(MeasureUnit::createAmpere(status));
    measureUnit.adoptInstead(MeasureUnit::createMilliampere(status));
    measureUnit.adoptInstead(MeasureUnit::createOhm(status));
    measureUnit.adoptInstead(MeasureUnit::createVolt(status));
    measureUnit.adoptInstead(MeasureUnit::createCalorie(status));
    measureUnit.adoptInstead(MeasureUnit::createFoodcalorie(status));
    measureUnit.adoptInstead(MeasureUnit::createJoule(status));
    measureUnit.adoptInstead(MeasureUnit::createKilocalorie(status));
    measureUnit.adoptInstead(MeasureUnit::createKilojoule(status));
    measureUnit.adoptInstead(MeasureUnit::createKilowattHour(status));
    measureUnit.adoptInstead(MeasureUnit::createGigahertz(status));
    measureUnit.adoptInstead(MeasureUnit::createHertz(status));
    measureUnit.adoptInstead(MeasureUnit::createKilohertz(status));
    measureUnit.adoptInstead(MeasureUnit::createMegahertz(status));
    measureUnit.adoptInstead(MeasureUnit::createAstronomicalUnit(status));
    measureUnit.adoptInstead(MeasureUnit::createCentimeter(status));
    measureUnit.adoptInstead(MeasureUnit::createDecimeter(status));
    measureUnit.adoptInstead(MeasureUnit::createFathom(status));
    measureUnit.adoptInstead(MeasureUnit::createFoot(status));
    measureUnit.adoptInstead(MeasureUnit::createFurlong(status));
    measureUnit.adoptInstead(MeasureUnit::createInch(status));
    measureUnit.adoptInstead(MeasureUnit::createKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createLightYear(status));
    measureUnit.adoptInstead(MeasureUnit::createMeter(status));
    measureUnit.adoptInstead(MeasureUnit::createMicrometer(status));
    measureUnit.adoptInstead(MeasureUnit::createMile(status));
    measureUnit.adoptInstead(MeasureUnit::createMillimeter(status));
    measureUnit.adoptInstead(MeasureUnit::createNanometer(status));
    measureUnit.adoptInstead(MeasureUnit::createNauticalMile(status));
    measureUnit.adoptInstead(MeasureUnit::createParsec(status));
    measureUnit.adoptInstead(MeasureUnit::createPicometer(status));
    measureUnit.adoptInstead(MeasureUnit::createYard(status));
    measureUnit.adoptInstead(MeasureUnit::createLux(status));
    measureUnit.adoptInstead(MeasureUnit::createCarat(status));
    measureUnit.adoptInstead(MeasureUnit::createGram(status));
    measureUnit.adoptInstead(MeasureUnit::createKilogram(status));
    measureUnit.adoptInstead(MeasureUnit::createMetricTon(status));
    measureUnit.adoptInstead(MeasureUnit::createMicrogram(status));
    measureUnit.adoptInstead(MeasureUnit::createMilligram(status));
    measureUnit.adoptInstead(MeasureUnit::createOunce(status));
    measureUnit.adoptInstead(MeasureUnit::createOunceTroy(status));
    measureUnit.adoptInstead(MeasureUnit::createPound(status));
    measureUnit.adoptInstead(MeasureUnit::createStone(status));
    measureUnit.adoptInstead(MeasureUnit::createTon(status));
    measureUnit.adoptInstead(MeasureUnit::createGigawatt(status));
    measureUnit.adoptInstead(MeasureUnit::createHorsepower(status));
    measureUnit.adoptInstead(MeasureUnit::createKilowatt(status));
    measureUnit.adoptInstead(MeasureUnit::createMegawatt(status));
    measureUnit.adoptInstead(MeasureUnit::createMilliwatt(status));
    measureUnit.adoptInstead(MeasureUnit::createWatt(status));
    measureUnit.adoptInstead(MeasureUnit::createHectopascal(status));
    measureUnit.adoptInstead(MeasureUnit::createInchHg(status));
    measureUnit.adoptInstead(MeasureUnit::createMillibar(status));
    measureUnit.adoptInstead(MeasureUnit::createMillimeterOfMercury(status));
    measureUnit.adoptInstead(MeasureUnit::createPoundPerSquareInch(status));
    measureUnit.adoptInstead(MeasureUnit::createKarat(status));
    measureUnit.adoptInstead(MeasureUnit::createKilometerPerHour(status));
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnit.adoptInstead(MeasureUnit::createKelvin(status));
    measureUnit.adoptInstead(MeasureUnit::createAcreFoot(status));
    measureUnit.adoptInstead(MeasureUnit::createBushel(status));
    measureUnit.adoptInstead(MeasureUnit::createCentiliter(status));
    measureUnit.adoptInstead(MeasureUnit::createCubicCentimeter(status));
    measureUnit.adoptInstead(MeasureUnit::createCubicFoot(status));
    measureUnit.adoptInstead(MeasureUnit::createCubicInch(status));
    measureUnit.adoptInstead(MeasureUnit::createCubicKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createCubicMeter(status));
    measureUnit.adoptInstead(MeasureUnit::createCubicMile(status));
    measureUnit.adoptInstead(MeasureUnit::createCubicYard(status));
    measureUnit.adoptInstead(MeasureUnit::createCup(status));
    measureUnit.adoptInstead(MeasureUnit::createDeciliter(status));
    measureUnit.adoptInstead(MeasureUnit::createFluidOunce(status));
    measureUnit.adoptInstead(MeasureUnit::createGallon(status));
    measureUnit.adoptInstead(MeasureUnit::createHectoliter(status));
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    measureUnit.adoptInstead(MeasureUnit::createMegaliter(status));
    measureUnit.adoptInstead(MeasureUnit::createMilliliter(status));
    measureUnit.adoptInstead(MeasureUnit::createPint(status));
    measureUnit.adoptInstead(MeasureUnit::createQuart(status));
    measureUnit.adoptInstead(MeasureUnit::createTablespoon(status));
    measureUnit.adoptInstead(MeasureUnit::createTeaspoon(status));
    assertSuccess("", status);
}

void MeasureFormatTest::TestBasic() {
    UErrorCode status = U_ZERO_ERROR;
    MeasureUnit *ptr1 = MeasureUnit::createArcMinute(status);
    MeasureUnit *ptr2 = MeasureUnit::createArcMinute(status);
    if (!(*ptr1 == *ptr2)) {
        errln("Expect == to work.");
    }
    if (*ptr1 != *ptr2) {
        errln("Expect != to work.");
    }
    MeasureUnit *ptr3 = MeasureUnit::createMeter(status);
    if (*ptr1 == *ptr3) {
        errln("Expect == to work.");
    }
    if (!(*ptr1 != *ptr3)) {
        errln("Expect != to work.");
    }
    MeasureUnit *ptr4 = (MeasureUnit *) ptr1->clone();
    if (*ptr1 != *ptr4) {
        errln("Expect clone to work.");
    }
    MeasureUnit stack;
    stack = *ptr1;
    if (*ptr1 != stack) {
        errln("Expect assignment to work.");
    }

    delete ptr1;
    delete ptr2;
    delete ptr3;
    delete ptr4;
}

void MeasureFormatTest::TestGetAvailable() {
    MeasureUnit *units = NULL;
    UErrorCode status = U_ZERO_ERROR;
    int32_t totalCount = MeasureUnit::getAvailable(units, 0, status);
    while (status == U_BUFFER_OVERFLOW_ERROR) {
        status = U_ZERO_ERROR;
        delete [] units;
        units = new MeasureUnit[totalCount];
        totalCount = MeasureUnit::getAvailable(units, totalCount, status);
    }
    if (U_FAILURE(status)) {
        dataerrln("Failure creating format object - %s", u_errorName(status));
        delete [] units;
        return;
    }
    if (totalCount < 200) {
        errln("Expect at least 200 measure units including currencies.");
    }
    delete [] units;
    StringEnumeration *types = MeasureUnit::getAvailableTypes(status);
    if (U_FAILURE(status)) {
        dataerrln("Failure getting types - %s", u_errorName(status));
        delete types;
        return;
    }
    if (types->count(status) < 10) {
        errln("Expect at least 10 distinct unit types.");
    }
    units = NULL;
    int32_t unitCapacity = 0;
    int32_t unitCountSum = 0;
    for (
            const char* type = types->next(NULL, status);
            type != NULL;
            type = types->next(NULL, status)) {
        int32_t unitCount = MeasureUnit::getAvailable(type, units, unitCapacity, status);
        while (status == U_BUFFER_OVERFLOW_ERROR) {
            status = U_ZERO_ERROR;
            delete [] units;
            units = new MeasureUnit[unitCount];
            unitCapacity = unitCount;
            unitCount = MeasureUnit::getAvailable(type, units, unitCapacity, status);
        }
        if (U_FAILURE(status)) {
            dataerrln("Failure getting units - %s", u_errorName(status));
            delete [] units;
            delete types;
            return;
        }
        if (unitCount < 1) {
            errln("Expect at least one unit count per type.");
        }
        unitCountSum += unitCount;
    }
    if (unitCountSum != totalCount) {
        errln("Expected total unit count to equal sum of unit counts by type.");
    }
    delete [] units;
    delete types;
}

void MeasureFormatTest::TestExamplesInDocs() {
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat fmtFr(Locale::getFrench(), UMEASFMT_WIDTH_SHORT, status);
    MeasureFormat fmtFrFull(
            Locale::getFrench(), UMEASFMT_WIDTH_WIDE, status);
    MeasureFormat fmtFrNarrow(
            Locale::getFrench(), UMEASFMT_WIDTH_NARROW, status);
    MeasureFormat fmtEn(Locale::getUS(), UMEASFMT_WIDTH_WIDE, status);
    if (!assertSuccess("Error creating formatters", status)) {
        return;
    }
    Measure measureC(23, MeasureUnit::createCelsius(status), status);
    Measure measureF(70, MeasureUnit::createFahrenheit(status), status);
    Measure feetAndInches[] = {
            Measure(70, MeasureUnit::createFoot(status), status),
            Measure(5.3, MeasureUnit::createInch(status), status)};
    Measure footAndInch[] = {
            Measure(1, MeasureUnit::createFoot(status), status),
            Measure(1, MeasureUnit::createInch(status), status)};
    Measure inchAndFeet[] = {
            Measure(1, MeasureUnit::createInch(status), status),
            Measure(2, MeasureUnit::createFoot(status), status)};
    if (!assertSuccess("Error creating measurements.", status)) {
        return;
    }
    verifyFormat(
            "Celsius",
            fmtFr,
            &measureC,
            1,
            "23 \\u00B0C");
    verifyFormatWithPrefix(
            "Celsius",
            fmtFr,
            "Prefix: ",
            &measureC,
            1,
            "Prefix: 23 \\u00B0C");
    verifyFormat(
            "Fahrenheit",
            fmtFr,
            &measureF,
            1,
            "70 \\u00B0F");
    verifyFormat(
            "Feet and inches",
            fmtFrFull,
            feetAndInches,
            UPRV_LENGTHOF(feetAndInches),
            "70 pieds et 5,3 pouces");
    verifyFormatWithPrefix(
            "Feet and inches",
            fmtFrFull,
            "Prefix: ",
            feetAndInches,
            UPRV_LENGTHOF(feetAndInches),
            "Prefix: 70 pieds et 5,3 pouces");
    verifyFormat(
            "Foot and inch",
            fmtFrFull,
            footAndInch,
            UPRV_LENGTHOF(footAndInch),
            "1 pied et 1 pouce");
    verifyFormat(
            "Foot and inch narrow",
            fmtFrNarrow,
            footAndInch,
            UPRV_LENGTHOF(footAndInch),
            "1\\u2032 1\\u2033");
    verifyFormat(
            "Inch and feet",
            fmtEn,
            inchAndFeet,
            UPRV_LENGTHOF(inchAndFeet),
            "1 inch, 2 feet");
}

void MeasureFormatTest::TestFormatPeriodEn() {
    UErrorCode status = U_ZERO_ERROR;
    Measure t_19m[] = {Measure(19, MeasureUnit::createMinute(status), status)};
    Measure t_1h_23_5s[] = {
            Measure(1.0, MeasureUnit::createHour(status), status),
            Measure(23.5, MeasureUnit::createSecond(status), status)
    };
    Measure t_1h_23_5m[] = {
            Measure(1.0, MeasureUnit::createHour(status), status),
            Measure(23.5, MeasureUnit::createMinute(status), status)
    };
    Measure t_1h_0m_23s[] = {
            Measure(
                    1.0,
                    TimeUnit::createInstance(
                            TimeUnit::UTIMEUNIT_HOUR, status),
                    status),
            Measure(
                    0.0,
                    TimeUnit::createInstance(
                            TimeUnit::UTIMEUNIT_MINUTE, status),
                     status),
            Measure(
                    23,
                    TimeUnit::createInstance(
                            TimeUnit::UTIMEUNIT_SECOND, status),
                    status)
    };
    Measure t_2y_5M_3w_4d[] = {
            Measure(2.0, MeasureUnit::createYear(status), status),
            Measure(5.0, MeasureUnit::createMonth(status), status),
            Measure(3.0, MeasureUnit::createWeek(status), status),
            Measure(4.0, MeasureUnit::createDay(status), status)
    };
    Measure t_1m_59_9996s[] = {
            Measure(1.0, MeasureUnit::createMinute(status), status),
            Measure(59.9996, MeasureUnit::createSecond(status), status)
    };
    Measure t_5h_17m[] = {
            Measure(5.0, MeasureUnit::createHour(status), status),
            Measure(17.0, MeasureUnit::createMinute(status), status)
    };
    Measure t_neg5h_17m[] = {
            Measure(-5.0, MeasureUnit::createHour(status), status),
            Measure(17.0, MeasureUnit::createMinute(status), status)
    };
    Measure t_19m_28s[] = {
            Measure(19.0, MeasureUnit::createMinute(status), status),
            Measure(28.0, MeasureUnit::createSecond(status), status)
    };
    Measure t_0h_0m_9s[] = {
            Measure(0.0, MeasureUnit::createHour(status), status),
            Measure(0.0, MeasureUnit::createMinute(status), status),
            Measure(9.0, MeasureUnit::createSecond(status), status)
    };
    Measure t_0h_0m_17s[] = {
            Measure(0.0, MeasureUnit::createHour(status), status),
            Measure(0.0, MeasureUnit::createMinute(status), status),
            Measure(17.0, MeasureUnit::createSecond(status), status)
    };
    Measure t_6h_56_92m[] = {
            Measure(6.0, MeasureUnit::createHour(status), status),
            Measure(56.92, MeasureUnit::createMinute(status), status)
    };
    Measure t_3h_4s_5m[] = {
            Measure(3.0, MeasureUnit::createHour(status), status),
            Measure(4.0, MeasureUnit::createSecond(status), status),
            Measure(5.0, MeasureUnit::createMinute(status), status)
    };
    Measure t_6_7h_56_92m[] = {
            Measure(6.7, MeasureUnit::createHour(status), status),
            Measure(56.92, MeasureUnit::createMinute(status), status)
    };

    Measure t_3h_5h[] = {
            Measure(3.0, MeasureUnit::createHour(status), status),
            Measure(5.0, MeasureUnit::createHour(status), status)
    };

    if (!assertSuccess("Error creating Measure objects", status)) {
        return;
    }
    
    ExpectedResult fullData[] = {
            {t_1m_59_9996s, UPRV_LENGTHOF(t_1m_59_9996s), "1 minute, 59.9996 seconds"},
            {t_19m, UPRV_LENGTHOF(t_19m), "19 minutes"},
            {t_1h_23_5s, UPRV_LENGTHOF(t_1h_23_5s), "1 hour, 23.5 seconds"},
            {t_1h_23_5m, UPRV_LENGTHOF(t_1h_23_5m), "1 hour, 23.5 minutes"},
            {t_1h_0m_23s, UPRV_LENGTHOF(t_1h_0m_23s), "1 hour, 0 minutes, 23 seconds"},
            {t_2y_5M_3w_4d, UPRV_LENGTHOF(t_2y_5M_3w_4d), "2 years, 5 months, 3 weeks, 4 days"}};

    ExpectedResult abbrevData[] = {
            {t_1m_59_9996s, UPRV_LENGTHOF(t_1m_59_9996s), "1 min, 59.9996 secs"},
            {t_19m, UPRV_LENGTHOF(t_19m), "19 mins"},
            {t_1h_23_5s, UPRV_LENGTHOF(t_1h_23_5s), "1 hr, 23.5 secs"},
            {t_1h_23_5m, UPRV_LENGTHOF(t_1h_23_5m), "1 hr, 23.5 mins"},
            {t_1h_0m_23s, UPRV_LENGTHOF(t_1h_0m_23s), "1 hr, 0 mins, 23 secs"},
            {t_2y_5M_3w_4d, UPRV_LENGTHOF(t_2y_5M_3w_4d), "2 yrs, 5 mths, 3 wks, 4 days"}};

    ExpectedResult narrowData[] = {
            {t_1m_59_9996s, UPRV_LENGTHOF(t_1m_59_9996s), "1m 59.9996s"},
            {t_19m, UPRV_LENGTHOF(t_19m), "19m"},
            {t_1h_23_5s, UPRV_LENGTHOF(t_1h_23_5s), "1h 23.5s"},
            {t_1h_23_5m, UPRV_LENGTHOF(t_1h_23_5m), "1h 23.5m"},
            {t_1h_0m_23s, UPRV_LENGTHOF(t_1h_0m_23s), "1h 0m 23s"},
            {t_2y_5M_3w_4d, UPRV_LENGTHOF(t_2y_5M_3w_4d), "2y 5m 3w 4d"}};

    ExpectedResult numericData[] = {
            {t_1m_59_9996s, UPRV_LENGTHOF(t_1m_59_9996s), "1:59.9996"},
            {t_19m, UPRV_LENGTHOF(t_19m), "19m"},
            {t_1h_23_5s, UPRV_LENGTHOF(t_1h_23_5s), "1:00:23.5"},
            {t_1h_23_5m, UPRV_LENGTHOF(t_1h_23_5m), "1:23.5"},
            {t_1h_0m_23s, UPRV_LENGTHOF(t_1h_0m_23s), "1:00:23"},
            {t_5h_17m, UPRV_LENGTHOF(t_5h_17m), "5:17"},
            {t_neg5h_17m, UPRV_LENGTHOF(t_neg5h_17m), "-5h 17m"},
            {t_19m_28s, UPRV_LENGTHOF(t_19m_28s), "19:28"},
            {t_2y_5M_3w_4d, UPRV_LENGTHOF(t_2y_5M_3w_4d), "2y 5m 3w 4d"},
            {t_0h_0m_9s, UPRV_LENGTHOF(t_0h_0m_9s), "0:00:09"},
            {t_6h_56_92m, UPRV_LENGTHOF(t_6h_56_92m), "6:56.92"},
            {t_6_7h_56_92m, UPRV_LENGTHOF(t_6_7h_56_92m), "6:56.92"},
            {t_3h_4s_5m, UPRV_LENGTHOF(t_3h_4s_5m), "3h 4s 5m"},
            {t_3h_5h, UPRV_LENGTHOF(t_3h_5h), "3h 5h"}};

    ExpectedResult fullDataDe[] = {
            {t_1m_59_9996s, UPRV_LENGTHOF(t_1m_59_9996s), "1 Minute und 59,9996 Sekunden"},
            {t_19m, UPRV_LENGTHOF(t_19m), "19 Minuten"},
            {t_1h_23_5s, UPRV_LENGTHOF(t_1h_23_5s), "1 Stunde und 23,5 Sekunden"},
            {t_1h_23_5m, UPRV_LENGTHOF(t_1h_23_5m), "1 Stunde und 23,5 Minuten"},
            {t_1h_0m_23s, UPRV_LENGTHOF(t_1h_0m_23s), "1 Stunde, 0 Minuten und 23 Sekunden"},
            {t_2y_5M_3w_4d, UPRV_LENGTHOF(t_2y_5M_3w_4d), "2 Jahre, 5 Monate, 3 Wochen und 4 Tage"}};

    ExpectedResult numericDataDe[] = {
            {t_1m_59_9996s, UPRV_LENGTHOF(t_1m_59_9996s), "1:59,9996"},
            {t_19m, UPRV_LENGTHOF(t_19m), "19 Min."},
            {t_1h_23_5s, UPRV_LENGTHOF(t_1h_23_5s), "1:00:23,5"},
            {t_1h_23_5m, UPRV_LENGTHOF(t_1h_23_5m), "1:23,5"},
            {t_1h_0m_23s, UPRV_LENGTHOF(t_1h_0m_23s), "1:00:23"},
            {t_5h_17m, UPRV_LENGTHOF(t_5h_17m), "5:17"},
            {t_19m_28s, UPRV_LENGTHOF(t_19m_28s), "19:28"},
            {t_2y_5M_3w_4d, UPRV_LENGTHOF(t_2y_5M_3w_4d), "2 J, 5 M, 3 W und 4 T"},
            {t_0h_0m_17s, UPRV_LENGTHOF(t_0h_0m_17s), "0:00:17"},
            {t_6h_56_92m, UPRV_LENGTHOF(t_6h_56_92m), "6:56,92"},
            {t_3h_5h, UPRV_LENGTHOF(t_3h_5h), "3 Std., 5 Std."}};

    Locale en(Locale::getEnglish());
    LocalPointer<NumberFormat> nf(NumberFormat::createInstance(en, status));
    if (U_FAILURE(status)) {
        dataerrln("Error creating number format en object - %s", u_errorName(status));
        return;
    }
    nf->setMaximumFractionDigits(4);
    MeasureFormat mf(en, UMEASFMT_WIDTH_WIDE, (NumberFormat *) nf->clone(), status);
    if (!assertSuccess("Error creating measure format en WIDE", status)) {
        return;
    }
    verifyFormat("en WIDE", mf, fullData, UPRV_LENGTHOF(fullData));

    // exercise copy constructor
    {
        MeasureFormat mf2(mf);
        verifyFormat("en WIDE copy", mf2, fullData, UPRV_LENGTHOF(fullData));
    }
    // exercise clone
    {
        MeasureFormat *mf3 = (MeasureFormat *) mf.clone();
        verifyFormat("en WIDE copy", *mf3, fullData, UPRV_LENGTHOF(fullData));
        delete mf3;
    }
    mf = MeasureFormat(en, UMEASFMT_WIDTH_SHORT, (NumberFormat *) nf->clone(), status);
    if (!assertSuccess("Error creating measure format en SHORT", status)) {
        return;
    }
    verifyFormat("en SHORT", mf, abbrevData, UPRV_LENGTHOF(abbrevData));
    mf = MeasureFormat(en, UMEASFMT_WIDTH_NARROW, (NumberFormat *) nf->clone(), status);
    if (!assertSuccess("Error creating measure format en NARROW", status)) {
        return;
    }
    verifyFormat("en NARROW", mf, narrowData, UPRV_LENGTHOF(narrowData));
    mf = MeasureFormat(en, UMEASFMT_WIDTH_NUMERIC, (NumberFormat *) nf->clone(), status);
    if (!assertSuccess("Error creating measure format en NUMERIC", status)) {
        return;
    }
    verifyFormat("en NUMERIC", mf, numericData, UPRV_LENGTHOF(numericData));
    
    Locale de(Locale::getGerman());
    nf.adoptInstead(NumberFormat::createInstance(de, status));
    if (!assertSuccess("Error creating number format de object", status)) {
        return;
    }
    nf->setMaximumFractionDigits(4);
    mf = MeasureFormat(de, UMEASFMT_WIDTH_WIDE, (NumberFormat *) nf->clone(), status);
    if (!assertSuccess("Error creating measure format de WIDE", status)) {
        return;
    }
    verifyFormat("de WIDE", mf, fullDataDe, UPRV_LENGTHOF(fullDataDe));
    mf = MeasureFormat(de, UMEASFMT_WIDTH_NUMERIC, (NumberFormat *) nf->clone(), status);
    if (!assertSuccess("Error creating measure format de NUMERIC", status)) {
        return;
    }
    verifyFormat("de NUMERIC", mf, numericDataDe, UPRV_LENGTHOF(numericDataDe));
}

void MeasureFormatTest::Test10219FractionalPlurals() {
    Locale en(Locale::getEnglish());
    double values[] = {1.588, 1.011};
    const char *expected[2][3] = {
            {"1 minute", "1.5 minutes", "1.58 minutes"},
            {"1 minute", "1.0 minutes", "1.01 minutes"}
    };
    UErrorCode status = U_ZERO_ERROR;
    for (int j = 0; j < UPRV_LENGTHOF(values); j++) {
        for (int i = 0; i < UPRV_LENGTHOF(expected[j]); i++) {
            DecimalFormat *df =
                (DecimalFormat *) NumberFormat::createInstance(en, status);
            if (U_FAILURE(status)) {
                dataerrln("Error creating Number format - %s", u_errorName(status));
                return;
            }
            df->setRoundingMode(DecimalFormat::kRoundDown);
            df->setMinimumFractionDigits(i);
            df->setMaximumFractionDigits(i);
            MeasureFormat mf(en, UMEASFMT_WIDTH_WIDE, df, status);
            if (!assertSuccess("Error creating Measure format", status)) {
                return;
            }
            Measure measure(values[j], MeasureUnit::createMinute(status), status);
            if (!assertSuccess("Error creating Measure unit", status)) {
                return;
            }
            verifyFormat("Test10219", mf, &measure, 1, expected[j][i]);
        }   
    }
}

static MeasureUnit toMeasureUnit(MeasureUnit *adopted) {
    MeasureUnit result(*adopted);
    delete adopted;
    return result;
}

void MeasureFormatTest::TestGreek() {
    Locale locales[] = {Locale("el_GR"), Locale("el")};
    UErrorCode status = U_ZERO_ERROR;
    MeasureUnit units[] = {
        toMeasureUnit(MeasureUnit::createSecond(status)),
        toMeasureUnit(MeasureUnit::createMinute(status)),
        toMeasureUnit(MeasureUnit::createHour(status)),
        toMeasureUnit(MeasureUnit::createDay(status)),
        toMeasureUnit(MeasureUnit::createWeek(status)),
        toMeasureUnit(MeasureUnit::createMonth(status)),
        toMeasureUnit(MeasureUnit::createYear(status))};
    if (!assertSuccess("Error creating Measure units", status)) {
        return;
    }
    UMeasureFormatWidth styles[] = {
            UMEASFMT_WIDTH_WIDE,
            UMEASFMT_WIDTH_SHORT};
    int32_t numbers[] = {1, 7};
    const char *expected[] = {
        // "el_GR" 1 wide
        "1 \\u03B4\\u03B5\\u03C5\\u03C4\\u03B5\\u03C1\\u03CC\\u03BB\\u03B5\\u03C0\\u03C4\\u03BF",
        "1 \\u03BB\\u03B5\\u03C0\\u03C4\\u03CC",
        "1 \\u03CE\\u03C1\\u03B1",
        "1 \\u03B7\\u03BC\\u03AD\\u03C1\\u03B1",
        "1 \\u03B5\\u03B2\\u03B4\\u03BF\\u03BC\\u03AC\\u03B4\\u03B1",
        "1 \\u03BC\\u03AE\\u03BD\\u03B1\\u03C2",
        "1 \\u03AD\\u03C4\\u03BF\\u03C2",
        // "el_GR" 1 short
        "1 \\u03B4\\u03B5\\u03C5\\u03C4.",
        "1 \\u03BB\\u03B5\\u03C0.",
        "1 \\u03CE\\u03C1\\u03B1",
        "1 \\u03B7\\u03BC\\u03AD\\u03C1\\u03B1",
        "1 \\u03B5\\u03B2\\u03B4.",
        "1 \\u03BC\\u03AE\\u03BD.",
        "1 \\u03AD\\u03C4.",	        // year (one)
        // "el_GR" 7 wide
        "7 \\u03B4\\u03B5\\u03C5\\u03C4\\u03B5\\u03C1\\u03CC\\u03BB\\u03B5\\u03C0\\u03C4\\u03B1",
        "7 \\u03BB\\u03B5\\u03C0\\u03C4\\u03AC",
        "7 \\u03CE\\u03C1\\u03B5\\u03C2",
        "7 \\u03B7\\u03BC\\u03AD\\u03C1\\u03B5\\u03C2",
        "7 \\u03B5\\u03B2\\u03B4\\u03BF\\u03BC\\u03AC\\u03B4\\u03B5\\u03C2",
        "7 \\u03BC\\u03AE\\u03BD\\u03B5\\u03C2",
        "7 \\u03AD\\u03C4\\u03B7",
        // "el_GR" 7 short
        "7 \\u03B4\\u03B5\\u03C5\\u03C4.",
        "7 \\u03BB\\u03B5\\u03C0.",
        "7 \\u03CE\\u03C1.",		    // hour (other)
        "7 \\u03B7\\u03BC\\u03AD\\u03C1\\u03B5\\u03C2",
        "7 \\u03B5\\u03B2\\u03B4.",
        "7 \\u03BC\\u03AE\\u03BD.",
        "7 \\u03AD\\u03C4.",            // year (other)

        // "el" 1 wide
        "1 \\u03B4\\u03B5\\u03C5\\u03C4\\u03B5\\u03C1\\u03CC\\u03BB\\u03B5\\u03C0\\u03C4\\u03BF",
        "1 \\u03BB\\u03B5\\u03C0\\u03C4\\u03CC",
        "1 \\u03CE\\u03C1\\u03B1",
        "1 \\u03B7\\u03BC\\u03AD\\u03C1\\u03B1",
        "1 \\u03B5\\u03B2\\u03B4\\u03BF\\u03BC\\u03AC\\u03B4\\u03B1",
        "1 \\u03BC\\u03AE\\u03BD\\u03B1\\u03C2",
        "1 \\u03AD\\u03C4\\u03BF\\u03C2",
        // "el" 1 short
        "1 \\u03B4\\u03B5\\u03C5\\u03C4.",
        "1 \\u03BB\\u03B5\\u03C0.",
        "1 \\u03CE\\u03C1\\u03B1",
        "1 \\u03B7\\u03BC\\u03AD\\u03C1\\u03B1",
        "1 \\u03B5\\u03B2\\u03B4.",
        "1 \\u03BC\\u03AE\\u03BD.",
        "1 \\u03AD\\u03C4.",	        // year (one)
        // "el" 7 wide
        "7 \\u03B4\\u03B5\\u03C5\\u03C4\\u03B5\\u03C1\\u03CC\\u03BB\\u03B5\\u03C0\\u03C4\\u03B1",
        "7 \\u03BB\\u03B5\\u03C0\\u03C4\\u03AC",
        "7 \\u03CE\\u03C1\\u03B5\\u03C2",
        "7 \\u03B7\\u03BC\\u03AD\\u03C1\\u03B5\\u03C2",
        "7 \\u03B5\\u03B2\\u03B4\\u03BF\\u03BC\\u03AC\\u03B4\\u03B5\\u03C2",
        "7 \\u03BC\\u03AE\\u03BD\\u03B5\\u03C2",
        "7 \\u03AD\\u03C4\\u03B7",
        // "el" 7 short
        "7 \\u03B4\\u03B5\\u03C5\\u03C4.",
        "7 \\u03BB\\u03B5\\u03C0.",
        "7 \\u03CE\\u03C1.",		    // hour (other)
        "7 \\u03B7\\u03BC\\u03AD\\u03C1\\u03B5\\u03C2",
        "7 \\u03B5\\u03B2\\u03B4.",
        "7 \\u03BC\\u03AE\\u03BD.",
        "7 \\u03AD\\u03C4."};           // year (other)

    int32_t counter = 0;
    for (int32_t locIndex = 0; locIndex < UPRV_LENGTHOF(locales); ++locIndex ) {
        for( int32_t numIndex = 0; numIndex < UPRV_LENGTHOF(numbers); ++numIndex ) {
            for ( int32_t styleIndex = 0; styleIndex < UPRV_LENGTHOF(styles); ++styleIndex ) {
                for ( int32_t unitIndex = 0; unitIndex < UPRV_LENGTHOF(units); ++unitIndex ) {
                    Measure measure(numbers[numIndex], new MeasureUnit(units[unitIndex]), status);
                    if (!assertSuccess("Error creating Measure", status)) {
                        return;
                    }
                    MeasureFormat fmt(locales[locIndex], styles[styleIndex], status);
                    if (!assertSuccess("Error creating Measure format", status)) {
                        return;
                    }
                    verifyFormat("TestGreek", fmt, &measure, 1, expected[counter]);
                    ++counter;
                }
            }
        }
    }
}

void MeasureFormatTest::TestFormatSingleArg() {
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat fmt("en", UMEASFMT_WIDTH_WIDE, status);
    if (!assertSuccess("Error creating formatter", status)) {
        return;
    }
    UnicodeString buffer;
    FieldPosition pos(0);
    fmt.format(
            new Measure(3.5, MeasureUnit::createFoot(status), status),
            buffer,
            pos,
            status);
    if (!assertSuccess("Error formatting", status)) {
        return;
    }
    assertEquals(
            "TestFormatSingleArg",
            UnicodeString("3.5 feet"),
            buffer);
}

void MeasureFormatTest::TestFormatMeasuresZeroArg() {
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat fmt("en", UMEASFMT_WIDTH_WIDE, status);
    verifyFormat("TestFormatMeasuresZeroArg", fmt, NULL, 0, "");
}

void MeasureFormatTest::TestMultiplesWithPer() {
    Locale en("en");
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> second(MeasureUnit::createSecond(status));
    LocalPointer<MeasureUnit> minute(MeasureUnit::createMinute(status));
    if (!assertSuccess("", status)) {
        return;
    }

    // Per unit test.
    helperTestMultiplesWithPer(
            en, UMEASFMT_WIDTH_WIDE, *second, "2 miles, 1 foot, 2.3 inches per second");
    helperTestMultiplesWithPer(
            en, UMEASFMT_WIDTH_SHORT, *second, "2 mi, 1 ft, 2.3 inps");
    helperTestMultiplesWithPer(
            en, UMEASFMT_WIDTH_NARROW, *second, "2mi 1\\u2032 2.3\\u2033/s");

    // Fallback compound per test
    helperTestMultiplesWithPer(
            en, UMEASFMT_WIDTH_WIDE, *minute, "2 miles, 1 foot, 2.3 inches per minute");
    helperTestMultiplesWithPer(
            en, UMEASFMT_WIDTH_SHORT, *minute, "2 mi, 1 ft, 2.3 in/min");
    helperTestMultiplesWithPer(
            en, UMEASFMT_WIDTH_NARROW, *minute, "2mi 1\\u2032 2.3\\u2033/m");
}

void MeasureFormatTest::TestSimplePer() {
    Locale en("en");
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> second(MeasureUnit::createSecond(status));
    LocalPointer<MeasureUnit> minute(MeasureUnit::createMinute(status));
    LocalPointer<MeasureUnit> pound(MeasureUnit::createPound(status));
    if (!assertSuccess("", status)) {
        return;
    }

    helperTestSimplePer(
            en, UMEASFMT_WIDTH_SHORT, 1.0, *pound, *second, "1 lbps");
    helperTestSimplePer(
            en, UMEASFMT_WIDTH_SHORT, 2.0, *pound, *second, "2 lbps");
    helperTestSimplePer(
            en, UMEASFMT_WIDTH_SHORT, 1.0, *pound, *minute, "1 lb/min");
    helperTestSimplePer(
            en, UMEASFMT_WIDTH_SHORT, 2.0, *pound, *minute, "2 lb/min");
}

void MeasureFormatTest::TestNumeratorPlurals() {
    Locale pl("pl");
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> second(MeasureUnit::createSecond(status));
    LocalPointer<MeasureUnit> foot(MeasureUnit::createFoot(status));
    if (!assertSuccess("", status)) {
        return;
    }

    helperTestSimplePer(
            pl, UMEASFMT_WIDTH_WIDE, 1.0, *foot, *second, "1 stopa na sekund\\u0119");
    helperTestSimplePer(
            pl, UMEASFMT_WIDTH_WIDE, 2.0, *foot, *second, "2 stopy na sekund\\u0119");
    helperTestSimplePer(
            pl, UMEASFMT_WIDTH_WIDE, 5.0, *foot, *second, "5 st\\u00f3p na sekund\\u0119");
    helperTestSimplePer(
            pl, UMEASFMT_WIDTH_WIDE, 1.5, *foot, *second, "1,5 stopy na sekund\\u0119");
}

void MeasureFormatTest::helperTestSimplePer(
        const Locale &locale,
        UMeasureFormatWidth width,
        double value,
        const MeasureUnit &unit,
        const MeasureUnit &perUnit,
        const char *expected) {
    UErrorCode status = U_ZERO_ERROR;
    FieldPosition pos(0);
    MeasureFormat fmt(locale, width, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    Measure measure(value, (MeasureUnit *) unit.clone(), status);
    if (!assertSuccess("Error creating measure object", status)) {
        return;
    }
    UnicodeString buffer;
    fmt.formatMeasuresPer(
            &measure,
            1,
            perUnit,
            buffer,
            pos,
            status);
    if (!assertSuccess("Error formatting measures with per", status)) {
        return;
    }
    assertEquals(
            "TestSimplePer",
            UnicodeString(expected).unescape(),
            buffer);
}

void MeasureFormatTest::helperTestMultiplesWithPer(
        const Locale &locale,
        UMeasureFormatWidth width,
        const MeasureUnit &perUnit,
        const char *expected) {
    UErrorCode status = U_ZERO_ERROR;
    FieldPosition pos(0);
    MeasureFormat fmt(locale, width, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    Measure measures[] = {
            Measure(2, MeasureUnit::createMile(status), status),
            Measure(1, MeasureUnit::createFoot(status), status),
            Measure(2.3, MeasureUnit::createInch(status), status)};
    if (!assertSuccess("Error creating measures", status)) {
        return;
    }
    UnicodeString buffer;
    fmt.formatMeasuresPer(
            measures,
            UPRV_LENGTHOF(measures),
            perUnit,
            buffer,
            pos,
            status);
    if (!assertSuccess("Error formatting measures with per", status)) {
        return;
    }
    assertEquals(
            "TestMultiplesWithPer",
            UnicodeString(expected).unescape(),
            buffer);
}

void MeasureFormatTest::TestMultiples() {
    Locale ru("ru");
    Locale en("en");
    helperTestMultiples(en, UMEASFMT_WIDTH_WIDE, "2 miles, 1 foot, 2.3 inches");
    helperTestMultiples(en, UMEASFMT_WIDTH_SHORT, "2 mi, 1 ft, 2.3 in");
    helperTestMultiples(en, UMEASFMT_WIDTH_NARROW, "2mi 1\\u2032 2.3\\u2033");
    helperTestMultiples(ru, UMEASFMT_WIDTH_WIDE, "2 \\u043C\\u0438\\u043B\\u0438, 1 \\u0444\\u0443\\u0442 \\u0438 2,3 \\u0434\\u044E\\u0439\\u043C\\u0430");
    helperTestMultiples(ru, UMEASFMT_WIDTH_SHORT, "2 \\u043C\\u0438\\u043B\\u0438, 1 \\u0444\\u0443\\u0442, 2,3 \\u0434\\u044E\\u0439\\u043C.");
    helperTestMultiples(ru, UMEASFMT_WIDTH_NARROW, "2 \\u043C\\u0438\\u043B\\u044C 1 \\u0444\\u0443\\u0442 2,3 \\u0434\\u044E\\u0439\\u043C\\u0430");
}

void MeasureFormatTest::helperTestMultiples(
        const Locale &locale,
        UMeasureFormatWidth width,
        const char *expected) {
    UErrorCode status = U_ZERO_ERROR;
    FieldPosition pos(0);
    MeasureFormat fmt(locale, width, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    Measure measures[] = {
            Measure(2, MeasureUnit::createMile(status), status),
            Measure(1, MeasureUnit::createFoot(status), status),
            Measure(2.3, MeasureUnit::createInch(status), status)};
    if (!assertSuccess("Error creating measures", status)) {
        return;
    }
    UnicodeString buffer;
    fmt.formatMeasures(measures, UPRV_LENGTHOF(measures), buffer, pos, status);
    if (!assertSuccess("Error formatting measures", status)) {
        return;
    }
    assertEquals("TestMultiples", UnicodeString(expected).unescape(), buffer);
}

void MeasureFormatTest::TestGram() {
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat fmt("en", UMEASFMT_WIDTH_SHORT, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    Measure gram(1, MeasureUnit::createGram(status), status);
    Measure gforce(1, MeasureUnit::createGForce(status), status);
    if (!assertSuccess("Error creating measures", status)) {
        return;
    }
    verifyFormat("TestGram", fmt, &gram, 1, "1 g");
    verifyFormat("TestGram", fmt, &gforce, 1, "1 G");
}

void MeasureFormatTest::TestCurrencies() {
    UChar USD[] = {'U', 'S', 'D', 0};
    UErrorCode status = U_ZERO_ERROR;
    CurrencyAmount USD_1(1.0, USD, status);
    CurrencyAmount USD_2(2.0, USD, status);
    CurrencyAmount USD_NEG_1(-1.0, USD, status);
    if (!assertSuccess("Error creating measures", status)) {
        return;
    }
    Locale en("en");
    MeasureFormat fmt(en, UMEASFMT_WIDTH_WIDE, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    verifyFormat("TestCurrenciesWide", fmt, &USD_NEG_1, 1, "-1.00 US dollars");
    verifyFormat("TestCurrenciesWide", fmt, &USD_1, 1, "1.00 US dollars");
    verifyFormat("TestCurrenciesWide", fmt, &USD_2, 1, "2.00 US dollars");
    fmt = MeasureFormat(en, UMEASFMT_WIDTH_SHORT, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    verifyFormat("TestCurrenciesShort", fmt, &USD_NEG_1, 1, "-USD1.00");
    verifyFormat("TestCurrenciesShort", fmt, &USD_1, 1, "USD1.00");
    verifyFormat("TestCurrenciesShort", fmt, &USD_2, 1, "USD2.00");
    fmt = MeasureFormat(en, UMEASFMT_WIDTH_NARROW, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    verifyFormat("TestCurrenciesNarrow", fmt, &USD_NEG_1, 1, "-$1.00");
    verifyFormat("TestCurrenciesNarrow", fmt, &USD_1, 1, "$1.00");
    verifyFormat("TestCurrenciesNarrow", fmt, &USD_2, 1, "$2.00");
    fmt = MeasureFormat(en, UMEASFMT_WIDTH_NUMERIC, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    verifyFormat("TestCurrenciesNumeric", fmt, &USD_NEG_1, 1, "-$1.00");
    verifyFormat("TestCurrenciesNumeric", fmt, &USD_1, 1, "$1.00");
    verifyFormat("TestCurrenciesNumeric", fmt, &USD_2, 1, "$2.00");
}

void MeasureFormatTest::TestFieldPosition() {
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat fmt("en", UMEASFMT_WIDTH_SHORT, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    Measure measure(43.5, MeasureUnit::createFoot(status), status);
    if (!assertSuccess("Error creating measure object 1", status)) {
        return;
    }
    UnicodeString prefix("123456: ");
    verifyFieldPosition(
            "",
            fmt,
            prefix,
            &measure,
            1,
            NumberFormat::kDecimalSeparatorField,
            10,
            11);
    measure = Measure(43, MeasureUnit::createFoot(status), status);
    if (!assertSuccess("Error creating measure object 2", status)) {
        return;
    }
    verifyFieldPosition(
            "",
            fmt,
            prefix,
            &measure,
            1,
            NumberFormat::kDecimalSeparatorField,
            0,
            0);
}

void MeasureFormatTest::TestFieldPositionMultiple() {
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat fmt("en", UMEASFMT_WIDTH_SHORT, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    Measure first[] = {
            Measure(354, MeasureUnit::createMeter(status), status),
            Measure(23, MeasureUnit::createCentimeter(status), status)};
    Measure second[] = {
            Measure(354, MeasureUnit::createMeter(status), status),
            Measure(23, MeasureUnit::createCentimeter(status), status),
            Measure(5.4, MeasureUnit::createMillimeter(status), status)};
    Measure third[] = {
            Measure(3, MeasureUnit::createMeter(status), status),
            Measure(23, MeasureUnit::createCentimeter(status), status),
            Measure(5, MeasureUnit::createMillimeter(status), status)};
    if (!assertSuccess("Error creating measure objects", status)) {
        return;
    }
    UnicodeString prefix("123456: ");
    verifyFieldPosition(
            "Integer",
            fmt,
            prefix,
            first,
            UPRV_LENGTHOF(first),
            NumberFormat::kIntegerField,
            8,
            11);
    verifyFieldPosition(
            "Decimal separator",
            fmt,
            prefix,
            second,
            UPRV_LENGTHOF(second),
            NumberFormat::kDecimalSeparatorField,
            23,
            24);
    verifyFieldPosition(
            "no decimal separator",
            fmt,
            prefix,
            third,
            UPRV_LENGTHOF(third),
            NumberFormat::kDecimalSeparatorField,
            0,
            0);
}

void MeasureFormatTest::TestFieldPositionMultipleWithPer() {
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat fmt("en", UMEASFMT_WIDTH_SHORT, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    Measure first[] = {
            Measure(354, MeasureUnit::createMeter(status), status),
            Measure(23, MeasureUnit::createCentimeter(status), status)};
    Measure second[] = {
            Measure(354, MeasureUnit::createMeter(status), status),
            Measure(23, MeasureUnit::createCentimeter(status), status),
            Measure(5.4, MeasureUnit::createMillimeter(status), status)};
    Measure third[] = {
            Measure(3, MeasureUnit::createMeter(status), status),
            Measure(23, MeasureUnit::createCentimeter(status), status),
            Measure(5, MeasureUnit::createMillimeter(status), status)};
    if (!assertSuccess("Error creating measure objects", status)) {
        return;
    }
    UnicodeString prefix("123456: ");

    LocalPointer<MeasureUnit> secondUnit(MeasureUnit::createSecond(status));
    LocalPointer<MeasureUnit> minuteUnit(MeasureUnit::createMinute(status));
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }

    // per unit test
    verifyFieldPositionWithPer(
            "Integer",
            fmt,
            prefix,
            first,
            UPRV_LENGTHOF(first),
            *secondUnit,
            NumberFormat::kIntegerField,
            8,
            11);
    verifyFieldPositionWithPer(
            "Decimal separator",
            fmt,
            prefix,
            second,
            UPRV_LENGTHOF(second),
            *secondUnit,
            NumberFormat::kDecimalSeparatorField,
            23,
            24);
    verifyFieldPositionWithPer(
            "no decimal separator",
            fmt,
            prefix,
            third,
            UPRV_LENGTHOF(third),
            *secondUnit,
            NumberFormat::kDecimalSeparatorField,
            0,
            0);

    // Fallback to compound per test
    verifyFieldPositionWithPer(
            "Integer",
            fmt,
            prefix,
            first,
            UPRV_LENGTHOF(first),
            *minuteUnit,
            NumberFormat::kIntegerField,
            8,
            11);
    verifyFieldPositionWithPer(
            "Decimal separator",
            fmt,
            prefix,
            second,
            UPRV_LENGTHOF(second),
            *minuteUnit,
            NumberFormat::kDecimalSeparatorField,
            23,
            24);
    verifyFieldPositionWithPer(
            "no decimal separator",
            fmt,
            prefix,
            third,
            UPRV_LENGTHOF(third),
            *minuteUnit,
            NumberFormat::kDecimalSeparatorField,
            0,
            0);
}

void MeasureFormatTest::TestBadArg() {
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat fmt("en", UMEASFMT_WIDTH_SHORT, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    FieldPosition pos(0);
    UnicodeString buffer;
    fmt.format(
            9.3,
            buffer,
            pos,
            status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected ILLEGAL_ARGUMENT_ERROR");
    }
}

void MeasureFormatTest::TestEquality() {
    UErrorCode status = U_ZERO_ERROR;
    NumberFormat* nfeq = NumberFormat::createInstance("en", status);
    NumberFormat* nfne = NumberFormat::createInstance("fr", status);
    MeasureFormat fmt("en", UMEASFMT_WIDTH_SHORT, status);
    MeasureFormat fmtEq2("en", UMEASFMT_WIDTH_SHORT, nfeq, status);
    MeasureFormat fmtne1("en", UMEASFMT_WIDTH_WIDE, status);
    MeasureFormat fmtne2("fr", UMEASFMT_WIDTH_SHORT, status);
    MeasureFormat fmtne3("en", UMEASFMT_WIDTH_SHORT, nfne, status);
    if (U_FAILURE(status)) {
        dataerrln("Error creating MeasureFormats - %s", u_errorName(status));
        return;
    }
    MeasureFormat fmtEq(fmt);
    assertTrue("Equal", fmt == fmtEq);
    assertTrue("Equal2", fmt == fmtEq2);
    assertFalse("Equal Neg", fmt != fmtEq);
    assertTrue("Not Equal 1", fmt != fmtne1);
    assertFalse("Not Equal Neg 1", fmt == fmtne1);
    assertTrue("Not Equal 2", fmt != fmtne2);
    assertTrue("Not Equal 3", fmt != fmtne3);
}

void MeasureFormatTest::TestGroupingSeparator() {
    UErrorCode status = U_ZERO_ERROR;
    Locale en("en");
    MeasureFormat fmt(en, UMEASFMT_WIDTH_SHORT, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    Measure ms[] = {
            Measure(INT32_MAX, MeasureUnit::createYear(status), status),
            Measure(INT32_MIN, MeasureUnit::createMonth(status), status),
            Measure(-987, MeasureUnit::createDay(status), status),
            Measure(1362, MeasureUnit::createHour(status), status),
            Measure(987, MeasureUnit::createMinute(status), status)};
    FieldPosition pos(NumberFormat::kGroupingSeparatorField);
    UnicodeString appendTo;
    fmt.formatMeasures(ms, 5, appendTo, pos, status);
    if (!assertSuccess("Error formatting", status)) {
        return;
    }
    assertEquals(
            "grouping separator",
            "2,147,483,647 yrs, -2,147,483,648 mths, -987 days, 1,362 hrs, 987 mins",
            appendTo);
    assertEquals("begin index", 9, pos.getBeginIndex());
    assertEquals("end index", 10, pos.getEndIndex());
}

void MeasureFormatTest::TestDoubleZero() {
    UErrorCode status = U_ZERO_ERROR;
    Measure measures[] = {
        Measure(4.7, MeasureUnit::createHour(status), status),
        Measure(23, MeasureUnit::createMinute(status), status),
        Measure(16, MeasureUnit::createSecond(status), status)};
    Locale en("en");
    NumberFormat *nf = NumberFormat::createInstance(en, status);
    MeasureFormat fmt("en", UMEASFMT_WIDTH_WIDE, nf, status);
    UnicodeString appendTo;
    FieldPosition pos(FieldPosition::DONT_CARE);
    if (U_FAILURE(status)) {
        dataerrln("Error creating formatter - %s", u_errorName(status));
        return;
    }
    nf->setMinimumFractionDigits(2);
    nf->setMaximumFractionDigits(2);
    fmt.formatMeasures(measures, UPRV_LENGTHOF(measures), appendTo, pos, status);
    if (!assertSuccess("Error formatting", status)) {
        return;
    }
    assertEquals(
            "TestDoubleZero",
            UnicodeString("4 hours, 23 minutes, 16.00 seconds"),
            appendTo);
    measures[0] = Measure(-4.7, MeasureUnit::createHour(status), status);
    appendTo.remove();
    fmt.formatMeasures(measures, UPRV_LENGTHOF(measures), appendTo, pos, status);
    if (!assertSuccess("Error formatting", status)) {
        return;
    }
    assertEquals(
            "TestDoubleZero",
            UnicodeString("-4 hours, 23 minutes, 16.00 seconds"),
            appendTo);
}

void MeasureFormatTest::verifyFieldPosition(
        const char *description,
        const MeasureFormat &fmt,
        const UnicodeString &prefix,
        const Measure *measures,
        int32_t measureCount,
        NumberFormat::EAlignmentFields field,
        int32_t start,
        int32_t end) {
    // 8 char lead
    UnicodeString result(prefix);
    FieldPosition pos(field);
    UErrorCode status = U_ZERO_ERROR;
    CharString ch;
    const char *descPrefix = ch.append(description, status)
            .append(": ", status).data();
    CharString beginIndex;
    beginIndex.append(descPrefix, status).append("beginIndex", status);
    CharString endIndex;
    endIndex.append(descPrefix, status).append("endIndex", status);
    fmt.formatMeasures(measures, measureCount, result, pos, status);
    if (!assertSuccess("Error formatting", status)) {
        return;
    }
    assertEquals(beginIndex.data(), start, pos.getBeginIndex());
    assertEquals(endIndex.data(), end, pos.getEndIndex());
}

void MeasureFormatTest::verifyFieldPositionWithPer(
        const char *description,
        const MeasureFormat &fmt,
        const UnicodeString &prefix,
        const Measure *measures,
        int32_t measureCount,
        const MeasureUnit &perUnit,
        NumberFormat::EAlignmentFields field,
        int32_t start,
        int32_t end) {
    UnicodeString result(prefix);
    FieldPosition pos(field);
    UErrorCode status = U_ZERO_ERROR;
    CharString ch;
    const char *descPrefix = ch.append(description, status)
            .append(": ", status).data();
    CharString beginIndex;
    beginIndex.append(descPrefix, status).append("beginIndex", status);
    CharString endIndex;
    endIndex.append(descPrefix, status).append("endIndex", status);
    fmt.formatMeasuresPer(
            measures,
            measureCount,
            perUnit,
            result,
            pos,
            status);
    if (!assertSuccess("Error formatting", status)) {
        return;
    }
    assertEquals(beginIndex.data(), start, pos.getBeginIndex());
    assertEquals(endIndex.data(), end, pos.getEndIndex());
}

void MeasureFormatTest::verifyFormat(
        const char *description,
        const MeasureFormat &fmt,
        const Measure *measures,
        int32_t measureCount,
        const char *expected) {
    verifyFormatWithPrefix(
            description,
            fmt,
            "",
            measures,
            measureCount,
            expected);
}

void MeasureFormatTest::verifyFormatWithPrefix(
        const char *description,
        const MeasureFormat &fmt,
        const UnicodeString &prefix,
        const Measure *measures,
        int32_t measureCount,
        const char *expected) {
    UnicodeString result(prefix);
    FieldPosition pos(0);
    UErrorCode status = U_ZERO_ERROR;
    fmt.formatMeasures(measures, measureCount, result, pos, status);
    if (!assertSuccess("Error formatting", status)) {
        return;
    }
    assertEquals(description, UnicodeString(expected).unescape(), result);
}

void MeasureFormatTest::verifyFormat(
        const char *description,
        const MeasureFormat &fmt,
        const ExpectedResult *expectedResults,
        int32_t count) {
    for (int32_t i = 0; i < count; ++i) {
        verifyFormat(description, fmt, expectedResults[i].measures, expectedResults[i].count, expectedResults[i].expected);
    }
}

extern IntlTest *createMeasureFormatTest() {
    return new MeasureFormatTest();
}

#endif

