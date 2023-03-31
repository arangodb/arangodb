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
/// @author Alexandru Petenchea
////////////////////////////////////////////////////////////////////////////////

#include "Replication2/StateMachines/Document/DocumentStateNetworkHandler.h"

#include "Basics/Exceptions.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringUtils.h"
#include "Network/Methods.h"

namespace arangodb::replication2::replicated_state::document {

DocumentStateLeaderInterface::DocumentStateLeaderInterface(
    ParticipantId participantId, GlobalLogIdentifier gid,
    network::ConnectionPool* pool)
    : _participantId(std::move(participantId)),
      _gid(std::move(gid)),
      _pool(pool) {}

auto DocumentStateLeaderInterface::startSnapshot(LogIndex waitForIndex)
    -> futures::Future<ResultT<SnapshotBatch>> {
  auto path =
      basics::StringUtils::joinT("/", StaticStrings::ApiDocumentStateExternal,
                                 _gid.id, "snapshot", "start");
  network::RequestOptions opts;
  opts.database = _gid.database;
  opts.param("waitForIndex", std::to_string(waitForIndex.value));
  return postSnapshotRequest(std::move(path), opts);
}

auto DocumentStateLeaderInterface::nextSnapshotBatch(SnapshotId id)
    -> futures::Future<ResultT<SnapshotBatch>> {
  auto path =
      basics::StringUtils::joinT("/", StaticStrings::ApiDocumentStateExternal,
                                 _gid.id, "snapshot", "next", to_string(id));
  network::RequestOptions opts;
  opts.database = _gid.database;
  return postSnapshotRequest(std::move(path), opts);
}

auto DocumentStateLeaderInterface::finishSnapshot(SnapshotId id)
    -> futures::Future<Result> {
  auto path =
      basics::StringUtils::joinT("/", StaticStrings::ApiDocumentStateExternal,
                                 _gid.id, "snapshot", "finish", to_string(id));
  network::RequestOptions opts;
  opts.database = _gid.database;
  return network::sendRequest(_pool, "server:" + _participantId,
                              fuerte::RestVerb::Delete, std::move(path), {},
                              opts)
      .thenValue([](network::Response&& resp) -> Result {
        if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
          return resp.combinedResult();
        }
        return Result{};
      });
}

auto DocumentStateLeaderInterface::postSnapshotRequest(
    std::string path, network::RequestOptions const& opts)
    -> futures::Future<ResultT<SnapshotBatch>> {
  return network::sendRequest(_pool, "server:" + _participantId,
                              fuerte::RestVerb::Post, std::move(path), {}, opts)
      .thenValue([](network::Response&& resp) -> ResultT<SnapshotBatch> {
        if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
          return resp.combinedResult();
        }
        auto slice = resp.slice();
        return velocypack::deserialize<SnapshotBatch>(slice.get("result"));
      });
}

DocumentStateNetworkHandler::DocumentStateNetworkHandler(
    GlobalLogIdentifier gid, network::ConnectionPool* pool)
    : _gid(std::move(gid)), _pool(pool) {}

auto DocumentStateNetworkHandler::getLeaderInterface(
    ParticipantId participantId) noexcept
    -> std::shared_ptr<IDocumentStateLeaderInterface> {
  return std::make_shared<DocumentStateLeaderInterface>(participantId, _gid,
                                                        _pool);
}

}  // namespace arangodb::replication2::replicated_state::document
