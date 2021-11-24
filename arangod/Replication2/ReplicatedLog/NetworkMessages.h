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


#include <Basics/ErrorCode.h>
#include <Containers/ImmerMemoryPolicy.h>

#include <string>
#include <ostream>

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

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/types.h"

namespace arangodb {
class Result;
}

namespace arangodb::replication2::replicated_log {

struct MessageId {
  constexpr MessageId() noexcept : value{0} {}
  constexpr explicit MessageId(std::uint64_t value) noexcept : value{value} {}

  friend auto operator<=>(MessageId, MessageId) noexcept = default;
  friend auto operator++(MessageId& id) -> MessageId&;
  friend auto operator<<(std::ostream& os, MessageId id) -> std::ostream&;

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
 private:
  std::uint64_t value;
};

auto operator++(MessageId& id) -> MessageId&;
auto operator<<(std::ostream& os, MessageId id) -> std::ostream&;

struct AppendEntriesResult {
  LogTerm const logTerm;
  ErrorCode const errorCode;
  AppendEntriesErrorReason const reason;
  MessageId messageId;

  std::optional<TermIndexPair> conflict;

  [[nodiscard]] auto isSuccess() const noexcept -> bool;

  AppendEntriesResult(LogTerm, MessageId, TermIndexPair conflict) noexcept;
  AppendEntriesResult(LogTerm, MessageId) noexcept;
  AppendEntriesResult(LogTerm logTerm, ErrorCode errorCode,
                      AppendEntriesErrorReason reason, MessageId) noexcept;
  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> AppendEntriesResult;

  static auto withConflict(LogTerm, MessageId, TermIndexPair conflict) noexcept -> AppendEntriesResult;
  static auto withRejection(LogTerm, MessageId, AppendEntriesErrorReason) noexcept -> AppendEntriesResult;
  static auto withPersistenceError(LogTerm, MessageId, Result const&) noexcept -> AppendEntriesResult;
  static auto withOk(LogTerm, MessageId) noexcept -> AppendEntriesResult;
};

struct AppendEntriesRequest {
  using EntryContainer =
      ::immer::flex_vector<InMemoryLogEntry, arangodb::immer::arango_memory_policy>;

  LogTerm leaderTerm;
  ParticipantId leaderId;
  TermIndexPair prevLogEntry;
  LogIndex leaderCommit;
  LogIndex largestCommonIndex;
  MessageId messageId;
  EntryContainer entries{};
  bool waitForSync = false;

  AppendEntriesRequest() = default;
  AppendEntriesRequest(LogTerm leaderTerm, ParticipantId leaderId,
                       TermIndexPair prevLogEntry, LogIndex leaderCommit,
                       LogIndex largestCommonIndex, MessageId messageId,
                       bool waitForSync, EntryContainer entries);
  ~AppendEntriesRequest() noexcept = default;

  AppendEntriesRequest(AppendEntriesRequest&& other) noexcept;
  AppendEntriesRequest(AppendEntriesRequest const& other) = default;
  auto operator=(AppendEntriesRequest&& other) noexcept -> AppendEntriesRequest&;
  auto operator=(AppendEntriesRequest const& other) -> AppendEntriesRequest& = default;

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> AppendEntriesRequest;
};

}  // namespace arangodb::replication2::replicated_log

namespace arangodb {
template <>
struct velocypack::Extractor<replication2::replicated_log::MessageId> {
  static auto extract(velocypack::Slice slice) -> replication2::replicated_log::MessageId {
    return replication2::replicated_log::MessageId{slice.getNumericValue<uint64_t>()};
  }
};
}
