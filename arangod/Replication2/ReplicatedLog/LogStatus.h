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

#include <optional>
#include <unordered_map>
#include <variant>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Replication2/ReplicatedLog/types.h"

namespace arangodb::replication2::replicated_log {

struct FollowerStatistics : LogStatistics {
  AppendEntriesErrorReason lastErrorReason;
  std::chrono::duration<double, std::milli> lastRequestLatencyMS;
  FollowerState internalState;
  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> FollowerStatistics;

  friend auto operator==(FollowerStatistics const& left, FollowerStatistics const& right) noexcept -> bool;
  friend auto operator!=(FollowerStatistics const& left, FollowerStatistics const& right) noexcept -> bool;
};

[[nodiscard]] auto operator==(FollowerStatistics const& left, FollowerStatistics const& right) noexcept -> bool;
[[nodiscard]] auto operator!=(FollowerStatistics const& left, FollowerStatistics const& right) noexcept -> bool;

struct LeaderStatus {
  LogStatistics local;
  LogTerm term;
  LogIndex largestCommonIndex;
  bool leadershipEstablished{false};
  std::unordered_map<ParticipantId, FollowerStatistics> follower;
  // now() - insertTP of last uncommitted entry
  std::chrono::duration<double, std::milli> commitLagMS;
  CommitFailReason lastCommitStatus;

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> LeaderStatus;

  friend auto operator==(LeaderStatus const& left, LeaderStatus const& right) -> bool;
  friend auto operator!=(LeaderStatus const& left, LeaderStatus const& right) -> bool;
};

[[nodiscard]] auto operator==(LeaderStatus const& left, LeaderStatus const& right) -> bool;
[[nodiscard]] auto operator!=(LeaderStatus const& left, LeaderStatus const& right) -> bool;

struct FollowerStatus {
  LogStatistics local;
  std::optional<ParticipantId> leader;
  LogTerm term;
  LogIndex largestCommonIndex;

  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> FollowerStatus;
};

struct UnconfiguredStatus {
  void toVelocyPack(velocypack::Builder& builder) const;
  static auto fromVelocyPack(velocypack::Slice slice) -> UnconfiguredStatus;
};

struct LogStatus {
  using VariantType = std::variant<UnconfiguredStatus, LeaderStatus, FollowerStatus>;

  // default constructs as unconfigured status
  LogStatus() = default;
  explicit LogStatus(UnconfiguredStatus) noexcept;
  explicit LogStatus(LeaderStatus) noexcept;
  explicit LogStatus(FollowerStatus) noexcept;

  [[nodiscard]] auto getVariant() const noexcept -> VariantType const&;

  [[nodiscard]] auto getCurrentTerm() const noexcept -> std::optional<LogTerm>;
  [[nodiscard]] auto getLocalStatistics() const noexcept -> std::optional<LogStatistics>;

  static auto fromVelocyPack(velocypack::Slice slice) -> LogStatus;
  void toVelocyPack(velocypack::Builder& builder) const;
 private:
  VariantType _variant;
};

}
