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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REPLICATION_REPLICATION_APPLIER_H
#define ARANGOD_REPLICATION_REPLICATION_APPLIER_H 1

#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Basics/Thread.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/ReplicationApplierState.h"

namespace arangodb {
class InitialSyncer;
class TailingSyncer;

namespace velocypack {
class Builder;
}

/// @brief replication applier interface
class ReplicationApplier {
  friend class TailingSyncer;

 public:
  ReplicationApplier(ReplicationApplierConfiguration const& configuration,
                     std::string&& databaseName);

  virtual ~ReplicationApplier() = default;

  ReplicationApplier(ReplicationApplier const&) = delete;
  ReplicationApplier& operator=(ReplicationApplier const&) = delete;

  std::string const& databaseName() const { return _databaseName; }

  /// @brief whether or not the applier is the global one
  virtual bool isGlobal() const = 0;

  /// @brief execute the check condition
  virtual bool applies() const = 0;

  /// @brief stop the applier and "forget" everything
  virtual void forget() = 0;

  /// @brief test if the replication applier is running
  bool isActive() const;

  /// @brief test if the repication applier is performing initial sync
  bool isInitializing() const;

  /// @brief test if the replication applier is shutting down
  bool isShuttingDown() const;

  /// @brief set the applier state to tailing
  void markThreadTailing();

  /// @brief set the applier state to stopped
  void markThreadStopped();

  /// @brief perform a complete replication dump and then tail continiously
  void startReplication();

  /// @brief switch to tailing mode, DO NOT USE EXTERNALLY
  void continueTailing(TRI_voc_tick_t initialTick, bool useTick);

  /// @brief start the replication applier in tailing
  void startTailing(TRI_voc_tick_t initialTick, bool useTick);

  /// @brief stop the replication applier, resets the error message
  void stop();

  /// @brief stop the replication applier
  void stop(Result const& r);

  /// @brief stop the replication applier and join the apply thread
  void stopAndJoin();

  /// @brief sleeps for the specific number of microseconds if the
  /// applier is still active, and returns true. if the applier is not
  /// active anymore, returns false
  bool sleepIfStillActive(uint64_t sleepTime);

  /// @brief configure the replication applier
  virtual void reconfigure(ReplicationApplierConfiguration const& configuration);

  /// @brief load the applier state from persistent storage
  bool loadState();

  /// @brief load the applier state from persistent storage
  /// must currently be called while holding the write-lock
  /// returns whether a previous state was found
  bool loadStateNoLock();

  /// @brief store the configuration for the applier
  virtual void storeConfiguration(bool doSync) = 0;

  /// @brief remove the replication application state file
  void removeState();

  /// @brief store the applier state in persistent storage
  void persistState(bool doSync);
  Result persistStateResult(bool doSync);

  Result resetState(bool reducedSet = false);

  /// @brief store the current applier state in the passed vpack builder
  void toVelocyPack(arangodb::velocypack::Builder& result) const;

  /// @brief return the current configuration
  ReplicationApplierConfiguration configuration() const;

  /// @brief return the current endpoint
  std::string endpoint() const;

  /// @brief return last persisted tick
  TRI_voc_tick_t lastTick() const;

  /// @brief block the replication applier from starting
  Result preventStart();

  /// @brief whether or not autostart option was set
  bool autoStart() const;

  /// @brief whether or not the applier has a state already
  bool hasState() const;

  /// @brief check whether the initial synchronization should be stopped
  bool stopInitialSynchronization() const;

  /// @brief unblock the replication applier from starting
  void allowStart();

  /// @brief register an applier error
  void setError(arangodb::Result const&);

  Result lastError() const;

  /// @brief set the progress
  void setProgress(char const* msg);
  void setProgress(std::string const& msg);

  virtual std::shared_ptr<InitialSyncer> buildInitialSyncer() const = 0;
  virtual std::shared_ptr<TailingSyncer> buildTailingSyncer(
      TRI_voc_tick_t initialTick, bool useTick) const = 0;

 protected:
  virtual std::string getStateFilename() const = 0;

  /// @brief register an applier error
  void setErrorNoLock(arangodb::Result const&);
  void setProgressNoLock(std::string const& msg);

 private:
  /// Perform some common ops for startReplication / startTailing
  void doStart(std::function<void()>&&, ReplicationApplierState::ActivityPhase);

  /// @brief stop the replication applier and join the apply thread
  void doStop(Result const& r, bool joinThread);

  static void readTick(arangodb::velocypack::Slice const& slice,
                       char const* attributeName, TRI_voc_tick_t& dst, bool allowNull);

 protected:
  ReplicationApplierConfiguration _configuration;
  ReplicationApplierState _state;
  /// @brief workaround for deadlock in stop() method
  /// check for termination without needing _statusLock
  mutable arangodb::basics::ReadWriteLock _statusLock;

  // used only for logging
  std::string _databaseName;
  std::unique_ptr<Thread> _thread;
};

}  // namespace arangodb

#endif
