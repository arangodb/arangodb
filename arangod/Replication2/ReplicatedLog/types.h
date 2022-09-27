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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <variant>
#include <optional>

#include "Basics/StaticStrings.h"
#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Inspection/Transformers.h"

namespace arangodb::futures {
template<typename T>
class Future;
}

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::replication2::replicated_log {

namespace static_strings {
inline constexpr std::string_view upToDateString = "up-to-date";
inline constexpr std::string_view errorBackoffString = "error-backoff";
inline constexpr std::string_view requestInFlightString = "request-in-flight";
}  // namespace static_strings

struct FollowerState {
  struct UpToDate {
    friend auto operator==(UpToDate const& left, UpToDate const& right) noexcept
        -> bool = default;
  };
  struct ErrorBackoff {
    std::chrono::duration<double, std::milli> durationMS;
    std::size_t retryCount;
    friend auto operator==(ErrorBackoff const& left,
                           ErrorBackoff const& right) noexcept
        -> bool = default;
  };
  struct RequestInFlight {
    std::chrono::duration<double, std::milli> durationMS;

    friend auto operator==(RequestInFlight const& left,
                           RequestInFlight const& right) noexcept
        -> bool = default;
  };

  std::variant<UpToDate, ErrorBackoff, RequestInFlight> value;

  static auto withUpToDate() noexcept -> FollowerState;
  static auto withErrorBackoff(std::chrono::duration<double, std::milli>,
                               std::size_t retryCount) noexcept
      -> FollowerState;
  static auto withRequestInFlight(
      std::chrono::duration<double, std::milli>) noexcept -> FollowerState;
  static auto fromVelocyPack(velocypack::Slice) -> FollowerState;
  void toVelocyPack(velocypack::Builder&) const;

  FollowerState() = default;

  friend auto operator==(FollowerState const& left,
                         FollowerState const& right) noexcept -> bool = default;

 private:
  template<typename... Args>
  explicit FollowerState(std::in_place_t, Args&&... args)
      : value(std::forward<Args>(args)...) {}
};

template<class Inspector>
auto inspect(Inspector& f, FollowerState::UpToDate& x) {
  auto state = std::string{static_strings::upToDateString};
  return f.object(x).fields(f.field("state", state));
}

template<class Inspector>
auto inspect(Inspector& f, FollowerState::ErrorBackoff& x) {
  auto state = std::string{static_strings::errorBackoffString};
  return f.object(x).fields(
      f.field("state", state),
      f.field("durationMS", x.durationMS)
          .transformWith(inspection::DurationTransformer<
                         std::chrono::duration<double, std::milli>>{}),
      f.field("retryCount", x.retryCount));
}

template<class Inspector>
auto inspect(Inspector& f, FollowerState::RequestInFlight& x) {
  auto state = std::string{static_strings::requestInFlightString};
  return f.object(x).fields(
      f.field("state", state),
      f.field("durationMS", x.durationMS)
          .transformWith(inspection::DurationTransformer<
                         std::chrono::duration<double, std::milli>>{}));
}

template<class Inspector>
auto inspect(Inspector& f, FollowerState& x) {
  if constexpr (Inspector::isLoading) {
    x = FollowerState::fromVelocyPack(f.slice());
  } else {
    x.toVelocyPack(f.builder());
  }
  return arangodb::inspection::Status::Success{};
}

auto to_string(FollowerState const&) -> std::string_view;

struct AppendEntriesRequest;
struct AppendEntriesResult;

#if (defined(__GNUC__) && !defined(__clang__))
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
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
    kPrevAppendEntriesInFlight,
  };

  ErrorType error = ErrorType::kNone;
  std::optional<std::string> details = std::nullopt;

  [[nodiscard]] auto getErrorMessage() const noexcept -> std::string_view;
  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice slice)
      -> AppendEntriesErrorReason;
  static auto errorTypeFromString(std::string_view str) -> ErrorType;

  friend auto operator==(AppendEntriesErrorReason const& left,
                         AppendEntriesErrorReason const& right) noexcept
      -> bool = default;
};
#if (defined(__GNUC__) && !defined(__clang__))
#pragma GCC diagnostic pop
#endif

[[nodiscard]] auto to_string(AppendEntriesErrorReason::ErrorType error) noexcept
    -> std::string_view;
struct AppendEntriesErrorReasonTypeStringTransformer {
  using SerializedType = std::string;
  auto toSerialized(AppendEntriesErrorReason::ErrorType source,
                    std::string& target) const -> inspection::Status;
  auto fromSerialized(std::string const& source,
                      AppendEntriesErrorReason::ErrorType& target) const
      -> inspection::Status;
};

template<class Inspector>
auto inspect(Inspector& f, AppendEntriesErrorReason& x) {
  auto state = std::string{static_strings::requestInFlightString};
  auto errorMessage = std::string{x.getErrorMessage()};
  return f.object(x).fields(
      f.field("details", x.details),
      f.field("errorMessage", errorMessage).fallback(std::string{}),
      f.field("error", x.error)
          .transformWith(AppendEntriesErrorReasonTypeStringTransformer{}));
}

struct LogStatistics {
  TermIndexPair spearHead{};
  LogIndex commitIndex{};
  LogIndex firstIndex{};
  LogIndex releaseIndex{};

  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice slice)
      -> LogStatistics;

  friend auto operator==(LogStatistics const& left,
                         LogStatistics const& right) noexcept -> bool = default;
};

template<class Inspector>
auto inspect(Inspector& f, LogStatistics& x) {
  using namespace arangodb;
  return f.object(x).fields(
      f.field(StaticStrings::Spearhead, x.spearHead),
      f.field(StaticStrings::CommitIndex, x.commitIndex),
      f.field(StaticStrings::FirstIndex, x.firstIndex),
      f.field(StaticStrings::ReleaseIndex, x.releaseIndex));
}

struct AbstractFollower {
  virtual ~AbstractFollower() = default;
  [[nodiscard]] virtual auto getParticipantId() const noexcept
      -> ParticipantId const& = 0;
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
