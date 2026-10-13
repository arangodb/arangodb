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
#include <concepts>
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

ResultT<std::vector<UChar32>> parseWhitespace(
    functions::VPackFunctionParametersView parameters,
    velocypack::Options const& vopts) {
  if (parameters.size() == 2) {
    return toCodePoints(toUnicodeString(
        vopts, functions::extractFunctionParameterValue(parameters, 1)));
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

AqlValue functions::callStringFunction(ExpressionContext* ctx,
                                       StringFunctionsName name,
                                       VPackFunctionParametersView parameters) {
  auto& trx = ctx->trx();
  auto& cache = ctx->functionsCache();
  StringFunctionContext env{trx.vpackOptions(), cache.locale(trx.vocbase()),
                            cache, ctx};
  auto result = runStringFunction(name, parameters, env);
  if (result.ok()) {
    return std::move(*result);
  }
  // the ErrorCode overload formats mismatch messages with the function name
  if (result.errorNumber() == TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH ||
      result.errorNumber() ==
          TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH) {
    registerWarning(ctx, functionName(name), result.errorNumber());
  } else {
    registerWarning(ctx, functionName(name), result.result());
  }
  return AqlValue(AqlValueHintNull());
}

namespace functions {
namespace {

/// @brief function TO_STRING
class ToString {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    appendString(*_text, env.vopts,
                 extractFunctionParameterValue(parameters, 0));
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const&) const {
    return AqlValue(std::string_view{*_text});
  }

 private:
  ThreadLocalStringLeaser::Lease _text = ThreadLocalStringLeaser::lease();
};

/// @brief function TO_CHAR
class ToChar {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const&) {
    AqlValue const& value = extractFunctionParameterValue(parameters, 0);

    int64_t v = -1;
    if (ADB_UNLIKELY(value.isNumber())) {
      v = value.toInt64();
    }
    if (v < 0) {
      return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
    }
    _codePoint = static_cast<uint32_t>(v);
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const&) const {
    char buffer[6];
    int32_t offset = 0;
    U8_APPEND_UNSAFE(&buffer[0], offset, _codePoint);

    return AqlValue(std::string_view(&buffer[0], static_cast<size_t>(offset)));
  }

 private:
  UChar32 _codePoint = 0;
};

/// @brief function REPEAT
class Repeat {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    _value = &extractFunctionParameterValue(parameters, 0);
    _repetitions = extractFunctionParameterValue(parameters, 1).toInt64();
    if (_repetitions < 0) {
      return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
    }
    if (parameters.size() > 2) {
      appendString(*_separator, env.vopts,
                   extractFunctionParameterValue(parameters, 2));
    }
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const& env) const {
    bool const emptyValue =
        _value->isNull(true) ||
        (_value->isString() && _value->slice().getStringLength() == 0);
    if (_repetitions == 0 || (emptyValue && _separator->empty())) {
      return AqlValue(std::string_view("", 0));
    }

    auto buffer = ThreadLocalStringLeaser::lease();
    velocypack::SizeConstrainedStringSink adapter(
        buffer.get(),
        /*maxLength*/ 16 * 1024 * 1024);

    for (int64_t i = 0; i < _repetitions; ++i) {
      if (i > 0 && !_separator->empty()) {
        adapter.append(*_separator);
      }
      appendAsString(env.vopts, adapter, *_value);
      if (adapter.overflowed()) {
        return Result{
            TRI_ERROR_RESOURCE_LIMIT,
            "Output string of AQL REPEAT function was limited to 16MB."};
      }
    }

    return AqlValue(std::string_view(buffer->data(), buffer->size()));
  }

 private:
  AqlValue const* _value = nullptr;
  int64_t _repetitions = 0;
  ThreadLocalStringLeaser::Lease _separator = ThreadLocalStringLeaser::lease();
};

struct StringSearchArguments {
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    text = toUnicodeString(env.vopts,
                           extractFunctionParameterValue(parameters, 0));
    search = toUnicodeString(env.vopts,
                             extractFunctionParameterValue(parameters, 1));
    if (parameters.size() >= 3) {
      start = extractFunctionParameterValue(parameters, 2).toInt64();
    }
    if (parameters.size() == 4) {
      AqlValue const& optionalEnd =
          extractFunctionParameterValue(parameters, 3);
      if (!optionalEnd.isNull(true)) {
        end = optionalEnd.toInt64();
      }
    }
    return {};
  }

  icu_64_64::UnicodeString text;
  icu_64_64::UnicodeString search;
  int64_t start = 0;
  std::optional<int64_t> end;
};

/// @brief function FIND_FIRST
/// FIND_FIRST(text, search, start, end) → position
class FindFirst {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    return _arguments.parse(parameters, env);
  }

  ResultT<AqlValue> execute(StringFunctionContext const& env) const {
    auto const& [text, search, start, end] = _arguments;
    if (start < 0) {
      return AqlValue(AqlValueHintInt(-1));
    }
    int64_t maxEnd = text.length();
    if (end.has_value()) {
      maxEnd = *end;
      if ((maxEnd < start) || (maxEnd < 0)) {
        return AqlValue(AqlValueHintInt(-1));
      }
    }

    auto const pos = searchInRange(env.locale, text, search, start, maxEnd,
                                   SearchMatch::kFirst);
    if (pos.fail()) {
      return pos.result();
    }
    return AqlValue(AqlValueHintInt(*pos));
  }

 private:
  StringSearchArguments _arguments;
};

/// @brief function FIND_LAST
/// FIND_LAST(text, search, start, end) → position
class FindLast {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    return _arguments.parse(parameters, env);
  }

  ResultT<AqlValue> execute(StringFunctionContext const& env) const {
    auto const& [text, search, start, end] = _arguments;
    if (start < 0) {
      return AqlValue(AqlValueHintInt(-1));
    }
    int64_t maxEnd = text.length();
    int emptySearchCludge = 0;
    if (end.has_value()) {
      maxEnd = *end;
      if ((maxEnd < start) || (maxEnd < 0)) {
        return AqlValue(AqlValueHintInt(-1));
      }
      emptySearchCludge = 1;
    }

    if (search.isEmpty()) {
      return AqlValue(AqlValueHintInt(maxEnd + emptySearchCludge));
    }

    auto const pos = searchInRange(env.locale, text, search, start, maxEnd,
                                   SearchMatch::kLast);
    if (pos.fail()) {
      return pos.result();
    }
    return AqlValue(AqlValueHintInt(*pos));
  }

 private:
  StringSearchArguments _arguments;
};

/// @brief function CONCAT_SEPARATOR
class ConcatSeparator {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    appendString(_separator, env.vopts,
                 extractFunctionParameterValue(parameters, 0));
    _members = parameters.subspan(std::min<size_t>(1, parameters.size()));
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const& env) const {
    auto const& vopts = env.vopts;
    auto buffer = ThreadLocalStringLeaser::lease();
    velocypack::StringSink adapter(buffer.get());

    bool found = false;

    if (_members.size() == 1 && _members[0].isArray()) {
      AqlValue const& member = _members[0];
      // reserve *some* space
      buffer->reserve((_separator.size() + 10) * member.length());

      AqlValueMaterializer materializer(&vopts);
      VPackSlice slice = materializer.slice(member);

      for (VPackSlice it : VPackArrayIterator(slice)) {
        if (it.isNull()) {
          continue;
        }
        if (found) {
          buffer->append(_separator);
        }
        // convert member to a string and append
        appendAsString(vopts, adapter, AqlValue(it.begin()));
        found = true;
      }
      return AqlValue(std::string_view{buffer->data(), buffer->length()});
    }

    // reserve *some* space
    buffer->reserve((_separator.size() + 10) * (_members.size() + 1));
    for (AqlValue const& member : _members) {
      if (member.isNull(true)) {
        continue;
      }
      if (found) {
        buffer->append(_separator);
      }

      // convert member to a string and append
      appendAsString(vopts, adapter, member);
      found = true;
    }

    return AqlValue(std::string_view{buffer->data(), buffer->length()});
  }

 private:
  std::string _separator;
  VPackFunctionParametersView _members;
};

/// @brief function CHAR_LENGTH
class CharLength {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const&) {
    _value = &extractFunctionParameterValue(parameters, 0);
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const& env) const {
    auto* vopts = &env.vopts;
    AqlValue const& value = *_value;
    size_t length = 0;

    if (value.isArray() || value.isObject()) {
      AqlValueMaterializer materializer(vopts);
      VPackSlice slice = materializer.slice(value);

      auto buffer = ThreadLocalStringLeaser::lease();
      velocypack::StringSink adapter(buffer.get());

      VPackDumper dumper(&adapter, vopts);
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

 private:
  AqlValue const* _value = nullptr;
};

/// @brief function LOWER
class Lower {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    appendString(*_text, env.vopts,
                 extractFunctionParameterValue(parameters, 0));
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const& env) const {
    icu_64_64::UnicodeString unicodeStr(_text->data(),
                                        static_cast<int32_t>(_text->length()));
    unicodeStr.toLower(env.locale);
    std::string utf8;
    unicodeStr.toUTF8String(utf8);
    return AqlValue(utf8);
  }

 private:
  ThreadLocalStringLeaser::Lease _text = ThreadLocalStringLeaser::lease();
};

/// @brief function UPPER
class Upper {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    appendString(*_text, env.vopts,
                 extractFunctionParameterValue(parameters, 0));
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const& env) const {
    icu_64_64::UnicodeString unicodeStr(_text->data(),
                                        static_cast<int32_t>(_text->length()));
    unicodeStr.toUpper(env.locale);
    std::string utf8;
    unicodeStr.toUTF8String(utf8);
    return AqlValue(utf8);
  }

 private:
  ThreadLocalStringLeaser::Lease _text = ThreadLocalStringLeaser::lease();
};

/// @brief function SUBSTRING
class Substring {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    appendString(*_text, env.vopts,
                 extractFunctionParameterValue(parameters, 0));
    _offset = static_cast<int32_t>(
        extractFunctionParameterValue(parameters, 1).toInt64());
    if (parameters.size() == 3) {
      _length = static_cast<int32_t>(
          extractFunctionParameterValue(parameters, 2).toInt64());
    }
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const&) const {
    icu_64_64::UnicodeString unicodeStr(_text->data(),
                                        static_cast<int32_t>(_text->length()));
    int32_t offset = _offset;
    if (offset < 0) {
      offset = unicodeStr.moveIndex32(
          unicodeStr.moveIndex32(unicodeStr.length(), 0), offset);
    } else {
      offset = unicodeStr.moveIndex32(0, offset);
    }

    std::string utf8;
    unicodeStr
        .tempSubString(offset, unicodeStr.moveIndex32(offset, _length) - offset)
        .toUTF8String(utf8);

    return AqlValue(utf8);
  }

 private:
  ThreadLocalStringLeaser::Lease _text = ThreadLocalStringLeaser::lease();
  int32_t _offset = 0;
  int32_t _length = INT32_MAX;
};

/// @brief function SUBSTRING_BYTES
class SubstringBytes {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const&) {
    AqlValue const& value = extractFunctionParameterValue(parameters, 0);
    if (!value.isString()) {
      return Result{TRI_ERROR_BAD_PARAMETER};
    }

    _text = value.slice().stringView();
    if (_text.empty()) {
      return {};
    }

    auto const strLen = static_cast<int64_t>(_text.size());
    _length = strLen;
    switch (parameters.size()) {
      case 5:
        _right = extractFunctionParameterValue(parameters, 4).toInt64();
        [[fallthrough]];
      case 4:
        _left = extractFunctionParameterValue(parameters, 3).toInt64();
        if (parameters.size() == 4) {
          _right = _left;
        }
        [[fallthrough]];
      case 3:
        _length = extractFunctionParameterValue(parameters, 2).toInt64();
        if (_length < 0) {
          _length = 0;
        }
        [[fallthrough]];
      case 2:
        _offset = extractFunctionParameterValue(parameters, 1).toInt64();
        if (_offset < 0) {
          _offset = std::max(int64_t{0}, strLen + _offset);
        } else {
          _offset = std::min(_offset, strLen);
        }
        break;
      default:
        ADB_UNREACHABLE;
    }
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const&) const {
    if (_text.empty()) {
      return AqlValue{velocypack::Slice::emptyStringSlice()};
    }

    auto const strLen = static_cast<int64_t>(_text.size());
    auto* const begin = reinterpret_cast<irs::byte_type const*>(_text.data());
    auto* const end = begin + _text.size();

    auto* lhsIt = begin + _offset;
    auto* rhsIt = lhsIt + std::min(_length, strLen - _offset);

    static constexpr auto kMaskBits = 0xC0U;
    static constexpr auto kHelpByte = 0x80U;

    if ((lhsIt != end && (*lhsIt & kMaskBits) == kHelpByte) ||
        (rhsIt != end && (*rhsIt & kMaskBits) == kHelpByte)) {
      return Result{TRI_ERROR_BAD_PARAMETER};
    }

    auto shift = [&](auto limit, auto& it, auto to, auto update) {
      while (limit > 0 && it != to) {
        limit -= static_cast<int64_t>((*it & kMaskBits) != kHelpByte);
        it += update;
      }
      if (it == end) {
        return;
      }
      for (; it != to && (*it & kMaskBits) == kHelpByte;) {
        it += update;
      }
    };

    shift(_left, lhsIt, begin, -1);
    shift(_right, rhsIt, end, 1);

    return AqlValue{std::string_view{reinterpret_cast<char const*>(lhsIt),
                                     reinterpret_cast<char const*>(rhsIt)}};
  }

 private:
  std::string_view _text;
  int64_t _offset = 0;
  int64_t _length = 0;
  int64_t _left = 0;
  int64_t _right = 0;
};

/// @brief function LEFT str, length
class Left {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    appendString(*_text, env.vopts,
                 extractFunctionParameterValue(parameters, 0));
    _length = static_cast<int32_t>(
        extractFunctionParameterValue(parameters, 1).toInt64());
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const&) const {
    icu_64_64::UnicodeString unicodeStr(_text->data(),
                                        static_cast<int32_t>(_text->length()));
    icu_64_64::UnicodeString left =
        unicodeStr.tempSubString(0, unicodeStr.moveIndex32(0, _length));

    std::string utf8;
    left.toUTF8String(utf8);
    return AqlValue(utf8);
  }

 private:
  ThreadLocalStringLeaser::Lease _text = ThreadLocalStringLeaser::lease();
  uint32_t _length = 0;
};

/// @brief function RIGHT
class Right {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    appendString(*_text, env.vopts,
                 extractFunctionParameterValue(parameters, 0));
    _length = static_cast<int32_t>(
        extractFunctionParameterValue(parameters, 1).toInt64());
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const&) const {
    icu_64_64::UnicodeString unicodeStr(_text->data(),
                                        static_cast<int32_t>(_text->length()));
    icu_64_64::UnicodeString right =
        unicodeStr.tempSubString(unicodeStr.moveIndex32(
            unicodeStr.length(), -static_cast<int32_t>(_length)));

    std::string utf8;
    right.toUTF8String(utf8);
    return AqlValue(utf8);
  }

 private:
  ThreadLocalStringLeaser::Lease _text = ThreadLocalStringLeaser::lease();
  uint32_t _length = 0;
};

/// @brief function TRIM
class Trim {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    appendString(*_text, env.vopts,
                 extractFunctionParameterValue(parameters, 0));
    icu_64_64::UnicodeString whitespace(kDefaultWhitespace);

    if (parameters.size() == 2) {
      AqlValue const& optional = extractFunctionParameterValue(parameters, 1);

      if (optional.isNumber()) {
        _howToTrim = optional.toInt64();

        if (_howToTrim < 0 || 2 < _howToTrim) {
          _howToTrim = 0;
        }
      } else if (optional.isString()) {
        whitespace = toUnicodeString(env.vopts, optional);
      }
    }

    auto codePoints = toCodePoints(whitespace);
    if (codePoints.fail()) {
      return std::move(codePoints).result();
    }
    _whitespace = std::move(*codePoints);
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const&) const {
    return trimmed(*_text, _whitespace, /*left*/ _howToTrim <= 1,
                   /*right*/ _howToTrim == 2 || _howToTrim == 0);
  }

 private:
  ThreadLocalStringLeaser::Lease _text = ThreadLocalStringLeaser::lease();
  std::vector<UChar32> _whitespace;
  int64_t _howToTrim = 0;
};

/// @brief function LTRIM
class LTrim {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    appendString(*_text, env.vopts,
                 extractFunctionParameterValue(parameters, 0));
    auto codePoints = parseWhitespace(parameters, env.vopts);
    if (codePoints.fail()) {
      return std::move(codePoints).result();
    }
    _whitespace = std::move(*codePoints);
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const&) const {
    return trimmed(*_text, _whitespace, /*left*/ true, /*right*/ false);
  }

 private:
  ThreadLocalStringLeaser::Lease _text = ThreadLocalStringLeaser::lease();
  std::vector<UChar32> _whitespace;
};

/// @brief function RTRIM
class RTrim {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    appendString(*_text, env.vopts,
                 extractFunctionParameterValue(parameters, 0));
    auto codePoints = parseWhitespace(parameters, env.vopts);
    if (codePoints.fail()) {
      return std::move(codePoints).result();
    }
    _whitespace = std::move(*codePoints);
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const&) const {
    return trimmed(*_text, _whitespace, /*left*/ false, /*right*/ true);
  }

 private:
  ThreadLocalStringLeaser::Lease _text = ThreadLocalStringLeaser::lease();
  std::vector<UChar32> _whitespace;
};

/// @brief function CONTAINS
class Contains {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    _text = toUnicodeString(env.vopts,
                            extractFunctionParameterValue(parameters, 0));
    _search = toUnicodeString(env.vopts,
                              extractFunctionParameterValue(parameters, 1));
    _returnIndex = extractFunctionParameterValue(parameters, 2).toBoolean();
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const& env) const {
    auto const pos = searchInRange(env.locale, _text, _search, 0,
                                   _text.length(), SearchMatch::kFirst);
    if (pos.fail()) {
      return pos.result();
    }
    if (_returnIndex) {
      return AqlValue(AqlValueHintInt(*pos));
    }
    return AqlValue(AqlValueHintBool(*pos != -1));
  }

 private:
  icu_64_64::UnicodeString _text;
  icu_64_64::UnicodeString _search;
  bool _returnIndex = false;
};

/// @brief function CONCAT
class Concat {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const&) {
    _members = parameters;
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const& env) const {
    auto const& vopts = env.vopts;
    auto buffer = ThreadLocalStringLeaser::lease();
    velocypack::StringSink adapter(buffer.get());

    if (_members.size() == 1 && _members[0].isArray()) {
      AqlValueMaterializer materializer(&vopts);
      VPackSlice slice = materializer.slice(_members[0]);

      for (VPackSlice it : VPackArrayIterator(slice)) {
        if (it.isNull()) {
          continue;
        }
        // convert member to a string and append
        appendAsString(vopts, adapter, AqlValue(it.begin()));
      }
      return AqlValue(std::string_view{buffer->data(), buffer->length()});
    }

    for (AqlValue const& member : _members) {
      if (member.isNull(true)) {
        continue;
      }

      // convert member to a string and append
      appendAsString(vopts, adapter, member);
    }

    return AqlValue(std::string_view{buffer->data(), buffer->length()});
  }

 private:
  VPackFunctionParametersView _members;
};

/// @brief function LIKE
class Like {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    bool const caseInsensitive = getBooleanParameter(parameters, 2, false);
    auto pattern = ThreadLocalStringLeaser::lease();
    appendString(*pattern, env.vopts,
                 extractFunctionParameterValue(parameters, 1));

    // the matcher is owned by the context!
    _matcher = env.cache.buildLikeMatcher(*pattern, caseInsensitive);
    if (_matcher == nullptr) {
      // compiling regular expression failed
      return Result{TRI_ERROR_QUERY_INVALID_REGEX};
    }
    abortMatchWhenCancelled(*_matcher, env.cancellationToken);

    appendString(*_text, env.vopts,
                 extractFunctionParameterValue(parameters, 0));
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const& env) const {
    bool error = false;
    bool const result = basics::Utf8Helper::DefaultUtf8Helper.matches(
        _matcher, _text->data(), _text->length(), false, error);

    if (error) {
      throwIfCancelled(env.cancellationToken);
      // compiling regular expression failed
      return Result{TRI_ERROR_QUERY_INVALID_REGEX};
    }

    return AqlValue(AqlValueHintBool(result));
  }

 private:
  ThreadLocalStringLeaser::Lease _text = ThreadLocalStringLeaser::lease();
  icu_64_64::RegexMatcher* _matcher = nullptr;
};

/// @brief function SPLIT
class Split {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    // cheapest parameter checks first:
    if (parameters.size() == 3) {
      AqlValue const& aqlLimit = extractFunctionParameterValue(parameters, 2);
      if (!aqlLimit.isNumber()) {
        return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
      }
      _limit = aqlLimit.toInt64();
      if (*_limit <= 0) {
        return {};
      }
    }

    _value = &extractFunctionParameterValue(parameters, 0);
    if (parameters.size() == 1) {
      return {};
    }

    AqlValue const& separator = extractFunctionParameterValue(parameters, 1);
    if (separator.isObject()) {
      return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
    }

    velocypack::StringSink adapter(_text.get());
    stringify(&env.vopts, adapter, _value->slice());

    // the matcher is owned by the context!
    _matcher =
        env.cache.buildSplitMatcher(separator, &env.vopts, _isEmptyExpression);
    if (_matcher == nullptr) {
      // compiling regular expression failed
      return Result{TRI_ERROR_QUERY_INVALID_REGEX};
    }
    abortMatchWhenCancelled(*_matcher, env.cancellationToken);
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const& env) const {
    // these are edge cases which are documented to have these return values:
    if (_limit.has_value() && *_limit < 0) {
      return AqlValue(AqlValueHintNull());
    }
    if (_limit.has_value() && *_limit == 0) {
      return AqlValue(AqlValueHintEmptyArray());
    }

    if (_matcher == nullptr) {
      // pre-documented edge-case: if we only have the first parameter, return
      // it.
      auto builder = ThreadLocalBuilderLeaser::lease();
      builder->openArray();
      builder->add(_value->slice());
      builder->close();
      return AqlValue(builder->slice(), builder->size());
    }

    int64_t const limitNumber = _limit.value_or(-1);
    icu_64_64::UnicodeString valueToSplit(
        _text->data(), static_cast<int32_t>(_text->length()));

    auto result = ThreadLocalBuilderLeaser::lease();
    result->openArray();
    if (!_isEmptyExpression && _text->empty()) {
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
      auto uCount =
          _matcher->split(valueToSplit, uResults, nrResults, errorCode);
      throwIfCancelled(env.cancellationToken);
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
          ((copyThisTime == nrResults) || _isEmptyExpression)) {
        // ICU will give us a trailing empty string we don't care for if we
        // split with empty strings.
        copyThisTime--;
      }

      int64_t i = 0;
      while ((i < copyThisTime) &&
             ((limitNumber < 0) || (totalCount < limitNumber))) {
        if ((i == 0) && _isEmptyExpression) {
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

 private:
  ThreadLocalStringLeaser::Lease _text = ThreadLocalStringLeaser::lease();
  AqlValue const* _value = nullptr;
  icu_64_64::RegexMatcher* _matcher = nullptr;
  std::optional<int64_t> _limit;
  bool _isEmptyExpression = false;
};

class Substitute {
 public:
  Result parse(VPackFunctionParametersView parameters,
               StringFunctionContext const& env) {
    auto const& vopts = env.vopts;
    AqlValue const& search =
        aql::functions::extractFunctionParameterValue(parameters, 1);
    AqlValueMaterializer materializer(&vopts);

    if (search.isObject()) {
      if (parameters.size() > 3) {
        return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH};
      }
      if (parameters.size() == 3) {
        _limit = aql::functions::extractFunctionParameterValue(parameters, 2)
                     .toInt64();
      }
      VPackSlice slice = materializer.slice(search);
      _search.reserve(slice.length());
      _replace.reserve(slice.length());
      for (auto it :
           VPackObjectIterator(slice, /*useSequentialIteration*/ true)) {
        velocypack::ValueLength length;
        char const* str = it.key.getString(length);
        _search.push_back(
            icu_64_64::UnicodeString(str, static_cast<int32_t>(length)));
        if (it.value.isNull()) {
          // null replacement value => replace with an empty string
          _replace.push_back(icu_64_64::UnicodeString());
        } else if (it.value.isString()) {
          // string case
          str = it.value.getStringUnchecked(length);
          _replace.push_back(
              icu_64_64::UnicodeString(str, static_cast<int32_t>(length)));
        } else {
          // non strings
          return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
        }
      }
    } else {
      if (parameters.size() < 2) {
        return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH};
      }
      if (parameters.size() == 4) {
        _limit = aql::functions::extractFunctionParameterValue(parameters, 3)
                     .toInt64();
      }

      VPackSlice slice = materializer.slice(search);
      if (search.isArray()) {
        for (VPackSlice it : VPackArrayIterator(slice)) {
          if (it.isString()) {
            velocypack::ValueLength length;
            char const* str = it.getStringUnchecked(length);
            _search.push_back(
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
        _search.push_back(
            icu_64_64::UnicodeString(str, static_cast<int32_t>(length)));
      }
      if (parameters.size() > 2) {
        AqlValue const& replace =
            aql::functions::extractFunctionParameterValue(parameters, 2);
        AqlValueMaterializer materializer2(&vopts);
        VPackSlice rslice = materializer2.slice(replace);
        if (replace.isArray()) {
          for (VPackSlice it : VPackArrayIterator(rslice)) {
            if (it.isNull()) {
              // null replacement value => replace with an empty string
              _replace.push_back(icu_64_64::UnicodeString());
            } else if (it.isString()) {
              velocypack::ValueLength length;
              char const* str = it.getStringUnchecked(length);
              _replace.push_back(
                  icu_64_64::UnicodeString(str, static_cast<int32_t>(length)));
            } else {
              return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
            }
          }
        } else if (replace.isString()) {
          // a plain string replaces every search pattern
          velocypack::ValueLength length;
          char const* str = rslice.getString(length);
          _replace.assign(
              _search.size(),
              icu_64_64::UnicodeString(str, static_cast<int32_t>(length)));
        } else {
          return Result{TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH};
        }
      }
    }

    appendString(*_text, vopts,
                 aql::functions::extractFunctionParameterValue(parameters, 0));
    return {};
  }

  ResultT<AqlValue> execute(StringFunctionContext const& env) const {
    if (_limit == 0 || _search.empty()) {
      return AqlValue(std::string_view{*_text});
    }
    icu_64_64::UnicodeString unicodeStr(_text->data(),
                                        static_cast<int32_t>(_text->size()));

    // we can't copy the search instances, thus use pointers.
    // ICU's StringSearch rejects empty patterns and texts
    std::vector<std::unique_ptr<icu_64_64::StringSearch>> searchVec;
    searchVec.reserve(_search.size());
    UErrorCode status = U_ZERO_ERROR;
    for (auto const& searchStr : _search) {
      if (searchStr.isEmpty() || unicodeStr.isEmpty()) {
        searchVec.push_back(nullptr);
        continue;
      }
      searchVec.push_back(std::make_unique<icu_64_64::StringSearch>(
          searchStr, unicodeStr, env.locale, nullptr, status));
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
        return {_search[which].isEmpty() ? 0 : USEARCH_DONE, 0};
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
    srchResultPtrs.reserve(_search.size());
    for (size_t i = 0; i < _search.size(); ++i) {
      // We now find the first hit for each search string.
      srchResultPtrs.push_back(firstMatch(i));
      if (U_FAILURE(status)) {
        return icuError(status);
      }
    }

    icu_64_64::UnicodeString result;
    int32_t lastStart = 0;
    for (int64_t count = 0; _limit == -1 || count < _limit; ++count) {
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
      if (which < _replace.size()) {
        result.append(_replace[which]);
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

 private:
  ThreadLocalStringLeaser::Lease _text = ThreadLocalStringLeaser::lease();
  std::vector<icu_64_64::UnicodeString> _search;
  std::vector<icu_64_64::UnicodeString> _replace;
  int64_t _limit = -1;
};

// parse() may keep views into the parameters; instances live for one call
template<typename T>
concept StringFunction = std::default_initializable<T> &&
    requires(T fn, T const& cfn, VPackFunctionParametersView parameters,
             StringFunctionContext const& env) {
  { fn.parse(parameters, env) } -> std::same_as<Result>;
  { cfn.execute(env) } -> std::same_as<ResultT<AqlValue>>;
};

template<StringFunction F>
ResultT<AqlValue> run(VPackFunctionParametersView parameters,
                      StringFunctionContext const& env) {
  F fn;
  if (auto result = fn.parse(parameters, env); result.fail()) {
    return result;
  }
  return fn.execute(env);
}

}  // namespace
}  // namespace functions

ResultT<AqlValue> functions::runStringFunction(
    StringFunctionsName name, VPackFunctionParametersView parameters,
    StringFunctionContext const& env) {
  switch (name) {
    case StringFunctionsName::kSubstitute:
      return run<Substitute>(parameters, env);
    case StringFunctionsName::kToString:
      return run<ToString>(parameters, env);
    case StringFunctionsName::kToChar:
      return run<ToChar>(parameters, env);
    case StringFunctionsName::kRepeat:
      return run<Repeat>(parameters, env);
    case StringFunctionsName::kFindFirst:
      return run<FindFirst>(parameters, env);
    case StringFunctionsName::kFindLast:
      return run<FindLast>(parameters, env);
    case StringFunctionsName::kConcatSeparator:
      return run<ConcatSeparator>(parameters, env);
    case StringFunctionsName::kCharLength:
      return run<CharLength>(parameters, env);
    case StringFunctionsName::kLower:
      return run<Lower>(parameters, env);
    case StringFunctionsName::kUpper:
      return run<Upper>(parameters, env);
    case StringFunctionsName::kSubstring:
      return run<Substring>(parameters, env);
    case StringFunctionsName::kSubstringBytes:
      return run<SubstringBytes>(parameters, env);
    case StringFunctionsName::kLeft:
      return run<Left>(parameters, env);
    case StringFunctionsName::kRight:
      return run<Right>(parameters, env);
    case StringFunctionsName::kTrim:
      return run<Trim>(parameters, env);
    case StringFunctionsName::kLTrim:
      return run<LTrim>(parameters, env);
    case StringFunctionsName::kRTrim:
      return run<RTrim>(parameters, env);
    case StringFunctionsName::kConcat:
      return run<Concat>(parameters, env);
    case StringFunctionsName::kContains:
      return run<Contains>(parameters, env);
    case StringFunctionsName::kLike:
      return run<Like>(parameters, env);
    case StringFunctionsName::kSplit:
      return run<Split>(parameters, env);
  }
  ADB_UNREACHABLE;
}

}  // namespace arangodb::aql
