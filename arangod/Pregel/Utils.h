////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_PREGEL_UTILS_H
#define ARANGODB_PREGEL_UTILS_H 1

#include <cstdint>

#include "Basics/Common.h"
#include "Cluster/ClusterInfo.h"
#include "Pregel/WorkerConfig.h"

struct TRI_vocbase_t;

namespace arangodb {
class LogicalCollection;
namespace pregel {

class Utils {
  Utils() = delete;

 public:
  // constants
  static std::string const apiPrefix;
  static std::string const conductorPrefix;
  static std::string const workerPrefix;

  static std::string const edgeShardingKey;
  static std::string const startExecutionPath;
  static std::string const finishedStartupPath;
  static std::string const prepareGSSPath;
  static std::string const startGSSPath;
  static std::string const finishedWorkerStepPath;
  static std::string const finishedWorkerFinalizationPath;
  static std::string const cancelGSSPath;
  static std::string const messagesPath;
  static std::string const finalizeExecutionPath;
  static std::string const startRecoveryPath;
  static std::string const continueRecoveryPath;
  static std::string const finishedRecoveryPath;
  static std::string const finalizeRecoveryPath;
  static std::string const storeCheckpointPath;
  static std::string const aqlResultsPath;

  static std::string const executionNumberKey;
  static std::string const algorithmKey;
  static std::string const coordinatorIdKey;
  static std::string const collectionPlanIdMapKey;
  static std::string const edgeCollectionRestrictionsKey;
  static std::string const vertexShardsKey;
  static std::string const edgeShardsKey;
  static std::string const globalShardListKey;
  static std::string const userParametersKey;
  static std::string const asyncModeKey;
  static std::string const useMemoryMapsKey;
  static std::string const parallelismKey;
  static std::string const activateAllKey;

  /// Current global superstep
  static std::string const globalSuperstepKey;
  static std::string const phaseFirstStepKey;

  /// Communicate number of loaded vertices to conductor
  static std::string const vertexCountKey;

  /// Communicate number of loaded edges to conductor
  static std::string const edgeCountKey;

  /// Shard id, part of message header
  static std::string const shardIdKey;

  /// holds messages
  static std::string const messagesKey;

  /// sender cluster id
  static std::string const senderKey;

  /// Recovery method name
  static std::string const recoveryMethodKey;

  /// Tells workers to store the result into the collections
  /// otherwise dicard results
  static std::string const storeResultsKey;

  /// Holds aggregated values
  static std::string const aggregatorValuesKey;

  /// Dict of messages sent from WorkerContext to MasterContext after every GSS
  static std::string const workerToMasterMessagesKey;

  /// Dict of messages sent from MasterContext to all WorkerContexts before every GSS
  static std::string const masterToWorkerMessagesKey;

  /// Communicates the # of active vertices to the conductor
  static std::string const activeCountKey;

  /// Used to track number of messages received during the last
  /// superstep, by the worker (bookkeeping)
  static std::string const receivedCountKey;

  /// Used to track number of messages send during the last
  /// superstep (bookkeeping)
  static std::string const sendCountKey;

  /// Used to communicate to enter the next phase
  /// only send by the conductor
  static std::string const enterNextGSSKey;

  static std::string const compensate;
  static std::string const rollback;
  static std::string const reportsKey;

  // pass the db name and either "worker" or "conductor" as target.
  static std::string baseUrl(std::string const& target);

  static int64_t countDocuments(TRI_vocbase_t* vocbase, std::string const& collection);

  static ErrorCode resolveShard(ClusterInfo& ci, WorkerConfig const* config,
                                std::string const& collectionName, std::string const& shardKey,
                                arangodb::velocypack::StringRef vertexKey,
                                std::string& responsibleShard);
};
}  // namespace pregel
}  // namespace arangodb
#endif
