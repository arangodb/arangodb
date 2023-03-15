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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#include "TermIndexMapping.h"
#include "LogCommon.h"
#include "Basics/debugging.h"

using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;

auto TermIndexMapping::getTermRange(LogTerm t) const noexcept
    -> std::optional<LogRange> {
  if (auto iter = _mapping.find(t); iter != _mapping.end()) {
    return iter->second;
  }
  return std::nullopt;
}

auto TermIndexMapping::getFirstIndexOfTerm(LogTerm term) const noexcept
    -> std::optional<LogIndex> {
  if (auto range = getTermRange(term); range.has_value()) {
    return range->from;
  }
  return std::nullopt;
}

auto operator+(LogTerm t, std::uint64_t delta) -> LogTerm {
  return LogTerm{t.value + delta};
}

void TermIndexMapping::insert(LogRange range, LogTerm term) noexcept {
  if (_mapping.empty()) {
    _mapping.emplace(term, range);
  } else {
    auto [prevTerm, prevRange] = *_mapping.rbegin();
    ADB_PROD_ASSERT(prevRange.to == range.from);
    ADB_PROD_ASSERT(prevTerm <= term);
    if (prevTerm == term) {
      _mapping.rbegin()->second.to = range.to;
    } else {
      _mapping.emplace_hint(_mapping.end(), term, range);
    }
  }
}

void TermIndexMapping::removeFront(LogIndex stop) noexcept {
  auto keep = std::find_if(_mapping.begin(), _mapping.end(), [&](auto pair) {
    return pair.second.contains(stop);
  });

  if (keep != _mapping.end()) {
    keep->second.from = stop;
  }

  _mapping.erase(_mapping.begin(), keep);
}

void TermIndexMapping::removeBack(LogIndex start) noexcept {
  auto keep = std::find_if(_mapping.rbegin(), _mapping.rend(),
                           [&](auto pair) { return pair.second.from < start; });

  ADB_PROD_ASSERT(_mapping.rbegin().base() == _mapping.end());

  if (keep != _mapping.rend()) {
    keep->second.to = start;
  }

  _mapping.erase(keep.base(), _mapping.end());
}

auto TermIndexMapping::getTermOfIndex(LogIndex idx) const noexcept
    -> std::optional<LogTerm> {
  auto term = std::find_if(_mapping.begin(), _mapping.end(), [&](auto pair) {
    return pair.second.contains(idx);
  });

  if (term != _mapping.end()) {
    return term->first;
  }
  return std::nullopt;
}

auto TermIndexMapping::getLastIndex() const noexcept
    -> std::optional<TermIndexPair> {
  if (not _mapping.empty()) {
    auto [term, range] = *_mapping.rbegin();
    return TermIndexPair{term, range.to.saturatedDecrement()};
  }
  return std::nullopt;
}

auto TermIndexMapping::getFirstIndex() const noexcept
    -> std::optional<TermIndexPair> {
  if (not _mapping.empty()) {
    auto [term, range] = *_mapping.begin();
    return TermIndexPair{term, range.from};
  }
  return std::nullopt;
}

void TermIndexMapping::insert(LogIndex idx, LogTerm term) noexcept {
  auto iter = std::invoke([&] {
    if (_mapping.empty()) {
      return _mapping.emplace(term, LogRange{idx, idx}).first;
    } else if (auto iter = _mapping.rbegin(); iter->first != term) {
      ADB_PROD_ASSERT(iter->first < term);
      ADB_PROD_ASSERT(iter->second.to == idx);

      return _mapping.emplace(term, LogRange{idx, idx}).first;
    } else {
      return std::prev(_mapping.end());
    }
  });

  ADB_PROD_ASSERT(iter->second.to == idx);
  iter->second.to = idx + 1;
}

void TermIndexMapping::append(TermIndexMapping const& other) noexcept {
  // TODO optimize
  for (auto [term, range] : other._mapping) {
    insert(range, term);
  }
}
