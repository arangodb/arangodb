////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/ReadWriteLock.h"
#include "Basics/Result.h"
#include "Replication/ReplicationApplierConfiguration.h"
#include "Replication/ReplicationApplierState.h"

namespace arangodb {
class TailingSyncer;

/// @brief replication applier interface
class ReplicationApplier {
  friend class TailingSyncer;

 public:
  ReplicationApplier(ReplicationApplierConfiguration const& configuration,
                     std::string&& databaseName);

  virtual ~ReplicationApplier() = default;

  ReplicationApplier(ReplicationApplier const&) = delete;
  ReplicationApplier& operator=(ReplicationApplier const&) = delete;

  /// @brief whether or not the applier is the global one
  virtual bool isGlobal() const = 0;

  /// @brief execute the check condition
  virtual bool applies() const = 0;

  /// @brief stop the replication applier, resets the error message
  void stop();

  /// @brief stop the replication applier and join the apply thread
  void stopAndJoin();

  /// @brief load the applier state from persistent storage
  bool loadState();

  /// @brief load the applier state from persistent storage
  /// must currently be called while holding the write-lock
  /// returns whether a previous state was found
  bool loadStateNoLock();

  /// @brief store the applier state in persistent storage
  void persistState(bool doSync);
  Result persistStateResult(bool doSync);

  /// @brief block the replication applier from starting
  Result preventStart();

  /// @brief whether or not the applier has a state already
  bool hasState() const;

  /// @brief check whether the initial synchronization should be stopped
  bool stopInitialSynchronization() const;

  /// @brief unblock the replication applier from starting
  void allowStart();

  /// @brief set the progress
  void setProgress(char const* msg);
  void setProgress(std::string const& msg);

 protected:
  virtual std::string getStateFilename() const = 0;

  void setProgressNoLock(std::string const& msg);

 private:
  /// @brief stop the replication applier and join the apply thread
  void doStop(Result const& r, bool joinThread);

  static void readTick(arangodb::velocypack::Slice const& slice,
                       char const* attributeName, TRI_voc_tick_t& dst,
                       bool allowNull);

 protected:
  ReplicationApplierConfiguration _configuration;
  ReplicationApplierState _state;
  /// @brief workaround for deadlock in stop() method
  /// check for termination without needing _statusLock
  mutable arangodb::basics::ReadWriteLock _statusLock;

  // used only for logging
  std::string _databaseName;
};

}  // namespace arangodb
