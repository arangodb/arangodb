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
#pragma once

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"

namespace arangodb {

template<typename T>
class ResultT;

namespace futures {
template<typename T>
class Future;
}

namespace network {
class ConnectionPool;
struct RequestOptions;
}  // namespace network

namespace replication2::replicated_state::document {

/**
 * An interface used to communicate with the leader remotely.
 */
struct IDocumentStateLeaderInterface {
  virtual ~IDocumentStateLeaderInterface() = default;
  virtual auto startSnapshot() -> futures::Future<ResultT<SnapshotConfig>> = 0;
  virtual auto nextSnapshotBatch(SnapshotId id)
      -> futures::Future<ResultT<SnapshotBatch>> = 0;
  virtual auto finishSnapshot(SnapshotId id) -> futures::Future<Result> = 0;
};

class DocumentStateLeaderInterface
    : public IDocumentStateLeaderInterface,
      public std::enable_shared_from_this<DocumentStateLeaderInterface> {
 public:
  explicit DocumentStateLeaderInterface(ParticipantId participantId,
                                        GlobalLogIdentifier gid,
                                        network::ConnectionPool* pool);

  auto startSnapshot() -> futures::Future<ResultT<SnapshotConfig>> override;
  auto nextSnapshotBatch(SnapshotId id)
      -> futures::Future<ResultT<SnapshotBatch>> override;
  auto finishSnapshot(SnapshotId id) -> futures::Future<Result> override;

 private:
  template<class T>
  auto postSnapshotRequest(std::string path, VPackBufferUInt8 payload,
                           network::RequestOptions const& opts)
      -> futures::Future<ResultT<T>>;

 private:
  ParticipantId _participantId;
  GlobalLogIdentifier _gid;
  network::ConnectionPool* _pool;
};

/**
 * Abstraction for network communication between participants.
 */
struct IDocumentStateNetworkHandler {
  virtual ~IDocumentStateNetworkHandler() = default;
  virtual auto getLeaderInterface(ParticipantId participantId) noexcept
      -> std::shared_ptr<IDocumentStateLeaderInterface> = 0;
};

class DocumentStateNetworkHandler : public IDocumentStateNetworkHandler {
 public:
  explicit DocumentStateNetworkHandler(GlobalLogIdentifier gid,
                                       network::ConnectionPool* pool);

  auto getLeaderInterface(ParticipantId participantId) noexcept
      -> std::shared_ptr<IDocumentStateLeaderInterface> override;

 private:
  GlobalLogIdentifier _gid;
  network::ConnectionPool* _pool;
};

}  // namespace replication2::replicated_state::document
}  // namespace arangodb
