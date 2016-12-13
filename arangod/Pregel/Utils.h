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

#ifndef ARANGODB_PREGEL_UTILS_H
#define ARANGODB_PREGEL_UTILS_H 1

#include <cstdint>
#include "Basics/Common.h"
#include "Cluster/ClusterComm.h"

struct TRI_vocbase_t;

namespace arangodb {
class LogicalCollection;
namespace pregel {

class Utils {
  Utils() = delete;

 public:
  // constants

  static std::string const edgeShardingKey;
  static std::string const apiPrefix;
  static std::string const startExecutionPath;
  static std::string const finishedStartupPath;
  static std::string const prepareGSSPath;
  static std::string const startGSSPath;
  static std::string const finishedWorkerStepPath;
  static std::string const cancelGSSPath;
  static std::string const messagesPath;
  static std::string const finalizeExecutionPath;
  static std::string const startRecoveryPath;
  static std::string const continueRecoveryPath;
  static std::string const finishedRecoveryPath;

  static std::string const executionNumberKey;
  static std::string const algorithmKey;
  static std::string const coordinatorIdKey;
  static std::string const collectionPlanIdMapKey;
  static std::string const vertexShardsKey;
  static std::string const edgeShardsKey;
  static std::string const globalShardListKey;
  static std::string const totalVertexCount;
  static std::string const totalEdgeCount;
  static std::string const asyncMode;
  static std::string const gssDone;

  static std::string const globalSuperstepKey;
  static std::string const messagesKey;
  static std::string const senderKey;
  static std::string const recoveryMethodKey;
  static std::string const compensate;
  static std::string const rollback;

  static std::string const storeResultsKey;
  static std::string const aggregatorValuesKey;
  static std::string const activeCountKey;
  static std::string const receivedCountKey;
  static std::string const sendCountKey;
  static std::string const superstepRuntimeKey;

  // User parameters
  static std::string const userParametersKey;

  static std::string baseUrl(std::string dbName);
  static void printResponses(std::vector<ClusterCommRequest> const& requests);

  static int64_t countDocuments(TRI_vocbase_t* vocbase,
                                std::string const& collection);
  static std::shared_ptr<LogicalCollection> resolveCollection(
      std::string const& database, std::string const& collectionName,
      std::map<std::string, std::string> const& collectionPlanIdMap);
  static void resolveShard(LogicalCollection* info, std::string const& shardKey,
                           std::string const& vertexKey,
                           std::string& responsibleShard);
};
}
}
#endif
