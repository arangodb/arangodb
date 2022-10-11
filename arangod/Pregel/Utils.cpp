////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
#include "Logger/LogMacros.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"
#include "Pregel/Worker/WorkerConfig.h"

using namespace arangodb;
using namespace arangodb::pregel;

std::string const Utils::apiPrefix = "/_api/pregel/";

std::string const Utils::executionNumberKey = "exn";
std::string const Utils::senderKey = "sender";

std::string const Utils::threshold = "threshold";
std::string const Utils::maxNumIterations = "maxNumIterations";
std::string const Utils::maxGSS = "maxGSS";
std::string const Utils::useMemoryMapsKey = "useMemoryMaps";
std::string const Utils::parallelismKey = "parallelism";

std::string const Utils::globalSuperstepKey = "gss";
std::string const Utils::phaseFirstStepKey = "phase-first-step";
std::string const Utils::aggregatorValuesKey = "aggregators";

size_t const Utils::batchOfVerticesStoredBeforeUpdatingStatus = 1000;
size_t const Utils::batchOfVerticesProcessedBeforeUpdatingStatus = 1000;

ErrorCode Utils::resolveShard(ClusterInfo& ci, WorkerConfig const* config,
                              std::string const& collectionName,
                              std::string const& shardKey,
                              std::string_view vertexKey,
                              std::string& responsibleShard) {
  if (!ServerState::instance()->isRunningInCluster()) {
    responsibleShard = collectionName;
    return TRI_ERROR_NO_ERROR;
  }

  auto const& planIDMap = config->collectionPlanIdMap();
  std::shared_ptr<LogicalCollection> info;
  auto const& it = planIDMap.find(collectionName);
  if (it != planIDMap.end()) {
    info = ci.getCollectionNT(config->database(), it->second);  // might throw
    if (info == nullptr) {
      return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
    }
  } else {
    LOG_TOPIC("67fda", ERR, Logger::PREGEL)
        << "The collection could not be translated to a planID";
    return TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND;
  }

  TRI_ASSERT(info != nullptr);

  VPackBuilder partial;
  partial.openObject();
  partial.add(shardKey, VPackValuePair(vertexKey.data(), vertexKey.size(),
                                       VPackValueType::String));
  partial.close();
  //  LOG_TOPIC("00a5c", INFO, Logger::PREGEL) << "Partial doc: " <<
  //  partial.toJson();
  return info->getResponsibleShard(partial.slice(), false, responsibleShard);
}
