////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "NetworkAttachedFollower.h"

#include <utility>

#include <Network/ConnectionPool.h>
#include <Network/Methods.h>

#include "Replication2/ReplicatedLog/NetworkMessages.h"
#include "Basics/StringUtils.h"
#include "Basics/Exceptions.h"
#include "Cluster/ServerState.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

NetworkAttachedFollower::NetworkAttachedFollower(network::ConnectionPool* pool,
                                                 ParticipantId id,
                                                 DatabaseID database,
                                                 LogId logId)
    : pool(pool),
      id(std::move(id)),
      database(std::move(database)),
      logId(logId) {}

auto NetworkAttachedFollower::getParticipantId() const noexcept
    -> ParticipantId const& {
  return id;
}

auto NetworkAttachedFollower::appendEntries(AppendEntriesRequest request)
    -> futures::Future<AppendEntriesResult> {
  VPackBufferUInt8 buffer;
  buffer.reserve(1024 * 1024); // TODO - use config params
  {
    VPackBuilder builder(buffer);
    request.toVelocyPack(builder);
  }

  auto path = basics::StringUtils::joinT("/", StaticStrings::ApiLogInternal,
                                         logId, "append-entries");
  network::RequestOptions opts;
  opts.database = database;
  auto f = network::sendRequest(pool, "server:" + id,
                                arangodb::fuerte::RestVerb::Post, path,
                                std::move(buffer), opts);

  return std::move(f).thenValue(
      [](network::Response result) -> AppendEntriesResult {
        if (result.fail() || !fuerte::statusIsSuccess(result.statusCode())) {
          THROW_ARANGO_EXCEPTION(result.combinedResult());
        }
        TRI_ASSERT(result.slice().get("error").isFalse());
        return AppendEntriesResult::fromVelocyPack(
            result.slice().get("result"));
      });
}

NetworkLeaderCommunicator::NetworkLeaderCommunicator(
    network::ConnectionPool* pool, ParticipantId leader, DatabaseID database,
    LogId logId)
    : pool(pool),
      leader(std::move(leader)),
      database(std::move(database)),
      logId(logId) {}

auto NetworkLeaderCommunicator::getParticipantId() const noexcept
    -> ParticipantId const& {
  return leader;
}

auto NetworkLeaderCommunicator::reportSnapshotAvailable(MessageId mid) noexcept
    -> futures::Future<Result> {
  auto path =
      basics::StringUtils::joinT("/", StaticStrings::ApiLogInternal, logId,
                                 StaticStrings::ApiUpdateSnapshotStatus);
  network::RequestOptions opts;
  opts.database = database;
  opts.parameters["follower"] = ServerState::instance()->getId();
  auto payload = velocypack::Buffer<uint8_t>();
  {
    auto const message = SnapshotAvailableReport{.messageId = mid};
    auto builder = velocypack::Builder(payload);
    velocypack::serialize(builder, message);
  }
  auto f = network::sendRequest(pool, "server:" + leader,
                                arangodb::fuerte::RestVerb::Post, path,
                                std::move(payload), opts);

  return std::move(f).thenValue(
      [](network::Response result) { return result.combinedResult(); });
}
