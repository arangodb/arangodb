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
#include "Basics/tryEmplaceHelper.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Replication/common-defines.h"
#include "Replication/utilities.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

namespace arangodb {
// Helper for logging

struct SyncerInfo {
  explicit SyncerInfo(ReplicationClientProgress const& progress)
      : syncerId(progress.syncerId),
        clientId(progress.clientId),
        clientInfo(progress.clientInfo) {}

  SyncerInfo(SyncerId syncerId, ServerId clientId, std::string const& clientInfo)
      : syncerId(syncerId), clientId(clientId), clientInfo(clientInfo) {}

  SyncerId const syncerId;
  ServerId const clientId;
  std::string const clientInfo;
};

std::ostream& operator<<(std::ostream& ostream, SyncerInfo const& info) {
  ostream << "syncer " << info.syncerId.toString() << " from client "
          << info.clientId.id();
  if (!info.clientInfo.empty()) {
    ostream << " (" << info.clientInfo << ")";
  }
  return ostream;
}

}  // namespace arangodb

/// @brief simply extend the lifetime of a specific client, so that its entry
/// does not expire but does not update the client's lastServedTick value
void ReplicationClientsProgressTracker::extend(SyncerId syncerId, ServerId clientId,
                                               std::string const& clientInfo, double ttl) {
  auto const key = getKey(syncerId, clientId);
  if (key.first == KeyType::INVALID) {
    // we will not store any info for these client ids
    return;
  }

  if (ttl <= 0.0) {
    ttl = replutils::BatchInfo::DefaultTimeout;
  }

  double const timestamp = []() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
  }();
  double const expires = timestamp + ttl;

  WRITE_LOCKER(writeLocker, _lock);

  auto it = _clients.find(key);

  if (it == _clients.end()) {
    LOG_TOPIC("a895c", TRACE, Logger::REPLICATION)
        << "replication client entry for "
        << SyncerInfo{syncerId, clientId, clientInfo} << " not found";
    return;
  }

  LOG_TOPIC("f1c60", TRACE, Logger::REPLICATION)
      << "updating replication client entry for "
      << SyncerInfo{syncerId, clientId, clientInfo} << " using TTL " << ttl;
  (*it).second.lastSeenStamp = timestamp;
  (*it).second.expireStamp = expires;
}

/// @brief simply update the progress of a specific client, so that its entry
/// does not expire this will update the client's lastServedTick value
void ReplicationClientsProgressTracker::track(SyncerId syncerId, ServerId clientId,
                                              std::string const& clientInfo,
                                              TRI_voc_tick_t lastServedTick, double ttl) {
  auto const key = getKey(syncerId, clientId);
  if (key.first == KeyType::INVALID) {
    // we will not store any info for these client ids
    return;
  }

  if (ttl <= 0.0) {
    ttl = replutils::BatchInfo::DefaultTimeout;
  }
  double const timestamp = []() {
    using namespace std::chrono;
    return duration<double>(steady_clock::now().time_since_epoch()).count();
  }();
  double const expires = timestamp + ttl;

  WRITE_LOCKER(writeLocker, _lock);

  // insert new client entry
  auto const [it, inserted] = _clients.try_emplace(
    key,
    arangodb::lazyConstruct([&]{
      return ReplicationClientProgress(timestamp, expires, lastServedTick,
                                                  syncerId, clientId, clientInfo);
    })
  );
  auto const syncer = syncerId.toString();

  if (inserted) {
    LOG_TOPIC("69c75", TRACE, Logger::REPLICATION)
        << "inserting replication client entry for " << SyncerInfo{it->second}
        << " using TTL " << ttl << ", last tick: " << lastServedTick;
    return;
  }

  // update an existing client entry
  it->second.lastSeenStamp = timestamp;
  it->second.expireStamp = expires;
  if (lastServedTick > 0) {
    it->second.lastServedTick = lastServedTick;
    LOG_TOPIC("47d4a", TRACE, Logger::REPLICATION)
          << "updating replication client entry for " << SyncerInfo{it->second}
        << " using TTL " << ttl << ", last tick: " << lastServedTick;
  } else {
    LOG_TOPIC("fce26", TRACE, Logger::REPLICATION)
          << "updating replication client entry for " << SyncerInfo{it->second}
        << " using TTL " << ttl;
  }
}

/// @brief serialize the existing clients to a VelocyPack builder
void ReplicationClientsProgressTracker::toVelocyPack(velocypack::Builder& builder) const {
  TRI_ASSERT(builder.isOpenArray());
  READ_LOCKER(readLocker, _lock);

  for (auto const& it : _clients) {
    auto const& progress = it.second;
    builder.add(VPackValue(VPackValueType::Object));
    builder.add("syncerId", VPackValue(progress.syncerId.toString()));
    builder.add("serverId", VPackValue(std::to_string(progress.clientId.id())));
    builder.add("clientInfo", VPackValue(progress.clientInfo));

    char buffer[21];
    // lastSeenStamp and expireStamp use the steady_clock. Convert them to
    // system_clock before serialization.
    double const lastSeenStamp =
        ReplicationClientProgress::steadyClockToSystemClock(progress.lastSeenStamp);
    double const expireStamp =
        ReplicationClientProgress::steadyClockToSystemClock(progress.expireStamp);
    TRI_GetTimeStampReplication(lastSeenStamp, &buffer[0], sizeof(buffer));
    builder.add("time", VPackValue(buffer));

    TRI_GetTimeStampReplication(expireStamp, &buffer[0], sizeof(buffer));
    builder.add("expires", VPackValue(buffer));

    builder.add("lastServedTick", VPackValue(std::to_string(progress.lastServedTick)));
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
      auto const& progress = it->second;
      // found an entry that is already expired
      LOG_TOPIC("8d7db", DEBUG, Logger::REPLICATION)
          << "removing expired replication client entry for " << SyncerInfo{progress};
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

void ReplicationClientsProgressTracker::untrack(SyncerId const syncerId, ServerId const clientId,
                                                std::string const& clientInfo) {
  auto const key = getKey(syncerId, clientId);
  if (key.first == KeyType::INVALID) {
    // Don't hash an invalid key
    return;
  }
  LOG_TOPIC("c26ab", TRACE, Logger::REPLICATION)
      << "removing replication client entry for "
      << SyncerInfo{syncerId, clientId, clientInfo};

  WRITE_LOCKER(writeLocker, _lock);
  _clients.erase(key);
}

double ReplicationClientProgress::steadyClockToSystemClock(double steadyTimestamp) {
  using namespace std::chrono;

  auto steadyTimePoint =
      time_point<steady_clock, duration<double>>(duration<double>(steadyTimestamp));
  auto systemTimePoint =
      system_clock::now() +
      duration_cast<system_clock::duration>(steadyTimePoint - steady_clock::now());

  return duration<double>(systemTimePoint.time_since_epoch()).count();
}

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
ReplicationClientsProgressTracker::~ReplicationClientsProgressTracker() {
  if (!_clients.empty() && Logger::isEnabled(LogLevel::TRACE, Logger::REPLICATION)) {
    VPackBuilder builder;
    builder.openArray();
    toVelocyPack(builder);
    builder.close();
    LOG_TOPIC("953e1", TRACE, Logger::REPLICATION)
        << "remaining replication client entries when progress tracker is "
           "removed: "
        << builder.slice().toJson();
  }
}
#endif
