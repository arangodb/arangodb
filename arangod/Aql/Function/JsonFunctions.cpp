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
#include "Transaction/Context.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/Dumper.h>
#include <velocypack/Iterator.h>
#include <velocypack/Parser.h>
#include <velocypack/Sink.h>
#include <velocypack/Slice.h>

#include <memory>
#include <string_view>

using namespace arangodb;

namespace arangodb::aql {

/// @brief function JSON_STRINGIFY
AqlValue functions::JsonStringify(ExpressionContext* exprCtx, AstNode const&,
                                  VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &exprCtx->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);

  transaction::StringLeaser buffer(trx);
  velocypack::StringSink adapter(buffer.get());

  VPackDumper dumper(&adapter, trx->transactionContextPtr()->getVPackOptions());
  dumper.dump(slice);

  return AqlValue(std::string_view{buffer->data(), buffer->length()});
}

/// @brief function JSON_PARSE
AqlValue functions::JsonParse(ExpressionContext* expressionContext,
                              AstNode const&,
                              VPackFunctionParametersView parameters) {
  static char const* AFN = "JSON_PARSE";

  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValueMaterializer materializer(vopts);
  VPackSlice slice = materializer.slice(value);

  if (!slice.isString()) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }

  VPackValueLength l;
  char const* p = slice.getStringUnchecked(l);

  try {
    std::shared_ptr<VPackBuilder> builder = velocypack::Parser::fromJson(p, l);
    return AqlValue(builder->slice(), builder->size());
  } catch (...) {
    registerWarning(expressionContext, AFN,
                    TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
    return AqlValue(AqlValueHintNull());
  }
}

}  // namespace arangodb::aql
