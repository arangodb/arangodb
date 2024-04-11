// © 2024 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html

#include "unicode/utypes.h"

#ifndef U_HIDE_DEPRECATED_API

#ifndef MESSAGEFORMAT_PARSER_H
#define MESSAGEFORMAT_PARSER_H

#include "unicode/messageformat2_data_model.h"
#include "unicode/parseerr.h"

#include "messageformat2_allocation.h"
#include "messageformat2_errors.h"

#if U_SHOW_CPLUSPLUS_API

#if !UCONFIG_NO_FORMATTING

#if !UCONFIG_NO_MF2

U_NAMESPACE_BEGIN

namespace message2 {

    using namespace data_model;

    // Parser class (private)
    class Parser : public UMemory {
    public:
	virtual ~Parser();
    private:
        friend class MessageFormatter;

        void parse(UParseError&, UErrorCode&);

	/*
	  Use an internal "parse error" structure to make it easier to translate
	  absolute offsets to line offsets.
	  This is translated back to a `UParseError` at the end of parsing.
	*/
	typedef struct MessageParseError {
	    // The line on which the error occurred
	    uint32_t line;
	    // The offset, relative to the erroneous line, on which the error occurred
	    uint32_t offset;
	    // The total number of characters seen before advancing to the current line. It has a value of 0 if line == 0.
	    // It includes newline characters, because the index does too.
	    uint32_t lengthBeforeCurrentLine;

	    // This parser doesn't yet use the last two fields.
	    UChar   preContext[U_PARSE_CONTEXT_LEN];
	    UChar   postContext[U_PARSE_CONTEXT_LEN];
	} MessageParseError;

	Parser(const UnicodeString &input, MFDataModel::Builder& dataModelBuilder, StaticErrors& e, UnicodeString& normalizedInputRef)
	  : source(input), index(0), errors(e), normalizedInput(normalizedInputRef), dataModel(dataModelBuilder) {
	  parseError.line = 0;
	  parseError.offset = 0;
	  parseError.lengthBeforeCurrentLine = 0;
	  parseError.preContext[0] = '\0';
	  parseError.postContext[0] = '\0';
	}

	// Used so `parseEscapeSequence()` can handle all types of escape sequences
	// (literal, text, and reserved)
	typedef enum { LITERAL, TEXT, RESERVED } EscapeKind;

	static void translateParseError(const MessageParseError&, UParseError&);
	static void setParseError(MessageParseError&, uint32_t);
	void maybeAdvanceLine();
        Pattern parseSimpleMessage(UErrorCode&);
        void parseBody(UErrorCode&);
	void parseDeclarations(UErrorCode&);
        void parseUnsupportedStatement(UErrorCode&);
        void parseLocalDeclaration(UErrorCode&);
        void parseInputDeclaration(UErrorCode&);
	void parseSelectors(UErrorCode&);

	void parseWhitespaceMaybeRequired(bool, UErrorCode&);
	void parseRequiredWhitespace(UErrorCode&);
	void parseOptionalWhitespace(UErrorCode&);
	void parseToken(UChar32, UErrorCode&);
	void parseTokenWithWhitespace(UChar32, UErrorCode&);
	template <int32_t N>
	void parseToken(const UChar32 (&)[N], UErrorCode&);
	template <int32_t N>
	void parseTokenWithWhitespace(const UChar32 (&)[N], UErrorCode&);
        bool nextIsMatch() const;
	UnicodeString parseName(UErrorCode&);
        UnicodeString parseIdentifier(UErrorCode&);
        UnicodeString parseDigits(UErrorCode&);
	VariableName parseVariableName(UErrorCode&);
	FunctionName parseFunction(UErrorCode&);
	void parseEscapeSequence(EscapeKind, UnicodeString&, UErrorCode&);
	void parseLiteralEscape(UnicodeString&, UErrorCode&);
        Literal parseUnquotedLiteral(UErrorCode&);
        Literal parseQuotedLiteral(UErrorCode&);
	Literal parseLiteral(UErrorCode&);
        void parseAttribute(UVector&, UErrorCode&);
        OptionMap parseAttributes(UErrorCode&);
        void parseOption(Operator::Builder&, UErrorCode&);
        void parseOption(UVector&, UErrorCode&);
        void parseOptions(Operator::Builder&, UErrorCode&);
        OptionMap parseOptions(UErrorCode&);
	void parseReservedEscape(UnicodeString&, UErrorCode&);
	void parseReservedChunk(Reserved::Builder&, UErrorCode&);
	Reserved parseReserved(UErrorCode&);
        Reserved parseReservedBody(Reserved::Builder&, UErrorCode&);
	Operator parseAnnotation(UErrorCode&);
	void parseLiteralOrVariableWithAnnotation(bool, Expression::Builder&, UErrorCode&);
        Markup parseMarkup(UErrorCode&);
	Expression parseExpression(UErrorCode&);
        std::variant<Expression, Markup> parsePlaceholder(UErrorCode&);
	void parseTextEscape(UnicodeString&, UErrorCode&);
	UnicodeString parseText(UErrorCode&);
	Key parseKey(UErrorCode&);
	SelectorKeys parseNonEmptyKeys(UErrorCode&);
	void errorPattern(UErrorCode& status);
	Pattern parseQuotedPattern(UErrorCode&);

	// The input string
	const UnicodeString &source;
	// The current position within the input string
	uint32_t index;
	// Represents the current line (and when an error is indicated),
	// character offset within the line of the parse error
	MessageParseError parseError;

	// The structure to use for recording errors
	StaticErrors& errors;

	// Normalized version of the input string (optional whitespace removed)
	UnicodeString& normalizedInput;

	// The parent builder
	MFDataModel::Builder &dataModel;
    }; // class Parser

} // namespace message2

U_NAMESPACE_END

#endif /* #if !UCONFIG_NO_MF2 */

#endif /* #if !UCONFIG_NO_FORMATTING */

#endif /* U_SHOW_CPLUSPLUS_API */

#endif // MESSAGEFORMAT_PARSER_H

#endif // U_HIDE_DEPRECATED_API
// eof
