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

#include "types.h"

#include <Basics/Exceptions.h>
#include <Basics/debugging.h>
#include <Basics/overload.h>
#include <Basics/voc-errors.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <cstddef>
#include <functional>
#include <utility>

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/flex_vector_transient.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

#include "Replication2/ReplicatedLog/messages.h"

using namespace arangodb;
using namespace arangodb::replication2;

replicated_log::QuorumData::QuorumData(LogIndex const& index, LogTerm term,
                                       std::vector<ParticipantId> quorum)
    : index(index), term(term), quorum(std::move(quorum)) {}

replicated_log::QuorumData::QuorumData(VPackSlice slice) {
  index = LogIndex{slice.get("index").extract<std::size_t>()};
  term = LogTerm{slice.get("term").extract<std::size_t>()};
  for (auto part : VPackArrayIterator(slice.get("quorum"))) {
    quorum.push_back(part.copyString());
  }
}

void replicated_log::QuorumData::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("index", VPackValue(index.value));
  builder.add("term", VPackValue(term.value));
  {
    VPackArrayBuilder ab(&builder, "quorum");
    for (auto const& part : quorum) {
      builder.add(VPackValue(part));
    }
  }
}

void replicated_log::AppendEntriesResult::toVelocyPack(velocypack::Builder& builder) const {
  {
    velocypack::ObjectBuilder ob(&builder);
    builder.add("term", VPackValue(logTerm.value));
    builder.add("errorCode", VPackValue(errorCode));
    builder.add("reason", VPackValue(int(reason)));
    builder.add("messageId", VPackValue(messageId.value));
  }
}

auto replicated_log::AppendEntriesResult::fromVelocyPack(velocypack::Slice slice)
    -> AppendEntriesResult {
  auto logTerm = LogTerm{slice.get("term").extract<size_t>()};
  auto errorCode = ErrorCode{slice.get("errorCode").extract<int>()};
  auto reason = AppendEntriesErrorReason{slice.get("reason").extract<int>()};
  auto messageId = MessageId{slice.get("messageId").extract<uint64_t>()};

  TRI_ASSERT(errorCode == TRI_ERROR_NO_ERROR ||
             reason != replicated_log::AppendEntriesErrorReason::NONE);
  return AppendEntriesResult{logTerm, errorCode, reason, messageId};
}

replicated_log::AppendEntriesResult::AppendEntriesResult(LogTerm logTerm, ErrorCode errorCode,
                                                         AppendEntriesErrorReason reason, MessageId id)
    : logTerm(logTerm), errorCode(errorCode), reason(reason), messageId(id) {
  TRI_ASSERT(errorCode == TRI_ERROR_NO_ERROR ||
             reason != replicated_log::AppendEntriesErrorReason::NONE);
}
replicated_log::AppendEntriesResult::AppendEntriesResult(LogTerm logTerm, MessageId id)
    : AppendEntriesResult(logTerm, TRI_ERROR_NO_ERROR, AppendEntriesErrorReason::NONE, id) {}

void replicated_log::AppendEntriesRequest::toVelocyPack(velocypack::Builder& builder) const {
  {
    velocypack::ObjectBuilder ob(&builder);
    builder.add("leaderTerm", VPackValue(leaderTerm.value));
    builder.add("leaderId", VPackValue(leaderId));
    builder.add("prevLogTerm", VPackValue(prevLogTerm.value));
    builder.add("prevLogIndex", VPackValue(prevLogIndex.value));
    builder.add("leaderCommit", VPackValue(leaderCommit.value));
    builder.add("messageId", VPackValue(messageId.value));
    builder.add("waitForSync", VPackValue(waitForSync));
    builder.add("entries", VPackValue(VPackValueType::Array));
    for (auto const& it : entries) {
      it.toVelocyPack(builder);
    }
    builder.close();  // close entries
  }
}

auto replicated_log::AppendEntriesRequest::fromVelocyPack(velocypack::Slice slice)
    -> AppendEntriesRequest {
  auto leaderTerm = LogTerm{slice.get("leaderTerm").getNumericValue<size_t>()};
  auto leaderId = ParticipantId{slice.get("leaderId").copyString()};
  auto prevLogTerm = LogTerm{slice.get("prevLogTerm").getNumericValue<size_t>()};
  auto prevLogIndex = LogIndex{slice.get("prevLogIndex").getNumericValue<size_t>()};
  auto leaderCommit = LogIndex{slice.get("leaderCommit").getNumericValue<size_t>()};
  auto messageId = MessageId{slice.get("messageId").getNumericValue<uint64_t>()};
  auto waitForSync = slice.get("waitForSync").extract<bool>();
  auto entries = std::invoke([&] {
    auto entriesVp = velocypack::ArrayIterator(slice.get("entries"));
    auto transientEntries = immer::flex_vector_transient<LogEntry>{};
    std::transform(entriesVp.begin(), entriesVp.end(), std::back_inserter(transientEntries),
                   [](auto const& it) { return LogEntry::fromVelocyPack(it); });
    return std::move(transientEntries).persistent();
  });

  return AppendEntriesRequest{leaderTerm,        leaderId,     prevLogTerm,
                              prevLogIndex,      leaderCommit, messageId,
                              waitForSync, std::move(entries)};
}

auto replicated_log::operator<=(replicated_log::TermIndexPair const& left,
                                replicated_log::TermIndexPair const& right) noexcept -> bool {
  if (left.term < right.term) {
    return true;
  } else if (left.term == right.term) {
    return left.index <= right.index;
  } else {
    return false;
  }
}

void replicated_log::TermIndexPair::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("term", VPackValue(term.value));
  builder.add("index", VPackValue(index.value));
}

void replicated_log::LogStatistics::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("commitIndex", VPackValue(commitIndex.value));
  builder.add(VPackValue("spearHead"));
  spearHead.toVelocyPack(builder);
}

void replicated_log::UnconfiguredStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue("unconfigured"));
}

void replicated_log::FollowerStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue("follower"));
  builder.add("leader", VPackValue(leader));
  builder.add("term", VPackValue(term.value));
  builder.add(VPackValue("local"));
  local.toVelocyPack(builder);
}

void replicated_log::LeaderStatus::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("role", VPackValue("leader"));
  builder.add("term", VPackValue(term.value));
  builder.add(VPackValue("local"));
  local.toVelocyPack(builder);
  {
    VPackObjectBuilder ob2(&builder, "follower");
    for (auto const& [id, stat] : follower) {
      builder.add(VPackValue(id));
      stat.toVelocyPack(builder);
    }
  }
}

void replicated_log::LeaderStatus::FollowerStatistics::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add("commitIndex", VPackValue(commitIndex.value));
  builder.add(VPackValue("spearHead"));
  spearHead.toVelocyPack(builder);
  builder.add("lastErrorReason", VPackValue(int(lastErrorReason)));
  builder.add("lastErrorReasonMessage", VPackValue(to_string(lastErrorReason)));
  builder.add("lastRequestLatencyMS", VPackValue(lastRequestLatencyMS));
}

std::string arangodb::replication2::replicated_log::to_string(replicated_log::AppendEntriesErrorReason reason) {
  switch (reason) {
    case replicated_log::AppendEntriesErrorReason::NONE:
      return {};
    case replicated_log::AppendEntriesErrorReason::INVALID_LEADER_ID:
      return "leader id was invalid";
    case replicated_log::AppendEntriesErrorReason::LOST_LOG_CORE:
      return "term has changed and an internal state was lost";
    case replicated_log::AppendEntriesErrorReason::WRONG_TERM:
      return "current term is different from leader term";
    case replicated_log::AppendEntriesErrorReason::NO_PREV_LOG_MATCH:
      return "previous log index did not match";
    case AppendEntriesErrorReason::MESSAGE_OUTDATED:
      return "message was outdated";
    case AppendEntriesErrorReason::PERSISTENCE_FAILURE:
      return "persisting the log entries failed";
  }
  THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
}

auto arangodb::replication2::replicated_log::MessageId::operator<=(MessageId other) const
    -> bool {
  return value <= other.value;
}

auto arangodb::replication2::replicated_log::getCurrentTerm(LogStatus const& status) noexcept
    -> std::optional<LogTerm> {
  return std::visit(
      overload{[&](replicated_log::UnconfiguredStatus) -> std::optional<LogTerm> {
                 return std::nullopt;
               },
               [&](replicated_log::LeaderStatus const& s) -> std::optional<LogTerm> {
                 return s.term;
               },
               [&](replicated_log::FollowerStatus const& s) -> std::optional<LogTerm> {
                 return s.term;
               }},
      status);
}

auto arangodb::replication2::replicated_log::getLocalStatistics(LogStatus const& status) noexcept
    -> std::optional<LogStatistics> {
  return std::visit(
      overload{[&](replicated_log::UnconfiguredStatus const& s) -> std::optional<LogStatistics> {
                 return std::nullopt;
               },
               [&](replicated_log::LeaderStatus const& s) -> std::optional<LogStatistics> {
                 return s.local;
               },
               [&](replicated_log::FollowerStatus const& s) -> std::optional<LogStatistics> {
                 return s.local;
               }},
      status);
}
