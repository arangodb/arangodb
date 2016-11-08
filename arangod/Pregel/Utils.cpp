////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Utils.h"
#include "Basics/StringUtils.h"

#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "VocBase/vocbase.h"

using namespace arangodb::pregel;

std::string const Utils::apiPrefix = "/_api/pregel/";

std::string const Utils::startExecutionPath = "startExecution";
std::string const Utils::startGSSPath = "startGSS";
std::string const Utils::finishedGSSPath = "finishedGSS";
std::string const Utils::messagesPath = "messages";
std::string const Utils::finalizeExecutionPath = "finalizeExecution";

std::string const Utils::executionNumberKey = "exn";
std::string const Utils::totalVertexCount = "vertexCount";
std::string const Utils::totalEdgeCount = "edgeCount";

std::string const Utils::collectionPlanIdMapKey = "collectionPlanIdMap";
std::string const Utils::vertexShardsListKey = "vertexShards";
std::string const Utils::edgeShardsListKey = "edgeShards";

// std::string const Utils::resultShardKey = "resultShard";

std::string const Utils::coordinatorIdKey = "coordinatorId";
std::string const Utils::algorithmKey = "algorithm";
std::string const Utils::globalSuperstepKey = "gss";
std::string const Utils::messagesKey = "msgs";
std::string const Utils::senderKey = "sender";
std::string const Utils::doneKey = "done";

std::string const Utils::parameterMapKey = "params";
std::string const Utils::aggregatorValuesKey = "aggregators";

std::string const Utils::edgeShardingKey = "_vertex";

std::string Utils::baseUrl(std::string dbName) {
  return "/_db/" + basics::StringUtils::urlEncode(dbName) + Utils::apiPrefix;
}

std::string Utils::collectionFromToValue(std::string const& graphKey) {
  std::size_t pos = graphKey.find('/');
  return graphKey.substr(0, pos + 1);
}

std::string Utils::vertexKeyFromToValue(std::string const& graphKey) {
  std::size_t pos = graphKey.find('/');
  return graphKey.substr(
      pos + 1, graphKey.length() - pos -
                   1);  // if pos == -1 (npos) the entire string is returned
}

int64_t Utils::countDocuments(TRI_vocbase_t* vocbase,
                              CollectionID const& collection) {
  SingleCollectionTransaction trx(StandaloneTransactionContext::Create(vocbase),
                                  collection, TRI_TRANSACTION_READ);

  int res = trx.begin();
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  OperationResult opResult = trx.count(collection);
  res = trx.finish(opResult.code);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
  VPackSlice s = opResult.slice();
  TRI_ASSERT(s.isNumber());
  return s.getInt();
}
