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
#include <velocypack/Value.h>

#include "Inspection/Format.h"
#include "Inspection/Status.h"
#include "Inspection/Types.h"
#include "Basics/ErrorCode.h"

#include <Basics/Identifier.h>
#include <Containers/FlatHashMap.h>
#include <Containers/ImmerMemoryPolicy.h>

namespace arangodb::velocypack {
class Builder;
class Slice;
}  // namespace arangodb::velocypack
namespace arangodb {
template<typename T>
class ResultT;
}
namespace arangodb::replication2 {

struct LogIndex {
  constexpr LogIndex() noexcept : value{0} {}
  constexpr explicit LogIndex(std::uint64_t value) noexcept : value{value} {}
  std::uint64_t value;

  [[nodiscard]] auto saturatedDecrement(uint64_t delta = 1) const noexcept
      -> LogIndex;

  friend auto operator<=>(LogIndex const&, LogIndex const&) = default;

  [[nodiscard]] auto operator+(std::uint64_t delta) const -> LogIndex;
  auto operator+=(std::uint64_t delta) -> LogIndex&;
  auto operator++() -> LogIndex&;

  friend auto operator<<(std::ostream&, LogIndex) -> std::ostream&;

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

auto operator<<(std::ostream&, LogIndex) -> std::ostream&;

template<class Inspector>
auto inspect(Inspector& f, LogIndex& x) {
  if constexpr (Inspector::isLoading) {
    auto v = uint64_t{0};
    auto res = f.apply(v);
    if (res.ok()) {
      x = LogIndex(v);
    }
    return res;
  } else {
    return f.apply(x.value);
  }
}

struct LogTerm {
  constexpr LogTerm() noexcept : value{0} {}
  constexpr explicit LogTerm(std::uint64_t value) noexcept : value{value} {}
  std::uint64_t value;
  friend auto operator<=>(LogTerm const&, LogTerm const&) = default;
  friend auto operator<<(std::ostream&, LogTerm) -> std::ostream&;

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

auto operator<<(std::ostream&, LogTerm) -> std::ostream&;

template<class Inspector>
auto inspect(Inspector& f, LogTerm& x) {
  if constexpr (Inspector::isLoading) {
    auto v = uint64_t{0};
    auto res = f.apply(v);
    if (res.ok()) {
      x = LogTerm(v);
    }
    return res;
  } else {
    return f.apply(x.value);
  }
}

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

template<class Inspector>
auto inspect(Inspector& f, TermIndexPair& x) {
  return f.object(x).fields(f.field("term", x.term), f.field("index", x.index));
}

struct LogRange {
  LogIndex from{0};
  LogIndex to{0};

  LogRange() noexcept = default;
  LogRange(LogIndex from, LogIndex to) noexcept;

  [[nodiscard]] auto empty() const noexcept -> bool;
  [[nodiscard]] auto count() const noexcept -> std::size_t;
  [[nodiscard]] auto contains(LogIndex idx) const noexcept -> bool;
  [[nodiscard]] auto contains(LogRange idx) const noexcept -> bool;

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

  [[nodiscard]] auto begin() const noexcept -> Iterator;
  [[nodiscard]] auto end() const noexcept -> Iterator;
};

auto operator==(LogRange, LogRange) noexcept -> bool;
auto operator<<(std::ostream& os, LogRange const& r) -> std::ostream&;

template<class Inspector>
auto inspect(Inspector& f, LogRange& x) {
  return f.object(x).fields(f.field("from", x.from), f.field("to", x.to));
}

auto intersect(LogRange a, LogRange b) noexcept -> LogRange;
auto to_string(LogRange const&) -> std::string;

using ParticipantId = std::string;

class LogId : public arangodb::basics::Identifier {
 public:
  using arangodb::basics::Identifier::Identifier;

  static auto fromString(std::string_view) noexcept -> std::optional<LogId>;

  [[nodiscard]] explicit operator velocypack::Value() const noexcept;
};

template<class Inspector>
auto inspect(Inspector& f, LogId& x) {
  if constexpr (Inspector::isLoading) {
    auto v = uint64_t{0};
    auto res = f.apply(v);
    if (res.ok()) {
      x = LogId(v);
    }
    return res;
  } else {
    // TODO this is a hack to make the compiler happy who does not want
    //      to assign x.id() (unsigned long int) to what it expects (unsigned
    //      long int&)
    auto id = x.id();
    return f.apply(id);
  }
}

auto to_string(LogId logId) -> std::string;

struct GlobalLogIdentifier {
  GlobalLogIdentifier(std::string database, LogId id);
  GlobalLogIdentifier() = default;
  std::string database{};
  LogId id{};

  template<class Inspector>
  inline friend auto inspect(Inspector& f, GlobalLogIdentifier& gid) {
    return f.object(gid).fields(f.field("database", gid.database),
                                f.field("id", gid.id));
  }
};

auto to_string(GlobalLogIdentifier const&) -> std::string;

auto operator<<(std::ostream&, GlobalLogIdentifier const&) -> std::ostream&;

struct ParticipantFlags {
  bool forced = false;
  bool allowedInQuorum = true;
  bool allowedAsLeader = true;

  friend auto operator==(ParticipantFlags const& left,
                         ParticipantFlags const& right) noexcept
      -> bool = default;

  friend auto operator<<(std::ostream&, ParticipantFlags const&)
      -> std::ostream&;
};

template<class Inspector>
auto inspect(Inspector& f, ParticipantFlags& x) {
  return f.object(x).fields(
      f.field("forced", x.forced).fallback(false),
      f.field("allowedInQuorum", x.allowedInQuorum).fallback(true),
      f.field("allowedAsLeader", x.allowedAsLeader).fallback(true));
}

auto operator<<(std::ostream&, ParticipantFlags const&) -> std::ostream&;

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
  static inline constexpr std::size_t defaultThresholdLogCompaction{1000};

  std::size_t _thresholdNetworkBatchSize{defaultThresholdNetworkBatchSize};
  std::size_t _thresholdRocksDBWriteBatchSize{
      defaultThresholdRocksDBWriteBatchSize};
  std::size_t _thresholdLogCompaction{defaultThresholdLogCompaction};
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
    friend auto operator==(NothingToCommit const& left,
                           NothingToCommit const& right) noexcept
        -> bool = default;

    template<typename Inspector>
    friend auto inspect(Inspector& f, NothingToCommit& x) {
      return f.object(x).fields();
    }
  };
  struct QuorumSizeNotReached {
    struct ParticipantInfo {
      bool isAllowedInQuorum{};
      bool snapshotAvailable{};
      TermIndexPair lastAcknowledged;
      friend auto operator==(ParticipantInfo const& left,
                             ParticipantInfo const& right) noexcept
          -> bool = default;
      template<typename Inspector>
      friend auto inspect(Inspector& f, ParticipantInfo& x) {
        return f.object(x).fields(
            f.field("isAllowedInQuorum", x.isAllowedInQuorum),
            f.field("snapshotAvailable", x.snapshotAvailable),
            f.field("lastAcknowledged", x.lastAcknowledged));
      }
    };
    using who_type = containers::FlatHashMap<ParticipantId, ParticipantInfo>;

    who_type who;
    TermIndexPair spearhead;
    friend auto operator==(QuorumSizeNotReached const& left,
                           QuorumSizeNotReached const& right) noexcept
        -> bool = default;

    template<typename Inspector>
    friend auto inspect(Inspector& f, QuorumSizeNotReached& x) {
      return f.object(x).fields(f.field("who", x.who),
                                f.field("spearhead", x.spearhead));
    }
  };
  struct ForcedParticipantNotInQuorum {
    ParticipantId who;
    friend auto operator==(ForcedParticipantNotInQuorum const& left,
                           ForcedParticipantNotInQuorum const& right) noexcept
        -> bool = default;
    template<typename Inspector>
    friend auto inspect(Inspector& f, ForcedParticipantNotInQuorum& x) {
      return f.object(x).fields(f.field("who", x.who));
    }
  };
  struct NonEligibleServerRequiredForQuorum {
    enum class Why {
      kNotAllowedInQuorum,
      // WrongTerm might be misleading, because the follower might be in the
      // right term, it just never has acked an entry of the current term.
      kWrongTerm,
      kSnapshotMissing,
    };

    using CandidateMap = std::unordered_map<ParticipantId, Why>;

    CandidateMap candidates;

    friend auto operator==(
        NonEligibleServerRequiredForQuorum const& left,
        NonEligibleServerRequiredForQuorum const& right) noexcept
        -> bool = default;
    template<typename Inspector>
    friend auto inspect(Inspector& f, NonEligibleServerRequiredForQuorum& x) {
      return f.object(x).fields(f.field("candidates", x.candidates));
    }

    template<class Inspector>
    friend auto inspect(Inspector& f, Why& x) {
      return f.enumeration(x).values(
          Why::kNotAllowedInQuorum, "notAllowedInQuorum", Why::kWrongTerm,
          "wrongTerm", Why::kSnapshotMissing, "snapshotMissing");
    }
  };
  struct FewerParticipantsThanWriteConcern {
    std::size_t effectiveWriteConcern{};
    std::size_t numParticipants{};
    friend auto operator==(
        FewerParticipantsThanWriteConcern const& left,
        FewerParticipantsThanWriteConcern const& right) noexcept
        -> bool = default;
    template<typename Inspector>
    friend auto inspect(Inspector& f, FewerParticipantsThanWriteConcern& x) {
      return f.object(x).fields(
          f.field("effectiveWriteConcern", x.effectiveWriteConcern),
          f.field("numParticipants", x.numParticipants));
    }
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

  friend auto operator==(CommitFailReason const& left,
                         CommitFailReason const& right) -> bool = default;

  template<typename Inspector>
  friend auto inspect(Inspector& f, CommitFailReason& x) {
    namespace insp = arangodb::inspection;
    return f.variant(x.value).embedded("reason").alternatives(
        insp::type<NothingToCommit>("NothingToCommit"),
        insp::type<QuorumSizeNotReached>("QuorumSizeNotReached"),
        insp::type<ForcedParticipantNotInQuorum>(
            "ForcedParticipantNotInQuorum"),
        insp::type<NonEligibleServerRequiredForQuorum>(
            "NonEligibleServerRequiredForQuorum"),
        insp::type<FewerParticipantsThanWriteConcern>(
            "FewerParticipantsThanWriteConcern"));
  }

 private:
  template<typename... Args>
  explicit CommitFailReason(std::in_place_t, Args&&... args) noexcept;
};

auto to_string(
    CommitFailReason::NonEligibleServerRequiredForQuorum::Why) noexcept
    -> std::string_view;

auto operator<<(std::ostream&,
                CommitFailReason::QuorumSizeNotReached::ParticipantInfo)
    -> std::ostream&;

auto to_string(CommitFailReason const&) -> std::string;

struct CompactionStopReason {
  struct CompactionThresholdNotReached {
    LogIndex nextCompactionAt;
    template<class Inspector>
    friend auto inspect(Inspector& f, CompactionThresholdNotReached& x) {
      return f.object(x).fields(
          f.field("nextCompactionAt", x.nextCompactionAt));
    }
    friend auto operator==(CompactionThresholdNotReached const& left,
                           CompactionThresholdNotReached const& right) noexcept
        -> bool = default;
  };
  struct NotReleasedByStateMachine {
    LogIndex releasedIndex;
    template<class Inspector>
    friend auto inspect(Inspector& f, NotReleasedByStateMachine& x) {
      return f.object(x).fields(f.field("releasedIndex", x.releasedIndex));
    }
    friend auto operator==(NotReleasedByStateMachine const& left,
                           NotReleasedByStateMachine const& right) noexcept
        -> bool = default;
  };
  struct ParticipantMissingEntries {
    ParticipantId who;
    template<class Inspector>
    friend auto inspect(Inspector& f, ParticipantMissingEntries& x) {
      return f.object(x).fields(f.field("who", x.who));
    }
    friend auto operator==(ParticipantMissingEntries const& left,
                           ParticipantMissingEntries const& right) noexcept
        -> bool = default;
  };
  struct LeaderBlocksReleaseEntry {
    LogIndex lowestIndexToKeep;
    template<class Inspector>
    friend auto inspect(Inspector& f, LeaderBlocksReleaseEntry& x) {
      return f.object(x).fields(
          f.field("lowestIndexToKeep", x.lowestIndexToKeep));
    }
    friend auto operator==(LeaderBlocksReleaseEntry const& left,
                           LeaderBlocksReleaseEntry const& right) noexcept
        -> bool = default;
  };

  struct NothingToCompact {
    template<class Inspector>
    friend auto inspect(Inspector& f, NothingToCompact& x) {
      return f.object(x).fields();
    }
    friend auto operator==(NothingToCompact const& left,
                           NothingToCompact const& right) noexcept
        -> bool = default;
  };

  std::variant<CompactionThresholdNotReached, NotReleasedByStateMachine,
               ParticipantMissingEntries, LeaderBlocksReleaseEntry,
               NothingToCompact>
      value;

  friend auto operator==(CompactionStopReason const& left,
                         CompactionStopReason const& right) noexcept
      -> bool = default;

  template<class Inspector>
  friend auto inspect(Inspector& f, CompactionStopReason& x) {
    namespace insp = arangodb::inspection;
    return f.variant(x.value).embedded("reason").alternatives(
        insp::type<CompactionThresholdNotReached>(
            "CompactionThresholdNotReached"),
        insp::type<NotReleasedByStateMachine>("NotReleasedByStateMachine"),
        insp::type<LeaderBlocksReleaseEntry>("LeaderBlocksRelease"),
        insp::type<NothingToCompact>("NothingToCompact"),
        insp::type<ParticipantMissingEntries>("ParticipantMissingEntries"));
  }
};

auto operator<<(std::ostream&, CompactionStopReason const&) -> std::ostream&;
auto to_string(CompactionStopReason const&) -> std::string;

struct CompactionResult {
  std::size_t numEntriesCompacted{0};
  LogRange range;
  std::optional<CompactionStopReason> stopReason;

  friend auto operator==(CompactionResult const& left,
                         CompactionResult const& right) noexcept
      -> bool = default;

  template<class Inspector>
  friend auto inspect(Inspector& f, CompactionResult& x) {
    return f.object(x).fields(
        f.field("numEntriesCompacted", x.numEntriesCompacted),
        f.field("stopReason", x.stopReason));
  }
};

struct CompactionResponse {
  struct Error {
    ErrorCode error{0};
    std::string errorMessage;

    template<class Inspector>
    friend auto inspect(Inspector& f, Error& x) {
      return f.object(x).fields(f.field("error", x.error),
                                f.field("errorMessage", x.errorMessage));
    }
  };
  static auto fromResult(ResultT<CompactionResult>) -> CompactionResponse;

  std::variant<CompactionResult, Error> value;

  template<class Inspector>
  friend auto inspect(Inspector& f, CompactionResponse& x) {
    namespace insp = arangodb::inspection;
    return f.variant(x.value).embedded("result").alternatives(
        insp::type<CompactionResult>("ok"),
        insp::type<CompactionResponse::Error>("error"));
  }
};

}  // namespace replicated_log

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
struct fmt::formatter<arangodb::replication2::LogId>
    : fmt::formatter<arangodb::basics::Identifier> {};

template<>
struct fmt::formatter<arangodb::replication2::GlobalLogIdentifier>
    : arangodb::inspection::inspection_formatter {};

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
