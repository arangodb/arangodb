////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CLUSTER_REST_REPLICATION_HANDLER_H
#define ARANGOD_CLUSTER_CLUSTER_REST_REPLICATION_HANDLER_H 1

#include "RestHandler/RestReplicationHandler.h"

namespace arangodb {

/// @brief replication request handler
class ClusterRestReplicationHandler : public RestReplicationHandler {
 public:
  ClusterRestReplicationHandler(application_features::ApplicationServer&,
                                GeneralRequest*, GeneralResponse*);
  ~ClusterRestReplicationHandler() = default;

 public:
  char const* name() const override final {
    return "ClusterRestReplicationHandler";
  }

 private:
  /// @brief handle a follow command for the replication log
  void handleCommandLoggerFollow() override;

  /// @brief handle the command to determine the transactions that were open
  /// at a certain point in time
  void handleCommandDetermineOpenTransactions() override;

  /// @brief handle a batch command
  void handleCommandBatch() override;

  /// @brief add or remove a WAL logfile barrier
  void handleCommandBarrier() override;

  /// @brief return the inventory (current replication and collection state)
  void handleCommandInventory() override;

  /// @brief produce list of keys for a specific collection
  void handleCommandCreateKeys() override;

  /// @brief returns a key range
  void handleCommandGetKeys() override;

  /// @brief returns date for a key range
  void handleCommandFetchKeys() override;

  /// @brief remove a list of keys for a specific collection
  void handleCommandRemoveKeys() override;

  /// @brief handle a dump command for a specific collection
  void handleCommandDump() override;
};
}  // namespace arangodb

#endif
