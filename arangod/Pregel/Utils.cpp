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

#include "Cluster/ClusterInfo.h"
#include "Utils/SingleCollectionTransaction.h"
#include "Utils/StandaloneTransactionContext.h"

#include "VocBase/LogicalCollection.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::pregel;

std::string const Utils::apiPrefix = "/_api/pregel/";
std::string const Utils::startExecutionPath = "startExecution";
std::string const Utils::finishedStartupPath = "finishedStartup";
std::string const Utils::prepareGSSPath = "prepareGSS";
std::string const Utils::startGSSPath = "startGSS";
std::string const Utils::finishedWorkerStepPath = "finishedStep";
std::string const Utils::cancelGSSPath = "cancelGSS";
std::string const Utils::messagesPath = "messages";
std::string const Utils::finalizeExecutionPath = "finalizeExecution";
std::string const Utils::startRecoveryPath = "startRecovery";
std::string const Utils::continueRecoveryPath = "continueRecovery";
std::string const Utils::finishedRecoveryPath = "finishedRecovery";

std::string const Utils::executionNumberKey = "exn";
std::string const Utils::collectionPlanIdMapKey = "collectionPlanIdMap";
std::string const Utils::vertexShardsKey = "vertexShards";
std::string const Utils::edgeShardsKey = "edgeShards";
std::string const Utils::globalShardListKey = "globalShardList";
std::string const Utils::vertexCount = "vertexCount";
std::string const Utils::edgeCount = "edgeCount";
std::string const Utils::asyncMode = "async";

std::string const Utils::coordinatorIdKey = "coordinatorId";
std::string const Utils::algorithmKey = "algorithm";
std::string const Utils::globalSuperstepKey = "gss";
std::string const Utils::messagesKey = "msgs";
std::string const Utils::senderKey = "sender";
std::string const Utils::recoveryMethodKey = "rmethod";
std::string const Utils::compensate = "compensate";
std::string const Utils::rollback = "rollback";

std::string const Utils::storeResultsKey = "storeResults";
std::string const Utils::aggregatorValuesKey = "aggregators";
std::string const Utils::activeCountKey = "activeCount";
std::string const Utils::receivedCountKey = "receivedCount";
std::string const Utils::sendCountKey = "sendCount";
std::string const Utils::superstepRuntimeKey = "superstepRuntime";

std::string const Utils::userParametersKey = "userparams";

std::string Utils::baseUrl(std::string dbName) {
  return "/_db/" + basics::StringUtils::urlEncode(dbName) + Utils::apiPrefix;
}

void Utils::printResponses(std::vector<ClusterCommRequest> const& requests) {
  for (auto const& req : requests) {
    auto& res = req.result;
    if (res.status == CL_COMM_RECEIVED &&
        res.answer_code != rest::ResponseCode::OK) {
      LOG(ERR) << "Error sending request to "
               << req.destination << ". Payload: " << res.answer->payload().toJson();
    }
  }
}

std::shared_ptr<LogicalCollection> Utils::resolveCollection(
    std::string const& database, std::string const& collectionName,
    std::map<std::string, std::string> const& collectionPlanIdMap) {
  ClusterInfo* ci = ClusterInfo::instance();
  auto const& it = collectionPlanIdMap.find(collectionName);
  if (it != collectionPlanIdMap.end()) {
    return ci->getCollection(database, it->second);
  }
  return std::shared_ptr<LogicalCollection>();
}

void Utils::resolveShard(LogicalCollection* info, std::string const& shardKey,
                         std::string const& vertexKey,
                         std::string& responsibleShard) {
  ClusterInfo* ci = ClusterInfo::instance();
  bool usesDefaultShardingAttributes;
  VPackBuilder partial;
  partial.openObject();
  partial.add(shardKey, VPackValue(vertexKey));
  partial.close();
  //  LOG(INFO) << "Partial doc: " << partial.toJson();
  int res =
      ci->getResponsibleShard(info, partial.slice(), true, responsibleShard,
                              usesDefaultShardingAttributes);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION_MESSAGE(res,
                                   "could not resolve the responsible shard");
  }
  TRI_ASSERT(usesDefaultShardingAttributes);  // should be true anyway
}
