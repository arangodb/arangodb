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

/// @brief simply extend the lifetime of a specific client, so that its entry does not expire
/// does not update the client's lastServedTick value
void ReplicationClientsProgressTracker::extend(std::string const& clientId, double ttl) {
  if (clientId.empty() || clientId == "none") {
    // we will not store any info for these client ids
    return;
  }

  if (ttl <= 0.0) {
    ttl = replutils::BatchInfo::DefaultTimeout;
  }

  double const timestamp = TRI_microtime();
  double const expires = timestamp + ttl;

  WRITE_LOCKER(writeLocker, _lock);

  auto it = _clients.find(clientId);

  if (it == _clients.end()) {
    LOG_TOPIC("a895c", TRACE, Logger::REPLICATION)
        << "replication client entry for client '" << clientId << "' not found";
    return;
  }

  LOG_TOPIC("f1c60", TRACE, Logger::REPLICATION)
      << "updating replication client entry for client '" << clientId
      << "' using TTL " << ttl;
  (*it).second.lastSeenStamp = timestamp;
  (*it).second.expireStamp = expires;
}
  
/// @brief simply update the progress of a specific client, so that its entry does not expire
/// this will update the client's lastServedTick value
void ReplicationClientsProgressTracker::track(std::string const& clientId, uint64_t lastServedTick, double ttl) {
  if (clientId.empty() || clientId == "none") {
    // we will not store any info for these client ids
    return;
  }

  if (ttl <= 0.0) {
    ttl = replutils::BatchInfo::DefaultTimeout;
  }
  double const timestamp = TRI_microtime();
  double const expires = timestamp + ttl;

  WRITE_LOCKER(writeLocker, _lock);

  auto it = _clients.find(clientId);

  if (it == _clients.end()) {
    // insert new client entry
    _clients.emplace(clientId, ReplicationClientProgress(timestamp, expires, lastServedTick));
    LOG_TOPIC("69c75", TRACE, Logger::REPLICATION)
        << "inserting replication client entry for client '" << clientId
        << "' using TTL " << ttl << ", last tick: " << lastServedTick;
    return;
  }

  // update an existing client entry
  (*it).second.lastSeenStamp = timestamp;
  (*it).second.expireStamp = expires;
  if (lastServedTick > 0) {
    (*it).second.lastServedTick = lastServedTick;
    LOG_TOPIC("47d4a", TRACE, Logger::REPLICATION)
        << "updating replication client entry for client '" << clientId
        << "' using TTL " << ttl << ", last tick: " << lastServedTick;
  } else {
    LOG_TOPIC("fce26", TRACE, Logger::REPLICATION)
        << "updating replication client entry for client '" << clientId
        << "' using TTL " << ttl;
  }
}

/// @brief serialize the existing clients to a VelocyPack builder
void ReplicationClientsProgressTracker::toVelocyPack(velocypack::Builder& builder) const {
  READ_LOCKER(readLocker, _lock);

  for (auto const& it : _clients) {
    builder.add(VPackValue(VPackValueType::Object));
    builder.add("serverId", VPackValue(it.first));

    char buffer[21];
    TRI_GetTimeStampReplication(it.second.lastSeenStamp, &buffer[0], sizeof(buffer));
    builder.add("time", VPackValue(buffer));

    TRI_GetTimeStampReplication(it.second.expireStamp, &buffer[0], sizeof(buffer));
    builder.add("expires", VPackValue(buffer));

    builder.add("lastServedTick", VPackValue(it.second.lastServedTick));
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
    if ((*it).second.expireStamp < thresholdStamp) {
      // found an entry that is already expired
      LOG_TOPIC("8d7db", DEBUG, Logger::REPLICATION)
          << "removing expired replication client entry for client '" << (*it).first << "'";
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
