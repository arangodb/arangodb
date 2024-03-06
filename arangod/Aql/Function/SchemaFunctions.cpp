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
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/Validators.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <absl/strings/str_cat.h>

using namespace arangodb;

namespace arangodb::aql {

AqlValue functions::SchemaGet(ExpressionContext* expressionContext,
                              AstNode const&,
                              VPackFunctionParametersView parameters) {
  transaction::Methods* trx = &expressionContext->trx();
  // SCHEMA_GET(collectionName) -> schema object
  std::string const collectionName = extractCollectionName(trx, parameters, 0);

  if (collectionName.empty()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "could not extract collection name from parameters");
  }

  std::shared_ptr<LogicalCollection> logicalCollection;
  methods::Collections::lookup(trx->vocbase(), collectionName,
                               logicalCollection);
  if (!logicalCollection) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        absl::StrCat("could not find collection: ", collectionName));
  }

  transaction::BuilderLeaser builder(trx);
  logicalCollection->schemaToVelocyPack(*builder.get());
  VPackSlice slice = builder->slice();

  if (!slice.isObject()) {
    return AqlValue(AqlValueHintNull{});
  }

  auto ruleSlice = slice.get(StaticStrings::ValidationParameterRule);

  if (!ruleSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("schema definition for collection ", collectionName,
                     " has no rule object"));
  }

  return AqlValue(slice, builder->size());
}

AqlValue functions::SchemaValidate(ExpressionContext* expressionContext,
                                   AstNode const&,
                                   VPackFunctionParametersView parameters) {
  // SCHEMA_VALIDATE(doc, schema object) -> { valid (bool), [errorMessage
  // (string)] }
  static char const* AFN = "SCHEMA_VALIDATE";
  transaction::Methods* trx = &expressionContext->trx();
  auto* vopts = &trx->vpackOptions();

  AqlValue const& docValue =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  AqlValue const& schemaValue =
      aql::functions::extractFunctionParameterValue(parameters, 1);

  if (!docValue.isObject()) {
    registerWarning(expressionContext, AFN,
                    Result(TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH,
                           "expecting document object"));
    return AqlValue(AqlValueHintNull());
  }

  if (schemaValue.isNull(false) ||
      (schemaValue.isObject() && schemaValue.length() == 0)) {
    // schema is null or {}
    transaction::BuilderLeaser resultBuilder(trx);
    {
      VPackObjectBuilder guard(resultBuilder.builder());
      resultBuilder->add("valid", VPackValue(true));
    }
    return AqlValue(resultBuilder->slice(), resultBuilder->size());
  }

  if (!schemaValue.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("second parameter is not a schema object: ",
                     schemaValue.slice().toJson()));
  }
  auto* validator = expressionContext->buildValidator(schemaValue.slice());

  // store and restore validation level this is cheaper than modifying the VPack
  auto storedLevel = validator->level();

  // override level so the validation will be executed no matter what
  validator->setLevel(ValidationLevel::Strict);

  Result res;
  {
    ScopeGuard guard([storedLevel, &validator]() noexcept {
      validator->setLevel(storedLevel);
    });

    res = validator->validateOne(docValue.slice(), vopts);
  }

  transaction::BuilderLeaser resultBuilder(trx);
  {
    VPackObjectBuilder guard(resultBuilder.builder());
    resultBuilder->add("valid", VPackValue(res.ok()));
    if (res.fail()) {
      resultBuilder->add(StaticStrings::ErrorMessage,
                         VPackValue(res.errorMessage()));
    }
  }

  return AqlValue(resultBuilder->slice(), resultBuilder->size());
}

}  // namespace arangodb::aql
