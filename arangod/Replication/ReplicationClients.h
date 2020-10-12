////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/debugging.h"
#include "Replication/SyncerId.h"
#include "VocBase/Identifiers/ServerId.h"

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
  TRI_voc_tick_t lastServedTick;
  /// @brief syncer id of the client
  SyncerId const syncerId;
  /// @brief server id of the client
  ServerId const clientId;
  /// @brief short descriptive information about the client
  std::string const clientInfo;

  ReplicationClientProgress(double lastSeenStamp, double expireStamp,
                            uint64_t lastServedTick, SyncerId syncerId,
                            ServerId clientId, std::string clientInfo)
      : lastSeenStamp(lastSeenStamp),
        expireStamp(expireStamp),
        lastServedTick(lastServedTick),
        syncerId(syncerId),
        clientId(clientId),
        clientInfo(std::move(clientInfo)) {}

  static double steadyClockToSystemClock(double steadyTimestamp);
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
  void extend(SyncerId syncerId, ServerId clientId, std::string const& clientInfo, double ttl);

  /// @brief simply update the progress of a specific syncer, so that its entry
  /// does not expire this will update the syncer's lastServedTick value
  void track(SyncerId syncerId, ServerId clientId, std::string const& clientInfo,
             TRI_voc_tick_t lastServedTick, double ttl);

  /// @brief remove a specific syncer's entry
  void untrack(SyncerId syncerId, ServerId clientId, std::string const& clientInfo);

  /// @brief serialize the existing syncers to a VelocyPack builder
  void toVelocyPack(velocypack::Builder& builder) const;

  /// @brief garbage-collect the existing list of syncers
  /// thresholdStamp is the timestamp before all older entries will
  /// be collected
  void garbageCollect(double thresholdStamp);

  /// @brief return the lowest lastServedTick value for all syncers
  /// returns UINT64_MAX in case no syncers are registered
  TRI_voc_tick_t lowestServedValue() const;

 private:
  // Make sure the underlying integer types for SyncerIDs and ClientIDs are the
  // same, so we can use one entry
  static_assert(std::is_same<decltype(SyncerId::value), ServerId::BaseType>::value,
                "Assuming identical underlying integer types. If these are "
                "changed, the client-map key must be changed, too.");
  enum class KeyType { INVALID, SYNCER_ID, SERVER_ID };
  union ClientKeyUnion {
    SyncerId syncerId;
    ServerId clientId;
  };
  using ClientKey = std::pair<KeyType, ClientKeyUnion>;
  class ClientHash {
   public:
    inline size_t operator()(ClientKey const key) const noexcept {
      switch (key.first) {
        case KeyType::SYNCER_ID: {
          auto rv = key.second.syncerId.value;
          return std::hash<decltype(rv)>()(rv);
        }
        case KeyType::SERVER_ID: {
          auto rv = key.second.clientId;
          return std::hash<decltype(rv)>()(rv);
        }
        case KeyType::INVALID: {
          // Should never be added to the map
          TRI_ASSERT(false);
          return 0;
        }
      }
      TRI_ASSERT(false);
      return 0;
    };
  };
  class ClientEqual {
   public:
    inline bool operator()(ClientKey const& left, ClientKey const& right) const noexcept {
      if (left.first != right.first) {
        return false;
      }
      switch (left.first) {
        case KeyType::SYNCER_ID:
          return left.second.syncerId == right.second.syncerId;
        case KeyType::SERVER_ID:
          return left.second.clientId == right.second.clientId;
        case KeyType::INVALID:
          // Should never be added to the map
          TRI_ASSERT(false);
          return true;
      }
      TRI_ASSERT(false);
      return true;
    }
  };

  static inline ClientKey getKey(SyncerId const syncerId, ServerId const clientId) {
    // For backwards compatible APIs, we might not have a syncer ID;
    // fall back to the clientId in that case. SyncerId was introduced in 3.4.9 and 3.5.0.
    // The only public API using this, /_api/wal/tail, marked the serverId
    // parameter (corresponding to clientId here) as deprecated in 3.5.0.

    // Also, so these values cannot interfere with each other, prefix them to
    // make them disjoint.

    ClientKeyUnion keyUnion{};
    KeyType keyType = KeyType::INVALID;

    if (syncerId.value != 0) {
      keyUnion.syncerId = syncerId;
      keyType = KeyType::SYNCER_ID;
    } else if (clientId.isSet()) {
      keyUnion.clientId = clientId;
      keyType = KeyType::SERVER_ID;
    }

    return ClientKey(keyType, keyUnion);
  }

 private:
  mutable basics::ReadWriteLock _lock;

  /// @brief mapping from (SyncerId | ClientServerId) -> progress
  std::unordered_map<ClientKey, ReplicationClientProgress, ClientHash, ClientEqual> _clients;
};

}  // namespace arangodb

#endif
