// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

#include "unicode/messageformat2_data_model.h"
#include "messageformat2_allocation.h"
#include "messageformat2_macros.h"
#include "uvector.h"

U_NAMESPACE_BEGIN

namespace message2 {

// Implementation

//------------------ SelectorKeys

const Key* SelectorKeys::getKeysInternal() const {
    return keys.getAlias();
}

// Lexically order key lists
bool SelectorKeys::operator<(const SelectorKeys& other) const {
    // Handle key lists of different sizes first --
    // this case does have to be handled (even though it would
    // reflect a data model error) because of the need to produce
    // partial output
    if (len < other.len) {
        return true;
    }
    if (len > other.len) {
        return false;
    }

    for (int32_t i = 0; i < len; i++) {
        if (keys[i] < other.keys[i]) {
            return true;
        }
        if (!(keys[i] == other.keys[i])) {
            return false;
        }
    }
    // If we've reached here, all keys must be equal
    return false;
}

SelectorKeys::Builder::Builder(UErrorCode& status) {
    keys = createUVector(status);
}

SelectorKeys::Builder& SelectorKeys::Builder::add(Key&& key, UErrorCode& status) noexcept {
    U_ASSERT(keys != nullptr);
    if (U_SUCCESS(status)) {
        Key* k = create<Key>(std::move(key), status);
        keys->adoptElement(k, status);
    }
    return *this;
}

SelectorKeys SelectorKeys::Builder::build(UErrorCode& status) const {
    if (U_FAILURE(status)) {
        return {};
    }
    U_ASSERT(keys != nullptr);
    return SelectorKeys(*keys, status);
}

SelectorKeys::Builder::~Builder() {
    if (keys != nullptr) {
        delete keys;
    }
}

SelectorKeys::SelectorKeys(const UVector& ks, UErrorCode& status) : len(ks.size()) {
    if (U_FAILURE(status)) {
        return;
    }
    Key* result = copyVectorToArray<Key>(ks, len);
    if (result == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        len = 0;
        return;
    }
    keys.adoptInstead(result);
}

SelectorKeys& SelectorKeys::operator=(SelectorKeys other) noexcept {
    swap(*this, other);
    return *this;
}

SelectorKeys::SelectorKeys(const SelectorKeys& other) : len(other.len) {
    keys.adoptInstead(copyArray(other.keys.getAlias(), len));
}

SelectorKeys::~SelectorKeys() {
    len = 0;
}

//------------------ Literal

bool Literal::operator<(const Literal& other) const {
    // Ignore quoting for the purposes of ordering
    return contents < other.contents;
}

bool Literal::operator==(const Literal& other) const {
    // Ignore quoting for the purposes of ordering
    return contents == other.contents;
}

UnicodeString Literal::quoted() const {
    UnicodeString result(PIPE);
    result += unquoted();
    result += PIPE;
    return result;
}

const UnicodeString& Literal::unquoted() const { return contents; }

Literal& Literal::operator=(Literal other) noexcept {
    swap(*this, other);

    return *this;
}

Literal::~Literal() {
    thisIsQuoted = false;
}

//------------------ Operand

Operand::Operand(const Operand& other) : contents(other.contents) {}

Operand& Operand::operator=(Operand other) noexcept {
    swap(*this, other);

    return *this;
}

UBool Operand::isVariable() const {
    return (contents.has_value() && std::holds_alternative<VariableName>(*contents));
}
UBool Operand::isLiteral() const {
    return (contents.has_value() && std::holds_alternative<Literal>(*contents));
}
UBool Operand::isNull() const { return !contents.has_value(); }

const Literal& Operand::asLiteral() const {
    U_ASSERT(isLiteral());
    return *(std::get_if<Literal>(&(*contents)));
}

const VariableName& Operand::asVariable() const {
    U_ASSERT(isVariable());
    return *(std::get_if<VariableName>(&(*contents)));
}

Operand::~Operand() {}

//---------------- Key

Key& Key::operator=(Key other) noexcept {
    swap(*this, other);
    return *this;
}

bool Key::operator<(const Key& other) const {
    // Arbitrarily treat * as greater than all concrete keys
    if (isWildcard()) {
        return false;
    }
    if (other.isWildcard()) {
        return true;
    }
    return (asLiteral() < other.asLiteral());
}

bool Key::operator==(const Key& other) const {
    if (isWildcard()) {
        return other.isWildcard();
    }
    return (asLiteral() == other.asLiteral());
}

const Literal& Key::asLiteral() const {
    U_ASSERT(!isWildcard());
    return *contents;
}

Key::~Key() {}

// ------------ Reserved

// Copy constructor
Reserved::Reserved(const Reserved& other) {
    len = other.len;
    parts.adoptInstead(copyArray(other.parts.getAlias(), len));
}

Reserved& Reserved::operator=(Reserved other) noexcept {
    swap(*this, other);
    return *this;
}

Reserved::Reserved(const UVector& ps, UErrorCode& status) noexcept : len(ps.size()) {
    if (U_FAILURE(status)) {
        return;
    }
    parts = LocalArray<Literal>(copyVectorToArray<Literal>(ps, len));
}

int32_t Reserved::numParts() const {
    return len;
}

const Literal& Reserved::getPart(int32_t i) const {
    U_ASSERT(i < numParts());
    return parts[i];
}

Reserved::Builder::Builder(UErrorCode& status) {
    parts = createUVector(status);
}

Reserved Reserved::Builder::build(UErrorCode& status) const noexcept {
    if (U_FAILURE(status)) {
        return {};
    }
    U_ASSERT(parts != nullptr);
    return Reserved(*parts, status);
}

Reserved::Builder& Reserved::Builder::add(Literal&& part, UErrorCode& status) noexcept {
    U_ASSERT(parts != nullptr);
    if (U_SUCCESS(status)) {
        Literal* l = create<Literal>(std::move(part), status);
        parts->adoptElement(l, status);
    }
    return *this;
}

Reserved::Builder::~Builder() {
    if (parts != nullptr) {
        delete parts;
    }
}

Reserved::~Reserved() {
    len = 0;
}

//------------------------ Operator

OptionMap::OptionMap(const UVector& opts, UErrorCode& status) {
    CHECK_ERROR(status);

    len = opts.size();
    Option* result = copyVectorToArray<Option>(opts, len);
    if (result == nullptr) {
        bogus = true;
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    options.adoptInstead(result);
    bogus = false;
}

OptionMap::OptionMap(const OptionMap& other) : len(other.len) {
    U_ASSERT(!other.bogus);
    Option* result = copyArray(other.options.getAlias(), len);
    if (result == nullptr && len != 0) {
        bogus = true;
        return;
    }
    bogus = false;
    options.adoptInstead(result);
}

OptionMap& OptionMap::operator=(OptionMap other) {
    swap(*this, other);
    return *this;
}

const Option& OptionMap::getOption(int32_t i, UErrorCode& status) const {
    if (U_FAILURE(status) || bogus) {
        if (bogus) {
            status = U_MEMORY_ALLOCATION_ERROR;
        }
    } else {
        U_ASSERT(options.isValid());
        U_ASSERT(i < len);
    }
    return options[i];
}

int32_t OptionMap::size() const {
    U_ASSERT(options.isValid() || len == 0);
    return len;
}

Operator::Builder& Operator::Builder::setOptionMap(OptionMap&& m) {
    optionMap = std::move(m);
    delete options;
    options = nullptr;
    return *this;
}

OptionMap::~OptionMap() {}

const Reserved& Operator::asReserved() const {
    U_ASSERT(isReserved());
    return *(std::get_if<Reserved>(&contents));
}

const OptionMap& Operator::getOptionsInternal() const {
    U_ASSERT(!isReserved());
    return std::get_if<Callable>(&contents)->getOptions();
}

Option::Option(const Option& other): name(other.name), rand(other.rand) {}

Option& Option::operator=(Option other) noexcept {
    swap(*this, other);
    return *this;
}

Option::~Option() {}

Operator::Builder::Builder(UErrorCode& status) {
    options = createStringUVector(status);
}

Operator::Builder& Operator::Builder::setReserved(Reserved&& reserved) {
    isReservedSequence = true;
    hasFunctionName = false;
    asReserved = std::move(reserved);
    return *this;
}

Operator::Builder& Operator::Builder::setFunctionName(FunctionName&& func) {
    isReservedSequence = false;
    hasFunctionName = true;
    functionName = std::move(func);
    return *this;
}

const FunctionName& Operator::getFunctionName() const {
    U_ASSERT(!isReserved());
    return std::get_if<Callable>(&contents)->getName();
}

static UBool hasOptionNamed(const UVector& v, const UnicodeString& s) {
    for (int32_t i = 0; i < v.size(); i++) {
        const Option* opt = static_cast<Option*>(v[i]);
        U_ASSERT(opt != nullptr);
        if (opt->getName() == s) {
            return true;
        }
    }
    return false;
}

Operator::Builder& Operator::Builder::addOption(const UnicodeString &key, Operand&& value, UErrorCode& errorCode) noexcept {
    THIS_ON_ERROR(errorCode);

    isReservedSequence = false;
    hasOptions = true;
    U_ASSERT(options != nullptr);
    // If the option name is already in the map, emit a data model error
    if (hasOptionNamed(*options, key)) {
        errorCode = U_MF_DUPLICATE_OPTION_NAME_ERROR;
    } else {
        Option* newOption = create<Option>(Option(key, std::move(value)), errorCode);
        THIS_ON_ERROR(errorCode);
        options->adoptElement(newOption, errorCode);
    }
    return *this;
}

Operator Operator::Builder::build(UErrorCode& errorCode) {
    Operator result;
    if (U_FAILURE(errorCode)) {
        return result;
    }
    // Must be either reserved or function, not both; enforced by methods
    if (isReservedSequence) {
        // Methods enforce that the function name and options are unset
        // if `setReserved()` is called, so if they were valid, that
        // would indicate a bug.
        U_ASSERT(!hasOptions && !hasFunctionName);
        result = Operator(asReserved);
    } else {
        if (!hasFunctionName) {
            // Neither function name nor reserved was set
            // There is no default, so this case could occur if the
            // caller creates a builder and doesn't make any calls
            // before calling build().
            errorCode = U_INVALID_STATE_ERROR;
            return result;
        }
        if (options != nullptr) {
            // Initialize options from what was set with setOptions()
            optionMap = OptionMap(*options, errorCode);
         }
        result = Operator(functionName, optionMap);
    }
    return result;
}

Operator::Operator(const Operator& other) noexcept : contents(other.contents) {}

Operator& Operator::operator=(Operator other) noexcept {
    swap(*this, other);
    return *this;
}

// Function call
Operator::Operator(const FunctionName& f, const UVector& optsVector, UErrorCode& status) : contents(Callable(f, OptionMap(optsVector, status))) {}

Operator::Operator(const FunctionName& f, const OptionMap& opts) : contents(Callable(f, opts)) {}

Operator::Builder::~Builder() {
    if (options != nullptr) {
        delete options;
    }
}

Operator::~Operator() {}

Callable& Callable::operator=(Callable other) noexcept {
    swap(*this, other);
    return *this;
}

Callable::Callable(const Callable& other) : name(other.name), options(other.options) {}

Callable::~Callable() {}

// ------------ Markup

Markup::Builder::Builder(UErrorCode& status) {
    options = createUVector(status);
    attributes = createUVector(status);
}

Markup::Builder& Markup::Builder::setAttributeMap(OptionMap&& m) {
    attributeMap = std::move(m);
    delete attributes;
    attributes = nullptr;
    return *this;
}

Markup::Builder& Markup::Builder::setOptionMap(OptionMap&& m) {
    optionMap = std::move(m);
    delete options;
    options = nullptr;
    return *this;
}

Markup::Markup(UMarkupType ty, UnicodeString n, OptionMap&& o, OptionMap&& a) : type(ty), name(n), options(std::move(o)), attributes(std::move(a)) {
}

Markup Markup::Builder::build(UErrorCode& errorCode) {
    Markup result;

    if (U_FAILURE(errorCode)) {
        return result;
    }

    if (type == UMARKUP_COUNT || name.length() == 0) {
        // One of `setOpen()`, `setClose()`, or `setStandalone()`
        // must be called before calling build()
        // setName() must be called before calling build()
        errorCode = U_INVALID_STATE_ERROR;
    } else {
        if (options != nullptr) {
            // Initialize options from what was done with setOptions
            optionMap = OptionMap(*options, errorCode);
        }
        if (attributes != nullptr) {
            attributeMap = OptionMap(*attributes, errorCode);
        }
        result = Markup(type, name, std::move(optionMap), std::move(attributeMap));
    }
    return result;
}

Markup::Builder::~Builder() {
    if (options != nullptr) {
        delete options;
    }
    if (attributes != nullptr) {
        delete attributes;
    }
}

Markup::~Markup() {}

// ------------ Expression

Expression::Builder::Builder(UErrorCode& status) {
    attributes = createUVector(status);
}

UBool Expression::isStandaloneAnnotation() const {
    return rand.isNull();
}

// Returns true for function calls with operands as well as
// standalone annotations.
// Reserved sequences are not function calls
UBool Expression::isFunctionCall() const {
    return (rator.has_value() && !rator->isReserved());
}

UBool Expression::isReserved() const {
    return (rator.has_value() && rator->isReserved());
}

const Operator* Expression::getOperator(UErrorCode& status) const {
    NULL_ON_ERROR(status);

    if (!(isReserved() || isFunctionCall())) {
        status = U_INVALID_STATE_ERROR;
        return nullptr;
    }
    U_ASSERT(rator);
    return &(*rator);
}

// May return null operand
const Operand& Expression::getOperand() const { return rand; }

Expression::Builder& Expression::Builder::setOperand(Operand&& rAnd) {
    hasOperand = true;
    rand = std::move(rAnd);
    return *this;
}

Expression::Builder& Expression::Builder::setOperator(Operator&& rAtor) {
    hasOperator = true;
    rator = std::move(rAtor);
    return *this;
}

Expression::Builder& Expression::Builder::setAttributeMap(OptionMap&& m) {
    attributeMap = std::move(m);
    delete attributes;
    attributes = nullptr;
    return *this;
}

Expression Expression::Builder::build(UErrorCode& errorCode) {
    Expression result;

    if (U_FAILURE(errorCode)) {
        return result;
    }

    if ((!hasOperand || rand.isNull()) && !hasOperator) {
        errorCode = U_INVALID_STATE_ERROR;
        return result;
    }

    if (attributes != nullptr) {
        attributeMap = OptionMap(*attributes, errorCode);
    }
    if (hasOperand && hasOperator) {
        result = Expression(rator, rand, attributeMap);
    } else if (hasOperand && !hasOperator) {
        result = Expression(rand, attributeMap);
    } else {
        // rator is valid, rand is not valid
        result = Expression(rator, attributeMap);
    }
    return result;
}

Expression::Expression() : rator(std::nullopt) {}

Expression::Expression(const Expression& other) : rator(other.rator), rand(other.rand), attributes(other.attributes) {}

Expression& Expression::operator=(Expression other) noexcept {
    swap(*this, other);
    return *this;
}

Expression::Builder::~Builder() {
    if (attributes != nullptr) {
        delete attributes;
    }
}

Expression::~Expression() {}

// ----------- UnsupportedStatement

UnsupportedStatement::Builder::Builder(UErrorCode& status) {
    expressions = createUVector(status);
}

UnsupportedStatement::Builder& UnsupportedStatement::Builder::setKeyword(const UnicodeString& k) {
    keyword = k;
    return *this;
}

UnsupportedStatement::Builder& UnsupportedStatement::Builder::setBody(Reserved&& r) {
    body.emplace(r);
    return *this;
}

UnsupportedStatement::Builder& UnsupportedStatement::Builder::addExpression(Expression&& e, UErrorCode& status) {
    U_ASSERT(expressions != nullptr);
    if (U_SUCCESS(status)) {
        Expression* expr = create<Expression>(std::move(e), status);
        expressions->adoptElement(expr, status);
    }
    return *this;
}

UnsupportedStatement UnsupportedStatement::Builder::build(UErrorCode& status) const {
    if (U_SUCCESS(status)) {
        U_ASSERT(expressions != nullptr);
        if (keyword.length() <= 0) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
        } else if (expressions->size() < 1) {
            status = U_ILLEGAL_ARGUMENT_ERROR;
        } else {
            return UnsupportedStatement(keyword, body, *expressions, status);
        }
    }
    return {};
}

const Reserved* UnsupportedStatement::getBody(UErrorCode& errorCode) const {
    if (U_SUCCESS(errorCode)) {
        if (body.has_value()) {
            return &(*body);
        }
        errorCode = U_ILLEGAL_ARGUMENT_ERROR;
    }
    return nullptr;
}

UnsupportedStatement::UnsupportedStatement(const UnicodeString& k,
                                           const std::optional<Reserved>& r,
                                           const UVector& es,
                                           UErrorCode& status) : keyword(k), body(r) {
    CHECK_ERROR(status);

    U_ASSERT(es.size() >= 1);
    Expression* result = copyVectorToArray<Expression>(es, expressionsLen);
    if (result == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        expressionsLen = 0;
        return;
    }
    expressions.adoptInstead(result);
}

UnsupportedStatement::UnsupportedStatement(const UnsupportedStatement& other) {
    keyword = other.keyword;
    body = other.body;
    expressionsLen = other.expressionsLen;
    U_ASSERT(expressionsLen > 0);
    expressions.adoptInstead(copyArray(other.expressions.getAlias(), expressionsLen));
}

UnsupportedStatement& UnsupportedStatement::operator=(UnsupportedStatement other) noexcept {
    swap(*this, other);
    return *this;
}

UnsupportedStatement::Builder::~Builder() {
    if (expressions != nullptr) {
        delete expressions;
    }
}

UnsupportedStatement::~UnsupportedStatement() {}
// ----------- PatternPart

// PatternPart needs a copy constructor in order to make Pattern deeply copyable
// If !isRawText and the copy of the other expression fails,
// then isBogus() will be true for this PatternPart
PatternPart::PatternPart(const PatternPart& other) : piece(other.piece) {}

const Expression& PatternPart::contents() const {
    U_ASSERT(isExpression());
    return *std::get_if<Expression>(&piece);
}

const Markup& PatternPart::asMarkup() const {
    U_ASSERT(isMarkup());
    return *std::get_if<Markup>(&piece);
}

// Precondition: isText();
const UnicodeString& PatternPart::asText() const {
    U_ASSERT(isText());
    return *std::get_if<UnicodeString>(&piece);
}

PatternPart& PatternPart::operator=(PatternPart other) noexcept {
    swap(*this, other);
    return *this;
}

PatternPart::~PatternPart() {}

// ---------------- Pattern

Pattern::Pattern(const UVector& ps, UErrorCode& status) : len(ps.size()) {
    if (U_FAILURE(status)) {
        return;
    }
    PatternPart* result = copyVectorToArray<PatternPart>(ps, len);
    if (result == nullptr) {
        status = U_MEMORY_ALLOCATION_ERROR;
        return;
    }
    parts.adoptInstead(result);
}

// Copy constructor
Pattern::Pattern(const Pattern& other) noexcept : len(other.len) {
    parts.adoptInstead(copyArray(other.parts.getAlias(), len));
}

const PatternPart& Pattern::getPart(int32_t i) const {
    U_ASSERT(i < numParts());
    return parts[i];
}

Pattern::Builder::Builder(UErrorCode& status) {
    parts = createUVector(status);
}

Pattern Pattern::Builder::build(UErrorCode& status) const noexcept {
    if (U_FAILURE(status)) {
        return {};
    }
    U_ASSERT(parts != nullptr);
    return Pattern(*parts, status);
}

Pattern::Builder& Pattern::Builder::add(Expression&& part, UErrorCode& status) noexcept {
    U_ASSERT(parts != nullptr);
    if (U_SUCCESS(status)) {
        PatternPart* l = create<PatternPart>(PatternPart(std::move(part)), status);
        parts->adoptElement(l, status);
    }
    return *this;
}

Pattern::Builder& Pattern::Builder::add(Markup&& part, UErrorCode& status) noexcept {
    U_ASSERT(parts != nullptr);
    if (U_SUCCESS(status)) {
        PatternPart* l = create<PatternPart>(PatternPart(std::move(part)), status);
        parts->adoptElement(l, status);
    }
    return *this;
}

Pattern::Builder& Pattern::Builder::add(UnicodeString&& part, UErrorCode& status) noexcept {
    U_ASSERT(parts != nullptr);
    if (U_SUCCESS(status)) {
        PatternPart* l = create<PatternPart>(PatternPart(std::move(part)), status);
        parts->adoptElement(l, status);
    }
    return *this;
}

Pattern& Pattern::operator=(Pattern other) noexcept {
    swap(*this, other);

    return *this;
}

Pattern::Builder::~Builder() {
    if (parts != nullptr) {
        delete parts;
    }
}

Pattern::~Pattern() {}

// ---------------- Binding

const Expression& Binding::getValue() const {
    return expr;
}

/* static */ Binding Binding::input(UnicodeString&& variableName, Expression&& rhs, UErrorCode& errorCode) {
    Binding b;
    if (U_SUCCESS(errorCode)) {
        const Operand& rand = rhs.getOperand();
        if (!(rand.isVariable() && (rand.asVariable() == variableName))) {
            errorCode = U_INVALID_STATE_ERROR;
        } else {
            const Operator* rator = rhs.getOperator(errorCode);
            bool hasOperator = U_SUCCESS(errorCode);
            if (hasOperator && rator->isReserved()) {
                errorCode = U_INVALID_STATE_ERROR;
            } else {
                // Clear error code -- the "error" from the absent operator
                // is handled
                errorCode = U_ZERO_ERROR;
                b = Binding(variableName, std::move(rhs));
                b.local = false;
                if (hasOperator) {
                    rator = b.getValue().getOperator(errorCode);
                    U_ASSERT(U_SUCCESS(errorCode));
                    b.annotation = std::get_if<Callable>(&(rator->contents));
                } else {
                    b.annotation = nullptr;
                }
                U_ASSERT(!hasOperator || b.annotation != nullptr);
            }
        }
    }
    return b;
}

const OptionMap& Binding::getOptionsInternal() const {
    U_ASSERT(annotation != nullptr);
    return annotation->getOptions();
}

void Binding::updateAnnotation() {
    UErrorCode localErrorCode = U_ZERO_ERROR;
    const Operator* rator = expr.getOperator(localErrorCode);
    if (U_FAILURE(localErrorCode) || rator->isReserved()) {
        return;
    }
    U_ASSERT(U_SUCCESS(localErrorCode) && !rator->isReserved());
    annotation = std::get_if<Callable>(&(rator->contents));
}

Binding::Binding(const Binding& other) : var(other.var), expr(other.expr), local(other.local) {
    updateAnnotation();
}

Binding& Binding::operator=(Binding other) noexcept {
    swap(*this, other);
    return *this;
}

Binding::~Binding() {}

// --------------- Variant

Variant& Variant::operator=(Variant other) noexcept {
    swap(*this, other);
    return *this;
}

Variant::Variant(const Variant& other) : k(other.k), p(other.p) {}

Variant::~Variant() {}

// ------------- Matcher

Matcher& Matcher::operator=(Matcher other) {
    swap(*this, other);
    return *this;
}

Matcher::Matcher(const Matcher& other) {
    numSelectors = other.numSelectors;
    numVariants = other.numVariants;
    selectors.adoptInstead(copyArray<Expression>(other.selectors.getAlias(), numSelectors));
    variants.adoptInstead(copyArray<Variant>(other.variants.getAlias(), numVariants));
}

// --------------- MFDataModel

const Pattern& MFDataModel::getPattern() const {
    if (std::holds_alternative<Matcher>(body)) {
        // Return reference to empty pattern if this is a selectors message
        return empty;
    }
    return *(std::get_if<Pattern>(&body));
}

const Binding* MFDataModel::getLocalVariablesInternal() const {
    U_ASSERT(!bogus);
    U_ASSERT(bindings.isValid());
    return bindings.getAlias();
}

const Expression* MFDataModel::getSelectorsInternal() const {
    U_ASSERT(!bogus);
    U_ASSERT(!hasPattern());
    return std::get_if<Matcher>(&body)->selectors.getAlias();
}

const Variant* MFDataModel::getVariantsInternal() const {
    U_ASSERT(!bogus);
    U_ASSERT(!hasPattern());
    return std::get_if<Matcher>(&body)->variants.getAlias();
}

const UnsupportedStatement* MFDataModel::getUnsupportedStatementsInternal() const {
    U_ASSERT(!bogus);
    U_ASSERT(unsupportedStatements.isValid());
    return unsupportedStatements.getAlias();
}


MFDataModel::Builder::Builder(UErrorCode& status) {
    bindings = createUVector(status);
    unsupportedStatements = createUVector(status);
}

// Invalidate pattern and create selectors/variants if necessary
void MFDataModel::Builder::buildSelectorsMessage(UErrorCode& status) {
    CHECK_ERROR(status);

    if (hasPattern) {
        selectors = createUVector(status);
        variants = createUVector(status);
        hasPattern = false;
    }
    hasPattern = false;
    hasSelectors = true;
}

void MFDataModel::Builder::checkDuplicate(const VariableName& var, UErrorCode& status) const {
    CHECK_ERROR(status);

    // This means that handling declarations is quadratic in the number of variables,
    // but the `UVector` of locals in the builder could be changed to a `Hashtable`
    // if that's a problem
    // Note: this also doesn't check _all_ duplicate declaration errors,
    // see MessageFormatter::Checker::checkDeclarations()
    for (int32_t i = 0; i < bindings->size(); i++) {
        if ((static_cast<Binding*>(bindings->elementAt(i)))->getVariable() == var) {
            status = U_MF_DUPLICATE_DECLARATION_ERROR;
            break;
        }
    }
}

MFDataModel::Builder& MFDataModel::Builder::addBinding(Binding&& b, UErrorCode& status) {
    if (U_SUCCESS(status)) {
        U_ASSERT(bindings != nullptr);
        checkDuplicate(b.getVariable(), status);
        UErrorCode savedStatus = status;
        if (status == U_MF_DUPLICATE_DECLARATION_ERROR) {
            // Want to add the binding anyway even if it's a duplicate
            status = U_ZERO_ERROR;
        }
        bindings->adoptElement(create<Binding>(std::move(b), status), status);
        if (U_SUCCESS(status) || savedStatus == U_MF_DUPLICATE_DECLARATION_ERROR) {
            status = savedStatus;
        }
    }
    return *this;
}

MFDataModel::Builder& MFDataModel::Builder::addUnsupportedStatement(UnsupportedStatement&& s, UErrorCode& status) {
    if (U_SUCCESS(status)) {
        U_ASSERT(unsupportedStatements != nullptr);
        unsupportedStatements->adoptElement(create<UnsupportedStatement>(std::move(s), status), status);
    }
    return *this;
}

/*
  selector must be non-null
*/
MFDataModel::Builder& MFDataModel::Builder::addSelector(Expression&& selector, UErrorCode& status) noexcept {
    THIS_ON_ERROR(status);

    buildSelectorsMessage(status);
    U_ASSERT(selectors != nullptr);
    selectors->adoptElement(create<Expression>(std::move(selector), status), status);

    return *this;
}

/*
  `pattern` must be non-null
*/
MFDataModel::Builder& MFDataModel::Builder::addVariant(SelectorKeys&& keys, Pattern&& pattern, UErrorCode& errorCode) noexcept {
    buildSelectorsMessage(errorCode);
    Variant* v = create<Variant>(Variant(std::move(keys), std::move(pattern)), errorCode);
    if (U_SUCCESS(errorCode)) {
        variants->adoptElement(v, errorCode);
    }
    return *this;
}

MFDataModel::Builder& MFDataModel::Builder::setPattern(Pattern&& pat) {
    pattern = std::move(pat);
    hasPattern = true;
    hasSelectors = false;
    // Invalidate variants
    if (variants != nullptr) {
        variants->removeAllElements();
    }
    return *this;
}

MFDataModel::MFDataModel(const MFDataModel& other) : body(Pattern()) {
    U_ASSERT(!other.bogus);

    if (other.hasPattern()) {
        body.emplace<Pattern>(Pattern(*std::get_if<Pattern>(&other.body)));
    } else {
        const Expression* otherSelectors = other.getSelectorsInternal();
        const Variant* otherVariants = other.getVariantsInternal();
        int32_t numSelectors = other.numSelectors();
        int32_t numVariants = other.numVariants();
        Expression* copiedSelectors = copyArray(otherSelectors, numSelectors);
        Variant* copiedVariants = copyArray(otherVariants, numVariants);
        if (!(copiedSelectors != nullptr && copiedVariants != nullptr)) {
            bogus = true;
        }
        body.emplace<Matcher>(Matcher(copiedSelectors, numSelectors, copiedVariants, numVariants));
    }

    bindingsLen = other.bindingsLen;
    bindings.adoptInstead(copyArray(other.bindings.getAlias(), bindingsLen));
    if (!bindings.isValid()) {
        bogus = true;
    }
    unsupportedStatementsLen = other.unsupportedStatementsLen;
    unsupportedStatements.adoptInstead(copyArray(other.unsupportedStatements.getAlias(), unsupportedStatementsLen));
    if (!unsupportedStatements.isValid()) {
        bogus = true;
    }
}

MFDataModel::MFDataModel(const MFDataModel::Builder& builder, UErrorCode& errorCode) noexcept : body(Pattern()) {
    CHECK_ERROR(errorCode);

    if (builder.hasPattern) {
        body.emplace<Pattern>(builder.pattern);
    } else {
        int32_t numVariants = builder.variants == nullptr ? 0 : builder.variants->size();
        int32_t numSelectors = builder.selectors == nullptr ? 0 : builder.selectors->size();
        Variant* variants = copyVectorToArray<Variant>(*builder.variants, numVariants);
        Expression* selectors = copyVectorToArray<Expression>(*builder.selectors, numSelectors);
        bogus &= (variants != nullptr && selectors != nullptr);
        body.emplace<Matcher>(Matcher(selectors, numSelectors, variants, numVariants));
    }

    U_ASSERT(builder.bindings != nullptr);
    bindingsLen = builder.bindings->size();
    bindings.adoptInstead(copyVectorToArray<Binding>(*builder.bindings, bindingsLen));
    unsupportedStatementsLen = builder.unsupportedStatements->size();
    unsupportedStatements.adoptInstead(copyVectorToArray<UnsupportedStatement>(*builder.unsupportedStatements, unsupportedStatementsLen));
    bogus &= ((bool) (bindings.isValid() && unsupportedStatements.isValid()));
}

MFDataModel::MFDataModel() : body(Pattern()) {}

MFDataModel& MFDataModel::operator=(MFDataModel other) noexcept {
    swap(*this, other);
    return *this;
}

MFDataModel MFDataModel::Builder::build(UErrorCode& errorCode) const noexcept {
    if (U_FAILURE(errorCode)) {
        return {};
    }
    if (!hasPattern && !hasSelectors) {
        errorCode = U_INVALID_STATE_ERROR;
    }
    return MFDataModel(*this, errorCode);
}

MFDataModel::~MFDataModel() {}
MFDataModel::Builder::~Builder() {
    if (selectors != nullptr) {
        delete selectors;
    }
    if (variants != nullptr) {
        delete variants;
    }
    if (bindings != nullptr) {
        delete bindings;
    }
    if (unsupportedStatements != nullptr) {
        delete unsupportedStatements;
    }
}
} // namespace message2

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */
