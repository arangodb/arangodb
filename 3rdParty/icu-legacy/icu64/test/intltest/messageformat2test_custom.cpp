// © 2024 and later: Unicode, Inc. and others.

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "plurrule_impl.h"

#include "unicode/listformatter.h"
#include "messageformat2test.h"
#include "hash.h"
#include "intltest.h"


using namespace message2;
using namespace pluralimpl;

/*
Tests reflect the syntax specified in

  https://github.com/unicode-org/message-format-wg/commits/main/spec/message.abnf

as of the following commit from 2023-05-09:
  https://github.com/unicode-org/message-format-wg/commit/194f6efcec5bf396df36a19bd6fa78d1fa2e0867
*/

using namespace data_model;

void TestMessageFormat2::testPersonFormatter(IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    MFFunctionRegistry customRegistry(MFFunctionRegistry::Builder(errorCode)
                                      .adoptFormatter(FunctionName("person"), new PersonNameFormatterFactory(), errorCode)
                                      .build());
    UnicodeString name = "name";
    LocalPointer<Person> person(new Person(UnicodeString("Mr."), UnicodeString("John"), UnicodeString("Doe")));
    TestCase::Builder testBuilder;
    testBuilder.setName("testPersonFormatter");
    testBuilder.setLocale(Locale("en"));

    TestCase test = testBuilder.setPattern("Hello {$name :person formality=formal}")
        .setArgument(name, person.getAlias())
        .setExpected("Hello {$name}")
        .setExpectedError(U_MF_UNKNOWN_FUNCTION_ERROR)
        .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("Hello {$name :person formality=informal}")
                                .setArgument(name, person.getAlias())
                                .setExpected("Hello {$name}")
                                .setExpectedError(U_MF_UNKNOWN_FUNCTION_ERROR)
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    testBuilder.setFunctionRegistry(&customRegistry);

    test = testBuilder.setPattern("Hello {$name :person formality=formal}")
                                .setArgument(name, person.getAlias())
                                .setExpected("Hello Mr. Doe")
                                .setExpectSuccess()
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("Hello {$name :person formality=informal}")
                                .setArgument(name, person.getAlias())
                                .setExpected("Hello John")
                                .setExpectSuccess()
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("Hello {$name :person formality=formal length=long}")
                                .setArgument(name, person.getAlias())
                                .setExpected("Hello Mr. John Doe")
                                .setExpectSuccess()
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("Hello {$name :person formality=formal length=medium}")
                                .setArgument(name, person.getAlias())
                                .setExpected("Hello John Doe")
                                .setExpectSuccess()
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setPattern("Hello {$name :person formality=formal length=short}")
                                .setArgument(name, person.getAlias())
                                .setExpected("Hello Mr. Doe")
                                .setExpectSuccess()
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

void TestMessageFormat2::testCustomFunctionsComplexMessage(IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    MFFunctionRegistry customRegistry(MFFunctionRegistry::Builder(errorCode)
                                      .adoptFormatter(FunctionName("person"), new PersonNameFormatterFactory(), errorCode)
                                      .build());
    UnicodeString host = "host";
    UnicodeString hostGender = "hostGender";
    UnicodeString guest = "guest";
    UnicodeString guestCount = "guestCount";

    LocalPointer<Person> jane(new Person(UnicodeString("Ms."), UnicodeString("Jane"), UnicodeString("Doe")));
    LocalPointer<Person> john(new Person(UnicodeString("Mr."), UnicodeString("John"), UnicodeString("Doe")));
    LocalPointer<Person> anonymous(new Person(UnicodeString("Mx."), UnicodeString("Anonymous"), UnicodeString("Doe")));

    if (!jane.isValid() || !john.isValid() || !anonymous.isValid()) {
       ((UErrorCode&) errorCode) = U_MEMORY_ALLOCATION_ERROR;
       return;
   }

    UnicodeString message = ".local $hostName = {$host :person length=long}\n\
                .local $guestName = {$guest :person length=long}\n\
                .input {$guestCount :number}\n\
                .match {$hostGender :string} {$guestCount :number}\n\
                 female 0 {{{$hostName} does not give a party.}}\n\
                 female 1 {{{$hostName} invites {$guestName} to her party.}}\n\
                 female 2 {{{$hostName} invites {$guestName} and one other person to her party.}}\n\
                 female * {{{$hostName} invites {$guestCount} people, including {$guestName}, to her party.}}\n\
                 male 0 {{{$hostName} does not give a party.}}\n\
                 male 1 {{{$hostName} invites {$guestName} to his party.}}\n\
                 male 2 {{{$hostName} invites {$guestName} and one other person to his party.}}\n\
                 male * {{{$hostName} invites {$guestCount} people, including {$guestName}, to his party.}}\n\
                 * 0 {{{$hostName} does not give a party.}}\n\
                 * 1 {{{$hostName} invites {$guestName} to their party.}}\n\
                 * 2 {{{$hostName} invites {$guestName} and one other person to their party.}}\n\
                 * * {{{$hostName} invites {$guestCount} people, including {$guestName}, to their party.}}";


    TestCase::Builder testBuilder;
    testBuilder.setName("testCustomFunctionsComplexMessage");
    testBuilder.setLocale(Locale("en"));
    testBuilder.setPattern(message);
    testBuilder.setFunctionRegistry(&customRegistry);

    TestCase test = testBuilder.setArgument(host, jane.getAlias())
        .setArgument(hostGender, "female")
        .setArgument(guest, john.getAlias())
        .setArgument(guestCount, (int64_t) 3)
        .setExpected("Ms. Jane Doe invites 3 people, including Mr. John Doe, to her party.")
        .setExpectSuccess()
        .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setArgument(host, jane.getAlias())
                                .setArgument(hostGender, "female")
                                .setArgument(guest, john.getAlias())
                                .setArgument(guestCount, (int64_t) 2)
                                .setExpected("Ms. Jane Doe invites Mr. John Doe and one other person to her party.")
                                .setExpectSuccess()
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setArgument(host, jane.getAlias())
                                .setArgument(hostGender, "female")
                                .setArgument(guest, john.getAlias())
                                .setArgument(guestCount, (int64_t) 1)
                                .setExpected("Ms. Jane Doe invites Mr. John Doe to her party.")
                                .setExpectSuccess()
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setArgument(host, john.getAlias())
                                .setArgument(hostGender, "male")
                                .setArgument(guest, jane.getAlias())
                                .setArgument(guestCount, (int64_t) 3)
                                .setExpected("Mr. John Doe invites 3 people, including Ms. Jane Doe, to his party.")
                                .setExpectSuccess()
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setArgument(host, anonymous.getAlias())
                                .setArgument(hostGender, "unknown")
                                .setArgument(guest, jane.getAlias())
                                .setArgument(guestCount, (int64_t) 2)
                                .setExpected("Mx. Anonymous Doe invites Ms. Jane Doe and one other person to their party.")
                                .setExpectSuccess()
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

void TestMessageFormat2::testCustomFunctions() {
  IcuTestErrorCode errorCode(*this, "testCustomFunctions");

  testPersonFormatter(errorCode);
  testCustomFunctionsComplexMessage(errorCode);
  testGrammarCasesFormatter(errorCode);
  testListFormatter(errorCode);
  testMessageRefFormatter(errorCode);
}


// -------------- Custom function implementations

Formatter* PersonNameFormatterFactory::createFormatter(const Locale& locale, UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return nullptr;
    }

    // Locale not used
    (void) locale;

    Formatter* result = new PersonNameFormatter();
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

message2::FormattedPlaceholder PersonNameFormatter::format(FormattedPlaceholder&& arg, FunctionOptions&& options, UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return {};
    }

    message2::FormattedPlaceholder errorVal = message2::FormattedPlaceholder("not a person");

    if (!arg.canFormat() || arg.asFormattable().getType() != UFMT_OBJECT) {
        return errorVal;
    }
    const Formattable& toFormat = arg.asFormattable();

    FunctionOptionsMap opt = options.getOptions();
    bool hasFormality = opt.count("formality") > 0 && opt["formality"].getType() == UFMT_STRING;
    bool hasLength = opt.count("length") > 0 && opt["length"].getType() == UFMT_STRING;

    bool useFormal = hasFormality && opt["formality"].getString(errorCode) == "formal";
    UnicodeString length = hasLength ? opt["length"].getString(errorCode) : "short";

    const FormattableObject* fp = toFormat.getObject(errorCode);
    U_ASSERT(U_SUCCESS(errorCode));

    if (fp == nullptr || fp->tag() != u"person") {
        return errorVal;
    }
    const Person* p = static_cast<const Person*>(fp);

    UnicodeString title = p->title;
    UnicodeString firstName = p->firstName;
    UnicodeString lastName = p->lastName;

    UnicodeString result;
    if (length == "long") {
        result += title;
        result += " ";
        result += firstName;
        result += " ";
        result += lastName;
    } else if (length == "medium") {
        if (useFormal) {
            result += firstName;
            result += " ";
            result += lastName;
        } else {
            result += title;
            result += " ";
            result += firstName;
        }
    } else if (useFormal) {
        // Default to "short" length
        result += title;
        result += " ";
        result += lastName;
    } else {
        result += firstName;
    }

    return FormattedPlaceholder(arg, FormattedValue(std::move(result)));
}

FormattableProperties::~FormattableProperties() {}
Person::~Person() {}

/*
  See ICU4J: CustomFormatterGrammarCaseTest.java
*/
Formatter* GrammarCasesFormatterFactory::createFormatter(const Locale& locale, UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return nullptr;
    }

    // Locale not used
    (void) locale;

    Formatter* result = new GrammarCasesFormatter();
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}


/* static */ void GrammarCasesFormatter::getDativeAndGenitive(const UnicodeString& value, UnicodeString& result) const {
    UnicodeString postfix;
    if (value.endsWith("ana")) {
        value.extract(0,  value.length() - 3, postfix);
        postfix += "nei";
    }
    else if (value.endsWith("ca")) {
        value.extract(0, value.length() - 2, postfix);
        postfix += "căi";
    }
    else if (value.endsWith("ga")) {
        value.extract(0, value.length() - 2, postfix);
        postfix += "găi";
    }
    else if (value.endsWith("a")) {
        value.extract(0, value.length() - 1, postfix);
        postfix += "ei";
    }
    else {
        postfix = "lui " + value;
    }
    result += postfix;
}

message2::FormattedPlaceholder GrammarCasesFormatter::format(FormattedPlaceholder&& arg, FunctionOptions&& options, UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return {};
    }

    // Argument must be present
    if (!arg.canFormat()) {
        errorCode = U_MF_FORMATTING_ERROR;
        return message2::FormattedPlaceholder("grammarBB");
    }

    // Assumes the argument is not-yet-formatted
    const Formattable& toFormat = arg.asFormattable();
    UnicodeString result;

    FunctionOptionsMap opt = options.getOptions();
    switch (toFormat.getType()) {
        case UFMT_STRING: {
            const UnicodeString& in = toFormat.getString(errorCode);
            bool hasCase = opt.count("case") > 0;
            bool caseIsString = opt["case"].getType() == UFMT_STRING;
            if (hasCase && caseIsString && (opt["case"].getString(errorCode) == "dative" || opt["case"].getString(errorCode) == "genitive")) {
                getDativeAndGenitive(in, result);
            } else {
                result += in;
            }
            U_ASSERT(U_SUCCESS(errorCode));
            break;
        }
        default: {
            result += toFormat.getString(errorCode);
            break;
        }
    }

    return message2::FormattedPlaceholder(arg, FormattedValue(std::move(result)));
}

void TestMessageFormat2::testGrammarCasesFormatter(IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    MFFunctionRegistry customRegistry = MFFunctionRegistry::Builder(errorCode)
        .adoptFormatter(FunctionName("grammarBB"), new GrammarCasesFormatterFactory(), errorCode)
        .build();

    TestCase::Builder testBuilder;
    testBuilder.setName("testGrammarCasesFormatter - genitive");
    testBuilder.setFunctionRegistry(&customRegistry);
    testBuilder.setLocale(Locale("ro"));
    testBuilder.setPattern("Cartea {$owner :grammarBB case=genitive}");
    TestCase test = testBuilder.setArgument("owner", "Maria")
                                .setExpected("Cartea Mariei")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setArgument("owner", "Rodica")
                                .setExpected("Cartea Rodicăi")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setArgument("owner", "Ileana")
                                .setExpected("Cartea Ilenei")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setArgument("owner", "Petre")
                                .setExpected("Cartea lui Petre")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    testBuilder.setName("testGrammarCasesFormatter - nominative");
    testBuilder.setPattern("M-a sunat {$owner :grammarBB case=nominative}");

    test = testBuilder.setArgument("owner", "Maria")
                                .setExpected("M-a sunat Maria")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setArgument("owner", "Rodica")
                                .setExpected("M-a sunat Rodica")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setArgument("owner", "Ileana")
                                .setExpected("M-a sunat Ileana")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setArgument("owner", "Petre")
                                .setExpected("M-a sunat Petre")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

/*
  See ICU4J: CustomFormatterListTest.java
*/
Formatter* ListFormatterFactory::createFormatter(const Locale& locale, UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return nullptr;
    }

    Formatter* result = new ListFormatter(locale);
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

message2::FormattedPlaceholder message2::ListFormatter::format(FormattedPlaceholder&& arg, FunctionOptions&& options, UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return {};
    }

    message2::FormattedPlaceholder errorVal = FormattedPlaceholder("listformat");

    // Argument must be present
    if (!arg.canFormat()) {
        errorCode = U_MF_FORMATTING_ERROR;
        return errorVal;
    }
    // Assumes arg is not-yet-formatted
    const Formattable& toFormat = arg.asFormattable();

    FunctionOptionsMap opt = options.getOptions();
    bool hasType = opt.count("type") > 0 && opt["type"].getType() == UFMT_STRING;
    UListFormatterType type = UListFormatterType::ULISTFMT_TYPE_AND;
    if (hasType) {
        if (opt["type"].getString(errorCode) == "OR") {
            type = UListFormatterType::ULISTFMT_TYPE_OR;
        } else if (opt["type"].getString(errorCode) == "UNITS") {
            type = UListFormatterType::ULISTFMT_TYPE_UNITS;
        }
    }
    bool hasWidth = opt.count("width") > 0 && opt["width"].getType() == UFMT_STRING;
    UListFormatterWidth width = UListFormatterWidth::ULISTFMT_WIDTH_WIDE;
    if (hasWidth) {
        if (opt["width"].getString(errorCode) == "SHORT") {
            width = UListFormatterWidth::ULISTFMT_WIDTH_SHORT;
        } else if (opt["width"].getString(errorCode) == "NARROW") {
            width = UListFormatterWidth::ULISTFMT_WIDTH_NARROW;
        }
    }
    U_ASSERT(U_SUCCESS(errorCode));
    LocalPointer<icu::ListFormatter> lf(icu::ListFormatter::createInstance(locale, type, width, errorCode));
    if (U_FAILURE(errorCode)) {
        return {};
    }

    UnicodeString result;

    switch (toFormat.getType()) {
        case UFMT_ARRAY: {
            int32_t n_items;
            const Formattable* objs = toFormat.getArray(n_items, errorCode);
            if (U_FAILURE(errorCode)) {
                errorCode = U_MF_FORMATTING_ERROR;
                return errorVal;
            }
            UnicodeString* parts = new UnicodeString[n_items];
            if (parts == nullptr) {
                errorCode = U_MEMORY_ALLOCATION_ERROR;
                return {};
            }
            for (int32_t i = 0; i < n_items; i++) {
                parts[i] = objs[i].getString(errorCode);
            }
            U_ASSERT(U_SUCCESS(errorCode));
            lf->format(parts, n_items, result, errorCode);
            delete[] parts;
            break;
        }
        default: {
            result += toFormat.getString(errorCode);
            U_ASSERT(U_SUCCESS(errorCode));
            break;
        }
    }

    return FormattedPlaceholder(arg, FormattedValue(std::move(result)));
}

void TestMessageFormat2::testListFormatter(IcuTestErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return;
    }
    const message2::Formattable progLanguages[3] = {
        message2::Formattable("C/C++"),
        message2::Formattable("Java"),
        message2::Formattable("Python")
    };

    TestCase::Builder testBuilder;

    MFFunctionRegistry reg = MFFunctionRegistry::Builder(errorCode)
        .adoptFormatter(FunctionName("listformat"), new ListFormatterFactory(), errorCode)
        .build();
    CHECK_ERROR(errorCode);

    testBuilder.setFunctionRegistry(&reg);
    testBuilder.setArgument("languages", progLanguages, 3);

    TestCase test = testBuilder.setName("testListFormatter")
        .setPattern("I know {$languages :listformat type=AND}!")
        .setExpected("I know C/C++, Java, and Python!")
        .build();
    TestUtils::runTestCase(*this, test, errorCode);

    test = testBuilder.setName("testListFormatter")
                      .setPattern("You are allowed to use {$languages :listformat type=OR}!")
                      .setExpected("You are allowed to use C/C++, Java, or Python!")
                      .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

/*
  See ICU4J: CustomFormatterMessageRefTest.java
*/

/* static */ Hashtable* message2::ResourceManager::properties(UErrorCode& errorCode) {
    NULL_ON_ERROR(errorCode);

    UnicodeString* firefox = new UnicodeString(".match {$gcase :string}  genitive {{Firefoxin}}  * {{Firefox}}");
    UnicodeString* chrome = new UnicodeString(".match {$gcase :string}  genitive {{Chromen}}  * {{Chrome}}");
    UnicodeString* safari = new UnicodeString(".match {$gcase :string}  genitive {{Safarin}}  * {{Safari}}");

    if (firefox != nullptr && chrome != nullptr && safari != nullptr) {
        Hashtable* result = new Hashtable(uhash_compareUnicodeString, nullptr, errorCode);
        if (result == nullptr) {
            return nullptr;
        }
        result->setValueDeleter(uprv_deleteUObject);
        result->put("safari", safari, errorCode);
        result->put("firefox", firefox, errorCode);
        result->put("chrome", chrome, errorCode);
        return result;
    }

    // Allocation failed
    errorCode = U_MEMORY_ALLOCATION_ERROR;
    if (firefox != nullptr) {
        delete firefox;
    }
    if (chrome != nullptr) {
        delete chrome;
    }
    if (safari != nullptr) {
        delete safari;
    }
    return nullptr;
}

Formatter* ResourceManagerFactory::createFormatter(const Locale& locale, UErrorCode& errorCode) {
    if (U_FAILURE(errorCode)) {
        return nullptr;
    }

    Formatter* result = new ResourceManager(locale);
    if (result == nullptr) {
        errorCode = U_MEMORY_ALLOCATION_ERROR;
    }
    return result;
}

using Arguments = MessageArguments;

static Arguments localToGlobal(const FunctionOptionsMap& opts, UErrorCode& status) {
    if (U_FAILURE(status)) {
        return {};
    }
    return MessageArguments(opts, status);
}

message2::FormattedPlaceholder ResourceManager::format(FormattedPlaceholder&& arg, FunctionOptions&& options, UErrorCode& errorCode) const {
    if (U_FAILURE(errorCode)) {
        return {};
    }

    message2::FormattedPlaceholder errorVal = message2::FormattedPlaceholder("msgref");

    // Argument must be present
    if (!arg.canFormat()) {
        errorCode = U_MF_FORMATTING_ERROR;
        return errorVal;
    }

    // Assumes arg is not-yet-formatted
    const Formattable& toFormat = arg.asFormattable();
    UnicodeString in;
    switch (toFormat.getType()) {
        case UFMT_STRING: {
            in = toFormat.getString(errorCode);
            break;
        }
        default: {
            // Ignore non-strings
            return errorVal;
        }
    }
    FunctionOptionsMap opt = options.getOptions();
    bool hasProperties = opt.count("resbundle") > 0 && opt["resbundle"].getType() == UFMT_OBJECT && opt["resbundle"].getObject(errorCode)->tag() == u"properties";

    // If properties were provided, look up the given string in the properties,
    // yielding a message
    if (hasProperties) {
        const FormattableProperties* properties = reinterpret_cast<const FormattableProperties*>(opt["resbundle"].getObject(errorCode));
        U_ASSERT(U_SUCCESS(errorCode));
        UnicodeString* msg = static_cast<UnicodeString*>(properties->properties->get(in));
        if (msg == nullptr) {
            // No message given for this key -- error out
            errorCode = U_MF_FORMATTING_ERROR;
            return errorVal;
        }
	MessageFormatter::Builder mfBuilder(errorCode);
        UParseError parseErr;
        // Any parse/data model errors will be propagated
	MessageFormatter mf = mfBuilder.setPattern(*msg, parseErr, errorCode).build(errorCode);
        Arguments arguments = localToGlobal(opt, errorCode);
        if (U_FAILURE(errorCode)) {
            return errorVal;
        }

        UErrorCode savedStatus = errorCode;
        UnicodeString result = mf.formatToString(arguments, errorCode);
        // Here, we want to ignore errors (this matches the behavior in the ICU4J test).
        // For example: we want $gcase to default to "$gcase" if the gcase option was
        // omitted.
        if (U_FAILURE(errorCode)) {
            errorCode = savedStatus;
        }
        return FormattedPlaceholder(arg, FormattedValue(std::move(result)));
    } else {
        // Properties must be provided
        errorCode = U_MF_FORMATTING_ERROR;
    }
    return errorVal;
}


void TestMessageFormat2::testMessageRefFormatter(IcuTestErrorCode& errorCode) {
    CHECK_ERROR(errorCode);

    Hashtable* properties = ResourceManager::properties(errorCode);
    CHECK_ERROR(errorCode);
    LocalPointer<FormattableProperties> fProperties(new FormattableProperties(properties));
    if (!fProperties.isValid()) {
        ((UErrorCode&) errorCode) = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    MFFunctionRegistry reg = MFFunctionRegistry::Builder(errorCode)
        .adoptFormatter(FunctionName("msgRef"), new ResourceManagerFactory(), errorCode)
        .build();
    CHECK_ERROR(errorCode);

    TestCase::Builder testBuilder;
    testBuilder.setLocale(Locale("ro"));
    testBuilder.setFunctionRegistry(&reg);
    testBuilder.setPattern(*((UnicodeString*) properties->get("firefox")));
    testBuilder.setName("message-ref");

    TestCase test = testBuilder.setArgument("gcase", "whatever")
                                .setExpected("Firefox")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setArgument("gcase", "genitive")
                                .setExpected("Firefoxin")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    testBuilder.setPattern(*((UnicodeString*) properties->get("chrome")));

    test = testBuilder.setArgument("gcase", "whatever")
                                .setExpected("Chrome")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setArgument("gcase", "genitive")
                                .setExpected("Chromen")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    testBuilder.setArgument("res", fProperties.getAlias());

    testBuilder.setPattern("Please start {$browser :msgRef gcase=genitive resbundle=$res}");
    test = testBuilder.setArgument("browser", "firefox")
                                .setExpected("Please start Firefoxin")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setArgument("browser", "chrome")
                                .setExpected("Please start Chromen")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setArgument("browser", "safari")
                                .setExpected("Please start Safarin")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);

    testBuilder.setPattern("Please start {$browser :msgRef resbundle=$res}");
    test = testBuilder.setArgument("browser", "firefox")
                                .setExpected("Please start Firefox")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setArgument("browser", "chrome")
                                .setExpected("Please start Chrome")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
    test = testBuilder.setArgument("browser", "safari")
                                .setExpected("Please start Safari")
                                .build();
    TestUtils::runTestCase(*this, test, errorCode);
}

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */
