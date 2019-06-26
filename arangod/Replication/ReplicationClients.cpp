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

#include "ReplicationClients.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "Logger/Logger.h"
#include "Replication/common-defines.h"
#include "Replication/utilities.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

/// @brief simply extend the lifetime of a specific client, so that its entry
/// does not expire does not update the client's lastServedTick value
void ReplicationClientsProgressTracker::extend(std::string const& clientId,
                                               std::string const& shardId, double ttl) {
  if (clientId.empty() || clientId == "none") {
    // we will not store any info for these client ids
    return;
  }

  if (ttl <= 0.0) {
    ttl = replutils::BatchInfo::DefaultTimeout;
  }

  double const timestamp = TRI_microtime();
  double const expires = timestamp + ttl;

  auto key = std::make_pair(clientId, shardId);

  WRITE_LOCKER(writeLocker, _lock);

  auto it = _clients.find(key);

  if (it == _clients.end()) {
    LOG_TOPIC("a895c", TRACE, Logger::REPLICATION)
        << "replication client entry for client '" << clientId
        << "' and shard '" << shardId << "' not found";
    return;
  }

  LOG_TOPIC("f1c60", TRACE, Logger::REPLICATION)
      << "updating replication client entry for client '" << clientId
      << "' and shard '" << shardId << "' using TTL " << ttl;
  (*it).second.lastSeenStamp = timestamp;
  (*it).second.expireStamp = expires;
}

/// @brief simply update the progress of a specific client, so that its entry
/// does not expire this will update the client's lastServedTick value
void ReplicationClientsProgressTracker::track(std::string const& clientId,
                                              std::string const& shardId,
                                              uint64_t lastServedTick, double ttl) {
  if (clientId.empty() || clientId == "none") {
    // we will not store any info for these client ids
    return;
  }

  if (ttl <= 0.0) {
    ttl = replutils::BatchInfo::DefaultTimeout;
  }
  double const timestamp = TRI_microtime();
  double const expires = timestamp + ttl;

  auto key = std::make_pair(clientId, shardId);

  WRITE_LOCKER(writeLocker, _lock);

  // TODO why find + emplace, and not just emplace and look if it succeeded?

  auto it = _clients.find(key);

  if (it == _clients.end()) {
    // insert new client entry
    _clients.emplace(key, ReplicationClientProgress(timestamp, expires, lastServedTick));
    LOG_TOPIC("69c75", TRACE, Logger::REPLICATION)
        << "inserting replication client entry for client '" << clientId << "' and shard '"
        << shardId << "' using TTL " << ttl << ", last tick: " << lastServedTick;
    return;
  }

  // update an existing client entry
  (*it).second.lastSeenStamp = timestamp;
  (*it).second.expireStamp = expires;
  if (lastServedTick > 0) {
    (*it).second.lastServedTick = lastServedTick;
    LOG_TOPIC("47d4a", TRACE, Logger::REPLICATION)
        << "updating replication client entry for client '" << clientId << "' and shard '"
        << shardId << "' using TTL " << ttl << ", last tick: " << lastServedTick;
  } else {
    LOG_TOPIC("fce26", TRACE, Logger::REPLICATION)
        << "updating replication client entry for client '" << clientId
        << "' and shard '" << shardId << "' using TTL " << ttl;
  }
}

/// @brief serialize the existing clients to a VelocyPack builder
void ReplicationClientsProgressTracker::toVelocyPack(velocypack::Builder& builder) const {
  TRI_ASSERT(builder.isOpenArray());
  READ_LOCKER(readLocker, _lock);

  for (auto const& it : _clients) {
    auto const& key = it.first;
    auto const& value = it.second;
    std::string const& clientId = key.first;
    std::string const& shardId = key.second;
    builder.add(VPackValue(VPackValueType::Object));
    // TODO what to do with this? add shardId? does it have to be backwards compatible?
    //  if so, would it suffice to use one combined string here (and must we use that as
    //  a key in the _clients map as well)?
    builder.add("serverId", VPackValue(clientId));
    builder.add("collection", VPackValue(shardId));

    char buffer[21];
    TRI_GetTimeStampReplication(value.lastSeenStamp, &buffer[0], sizeof(buffer));
    builder.add("time", VPackValue(buffer));

    TRI_GetTimeStampReplication(value.expireStamp, &buffer[0], sizeof(buffer));
    builder.add("expires", VPackValue(buffer));

    builder.add("lastServedTick", VPackValue(value.lastServedTick));
    builder.close();
  }
}

/// @brief garbage-collect the existing list of clients
/// thresholdStamp is the timestamp before all older entries will
/// be collected
void ReplicationClientsProgressTracker::garbageCollect(double thresholdStamp) {
  LOG_TOPIC("11a30", TRACE, Logger::REPLICATION)
      << "garbage collecting replication client entries";

  WRITE_LOCKER(writeLocker, _lock);

  auto it = _clients.begin();

  while (it != _clients.end()) {
    if (it->second.expireStamp < thresholdStamp) {
      auto const& key = it->first;
      // found an entry that is already expired
      LOG_TOPIC("8d7db", DEBUG, Logger::REPLICATION)
          << "removing expired replication client entry for client '"
          << key.first << "' and shard '" << key.second << "'";
      it = _clients.erase(it);
    } else {
      ++it;
    }
  }
}

/// @brief return the lowest lastServedTick value for all clients
/// returns UINT64_MAX in case no clients are registered
uint64_t ReplicationClientsProgressTracker::lowestServedValue() const {
  uint64_t value = UINT64_MAX;
  READ_LOCKER(readLocker, _lock);
  for (auto const& it : _clients) {
    value = std::min(value, it.second.lastServedTick);
  }
  return value;
}

void ReplicationClientsProgressTracker::untrack(std::string const& clientId,
                                                std::string const& shardId) {
  LOG_TOPIC("69c75", TRACE, Logger::REPLICATION)
      << "removing replication client entry for client '" << clientId
      << "' and shard '" << shardId << "'";
  _clients.erase({clientId, shardId});
}

ReplicationClientsProgressTracker::~ReplicationClientsProgressTracker() {
  if (!_clients.empty()) {
    VPackBuilder builder;
    builder.openArray();
    toVelocyPack(builder);
    builder.close();
    LOG_TOPIC("69c75", TRACE, Logger::REPLICATION)
        << "remaining replication client entries when progress tracker is "
           "removed: "
        << builder.slice().toJson();
  }
}
