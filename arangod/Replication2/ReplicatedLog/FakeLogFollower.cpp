////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright $YEAR-2021 ArangoDB GmbH, Cologne, Germany
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

#include "FakeLogFollower.h"

#include <Futures/Future.h>
#include <Network/ConnectionPool.h>
#include <Network/Methods.h>

#include "Replication2/ReplicatedLog/Common.h"
#include "Replication2/ReplicatedLog/types.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

FakeLogFollower::FakeLogFollower(network::ConnectionPool* pool, ParticipantId id,
                                 std::string database, LogId logId)
    : pool(pool), id(std::move(id)), database(database), logId(logId) {}

auto FakeLogFollower::getParticipantId() const noexcept -> ParticipantId const& {
  return id;
}

auto FakeLogFollower::appendEntries(AppendEntriesRequest request)
    -> futures::Future<AppendEntriesResult> {
  VPackBufferUInt8 buffer;
  {
    VPackBuilder builder(buffer);
    request.toVelocyPack(builder);
  }

  auto path =
      "_api/log/" + std::__cxx11::to_string(logId.id()) + "/appendEntries";

  network::RequestOptions opts;
  opts.database = database;
  LOG_DEVEL << "sending append entries to " << id << " with payload "
            << VPackSlice(buffer.data()).toJson();
  auto f = network::sendRequest(pool, "server:" + id, arangodb::fuerte::RestVerb::Post,
                                path, std::move(buffer), opts);

  return std::move(f).thenValue([this](network::Response result) -> AppendEntriesResult {
    LOG_DEVEL << "Append entries for " << id
              << " returned, fuerte ok = " << result.ok();
    if (result.fail()) {
      // TODO use better exception calss
      throw std::runtime_error("network error");
    }
    LOG_DEVEL << "Result for " << id << " is " << result.slice().toJson();
    TRI_ASSERT(result.slice().get("error").isFalse());  // TODO
    return AppendEntriesResult::fromVelocyPack(result.slice().get("result"));
  });
}
