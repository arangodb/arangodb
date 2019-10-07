// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Features shared by parsing and pre-parsing scanners.

#include "src/parsing/scanner.h"

#include <stdint.h>

#include <cmath>

#include "src/ast/ast-value-factory.h"
#include "src/conversions-inl.h"
#include "src/objects/bigint.h"
#include "src/parsing/duplicate-finder.h"  // For Scanner::FindSymbol
#include "src/parsing/scanner-inl.h"

namespace v8 {
namespace internal {

class Scanner::ErrorState {
 public:
  ErrorState(MessageTemplate::Template* message_stack,
             Scanner::Location* location_stack)
      : message_stack_(message_stack),
        old_message_(*message_stack),
        location_stack_(location_stack),
        old_location_(*location_stack) {
    *message_stack_ = MessageTemplate::kNone;
    *location_stack_ = Location::invalid();
  }

  ~ErrorState() {
    *message_stack_ = old_message_;
    *location_stack_ = old_location_;
  }

  void MoveErrorTo(TokenDesc* dest) {
    if (*message_stack_ == MessageTemplate::kNone) {
      return;
    }
    if (dest->invalid_template_escape_message == MessageTemplate::kNone) {
      dest->invalid_template_escape_message = *message_stack_;
      dest->invalid_template_escape_location = *location_stack_;
    }
    *message_stack_ = MessageTemplate::kNone;
    *location_stack_ = Location::invalid();
  }

 private:
  MessageTemplate::Template* const message_stack_;
  MessageTemplate::Template const old_message_;
  Scanner::Location* const location_stack_;
  Scanner::Location const old_location_;
};

// ----------------------------------------------------------------------------
// Scanner::LiteralBuffer

Handle<String> Scanner::LiteralBuffer::Internalize(Isolate* isolate) const {
  DCHECK(is_used_);
  if (is_one_byte()) {
    return isolate->factory()->InternalizeOneByteString(one_byte_literal());
  }
  return isolate->factory()->InternalizeTwoByteString(two_byte_literal());
}

int Scanner::LiteralBuffer::NewCapacity(int min_capacity) {
  int capacity = Max(min_capacity, backing_store_.length());
  int new_capacity = Min(capacity * kGrowthFactory, capacity + kMaxGrowth);
  return new_capacity;
}

void Scanner::LiteralBuffer::ExpandBuffer() {
  Vector<byte> new_store = Vector<byte>::New(NewCapacity(kInitialCapacity));
  MemCopy(new_store.start(), backing_store_.start(), position_);
  backing_store_.Dispose();
  backing_store_ = new_store;
}

void Scanner::LiteralBuffer::ConvertToTwoByte() {
  DCHECK(is_one_byte_);
  Vector<byte> new_store;
  int new_content_size = position_ * kUC16Size;
  if (new_content_size >= backing_store_.length()) {
    // Ensure room for all currently read code units as UC16 as well
    // as the code unit about to be stored.
    new_store = Vector<byte>::New(NewCapacity(new_content_size));
  } else {
    new_store = backing_store_;
  }
  uint8_t* src = backing_store_.start();
  uint16_t* dst = reinterpret_cast<uint16_t*>(new_store.start());
  for (int i = position_ - 1; i >= 0; i--) {
    dst[i] = src[i];
  }
  if (new_store.start() != backing_store_.start()) {
    backing_store_.Dispose();
    backing_store_ = new_store;
  }
  position_ = new_content_size;
  is_one_byte_ = false;
}

void Scanner::LiteralBuffer::AddTwoByteChar(uc32 code_unit) {
  DCHECK(!is_one_byte_);
  if (position_ >= backing_store_.length()) ExpandBuffer();
  if (code_unit <=
      static_cast<uc32>(unibrow::Utf16::kMaxNonSurrogateCharCode)) {
    *reinterpret_cast<uint16_t*>(&backing_store_[position_]) = code_unit;
    position_ += kUC16Size;
  } else {
    *reinterpret_cast<uint16_t*>(&backing_store_[position_]) =
        unibrow::Utf16::LeadSurrogate(code_unit);
    position_ += kUC16Size;
    if (position_ >= backing_store_.length()) ExpandBuffer();
    *reinterpret_cast<uint16_t*>(&backing_store_[position_]) =
        unibrow::Utf16::TrailSurrogate(code_unit);
    position_ += kUC16Size;
  }
}

// ----------------------------------------------------------------------------
// Scanner::BookmarkScope

const size_t Scanner::BookmarkScope::kBookmarkAtFirstPos =
    std::numeric_limits<size_t>::max() - 2;
const size_t Scanner::BookmarkScope::kNoBookmark =
    std::numeric_limits<size_t>::max() - 1;
const size_t Scanner::BookmarkScope::kBookmarkWasApplied =
    std::numeric_limits<size_t>::max();

void Scanner::BookmarkScope::Set() {
  DCHECK_EQ(bookmark_, kNoBookmark);

  // The first token is a bit special, since current_ will still be
  // uninitialized. In this case, store kBookmarkAtFirstPos and special-case it
  // when
  // applying the bookmark.
  DCHECK_IMPLIES(scanner_->current().token == Token::UNINITIALIZED,
                 scanner_->current().location.beg_pos ==
                     scanner_->next().location.beg_pos);
  bookmark_ = (scanner_->current().token == Token::UNINITIALIZED)
                  ? kBookmarkAtFirstPos
                  : scanner_->location().beg_pos;
}

void Scanner::BookmarkScope::Apply() {
  DCHECK(HasBeenSet());  // Caller hasn't called SetBookmark.
  if (bookmark_ == kBookmarkAtFirstPos) {
    scanner_->SeekNext(0);
  } else {
    scanner_->SeekNext(bookmark_);
    scanner_->Next();
    DCHECK_EQ(scanner_->location().beg_pos, static_cast<int>(bookmark_));
  }
  bookmark_ = kBookmarkWasApplied;
}

bool Scanner::BookmarkScope::HasBeenSet() const {
  return bookmark_ != kNoBookmark && bookmark_ != kBookmarkWasApplied;
}

bool Scanner::BookmarkScope::HasBeenApplied() const {
  return bookmark_ == kBookmarkWasApplied;
}

// ----------------------------------------------------------------------------
// Scanner

Scanner::Scanner(UnicodeCache* unicode_cache, Utf16CharacterStream* source,
                 bool is_module)
    : unicode_cache_(unicode_cache),
      source_(source),
      found_html_comment_(false),
      allow_harmony_numeric_separator_(false),
      is_module_(is_module),
      octal_pos_(Location::invalid()),
      octal_message_(MessageTemplate::kNone) {
  DCHECK_NOT_NULL(source);
}

void Scanner::Initialize() {
  // Need to capture identifiers in order to recognize "get" and "set"
  // in object literals.
  Init();
  next().after_line_terminator = true;
  Scan();
}

template <bool capture_raw, bool unicode>
uc32 Scanner::ScanHexNumber(int expected_length) {
  DCHECK_LE(expected_length, 4);  // prevent overflow

  int begin = source_pos() - 2;
  uc32 x = 0;
  for (int i = 0; i < expected_length; i++) {
    int d = HexValue(c0_);
    if (d < 0) {
      ReportScannerError(Location(begin, begin + expected_length + 2),
                         unicode
                             ? MessageTemplate::kInvalidUnicodeEscapeSequence
                             : MessageTemplate::kInvalidHexEscapeSequence);
      return -1;
    }
    x = x * 16 + d;
    Advance<capture_raw>();
  }

  return x;
}

template <bool capture_raw>
uc32 Scanner::ScanUnlimitedLengthHexNumber(int max_value, int beg_pos) {
  uc32 x = 0;
  int d = HexValue(c0_);
  if (d < 0) return -1;

  while (d >= 0) {
    x = x * 16 + d;
    if (x > max_value) {
      ReportScannerError(Location(beg_pos, source_pos() + 1),
                         MessageTemplate::kUndefinedUnicodeCodePoint);
      return -1;
    }
    Advance<capture_raw>();
    d = HexValue(c0_);
  }

  return x;
}

Token::Value Scanner::Next() {
  // Rotate through tokens.
  TokenDesc* previous = current_;
  current_ = next_;
  // Either we already have the next token lined up, in which case next_next_
  // simply becomes next_. In that case we use current_ as new next_next_ and
  // clear its token to indicate that it wasn't scanned yet. Otherwise we use
  // current_ as next_ and scan into it, leaving next_next_ uninitialized.
  if (V8_LIKELY(next_next().token == Token::UNINITIALIZED)) {
    next_ = previous;
    next().after_line_terminator = false;
    Scan();
  } else {
    next_ = next_next_;
    next_next_ = previous;
    previous->token = Token::UNINITIALIZED;
    previous->contextual_token = Token::UNINITIALIZED;
    DCHECK_NE(Token::UNINITIALIZED, current().token);
  }
  return current().token;
}

Token::Value Scanner::PeekAhead() {
  DCHECK(next().token != Token::DIV);
  DCHECK(next().token != Token::ASSIGN_DIV);

  if (next_next().token != Token::UNINITIALIZED) {
    return next_next().token;
  }
  TokenDesc* temp = next_;
  next_ = next_next_;
  next().after_line_terminator = false;
  Scan();
  next_next_ = next_;
  next_ = temp;
  return next_next().token;
}

Token::Value Scanner::SkipSingleHTMLComment() {
  if (is_module_) {
    ReportScannerError(source_pos(), MessageTemplate::kHtmlCommentInModule);
    return Token::ILLEGAL;
  }
  return SkipSingleLineComment();
}

Token::Value Scanner::SkipSingleLineComment() {
  // The line terminator at the end of the line is not considered
  // to be part of the single-line comment; it is recognized
  // separately by the lexical grammar and becomes part of the
  // stream of input elements for the syntactic grammar (see
  // ECMA-262, section 7.4).
  AdvanceUntil([](uc32 c0_) { return unibrow::IsLineTerminator(c0_); });

  return Token::WHITESPACE;
}

Token::Value Scanner::SkipSourceURLComment() {
  TryToParseSourceURLComment();
  while (c0_ != kEndOfInput && !unibrow::IsLineTerminator(c0_)) {
    Advance();
  }

  return Token::WHITESPACE;
}

void Scanner::TryToParseSourceURLComment() {
  // Magic comments are of the form: //[#@]\s<name>=\s*<value>\s*.* and this
  // function will just return if it cannot parse a magic comment.
  DCHECK(!unicode_cache_->IsWhiteSpaceOrLineTerminator(kEndOfInput));
  if (!unicode_cache_->IsWhiteSpace(c0_)) return;
  Advance();
  LiteralBuffer name;
  name.Start();

  while (c0_ != kEndOfInput &&
         !unicode_cache_->IsWhiteSpaceOrLineTerminator(c0_) && c0_ != '=') {
    name.AddChar(c0_);
    Advance();
  }
  if (!name.is_one_byte()) return;
  Vector<const uint8_t> name_literal = name.one_byte_literal();
  LiteralBuffer* value;
  if (name_literal == STATIC_CHAR_VECTOR("sourceURL")) {
    value = &source_url_;
  } else if (name_literal == STATIC_CHAR_VECTOR("sourceMappingURL")) {
    value = &source_mapping_url_;
  } else {
    return;
  }
  if (c0_ != '=')
    return;
  value->Drop();
  value->Start();
  Advance();
  while (unicode_cache_->IsWhiteSpace(c0_)) {
    Advance();
  }
  while (c0_ != kEndOfInput && !unibrow::IsLineTerminator(c0_)) {
    // Disallowed characters.
    if (c0_ == '"' || c0_ == '\'') {
      value->Drop();
      return;
    }
    if (unicode_cache_->IsWhiteSpace(c0_)) {
      break;
    }
    value->AddChar(c0_);
    Advance();
  }
  // Allow whitespace at the end.
  while (c0_ != kEndOfInput && !unibrow::IsLineTerminator(c0_)) {
    if (!unicode_cache_->IsWhiteSpace(c0_)) {
      value->Drop();
      break;
    }
    Advance();
  }
}

Token::Value Scanner::SkipMultiLineComment() {
  DCHECK_EQ(c0_, '*');
  Advance();

  while (c0_ != kEndOfInput) {
    DCHECK(!unibrow::IsLineTerminator(kEndOfInput));
    if (!HasLineTerminatorBeforeNext() && unibrow::IsLineTerminator(c0_)) {
      // Following ECMA-262, section 7.4, a comment containing
      // a newline will make the comment count as a line-terminator.
      next().after_line_terminator = true;
    }

    while (V8_UNLIKELY(c0_ == '*')) {
      Advance();
      if (c0_ == '/') {
        Advance();
        return Token::WHITESPACE;
      }
    }
    Advance();
  }

  // Unterminated multi-line comment.
  return Token::ILLEGAL;
}

Token::Value Scanner::ScanHtmlComment() {
  // Check for <!-- comments.
  DCHECK_EQ(c0_, '!');
  Advance();
  if (c0_ != '-' || Peek() != '-') {
    PushBack('!');  // undo Advance()
    return Token::LT;
  }
  Advance();

  found_html_comment_ = true;
  return SkipSingleHTMLComment();
}

#ifdef DEBUG
void Scanner::SanityCheckTokenDesc(const TokenDesc& token) const {
  // Most tokens should not have literal_chars or even raw_literal chars.
  // The rules are:
  // - UNINITIALIZED: we don't care.
  // - TEMPLATE_*: need both literal + raw literal chars.
  // - IDENTIFIERS, STRINGS, etc.: need a literal, but no raw literal.
  // - all others: should have neither.
  // Furthermore, only TEMPLATE_* tokens can have a
  // invalid_template_escape_message.

  switch (token.token) {
    case Token::UNINITIALIZED:
      // token.literal_chars & other members might be garbage. That's ok.
      break;
    case Token::TEMPLATE_SPAN:
    case Token::TEMPLATE_TAIL:
      DCHECK(token.raw_literal_chars.is_used());
      DCHECK(token.literal_chars.is_used());
      break;
    case Token::ESCAPED_KEYWORD:
    case Token::ESCAPED_STRICT_RESERVED_WORD:
    case Token::FUTURE_STRICT_RESERVED_WORD:
    case Token::IDENTIFIER:
    case Token::NUMBER:
    case Token::BIGINT:
    case Token::REGEXP_LITERAL:
    case Token::SMI:
    case Token::STRING:
    case Token::PRIVATE_NAME:
      DCHECK(token.literal_chars.is_used());
      DCHECK(!token.raw_literal_chars.is_used());
      DCHECK_EQ(token.invalid_template_escape_message, MessageTemplate::kNone);
      break;
    default:
      DCHECK(!token.literal_chars.is_used());
      DCHECK(!token.raw_literal_chars.is_used());
      DCHECK_EQ(token.invalid_template_escape_message, MessageTemplate::kNone);
      break;
  }

  DCHECK_IMPLIES(token.token != Token::IDENTIFIER,
                 token.contextual_token == Token::UNINITIALIZED);
  DCHECK_IMPLIES(token.contextual_token != Token::UNINITIALIZED,
                 token.token == Token::IDENTIFIER &&
                     Token::IsContextualKeyword(token.contextual_token));
  DCHECK(!Token::IsContextualKeyword(token.token));
}
#endif  // DEBUG

void Scanner::SeekForward(int pos) {
  // After this call, we will have the token at the given position as
  // the "next" token. The "current" token will be invalid.
  if (pos == next().location.beg_pos) return;
  int current_pos = source_pos();
  DCHECK_EQ(next().location.end_pos, current_pos);
  // Positions inside the lookahead token aren't supported.
  DCHECK(pos >= current_pos);
  if (pos != current_pos) {
    source_->Seek(pos);
    Advance();
    // This function is only called to seek to the location
    // of the end of a function (at the "}" token). It doesn't matter
    // whether there was a line terminator in the part we skip.
    next().after_line_terminator = false;
  }
  Scan();
}

template <bool capture_raw>
bool Scanner::ScanEscape() {
  uc32 c = c0_;
  Advance<capture_raw>();

  // Skip escaped newlines.
  DCHECK(!unibrow::IsLineTerminator(kEndOfInput));
  if (!capture_raw && unibrow::IsLineTerminator(c)) {
    // Allow escaped CR+LF newlines in multiline string literals.
    if (IsCarriageReturn(c) && IsLineFeed(c0_)) Advance();
    return true;
  }

  switch (c) {
    case '\'':  // fall through
    case '"' :  // fall through
    case '\\': break;
    case 'b' : c = '\b'; break;
    case 'f' : c = '\f'; break;
    case 'n' : c = '\n'; break;
    case 'r' : c = '\r'; break;
    case 't' : c = '\t'; break;
    case 'u' : {
      c = ScanUnicodeEscape<capture_raw>();
      if (c < 0) return false;
      break;
    }
    case 'v':
      c = '\v';
      break;
    case 'x': {
      c = ScanHexNumber<capture_raw>(2);
      if (c < 0) return false;
      break;
    }
    case '0':  // Fall through.
    case '1':  // fall through
    case '2':  // fall through
    case '3':  // fall through
    case '4':  // fall through
    case '5':  // fall through
    case '6':  // fall through
    case '7':
      c = ScanOctalEscape<capture_raw>(c, 2);
      break;
  }

  // Other escaped characters are interpreted as their non-escaped version.
  AddLiteralChar(c);
  return true;
}

template <bool capture_raw>
uc32 Scanner::ScanOctalEscape(uc32 c, int length) {
  uc32 x = c - '0';
  int i = 0;
  for (; i < length; i++) {
    int d = c0_ - '0';
    if (d < 0 || d > 7) break;
    int nx = x * 8 + d;
    if (nx >= 256) break;
    x = nx;
    Advance<capture_raw>();
  }
  // Anything except '\0' is an octal escape sequence, illegal in strict mode.
  // Remember the position of octal escape sequences so that an error
  // can be reported later (in strict mode).
  // We don't report the error immediately, because the octal escape can
  // occur before the "use strict" directive.
  if (c != '0' || i > 0 || IsNonOctalDecimalDigit(c0_)) {
    octal_pos_ = Location(source_pos() - i - 1, source_pos() - 1);
    octal_message_ = capture_raw ? MessageTemplate::kTemplateOctalLiteral
                                 : MessageTemplate::kStrictOctalEscape;
  }
  return x;
}

Token::Value Scanner::ScanString() {
  uc32 quote = c0_;
  Advance();  // consume quote

  LiteralScope literal(this);
  while (true) {
    if (c0_ == quote) {
      literal.Complete();
      Advance();
      return Token::STRING;
    }
    if (c0_ == kEndOfInput || unibrow::IsStringLiteralLineTerminator(c0_)) {
      return Token::ILLEGAL;
    }
    if (c0_ == '\\') {
      Advance();
      // TODO(verwaest): Check whether we can remove the additional check.
      if (c0_ == kEndOfInput || !ScanEscape<false>()) {
        return Token::ILLEGAL;
      }
      continue;
    }
    AddLiteralCharAdvance();
  }
}

Token::Value Scanner::ScanPrivateName() {
  if (!allow_harmony_private_fields()) {
    ReportScannerError(source_pos(),
                       MessageTemplate::kInvalidOrUnexpectedToken);
    return Token::ILLEGAL;
  }

  LiteralScope literal(this);
  DCHECK_EQ(c0_, '#');
  DCHECK(!unicode_cache_->IsIdentifierStart(kEndOfInput));
  if (!unicode_cache_->IsIdentifierStart(Peek())) {
    ReportScannerError(source_pos(),
                       MessageTemplate::kInvalidOrUnexpectedToken);
    return Token::ILLEGAL;
  }

  AddLiteralCharAdvance();
  Token::Value token = ScanIdentifierOrKeywordInner(&literal);
  return token == Token::ILLEGAL ? Token::ILLEGAL : Token::PRIVATE_NAME;
}

Token::Value Scanner::ScanTemplateSpan() {
  // When scanning a TemplateSpan, we are looking for the following construct:
  // TEMPLATE_SPAN ::
  //     ` LiteralChars* ${
  //   | } LiteralChars* ${
  //
  // TEMPLATE_TAIL ::
  //     ` LiteralChars* `
  //   | } LiteralChar* `
  //
  // A TEMPLATE_SPAN should always be followed by an Expression, while a
  // TEMPLATE_TAIL terminates a TemplateLiteral and does not need to be
  // followed by an Expression.

  // These scoped helpers save and restore the original error state, so that we
  // can specially treat invalid escape sequences in templates (which are
  // handled by the parser).
  ErrorState scanner_error_state(&scanner_error_, &scanner_error_location_);
  ErrorState octal_error_state(&octal_message_, &octal_pos_);

  Token::Value result = Token::TEMPLATE_SPAN;
  LiteralScope literal(this);
  next().raw_literal_chars.Start();
  const bool capture_raw = true;
  while (true) {
    uc32 c = c0_;
    if (c == '`') {
      Advance();  // Consume '`'
      result = Token::TEMPLATE_TAIL;
      break;
    } else if (c == '$' && Peek() == '{') {
      Advance();  // Consume '$'
      Advance();  // Consume '{'
      break;
    } else if (c == '\\') {
      Advance();  // Consume '\\'
      DCHECK(!unibrow::IsLineTerminator(kEndOfInput));
      if (capture_raw) AddRawLiteralChar('\\');
      if (unibrow::IsLineTerminator(c0_)) {
        // The TV of LineContinuation :: \ LineTerminatorSequence is the empty
        // code unit sequence.
        uc32 lastChar = c0_;
        Advance();
        if (lastChar == '\r') {
          // Also skip \n.
          if (c0_ == '\n') Advance();
          lastChar = '\n';
        }
        if (capture_raw) AddRawLiteralChar(lastChar);
      } else {
        bool success = ScanEscape<capture_raw>();
        USE(success);
        DCHECK_EQ(!success, has_error());
        // For templates, invalid escape sequence checking is handled in the
        // parser.
        scanner_error_state.MoveErrorTo(next_);
        octal_error_state.MoveErrorTo(next_);
      }
    } else if (c < 0) {
      // Unterminated template literal
      break;
    } else {
      Advance();  // Consume c.
      // The TRV of LineTerminatorSequence :: <CR> is the CV 0x000A.
      // The TRV of LineTerminatorSequence :: <CR><LF> is the sequence
      // consisting of the CV 0x000A.
      if (c == '\r') {
        if (c0_ == '\n') Advance();  // Consume '\n'
        c = '\n';
      }
      if (capture_raw) AddRawLiteralChar(c);
      AddLiteralChar(c);
    }
  }
  literal.Complete();
  next().location.end_pos = source_pos();
  next().token = result;
  next().contextual_token = Token::UNINITIALIZED;

  return result;
}

Handle<String> Scanner::SourceUrl(Isolate* isolate) const {
  Handle<String> tmp;
  if (source_url_.length() > 0) {
    DCHECK(source_url_.is_used());
    tmp = source_url_.Internalize(isolate);
  }
  return tmp;
}

Handle<String> Scanner::SourceMappingUrl(Isolate* isolate) const {
  Handle<String> tmp;
  if (source_mapping_url_.length() > 0) {
    DCHECK(source_mapping_url_.is_used());
    tmp = source_mapping_url_.Internalize(isolate);
  }
  return tmp;
}

bool Scanner::ScanDigitsWithNumericSeparators(bool (*predicate)(uc32 ch),
                                              bool is_check_first_digit) {
  // we must have at least one digit after 'x'/'b'/'o'
  if (is_check_first_digit && !predicate(c0_)) return false;

  bool separator_seen = false;
  while (predicate(c0_) || c0_ == '_') {
    if (c0_ == '_') {
      Advance();
      if (c0_ == '_') {
        ReportScannerError(Location(source_pos(), source_pos() + 1),
                           MessageTemplate::kContinuousNumericSeparator);
        return false;
      }
      separator_seen = true;
      continue;
    }
    separator_seen = false;
    AddLiteralCharAdvance();
  }

  if (separator_seen) {
    ReportScannerError(Location(source_pos(), source_pos() + 1),
                       MessageTemplate::kTrailingNumericSeparator);
    return false;
  }

  return true;
}

bool Scanner::ScanDecimalDigits() {
  if (allow_harmony_numeric_separator()) {
    return ScanDigitsWithNumericSeparators(&IsDecimalDigit, false);
  }
  while (IsDecimalDigit(c0_)) {
    AddLiteralCharAdvance();
  }
  return true;
}

bool Scanner::ScanDecimalAsSmiWithNumericSeparators(uint64_t* value) {
  bool separator_seen = false;
  while (IsDecimalDigit(c0_) || c0_ == '_') {
    if (c0_ == '_') {
      Advance();
      if (c0_ == '_') {
        ReportScannerError(Location(source_pos(), source_pos() + 1),
                           MessageTemplate::kContinuousNumericSeparator);
        return false;
      }
      separator_seen = true;
      continue;
    }
    separator_seen = false;
    *value = 10 * *value + (c0_ - '0');
    uc32 first_char = c0_;
    Advance();
    AddLiteralChar(first_char);
  }

  if (separator_seen) {
    ReportScannerError(Location(source_pos(), source_pos() + 1),
                       MessageTemplate::kTrailingNumericSeparator);
    return false;
  }

  return true;
}

bool Scanner::ScanDecimalAsSmi(uint64_t* value) {
  if (allow_harmony_numeric_separator()) {
    return ScanDecimalAsSmiWithNumericSeparators(value);
  }

  while (IsDecimalDigit(c0_)) {
    *value = 10 * *value + (c0_ - '0');
    uc32 first_char = c0_;
    Advance();
    AddLiteralChar(first_char);
  }
  return true;
}

bool Scanner::ScanBinaryDigits() {
  if (allow_harmony_numeric_separator()) {
    return ScanDigitsWithNumericSeparators(&IsBinaryDigit, true);
  }

  // we must have at least one binary digit after 'b'/'B'
  if (!IsBinaryDigit(c0_)) {
    return false;
  }

  while (IsBinaryDigit(c0_)) {
    AddLiteralCharAdvance();
  }
  return true;
}

bool Scanner::ScanOctalDigits() {
  if (allow_harmony_numeric_separator()) {
    return ScanDigitsWithNumericSeparators(&IsOctalDigit, true);
  }

  // we must have at least one octal digit after 'o'/'O'
  if (!IsOctalDigit(c0_)) {
    return false;
  }

  while (IsOctalDigit(c0_)) {
    AddLiteralCharAdvance();
  }
  return true;
}

bool Scanner::ScanImplicitOctalDigits(int start_pos,
                                      Scanner::NumberKind* kind) {
  *kind = IMPLICIT_OCTAL;

  while (true) {
    // (possible) octal number
    if (IsNonOctalDecimalDigit(c0_)) {
      *kind = DECIMAL_WITH_LEADING_ZERO;
      return true;
    }
    if (!IsOctalDigit(c0_)) {
      // Octal literal finished.
      octal_pos_ = Location(start_pos, source_pos());
      octal_message_ = MessageTemplate::kStrictOctalLiteral;
      return true;
    }
    AddLiteralCharAdvance();
  }
}

bool Scanner::ScanHexDigits() {
  if (allow_harmony_numeric_separator()) {
    return ScanDigitsWithNumericSeparators(&IsHexDigit, true);
  }

  // we must have at least one hex digit after 'x'/'X'
  if (!IsHexDigit(c0_)) {
    return false;
  }

  while (IsHexDigit(c0_)) {
    AddLiteralCharAdvance();
  }
  return true;
}

bool Scanner::ScanSignedInteger() {
  if (c0_ == '+' || c0_ == '-') AddLiteralCharAdvance();
  // we must have at least one decimal digit after 'e'/'E'
  if (!IsDecimalDigit(c0_)) return false;
  return ScanDecimalDigits();
}

Token::Value Scanner::ScanNumber(bool seen_period) {
  DCHECK(IsDecimalDigit(c0_));  // the first digit of the number or the fraction

  NumberKind kind = DECIMAL;

  LiteralScope literal(this);
  bool at_start = !seen_period;
  int start_pos = source_pos();  // For reporting octal positions.
  if (seen_period) {
    // we have already seen a decimal point of the float
    AddLiteralChar('.');
    if (allow_harmony_numeric_separator() && c0_ == '_') {
      return Token::ILLEGAL;
    }
    // we know we have at least one digit
    if (!ScanDecimalDigits()) return Token::ILLEGAL;
  } else {
    // if the first character is '0' we must check for octals and hex
    if (c0_ == '0') {
      AddLiteralCharAdvance();

      // either 0, 0exxx, 0Exxx, 0.xxx, a hex number, a binary number or
      // an octal number.
      if (c0_ == 'x' || c0_ == 'X') {
        AddLiteralCharAdvance();
        kind = HEX;
        if (!ScanHexDigits()) return Token::ILLEGAL;
      } else if (c0_ == 'o' || c0_ == 'O') {
        AddLiteralCharAdvance();
        kind = OCTAL;
        if (!ScanOctalDigits()) return Token::ILLEGAL;
      } else if (c0_ == 'b' || c0_ == 'B') {
        AddLiteralCharAdvance();
        kind = BINARY;
        if (!ScanBinaryDigits()) return Token::ILLEGAL;
      } else if (IsOctalDigit(c0_)) {
        kind = IMPLICIT_OCTAL;
        if (!ScanImplicitOctalDigits(start_pos, &kind)) {
          return Token::ILLEGAL;
        }
        if (kind == DECIMAL_WITH_LEADING_ZERO) {
          at_start = false;
        }
      } else if (IsNonOctalDecimalDigit(c0_)) {
        kind = DECIMAL_WITH_LEADING_ZERO;
      } else if (allow_harmony_numeric_separator() && c0_ == '_') {
        ReportScannerError(Location(source_pos(), source_pos() + 1),
                           MessageTemplate::kZeroDigitNumericSeparator);
        return Token::ILLEGAL;
      }
    }

    // Parse decimal digits and allow trailing fractional part.
    if (kind == DECIMAL || kind == DECIMAL_WITH_LEADING_ZERO) {
      // This is an optimization for parsing Decimal numbers as Smi's.
      if (at_start) {
        uint64_t value = 0;
        // scan subsequent decimal digits
        if (!ScanDecimalAsSmi(&value)) {
          return Token::ILLEGAL;
        }

        if (next().literal_chars.one_byte_literal().length() <= 10 &&
            value <= Smi::kMaxValue && c0_ != '.' &&
            !unicode_cache_->IsIdentifierStart(c0_)) {
          next().smi_value_ = static_cast<uint32_t>(value);
          literal.Complete();

          if (kind == DECIMAL_WITH_LEADING_ZERO) {
            octal_pos_ = Location(start_pos, source_pos());
            octal_message_ = MessageTemplate::kStrictDecimalWithLeadingZero;
          }
          return Token::SMI;
        }
      }

      if (!ScanDecimalDigits()) return Token::ILLEGAL;
      if (c0_ == '.') {
        seen_period = true;
        AddLiteralCharAdvance();
        if (allow_harmony_numeric_separator() && c0_ == '_') {
          return Token::ILLEGAL;
        }
        if (!ScanDecimalDigits()) return Token::ILLEGAL;
      }
    }
  }

  bool is_bigint = false;
  if (c0_ == 'n' && !seen_period &&
      (kind == DECIMAL || kind == HEX || kind == OCTAL || kind == BINARY)) {
    // Check that the literal is within our limits for BigInt length.
    // For simplicity, use 4 bits per character to calculate the maximum
    // allowed literal length.
    static const int kMaxBigIntCharacters = BigInt::kMaxLengthBits / 4;
    int length = source_pos() - start_pos - (kind != DECIMAL ? 2 : 0);
    if (length > kMaxBigIntCharacters) {
      ReportScannerError(Location(start_pos, source_pos()),
                         MessageTemplate::kBigIntTooBig);
      return Token::ILLEGAL;
    }

    is_bigint = true;
    Advance();
  } else if (c0_ == 'e' || c0_ == 'E') {
    // scan exponent, if any
    DCHECK(kind != HEX);  // 'e'/'E' must be scanned as part of the hex number

    if (!(kind == DECIMAL || kind == DECIMAL_WITH_LEADING_ZERO))
      return Token::ILLEGAL;

    // scan exponent
    AddLiteralCharAdvance();

    if (!ScanSignedInteger()) return Token::ILLEGAL;
  }

  // The source character immediately following a numeric literal must
  // not be an identifier start or a decimal digit; see ECMA-262
  // section 7.8.3, page 17 (note that we read only one decimal digit
  // if the value is 0).
  if (IsDecimalDigit(c0_) || unicode_cache_->IsIdentifierStart(c0_)) {
    return Token::ILLEGAL;
  }

  literal.Complete();

  if (kind == DECIMAL_WITH_LEADING_ZERO) {
    octal_pos_ = Location(start_pos, source_pos());
    octal_message_ = MessageTemplate::kStrictDecimalWithLeadingZero;
  }

  return is_bigint ? Token::BIGINT : Token::NUMBER;
}

uc32 Scanner::ScanIdentifierUnicodeEscape() {
  Advance();
  if (c0_ != 'u') return -1;
  Advance();
  return ScanUnicodeEscape<false>();
}

template <bool capture_raw>
uc32 Scanner::ScanUnicodeEscape() {
  // Accept both \uxxxx and \u{xxxxxx}. In the latter case, the number of
  // hex digits between { } is arbitrary. \ and u have already been read.
  if (c0_ == '{') {
    int begin = source_pos() - 2;
    Advance<capture_raw>();
    uc32 cp = ScanUnlimitedLengthHexNumber<capture_raw>(0x10FFFF, begin);
    if (cp < 0 || c0_ != '}') {
      ReportScannerError(source_pos(),
                         MessageTemplate::kInvalidUnicodeEscapeSequence);
      return -1;
    }
    Advance<capture_raw>();
    return cp;
  }
  const bool unicode = true;
  return ScanHexNumber<capture_raw, unicode>(4);
}

Token::Value Scanner::ScanIdentifierOrKeywordInnerSlow(LiteralScope* literal,
                                                       bool escaped) {
  while (true) {
    if (c0_ == '\\') {
      escaped = true;
      uc32 c = ScanIdentifierUnicodeEscape();
      // Only allow legal identifier part characters.
      // TODO(verwaest): Make this true.
      // DCHECK(!unicode_cache_->IsIdentifierPart('\\'));
      DCHECK(!unicode_cache_->IsIdentifierPart(-1));
      if (c == '\\' || !unicode_cache_->IsIdentifierPart(c)) {
        return Token::ILLEGAL;
      }
      AddLiteralChar(c);
    } else if (unicode_cache_->IsIdentifierPart(c0_) ||
               (CombineSurrogatePair() &&
                unicode_cache_->IsIdentifierPart(c0_))) {
      AddLiteralCharAdvance();
    } else {
      break;
    }
  }

  if (next().literal_chars.is_one_byte()) {
    Vector<const uint8_t> chars = next().literal_chars.one_byte_literal();
    Token::Value token =
        KeywordOrIdentifierToken(chars.start(), chars.length());
    /* TODO(adamk): YIELD should be handled specially. */
    if (token == Token::FUTURE_STRICT_RESERVED_WORD) {
      literal->Complete();
      if (escaped) return Token::ESCAPED_STRICT_RESERVED_WORD;
      return token;
    }
    if (token == Token::IDENTIFIER || Token::IsContextualKeyword(token)) {
      literal->Complete();
      return token;
    }

    if (!escaped) return token;

    literal->Complete();
    if (token == Token::LET || token == Token::STATIC) {
      return Token::ESCAPED_STRICT_RESERVED_WORD;
    }
    return Token::ESCAPED_KEYWORD;
  }

  literal->Complete();
  return Token::IDENTIFIER;
}

bool Scanner::ScanRegExpPattern() {
  DCHECK_EQ(Token::UNINITIALIZED, next_next().token);
  DCHECK(next().token == Token::DIV || next().token == Token::ASSIGN_DIV);

  // Scan: ('/' | '/=') RegularExpressionBody '/' RegularExpressionFlags
  bool in_character_class = false;

  // Scan regular expression body: According to ECMA-262, 3rd, 7.8.5,
  // the scanner should pass uninterpreted bodies to the RegExp
  // constructor.
  LiteralScope literal(this);
  if (next().token == Token::ASSIGN_DIV) {
    AddLiteralChar('=');
  }

  while (c0_ != '/' || in_character_class) {
    if (c0_ == kEndOfInput || unibrow::IsLineTerminator(c0_)) {
      return false;
    }
    if (c0_ == '\\') {  // Escape sequence.
      AddLiteralCharAdvance();
      if (c0_ == kEndOfInput || unibrow::IsLineTerminator(c0_)) {
        return false;
      }
      AddLiteralCharAdvance();
      // If the escape allows more characters, i.e., \x??, \u????, or \c?,
      // only "safe" characters are allowed (letters, digits, underscore),
      // otherwise the escape isn't valid and the invalid character has
      // its normal meaning. I.e., we can just continue scanning without
      // worrying whether the following characters are part of the escape
      // or not, since any '/', '\\' or '[' is guaranteed to not be part
      // of the escape sequence.

      // TODO(896): At some point, parse RegExps more thoroughly to capture
      // octal esacpes in strict mode.
    } else {  // Unescaped character.
      if (c0_ == '[') in_character_class = true;
      if (c0_ == ']') in_character_class = false;
      AddLiteralCharAdvance();
    }
  }
  Advance();  // consume '/'

  literal.Complete();
  next().token = Token::REGEXP_LITERAL;
  next().contextual_token = Token::UNINITIALIZED;
  return true;
}


Maybe<RegExp::Flags> Scanner::ScanRegExpFlags() {
  DCHECK_EQ(Token::REGEXP_LITERAL, next().token);

  // Scan regular expression flags.
  int flags = 0;
  while (unicode_cache_->IsIdentifierPart(c0_)) {
    RegExp::Flags flag = RegExp::kNone;
    switch (c0_) {
      case 'g':
        flag = RegExp::kGlobal;
        break;
      case 'i':
        flag = RegExp::kIgnoreCase;
        break;
      case 'm':
        flag = RegExp::kMultiline;
        break;
      case 's':
        flag = RegExp::kDotAll;
        break;
      case 'u':
        flag = RegExp::kUnicode;
        break;
      case 'y':
        flag = RegExp::kSticky;
        break;
      default:
        return Nothing<RegExp::Flags>();
    }
    if (flags & flag) {
      return Nothing<RegExp::Flags>();
    }
    Advance();
    flags |= flag;
  }

  next().location.end_pos = source_pos();
  return Just(RegExp::Flags(flags));
}

const AstRawString* Scanner::CurrentSymbol(
    AstValueFactory* ast_value_factory) const {
  if (is_literal_one_byte()) {
    return ast_value_factory->GetOneByteString(literal_one_byte_string());
  }
  return ast_value_factory->GetTwoByteString(literal_two_byte_string());
}

const AstRawString* Scanner::NextSymbol(
    AstValueFactory* ast_value_factory) const {
  if (is_next_literal_one_byte()) {
    return ast_value_factory->GetOneByteString(next_literal_one_byte_string());
  }
  return ast_value_factory->GetTwoByteString(next_literal_two_byte_string());
}

const AstRawString* Scanner::CurrentRawSymbol(
    AstValueFactory* ast_value_factory) const {
  if (is_raw_literal_one_byte()) {
    return ast_value_factory->GetOneByteString(raw_literal_one_byte_string());
  }
  return ast_value_factory->GetTwoByteString(raw_literal_two_byte_string());
}


double Scanner::DoubleValue() {
  DCHECK(is_literal_one_byte());
  return StringToDouble(
      unicode_cache_,
      literal_one_byte_string(),
      ALLOW_HEX | ALLOW_OCTAL | ALLOW_IMPLICIT_OCTAL | ALLOW_BINARY);
}

const char* Scanner::CurrentLiteralAsCString(Zone* zone) const {
  DCHECK(is_literal_one_byte());
  Vector<const uint8_t> vector = literal_one_byte_string();
  int length = vector.length();
  char* buffer = zone->NewArray<char>(length + 1);
  memcpy(buffer, vector.start(), length);
  buffer[length] = '\0';
  return buffer;
}

bool Scanner::IsDuplicateSymbol(DuplicateFinder* duplicate_finder,
                                AstValueFactory* ast_value_factory) const {
  DCHECK_NOT_NULL(duplicate_finder);
  DCHECK_NOT_NULL(ast_value_factory);
  const AstRawString* string = CurrentSymbol(ast_value_factory);
  return !duplicate_finder->known_symbols_.insert(string).second;
}

void Scanner::SeekNext(size_t position) {
  // Use with care: This cleanly resets most, but not all scanner state.
  // TODO(vogelheim): Fix this, or at least DCHECK the relevant conditions.

  // To re-scan from a given character position, we need to:
  // 1, Reset the current_, next_ and next_next_ tokens
  //    (next_ + next_next_ will be overwrittem by Next(),
  //     current_ will remain unchanged, so overwrite it fully.)
  for (TokenDesc& token : token_storage_) {
    token.token = Token::UNINITIALIZED;
    token.contextual_token = Token::UNINITIALIZED;
  }
  // 2, reset the source to the desired position,
  source_->Seek(position);
  // 3, re-scan, by scanning the look-ahead char + 1 token (next_).
  c0_ = source_->Advance();
  next().after_line_terminator = false;
  Scan();
  DCHECK_EQ(next().location.beg_pos, static_cast<int>(position));
}

}  // namespace internal
}  // namespace v8
