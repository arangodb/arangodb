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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Replication2/ReplicatedLog/Common.h"
#include "Replication2/ReplicatedLog/types.h"

#include <Basics/ErrorCode.h>
#include <Basics/voc-errors.h>

#include <string>
#include <utility>

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/flex_vector.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif


namespace arangodb::replication2::replicated_log {

struct MessageId : implement_compare<MessageId> {
  constexpr MessageId() noexcept : value{0} {}
  constexpr explicit MessageId(std::uint64_t value) noexcept : value{value} {}
  std::uint64_t value;
  [[nodiscard]] auto operator<=(MessageId) const -> bool;

  friend auto operator++(MessageId& id) -> MessageId& {
    ++id.value;
    return id;
  }

  friend auto operator<<(std::ostream& os, MessageId id) -> std::ostream& {
    return os << id.value;
  }
};


struct AppendEntriesResult {
  LogTerm const logTerm = LogTerm{};
  ErrorCode const errorCode = TRI_ERROR_NO_ERROR;
  AppendEntriesErrorReason const reason = AppendEntriesErrorReason::NONE;
  MessageId messageId;

  [[nodiscard]] auto isSuccess() const noexcept -> bool {
    return errorCode == TRI_ERROR_NO_ERROR;
  }

  explicit AppendEntriesResult(LogTerm, MessageId);
  AppendEntriesResult(LogTerm logTerm, ErrorCode errorCode,
                      AppendEntriesErrorReason reason, MessageId);
  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> AppendEntriesResult;
};

auto to_string(AppendEntriesErrorReason reason) -> std::string;

struct AppendEntriesRequest {
  AppendEntriesRequest() = default;
  AppendEntriesRequest(LogTerm leaderTerm, ParticipantId leaderId, LogTerm prevLogTerm,
                       LogIndex prevLogIndex, LogIndex leaderCommit, MessageId messageId,
                       bool waitForSync, immer::flex_vector<LogEntry> entries)
      : leaderTerm(leaderTerm),
        leaderId(std::move(leaderId)),
        prevLogTerm(prevLogTerm),
        prevLogIndex(prevLogIndex),
        leaderCommit(leaderCommit),
        messageId(messageId),
        waitForSync(waitForSync),
        entries(std::move(entries)) {}

  AppendEntriesRequest(AppendEntriesRequest&& other) noexcept;
  AppendEntriesRequest(AppendEntriesRequest const& other) = default;
  auto operator=(AppendEntriesRequest&& other) noexcept -> AppendEntriesRequest&;
  auto operator=(AppendEntriesRequest const& other) -> AppendEntriesRequest& = default;
  ~AppendEntriesRequest() noexcept = default;

  // TODO reorder members for a more efficient layout
  LogTerm leaderTerm;
  ParticipantId leaderId;
  // TODO assert index == 0 <=> term == 0
  LogTerm prevLogTerm;
  LogIndex prevLogIndex;
  LogIndex leaderCommit;
  MessageId messageId;
  bool waitForSync;
  immer::flex_vector<LogEntry> entries{};

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> AppendEntriesRequest;
};

}  // namespace arangodb::replication2::replicated_log
