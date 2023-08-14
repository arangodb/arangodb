////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Replication2/ReplicatedLog/ILogInterfaces.h"

#include <gmock/gmock.h>

// TODO move this into a separate file
namespace testing {
namespace internal {

template<size_t k, typename Ptr>
struct MoveArgAction {
  Ptr pointer;

  template<typename... Args>
  void operator()(Args&&... args) const {
    *pointer = std::move(std::get<k>(std::tie(args...)));
  }
};
}  // namespace internal
// Action MoveArg<k>(pointer) saves the k-th (0-based) argument of the
// mock function to *pointer.
template<size_t k, typename Ptr>
internal::MoveArgAction<k, Ptr> MoveArg(Ptr pointer) {
  return {pointer};
}
}  // namespace testing

namespace arangodb::replication2::test {
struct ReplicatedStateHandleMock : replicated_log::IReplicatedStateHandle {
  MOCK_METHOD(std::unique_ptr<replicated_log::IReplicatedLogMethodsBase>,
              resignCurrentState, (), (noexcept, override));
  MOCK_METHOD(void, leadershipEstablished,
              (std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods>),
              (override));
  MOCK_METHOD(void, becomeFollower,
              (std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods>),
              (override));
  MOCK_METHOD(void, acquireSnapshot, (ServerID leader, LogIndex, std::uint64_t),
              (noexcept, override));
  MOCK_METHOD(replicated_state::Status, getInternalStatus, (),
              (const, override));
  MOCK_METHOD(void, updateCommitIndex, (LogIndex), (noexcept, override));

  void expectLeader() {
    EXPECT_CALL(*this, leadershipEstablished)
        .WillOnce(testing::MoveArg<0>(&logLeaderMethods));
    EXPECT_CALL(*this, resignCurrentState)
        // .WillOnce(testing::Return(std::move(logLeaderMethods)));
        .WillOnce([&]() {
          TRI_ASSERT(logLeaderMethods != nullptr);
          return std::move(logLeaderMethods);
        });

    EXPECT_CALL(*this, becomeFollower).Times(0);
  }
  void expectFollower() {
    EXPECT_CALL(*this, becomeFollower)
        .WillOnce(testing::MoveArg<0>(&logFollowerMethods));
    EXPECT_CALL(*this, resignCurrentState)
        .WillOnce(testing::Return(std::move(logFollowerMethods)));

    EXPECT_CALL(*this, leadershipEstablished).Times(0);
  }
  std::unique_ptr<replicated_log::IReplicatedLogLeaderMethods> logLeaderMethods;
  std::unique_ptr<replicated_log::IReplicatedLogFollowerMethods>
      logFollowerMethods;
};

}  // namespace arangodb::replication2::test
