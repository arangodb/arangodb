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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "InMemoryLog.h"

#include "Replication2/LoggerContext.h"
#include "Replication2/ReplicatedLog/ReplicatedLogIterator.h"
#include "Replication2/ReplicatedLog/TermIndexMapping.h"
#include "Replication2/Storage/IStorageEngineMethods.h"

#include <Basics/Exceptions.h>
#include <Basics/StringUtils.h>
#include <Basics/application-exit.h>
#include <Basics/debugging.h>
#include <Containers/ImmerMemoryPolicy.h>

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
#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

using namespace arangodb;
using namespace arangodb::replication2;

auto replicated_log::InMemoryLog::getLastIndex() const noexcept -> LogIndex {
  return getLastTermIndexPair().index;
}

auto replicated_log::InMemoryLog::getLastTermIndexPair() const noexcept
    -> TermIndexPair {
  if (_log.empty()) {
    return {};
  }
  return _log.back().entry().logTermIndexPair();
}

auto replicated_log::InMemoryLog::getLastTerm() const noexcept -> LogTerm {
  return getLastTermIndexPair().term;
}

auto replicated_log::InMemoryLog::getNextIndex() const noexcept -> LogIndex {
  return _first + _log.size();
}

auto replicated_log::InMemoryLog::getEntryByIndex(
    LogIndex const idx) const noexcept -> std::optional<InMemoryLogEntry> {
  if (_first + _log.size() <= idx || idx < _first) {
    return std::nullopt;
  }

  auto const& e = _log.at(idx.value - _first.value);
  ADB_PROD_ASSERT(e.entry().logIndex() == idx)
      << "idx = " << idx << ", entry = " << e.entry().logIndex();
  return e;
}

auto replicated_log::InMemoryLog::slice(LogIndex from, LogIndex to) const
    -> log_type {
  from = std::max(from, _first);
  to = std::max(to, _first);
  ADB_PROD_ASSERT(from <= to)
      << "from = " << from << ", to = " << to << ", _first = " << _first;
  auto res = _log.take(to.value - _first.value).drop(from.value - _first.value);
  ADB_PROD_ASSERT(res.size() <= to.value - from.value)
      << "res.size() = " << res.size() << ", to = " << to.value
      << ", from = " << from.value << ", first = " << _first;
  return res;
}

auto replicated_log::InMemoryLog::getFirstIndexOfTerm(
    LogTerm term) const noexcept -> std::optional<LogIndex> {
  auto it = std::lower_bound(_log.begin(), _log.end(), term,
                             [](auto const& entry, auto const& term) {
                               return term > entry.entry().logTerm();
                             });

  if (it != _log.end() && it->entry().logTerm() == term) {
    return it->entry().logIndex();
  } else {
    return std::nullopt;
  }
}

auto replicated_log::InMemoryLog::getLastIndexOfTerm(
    LogTerm term) const noexcept -> std::optional<LogIndex> {
  // Note that we're using reverse iterators
  auto it = std::lower_bound(_log.rbegin(), _log.rend(), term,
                             [](auto const& entry, auto const& term) {
                               // Note that this is flipped
                               return entry.entry().logTerm() > term;
                             });

  if (it != _log.rend() && it->entry().logTerm() == term) {
    return it->entry().logIndex();
  } else {
    return std::nullopt;
  }
}

replicated_log::InMemoryLog::InMemoryLog(log_type log)
    : _log(std::move(log)),
      _first(_log.empty() ? LogIndex{1} : _log.front().entry().logIndex()) {}

replicated_log::InMemoryLog::InMemoryLog(log_type log, LogIndex first)
    : _log(std::move(log)), _first(first) {
  TRI_ASSERT(_log.empty() || first == _log.front().entry().logIndex())
      << " log.empty = " << std::boolalpha << _log.empty()
      << " first = " << first << " log.front.idx = "
      << (!_log.empty() ? _log.front().entry().logIndex().value : 0);
}

#if (_MSC_VER >= 1)
// suppress false positive warning:
#pragma warning(push)
// function assumed not to throw an exception but does
#pragma warning(disable : 4297)
#endif
replicated_log::InMemoryLog::InMemoryLog(
    replicated_log::InMemoryLog&& other) noexcept try
    : _log(std::move(other._log)), _first(other._first) {
  other._first = LogIndex{1};
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
  FATAL_ERROR_ABORT();
} catch (...) {
  LOG_TOPIC("9771c", FATAL, Logger::REPLICATION2)
      << "Caught an exception when moving an InMemoryLog. This is fatal, as "
         "consistency of persistent and in-memory state can no longer be "
         "guaranteed. The process will terminate now.";
  FATAL_ERROR_ABORT();
}
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

auto replicated_log::InMemoryLog::operator=(
    replicated_log::InMemoryLog&& other) noexcept
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
  _first = other._first;
  other._first = LogIndex{1};
  return *this;
} catch (std::exception const& ex) {
  LOG_TOPIC("bf5c5", FATAL, Logger::REPLICATION2)
      << "Caught an exception when moving an InMemoryLog. This is fatal, as "
         "consistency of persistent and in-memory state can no longer be "
         "guaranteed. The process will terminate now. The exception was: "
      << ex.what();
  FATAL_ERROR_ABORT();
} catch (...) {
  LOG_TOPIC("2c084", FATAL, Logger::REPLICATION2)
      << "Caught an exception when moving an InMemoryLog. This is fatal, as "
         "consistency of persistent and in-memory state can no longer be "
         "guaranteed. The process will terminate now.";
  FATAL_ERROR_ABORT();
}

auto replicated_log::InMemoryLog::getIteratorFrom(LogIndex fromIdx) const
    -> std::unique_ptr<LogViewIterator> {
  return getRangeIteratorFrom(fromIdx);
}

auto replicated_log::InMemoryLog::getRangeIteratorFrom(LogIndex fromIdx) const
    -> std::unique_ptr<LogViewRangeIterator> {
  // if we want to have read from log entry 1 onwards, we have to drop
  // no entries, because log entry 0 does not exist.
  auto log = _log.drop(fromIdx.saturatedDecrement(_first.value).value);
  return std::make_unique<ReplicatedLogIterator>(std::move(log));
}

auto replicated_log::InMemoryLog::getMemtryIteratorFrom(LogIndex fromIdx) const
    -> std::unique_ptr<InMemoryLogIterator> {
  // if we want to have read from log entry 1 onwards, we have to drop
  // no entries, because log entry 0 does not exist.
  auto log = _log.drop(fromIdx.saturatedDecrement(_first.value).value);
  return std::make_unique<InMemoryLogIteratorImpl>(std::move(log));
}

auto replicated_log::InMemoryLog::getMemtryIteratorRange(LogIndex fromIdx,
                                                         LogIndex toIdx) const
    -> std::unique_ptr<InMemoryLogIterator> {
  auto log = _log.take(toIdx.saturatedDecrement(_first.value).value)
                 .drop(fromIdx.saturatedDecrement(_first.value).value);
  return std::make_unique<InMemoryLogIteratorImpl>(std::move(log));
}

auto replicated_log::InMemoryLog::getMemtryIteratorRange(LogRange range) const
    -> std::unique_ptr<InMemoryLogIterator> {
  return getMemtryIteratorRange(range.from, range.to);
}

auto replicated_log::InMemoryLog::getLogIterator() const
    -> std::unique_ptr<LogIterator> {
  struct Iterator : LogIterator {
    explicit Iterator(log_type container) : _iter(std::move(container)) {}

    auto next() -> std::optional<LogEntry> override {
      auto e = _iter.next();
      if (e) {
        return e->entry();
      }
      return std::nullopt;
    }

   private:
    InMemoryLogIteratorImpl _iter;
  };

  return std::make_unique<Iterator>(_log);
}

auto replicated_log::InMemoryLog::getIteratorRange(LogIndex fromIdx,
                                                   LogIndex toIdx) const
    -> std::unique_ptr<LogViewRangeIterator> {
  auto log = _log.take(toIdx.saturatedDecrement(_first.value).value)
                 .drop(fromIdx.saturatedDecrement(_first.value).value);
  return std::make_unique<ReplicatedLogIterator>(std::move(log));
}

void replicated_log::InMemoryLog::appendInPlace(LoggerContext const& logContext,
                                                InMemoryLogEntry entry) {
  if (getNextIndex() != entry.entry().logIndex()) {
    using namespace basics::StringUtils;
    auto message = concatT(
        "Trying to append a log entry with "
        "mismatching log index. Last log index is ",
        getLastIndex(), ", but the new entry has ", entry.entry().logIndex());
    LOG_CTX("e2775", ERR, logContext) << message;
    basics::abortOrThrow(TRI_ERROR_INTERNAL, std::move(message), ADB_HERE);
  }
  _log = _log.push_back(std::move(entry));
}

auto replicated_log::InMemoryLog::append(InMemoryLog const& entries) const
    -> InMemoryLog {
  ADB_PROD_ASSERT(empty() || entries.empty() ||
                  getNextIndex() == entries.getFirstIndex())
      << std::boolalpha << "entries.empty() = " << entries.empty()
      << ", front = " << entries.getFirstIndex()
      << ", getNextIndex = " << getNextIndex();
  auto transient = _log.transient();
  transient.append(entries._log.transient());
  return InMemoryLog{std::move(transient).persistent()};
}

auto replicated_log::InMemoryLog::append(log_type entries) const
    -> InMemoryLog {
  ADB_PROD_ASSERT(entries.empty() ||
                  getNextIndex() == entries.front().entry().logIndex())
      << std::boolalpha << "entries.empty() = " << entries.empty()
      << ", front = " << entries.front().entry().logIndex()
      << ", getNextIndex = " << getNextIndex();
  auto transient = _log.transient();
  transient.append(std::move(entries).transient());
  return InMemoryLog{std::move(transient).persistent(), _first};
}

auto replicated_log::InMemoryLog::append(
    log_type_persisted const& entries) const -> InMemoryLog {
  ADB_PROD_ASSERT(entries.empty() ||
                  getNextIndex() == entries.front().logIndex())
      << std::boolalpha << "entries.empty() = " << entries.empty()
      << ", front = " << entries.front().logIndex()
      << ", getNextIndex = " << getNextIndex();
  auto transient = _log.transient();
  for (auto const& entry : entries) {
    transient.push_back(InMemoryLogEntry(entry));
  }
  return InMemoryLog{std::move(transient).persistent(), _first};
}

auto replicated_log::InMemoryLog::takeSnapshotUpToAndIncluding(
    LogIndex until) const -> InMemoryLog {
  return removeBack(until + 1);
}

auto replicated_log::InMemoryLog::removeBack(LogIndex start) const
    -> InMemoryLog {
  return InMemoryLog{_log.take(start.saturatedDecrement(_first.value).value)};
}

auto replicated_log::InMemoryLog::removeFront(LogIndex stop) const
    -> replicated_log::InMemoryLog {
  return InMemoryLog{_log.drop(stop.saturatedDecrement(_first.value).value)};
}

auto replicated_log::InMemoryLog::release(LogIndex stop) const
    -> replicated_log::InMemoryLog {
  return removeFront(stop);
}

auto replicated_log::InMemoryLog::copyFlexVector() const -> log_type {
  return _log;
}

auto replicated_log::InMemoryLog::computeTermIndexMap() const
    -> replicated_log::TermIndexMapping {
  // TODO this is a quick way to implement this.
  // However, in the future we might track this information in the InMemoryLog
  // itself and update it as we go.
  TermIndexMapping mapping;
  for (auto const& ent : _log) {
    mapping.insert(ent.entry().logIndex(), ent.entry().logTerm());
  }
  return mapping;
}

auto replicated_log::InMemoryLog::back() const noexcept
    -> log_type::const_reference {
  return _log.back();
}

auto replicated_log::InMemoryLog::empty() const noexcept -> bool {
  return _log.empty();
}

auto replicated_log::InMemoryLog::getLastEntry() const noexcept
    -> std::optional<InMemoryLogEntry> {
  if (_log.empty()) {
    return std::nullopt;
  }
  return _log.back();
}

auto replicated_log::InMemoryLog::getFirstEntry() const noexcept
    -> std::optional<InMemoryLogEntry> {
  if (_log.empty()) {
    return std::nullopt;
  }
  return _log.front();
}

auto replicated_log::InMemoryLog::dump(
    replicated_log::InMemoryLog::log_type const& log) -> std::string {
  auto builder = velocypack::Builder();
  auto stream = std::stringstream();
  stream << "[";
  bool first = true;
  for (auto const& it : log) {
    if (first) {
      first = false;
    } else {
      stream << ", ";
    }
    it.entry().toVelocyPack(builder);
    stream << builder.toJson();
    builder.clear();
  }
  stream << "]";

  return stream.str();
}

auto replicated_log::InMemoryLog::dump() const -> std::string {
  return dump(_log);
}

auto replicated_log::InMemoryLog::getIndexRange() const noexcept -> LogRange {
  return {_first, _first + _log.size()};
}

auto replicated_log::InMemoryLog::getFirstIndex() const noexcept -> LogIndex {
  return _first;
}

auto replicated_log::InMemoryLog::getIteratorRange(LogRange bounds) const
    -> std::unique_ptr<LogViewRangeIterator> {
  return getIteratorRange(bounds.from, bounds.to);
}

replicated_log::InMemoryLog::InMemoryLog(LogIndex first) : _first(first) {}
