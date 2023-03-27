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
#include "Cluster/ServerState.h"
#include "Logger/Logger.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"

namespace arangodb::replication2::replicated_state::document {

DocumentStateLeaderInterface::DocumentStateLeaderInterface(
    ParticipantId participantId, GlobalLogIdentifier gid,
    network::ConnectionPool* pool)
    : _participantId(std::move(participantId)),
      _gid(std::move(gid)),
      _pool(pool) {}

auto DocumentStateLeaderInterface::startSnapshot()
    -> futures::Future<ResultT<SnapshotConfig>> {
  VPackBuilder builder;
  auto params =
      SnapshotParams::Start{.serverId = ServerState::instance()->getId(),
                            .rebootId = ServerState::instance()->getRebootId()};
  velocypack::serialize(builder, params);

  auto path =
      basics::StringUtils::joinT("/", StaticStrings::ApiDocumentStateExternal,
                                 _gid.id, "snapshot", "start");
  network::RequestOptions opts;
  opts.database = _gid.database;
  return postSnapshotRequest<SnapshotConfig>(std::move(path),
                                             std::move(*builder.steal()), opts);
}

auto DocumentStateLeaderInterface::nextSnapshotBatch(SnapshotId id)
    -> futures::Future<ResultT<SnapshotBatch>> {
  auto path =
      basics::StringUtils::joinT("/", StaticStrings::ApiDocumentStateExternal,
                                 _gid.id, "snapshot", "next", to_string(id));
  network::RequestOptions opts;
  opts.database = _gid.database;
  return postSnapshotRequest<SnapshotBatch>(std::move(path), {}, opts);
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
      .thenValue([self = shared_from_this(), id = id](
                     network::Response&& resp) -> futures::Future<Result> {
        // Only retry on network error
        if (resp.fail()) {
          LOG_TOPIC("2e771", ERR, Logger::REPLICATION2)
              << "Failed to finish snapshot " << id << " on "
              << self->_participantId << ": " << resp.combinedResult()
              << " - retrying in 5 seconds";
          std::this_thread::sleep_for(std::chrono::seconds(5));
          return self->finishSnapshot(id);
        } else if (!fuerte::statusIsSuccess(resp.statusCode())) {
          return resp.combinedResult();
        }
        return Result{};
      });
}

template<class T>
auto DocumentStateLeaderInterface::postSnapshotRequest(
    std::string path, VPackBufferUInt8 payload,
    network::RequestOptions const& opts) -> futures::Future<ResultT<T>> {
  return network::sendRequest(_pool, "server:" + _participantId,
                              fuerte::RestVerb::Post, std::move(path),
                              std::move(payload), opts)
      .thenValue([](network::Response&& resp) -> ResultT<T> {
        if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
          return resp.combinedResult();
        }
        auto slice = resp.slice();
        return velocypack::deserialize<T>(slice.get("result"));
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
