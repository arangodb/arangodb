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

#include "InMemoryLog.h"

#include "Replication2/LogContext.h"
#include "Replication2/ReplicatedLog/LogCore.h"
#include "Replication2/ReplicatedLog/ReplicatedLogIterator.h"

#include <Basics/Exceptions.h>
#include <Basics/StringUtils.h>
#include <Basics/debugging.h>
#include <Containers/ImmerMemoryPolicy.h>

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

using namespace arangodb;
using namespace arangodb::replication2;

auto replicated_log::InMemoryLog::getLastIndex() const noexcept -> LogIndex {
  return LogIndex{_log.size()};
}

auto replicated_log::InMemoryLog::getLastTermIndexPair() const noexcept -> TermIndexPair{
  if (_log.empty()) {
    return {};
  }
  return _log.back().logTermIndexPair();
}

auto replicated_log::InMemoryLog::getLastTerm() const noexcept -> LogTerm {
  if (_log.empty()) {
    return LogTerm{0};
  }
  return _log.back().logTerm();
}

auto replicated_log::InMemoryLog::getNextIndex() const noexcept -> LogIndex {
  return LogIndex{_log.size() + 1};
}

auto replicated_log::InMemoryLog::getEntryByIndex(LogIndex const idx) const noexcept
    -> std::optional<LogEntry> {
  if (_log.size() < idx.value || idx.value == 0) {
    return std::nullopt;
  }

  auto const& e = _log.at(idx.value - 1);
  TRI_ASSERT(e.logIndex() == idx);
  return e;
}

auto replicated_log::InMemoryLog::splice(LogIndex from, LogIndex to) const -> log_type {
  from = LogIndex{std::max<decltype(from.value)>(from.value, 1)};
  auto res = _log.take(to.value - 1).drop(from.value - 1);
  TRI_ASSERT(res.size() == to.value - from.value);
  return res;
}

auto replicated_log::InMemoryLog::getFirstIndexOfTerm(LogTerm term) const noexcept -> std::optional<LogIndex> {
  auto it = std::lower_bound(_log.begin(), _log.end(), term,
                             [](auto const& entry, auto const& term) {
                               return term > entry.logTerm();
                             });

  if (it != _log.end() && it->logTerm() == term) {
    return it->logIndex();
  } else {
    return std::nullopt;
  }
}

auto replicated_log::InMemoryLog::getLastIndexOfTerm(LogTerm term) const noexcept
    -> std::optional<LogIndex> {
  // Note that we're using reverse iterators
  auto it = std::lower_bound(_log.rbegin(), _log.rend(), term,
                             [](auto const& entry, auto const& term) {
                               // Note that this is flipped
                               return entry.logTerm() > term;
                             });

  if (it != _log.rend() && it->logTerm() == term) {
    return it->logIndex();
  } else {
    return std::nullopt;
  }
}

replicated_log::InMemoryLog::InMemoryLog(LogContext const& logContext,
                                         replicated_log::LogCore const& logCore) {
  // TODO this is a cheap trick for now. Later we should be aware of the fact
  //      that the log might not start at 1.
  auto iter = logCore.read(LogIndex{0});
  auto log = _log.transient();
  while (auto entry = iter->next()) {
    log.push_back(std::move(entry).value());
  }
  _log = std::move(log).persistent();
}

replicated_log::InMemoryLog::InMemoryLog(log_type log) : _log(std::move(log)) {}

replicated_log::InMemoryLog::InMemoryLog(replicated_log::InMemoryLog&& other) noexcept try
    : _log(std::move(other._log)) {
  // Note that immer::flex_vector is currently not nothrow move-assignable,
  // though it probably does not throw any exceptions. However, we *need* this
  // to be noexcept, otherwise we cannot keep the persistent and in-memory state
  // in sync.
  //
  // So even if flex_vector could actually throw here, we deliberately add the
  // noexcept!
  //
  // The try/catch is *only* for logging, but *must* terminate (e.g. by
  // rethrowing) the process if an exception is caught.
} catch (std::exception const& ex) {
  LOG_TOPIC("33563", FATAL, Logger::REPLICATION2)
      << "Caught an exception when moving an InMemoryLog. This is fatal, as "
         "consistency of persistent and in-memory state can no longer be "
         "guaranteed. The process will terminate now. The exception was: "
      << ex.what();
  throw;
} catch (...) {
  LOG_TOPIC("9771c", FATAL, Logger::REPLICATION2)
      << "Caught an exception when moving an InMemoryLog. This is fatal, as "
         "consistency of persistent and in-memory state can no longer be "
         "guaranteed. The process will terminate now.";
  throw;
}

auto replicated_log::InMemoryLog::operator=(replicated_log::InMemoryLog&& other) noexcept
    -> replicated_log::InMemoryLog& try {
  // Note that immer::flex_vector is currently not nothrow move-assignable,
  // though it probably does not throw any exceptions. However, we *need* this
  // to be noexcept, otherwise we cannot keep the persistent and in-memory state
  // in sync.
  //
  // So even if flex_vector could actually throw here, we deliberately add the
  // noexcept!
  //
  // The try/catch is *only* for logging, but *must* terminate (e.g. by
  // rethrowing) the process if an exception is caught.
  _log = std::move(other._log);
  return *this;
} catch (std::exception const& ex) {
  LOG_TOPIC("bf5c5", FATAL, Logger::REPLICATION2)
      << "Caught an exception when moving an InMemoryLog. This is fatal, as "
         "consistency of persistent and in-memory state can no longer be "
         "guaranteed. The process will terminate now. The exception was: "
      << ex.what();
  throw;
} catch (...) {
  LOG_TOPIC("2c084", FATAL, Logger::REPLICATION2)
      << "Caught an exception when moving an InMemoryLog. This is fatal, as "
         "consistency of persistent and in-memory state can no longer be "
         "guaranteed. The process will terminate now.";
  throw;
}

auto replicated_log::InMemoryLog::getIteratorFrom(LogIndex fromIdx) const
    -> std::unique_ptr<LogIterator> {
  auto log = _log.drop(fromIdx.value);
  return std::make_unique<ReplicatedLogIterator>(std::move(log));
}

auto replicated_log::InMemoryLog::appendInPlace(LogContext const& logContext,
                                                LogEntry&& entry) -> void {
  if (!_log.empty() && _log.back().logIndex() + 1 != entry.logIndex()) {
    using namespace basics::StringUtils;
    auto message = concatT(
        "Trying to append a log entry with "
        "mismatching log index. Last log index is ",
        _log.back().logIndex(), ", but the new entry has ", entry.logIndex());
    LOG_CTX("e2775", ERR, logContext) << message;
    ASSERT_OR_THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, std::move(message));
  }
  _log = _log.push_back(std::move(entry));
}

auto replicated_log::InMemoryLog::appendInPlace(LogContext const& logContext,
                                                LogEntry const& entry) -> void {
  return appendInPlace(logContext, LogEntry{entry});
}

auto replicated_log::InMemoryLog::append(LogContext const& logContext,
                                         log_type entries) const -> InMemoryLog {
  auto transient = _log.transient();
  transient.append(std::move(entries).transient());
  return InMemoryLog{std::move(transient).persistent()};
}

auto replicated_log::InMemoryLog::takeSnapshotUpToAndIncluding(LogIndex until) const
    -> InMemoryLog {
  return InMemoryLog(_log.take(until.value));
}

auto replicated_log::InMemoryLog::copyFlexVector() const -> log_type {
  return _log;
}

auto replicated_log::InMemoryLog::back() const noexcept -> log_type::const_reference {
  return _log.back();
}

auto replicated_log::InMemoryLog::empty() const noexcept -> bool {
  return _log.empty();
}

auto replicated_log::InMemoryLog::getLastEntry() const noexcept -> std::optional<LogEntry> {
  if (_log.empty()) {
    return std::nullopt;
  }
  return _log.back();
}

auto replicated_log::InMemoryLog::getFirstEntry() const noexcept -> std::optional<LogEntry> {
  if (_log.empty()) {
    return std::nullopt;
  }
  return _log.front();
}
