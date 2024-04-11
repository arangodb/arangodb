// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#ifndef _TESTMESSAGEFORMAT2
#define _TESTMESSAGEFORMAT2

#include "unicode/rep.h"
#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "messageformat2test_utils.h"
#include "unicode/unistr.h"
#include "unicode/messageformat2_formattable.h"
#include "unicode/parseerr.h"
#include "intltest.h"

/**
 * TestMessageFormat2 tests MessageFormat2
 */

struct TestResult {
    const UnicodeString pattern;
    const UnicodeString output;
};

struct TestResultError {
    const UnicodeString pattern;
    const UnicodeString output;
    UErrorCode expected;
};

class TestMessageFormat2: public IntlTest {
public:
    void runIndexedTest( int32_t index, UBool exec, const char* &name, char* par = NULL ) override;

    /**
     * test MessageFormat2 with various given patterns
     **/
    void testVariousPatterns(void);
    void featureTests(void);
    void messageFormat1Tests(void);
    void testAPICustomFunctions(void);
    // Test custom functions
    void testCustomFunctions(void);
    // Test standard functions
    void testBuiltInFunctions(void);
    void testDataModelErrors(void);
    void testResolutionErrors(void);
    // Test the data model API
    void testDataModelAPI(void);
    void testAPI(void);
    void testInvalidPatterns(void);
    void testAPISimple(void);

private:

    void testSemanticallyInvalidPattern(uint32_t, const UnicodeString&, UErrorCode);
    void testRuntimeErrorPattern(uint32_t, const UnicodeString&, UErrorCode);
    void testRuntimeWarningPattern(uint32_t, const UnicodeString&, const UnicodeString&, UErrorCode);
    void testInvalidPattern(uint32_t, const UnicodeString&);
    void testInvalidPattern(uint32_t, const UnicodeString&, uint32_t);
    void testInvalidPattern(uint32_t, const UnicodeString&, uint32_t, uint32_t);
    void testValidPatterns(const TestResult*, int32_t, IcuTestErrorCode&);
    void testResolutionErrors(IcuTestErrorCode&);
    void testNoSyntaxErrors(const UnicodeString*, int32_t, IcuTestErrorCode&);
    void jsonTests(IcuTestErrorCode&);
    void specTests();
    void runSpecTests(IcuTestErrorCode&);

    // Built-in function testing
    void testDateTime(IcuTestErrorCode&);
    void testNumbers(IcuTestErrorCode&);

    // Custom function testing
    void testPersonFormatter(IcuTestErrorCode&);
    void testCustomFunctionsComplexMessage(IcuTestErrorCode&);
    void testGrammarCasesFormatter(IcuTestErrorCode&);
    void testListFormatter(IcuTestErrorCode&);
    void testMessageRefFormatter(IcuTestErrorCode&);

    // Feature tests
    void testEmptyMessage(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testPlainText(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testPlaceholders(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testArgumentMissing(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testDefaultLocale(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testSpecialPluralWithDecimals(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testDefaultFunctionAndOptions(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testSimpleSelection(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testComplexSelection(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testSimpleLocalVariable(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testLocalVariableWithSelect(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testDateFormat(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testPlural(message2::TestCase::Builder&, IcuTestErrorCode&);

    void testPluralOrdinal(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testDeclareBeforeUse(message2::TestCase::Builder&, IcuTestErrorCode&);

    // MessageFormat 1 tests
    void testSample(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testStaticFormat(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testSimpleFormat(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testSelectFormatToPattern(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testMessageFormatDateTimeSkeleton(message2::TestCase::Builder&, IcuTestErrorCode&);
    void testMf1Behavior(message2::TestCase::Builder&, IcuTestErrorCode&);

}; // class TestMessageFormat2

U_NAMESPACE_BEGIN

namespace message2 {

// Custom function classes
class PersonNameFormatterFactory : public FormatterFactory {

    public:
    Formatter* createFormatter(const Locale&, UErrorCode&) override;
};

class Person : public FormattableObject {
    public:
    UnicodeString title;
    UnicodeString firstName;
    UnicodeString lastName;
    Person(UnicodeString t, UnicodeString f, UnicodeString l) : title(t), firstName(f), lastName(l), tagName("person") {}
    ~Person();
    const UnicodeString& tag() const override { return tagName; }
    private:
    const UnicodeString tagName;
};

class PersonNameFormatter : public Formatter {
    public:
    FormattedPlaceholder format(FormattedPlaceholder&&, FunctionOptions&& opts, UErrorCode& errorCode) const override;
};

class FormattableProperties : public FormattableObject {
    public:
    const UnicodeString& tag() const override { return tagName; }
    FormattableProperties(Hashtable* hash) : properties(hash), tagName("properties") {
        U_ASSERT(hash != nullptr);
    }
    ~FormattableProperties();
    LocalPointer<Hashtable> properties;
private:
    const UnicodeString tagName;
};

class GrammarCasesFormatterFactory : public FormatterFactory {
    public:
    Formatter* createFormatter(const Locale&, UErrorCode&) override;
};

class GrammarCasesFormatter : public Formatter {
    public:
    FormattedPlaceholder format(FormattedPlaceholder&&, FunctionOptions&& opts, UErrorCode& errorCode) const override;
    static MFFunctionRegistry customRegistry(UErrorCode&);
    private:
    void getDativeAndGenitive(const UnicodeString&, UnicodeString& result) const;
};

class ListFormatterFactory : public FormatterFactory {
    public:
    Formatter* createFormatter(const Locale&, UErrorCode&) override;
};

class ListFormatter : public Formatter {
    public:
    FormattedPlaceholder format(FormattedPlaceholder&&, FunctionOptions&& opts, UErrorCode& errorCode) const override;
    static MFFunctionRegistry customRegistry(UErrorCode&);
    private:
    friend class ListFormatterFactory;
    const Locale& locale;
    ListFormatter(const Locale& loc) : locale(loc) {}
};

class ResourceManagerFactory : public FormatterFactory {
    public:
    Formatter* createFormatter(const Locale&, UErrorCode&) override;
};

class ResourceManager : public Formatter {
    public:
    FormattedPlaceholder format(FormattedPlaceholder&&, FunctionOptions&& opts, UErrorCode& errorCode) const override;
    static MFFunctionRegistry customRegistry(UErrorCode&);
    static Hashtable* properties(UErrorCode&);
    static UnicodeString propertiesAsString(const Hashtable&);
    static Hashtable* parseProperties(const UnicodeString&, UErrorCode&);

    private:
    friend class ResourceManagerFactory;
    ResourceManager(const Locale& loc) : locale(loc) {}
    const Locale& locale;
};

} // namespace message2
U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif
