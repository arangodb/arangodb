// © 2020 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html#License

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#include "intltest.h"
#include "unicode/unistr.h"
#include "units_router.h"


class UnitsRouterTest : public IntlTest {
  public:
    UnitsRouterTest() {}

    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par = nullptr) override;

    void testBasic();
};

extern IntlTest *createUnitsRouterTest() { return new UnitsRouterTest(); }

void UnitsRouterTest::runIndexedTest(int32_t index, UBool exec, const char *&name, char * /*par*/) {
    if (exec) { logln("TestSuite UnitsRouterTest: "); }
    TESTCASE_AUTO_BEGIN;
    TESTCASE_AUTO(testBasic);
    TESTCASE_AUTO_END;
}

void UnitsRouterTest::testBasic() { IcuTestErrorCode status(*this, "UnitsRouter testBasic"); }

#endif /* #if !UCONFIG_NO_FORMATTING */
