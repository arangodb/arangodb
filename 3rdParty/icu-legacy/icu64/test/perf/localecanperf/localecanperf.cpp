/*
***********************************************************************
* © 2020 and later: Unicode, Inc. and others.
* License & terms of use: http://www.unicode.org/copyright.html
***********************************************************************
*/

#include <algorithm>
#include <vector>
#include <string>

#include "unicode/locid.h"
#include "unicode/uperf.h"

//
// Test case ...
//
class LocaleCreateCanonical : public UPerfFunction {
public:
    LocaleCreateCanonical() {
        testCases.emplace_back("en");
        testCases.emplace_back("en-US");
        testCases.emplace_back("ja-JP");
        testCases.emplace_back("zh-Hant-CN");
        testCases.emplace_back("hy-SU");
    }
    ~LocaleCreateCanonical() {  }
    virtual void call(UErrorCode* /* status */)
    {
        std::for_each(testCases.begin(), testCases.end(),
                      [](const std::string& s)
                      {
                          Locale l = Locale::createCanonical(s.c_str());
                      });
    }
    virtual long getOperationsPerIteration() { return testCases.size(); }
    virtual long getEventsPerIteration() { return testCases.size(); }
private:
    std::vector<std::string> testCases;
};

class LocaleCanonicalizationPerfTest : public UPerfTest
{
public:
    LocaleCanonicalizationPerfTest(
        int32_t argc, const char *argv[], UErrorCode &status)
            : UPerfTest(argc, argv, nullptr, 0, "localecanperf", status)
    {
    }

    ~LocaleCanonicalizationPerfTest()
    {
    }
    virtual UPerfFunction* runIndexedTest(
        int32_t index, UBool exec, const char *&name, char *par = nullptr);

private:
    UPerfFunction* TestLocaleCreateCanonical()
    {
        return new LocaleCreateCanonical();
    }
};

UPerfFunction*
LocaleCanonicalizationPerfTest::runIndexedTest(
    int32_t index, UBool exec, const char *&name, char *par /*= nullptr*/)
{
    (void)par;
    TESTCASE_AUTO_BEGIN;

    TESTCASE_AUTO(TestLocaleCreateCanonical);

    TESTCASE_AUTO_END;
    return nullptr;
}

int main(int argc, const char *argv[])
{
    UErrorCode status = U_ZERO_ERROR;
    LocaleCanonicalizationPerfTest test(argc, argv, status);

    if (U_FAILURE(status)){
        fprintf(stderr, "The error is %s\n", u_errorName(status));
        test.usage();
        return status;
    }

    if (test.run() == false){
        test.usage();
        fprintf(stderr, "FAILED: Tests could not be run please check the arguments.\n");
        return -1;
    }
    return 0;
}
