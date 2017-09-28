////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_REPLICATION_GLOBAL_INITIAL_SYNCER_H
#define ARANGOD_REPLICATION_GLOBAL_INITIAL_SYNCER_H 1

#include "Replication/InitialSyncer.h"

namespace arangodb {

/// Meta Syncer driving multiple initial syncer
class GlobalInitialSyncer {

 private:
  /// @brief apply phases
  typedef enum {
    PHASE_NONE,
    PHASE_INIT,
    PHASE_VALIDATE,
    PHASE_DROP_CREATE,
    PHASE_DUMP
  } sync_phase_e;

 public:
  GlobalInitialSyncer(TRI_vocbase_t*,
                      ReplicationApplierConfiguration const*,
                      std::unordered_map<std::string, bool> const&,
                      Syncer::RestrictType, bool verbose, bool skipCreateDrop);

  ~GlobalInitialSyncer();

 public:
  /// @brief run method, performs a full synchronization
  int run(std::string& errorMsg, bool incremental);

  std::string progress() { return _progress; }
  
  TRI_vocbase_t* vocbase() const {
    TRI_ASSERT(_vocbaseCache.size() == 1);
    return _vocbaseCache.begin()->second.vocbase();
  }

 private:
  
  /// @brief set a progress message
  void setProgress(std::string const& msg);

  /// @brief send a "start batch" command
  int sendStartBatch(std::string&);

  /// @brief send a "finish batch" command
  int sendFinishBatch();

  /// @brief check whether the initial synchronization should be aborted
  bool checkAborted();

  /// @brief fetch the server's inventory
  int fetchInventory(arangodb::velocypack::Builder& builder, std::string& errorMsg);

};
}

#endif
