/*
**********************************************************************
* Copyright (c) 2004-2014, International Business Machines
* Corporation and others.  All Rights Reserved.
**********************************************************************
* Author: Alan Liu
* Created: April 26, 2004
* Since: ICU 3.0
**********************************************************************
*/
#include "utypeinfo.h" // for 'typeid' to work

#include "unicode/measunit.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/uenum.h"
#include "ustrenum.h"
#include "cstring.h"
#include "uassert.h"

U_NAMESPACE_BEGIN

UOBJECT_DEFINE_RTTI_IMPLEMENTATION(MeasureUnit)

// All code between the "Start generated code" comment and
// the "End generated code" comment is auto generated code
// and must not be edited manually. For instructions on how to correctly
// update this code, refer to:
// https://sites.google.com/site/icusite/design/formatting/measureformat/updating-measure-unit
//
// Start generated code

static const int32_t gOffsets[] = {
    0,
    2,
    6,
    15,
    17,
    277,
    287,
    297,
    301,
    307,
    311,
    329,
    330,
    341,
    347,
    352,
    353,
    356,
    359,
    381
};

static const int32_t gIndexes[] = {
    0,
    2,
    6,
    15,
    17,
    17,
    27,
    37,
    41,
    47,
    51,
    69,
    70,
    81,
    87,
    92,
    93,
    96,
    99,
    121
};

static const char * const gTypes[] = {
    "acceleration",
    "angle",
    "area",
    "consumption",
    "currency",
    "digital",
    "duration",
    "electric",
    "energy",
    "frequency",
    "length",
    "light",
    "mass",
    "power",
    "pressure",
    "proportion",
    "speed",
    "temperature",
    "volume"
};

static const char * const gSubTypes[] = {
    "g-force",
    "meter-per-second-squared",
    "arc-minute",
    "arc-second",
    "degree",
    "radian",
    "acre",
    "hectare",
    "square-centimeter",
    "square-foot",
    "square-inch",
    "square-kilometer",
    "square-meter",
    "square-mile",
    "square-yard",
    "liter-per-kilometer",
    "mile-per-gallon",
    "ADP",
    "AED",
    "AFA",
    "AFN",
    "ALL",
    "AMD",
    "ANG",
    "AOA",
    "AON",
    "AOR",
    "ARA",
    "ARP",
    "ARS",
    "ATS",
    "AUD",
    "AWG",
    "AYM",
    "AZM",
    "AZN",
    "BAD",
    "BAM",
    "BBD",
    "BDT",
    "BEC",
    "BEF",
    "BEL",
    "BGL",
    "BGN",
    "BHD",
    "BIF",
    "BMD",
    "BND",
    "BOB",
    "BOV",
    "BRC",
    "BRE",
    "BRL",
    "BRN",
    "BRR",
    "BSD",
    "BTN",
    "BWP",
    "BYB",
    "BYR",
    "BZD",
    "CAD",
    "CDF",
    "CHC",
    "CHE",
    "CHF",
    "CHW",
    "CLF",
    "CLP",
    "CNY",
    "COP",
    "COU",
    "CRC",
    "CSD",
    "CSK",
    "CUC",
    "CUP",
    "CVE",
    "CYP",
    "CZK",
    "DDM",
    "DEM",
    "DJF",
    "DKK",
    "DOP",
    "DZD",
    "ECS",
    "ECV",
    "EEK",
    "EGP",
    "ERN",
    "ESA",
    "ESB",
    "ESP",
    "ETB",
    "EUR",
    "FIM",
    "FJD",
    "FKP",
    "FRF",
    "GBP",
    "GEK",
    "GEL",
    "GHC",
    "GHP",
    "GHS",
    "GIP",
    "GMD",
    "GNF",
    "GQE",
    "GRD",
    "GTQ",
    "GWP",
    "GYD",
    "HKD",
    "HNL",
    "HRD",
    "HRK",
    "HTG",
    "HUF",
    "IDR",
    "IEP",
    "ILS",
    "INR",
    "IQD",
    "IRR",
    "ISK",
    "ITL",
    "JMD",
    "JOD",
    "JPY",
    "KES",
    "KGS",
    "KHR",
    "KMF",
    "KPW",
    "KRW",
    "KWD",
    "KYD",
    "KZT",
    "LAK",
    "LBP",
    "LKR",
    "LRD",
    "LSL",
    "LTL",
    "LTT",
    "LUC",
    "LUF",
    "LUL",
    "LVL",
    "LVR",
    "LYD",
    "MAD",
    "MDL",
    "MGA",
    "MGF",
    "MKD",
    "MLF",
    "MMK",
    "MNT",
    "MOP",
    "MRO",
    "MTL",
    "MUR",
    "MVR",
    "MWK",
    "MXN",
    "MXV",
    "MYR",
    "MZM",
    "MZN",
    "NAD",
    "NGN",
    "NIO",
    "NLG",
    "NOK",
    "NPR",
    "NZD",
    "OMR",
    "PAB",
    "PEI",
    "PEN",
    "PES",
    "PGK",
    "PHP",
    "PKR",
    "PLN",
    "PLZ",
    "PTE",
    "PYG",
    "QAR",
    "ROL",
    "RON",
    "RSD",
    "RUB",
    "RUR",
    "RWF",
    "SAR",
    "SBD",
    "SCR",
    "SDD",
    "SDG",
    "SEK",
    "SGD",
    "SHP",
    "SIT",
    "SKK",
    "SLL",
    "SOS",
    "SRD",
    "SRG",
    "SSP",
    "STD",
    "SVC",
    "SYP",
    "SZL",
    "THB",
    "TJR",
    "TJS",
    "TMM",
    "TMT",
    "TND",
    "TOP",
    "TPE",
    "TRL",
    "TRY",
    "TTD",
    "TWD",
    "TZS",
    "UAH",
    "UAK",
    "UGX",
    "USD",
    "USN",
    "USS",
    "UYI",
    "UYU",
    "UZS",
    "VEB",
    "VEF",
    "VND",
    "VUV",
    "WST",
    "XAF",
    "XAG",
    "XAU",
    "XBA",
    "XBB",
    "XBC",
    "XBD",
    "XCD",
    "XDR",
    "XEU",
    "XOF",
    "XPD",
    "XPF",
    "XPT",
    "XSU",
    "XTS",
    "XUA",
    "XXX",
    "YDD",
    "YER",
    "YUM",
    "YUN",
    "ZAL",
    "ZAR",
    "ZMK",
    "ZMW",
    "ZRN",
    "ZRZ",
    "ZWD",
    "ZWL",
    "ZWN",
    "ZWR",
    "bit",
    "byte",
    "gigabit",
    "gigabyte",
    "kilobit",
    "kilobyte",
    "megabit",
    "megabyte",
    "terabit",
    "terabyte",
    "day",
    "hour",
    "microsecond",
    "millisecond",
    "minute",
    "month",
    "nanosecond",
    "second",
    "week",
    "year",
    "ampere",
    "milliampere",
    "ohm",
    "volt",
    "calorie",
    "foodcalorie",
    "joule",
    "kilocalorie",
    "kilojoule",
    "kilowatt-hour",
    "gigahertz",
    "hertz",
    "kilohertz",
    "megahertz",
    "astronomical-unit",
    "centimeter",
    "decimeter",
    "fathom",
    "foot",
    "furlong",
    "inch",
    "kilometer",
    "light-year",
    "meter",
    "micrometer",
    "mile",
    "millimeter",
    "nanometer",
    "nautical-mile",
    "parsec",
    "picometer",
    "yard",
    "lux",
    "carat",
    "gram",
    "kilogram",
    "metric-ton",
    "microgram",
    "milligram",
    "ounce",
    "ounce-troy",
    "pound",
    "stone",
    "ton",
    "gigawatt",
    "horsepower",
    "kilowatt",
    "megawatt",
    "milliwatt",
    "watt",
    "hectopascal",
    "inch-hg",
    "millibar",
    "millimeter-of-mercury",
    "pound-per-square-inch",
    "karat",
    "kilometer-per-hour",
    "meter-per-second",
    "mile-per-hour",
    "celsius",
    "fahrenheit",
    "kelvin",
    "acre-foot",
    "bushel",
    "centiliter",
    "cubic-centimeter",
    "cubic-foot",
    "cubic-inch",
    "cubic-kilometer",
    "cubic-meter",
    "cubic-mile",
    "cubic-yard",
    "cup",
    "deciliter",
    "fluid-ounce",
    "gallon",
    "hectoliter",
    "liter",
    "megaliter",
    "milliliter",
    "pint",
    "quart",
    "tablespoon",
    "teaspoon"
};

MeasureUnit *MeasureUnit::createGForce(UErrorCode &status) {
    return MeasureUnit::create(0, 0, status);
}

MeasureUnit *MeasureUnit::createMeterPerSecondSquared(UErrorCode &status) {
    return MeasureUnit::create(0, 1, status);
}

MeasureUnit *MeasureUnit::createArcMinute(UErrorCode &status) {
    return MeasureUnit::create(1, 0, status);
}

MeasureUnit *MeasureUnit::createArcSecond(UErrorCode &status) {
    return MeasureUnit::create(1, 1, status);
}

MeasureUnit *MeasureUnit::createDegree(UErrorCode &status) {
    return MeasureUnit::create(1, 2, status);
}

MeasureUnit *MeasureUnit::createRadian(UErrorCode &status) {
    return MeasureUnit::create(1, 3, status);
}

MeasureUnit *MeasureUnit::createAcre(UErrorCode &status) {
    return MeasureUnit::create(2, 0, status);
}

MeasureUnit *MeasureUnit::createHectare(UErrorCode &status) {
    return MeasureUnit::create(2, 1, status);
}

MeasureUnit *MeasureUnit::createSquareCentimeter(UErrorCode &status) {
    return MeasureUnit::create(2, 2, status);
}

MeasureUnit *MeasureUnit::createSquareFoot(UErrorCode &status) {
    return MeasureUnit::create(2, 3, status);
}

MeasureUnit *MeasureUnit::createSquareInch(UErrorCode &status) {
    return MeasureUnit::create(2, 4, status);
}

MeasureUnit *MeasureUnit::createSquareKilometer(UErrorCode &status) {
    return MeasureUnit::create(2, 5, status);
}

MeasureUnit *MeasureUnit::createSquareMeter(UErrorCode &status) {
    return MeasureUnit::create(2, 6, status);
}

MeasureUnit *MeasureUnit::createSquareMile(UErrorCode &status) {
    return MeasureUnit::create(2, 7, status);
}

MeasureUnit *MeasureUnit::createSquareYard(UErrorCode &status) {
    return MeasureUnit::create(2, 8, status);
}

MeasureUnit *MeasureUnit::createLiterPerKilometer(UErrorCode &status) {
    return MeasureUnit::create(3, 0, status);
}

MeasureUnit *MeasureUnit::createMilePerGallon(UErrorCode &status) {
    return MeasureUnit::create(3, 1, status);
}

MeasureUnit *MeasureUnit::createBit(UErrorCode &status) {
    return MeasureUnit::create(5, 0, status);
}

MeasureUnit *MeasureUnit::createByte(UErrorCode &status) {
    return MeasureUnit::create(5, 1, status);
}

MeasureUnit *MeasureUnit::createGigabit(UErrorCode &status) {
    return MeasureUnit::create(5, 2, status);
}

MeasureUnit *MeasureUnit::createGigabyte(UErrorCode &status) {
    return MeasureUnit::create(5, 3, status);
}

MeasureUnit *MeasureUnit::createKilobit(UErrorCode &status) {
    return MeasureUnit::create(5, 4, status);
}

MeasureUnit *MeasureUnit::createKilobyte(UErrorCode &status) {
    return MeasureUnit::create(5, 5, status);
}

MeasureUnit *MeasureUnit::createMegabit(UErrorCode &status) {
    return MeasureUnit::create(5, 6, status);
}

MeasureUnit *MeasureUnit::createMegabyte(UErrorCode &status) {
    return MeasureUnit::create(5, 7, status);
}

MeasureUnit *MeasureUnit::createTerabit(UErrorCode &status) {
    return MeasureUnit::create(5, 8, status);
}

MeasureUnit *MeasureUnit::createTerabyte(UErrorCode &status) {
    return MeasureUnit::create(5, 9, status);
}

MeasureUnit *MeasureUnit::createDay(UErrorCode &status) {
    return MeasureUnit::create(6, 0, status);
}

MeasureUnit *MeasureUnit::createHour(UErrorCode &status) {
    return MeasureUnit::create(6, 1, status);
}

MeasureUnit *MeasureUnit::createMicrosecond(UErrorCode &status) {
    return MeasureUnit::create(6, 2, status);
}

MeasureUnit *MeasureUnit::createMillisecond(UErrorCode &status) {
    return MeasureUnit::create(6, 3, status);
}

MeasureUnit *MeasureUnit::createMinute(UErrorCode &status) {
    return MeasureUnit::create(6, 4, status);
}

MeasureUnit *MeasureUnit::createMonth(UErrorCode &status) {
    return MeasureUnit::create(6, 5, status);
}

MeasureUnit *MeasureUnit::createNanosecond(UErrorCode &status) {
    return MeasureUnit::create(6, 6, status);
}

MeasureUnit *MeasureUnit::createSecond(UErrorCode &status) {
    return MeasureUnit::create(6, 7, status);
}

MeasureUnit *MeasureUnit::createWeek(UErrorCode &status) {
    return MeasureUnit::create(6, 8, status);
}

MeasureUnit *MeasureUnit::createYear(UErrorCode &status) {
    return MeasureUnit::create(6, 9, status);
}

MeasureUnit *MeasureUnit::createAmpere(UErrorCode &status) {
    return MeasureUnit::create(7, 0, status);
}

MeasureUnit *MeasureUnit::createMilliampere(UErrorCode &status) {
    return MeasureUnit::create(7, 1, status);
}

MeasureUnit *MeasureUnit::createOhm(UErrorCode &status) {
    return MeasureUnit::create(7, 2, status);
}

MeasureUnit *MeasureUnit::createVolt(UErrorCode &status) {
    return MeasureUnit::create(7, 3, status);
}

MeasureUnit *MeasureUnit::createCalorie(UErrorCode &status) {
    return MeasureUnit::create(8, 0, status);
}

MeasureUnit *MeasureUnit::createFoodcalorie(UErrorCode &status) {
    return MeasureUnit::create(8, 1, status);
}

MeasureUnit *MeasureUnit::createJoule(UErrorCode &status) {
    return MeasureUnit::create(8, 2, status);
}

MeasureUnit *MeasureUnit::createKilocalorie(UErrorCode &status) {
    return MeasureUnit::create(8, 3, status);
}

MeasureUnit *MeasureUnit::createKilojoule(UErrorCode &status) {
    return MeasureUnit::create(8, 4, status);
}

MeasureUnit *MeasureUnit::createKilowattHour(UErrorCode &status) {
    return MeasureUnit::create(8, 5, status);
}

MeasureUnit *MeasureUnit::createGigahertz(UErrorCode &status) {
    return MeasureUnit::create(9, 0, status);
}

MeasureUnit *MeasureUnit::createHertz(UErrorCode &status) {
    return MeasureUnit::create(9, 1, status);
}

MeasureUnit *MeasureUnit::createKilohertz(UErrorCode &status) {
    return MeasureUnit::create(9, 2, status);
}

MeasureUnit *MeasureUnit::createMegahertz(UErrorCode &status) {
    return MeasureUnit::create(9, 3, status);
}

MeasureUnit *MeasureUnit::createAstronomicalUnit(UErrorCode &status) {
    return MeasureUnit::create(10, 0, status);
}

MeasureUnit *MeasureUnit::createCentimeter(UErrorCode &status) {
    return MeasureUnit::create(10, 1, status);
}

MeasureUnit *MeasureUnit::createDecimeter(UErrorCode &status) {
    return MeasureUnit::create(10, 2, status);
}

MeasureUnit *MeasureUnit::createFathom(UErrorCode &status) {
    return MeasureUnit::create(10, 3, status);
}

MeasureUnit *MeasureUnit::createFoot(UErrorCode &status) {
    return MeasureUnit::create(10, 4, status);
}

MeasureUnit *MeasureUnit::createFurlong(UErrorCode &status) {
    return MeasureUnit::create(10, 5, status);
}

MeasureUnit *MeasureUnit::createInch(UErrorCode &status) {
    return MeasureUnit::create(10, 6, status);
}

MeasureUnit *MeasureUnit::createKilometer(UErrorCode &status) {
    return MeasureUnit::create(10, 7, status);
}

MeasureUnit *MeasureUnit::createLightYear(UErrorCode &status) {
    return MeasureUnit::create(10, 8, status);
}

MeasureUnit *MeasureUnit::createMeter(UErrorCode &status) {
    return MeasureUnit::create(10, 9, status);
}

MeasureUnit *MeasureUnit::createMicrometer(UErrorCode &status) {
    return MeasureUnit::create(10, 10, status);
}

MeasureUnit *MeasureUnit::createMile(UErrorCode &status) {
    return MeasureUnit::create(10, 11, status);
}

MeasureUnit *MeasureUnit::createMillimeter(UErrorCode &status) {
    return MeasureUnit::create(10, 12, status);
}

MeasureUnit *MeasureUnit::createNanometer(UErrorCode &status) {
    return MeasureUnit::create(10, 13, status);
}

MeasureUnit *MeasureUnit::createNauticalMile(UErrorCode &status) {
    return MeasureUnit::create(10, 14, status);
}

MeasureUnit *MeasureUnit::createParsec(UErrorCode &status) {
    return MeasureUnit::create(10, 15, status);
}

MeasureUnit *MeasureUnit::createPicometer(UErrorCode &status) {
    return MeasureUnit::create(10, 16, status);
}

MeasureUnit *MeasureUnit::createYard(UErrorCode &status) {
    return MeasureUnit::create(10, 17, status);
}

MeasureUnit *MeasureUnit::createLux(UErrorCode &status) {
    return MeasureUnit::create(11, 0, status);
}

MeasureUnit *MeasureUnit::createCarat(UErrorCode &status) {
    return MeasureUnit::create(12, 0, status);
}

MeasureUnit *MeasureUnit::createGram(UErrorCode &status) {
    return MeasureUnit::create(12, 1, status);
}

MeasureUnit *MeasureUnit::createKilogram(UErrorCode &status) {
    return MeasureUnit::create(12, 2, status);
}

MeasureUnit *MeasureUnit::createMetricTon(UErrorCode &status) {
    return MeasureUnit::create(12, 3, status);
}

MeasureUnit *MeasureUnit::createMicrogram(UErrorCode &status) {
    return MeasureUnit::create(12, 4, status);
}

MeasureUnit *MeasureUnit::createMilligram(UErrorCode &status) {
    return MeasureUnit::create(12, 5, status);
}

MeasureUnit *MeasureUnit::createOunce(UErrorCode &status) {
    return MeasureUnit::create(12, 6, status);
}

MeasureUnit *MeasureUnit::createOunceTroy(UErrorCode &status) {
    return MeasureUnit::create(12, 7, status);
}

MeasureUnit *MeasureUnit::createPound(UErrorCode &status) {
    return MeasureUnit::create(12, 8, status);
}

MeasureUnit *MeasureUnit::createStone(UErrorCode &status) {
    return MeasureUnit::create(12, 9, status);
}

MeasureUnit *MeasureUnit::createTon(UErrorCode &status) {
    return MeasureUnit::create(12, 10, status);
}

MeasureUnit *MeasureUnit::createGigawatt(UErrorCode &status) {
    return MeasureUnit::create(13, 0, status);
}

MeasureUnit *MeasureUnit::createHorsepower(UErrorCode &status) {
    return MeasureUnit::create(13, 1, status);
}

MeasureUnit *MeasureUnit::createKilowatt(UErrorCode &status) {
    return MeasureUnit::create(13, 2, status);
}

MeasureUnit *MeasureUnit::createMegawatt(UErrorCode &status) {
    return MeasureUnit::create(13, 3, status);
}

MeasureUnit *MeasureUnit::createMilliwatt(UErrorCode &status) {
    return MeasureUnit::create(13, 4, status);
}

MeasureUnit *MeasureUnit::createWatt(UErrorCode &status) {
    return MeasureUnit::create(13, 5, status);
}

MeasureUnit *MeasureUnit::createHectopascal(UErrorCode &status) {
    return MeasureUnit::create(14, 0, status);
}

MeasureUnit *MeasureUnit::createInchHg(UErrorCode &status) {
    return MeasureUnit::create(14, 1, status);
}

MeasureUnit *MeasureUnit::createMillibar(UErrorCode &status) {
    return MeasureUnit::create(14, 2, status);
}

MeasureUnit *MeasureUnit::createMillimeterOfMercury(UErrorCode &status) {
    return MeasureUnit::create(14, 3, status);
}

MeasureUnit *MeasureUnit::createPoundPerSquareInch(UErrorCode &status) {
    return MeasureUnit::create(14, 4, status);
}

MeasureUnit *MeasureUnit::createKarat(UErrorCode &status) {
    return MeasureUnit::create(15, 0, status);
}

MeasureUnit *MeasureUnit::createKilometerPerHour(UErrorCode &status) {
    return MeasureUnit::create(16, 0, status);
}

MeasureUnit *MeasureUnit::createMeterPerSecond(UErrorCode &status) {
    return MeasureUnit::create(16, 1, status);
}

MeasureUnit *MeasureUnit::createMilePerHour(UErrorCode &status) {
    return MeasureUnit::create(16, 2, status);
}

MeasureUnit *MeasureUnit::createCelsius(UErrorCode &status) {
    return MeasureUnit::create(17, 0, status);
}

MeasureUnit *MeasureUnit::createFahrenheit(UErrorCode &status) {
    return MeasureUnit::create(17, 1, status);
}

MeasureUnit *MeasureUnit::createKelvin(UErrorCode &status) {
    return MeasureUnit::create(17, 2, status);
}

MeasureUnit *MeasureUnit::createAcreFoot(UErrorCode &status) {
    return MeasureUnit::create(18, 0, status);
}

MeasureUnit *MeasureUnit::createBushel(UErrorCode &status) {
    return MeasureUnit::create(18, 1, status);
}

MeasureUnit *MeasureUnit::createCentiliter(UErrorCode &status) {
    return MeasureUnit::create(18, 2, status);
}

MeasureUnit *MeasureUnit::createCubicCentimeter(UErrorCode &status) {
    return MeasureUnit::create(18, 3, status);
}

MeasureUnit *MeasureUnit::createCubicFoot(UErrorCode &status) {
    return MeasureUnit::create(18, 4, status);
}

MeasureUnit *MeasureUnit::createCubicInch(UErrorCode &status) {
    return MeasureUnit::create(18, 5, status);
}

MeasureUnit *MeasureUnit::createCubicKilometer(UErrorCode &status) {
    return MeasureUnit::create(18, 6, status);
}

MeasureUnit *MeasureUnit::createCubicMeter(UErrorCode &status) {
    return MeasureUnit::create(18, 7, status);
}

MeasureUnit *MeasureUnit::createCubicMile(UErrorCode &status) {
    return MeasureUnit::create(18, 8, status);
}

MeasureUnit *MeasureUnit::createCubicYard(UErrorCode &status) {
    return MeasureUnit::create(18, 9, status);
}

MeasureUnit *MeasureUnit::createCup(UErrorCode &status) {
    return MeasureUnit::create(18, 10, status);
}

MeasureUnit *MeasureUnit::createDeciliter(UErrorCode &status) {
    return MeasureUnit::create(18, 11, status);
}

MeasureUnit *MeasureUnit::createFluidOunce(UErrorCode &status) {
    return MeasureUnit::create(18, 12, status);
}

MeasureUnit *MeasureUnit::createGallon(UErrorCode &status) {
    return MeasureUnit::create(18, 13, status);
}

MeasureUnit *MeasureUnit::createHectoliter(UErrorCode &status) {
    return MeasureUnit::create(18, 14, status);
}

MeasureUnit *MeasureUnit::createLiter(UErrorCode &status) {
    return MeasureUnit::create(18, 15, status);
}

MeasureUnit *MeasureUnit::createMegaliter(UErrorCode &status) {
    return MeasureUnit::create(18, 16, status);
}

MeasureUnit *MeasureUnit::createMilliliter(UErrorCode &status) {
    return MeasureUnit::create(18, 17, status);
}

MeasureUnit *MeasureUnit::createPint(UErrorCode &status) {
    return MeasureUnit::create(18, 18, status);
}

MeasureUnit *MeasureUnit::createQuart(UErrorCode &status) {
    return MeasureUnit::create(18, 19, status);
}

MeasureUnit *MeasureUnit::createTablespoon(UErrorCode &status) {
    return MeasureUnit::create(18, 20, status);
}

MeasureUnit *MeasureUnit::createTeaspoon(UErrorCode &status) {
    return MeasureUnit::create(18, 21, status);
}

// End generated code

static int32_t binarySearch(
        const char * const * array, int32_t start, int32_t end, const char * key) {
    while (start < end) {
        int32_t mid = (start + end) / 2;
        int32_t cmp = uprv_strcmp(array[mid], key);
        if (cmp < 0) {
            start = mid + 1;
            continue;
        }
        if (cmp == 0) {
            return mid;
        }
        end = mid;
    }
    return -1;
}
    
MeasureUnit::MeasureUnit(const MeasureUnit &other)
        : fTypeId(other.fTypeId), fSubTypeId(other.fSubTypeId) {
    uprv_strcpy(fCurrency, other.fCurrency);
}

MeasureUnit &MeasureUnit::operator=(const MeasureUnit &other) {
    if (this == &other) {
        return *this;
    }
    fTypeId = other.fTypeId;
    fSubTypeId = other.fSubTypeId;
    uprv_strcpy(fCurrency, other.fCurrency);
    return *this;
}

UObject *MeasureUnit::clone() const {
    return new MeasureUnit(*this);
}

MeasureUnit::~MeasureUnit() {
}

const char *MeasureUnit::getType() const {
    return gTypes[fTypeId];
}

const char *MeasureUnit::getSubtype() const {
    return fCurrency[0] == 0 ? gSubTypes[getOffset()] : fCurrency;
}

UBool MeasureUnit::operator==(const UObject& other) const {
    if (this == &other) {  // Same object, equal
        return TRUE;
    }
    if (typeid(*this) != typeid(other)) { // Different types, not equal
        return FALSE;
    }
    const MeasureUnit &rhs = static_cast<const MeasureUnit&>(other);
    return (
            fTypeId == rhs.fTypeId
            && fSubTypeId == rhs.fSubTypeId
            && uprv_strcmp(fCurrency, rhs.fCurrency) == 0);
}

int32_t MeasureUnit::getIndex() const {
    return gIndexes[fTypeId] + fSubTypeId;
}

int32_t MeasureUnit::getAvailable(
        MeasureUnit *dest,
        int32_t destCapacity,
        UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) {
        return 0;
    }
    if (destCapacity < UPRV_LENGTHOF(gSubTypes)) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return UPRV_LENGTHOF(gSubTypes);
    }
    int32_t idx = 0;
    for (int32_t typeIdx = 0; typeIdx < UPRV_LENGTHOF(gTypes); ++typeIdx) {
        int32_t len = gOffsets[typeIdx + 1] - gOffsets[typeIdx];
        for (int32_t subTypeIdx = 0; subTypeIdx < len; ++subTypeIdx) {
            dest[idx].setTo(typeIdx, subTypeIdx);
            ++idx;
        }
    }
    U_ASSERT(idx == UPRV_LENGTHOF(gSubTypes));
    return UPRV_LENGTHOF(gSubTypes);
}

int32_t MeasureUnit::getAvailable(
        const char *type,
        MeasureUnit *dest,
        int32_t destCapacity,
        UErrorCode &errorCode) {
    if (U_FAILURE(errorCode)) {
        return 0;
    }
    int32_t typeIdx = binarySearch(gTypes, 0, UPRV_LENGTHOF(gTypes), type);
    if (typeIdx == -1) {
        return 0;
    }
    int32_t len = gOffsets[typeIdx + 1] - gOffsets[typeIdx];
    if (destCapacity < len) {
        errorCode = U_BUFFER_OVERFLOW_ERROR;
        return len;
    }
    for (int subTypeIdx = 0; subTypeIdx < len; ++subTypeIdx) {
        dest[subTypeIdx].setTo(typeIdx, subTypeIdx);
    }
    return len;
}

StringEnumeration* MeasureUnit::getAvailableTypes(UErrorCode &errorCode) {
    UEnumeration *uenum = uenum_openCharStringsEnumeration(
            gTypes, UPRV_LENGTHOF(gTypes), &errorCode);
    if (U_FAILURE(errorCode)) {
        uenum_close(uenum);
        return NULL;
    }
    StringEnumeration *result = new UStringEnumeration(uenum);
    if (result == NULL) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
        uenum_close(uenum);
        return NULL;
    }
    return result;
}

int32_t MeasureUnit::getIndexCount() {
    return gIndexes[UPRV_LENGTHOF(gIndexes) - 1];
}

MeasureUnit *MeasureUnit::create(int typeId, int subTypeId, UErrorCode &status) {
    if (U_FAILURE(status)) {
        return NULL;
    }
    MeasureUnit *result = new MeasureUnit(typeId, subTypeId);
    if (result == NULL) {
        status = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

void MeasureUnit::initTime(const char *timeId) {
    int32_t result = binarySearch(gTypes, 0, UPRV_LENGTHOF(gTypes), "duration");
    U_ASSERT(result != -1);
    fTypeId = result;
    result = binarySearch(gSubTypes, gOffsets[fTypeId], gOffsets[fTypeId + 1], timeId);
    U_ASSERT(result != -1);
    fSubTypeId = result - gOffsets[fTypeId]; 
}

void MeasureUnit::initCurrency(const char *isoCurrency) {
    int32_t result = binarySearch(gTypes, 0, UPRV_LENGTHOF(gTypes), "currency");
    U_ASSERT(result != -1);
    fTypeId = result;
    result = binarySearch(
            gSubTypes, gOffsets[fTypeId], gOffsets[fTypeId + 1], isoCurrency);
    if (result != -1) {
        fSubTypeId = result - gOffsets[fTypeId];
    } else {
        uprv_strncpy(fCurrency, isoCurrency, UPRV_LENGTHOF(fCurrency));
    }
}

void MeasureUnit::setTo(int32_t typeId, int32_t subTypeId) {
    fTypeId = typeId;
    fSubTypeId = subTypeId;
    fCurrency[0] = 0;
}

int32_t MeasureUnit::getOffset() const {
    return gOffsets[fTypeId] + fSubTypeId;
}

U_NAMESPACE_END

#endif /* !UNCONFIG_NO_FORMATTING */
