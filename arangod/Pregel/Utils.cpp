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
#include "Cluster/ServerState.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

using namespace arangodb;
using namespace arangodb::pregel;

std::string const Utils::apiPrefix = "/_api/pregel/";
std::string const Utils::conductorPrefix = "conductor";
std::string const Utils::workerPrefix = "worker";

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
std::string const Utils::finalizeRecoveryPath = "finalizeRecovery";
std::string const Utils::storeCheckpointPath = "storeCheckpoint";
std::string const Utils::aqlResultsPath = "aqlResult";

std::string const Utils::executionNumberKey = "exn";
std::string const Utils::algorithmKey = "algorithm";
std::string const Utils::coordinatorIdKey = "coordinatorId";
std::string const Utils::collectionPlanIdMapKey = "collectionPlanIdMap";
std::string const Utils::vertexShardsKey = "vertexShards";
std::string const Utils::edgeShardsKey = "edgeShards";
std::string const Utils::globalShardListKey = "globalShardList";
std::string const Utils::userParametersKey = "userparams";
std::string const Utils::asyncModeKey = "asyncMode";
std::string const Utils::lazyLoadingKey = "lazyloading";
std::string const Utils::parallelismKey = "parallelism";

std::string const Utils::globalSuperstepKey = "gss";
std::string const Utils::vertexCountKey = "vertexCount";
std::string const Utils::edgeCountKey = "edgeCount";
std::string const Utils::shardIdKey = "shrdId";
std::string const Utils::messagesKey = "msgs";
std::string const Utils::senderKey = "sender";
std::string const Utils::recoveryMethodKey = "rmethod";
std::string const Utils::storeResultsKey = "storeResults";
std::string const Utils::aggregatorValuesKey = "aggregators";
std::string const Utils::activeCountKey = "activeCount";
std::string const Utils::receivedCountKey = "receivedCount";
std::string const Utils::sendCountKey = "sendCount";
std::string const Utils::enterNextGSSKey = "nextGSS";

std::string const Utils::compensate = "compensate";
std::string const Utils::rollback = "rollback";

std::string Utils::baseUrl(std::string const& dbName, std::string const& prefix) {
  return "/_db/" + basics::StringUtils::urlEncode(dbName) + Utils::apiPrefix +
         prefix + "/";
}

void Utils::printResponses(std::vector<ClusterCommRequest> const& requests) {
  for (auto const& req : requests) {
    auto& res = req.result;
    if (res.status == CL_COMM_RECEIVED && res.answer_code != rest::ResponseCode::OK) {
      LOG_TOPIC(ERR, Logger::PREGEL)
          << "Error sending request to " << req.destination
          << ". Payload: " << res.answer->payload().toJson();
    }
  }
}

int Utils::resolveShard(WorkerConfig const* config, std::string const& collectionName,
                        std::string const& shardKey, std::string const& vertexKey,
                        std::string& responsibleShard) {
  if (ServerState::instance()->isRunningInCluster() == false) {
    responsibleShard = collectionName;
    return TRI_ERROR_NO_ERROR;
  }

  auto const& planIDMap = config->collectionPlanIdMap();
  ClusterInfo* ci = ClusterInfo::instance();
  std::shared_ptr<LogicalCollection> info;
  auto const& it = planIDMap.find(collectionName);
  if (it != planIDMap.end()) {
    info = ci->getCollection(config->database(), it->second);  // might throw
    if (info == nullptr) {
      return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
    }
  } else {
    LOG_TOPIC(ERR, Logger::PREGEL)
        << "The collection could not be translated to a planID";
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  TRI_ASSERT(info != nullptr);

  VPackBuilder partial;
  partial.openObject();
  partial.add(shardKey, VPackValue(vertexKey));
  partial.close();
  //  LOG_TOPIC(INFO, Logger::PREGEL) << "Partial doc: " << partial.toJson();
  return info->getResponsibleShard(partial.slice(), false, responsibleShard);
}
