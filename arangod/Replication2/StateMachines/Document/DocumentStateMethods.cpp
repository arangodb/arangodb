////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Replication2/StateMachines/Document/DocumentStateMethods.h"

#include <Basics/Exceptions.h>
#include <Basics/Exceptions.tpp>
#include "Cluster/ServerState.h"
#include "Futures/Future.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "VocBase/vocbase.h"

#include <memory>

namespace arangodb::replication2 {

class DocumentStateMethodsDBServer final : public DocumentStateMethods {
 public:
  explicit DocumentStateMethodsDBServer(TRI_vocbase_t& vocbase)
      : _vocbase(vocbase) {}

  [[nodiscard]] auto getSnapshot(LogId logId, LogIndex waitForIndex) const
      -> futures::Future<ResultT<velocypack::SharedSlice>> override {
    auto leaderResult = getDocumentStateLeaderById(logId);
    if (leaderResult.fail()) {
      return leaderResult.result();
    }

    // Dummy values
    VPackBuilder builder;
    {
      VPackArrayBuilder ab(&builder);
      {
        VPackObjectBuilder ob(&builder);
        builder.add("_key", "test1");
        builder.add("_id", "testCollection/test1");
        builder.add("_rev", "_e7EifO6---");
        builder.add("name", "testInsert1");
        builder.add("baz", 1);
      }
      {
        VPackObjectBuilder ob(&builder);
        builder.add("_key", "test2");
        builder.add("_id", "testCollection/test2");
        builder.add("_rev", "_e7EifO6--_");
        builder.add("name", "testInsert2");
        builder.add("baz", 2);
      }
    }
    return futures::Future<ResultT<velocypack::SharedSlice>>{
        builder.sharedSlice()};
  }

 private:
  [[nodiscard]] auto getDocumentStateLeaderById(LogId logId) const -> ResultT<
      std::shared_ptr<replicated_state::document::DocumentLeaderState>> {
    using DocumentStateType =
        std::shared_ptr<replicated_state::document::DocumentLeaderState>;

    auto stateMachine =
        std::dynamic_pointer_cast<replicated_state::ReplicatedState<
            replicated_state::document::DocumentState>>(
            _vocbase.getReplicatedStateById(logId));
    if (stateMachine == nullptr) {
      return ResultT<DocumentStateType>::error(
          TRI_ERROR_REPLICATION_REPLICATED_STATE_NOT_FOUND,
          fmt::format("DocumentState {} not found", logId));
    }

    auto leader = stateMachine->getLeader();
    if (leader == nullptr) {
      return ResultT<DocumentStateType>::error(
          TRI_ERROR_CLUSTER_NOT_LEADER,
          fmt::format("Failed to get leader of DocumentState with id {}",
                      logId));
    }
    return leader;
  }

 private:
  TRI_vocbase_t& _vocbase;
};

auto DocumentStateMethods::createInstance(TRI_vocbase_t& vocbase)
    -> std::shared_ptr<DocumentStateMethods> {
  switch (ServerState::instance()->getRole()) {
    case ServerState::ROLE_DBSERVER:
      return std::make_shared<DocumentStateMethodsDBServer>(vocbase);
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_NOT_IMPLEMENTED,
                                     "API available only on DB Servers");
  }
}

}  // namespace arangodb::replication2
