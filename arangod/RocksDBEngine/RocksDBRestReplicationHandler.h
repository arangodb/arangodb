////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ROCKSDB_REST_REPLICATION_HANDLER_H
#define ARANGOD_ROCKSDB_ROCKSDB_REST_REPLICATION_HANDLER_H 1

#include "RestHandler/RestReplicationHandler.h"

#include "RocksDBEngine/RocksDBReplicationManager.h"

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief replication request handler
////////////////////////////////////////////////////////////////////////////////

class RocksDBRestReplicationHandler : public RestReplicationHandler {
 public:
  RocksDBRestReplicationHandler(GeneralRequest*, GeneralResponse*);
  ~RocksDBRestReplicationHandler();

 public:

  char const* name() const override final {
    return "RocksDBRestReplicationHandler";
  }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the state of the replication logger
  /// @route GET logger-state
  /// @caller Syncer::getMasterState
  /// @response VPackObject describing the ServerState in a certain point
  ///           * state (server state)
  ///           * server (version / id)
  ///           * clients (list of followers)
  //////////////////////////////////////////////////////////////////////////////

  void handleCommandLoggerState() override;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the available logfile range
  /// @route GET logger-tick-ranges
  /// @caller js/client/modules/@arangodb/replication.js
  /// @response VPackArray, containing info about each datafile
  ///           * filename
  ///           * status
  ///           * tickMin - tickMax
  //////////////////////////////////////////////////////////////////////////////
  
  void handleCommandLoggerTickRanges() override;
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the first tick available in a logfile
  /// @route GET logger-first-tick
  /// @caller js/client/modules/@arangodb/replication.js
  /// @response VPackObject with minTick of LogfileManager->ranges()
  //////////////////////////////////////////////////////////////////////////////
  
  void handleCommandLoggerFirstTick() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handle a follow command for the replication log
  //////////////////////////////////////////////////////////////////////////////

  void handleCommandLoggerFollow() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handle the command to determine the transactions that were open
  /// at a certain point in time
  //////////////////////////////////////////////////////////////////////////////

  void handleCommandDetermineOpenTransactions() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handle a batch command
  //////////////////////////////////////////////////////////////////////////////

  void handleCommandBatch() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the inventory (current replication and collection state)
  //////////////////////////////////////////////////////////////////////////////

  void handleCommandInventory() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief produce list of keys for a specific collection
  //////////////////////////////////////////////////////////////////////////////

  void handleCommandCreateKeys() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns a key range
  //////////////////////////////////////////////////////////////////////////////

  void handleCommandGetKeys() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns date for a key range
  //////////////////////////////////////////////////////////////////////////////

  void handleCommandFetchKeys() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief remove a list of keys for a specific collection
  //////////////////////////////////////////////////////////////////////////////

  void handleCommandRemoveKeys() override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handle a dump command for a specific collection
  //////////////////////////////////////////////////////////////////////////////

  void handleCommandDump() override;

 private:

  /// Manage RocksDBReplicationContext containing the dump state for the initial
  /// sync and incremental sync
  RocksDBReplicationManager* _manager;
};
}

#endif
