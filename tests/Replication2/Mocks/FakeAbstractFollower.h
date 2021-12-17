////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/voc-errors.h"

#include "Replication2/ReplicatedLog/ILogInterfaces.h"
#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/types.h"

namespace arangodb::replication2::test {

/**
 * This class queue all append entries requests and allows to expect them.
 * You can then define a custom response to those append entries requests.
 *
 * It only models an AbstractFollower. If you want to have full control,
 * consider using the FakeFollower.
 */
struct FakeAbstractFollower : AbstractFollower {
  explicit FakeAbstractFollower(ParticipantId id) : participantId(std::move(id)) {}

  [[nodiscard]] auto getParticipantId() const noexcept -> ParticipantId const& override {
    return participantId;
  };

  auto appendEntries(AppendEntriesRequest request)
      -> futures::Future<AppendEntriesResult> override {
    return requests.emplace_back(std::move(request)).promise.getFuture();
  }

  void resolveRequest(AppendEntriesResult result) {
    requests.front().promise.setValue(std::move(result));
    requests.pop_front();
  }

  void resolveWithOk() {
    resolveRequest(AppendEntriesResult{LogTerm{4}, TRI_ERROR_NO_ERROR,
                                       {}, currentRequest().messageId});
  }

  template <typename E>
  void resolveRequestWithException(E&& e) {
    requests.front().promise.setException(std::forward<E>(e));
    requests.pop_front();
  }

  [[nodiscard]] auto currentRequest() const -> AppendEntriesRequest const& {
    return requests.front().request;
  }

  [[nodiscard]] auto hasPendingRequests() const -> bool { return !requests.empty(); }

  void handleAllRequestsWithOk() {
    while (hasPendingRequests()) {
      resolveWithOk();
    }
  }

  struct AsyncRequest {
    explicit AsyncRequest(AppendEntriesRequest request)
        : request(std::move(request)) {}
    AppendEntriesRequest request;
    futures::Promise<AppendEntriesResult> promise;
  };

  std::deque<AsyncRequest> requests;
  ParticipantId participantId;
};
}  // namespace arangodb::replication2::test
