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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_REPLICATION_REPLICATION_CLIENTS_H
#define ARANGOD_REPLICATION_REPLICATION_CLIENTS_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Replication/SyncerId.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

/// @brief struct representing how far a replication client (syncer)
/// has come in terms of WAL tailing
struct ReplicationClientProgress {
  /// @brief timestamp of when client last contacted us
  double lastSeenStamp;
  /// @brief timestamp of when this entry will be considered expired
  double expireStamp;
  /// @brief last log tick/WAL tick that was served for this client
  uint64_t lastServedTick;
  /// @brief server id of the client
  std::string const clientId;

  ReplicationClientProgress(double lastSeenStamp, double expireStamp,
                            uint64_t lastServedTick, std::string clientId)
      : lastSeenStamp(lastSeenStamp),
        expireStamp(expireStamp),
        lastServedTick(lastServedTick),
        clientId(std::move(clientId)) {}
};

/// @brief class to track progress of individual replication clients (syncers)
/// for a particular database
class ReplicationClientsProgressTracker {
 public:
  ReplicationClientsProgressTracker() = default;
#ifndef ARANGODB_ENABLE_MAINTAINER_MODE
  ~ReplicationClientsProgressTracker() = default;
#else
  ~ReplicationClientsProgressTracker();
#endif

  ReplicationClientsProgressTracker(ReplicationClientsProgressTracker const&) = delete;
  ReplicationClientsProgressTracker& operator=(ReplicationClientsProgressTracker const&) = delete;

  /// @brief simply extend the lifetime of a specific syncer, so that its entry
  /// does not expire does not update the syncer's lastServedTick value
  void extend(SyncerId syncerId, std::string const& clientId, double ttl);

  /// @brief simply update the progress of a specific syncer, so that its entry
  /// does not expire this will update the syncer's lastServedTick value
  void track(SyncerId syncerId, std::string const& clientId,
             uint64_t lastServedTick, double ttl);

  /// @brief remove a specific syncer's entry
  void untrack(SyncerId syncerId, std::string const& clientId);

  /// @brief serialize the existing syncers to a VelocyPack builder
  void toVelocyPack(velocypack::Builder& builder) const;

  /// @brief garbage-collect the existing list of syncers
  /// thresholdStamp is the timestamp before all older entries will
  /// be collected
  void garbageCollect(double thresholdStamp);

  /// @brief return the lowest lastServedTick value for all syncers
  /// returns UINT64_MAX in case no syncers are registered
  uint64_t lowestServedValue() const;

 private:
  static inline std::string getKey(SyncerId syncerId, std::string const& clientId) {
    // For backwards compatible APIs, we might not have a syncer ID;
    // fall back to the clientId in that case. SyncerId was introduced in 3.5.0.
    // The only public API using this, /_api/wal/tail, marked the serverId
    // parameter (corresponding to clientId here) as deprecated in 3.5.0.
    if (syncerId.value != 0) {
      return syncerId.toString();
    }

    return clientId;
  }

 private:
  mutable basics::ReadWriteLock _lock;

  /// @brief mapping from SyncerId -> progress
  std::unordered_map<std::string, ReplicationClientProgress> _clients;
};

}  // namespace arangodb

#endif
