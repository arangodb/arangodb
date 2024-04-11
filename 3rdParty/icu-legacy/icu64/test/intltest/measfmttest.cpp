// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
* Copyright (C) 2014-2016, International Business Machines Corporation and    *
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

#include "charstr.h"
#include "cstr.h"
#include "cstring.h"
#include "measunit_impl.h"
#include "unicode/decimfmt.h"
#include "unicode/measfmt.h"
#include "unicode/measure.h"
#include "unicode/measunit.h"
#include "unicode/strenum.h"
#include "unicode/tmunit.h"
#include "unicode/plurrule.h"
#include "unicode/ustring.h"
#include "unicode/reldatefmt.h"
#include "unicode/rbnf.h"

struct ExpectedResult {
    const Measure *measures;
    int32_t count;
    const char *expected;
};

class MeasureFormatTest : public IntlTest {
public:
    MeasureFormatTest() {
    }

    void runIndexedTest(int32_t index, UBool exec, const char*& name, char* par = nullptr) override;

private:
    void TestBasic();
    void TestCompatible53();
    void TestCompatible54();
    void TestCompatible55();
    void TestCompatible56();
    void TestCompatible57();
    void TestCompatible58();
    void TestCompatible59();
    void TestCompatible63();
    void TestCompatible64();
    void TestCompatible65();
    void TestCompatible68();
    void TestCompatible69();
    void TestCompatible70();
    void TestCompatible72();
    void TestCompatible73();
    void TestCompatible74();
    void TestGetAvailable();
    void TestExamplesInDocs();
    void TestFormatPeriodEn();
    void Test10219FractionalPlurals();
    void TestGreek();
    void TestFormatSingleArg();
    void TestFormatMeasuresZeroArg();
    void TestSimplePer();
    void TestNumeratorPlurals();
    void TestMultiples();
    void TestManyLocaleDurations();
    void TestGram();
    void TestCurrencies();
    void TestDisplayNames();
    void TestFieldPosition();
    void TestFieldPositionMultiple();
    void TestBadArg();
    void TestEquality();
    void TestGroupingSeparator();
    void TestDoubleZero();
    void TestUnitPerUnitResolution();
    void TestIndividualPluralFallback();
    void Test20332_PersonUnits();
    void TestNumericTime();
    void TestNumericTimeSomeSpecialFormats();
    void TestIdentifiers();
    void TestInvalidIdentifiers();
    void TestIdentifierDetails();
    void TestPrefixes();
    void TestParseBuiltIns();
    void TestParseToBuiltIn();
    void TestKilogramIdentifier();
    void TestCompoundUnitOperations();
    void TestDimensionlessBehaviour();
    void Test21060_AddressSanitizerProblem();
    void Test21223_FrenchDuration();
    void TestInternalMeasureUnitImpl();
    void TestMeasureEquality();

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
    void helperTestSimplePer(
        const Locale &locale,
        UMeasureFormatWidth width,
        double value,
        const MeasureUnit &unit,
        const MeasureUnit &perUnit,
        const char *expected,
        int32_t field,
        int32_t expected_start,
        int32_t expected_end);
    void helperTestMultiples(
        const Locale &locale,
        UMeasureFormatWidth width,
        const char *expected);
    void helperTestManyLocaleDurations(
        const char *localeID,
        UMeasureFormatWidth width,
        const Measure *measures,
        int32_t measureCount,
        const char *expected);
    void helperTestDisplayName(
        const MeasureUnit *unit,
        const char *localeID,
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
    void verifySingleUnit(
        const MeasureUnit& unit,
        UMeasurePrefix unitPrefix,
        int8_t power,
        const char* identifier);
    void verifyCompoundUnit(
        const MeasureUnit& unit,
        const char* identifier,
        const char** subIdentifiers,
        int32_t subIdentifierCount);
    void verifyMixedUnit(
        const MeasureUnit& unit,
        const char* identifier,
        const char** subIdentifiers,
        int32_t subIdentifierCount);
};

void MeasureFormatTest::runIndexedTest(
        int32_t index, UBool exec, const char *&name, char *) {
    if (exec) {
        logln("TestSuite MeasureFormatTest: ");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(TestBasic);
    TESTCASE_AUTO(TestCompatible53);
    TESTCASE_AUTO(TestCompatible54);
    TESTCASE_AUTO(TestCompatible55);
    TESTCASE_AUTO(TestCompatible56);
    TESTCASE_AUTO(TestCompatible57);
    TESTCASE_AUTO(TestCompatible58);
    TESTCASE_AUTO(TestCompatible59);
    TESTCASE_AUTO(TestCompatible63);
    TESTCASE_AUTO(TestCompatible64);
    TESTCASE_AUTO(TestCompatible65);
    TESTCASE_AUTO(TestCompatible68);
    TESTCASE_AUTO(TestCompatible69);
    TESTCASE_AUTO(TestCompatible70);
    TESTCASE_AUTO(TestCompatible72);
    TESTCASE_AUTO(TestCompatible73);
    TESTCASE_AUTO(TestCompatible74);
    TESTCASE_AUTO(TestGetAvailable);
    TESTCASE_AUTO(TestExamplesInDocs);
    TESTCASE_AUTO(TestFormatPeriodEn);
    TESTCASE_AUTO(Test10219FractionalPlurals);
    TESTCASE_AUTO(TestGreek);
    TESTCASE_AUTO(TestFormatSingleArg);
    TESTCASE_AUTO(TestFormatMeasuresZeroArg);
    TESTCASE_AUTO(TestSimplePer);
    TESTCASE_AUTO(TestNumeratorPlurals);
    TESTCASE_AUTO(TestMultiples);
    TESTCASE_AUTO(TestManyLocaleDurations);
    TESTCASE_AUTO(TestGram);
    TESTCASE_AUTO(TestCurrencies);
    TESTCASE_AUTO(TestDisplayNames);
    TESTCASE_AUTO(TestFieldPosition);
    TESTCASE_AUTO(TestFieldPositionMultiple);
    TESTCASE_AUTO(TestBadArg);
    TESTCASE_AUTO(TestEquality);
    TESTCASE_AUTO(TestGroupingSeparator);
    TESTCASE_AUTO(TestDoubleZero);
    TESTCASE_AUTO(TestUnitPerUnitResolution);
    TESTCASE_AUTO(TestIndividualPluralFallback);
    TESTCASE_AUTO(Test20332_PersonUnits);
    TESTCASE_AUTO(TestNumericTime);
    TESTCASE_AUTO(TestNumericTimeSomeSpecialFormats);
    TESTCASE_AUTO(TestIdentifiers);
    TESTCASE_AUTO(TestInvalidIdentifiers);
    TESTCASE_AUTO(TestIdentifierDetails);
    TESTCASE_AUTO(TestPrefixes);
    TESTCASE_AUTO(TestParseBuiltIns);
    TESTCASE_AUTO(TestParseToBuiltIn);
    TESTCASE_AUTO(TestKilogramIdentifier);
    TESTCASE_AUTO(TestCompoundUnitOperations);
    TESTCASE_AUTO(TestDimensionlessBehaviour);
    TESTCASE_AUTO(Test21060_AddressSanitizerProblem);
    TESTCASE_AUTO(Test21223_FrenchDuration);
    TESTCASE_AUTO(TestInternalMeasureUnitImpl);
    TESTCASE_AUTO(TestMeasureEquality);
    TESTCASE_AUTO_END;
}

void MeasureFormatTest::TestCompatible53() {
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

void MeasureFormatTest::TestCompatible54() {
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

void MeasureFormatTest::TestCompatible55() {
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
    measureUnit.adoptInstead(MeasureUnit::createGenericTemperature(status));
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

void MeasureFormatTest::TestCompatible56() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecondSquared(status));
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnit.adoptInstead(MeasureUnit::createRadian(status));
    measureUnit.adoptInstead(MeasureUnit::createRevolutionAngle(status));
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareCentimeter(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareInch(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareYard(status));
    measureUnit.adoptInstead(MeasureUnit::createLiterPer100Kilometers(status));
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
    measureUnit.adoptInstead(MeasureUnit::createCentury(status));
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
    measureUnit.adoptInstead(MeasureUnit::createMileScandinavian(status));
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
    measureUnit.adoptInstead(MeasureUnit::createKnot(status));
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnit.adoptInstead(MeasureUnit::createGenericTemperature(status));
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
    measureUnit.adoptInstead(MeasureUnit::createCupMetric(status));
    measureUnit.adoptInstead(MeasureUnit::createDeciliter(status));
    measureUnit.adoptInstead(MeasureUnit::createFluidOunce(status));
    measureUnit.adoptInstead(MeasureUnit::createGallon(status));
    measureUnit.adoptInstead(MeasureUnit::createHectoliter(status));
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    measureUnit.adoptInstead(MeasureUnit::createMegaliter(status));
    measureUnit.adoptInstead(MeasureUnit::createMilliliter(status));
    measureUnit.adoptInstead(MeasureUnit::createPint(status));
    measureUnit.adoptInstead(MeasureUnit::createPintMetric(status));
    measureUnit.adoptInstead(MeasureUnit::createQuart(status));
    measureUnit.adoptInstead(MeasureUnit::createTablespoon(status));
    measureUnit.adoptInstead(MeasureUnit::createTeaspoon(status));
    assertSuccess("", status);
}

void MeasureFormatTest::TestCompatible57() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecondSquared(status));
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnit.adoptInstead(MeasureUnit::createRadian(status));
    measureUnit.adoptInstead(MeasureUnit::createRevolutionAngle(status));
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareCentimeter(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareInch(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareYard(status));
    measureUnit.adoptInstead(MeasureUnit::createKarat(status));
    measureUnit.adoptInstead(MeasureUnit::createMilligramPerDeciliter(status));
    measureUnit.adoptInstead(MeasureUnit::createMillimolePerLiter(status));
    measureUnit.adoptInstead(MeasureUnit::createPartPerMillion(status));
    measureUnit.adoptInstead(MeasureUnit::createLiterPer100Kilometers(status));
    measureUnit.adoptInstead(MeasureUnit::createLiterPerKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallon(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallonImperial(status));
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
    measureUnit.adoptInstead(MeasureUnit::createCentury(status));
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
    measureUnit.adoptInstead(MeasureUnit::createMileScandinavian(status));
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
    measureUnit.adoptInstead(MeasureUnit::createKilometerPerHour(status));
    measureUnit.adoptInstead(MeasureUnit::createKnot(status));
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnit.adoptInstead(MeasureUnit::createGenericTemperature(status));
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
    measureUnit.adoptInstead(MeasureUnit::createCupMetric(status));
    measureUnit.adoptInstead(MeasureUnit::createDeciliter(status));
    measureUnit.adoptInstead(MeasureUnit::createFluidOunce(status));
    measureUnit.adoptInstead(MeasureUnit::createGallon(status));
    measureUnit.adoptInstead(MeasureUnit::createGallonImperial(status));
    measureUnit.adoptInstead(MeasureUnit::createHectoliter(status));
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    measureUnit.adoptInstead(MeasureUnit::createMegaliter(status));
    measureUnit.adoptInstead(MeasureUnit::createMilliliter(status));
    measureUnit.adoptInstead(MeasureUnit::createPint(status));
    measureUnit.adoptInstead(MeasureUnit::createPintMetric(status));
    measureUnit.adoptInstead(MeasureUnit::createQuart(status));
    measureUnit.adoptInstead(MeasureUnit::createTablespoon(status));
    measureUnit.adoptInstead(MeasureUnit::createTeaspoon(status));
    assertSuccess("", status);
}

void MeasureFormatTest::TestCompatible58() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecondSquared(status));
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnit.adoptInstead(MeasureUnit::createRadian(status));
    measureUnit.adoptInstead(MeasureUnit::createRevolutionAngle(status));
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareCentimeter(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareInch(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareYard(status));
    measureUnit.adoptInstead(MeasureUnit::createKarat(status));
    measureUnit.adoptInstead(MeasureUnit::createMilligramPerDeciliter(status));
    measureUnit.adoptInstead(MeasureUnit::createMillimolePerLiter(status));
    measureUnit.adoptInstead(MeasureUnit::createPartPerMillion(status));
    measureUnit.adoptInstead(MeasureUnit::createLiterPer100Kilometers(status));
    measureUnit.adoptInstead(MeasureUnit::createLiterPerKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallon(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallonImperial(status));
    // measureUnit.adoptInstead(MeasureUnit::createEast(status));
    // measureUnit.adoptInstead(MeasureUnit::createNorth(status));
    // measureUnit.adoptInstead(MeasureUnit::createSouth(status));
    // measureUnit.adoptInstead(MeasureUnit::createWest(status));
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
    measureUnit.adoptInstead(MeasureUnit::createCentury(status));
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
    measureUnit.adoptInstead(MeasureUnit::createMileScandinavian(status));
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
    measureUnit.adoptInstead(MeasureUnit::createKilometerPerHour(status));
    measureUnit.adoptInstead(MeasureUnit::createKnot(status));
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnit.adoptInstead(MeasureUnit::createGenericTemperature(status));
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
    measureUnit.adoptInstead(MeasureUnit::createCupMetric(status));
    measureUnit.adoptInstead(MeasureUnit::createDeciliter(status));
    measureUnit.adoptInstead(MeasureUnit::createFluidOunce(status));
    measureUnit.adoptInstead(MeasureUnit::createGallon(status));
    measureUnit.adoptInstead(MeasureUnit::createGallonImperial(status));
    measureUnit.adoptInstead(MeasureUnit::createHectoliter(status));
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    measureUnit.adoptInstead(MeasureUnit::createMegaliter(status));
    measureUnit.adoptInstead(MeasureUnit::createMilliliter(status));
    measureUnit.adoptInstead(MeasureUnit::createPint(status));
    measureUnit.adoptInstead(MeasureUnit::createPintMetric(status));
    measureUnit.adoptInstead(MeasureUnit::createQuart(status));
    measureUnit.adoptInstead(MeasureUnit::createTablespoon(status));
    measureUnit.adoptInstead(MeasureUnit::createTeaspoon(status));
    assertSuccess("", status);
}

void MeasureFormatTest::TestCompatible59() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecondSquared(status));
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnit.adoptInstead(MeasureUnit::createRadian(status));
    measureUnit.adoptInstead(MeasureUnit::createRevolutionAngle(status));
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareCentimeter(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareInch(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareYard(status));
    measureUnit.adoptInstead(MeasureUnit::createKarat(status));
    measureUnit.adoptInstead(MeasureUnit::createMilligramPerDeciliter(status));
    measureUnit.adoptInstead(MeasureUnit::createMillimolePerLiter(status));
    measureUnit.adoptInstead(MeasureUnit::createPartPerMillion(status));
    measureUnit.adoptInstead(MeasureUnit::createLiterPer100Kilometers(status));
    measureUnit.adoptInstead(MeasureUnit::createLiterPerKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallon(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallonImperial(status));
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
    measureUnit.adoptInstead(MeasureUnit::createCentury(status));
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
    measureUnit.adoptInstead(MeasureUnit::createMileScandinavian(status));
    measureUnit.adoptInstead(MeasureUnit::createMillimeter(status));
    measureUnit.adoptInstead(MeasureUnit::createNanometer(status));
    measureUnit.adoptInstead(MeasureUnit::createNauticalMile(status));
    measureUnit.adoptInstead(MeasureUnit::createParsec(status));
    measureUnit.adoptInstead(MeasureUnit::createPicometer(status));
    measureUnit.adoptInstead(MeasureUnit::createPoint(status));
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
    measureUnit.adoptInstead(MeasureUnit::createKilometerPerHour(status));
    measureUnit.adoptInstead(MeasureUnit::createKnot(status));
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnit.adoptInstead(MeasureUnit::createGenericTemperature(status));
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
    measureUnit.adoptInstead(MeasureUnit::createCupMetric(status));
    measureUnit.adoptInstead(MeasureUnit::createDeciliter(status));
    measureUnit.adoptInstead(MeasureUnit::createFluidOunce(status));
    measureUnit.adoptInstead(MeasureUnit::createGallon(status));
    measureUnit.adoptInstead(MeasureUnit::createGallonImperial(status));
    measureUnit.adoptInstead(MeasureUnit::createHectoliter(status));
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    measureUnit.adoptInstead(MeasureUnit::createMegaliter(status));
    measureUnit.adoptInstead(MeasureUnit::createMilliliter(status));
    measureUnit.adoptInstead(MeasureUnit::createPint(status));
    measureUnit.adoptInstead(MeasureUnit::createPintMetric(status));
    measureUnit.adoptInstead(MeasureUnit::createQuart(status));
    measureUnit.adoptInstead(MeasureUnit::createTablespoon(status));
    measureUnit.adoptInstead(MeasureUnit::createTeaspoon(status));
    assertSuccess("", status);
}

// Note that TestCompatible60(), TestCompatible61(), TestCompatible62()
// would be the same as TestCompatible59(), no need to add them.

void MeasureFormatTest::TestCompatible63() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecondSquared(status));
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnit.adoptInstead(MeasureUnit::createRadian(status));
    measureUnit.adoptInstead(MeasureUnit::createRevolutionAngle(status));
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareCentimeter(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareInch(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnit.adoptInstead(MeasureUnit::createSquareYard(status));
    measureUnit.adoptInstead(MeasureUnit::createKarat(status));
    measureUnit.adoptInstead(MeasureUnit::createMilligramPerDeciliter(status));
    measureUnit.adoptInstead(MeasureUnit::createMillimolePerLiter(status));
    measureUnit.adoptInstead(MeasureUnit::createPartPerMillion(status));
    measureUnit.adoptInstead(MeasureUnit::createPercent(status));
    measureUnit.adoptInstead(MeasureUnit::createPermille(status));
    measureUnit.adoptInstead(MeasureUnit::createLiterPer100Kilometers(status));
    measureUnit.adoptInstead(MeasureUnit::createLiterPerKilometer(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallon(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallonImperial(status));
    measureUnit.adoptInstead(MeasureUnit::createBit(status));
    measureUnit.adoptInstead(MeasureUnit::createByte(status));
    measureUnit.adoptInstead(MeasureUnit::createGigabit(status));
    measureUnit.adoptInstead(MeasureUnit::createGigabyte(status));
    measureUnit.adoptInstead(MeasureUnit::createKilobit(status));
    measureUnit.adoptInstead(MeasureUnit::createKilobyte(status));
    measureUnit.adoptInstead(MeasureUnit::createMegabit(status));
    measureUnit.adoptInstead(MeasureUnit::createMegabyte(status));
    measureUnit.adoptInstead(MeasureUnit::createPetabyte(status));
    measureUnit.adoptInstead(MeasureUnit::createTerabit(status));
    measureUnit.adoptInstead(MeasureUnit::createTerabyte(status));
    measureUnit.adoptInstead(MeasureUnit::createCentury(status));
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
    measureUnit.adoptInstead(MeasureUnit::createMileScandinavian(status));
    measureUnit.adoptInstead(MeasureUnit::createMillimeter(status));
    measureUnit.adoptInstead(MeasureUnit::createNanometer(status));
    measureUnit.adoptInstead(MeasureUnit::createNauticalMile(status));
    measureUnit.adoptInstead(MeasureUnit::createParsec(status));
    measureUnit.adoptInstead(MeasureUnit::createPicometer(status));
    measureUnit.adoptInstead(MeasureUnit::createPoint(status));
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
    measureUnit.adoptInstead(MeasureUnit::createAtmosphere(status));
    measureUnit.adoptInstead(MeasureUnit::createHectopascal(status));
    measureUnit.adoptInstead(MeasureUnit::createInchHg(status));
    measureUnit.adoptInstead(MeasureUnit::createMillibar(status));
    measureUnit.adoptInstead(MeasureUnit::createMillimeterOfMercury(status));
    measureUnit.adoptInstead(MeasureUnit::createPoundPerSquareInch(status));
    measureUnit.adoptInstead(MeasureUnit::createKilometerPerHour(status));
    measureUnit.adoptInstead(MeasureUnit::createKnot(status));
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnit.adoptInstead(MeasureUnit::createGenericTemperature(status));
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
    measureUnit.adoptInstead(MeasureUnit::createCupMetric(status));
    measureUnit.adoptInstead(MeasureUnit::createDeciliter(status));
    measureUnit.adoptInstead(MeasureUnit::createFluidOunce(status));
    measureUnit.adoptInstead(MeasureUnit::createGallon(status));
    measureUnit.adoptInstead(MeasureUnit::createGallonImperial(status));
    measureUnit.adoptInstead(MeasureUnit::createHectoliter(status));
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    measureUnit.adoptInstead(MeasureUnit::createMegaliter(status));
    measureUnit.adoptInstead(MeasureUnit::createMilliliter(status));
    measureUnit.adoptInstead(MeasureUnit::createPint(status));
    measureUnit.adoptInstead(MeasureUnit::createPintMetric(status));
    measureUnit.adoptInstead(MeasureUnit::createQuart(status));
    measureUnit.adoptInstead(MeasureUnit::createTablespoon(status));
    measureUnit.adoptInstead(MeasureUnit::createTeaspoon(status));
    assertSuccess("", status);
}

void MeasureFormatTest::TestCompatible64() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    MeasureUnit measureUnitValue;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnitValue = MeasureUnit::getGForce();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecondSquared(status));
    measureUnitValue = MeasureUnit::getMeterPerSecondSquared();
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnitValue = MeasureUnit::getArcMinute();
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnitValue = MeasureUnit::getArcSecond();
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnitValue = MeasureUnit::getDegree();
    measureUnit.adoptInstead(MeasureUnit::createRadian(status));
    measureUnitValue = MeasureUnit::getRadian();
    measureUnit.adoptInstead(MeasureUnit::createRevolutionAngle(status));
    measureUnitValue = MeasureUnit::getRevolutionAngle();
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnitValue = MeasureUnit::getAcre();
    measureUnit.adoptInstead(MeasureUnit::createDunam(status));
    measureUnitValue = MeasureUnit::getDunam();
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnitValue = MeasureUnit::getHectare();
    measureUnit.adoptInstead(MeasureUnit::createSquareCentimeter(status));
    measureUnitValue = MeasureUnit::getSquareCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnitValue = MeasureUnit::getSquareFoot();
    measureUnit.adoptInstead(MeasureUnit::createSquareInch(status));
    measureUnitValue = MeasureUnit::getSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnitValue = MeasureUnit::getSquareKilometer();
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnitValue = MeasureUnit::getSquareMeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnitValue = MeasureUnit::getSquareMile();
    measureUnit.adoptInstead(MeasureUnit::createSquareYard(status));
    measureUnitValue = MeasureUnit::getSquareYard();
    measureUnit.adoptInstead(MeasureUnit::createKarat(status));
    measureUnitValue = MeasureUnit::getKarat();
    measureUnit.adoptInstead(MeasureUnit::createMilligramPerDeciliter(status));
    measureUnitValue = MeasureUnit::getMilligramPerDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createMillimolePerLiter(status));
    measureUnitValue = MeasureUnit::getMillimolePerLiter();
    measureUnit.adoptInstead(MeasureUnit::createMole(status));
    measureUnitValue = MeasureUnit::getMole();
    measureUnit.adoptInstead(MeasureUnit::createPartPerMillion(status));
    measureUnitValue = MeasureUnit::getPartPerMillion();
    measureUnit.adoptInstead(MeasureUnit::createPercent(status));
    measureUnitValue = MeasureUnit::getPercent();
    measureUnit.adoptInstead(MeasureUnit::createPermille(status));
    measureUnitValue = MeasureUnit::getPermille();
    measureUnit.adoptInstead(MeasureUnit::createPermyriad(status));
    measureUnitValue = MeasureUnit::getPermyriad();
    measureUnit.adoptInstead(MeasureUnit::createLiterPer100Kilometers(status));
    measureUnitValue = MeasureUnit::getLiterPer100Kilometers();
    measureUnit.adoptInstead(MeasureUnit::createLiterPerKilometer(status));
    measureUnitValue = MeasureUnit::getLiterPerKilometer();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallon(status));
    measureUnitValue = MeasureUnit::getMilePerGallon();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallonImperial(status));
    measureUnitValue = MeasureUnit::getMilePerGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createBit(status));
    measureUnitValue = MeasureUnit::getBit();
    measureUnit.adoptInstead(MeasureUnit::createByte(status));
    measureUnitValue = MeasureUnit::getByte();
    measureUnit.adoptInstead(MeasureUnit::createGigabit(status));
    measureUnitValue = MeasureUnit::getGigabit();
    measureUnit.adoptInstead(MeasureUnit::createGigabyte(status));
    measureUnitValue = MeasureUnit::getGigabyte();
    measureUnit.adoptInstead(MeasureUnit::createKilobit(status));
    measureUnitValue = MeasureUnit::getKilobit();
    measureUnit.adoptInstead(MeasureUnit::createKilobyte(status));
    measureUnitValue = MeasureUnit::getKilobyte();
    measureUnit.adoptInstead(MeasureUnit::createMegabit(status));
    measureUnitValue = MeasureUnit::getMegabit();
    measureUnit.adoptInstead(MeasureUnit::createMegabyte(status));
    measureUnitValue = MeasureUnit::getMegabyte();
    measureUnit.adoptInstead(MeasureUnit::createPetabyte(status));
    measureUnitValue = MeasureUnit::getPetabyte();
    measureUnit.adoptInstead(MeasureUnit::createTerabit(status));
    measureUnitValue = MeasureUnit::getTerabit();
    measureUnit.adoptInstead(MeasureUnit::createTerabyte(status));
    measureUnitValue = MeasureUnit::getTerabyte();
    measureUnit.adoptInstead(MeasureUnit::createCentury(status));
    measureUnitValue = MeasureUnit::getCentury();
    measureUnit.adoptInstead(MeasureUnit::createDay(status));
    measureUnitValue = MeasureUnit::getDay();
    measureUnit.adoptInstead(MeasureUnit::createDayPerson(status));
    measureUnitValue = MeasureUnit::getDayPerson();
    measureUnit.adoptInstead(MeasureUnit::createHour(status));
    measureUnitValue = MeasureUnit::getHour();
    measureUnit.adoptInstead(MeasureUnit::createMicrosecond(status));
    measureUnitValue = MeasureUnit::getMicrosecond();
    measureUnit.adoptInstead(MeasureUnit::createMillisecond(status));
    measureUnitValue = MeasureUnit::getMillisecond();
    measureUnit.adoptInstead(MeasureUnit::createMinute(status));
    measureUnitValue = MeasureUnit::getMinute();
    measureUnit.adoptInstead(MeasureUnit::createMonth(status));
    measureUnitValue = MeasureUnit::getMonth();
    measureUnit.adoptInstead(MeasureUnit::createMonthPerson(status));
    measureUnitValue = MeasureUnit::getMonthPerson();
    measureUnit.adoptInstead(MeasureUnit::createNanosecond(status));
    measureUnitValue = MeasureUnit::getNanosecond();
    measureUnit.adoptInstead(MeasureUnit::createSecond(status));
    measureUnitValue = MeasureUnit::getSecond();
    measureUnit.adoptInstead(MeasureUnit::createWeek(status));
    measureUnitValue = MeasureUnit::getWeek();
    measureUnit.adoptInstead(MeasureUnit::createWeekPerson(status));
    measureUnitValue = MeasureUnit::getWeekPerson();
    measureUnit.adoptInstead(MeasureUnit::createYear(status));
    measureUnitValue = MeasureUnit::getYear();
    measureUnit.adoptInstead(MeasureUnit::createYearPerson(status));
    measureUnitValue = MeasureUnit::getYearPerson();
    measureUnit.adoptInstead(MeasureUnit::createAmpere(status));
    measureUnitValue = MeasureUnit::getAmpere();
    measureUnit.adoptInstead(MeasureUnit::createMilliampere(status));
    measureUnitValue = MeasureUnit::getMilliampere();
    measureUnit.adoptInstead(MeasureUnit::createOhm(status));
    measureUnitValue = MeasureUnit::getOhm();
    measureUnit.adoptInstead(MeasureUnit::createVolt(status));
    measureUnitValue = MeasureUnit::getVolt();
    measureUnit.adoptInstead(MeasureUnit::createBritishThermalUnit(status));
    measureUnitValue = MeasureUnit::getBritishThermalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCalorie(status));
    measureUnitValue = MeasureUnit::getCalorie();
    measureUnit.adoptInstead(MeasureUnit::createElectronvolt(status));
    measureUnitValue = MeasureUnit::getElectronvolt();
    measureUnit.adoptInstead(MeasureUnit::createFoodcalorie(status));
    measureUnitValue = MeasureUnit::getFoodcalorie();
    measureUnit.adoptInstead(MeasureUnit::createJoule(status));
    measureUnitValue = MeasureUnit::getJoule();
    measureUnit.adoptInstead(MeasureUnit::createKilocalorie(status));
    measureUnitValue = MeasureUnit::getKilocalorie();
    measureUnit.adoptInstead(MeasureUnit::createKilojoule(status));
    measureUnitValue = MeasureUnit::getKilojoule();
    measureUnit.adoptInstead(MeasureUnit::createKilowattHour(status));
    measureUnitValue = MeasureUnit::getKilowattHour();
    measureUnit.adoptInstead(MeasureUnit::createNewton(status));
    measureUnitValue = MeasureUnit::getNewton();
    measureUnit.adoptInstead(MeasureUnit::createPoundForce(status));
    measureUnitValue = MeasureUnit::getPoundForce();
    measureUnit.adoptInstead(MeasureUnit::createGigahertz(status));
    measureUnitValue = MeasureUnit::getGigahertz();
    measureUnit.adoptInstead(MeasureUnit::createHertz(status));
    measureUnitValue = MeasureUnit::getHertz();
    measureUnit.adoptInstead(MeasureUnit::createKilohertz(status));
    measureUnitValue = MeasureUnit::getKilohertz();
    measureUnit.adoptInstead(MeasureUnit::createMegahertz(status));
    measureUnitValue = MeasureUnit::getMegahertz();
    measureUnit.adoptInstead(MeasureUnit::createAstronomicalUnit(status));
    measureUnitValue = MeasureUnit::getAstronomicalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCentimeter(status));
    measureUnitValue = MeasureUnit::getCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDecimeter(status));
    measureUnitValue = MeasureUnit::getDecimeter();
    measureUnit.adoptInstead(MeasureUnit::createFathom(status));
    measureUnitValue = MeasureUnit::getFathom();
    measureUnit.adoptInstead(MeasureUnit::createFoot(status));
    measureUnitValue = MeasureUnit::getFoot();
    measureUnit.adoptInstead(MeasureUnit::createFurlong(status));
    measureUnitValue = MeasureUnit::getFurlong();
    measureUnit.adoptInstead(MeasureUnit::createInch(status));
    measureUnitValue = MeasureUnit::getInch();
    measureUnit.adoptInstead(MeasureUnit::createKilometer(status));
    measureUnitValue = MeasureUnit::getKilometer();
    measureUnit.adoptInstead(MeasureUnit::createLightYear(status));
    measureUnitValue = MeasureUnit::getLightYear();
    measureUnit.adoptInstead(MeasureUnit::createMeter(status));
    measureUnitValue = MeasureUnit::getMeter();
    measureUnit.adoptInstead(MeasureUnit::createMicrometer(status));
    measureUnitValue = MeasureUnit::getMicrometer();
    measureUnit.adoptInstead(MeasureUnit::createMile(status));
    measureUnitValue = MeasureUnit::getMile();
    measureUnit.adoptInstead(MeasureUnit::createMileScandinavian(status));
    measureUnitValue = MeasureUnit::getMileScandinavian();
    measureUnit.adoptInstead(MeasureUnit::createMillimeter(status));
    measureUnitValue = MeasureUnit::getMillimeter();
    measureUnit.adoptInstead(MeasureUnit::createNanometer(status));
    measureUnitValue = MeasureUnit::getNanometer();
    measureUnit.adoptInstead(MeasureUnit::createNauticalMile(status));
    measureUnitValue = MeasureUnit::getNauticalMile();
    measureUnit.adoptInstead(MeasureUnit::createParsec(status));
    measureUnitValue = MeasureUnit::getParsec();
    measureUnit.adoptInstead(MeasureUnit::createPicometer(status));
    measureUnitValue = MeasureUnit::getPicometer();
    measureUnit.adoptInstead(MeasureUnit::createPoint(status));
    measureUnitValue = MeasureUnit::getPoint();
    measureUnit.adoptInstead(MeasureUnit::createSolarRadius(status));
    measureUnitValue = MeasureUnit::getSolarRadius();
    measureUnit.adoptInstead(MeasureUnit::createYard(status));
    measureUnitValue = MeasureUnit::getYard();
    measureUnit.adoptInstead(MeasureUnit::createLux(status));
    measureUnitValue = MeasureUnit::getLux();
    measureUnit.adoptInstead(MeasureUnit::createSolarLuminosity(status));
    measureUnitValue = MeasureUnit::getSolarLuminosity();
    measureUnit.adoptInstead(MeasureUnit::createCarat(status));
    measureUnitValue = MeasureUnit::getCarat();
    measureUnit.adoptInstead(MeasureUnit::createDalton(status));
    measureUnitValue = MeasureUnit::getDalton();
    measureUnit.adoptInstead(MeasureUnit::createEarthMass(status));
    measureUnitValue = MeasureUnit::getEarthMass();
    measureUnit.adoptInstead(MeasureUnit::createGram(status));
    measureUnitValue = MeasureUnit::getGram();
    measureUnit.adoptInstead(MeasureUnit::createKilogram(status));
    measureUnitValue = MeasureUnit::getKilogram();
    measureUnit.adoptInstead(MeasureUnit::createMetricTon(status));
    measureUnitValue = MeasureUnit::getMetricTon();
    measureUnit.adoptInstead(MeasureUnit::createMicrogram(status));
    measureUnitValue = MeasureUnit::getMicrogram();
    measureUnit.adoptInstead(MeasureUnit::createMilligram(status));
    measureUnitValue = MeasureUnit::getMilligram();
    measureUnit.adoptInstead(MeasureUnit::createOunce(status));
    measureUnitValue = MeasureUnit::getOunce();
    measureUnit.adoptInstead(MeasureUnit::createOunceTroy(status));
    measureUnitValue = MeasureUnit::getOunceTroy();
    measureUnit.adoptInstead(MeasureUnit::createPound(status));
    measureUnitValue = MeasureUnit::getPound();
    measureUnit.adoptInstead(MeasureUnit::createSolarMass(status));
    measureUnitValue = MeasureUnit::getSolarMass();
    measureUnit.adoptInstead(MeasureUnit::createStone(status));
    measureUnitValue = MeasureUnit::getStone();
    measureUnit.adoptInstead(MeasureUnit::createTon(status));
    measureUnitValue = MeasureUnit::getTon();
    measureUnit.adoptInstead(MeasureUnit::createGigawatt(status));
    measureUnitValue = MeasureUnit::getGigawatt();
    measureUnit.adoptInstead(MeasureUnit::createHorsepower(status));
    measureUnitValue = MeasureUnit::getHorsepower();
    measureUnit.adoptInstead(MeasureUnit::createKilowatt(status));
    measureUnitValue = MeasureUnit::getKilowatt();
    measureUnit.adoptInstead(MeasureUnit::createMegawatt(status));
    measureUnitValue = MeasureUnit::getMegawatt();
    measureUnit.adoptInstead(MeasureUnit::createMilliwatt(status));
    measureUnitValue = MeasureUnit::getMilliwatt();
    measureUnit.adoptInstead(MeasureUnit::createWatt(status));
    measureUnitValue = MeasureUnit::getWatt();
    measureUnit.adoptInstead(MeasureUnit::createAtmosphere(status));
    measureUnitValue = MeasureUnit::getAtmosphere();
    measureUnit.adoptInstead(MeasureUnit::createHectopascal(status));
    measureUnitValue = MeasureUnit::getHectopascal();
    measureUnit.adoptInstead(MeasureUnit::createInchHg(status));
    measureUnitValue = MeasureUnit::getInchHg();
    measureUnit.adoptInstead(MeasureUnit::createKilopascal(status));
    measureUnitValue = MeasureUnit::getKilopascal();
    measureUnit.adoptInstead(MeasureUnit::createMegapascal(status));
    measureUnitValue = MeasureUnit::getMegapascal();
    measureUnit.adoptInstead(MeasureUnit::createMillibar(status));
    measureUnitValue = MeasureUnit::getMillibar();
    measureUnit.adoptInstead(MeasureUnit::createMillimeterOfMercury(status));
    measureUnitValue = MeasureUnit::getMillimeterOfMercury();
    measureUnit.adoptInstead(MeasureUnit::createPoundPerSquareInch(status));
    measureUnitValue = MeasureUnit::getPoundPerSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createKilometerPerHour(status));
    measureUnitValue = MeasureUnit::getKilometerPerHour();
    measureUnit.adoptInstead(MeasureUnit::createKnot(status));
    measureUnitValue = MeasureUnit::getKnot();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnitValue = MeasureUnit::getMeterPerSecond();
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnitValue = MeasureUnit::getMilePerHour();
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnitValue = MeasureUnit::getCelsius();
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnitValue = MeasureUnit::getFahrenheit();
    measureUnit.adoptInstead(MeasureUnit::createGenericTemperature(status));
    measureUnitValue = MeasureUnit::getGenericTemperature();
    measureUnit.adoptInstead(MeasureUnit::createKelvin(status));
    measureUnitValue = MeasureUnit::getKelvin();
    measureUnit.adoptInstead(MeasureUnit::createNewtonMeter(status));
    measureUnitValue = MeasureUnit::getNewtonMeter();
    measureUnit.adoptInstead(MeasureUnit::createPoundFoot(status));
    measureUnitValue = MeasureUnit::getPoundFoot();
    measureUnit.adoptInstead(MeasureUnit::createAcreFoot(status));
    measureUnitValue = MeasureUnit::getAcreFoot();
    measureUnit.adoptInstead(MeasureUnit::createBarrel(status));
    measureUnitValue = MeasureUnit::getBarrel();
    measureUnit.adoptInstead(MeasureUnit::createBushel(status));
    measureUnitValue = MeasureUnit::getBushel();
    measureUnit.adoptInstead(MeasureUnit::createCentiliter(status));
    measureUnitValue = MeasureUnit::getCentiliter();
    measureUnit.adoptInstead(MeasureUnit::createCubicCentimeter(status));
    measureUnitValue = MeasureUnit::getCubicCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicFoot(status));
    measureUnitValue = MeasureUnit::getCubicFoot();
    measureUnit.adoptInstead(MeasureUnit::createCubicInch(status));
    measureUnitValue = MeasureUnit::getCubicInch();
    measureUnit.adoptInstead(MeasureUnit::createCubicKilometer(status));
    measureUnitValue = MeasureUnit::getCubicKilometer();
    measureUnit.adoptInstead(MeasureUnit::createCubicMeter(status));
    measureUnitValue = MeasureUnit::getCubicMeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicMile(status));
    measureUnitValue = MeasureUnit::getCubicMile();
    measureUnit.adoptInstead(MeasureUnit::createCubicYard(status));
    measureUnitValue = MeasureUnit::getCubicYard();
    measureUnit.adoptInstead(MeasureUnit::createCup(status));
    measureUnitValue = MeasureUnit::getCup();
    measureUnit.adoptInstead(MeasureUnit::createCupMetric(status));
    measureUnitValue = MeasureUnit::getCupMetric();
    measureUnit.adoptInstead(MeasureUnit::createDeciliter(status));
    measureUnitValue = MeasureUnit::getDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunce(status));
    measureUnitValue = MeasureUnit::getFluidOunce();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunceImperial(status));
    measureUnitValue = MeasureUnit::getFluidOunceImperial();
    measureUnit.adoptInstead(MeasureUnit::createGallon(status));
    measureUnitValue = MeasureUnit::getGallon();
    measureUnit.adoptInstead(MeasureUnit::createGallonImperial(status));
    measureUnitValue = MeasureUnit::getGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createHectoliter(status));
    measureUnitValue = MeasureUnit::getHectoliter();
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    measureUnitValue = MeasureUnit::getLiter();
    measureUnit.adoptInstead(MeasureUnit::createMegaliter(status));
    measureUnitValue = MeasureUnit::getMegaliter();
    measureUnit.adoptInstead(MeasureUnit::createMilliliter(status));
    measureUnitValue = MeasureUnit::getMilliliter();
    measureUnit.adoptInstead(MeasureUnit::createPint(status));
    measureUnitValue = MeasureUnit::getPint();
    measureUnit.adoptInstead(MeasureUnit::createPintMetric(status));
    measureUnitValue = MeasureUnit::getPintMetric();
    measureUnit.adoptInstead(MeasureUnit::createQuart(status));
    measureUnitValue = MeasureUnit::getQuart();
    measureUnit.adoptInstead(MeasureUnit::createTablespoon(status));
    measureUnitValue = MeasureUnit::getTablespoon();
    measureUnit.adoptInstead(MeasureUnit::createTeaspoon(status));
    measureUnitValue = MeasureUnit::getTeaspoon();
    assertSuccess("", status);
}

void MeasureFormatTest::TestCompatible65() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    MeasureUnit measureUnitValue;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnitValue = MeasureUnit::getGForce();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecondSquared(status));
    measureUnitValue = MeasureUnit::getMeterPerSecondSquared();
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnitValue = MeasureUnit::getArcMinute();
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnitValue = MeasureUnit::getArcSecond();
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnitValue = MeasureUnit::getDegree();
    measureUnit.adoptInstead(MeasureUnit::createRadian(status));
    measureUnitValue = MeasureUnit::getRadian();
    measureUnit.adoptInstead(MeasureUnit::createRevolutionAngle(status));
    measureUnitValue = MeasureUnit::getRevolutionAngle();
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnitValue = MeasureUnit::getAcre();
    measureUnit.adoptInstead(MeasureUnit::createDunam(status));
    measureUnitValue = MeasureUnit::getDunam();
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnitValue = MeasureUnit::getHectare();
    measureUnit.adoptInstead(MeasureUnit::createSquareCentimeter(status));
    measureUnitValue = MeasureUnit::getSquareCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnitValue = MeasureUnit::getSquareFoot();
    measureUnit.adoptInstead(MeasureUnit::createSquareInch(status));
    measureUnitValue = MeasureUnit::getSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnitValue = MeasureUnit::getSquareKilometer();
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnitValue = MeasureUnit::getSquareMeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnitValue = MeasureUnit::getSquareMile();
    measureUnit.adoptInstead(MeasureUnit::createSquareYard(status));
    measureUnitValue = MeasureUnit::getSquareYard();
    measureUnit.adoptInstead(MeasureUnit::createKarat(status));
    measureUnitValue = MeasureUnit::getKarat();
    measureUnit.adoptInstead(MeasureUnit::createMilligramPerDeciliter(status));
    measureUnitValue = MeasureUnit::getMilligramPerDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createMillimolePerLiter(status));
    measureUnitValue = MeasureUnit::getMillimolePerLiter();
    measureUnit.adoptInstead(MeasureUnit::createMole(status));
    measureUnitValue = MeasureUnit::getMole();
    measureUnit.adoptInstead(MeasureUnit::createPartPerMillion(status));
    measureUnitValue = MeasureUnit::getPartPerMillion();
    measureUnit.adoptInstead(MeasureUnit::createPercent(status));
    measureUnitValue = MeasureUnit::getPercent();
    measureUnit.adoptInstead(MeasureUnit::createPermille(status));
    measureUnitValue = MeasureUnit::getPermille();
    measureUnit.adoptInstead(MeasureUnit::createPermyriad(status));
    measureUnitValue = MeasureUnit::getPermyriad();
    measureUnit.adoptInstead(MeasureUnit::createLiterPer100Kilometers(status));
    measureUnitValue = MeasureUnit::getLiterPer100Kilometers();
    measureUnit.adoptInstead(MeasureUnit::createLiterPerKilometer(status));
    measureUnitValue = MeasureUnit::getLiterPerKilometer();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallon(status));
    measureUnitValue = MeasureUnit::getMilePerGallon();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallonImperial(status));
    measureUnitValue = MeasureUnit::getMilePerGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createBit(status));
    measureUnitValue = MeasureUnit::getBit();
    measureUnit.adoptInstead(MeasureUnit::createByte(status));
    measureUnitValue = MeasureUnit::getByte();
    measureUnit.adoptInstead(MeasureUnit::createGigabit(status));
    measureUnitValue = MeasureUnit::getGigabit();
    measureUnit.adoptInstead(MeasureUnit::createGigabyte(status));
    measureUnitValue = MeasureUnit::getGigabyte();
    measureUnit.adoptInstead(MeasureUnit::createKilobit(status));
    measureUnitValue = MeasureUnit::getKilobit();
    measureUnit.adoptInstead(MeasureUnit::createKilobyte(status));
    measureUnitValue = MeasureUnit::getKilobyte();
    measureUnit.adoptInstead(MeasureUnit::createMegabit(status));
    measureUnitValue = MeasureUnit::getMegabit();
    measureUnit.adoptInstead(MeasureUnit::createMegabyte(status));
    measureUnitValue = MeasureUnit::getMegabyte();
    measureUnit.adoptInstead(MeasureUnit::createPetabyte(status));
    measureUnitValue = MeasureUnit::getPetabyte();
    measureUnit.adoptInstead(MeasureUnit::createTerabit(status));
    measureUnitValue = MeasureUnit::getTerabit();
    measureUnit.adoptInstead(MeasureUnit::createTerabyte(status));
    measureUnitValue = MeasureUnit::getTerabyte();
    measureUnit.adoptInstead(MeasureUnit::createCentury(status));
    measureUnitValue = MeasureUnit::getCentury();
    measureUnit.adoptInstead(MeasureUnit::createDay(status));
    measureUnitValue = MeasureUnit::getDay();
    measureUnit.adoptInstead(MeasureUnit::createDayPerson(status));
    measureUnitValue = MeasureUnit::getDayPerson();
    measureUnit.adoptInstead(MeasureUnit::createDecade(status));
    measureUnitValue = MeasureUnit::getDecade();
    measureUnit.adoptInstead(MeasureUnit::createHour(status));
    measureUnitValue = MeasureUnit::getHour();
    measureUnit.adoptInstead(MeasureUnit::createMicrosecond(status));
    measureUnitValue = MeasureUnit::getMicrosecond();
    measureUnit.adoptInstead(MeasureUnit::createMillisecond(status));
    measureUnitValue = MeasureUnit::getMillisecond();
    measureUnit.adoptInstead(MeasureUnit::createMinute(status));
    measureUnitValue = MeasureUnit::getMinute();
    measureUnit.adoptInstead(MeasureUnit::createMonth(status));
    measureUnitValue = MeasureUnit::getMonth();
    measureUnit.adoptInstead(MeasureUnit::createMonthPerson(status));
    measureUnitValue = MeasureUnit::getMonthPerson();
    measureUnit.adoptInstead(MeasureUnit::createNanosecond(status));
    measureUnitValue = MeasureUnit::getNanosecond();
    measureUnit.adoptInstead(MeasureUnit::createSecond(status));
    measureUnitValue = MeasureUnit::getSecond();
    measureUnit.adoptInstead(MeasureUnit::createWeek(status));
    measureUnitValue = MeasureUnit::getWeek();
    measureUnit.adoptInstead(MeasureUnit::createWeekPerson(status));
    measureUnitValue = MeasureUnit::getWeekPerson();
    measureUnit.adoptInstead(MeasureUnit::createYear(status));
    measureUnitValue = MeasureUnit::getYear();
    measureUnit.adoptInstead(MeasureUnit::createYearPerson(status));
    measureUnitValue = MeasureUnit::getYearPerson();
    measureUnit.adoptInstead(MeasureUnit::createAmpere(status));
    measureUnitValue = MeasureUnit::getAmpere();
    measureUnit.adoptInstead(MeasureUnit::createMilliampere(status));
    measureUnitValue = MeasureUnit::getMilliampere();
    measureUnit.adoptInstead(MeasureUnit::createOhm(status));
    measureUnitValue = MeasureUnit::getOhm();
    measureUnit.adoptInstead(MeasureUnit::createVolt(status));
    measureUnitValue = MeasureUnit::getVolt();
    measureUnit.adoptInstead(MeasureUnit::createBritishThermalUnit(status));
    measureUnitValue = MeasureUnit::getBritishThermalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCalorie(status));
    measureUnitValue = MeasureUnit::getCalorie();
    measureUnit.adoptInstead(MeasureUnit::createElectronvolt(status));
    measureUnitValue = MeasureUnit::getElectronvolt();
    measureUnit.adoptInstead(MeasureUnit::createFoodcalorie(status));
    measureUnitValue = MeasureUnit::getFoodcalorie();
    measureUnit.adoptInstead(MeasureUnit::createJoule(status));
    measureUnitValue = MeasureUnit::getJoule();
    measureUnit.adoptInstead(MeasureUnit::createKilocalorie(status));
    measureUnitValue = MeasureUnit::getKilocalorie();
    measureUnit.adoptInstead(MeasureUnit::createKilojoule(status));
    measureUnitValue = MeasureUnit::getKilojoule();
    measureUnit.adoptInstead(MeasureUnit::createKilowattHour(status));
    measureUnitValue = MeasureUnit::getKilowattHour();
    measureUnit.adoptInstead(MeasureUnit::createThermUs(status));
    measureUnitValue = MeasureUnit::getThermUs();
    measureUnit.adoptInstead(MeasureUnit::createNewton(status));
    measureUnitValue = MeasureUnit::getNewton();
    measureUnit.adoptInstead(MeasureUnit::createPoundForce(status));
    measureUnitValue = MeasureUnit::getPoundForce();
    measureUnit.adoptInstead(MeasureUnit::createGigahertz(status));
    measureUnitValue = MeasureUnit::getGigahertz();
    measureUnit.adoptInstead(MeasureUnit::createHertz(status));
    measureUnitValue = MeasureUnit::getHertz();
    measureUnit.adoptInstead(MeasureUnit::createKilohertz(status));
    measureUnitValue = MeasureUnit::getKilohertz();
    measureUnit.adoptInstead(MeasureUnit::createMegahertz(status));
    measureUnitValue = MeasureUnit::getMegahertz();
    measureUnit.adoptInstead(MeasureUnit::createDotPerCentimeter(status));
    measureUnitValue = MeasureUnit::getDotPerCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDotPerInch(status));
    measureUnitValue = MeasureUnit::getDotPerInch();
    measureUnit.adoptInstead(MeasureUnit::createEm(status));
    measureUnitValue = MeasureUnit::getEm();
    measureUnit.adoptInstead(MeasureUnit::createMegapixel(status));
    measureUnitValue = MeasureUnit::getMegapixel();
    measureUnit.adoptInstead(MeasureUnit::createPixel(status));
    measureUnitValue = MeasureUnit::getPixel();
    measureUnit.adoptInstead(MeasureUnit::createPixelPerCentimeter(status));
    measureUnitValue = MeasureUnit::getPixelPerCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createPixelPerInch(status));
    measureUnitValue = MeasureUnit::getPixelPerInch();
    measureUnit.adoptInstead(MeasureUnit::createAstronomicalUnit(status));
    measureUnitValue = MeasureUnit::getAstronomicalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCentimeter(status));
    measureUnitValue = MeasureUnit::getCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDecimeter(status));
    measureUnitValue = MeasureUnit::getDecimeter();
    measureUnit.adoptInstead(MeasureUnit::createFathom(status));
    measureUnitValue = MeasureUnit::getFathom();
    measureUnit.adoptInstead(MeasureUnit::createFoot(status));
    measureUnitValue = MeasureUnit::getFoot();
    measureUnit.adoptInstead(MeasureUnit::createFurlong(status));
    measureUnitValue = MeasureUnit::getFurlong();
    measureUnit.adoptInstead(MeasureUnit::createInch(status));
    measureUnitValue = MeasureUnit::getInch();
    measureUnit.adoptInstead(MeasureUnit::createKilometer(status));
    measureUnitValue = MeasureUnit::getKilometer();
    measureUnit.adoptInstead(MeasureUnit::createLightYear(status));
    measureUnitValue = MeasureUnit::getLightYear();
    measureUnit.adoptInstead(MeasureUnit::createMeter(status));
    measureUnitValue = MeasureUnit::getMeter();
    measureUnit.adoptInstead(MeasureUnit::createMicrometer(status));
    measureUnitValue = MeasureUnit::getMicrometer();
    measureUnit.adoptInstead(MeasureUnit::createMile(status));
    measureUnitValue = MeasureUnit::getMile();
    measureUnit.adoptInstead(MeasureUnit::createMileScandinavian(status));
    measureUnitValue = MeasureUnit::getMileScandinavian();
    measureUnit.adoptInstead(MeasureUnit::createMillimeter(status));
    measureUnitValue = MeasureUnit::getMillimeter();
    measureUnit.adoptInstead(MeasureUnit::createNanometer(status));
    measureUnitValue = MeasureUnit::getNanometer();
    measureUnit.adoptInstead(MeasureUnit::createNauticalMile(status));
    measureUnitValue = MeasureUnit::getNauticalMile();
    measureUnit.adoptInstead(MeasureUnit::createParsec(status));
    measureUnitValue = MeasureUnit::getParsec();
    measureUnit.adoptInstead(MeasureUnit::createPicometer(status));
    measureUnitValue = MeasureUnit::getPicometer();
    measureUnit.adoptInstead(MeasureUnit::createPoint(status));
    measureUnitValue = MeasureUnit::getPoint();
    measureUnit.adoptInstead(MeasureUnit::createSolarRadius(status));
    measureUnitValue = MeasureUnit::getSolarRadius();
    measureUnit.adoptInstead(MeasureUnit::createYard(status));
    measureUnitValue = MeasureUnit::getYard();
    measureUnit.adoptInstead(MeasureUnit::createLux(status));
    measureUnitValue = MeasureUnit::getLux();
    measureUnit.adoptInstead(MeasureUnit::createSolarLuminosity(status));
    measureUnitValue = MeasureUnit::getSolarLuminosity();
    measureUnit.adoptInstead(MeasureUnit::createCarat(status));
    measureUnitValue = MeasureUnit::getCarat();
    measureUnit.adoptInstead(MeasureUnit::createDalton(status));
    measureUnitValue = MeasureUnit::getDalton();
    measureUnit.adoptInstead(MeasureUnit::createEarthMass(status));
    measureUnitValue = MeasureUnit::getEarthMass();
    measureUnit.adoptInstead(MeasureUnit::createGram(status));
    measureUnitValue = MeasureUnit::getGram();
    measureUnit.adoptInstead(MeasureUnit::createKilogram(status));
    measureUnitValue = MeasureUnit::getKilogram();
    measureUnit.adoptInstead(MeasureUnit::createMetricTon(status));
    measureUnitValue = MeasureUnit::getMetricTon();
    measureUnit.adoptInstead(MeasureUnit::createMicrogram(status));
    measureUnitValue = MeasureUnit::getMicrogram();
    measureUnit.adoptInstead(MeasureUnit::createMilligram(status));
    measureUnitValue = MeasureUnit::getMilligram();
    measureUnit.adoptInstead(MeasureUnit::createOunce(status));
    measureUnitValue = MeasureUnit::getOunce();
    measureUnit.adoptInstead(MeasureUnit::createOunceTroy(status));
    measureUnitValue = MeasureUnit::getOunceTroy();
    measureUnit.adoptInstead(MeasureUnit::createPound(status));
    measureUnitValue = MeasureUnit::getPound();
    measureUnit.adoptInstead(MeasureUnit::createSolarMass(status));
    measureUnitValue = MeasureUnit::getSolarMass();
    measureUnit.adoptInstead(MeasureUnit::createStone(status));
    measureUnitValue = MeasureUnit::getStone();
    measureUnit.adoptInstead(MeasureUnit::createTon(status));
    measureUnitValue = MeasureUnit::getTon();
    measureUnit.adoptInstead(MeasureUnit::createGigawatt(status));
    measureUnitValue = MeasureUnit::getGigawatt();
    measureUnit.adoptInstead(MeasureUnit::createHorsepower(status));
    measureUnitValue = MeasureUnit::getHorsepower();
    measureUnit.adoptInstead(MeasureUnit::createKilowatt(status));
    measureUnitValue = MeasureUnit::getKilowatt();
    measureUnit.adoptInstead(MeasureUnit::createMegawatt(status));
    measureUnitValue = MeasureUnit::getMegawatt();
    measureUnit.adoptInstead(MeasureUnit::createMilliwatt(status));
    measureUnitValue = MeasureUnit::getMilliwatt();
    measureUnit.adoptInstead(MeasureUnit::createWatt(status));
    measureUnitValue = MeasureUnit::getWatt();
    measureUnit.adoptInstead(MeasureUnit::createAtmosphere(status));
    measureUnitValue = MeasureUnit::getAtmosphere();
    measureUnit.adoptInstead(MeasureUnit::createBar(status));
    measureUnitValue = MeasureUnit::getBar();
    measureUnit.adoptInstead(MeasureUnit::createHectopascal(status));
    measureUnitValue = MeasureUnit::getHectopascal();
    measureUnit.adoptInstead(MeasureUnit::createInchHg(status));
    measureUnitValue = MeasureUnit::getInchHg();
    measureUnit.adoptInstead(MeasureUnit::createKilopascal(status));
    measureUnitValue = MeasureUnit::getKilopascal();
    measureUnit.adoptInstead(MeasureUnit::createMegapascal(status));
    measureUnitValue = MeasureUnit::getMegapascal();
    measureUnit.adoptInstead(MeasureUnit::createMillibar(status));
    measureUnitValue = MeasureUnit::getMillibar();
    measureUnit.adoptInstead(MeasureUnit::createMillimeterOfMercury(status));
    measureUnitValue = MeasureUnit::getMillimeterOfMercury();
    measureUnit.adoptInstead(MeasureUnit::createPascal(status));
    measureUnitValue = MeasureUnit::getPascal();
    measureUnit.adoptInstead(MeasureUnit::createPoundPerSquareInch(status));
    measureUnitValue = MeasureUnit::getPoundPerSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createKilometerPerHour(status));
    measureUnitValue = MeasureUnit::getKilometerPerHour();
    measureUnit.adoptInstead(MeasureUnit::createKnot(status));
    measureUnitValue = MeasureUnit::getKnot();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnitValue = MeasureUnit::getMeterPerSecond();
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnitValue = MeasureUnit::getMilePerHour();
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnitValue = MeasureUnit::getCelsius();
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnitValue = MeasureUnit::getFahrenheit();
    measureUnit.adoptInstead(MeasureUnit::createGenericTemperature(status));
    measureUnitValue = MeasureUnit::getGenericTemperature();
    measureUnit.adoptInstead(MeasureUnit::createKelvin(status));
    measureUnitValue = MeasureUnit::getKelvin();
    measureUnit.adoptInstead(MeasureUnit::createNewtonMeter(status));
    measureUnitValue = MeasureUnit::getNewtonMeter();
    measureUnit.adoptInstead(MeasureUnit::createPoundFoot(status));
    measureUnitValue = MeasureUnit::getPoundFoot();
    measureUnit.adoptInstead(MeasureUnit::createAcreFoot(status));
    measureUnitValue = MeasureUnit::getAcreFoot();
    measureUnit.adoptInstead(MeasureUnit::createBarrel(status));
    measureUnitValue = MeasureUnit::getBarrel();
    measureUnit.adoptInstead(MeasureUnit::createBushel(status));
    measureUnitValue = MeasureUnit::getBushel();
    measureUnit.adoptInstead(MeasureUnit::createCentiliter(status));
    measureUnitValue = MeasureUnit::getCentiliter();
    measureUnit.adoptInstead(MeasureUnit::createCubicCentimeter(status));
    measureUnitValue = MeasureUnit::getCubicCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicFoot(status));
    measureUnitValue = MeasureUnit::getCubicFoot();
    measureUnit.adoptInstead(MeasureUnit::createCubicInch(status));
    measureUnitValue = MeasureUnit::getCubicInch();
    measureUnit.adoptInstead(MeasureUnit::createCubicKilometer(status));
    measureUnitValue = MeasureUnit::getCubicKilometer();
    measureUnit.adoptInstead(MeasureUnit::createCubicMeter(status));
    measureUnitValue = MeasureUnit::getCubicMeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicMile(status));
    measureUnitValue = MeasureUnit::getCubicMile();
    measureUnit.adoptInstead(MeasureUnit::createCubicYard(status));
    measureUnitValue = MeasureUnit::getCubicYard();
    measureUnit.adoptInstead(MeasureUnit::createCup(status));
    measureUnitValue = MeasureUnit::getCup();
    measureUnit.adoptInstead(MeasureUnit::createCupMetric(status));
    measureUnitValue = MeasureUnit::getCupMetric();
    measureUnit.adoptInstead(MeasureUnit::createDeciliter(status));
    measureUnitValue = MeasureUnit::getDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunce(status));
    measureUnitValue = MeasureUnit::getFluidOunce();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunceImperial(status));
    measureUnitValue = MeasureUnit::getFluidOunceImperial();
    measureUnit.adoptInstead(MeasureUnit::createGallon(status));
    measureUnitValue = MeasureUnit::getGallon();
    measureUnit.adoptInstead(MeasureUnit::createGallonImperial(status));
    measureUnitValue = MeasureUnit::getGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createHectoliter(status));
    measureUnitValue = MeasureUnit::getHectoliter();
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    measureUnitValue = MeasureUnit::getLiter();
    measureUnit.adoptInstead(MeasureUnit::createMegaliter(status));
    measureUnitValue = MeasureUnit::getMegaliter();
    measureUnit.adoptInstead(MeasureUnit::createMilliliter(status));
    measureUnitValue = MeasureUnit::getMilliliter();
    measureUnit.adoptInstead(MeasureUnit::createPint(status));
    measureUnitValue = MeasureUnit::getPint();
    measureUnit.adoptInstead(MeasureUnit::createPintMetric(status));
    measureUnitValue = MeasureUnit::getPintMetric();
    measureUnit.adoptInstead(MeasureUnit::createQuart(status));
    measureUnitValue = MeasureUnit::getQuart();
    measureUnit.adoptInstead(MeasureUnit::createTablespoon(status));
    measureUnitValue = MeasureUnit::getTablespoon();
    measureUnit.adoptInstead(MeasureUnit::createTeaspoon(status));
    measureUnitValue = MeasureUnit::getTeaspoon();
    assertSuccess("", status);
}

// Note that TestCompatible66(), TestCompatible67()
// would be the same as TestCompatible65(), no need to add them.

void MeasureFormatTest::TestCompatible68() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    MeasureUnit measureUnitValue;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnitValue = MeasureUnit::getGForce();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecondSquared(status));
    measureUnitValue = MeasureUnit::getMeterPerSecondSquared();
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnitValue = MeasureUnit::getArcMinute();
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnitValue = MeasureUnit::getArcSecond();
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnitValue = MeasureUnit::getDegree();
    measureUnit.adoptInstead(MeasureUnit::createRadian(status));
    measureUnitValue = MeasureUnit::getRadian();
    measureUnit.adoptInstead(MeasureUnit::createRevolutionAngle(status));
    measureUnitValue = MeasureUnit::getRevolutionAngle();
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnitValue = MeasureUnit::getAcre();
    measureUnit.adoptInstead(MeasureUnit::createDunam(status));
    measureUnitValue = MeasureUnit::getDunam();
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnitValue = MeasureUnit::getHectare();
    measureUnit.adoptInstead(MeasureUnit::createSquareCentimeter(status));
    measureUnitValue = MeasureUnit::getSquareCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnitValue = MeasureUnit::getSquareFoot();
    measureUnit.adoptInstead(MeasureUnit::createSquareInch(status));
    measureUnitValue = MeasureUnit::getSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnitValue = MeasureUnit::getSquareKilometer();
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnitValue = MeasureUnit::getSquareMeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnitValue = MeasureUnit::getSquareMile();
    measureUnit.adoptInstead(MeasureUnit::createSquareYard(status));
    measureUnitValue = MeasureUnit::getSquareYard();
    measureUnit.adoptInstead(MeasureUnit::createKarat(status));
    measureUnitValue = MeasureUnit::getKarat();
    measureUnit.adoptInstead(MeasureUnit::createMilligramPerDeciliter(status));
    measureUnitValue = MeasureUnit::getMilligramPerDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createMillimolePerLiter(status));
    measureUnitValue = MeasureUnit::getMillimolePerLiter();
    measureUnit.adoptInstead(MeasureUnit::createMole(status));
    measureUnitValue = MeasureUnit::getMole();
    measureUnit.adoptInstead(MeasureUnit::createPercent(status));
    measureUnitValue = MeasureUnit::getPercent();
    measureUnit.adoptInstead(MeasureUnit::createPermille(status));
    measureUnitValue = MeasureUnit::getPermille();
    measureUnit.adoptInstead(MeasureUnit::createPartPerMillion(status));
    measureUnitValue = MeasureUnit::getPartPerMillion();
    measureUnit.adoptInstead(MeasureUnit::createPermyriad(status));
    measureUnitValue = MeasureUnit::getPermyriad();
    measureUnit.adoptInstead(MeasureUnit::createLiterPer100Kilometers(status));
    measureUnitValue = MeasureUnit::getLiterPer100Kilometers();
    measureUnit.adoptInstead(MeasureUnit::createLiterPerKilometer(status));
    measureUnitValue = MeasureUnit::getLiterPerKilometer();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallon(status));
    measureUnitValue = MeasureUnit::getMilePerGallon();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallonImperial(status));
    measureUnitValue = MeasureUnit::getMilePerGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createBit(status));
    measureUnitValue = MeasureUnit::getBit();
    measureUnit.adoptInstead(MeasureUnit::createByte(status));
    measureUnitValue = MeasureUnit::getByte();
    measureUnit.adoptInstead(MeasureUnit::createGigabit(status));
    measureUnitValue = MeasureUnit::getGigabit();
    measureUnit.adoptInstead(MeasureUnit::createGigabyte(status));
    measureUnitValue = MeasureUnit::getGigabyte();
    measureUnit.adoptInstead(MeasureUnit::createKilobit(status));
    measureUnitValue = MeasureUnit::getKilobit();
    measureUnit.adoptInstead(MeasureUnit::createKilobyte(status));
    measureUnitValue = MeasureUnit::getKilobyte();
    measureUnit.adoptInstead(MeasureUnit::createMegabit(status));
    measureUnitValue = MeasureUnit::getMegabit();
    measureUnit.adoptInstead(MeasureUnit::createMegabyte(status));
    measureUnitValue = MeasureUnit::getMegabyte();
    measureUnit.adoptInstead(MeasureUnit::createPetabyte(status));
    measureUnitValue = MeasureUnit::getPetabyte();
    measureUnit.adoptInstead(MeasureUnit::createTerabit(status));
    measureUnitValue = MeasureUnit::getTerabit();
    measureUnit.adoptInstead(MeasureUnit::createTerabyte(status));
    measureUnitValue = MeasureUnit::getTerabyte();
    measureUnit.adoptInstead(MeasureUnit::createCentury(status));
    measureUnitValue = MeasureUnit::getCentury();
    measureUnit.adoptInstead(MeasureUnit::createDay(status));
    measureUnitValue = MeasureUnit::getDay();
    measureUnit.adoptInstead(MeasureUnit::createDayPerson(status));
    measureUnitValue = MeasureUnit::getDayPerson();
    measureUnit.adoptInstead(MeasureUnit::createDecade(status));
    measureUnitValue = MeasureUnit::getDecade();
    measureUnit.adoptInstead(MeasureUnit::createHour(status));
    measureUnitValue = MeasureUnit::getHour();
    measureUnit.adoptInstead(MeasureUnit::createMicrosecond(status));
    measureUnitValue = MeasureUnit::getMicrosecond();
    measureUnit.adoptInstead(MeasureUnit::createMillisecond(status));
    measureUnitValue = MeasureUnit::getMillisecond();
    measureUnit.adoptInstead(MeasureUnit::createMinute(status));
    measureUnitValue = MeasureUnit::getMinute();
    measureUnit.adoptInstead(MeasureUnit::createMonth(status));
    measureUnitValue = MeasureUnit::getMonth();
    measureUnit.adoptInstead(MeasureUnit::createMonthPerson(status));
    measureUnitValue = MeasureUnit::getMonthPerson();
    measureUnit.adoptInstead(MeasureUnit::createNanosecond(status));
    measureUnitValue = MeasureUnit::getNanosecond();
    measureUnit.adoptInstead(MeasureUnit::createSecond(status));
    measureUnitValue = MeasureUnit::getSecond();
    measureUnit.adoptInstead(MeasureUnit::createWeek(status));
    measureUnitValue = MeasureUnit::getWeek();
    measureUnit.adoptInstead(MeasureUnit::createWeekPerson(status));
    measureUnitValue = MeasureUnit::getWeekPerson();
    measureUnit.adoptInstead(MeasureUnit::createYear(status));
    measureUnitValue = MeasureUnit::getYear();
    measureUnit.adoptInstead(MeasureUnit::createYearPerson(status));
    measureUnitValue = MeasureUnit::getYearPerson();
    measureUnit.adoptInstead(MeasureUnit::createAmpere(status));
    measureUnitValue = MeasureUnit::getAmpere();
    measureUnit.adoptInstead(MeasureUnit::createMilliampere(status));
    measureUnitValue = MeasureUnit::getMilliampere();
    measureUnit.adoptInstead(MeasureUnit::createOhm(status));
    measureUnitValue = MeasureUnit::getOhm();
    measureUnit.adoptInstead(MeasureUnit::createVolt(status));
    measureUnitValue = MeasureUnit::getVolt();
    measureUnit.adoptInstead(MeasureUnit::createBritishThermalUnit(status));
    measureUnitValue = MeasureUnit::getBritishThermalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCalorie(status));
    measureUnitValue = MeasureUnit::getCalorie();
    measureUnit.adoptInstead(MeasureUnit::createElectronvolt(status));
    measureUnitValue = MeasureUnit::getElectronvolt();
    measureUnit.adoptInstead(MeasureUnit::createFoodcalorie(status));
    measureUnitValue = MeasureUnit::getFoodcalorie();
    measureUnit.adoptInstead(MeasureUnit::createJoule(status));
    measureUnitValue = MeasureUnit::getJoule();
    measureUnit.adoptInstead(MeasureUnit::createKilocalorie(status));
    measureUnitValue = MeasureUnit::getKilocalorie();
    measureUnit.adoptInstead(MeasureUnit::createKilojoule(status));
    measureUnitValue = MeasureUnit::getKilojoule();
    measureUnit.adoptInstead(MeasureUnit::createKilowattHour(status));
    measureUnitValue = MeasureUnit::getKilowattHour();
    measureUnit.adoptInstead(MeasureUnit::createThermUs(status));
    measureUnitValue = MeasureUnit::getThermUs();
    measureUnit.adoptInstead(MeasureUnit::createNewton(status));
    measureUnitValue = MeasureUnit::getNewton();
    measureUnit.adoptInstead(MeasureUnit::createPoundForce(status));
    measureUnitValue = MeasureUnit::getPoundForce();
    measureUnit.adoptInstead(MeasureUnit::createGigahertz(status));
    measureUnitValue = MeasureUnit::getGigahertz();
    measureUnit.adoptInstead(MeasureUnit::createHertz(status));
    measureUnitValue = MeasureUnit::getHertz();
    measureUnit.adoptInstead(MeasureUnit::createKilohertz(status));
    measureUnitValue = MeasureUnit::getKilohertz();
    measureUnit.adoptInstead(MeasureUnit::createMegahertz(status));
    measureUnitValue = MeasureUnit::getMegahertz();
    measureUnit.adoptInstead(MeasureUnit::createDot(status));
    measureUnitValue = MeasureUnit::getDot();
    measureUnit.adoptInstead(MeasureUnit::createDotPerCentimeter(status));
    measureUnitValue = MeasureUnit::getDotPerCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDotPerInch(status));
    measureUnitValue = MeasureUnit::getDotPerInch();
    measureUnit.adoptInstead(MeasureUnit::createEm(status));
    measureUnitValue = MeasureUnit::getEm();
    measureUnit.adoptInstead(MeasureUnit::createMegapixel(status));
    measureUnitValue = MeasureUnit::getMegapixel();
    measureUnit.adoptInstead(MeasureUnit::createPixel(status));
    measureUnitValue = MeasureUnit::getPixel();
    measureUnit.adoptInstead(MeasureUnit::createPixelPerCentimeter(status));
    measureUnitValue = MeasureUnit::getPixelPerCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createPixelPerInch(status));
    measureUnitValue = MeasureUnit::getPixelPerInch();
    measureUnit.adoptInstead(MeasureUnit::createAstronomicalUnit(status));
    measureUnitValue = MeasureUnit::getAstronomicalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCentimeter(status));
    measureUnitValue = MeasureUnit::getCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDecimeter(status));
    measureUnitValue = MeasureUnit::getDecimeter();
    measureUnit.adoptInstead(MeasureUnit::createEarthRadius(status));
    measureUnitValue = MeasureUnit::getEarthRadius();
    measureUnit.adoptInstead(MeasureUnit::createFathom(status));
    measureUnitValue = MeasureUnit::getFathom();
    measureUnit.adoptInstead(MeasureUnit::createFoot(status));
    measureUnitValue = MeasureUnit::getFoot();
    measureUnit.adoptInstead(MeasureUnit::createFurlong(status));
    measureUnitValue = MeasureUnit::getFurlong();
    measureUnit.adoptInstead(MeasureUnit::createInch(status));
    measureUnitValue = MeasureUnit::getInch();
    measureUnit.adoptInstead(MeasureUnit::createKilometer(status));
    measureUnitValue = MeasureUnit::getKilometer();
    measureUnit.adoptInstead(MeasureUnit::createLightYear(status));
    measureUnitValue = MeasureUnit::getLightYear();
    measureUnit.adoptInstead(MeasureUnit::createMeter(status));
    measureUnitValue = MeasureUnit::getMeter();
    measureUnit.adoptInstead(MeasureUnit::createMicrometer(status));
    measureUnitValue = MeasureUnit::getMicrometer();
    measureUnit.adoptInstead(MeasureUnit::createMile(status));
    measureUnitValue = MeasureUnit::getMile();
    measureUnit.adoptInstead(MeasureUnit::createMileScandinavian(status));
    measureUnitValue = MeasureUnit::getMileScandinavian();
    measureUnit.adoptInstead(MeasureUnit::createMillimeter(status));
    measureUnitValue = MeasureUnit::getMillimeter();
    measureUnit.adoptInstead(MeasureUnit::createNanometer(status));
    measureUnitValue = MeasureUnit::getNanometer();
    measureUnit.adoptInstead(MeasureUnit::createNauticalMile(status));
    measureUnitValue = MeasureUnit::getNauticalMile();
    measureUnit.adoptInstead(MeasureUnit::createParsec(status));
    measureUnitValue = MeasureUnit::getParsec();
    measureUnit.adoptInstead(MeasureUnit::createPicometer(status));
    measureUnitValue = MeasureUnit::getPicometer();
    measureUnit.adoptInstead(MeasureUnit::createPoint(status));
    measureUnitValue = MeasureUnit::getPoint();
    measureUnit.adoptInstead(MeasureUnit::createSolarRadius(status));
    measureUnitValue = MeasureUnit::getSolarRadius();
    measureUnit.adoptInstead(MeasureUnit::createYard(status));
    measureUnitValue = MeasureUnit::getYard();
    measureUnit.adoptInstead(MeasureUnit::createCandela(status));
    measureUnitValue = MeasureUnit::getCandela();
    measureUnit.adoptInstead(MeasureUnit::createLumen(status));
    measureUnitValue = MeasureUnit::getLumen();
    measureUnit.adoptInstead(MeasureUnit::createLux(status));
    measureUnitValue = MeasureUnit::getLux();
    measureUnit.adoptInstead(MeasureUnit::createSolarLuminosity(status));
    measureUnitValue = MeasureUnit::getSolarLuminosity();
    measureUnit.adoptInstead(MeasureUnit::createCarat(status));
    measureUnitValue = MeasureUnit::getCarat();
    measureUnit.adoptInstead(MeasureUnit::createDalton(status));
    measureUnitValue = MeasureUnit::getDalton();
    measureUnit.adoptInstead(MeasureUnit::createEarthMass(status));
    measureUnitValue = MeasureUnit::getEarthMass();
    measureUnit.adoptInstead(MeasureUnit::createGrain(status));
    measureUnitValue = MeasureUnit::getGrain();
    measureUnit.adoptInstead(MeasureUnit::createGram(status));
    measureUnitValue = MeasureUnit::getGram();
    measureUnit.adoptInstead(MeasureUnit::createKilogram(status));
    measureUnitValue = MeasureUnit::getKilogram();
    measureUnit.adoptInstead(MeasureUnit::createMetricTon(status));
    measureUnitValue = MeasureUnit::getMetricTon();
    measureUnit.adoptInstead(MeasureUnit::createMicrogram(status));
    measureUnitValue = MeasureUnit::getMicrogram();
    measureUnit.adoptInstead(MeasureUnit::createMilligram(status));
    measureUnitValue = MeasureUnit::getMilligram();
    measureUnit.adoptInstead(MeasureUnit::createOunce(status));
    measureUnitValue = MeasureUnit::getOunce();
    measureUnit.adoptInstead(MeasureUnit::createOunceTroy(status));
    measureUnitValue = MeasureUnit::getOunceTroy();
    measureUnit.adoptInstead(MeasureUnit::createPound(status));
    measureUnitValue = MeasureUnit::getPound();
    measureUnit.adoptInstead(MeasureUnit::createSolarMass(status));
    measureUnitValue = MeasureUnit::getSolarMass();
    measureUnit.adoptInstead(MeasureUnit::createStone(status));
    measureUnitValue = MeasureUnit::getStone();
    measureUnit.adoptInstead(MeasureUnit::createTon(status));
    measureUnitValue = MeasureUnit::getTon();
    measureUnit.adoptInstead(MeasureUnit::createGigawatt(status));
    measureUnitValue = MeasureUnit::getGigawatt();
    measureUnit.adoptInstead(MeasureUnit::createHorsepower(status));
    measureUnitValue = MeasureUnit::getHorsepower();
    measureUnit.adoptInstead(MeasureUnit::createKilowatt(status));
    measureUnitValue = MeasureUnit::getKilowatt();
    measureUnit.adoptInstead(MeasureUnit::createMegawatt(status));
    measureUnitValue = MeasureUnit::getMegawatt();
    measureUnit.adoptInstead(MeasureUnit::createMilliwatt(status));
    measureUnitValue = MeasureUnit::getMilliwatt();
    measureUnit.adoptInstead(MeasureUnit::createWatt(status));
    measureUnitValue = MeasureUnit::getWatt();
    measureUnit.adoptInstead(MeasureUnit::createAtmosphere(status));
    measureUnitValue = MeasureUnit::getAtmosphere();
    measureUnit.adoptInstead(MeasureUnit::createBar(status));
    measureUnitValue = MeasureUnit::getBar();
    measureUnit.adoptInstead(MeasureUnit::createHectopascal(status));
    measureUnitValue = MeasureUnit::getHectopascal();
    measureUnit.adoptInstead(MeasureUnit::createInchHg(status));
    measureUnitValue = MeasureUnit::getInchHg();
    measureUnit.adoptInstead(MeasureUnit::createKilopascal(status));
    measureUnitValue = MeasureUnit::getKilopascal();
    measureUnit.adoptInstead(MeasureUnit::createMegapascal(status));
    measureUnitValue = MeasureUnit::getMegapascal();
    measureUnit.adoptInstead(MeasureUnit::createMillibar(status));
    measureUnitValue = MeasureUnit::getMillibar();
    measureUnit.adoptInstead(MeasureUnit::createMillimeterOfMercury(status));
    measureUnitValue = MeasureUnit::getMillimeterOfMercury();
    measureUnit.adoptInstead(MeasureUnit::createPascal(status));
    measureUnitValue = MeasureUnit::getPascal();
    measureUnit.adoptInstead(MeasureUnit::createPoundPerSquareInch(status));
    measureUnitValue = MeasureUnit::getPoundPerSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createKilometerPerHour(status));
    measureUnitValue = MeasureUnit::getKilometerPerHour();
    measureUnit.adoptInstead(MeasureUnit::createKnot(status));
    measureUnitValue = MeasureUnit::getKnot();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnitValue = MeasureUnit::getMeterPerSecond();
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnitValue = MeasureUnit::getMilePerHour();
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnitValue = MeasureUnit::getCelsius();
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnitValue = MeasureUnit::getFahrenheit();
    measureUnit.adoptInstead(MeasureUnit::createGenericTemperature(status));
    measureUnitValue = MeasureUnit::getGenericTemperature();
    measureUnit.adoptInstead(MeasureUnit::createKelvin(status));
    measureUnitValue = MeasureUnit::getKelvin();
    measureUnit.adoptInstead(MeasureUnit::createNewtonMeter(status));
    measureUnitValue = MeasureUnit::getNewtonMeter();
    measureUnit.adoptInstead(MeasureUnit::createPoundFoot(status));
    measureUnitValue = MeasureUnit::getPoundFoot();
    measureUnit.adoptInstead(MeasureUnit::createAcreFoot(status));
    measureUnitValue = MeasureUnit::getAcreFoot();
    measureUnit.adoptInstead(MeasureUnit::createBarrel(status));
    measureUnitValue = MeasureUnit::getBarrel();
    measureUnit.adoptInstead(MeasureUnit::createBushel(status));
    measureUnitValue = MeasureUnit::getBushel();
    measureUnit.adoptInstead(MeasureUnit::createCentiliter(status));
    measureUnitValue = MeasureUnit::getCentiliter();
    measureUnit.adoptInstead(MeasureUnit::createCubicCentimeter(status));
    measureUnitValue = MeasureUnit::getCubicCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicFoot(status));
    measureUnitValue = MeasureUnit::getCubicFoot();
    measureUnit.adoptInstead(MeasureUnit::createCubicInch(status));
    measureUnitValue = MeasureUnit::getCubicInch();
    measureUnit.adoptInstead(MeasureUnit::createCubicKilometer(status));
    measureUnitValue = MeasureUnit::getCubicKilometer();
    measureUnit.adoptInstead(MeasureUnit::createCubicMeter(status));
    measureUnitValue = MeasureUnit::getCubicMeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicMile(status));
    measureUnitValue = MeasureUnit::getCubicMile();
    measureUnit.adoptInstead(MeasureUnit::createCubicYard(status));
    measureUnitValue = MeasureUnit::getCubicYard();
    measureUnit.adoptInstead(MeasureUnit::createCup(status));
    measureUnitValue = MeasureUnit::getCup();
    measureUnit.adoptInstead(MeasureUnit::createCupMetric(status));
    measureUnitValue = MeasureUnit::getCupMetric();
    measureUnit.adoptInstead(MeasureUnit::createDeciliter(status));
    measureUnitValue = MeasureUnit::getDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createDessertSpoon(status));
    measureUnitValue = MeasureUnit::getDessertSpoon();
    measureUnit.adoptInstead(MeasureUnit::createDessertSpoonImperial(status));
    measureUnitValue = MeasureUnit::getDessertSpoonImperial();
    measureUnit.adoptInstead(MeasureUnit::createDram(status));
    measureUnitValue = MeasureUnit::getDram();
    measureUnit.adoptInstead(MeasureUnit::createDrop(status));
    measureUnitValue = MeasureUnit::getDrop();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunce(status));
    measureUnitValue = MeasureUnit::getFluidOunce();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunceImperial(status));
    measureUnitValue = MeasureUnit::getFluidOunceImperial();
    measureUnit.adoptInstead(MeasureUnit::createGallon(status));
    measureUnitValue = MeasureUnit::getGallon();
    measureUnit.adoptInstead(MeasureUnit::createGallonImperial(status));
    measureUnitValue = MeasureUnit::getGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createHectoliter(status));
    measureUnitValue = MeasureUnit::getHectoliter();
    measureUnit.adoptInstead(MeasureUnit::createJigger(status));
    measureUnitValue = MeasureUnit::getJigger();
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    measureUnitValue = MeasureUnit::getLiter();
    measureUnit.adoptInstead(MeasureUnit::createMegaliter(status));
    measureUnitValue = MeasureUnit::getMegaliter();
    measureUnit.adoptInstead(MeasureUnit::createMilliliter(status));
    measureUnitValue = MeasureUnit::getMilliliter();
    measureUnit.adoptInstead(MeasureUnit::createPinch(status));
    measureUnitValue = MeasureUnit::getPinch();
    measureUnit.adoptInstead(MeasureUnit::createPint(status));
    measureUnitValue = MeasureUnit::getPint();
    measureUnit.adoptInstead(MeasureUnit::createPintMetric(status));
    measureUnitValue = MeasureUnit::getPintMetric();
    measureUnit.adoptInstead(MeasureUnit::createQuart(status));
    measureUnitValue = MeasureUnit::getQuart();
    measureUnit.adoptInstead(MeasureUnit::createQuartImperial(status));
    measureUnitValue = MeasureUnit::getQuartImperial();
    measureUnit.adoptInstead(MeasureUnit::createTablespoon(status));
    measureUnitValue = MeasureUnit::getTablespoon();
    measureUnit.adoptInstead(MeasureUnit::createTeaspoon(status));
    measureUnitValue = MeasureUnit::getTeaspoon();
    assertSuccess("", status);
}

void MeasureFormatTest::TestCompatible69() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    MeasureUnit measureUnitValue;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnitValue = MeasureUnit::getGForce();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecondSquared(status));
    measureUnitValue = MeasureUnit::getMeterPerSecondSquared();
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnitValue = MeasureUnit::getArcMinute();
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnitValue = MeasureUnit::getArcSecond();
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnitValue = MeasureUnit::getDegree();
    measureUnit.adoptInstead(MeasureUnit::createRadian(status));
    measureUnitValue = MeasureUnit::getRadian();
    measureUnit.adoptInstead(MeasureUnit::createRevolutionAngle(status));
    measureUnitValue = MeasureUnit::getRevolutionAngle();
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnitValue = MeasureUnit::getAcre();
    measureUnit.adoptInstead(MeasureUnit::createDunam(status));
    measureUnitValue = MeasureUnit::getDunam();
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnitValue = MeasureUnit::getHectare();
    measureUnit.adoptInstead(MeasureUnit::createSquareCentimeter(status));
    measureUnitValue = MeasureUnit::getSquareCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnitValue = MeasureUnit::getSquareFoot();
    measureUnit.adoptInstead(MeasureUnit::createSquareInch(status));
    measureUnitValue = MeasureUnit::getSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnitValue = MeasureUnit::getSquareKilometer();
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnitValue = MeasureUnit::getSquareMeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnitValue = MeasureUnit::getSquareMile();
    measureUnit.adoptInstead(MeasureUnit::createSquareYard(status));
    measureUnitValue = MeasureUnit::getSquareYard();
    measureUnit.adoptInstead(MeasureUnit::createKarat(status));
    measureUnitValue = MeasureUnit::getKarat();
    measureUnit.adoptInstead(MeasureUnit::createMilligramOfglucosePerDeciliter(status));
    measureUnitValue = MeasureUnit::getMilligramOfglucosePerDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createMilligramPerDeciliter(status));
    measureUnitValue = MeasureUnit::getMilligramPerDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createMillimolePerLiter(status));
    measureUnitValue = MeasureUnit::getMillimolePerLiter();
    measureUnit.adoptInstead(MeasureUnit::createMole(status));
    measureUnitValue = MeasureUnit::getMole();
    measureUnit.adoptInstead(MeasureUnit::createPercent(status));
    measureUnitValue = MeasureUnit::getPercent();
    measureUnit.adoptInstead(MeasureUnit::createPermille(status));
    measureUnitValue = MeasureUnit::getPermille();
    measureUnit.adoptInstead(MeasureUnit::createPartPerMillion(status));
    measureUnitValue = MeasureUnit::getPartPerMillion();
    measureUnit.adoptInstead(MeasureUnit::createPermyriad(status));
    measureUnitValue = MeasureUnit::getPermyriad();
    measureUnit.adoptInstead(MeasureUnit::createLiterPer100Kilometers(status));
    measureUnitValue = MeasureUnit::getLiterPer100Kilometers();
    measureUnit.adoptInstead(MeasureUnit::createLiterPerKilometer(status));
    measureUnitValue = MeasureUnit::getLiterPerKilometer();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallon(status));
    measureUnitValue = MeasureUnit::getMilePerGallon();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallonImperial(status));
    measureUnitValue = MeasureUnit::getMilePerGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createBit(status));
    measureUnitValue = MeasureUnit::getBit();
    measureUnit.adoptInstead(MeasureUnit::createByte(status));
    measureUnitValue = MeasureUnit::getByte();
    measureUnit.adoptInstead(MeasureUnit::createGigabit(status));
    measureUnitValue = MeasureUnit::getGigabit();
    measureUnit.adoptInstead(MeasureUnit::createGigabyte(status));
    measureUnitValue = MeasureUnit::getGigabyte();
    measureUnit.adoptInstead(MeasureUnit::createKilobit(status));
    measureUnitValue = MeasureUnit::getKilobit();
    measureUnit.adoptInstead(MeasureUnit::createKilobyte(status));
    measureUnitValue = MeasureUnit::getKilobyte();
    measureUnit.adoptInstead(MeasureUnit::createMegabit(status));
    measureUnitValue = MeasureUnit::getMegabit();
    measureUnit.adoptInstead(MeasureUnit::createMegabyte(status));
    measureUnitValue = MeasureUnit::getMegabyte();
    measureUnit.adoptInstead(MeasureUnit::createPetabyte(status));
    measureUnitValue = MeasureUnit::getPetabyte();
    measureUnit.adoptInstead(MeasureUnit::createTerabit(status));
    measureUnitValue = MeasureUnit::getTerabit();
    measureUnit.adoptInstead(MeasureUnit::createTerabyte(status));
    measureUnitValue = MeasureUnit::getTerabyte();
    measureUnit.adoptInstead(MeasureUnit::createCentury(status));
    measureUnitValue = MeasureUnit::getCentury();
    measureUnit.adoptInstead(MeasureUnit::createDay(status));
    measureUnitValue = MeasureUnit::getDay();
    measureUnit.adoptInstead(MeasureUnit::createDayPerson(status));
    measureUnitValue = MeasureUnit::getDayPerson();
    measureUnit.adoptInstead(MeasureUnit::createDecade(status));
    measureUnitValue = MeasureUnit::getDecade();
    measureUnit.adoptInstead(MeasureUnit::createHour(status));
    measureUnitValue = MeasureUnit::getHour();
    measureUnit.adoptInstead(MeasureUnit::createMicrosecond(status));
    measureUnitValue = MeasureUnit::getMicrosecond();
    measureUnit.adoptInstead(MeasureUnit::createMillisecond(status));
    measureUnitValue = MeasureUnit::getMillisecond();
    measureUnit.adoptInstead(MeasureUnit::createMinute(status));
    measureUnitValue = MeasureUnit::getMinute();
    measureUnit.adoptInstead(MeasureUnit::createMonth(status));
    measureUnitValue = MeasureUnit::getMonth();
    measureUnit.adoptInstead(MeasureUnit::createMonthPerson(status));
    measureUnitValue = MeasureUnit::getMonthPerson();
    measureUnit.adoptInstead(MeasureUnit::createNanosecond(status));
    measureUnitValue = MeasureUnit::getNanosecond();
    measureUnit.adoptInstead(MeasureUnit::createSecond(status));
    measureUnitValue = MeasureUnit::getSecond();
    measureUnit.adoptInstead(MeasureUnit::createWeek(status));
    measureUnitValue = MeasureUnit::getWeek();
    measureUnit.adoptInstead(MeasureUnit::createWeekPerson(status));
    measureUnitValue = MeasureUnit::getWeekPerson();
    measureUnit.adoptInstead(MeasureUnit::createYear(status));
    measureUnitValue = MeasureUnit::getYear();
    measureUnit.adoptInstead(MeasureUnit::createYearPerson(status));
    measureUnitValue = MeasureUnit::getYearPerson();
    measureUnit.adoptInstead(MeasureUnit::createAmpere(status));
    measureUnitValue = MeasureUnit::getAmpere();
    measureUnit.adoptInstead(MeasureUnit::createMilliampere(status));
    measureUnitValue = MeasureUnit::getMilliampere();
    measureUnit.adoptInstead(MeasureUnit::createOhm(status));
    measureUnitValue = MeasureUnit::getOhm();
    measureUnit.adoptInstead(MeasureUnit::createVolt(status));
    measureUnitValue = MeasureUnit::getVolt();
    measureUnit.adoptInstead(MeasureUnit::createBritishThermalUnit(status));
    measureUnitValue = MeasureUnit::getBritishThermalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCalorie(status));
    measureUnitValue = MeasureUnit::getCalorie();
    measureUnit.adoptInstead(MeasureUnit::createElectronvolt(status));
    measureUnitValue = MeasureUnit::getElectronvolt();
    measureUnit.adoptInstead(MeasureUnit::createFoodcalorie(status));
    measureUnitValue = MeasureUnit::getFoodcalorie();
    measureUnit.adoptInstead(MeasureUnit::createJoule(status));
    measureUnitValue = MeasureUnit::getJoule();
    measureUnit.adoptInstead(MeasureUnit::createKilocalorie(status));
    measureUnitValue = MeasureUnit::getKilocalorie();
    measureUnit.adoptInstead(MeasureUnit::createKilojoule(status));
    measureUnitValue = MeasureUnit::getKilojoule();
    measureUnit.adoptInstead(MeasureUnit::createKilowattHour(status));
    measureUnitValue = MeasureUnit::getKilowattHour();
    measureUnit.adoptInstead(MeasureUnit::createThermUs(status));
    measureUnitValue = MeasureUnit::getThermUs();
    measureUnit.adoptInstead(MeasureUnit::createNewton(status));
    measureUnitValue = MeasureUnit::getNewton();
    measureUnit.adoptInstead(MeasureUnit::createPoundForce(status));
    measureUnitValue = MeasureUnit::getPoundForce();
    measureUnit.adoptInstead(MeasureUnit::createGigahertz(status));
    measureUnitValue = MeasureUnit::getGigahertz();
    measureUnit.adoptInstead(MeasureUnit::createHertz(status));
    measureUnitValue = MeasureUnit::getHertz();
    measureUnit.adoptInstead(MeasureUnit::createKilohertz(status));
    measureUnitValue = MeasureUnit::getKilohertz();
    measureUnit.adoptInstead(MeasureUnit::createMegahertz(status));
    measureUnitValue = MeasureUnit::getMegahertz();
    measureUnit.adoptInstead(MeasureUnit::createDot(status));
    measureUnitValue = MeasureUnit::getDot();
    measureUnit.adoptInstead(MeasureUnit::createDotPerCentimeter(status));
    measureUnitValue = MeasureUnit::getDotPerCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDotPerInch(status));
    measureUnitValue = MeasureUnit::getDotPerInch();
    measureUnit.adoptInstead(MeasureUnit::createEm(status));
    measureUnitValue = MeasureUnit::getEm();
    measureUnit.adoptInstead(MeasureUnit::createMegapixel(status));
    measureUnitValue = MeasureUnit::getMegapixel();
    measureUnit.adoptInstead(MeasureUnit::createPixel(status));
    measureUnitValue = MeasureUnit::getPixel();
    measureUnit.adoptInstead(MeasureUnit::createPixelPerCentimeter(status));
    measureUnitValue = MeasureUnit::getPixelPerCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createPixelPerInch(status));
    measureUnitValue = MeasureUnit::getPixelPerInch();
    measureUnit.adoptInstead(MeasureUnit::createAstronomicalUnit(status));
    measureUnitValue = MeasureUnit::getAstronomicalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCentimeter(status));
    measureUnitValue = MeasureUnit::getCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDecimeter(status));
    measureUnitValue = MeasureUnit::getDecimeter();
    measureUnit.adoptInstead(MeasureUnit::createEarthRadius(status));
    measureUnitValue = MeasureUnit::getEarthRadius();
    measureUnit.adoptInstead(MeasureUnit::createFathom(status));
    measureUnitValue = MeasureUnit::getFathom();
    measureUnit.adoptInstead(MeasureUnit::createFoot(status));
    measureUnitValue = MeasureUnit::getFoot();
    measureUnit.adoptInstead(MeasureUnit::createFurlong(status));
    measureUnitValue = MeasureUnit::getFurlong();
    measureUnit.adoptInstead(MeasureUnit::createInch(status));
    measureUnitValue = MeasureUnit::getInch();
    measureUnit.adoptInstead(MeasureUnit::createKilometer(status));
    measureUnitValue = MeasureUnit::getKilometer();
    measureUnit.adoptInstead(MeasureUnit::createLightYear(status));
    measureUnitValue = MeasureUnit::getLightYear();
    measureUnit.adoptInstead(MeasureUnit::createMeter(status));
    measureUnitValue = MeasureUnit::getMeter();
    measureUnit.adoptInstead(MeasureUnit::createMicrometer(status));
    measureUnitValue = MeasureUnit::getMicrometer();
    measureUnit.adoptInstead(MeasureUnit::createMile(status));
    measureUnitValue = MeasureUnit::getMile();
    measureUnit.adoptInstead(MeasureUnit::createMileScandinavian(status));
    measureUnitValue = MeasureUnit::getMileScandinavian();
    measureUnit.adoptInstead(MeasureUnit::createMillimeter(status));
    measureUnitValue = MeasureUnit::getMillimeter();
    measureUnit.adoptInstead(MeasureUnit::createNanometer(status));
    measureUnitValue = MeasureUnit::getNanometer();
    measureUnit.adoptInstead(MeasureUnit::createNauticalMile(status));
    measureUnitValue = MeasureUnit::getNauticalMile();
    measureUnit.adoptInstead(MeasureUnit::createParsec(status));
    measureUnitValue = MeasureUnit::getParsec();
    measureUnit.adoptInstead(MeasureUnit::createPicometer(status));
    measureUnitValue = MeasureUnit::getPicometer();
    measureUnit.adoptInstead(MeasureUnit::createPoint(status));
    measureUnitValue = MeasureUnit::getPoint();
    measureUnit.adoptInstead(MeasureUnit::createSolarRadius(status));
    measureUnitValue = MeasureUnit::getSolarRadius();
    measureUnit.adoptInstead(MeasureUnit::createYard(status));
    measureUnitValue = MeasureUnit::getYard();
    measureUnit.adoptInstead(MeasureUnit::createCandela(status));
    measureUnitValue = MeasureUnit::getCandela();
    measureUnit.adoptInstead(MeasureUnit::createLumen(status));
    measureUnitValue = MeasureUnit::getLumen();
    measureUnit.adoptInstead(MeasureUnit::createLux(status));
    measureUnitValue = MeasureUnit::getLux();
    measureUnit.adoptInstead(MeasureUnit::createSolarLuminosity(status));
    measureUnitValue = MeasureUnit::getSolarLuminosity();
    measureUnit.adoptInstead(MeasureUnit::createCarat(status));
    measureUnitValue = MeasureUnit::getCarat();
    measureUnit.adoptInstead(MeasureUnit::createDalton(status));
    measureUnitValue = MeasureUnit::getDalton();
    measureUnit.adoptInstead(MeasureUnit::createEarthMass(status));
    measureUnitValue = MeasureUnit::getEarthMass();
    measureUnit.adoptInstead(MeasureUnit::createGrain(status));
    measureUnitValue = MeasureUnit::getGrain();
    measureUnit.adoptInstead(MeasureUnit::createGram(status));
    measureUnitValue = MeasureUnit::getGram();
    measureUnit.adoptInstead(MeasureUnit::createKilogram(status));
    measureUnitValue = MeasureUnit::getKilogram();
    measureUnit.adoptInstead(MeasureUnit::createMetricTon(status));
    measureUnitValue = MeasureUnit::getMetricTon();
    measureUnit.adoptInstead(MeasureUnit::createMicrogram(status));
    measureUnitValue = MeasureUnit::getMicrogram();
    measureUnit.adoptInstead(MeasureUnit::createMilligram(status));
    measureUnitValue = MeasureUnit::getMilligram();
    measureUnit.adoptInstead(MeasureUnit::createOunce(status));
    measureUnitValue = MeasureUnit::getOunce();
    measureUnit.adoptInstead(MeasureUnit::createOunceTroy(status));
    measureUnitValue = MeasureUnit::getOunceTroy();
    measureUnit.adoptInstead(MeasureUnit::createPound(status));
    measureUnitValue = MeasureUnit::getPound();
    measureUnit.adoptInstead(MeasureUnit::createSolarMass(status));
    measureUnitValue = MeasureUnit::getSolarMass();
    measureUnit.adoptInstead(MeasureUnit::createStone(status));
    measureUnitValue = MeasureUnit::getStone();
    measureUnit.adoptInstead(MeasureUnit::createTon(status));
    measureUnitValue = MeasureUnit::getTon();
    measureUnit.adoptInstead(MeasureUnit::createGigawatt(status));
    measureUnitValue = MeasureUnit::getGigawatt();
    measureUnit.adoptInstead(MeasureUnit::createHorsepower(status));
    measureUnitValue = MeasureUnit::getHorsepower();
    measureUnit.adoptInstead(MeasureUnit::createKilowatt(status));
    measureUnitValue = MeasureUnit::getKilowatt();
    measureUnit.adoptInstead(MeasureUnit::createMegawatt(status));
    measureUnitValue = MeasureUnit::getMegawatt();
    measureUnit.adoptInstead(MeasureUnit::createMilliwatt(status));
    measureUnitValue = MeasureUnit::getMilliwatt();
    measureUnit.adoptInstead(MeasureUnit::createWatt(status));
    measureUnitValue = MeasureUnit::getWatt();
    measureUnit.adoptInstead(MeasureUnit::createAtmosphere(status));
    measureUnitValue = MeasureUnit::getAtmosphere();
    measureUnit.adoptInstead(MeasureUnit::createBar(status));
    measureUnitValue = MeasureUnit::getBar();
    measureUnit.adoptInstead(MeasureUnit::createHectopascal(status));
    measureUnitValue = MeasureUnit::getHectopascal();
    measureUnit.adoptInstead(MeasureUnit::createInchHg(status));
    measureUnitValue = MeasureUnit::getInchHg();
    measureUnit.adoptInstead(MeasureUnit::createKilopascal(status));
    measureUnitValue = MeasureUnit::getKilopascal();
    measureUnit.adoptInstead(MeasureUnit::createMegapascal(status));
    measureUnitValue = MeasureUnit::getMegapascal();
    measureUnit.adoptInstead(MeasureUnit::createMillibar(status));
    measureUnitValue = MeasureUnit::getMillibar();
    measureUnit.adoptInstead(MeasureUnit::createMillimeterOfMercury(status));
    measureUnitValue = MeasureUnit::getMillimeterOfMercury();
    measureUnit.adoptInstead(MeasureUnit::createPascal(status));
    measureUnitValue = MeasureUnit::getPascal();
    measureUnit.adoptInstead(MeasureUnit::createPoundPerSquareInch(status));
    measureUnitValue = MeasureUnit::getPoundPerSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createKilometerPerHour(status));
    measureUnitValue = MeasureUnit::getKilometerPerHour();
    measureUnit.adoptInstead(MeasureUnit::createKnot(status));
    measureUnitValue = MeasureUnit::getKnot();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnitValue = MeasureUnit::getMeterPerSecond();
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnitValue = MeasureUnit::getMilePerHour();
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnitValue = MeasureUnit::getCelsius();
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnitValue = MeasureUnit::getFahrenheit();
    measureUnit.adoptInstead(MeasureUnit::createGenericTemperature(status));
    measureUnitValue = MeasureUnit::getGenericTemperature();
    measureUnit.adoptInstead(MeasureUnit::createKelvin(status));
    measureUnitValue = MeasureUnit::getKelvin();
    measureUnit.adoptInstead(MeasureUnit::createNewtonMeter(status));
    measureUnitValue = MeasureUnit::getNewtonMeter();
    measureUnit.adoptInstead(MeasureUnit::createPoundFoot(status));
    measureUnitValue = MeasureUnit::getPoundFoot();
    measureUnit.adoptInstead(MeasureUnit::createAcreFoot(status));
    measureUnitValue = MeasureUnit::getAcreFoot();
    measureUnit.adoptInstead(MeasureUnit::createBarrel(status));
    measureUnitValue = MeasureUnit::getBarrel();
    measureUnit.adoptInstead(MeasureUnit::createBushel(status));
    measureUnitValue = MeasureUnit::getBushel();
    measureUnit.adoptInstead(MeasureUnit::createCentiliter(status));
    measureUnitValue = MeasureUnit::getCentiliter();
    measureUnit.adoptInstead(MeasureUnit::createCubicCentimeter(status));
    measureUnitValue = MeasureUnit::getCubicCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicFoot(status));
    measureUnitValue = MeasureUnit::getCubicFoot();
    measureUnit.adoptInstead(MeasureUnit::createCubicInch(status));
    measureUnitValue = MeasureUnit::getCubicInch();
    measureUnit.adoptInstead(MeasureUnit::createCubicKilometer(status));
    measureUnitValue = MeasureUnit::getCubicKilometer();
    measureUnit.adoptInstead(MeasureUnit::createCubicMeter(status));
    measureUnitValue = MeasureUnit::getCubicMeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicMile(status));
    measureUnitValue = MeasureUnit::getCubicMile();
    measureUnit.adoptInstead(MeasureUnit::createCubicYard(status));
    measureUnitValue = MeasureUnit::getCubicYard();
    measureUnit.adoptInstead(MeasureUnit::createCup(status));
    measureUnitValue = MeasureUnit::getCup();
    measureUnit.adoptInstead(MeasureUnit::createCupMetric(status));
    measureUnitValue = MeasureUnit::getCupMetric();
    measureUnit.adoptInstead(MeasureUnit::createDeciliter(status));
    measureUnitValue = MeasureUnit::getDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createDessertSpoon(status));
    measureUnitValue = MeasureUnit::getDessertSpoon();
    measureUnit.adoptInstead(MeasureUnit::createDessertSpoonImperial(status));
    measureUnitValue = MeasureUnit::getDessertSpoonImperial();
    measureUnit.adoptInstead(MeasureUnit::createDram(status));
    measureUnitValue = MeasureUnit::getDram();
    measureUnit.adoptInstead(MeasureUnit::createDrop(status));
    measureUnitValue = MeasureUnit::getDrop();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunce(status));
    measureUnitValue = MeasureUnit::getFluidOunce();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunceImperial(status));
    measureUnitValue = MeasureUnit::getFluidOunceImperial();
    measureUnit.adoptInstead(MeasureUnit::createGallon(status));
    measureUnitValue = MeasureUnit::getGallon();
    measureUnit.adoptInstead(MeasureUnit::createGallonImperial(status));
    measureUnitValue = MeasureUnit::getGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createHectoliter(status));
    measureUnitValue = MeasureUnit::getHectoliter();
    measureUnit.adoptInstead(MeasureUnit::createJigger(status));
    measureUnitValue = MeasureUnit::getJigger();
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    measureUnitValue = MeasureUnit::getLiter();
    measureUnit.adoptInstead(MeasureUnit::createMegaliter(status));
    measureUnitValue = MeasureUnit::getMegaliter();
    measureUnit.adoptInstead(MeasureUnit::createMilliliter(status));
    measureUnitValue = MeasureUnit::getMilliliter();
    measureUnit.adoptInstead(MeasureUnit::createPinch(status));
    measureUnitValue = MeasureUnit::getPinch();
    measureUnit.adoptInstead(MeasureUnit::createPint(status));
    measureUnitValue = MeasureUnit::getPint();
    measureUnit.adoptInstead(MeasureUnit::createPintMetric(status));
    measureUnitValue = MeasureUnit::getPintMetric();
    measureUnit.adoptInstead(MeasureUnit::createQuart(status));
    measureUnitValue = MeasureUnit::getQuart();
    measureUnit.adoptInstead(MeasureUnit::createQuartImperial(status));
    measureUnitValue = MeasureUnit::getQuartImperial();
    measureUnit.adoptInstead(MeasureUnit::createTablespoon(status));
    measureUnitValue = MeasureUnit::getTablespoon();
    measureUnit.adoptInstead(MeasureUnit::createTeaspoon(status));
    measureUnitValue = MeasureUnit::getTeaspoon();
    assertSuccess("", status);
}

void MeasureFormatTest::TestCompatible70() { // TestCompatible71 would be identical
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    MeasureUnit measureUnitValue;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnitValue = MeasureUnit::getGForce();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecondSquared(status));
    measureUnitValue = MeasureUnit::getMeterPerSecondSquared();
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnitValue = MeasureUnit::getArcMinute();
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnitValue = MeasureUnit::getArcSecond();
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnitValue = MeasureUnit::getDegree();
    measureUnit.adoptInstead(MeasureUnit::createRadian(status));
    measureUnitValue = MeasureUnit::getRadian();
    measureUnit.adoptInstead(MeasureUnit::createRevolutionAngle(status));
    measureUnitValue = MeasureUnit::getRevolutionAngle();
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnitValue = MeasureUnit::getAcre();
    measureUnit.adoptInstead(MeasureUnit::createDunam(status));
    measureUnitValue = MeasureUnit::getDunam();
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnitValue = MeasureUnit::getHectare();
    measureUnit.adoptInstead(MeasureUnit::createSquareCentimeter(status));
    measureUnitValue = MeasureUnit::getSquareCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnitValue = MeasureUnit::getSquareFoot();
    measureUnit.adoptInstead(MeasureUnit::createSquareInch(status));
    measureUnitValue = MeasureUnit::getSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnitValue = MeasureUnit::getSquareKilometer();
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnitValue = MeasureUnit::getSquareMeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnitValue = MeasureUnit::getSquareMile();
    measureUnit.adoptInstead(MeasureUnit::createSquareYard(status));
    measureUnitValue = MeasureUnit::getSquareYard();
    measureUnit.adoptInstead(MeasureUnit::createItem(status));
    measureUnitValue = MeasureUnit::getItem();
    measureUnit.adoptInstead(MeasureUnit::createKarat(status));
    measureUnitValue = MeasureUnit::getKarat();
    measureUnit.adoptInstead(MeasureUnit::createMilligramOfglucosePerDeciliter(status));
    measureUnitValue = MeasureUnit::getMilligramOfglucosePerDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createMilligramPerDeciliter(status));
    measureUnitValue = MeasureUnit::getMilligramPerDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createMillimolePerLiter(status));
    measureUnitValue = MeasureUnit::getMillimolePerLiter();
    measureUnit.adoptInstead(MeasureUnit::createMole(status));
    measureUnitValue = MeasureUnit::getMole();
    measureUnit.adoptInstead(MeasureUnit::createPercent(status));
    measureUnitValue = MeasureUnit::getPercent();
    measureUnit.adoptInstead(MeasureUnit::createPermille(status));
    measureUnitValue = MeasureUnit::getPermille();
    measureUnit.adoptInstead(MeasureUnit::createPartPerMillion(status));
    measureUnitValue = MeasureUnit::getPartPerMillion();
    measureUnit.adoptInstead(MeasureUnit::createPermyriad(status));
    measureUnitValue = MeasureUnit::getPermyriad();
    measureUnit.adoptInstead(MeasureUnit::createLiterPer100Kilometers(status));
    measureUnitValue = MeasureUnit::getLiterPer100Kilometers();
    measureUnit.adoptInstead(MeasureUnit::createLiterPerKilometer(status));
    measureUnitValue = MeasureUnit::getLiterPerKilometer();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallon(status));
    measureUnitValue = MeasureUnit::getMilePerGallon();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallonImperial(status));
    measureUnitValue = MeasureUnit::getMilePerGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createBit(status));
    measureUnitValue = MeasureUnit::getBit();
    measureUnit.adoptInstead(MeasureUnit::createByte(status));
    measureUnitValue = MeasureUnit::getByte();
    measureUnit.adoptInstead(MeasureUnit::createGigabit(status));
    measureUnitValue = MeasureUnit::getGigabit();
    measureUnit.adoptInstead(MeasureUnit::createGigabyte(status));
    measureUnitValue = MeasureUnit::getGigabyte();
    measureUnit.adoptInstead(MeasureUnit::createKilobit(status));
    measureUnitValue = MeasureUnit::getKilobit();
    measureUnit.adoptInstead(MeasureUnit::createKilobyte(status));
    measureUnitValue = MeasureUnit::getKilobyte();
    measureUnit.adoptInstead(MeasureUnit::createMegabit(status));
    measureUnitValue = MeasureUnit::getMegabit();
    measureUnit.adoptInstead(MeasureUnit::createMegabyte(status));
    measureUnitValue = MeasureUnit::getMegabyte();
    measureUnit.adoptInstead(MeasureUnit::createPetabyte(status));
    measureUnitValue = MeasureUnit::getPetabyte();
    measureUnit.adoptInstead(MeasureUnit::createTerabit(status));
    measureUnitValue = MeasureUnit::getTerabit();
    measureUnit.adoptInstead(MeasureUnit::createTerabyte(status));
    measureUnitValue = MeasureUnit::getTerabyte();
    measureUnit.adoptInstead(MeasureUnit::createCentury(status));
    measureUnitValue = MeasureUnit::getCentury();
    measureUnit.adoptInstead(MeasureUnit::createDay(status));
    measureUnitValue = MeasureUnit::getDay();
    measureUnit.adoptInstead(MeasureUnit::createDayPerson(status));
    measureUnitValue = MeasureUnit::getDayPerson();
    measureUnit.adoptInstead(MeasureUnit::createDecade(status));
    measureUnitValue = MeasureUnit::getDecade();
    measureUnit.adoptInstead(MeasureUnit::createHour(status));
    measureUnitValue = MeasureUnit::getHour();
    measureUnit.adoptInstead(MeasureUnit::createMicrosecond(status));
    measureUnitValue = MeasureUnit::getMicrosecond();
    measureUnit.adoptInstead(MeasureUnit::createMillisecond(status));
    measureUnitValue = MeasureUnit::getMillisecond();
    measureUnit.adoptInstead(MeasureUnit::createMinute(status));
    measureUnitValue = MeasureUnit::getMinute();
    measureUnit.adoptInstead(MeasureUnit::createMonth(status));
    measureUnitValue = MeasureUnit::getMonth();
    measureUnit.adoptInstead(MeasureUnit::createMonthPerson(status));
    measureUnitValue = MeasureUnit::getMonthPerson();
    measureUnit.adoptInstead(MeasureUnit::createNanosecond(status));
    measureUnitValue = MeasureUnit::getNanosecond();
    measureUnit.adoptInstead(MeasureUnit::createSecond(status));
    measureUnitValue = MeasureUnit::getSecond();
    measureUnit.adoptInstead(MeasureUnit::createWeek(status));
    measureUnitValue = MeasureUnit::getWeek();
    measureUnit.adoptInstead(MeasureUnit::createWeekPerson(status));
    measureUnitValue = MeasureUnit::getWeekPerson();
    measureUnit.adoptInstead(MeasureUnit::createYear(status));
    measureUnitValue = MeasureUnit::getYear();
    measureUnit.adoptInstead(MeasureUnit::createYearPerson(status));
    measureUnitValue = MeasureUnit::getYearPerson();
    measureUnit.adoptInstead(MeasureUnit::createAmpere(status));
    measureUnitValue = MeasureUnit::getAmpere();
    measureUnit.adoptInstead(MeasureUnit::createMilliampere(status));
    measureUnitValue = MeasureUnit::getMilliampere();
    measureUnit.adoptInstead(MeasureUnit::createOhm(status));
    measureUnitValue = MeasureUnit::getOhm();
    measureUnit.adoptInstead(MeasureUnit::createVolt(status));
    measureUnitValue = MeasureUnit::getVolt();
    measureUnit.adoptInstead(MeasureUnit::createBritishThermalUnit(status));
    measureUnitValue = MeasureUnit::getBritishThermalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCalorie(status));
    measureUnitValue = MeasureUnit::getCalorie();
    measureUnit.adoptInstead(MeasureUnit::createElectronvolt(status));
    measureUnitValue = MeasureUnit::getElectronvolt();
    measureUnit.adoptInstead(MeasureUnit::createFoodcalorie(status));
    measureUnitValue = MeasureUnit::getFoodcalorie();
    measureUnit.adoptInstead(MeasureUnit::createJoule(status));
    measureUnitValue = MeasureUnit::getJoule();
    measureUnit.adoptInstead(MeasureUnit::createKilocalorie(status));
    measureUnitValue = MeasureUnit::getKilocalorie();
    measureUnit.adoptInstead(MeasureUnit::createKilojoule(status));
    measureUnitValue = MeasureUnit::getKilojoule();
    measureUnit.adoptInstead(MeasureUnit::createKilowattHour(status));
    measureUnitValue = MeasureUnit::getKilowattHour();
    measureUnit.adoptInstead(MeasureUnit::createThermUs(status));
    measureUnitValue = MeasureUnit::getThermUs();
    measureUnit.adoptInstead(MeasureUnit::createKilowattHourPer100Kilometer(status));
    measureUnitValue = MeasureUnit::getKilowattHourPer100Kilometer();
    measureUnit.adoptInstead(MeasureUnit::createNewton(status));
    measureUnitValue = MeasureUnit::getNewton();
    measureUnit.adoptInstead(MeasureUnit::createPoundForce(status));
    measureUnitValue = MeasureUnit::getPoundForce();
    measureUnit.adoptInstead(MeasureUnit::createGigahertz(status));
    measureUnitValue = MeasureUnit::getGigahertz();
    measureUnit.adoptInstead(MeasureUnit::createHertz(status));
    measureUnitValue = MeasureUnit::getHertz();
    measureUnit.adoptInstead(MeasureUnit::createKilohertz(status));
    measureUnitValue = MeasureUnit::getKilohertz();
    measureUnit.adoptInstead(MeasureUnit::createMegahertz(status));
    measureUnitValue = MeasureUnit::getMegahertz();
    measureUnit.adoptInstead(MeasureUnit::createDot(status));
    measureUnitValue = MeasureUnit::getDot();
    measureUnit.adoptInstead(MeasureUnit::createDotPerCentimeter(status));
    measureUnitValue = MeasureUnit::getDotPerCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDotPerInch(status));
    measureUnitValue = MeasureUnit::getDotPerInch();
    measureUnit.adoptInstead(MeasureUnit::createEm(status));
    measureUnitValue = MeasureUnit::getEm();
    measureUnit.adoptInstead(MeasureUnit::createMegapixel(status));
    measureUnitValue = MeasureUnit::getMegapixel();
    measureUnit.adoptInstead(MeasureUnit::createPixel(status));
    measureUnitValue = MeasureUnit::getPixel();
    measureUnit.adoptInstead(MeasureUnit::createPixelPerCentimeter(status));
    measureUnitValue = MeasureUnit::getPixelPerCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createPixelPerInch(status));
    measureUnitValue = MeasureUnit::getPixelPerInch();
    measureUnit.adoptInstead(MeasureUnit::createAstronomicalUnit(status));
    measureUnitValue = MeasureUnit::getAstronomicalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCentimeter(status));
    measureUnitValue = MeasureUnit::getCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDecimeter(status));
    measureUnitValue = MeasureUnit::getDecimeter();
    measureUnit.adoptInstead(MeasureUnit::createEarthRadius(status));
    measureUnitValue = MeasureUnit::getEarthRadius();
    measureUnit.adoptInstead(MeasureUnit::createFathom(status));
    measureUnitValue = MeasureUnit::getFathom();
    measureUnit.adoptInstead(MeasureUnit::createFoot(status));
    measureUnitValue = MeasureUnit::getFoot();
    measureUnit.adoptInstead(MeasureUnit::createFurlong(status));
    measureUnitValue = MeasureUnit::getFurlong();
    measureUnit.adoptInstead(MeasureUnit::createInch(status));
    measureUnitValue = MeasureUnit::getInch();
    measureUnit.adoptInstead(MeasureUnit::createKilometer(status));
    measureUnitValue = MeasureUnit::getKilometer();
    measureUnit.adoptInstead(MeasureUnit::createLightYear(status));
    measureUnitValue = MeasureUnit::getLightYear();
    measureUnit.adoptInstead(MeasureUnit::createMeter(status));
    measureUnitValue = MeasureUnit::getMeter();
    measureUnit.adoptInstead(MeasureUnit::createMicrometer(status));
    measureUnitValue = MeasureUnit::getMicrometer();
    measureUnit.adoptInstead(MeasureUnit::createMile(status));
    measureUnitValue = MeasureUnit::getMile();
    measureUnit.adoptInstead(MeasureUnit::createMileScandinavian(status));
    measureUnitValue = MeasureUnit::getMileScandinavian();
    measureUnit.adoptInstead(MeasureUnit::createMillimeter(status));
    measureUnitValue = MeasureUnit::getMillimeter();
    measureUnit.adoptInstead(MeasureUnit::createNanometer(status));
    measureUnitValue = MeasureUnit::getNanometer();
    measureUnit.adoptInstead(MeasureUnit::createNauticalMile(status));
    measureUnitValue = MeasureUnit::getNauticalMile();
    measureUnit.adoptInstead(MeasureUnit::createParsec(status));
    measureUnitValue = MeasureUnit::getParsec();
    measureUnit.adoptInstead(MeasureUnit::createPicometer(status));
    measureUnitValue = MeasureUnit::getPicometer();
    measureUnit.adoptInstead(MeasureUnit::createPoint(status));
    measureUnitValue = MeasureUnit::getPoint();
    measureUnit.adoptInstead(MeasureUnit::createSolarRadius(status));
    measureUnitValue = MeasureUnit::getSolarRadius();
    measureUnit.adoptInstead(MeasureUnit::createYard(status));
    measureUnitValue = MeasureUnit::getYard();
    measureUnit.adoptInstead(MeasureUnit::createCandela(status));
    measureUnitValue = MeasureUnit::getCandela();
    measureUnit.adoptInstead(MeasureUnit::createLumen(status));
    measureUnitValue = MeasureUnit::getLumen();
    measureUnit.adoptInstead(MeasureUnit::createLux(status));
    measureUnitValue = MeasureUnit::getLux();
    measureUnit.adoptInstead(MeasureUnit::createSolarLuminosity(status));
    measureUnitValue = MeasureUnit::getSolarLuminosity();
    measureUnit.adoptInstead(MeasureUnit::createCarat(status));
    measureUnitValue = MeasureUnit::getCarat();
    measureUnit.adoptInstead(MeasureUnit::createDalton(status));
    measureUnitValue = MeasureUnit::getDalton();
    measureUnit.adoptInstead(MeasureUnit::createEarthMass(status));
    measureUnitValue = MeasureUnit::getEarthMass();
    measureUnit.adoptInstead(MeasureUnit::createGrain(status));
    measureUnitValue = MeasureUnit::getGrain();
    measureUnit.adoptInstead(MeasureUnit::createGram(status));
    measureUnitValue = MeasureUnit::getGram();
    measureUnit.adoptInstead(MeasureUnit::createKilogram(status));
    measureUnitValue = MeasureUnit::getKilogram();
    measureUnit.adoptInstead(MeasureUnit::createMetricTon(status));
    measureUnitValue = MeasureUnit::getMetricTon();
    measureUnit.adoptInstead(MeasureUnit::createMicrogram(status));
    measureUnitValue = MeasureUnit::getMicrogram();
    measureUnit.adoptInstead(MeasureUnit::createMilligram(status));
    measureUnitValue = MeasureUnit::getMilligram();
    measureUnit.adoptInstead(MeasureUnit::createOunce(status));
    measureUnitValue = MeasureUnit::getOunce();
    measureUnit.adoptInstead(MeasureUnit::createOunceTroy(status));
    measureUnitValue = MeasureUnit::getOunceTroy();
    measureUnit.adoptInstead(MeasureUnit::createPound(status));
    measureUnitValue = MeasureUnit::getPound();
    measureUnit.adoptInstead(MeasureUnit::createSolarMass(status));
    measureUnitValue = MeasureUnit::getSolarMass();
    measureUnit.adoptInstead(MeasureUnit::createStone(status));
    measureUnitValue = MeasureUnit::getStone();
    measureUnit.adoptInstead(MeasureUnit::createTon(status));
    measureUnitValue = MeasureUnit::getTon();
    measureUnit.adoptInstead(MeasureUnit::createGigawatt(status));
    measureUnitValue = MeasureUnit::getGigawatt();
    measureUnit.adoptInstead(MeasureUnit::createHorsepower(status));
    measureUnitValue = MeasureUnit::getHorsepower();
    measureUnit.adoptInstead(MeasureUnit::createKilowatt(status));
    measureUnitValue = MeasureUnit::getKilowatt();
    measureUnit.adoptInstead(MeasureUnit::createMegawatt(status));
    measureUnitValue = MeasureUnit::getMegawatt();
    measureUnit.adoptInstead(MeasureUnit::createMilliwatt(status));
    measureUnitValue = MeasureUnit::getMilliwatt();
    measureUnit.adoptInstead(MeasureUnit::createWatt(status));
    measureUnitValue = MeasureUnit::getWatt();
    measureUnit.adoptInstead(MeasureUnit::createAtmosphere(status));
    measureUnitValue = MeasureUnit::getAtmosphere();
    measureUnit.adoptInstead(MeasureUnit::createBar(status));
    measureUnitValue = MeasureUnit::getBar();
    measureUnit.adoptInstead(MeasureUnit::createHectopascal(status));
    measureUnitValue = MeasureUnit::getHectopascal();
    measureUnit.adoptInstead(MeasureUnit::createInchHg(status));
    measureUnitValue = MeasureUnit::getInchHg();
    measureUnit.adoptInstead(MeasureUnit::createKilopascal(status));
    measureUnitValue = MeasureUnit::getKilopascal();
    measureUnit.adoptInstead(MeasureUnit::createMegapascal(status));
    measureUnitValue = MeasureUnit::getMegapascal();
    measureUnit.adoptInstead(MeasureUnit::createMillibar(status));
    measureUnitValue = MeasureUnit::getMillibar();
    measureUnit.adoptInstead(MeasureUnit::createMillimeterOfMercury(status));
    measureUnitValue = MeasureUnit::getMillimeterOfMercury();
    measureUnit.adoptInstead(MeasureUnit::createPascal(status));
    measureUnitValue = MeasureUnit::getPascal();
    measureUnit.adoptInstead(MeasureUnit::createPoundPerSquareInch(status));
    measureUnitValue = MeasureUnit::getPoundPerSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createKilometerPerHour(status));
    measureUnitValue = MeasureUnit::getKilometerPerHour();
    measureUnit.adoptInstead(MeasureUnit::createKnot(status));
    measureUnitValue = MeasureUnit::getKnot();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnitValue = MeasureUnit::getMeterPerSecond();
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnitValue = MeasureUnit::getMilePerHour();
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnitValue = MeasureUnit::getCelsius();
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnitValue = MeasureUnit::getFahrenheit();
    measureUnit.adoptInstead(MeasureUnit::createGenericTemperature(status));
    measureUnitValue = MeasureUnit::getGenericTemperature();
    measureUnit.adoptInstead(MeasureUnit::createKelvin(status));
    measureUnitValue = MeasureUnit::getKelvin();
    measureUnit.adoptInstead(MeasureUnit::createNewtonMeter(status));
    measureUnitValue = MeasureUnit::getNewtonMeter();
    measureUnit.adoptInstead(MeasureUnit::createPoundFoot(status));
    measureUnitValue = MeasureUnit::getPoundFoot();
    measureUnit.adoptInstead(MeasureUnit::createAcreFoot(status));
    measureUnitValue = MeasureUnit::getAcreFoot();
    measureUnit.adoptInstead(MeasureUnit::createBarrel(status));
    measureUnitValue = MeasureUnit::getBarrel();
    measureUnit.adoptInstead(MeasureUnit::createBushel(status));
    measureUnitValue = MeasureUnit::getBushel();
    measureUnit.adoptInstead(MeasureUnit::createCentiliter(status));
    measureUnitValue = MeasureUnit::getCentiliter();
    measureUnit.adoptInstead(MeasureUnit::createCubicCentimeter(status));
    measureUnitValue = MeasureUnit::getCubicCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicFoot(status));
    measureUnitValue = MeasureUnit::getCubicFoot();
    measureUnit.adoptInstead(MeasureUnit::createCubicInch(status));
    measureUnitValue = MeasureUnit::getCubicInch();
    measureUnit.adoptInstead(MeasureUnit::createCubicKilometer(status));
    measureUnitValue = MeasureUnit::getCubicKilometer();
    measureUnit.adoptInstead(MeasureUnit::createCubicMeter(status));
    measureUnitValue = MeasureUnit::getCubicMeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicMile(status));
    measureUnitValue = MeasureUnit::getCubicMile();
    measureUnit.adoptInstead(MeasureUnit::createCubicYard(status));
    measureUnitValue = MeasureUnit::getCubicYard();
    measureUnit.adoptInstead(MeasureUnit::createCup(status));
    measureUnitValue = MeasureUnit::getCup();
    measureUnit.adoptInstead(MeasureUnit::createCupMetric(status));
    measureUnitValue = MeasureUnit::getCupMetric();
    measureUnit.adoptInstead(MeasureUnit::createDeciliter(status));
    measureUnitValue = MeasureUnit::getDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createDessertSpoon(status));
    measureUnitValue = MeasureUnit::getDessertSpoon();
    measureUnit.adoptInstead(MeasureUnit::createDessertSpoonImperial(status));
    measureUnitValue = MeasureUnit::getDessertSpoonImperial();
    measureUnit.adoptInstead(MeasureUnit::createDram(status));
    measureUnitValue = MeasureUnit::getDram();
    measureUnit.adoptInstead(MeasureUnit::createDrop(status));
    measureUnitValue = MeasureUnit::getDrop();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunce(status));
    measureUnitValue = MeasureUnit::getFluidOunce();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunceImperial(status));
    measureUnitValue = MeasureUnit::getFluidOunceImperial();
    measureUnit.adoptInstead(MeasureUnit::createGallon(status));
    measureUnitValue = MeasureUnit::getGallon();
    measureUnit.adoptInstead(MeasureUnit::createGallonImperial(status));
    measureUnitValue = MeasureUnit::getGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createHectoliter(status));
    measureUnitValue = MeasureUnit::getHectoliter();
    measureUnit.adoptInstead(MeasureUnit::createJigger(status));
    measureUnitValue = MeasureUnit::getJigger();
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    measureUnitValue = MeasureUnit::getLiter();
    measureUnit.adoptInstead(MeasureUnit::createMegaliter(status));
    measureUnitValue = MeasureUnit::getMegaliter();
    measureUnit.adoptInstead(MeasureUnit::createMilliliter(status));
    measureUnitValue = MeasureUnit::getMilliliter();
    measureUnit.adoptInstead(MeasureUnit::createPinch(status));
    measureUnitValue = MeasureUnit::getPinch();
    measureUnit.adoptInstead(MeasureUnit::createPint(status));
    measureUnitValue = MeasureUnit::getPint();
    measureUnit.adoptInstead(MeasureUnit::createPintMetric(status));
    measureUnitValue = MeasureUnit::getPintMetric();
    measureUnit.adoptInstead(MeasureUnit::createQuart(status));
    measureUnitValue = MeasureUnit::getQuart();
    measureUnit.adoptInstead(MeasureUnit::createQuartImperial(status));
    measureUnitValue = MeasureUnit::getQuartImperial();
    measureUnit.adoptInstead(MeasureUnit::createTablespoon(status));
    measureUnitValue = MeasureUnit::getTablespoon();
    measureUnit.adoptInstead(MeasureUnit::createTeaspoon(status));
    measureUnitValue = MeasureUnit::getTeaspoon();
    assertSuccess("", status);
}

// TestCompatible71 would be identical to TestCompatible70,
// no need to add it

void MeasureFormatTest::TestCompatible72() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    MeasureUnit measureUnitValue;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnitValue = MeasureUnit::getGForce();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecondSquared(status));
    measureUnitValue = MeasureUnit::getMeterPerSecondSquared();
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnitValue = MeasureUnit::getArcMinute();
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnitValue = MeasureUnit::getArcSecond();
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnitValue = MeasureUnit::getDegree();
    measureUnit.adoptInstead(MeasureUnit::createRadian(status));
    measureUnitValue = MeasureUnit::getRadian();
    measureUnit.adoptInstead(MeasureUnit::createRevolutionAngle(status));
    measureUnitValue = MeasureUnit::getRevolutionAngle();
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnitValue = MeasureUnit::getAcre();
    measureUnit.adoptInstead(MeasureUnit::createDunam(status));
    measureUnitValue = MeasureUnit::getDunam();
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnitValue = MeasureUnit::getHectare();
    measureUnit.adoptInstead(MeasureUnit::createSquareCentimeter(status));
    measureUnitValue = MeasureUnit::getSquareCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnitValue = MeasureUnit::getSquareFoot();
    measureUnit.adoptInstead(MeasureUnit::createSquareInch(status));
    measureUnitValue = MeasureUnit::getSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnitValue = MeasureUnit::getSquareKilometer();
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnitValue = MeasureUnit::getSquareMeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnitValue = MeasureUnit::getSquareMile();
    measureUnit.adoptInstead(MeasureUnit::createSquareYard(status));
    measureUnitValue = MeasureUnit::getSquareYard();
    measureUnit.adoptInstead(MeasureUnit::createItem(status));
    measureUnitValue = MeasureUnit::getItem();
    measureUnit.adoptInstead(MeasureUnit::createKarat(status));
    measureUnitValue = MeasureUnit::getKarat();
    measureUnit.adoptInstead(MeasureUnit::createMilligramOfglucosePerDeciliter(status));
    measureUnitValue = MeasureUnit::getMilligramOfglucosePerDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createMilligramPerDeciliter(status));
    measureUnitValue = MeasureUnit::getMilligramPerDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createMillimolePerLiter(status));
    measureUnitValue = MeasureUnit::getMillimolePerLiter();
    measureUnit.adoptInstead(MeasureUnit::createMole(status));
    measureUnitValue = MeasureUnit::getMole();
    measureUnit.adoptInstead(MeasureUnit::createPercent(status));
    measureUnitValue = MeasureUnit::getPercent();
    measureUnit.adoptInstead(MeasureUnit::createPermille(status));
    measureUnitValue = MeasureUnit::getPermille();
    measureUnit.adoptInstead(MeasureUnit::createPartPerMillion(status));
    measureUnitValue = MeasureUnit::getPartPerMillion();
    measureUnit.adoptInstead(MeasureUnit::createPermyriad(status));
    measureUnitValue = MeasureUnit::getPermyriad();
    measureUnit.adoptInstead(MeasureUnit::createLiterPer100Kilometers(status));
    measureUnitValue = MeasureUnit::getLiterPer100Kilometers();
    measureUnit.adoptInstead(MeasureUnit::createLiterPerKilometer(status));
    measureUnitValue = MeasureUnit::getLiterPerKilometer();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallon(status));
    measureUnitValue = MeasureUnit::getMilePerGallon();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallonImperial(status));
    measureUnitValue = MeasureUnit::getMilePerGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createBit(status));
    measureUnitValue = MeasureUnit::getBit();
    measureUnit.adoptInstead(MeasureUnit::createByte(status));
    measureUnitValue = MeasureUnit::getByte();
    measureUnit.adoptInstead(MeasureUnit::createGigabit(status));
    measureUnitValue = MeasureUnit::getGigabit();
    measureUnit.adoptInstead(MeasureUnit::createGigabyte(status));
    measureUnitValue = MeasureUnit::getGigabyte();
    measureUnit.adoptInstead(MeasureUnit::createKilobit(status));
    measureUnitValue = MeasureUnit::getKilobit();
    measureUnit.adoptInstead(MeasureUnit::createKilobyte(status));
    measureUnitValue = MeasureUnit::getKilobyte();
    measureUnit.adoptInstead(MeasureUnit::createMegabit(status));
    measureUnitValue = MeasureUnit::getMegabit();
    measureUnit.adoptInstead(MeasureUnit::createMegabyte(status));
    measureUnitValue = MeasureUnit::getMegabyte();
    measureUnit.adoptInstead(MeasureUnit::createPetabyte(status));
    measureUnitValue = MeasureUnit::getPetabyte();
    measureUnit.adoptInstead(MeasureUnit::createTerabit(status));
    measureUnitValue = MeasureUnit::getTerabit();
    measureUnit.adoptInstead(MeasureUnit::createTerabyte(status));
    measureUnitValue = MeasureUnit::getTerabyte();
    measureUnit.adoptInstead(MeasureUnit::createCentury(status));
    measureUnitValue = MeasureUnit::getCentury();
    measureUnit.adoptInstead(MeasureUnit::createDay(status));
    measureUnitValue = MeasureUnit::getDay();
    measureUnit.adoptInstead(MeasureUnit::createDayPerson(status));
    measureUnitValue = MeasureUnit::getDayPerson();
    measureUnit.adoptInstead(MeasureUnit::createDecade(status));
    measureUnitValue = MeasureUnit::getDecade();
    measureUnit.adoptInstead(MeasureUnit::createHour(status));
    measureUnitValue = MeasureUnit::getHour();
    measureUnit.adoptInstead(MeasureUnit::createMicrosecond(status));
    measureUnitValue = MeasureUnit::getMicrosecond();
    measureUnit.adoptInstead(MeasureUnit::createMillisecond(status));
    measureUnitValue = MeasureUnit::getMillisecond();
    measureUnit.adoptInstead(MeasureUnit::createMinute(status));
    measureUnitValue = MeasureUnit::getMinute();
    measureUnit.adoptInstead(MeasureUnit::createMonth(status));
    measureUnitValue = MeasureUnit::getMonth();
    measureUnit.adoptInstead(MeasureUnit::createMonthPerson(status));
    measureUnitValue = MeasureUnit::getMonthPerson();
    measureUnit.adoptInstead(MeasureUnit::createNanosecond(status));
    measureUnitValue = MeasureUnit::getNanosecond();
    measureUnit.adoptInstead(MeasureUnit::createQuarter(status));
    measureUnitValue = MeasureUnit::getQuarter();
    measureUnit.adoptInstead(MeasureUnit::createSecond(status));
    measureUnitValue = MeasureUnit::getSecond();
    measureUnit.adoptInstead(MeasureUnit::createWeek(status));
    measureUnitValue = MeasureUnit::getWeek();
    measureUnit.adoptInstead(MeasureUnit::createWeekPerson(status));
    measureUnitValue = MeasureUnit::getWeekPerson();
    measureUnit.adoptInstead(MeasureUnit::createYear(status));
    measureUnitValue = MeasureUnit::getYear();
    measureUnit.adoptInstead(MeasureUnit::createYearPerson(status));
    measureUnitValue = MeasureUnit::getYearPerson();
    measureUnit.adoptInstead(MeasureUnit::createAmpere(status));
    measureUnitValue = MeasureUnit::getAmpere();
    measureUnit.adoptInstead(MeasureUnit::createMilliampere(status));
    measureUnitValue = MeasureUnit::getMilliampere();
    measureUnit.adoptInstead(MeasureUnit::createOhm(status));
    measureUnitValue = MeasureUnit::getOhm();
    measureUnit.adoptInstead(MeasureUnit::createVolt(status));
    measureUnitValue = MeasureUnit::getVolt();
    measureUnit.adoptInstead(MeasureUnit::createBritishThermalUnit(status));
    measureUnitValue = MeasureUnit::getBritishThermalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCalorie(status));
    measureUnitValue = MeasureUnit::getCalorie();
    measureUnit.adoptInstead(MeasureUnit::createElectronvolt(status));
    measureUnitValue = MeasureUnit::getElectronvolt();
    measureUnit.adoptInstead(MeasureUnit::createFoodcalorie(status));
    measureUnitValue = MeasureUnit::getFoodcalorie();
    measureUnit.adoptInstead(MeasureUnit::createJoule(status));
    measureUnitValue = MeasureUnit::getJoule();
    measureUnit.adoptInstead(MeasureUnit::createKilocalorie(status));
    measureUnitValue = MeasureUnit::getKilocalorie();
    measureUnit.adoptInstead(MeasureUnit::createKilojoule(status));
    measureUnitValue = MeasureUnit::getKilojoule();
    measureUnit.adoptInstead(MeasureUnit::createKilowattHour(status));
    measureUnitValue = MeasureUnit::getKilowattHour();
    measureUnit.adoptInstead(MeasureUnit::createThermUs(status));
    measureUnitValue = MeasureUnit::getThermUs();
    measureUnit.adoptInstead(MeasureUnit::createKilowattHourPer100Kilometer(status));
    measureUnitValue = MeasureUnit::getKilowattHourPer100Kilometer();
    measureUnit.adoptInstead(MeasureUnit::createNewton(status));
    measureUnitValue = MeasureUnit::getNewton();
    measureUnit.adoptInstead(MeasureUnit::createPoundForce(status));
    measureUnitValue = MeasureUnit::getPoundForce();
    measureUnit.adoptInstead(MeasureUnit::createGigahertz(status));
    measureUnitValue = MeasureUnit::getGigahertz();
    measureUnit.adoptInstead(MeasureUnit::createHertz(status));
    measureUnitValue = MeasureUnit::getHertz();
    measureUnit.adoptInstead(MeasureUnit::createKilohertz(status));
    measureUnitValue = MeasureUnit::getKilohertz();
    measureUnit.adoptInstead(MeasureUnit::createMegahertz(status));
    measureUnitValue = MeasureUnit::getMegahertz();
    measureUnit.adoptInstead(MeasureUnit::createDot(status));
    measureUnitValue = MeasureUnit::getDot();
    measureUnit.adoptInstead(MeasureUnit::createDotPerCentimeter(status));
    measureUnitValue = MeasureUnit::getDotPerCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDotPerInch(status));
    measureUnitValue = MeasureUnit::getDotPerInch();
    measureUnit.adoptInstead(MeasureUnit::createEm(status));
    measureUnitValue = MeasureUnit::getEm();
    measureUnit.adoptInstead(MeasureUnit::createMegapixel(status));
    measureUnitValue = MeasureUnit::getMegapixel();
    measureUnit.adoptInstead(MeasureUnit::createPixel(status));
    measureUnitValue = MeasureUnit::getPixel();
    measureUnit.adoptInstead(MeasureUnit::createPixelPerCentimeter(status));
    measureUnitValue = MeasureUnit::getPixelPerCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createPixelPerInch(status));
    measureUnitValue = MeasureUnit::getPixelPerInch();
    measureUnit.adoptInstead(MeasureUnit::createAstronomicalUnit(status));
    measureUnitValue = MeasureUnit::getAstronomicalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCentimeter(status));
    measureUnitValue = MeasureUnit::getCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDecimeter(status));
    measureUnitValue = MeasureUnit::getDecimeter();
    measureUnit.adoptInstead(MeasureUnit::createEarthRadius(status));
    measureUnitValue = MeasureUnit::getEarthRadius();
    measureUnit.adoptInstead(MeasureUnit::createFathom(status));
    measureUnitValue = MeasureUnit::getFathom();
    measureUnit.adoptInstead(MeasureUnit::createFoot(status));
    measureUnitValue = MeasureUnit::getFoot();
    measureUnit.adoptInstead(MeasureUnit::createFurlong(status));
    measureUnitValue = MeasureUnit::getFurlong();
    measureUnit.adoptInstead(MeasureUnit::createInch(status));
    measureUnitValue = MeasureUnit::getInch();
    measureUnit.adoptInstead(MeasureUnit::createKilometer(status));
    measureUnitValue = MeasureUnit::getKilometer();
    measureUnit.adoptInstead(MeasureUnit::createLightYear(status));
    measureUnitValue = MeasureUnit::getLightYear();
    measureUnit.adoptInstead(MeasureUnit::createMeter(status));
    measureUnitValue = MeasureUnit::getMeter();
    measureUnit.adoptInstead(MeasureUnit::createMicrometer(status));
    measureUnitValue = MeasureUnit::getMicrometer();
    measureUnit.adoptInstead(MeasureUnit::createMile(status));
    measureUnitValue = MeasureUnit::getMile();
    measureUnit.adoptInstead(MeasureUnit::createMileScandinavian(status));
    measureUnitValue = MeasureUnit::getMileScandinavian();
    measureUnit.adoptInstead(MeasureUnit::createMillimeter(status));
    measureUnitValue = MeasureUnit::getMillimeter();
    measureUnit.adoptInstead(MeasureUnit::createNanometer(status));
    measureUnitValue = MeasureUnit::getNanometer();
    measureUnit.adoptInstead(MeasureUnit::createNauticalMile(status));
    measureUnitValue = MeasureUnit::getNauticalMile();
    measureUnit.adoptInstead(MeasureUnit::createParsec(status));
    measureUnitValue = MeasureUnit::getParsec();
    measureUnit.adoptInstead(MeasureUnit::createPicometer(status));
    measureUnitValue = MeasureUnit::getPicometer();
    measureUnit.adoptInstead(MeasureUnit::createPoint(status));
    measureUnitValue = MeasureUnit::getPoint();
    measureUnit.adoptInstead(MeasureUnit::createSolarRadius(status));
    measureUnitValue = MeasureUnit::getSolarRadius();
    measureUnit.adoptInstead(MeasureUnit::createYard(status));
    measureUnitValue = MeasureUnit::getYard();
    measureUnit.adoptInstead(MeasureUnit::createCandela(status));
    measureUnitValue = MeasureUnit::getCandela();
    measureUnit.adoptInstead(MeasureUnit::createLumen(status));
    measureUnitValue = MeasureUnit::getLumen();
    measureUnit.adoptInstead(MeasureUnit::createLux(status));
    measureUnitValue = MeasureUnit::getLux();
    measureUnit.adoptInstead(MeasureUnit::createSolarLuminosity(status));
    measureUnitValue = MeasureUnit::getSolarLuminosity();
    measureUnit.adoptInstead(MeasureUnit::createCarat(status));
    measureUnitValue = MeasureUnit::getCarat();
    measureUnit.adoptInstead(MeasureUnit::createDalton(status));
    measureUnitValue = MeasureUnit::getDalton();
    measureUnit.adoptInstead(MeasureUnit::createEarthMass(status));
    measureUnitValue = MeasureUnit::getEarthMass();
    measureUnit.adoptInstead(MeasureUnit::createGrain(status));
    measureUnitValue = MeasureUnit::getGrain();
    measureUnit.adoptInstead(MeasureUnit::createGram(status));
    measureUnitValue = MeasureUnit::getGram();
    measureUnit.adoptInstead(MeasureUnit::createKilogram(status));
    measureUnitValue = MeasureUnit::getKilogram();
    measureUnit.adoptInstead(MeasureUnit::createMicrogram(status));
    measureUnitValue = MeasureUnit::getMicrogram();
    measureUnit.adoptInstead(MeasureUnit::createMilligram(status));
    measureUnitValue = MeasureUnit::getMilligram();
    measureUnit.adoptInstead(MeasureUnit::createOunce(status));
    measureUnitValue = MeasureUnit::getOunce();
    measureUnit.adoptInstead(MeasureUnit::createOunceTroy(status));
    measureUnitValue = MeasureUnit::getOunceTroy();
    measureUnit.adoptInstead(MeasureUnit::createPound(status));
    measureUnitValue = MeasureUnit::getPound();
    measureUnit.adoptInstead(MeasureUnit::createSolarMass(status));
    measureUnitValue = MeasureUnit::getSolarMass();
    measureUnit.adoptInstead(MeasureUnit::createStone(status));
    measureUnitValue = MeasureUnit::getStone();
    measureUnit.adoptInstead(MeasureUnit::createTon(status));
    measureUnitValue = MeasureUnit::getTon();
    measureUnit.adoptInstead(MeasureUnit::createTonne(status));
    measureUnitValue = MeasureUnit::getTonne();
    measureUnit.adoptInstead(MeasureUnit::createGigawatt(status));
    measureUnitValue = MeasureUnit::getGigawatt();
    measureUnit.adoptInstead(MeasureUnit::createHorsepower(status));
    measureUnitValue = MeasureUnit::getHorsepower();
    measureUnit.adoptInstead(MeasureUnit::createKilowatt(status));
    measureUnitValue = MeasureUnit::getKilowatt();
    measureUnit.adoptInstead(MeasureUnit::createMegawatt(status));
    measureUnitValue = MeasureUnit::getMegawatt();
    measureUnit.adoptInstead(MeasureUnit::createMilliwatt(status));
    measureUnitValue = MeasureUnit::getMilliwatt();
    measureUnit.adoptInstead(MeasureUnit::createWatt(status));
    measureUnitValue = MeasureUnit::getWatt();
    measureUnit.adoptInstead(MeasureUnit::createAtmosphere(status));
    measureUnitValue = MeasureUnit::getAtmosphere();
    measureUnit.adoptInstead(MeasureUnit::createBar(status));
    measureUnitValue = MeasureUnit::getBar();
    measureUnit.adoptInstead(MeasureUnit::createHectopascal(status));
    measureUnitValue = MeasureUnit::getHectopascal();
    measureUnit.adoptInstead(MeasureUnit::createInchHg(status));
    measureUnitValue = MeasureUnit::getInchHg();
    measureUnit.adoptInstead(MeasureUnit::createKilopascal(status));
    measureUnitValue = MeasureUnit::getKilopascal();
    measureUnit.adoptInstead(MeasureUnit::createMegapascal(status));
    measureUnitValue = MeasureUnit::getMegapascal();
    measureUnit.adoptInstead(MeasureUnit::createMillibar(status));
    measureUnitValue = MeasureUnit::getMillibar();
    measureUnit.adoptInstead(MeasureUnit::createMillimeterOfMercury(status));
    measureUnitValue = MeasureUnit::getMillimeterOfMercury();
    measureUnit.adoptInstead(MeasureUnit::createPascal(status));
    measureUnitValue = MeasureUnit::getPascal();
    measureUnit.adoptInstead(MeasureUnit::createPoundPerSquareInch(status));
    measureUnitValue = MeasureUnit::getPoundPerSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createKilometerPerHour(status));
    measureUnitValue = MeasureUnit::getKilometerPerHour();
    measureUnit.adoptInstead(MeasureUnit::createKnot(status));
    measureUnitValue = MeasureUnit::getKnot();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnitValue = MeasureUnit::getMeterPerSecond();
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnitValue = MeasureUnit::getMilePerHour();
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnitValue = MeasureUnit::getCelsius();
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnitValue = MeasureUnit::getFahrenheit();
    measureUnit.adoptInstead(MeasureUnit::createGenericTemperature(status));
    measureUnitValue = MeasureUnit::getGenericTemperature();
    measureUnit.adoptInstead(MeasureUnit::createKelvin(status));
    measureUnitValue = MeasureUnit::getKelvin();
    measureUnit.adoptInstead(MeasureUnit::createNewtonMeter(status));
    measureUnitValue = MeasureUnit::getNewtonMeter();
    measureUnit.adoptInstead(MeasureUnit::createPoundFoot(status));
    measureUnitValue = MeasureUnit::getPoundFoot();
    measureUnit.adoptInstead(MeasureUnit::createAcreFoot(status));
    measureUnitValue = MeasureUnit::getAcreFoot();
    measureUnit.adoptInstead(MeasureUnit::createBarrel(status));
    measureUnitValue = MeasureUnit::getBarrel();
    measureUnit.adoptInstead(MeasureUnit::createBushel(status));
    measureUnitValue = MeasureUnit::getBushel();
    measureUnit.adoptInstead(MeasureUnit::createCentiliter(status));
    measureUnitValue = MeasureUnit::getCentiliter();
    measureUnit.adoptInstead(MeasureUnit::createCubicCentimeter(status));
    measureUnitValue = MeasureUnit::getCubicCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicFoot(status));
    measureUnitValue = MeasureUnit::getCubicFoot();
    measureUnit.adoptInstead(MeasureUnit::createCubicInch(status));
    measureUnitValue = MeasureUnit::getCubicInch();
    measureUnit.adoptInstead(MeasureUnit::createCubicKilometer(status));
    measureUnitValue = MeasureUnit::getCubicKilometer();
    measureUnit.adoptInstead(MeasureUnit::createCubicMeter(status));
    measureUnitValue = MeasureUnit::getCubicMeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicMile(status));
    measureUnitValue = MeasureUnit::getCubicMile();
    measureUnit.adoptInstead(MeasureUnit::createCubicYard(status));
    measureUnitValue = MeasureUnit::getCubicYard();
    measureUnit.adoptInstead(MeasureUnit::createCup(status));
    measureUnitValue = MeasureUnit::getCup();
    measureUnit.adoptInstead(MeasureUnit::createCupMetric(status));
    measureUnitValue = MeasureUnit::getCupMetric();
    measureUnit.adoptInstead(MeasureUnit::createDeciliter(status));
    measureUnitValue = MeasureUnit::getDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createDessertSpoon(status));
    measureUnitValue = MeasureUnit::getDessertSpoon();
    measureUnit.adoptInstead(MeasureUnit::createDessertSpoonImperial(status));
    measureUnitValue = MeasureUnit::getDessertSpoonImperial();
    measureUnit.adoptInstead(MeasureUnit::createDram(status));
    measureUnitValue = MeasureUnit::getDram();
    measureUnit.adoptInstead(MeasureUnit::createDrop(status));
    measureUnitValue = MeasureUnit::getDrop();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunce(status));
    measureUnitValue = MeasureUnit::getFluidOunce();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunceImperial(status));
    measureUnitValue = MeasureUnit::getFluidOunceImperial();
    measureUnit.adoptInstead(MeasureUnit::createGallon(status));
    measureUnitValue = MeasureUnit::getGallon();
    measureUnit.adoptInstead(MeasureUnit::createGallonImperial(status));
    measureUnitValue = MeasureUnit::getGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createHectoliter(status));
    measureUnitValue = MeasureUnit::getHectoliter();
    measureUnit.adoptInstead(MeasureUnit::createJigger(status));
    measureUnitValue = MeasureUnit::getJigger();
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    measureUnitValue = MeasureUnit::getLiter();
    measureUnit.adoptInstead(MeasureUnit::createMegaliter(status));
    measureUnitValue = MeasureUnit::getMegaliter();
    measureUnit.adoptInstead(MeasureUnit::createMilliliter(status));
    measureUnitValue = MeasureUnit::getMilliliter();
    measureUnit.adoptInstead(MeasureUnit::createPinch(status));
    measureUnitValue = MeasureUnit::getPinch();
    measureUnit.adoptInstead(MeasureUnit::createPint(status));
    measureUnitValue = MeasureUnit::getPint();
    measureUnit.adoptInstead(MeasureUnit::createPintMetric(status));
    measureUnitValue = MeasureUnit::getPintMetric();
    measureUnit.adoptInstead(MeasureUnit::createQuart(status));
    measureUnitValue = MeasureUnit::getQuart();
    measureUnit.adoptInstead(MeasureUnit::createQuartImperial(status));
    measureUnitValue = MeasureUnit::getQuartImperial();
    measureUnit.adoptInstead(MeasureUnit::createTablespoon(status));
    measureUnitValue = MeasureUnit::getTablespoon();
    measureUnit.adoptInstead(MeasureUnit::createTeaspoon(status));
    measureUnitValue = MeasureUnit::getTeaspoon();
    assertSuccess("", status);
}

void MeasureFormatTest::TestCompatible73() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    MeasureUnit measureUnitValue;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnitValue = MeasureUnit::getGForce();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecondSquared(status));
    measureUnitValue = MeasureUnit::getMeterPerSecondSquared();
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnitValue = MeasureUnit::getArcMinute();
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnitValue = MeasureUnit::getArcSecond();
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnitValue = MeasureUnit::getDegree();
    measureUnit.adoptInstead(MeasureUnit::createRadian(status));
    measureUnitValue = MeasureUnit::getRadian();
    measureUnit.adoptInstead(MeasureUnit::createRevolutionAngle(status));
    measureUnitValue = MeasureUnit::getRevolutionAngle();
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnitValue = MeasureUnit::getAcre();
    measureUnit.adoptInstead(MeasureUnit::createDunam(status));
    measureUnitValue = MeasureUnit::getDunam();
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnitValue = MeasureUnit::getHectare();
    measureUnit.adoptInstead(MeasureUnit::createSquareCentimeter(status));
    measureUnitValue = MeasureUnit::getSquareCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnitValue = MeasureUnit::getSquareFoot();
    measureUnit.adoptInstead(MeasureUnit::createSquareInch(status));
    measureUnitValue = MeasureUnit::getSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnitValue = MeasureUnit::getSquareKilometer();
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnitValue = MeasureUnit::getSquareMeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnitValue = MeasureUnit::getSquareMile();
    measureUnit.adoptInstead(MeasureUnit::createSquareYard(status));
    measureUnitValue = MeasureUnit::getSquareYard();
    measureUnit.adoptInstead(MeasureUnit::createItem(status));
    measureUnitValue = MeasureUnit::getItem();
    measureUnit.adoptInstead(MeasureUnit::createKarat(status));
    measureUnitValue = MeasureUnit::getKarat();
    measureUnit.adoptInstead(MeasureUnit::createMilligramOfglucosePerDeciliter(status));
    measureUnitValue = MeasureUnit::getMilligramOfglucosePerDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createMilligramPerDeciliter(status));
    measureUnitValue = MeasureUnit::getMilligramPerDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createMillimolePerLiter(status));
    measureUnitValue = MeasureUnit::getMillimolePerLiter();
    measureUnit.adoptInstead(MeasureUnit::createMole(status));
    measureUnitValue = MeasureUnit::getMole();
    measureUnit.adoptInstead(MeasureUnit::createPercent(status));
    measureUnitValue = MeasureUnit::getPercent();
    measureUnit.adoptInstead(MeasureUnit::createPermille(status));
    measureUnitValue = MeasureUnit::getPermille();
    measureUnit.adoptInstead(MeasureUnit::createPartPerMillion(status));
    measureUnitValue = MeasureUnit::getPartPerMillion();
    measureUnit.adoptInstead(MeasureUnit::createPermyriad(status));
    measureUnitValue = MeasureUnit::getPermyriad();
    measureUnit.adoptInstead(MeasureUnit::createLiterPer100Kilometers(status));
    measureUnitValue = MeasureUnit::getLiterPer100Kilometers();
    measureUnit.adoptInstead(MeasureUnit::createLiterPerKilometer(status));
    measureUnitValue = MeasureUnit::getLiterPerKilometer();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallon(status));
    measureUnitValue = MeasureUnit::getMilePerGallon();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallonImperial(status));
    measureUnitValue = MeasureUnit::getMilePerGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createBit(status));
    measureUnitValue = MeasureUnit::getBit();
    measureUnit.adoptInstead(MeasureUnit::createByte(status));
    measureUnitValue = MeasureUnit::getByte();
    measureUnit.adoptInstead(MeasureUnit::createGigabit(status));
    measureUnitValue = MeasureUnit::getGigabit();
    measureUnit.adoptInstead(MeasureUnit::createGigabyte(status));
    measureUnitValue = MeasureUnit::getGigabyte();
    measureUnit.adoptInstead(MeasureUnit::createKilobit(status));
    measureUnitValue = MeasureUnit::getKilobit();
    measureUnit.adoptInstead(MeasureUnit::createKilobyte(status));
    measureUnitValue = MeasureUnit::getKilobyte();
    measureUnit.adoptInstead(MeasureUnit::createMegabit(status));
    measureUnitValue = MeasureUnit::getMegabit();
    measureUnit.adoptInstead(MeasureUnit::createMegabyte(status));
    measureUnitValue = MeasureUnit::getMegabyte();
    measureUnit.adoptInstead(MeasureUnit::createPetabyte(status));
    measureUnitValue = MeasureUnit::getPetabyte();
    measureUnit.adoptInstead(MeasureUnit::createTerabit(status));
    measureUnitValue = MeasureUnit::getTerabit();
    measureUnit.adoptInstead(MeasureUnit::createTerabyte(status));
    measureUnitValue = MeasureUnit::getTerabyte();
    measureUnit.adoptInstead(MeasureUnit::createCentury(status));
    measureUnitValue = MeasureUnit::getCentury();
    measureUnit.adoptInstead(MeasureUnit::createDay(status));
    measureUnitValue = MeasureUnit::getDay();
    measureUnit.adoptInstead(MeasureUnit::createDayPerson(status));
    measureUnitValue = MeasureUnit::getDayPerson();
    measureUnit.adoptInstead(MeasureUnit::createDecade(status));
    measureUnitValue = MeasureUnit::getDecade();
    measureUnit.adoptInstead(MeasureUnit::createHour(status));
    measureUnitValue = MeasureUnit::getHour();
    measureUnit.adoptInstead(MeasureUnit::createMicrosecond(status));
    measureUnitValue = MeasureUnit::getMicrosecond();
    measureUnit.adoptInstead(MeasureUnit::createMillisecond(status));
    measureUnitValue = MeasureUnit::getMillisecond();
    measureUnit.adoptInstead(MeasureUnit::createMinute(status));
    measureUnitValue = MeasureUnit::getMinute();
    measureUnit.adoptInstead(MeasureUnit::createMonth(status));
    measureUnitValue = MeasureUnit::getMonth();
    measureUnit.adoptInstead(MeasureUnit::createMonthPerson(status));
    measureUnitValue = MeasureUnit::getMonthPerson();
    measureUnit.adoptInstead(MeasureUnit::createNanosecond(status));
    measureUnitValue = MeasureUnit::getNanosecond();
    measureUnit.adoptInstead(MeasureUnit::createQuarter(status));
    measureUnitValue = MeasureUnit::getQuarter();
    measureUnit.adoptInstead(MeasureUnit::createSecond(status));
    measureUnitValue = MeasureUnit::getSecond();
    measureUnit.adoptInstead(MeasureUnit::createWeek(status));
    measureUnitValue = MeasureUnit::getWeek();
    measureUnit.adoptInstead(MeasureUnit::createWeekPerson(status));
    measureUnitValue = MeasureUnit::getWeekPerson();
    measureUnit.adoptInstead(MeasureUnit::createYear(status));
    measureUnitValue = MeasureUnit::getYear();
    measureUnit.adoptInstead(MeasureUnit::createYearPerson(status));
    measureUnitValue = MeasureUnit::getYearPerson();
    measureUnit.adoptInstead(MeasureUnit::createAmpere(status));
    measureUnitValue = MeasureUnit::getAmpere();
    measureUnit.adoptInstead(MeasureUnit::createMilliampere(status));
    measureUnitValue = MeasureUnit::getMilliampere();
    measureUnit.adoptInstead(MeasureUnit::createOhm(status));
    measureUnitValue = MeasureUnit::getOhm();
    measureUnit.adoptInstead(MeasureUnit::createVolt(status));
    measureUnitValue = MeasureUnit::getVolt();
    measureUnit.adoptInstead(MeasureUnit::createBritishThermalUnit(status));
    measureUnitValue = MeasureUnit::getBritishThermalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCalorie(status));
    measureUnitValue = MeasureUnit::getCalorie();
    measureUnit.adoptInstead(MeasureUnit::createElectronvolt(status));
    measureUnitValue = MeasureUnit::getElectronvolt();
    measureUnit.adoptInstead(MeasureUnit::createFoodcalorie(status));
    measureUnitValue = MeasureUnit::getFoodcalorie();
    measureUnit.adoptInstead(MeasureUnit::createJoule(status));
    measureUnitValue = MeasureUnit::getJoule();
    measureUnit.adoptInstead(MeasureUnit::createKilocalorie(status));
    measureUnitValue = MeasureUnit::getKilocalorie();
    measureUnit.adoptInstead(MeasureUnit::createKilojoule(status));
    measureUnitValue = MeasureUnit::getKilojoule();
    measureUnit.adoptInstead(MeasureUnit::createKilowattHour(status));
    measureUnitValue = MeasureUnit::getKilowattHour();
    measureUnit.adoptInstead(MeasureUnit::createThermUs(status));
    measureUnitValue = MeasureUnit::getThermUs();
    measureUnit.adoptInstead(MeasureUnit::createKilowattHourPer100Kilometer(status));
    measureUnitValue = MeasureUnit::getKilowattHourPer100Kilometer();
    measureUnit.adoptInstead(MeasureUnit::createNewton(status));
    measureUnitValue = MeasureUnit::getNewton();
    measureUnit.adoptInstead(MeasureUnit::createPoundForce(status));
    measureUnitValue = MeasureUnit::getPoundForce();
    measureUnit.adoptInstead(MeasureUnit::createGigahertz(status));
    measureUnitValue = MeasureUnit::getGigahertz();
    measureUnit.adoptInstead(MeasureUnit::createHertz(status));
    measureUnitValue = MeasureUnit::getHertz();
    measureUnit.adoptInstead(MeasureUnit::createKilohertz(status));
    measureUnitValue = MeasureUnit::getKilohertz();
    measureUnit.adoptInstead(MeasureUnit::createMegahertz(status));
    measureUnitValue = MeasureUnit::getMegahertz();
    measureUnit.adoptInstead(MeasureUnit::createDot(status));
    measureUnitValue = MeasureUnit::getDot();
    measureUnit.adoptInstead(MeasureUnit::createDotPerCentimeter(status));
    measureUnitValue = MeasureUnit::getDotPerCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDotPerInch(status));
    measureUnitValue = MeasureUnit::getDotPerInch();
    measureUnit.adoptInstead(MeasureUnit::createEm(status));
    measureUnitValue = MeasureUnit::getEm();
    measureUnit.adoptInstead(MeasureUnit::createMegapixel(status));
    measureUnitValue = MeasureUnit::getMegapixel();
    measureUnit.adoptInstead(MeasureUnit::createPixel(status));
    measureUnitValue = MeasureUnit::getPixel();
    measureUnit.adoptInstead(MeasureUnit::createPixelPerCentimeter(status));
    measureUnitValue = MeasureUnit::getPixelPerCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createPixelPerInch(status));
    measureUnitValue = MeasureUnit::getPixelPerInch();
    measureUnit.adoptInstead(MeasureUnit::createAstronomicalUnit(status));
    measureUnitValue = MeasureUnit::getAstronomicalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCentimeter(status));
    measureUnitValue = MeasureUnit::getCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDecimeter(status));
    measureUnitValue = MeasureUnit::getDecimeter();
    measureUnit.adoptInstead(MeasureUnit::createEarthRadius(status));
    measureUnitValue = MeasureUnit::getEarthRadius();
    measureUnit.adoptInstead(MeasureUnit::createFathom(status));
    measureUnitValue = MeasureUnit::getFathom();
    measureUnit.adoptInstead(MeasureUnit::createFoot(status));
    measureUnitValue = MeasureUnit::getFoot();
    measureUnit.adoptInstead(MeasureUnit::createFurlong(status));
    measureUnitValue = MeasureUnit::getFurlong();
    measureUnit.adoptInstead(MeasureUnit::createInch(status));
    measureUnitValue = MeasureUnit::getInch();
    measureUnit.adoptInstead(MeasureUnit::createKilometer(status));
    measureUnitValue = MeasureUnit::getKilometer();
    measureUnit.adoptInstead(MeasureUnit::createLightYear(status));
    measureUnitValue = MeasureUnit::getLightYear();
    measureUnit.adoptInstead(MeasureUnit::createMeter(status));
    measureUnitValue = MeasureUnit::getMeter();
    measureUnit.adoptInstead(MeasureUnit::createMicrometer(status));
    measureUnitValue = MeasureUnit::getMicrometer();
    measureUnit.adoptInstead(MeasureUnit::createMile(status));
    measureUnitValue = MeasureUnit::getMile();
    measureUnit.adoptInstead(MeasureUnit::createMileScandinavian(status));
    measureUnitValue = MeasureUnit::getMileScandinavian();
    measureUnit.adoptInstead(MeasureUnit::createMillimeter(status));
    measureUnitValue = MeasureUnit::getMillimeter();
    measureUnit.adoptInstead(MeasureUnit::createNanometer(status));
    measureUnitValue = MeasureUnit::getNanometer();
    measureUnit.adoptInstead(MeasureUnit::createNauticalMile(status));
    measureUnitValue = MeasureUnit::getNauticalMile();
    measureUnit.adoptInstead(MeasureUnit::createParsec(status));
    measureUnitValue = MeasureUnit::getParsec();
    measureUnit.adoptInstead(MeasureUnit::createPicometer(status));
    measureUnitValue = MeasureUnit::getPicometer();
    measureUnit.adoptInstead(MeasureUnit::createPoint(status));
    measureUnitValue = MeasureUnit::getPoint();
    measureUnit.adoptInstead(MeasureUnit::createSolarRadius(status));
    measureUnitValue = MeasureUnit::getSolarRadius();
    measureUnit.adoptInstead(MeasureUnit::createYard(status));
    measureUnitValue = MeasureUnit::getYard();
    measureUnit.adoptInstead(MeasureUnit::createCandela(status));
    measureUnitValue = MeasureUnit::getCandela();
    measureUnit.adoptInstead(MeasureUnit::createLumen(status));
    measureUnitValue = MeasureUnit::getLumen();
    measureUnit.adoptInstead(MeasureUnit::createLux(status));
    measureUnitValue = MeasureUnit::getLux();
    measureUnit.adoptInstead(MeasureUnit::createSolarLuminosity(status));
    measureUnitValue = MeasureUnit::getSolarLuminosity();
    measureUnit.adoptInstead(MeasureUnit::createCarat(status));
    measureUnitValue = MeasureUnit::getCarat();
    measureUnit.adoptInstead(MeasureUnit::createDalton(status));
    measureUnitValue = MeasureUnit::getDalton();
    measureUnit.adoptInstead(MeasureUnit::createEarthMass(status));
    measureUnitValue = MeasureUnit::getEarthMass();
    measureUnit.adoptInstead(MeasureUnit::createGrain(status));
    measureUnitValue = MeasureUnit::getGrain();
    measureUnit.adoptInstead(MeasureUnit::createGram(status));
    measureUnitValue = MeasureUnit::getGram();
    measureUnit.adoptInstead(MeasureUnit::createKilogram(status));
    measureUnitValue = MeasureUnit::getKilogram();
    measureUnit.adoptInstead(MeasureUnit::createMicrogram(status));
    measureUnitValue = MeasureUnit::getMicrogram();
    measureUnit.adoptInstead(MeasureUnit::createMilligram(status));
    measureUnitValue = MeasureUnit::getMilligram();
    measureUnit.adoptInstead(MeasureUnit::createOunce(status));
    measureUnitValue = MeasureUnit::getOunce();
    measureUnit.adoptInstead(MeasureUnit::createOunceTroy(status));
    measureUnitValue = MeasureUnit::getOunceTroy();
    measureUnit.adoptInstead(MeasureUnit::createPound(status));
    measureUnitValue = MeasureUnit::getPound();
    measureUnit.adoptInstead(MeasureUnit::createSolarMass(status));
    measureUnitValue = MeasureUnit::getSolarMass();
    measureUnit.adoptInstead(MeasureUnit::createStone(status));
    measureUnitValue = MeasureUnit::getStone();
    measureUnit.adoptInstead(MeasureUnit::createTon(status));
    measureUnitValue = MeasureUnit::getTon();
    measureUnit.adoptInstead(MeasureUnit::createTonne(status));
    measureUnitValue = MeasureUnit::getTonne();
    measureUnit.adoptInstead(MeasureUnit::createGigawatt(status));
    measureUnitValue = MeasureUnit::getGigawatt();
    measureUnit.adoptInstead(MeasureUnit::createHorsepower(status));
    measureUnitValue = MeasureUnit::getHorsepower();
    measureUnit.adoptInstead(MeasureUnit::createKilowatt(status));
    measureUnitValue = MeasureUnit::getKilowatt();
    measureUnit.adoptInstead(MeasureUnit::createMegawatt(status));
    measureUnitValue = MeasureUnit::getMegawatt();
    measureUnit.adoptInstead(MeasureUnit::createMilliwatt(status));
    measureUnitValue = MeasureUnit::getMilliwatt();
    measureUnit.adoptInstead(MeasureUnit::createWatt(status));
    measureUnitValue = MeasureUnit::getWatt();
    measureUnit.adoptInstead(MeasureUnit::createAtmosphere(status));
    measureUnitValue = MeasureUnit::getAtmosphere();
    measureUnit.adoptInstead(MeasureUnit::createBar(status));
    measureUnitValue = MeasureUnit::getBar();
    measureUnit.adoptInstead(MeasureUnit::createHectopascal(status));
    measureUnitValue = MeasureUnit::getHectopascal();
    measureUnit.adoptInstead(MeasureUnit::createInchHg(status));
    measureUnitValue = MeasureUnit::getInchHg();
    measureUnit.adoptInstead(MeasureUnit::createKilopascal(status));
    measureUnitValue = MeasureUnit::getKilopascal();
    measureUnit.adoptInstead(MeasureUnit::createMegapascal(status));
    measureUnitValue = MeasureUnit::getMegapascal();
    measureUnit.adoptInstead(MeasureUnit::createMillibar(status));
    measureUnitValue = MeasureUnit::getMillibar();
    measureUnit.adoptInstead(MeasureUnit::createMillimeterOfMercury(status));
    measureUnitValue = MeasureUnit::getMillimeterOfMercury();
    measureUnit.adoptInstead(MeasureUnit::createPascal(status));
    measureUnitValue = MeasureUnit::getPascal();
    measureUnit.adoptInstead(MeasureUnit::createPoundPerSquareInch(status));
    measureUnitValue = MeasureUnit::getPoundPerSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createBeaufort(status));
    measureUnitValue = MeasureUnit::getBeaufort();
    measureUnit.adoptInstead(MeasureUnit::createKilometerPerHour(status));
    measureUnitValue = MeasureUnit::getKilometerPerHour();
    measureUnit.adoptInstead(MeasureUnit::createKnot(status));
    measureUnitValue = MeasureUnit::getKnot();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnitValue = MeasureUnit::getMeterPerSecond();
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnitValue = MeasureUnit::getMilePerHour();
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnitValue = MeasureUnit::getCelsius();
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnitValue = MeasureUnit::getFahrenheit();
    measureUnit.adoptInstead(MeasureUnit::createGenericTemperature(status));
    measureUnitValue = MeasureUnit::getGenericTemperature();
    measureUnit.adoptInstead(MeasureUnit::createKelvin(status));
    measureUnitValue = MeasureUnit::getKelvin();
    measureUnit.adoptInstead(MeasureUnit::createNewtonMeter(status));
    measureUnitValue = MeasureUnit::getNewtonMeter();
    measureUnit.adoptInstead(MeasureUnit::createPoundFoot(status));
    measureUnitValue = MeasureUnit::getPoundFoot();
    measureUnit.adoptInstead(MeasureUnit::createAcreFoot(status));
    measureUnitValue = MeasureUnit::getAcreFoot();
    measureUnit.adoptInstead(MeasureUnit::createBarrel(status));
    measureUnitValue = MeasureUnit::getBarrel();
    measureUnit.adoptInstead(MeasureUnit::createBushel(status));
    measureUnitValue = MeasureUnit::getBushel();
    measureUnit.adoptInstead(MeasureUnit::createCentiliter(status));
    measureUnitValue = MeasureUnit::getCentiliter();
    measureUnit.adoptInstead(MeasureUnit::createCubicCentimeter(status));
    measureUnitValue = MeasureUnit::getCubicCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicFoot(status));
    measureUnitValue = MeasureUnit::getCubicFoot();
    measureUnit.adoptInstead(MeasureUnit::createCubicInch(status));
    measureUnitValue = MeasureUnit::getCubicInch();
    measureUnit.adoptInstead(MeasureUnit::createCubicKilometer(status));
    measureUnitValue = MeasureUnit::getCubicKilometer();
    measureUnit.adoptInstead(MeasureUnit::createCubicMeter(status));
    measureUnitValue = MeasureUnit::getCubicMeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicMile(status));
    measureUnitValue = MeasureUnit::getCubicMile();
    measureUnit.adoptInstead(MeasureUnit::createCubicYard(status));
    measureUnitValue = MeasureUnit::getCubicYard();
    measureUnit.adoptInstead(MeasureUnit::createCup(status));
    measureUnitValue = MeasureUnit::getCup();
    measureUnit.adoptInstead(MeasureUnit::createCupMetric(status));
    measureUnitValue = MeasureUnit::getCupMetric();
    measureUnit.adoptInstead(MeasureUnit::createDeciliter(status));
    measureUnitValue = MeasureUnit::getDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createDessertSpoon(status));
    measureUnitValue = MeasureUnit::getDessertSpoon();
    measureUnit.adoptInstead(MeasureUnit::createDessertSpoonImperial(status));
    measureUnitValue = MeasureUnit::getDessertSpoonImperial();
    measureUnit.adoptInstead(MeasureUnit::createDram(status));
    measureUnitValue = MeasureUnit::getDram();
    measureUnit.adoptInstead(MeasureUnit::createDrop(status));
    measureUnitValue = MeasureUnit::getDrop();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunce(status));
    measureUnitValue = MeasureUnit::getFluidOunce();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunceImperial(status));
    measureUnitValue = MeasureUnit::getFluidOunceImperial();
    measureUnit.adoptInstead(MeasureUnit::createGallon(status));
    measureUnitValue = MeasureUnit::getGallon();
    measureUnit.adoptInstead(MeasureUnit::createGallonImperial(status));
    measureUnitValue = MeasureUnit::getGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createHectoliter(status));
    measureUnitValue = MeasureUnit::getHectoliter();
    measureUnit.adoptInstead(MeasureUnit::createJigger(status));
    measureUnitValue = MeasureUnit::getJigger();
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    measureUnitValue = MeasureUnit::getLiter();
    measureUnit.adoptInstead(MeasureUnit::createMegaliter(status));
    measureUnitValue = MeasureUnit::getMegaliter();
    measureUnit.adoptInstead(MeasureUnit::createMilliliter(status));
    measureUnitValue = MeasureUnit::getMilliliter();
    measureUnit.adoptInstead(MeasureUnit::createPinch(status));
    measureUnitValue = MeasureUnit::getPinch();
    measureUnit.adoptInstead(MeasureUnit::createPint(status));
    measureUnitValue = MeasureUnit::getPint();
    measureUnit.adoptInstead(MeasureUnit::createPintMetric(status));
    measureUnitValue = MeasureUnit::getPintMetric();
    measureUnit.adoptInstead(MeasureUnit::createQuart(status));
    measureUnitValue = MeasureUnit::getQuart();
    measureUnit.adoptInstead(MeasureUnit::createQuartImperial(status));
    measureUnitValue = MeasureUnit::getQuartImperial();
    measureUnit.adoptInstead(MeasureUnit::createTablespoon(status));
    measureUnitValue = MeasureUnit::getTablespoon();
    measureUnit.adoptInstead(MeasureUnit::createTeaspoon(status));
    measureUnitValue = MeasureUnit::getTeaspoon();
    assertSuccess("", status);
}

void MeasureFormatTest::TestCompatible74() {
    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<MeasureUnit> measureUnit;
    MeasureUnit measureUnitValue;
    measureUnit.adoptInstead(MeasureUnit::createGForce(status));
    measureUnitValue = MeasureUnit::getGForce();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecondSquared(status));
    measureUnitValue = MeasureUnit::getMeterPerSecondSquared();
    measureUnit.adoptInstead(MeasureUnit::createArcMinute(status));
    measureUnitValue = MeasureUnit::getArcMinute();
    measureUnit.adoptInstead(MeasureUnit::createArcSecond(status));
    measureUnitValue = MeasureUnit::getArcSecond();
    measureUnit.adoptInstead(MeasureUnit::createDegree(status));
    measureUnitValue = MeasureUnit::getDegree();
    measureUnit.adoptInstead(MeasureUnit::createRadian(status));
    measureUnitValue = MeasureUnit::getRadian();
    measureUnit.adoptInstead(MeasureUnit::createRevolutionAngle(status));
    measureUnitValue = MeasureUnit::getRevolutionAngle();
    measureUnit.adoptInstead(MeasureUnit::createAcre(status));
    measureUnitValue = MeasureUnit::getAcre();
    measureUnit.adoptInstead(MeasureUnit::createDunam(status));
    measureUnitValue = MeasureUnit::getDunam();
    measureUnit.adoptInstead(MeasureUnit::createHectare(status));
    measureUnitValue = MeasureUnit::getHectare();
    measureUnit.adoptInstead(MeasureUnit::createSquareCentimeter(status));
    measureUnitValue = MeasureUnit::getSquareCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareFoot(status));
    measureUnitValue = MeasureUnit::getSquareFoot();
    measureUnit.adoptInstead(MeasureUnit::createSquareInch(status));
    measureUnitValue = MeasureUnit::getSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createSquareKilometer(status));
    measureUnitValue = MeasureUnit::getSquareKilometer();
    measureUnit.adoptInstead(MeasureUnit::createSquareMeter(status));
    measureUnitValue = MeasureUnit::getSquareMeter();
    measureUnit.adoptInstead(MeasureUnit::createSquareMile(status));
    measureUnitValue = MeasureUnit::getSquareMile();
    measureUnit.adoptInstead(MeasureUnit::createSquareYard(status));
    measureUnitValue = MeasureUnit::getSquareYard();
    measureUnit.adoptInstead(MeasureUnit::createItem(status));
    measureUnitValue = MeasureUnit::getItem();
    measureUnit.adoptInstead(MeasureUnit::createKarat(status));
    measureUnitValue = MeasureUnit::getKarat();
    measureUnit.adoptInstead(MeasureUnit::createMilligramOfglucosePerDeciliter(status));
    measureUnitValue = MeasureUnit::getMilligramOfglucosePerDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createMilligramPerDeciliter(status));
    measureUnitValue = MeasureUnit::getMilligramPerDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createMillimolePerLiter(status));
    measureUnitValue = MeasureUnit::getMillimolePerLiter();
    measureUnit.adoptInstead(MeasureUnit::createMole(status));
    measureUnitValue = MeasureUnit::getMole();
    measureUnit.adoptInstead(MeasureUnit::createPercent(status));
    measureUnitValue = MeasureUnit::getPercent();
    measureUnit.adoptInstead(MeasureUnit::createPermille(status));
    measureUnitValue = MeasureUnit::getPermille();
    measureUnit.adoptInstead(MeasureUnit::createPartPerMillion(status));
    measureUnitValue = MeasureUnit::getPartPerMillion();
    measureUnit.adoptInstead(MeasureUnit::createPermyriad(status));
    measureUnitValue = MeasureUnit::getPermyriad();
    measureUnit.adoptInstead(MeasureUnit::createLiterPer100Kilometers(status));
    measureUnitValue = MeasureUnit::getLiterPer100Kilometers();
    measureUnit.adoptInstead(MeasureUnit::createLiterPerKilometer(status));
    measureUnitValue = MeasureUnit::getLiterPerKilometer();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallon(status));
    measureUnitValue = MeasureUnit::getMilePerGallon();
    measureUnit.adoptInstead(MeasureUnit::createMilePerGallonImperial(status));
    measureUnitValue = MeasureUnit::getMilePerGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createBit(status));
    measureUnitValue = MeasureUnit::getBit();
    measureUnit.adoptInstead(MeasureUnit::createByte(status));
    measureUnitValue = MeasureUnit::getByte();
    measureUnit.adoptInstead(MeasureUnit::createGigabit(status));
    measureUnitValue = MeasureUnit::getGigabit();
    measureUnit.adoptInstead(MeasureUnit::createGigabyte(status));
    measureUnitValue = MeasureUnit::getGigabyte();
    measureUnit.adoptInstead(MeasureUnit::createKilobit(status));
    measureUnitValue = MeasureUnit::getKilobit();
    measureUnit.adoptInstead(MeasureUnit::createKilobyte(status));
    measureUnitValue = MeasureUnit::getKilobyte();
    measureUnit.adoptInstead(MeasureUnit::createMegabit(status));
    measureUnitValue = MeasureUnit::getMegabit();
    measureUnit.adoptInstead(MeasureUnit::createMegabyte(status));
    measureUnitValue = MeasureUnit::getMegabyte();
    measureUnit.adoptInstead(MeasureUnit::createPetabyte(status));
    measureUnitValue = MeasureUnit::getPetabyte();
    measureUnit.adoptInstead(MeasureUnit::createTerabit(status));
    measureUnitValue = MeasureUnit::getTerabit();
    measureUnit.adoptInstead(MeasureUnit::createTerabyte(status));
    measureUnitValue = MeasureUnit::getTerabyte();
    measureUnit.adoptInstead(MeasureUnit::createCentury(status));
    measureUnitValue = MeasureUnit::getCentury();
    measureUnit.adoptInstead(MeasureUnit::createDay(status));
    measureUnitValue = MeasureUnit::getDay();
    measureUnit.adoptInstead(MeasureUnit::createDayPerson(status));
    measureUnitValue = MeasureUnit::getDayPerson();
    measureUnit.adoptInstead(MeasureUnit::createDecade(status));
    measureUnitValue = MeasureUnit::getDecade();
    measureUnit.adoptInstead(MeasureUnit::createHour(status));
    measureUnitValue = MeasureUnit::getHour();
    measureUnit.adoptInstead(MeasureUnit::createMicrosecond(status));
    measureUnitValue = MeasureUnit::getMicrosecond();
    measureUnit.adoptInstead(MeasureUnit::createMillisecond(status));
    measureUnitValue = MeasureUnit::getMillisecond();
    measureUnit.adoptInstead(MeasureUnit::createMinute(status));
    measureUnitValue = MeasureUnit::getMinute();
    measureUnit.adoptInstead(MeasureUnit::createMonth(status));
    measureUnitValue = MeasureUnit::getMonth();
    measureUnit.adoptInstead(MeasureUnit::createMonthPerson(status));
    measureUnitValue = MeasureUnit::getMonthPerson();
    measureUnit.adoptInstead(MeasureUnit::createNanosecond(status));
    measureUnitValue = MeasureUnit::getNanosecond();
    measureUnit.adoptInstead(MeasureUnit::createQuarter(status));
    measureUnitValue = MeasureUnit::getQuarter();
    measureUnit.adoptInstead(MeasureUnit::createSecond(status));
    measureUnitValue = MeasureUnit::getSecond();
    measureUnit.adoptInstead(MeasureUnit::createWeek(status));
    measureUnitValue = MeasureUnit::getWeek();
    measureUnit.adoptInstead(MeasureUnit::createWeekPerson(status));
    measureUnitValue = MeasureUnit::getWeekPerson();
    measureUnit.adoptInstead(MeasureUnit::createYear(status));
    measureUnitValue = MeasureUnit::getYear();
    measureUnit.adoptInstead(MeasureUnit::createYearPerson(status));
    measureUnitValue = MeasureUnit::getYearPerson();
    measureUnit.adoptInstead(MeasureUnit::createAmpere(status));
    measureUnitValue = MeasureUnit::getAmpere();
    measureUnit.adoptInstead(MeasureUnit::createMilliampere(status));
    measureUnitValue = MeasureUnit::getMilliampere();
    measureUnit.adoptInstead(MeasureUnit::createOhm(status));
    measureUnitValue = MeasureUnit::getOhm();
    measureUnit.adoptInstead(MeasureUnit::createVolt(status));
    measureUnitValue = MeasureUnit::getVolt();
    measureUnit.adoptInstead(MeasureUnit::createBritishThermalUnit(status));
    measureUnitValue = MeasureUnit::getBritishThermalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCalorie(status));
    measureUnitValue = MeasureUnit::getCalorie();
    measureUnit.adoptInstead(MeasureUnit::createElectronvolt(status));
    measureUnitValue = MeasureUnit::getElectronvolt();
    measureUnit.adoptInstead(MeasureUnit::createFoodcalorie(status));
    measureUnitValue = MeasureUnit::getFoodcalorie();
    measureUnit.adoptInstead(MeasureUnit::createJoule(status));
    measureUnitValue = MeasureUnit::getJoule();
    measureUnit.adoptInstead(MeasureUnit::createKilocalorie(status));
    measureUnitValue = MeasureUnit::getKilocalorie();
    measureUnit.adoptInstead(MeasureUnit::createKilojoule(status));
    measureUnitValue = MeasureUnit::getKilojoule();
    measureUnit.adoptInstead(MeasureUnit::createKilowattHour(status));
    measureUnitValue = MeasureUnit::getKilowattHour();
    measureUnit.adoptInstead(MeasureUnit::createThermUs(status));
    measureUnitValue = MeasureUnit::getThermUs();
    measureUnit.adoptInstead(MeasureUnit::createKilowattHourPer100Kilometer(status));
    measureUnitValue = MeasureUnit::getKilowattHourPer100Kilometer();
    measureUnit.adoptInstead(MeasureUnit::createNewton(status));
    measureUnitValue = MeasureUnit::getNewton();
    measureUnit.adoptInstead(MeasureUnit::createPoundForce(status));
    measureUnitValue = MeasureUnit::getPoundForce();
    measureUnit.adoptInstead(MeasureUnit::createGigahertz(status));
    measureUnitValue = MeasureUnit::getGigahertz();
    measureUnit.adoptInstead(MeasureUnit::createHertz(status));
    measureUnitValue = MeasureUnit::getHertz();
    measureUnit.adoptInstead(MeasureUnit::createKilohertz(status));
    measureUnitValue = MeasureUnit::getKilohertz();
    measureUnit.adoptInstead(MeasureUnit::createMegahertz(status));
    measureUnitValue = MeasureUnit::getMegahertz();
    measureUnit.adoptInstead(MeasureUnit::createDot(status));
    measureUnitValue = MeasureUnit::getDot();
    measureUnit.adoptInstead(MeasureUnit::createDotPerCentimeter(status));
    measureUnitValue = MeasureUnit::getDotPerCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDotPerInch(status));
    measureUnitValue = MeasureUnit::getDotPerInch();
    measureUnit.adoptInstead(MeasureUnit::createEm(status));
    measureUnitValue = MeasureUnit::getEm();
    measureUnit.adoptInstead(MeasureUnit::createMegapixel(status));
    measureUnitValue = MeasureUnit::getMegapixel();
    measureUnit.adoptInstead(MeasureUnit::createPixel(status));
    measureUnitValue = MeasureUnit::getPixel();
    measureUnit.adoptInstead(MeasureUnit::createPixelPerCentimeter(status));
    measureUnitValue = MeasureUnit::getPixelPerCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createPixelPerInch(status));
    measureUnitValue = MeasureUnit::getPixelPerInch();
    measureUnit.adoptInstead(MeasureUnit::createAstronomicalUnit(status));
    measureUnitValue = MeasureUnit::getAstronomicalUnit();
    measureUnit.adoptInstead(MeasureUnit::createCentimeter(status));
    measureUnitValue = MeasureUnit::getCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createDecimeter(status));
    measureUnitValue = MeasureUnit::getDecimeter();
    measureUnit.adoptInstead(MeasureUnit::createEarthRadius(status));
    measureUnitValue = MeasureUnit::getEarthRadius();
    measureUnit.adoptInstead(MeasureUnit::createFathom(status));
    measureUnitValue = MeasureUnit::getFathom();
    measureUnit.adoptInstead(MeasureUnit::createFoot(status));
    measureUnitValue = MeasureUnit::getFoot();
    measureUnit.adoptInstead(MeasureUnit::createFurlong(status));
    measureUnitValue = MeasureUnit::getFurlong();
    measureUnit.adoptInstead(MeasureUnit::createInch(status));
    measureUnitValue = MeasureUnit::getInch();
    measureUnit.adoptInstead(MeasureUnit::createKilometer(status));
    measureUnitValue = MeasureUnit::getKilometer();
    measureUnit.adoptInstead(MeasureUnit::createLightYear(status));
    measureUnitValue = MeasureUnit::getLightYear();
    measureUnit.adoptInstead(MeasureUnit::createMeter(status));
    measureUnitValue = MeasureUnit::getMeter();
    measureUnit.adoptInstead(MeasureUnit::createMicrometer(status));
    measureUnitValue = MeasureUnit::getMicrometer();
    measureUnit.adoptInstead(MeasureUnit::createMile(status));
    measureUnitValue = MeasureUnit::getMile();
    measureUnit.adoptInstead(MeasureUnit::createMileScandinavian(status));
    measureUnitValue = MeasureUnit::getMileScandinavian();
    measureUnit.adoptInstead(MeasureUnit::createMillimeter(status));
    measureUnitValue = MeasureUnit::getMillimeter();
    measureUnit.adoptInstead(MeasureUnit::createNanometer(status));
    measureUnitValue = MeasureUnit::getNanometer();
    measureUnit.adoptInstead(MeasureUnit::createNauticalMile(status));
    measureUnitValue = MeasureUnit::getNauticalMile();
    measureUnit.adoptInstead(MeasureUnit::createParsec(status));
    measureUnitValue = MeasureUnit::getParsec();
    measureUnit.adoptInstead(MeasureUnit::createPicometer(status));
    measureUnitValue = MeasureUnit::getPicometer();
    measureUnit.adoptInstead(MeasureUnit::createPoint(status));
    measureUnitValue = MeasureUnit::getPoint();
    measureUnit.adoptInstead(MeasureUnit::createSolarRadius(status));
    measureUnitValue = MeasureUnit::getSolarRadius();
    measureUnit.adoptInstead(MeasureUnit::createYard(status));
    measureUnitValue = MeasureUnit::getYard();
    measureUnit.adoptInstead(MeasureUnit::createCandela(status));
    measureUnitValue = MeasureUnit::getCandela();
    measureUnit.adoptInstead(MeasureUnit::createLumen(status));
    measureUnitValue = MeasureUnit::getLumen();
    measureUnit.adoptInstead(MeasureUnit::createLux(status));
    measureUnitValue = MeasureUnit::getLux();
    measureUnit.adoptInstead(MeasureUnit::createSolarLuminosity(status));
    measureUnitValue = MeasureUnit::getSolarLuminosity();
    measureUnit.adoptInstead(MeasureUnit::createCarat(status));
    measureUnitValue = MeasureUnit::getCarat();
    measureUnit.adoptInstead(MeasureUnit::createDalton(status));
    measureUnitValue = MeasureUnit::getDalton();
    measureUnit.adoptInstead(MeasureUnit::createEarthMass(status));
    measureUnitValue = MeasureUnit::getEarthMass();
    measureUnit.adoptInstead(MeasureUnit::createGrain(status));
    measureUnitValue = MeasureUnit::getGrain();
    measureUnit.adoptInstead(MeasureUnit::createGram(status));
    measureUnitValue = MeasureUnit::getGram();
    measureUnit.adoptInstead(MeasureUnit::createKilogram(status));
    measureUnitValue = MeasureUnit::getKilogram();
    measureUnit.adoptInstead(MeasureUnit::createMicrogram(status));
    measureUnitValue = MeasureUnit::getMicrogram();
    measureUnit.adoptInstead(MeasureUnit::createMilligram(status));
    measureUnitValue = MeasureUnit::getMilligram();
    measureUnit.adoptInstead(MeasureUnit::createOunce(status));
    measureUnitValue = MeasureUnit::getOunce();
    measureUnit.adoptInstead(MeasureUnit::createOunceTroy(status));
    measureUnitValue = MeasureUnit::getOunceTroy();
    measureUnit.adoptInstead(MeasureUnit::createPound(status));
    measureUnitValue = MeasureUnit::getPound();
    measureUnit.adoptInstead(MeasureUnit::createSolarMass(status));
    measureUnitValue = MeasureUnit::getSolarMass();
    measureUnit.adoptInstead(MeasureUnit::createStone(status));
    measureUnitValue = MeasureUnit::getStone();
    measureUnit.adoptInstead(MeasureUnit::createTon(status));
    measureUnitValue = MeasureUnit::getTon();
    measureUnit.adoptInstead(MeasureUnit::createTonne(status));
    measureUnitValue = MeasureUnit::getTonne();
    measureUnit.adoptInstead(MeasureUnit::createGigawatt(status));
    measureUnitValue = MeasureUnit::getGigawatt();
    measureUnit.adoptInstead(MeasureUnit::createHorsepower(status));
    measureUnitValue = MeasureUnit::getHorsepower();
    measureUnit.adoptInstead(MeasureUnit::createKilowatt(status));
    measureUnitValue = MeasureUnit::getKilowatt();
    measureUnit.adoptInstead(MeasureUnit::createMegawatt(status));
    measureUnitValue = MeasureUnit::getMegawatt();
    measureUnit.adoptInstead(MeasureUnit::createMilliwatt(status));
    measureUnitValue = MeasureUnit::getMilliwatt();
    measureUnit.adoptInstead(MeasureUnit::createWatt(status));
    measureUnitValue = MeasureUnit::getWatt();
    measureUnit.adoptInstead(MeasureUnit::createAtmosphere(status));
    measureUnitValue = MeasureUnit::getAtmosphere();
    measureUnit.adoptInstead(MeasureUnit::createBar(status));
    measureUnitValue = MeasureUnit::getBar();
    measureUnit.adoptInstead(MeasureUnit::createGasolineEnergyDensity(status));
    measureUnitValue = MeasureUnit::getGasolineEnergyDensity();
    measureUnit.adoptInstead(MeasureUnit::createHectopascal(status));
    measureUnitValue = MeasureUnit::getHectopascal();
    measureUnit.adoptInstead(MeasureUnit::createInchHg(status));
    measureUnitValue = MeasureUnit::getInchHg();
    measureUnit.adoptInstead(MeasureUnit::createKilopascal(status));
    measureUnitValue = MeasureUnit::getKilopascal();
    measureUnit.adoptInstead(MeasureUnit::createMegapascal(status));
    measureUnitValue = MeasureUnit::getMegapascal();
    measureUnit.adoptInstead(MeasureUnit::createMillibar(status));
    measureUnitValue = MeasureUnit::getMillibar();
    measureUnit.adoptInstead(MeasureUnit::createMillimeterOfMercury(status));
    measureUnitValue = MeasureUnit::getMillimeterOfMercury();
    measureUnit.adoptInstead(MeasureUnit::createPascal(status));
    measureUnitValue = MeasureUnit::getPascal();
    measureUnit.adoptInstead(MeasureUnit::createPoundPerSquareInch(status));
    measureUnitValue = MeasureUnit::getPoundPerSquareInch();
    measureUnit.adoptInstead(MeasureUnit::createBeaufort(status));
    measureUnitValue = MeasureUnit::getBeaufort();
    measureUnit.adoptInstead(MeasureUnit::createKilometerPerHour(status));
    measureUnitValue = MeasureUnit::getKilometerPerHour();
    measureUnit.adoptInstead(MeasureUnit::createKnot(status));
    measureUnitValue = MeasureUnit::getKnot();
    measureUnit.adoptInstead(MeasureUnit::createMeterPerSecond(status));
    measureUnitValue = MeasureUnit::getMeterPerSecond();
    measureUnit.adoptInstead(MeasureUnit::createMilePerHour(status));
    measureUnitValue = MeasureUnit::getMilePerHour();
    measureUnit.adoptInstead(MeasureUnit::createCelsius(status));
    measureUnitValue = MeasureUnit::getCelsius();
    measureUnit.adoptInstead(MeasureUnit::createFahrenheit(status));
    measureUnitValue = MeasureUnit::getFahrenheit();
    measureUnit.adoptInstead(MeasureUnit::createGenericTemperature(status));
    measureUnitValue = MeasureUnit::getGenericTemperature();
    measureUnit.adoptInstead(MeasureUnit::createKelvin(status));
    measureUnitValue = MeasureUnit::getKelvin();
    measureUnit.adoptInstead(MeasureUnit::createNewtonMeter(status));
    measureUnitValue = MeasureUnit::getNewtonMeter();
    measureUnit.adoptInstead(MeasureUnit::createPoundFoot(status));
    measureUnitValue = MeasureUnit::getPoundFoot();
    measureUnit.adoptInstead(MeasureUnit::createAcreFoot(status));
    measureUnitValue = MeasureUnit::getAcreFoot();
    measureUnit.adoptInstead(MeasureUnit::createBarrel(status));
    measureUnitValue = MeasureUnit::getBarrel();
    measureUnit.adoptInstead(MeasureUnit::createBushel(status));
    measureUnitValue = MeasureUnit::getBushel();
    measureUnit.adoptInstead(MeasureUnit::createCentiliter(status));
    measureUnitValue = MeasureUnit::getCentiliter();
    measureUnit.adoptInstead(MeasureUnit::createCubicCentimeter(status));
    measureUnitValue = MeasureUnit::getCubicCentimeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicFoot(status));
    measureUnitValue = MeasureUnit::getCubicFoot();
    measureUnit.adoptInstead(MeasureUnit::createCubicInch(status));
    measureUnitValue = MeasureUnit::getCubicInch();
    measureUnit.adoptInstead(MeasureUnit::createCubicKilometer(status));
    measureUnitValue = MeasureUnit::getCubicKilometer();
    measureUnit.adoptInstead(MeasureUnit::createCubicMeter(status));
    measureUnitValue = MeasureUnit::getCubicMeter();
    measureUnit.adoptInstead(MeasureUnit::createCubicMile(status));
    measureUnitValue = MeasureUnit::getCubicMile();
    measureUnit.adoptInstead(MeasureUnit::createCubicYard(status));
    measureUnitValue = MeasureUnit::getCubicYard();
    measureUnit.adoptInstead(MeasureUnit::createCup(status));
    measureUnitValue = MeasureUnit::getCup();
    measureUnit.adoptInstead(MeasureUnit::createCupMetric(status));
    measureUnitValue = MeasureUnit::getCupMetric();
    measureUnit.adoptInstead(MeasureUnit::createDeciliter(status));
    measureUnitValue = MeasureUnit::getDeciliter();
    measureUnit.adoptInstead(MeasureUnit::createDessertSpoon(status));
    measureUnitValue = MeasureUnit::getDessertSpoon();
    measureUnit.adoptInstead(MeasureUnit::createDessertSpoonImperial(status));
    measureUnitValue = MeasureUnit::getDessertSpoonImperial();
    measureUnit.adoptInstead(MeasureUnit::createDram(status));
    measureUnitValue = MeasureUnit::getDram();
    measureUnit.adoptInstead(MeasureUnit::createDrop(status));
    measureUnitValue = MeasureUnit::getDrop();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunce(status));
    measureUnitValue = MeasureUnit::getFluidOunce();
    measureUnit.adoptInstead(MeasureUnit::createFluidOunceImperial(status));
    measureUnitValue = MeasureUnit::getFluidOunceImperial();
    measureUnit.adoptInstead(MeasureUnit::createGallon(status));
    measureUnitValue = MeasureUnit::getGallon();
    measureUnit.adoptInstead(MeasureUnit::createGallonImperial(status));
    measureUnitValue = MeasureUnit::getGallonImperial();
    measureUnit.adoptInstead(MeasureUnit::createHectoliter(status));
    measureUnitValue = MeasureUnit::getHectoliter();
    measureUnit.adoptInstead(MeasureUnit::createJigger(status));
    measureUnitValue = MeasureUnit::getJigger();
    measureUnit.adoptInstead(MeasureUnit::createLiter(status));
    measureUnitValue = MeasureUnit::getLiter();
    measureUnit.adoptInstead(MeasureUnit::createMegaliter(status));
    measureUnitValue = MeasureUnit::getMegaliter();
    measureUnit.adoptInstead(MeasureUnit::createMilliliter(status));
    measureUnitValue = MeasureUnit::getMilliliter();
    measureUnit.adoptInstead(MeasureUnit::createPinch(status));
    measureUnitValue = MeasureUnit::getPinch();
    measureUnit.adoptInstead(MeasureUnit::createPint(status));
    measureUnitValue = MeasureUnit::getPint();
    measureUnit.adoptInstead(MeasureUnit::createPintMetric(status));
    measureUnitValue = MeasureUnit::getPintMetric();
    measureUnit.adoptInstead(MeasureUnit::createQuart(status));
    measureUnitValue = MeasureUnit::getQuart();
    measureUnit.adoptInstead(MeasureUnit::createQuartImperial(status));
    measureUnitValue = MeasureUnit::getQuartImperial();
    measureUnit.adoptInstead(MeasureUnit::createTablespoon(status));
    measureUnitValue = MeasureUnit::getTablespoon();
    measureUnit.adoptInstead(MeasureUnit::createTeaspoon(status));
    measureUnitValue = MeasureUnit::getTeaspoon();
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
    MeasureUnit *ptr4 = ptr1->clone();
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
    MeasureUnit *units = nullptr;
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
    units = nullptr;
    int32_t unitCapacity = 0;
    int32_t unitCountSum = 0;
    for (
            const char* type = types->next(nullptr, status);
            type != nullptr;
            type = types->next(nullptr, status)) {
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
    Measure measureC((double)23, MeasureUnit::createCelsius(status), status);
    Measure measureF((double)70, MeasureUnit::createFahrenheit(status), status);
    Measure feetAndInches[] = {
            Measure((double)70, MeasureUnit::createFoot(status), status),
            Measure((double)5.3, MeasureUnit::createInch(status), status)};
    Measure footAndInch[] = {
            Measure((double)1, MeasureUnit::createFoot(status), status),
            Measure((double)1, MeasureUnit::createInch(status), status)};
    Measure inchAndFeet[] = {
            Measure((double)1, MeasureUnit::createInch(status), status),
            Measure((double)2, MeasureUnit::createFoot(status), status)};
    if (!assertSuccess("Error creating measurements.", status)) {
        return;
    }
    verifyFormat(
            "Celsius",
            fmtFr,
            &measureC,
            1,
            "23\\u202F\\u00B0C");
    verifyFormatWithPrefix(
            "Celsius",
            fmtFr,
            "Prefix: ",
            &measureC,
            1,
            "Prefix: 23\\u202F\\u00B0C");
    verifyFormat(
            "Fahrenheit",
            fmtFr,
            &measureF,
            1,
            "70\\u202F\\u00B0F");
    verifyFormat(
            "Feet and inches",
            fmtFrFull,
            feetAndInches,
            UPRV_LENGTHOF(feetAndInches),
            "70 pieds et 5,3\\u00A0pouces");
    verifyFormatWithPrefix(
            "Feet and inches",
            fmtFrFull,
            "Prefix: ",
            feetAndInches,
            UPRV_LENGTHOF(feetAndInches),
            "Prefix: 70 pieds et 5,3\\u00A0pouces");
    verifyFormat(
            "Foot and inch",
            fmtFrFull,
            footAndInch,
            UPRV_LENGTHOF(footAndInch),
            "1\\u00A0pied et 1\\u00A0pouce");
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
    Measure t_1y[] = {Measure((double)1, MeasureUnit::createYear(status), status)};
    Measure t_5M[] = {Measure((double)5, MeasureUnit::createMonth(status), status)};
    Measure t_4d[] = {Measure((double)4, MeasureUnit::createDay(status), status)};
    Measure t_2h[] = {Measure((double)2, MeasureUnit::createHour(status), status)};
    Measure t_19m[] = {Measure((double)19, MeasureUnit::createMinute(status), status)};
    Measure t_1h_23_5s[] = {
            Measure((double)1.0, MeasureUnit::createHour(status), status),
            Measure((double)23.5, MeasureUnit::createSecond(status), status)
    };
    Measure t_1h_23_5m[] = {
            Measure((double)1.0, MeasureUnit::createHour(status), status),
            Measure((double)23.5, MeasureUnit::createMinute(status), status)
    };
    Measure t_1h_0m_23s[] = {
            Measure(
                    (double)1.0,
                    TimeUnit::createInstance(
                            TimeUnit::UTIMEUNIT_HOUR, status),
                    status),
            Measure(
                    (double)0.0,
                    TimeUnit::createInstance(
                            TimeUnit::UTIMEUNIT_MINUTE, status),
                     status),
            Measure(
                    (double)23.0,
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
            {t_1m_59_9996s, UPRV_LENGTHOF(t_1m_59_9996s), "1 min, 59.9996 sec"},
            {t_19m, UPRV_LENGTHOF(t_19m), "19 min"},
            {t_1h_23_5s, UPRV_LENGTHOF(t_1h_23_5s), "1 hr, 23.5 sec"},
            {t_1h_23_5m, UPRV_LENGTHOF(t_1h_23_5m), "1 hr, 23.5 min"},
            {t_1h_0m_23s, UPRV_LENGTHOF(t_1h_0m_23s), "1 hr, 0 min, 23 sec"},
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
            {t_1m_59_9996s, UPRV_LENGTHOF(t_1m_59_9996s), "1 Minute, 59,9996 Sekunden"},
            {t_19m, UPRV_LENGTHOF(t_19m), "19 Minuten"},
            {t_1h_23_5s, UPRV_LENGTHOF(t_1h_23_5s), "1 Stunde, 23,5 Sekunden"},
            {t_1h_23_5m, UPRV_LENGTHOF(t_1h_23_5m), "1 Stunde, 23,5 Minuten"},
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

    ExpectedResult numericDataBn[] = {
            {t_1m_59_9996s, UPRV_LENGTHOF(t_1m_59_9996s), "\\u09E7:\\u09EB\\u09EF.\\u09EF\\u09EF\\u09EF\\u09EC"},
            {t_19m, UPRV_LENGTHOF(t_19m), "\\u09E7\\u09EF \\u09AE\\u09BF\\u0983"},
            {t_1h_23_5s, UPRV_LENGTHOF(t_1h_23_5s), "\\u09E7:\\u09E6\\u09E6:\\u09E8\\u09E9.\\u09EB"},
            {t_1h_0m_23s, UPRV_LENGTHOF(t_1h_0m_23s), "\\u09E7:\\u09E6\\u09E6:\\u09E8\\u09E9"},
            {t_1h_23_5m, UPRV_LENGTHOF(t_1h_23_5m), "\\u09E7:\\u09E8\\u09E9.\\u09EB"},
            {t_5h_17m, UPRV_LENGTHOF(t_5h_17m), "\\u09EB:\\u09E7\\u09ED"},
            {t_19m_28s, UPRV_LENGTHOF(t_19m_28s), "\\u09E7\\u09EF:\\u09E8\\u09EE"},
            {t_2y_5M_3w_4d, UPRV_LENGTHOF(t_2y_5M_3w_4d), "\\u09E8 \\u09AC\\u099B\\u09B0, \\u09EB \\u09AE\\u09BE\\u09B8, \\u09E9 \\u09B8\\u09AA\\u09CD\\u09A4\\u09BE\\u09B9, \\u09EA \\u09A6\\u09BF\\u09A8"},
            {t_0h_0m_17s, UPRV_LENGTHOF(t_0h_0m_17s), "\\u09E6:\\u09E6\\u09E6:\\u09E7\\u09ED"},
            {t_6h_56_92m, UPRV_LENGTHOF(t_6h_56_92m), "\\u09EC:\\u09EB\\u09EC.\\u09EF\\u09E8"},
            {t_3h_5h, UPRV_LENGTHOF(t_3h_5h), "\\u09E9 \\u0998\\u0983, \\u09EB \\u0998\\u0983"}};

    ExpectedResult numericDataBnLatn[] = {
            {t_1m_59_9996s, UPRV_LENGTHOF(t_1m_59_9996s), "1:59.9996"},
            {t_19m, UPRV_LENGTHOF(t_19m), "19 \\u09AE\\u09BF\\u0983"},
            {t_1h_23_5s, UPRV_LENGTHOF(t_1h_23_5s), "1:00:23.5"},
            {t_1h_0m_23s, UPRV_LENGTHOF(t_1h_0m_23s), "1:00:23"},
            {t_1h_23_5m, UPRV_LENGTHOF(t_1h_23_5m), "1:23.5"},
            {t_5h_17m, UPRV_LENGTHOF(t_5h_17m), "5:17"},
            {t_19m_28s, UPRV_LENGTHOF(t_19m_28s), "19:28"},
            {t_2y_5M_3w_4d, UPRV_LENGTHOF(t_2y_5M_3w_4d), "2 \\u09AC\\u099B\\u09B0, 5 \\u09AE\\u09BE\\u09B8, 3 \\u09B8\\u09AA\\u09CD\\u09A4\\u09BE\\u09B9, 4 \\u09A6\\u09BF\\u09A8"},
            {t_0h_0m_17s, UPRV_LENGTHOF(t_0h_0m_17s), "0:00:17"},
            {t_6h_56_92m, UPRV_LENGTHOF(t_6h_56_92m), "6:56.92"},
            {t_3h_5h, UPRV_LENGTHOF(t_3h_5h), "3 \\u0998\\u0983, 5 \\u0998\\u0983"}};

    ExpectedResult fullDataSpellout[] = {
            {t_1y, UPRV_LENGTHOF(t_1y), "one year"},
            {t_5M, UPRV_LENGTHOF(t_5M), "five months"},
            {t_4d, UPRV_LENGTHOF(t_4d), "four days"},
            {t_2h, UPRV_LENGTHOF(t_2h), "two hours"},
            {t_19m, UPRV_LENGTHOF(t_19m), "nineteen minutes"}};

    ExpectedResult fullDataSpelloutFr[] = {
            {t_1y, UPRV_LENGTHOF(t_1y), "un\\u00A0an"},
            {t_5M, UPRV_LENGTHOF(t_5M), "cinq\\u00A0mois"},
            {t_4d, UPRV_LENGTHOF(t_4d), "quatre\\u00A0jours"},
            {t_2h, UPRV_LENGTHOF(t_2h), "deux\\u00A0heures"},
            {t_19m, UPRV_LENGTHOF(t_19m), "dix-neuf minutes"}};

    Locale en(Locale::getEnglish());
    LocalPointer<NumberFormat> nf(NumberFormat::createInstance(en, status));
    if (U_FAILURE(status)) {
        dataerrln("Error creating number format en object - %s", u_errorName(status));
        return;
    }
    nf->setMaximumFractionDigits(4);
    MeasureFormat mf(en, UMEASFMT_WIDTH_WIDE, nf->clone(), status);
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
        MeasureFormat *mf3 = mf.clone();
        verifyFormat("en WIDE copy", *mf3, fullData, UPRV_LENGTHOF(fullData));
        delete mf3;
    }
    mf = MeasureFormat(en, UMEASFMT_WIDTH_SHORT, nf->clone(), status);
    if (!assertSuccess("Error creating measure format en SHORT", status)) {
        return;
    }
    verifyFormat("en SHORT", mf, abbrevData, UPRV_LENGTHOF(abbrevData));
    mf = MeasureFormat(en, UMEASFMT_WIDTH_NARROW, nf->clone(), status);
    if (!assertSuccess("Error creating measure format en NARROW", status)) {
        return;
    }
    verifyFormat("en NARROW", mf, narrowData, UPRV_LENGTHOF(narrowData));
    mf = MeasureFormat(en, UMEASFMT_WIDTH_NUMERIC, nf->clone(), status);
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
    mf = MeasureFormat(de, UMEASFMT_WIDTH_WIDE, nf->clone(), status);
    if (!assertSuccess("Error creating measure format de WIDE", status)) {
        return;
    }
    verifyFormat("de WIDE", mf, fullDataDe, UPRV_LENGTHOF(fullDataDe));
    mf = MeasureFormat(de, UMEASFMT_WIDTH_NUMERIC, nf->clone(), status);
    if (!assertSuccess("Error creating measure format de NUMERIC", status)) {
        return;
    }
    verifyFormat("de NUMERIC", mf, numericDataDe, UPRV_LENGTHOF(numericDataDe));

    Locale bengali("bn");
    nf.adoptInstead(NumberFormat::createInstance(bengali, status));
    if (!assertSuccess("Error creating number format de object", status)) {
        return;
    }
    nf->setMaximumFractionDigits(4);
    mf = MeasureFormat(bengali, UMEASFMT_WIDTH_NUMERIC, nf->clone(), status);
    if (!assertSuccess("Error creating measure format bn NUMERIC", status)) {
        return;
    }
    verifyFormat("bn NUMERIC", mf, numericDataBn, UPRV_LENGTHOF(numericDataBn));

    Locale bengaliLatin("bn-u-nu-latn");
    nf.adoptInstead(NumberFormat::createInstance(bengaliLatin, status));
    if (!assertSuccess("Error creating number format de object", status)) {
        return;
    }
    nf->setMaximumFractionDigits(4);
    mf = MeasureFormat(bengaliLatin, UMEASFMT_WIDTH_NUMERIC, nf->clone(), status);
    if (!assertSuccess("Error creating measure format bn-u-nu-latn NUMERIC", status)) {
        return;
    }
    verifyFormat("bn-u-nu-latn NUMERIC", mf, numericDataBnLatn, UPRV_LENGTHOF(numericDataBnLatn));

    status = U_ZERO_ERROR;
    LocalPointer<RuleBasedNumberFormat> rbnf(new RuleBasedNumberFormat(URBNF_SPELLOUT, en, status));
    if (U_FAILURE(status)) {
        dataerrln("Error creating rbnf en object - %s", u_errorName(status));
        return;
    }
    mf = MeasureFormat(en, UMEASFMT_WIDTH_WIDE, rbnf->clone(), status);
    if (!assertSuccess("Error creating measure format en WIDE with rbnf", status)) {
        return;
    }
    verifyFormat("en WIDE rbnf", mf, fullDataSpellout, UPRV_LENGTHOF(fullDataSpellout));

    Locale fr(Locale::getFrench());
    LocalPointer<RuleBasedNumberFormat> rbnffr(new RuleBasedNumberFormat(URBNF_SPELLOUT, fr, status));
    if (U_FAILURE(status)) {
        dataerrln("Error creating rbnf fr object - %s", u_errorName(status));
        return;
    }
    mf = MeasureFormat(fr, UMEASFMT_WIDTH_WIDE, rbnffr->clone(), status);
    if (!assertSuccess("Error creating measure format fr WIDE with rbnf", status)) {
        return;
    }
    verifyFormat("fr WIDE rbnf", mf, fullDataSpelloutFr, UPRV_LENGTHOF(fullDataSpellout));
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
                  dynamic_cast<DecimalFormat *>(NumberFormat::createInstance(en, status));
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
        "1 \\u03BB.",
        "1 \\u03CE.",
        "1 \\u03B7\\u03BC\\u03AD\\u03C1\\u03B1",
        "1 \\u03B5\\u03B2\\u03B4.",
        "1 \\u03BC\\u03AE\\u03BD.",
        "1 \\u03AD\\u03C4.",            // year (one)
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
        "7 \\u03BB.",
        "7 \\u03CE.",            // hour (other)
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
        "1 \\u03BB.",
        "1 \\u03CE.",
        "1 \\u03B7\\u03BC\\u03AD\\u03C1\\u03B1",
        "1 \\u03B5\\u03B2\\u03B4.",
        "1 \\u03BC\\u03AE\\u03BD.",
        "1 \\u03AD\\u03C4.",            // year (one)
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
        "7 \\u03BB.",
        "7 \\u03CE.",            // hour (other)
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
    FieldPosition pos(FieldPosition::DONT_CARE);
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
    verifyFormat("TestFormatMeasuresZeroArg", fmt, nullptr, 0, "");
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
            en, UMEASFMT_WIDTH_WIDE,
            1.0, *pound, *second, "1 pound per second");
    helperTestSimplePer(
            en, UMEASFMT_WIDTH_WIDE,
            2.0, *pound, *second, "2 pounds per second");
    helperTestSimplePer(
            en, UMEASFMT_WIDTH_WIDE,
            1.0, *pound, *minute, "1 pound per minute");
    helperTestSimplePer(
            en, UMEASFMT_WIDTH_WIDE,
            2.0, *pound, *minute, "2 pounds per minute");

    helperTestSimplePer(
            en, UMEASFMT_WIDTH_SHORT, 1.0, *pound, *second, "1 lb/s");
    helperTestSimplePer(
            en, UMEASFMT_WIDTH_SHORT, 2.0, *pound, *second, "2 lb/s");
    helperTestSimplePer(
            en, UMEASFMT_WIDTH_SHORT, 1.0, *pound, *minute, "1 lb/min");
    helperTestSimplePer(
            en, UMEASFMT_WIDTH_SHORT, 2.0, *pound, *minute, "2 lb/min");

    helperTestSimplePer(
            en, UMEASFMT_WIDTH_NARROW, 1.0, *pound, *second, "1#/s");
    helperTestSimplePer(
            en, UMEASFMT_WIDTH_NARROW, 2.0, *pound, *second, "2#/s");
    helperTestSimplePer(
            en, UMEASFMT_WIDTH_NARROW, 1.0, *pound, *minute, "1#/min");
    helperTestSimplePer(
            en, UMEASFMT_WIDTH_NARROW, 2.0, *pound, *minute, "2#/min");

    helperTestSimplePer(
            en, UMEASFMT_WIDTH_SHORT,
            23.3, *pound, *second, "23.3 lb/s",
            NumberFormat::kDecimalSeparatorField,
            2, 3);

    helperTestSimplePer(
            en, UMEASFMT_WIDTH_SHORT,
            23.3, *pound, *second, "23.3 lb/s",
            NumberFormat::kIntegerField,
            0, 2);

    helperTestSimplePer(
            en, UMEASFMT_WIDTH_SHORT,
            23.3, *pound, *minute, "23.3 lb/min",
            NumberFormat::kDecimalSeparatorField,
            2, 3);

    helperTestSimplePer(
            en, UMEASFMT_WIDTH_SHORT,
            23.3, *pound, *minute, "23.3 lb/min",
            NumberFormat::kIntegerField,
            0, 2);
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
            pl,
            UMEASFMT_WIDTH_WIDE,
            1.0, *foot, *second, "1 stopa na sekund\\u0119");
    helperTestSimplePer(
            pl,
            UMEASFMT_WIDTH_WIDE,
            2.0, *foot, *second, "2 stopy na sekund\\u0119");
    helperTestSimplePer(
            pl,
            UMEASFMT_WIDTH_WIDE,
            5.0, *foot, *second, "5 st\\u00f3p na sekund\\u0119");
    helperTestSimplePer(
            pl, UMEASFMT_WIDTH_WIDE,
            1.5, *foot, *second, "1,5 stopy na sekund\\u0119");
}

void MeasureFormatTest::helperTestSimplePer(
        const Locale &locale,
        UMeasureFormatWidth width,
        double value,
        const MeasureUnit &unit,
        const MeasureUnit &perUnit,
        const char *expected) {
    helperTestSimplePer(
            locale,
            width,
            value,
            unit,
            perUnit,
            expected,
            FieldPosition::DONT_CARE,
            0,
            0);
}

void MeasureFormatTest::helperTestSimplePer(
        const Locale &locale,
        UMeasureFormatWidth width,
        double value,
        const MeasureUnit &unit,
        const MeasureUnit &perUnit,
        const char *expected,
        int32_t field,
        int32_t expected_start,
        int32_t expected_end) {
    UErrorCode status = U_ZERO_ERROR;
    FieldPosition pos(field);
    MeasureFormat fmt(locale, width, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    Measure measure(value, unit.clone(), status);
    if (!assertSuccess("Error creating measure object", status)) {
        return;
    }
    UnicodeString prefix("prefix: ");
    UnicodeString buffer(prefix);
    fmt.formatMeasurePerUnit(
            measure,
            perUnit,
            buffer,
            pos,
            status);
    if (!assertSuccess("Error formatting measures with per", status)) {
        return;
    }
    UnicodeString uexpected(expected);
    uexpected = prefix + uexpected;
    assertEquals(
            "TestSimplePer",
            uexpected.unescape(),
            buffer);
    if (field != FieldPosition::DONT_CARE) {
        assertEquals(
                "Start", expected_start, pos.getBeginIndex() - prefix.length());
        assertEquals(
                "End", expected_end, pos.getEndIndex() - prefix.length());
    }
}

void MeasureFormatTest::TestMultiples() {
    Locale ru("ru");
    Locale en("en");
    helperTestMultiples(en, UMEASFMT_WIDTH_WIDE,   "2 miles, 1 foot, 2.3 inches");
    helperTestMultiples(en, UMEASFMT_WIDTH_SHORT,  "2 mi, 1 ft, 2.3 in");
    helperTestMultiples(en, UMEASFMT_WIDTH_NARROW, "2mi 1\\u2032 2.3\\u2033");
    helperTestMultiples(ru, UMEASFMT_WIDTH_WIDE,   "2 \\u043C\\u0438\\u043B\\u0438 1 \\u0444\\u0443\\u0442 2,3 \\u0434\\u044E\\u0439\\u043C\\u0430");
    helperTestMultiples(ru, UMEASFMT_WIDTH_SHORT,  "2 \\u043C\\u0438 1 \\u0444\\u0442 2,3 \\u0434\\u044E\\u0439\\u043C.");
    helperTestMultiples(ru, UMEASFMT_WIDTH_NARROW, "2 \\u043C\\u0438 1 \\u0444\\u0442 2,3 \\u0434\\u044E\\u0439\\u043C.");
}

void MeasureFormatTest::helperTestMultiples(
        const Locale &locale,
        UMeasureFormatWidth width,
        const char *expected) {
    UErrorCode status = U_ZERO_ERROR;
    FieldPosition pos(FieldPosition::DONT_CARE);
    MeasureFormat fmt(locale, width, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    Measure measures[] = {
            Measure(2.0, MeasureUnit::createMile(status), status),
            Measure(1.0, MeasureUnit::createFoot(status), status),
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

void MeasureFormatTest::TestManyLocaleDurations() {
    UErrorCode status = U_ZERO_ERROR;
    Measure measures[] = {
            Measure(5.0, MeasureUnit::createHour(status), status),
            Measure(37.0, MeasureUnit::createMinute(status), status)};
    if (!assertSuccess("Error creating measures", status)) {
        return;
    }
    helperTestManyLocaleDurations("da", UMEASFMT_WIDTH_NARROW,  measures, UPRV_LENGTHOF(measures), "5 t og 37 m");
    helperTestManyLocaleDurations("da", UMEASFMT_WIDTH_NUMERIC, measures, UPRV_LENGTHOF(measures), "5.37");
    helperTestManyLocaleDurations("de", UMEASFMT_WIDTH_NARROW,  measures, UPRV_LENGTHOF(measures), "5 Std., 37 Min.");
    helperTestManyLocaleDurations("de", UMEASFMT_WIDTH_NUMERIC, measures, UPRV_LENGTHOF(measures), "5:37");
    helperTestManyLocaleDurations("en", UMEASFMT_WIDTH_NARROW,  measures, UPRV_LENGTHOF(measures), "5h 37m");
    helperTestManyLocaleDurations("en", UMEASFMT_WIDTH_NUMERIC, measures, UPRV_LENGTHOF(measures), "5:37");
    helperTestManyLocaleDurations("en_GB", UMEASFMT_WIDTH_NARROW,  measures, UPRV_LENGTHOF(measures), "5h 37m");
    helperTestManyLocaleDurations("en_GB", UMEASFMT_WIDTH_NUMERIC, measures, UPRV_LENGTHOF(measures), "5:37");
    helperTestManyLocaleDurations("es", UMEASFMT_WIDTH_NARROW,  measures, UPRV_LENGTHOF(measures), "5h 37min");
    helperTestManyLocaleDurations("es", UMEASFMT_WIDTH_NUMERIC, measures, UPRV_LENGTHOF(measures), "5:37");
    helperTestManyLocaleDurations("fi", UMEASFMT_WIDTH_NARROW,  measures, UPRV_LENGTHOF(measures), "5t 37min");
    helperTestManyLocaleDurations("fi", UMEASFMT_WIDTH_NUMERIC, measures, UPRV_LENGTHOF(measures), "5.37");
    helperTestManyLocaleDurations("fr", UMEASFMT_WIDTH_NARROW,  measures, UPRV_LENGTHOF(measures), "5h 37min");
    helperTestManyLocaleDurations("fr", UMEASFMT_WIDTH_NUMERIC, measures, UPRV_LENGTHOF(measures), "5:37");
    helperTestManyLocaleDurations("is", UMEASFMT_WIDTH_NARROW,  measures, UPRV_LENGTHOF(measures), "5 klst. og 37 m\\u00EDn.");
    helperTestManyLocaleDurations("is", UMEASFMT_WIDTH_NUMERIC, measures, UPRV_LENGTHOF(measures), "5:37");
    helperTestManyLocaleDurations("ja", UMEASFMT_WIDTH_NARROW,  measures, UPRV_LENGTHOF(measures), "5h37m");
    helperTestManyLocaleDurations("ja", UMEASFMT_WIDTH_NUMERIC, measures, UPRV_LENGTHOF(measures), "5:37");
    helperTestManyLocaleDurations("nb", UMEASFMT_WIDTH_NARROW,  measures, UPRV_LENGTHOF(measures), "5t, 37m");
    helperTestManyLocaleDurations("nb", UMEASFMT_WIDTH_NUMERIC, measures, UPRV_LENGTHOF(measures), "5:37");
    helperTestManyLocaleDurations("nl", UMEASFMT_WIDTH_NARROW,  measures, UPRV_LENGTHOF(measures), "5 u, 37 m");
    helperTestManyLocaleDurations("nl", UMEASFMT_WIDTH_NUMERIC, measures, UPRV_LENGTHOF(measures), "5:37");
    helperTestManyLocaleDurations("nn", UMEASFMT_WIDTH_NARROW,  measures, UPRV_LENGTHOF(measures), "5t 37m");
    helperTestManyLocaleDurations("nn", UMEASFMT_WIDTH_NUMERIC, measures, UPRV_LENGTHOF(measures), "5:37");
    helperTestManyLocaleDurations("sv", UMEASFMT_WIDTH_NARROW,  measures, UPRV_LENGTHOF(measures), "5h 37m");
    helperTestManyLocaleDurations("sv", UMEASFMT_WIDTH_NUMERIC, measures, UPRV_LENGTHOF(measures), "5:37");
    helperTestManyLocaleDurations("zh", UMEASFMT_WIDTH_NARROW,  measures, UPRV_LENGTHOF(measures), "5\\u5C0F\\u65F637\\u5206\\u949F");
    helperTestManyLocaleDurations("zh", UMEASFMT_WIDTH_NUMERIC, measures, UPRV_LENGTHOF(measures), "5:37");
}

void MeasureFormatTest::helperTestManyLocaleDurations( const char *localeID,
                                                       UMeasureFormatWidth width,
                                                       const Measure *measures,
                                                       int32_t measureCount,
                                                       const char *expected) {
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat fmt(Locale(localeID), width, status);
    if (U_FAILURE(status)) {
        errln("Could not create MeasureFormat for locale %s, width %d, status: %s", localeID, (int)width, u_errorName(status));
        return;
    }
    UnicodeString buffer;
    FieldPosition pos(FieldPosition::DONT_CARE);
    fmt.formatMeasures(measures, measureCount, buffer, pos, status);
    if (U_FAILURE(status)) {
        errln("MeasureFormat::formatMeasures failed for locale %s, width %d, status: %s", localeID, (int)width, u_errorName(status));
        return;
    }
    UnicodeString expStr(UnicodeString(expected).unescape());
    if (buffer != expStr) {
        errln("MeasureFormat::formatMeasures for locale " + UnicodeString(localeID) + ", width " + width + ", expected \"" + expStr + "\", got \"" + buffer + "\"");
    }
}

void MeasureFormatTest::TestGram() {
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat fmt("en", UMEASFMT_WIDTH_SHORT, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    Measure gram((double)1, MeasureUnit::createGram(status), status);
    Measure gforce((double)1, MeasureUnit::createGForce(status), status);
    if (!assertSuccess("Error creating measures", status)) {
        return;
    }
    verifyFormat("TestGram", fmt, &gram, 1, "1 g");
    verifyFormat("TestGram", fmt, &gforce, 1, "1 G");
}

void MeasureFormatTest::TestCurrencies() {
    char16_t USD[4] = {};
    u_uastrcpy(USD, "USD");
    UErrorCode status = U_ZERO_ERROR;
    CurrencyUnit USD_unit(USD, status);
    assertEquals("Currency Unit", USD, USD_unit.getISOCurrency());
    if (!assertSuccess("Error creating CurrencyUnit", status)) {
        return;
    }
    CurrencyAmount USD_1(1.0, USD, status);
    assertEquals("Currency Code", USD, USD_1.getISOCurrency());
    CurrencyAmount USD_2(2.0, USD, status);
    CurrencyAmount USD_NEG_1(-1.0, USD, status);
    if (!assertSuccess("Error creating currencies", status)) {
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
    verifyFormat("TestCurrenciesShort", fmt, &USD_NEG_1, 1, "-USD\\u00A01.00");
    verifyFormat("TestCurrenciesShort", fmt, &USD_1, 1, "USD\\u00A01.00");
    verifyFormat("TestCurrenciesShort", fmt, &USD_2, 1, "USD\\u00A02.00");
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

void MeasureFormatTest::TestDisplayNames() {
    UErrorCode status = U_ZERO_ERROR;
    helperTestDisplayName( MeasureUnit::createYear(status), "en", UMEASFMT_WIDTH_WIDE, "years" );
    helperTestDisplayName( MeasureUnit::createYear(status), "ja", UMEASFMT_WIDTH_WIDE, "\\u5E74" );
    helperTestDisplayName( MeasureUnit::createYear(status), "es", UMEASFMT_WIDTH_WIDE, "a\\u00F1os" );
    helperTestDisplayName( MeasureUnit::createYear(status), "pt", UMEASFMT_WIDTH_WIDE, "anos" );
    helperTestDisplayName( MeasureUnit::createYear(status), "pt-PT", UMEASFMT_WIDTH_WIDE, "anos" );
    helperTestDisplayName( MeasureUnit::createAmpere(status), "en", UMEASFMT_WIDTH_WIDE, "amperes" );
    helperTestDisplayName( MeasureUnit::createAmpere(status), "ja", UMEASFMT_WIDTH_WIDE, "\\u30A2\\u30F3\\u30DA\\u30A2" );
    helperTestDisplayName( MeasureUnit::createAmpere(status), "es", UMEASFMT_WIDTH_WIDE, "amperios" );
    helperTestDisplayName( MeasureUnit::createAmpere(status), "pt", UMEASFMT_WIDTH_WIDE, "amperes" );
    helperTestDisplayName( MeasureUnit::createAmpere(status), "pt-PT", UMEASFMT_WIDTH_WIDE, "amperes" );
    helperTestDisplayName( MeasureUnit::createMeterPerSecondSquared(status), "pt", UMEASFMT_WIDTH_WIDE, "metros por segundo ao quadrado" );
    helperTestDisplayName( MeasureUnit::createMeterPerSecondSquared(status), "pt-PT", UMEASFMT_WIDTH_WIDE, "metros por segundo quadrado" );
    helperTestDisplayName( MeasureUnit::createSquareKilometer(status), "pt", UMEASFMT_WIDTH_NARROW, "km\\u00B2" );
    helperTestDisplayName( MeasureUnit::createSquareKilometer(status), "pt", UMEASFMT_WIDTH_SHORT, "km\\u00B2" );
    helperTestDisplayName( MeasureUnit::createSquareKilometer(status), "pt", UMEASFMT_WIDTH_WIDE, "quil\\u00F4metros quadrados" );
    helperTestDisplayName( MeasureUnit::createSecond(status), "pt-PT", UMEASFMT_WIDTH_NARROW, "s" );
    helperTestDisplayName( MeasureUnit::createSecond(status), "pt-PT", UMEASFMT_WIDTH_SHORT, "s" );
    helperTestDisplayName( MeasureUnit::createSecond(status), "pt-PT", UMEASFMT_WIDTH_WIDE, "segundos" );
    helperTestDisplayName( MeasureUnit::createSecond(status), "pt", UMEASFMT_WIDTH_NARROW, "s" );
    helperTestDisplayName( MeasureUnit::createSecond(status), "pt", UMEASFMT_WIDTH_SHORT, "s" );
    helperTestDisplayName( MeasureUnit::createSecond(status), "pt", UMEASFMT_WIDTH_WIDE, "segundos" );
    assertSuccess("Error creating measure units", status);
}

void MeasureFormatTest::helperTestDisplayName(const MeasureUnit *unit,
                            const char *localeID,
                            UMeasureFormatWidth width,
                            const char *expected) {
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat fmt(Locale(localeID), width, status);
    if (U_FAILURE(status)) {
        errln("Could not create MeasureFormat for locale %s, width %d, status: %s",
            localeID, (int)width, u_errorName(status));
        return;
    }

    UnicodeString dnam = fmt.getUnitDisplayName(*unit, status);
    if (U_FAILURE(status)) {
        errln("MeasureFormat::getUnitDisplayName failed for unit %s-%s, locale %s, width %d, status: %s",
            unit->getType(), unit->getSubtype(), localeID, (int)width, u_errorName(status));
        return;
    }

    UnicodeString expStr(UnicodeString(expected).unescape());
    if (dnam != expStr) {
        errln("MeasureFormat::getUnitDisplayName for unit %s-%s, locale %s, width %d: expected \"%s\", got \"%s\"",
            unit->getType(), unit->getSubtype(), localeID, (int)width, CStr(expStr)(), CStr(dnam)());
    }

    // Delete the measure unit
    delete unit;
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
    measure = Measure(43.0, MeasureUnit::createFoot(status), status);
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
      Measure((double)354, MeasureUnit::createMeter(status), status),
      Measure((double)23, MeasureUnit::createCentimeter(status), status)};
    Measure second[] = {
      Measure((double)354, MeasureUnit::createMeter(status), status),
      Measure((double)23, MeasureUnit::createCentimeter(status), status),
      Measure((double)5.4, MeasureUnit::createMillimeter(status), status)};
    Measure third[] = {
      Measure((double)3, MeasureUnit::createMeter(status), status),
      Measure((double)23, MeasureUnit::createCentimeter(status), status),
      Measure((double)5, MeasureUnit::createMillimeter(status), status)};
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

void MeasureFormatTest::TestBadArg() {
    UErrorCode status = U_ZERO_ERROR;
    MeasureFormat fmt("en", UMEASFMT_WIDTH_SHORT, status);
    if (!assertSuccess("Error creating format object", status)) {
        return;
    }
    FieldPosition pos(FieldPosition::DONT_CARE);
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
            Measure((int32_t)INT32_MAX, MeasureUnit::createYear(status), status),
            Measure((int32_t)INT32_MIN, MeasureUnit::createMonth(status), status),
            Measure(-987.0, MeasureUnit::createDay(status), status),
            Measure(1362.0, MeasureUnit::createHour(status), status),
            Measure(987.0, MeasureUnit::createMinute(status), status)};
    FieldPosition pos(NumberFormat::kGroupingSeparatorField);
    UnicodeString appendTo;
    fmt.formatMeasures(ms, 5, appendTo, pos, status);
    if (!assertSuccess("Error formatting", status)) {
        return;
    }
    assertEquals(
            "grouping separator",
            "2,147,483,647 yrs, -2,147,483,648 mths, -987 days, 1,362 hr, 987 min",
            appendTo);
    assertEquals("begin index", 1, pos.getBeginIndex());
    assertEquals("end index", 2, pos.getEndIndex());
}

void MeasureFormatTest::TestDoubleZero() {
    UErrorCode status = U_ZERO_ERROR;
    Measure measures[] = {
        Measure(4.7, MeasureUnit::createHour(status), status),
        Measure(23.0, MeasureUnit::createMinute(status), status),
        Measure(16.0, MeasureUnit::createSecond(status), status)};
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

void MeasureFormatTest::TestUnitPerUnitResolution() {
    UErrorCode status = U_ZERO_ERROR;
    Locale en("en");
    MeasureFormat fmt("en", UMEASFMT_WIDTH_SHORT, status);
    Measure measure(50.0, MeasureUnit::createPoundForce(status), status);
    LocalPointer<MeasureUnit> sqInch(MeasureUnit::createSquareInch(status));
    if (!assertSuccess("Create of format unit and per unit", status)) {
        return;
    }
    FieldPosition pos(FieldPosition::DONT_CARE);
    UnicodeString actual;
    fmt.formatMeasurePerUnit(
            measure,
            *sqInch,
            actual,
            pos,
            status);
    assertEquals("", "50 psi", actual);
}

void MeasureFormatTest::TestIndividualPluralFallback() {
    // See ticket #11986 "incomplete fallback in MeasureFormat".
    // In CLDR 28, fr_CA temperature-generic/short has only the "one" form,
    // and falls back to fr for the "other" form.
    IcuTestErrorCode errorCode(*this, "TestIndividualPluralFallback");
    MeasureFormat mf("fr_CA", UMEASFMT_WIDTH_SHORT, errorCode);
    if (errorCode.errIfFailureAndReset("MeasureFormat mf(...) failed.")) {
        return;
    }
    LocalPointer<Measure> twoDeg(
        new Measure(2.0, MeasureUnit::createGenericTemperature(errorCode), errorCode), errorCode);
    if (errorCode.errIfFailureAndReset("Creating twoDeg failed.")) {
        return;
    }
    UnicodeString expected = UNICODE_STRING_SIMPLE("2\\u00B0").unescape();
    UnicodeString actual;
    // Formattable adopts the pointer
    mf.format(Formattable(twoDeg.orphan()), actual, errorCode);
    if (errorCode.errIfFailureAndReset("mf.format(...) failed.")) {
        return;
    }
    assertEquals("2 deg temp in fr_CA", expected, actual, true);
    errorCode.errIfFailureAndReset("mf.format failed");
}

void MeasureFormatTest::Test20332_PersonUnits() {
    IcuTestErrorCode status(*this, "Test20332_PersonUnits");
    const struct TestCase {
        const char* locale;
        MeasureUnit* unitToAdopt;
        UMeasureFormatWidth width;
        const char* expected;
    } cases[] = {
        {"en-us", MeasureUnit::createYearPerson(status), UMEASFMT_WIDTH_NARROW, "25y"},
        {"en-us", MeasureUnit::createYearPerson(status), UMEASFMT_WIDTH_SHORT, "25 yrs"},
        {"en-us", MeasureUnit::createYearPerson(status), UMEASFMT_WIDTH_WIDE, "25 years"},
        {"en-us", MeasureUnit::createMonthPerson(status), UMEASFMT_WIDTH_NARROW, "25m"},
        {"en-us", MeasureUnit::createMonthPerson(status), UMEASFMT_WIDTH_SHORT, "25 mths"},
        {"en-us", MeasureUnit::createMonthPerson(status), UMEASFMT_WIDTH_WIDE, "25 months"},
        {"en-us", MeasureUnit::createWeekPerson(status), UMEASFMT_WIDTH_NARROW, "25w"},
        {"en-us", MeasureUnit::createWeekPerson(status), UMEASFMT_WIDTH_SHORT, "25 wks"},
        {"en-us", MeasureUnit::createWeekPerson(status), UMEASFMT_WIDTH_WIDE, "25 weeks"},
        {"en-us", MeasureUnit::createDayPerson(status), UMEASFMT_WIDTH_NARROW, "25d"},
        {"en-us", MeasureUnit::createDayPerson(status), UMEASFMT_WIDTH_SHORT, "25 days"},
        {"en-us", MeasureUnit::createDayPerson(status), UMEASFMT_WIDTH_WIDE, "25 days"}
    };
    for (const auto& cas : cases) {
        MeasureFormat mf(cas.locale, cas.width, status);
        if (status.errIfFailureAndReset()) { return; }
        Measure measure(25, cas.unitToAdopt, status);
        if (status.errIfFailureAndReset()) { return; }
        verifyFormat(cas.locale, mf, &measure, 1, cas.expected);
    }
}

void MeasureFormatTest::TestNumericTime() {
    IcuTestErrorCode status(*this, "TestNumericTime");

    MeasureFormat fmt("en", UMEASFMT_WIDTH_NUMERIC, status);

    Measure hours(112, MeasureUnit::createHour(status), status);
    Measure minutes(113, MeasureUnit::createMinute(status), status);
    Measure seconds(114, MeasureUnit::createSecond(status), status);
    Measure fhours(112.8765, MeasureUnit::createHour(status), status);
    Measure fminutes(113.8765, MeasureUnit::createMinute(status), status);
    Measure fseconds(114.8765, MeasureUnit::createSecond(status), status);
    if (status.errDataIfFailureAndReset(WHERE)) return;

    verifyFormat("hours", fmt, &hours, 1, "112h");
    verifyFormat("minutes", fmt, &minutes, 1, "113m");
    verifyFormat("seconds", fmt, &seconds, 1, "114s");

    verifyFormat("fhours", fmt, &fhours, 1, "112.876h");
    verifyFormat("fminutes", fmt, &fminutes, 1, "113.876m");
    verifyFormat("fseconds", fmt, &fseconds, 1, "114.876s");

    Measure hoursMinutes[2] = {hours, minutes};
    verifyFormat("hoursMinutes", fmt, hoursMinutes, 2, "112:113");
    Measure hoursSeconds[2] = {hours, seconds};
    verifyFormat("hoursSeconds", fmt, hoursSeconds, 2, "112:00:114");
    Measure minutesSeconds[2] = {minutes, seconds};
    verifyFormat("minutesSeconds", fmt, minutesSeconds, 2, "113:114");

    Measure hoursFminutes[2] = {hours, fminutes};
    verifyFormat("hoursFminutes", fmt, hoursFminutes, 2, "112:113.876");
    Measure hoursFseconds[2] = {hours, fseconds};
    verifyFormat("hoursFseconds", fmt, hoursFseconds, 2, "112:00:114.876");
    Measure minutesFseconds[2] = {minutes, fseconds};
    verifyFormat("hoursMminutesFsecondsinutes", fmt, minutesFseconds, 2, "113:114.876");

    Measure fhoursMinutes[2] = {fhours, minutes};
    verifyFormat("fhoursMinutes", fmt, fhoursMinutes, 2, "112:113");
    Measure fhoursSeconds[2] = {fhours, seconds};
    verifyFormat("fhoursSeconds", fmt, fhoursSeconds, 2, "112:00:114");
    Measure fminutesSeconds[2] = {fminutes, seconds};
    verifyFormat("fminutesSeconds", fmt, fminutesSeconds, 2, "113:114");

    Measure fhoursFminutes[2] = {fhours, fminutes};
    verifyFormat("fhoursFminutes", fmt, fhoursFminutes, 2, "112:113.876");
    Measure fhoursFseconds[2] = {fhours, fseconds};
    verifyFormat("fhoursFseconds", fmt, fhoursFseconds, 2, "112:00:114.876");
    Measure fminutesFseconds[2] = {fminutes, fseconds};
    verifyFormat("fminutesFseconds", fmt, fminutesFseconds, 2, "113:114.876");

    Measure hoursMinutesSeconds[3] = {hours, minutes, seconds};
    verifyFormat("hoursMinutesSeconds", fmt, hoursMinutesSeconds, 3, "112:113:114");
    Measure fhoursFminutesFseconds[3] = {fhours, fminutes, fseconds};
    verifyFormat("fhoursFminutesFseconds", fmt, fhoursFminutesFseconds, 3, "112:113:114.876");
}

void MeasureFormatTest::TestNumericTimeSomeSpecialFormats() {
    IcuTestErrorCode status(*this, "TestNumericTimeSomeSpecialFormats");

    Measure fhours(2.8765432, MeasureUnit::createHour(status), status);
    Measure fminutes(3.8765432, MeasureUnit::createMinute(status), status);
    if (status.errDataIfFailureAndReset(WHERE)) return;

    Measure fhoursFminutes[2] = {fhours, fminutes};

    // Latvian is one of the very few locales 0-padding the hour
    MeasureFormat fmtLt("lt", UMEASFMT_WIDTH_NUMERIC, status);
    if (status.errDataIfFailureAndReset(WHERE)) return;
    verifyFormat("Latvian fhoursFminutes", fmtLt, fhoursFminutes, 2, "02:03,877");

    // Danish is one of the very few locales using '.' as separator
    MeasureFormat fmtDa("da", UMEASFMT_WIDTH_NUMERIC, status);
    verifyFormat("Danish fhoursFminutes", fmtDa, fhoursFminutes, 2, "2.03,877");
}

void MeasureFormatTest::TestIdentifiers() {
    IcuTestErrorCode status(*this, "TestIdentifiers");
    struct TestCase {
        const char* id;
        const char* normalized;
    } cases[] = {
        // Correctly normalized identifiers should not change
        {"", ""},
        {"square-meter-per-square-meter", "square-meter-per-square-meter"},
        {"kilogram-meter-per-square-meter-square-second",
         "kilogram-meter-per-square-meter-square-second"},
        {"square-mile-and-square-foot", "square-mile-and-square-foot"},
        {"square-foot-and-square-mile", "square-foot-and-square-mile"},
        {"per-cubic-centimeter", "per-cubic-centimeter"},
        {"per-kilometer", "per-kilometer"},

        // Normalization of power and per
        {"pow2-foot-and-pow2-mile", "square-foot-and-square-mile"},
        {"gram-square-gram-per-dekagram", "cubic-gram-per-dekagram"},
        {"kilogram-per-meter-per-second", "kilogram-per-meter-second"},
        {"kilometer-per-second-per-megaparsec", "kilometer-per-megaparsec-second"},

        // Correct order of units, as per unitQuantities in CLDR's units.xml
        {"newton-meter", "newton-meter"},
        {"meter-newton", "newton-meter"},
        {"pound-force-foot", "pound-force-foot"},
        {"foot-pound-force", "pound-force-foot"},
        {"kilowatt-hour", "kilowatt-hour"},
        {"hour-kilowatt", "kilowatt-hour"},

        // Testing prefixes are parsed and produced correctly (ensures no
        // collisions in the enum values)
        {"yoctofoot", "yoctofoot"},
        {"zeptofoot", "zeptofoot"},
        {"attofoot", "attofoot"},
        {"femtofoot", "femtofoot"},
        {"picofoot", "picofoot"},
        {"nanofoot", "nanofoot"},
        {"microfoot", "microfoot"},
        {"millifoot", "millifoot"},
        {"centifoot", "centifoot"},
        {"decifoot", "decifoot"},
        {"foot", "foot"},
        {"dekafoot", "dekafoot"},
        {"hectofoot", "hectofoot"},
        {"kilofoot", "kilofoot"},
        {"megafoot", "megafoot"},
        {"gigafoot", "gigafoot"},
        {"terafoot", "terafoot"},
        {"petafoot", "petafoot"},
        {"exafoot", "exafoot"},
        {"zettafoot", "zettafoot"},
        {"yottafoot", "yottafoot"},
        {"kibibyte", "kibibyte"},
        {"mebibyte", "mebibyte"},
        {"gibibyte", "gibibyte"},
        {"tebibyte", "tebibyte"},
        {"pebibyte", "pebibyte"},
        {"exbibyte", "exbibyte"},
        {"zebibyte", "zebibyte"},
        {"yobibyte", "yobibyte"},

        // Testing aliases
        {"foodcalorie", "foodcalorie"},
        {"dot-per-centimeter", "dot-per-centimeter"},
        {"dot-per-inch", "dot-per-inch"},
        {"dot", "dot"},

        // Testing sort order of prefixes.
        {"megafoot-mebifoot-kibifoot-kilofoot", "mebifoot-megafoot-kibifoot-kilofoot"},
        {"per-megafoot-mebifoot-kibifoot-kilofoot", "per-mebifoot-megafoot-kibifoot-kilofoot"},
        {"megafoot-mebifoot-kibifoot-kilofoot-per-megafoot-mebifoot-kibifoot-kilofoot",
         "mebifoot-megafoot-kibifoot-kilofoot-per-mebifoot-megafoot-kibifoot-kilofoot"},
        {"microfoot-millifoot-megafoot-mebifoot-kibifoot-kilofoot",
         "mebifoot-megafoot-kibifoot-kilofoot-millifoot-microfoot"},
        {"per-microfoot-millifoot-megafoot-mebifoot-kibifoot-kilofoot",
         "per-mebifoot-megafoot-kibifoot-kilofoot-millifoot-microfoot"},
    };
    for (const auto &cas : cases) {
        status.setScope(cas.id);
        MeasureUnit unit = MeasureUnit::forIdentifier(cas.id, status);
        status.errIfFailureAndReset();
        const char* actual = unit.getIdentifier();
        assertEquals(cas.id, cas.normalized, actual);
        status.errIfFailureAndReset();
    }
}

void MeasureFormatTest::TestInvalidIdentifiers() {
    IcuTestErrorCode status(*this, "TestInvalidIdentifiers");

    const char *const inputs[] = {
        "kilo",
        "kilokilo",
        "onekilo",
        "meterkilo",
        "meter-kilo",
        "k",
        "meter-",
        "meter+",
        "-meter",
        "+meter",
        "-kilometer",
        "+kilometer",
        "-pow2-meter",
        "+pow2-meter",
        "p2-meter",
        "p4-meter",
        "+",
        "-",
        "-mile",
        "-and-mile",
        "-per-mile",
        "one",
        "one-one",
        "one-per-mile",
        "one-per-cubic-centimeter",
        "square--per-meter",
        "metersecond", // Must have compound part in between single units

        // Negative powers not supported in mixed units yet. TODO(CLDR-13701).
        "per-hour-and-hertz",
        "hertz-and-per-hour",

        // Compound units not supported in mixed units yet. TODO(CLDR-13701).
        "kilonewton-meter-and-newton-meter",
    };

    for (const auto& input : inputs) {
        status.setScope(input);
        MeasureUnit::forIdentifier(input, status);
        status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
    }
}

void MeasureFormatTest::TestIdentifierDetails() {
    IcuTestErrorCode status(*this, "TestIdentifierDetails()");

    MeasureUnit joule = MeasureUnit::forIdentifier("joule", status);
    status.assertSuccess();
    assertEquals("Initial joule", "joule", joule.getIdentifier());

    static_assert(UMEASURE_PREFIX_INTERNAL_MAX_SI < 99, "Tests assume there is no prefix 99.");
    static_assert(UMEASURE_PREFIX_INTERNAL_MAX_BIN < 99, "Tests assume there is no prefix 99.");
    MeasureUnit unit = joule.withPrefix((UMeasurePrefix)99, status);
    if (!status.expectErrorAndReset(U_UNSUPPORTED_ERROR)) {
        errln("Invalid prefix should result in an error.");
    }
    assertEquals("Invalid prefix results in no identifier", "", unit.getIdentifier());

    unit = joule.withPrefix(UMEASURE_PREFIX_HECTO, status);
    status.assertSuccess();
    assertEquals("foo identifier", "hectojoule", unit.getIdentifier());

    unit = unit.withPrefix(UMEASURE_PREFIX_EXBI, status);
    status.assertSuccess();
    assertEquals("foo identifier", "exbijoule", unit.getIdentifier());
}

void MeasureFormatTest::TestPrefixes() {
    IcuTestErrorCode status(*this, "TestPrefixes()");
    const struct TestCase {
        UMeasurePrefix prefix;
        int32_t expectedBase;
        int32_t expectedPower;
    } cases[] = {
        {UMEASURE_PREFIX_QUECTO, 10, -30},
        {UMEASURE_PREFIX_RONTO, 10, -27},
        {UMEASURE_PREFIX_YOCTO, 10, -24},
        {UMEASURE_PREFIX_ZEPTO, 10, -21},
        {UMEASURE_PREFIX_ATTO, 10, -18},
        {UMEASURE_PREFIX_FEMTO, 10, -15},
        {UMEASURE_PREFIX_PICO, 10, -12},
        {UMEASURE_PREFIX_NANO, 10, -9},
        {UMEASURE_PREFIX_MICRO, 10, -6},
        {UMEASURE_PREFIX_MILLI, 10, -3},
        {UMEASURE_PREFIX_CENTI, 10, -2},
        {UMEASURE_PREFIX_DECI, 10, -1},
        {UMEASURE_PREFIX_ONE, 10, 0},
        {UMEASURE_PREFIX_DEKA, 10, 1},
        {UMEASURE_PREFIX_HECTO, 10, 2},
        {UMEASURE_PREFIX_KILO, 10, 3},
        {UMEASURE_PREFIX_MEGA, 10, 6},
        {UMEASURE_PREFIX_GIGA, 10, 9},
        {UMEASURE_PREFIX_TERA, 10, 12},
        {UMEASURE_PREFIX_PETA, 10, 15},
        {UMEASURE_PREFIX_EXA, 10, 18},
        {UMEASURE_PREFIX_ZETTA, 10, 21},
        {UMEASURE_PREFIX_YOTTA, 10, 24},
        {UMEASURE_PREFIX_RONNA, 10, 27},
        {UMEASURE_PREFIX_QUETTA, 10, 30},
        {UMEASURE_PREFIX_KIBI, 1024, 1},
        {UMEASURE_PREFIX_MEBI, 1024, 2},
        {UMEASURE_PREFIX_GIBI, 1024, 3},
        {UMEASURE_PREFIX_TEBI, 1024, 4},
        {UMEASURE_PREFIX_PEBI, 1024, 5},
        {UMEASURE_PREFIX_EXBI, 1024, 6},
        {UMEASURE_PREFIX_ZEBI, 1024, 7},
        {UMEASURE_PREFIX_YOBI, 1024, 8},
    };

    for (auto cas : cases) {
        MeasureUnit m = MeasureUnit::getAmpere().withPrefix(cas.prefix, status);
        assertEquals("umeas_getPrefixPower()", cas.expectedPower,
                     umeas_getPrefixPower(m.getPrefix(status)));
        assertEquals("umeas_getPrefixBase()", cas.expectedBase,
                     umeas_getPrefixBase(m.getPrefix(status)));
    }
}

void MeasureFormatTest::TestParseBuiltIns() {
    IcuTestErrorCode status(*this, "TestParseBuiltIns()");
    int32_t totalCount = MeasureUnit::getAvailable(nullptr, 0, status);
    status.expectErrorAndReset(U_BUFFER_OVERFLOW_ERROR);
    std::unique_ptr<MeasureUnit[]> units(new MeasureUnit[totalCount]);
    totalCount = MeasureUnit::getAvailable(units.get(), totalCount, status);
    status.assertSuccess();
    for (int32_t i = 0; i < totalCount; i++) {
        MeasureUnit &unit = units[i];
        if (uprv_strcmp(unit.getType(), "currency") == 0) {
            continue;
        }

        // Prove that all built-in units are parseable, except "generic" temperature:
        MeasureUnit parsed = MeasureUnit::forIdentifier(unit.getIdentifier(), status);
        if (unit == MeasureUnit::getGenericTemperature()) {
            status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
        } else {
            status.assertSuccess();
            CharString msg;
            msg.append("parsed MeasureUnit '", status);
            msg.append(parsed.getIdentifier(), status);
            msg.append("' should equal built-in '", status);
            msg.append(unit.getIdentifier(), status);
            msg.append("'", status);
            status.assertSuccess();
            assertTrue(msg.data(), unit == parsed);
        }
    }
}

void MeasureFormatTest::TestParseToBuiltIn() {
    IcuTestErrorCode status(*this, "TestParseToBuiltIn()");
    const struct TestCase {
        const char *identifier;
        MeasureUnit expectedBuiltIn;
    } cases[] = {
        {"meter-per-second-per-second", MeasureUnit::getMeterPerSecondSquared()},
        {"meter-per-second-second", MeasureUnit::getMeterPerSecondSquared()},
        {"centimeter-centimeter", MeasureUnit::getSquareCentimeter()},
        {"square-foot", MeasureUnit::getSquareFoot()},
        {"pow2-inch", MeasureUnit::getSquareInch()},
        {"milligram-per-deciliter", MeasureUnit::getMilligramPerDeciliter()},
        {"pound-force-per-pow2-inch", MeasureUnit::getPoundPerSquareInch()},
        {"yard-pow2-yard", MeasureUnit::getCubicYard()},
        {"square-yard-yard", MeasureUnit::getCubicYard()},
    };

    for (const auto& cas : cases) {
        MeasureUnit fromIdent = MeasureUnit::forIdentifier(cas.identifier, status);
        status.assertSuccess();
        assertEquals("forIdentifier returns a normal built-in unit when it exists",
                     cas.expectedBuiltIn.getOffset(), fromIdent.getOffset());
        assertEquals("type", cas.expectedBuiltIn.getType(), fromIdent.getType());
        assertEquals("subType", cas.expectedBuiltIn.getSubtype(), fromIdent.getSubtype());
    }
}

// Kilogram is a "base unit", although it's also "gram" with a kilo- prefix.
// This tests that it is handled in the preferred manner.
void MeasureFormatTest::TestKilogramIdentifier() {
    IcuTestErrorCode status(*this, "TestKilogramIdentifier");

    // SI unit of mass
    MeasureUnit kilogram = MeasureUnit::forIdentifier("kilogram", status);
    // Metric mass unit
    MeasureUnit gram = MeasureUnit::forIdentifier("gram", status);
    // Microgram: still a built-in type
    MeasureUnit microgram = MeasureUnit::forIdentifier("microgram", status);
    // Nanogram: not a built-in type at this time
    MeasureUnit nanogram = MeasureUnit::forIdentifier("nanogram", status);
    status.assertSuccess();

    assertEquals("parsed kilogram equals built-in kilogram", MeasureUnit::getKilogram().getType(),
                 kilogram.getType());
    assertEquals("parsed kilogram equals built-in kilogram", MeasureUnit::getKilogram().getSubtype(),
                 kilogram.getSubtype());
    assertEquals("parsed gram equals built-in gram", MeasureUnit::getGram().getType(), gram.getType());
    assertEquals("parsed gram equals built-in gram", MeasureUnit::getGram().getSubtype(),
                 gram.getSubtype());
    assertEquals("parsed microgram equals built-in microgram", MeasureUnit::getMicrogram().getType(),
                 microgram.getType());
    assertEquals("parsed microgram equals built-in microgram", MeasureUnit::getMicrogram().getSubtype(),
                 microgram.getSubtype());
    assertEquals("nanogram", "", nanogram.getType());
    assertEquals("nanogram", "nanogram", nanogram.getIdentifier());

    assertEquals("prefix of kilogram", UMEASURE_PREFIX_KILO, kilogram.getPrefix(status));
    assertEquals("prefix of gram", UMEASURE_PREFIX_ONE, gram.getPrefix(status));
    assertEquals("prefix of microgram", UMEASURE_PREFIX_MICRO, microgram.getPrefix(status));
    assertEquals("prefix of nanogram", UMEASURE_PREFIX_NANO, nanogram.getPrefix(status));

    MeasureUnit tmp = kilogram.withPrefix(UMEASURE_PREFIX_MILLI, status);
    assertEquals(UnicodeString("Kilogram + milli should be milligram, got: ") + tmp.getIdentifier(),
                 MeasureUnit::getMilligram().getIdentifier(), tmp.getIdentifier());
}

void MeasureFormatTest::TestCompoundUnitOperations() {
    IcuTestErrorCode status(*this, "TestCompoundUnitOperations");

    MeasureUnit::forIdentifier("kilometer-per-second-joule", status);

    MeasureUnit kilometer = MeasureUnit::getKilometer();
    MeasureUnit cubicMeter = MeasureUnit::getCubicMeter();
    MeasureUnit meter = kilometer.withPrefix(UMEASURE_PREFIX_ONE, status);
    MeasureUnit centimeter1 = kilometer.withPrefix(UMEASURE_PREFIX_CENTI, status);
    MeasureUnit centimeter2 = meter.withPrefix(UMEASURE_PREFIX_CENTI, status);
    MeasureUnit cubicDecimeter = cubicMeter.withPrefix(UMEASURE_PREFIX_DECI, status);

    verifySingleUnit(kilometer, UMEASURE_PREFIX_KILO, 1, "kilometer");
    verifySingleUnit(meter, UMEASURE_PREFIX_ONE, 1, "meter");
    verifySingleUnit(centimeter1, UMEASURE_PREFIX_CENTI, 1, "centimeter");
    verifySingleUnit(centimeter2, UMEASURE_PREFIX_CENTI, 1, "centimeter");
    verifySingleUnit(cubicDecimeter, UMEASURE_PREFIX_DECI, 3, "cubic-decimeter");

    assertTrue("centimeter equality", centimeter1 == centimeter2);
    assertTrue("kilometer inequality", centimeter1 != kilometer);

    MeasureUnit squareMeter = meter.withDimensionality(2, status);
    MeasureUnit overCubicCentimeter = centimeter1.withDimensionality(-3, status);
    MeasureUnit quarticKilometer = kilometer.withDimensionality(4, status);
    MeasureUnit overQuarticKilometer1 = kilometer.withDimensionality(-4, status);

    verifySingleUnit(squareMeter, UMEASURE_PREFIX_ONE, 2, "square-meter");
    verifySingleUnit(overCubicCentimeter, UMEASURE_PREFIX_CENTI, -3, "per-cubic-centimeter");
    verifySingleUnit(quarticKilometer, UMEASURE_PREFIX_KILO, 4, "pow4-kilometer");
    verifySingleUnit(overQuarticKilometer1, UMEASURE_PREFIX_KILO, -4, "per-pow4-kilometer");

    assertTrue("power inequality", quarticKilometer != overQuarticKilometer1);

    MeasureUnit overQuarticKilometer2 = quarticKilometer.reciprocal(status);
    MeasureUnit overQuarticKilometer3 = kilometer.product(kilometer, status)
        .product(kilometer, status)
        .product(kilometer, status)
        .reciprocal(status);
    MeasureUnit overQuarticKilometer4 = meter.withDimensionality(4, status)
        .reciprocal(status)
        .withPrefix(UMEASURE_PREFIX_KILO, status);

    verifySingleUnit(overQuarticKilometer2, UMEASURE_PREFIX_KILO, -4, "per-pow4-kilometer");
    verifySingleUnit(overQuarticKilometer3, UMEASURE_PREFIX_KILO, -4, "per-pow4-kilometer");
    verifySingleUnit(overQuarticKilometer4, UMEASURE_PREFIX_KILO, -4, "per-pow4-kilometer");

    assertTrue("reciprocal equality", overQuarticKilometer1 == overQuarticKilometer2);
    assertTrue("reciprocal equality", overQuarticKilometer1 == overQuarticKilometer3);
    assertTrue("reciprocal equality", overQuarticKilometer1 == overQuarticKilometer4);

    MeasureUnit kiloSquareSecond = MeasureUnit::getSecond()
        .withDimensionality(2, status).withPrefix(UMEASURE_PREFIX_KILO, status);
    MeasureUnit meterSecond = meter.product(kiloSquareSecond, status);
    MeasureUnit cubicMeterSecond1 = meter.withDimensionality(3, status).product(kiloSquareSecond, status);
    MeasureUnit centimeterSecond1 = meter.withPrefix(UMEASURE_PREFIX_CENTI, status).product(kiloSquareSecond, status);
    MeasureUnit secondCubicMeter = kiloSquareSecond.product(meter.withDimensionality(3, status), status);
    MeasureUnit secondCentimeter = kiloSquareSecond.product(meter.withPrefix(UMEASURE_PREFIX_CENTI, status), status);
    MeasureUnit secondCentimeterPerKilometer = secondCentimeter.product(kilometer.reciprocal(status), status);

    verifySingleUnit(kiloSquareSecond, UMEASURE_PREFIX_KILO, 2, "square-kilosecond");
    const char* meterSecondSub[] = {"meter", "square-kilosecond"};
    verifyCompoundUnit(meterSecond, "meter-square-kilosecond",
        meterSecondSub, UPRV_LENGTHOF(meterSecondSub));
    const char* cubicMeterSecond1Sub[] = {"cubic-meter", "square-kilosecond"};
    verifyCompoundUnit(cubicMeterSecond1, "cubic-meter-square-kilosecond",
        cubicMeterSecond1Sub, UPRV_LENGTHOF(cubicMeterSecond1Sub));
    const char* centimeterSecond1Sub[] = {"centimeter", "square-kilosecond"};
    verifyCompoundUnit(centimeterSecond1, "centimeter-square-kilosecond",
        centimeterSecond1Sub, UPRV_LENGTHOF(centimeterSecond1Sub));
    const char* secondCubicMeterSub[] = {"cubic-meter", "square-kilosecond"};
    verifyCompoundUnit(secondCubicMeter, "cubic-meter-square-kilosecond",
        secondCubicMeterSub, UPRV_LENGTHOF(secondCubicMeterSub));
    const char* secondCentimeterSub[] = {"centimeter", "square-kilosecond"};
    verifyCompoundUnit(secondCentimeter, "centimeter-square-kilosecond",
        secondCentimeterSub, UPRV_LENGTHOF(secondCentimeterSub));
    const char* secondCentimeterPerKilometerSub[] = {"centimeter", "square-kilosecond", "per-kilometer"};
    verifyCompoundUnit(secondCentimeterPerKilometer, "centimeter-square-kilosecond-per-kilometer",
        secondCentimeterPerKilometerSub, UPRV_LENGTHOF(secondCentimeterPerKilometerSub));

    assertTrue("reordering equality", cubicMeterSecond1 == secondCubicMeter);
    assertTrue("additional simple units inequality", secondCubicMeter != secondCentimeter);

    // Don't allow get/set power or SI or binary prefix on compound units
    status.errIfFailureAndReset();
    meterSecond.getDimensionality(status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
    meterSecond.withDimensionality(3, status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
    meterSecond.getPrefix(status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
    meterSecond.withPrefix(UMEASURE_PREFIX_CENTI, status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);

    // Test that StringPiece does not overflow
    MeasureUnit centimeter3 = MeasureUnit::forIdentifier({secondCentimeter.getIdentifier(), 10}, status);
    verifySingleUnit(centimeter3, UMEASURE_PREFIX_CENTI, 1, "centimeter");
    assertTrue("string piece equality", centimeter1 == centimeter3);

    MeasureUnit footInch = MeasureUnit::forIdentifier("foot-and-inch", status);
    MeasureUnit inchFoot = MeasureUnit::forIdentifier("inch-and-foot", status);

    const char* footInchSub[] = {"foot", "inch"};
    verifyMixedUnit(footInch, "foot-and-inch",
        footInchSub, UPRV_LENGTHOF(footInchSub));
    const char* inchFootSub[] = {"inch", "foot"};
    verifyMixedUnit(inchFoot, "inch-and-foot",
        inchFootSub, UPRV_LENGTHOF(inchFootSub));

    assertTrue("order matters inequality", footInch != inchFoot);

    MeasureUnit dimensionless;
    MeasureUnit dimensionless2 = MeasureUnit::forIdentifier("", status);
    status.errIfFailureAndReset("Dimensionless MeasureUnit.");
    assertTrue("dimensionless equality", dimensionless == dimensionless2);

    // We support starting from an "identity" MeasureUnit and then combining it
    // with others via product:
    MeasureUnit kilometer2 = dimensionless.product(kilometer, status);
    status.errIfFailureAndReset("dimensionless.product(kilometer, status)");
    verifySingleUnit(kilometer2, UMEASURE_PREFIX_KILO, 1, "kilometer");
    assertTrue("kilometer equality", kilometer == kilometer2);

    // Test out-of-range powers
    MeasureUnit power15 = MeasureUnit::forIdentifier("pow15-kilometer", status);
    verifySingleUnit(power15, UMEASURE_PREFIX_KILO, 15, "pow15-kilometer");
    status.errIfFailureAndReset();
    MeasureUnit power16a = MeasureUnit::forIdentifier("pow16-kilometer", status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
    MeasureUnit power16b = power15.product(kilometer, status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
    MeasureUnit powerN15 = MeasureUnit::forIdentifier("per-pow15-kilometer", status);
    verifySingleUnit(powerN15, UMEASURE_PREFIX_KILO, -15, "per-pow15-kilometer");
    status.errIfFailureAndReset();
    MeasureUnit powerN16a = MeasureUnit::forIdentifier("per-pow16-kilometer", status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
    MeasureUnit powerN16b = powerN15.product(overQuarticKilometer1, status);
    status.expectErrorAndReset(U_ILLEGAL_ARGUMENT_ERROR);
}

void MeasureFormatTest::TestDimensionlessBehaviour() {
    IcuTestErrorCode status(*this, "TestDimensionlessBehaviour");
    MeasureUnit dimensionless;
    MeasureUnit modified;

    // At the time of writing, each of the seven groups below caused
    // Parser::from("") to be called:

    // splitToSingleUnits
    auto pair = dimensionless.splitToSingleUnits(status);
    int32_t count = pair.second;
    status.errIfFailureAndReset("dimensionless.splitToSingleUnits(...)");
    assertEquals("no singles in dimensionless", 0, count);

    // product(dimensionless)
    MeasureUnit mile = MeasureUnit::getMile();
    mile = mile.product(dimensionless, status);
    status.errIfFailureAndReset("mile.product(dimensionless, ...)");
    verifySingleUnit(mile, UMEASURE_PREFIX_ONE, 1, "mile");

    // dimensionless.getPrefix()
    UMeasurePrefix unitPrefix = dimensionless.getPrefix(status);
    status.errIfFailureAndReset("dimensionless.getPrefix(...)");
    assertEquals("dimensionless SIPrefix", UMEASURE_PREFIX_ONE, unitPrefix);

    // dimensionless.withPrefix()
    modified = dimensionless.withPrefix(UMEASURE_PREFIX_KILO, status);
    status.errIfFailureAndReset("dimensionless.withPrefix(...)");
    pair = dimensionless.splitToSingleUnits(status);
    count = pair.second;
    assertEquals("no singles in modified", 0, count);
    unitPrefix = modified.getPrefix(status);
    status.errIfFailureAndReset("modified.getPrefix(...)");
    assertEquals("modified SIPrefix", UMEASURE_PREFIX_ONE, unitPrefix);

    // dimensionless.getComplexity()
    UMeasureUnitComplexity complexity = dimensionless.getComplexity(status);
    status.errIfFailureAndReset("dimensionless.getComplexity(...)");
    assertEquals("dimensionless complexity", UMEASURE_UNIT_SINGLE, complexity);

    // Dimensionality is mostly meaningless for dimensionless units, but it's
    // still considered a SINGLE unit, so this code doesn't throw errors:

    // dimensionless.getDimensionality()
    int32_t dimensionality = dimensionless.getDimensionality(status);
    status.errIfFailureAndReset("dimensionless.getDimensionality(...)");
    assertEquals("dimensionless dimensionality", 0, dimensionality);

    // dimensionless.withDimensionality()
    dimensionless.withDimensionality(-1, status);
    status.errIfFailureAndReset("dimensionless.withDimensionality(...)");
    dimensionality = dimensionless.getDimensionality(status);
    status.errIfFailureAndReset("dimensionless.getDimensionality(...)");
    assertEquals("dimensionless dimensionality", 0, dimensionality);
}

// ICU-21060
void MeasureFormatTest::Test21060_AddressSanitizerProblem() {
    IcuTestErrorCode status(*this, "Test21060_AddressSanitizerProblem");

    MeasureUnit first = MeasureUnit::forIdentifier("", status);
    status.errIfFailureAndReset();

    // Experimentally, a compound unit like "kilogram-meter" failed. A single
    // unit like "kilogram" or "meter" did not fail, did not trigger the
    // problem.
    MeasureUnit crux = MeasureUnit::forIdentifier("per-meter", status);

    // Heap allocation of a new CharString for first.identifier happens here:
    first = first.product(crux, status);

    // Constructing second from first's identifier resulted in a failure later,
    // as second held a reference to a substring of first's identifier:
    MeasureUnit second = MeasureUnit::forIdentifier(first.getIdentifier(), status);

    // Heap is freed here, as an old first.identifier CharString is deallocated
    // and a new CharString is allocated:
    first = first.product(crux, status);

    // Proving we've had no failure yet:
    status.errIfFailureAndReset();

    // heap-use-after-free failure happened here, since a SingleUnitImpl had
    // held onto a StringPiece pointing at a substring of an identifier that was
    // freed above:
    second = second.product(crux, status);

    status.errIfFailureAndReset();
}

void MeasureFormatTest::Test21223_FrenchDuration() {
    IcuTestErrorCode status(*this, "Test21223_FrenchDuration");
    MeasureFormat mf("fr-FR", UMEASFMT_WIDTH_NARROW, status);
    Measure H5M10[] = {
        {5, MeasureUnit::createHour(status), status},
        {10, MeasureUnit::createMinute(status), status}
    };
    UnicodeString result;
    FieldPosition pos;
    mf.formatMeasures(H5M10, UPRV_LENGTHOF(H5M10), result, pos, status);
    assertEquals("Should have consistent spacing", u"5h 10min", result);

    // Test additional locales:
    // int32_t localeCount;
    // const Locale* locales = Locale::getAvailableLocales(localeCount);
    // for (int32_t i=0; i<localeCount; i++) {
    //     auto& loc = locales[i];
    //     MeasureFormat mf1(loc, UMEASFMT_WIDTH_NARROW, status);
    //     mf1.formatMeasures(H5M10, UPRV_LENGTHOF(H5M10), result.remove(), pos, status);
    //     assertFalse(result + u" " + loc.getName(), true);
    // }
}

void MeasureFormatTest::TestInternalMeasureUnitImpl() {
    IcuTestErrorCode status(*this, "TestInternalMeasureUnitImpl");
    MeasureUnitImpl mu1 = MeasureUnitImpl::forIdentifier("meter", status);
    status.assertSuccess();
    assertEquals("mu1 initial identifier", "", mu1.identifier.data());
    assertEquals("mu1 initial complexity", UMEASURE_UNIT_SINGLE, mu1.complexity);
    assertEquals("mu1 initial units length", 1, mu1.singleUnits.length());
    if (mu1.singleUnits.length() > 0) {
        assertEquals("mu1 initial units[0]", "meter", mu1.singleUnits[0]->getSimpleUnitID());
    }

    // Producing identifier via build(): the std::move() means mu1 gets modified
    // while it also gets assigned to tmp's internal fImpl.
    MeasureUnit tmp = std::move(mu1).build(status);
    status.assertSuccess();
    assertEquals("mu1 post-move-build identifier", "meter", mu1.identifier.data());
    assertEquals("mu1 post-move-build complexity", UMEASURE_UNIT_SINGLE, mu1.complexity);
    assertEquals("mu1 post-move-build units length", 1, mu1.singleUnits.length());
    if (mu1.singleUnits.length() > 0) {
        assertEquals("mu1 post-move-build units[0]", "meter", mu1.singleUnits[0]->getSimpleUnitID());
    }
    assertEquals("MeasureUnit tmp identifier", "meter", tmp.getIdentifier());

    // This temporary variable is used when forMeasureUnit's first parameter
    // lacks an fImpl instance:
    MeasureUnitImpl tmpMemory;
    const MeasureUnitImpl &tmpImplRef = MeasureUnitImpl::forMeasureUnit(tmp, tmpMemory, status);
    status.assertSuccess();
    assertEquals("tmpMemory identifier", "", tmpMemory.identifier.data());
    assertEquals("tmpMemory complexity", UMEASURE_UNIT_SINGLE, tmpMemory.complexity);
    assertEquals("tmpMemory units length", 1, tmpMemory.singleUnits.length());
    if (mu1.singleUnits.length() > 0) {
        assertEquals("tmpMemory units[0]", "meter", tmpMemory.singleUnits[0]->getSimpleUnitID());
    }
    assertEquals("tmpImplRef identifier", "", tmpImplRef.identifier.data());
    assertEquals("tmpImplRef complexity", UMEASURE_UNIT_SINGLE, tmpImplRef.complexity);

    MeasureUnitImpl mu2 = MeasureUnitImpl::forIdentifier("newton-meter", status);
    status.assertSuccess();
    mu1 = std::move(mu2);
    assertEquals("mu1 = move(mu2): identifier", "", mu1.identifier.data());
    assertEquals("mu1 = move(mu2): complexity", UMEASURE_UNIT_COMPOUND, mu1.complexity);
    assertEquals("mu1 = move(mu2): units length", 2, mu1.singleUnits.length());
    if (mu1.singleUnits.length() >= 2) {
        assertEquals("mu1 = move(mu2): units[0]", "newton", mu1.singleUnits[0]->getSimpleUnitID());
        assertEquals("mu1 = move(mu2): units[1]", "meter", mu1.singleUnits[1]->getSimpleUnitID());
    }

    mu1 = MeasureUnitImpl::forIdentifier("hour-and-minute-and-second", status);
    status.assertSuccess();
    assertEquals("mu1 = HMS: identifier", "", mu1.identifier.data());
    assertEquals("mu1 = HMS: complexity", UMEASURE_UNIT_MIXED, mu1.complexity);
    assertEquals("mu1 = HMS: units length", 3, mu1.singleUnits.length());
    if (mu1.singleUnits.length() >= 3) {
        assertEquals("mu1 = HMS: units[0]", "hour", mu1.singleUnits[0]->getSimpleUnitID());
        assertEquals("mu1 = HMS: units[1]", "minute", mu1.singleUnits[1]->getSimpleUnitID());
        assertEquals("mu1 = HMS: units[2]", "second", mu1.singleUnits[2]->getSimpleUnitID());
    }

    MeasureUnitImpl m2 = MeasureUnitImpl::forIdentifier("", status);
    m2.appendSingleUnit(SingleUnitImpl::forMeasureUnit(MeasureUnit::getMeter(), status), status);
    m2.appendSingleUnit(SingleUnitImpl::forMeasureUnit(MeasureUnit::getMeter(), status), status);
    status.assertSuccess();
    assertEquals("append meter twice: complexity", UMEASURE_UNIT_SINGLE, m2.complexity);
    assertEquals("append meter twice: units length", 1, m2.singleUnits.length());
    if (mu1.singleUnits.length() >= 1) {
        assertEquals("append meter twice: units[0]", "meter", m2.singleUnits[0]->getSimpleUnitID());
    }
    assertEquals("append meter twice: identifier", "square-meter",
                 std::move(m2).build(status).getIdentifier());

    MeasureUnitImpl mcm = MeasureUnitImpl::forIdentifier("", status);
    mcm.appendSingleUnit(SingleUnitImpl::forMeasureUnit(MeasureUnit::getMeter(), status), status);
    mcm.appendSingleUnit(SingleUnitImpl::forMeasureUnit(MeasureUnit::getCentimeter(), status), status);
    status.assertSuccess();
    assertEquals("append meter & centimeter: complexity", UMEASURE_UNIT_COMPOUND, mcm.complexity);
    assertEquals("append meter & centimeter: units length", 2, mcm.singleUnits.length());
    if (mu1.singleUnits.length() >= 2) {
        assertEquals("append meter & centimeter: units[0]", "meter",
                     mcm.singleUnits[0]->getSimpleUnitID());
        assertEquals("append meter & centimeter: units[1]", "meter",
                     mcm.singleUnits[1]->getSimpleUnitID());
    }
    assertEquals("append meter & centimeter: identifier", "meter-centimeter",
                 std::move(mcm).build(status).getIdentifier());

    MeasureUnitImpl m2m = MeasureUnitImpl::forIdentifier("meter-square-meter", status);
    status.assertSuccess();
    assertEquals("meter-square-meter: complexity", UMEASURE_UNIT_SINGLE, m2m.complexity);
    assertEquals("meter-square-meter: units length", 1, m2m.singleUnits.length());
    if (mu1.singleUnits.length() >= 1) {
        assertEquals("meter-square-meter: units[0]", "meter", m2m.singleUnits[0]->getSimpleUnitID());
    }
    assertEquals("meter-square-meter: identifier", "cubic-meter",
                 std::move(m2m).build(status).getIdentifier());
}

void MeasureFormatTest::TestMeasureEquality() {
    IcuTestErrorCode errorCode(*this, "TestMeasureEquality");
    Measure measures[] = {
        { 1., MeasureUnit::createLiter(errorCode), errorCode },
        { 1., MeasureUnit::createLiter(errorCode), errorCode },
        { 2., MeasureUnit::createLiter(errorCode), errorCode },
        { 1., MeasureUnit::createGram(errorCode), errorCode }
    };
    static const char *const names[] = { "1 liter", "another liter", "2 liters", "1 gram" };

    // Verify that C++20 -Wambiguous-reversed-operator isn't triggered.
    assertTrue("Equal", measures[0] == measures[1]);
    assertTrue("Not Equal", measures[2] != measures[3]);

    for (int32_t i = 0; i < UPRV_LENGTHOF(measures); ++i) {
        for (int32_t j = 0; j < UPRV_LENGTHOF(measures); ++j) {
            const Measure &a = measures[i];
            const UObject &b = measures[j];  // UObject for "other"
            std::string eq = std::string(names[i]) + std::string(" == ") + std::string(names[j]);
            std::string ne = std::string(names[i]) + std::string(" != ") + std::string(names[j]);
            // 1l = 1l
            bool expectedEquals = i == j || (i <= 1 && j <= 1);
            assertEquals(eq.c_str(), expectedEquals, a == b);
            assertEquals(ne.c_str(), !expectedEquals, a != b);
        }
    }

    UnicodeString s(u"?");
    for (int32_t i = 0; i < UPRV_LENGTHOF(measures); ++i) {
        const Measure &a = measures[i];
        std::string eq = std::string(names[i]) + std::string(" == UnicodeString");
        std::string ne = std::string(names[i]) + std::string(" != UnicodeString");
        assertEquals(eq.c_str(), false, a == s);
        assertEquals(ne.c_str(), true, a != s);
    }
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
    FieldPosition pos(FieldPosition::DONT_CARE);
    UErrorCode status = U_ZERO_ERROR;
    fmt.formatMeasures(measures, measureCount, result, pos, status);
    if (!assertSuccess("Error formatting", status)) {
        return;
    }
    assertEquals(description, ctou(expected), result);
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

void MeasureFormatTest::verifySingleUnit(
        const MeasureUnit& unit,
        UMeasurePrefix unitPrefix,
        int8_t power,
        const char* identifier) {
    IcuTestErrorCode status(*this, "verifySingleUnit");
    UnicodeString uid(identifier, -1, US_INV);
    assertEquals(uid + ": SI or binary prefix",
        unitPrefix,
        unit.getPrefix(status));
    status.errIfFailureAndReset("%s: SI or binary prefix", identifier);
    assertEquals(uid + ": Power",
        static_cast<int32_t>(power),
        static_cast<int32_t>(unit.getDimensionality(status)));
    status.errIfFailureAndReset("%s: Power", identifier);
    assertEquals(uid + ": Identifier",
        identifier,
        unit.getIdentifier());
    status.errIfFailureAndReset("%s: Identifier", identifier);
    assertTrue(uid + ": Constructor",
        unit == MeasureUnit::forIdentifier(identifier, status));
    status.errIfFailureAndReset("%s: Constructor", identifier);
    assertEquals(uid + ": Complexity",
        UMEASURE_UNIT_SINGLE,
        unit.getComplexity(status));
    status.errIfFailureAndReset("%s: Complexity", identifier);
}

void MeasureFormatTest::verifyCompoundUnit(
        const MeasureUnit& unit,
        const char* identifier,
        const char** subIdentifiers,
        int32_t subIdentifierCount) {
    IcuTestErrorCode status(*this, "verifyCompoundUnit");
    UnicodeString uid(identifier, -1, US_INV);
    assertEquals(uid + ": Identifier",
        identifier,
        unit.getIdentifier());
    status.errIfFailureAndReset("%s: Identifier", identifier);
    assertTrue(uid + ": Constructor",
        unit == MeasureUnit::forIdentifier(identifier, status));
    status.errIfFailureAndReset("%s: Constructor", identifier);
    assertEquals(uid + ": Complexity",
        UMEASURE_UNIT_COMPOUND,
        unit.getComplexity(status));
    status.errIfFailureAndReset("%s: Complexity", identifier);

    auto pair = unit.splitToSingleUnits(status);
    const LocalArray<MeasureUnit>& subUnits = pair.first;
    int32_t length = pair.second;
    assertEquals(uid + ": Length", subIdentifierCount, length);
    for (int32_t i = 0;; i++) {
        if (i >= subIdentifierCount || i >= length) break;
        assertEquals(uid + ": Sub-unit #" + Int64ToUnicodeString(i),
            subIdentifiers[i],
            subUnits[i].getIdentifier());
        assertEquals(uid + ": Sub-unit Complexity",
            UMEASURE_UNIT_SINGLE,
            subUnits[i].getComplexity(status));
    }
}

void MeasureFormatTest::verifyMixedUnit(
        const MeasureUnit& unit,
        const char* identifier,
        const char** subIdentifiers,
        int32_t subIdentifierCount) {
    IcuTestErrorCode status(*this, "verifyMixedUnit");
    UnicodeString uid(identifier, -1, US_INV);
    assertEquals(uid + ": Identifier",
        identifier,
        unit.getIdentifier());
    status.errIfFailureAndReset("%s: Identifier", identifier);
    assertTrue(uid + ": Constructor",
        unit == MeasureUnit::forIdentifier(identifier, status));
    status.errIfFailureAndReset("%s: Constructor", identifier);
    assertEquals(uid + ": Complexity",
        UMEASURE_UNIT_MIXED,
        unit.getComplexity(status));
    status.errIfFailureAndReset("%s: Complexity", identifier);

    auto pair = unit.splitToSingleUnits(status);
    const LocalArray<MeasureUnit>& subUnits = pair.first;
    int32_t length = pair.second;
    assertEquals(uid + ": Length", subIdentifierCount, length);
    for (int32_t i = 0;; i++) {
        if (i >= subIdentifierCount || i >= length) break;
        assertEquals(uid + ": Sub-unit #" + Int64ToUnicodeString(i),
            subIdentifiers[i],
            subUnits[i].getIdentifier());
    }
}

extern IntlTest *createMeasureFormatTest() {
    return new MeasureFormatTest();
}

#endif
