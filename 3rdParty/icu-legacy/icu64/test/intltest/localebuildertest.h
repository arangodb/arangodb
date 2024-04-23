// © 2018 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "intltest.h"
#include "unicode/localebuilder.h"


/**
 * Tests for the LocaleBuilder class
 **/
class LocaleBuilderTest: public IntlTest {
 public:
    LocaleBuilderTest();
    virtual ~LocaleBuilderTest();

    void runIndexedTest( int32_t index, UBool exec, const char* &name, char* par = NULL );

    void TestAddRemoveUnicodeLocaleAttribute(void);
    void TestAddRemoveUnicodeLocaleAttributeWellFormed(void);
    void TestAddUnicodeLocaleAttributeIllFormed(void);
    void TestLocaleBuilder(void);
    void TestLocaleBuilderBasic(void);
    void TestPosixCases(void);
    void TestSetExtensionOthers(void);
    void TestSetExtensionPU(void);
    void TestSetExtensionT(void);
    void TestSetExtensionU(void);
    void TestSetExtensionValidateOthersIllFormed(void);
    void TestSetExtensionValidateOthersWellFormed(void);
    void TestSetExtensionValidatePUIllFormed(void);
    void TestSetExtensionValidatePUWellFormed(void);
    void TestSetExtensionValidateTIllFormed(void);
    void TestSetExtensionValidateTWellFormed(void);
    void TestSetExtensionValidateUIllFormed(void);
    void TestSetExtensionValidateUWellFormed(void);
    void TestSetLanguageIllFormed(void);
    void TestSetLanguageWellFormed(void);
    void TestSetLocale(void);
    void TestSetRegionIllFormed(void);
    void TestSetRegionWellFormed(void);
    void TestSetScriptIllFormed(void);
    void TestSetScriptWellFormed(void);
    void TestSetUnicodeLocaleKeywordIllFormedKey(void);
    void TestSetUnicodeLocaleKeywordIllFormedValue(void);
    void TestSetUnicodeLocaleKeywordWellFormed(void);
    void TestSetVariantIllFormed(void);
    void TestSetVariantWellFormed(void);

 private:
    void Verify(LocaleBuilder& bld, const char* expected, const char* msg);
};
