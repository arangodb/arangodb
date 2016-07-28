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

#ifndef ARANGOD_CLUSTER_SERVER_JOB_H
#define ARANGOD_CLUSTER_SERVER_JOB_H 1

#include "Dispatcher/Job.h"

#include "Basics/Exceptions.h"
#include "Basics/Mutex.h"

namespace arangodb {
class HeartbeatThread;

struct DBServerAgencySyncResult {
  bool success;
  uint64_t planVersion;
  uint64_t currentVersion;

  DBServerAgencySyncResult() : success(false), planVersion(0), currentVersion(0) {
  }

  DBServerAgencySyncResult(const DBServerAgencySyncResult& other)
    : success(other.success),
    planVersion(other.planVersion),
    currentVersion(other.currentVersion) {
    }
};

class DBServerAgencySync : public arangodb::rest::Job {
  DBServerAgencySync(DBServerAgencySync const&) = delete;
  DBServerAgencySync& operator=(DBServerAgencySync const&) = delete;

 public:
  explicit DBServerAgencySync(HeartbeatThread* heartbeat);
  ~DBServerAgencySync();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief abandon job
  //////////////////////////////////////////////////////////////////////////////

  void abandon();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the job is detached
  //////////////////////////////////////////////////////////////////////////////

  inline bool isDetached() const { return true; }

  void work() override;

  bool cancel() override;

  void cleanup(rest::DispatcherQueue* queue) override;

  bool beginShutdown() {
    _shutdown = 1;
    return true;
  }

  void handleError(basics::Exception const& ex) override {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief execute job
  //////////////////////////////////////////////////////////////////////////////

  DBServerAgencySyncResult execute();

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the heartbeat thread
  //////////////////////////////////////////////////////////////////////////////

  HeartbeatThread* _heartbeat;

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief shutdown in progress
  //////////////////////////////////////////////////////////////////////////////

  volatile sig_atomic_t _shutdown;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief server is dead lock
  //////////////////////////////////////////////////////////////////////////////

  Mutex _abandonLock;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief server is dead
  //////////////////////////////////////////////////////////////////////////////

  bool _abandon;
};
}

#endif
