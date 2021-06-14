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

#include <Basics/debugging.h>

#if (_MSC_VER >= 1)
// suppress warnings:
#pragma warning(push)
// conversion from 'size_t' to 'immer::detail::rbts::count_t', possible loss of data
#pragma warning(disable : 4267)
// result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#pragma warning(disable : 4334)
#endif
#include <immer/flex_vector.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

using namespace arangodb;
using namespace arangodb::replication2;

auto replicated_log::InMemoryLog::getLastIndex() const noexcept -> LogIndex {
    return LogIndex{_log.size()};
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

auto replicated_log::InMemoryLog::splice(LogIndex from, LogIndex to) const
    -> immer::flex_vector<LogEntry> {
  from = LogIndex{std::max<decltype(from.value)>(from.value, 1)};
  auto res = _log.take(to.value - 1).drop(from.value - 1);
  TRI_ASSERT(res.size() == to.value - from.value);
  return res;
}

auto replicated_log::InMemoryLog::getFirstIndexOfTerm(LogTerm term) const noexcept -> std::optional<LogIndex> {
  auto it = std::lower_bound(_log.begin(), _log.end(), term, [](auto const& entry, auto const& term) {
    return term < entry.logTerm();
  });

  if (it != _log.end() && it->logTerm() == term) {
    return it->logIndex();
  } else {
    return std::nullopt;
  }
}
auto replicated_log::InMemoryLog::getLastIndexOfTerm(LogTerm term) const noexcept -> std::optional<LogIndex> {
  // Note that we're using reverse iterators
  auto it = std::lower_bound(_log.rbegin(), _log.rend(), term, [](auto const& entry, auto const& term) {
    // Note that this is flipped
    return entry.logTerm() < term;
  });

  if (it != _log.rend() && it->logTerm() == term) {
    return it->logIndex();
  } else {
    return std::nullopt;
  }
}
