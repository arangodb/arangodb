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

#ifndef ARANGOD_REPLICATION_REPLICATION_APPLIER_H
#define ARANGOD_REPLICATION_REPLICATION_APPLIER_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/ReplicationApplierState.h"

namespace arangodb {
class TailingSyncer;
class Thread;

namespace velocypack {
class Builder;
}

/// @brief replication applier interface
class ReplicationApplier {
 public:
  ReplicationApplier(ReplicationApplierConfiguration const& configuration, std::string&& databaseName);

  virtual ~ReplicationApplier();
  
  ReplicationApplier(ReplicationApplier const&) = delete;
  ReplicationApplier& operator=(ReplicationApplier const&) = delete;

  /// @brief stop the applier and "forget" everything
  virtual void forget() = 0;
  
  /// @brief start the replication applier
  virtual void start(TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId);
  
  /// @brief stop the replication applier
  virtual void stop(bool resetError, bool joinThread);

  /// @brief shuts down the replication applier
  virtual void shutdown();

  /// @brief configure the replication applier
  virtual void reconfigure(ReplicationApplierConfiguration const& configuration);

  /// @brief store the configuration for the applier
  virtual void storeConfiguration(bool doSync) = 0;

  /// @brief remove the replication application state file
  virtual void removeState();
  
  /// @brief load the applier state from persistent storage
  /// returns whether a previous state was found
  virtual bool loadState() = 0;
  
  /// @brief store the applier state in persistent storage
  virtual void persistState(bool doSync);
 
  /// @brief store the current applier state in the passed vpack builder 
  virtual void toVelocyPack(arangodb::velocypack::Builder& result) const;
  
  /// @brief return the current configuration
  virtual ReplicationApplierConfiguration configuration() const;

  bool isTerminated() { return _terminateThread.load(); }

  void setTermination(bool value) { _terminateThread.store(value); }

  /// @brief test if the replication applier is running
  bool isRunning() const;
  
  /// @brief pauses and checks whether the apply thread should terminate
  bool wait(uint64_t);
  
  /// @brief block the replication applier from starting
  int preventStart();
  
  /// @brief whether or not autostart option was set
  bool autoStart() const;
  
  /// @brief check whether the initial synchronization should be stopped
  bool stopInitialSynchronization() const;

  /// @brief stop the initial synchronization
  void stopInitialSynchronization(bool value);

  /// @brief unblock the replication applier from starting
  int allowStart();

  // TODO: can these be made protected?
  /// @brief register an applier error
  int setError(int errorCode, char const* msg);
  int setError(int errorCode, std::string const& msg);
  
  /// @brief set the progress
  void setProgress(char const* msg);
  void setProgress(std::string const& msg);

 protected:
  virtual std::unique_ptr<TailingSyncer> buildSyncer(TRI_voc_tick_t initialTick, bool useTick, TRI_voc_tick_t barrierId) = 0;
  
  virtual std::string getStateFilename() const = 0;

  /// @brief register an applier error
  int setErrorNoLock(int errorCode, std::string const& msg);
  void setProgressNoLock(std::string const& msg);

 protected:
  ReplicationApplierConfiguration _configuration;
  ReplicationApplierState _state;
  
  mutable arangodb::basics::ReadWriteLock _statusLock;
  std::atomic<bool> _terminateThread;

  std::string _databaseName;

  std::unique_ptr<Thread> _thread;
};

}

#endif
