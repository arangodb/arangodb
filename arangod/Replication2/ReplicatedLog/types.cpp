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
#include <Basics/StaticStrings.h>
#include <Basics/application-exit.h>
#include <Basics/debugging.h>
#include <Basics/overload.h>
#include <Basics/voc-errors.h>
#include <Containers/ImmerMemoryPolicy.h>
#include <Logger/LogMacros.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <cstddef>
#include <functional>
#include <utility>

#include "Replication2/ReplicatedLog/NetworkMessages.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

replicated_log::QuorumData::QuorumData(LogIndex index, LogTerm term,
                                       std::vector<ParticipantId> quorum)
    : index(index), term(term), quorum(std::move(quorum)) {}

replicated_log::QuorumData::QuorumData(LogIndex index, LogTerm term)
    : QuorumData(index, term, {}) {}

replicated_log::QuorumData::QuorumData(VPackSlice slice) {
  index = slice.get(StaticStrings::Index).extract<LogIndex>();
  term = slice.get("term").extract<LogTerm>();
  for (auto part : VPackArrayIterator(slice.get("quorum"))) {
    quorum.push_back(part.copyString());
  }
}

void replicated_log::QuorumData::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Index, VPackValue(index.value));
  builder.add("term", VPackValue(term.value));
  {
    VPackArrayBuilder ab(&builder, "quorum");
    for (auto const& part : quorum) {
      builder.add(VPackValue(part));
    }
  }
}


void replicated_log::LogStatistics::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::CommitIndex, VPackValue(commitIndex.value));
  builder.add(VPackValue(StaticStrings::Spearhead));
  spearHead.toVelocyPack(builder);
}

auto replicated_log::LogStatistics::fromVelocyPack(velocypack::Slice slice) -> LogStatistics {
  LogStatistics stats;
  stats.commitIndex = slice.get(StaticStrings::CommitIndex).extract<LogIndex>();
  stats.spearHead = TermIndexPair::fromVelocyPack(slice.get(StaticStrings::Spearhead));
  return stats;
}

auto arangodb::replication2::replicated_log::to_string(AppendEntriesErrorReason reason) noexcept
    -> std::string_view {
  switch (reason) {
    case AppendEntriesErrorReason::NONE:
      return {};
    case AppendEntriesErrorReason::INVALID_LEADER_ID:
      return "leader id was invalid";
    case AppendEntriesErrorReason::LOST_LOG_CORE:
      return "term has changed and an internal state was lost";
    case AppendEntriesErrorReason::WRONG_TERM:
      return "current term is different from leader term";
    case AppendEntriesErrorReason::NO_PREV_LOG_MATCH:
      return "previous log index did not match";
    case AppendEntriesErrorReason::MESSAGE_OUTDATED:
      return "message was outdated";
    case AppendEntriesErrorReason::PERSISTENCE_FAILURE:
      return "persisting the log entries failed";
    case AppendEntriesErrorReason::COMMUNICATION_ERROR:
      return "communicating with participant failed - network error";
  }
  LOG_TOPIC("c2058", FATAL, Logger::REPLICATION2)
      << "Invalid AppendEntriesErrorReason "
      << static_cast<std::underlying_type_t<decltype(reason)>>(reason);
  FATAL_ERROR_ABORT();
}
