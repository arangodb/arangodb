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
#include "Aql/AqlValue.h"
#include "Aql/AqlValueMaterializer.h"
#include "Aql/AstNode.h"
#include "Aql/ExpressionContext.h"
#include "Aql/Function.h"
#include "Aql/Functions.h"
#include "Basics/Exceptions.h"
#include "Cluster/ClusterFeature.h"
#include "Cluster/ClusterInfo.h"
#include "Cluster/ServerState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Utils/ExecContext.h"
#include "Utils/OperationOptions.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Methods/Collections.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <absl/strings/str_cat.h>

using namespace arangodb;

namespace arangodb::aql {

/// @brief function IS_SAME_COLLECTION
AqlValue functions::IsSameCollection(ExpressionContext* expressionContext,
                                     AstNode const&,
                                     VPackFunctionParametersView parameters) {
  static char const* AFN = "IS_SAME_COLLECTION";

  auto* trx = &expressionContext->trx();
  std::string const first = extractCollectionName(trx, parameters, 0);
  std::string const second = extractCollectionName(trx, parameters, 1);

  if (!first.empty() && !second.empty()) {
    return AqlValue(AqlValueHintBool(first == second));
  }

  registerWarning(expressionContext, AFN,
                  TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH);
  return AqlValue(AqlValueHintNull());
}

/// @brief function COLLECTIONS
AqlValue functions::Collections(ExpressionContext* exprCtx, AstNode const&,
                                VPackFunctionParametersView parameters) {
  transaction::BuilderLeaser builder(&exprCtx->trx());
  builder->openArray();

  auto& vocbase = exprCtx->vocbase();
  auto colls = methods::Collections::sorted(vocbase);

  size_t const n = colls.size();

  auto const& exec = ExecContext::current();
  for (size_t i = 0; i < n; ++i) {
    auto& coll = colls[i];

    if (!exec.canUseCollection(vocbase.name(), coll->name(), auth::Level::RO)) {
      continue;
    }

    builder->openObject();
    builder->add("_id", VPackValue(std::to_string(coll->id().id())));
    builder->add("name", VPackValue(coll->name()));
    builder->close();
  }

  builder->close();

  return AqlValue(builder->slice(), builder->size());
}

AqlValue functions::ShardId(ExpressionContext* expressionContext,
                            AstNode const&,
                            VPackFunctionParametersView parameters) {
  // Expecting 2 parameters
  // 0 : collection name or id
  // 1 : {shardKey1 : k1, shardKey2: k2, ..., shardKeyn: kn}
  auto const col =
      aql::functions::extractFunctionParameterValue(parameters, 0).slice();
  auto const keys =
      aql::functions::extractFunctionParameterValue(parameters, 1).slice();

  if (!col.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "first parameter to SHARD_ID must be a string");
  }
  if (!keys.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "second parameter to SHARD_ID must be an object");
  }

  auto& vocbase = expressionContext->trx().vocbase();
  auto role = ServerState::instance()->getRole();
  std::shared_ptr<LogicalCollection> collection;
  auto const colName = col.copyString();
  auto const& dbName = vocbase.name();
  auto const cluster = role == ServerState::ROLE_COORDINATOR ||
                       role == ServerState::ROLE_DBSERVER;

  if (cluster) {
    auto& ci = vocbase.server().getFeature<ClusterFeature>().clusterInfo();
    collection = ci.getCollection(dbName, colName);
  } else {  // single server, agents try to not break cluster
            // ready code
    methods::Collections::lookup(vocbase, colName, collection);
  }

  if (collection == nullptr) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND,
        absl::StrCat("could not find collection: ", colName, " in database ",
                     dbName));
  }

  std::string shardId;
  if (cluster) {
    auto maybeShardID = collection->getResponsibleShard(keys, true);
    if (maybeShardID.fail()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          maybeShardID.errorNumber(),
          absl::StrCat("could not find shard for document by shard keys ",
                       keys.toJson(), " in ", colName));
    }
    shardId = maybeShardID.get();
  } else {  // Agents, single server, AFO return the collection name in favour
            // of AQL universality
    shardId = colName;
  }

  return AqlValue{shardId};
}

/// @brief function COLLECTION_COUNT
AqlValue functions::CollectionCount(ExpressionContext* expressionContext,
                                    AstNode const&,
                                    VPackFunctionParametersView parameters) {
  // cppcheck-suppress variableScope
  static char const* AFN = "COLLECTION_COUNT";

  AqlValue const& element =
      aql::functions::extractFunctionParameterValue(parameters, 0);
  if (!element.isString()) {
    THROW_ARANGO_EXCEPTION_PARAMS(
        TRI_ERROR_QUERY_FUNCTION_ARGUMENT_TYPE_MISMATCH, AFN);
  }

  transaction::Methods* trx = &expressionContext->trx();

  TRI_ASSERT(ServerState::instance()->isSingleServerOrCoordinator());
  std::string const collectionName = element.slice().copyString();
  OperationOptions options(ExecContext::current());
  OperationResult res =
      trx->count(collectionName, transaction::CountType::kNormal, options);
  if (res.fail()) {
    THROW_ARANGO_EXCEPTION(res.result);
  }

  return AqlValue(res.slice());
}

}  // namespace arangodb::aql
