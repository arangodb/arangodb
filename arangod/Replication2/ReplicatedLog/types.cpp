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
#include <Basics/voc-errors.h>
#include <Logger/LogMacros.h>
#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include <cstddef>
#include <functional>
#include <utility>

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
  term = slice.get(StaticStrings::Term).extract<LogTerm>();
  for (auto part : VPackArrayIterator(slice.get("quorum"))) {
    quorum.push_back(part.copyString());
  }
}

void replicated_log::QuorumData::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Index, VPackValue(index.value));
  builder.add(StaticStrings::Term, VPackValue(term.value));
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
  builder.add("firstIndex", VPackValue(firstIndex.value));
  builder.add(VPackValue(StaticStrings::Spearhead));
  spearHead.toVelocyPack(builder);
}

auto replicated_log::LogStatistics::fromVelocyPack(velocypack::Slice slice) -> LogStatistics {
  LogStatistics stats;
  stats.commitIndex = slice.get(StaticStrings::CommitIndex).extract<LogIndex>();
  stats.firstIndex = slice.get("firstIndex").extract<LogIndex>();
  stats.spearHead = TermIndexPair::fromVelocyPack(slice.get(StaticStrings::Spearhead));
  return stats;
}

auto replicated_log::operator==(LogStatistics const& left,
                                LogStatistics const& right) noexcept -> bool {
  return left.spearHead == right.spearHead &&
         left.commitIndex == right.commitIndex &&
         left.firstIndex == right.firstIndex;
}

auto replicated_log::operator!=(LogStatistics const &left,
                                LogStatistics const& right) noexcept -> bool {
  return !(left == right);
}

auto replicated_log::AppendEntriesErrorReason::getErrorMessage()
    const noexcept -> std::string_view {
  switch (error) {
    case ErrorType::kNone:
      return "None";
    case ErrorType::kInvalidLeaderId:
      return "Leader id was invalid";
    case ErrorType::kLostLogCore:
      return "Term has changed and the internal state was lost";
    case ErrorType::kMessageOutdated:
      return "Message is outdated";
    case ErrorType::kWrongTerm:
      return "Term has changed and the internal state was lost";
    case ErrorType::kNoPrevLogMatch:
      return "Previous log index did not match";
    case ErrorType::kPersistenceFailure:
      return "Persisting the log entries failed";
    case ErrorType::kCommunicationError:
      return "Communicating with participant failed - network error";
  }
  LOG_TOPIC("ff21c", FATAL, Logger::REPLICATION2)
      << "Invalid AppendEntriesErrorReason "
      << static_cast<std::underlying_type_t<decltype(error)>>(error);
  FATAL_ERROR_ABORT();
}

constexpr static std::string_view kNoneString = "None";
constexpr static std::string_view kInvalidLeaderIdString = "InvalidLeaderId";
constexpr static std::string_view kLostLogCoreString = "LostLogCore";
constexpr static std::string_view kMessageOutdatedString = "MessageOutdated";
constexpr static std::string_view kWrongTermString = "WrongTerm";
constexpr static std::string_view kNoPrevLogMatchString = "NoPrevLogMatch";
constexpr static std::string_view kPersistenceFailureString =
    "PersistenceFailure";
constexpr static std::string_view kCommunicationErrorString =
    "CommunicationError";

auto replicated_log::AppendEntriesErrorReason::errorTypeFromString(std::string_view str)
    -> ErrorType {
  if (str == kNoneString) {
    return ErrorType::kNone;
  } else if (str == kInvalidLeaderIdString) {
    return ErrorType::kInvalidLeaderId;
  } else if (str == kLostLogCoreString) {
    return ErrorType::kLostLogCore;
  } else if (str == kMessageOutdatedString) {
    return ErrorType::kMessageOutdated;
  } else if (str == kWrongTermString) {
    return ErrorType::kWrongTerm;
  } else if (str == kNoPrevLogMatchString) {
    return ErrorType::kNoPrevLogMatch;
  } else if (str == kPersistenceFailureString) {
    return ErrorType::kPersistenceFailure;
  } else if (str == kCommunicationErrorString) {
    return ErrorType::kCommunicationError;
  }
  THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_BAD_PARAMETER,
                                "unknown error type %*s", str.size(), str.data());
}

auto replicated_log::to_string(AppendEntriesErrorReason::ErrorType error) noexcept
    -> std::string_view {
  switch (error) {
    case AppendEntriesErrorReason::ErrorType::kNone:
      return kNoneString;
    case AppendEntriesErrorReason::ErrorType::kInvalidLeaderId:
      return kInvalidLeaderIdString;
    case AppendEntriesErrorReason::ErrorType::kLostLogCore:
      return kLostLogCoreString;
    case AppendEntriesErrorReason::ErrorType::kMessageOutdated:
      return kMessageOutdatedString;
    case AppendEntriesErrorReason::ErrorType::kWrongTerm:
      return kWrongTermString;
    case AppendEntriesErrorReason::ErrorType::kNoPrevLogMatch:
      return kNoPrevLogMatchString;
    case AppendEntriesErrorReason::ErrorType::kPersistenceFailure:
      return kPersistenceFailureString;
    case AppendEntriesErrorReason::ErrorType::kCommunicationError:
      return kCommunicationErrorString;
  }
  LOG_TOPIC("c2058", FATAL, Logger::REPLICATION2)
      << "Invalid AppendEntriesErrorReason "
      << static_cast<std::underlying_type_t<decltype(error)>>(error);
  FATAL_ERROR_ABORT();
}

constexpr static std::string_view kDetailsString = "details";

void replicated_log::AppendEntriesErrorReason::toVelocyPack(velocypack::Builder& builder) const {
  VPackObjectBuilder ob(&builder);
  builder.add(StaticStrings::Error, VPackValue(to_string(error)));
  builder.add(StaticStrings::ErrorMessage, VPackValue(getErrorMessage()));
  if (details) {
    builder.add(VPackStringRef{kDetailsString}, VPackValue(details.value()));
  }
}

auto replicated_log::AppendEntriesErrorReason::fromVelocyPack(velocypack::Slice slice)
    -> AppendEntriesErrorReason {
  auto errorSlice = slice.get(StaticStrings::Error);
  TRI_ASSERT(errorSlice.isString()) << "Expected string, found: " << errorSlice.toJson();
  auto error = errorTypeFromString(errorSlice.copyString());

  std::optional<std::string> details;
  if (auto detailsSlice = slice.get(kDetailsString); !detailsSlice.isNone()) {
    details = detailsSlice.copyString();
  }
  return {error, std::move(details)};
}

auto FollowerState::withUpToDate() noexcept -> FollowerState {
  return FollowerState(std::in_place, UpToDate{});
}

auto FollowerState::withErrorBackoff(std::chrono::duration<double, std::milli> duration, std::size_t retryCount) noexcept
    -> FollowerState {
  return FollowerState(std::in_place, ErrorBackoff{duration, retryCount});
}

auto FollowerState::withRequestInFlight(std::chrono::duration<double, std::milli> duration) noexcept -> FollowerState {
  return FollowerState(std::in_place, RequestInFlight{duration});
}

constexpr static std::string_view upToDateString = "up-to-date";
constexpr static std::string_view errorBackoffString = "error-backoff";
constexpr static std::string_view requestInFlightString = "request-in-flight";

auto FollowerState::fromVelocyPack(velocypack::Slice slice) -> FollowerState {
  auto state = slice.get("state").extract<std::string_view>();
  if (state == errorBackoffString) {
    return FollowerState::withErrorBackoff(
        std::chrono::duration<double, std::milli>{slice.get("durationMS").extract<double>()},
        slice.get("retryCount").extract<std::size_t>());
  } else if (state == requestInFlightString) {
    return FollowerState::withRequestInFlight(std::chrono::duration<double, std::milli>{
        slice.get("durationMS").extract<double>()});
  } else {
    return FollowerState::withUpToDate();
  }
}

void FollowerState::toVelocyPack(velocypack::Builder& builder) const {
  struct ToVelocyPackVisitor {
    auto operator()(FollowerState::UpToDate const&) {
      builder.add("state", VPackValue(upToDateString));
    }

    auto operator()(FollowerState::ErrorBackoff const& err) {
      builder.add("state", VPackValue(errorBackoffString));
      builder.add("durationMS", VPackValue(err.durationMS.count()));
      builder.add("retryCount", VPackValue(err.retryCount));
    }

    auto operator()(FollowerState::RequestInFlight const& rif) {
      builder.add("state", VPackValue(requestInFlightString));
      builder.add("durationMS", VPackValue(rif.durationMS.count()));
    }

    velocypack::Builder& builder;
  };

  VPackObjectBuilder ob(&builder);
  std::visit(ToVelocyPackVisitor{builder}, value);
}

auto to_string(FollowerState const& state) -> std::string_view {
  struct ToStringVisitor {
    auto operator()(FollowerState::UpToDate const&) { return upToDateString; }
    auto operator()(FollowerState::ErrorBackoff const& err) {
      return errorBackoffString;
    }
    auto operator()(FollowerState::RequestInFlight const& rif) {
      return requestInFlightString;
    }
  };

  return std::visit(ToStringVisitor{}, state.value);
}
