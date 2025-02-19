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

#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Basics/StringUtils.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <arpa/inet.h>

using namespace arangodb;

namespace arangodb::aql {

/// @brief function IPV4_FROM_NUMBER
AqlValue functions::IpV4FromNumber(ExpressionContext* expressionContext,
                                   AstNode const&,
                                   VPackFunctionParametersView parameters) {
  static char const* AFN = "IPV4_FROM_NUMBER";

  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isNumber()) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  int64_t input = value.toInt64();
  if (input < 0 || static_cast<uint64_t>(input) > UINT32_MAX) {
    registerInvalidArgumentWarning(expressionContext, AFN);
    return AqlValue(AqlValueHintNull());
  }

  uint64_t number = static_cast<uint64_t>(input);

  // in theory, we only need a 15 bytes buffer here, as the maximum result
  // string is "255.255.255.255"
  char result[32];

  char* p = &result[0];
  // first part
  uint64_t digit = (number & 0xff000000ULL) >> 24ULL;
  p += basics::StringUtils::itoa(digit, p);
  *p++ = '.';
  // second part
  digit = (number & 0x00ff0000ULL) >> 16ULL;
  p += basics::StringUtils::itoa(digit, p);
  *p++ = '.';
  // third part
  digit = (number & 0x0000ff00ULL) >> 8ULL;
  p += basics::StringUtils::itoa(digit, p);
  *p++ = '.';
  // fourth part
  digit = (number & 0x0000ffULL);
  p += basics::StringUtils::itoa(digit, p);

  return AqlValue(std::string_view{&result[0], p});
}

/// @brief function IPV4_TO_NUMBER
AqlValue functions::IpV4ToNumber(ExpressionContext* expressionContext,
                                 AstNode const&,
                                 VPackFunctionParametersView parameters) {
  static char const* AFN = "IPV4_TO_NUMBER";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isString()) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);

  // parse the input string
  TRI_ASSERT(slice.isString());
  VPackValueLength l;
  char const* p = slice.getString(l);

  if (l >= 7 && l <= 15) {
    // min value is 0.0.0.0 (length = 7)
    // max value is 255.255.255.255 (length = 15)
    char buffer[16];
    memcpy(&buffer[0], p, l);
    // null-terminate the buffer
    buffer[l] = '\0';

    struct in_addr addr;
    memset(&addr, 0, sizeof(struct in_addr));
    int result = inet_pton(AF_INET, &buffer[0], &addr);

    if (result == 1) {
      return AqlValue(AqlValueHintUInt(
          basics::hostToBig(*reinterpret_cast<uint32_t*>(&addr))));
    }
  }

  registerInvalidArgumentWarning(expressionContext, AFN);
  return AqlValue(AqlValueHintNull());
}

/// @brief function IS_IPV4
AqlValue functions::IsIpV4(ExpressionContext* expressionContext, AstNode const&,
                           VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);

  if (!value.isString()) {
    return AqlValue(AqlValueHintBool(false));
  }

  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);

  // parse the input string
  TRI_ASSERT(slice.isString());
  VPackValueLength l;
  char const* p = slice.getString(l);

  if (l >= 7 && l <= 15) {
    // min value is 0.0.0.0 (length = 7)
    // max value is 255.255.255.255 (length = 15)
    char buffer[16];
    memcpy(&buffer[0], p, l);
    // null-terminate the buffer
    buffer[l] = '\0';

    struct in_addr addr;
    memset(&addr, 0, sizeof(struct in_addr));
    int result = inet_pton(AF_INET, &buffer[0], &addr);

    if (result == 1) {
      return AqlValue(AqlValueHintBool(true));
    }
  }

  return AqlValue(AqlValueHintBool(false));
}

}  // namespace arangodb::aql
