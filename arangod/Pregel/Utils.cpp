////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

using namespace arangodb;
using namespace arangodb::pregel;

std::string const Utils::apiPrefix = "/_api/pregel/";
std::string const Utils::conductorPrefix = "conductor";
std::string const Utils::workerPrefix = "worker";

std::string const Utils::startExecutionPath = "startExecution";
std::string const Utils::finishedStartupPath = "finishedStartup";
std::string const Utils::statusUpdatePath = "statusUpdate";
std::string const Utils::prepareGSSPath = "prepareGSS";
std::string const Utils::startGSSPath = "startGSS";
std::string const Utils::finishedWorkerStepPath = "finishedStep";
std::string const Utils::finishedWorkerFinalizationPath =
    "finishedFinalization";
std::string const Utils::messagesPath = "messages";
std::string const Utils::finalizeExecutionPath = "finalizeExecution";
std::string const Utils::storeCheckpointPath = "storeCheckpoint";
std::string const Utils::aqlResultsPath = "aqlResult";

std::string const Utils::executionNumberKey = "exn";
std::string const Utils::algorithmKey = "algorithm";
std::string const Utils::coordinatorIdKey = "coordinatorId";
std::string const Utils::collectionPlanIdMapKey = "collectionPlanIdMap";
std::string const Utils::edgeCollectionRestrictionsKey =
    "edgeCollectionRestrictions";
std::string const Utils::vertexShardsKey = "vertexShards";
std::string const Utils::edgeShardsKey = "edgeShards";
std::string const Utils::globalShardListKey = "globalShardList";
std::string const Utils::userParametersKey = "userparams";
std::string const Utils::parallelismKey = "parallelism";

std::string const Utils::globalSuperstepKey = "gss";
std::string const Utils::phaseFirstStepKey = "phase-first-step";
std::string const Utils::vertexCountKey = "vertexCount";
std::string const Utils::edgeCountKey = "edgeCount";
std::string const Utils::shardIdKey = "shrdId";
std::string const Utils::messagesKey = "msgs";
std::string const Utils::senderKey = "sender";
std::string const Utils::payloadKey = "payload";
std::string const Utils::storeResultsKey = "storeResults";
std::string const Utils::aggregatorValuesKey = "aggregators";
std::string const Utils::activeCountKey = "activeCount";
std::string const Utils::receivedCountKey = "receivedCount";
std::string const Utils::sendCountKey = "sendCount";
std::string const Utils::maxNumIterations = "maxNumIterations";
std::string const Utils::threshold = "threshold";
std::string const Utils::maxGSS = "maxGSS";

std::string const Utils::compensate = "compensate";
std::string const Utils::rollback = "rollback";
std::string const Utils::reportsKey = "reports";

std::string const Utils::workerToMasterMessagesKey = "workerToMasterMessages";
std::string const Utils::masterToWorkerMessagesKey = "masterToWorkerMessages";

std::string const Utils::equivalenceClass = "equivalenceClass";
std::string const Utils::inputColorsFieldName = "inputColorsFieldName";
std::string const Utils::outputColorsFieldName = "outputColorsFieldName";
std::string const Utils::numColors = "numColors";
std::string const Utils::colors = "colors";

size_t const Utils::batchOfVerticesStoredBeforeUpdatingStatus = 1000;
size_t const Utils::batchOfVerticesProcessedBeforeUpdatingStatus = 1000;

std::string Utils::baseUrl(std::string const& prefix) {
  return Utils::apiPrefix + prefix + "/";
}
