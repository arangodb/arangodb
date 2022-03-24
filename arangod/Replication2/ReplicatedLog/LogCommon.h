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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////
#pragma once
#include <chrono>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <ostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of
// data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift
// intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/box.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

#include <velocypack/Buffer.h>
#include <velocypack/SharedSlice.h>
#include <velocypack/Slice.h>

#include <Basics/Identifier.h>
#include <Containers/FlatHashMap.h>
#include <Containers/ImmerMemoryPolicy.h>

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack

namespace arangodb::replication2 {

struct LogIndex {
  constexpr LogIndex() noexcept : value{0} {}
  constexpr explicit LogIndex(std::uint64_t value) noexcept : value{value} {}
  std::uint64_t value;

  [[nodiscard]] auto saturatedDecrement(uint64_t delta = 1) const noexcept
      -> LogIndex;

  friend auto operator<=>(LogIndex const&, LogIndex const&) = default;

  [[nodiscard]] auto operator+(std::uint64_t delta) const -> LogIndex;

  friend auto operator<<(std::ostream&, LogIndex) -> std::ostream&;

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

auto operator<<(std::ostream&, LogIndex) -> std::ostream&;

struct LogTerm {
  constexpr LogTerm() noexcept : value{0} {}
  constexpr explicit LogTerm(std::uint64_t value) noexcept : value{value} {}
  std::uint64_t value;
  friend auto operator<=>(LogTerm const&, LogTerm const&) = default;
  friend auto operator<<(std::ostream&, LogTerm) -> std::ostream&;

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

auto operator<<(std::ostream&, LogTerm) -> std::ostream&;

[[nodiscard]] auto to_string(LogTerm term) -> std::string;
[[nodiscard]] auto to_string(LogIndex index) -> std::string;

struct TermIndexPair {
  LogTerm term{};
  LogIndex index{};

  TermIndexPair(LogTerm term, LogIndex index) noexcept;
  TermIndexPair() = default;

  void toVelocyPack(velocypack::Builder& builder) const;
  [[nodiscard]] static auto fromVelocyPack(velocypack::Slice) -> TermIndexPair;

  friend auto operator<=>(TermIndexPair const&, TermIndexPair const&) = default;
  friend auto operator<<(std::ostream&, TermIndexPair) -> std::ostream&;
};

auto operator<<(std::ostream&, TermIndexPair) -> std::ostream&;

struct LogRange {
  LogIndex from;
  LogIndex to;

  LogRange(LogIndex from, LogIndex to) noexcept;

  [[nodiscard]] auto empty() const noexcept -> bool;
  [[nodiscard]] auto count() const noexcept -> std::size_t;
  [[nodiscard]] auto contains(LogIndex idx) const noexcept -> bool;

  friend auto operator<<(std::ostream& os, LogRange const& r) -> std::ostream&;
  friend auto intersect(LogRange a, LogRange b) noexcept -> LogRange;

  struct Iterator {
    auto operator++() noexcept -> Iterator&;
    auto operator++(int) noexcept -> Iterator;
    auto operator*() const noexcept -> LogIndex;
    auto operator->() const noexcept -> LogIndex const*;
    friend auto operator==(Iterator const& a, Iterator const& b) noexcept
        -> bool = default;

   private:
    friend LogRange;
    explicit Iterator(LogIndex idx) : current(idx) {}
    LogIndex current;
  };

  friend auto operator==(LogRange, LogRange) noexcept -> bool = default;

  [[nodiscard]] auto begin() const noexcept -> Iterator;
  [[nodiscard]] auto end() const noexcept -> Iterator;
};

auto operator<<(std::ostream& os, LogRange const& r) -> std::ostream&;
auto intersect(LogRange a, LogRange b) noexcept -> LogRange;
auto to_string(LogRange const&) -> std::string;

using ParticipantId = std::string;

class LogId : public arangodb::basics::Identifier {
 public:
  using arangodb::basics::Identifier::Identifier;

  static auto fromString(std::string_view) noexcept -> std::optional<LogId>;

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

auto to_string(LogId logId) -> std::string;

struct GlobalLogIdentifier {
  GlobalLogIdentifier(std::string database, LogId id);
  std::string database;
  LogId id;
};

auto to_string(GlobalLogIdentifier const&) -> std::string;

struct LogConfig {
  std::size_t writeConcern = 1;
  std::size_t softWriteConcern = 1;
  std::size_t replicationFactor = 1;
  bool waitForSync = false;

  auto toVelocyPack(velocypack::Builder&) const -> void;
  explicit LogConfig(velocypack::Slice);
  LogConfig() noexcept = default;
  LogConfig(std::size_t writeConcern, std::size_t softWriteConcern,
            std::size_t replicationFactor, bool waitForSync) noexcept;

  friend auto operator==(LogConfig const& left, LogConfig const& right) noexcept
      -> bool = default;
};

struct ParticipantFlags {
  bool forced = false;
  bool allowedInQuorum = true;
  bool allowedAsLeader = true;

  friend auto operator==(ParticipantFlags const& left,
                         ParticipantFlags const& right) noexcept
      -> bool = default;

  friend auto operator<<(std::ostream&, ParticipantFlags const&)
      -> std::ostream&;

  void toVelocyPack(velocypack::Builder&) const;
  static auto fromVelocyPack(velocypack::Slice) -> ParticipantFlags;
};

auto operator<<(std::ostream&, ParticipantFlags const&) -> std::ostream&;

using ParticipantsFlagsMap =
    std::unordered_map<ParticipantId, ParticipantFlags>;

struct ParticipantsConfig {
  std::size_t generation = 0;
  ParticipantsFlagsMap participants;

  void toVelocyPack(velocypack::Builder&) const;
  static auto fromVelocyPack(velocypack::Slice) -> ParticipantsConfig;

  // to be defaulted soon
  friend auto operator==(ParticipantsConfig const& left,
                         ParticipantsConfig const& right) noexcept
      -> bool = default;
};

// These settings are initialised by the ReplicatedLogFeature based on command
// line arguments
struct ReplicatedLogGlobalSettings {
 public:
  static inline constexpr std::size_t defaultThresholdNetworkBatchSize{1024 *
                                                                       1024};
  static inline constexpr std::size_t minThresholdNetworkBatchSize{1024 * 1024};

  static inline constexpr std::size_t defaultThresholdRocksDBWriteBatchSize{
      1024 * 1024};
  static inline constexpr std::size_t minThresholdRocksDBWriteBatchSize{1024 *
                                                                        1024};

  std::size_t _thresholdNetworkBatchSize{defaultThresholdNetworkBatchSize};
  std::size_t _thresholdRocksDBWriteBatchSize{
      defaultThresholdRocksDBWriteBatchSize};
};

namespace replicated_log {
/*
 * Indicates why the commit index is not increasing as expected.
 * Even though some pending entries might have been committed, unless all
 * pending entries are committed, we say the commit index is behind. This object
 * gives an indication of why might that be.
 */
struct CommitFailReason {
  CommitFailReason() = default;

  struct NothingToCommit {
    static auto fromVelocyPack(velocypack::Slice) -> NothingToCommit;
    void toVelocyPack(velocypack::Builder& builder) const;
    friend auto operator==(NothingToCommit const& left,
                           NothingToCommit const& right) noexcept
        -> bool = default;
  };
  struct QuorumSizeNotReached {
    struct ParticipantInfo {
      bool isFailed{};
      bool isAllowedInQuorum{};
      TermIndexPair lastAcknowledged;
      static auto fromVelocyPack(velocypack::Slice) -> ParticipantInfo;
      void toVelocyPack(velocypack::Builder& builder) const;
      friend auto operator==(ParticipantInfo const& left,
                             ParticipantInfo const& right) noexcept
          -> bool = default;
    };
    using who_type = containers::FlatHashMap<ParticipantId, ParticipantInfo>;
    static auto fromVelocyPack(velocypack::Slice) -> QuorumSizeNotReached;
    void toVelocyPack(velocypack::Builder& builder) const;
    who_type who;
    TermIndexPair spearhead;
    friend auto operator==(QuorumSizeNotReached const& left,
                           QuorumSizeNotReached const& right) noexcept
        -> bool = default;
  };
  struct ForcedParticipantNotInQuorum {
    static auto fromVelocyPack(velocypack::Slice)
        -> ForcedParticipantNotInQuorum;
    void toVelocyPack(velocypack::Builder& builder) const;
    ParticipantId who;
    friend auto operator==(ForcedParticipantNotInQuorum const& left,
                           ForcedParticipantNotInQuorum const& right) noexcept
        -> bool = default;
  };
  struct NonEligibleServerRequiredForQuorum {
    enum Why {
      kNotAllowedInQuorum,
      // WrongTerm might be misleading, because the follower might be in the
      // right term, it just never has acked an entry of the current term.
      kWrongTerm,
    };
    static auto to_string(Why) noexcept -> std::string_view;

    using CandidateMap = std::unordered_map<ParticipantId, Why>;

    CandidateMap candidates;

    static auto fromVelocyPack(velocypack::Slice)
        -> NonEligibleServerRequiredForQuorum;
    void toVelocyPack(velocypack::Builder& builder) const;
    friend auto operator==(
        NonEligibleServerRequiredForQuorum const& left,
        NonEligibleServerRequiredForQuorum const& right) noexcept
        -> bool = default;
  };
  struct FewerParticipantsThanWriteConcern {
    std::size_t writeConcern{};
    std::size_t softWriteConcern{};
    std::size_t effectiveWriteConcern{};
    std::size_t numParticipants{};
    static auto fromVelocyPack(velocypack::Slice)
        -> FewerParticipantsThanWriteConcern;
    void toVelocyPack(velocypack::Builder& builder) const;
    friend auto operator==(
        FewerParticipantsThanWriteConcern const& left,
        FewerParticipantsThanWriteConcern const& right) noexcept
        -> bool = default;
  };
  std::variant<NothingToCommit, QuorumSizeNotReached,
               ForcedParticipantNotInQuorum, NonEligibleServerRequiredForQuorum,
               FewerParticipantsThanWriteConcern>
      value;

  static auto withNothingToCommit() noexcept -> CommitFailReason;
  static auto withQuorumSizeNotReached(QuorumSizeNotReached::who_type who,
                                       TermIndexPair spearhead) noexcept
      -> CommitFailReason;
  static auto withForcedParticipantNotInQuorum(ParticipantId who) noexcept
      -> CommitFailReason;
  static auto withNonEligibleServerRequiredForQuorum(
      NonEligibleServerRequiredForQuorum::CandidateMap) noexcept
      -> CommitFailReason;
  // This would have too many `std::size_t` arguments to not be confusing,
  // so taking the full object instead.
  static auto withFewerParticipantsThanWriteConcern(
      FewerParticipantsThanWriteConcern) -> CommitFailReason;

  static auto fromVelocyPack(velocypack::Slice) -> CommitFailReason;
  void toVelocyPack(velocypack::Builder& builder) const;

  friend auto operator==(CommitFailReason const& left,
                         CommitFailReason const& right) -> bool = default;

 private:
  template<typename... Args>
  explicit CommitFailReason(std::in_place_t, Args&&... args) noexcept;
};

auto operator<<(std::ostream&,
                CommitFailReason::QuorumSizeNotReached::ParticipantInfo)
    -> std::ostream&;

auto to_string(CommitFailReason const&) -> std::string;
}  // namespace replicated_log

template<class Inspector>
auto inspect(Inspector& f, LogId& x) {
  if constexpr (Inspector::isLoading) {
    int v{0};
    auto res = f.apply(v);
    if (res.ok()) {
      x = LogId(v);
    }
    return res;
  } else {
    return f.apply(*x.data());
  }
}

template<class Inspector>
auto inspect(Inspector& f, LogConfig& x) {
  return f.object(x).fields(f.field("writeConcern", x.writeConcern),
                            f.field("replicationFactor", x.replicationFactor),
                            f.field("softWriteConcern", x.softWriteConcern),
                            f.field("waitForSync", x.waitForSync));
}

}  // namespace arangodb::replication2

namespace arangodb {
template<>
struct velocypack::Extractor<replication2::LogTerm> {
  static auto extract(velocypack::Slice slice) -> replication2::LogTerm {
    return replication2::LogTerm{slice.getNumericValue<std::uint64_t>()};
  }
};

template<>
struct velocypack::Extractor<replication2::LogIndex> {
  static auto extract(velocypack::Slice slice) -> replication2::LogIndex {
    return replication2::LogIndex{slice.getNumericValue<std::uint64_t>()};
  }
};

template<>
struct velocypack::Extractor<replication2::LogId> {
  static auto extract(velocypack::Slice slice) -> replication2::LogId {
    return replication2::LogId{slice.getNumericValue<std::uint64_t>()};
  }
};

}  // namespace arangodb

template<>
struct std::hash<arangodb::replication2::LogIndex> {
  [[nodiscard]] auto operator()(
      arangodb::replication2::LogIndex const& v) const noexcept -> std::size_t {
    return std::hash<uint64_t>{}(v.value);
  }
};

template<>
struct std::hash<arangodb::replication2::LogTerm> {
  [[nodiscard]] auto operator()(
      arangodb::replication2::LogTerm const& v) const noexcept -> std::size_t {
    return std::hash<uint64_t>{}(v.value);
  }
};

template<>
struct std::hash<arangodb::replication2::LogId> {
  [[nodiscard]] auto operator()(
      arangodb::replication2::LogId const& v) const noexcept -> std::size_t {
    return std::hash<arangodb::basics::Identifier>{}(v);
  }
};
