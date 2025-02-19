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
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>

#include <absl/strings/escaping.h>

#include <string_view>

using namespace arangodb;

namespace arangodb::aql {

/// @brief function TO_BASE64
AqlValue functions::ToBase64(ExpressionContext* expr, AstNode const&,
                             VPackFunctionParametersView parameters) {
  auto& trx = expr->trx();
  AqlValue const& value = extractFunctionParameterValue(parameters, 0);

  transaction::StringLeaser buffer(&trx);
  velocypack::StringSink adapter(buffer.get());

  appendAsString(trx.vpackOptions(), adapter, value);

  std::string encoded = absl::Base64Escape({buffer->data(), buffer->length()});

  return AqlValue(std::move(encoded));
}

/// @brief function TO_HEX
AqlValue functions::ToHex(ExpressionContext* expr, AstNode const&,
                          VPackFunctionParametersView parameters) {
  auto& trx = expr->trx();
  AqlValue const& value = extractFunctionParameterValue(parameters, 0);

  transaction::StringLeaser buffer(&trx);
  velocypack::StringSink adapter(buffer.get());

  appendAsString(trx.vpackOptions(), adapter, value);

  std::string encoded =
      basics::StringUtils::encodeHex(buffer->data(), buffer->length());

  return AqlValue(encoded);
}

/// @brief function ENCODE_URI_COMPONENT
AqlValue functions::EncodeURIComponent(ExpressionContext* expr, AstNode const&,
                                       VPackFunctionParametersView parameters) {
  auto& trx = expr->trx();
  AqlValue const& value = extractFunctionParameterValue(parameters, 0);

  transaction::StringLeaser buffer(&trx);
  velocypack::StringSink adapter(buffer.get());

  appendAsString(trx.vpackOptions(), adapter, value);

  std::string encoded =
      basics::StringUtils::encodeURIComponent(buffer->data(), buffer->length());

  return AqlValue(encoded);
}

}  // namespace arangodb::aql
