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
#include "Basics/ReadWriteLock.h"
// TODO
#include "Basics/threads.h"
#include "Replication/ReplicationApplier.h"
#include "VocBase/voc-types.h"

struct TRI_vocbase_t;

namespace arangodb {

/// @brief replication applier for a single database
class DatabaseReplicationApplier final : public ReplicationApplier {
  friend class ContinuousSyncer; // TODO
  friend class RestReplicationHandler; // TODO

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
  int start(TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId) override;
  
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
 
  /// @brief store the current applier state in the passed vpack builder 
  /// expects builder to be in an open Object state
  void toVelocyPack(arangodb::velocypack::Builder& result) const override;

  /// @brief return the current configuration
  ReplicationApplierConfiguration configuration() const override;

  /// @brief pauses and checks whether the apply thread should terminate
  bool wait(uint64_t);

  /// @brief whether or not autostart option was set
  bool autoStart() const;

  bool isTerminated() { return _terminateThread.load(); }

  void setTermination(bool value) { _terminateThread.store(value); }
  
  /// @brief test if the replication applier is running
  bool isRunning() const;

  /// @brief block the replication applier from starting
  int preventStart();

  /// @brief unblock the replication applier from starting
  int allowStart();

  /// @brief check whether the initial synchronization should be stopped
  bool stopInitialSynchronization() const;

  /// @brief stop the initial synchronization
  void stopInitialSynchronization(bool value);

  /// @brief stop the replication applier
  int stop(bool resetError, bool joinThread);

  /// @brief set the progress
  void setProgress(char const* msg);
  void setProgress(std::string const& msg);

  /// @brief register an applier error
  int setError(int errorCode, char const* msg);
  int setError(int errorCode, std::string const& msg);
  
  /// @brief increase the starts counter
  void started() { ++_starts; }

  /// @brief factory function for creating a database-specific replication applier
  static DatabaseReplicationApplier* create(TRI_vocbase_t* vocbase);

  /// @brief load a persisted configuration for the applier
  static ReplicationApplierConfiguration loadConfiguration(TRI_vocbase_t* vocbase);

  /// @brief save the replication application configuration to a file
  void storeConfiguration(bool doSync);

 private:
  /// @brief register an applier error
  int setErrorNoLock(int errorCode, std::string const& msg);
  
  void setProgressNoLock(std::string const& msg);

  std::string getStateFilename() const;
   
 private:
  TRI_vocbase_t* _vocbase;
  std::string _databaseName; // TODO: required?
  std::atomic<uint64_t> _starts; 
  
  mutable arangodb::basics::ReadWriteLock _statusLock;
  std::atomic<bool> _terminateThread;
  TRI_thread_t _thread; // TODO
};

}

#endif
