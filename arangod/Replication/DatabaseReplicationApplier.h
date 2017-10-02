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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REPLICATION_DATABASE_REPLICATION_APPLIER_H
#define ARANGOD_REPLICATION_DATABASE_REPLICATION_APPLIER_H 1

#include "Basics/Common.h"
#include "Replication/ReplicationApplier.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {
/// @brief replication applier for a single database
class DatabaseReplicationApplier final : public ReplicationApplier {
  friend class DatabaseTailingSyncer;
  friend class RestReplicationHandler;

 public:
  explicit DatabaseReplicationApplier(TRI_vocbase_t* vocbase);

  DatabaseReplicationApplier(ReplicationApplierConfiguration const& configuration, 
                             TRI_vocbase_t* vocbase);

  ~DatabaseReplicationApplier();

  /// @brief stop the applier and "forget" everything
  void forget() override;

  /// @brief shuts down the replication applier
  void shutdown() override;
  
  /// @brief start the replication applier
  void start(TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId) override;
  
  /// @brief stop the replication applier
  void stop(bool resetError, bool joinThread) override;
  
  /// @brief configure the replication applier
  void reconfigure(ReplicationApplierConfiguration const& configuration) override;

  /// @brief remove the replication application state file
  void removeState() override;
  
  /// @brief load the applier state from persistent storage
  /// must currently be called while holding the write-lock
  /// returns whether a previous state was found
  bool loadState() override;
  
  /// @brief store the applier state in persistent storage
  /// must currently be called while holding the write-lock
  void persistState(bool doSync) override;
 
  /// @brief save the replication application configuration to a file
  void storeConfiguration(bool doSync) override;

  /// @brief factory function for creating a database-specific replication applier
  static DatabaseReplicationApplier* create(TRI_vocbase_t* vocbase);

  /// @brief load a persisted configuration for the applier
  static ReplicationApplierConfiguration loadConfiguration(TRI_vocbase_t* vocbase);
  
 protected:
  Thread* buildApplyThread(TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId) override;

  std::string getStateFilename() const override;
   
 private:
  TRI_vocbase_t* _vocbase;
};

}

#endif
