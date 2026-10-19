////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "Aql/Function/StringFunctions.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Basics/Exceptions.h"
#include "Basics/ICancellationToken.h"
#include "Basics/Result.h"
#include "Basics/ThreadLocalLeaser.h"
#include "Basics/Utf8Helper.h"
#include "Basics/fpconv.h"
#include "Basics/tri-strings.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>

#include <utils/utf8_utils.hpp>

#include <unicode/schriter.h>
#include <unicode/stsearch.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
#include <vector>

namespace arangodb::aql {
namespace {

Result icuError(UErrorCode status) {
  return Result{TRI_ERROR_ARANGO_ICU_ERROR,
                basics::Exception::FillExceptionString(
                    TRI_ERROR_ARANGO_ICU_ERROR, u_errorName_64_64(status))};
}

enum class SearchMatch { kFirst, kLast };

// Returns -1 if there is no match.
ResultT<int64_t> searchInRange(icu_64_64::Locale const& locale,
                               icu_64_64::UnicodeString const& text,
                               icu_64_64::UnicodeString const& pattern,
                               int64_t startOffset, int64_t maxEnd,
                               SearchMatch which) {
  if (pattern.isEmpty()) {
    return startOffset;
  }
  if (text.isEmpty() || startOffset >= text.length()) {
    return -1;
  }

  UErrorCode status = U_ZERO_ERROR;
  icu_64_64::StringSearch search(pattern, text, locale, nullptr, status);
  search.setAttribute(USEARCH_OVERLAP, USEARCH_ON, status);

  // ICU 64 previous functions have a bug documented in:
  // https://github.com/arangodb/arangodb/pull/23455
  // and the official report is here:
  // https://unicode-org.atlassian.net/browse/ICU-22393
  int64_t found = -1;
  auto from = static_cast<int32_t>(startOffset);
  while (from < text.length()) {
    int32_t const pos = search.following(from, status);
    if (U_FAILURE(status) || pos == USEARCH_DONE || pos > maxEnd) {
      break;
    }
    if (pos >= startOffset && pos + search.getMatchedLength() - 1 <= maxEnd) {
      found = pos;
      if (which == SearchMatch::kFirst) {
        break;
      }
    }
    from = text.moveIndex32(std::max(pos, from), 1);
  }
  if (U_FAILURE(status)) {
    return icuError(status);
  }
  return found;
}

void ltrimInternal(int32_t& startOffset, int32_t& endOffset,
                   icu_64_64::UnicodeString const& unicodeStr,
                   uint32_t numWhitespaces, UChar32 const* spaceChars) {
  for (; startOffset < endOffset;
       startOffset = unicodeStr.moveIndex32(startOffset, 1)) {
    bool found = false;

    for (uint32_t pos = 0; pos < numWhitespaces; pos++) {
      if (unicodeStr.char32At(startOffset) == spaceChars[pos]) {
        found = true;
        break;
      }
    }

    if (!found) {
      break;
    }
  }
}

void rtrimInternal(int32_t& startOffset, int32_t& endOffset,
                   icu_64_64::UnicodeString const& unicodeStr,
                   uint32_t numWhitespaces, UChar32 const* spaceChars) {
  if (unicodeStr.length() == 0) {
    return;
  }
  for (int32_t codePos = unicodeStr.moveIndex32(endOffset, -1);
       startOffset <= codePos; codePos = unicodeStr.moveIndex32(codePos, -1)) {
    bool found = false;

    for (uint32_t pos = 0; pos < numWhitespaces; pos++) {
      if (unicodeStr.char32At(codePos) == spaceChars[pos]) {
        found = true;
        --endOffset;
        break;
      }
    }

    if (!found || codePos == 0) {
      break;
    }
  }
}

constexpr char kDefaultWhitespace[] = "\r\n\t ";

void appendString(std::string& buffer, velocypack::Options const& vopts,
                  AqlValue const& value) {
  velocypack::StringSink sink(&buffer);
  functions::appendAsString(vopts, sink, value);
}

icu_64_64::UnicodeString toUnicodeString(velocypack::Options const& vopts,
                                         AqlValue const& value) {
  auto buffer = ThreadLocalStringLeaser::lease();
  appendString(*buffer, vopts, value);
  return icu_64_64::UnicodeString(buffer->data(),
                                  static_cast<int32_t>(buffer->length()));
}

ResultT<std::vector<UChar32>> toCodePoints(
    icu_64_64::UnicodeString const& str) {
  std::vector<UChar32> codePoints(str.countChar32());
  UErrorCode errorCode = U_ZERO_ERROR;
  str.toUTF32(codePoints.data(), static_cast<int32_t>(codePoints.size()),
              errorCode);
  if (U_FAILURE(errorCode)) {
    return icuError(errorCode);
  }
  return codePoints;
}

ThreadLocalStringLeaser::Lease stringOf(velocypack::Options const& vopts,
                                        AqlValue const& value) {
  auto buffer = ThreadLocalStringLeaser::lease();
  appendString(*buffer, vopts, value);
  return buffer;
}

ResultT<std::vector<UChar32>> whitespaceOf(velocypack::Options const& vopts,
                                           AqlValue const* chars) {
  if (chars != nullptr) {
    return toCodePoints(toUnicodeString(vopts, *chars));
  }
  return toCodePoints(icu_64_64::UnicodeString(kDefaultWhitespace));
}

AqlValue trimmed(std::string const& text,
                 std::vector<UChar32> const& whitespace, bool left,
                 bool right) {
  icu_64_64::UnicodeString unicodeStr(text.data(),
                                      static_cast<int32_t>(text.size()));
  auto const numWhitespaces = static_cast<uint32_t>(whitespace.size());
  int32_t startOffset = 0, endOffset = unicodeStr.length();
  if (left) {
    ltrimInternal(startOffset, endOffset, unicodeStr, numWhitespaces,
                  whitespace.data());
  }
  if (right) {
    rtrimInternal(startOffset, endOffset, unicodeStr, numWhitespaces,
                  whitespace.data());
  }

  std::string utf8;
  unicodeStr.tempSubString(startOffset, endOffset - startOffset)
      .toUTF8String(utf8);
  return AqlValue(utf8);
}

struct StringSearchArguments {
  icu_64_64::UnicodeString text;
  icu_64_64::UnicodeString search;
  int64_t start = 0;
  std::optional<int64_t> end;
};

StringSearchArguments stringSearchArguments(velocypack::Options const& vopts,
                                            AqlValue const& text,
                                            AqlValue const& search,
                                            AqlValue const* start,
                                            AqlValue const* end) {
  std::optional<int64_t> endPosition;
  if (end != nullptr && !end->isNull(true)) {
    endPosition = end->toInt64();
  }
  return {toUnicodeString(vopts, text), toUnicodeString(vopts, search),
          start != nullptr ? start->toInt64() : 0, endPosition};
}

void abortMatchWhenCancelled(icu_64_64::RegexMatcher& matcher,
                             ICancellationToken const* token) {
  if (token != nullptr) {
    basics::abortMatchWhenKilled(matcher, token);
  }
}

void throwIfCancelled(ICancellationToken const* token) {
  if (token != nullptr && token->killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
}

}  // namespace

functions::StringFunctionContext functions::makeStringFunctionContext(
    ExpressionContext* ctx) {
  auto& trx = ctx->trx();
  auto& vocbase = trx.vocbase();
  return {trx.vpackOptions(),
          [&vocbase]() -> icu_64_64::Locale const& {
            return vocbase.server().getFeature<LanguageFeature>().getLocale();
          },
          ctx->functionsCache(), ctx};
}

AqlValue functions::toAqlResult(ExpressionContext* ctx, std::string_view name,
                                ResultT<AqlValue>&& result) {
  if (result.ok()) {
    return std::move(*result);
  }
  // the ErrorCode overload formats mismatch messages with the function name
  if (result.errorNumber() == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH ||
      result.errorNumber() ==
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH) {
    registerWarning(ctx, name, result.errorNumber());
  } else {
    registerWarning(ctx, name, result.result());
  }
  return AqlValue(AqlValueHintNull());
}

/// @brief function TO_STRING
ResultT<AqlValue> functions::toString(StringFunctionContext const& ctx,
                                      AqlValue const& value) {
  auto text = stringOf(ctx.vopts, value);
  return AqlValue(std::string_view{*text});
}

/// @brief function TO_CHAR
ResultT<AqlValue> functions::toChar(StringFunctionContext const&,
                                    AqlValue const& codePoint) {
  int64_t v = -1;
  if (ADB_UNLIKELY(codePoint.isNumber())) {
    v = codePoint.toInt64();
  }
  if (v < 0) {
    return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
  }

  UChar32 c = static_cast<uint32_t>(v);
  char buffer[6];
  int32_t offset = 0;
  U8_APPEND_UNSAFE(&buffer[0], offset, c);

  return AqlValue(std::string_view(&buffer[0], static_cast<size_t>(offset)));
}

/// @brief function REPEAT
ResultT<AqlValue> functions::repeat(StringFunctionContext const& ctx,
                                    AqlValue const& value,
                                    AqlValue const& repetitions,
                                    AqlValue const* separator) {
  int64_t const count = repetitions.toInt64();
  if (count == 0) {
    return AqlValue(std::string_view("", 0));
  }
  if (count < 0) {
    return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
  }

  constexpr std::size_t kMaxOutputLength = 16 * 1024 * 1024;
  auto const outputLimitError = [] {
    return Result{TRI_ERROR_RESOURCE_LIMIT,
                  "Output string of AQL REPEAT function was limited to 16MB."};
  };

  auto sep = ThreadLocalStringLeaser::lease();
  if (separator != nullptr) {
    appendString(*sep, ctx.vopts, *separator);
  }

  auto text = ThreadLocalStringLeaser::lease();
  velocypack::SizeConstrainedStringSink textSink(text.get(), kMaxOutputLength);
  appendAsString(ctx.vopts, textSink, value);
  if (textSink.overflowed()) {
    return outputLimitError();
  }
  if (text->empty() && sep->empty()) {
    return AqlValue(std::string_view("", 0));
  }

  auto buffer = ThreadLocalStringLeaser::lease();
  velocypack::SizeConstrainedStringSink adapter(buffer.get(), kMaxOutputLength);

  for (int64_t i = 0; i < count; ++i) {
    if (i > 0) {
      adapter.append(*sep);
    }
    adapter.append(*text);
    if (adapter.overflowed()) {
      return outputLimitError();
    }
  }

  return AqlValue(std::string_view(buffer->data(), buffer->size()));
}

/// @brief function FIND_FIRST
/// FIND_FIRST(text, search, start, end) → position
ResultT<AqlValue> functions::findFirst(StringFunctionContext const& ctx,
                                       AqlValue const& text,
                                       AqlValue const& search,
                                       AqlValue const* start,
                                       AqlValue const* end) {
  auto const arguments =
      stringSearchArguments(ctx.vopts, text, search, start, end);
  if (arguments.start < 0) {
    return AqlValue(AqlValueHintInt(-1));
  }
  int64_t maxEnd = arguments.text.length();
  if (arguments.end.has_value()) {
    maxEnd = *arguments.end;
    if ((maxEnd < arguments.start) || (maxEnd < 0)) {
      return AqlValue(AqlValueHintInt(-1));
    }
  }

  auto const pos = searchInRange(ctx.locale(), arguments.text, arguments.search,
                                 arguments.start, maxEnd, SearchMatch::kFirst);
  if (pos.fail()) {
    return pos.result();
  }
  return AqlValue(AqlValueHintInt(*pos));
}

/// @brief function FIND_LAST
/// FIND_LAST(text, search, start, end) → position
ResultT<AqlValue> functions::findLast(StringFunctionContext const& ctx,
                                      AqlValue const& text,
                                      AqlValue const& search,
                                      AqlValue const* start,
                                      AqlValue const* end) {
  auto const arguments =
      stringSearchArguments(ctx.vopts, text, search, start, end);
  if (arguments.start < 0) {
    return AqlValue(AqlValueHintInt(-1));
  }
  int64_t maxEnd = arguments.text.length();
  int emptySearchCludge = 0;
  if (arguments.end.has_value()) {
    maxEnd = *arguments.end;
    if ((maxEnd < arguments.start) || (maxEnd < 0)) {
      return AqlValue(AqlValueHintInt(-1));
    }
    emptySearchCludge = 1;
  }

  if (arguments.search.isEmpty()) {
    return AqlValue(AqlValueHintInt(maxEnd + emptySearchCludge));
  }

  auto const pos = searchInRange(ctx.locale(), arguments.text, arguments.search,
                                 arguments.start, maxEnd, SearchMatch::kLast);
  if (pos.fail()) {
    return pos.result();
  }
  return AqlValue(AqlValueHintInt(*pos));
}

/// @brief function CONCAT_SEPARATOR
ResultT<AqlValue> functions::concatSeparator(
    StringFunctionContext const& ctx, AqlValue const& separator,
    VPackFunctionParametersView values) {
  auto const& vopts = ctx.vopts;
  auto const sep = stringOf(vopts, separator);
  auto buffer = ThreadLocalStringLeaser::lease();
  velocypack::StringSink adapter(buffer.get());

  bool found = false;

  if (values.size() == 1 && values[0].isArray()) {
    AqlValue const& member = values[0];
    // reserve *some* space
    buffer->reserve((sep->size() + 10) * member.length());

    AqlValueMaterializer materializer(&vopts);
    VPackSlice slice = materializer.slice(member);

    for (VPackSlice it : VPackArrayIterator(slice)) {
      if (it.isNull()) {
        continue;
      }
      if (found) {
        buffer->append(*sep);
      }
      // convert member to a string and append
      appendAsString(vopts, adapter, AqlValue(it.begin()));
      found = true;
    }
    return AqlValue(std::string_view{buffer->data(), buffer->length()});
  }

  // reserve *some* space
  buffer->reserve((sep->size() + 10) * (values.size() + 1));
  for (AqlValue const& member : values) {
    if (member.isNull(true)) {
      continue;
    }
    if (found) {
      buffer->append(*sep);
    }

    // convert member to a string and append
    appendAsString(vopts, adapter, member);
    found = true;
  }

  return AqlValue(std::string_view{buffer->data(), buffer->length()});
}

/// @brief function CHAR_LENGTH
ResultT<AqlValue> functions::charLength(StringFunctionContext const& ctx,
                                        AqlValue const& value) {
  size_t length = 0;

  if (value.isArray() || value.isObject()) {
    AqlValueMaterializer materializer(&ctx.vopts);
    VPackSlice slice = materializer.slice(value);

    auto buffer = ThreadLocalStringLeaser::lease();
    velocypack::StringSink adapter(buffer.get());

    VPackDumper dumper(&adapter, &ctx.vopts);
    dumper.dump(slice);

    length = buffer->length();

  } else if (value.isNull(true)) {
    length = 0;

  } else if (value.isBoolean()) {
    if (value.toBoolean()) {
      length = 4;
    } else {
      length = 5;
    }

  } else if (value.isNumber()) {
    double tmp = value.toDouble();
    if (std::isnan(tmp) || !std::isfinite(tmp)) {
      length = 0;
    } else {
      char buffer[24];
      length = static_cast<size_t>(fpconv_dtoa(tmp, buffer));
    }

  } else if (value.isString()) {
    VPackValueLength l;
    char const* p = value.slice().getStringUnchecked(l);
    length = TRI_CharLengthUtf8String(p, l);
  }

  return AqlValue(AqlValueHintUInt(length));
}

/// @brief function LOWER
ResultT<AqlValue> functions::lower(StringFunctionContext const& ctx,
                                   AqlValue const& value) {
  auto const text = stringOf(ctx.vopts, value);
  icu_64_64::UnicodeString unicodeStr(text->data(),
                                      static_cast<int32_t>(text->length()));
  unicodeStr.toLower(ctx.locale());
  std::string utf8;
  unicodeStr.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function UPPER
ResultT<AqlValue> functions::upper(StringFunctionContext const& ctx,
                                   AqlValue const& value) {
  auto const text = stringOf(ctx.vopts, value);
  icu_64_64::UnicodeString unicodeStr(text->data(),
                                      static_cast<int32_t>(text->length()));
  unicodeStr.toUpper(ctx.locale());
  std::string utf8;
  unicodeStr.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function SUBSTRING
ResultT<AqlValue> functions::substring(StringFunctionContext const& ctx,
                                       AqlValue const& value,
                                       AqlValue const& offset,
                                       AqlValue const* length) {
  auto const text = stringOf(ctx.vopts, value);
  icu_64_64::UnicodeString unicodeStr(text->data(),
                                      static_cast<int32_t>(text->length()));
  auto start = static_cast<int32_t>(offset.toInt64());
  int32_t count = INT32_MAX;
  if (length != nullptr) {
    count = static_cast<int32_t>(length->toInt64());
  }

  if (start < 0) {
    start = unicodeStr.moveIndex32(
        unicodeStr.moveIndex32(unicodeStr.length(), 0), start);
  } else {
    start = unicodeStr.moveIndex32(0, start);
  }

  std::string utf8;
  unicodeStr.tempSubString(start, unicodeStr.moveIndex32(start, count) - start)
      .toUTF8String(utf8);

  return AqlValue(utf8);
}

/// @brief function SUBSTRING_BYTES
ResultT<AqlValue> functions::substringBytes(
    StringFunctionContext const&, AqlValue const& value, AqlValue const& offset,
    AqlValue const* length, AqlValue const* left, AqlValue const* right) {
  if (!value.isString()) {
    return Result{TRI_ERROR_BAD_PARAMETER};
  }

  std::string_view const text = value.slice().stringView();
  if (text.empty()) {
    return AqlValue{velocypack::Slice::emptyStringSlice()};
  }

  auto const strLen = static_cast<int64_t>(text.size());
  int64_t count = strLen;
  if (length != nullptr) {
    count = std::max(int64_t{0}, length->toInt64());
  }
  int64_t start = offset.toInt64();
  if (start < 0) {
    start = std::max(int64_t{0}, strLen + start);
  } else {
    start = std::min(start, strLen);
  }
  int64_t leftChars = 0;
  int64_t rightChars = 0;
  if (left != nullptr) {
    leftChars = left->toInt64();
    rightChars = right != nullptr ? right->toInt64() : leftChars;
  }

  auto* const begin = reinterpret_cast<irs::byte_type const*>(text.data());
  auto* const end = begin + text.size();

  auto* lhsIt = begin + start;
  auto* rhsIt = lhsIt + std::min(count, strLen - start);

  static constexpr auto kMaskBits = 0xC0U;
  static constexpr auto kHelpByte = 0x80U;

  if ((lhsIt != end && (*lhsIt & kMaskBits) == kHelpByte) ||
      (rhsIt != end && (*rhsIt & kMaskBits) == kHelpByte)) {
    return Result{TRI_ERROR_BAD_PARAMETER};
  }

  while (leftChars > 0 && lhsIt != begin) {
    --lhsIt;
    leftChars -= static_cast<int64_t>((*lhsIt & kMaskBits) != kHelpByte);
  }
  while (rightChars > 0 && rhsIt != end) {
    rightChars -= static_cast<int64_t>((*rhsIt & kMaskBits) != kHelpByte);
    ++rhsIt;
  }
  while (rhsIt != end && (*rhsIt & kMaskBits) == kHelpByte) {
    ++rhsIt;
  }

  return AqlValue{std::string_view{reinterpret_cast<char const*>(lhsIt),
                                   reinterpret_cast<char const*>(rhsIt)}};
}

/// @brief function LEFT str, length
ResultT<AqlValue> functions::left(StringFunctionContext const& ctx,
                                  AqlValue const& value,
                                  AqlValue const& length) {
  auto const text = stringOf(ctx.vopts, value);
  uint32_t const count = static_cast<int32_t>(length.toInt64());
  icu_64_64::UnicodeString unicodeStr(text->data(),
                                      static_cast<int32_t>(text->length()));
  icu_64_64::UnicodeString left =
      unicodeStr.tempSubString(0, unicodeStr.moveIndex32(0, count));

  std::string utf8;
  left.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function RIGHT
ResultT<AqlValue> functions::right(StringFunctionContext const& ctx,
                                   AqlValue const& value,
                                   AqlValue const& length) {
  auto const text = stringOf(ctx.vopts, value);
  uint32_t const count = static_cast<int32_t>(length.toInt64());
  icu_64_64::UnicodeString unicodeStr(text->data(),
                                      static_cast<int32_t>(text->length()));
  icu_64_64::UnicodeString right =
      unicodeStr.tempSubString(unicodeStr.moveIndex32(
          unicodeStr.length(), -static_cast<int32_t>(count)));

  std::string utf8;
  right.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function TRIM
ResultT<AqlValue> functions::trim(StringFunctionContext const& ctx,
                                  AqlValue const& value,
                                  AqlValue const* typeOrChars) {
  auto const text = stringOf(ctx.vopts, value);
  int64_t howToTrim = 0;
  icu_64_64::UnicodeString whitespace(kDefaultWhitespace);

  if (typeOrChars != nullptr) {
    if (typeOrChars->isNumber()) {
      howToTrim = typeOrChars->toInt64();

      if (howToTrim < 0 || 2 < howToTrim) {
        howToTrim = 0;
      }
    } else if (typeOrChars->isString()) {
      whitespace = toUnicodeString(ctx.vopts, *typeOrChars);
    }
  }

  auto codePoints = toCodePoints(whitespace);
  if (codePoints.fail()) {
    return std::move(codePoints).result();
  }
  return trimmed(*text, *codePoints, /*left*/ howToTrim <= 1,
                 /*right*/ howToTrim == 2 || howToTrim == 0);
}

/// @brief function LTRIM
ResultT<AqlValue> functions::ltrim(StringFunctionContext const& ctx,
                                   AqlValue const& value,
                                   AqlValue const* chars) {
  auto const text = stringOf(ctx.vopts, value);
  auto codePoints = whitespaceOf(ctx.vopts, chars);
  if (codePoints.fail()) {
    return std::move(codePoints).result();
  }
  return trimmed(*text, *codePoints, /*left*/ true, /*right*/ false);
}

/// @brief function RTRIM
ResultT<AqlValue> functions::rtrim(StringFunctionContext const& ctx,
                                   AqlValue const& value,
                                   AqlValue const* chars) {
  auto const text = stringOf(ctx.vopts, value);
  auto codePoints = whitespaceOf(ctx.vopts, chars);
  if (codePoints.fail()) {
    return std::move(codePoints).result();
  }
  return trimmed(*text, *codePoints, /*left*/ false, /*right*/ true);
}

/// @brief function CONTAINS
ResultT<AqlValue> functions::contains(StringFunctionContext const& ctx,
                                      AqlValue const& text,
                                      AqlValue const& search,
                                      AqlValue const* returnIndex) {
  auto const haystack = toUnicodeString(ctx.vopts, text);
  auto const needle = toUnicodeString(ctx.vopts, search);
  bool const willReturnIndex =
      returnIndex != nullptr && returnIndex->toBoolean();

  auto const pos = searchInRange(ctx.locale(), haystack, needle, 0,
                                 haystack.length(), SearchMatch::kFirst);
  if (pos.fail()) {
    return pos.result();
  }
  if (willReturnIndex) {
    return AqlValue(AqlValueHintInt(*pos));
  }
  return AqlValue(AqlValueHintBool(*pos != -1));
}

/// @brief function CONCAT
ResultT<AqlValue> functions::concat(StringFunctionContext const& ctx,
                                    VPackFunctionParametersView values) {
  auto const& vopts = ctx.vopts;
  auto buffer = ThreadLocalStringLeaser::lease();
  velocypack::StringSink adapter(buffer.get());

  if (values.size() == 1 && values[0].isArray()) {
    AqlValueMaterializer materializer(&vopts);
    VPackSlice slice = materializer.slice(values[0]);

    for (VPackSlice it : VPackArrayIterator(slice)) {
      if (it.isNull()) {
        continue;
      }
      // convert member to a string and append
      appendAsString(vopts, adapter, AqlValue(it.begin()));
    }
    return AqlValue(std::string_view{buffer->data(), buffer->length()});
  }

  for (AqlValue const& member : values) {
    if (member.isNull(true)) {
      continue;
    }

    // convert member to a string and append
    appendAsString(vopts, adapter, member);
  }

  return AqlValue(std::string_view{buffer->data(), buffer->length()});
}

/// @brief function LIKE
ResultT<AqlValue> functions::like(StringFunctionContext const& ctx,
                                  AqlValue const& text, AqlValue const& pattern,
                                  AqlValue const* caseInsensitive) {
  auto const regex = stringOf(ctx.vopts, pattern);

  // the matcher is owned by the context!
  icu_64_64::RegexMatcher* matcher = ctx.cache.buildLikeMatcher(
      *regex, caseInsensitive != nullptr && caseInsensitive->toBoolean());
  if (matcher == nullptr) {
    // compiling regular expression failed
    return Result{TRI_ERROR_QUERY_INVALID_REGEX};
  }
  abortMatchWhenCancelled(*matcher, ctx.cancellationToken);

  auto const value = stringOf(ctx.vopts, text);
  bool error = false;
  bool const result = basics::Utf8Helper::DefaultUtf8Helper.matches(
      matcher, value->data(), value->length(), false, error);

  if (error) {
    throwIfCancelled(ctx.cancellationToken);
    // compiling regular expression failed
    return Result{TRI_ERROR_QUERY_INVALID_REGEX};
  }

  return AqlValue(AqlValueHintBool(result));
}

/// @brief function SPLIT
ResultT<AqlValue> functions::split(StringFunctionContext const& ctx,
                                   AqlValue const& value,
                                   AqlValue const* separator,
                                   AqlValue const* limit) {
  // cheapest parameter checks first:
  int64_t limitNumber = -1;
  if (limit != nullptr) {
    if (!limit->isNumber()) {
      return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
    }
    limitNumber = limit->toInt64();

    // these are edge cases which are documented to have these return values:
    if (limitNumber < 0) {
      return AqlValue(AqlValueHintNull());
    }
    if (limitNumber == 0) {
      return AqlValue(AqlValueHintEmptyArray());
    }
  }

  if (separator == nullptr) {
    // pre-documented edge-case: if we only have the first parameter, return it.
    auto builder = ThreadLocalBuilderLeaser::lease();
    builder->openArray();
    builder->add(value.slice());
    builder->close();
    return AqlValue(builder->slice(), builder->size());
  }
  if (separator->isObject()) {
    return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
  }

  // Get ready for ICU
  auto buffer = ThreadLocalStringLeaser::lease();
  velocypack::StringSink adapter(buffer.get());
  stringify(&ctx.vopts, adapter, value.slice());
  icu_64_64::UnicodeString valueToSplit(buffer->data(),
                                        static_cast<int32_t>(buffer->length()));
  bool isEmptyExpression = false;

  // the matcher is owned by the context!
  icu_64_64::RegexMatcher* matcher =
      ctx.cache.buildSplitMatcher(*separator, &ctx.vopts, isEmptyExpression);
  if (matcher == nullptr) {
    // compiling regular expression failed
    return Result{TRI_ERROR_QUERY_INVALID_REGEX};
  }
  abortMatchWhenCancelled(*matcher, ctx.cancellationToken);

  auto result = ThreadLocalBuilderLeaser::lease();
  result->openArray();
  if (!isEmptyExpression && buffer->empty()) {
    // Edge case: splitting an empty string by non-empty expression produces
    // an empty string again.
    result->add(VPackValue(""));
    result->close();
    return AqlValue(result->slice(), result->size());
  }

  std::string utf8;
  static const uint16_t nrResults = 16;
  icu_64_64::UnicodeString uResults[nrResults];
  int64_t totalCount = 0;
  while (true) {
    UErrorCode errorCode = U_ZERO_ERROR;
    auto uCount = matcher->split(valueToSplit, uResults, nrResults, errorCode);
    throwIfCancelled(ctx.cancellationToken);
    uint16_t copyThisTime = uCount;

    if (U_FAILURE(errorCode)) {
      return Result{TRI_ERROR_QUERY_INVALID_REGEX};
    }

    if (copyThisTime > nrResults) {
      // last hit is the remaining string to be fed into split in a subsequent
      // invocation
      copyThisTime--;
    }

    if ((copyThisTime > 0) &&
        ((copyThisTime == nrResults) || isEmptyExpression)) {
      // ICU will give us a trailing empty string we don't care for if we split
      // with empty strings.
      copyThisTime--;
    }

    int64_t i = 0;
    while ((i < copyThisTime) &&
           ((limitNumber < 0) || (totalCount < limitNumber))) {
      if ((i == 0) && isEmptyExpression) {
        // ICU will give us an empty string that we don't care for
        // as first value of one match-chunk
        i++;
        continue;
      }
      uResults[i].toUTF8String(utf8);
      result->add(VPackValue(utf8));
      utf8.clear();
      i++;
      totalCount++;
    }

    if (((uCount != nrResults)) ||  // fetch any / found less then N
        ((limitNumber >= 0) && (totalCount >= limitNumber))) {  // fetch N
      break;
    }
    // ok, we have more to parse in the last result slot, reiterate with it:
    if (uCount == nrResults) {
      valueToSplit = uResults[nrResults - 1];
    } else {
      // should not go beyound the last match!
      TRI_ASSERT(false);
      break;
    }
  }

  result->close();
  return AqlValue(result->slice(), result->size());
}

/// @brief function SUBSTITUTE
ResultT<AqlValue> functions::substitute(StringFunctionContext const& ctx,
                                        AqlValue const& value,
                                        AqlValue const& search,
                                        AqlValue const* replace,
                                        AqlValue const* limit) {
  auto const& vopts = ctx.vopts;
  int64_t maxReplacements = -1;
  AqlValueMaterializer materializer(&vopts);
  std::vector<icu_64_64::UnicodeString> matchPatterns;
  std::vector<icu_64_64::UnicodeString> replacePatterns;

  if (search.isObject()) {
    // with a mapping object, the third argument is the limit
    if (limit != nullptr) {
      return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH};
    }
    if (replace != nullptr) {
      maxReplacements = replace->toInt64();
    }
    VPackSlice slice = materializer.slice(search);
    matchPatterns.reserve(slice.length());
    replacePatterns.reserve(slice.length());
    for (auto it :
         VPackObjectIterator(slice, /*useSequentialIteration*/ true)) {
      velocypack::ValueLength length;
      char const* str = it.key.getString(length);
      matchPatterns.push_back(
          icu_64_64::UnicodeString(str, static_cast<int32_t>(length)));
      if (it.value.isNull()) {
        // null replacement value => replace with an empty string
        replacePatterns.push_back(icu_64_64::UnicodeString());
      } else if (it.value.isString()) {
        // string case
        str = it.value.getStringUnchecked(length);
        replacePatterns.push_back(
            icu_64_64::UnicodeString(str, static_cast<int32_t>(length)));
      } else {
        // non strings
        return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
      }
    }
  } else {
    if (limit != nullptr) {
      maxReplacements = limit->toInt64();
    }

    VPackSlice slice = materializer.slice(search);
    if (search.isArray()) {
      for (VPackSlice it : VPackArrayIterator(slice)) {
        if (it.isString()) {
          velocypack::ValueLength length;
          char const* str = it.getStringUnchecked(length);
          matchPatterns.push_back(
              icu_64_64::UnicodeString(str, static_cast<int32_t>(length)));
        } else {
          return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
        }
      }
    } else {
      if (!search.isString()) {
        return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
      }
      velocypack::ValueLength length;

      char const* str = slice.getStringUnchecked(length);
      matchPatterns.push_back(
          icu_64_64::UnicodeString(str, static_cast<int32_t>(length)));
    }
    if (replace != nullptr) {
      AqlValueMaterializer materializer2(&vopts);
      VPackSlice rslice = materializer2.slice(*replace);
      if (replace->isArray()) {
        for (VPackSlice it : VPackArrayIterator(rslice)) {
          if (it.isNull()) {
            // null replacement value => replace with an empty string
            replacePatterns.push_back(icu_64_64::UnicodeString());
          } else if (it.isString()) {
            velocypack::ValueLength length;
            char const* str = it.getStringUnchecked(length);
            replacePatterns.push_back(
                icu_64_64::UnicodeString(str, static_cast<int32_t>(length)));
          } else {
            return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
          }
        }
      } else if (replace->isString()) {
        // a plain string replaces every search pattern
        velocypack::ValueLength length;
        char const* str = rslice.getString(length);
        replacePatterns.assign(
            matchPatterns.size(),
            icu_64_64::UnicodeString(str, static_cast<int32_t>(length)));
      } else {
        return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
      }
    }
  }

  auto const text = stringOf(vopts, value);
  if (maxReplacements == 0 || matchPatterns.empty()) {
    return AqlValue(std::string_view{*text});
  }
  icu_64_64::UnicodeString unicodeStr(text->data(),
                                      static_cast<int32_t>(text->size()));

  // we can't copy the search instances, thus use pointers.
  // ICU's StringSearch rejects empty patterns and texts
  std::vector<std::unique_ptr<icu_64_64::StringSearch>> searchVec;
  searchVec.reserve(matchPatterns.size());
  UErrorCode status = U_ZERO_ERROR;
  for (auto const& searchStr : matchPatterns) {
    if (searchStr.isEmpty() || unicodeStr.isEmpty()) {
      searchVec.push_back(nullptr);
      continue;
    }
    searchVec.push_back(std::make_unique<icu_64_64::StringSearch>(
        searchStr, unicodeStr, ctx.locale(), nullptr, status));
    if (U_FAILURE(status)) {
      return icuError(status);
    }
  }

  // pair of the position and length of the found match
  using Match = std::pair<int32_t, int32_t>;
  auto const fromSearch = [&](size_t which, int32_t pos) -> Match {
    if (pos == USEARCH_DONE) {
      return {pos, 0};
    }
    return {pos, searchVec[which]->getMatchedLength()};
  };
  auto const firstMatch = [&](size_t which) -> Match {
    if (searchVec[which] == nullptr) {
      return {matchPatterns[which].isEmpty() ? 0 : USEARCH_DONE, 0};
    }
    return fromSearch(which, searchVec[which]->first(status));
  };
  auto const nextMatch = [&](size_t which, int32_t pos) -> Match {
    if (searchVec[which] == nullptr) {
      return {pos < unicodeStr.length() ? unicodeStr.moveIndex32(pos, 1)
                                        : USEARCH_DONE,
              0};
    }
    return fromSearch(which, searchVec[which]->next(status));
  };
  auto const matchFrom = [&](size_t which, int32_t pos) -> Match {
    if (searchVec[which] == nullptr) {
      return {pos, 0};
    }
    if (pos >= unicodeStr.length()) {
      return {USEARCH_DONE, 0};
    }
    return fromSearch(which, searchVec[which]->following(pos, status));
  };

  std::vector<Match> srchResultPtrs;
  std::string utf8;
  srchResultPtrs.reserve(matchPatterns.size());
  for (size_t i = 0; i < matchPatterns.size(); ++i) {
    // We now find the first hit for each search string.
    srchResultPtrs.push_back(firstMatch(i));
    if (U_FAILURE(status)) {
      return icuError(status);
    }
  }

  icu_64_64::UnicodeString result;
  int32_t lastStart = 0;
  for (int64_t count = 0; maxReplacements == -1 || count < maxReplacements;
       ++count) {
    auto best = std::ranges::min_element(
        srchResultPtrs, [](Match const& a, Match const& b) {
          return a.first != USEARCH_DONE &&
                 (b.first == USEARCH_DONE || a.first < b.first);
        });
    if (best->first == USEARCH_DONE) {
      break;
    }
    size_t const which = best - srchResultPtrs.begin();
    auto const [pos, len] = *best;

    result.append(unicodeStr, lastStart, pos - lastStart);
    if (which < replacePatterns.size()) {
      result.append(replacePatterns[which]);
    }
    lastStart = pos + len;

    srchResultPtrs[which] = nextMatch(which, pos);
    for (size_t i = 0; i < srchResultPtrs.size() && U_SUCCESS(status); ++i) {
      if (srchResultPtrs[i].first != USEARCH_DONE &&
          srchResultPtrs[i].first < lastStart) {
        srchResultPtrs[i] = matchFrom(i, lastStart);
      }
    }
    if (U_FAILURE(status)) {
      return icuError(status);
    }
  }
  // Append from the last found:
  result.append(unicodeStr, lastStart, unicodeStr.length() - lastStart);

  result.toUTF8String(utf8);
  return AqlValue(utf8);
}

}  // namespace arangodb::aql
