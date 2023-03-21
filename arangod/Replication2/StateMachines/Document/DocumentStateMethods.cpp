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

#include "Replication2/StateMachines/Document/DocumentStateMethods.h"

#include "Cluster/ServerState.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/StateMachines/Document/DocumentStateMachine.h"
#include "Replication2/StateMachines/Document/DocumentStateSnapshot.h"
#include "VocBase/vocbase.h"

#include <Basics/Exceptions.h>
#include <Basics/Exceptions.tpp>

#include <memory>

namespace arangodb::replication2 {

class DocumentStateMethodsDBServer final : public DocumentStateMethods {
 public:
  explicit DocumentStateMethodsDBServer(TRI_vocbase_t& vocbase)
      : _vocbase(vocbase) {}

  [[nodiscard]] auto processSnapshotRequest(
      LogId logId, replicated_state::document::SnapshotParams params) const
      -> ResultT<velocypack::SharedSlice> override {
    using namespace replicated_state;

    auto leaderResult = getDocumentStateLeaderById(logId);
    if (leaderResult.fail()) {
      return leaderResult.result();
    }
    auto leader = leaderResult.get();

    return std::visit(
        overload{
            [&](document::SnapshotParams::Start& params) {
              return processResult(leader->snapshotStart(params));
            },
            [&](document::SnapshotParams::Next& params) {
              return processResult(leader->snapshotNext(params));
            },
            [&](document::SnapshotParams::Finish& params) {
              auto result = leader->snapshotFinish(params);
              if (result.fail()) {
                return ResultT<velocypack::SharedSlice>::error(result);
              }
              return ResultT<velocypack::SharedSlice>::success(
                  velocypack::SharedSlice{});
            },
            [&](document::SnapshotParams::Status& params) {
              if (params.id.has_value()) {
                return processResult(leader->snapshotStatus(params.id.value()));
              }
              return processResult(leader->allSnapshotsStatus());
            },
        },
        params.params);
  }

  auto getAssociatedShardList(LogId logId) const
      -> std::vector<ShardID> override {
    auto stateMachine =
        std::dynamic_pointer_cast<replicated_state::ReplicatedState<
            replicated_state::document::DocumentState>>(
            _vocbase.getReplicatedStateById(logId).get());
    if (stateMachine == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_REPLICATION_REPLICATED_STATE_NOT_FOUND,
          fmt::format("DocumentState {} not found", logId));
    }

    if (auto leader = stateMachine->getLeader(); leader != nullptr) {
      return leader->getAssociatedShardList();
    } else if (auto follower = stateMachine->getFollower();
               follower != nullptr) {
      return follower->getAssociatedShardList();
    } else {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_UNCONFIGURED,
          fmt::format("Failed to get DocumentState with id {}; this "
                      "is unconfigured.",
                      logId));
    }
  }

 private:
  [[nodiscard]] auto getDocumentStateLeaderById(LogId logId) const -> ResultT<
      std::shared_ptr<replicated_state::document::DocumentLeaderState>> {
    using DocumentStateType =
        std::shared_ptr<replicated_state::document::DocumentLeaderState>;

    auto documentState = _vocbase.getReplicatedStateById(logId);
    if (documentState.fail()) {
      return documentState.result();
    }

    auto stateMachine =
        std::dynamic_pointer_cast<replicated_state::ReplicatedState<
            replicated_state::document::DocumentState>>(documentState.get());
    if (stateMachine == nullptr) {
      return ResultT<DocumentStateType>::error(
          TRI_ERROR_REPLICATION_REPLICATED_STATE_NOT_FOUND,
          fmt::format("DocumentState {} not found", logId));
    }

    auto leader = stateMachine->getLeader();
    if (leader == nullptr) {
      return ResultT<DocumentStateType>::error(
          TRI_ERROR_REPLICATION_REPLICATED_LOG_NOT_THE_LEADER,
          fmt::format("Failed to get leader of DocumentState with id {}; this "
                      "is not a leader instance.",
                      logId));
    }
    return leader;
  }

  template<typename T>
  [[nodiscard]] auto processResult(ResultT<T> result) const
      -> ResultT<velocypack::SharedSlice> {
    if (result.fail()) {
      return ResultT<velocypack::SharedSlice>::error(result.result());
    }
    return ResultT<velocypack::SharedSlice>::success(
        velocypack::serialize(result.get()));
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
