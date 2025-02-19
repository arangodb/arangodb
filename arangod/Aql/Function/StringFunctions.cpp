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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/LanguageFeature.h"
#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
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

#include <absl/strings/str_cat.h>

#include <utils/utf8_utils.hpp>

#include <unicode/schriter.h>
#include <unicode/stsearch.h>
#include <unicode/uchar.h>
#include <unicode/unistr.h>

#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

using namespace arangodb;

namespace arangodb::aql {
namespace {

void registerICUWarning(ExpressionContext* expressionContext,
                        std::string_view functionName, UErrorCode status) {
  std::string msg = absl::StrCat("in function '", functionName, "()': ");
  msg.append(basics::Exception::FillExceptionString(TRI_ERROR_ARANGO_ICU_ERROR,
                                                    u_errorName_64_64(status)));
  expressionContext->registerWarning(TRI_ERROR_ARANGO_ICU_ERROR, msg);
}

void ltrimInternal(int32_t& startOffset, int32_t& endOffset,
                   icu_64_64::UnicodeString& unicodeStr,
                   uint32_t numWhitespaces, UChar32* spaceChars) {
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
  }  // for
}

void rtrimInternal(int32_t& startOffset, int32_t& endOffset,
                   icu_64_64::UnicodeString& unicodeStr,
                   uint32_t numWhitespaces, UChar32* spaceChars) {
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
  }  // for
}

}  // namespace

/// @brief function TO_STRING
AqlValue functions::ToString(ExpressionContext* expr, AstNode const&,
                             VPackFunctionParametersView parameters) {
  auto& trx = expr->trx();
  AqlValue const& value = extractFunctionParameterValue(parameters, 0);

  transaction::StringLeaser buffer(&trx);
  velocypack::StringSink adapter(buffer.get());

  appendAsString(trx.vpackOptions(), adapter, value);
  return AqlValue(std::string_view{buffer->data(), buffer->length()});
}

AqlValue functions::ToChar(ExpressionContext* ctx, AstNode const&,
                           VPackFunctionParametersView parameters) {
  static char const* AFN = "TO_CHAR";
  AqlValue const& value = extractFunctionParameterValue(parameters, 0);

  int64_t v = -1;
  if (ADB_UNLIKELY(value.isNumber())) {
    v = value.toInt64();
  }
  if (v < 0) {
    registerInvalidArgumentWarning(ctx, AFN);
    return aql::AqlValue{aql::AqlValueHintNull{}};
  }

  UChar32 c = static_cast<uint32_t>(v);
  char buffer[6];
  int32_t offset = 0;
  U8_APPEND_UNSAFE(&buffer[0], offset, c);

  return AqlValue(std::string_view(&buffer[0], static_cast<size_t>(offset)));
}

AqlValue functions::Repeat(ExpressionContext* ctx, AstNode const&,
                           VPackFunctionParametersView parameters) {
  static char const* AFN = "REPEAT";
  AqlValue const& value = extractFunctionParameterValue(parameters, 0);

  AqlValue const& repetitions = extractFunctionParameterValue(parameters, 1);
  int64_t r = repetitions.toInt64();
  if (r == 0) {
    // empty string
    return AqlValue(std::string_view("", 0));
  }
  if (r < 0) {
    // negative number of repetitions
    registerInvalidArgumentWarning(ctx, AFN);
    return aql::AqlValue{aql::AqlValueHintNull{}};
  }

  auto& trx = ctx->trx();

  transaction::StringLeaser sepBuffer(&trx);
  velocypack::StringSink sepAdapter(sepBuffer.get());
  std::string_view separator;
  if (parameters.size() > 2) {
    // separator
    appendAsString(trx.vpackOptions(), sepAdapter,
                   extractFunctionParameterValue(parameters, 2));
    separator = {sepBuffer->data(), sepBuffer->size()};
  }

  transaction::StringLeaser buffer(&trx);
  velocypack::SizeConstrainedStringSink adapter(buffer.get(),
                                                /*maxLength*/ 16 * 1024 * 1024);

  for (int64_t i = 0; i < r; ++i) {
    if (i > 0 && !separator.empty()) {
      buffer->append(separator);
    }
    appendAsString(trx.vpackOptions(), adapter, value);
    if (adapter.overflowed()) {
      registerWarning(
          ctx, AFN,
          Result{TRI_ERROR_RESOURCE_LIMIT,
                 "Output string of AQL REPEAT function was limited to 16MB."});
      return AqlValue(AqlValueHintNull());
    }
  }

  // hand over string to AqlValue
  return AqlValue(std::string_view(buffer->data(), buffer->size()));
}

/// @brief function FIND_FIRST
/// FIND_FIRST(text, search, start, end) → position
AqlValue functions::FindFirst(ExpressionContext* expressionContext,
                              AstNode const&,
                              VPackFunctionParametersView parameters) {
  static char const* AFN = "FIND_FIRST";

  auto* trx = &expressionContext->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& searchValue =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  transaction::StringLeaser buf1(trx);
  velocypack::StringSink adapter(buf1.get());
  appendAsString(vopts, adapter, value);
  icu_64_64::UnicodeString uBuf(buf1->data(),
                                static_cast<int32_t>(buf1->length()));

  transaction::StringLeaser buf2(trx);
  velocypack::StringSink adapter2(buf2.get());
  appendAsString(vopts, adapter2, searchValue);
  icu_64_64::UnicodeString uSearchBuf(buf2->data(),
                                      static_cast<int32_t>(buf2->length()));
  auto searchLen = uSearchBuf.length();

  int64_t startOffset = 0;
  int64_t maxEnd = -1;

  if (parameters.size() >= 3) {
    AqlValue const& optionalStartOffset =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    startOffset = optionalStartOffset.toInt64();
    if (startOffset < 0) {
      return AqlValue(AqlValueHintInt(-1));
    }
  }

  maxEnd = uBuf.length();
  if (parameters.size() == 4) {
    AqlValue const& optionalEndMax =
        aql::functions::extractFunctionParameterValue(parameters, 3);
    if (!optionalEndMax.isNull(true)) {
      maxEnd = optionalEndMax.toInt64();
      if ((maxEnd < startOffset) || (maxEnd < 0)) {
        return AqlValue(AqlValueHintInt(-1));
      }
    }
  }

  if (searchLen == 0) {
    return AqlValue(AqlValueHintInt(startOffset));
  }
  if (uBuf.length() == 0) {
    return AqlValue(AqlValueHintInt(-1));
  }

  auto& server = trx->vocbase().server();
  auto locale = server.getFeature<LanguageFeature>().getLocale();
  UErrorCode status = U_ZERO_ERROR;
  icu_64_64::StringSearch search(uSearchBuf, uBuf, locale, nullptr, status);

  for (int pos = search.first(status); U_SUCCESS(status) && pos != USEARCH_DONE;
       pos = search.next(status)) {
    if (U_FAILURE(status)) {
      registerICUWarning(expressionContext, AFN, status);
      return AqlValue(AqlValueHintNull());
    }
    if ((pos >= startOffset) && ((pos + searchLen - 1) <= maxEnd)) {
      return AqlValue(AqlValueHintInt(pos));
    }
  }
  return AqlValue(AqlValueHintInt(-1));
}

/// @brief function FIND_LAST
/// FIND_FIRST(text, search, start, end) → position
AqlValue functions::FindLast(ExpressionContext* expressionContext,
                             AstNode const&,
                             VPackFunctionParametersView parameters) {
  static char const* AFN = "FIND_LAST";

  auto* trx = &expressionContext->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& searchValue =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  transaction::StringLeaser buf1(trx);
  velocypack::StringSink adapter(buf1.get());
  appendAsString(vopts, adapter, value);
  icu_64_64::UnicodeString uBuf(buf1->data(),
                                static_cast<int32_t>(buf1->length()));

  transaction::StringLeaser buf2(trx);
  velocypack::StringSink adapter2(buf2.get());
  appendAsString(vopts, adapter2, searchValue);
  icu_64_64::UnicodeString uSearchBuf(buf2->data(),
                                      static_cast<int32_t>(buf2->length()));
  auto searchLen = uSearchBuf.length();

  int64_t startOffset = 0;
  int64_t maxEnd = -1;

  if (parameters.size() >= 3) {
    AqlValue const& optionalStartOffset =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    startOffset = optionalStartOffset.toInt64();
    if (startOffset < 0) {
      return AqlValue(AqlValueHintInt(-1));
    }
  }

  maxEnd = uBuf.length();
  int emptySearchCludge = 0;
  if (parameters.size() == 4) {
    AqlValue const& optionalEndMax =
        aql::functions::extractFunctionParameterValue(parameters, 3);
    if (!optionalEndMax.isNull(true)) {
      maxEnd = optionalEndMax.toInt64();
      if ((maxEnd < startOffset) || (maxEnd < 0)) {
        return AqlValue(AqlValueHintInt(-1));
      }
      emptySearchCludge = 1;
    }
  }

  if (searchLen == 0) {
    return AqlValue(AqlValueHintInt(maxEnd + emptySearchCludge));
  }
  if (uBuf.length() == 0) {
    return AqlValue(AqlValueHintInt(-1));
  }

  auto& server = trx->vocbase().server();
  auto locale = server.getFeature<LanguageFeature>().getLocale();
  UErrorCode status = U_ZERO_ERROR;
  icu_64_64::StringSearch search(uSearchBuf, uBuf, locale, nullptr, status);

  int foundPos = -1;
  for (int pos = search.first(status); U_SUCCESS(status) && pos != USEARCH_DONE;
       pos = search.next(status)) {
    if (U_FAILURE(status)) {
      registerICUWarning(expressionContext, AFN, status);
      return AqlValue(AqlValueHintNull());
    }
    if ((pos >= startOffset) && ((pos + searchLen - 1) <= maxEnd)) {
      foundPos = pos;
    }
  }
  return AqlValue(AqlValueHintInt(foundPos));
}

/// @brief function CONCAT_SEPARATOR
AqlValue functions::ConcatSeparator(ExpressionContext* ctx, AstNode const&,
                                    VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &ctx->trx();
  auto const& vopts = trx->vpackOptions();
  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  bool found = false;
  size_t const n = parameters.size();

  AqlValue const& separator =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  appendAsString(vopts, adapter, separator);
  std::string const plainStr(buffer->data(), buffer->length());

  buffer->clear();

  if (n == 2) {
    AqlValue const& member =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (member.isArray()) {
      // reserve *some* space
      buffer->reserve((plainStr.size() + 10) * member.length());

      AqlValueMaterializer materializer(&vopts);
      VPackSlice slice = materializer.slice(member);

      for (VPackSlice it : VPackArrayIterator(slice)) {
        if (it.isNull()) {
          continue;
        }
        if (found) {
          buffer->append(plainStr);
        }
        // convert member to a string and append
        appendAsString(vopts, adapter, AqlValue(it.begin()));
        found = true;
      }
      return AqlValue(std::string_view{buffer->data(), buffer->length()});
    }
  }

  // reserve *some* space
  buffer->reserve((plainStr.size() + 10) * n);
  for (size_t i = 1; i < n; ++i) {
    AqlValue const& member =
        aql::functions::extractFunctionParameterValue(parameters, i);

    if (member.isNull(true)) {
      continue;
    }
    if (found) {
      buffer->append(plainStr);
    }

    // convert member to a string and append
    appendAsString(vopts, adapter, member);
    found = true;
  }

  return AqlValue(std::string_view{buffer->data(), buffer->length()});
}

/// @brief function CHAR_LENGTH
AqlValue functions::CharLength(ExpressionContext* ctx, AstNode const&,
                               VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &ctx->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  size_t length = 0;

  if (value.isArray() || value.isObject()) {
    AqlValueMaterializer materializer(vopts);
    VPackSlice slice = materializer.slice(value);

    transaction::StringLeaser buffer(trx);
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

/// @brief function LOWER
AqlValue functions::Lower(ExpressionContext* ctx, AstNode const&,
                          VPackFunctionParametersView parameters) {
  std::string utf8;
  transaction::Methods* trx = &ctx->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  appendAsString(vopts, adapter, value);

  icu_64_64::UnicodeString unicodeStr(buffer->data(),
                                      static_cast<int32_t>(buffer->length()));
  unicodeStr.toLower(nullptr);
  unicodeStr.toUTF8String(utf8);

  return AqlValue(utf8);
}

/// @brief function UPPER
AqlValue functions::Upper(ExpressionContext* ctx, AstNode const&,
                          VPackFunctionParametersView parameters) {
  std::string utf8;
  transaction::Methods* trx = &ctx->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  appendAsString(vopts, adapter, value);

  icu_64_64::UnicodeString unicodeStr(buffer->data(),
                                      static_cast<int32_t>(buffer->length()));
  unicodeStr.toUpper(nullptr);
  unicodeStr.toUTF8String(utf8);

  return AqlValue(utf8);
}

/// @brief function SUBSTRING
AqlValue functions::Substring(ExpressionContext* ctx, AstNode const&,
                              VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &ctx->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  int32_t length = INT32_MAX;

  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  appendAsString(vopts, adapter, value);
  icu_64_64::UnicodeString unicodeStr(buffer->data(),
                                      static_cast<int32_t>(buffer->length()));

  int32_t offset = static_cast<int32_t>(
      aql::functions::extractFunctionParameterValue(parameters, 1).toInt64());

  if (parameters.size() == 3) {
    length = static_cast<int32_t>(
        aql::functions::extractFunctionParameterValue(parameters, 2).toInt64());
  }

  if (offset < 0) {
    offset = unicodeStr.moveIndex32(
        unicodeStr.moveIndex32(unicodeStr.length(), 0), offset);
  } else {
    offset = unicodeStr.moveIndex32(0, offset);
  }

  std::string utf8;
  unicodeStr
      .tempSubString(offset, unicodeStr.moveIndex32(offset, length) - offset)
      .toUTF8String(utf8);

  return AqlValue(utf8);
}

AqlValue functions::SubstringBytes(ExpressionContext* ctx, AstNode const& node,
                                   VPackFunctionParametersView parameters) {
  auto const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isString()) {
    registerWarning(ctx, getFunctionName(node).data(), TRI_ERROR_BAD_PARAMETER);
    return AqlValue{AqlValueHintNull{}};
  }

  auto const str = value.slice().stringView();
  if (str.empty()) {
    return AqlValue{velocypack::Slice::emptyStringSlice()};
  }

  auto const strLen = static_cast<int64_t>(str.size());
  int64_t offset = 0;
  int64_t length = strLen;
  int64_t left = 0;
  int64_t right = 0;
  switch (parameters.size()) {
    case 5:
      right = aql::functions::extractFunctionParameterValue(parameters, 4)
                  .toInt64();
      [[fallthrough]];
    case 4:
      left = aql::functions::extractFunctionParameterValue(parameters, 3)
                 .toInt64();
      if (parameters.size() == 4) {
        right = left;
      }
      [[fallthrough]];
    case 3:
      length = aql::functions::extractFunctionParameterValue(parameters, 2)
                   .toInt64();
      if (length < 0) {
        length = 0;
      }
      [[fallthrough]];
    case 2:
      offset = aql::functions::extractFunctionParameterValue(parameters, 1)
                   .toInt64();
      if (offset < 0) {
        offset = std::max(int64_t{0}, strLen + offset);
      } else {
        offset = std::min(offset, strLen);
      }
      break;
    default:
      ADB_UNREACHABLE;
  }

  auto* const begin = reinterpret_cast<irs::byte_type const*>(str.data());
  auto* const end = begin + str.size();

  auto* lhsIt = begin + offset;
  auto* rhsIt = lhsIt + std::min(length, strLen - offset);

  static constexpr auto kMaskBits = 0xC0U;
  static constexpr auto kHelpByte = 0x80U;

  if ((lhsIt != end && (*lhsIt & kMaskBits) == kHelpByte) ||
      (rhsIt != end && (*rhsIt & kMaskBits) == kHelpByte)) {
    registerWarning(ctx, getFunctionName(node).data(), TRI_ERROR_BAD_PARAMETER);
    return AqlValue{AqlValueHintNull{}};
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

  shift(left, lhsIt, begin, -1);
  shift(right, rhsIt, end, 1);

  return AqlValue{std::string_view{reinterpret_cast<char const*>(lhsIt),
                                   reinterpret_cast<char const*>(rhsIt)}};
}

AqlValue functions::Substitute(ExpressionContext* expressionContext,
                               AstNode const&,
                               VPackFunctionParametersView parameters) {
  static char const* AFN = "SUBSTITUTE";

  transaction::Methods* trx = &expressionContext->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& search =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  int64_t limit = -1;
  AqlValueMaterializer materializer(&vopts);
  std::vector<icu_64_64::UnicodeString> matchPatterns;
  std::vector<icu_64_64::UnicodeString> replacePatterns;
  bool replaceWasPlainString = false;

  if (search.isObject()) {
    if (parameters.size() > 3) {
      registerWarning(expressionContext, AFN,
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }
    if (parameters.size() == 3) {
      limit = aql::functions::extractFunctionParameterValue(parameters, 2)
                  .toInt64();
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
        replacePatterns.push_back(icu_64_64::UnicodeString("", int32_t(0)));
      } else if (it.value.isString()) {
        // string case
        str = it.value.getStringUnchecked(length);
        replacePatterns.push_back(
            icu_64_64::UnicodeString(str, static_cast<int32_t>(length)));
      } else {
        // non strings
        registerInvalidArgumentWarning(expressionContext, AFN);
        return AqlValue(AqlValueHintNull());
      }
    }
  } else {
    if (parameters.size() < 2) {
      registerWarning(expressionContext, AFN,
                      TRI_ERROR_QUERY_FUNCTION_ARGUMENT_NUMBER_MISMATCH);
      return AqlValue(AqlValueHintNull());
    }
    if (parameters.size() == 4) {
      limit = aql::functions::extractFunctionParameterValue(parameters, 3)
                  .toInt64();
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
          registerInvalidArgumentWarning(expressionContext, AFN);
          return AqlValue(AqlValueHintNull());
        }
      }
    } else {
      if (!search.isString()) {
        registerInvalidArgumentWarning(expressionContext, AFN);
        return AqlValue(AqlValueHintNull());
      }
      velocypack::ValueLength length;

      char const* str = slice.getStringUnchecked(length);
      matchPatterns.push_back(
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
            replacePatterns.push_back(icu_64_64::UnicodeString("", int32_t(0)));
          } else if (it.isString()) {
            velocypack::ValueLength length;
            char const* str = it.getStringUnchecked(length);
            replacePatterns.push_back(
                icu_64_64::UnicodeString(str, static_cast<int32_t>(length)));
          } else {
            registerInvalidArgumentWarning(expressionContext, AFN);
            return AqlValue(AqlValueHintNull());
          }
        }
      } else if (replace.isString()) {
        // If we have a string as replacement,
        // it counts in for all found values.
        replaceWasPlainString = true;
        velocypack::ValueLength length;
        char const* str = rslice.getString(length);
        replacePatterns.push_back(
            icu_64_64::UnicodeString(str, static_cast<int32_t>(length)));
      } else {
        registerInvalidArgumentWarning(expressionContext, AFN);
        return AqlValue(AqlValueHintNull());
      }
    }
  }

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  if ((limit == 0) || (matchPatterns.size() == 0)) {
    // if the limit is 0, or we don't have any match pattern, return the source
    // string.
    return AqlValue(value);
  }

  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  appendAsString(vopts, adapter, value);
  icu_64_64::UnicodeString unicodeStr(buffer->data(),
                                      static_cast<int32_t>(buffer->length()));

  auto& server = trx->vocbase().server();
  auto locale = server.getFeature<LanguageFeature>().getLocale();
  // we can't copy the search instances, thus use pointers:
  std::vector<std::unique_ptr<icu_64_64::StringSearch>> searchVec;
  searchVec.reserve(matchPatterns.size());
  UErrorCode status = U_ZERO_ERROR;
  for (auto const& searchStr : matchPatterns) {
    // create a vector of string searches
    searchVec.push_back(std::make_unique<icu_64_64::StringSearch>(
        searchStr, unicodeStr, locale, nullptr, status));
    if (U_FAILURE(status)) {
      registerICUWarning(expressionContext, AFN, status);
      return AqlValue(AqlValueHintNull());
    }
  }

  std::vector<std::pair<int32_t, int32_t>> srchResultPtrs;
  std::string utf8;
  srchResultPtrs.reserve(matchPatterns.size());
  for (auto& search : searchVec) {
    // We now find the first hit for each search string.
    auto pos = search->first(status);
    if (U_FAILURE(status)) {
      registerICUWarning(expressionContext, AFN, status);
      return AqlValue(AqlValueHintNull());
    }

    int32_t len = 0;
    if (pos != USEARCH_DONE) {
      len = search->getMatchedLength();
    }
    srchResultPtrs.push_back(std::make_pair(pos, len));
  }

  icu_64_64::UnicodeString result;
  int32_t lastStart = 0;
  int64_t count = 0;
  while (true) {
    int which = -1;
    int32_t pos = USEARCH_DONE;
    int32_t mLen = 0;
    int i = 0;
    for (auto resultPair : srchResultPtrs) {
      // We locate the nearest matching search result.
      int32_t thisPos;
      thisPos = resultPair.first;
      if ((pos == USEARCH_DONE) || (pos > thisPos)) {
        if (thisPos != USEARCH_DONE) {
          pos = thisPos;
          which = i;
          mLen = resultPair.second;
        }
      }
      i++;
    }
    if (which == -1) {
      break;
    }
    // from last match to this match, copy the original string.
    result.append(unicodeStr, lastStart, pos - lastStart);
    if (replacePatterns.size() != 0) {
      if (replacePatterns.size() > (size_t)which) {
        result.append(replacePatterns[which]);
      } else if (replaceWasPlainString) {
        result.append(replacePatterns[0]);
      }
    }

    // lastStart is the place up to we searched the source string
    lastStart = pos + mLen;

    // we try to search the next occurance of this string
    auto& search = searchVec[which];
    pos = search->next(status);
    if (U_FAILURE(status)) {
      registerICUWarning(expressionContext, AFN, status);
      return AqlValue(AqlValueHintNull());
    }
    if (pos != USEARCH_DONE) {
      mLen = search->getMatchedLength();
    } else {
      mLen = -1;
    }
    srchResultPtrs[which] = std::make_pair(pos, mLen);

    which = 0;
    for (auto searchPair : srchResultPtrs) {
      // now we invalidate all search results that overlap with
      // our last search result and see whether we can find the
      // overlapped pattern again.
      // However, that mustn't overlap with the current lastStart
      // position either.
      int32_t thisPos;
      thisPos = searchPair.first;
      if ((thisPos != USEARCH_DONE) && (thisPos < lastStart)) {
        auto& search = searchVec[which];
        pos = thisPos;
        while ((pos < lastStart) && (pos != USEARCH_DONE)) {
          pos = search->next(status);
          if (U_FAILURE(status)) {
            registerICUWarning(expressionContext, AFN, status);
            return AqlValue(AqlValueHintNull());
          }
          if (pos != USEARCH_DONE) {
            mLen = search->getMatchedLength();
          }
          srchResultPtrs[which] = std::make_pair(pos, mLen);
        }
      }
      which++;
    }

    count++;
    if ((limit != -1) && (count >= limit)) {
      // Do we have a limit count?
      break;
    }
    // check whether none of our search objects has any more results
    bool allFound = true;
    for (auto resultPair : srchResultPtrs) {
      if (resultPair.first != USEARCH_DONE) {
        allFound = false;
        break;
      }
    }
    if (allFound) {
      break;
    }
  }
  // Append from the last found:
  result.append(unicodeStr, lastStart, unicodeStr.length() - lastStart);

  result.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function LEFT str, length
AqlValue functions::Left(ExpressionContext* ctx, AstNode const&,
                         VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &ctx->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue value = aql::functions::extractFunctionParameterValue(parameters, 0);
  uint32_t length = static_cast<int32_t>(
      aql::functions::extractFunctionParameterValue(parameters, 1).toInt64());

  std::string utf8;
  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  appendAsString(vopts, adapter, value);

  icu_64_64::UnicodeString unicodeStr(buffer->data(),
                                      static_cast<int32_t>(buffer->length()));
  icu_64_64::UnicodeString left =
      unicodeStr.tempSubString(0, unicodeStr.moveIndex32(0, length));

  left.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function RIGHT
AqlValue functions::Right(ExpressionContext* ctx, AstNode const&,
                          VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &ctx->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue value = aql::functions::extractFunctionParameterValue(parameters, 0);
  uint32_t length = static_cast<int32_t>(
      aql::functions::extractFunctionParameterValue(parameters, 1).toInt64());

  std::string utf8;
  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  appendAsString(vopts, adapter, value);

  icu_64_64::UnicodeString unicodeStr(buffer->data(),
                                      static_cast<int32_t>(buffer->length()));
  icu_64_64::UnicodeString right =
      unicodeStr.tempSubString(unicodeStr.moveIndex32(
          unicodeStr.length(), -static_cast<int32_t>(length)));

  right.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function TRIM
AqlValue functions::Trim(ExpressionContext* expressionContext, AstNode const&,
                         VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "TRIM";

  transaction::Methods* trx = &expressionContext->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());
  appendAsString(vopts, adapter, value);
  icu_64_64::UnicodeString unicodeStr(buffer->data(),
                                      static_cast<int32_t>(buffer->length()));

  int64_t howToTrim = 0;
  icu_64_64::UnicodeString whitespace("\r\n\t ");

  if (parameters.size() == 2) {
    AqlValue const& optional =
        aql::functions::extractFunctionParameterValue(parameters, 1);

    if (optional.isNumber()) {
      howToTrim = optional.toInt64();

      if (howToTrim < 0 || 2 < howToTrim) {
        howToTrim = 0;
      }
    } else if (optional.isString()) {
      buffer->clear();
      appendAsString(vopts, adapter, optional);
      whitespace = icu_64_64::UnicodeString(
          buffer->data(), static_cast<int32_t>(buffer->length()));
    }
  }

  uint32_t numWhitespaces = whitespace.countChar32();
  UErrorCode errorCode = U_ZERO_ERROR;
  auto spaceChars = std::make_unique<UChar32[]>(numWhitespaces);

  whitespace.toUTF32(spaceChars.get(), numWhitespaces, errorCode);
  if (U_FAILURE(errorCode)) {
    registerICUWarning(expressionContext, AFN, errorCode);
    return AqlValue(AqlValueHintNull());
  }

  int32_t startOffset = 0, endOffset = unicodeStr.length();

  if (howToTrim <= 1) {
    ltrimInternal(startOffset, endOffset, unicodeStr, numWhitespaces,
                  spaceChars.get());
  }

  if (howToTrim == 2 || howToTrim == 0) {
    rtrimInternal(startOffset, endOffset, unicodeStr, numWhitespaces,
                  spaceChars.get());
  }

  icu_64_64::UnicodeString result =
      unicodeStr.tempSubString(startOffset, endOffset - startOffset);
  std::string utf8;
  result.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function LTRIM
AqlValue functions::LTrim(ExpressionContext* expressionContext, AstNode const&,
                          VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "LTRIM";

  transaction::Methods* trx = &expressionContext->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());
  appendAsString(vopts, adapter, value);
  icu_64_64::UnicodeString unicodeStr(buffer->data(),
                                      static_cast<int32_t>(buffer->length()));
  icu_64_64::UnicodeString whitespace("\r\n\t ");

  if (parameters.size() == 2) {
    AqlValue const& pWhitespace =
        aql::functions::extractFunctionParameterValue(parameters, 1);
    buffer->clear();
    appendAsString(vopts, adapter, pWhitespace);
    whitespace = icu_64_64::UnicodeString(
        buffer->data(), static_cast<int32_t>(buffer->length()));
  }

  uint32_t numWhitespaces = whitespace.countChar32();
  UErrorCode errorCode = U_ZERO_ERROR;
  auto spaceChars = std::make_unique<UChar32[]>(numWhitespaces);

  whitespace.toUTF32(spaceChars.get(), numWhitespaces, errorCode);
  if (U_FAILURE(errorCode)) {
    registerICUWarning(expressionContext, AFN, errorCode);
    return AqlValue(AqlValueHintNull());
  }

  int32_t startOffset = 0, endOffset = unicodeStr.length();

  ltrimInternal(startOffset, endOffset, unicodeStr, numWhitespaces,
                spaceChars.get());

  icu_64_64::UnicodeString result =
      unicodeStr.tempSubString(startOffset, endOffset - startOffset);
  std::string utf8;
  result.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function RTRIM
AqlValue functions::RTrim(ExpressionContext* expressionContext, AstNode const&,
                          VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "RTRIM";

  transaction::Methods* trx = &expressionContext->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());
  appendAsString(vopts, adapter, value);
  icu_64_64::UnicodeString unicodeStr(buffer->data(),
                                      static_cast<int32_t>(buffer->length()));
  icu_64_64::UnicodeString whitespace("\r\n\t ");

  if (parameters.size() == 2) {
    AqlValue const& pWhitespace =
        aql::functions::extractFunctionParameterValue(parameters, 1);
    buffer->clear();
    appendAsString(vopts, adapter, pWhitespace);
    whitespace = icu_64_64::UnicodeString(
        buffer->data(), static_cast<int32_t>(buffer->length()));
  }

  uint32_t numWhitespaces = whitespace.countChar32();
  UErrorCode errorCode = U_ZERO_ERROR;
  auto spaceChars = std::make_unique<UChar32[]>(numWhitespaces);

  whitespace.toUTF32(spaceChars.get(), numWhitespaces, errorCode);
  if (U_FAILURE(errorCode)) {
    registerICUWarning(expressionContext, AFN, errorCode);
    return AqlValue(AqlValueHintNull());
  }

  int32_t startOffset = 0, endOffset = unicodeStr.length();

  rtrimInternal(startOffset, endOffset, unicodeStr, numWhitespaces,
                spaceChars.get());

  icu_64_64::UnicodeString result =
      unicodeStr.tempSubString(startOffset, endOffset - startOffset);
  std::string utf8;
  result.toUTF8String(utf8);
  return AqlValue(utf8);
}

/// @brief function CONTAINS
AqlValue functions::Contains(ExpressionContext* ctx, AstNode const&,
                             VPackFunctionParametersView parameters) {
  auto* trx = &ctx->trx();
  auto const& vopts = trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& search =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  AqlValue const& returnIndex =
      aql::functions::extractFunctionParameterValue(parameters, 2);

  bool const willReturnIndex = returnIndex.toBoolean();

  int result = -1;  // default is "not found"
  {
    transaction::StringLeaser buffer(trx);
    velocypack::StringSink adapter(buffer.get());

    appendAsString(vopts, adapter, value);
    size_t const valueLength = buffer->length();

    size_t const searchOffset = buffer->length();
    appendAsString(vopts, adapter, search);
    size_t const searchLength = buffer->length() - valueLength;

    if (searchLength > 0) {
      char const* found = static_cast<char const*>(
          memmem(buffer->data(), valueLength, buffer->data() + searchOffset,
                 searchLength));

      if (found != nullptr) {
        if (willReturnIndex) {
          // find offset into string
          int bytePosition = static_cast<int>(found - buffer->data());
          char const* p = buffer->data();
          int pos = 0;
          while (pos < bytePosition) {
            unsigned char c = static_cast<unsigned char>(*p);
            if (c < 128) {
              ++pos;
            } else if (c < 224) {
              pos += 2;
            } else if (c < 240) {
              pos += 3;
            } else if (c < 248) {
              pos += 4;
            }
          }
          result = pos;
        } else {
          // fake result position, but it does not matter as it will
          // only be compared to -1 later
          result = 0;
        }
      }
    }
  }

  if (willReturnIndex) {
    // return numeric value
    return AqlValue(AqlValueHintInt(result));
  }

  // return boolean
  return AqlValue(AqlValueHintBool(result != -1));
}

/// @brief function CONCAT
AqlValue functions::Concat(ExpressionContext* ctx, AstNode const&,
                           VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &ctx->trx();
  auto const& vopts = trx->vpackOptions();
  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  size_t const n = parameters.size();

  if (n == 1) {
    AqlValue const& member =
        aql::functions::extractFunctionParameterValue(parameters, 0);
    if (member.isArray()) {
      AqlValueMaterializer materializer(&vopts);
      VPackSlice slice = materializer.slice(member);

      for (VPackSlice it : VPackArrayIterator(slice)) {
        if (it.isNull()) {
          continue;
        }
        // convert member to a string and append
        appendAsString(vopts, adapter, AqlValue(it.begin()));
      }
      return AqlValue(std::string_view{buffer->data(), buffer->length()});
    }
  }

  for (size_t i = 0; i < n; ++i) {
    AqlValue const& member =
        aql::functions::extractFunctionParameterValue(parameters, i);

    if (member.isNull(true)) {
      continue;
    }

    // convert member to a string and append
    appendAsString(vopts, adapter, member);
  }

  return AqlValue(std::string_view{buffer->data(), buffer->length()});
}

/// @brief function LIKE
AqlValue functions::Like(ExpressionContext* expressionContext, AstNode const&,
                         VPackFunctionParametersView parameters) {
  static char const* AFN = "LIKE";

  transaction::Methods* trx = &expressionContext->trx();
  auto const& vopts = trx->vpackOptions();
  bool const caseInsensitive = getBooleanParameter(parameters, 2, false);
  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  // build pattern from parameter #1
  AqlValue const& regex =
      aql::functions::extractFunctionParameterValue(parameters, 1);
  appendAsString(vopts, adapter, regex);

  // the matcher is owned by the context!
  icu_64_64::RegexMatcher* matcher =
      expressionContext->buildLikeMatcher(*buffer, caseInsensitive);

  if (matcher == nullptr) {
    // compiling regular expression failed
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  // extract value
  buffer->clear();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  appendAsString(vopts, adapter, value);

  bool error = false;
  bool const result = basics::Utf8Helper::DefaultUtf8Helper.matches(
      matcher, buffer->data(), buffer->length(), false, error);

  if (error) {
    // compiling regular expression failed
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  return AqlValue(AqlValueHintBool(result));
}

/// @brief function SPLIT
AqlValue functions::Split(ExpressionContext* expressionContext, AstNode const&,
                          VPackFunctionParametersView parameters) {
  static char const* AFN = "SPLIT";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  // cheapest parameter checks first:
  int64_t limitNumber = -1;
  if (parameters.size() == 3) {
    AqlValue const& aqlLimit =
        aql::functions::extractFunctionParameterValue(parameters, 2);
    if (aqlLimit.isNumber()) {
      limitNumber = aqlLimit.toInt64();
    } else {
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }

    // these are edge cases which are documented to have these return values:
    if (limitNumber < 0) {
      return AqlValue(AqlValueHintNull());
    }
    if (limitNumber == 0) {
      return AqlValue(AqlValueHintEmptyArray());
    }
  }

  transaction::StringLeaser regexBuffer(trx);
  AqlValue aqlSeparatorExpression;
  if (parameters.size() >= 2) {
    aqlSeparatorExpression =
        aql::functions::extractFunctionParameterValue(parameters, 1);
    if (aqlSeparatorExpression.isObject()) {
      registerInvalidArgumentWarning(expressionContext, AFN);
      return AqlValue(AqlValueHintNull());
    }
  }

  AqlValue const& aqlValueToSplit =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (parameters.size() == 1) {
    // pre-documented edge-case: if we only have the first parameter, return it.
    transaction::BuilderLeaser builder(trx);
    builder->openArray();
    builder->add(aqlValueToSplit.slice());
    builder->close();
    return AqlValue(builder->slice(), builder->size());
  }

  // Get ready for ICU
  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());
  functions::stringify(vopts, adapter, aqlValueToSplit.slice());
  icu_64_64::UnicodeString valueToSplit(buffer->data(),
                                        static_cast<int32_t>(buffer->length()));
  bool isEmptyExpression = false;

  // the matcher is owned by the context!
  icu_64_64::RegexMatcher* matcher = expressionContext->buildSplitMatcher(
      aqlSeparatorExpression, &trx->vpackOptions(), isEmptyExpression);

  if (matcher == nullptr) {
    // compiling regular expression failed
    registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
    return AqlValue(AqlValueHintNull());
  }

  transaction::BuilderLeaser result(trx);
  result->openArray();
  if (!isEmptyExpression && (buffer->length() == 0)) {
    // Edge case: splitting an empty string by non-empty expression produces an
    // empty string again.
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
    uint16_t copyThisTime = uCount;

    if (U_FAILURE(errorCode)) {
      registerWarning(expressionContext, AFN, TRI_ERROR_QUERY_INVALID_REGEX);
      return AqlValue(AqlValueHintNull());
    }

    if ((copyThisTime > 0) && (copyThisTime > nrResults)) {
      // last hit is the remaining string to be fed into split in a subsequent
      // invocation
      copyThisTime--;
    }

    if ((copyThisTime > 0) &&
        ((copyThisTime == nrResults) || isEmptyExpression)) {
      // ICU will give us a traling empty string we don't care for if we split
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

}  // namespace arangodb::aql
