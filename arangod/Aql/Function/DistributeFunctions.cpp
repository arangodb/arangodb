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
#include "Basics/StaticStrings.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"

#ifdef USE_ENTERPRISE
#include "Enterprise/VocBase/SmartGraphSchema.h"
#endif

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <string_view>

using namespace arangodb;

namespace arangodb::aql {
namespace {
void buildKeyObject(VPackBuilder& builder, std::string_view key,
                    bool closeObject) {
  builder.openObject(true);
  builder.add(StaticStrings::KeyString, VPackValue(key));
  if (closeObject) {
    builder.close();
  }
}

AqlValue convertToObject(transaction::Methods& trx, VPackSlice input,
                         bool allowKeyConversionToObject, bool canUseCustomKey,
                         bool ignoreErrors) {
  // input is not an object.
  // if this happens, it must be a string key
  if (!input.isString() || !allowKeyConversionToObject) {
    if (ignoreErrors) {
      return AqlValue{};
    }
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DOCUMENT_TYPE_INVALID);
  }

  // check if custom keys are allowed in this context
  if (!canUseCustomKey) {
    if (ignoreErrors) {
      return AqlValue{};
    }
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY);
  }

  // convert string key into object with { _key: "string" }
  TRI_ASSERT(allowKeyConversionToObject);
  transaction::BuilderLeaser builder(&trx);
  buildKeyObject(*builder, input.stringView(), /*closeObject*/ true);
  return AqlValue{builder->slice()};
}

}  // namespace

AqlValue functions::MakeDistributeInput(
    aql::ExpressionContext* expressionContext, AstNode const&,
    VPackFunctionParametersView parameters) {
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  VPackSlice input = value.slice();  // will throw when wrong type

  if (!input.isObject()) {
    transaction::Methods& trx = expressionContext->trx();
    VPackSlice flags =
        aql::functions::extractFunctionParameterValue(parameters, 1).slice();
    bool allowKeyConversionToObject =
        flags["allowKeyConversionToObject"].getBool();
    bool canUseCustomKey = flags["canUseCustomKey"].getBool();
    bool ignoreErrors = flags["ignoreErrors"].getBool();
    return convertToObject(trx, input, allowKeyConversionToObject,
                           canUseCustomKey, ignoreErrors);
  }
  TRI_ASSERT(input.isObject());

  return AqlValue{input};
}

AqlValue functions::MakeDistributeInputWithKeyCreation(
    aql::ExpressionContext* expressionContext, AstNode const&,
    VPackFunctionParametersView parameters) {
  transaction::Methods& trx = expressionContext->trx();
  AqlValue value = aql::functions::extractFunctionParameterValue(parameters, 0);

  VPackSlice opts =
      aql::functions::extractFunctionParameterValue(parameters, 2).slice();
  bool allowSpecifiedKeys = opts["allowSpecifiedKeys"].getBool();
  bool ignoreErrors = opts["ignoreErrors"].getBool();
  std::string const collectionName = opts["collection"].copyString();

  // if empty, check alternative input register
  if (value.isNull(true)) {
    // value is set, but null
    // check if there is a second input register available (UPSERT makes use of
    // two input registers,
    // one for the search document, the other for the insert document)
    value = aql::functions::extractFunctionParameterValue(parameters, 1);
    allowSpecifiedKeys = false;
  }

  if (collectionName.empty()) {
    if (ignoreErrors) {
      return AqlValue{};
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "could not extract collection name from parameters");
  }

  std::shared_ptr<LogicalCollection> logicalCollection;
  methods::Collections::lookup(trx.vocbase(), collectionName,
                               logicalCollection);
  if (logicalCollection == nullptr) {
    if (ignoreErrors) {
      return AqlValue{};
    }
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        absl::StrCat("could not find collection ", collectionName));
  }

  bool canUseCustomKey =
      logicalCollection->usesDefaultShardKeys() || allowSpecifiedKeys;

  VPackSlice input = value.slice();  // will throw when wrong type
  if (!input.isObject()) {
    return convertToObject(trx, input, true, canUseCustomKey, ignoreErrors);
  }

  TRI_ASSERT(input.isObject());

  bool buildNewObject = !input.hasKey(StaticStrings::KeyString);
  // we are responsible for creating keys if none present

  if (!buildNewObject && !canUseCustomKey) {
    if (ignoreErrors) {
      return AqlValue{};
    }
    // the collection is not sharded by _key, but there is a _key present in
    // the data. a _key was given, but user is not allowed to specify _key
    THROW_ARANGO_EXCEPTION(TRI_ERROR_CLUSTER_MUST_NOT_SPECIFY_KEY);
  }

  if (buildNewObject && !logicalCollection->mustCreateKeyOnCoordinator()) {
    // if we only have a single shard, we let the DB server generate the
    // keys
    buildNewObject = false;
  }

  if (buildNewObject) {
    transaction::BuilderLeaser builder(&trx);
    buildKeyObject(
        *builder,
        std::string_view(logicalCollection->keyGenerator().generate(input)),
        /*closeObject*/ false);
    for (auto cur :
         VPackObjectIterator(input, /*useSequentialIteration*/ true)) {
      builder->add(cur.key.stringView(), cur.value);
    }
    builder->close();
    return AqlValue{builder->slice()};
  }

  return AqlValue{input};
}

AqlValue functions::MakeDistributeGraphInput(
    aql::ExpressionContext* expressionContext, AstNode const&,
    VPackFunctionParametersView parameters) {
  transaction::Methods& trx = expressionContext->trx();
  AqlValue const& value =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  VPackSlice input = value.slice();  // will throw when wrong type
  if (input.isString()) {
    // Need to fix this document.
    // We need id and key as input.

    transaction::BuilderLeaser builder(&trx);
    std::string_view s(input.stringView());
    size_t pos = s.find('/');
    if (pos == s.npos) {
      buildKeyObject(*builder, s, /*closeObject*/ true);
    } else {
      // s is an id string, so let's create an object with id + key
      auto key = s.substr(pos + 1);
      buildKeyObject(*builder, key, /*closeObject*/ false);
      builder->add(StaticStrings::IdString, input);
      builder->close();
    }

    return AqlValue{builder->slice()};
  }

  // check input value
  VPackSlice idSlice;
  bool valid = input.isObject();
  if (valid) {
    idSlice = input.get(StaticStrings::IdString);
    // no _id, no cookies
    valid = !idSlice.isNone();
  }

  if (!valid) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_PARSE,
        absl::StrCat("invalid start vertex. Must either be "
                     "an _id string value or an object with _id. "
                     "Instead got: ",
                     input.toJson()));
  }

  if (!input.hasKey(StaticStrings::KeyString)) {
    // The input is an object, but only contains an _id, not a _key value that
    // could be extracted. We can work with _id value only however so let us do
    // this.
    auto keyPart = transaction::helpers::extractKeyPart(idSlice);
    transaction::BuilderLeaser builder(&trx);
    buildKeyObject(*builder, std::string_view(keyPart), /*closeObject*/ false);
    for (auto cur : VPackObjectIterator(input)) {
      builder->add(cur.key.stringView(), cur.value);
    }
    builder->close();
    return AqlValue{builder->slice()};
  }

  return AqlValue{input};
}

#ifdef USE_ENTERPRISE
AqlValue functions::SelectSmartDistributeGraphInput(
    aql::ExpressionContext* expressionContext, AstNode const&,
    VPackFunctionParametersView parameters) {
  AqlValue const& from =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  VPackSlice input = from.slice();  // will throw when wrong type
  if (ADB_UNLIKELY(!input.isObject() ||
                   !input.hasKey(StaticStrings::IdString) ||
                   !input.get(StaticStrings::IdString).isString())) {
    // This is an internal use function, so the if condition should always be
    // true Just a protection against users typing this method by hand.
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_QUERY_PARSE,
        absl::StrCat("invalid start vertex. Must either be "
                     "an _id string value or an object with _id. "
                     "Instead got: ",
                     input.toJson()));
  }
  auto fromId = input.get(StaticStrings::IdString).stringView();
  auto res =
      SmartGraphValidationHelper::SmartValidationResult::validateVertexId(
          fromId, expressionContext->vocbase());
  if (res.ok()) {
    return AqlValue{input};
  }
  // From vertex is not smart. Use the other side.

  // It does not matter if the other side is actually smart.
  // Validity will be checked before (MAKE_DISTRIBUTE INPUT) and after
  // (Distribute/PathQuery)
  // If this vertex is Smart we shard by it.
  // If not, we assume it to be satellite, so it can be send anywhere.
  return aql::functions::extractFunctionParameterValue(parameters, 1);
}
#endif

}  // namespace arangodb::aql
