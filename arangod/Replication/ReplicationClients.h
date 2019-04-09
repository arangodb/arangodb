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

namespace arangodb {
namespace velocypack {
class Builder;
}

/// @brief struct representing how far a replication client
/// has come in terms of WAL tailing
struct ReplicationClientProgress {
  /// @brief timestamp of when client last contacted us
  double lastSeenStamp;
  /// @brief timestamp of when this entry will be considered expired
  double expireStamp;
  /// @brief last log tick/WAL tick that was served for this client
  uint64_t lastServedTick;

  ReplicationClientProgress(double lastSeenStamp, double expireStamp, uint64_t lastServedTick)
      : lastSeenStamp(lastSeenStamp),
        expireStamp(expireStamp),
        lastServedTick(lastServedTick) {}
};

/// @brief class to track progress of individual replication clients
/// for a particular database
class ReplicationClientsProgressTracker {
 public:
  ReplicationClientsProgressTracker() = default;
  ~ReplicationClientsProgressTracker() = default;

  ReplicationClientsProgressTracker(ReplicationClientsProgressTracker const&) = delete;
  ReplicationClientsProgressTracker& operator=(ReplicationClientsProgressTracker const&) = delete;

  /// @brief simply extend the lifetime of a specific client, so that its entry does not expire
  /// does not update the client's lastServedTick value
  void extend(std::string const& clientId, double ttl);
  
  /// @brief simply update the progress of a specific client, so that its entry does not expire
  /// this will update the client's lastServedTick value
  void track(std::string const& clientId, uint64_t lastServedTick, double ttl);

  /// @brief serialize the existing clients to a VelocyPack builder
  void toVelocyPack(velocypack::Builder& builder) const;

  /// @brief garbage-collect the existing list of clients
  /// thresholdStamp is the timestamp before all older entries will
  /// be collected
  void garbageCollect(double thresholdStamp);

  /// @brief return the lowest lastServedTick value for all clients
  /// returns UINT64_MAX in case no clients are registered
  uint64_t lowestServedValue() const;

 private:
  mutable basics::ReadWriteLock _lock;

  /// @brief mapping from client id -> progress
  std::unordered_map<std::string, ReplicationClientProgress> _clients; 
};

}  // namespace arangodb

#endif
