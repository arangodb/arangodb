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
#pragma once

#include "Replication2/ReplicatedLog/LogCommon.h"

#include "Basics/ResultT.h"
#include "Futures/Future.h"

namespace arangodb {
namespace network {
class ConnectionPool;
}
}  // namespace arangodb

namespace arangodb::replication2::replicated_state::document {

struct IDocumentStateLeaderInterface {
  virtual ~IDocumentStateLeaderInterface() = default;
  virtual auto getSnapshot(LogIndex waitForIndex)
      -> futures::Future<ResultT<velocypack::SharedSlice>> = 0;
};

class DocumentStateLeaderInterface : public IDocumentStateLeaderInterface {
 public:
  explicit DocumentStateLeaderInterface(ParticipantId participantId,
                                        GlobalLogIdentifier gid,
                                        network::ConnectionPool* pool);

  auto getSnapshot(LogIndex waitForIndex)
      -> futures::Future<ResultT<velocypack::SharedSlice>> override;

 private:
  ParticipantId _participantId;
  GlobalLogIdentifier _gid;
  network::ConnectionPool* _pool;
};

struct IDocumentStateNetworkHandler {
  virtual ~IDocumentStateNetworkHandler() = default;
  virtual auto getLeaderInterface(ParticipantId participantId)
      -> std::shared_ptr<IDocumentStateLeaderInterface> = 0;
};

class DocumentStateNetworkHandler : public IDocumentStateNetworkHandler {
 public:
  explicit DocumentStateNetworkHandler(GlobalLogIdentifier gid,
                                       network::ConnectionPool* pool);

  auto getLeaderInterface(ParticipantId participantId)
      -> std::shared_ptr<IDocumentStateLeaderInterface> override;

 private:
  GlobalLogIdentifier _gid;
  network::ConnectionPool* _pool;
};

}  // namespace arangodb::replication2::replicated_state::document
