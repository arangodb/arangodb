// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "unicode/calendar.h"
#include "unicode/localpointer.h"
#include "unicode/unistr.h"
#include "unicode/timezone.h"
#include "erarules.h"
#include "erarulestest.h"

void EraRulesTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/)
{
    if (exec) {
        logln("TestSuite EraRulesTest");
    }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(testAPIs);
    TESTCASE_AUTO(testJapanese);
    TESTCASE_AUTO_END;
}

void EraRulesTest::testAPIs() {
    const char * calTypes[] = {
        "gregorian",
        //"iso8601",
        "buddhist",
        "chinese",
        "coptic",
        "dangi",
        "ethiopic",
        "ethiopic-amete-alem",
        "hebrew",
        "indian",
        "islamic",
        "islamic-civil",
        "islamic-rgsa",
        "islamic-tbla",
        "islamic-umalqura",
        "japanese",
        "persian",
        "roc",
        //"unknown",
        nullptr
    };

    for (int32_t i = 0; calTypes[i] != nullptr; i++) {
        UErrorCode status = U_ZERO_ERROR;
        const char *calId = calTypes[i];

        LocalPointer<EraRules> rules1(EraRules::createInstance(calId, false, status));
        if (U_FAILURE(status)) {
            errln(UnicodeString("Era rules for ") + calId + " is not available.");
            continue;
        }

        LocalPointer<EraRules> rules2(EraRules::createInstance(calId, true, status));
        if (U_FAILURE(status)) {
            errln(UnicodeString("Era rules for ") + calId + " (including tentative eras) is not available.");
            continue;
        }

        int32_t numEras1 = rules1->getNumberOfEras();
        if (numEras1 <= 0) {
            errln(UnicodeString("Number of era rules for ") + calId + " is " + numEras1);
        }

        int32_t numEras2 = rules2->getNumberOfEras();
        if (numEras2 < numEras1) {
            errln(UnicodeString("Number of era including tentative eras is fewer than one without tentative eras in calendar: ")
                    + calId);
        }

        LocalPointer<Calendar> cal(Calendar::createInstance(*TimeZone::getGMT(), "en", status));
        if (U_FAILURE(status)) {
            errln("Failed to create a Calendar instance.");
            continue;
        }
        int32_t currentIdx = rules1->getCurrentEraIndex();
        int32_t currentYear = cal->get(UCAL_YEAR, status);
        int32_t idx = rules1->getEraIndex(
                currentYear, cal->get(UCAL_MONTH, status) + 1,
                cal->get(UCAL_DATE, status), status);
        if (U_FAILURE(status)) {
            errln("Error while getting index of era.");
            continue;
        }
        if (idx != currentIdx) {
            errln(UnicodeString("Current era index:") + currentIdx + " is different from era index of now:" + idx
                    + " in calendar:" + calId);
        }

        int32_t eraStartYear = rules1->getStartYear(currentIdx, status);
        if (U_FAILURE(status)) {
            errln(UnicodeString("Failed to get the start year of era index: ") + currentIdx + " in calendar: " + calId);
        }
        if (currentYear < eraStartYear) {
            errln(UnicodeString("Current era's start year is after the current year in calendar:") + calId);
        }
    }
}

void EraRulesTest::testJapanese() {
    const int32_t HEISEI = 235; // ICU4C does not define constants for eras

    UErrorCode status = U_ZERO_ERROR;
    LocalPointer<EraRules> rules(EraRules::createInstance("japanese", true, status));
    if (U_FAILURE(status)) {
        errln("Failed to get era rules for Japanese calendar.");
        return;
    }
    // Rules should have an era after Heisei
    int32_t numRules = rules->getNumberOfEras();
    if (numRules <= HEISEI) {
        errln("Era after Heisei is not available.");
        return;
    }
    int postHeiseiStartYear = rules->getStartYear(HEISEI + 1, status);
    if (U_FAILURE(status)) {
        errln("Failed to get the start year of era after Heisei.");
    }
    if (postHeiseiStartYear != 2019) {
        errln(UnicodeString("Era after Heisei should start in 2019, but got ") + postHeiseiStartYear);
    }
}

#endif /* #if !UCONFIG_NO_FORMATTING */

