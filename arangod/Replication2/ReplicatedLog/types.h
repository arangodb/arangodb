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

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <variant>
#include <optional>

#include "Replication2/ReplicatedLog/LogCommon.h"

namespace arangodb::futures {
template <typename T>
class Future;
}

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::replication2::replicated_log {

struct FollowerState {

  struct UpToDate {};
  struct ErrorBackoff {
    std::chrono::duration<double, std::milli> durationMS;
    std::size_t retryCount;
  };
  struct RequestInFlight {
    std::chrono::duration<double, std::milli> durationMS;
  };

  std::variant<UpToDate, ErrorBackoff, RequestInFlight> value;

  static auto withUpToDate() noexcept -> FollowerState;
  static auto withErrorBackoff(std::chrono::duration<double, std::milli>,
                               std::size_t retryCount) noexcept -> FollowerState;
  static auto withRequestInFlight(std::chrono::duration<double, std::milli>) noexcept
      -> FollowerState;
  static auto fromVelocyPack(velocypack::Slice) -> FollowerState;
  void toVelocyPack(velocypack::Builder&) const;

  FollowerState() = default;
 private:
  template<typename... Args>
  explicit FollowerState(std::in_place_t, Args&&... args) : value(std::forward<Args>(args)...) {}
};

auto to_string(FollowerState const&) -> std::string_view;

struct AppendEntriesRequest;
struct AppendEntriesResult;

struct AppendEntriesErrorReason {
  enum class ErrorType {
    kNone,
    kInvalidLeaderId,
    kLostLogCore,
    kMessageOutdated,
    kWrongTerm,
    kNoPrevLogMatch,
    kPersistenceFailure,
    kCommunicationError,
  };

  ErrorType error = ErrorType::kNone;
  std::optional<std::string> details = std::nullopt;

  [[nodiscard]] auto getDetails() const noexcept -> std::string_view;
  [[nodiscard]] auto getErrorMessage() const noexcept -> std::string_view;
  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice slice) -> AppendEntriesErrorReason;
  static auto errorTypeFromString(std::string_view str) -> ErrorType;

  friend auto operator==(AppendEntriesErrorReason const& left, AppendEntriesErrorReason const& right) noexcept -> bool = default;
};

[[nodiscard]] auto to_string(AppendEntriesErrorReason const& reason) noexcept -> std::string_view;

struct LogStatistics {
  TermIndexPair spearHead{};
  LogIndex commitIndex{};
  LogIndex firstIndex{};

  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice slice) -> LogStatistics;

  friend auto operator==(LogStatistics const& left, LogStatistics const& right) noexcept -> bool;
  friend auto operator!=(LogStatistics const& left, LogStatistics const& right) noexcept -> bool;
};

[[nodiscard]] auto operator==(LogStatistics const& left, LogStatistics const& right) noexcept -> bool;
[[nodiscard]] auto operator!=(LogStatistics const& left, LogStatistics const& right) noexcept -> bool;

struct AbstractFollower {
  virtual ~AbstractFollower() = default;
  [[nodiscard]] virtual auto getParticipantId() const noexcept -> ParticipantId const& = 0;
  [[nodiscard]] virtual auto appendEntries(AppendEntriesRequest)
      -> futures::Future<AppendEntriesResult> = 0;
};

struct QuorumData {
  QuorumData(LogIndex index, LogTerm term, std::vector<ParticipantId> quorum);
  QuorumData(LogIndex index, LogTerm term);
  explicit QuorumData(velocypack::Slice slice);

  LogIndex index;
  LogTerm term;
  std::vector<ParticipantId> quorum;  // might be empty on follower

  void toVelocyPack(velocypack::Builder& builder) const;
};

}  // namespace arangodb::replication2::replicated_log
