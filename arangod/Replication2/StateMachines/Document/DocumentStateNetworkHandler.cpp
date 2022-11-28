////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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

auto DocumentStateLeaderInterface::getSnapshot(LogIndex waitForIndex)
    -> futures::Future<ResultT<Snapshot>> {
  auto path = basics::StringUtils::joinT(
      "/", StaticStrings::ApiDocumentStateExternal, _gid.id, "snapshot");
  network::RequestOptions opts;
  opts.database = _gid.database;
  opts.param("waitForIndex", std::to_string(waitForIndex.value));

  return network::sendRequest(_pool, "server:" + _participantId,
                              fuerte::RestVerb::Get, path, {}, opts)
      .thenValue([](network::Response&& resp) -> ResultT<Snapshot> {
        if (resp.fail() || !fuerte::statusIsSuccess(resp.statusCode())) {
          return resp.combinedResult();
        } else {
          auto slice = resp.slice();
          return velocypack::deserialize<Snapshot>(slice.get("result"));
        }
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
