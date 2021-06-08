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

#pragma once

#include "Replication2/ReplicatedLog/Common.h"

#include <optional>

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

namespace arangodb::replication2::replicated_log {

/**
 * @brief The ephemeral part of the replicated log held in memory. Can hold more
 * recent entries than the corresponding persisted log, while the latter is
 * catching up. On startup (or currently, on creation of a leader or follower
 * instance), this is restored from the persisted log.
 */
struct InMemoryLog {
  [[nodiscard]] auto getLastIndex() const noexcept -> LogIndex;
  [[nodiscard]] auto getLastTerm() const noexcept -> LogTerm;
  [[nodiscard]] auto getNextIndex() const noexcept -> LogIndex;
  [[nodiscard]] auto getEntryByIndex(LogIndex idx) const noexcept -> std::optional<LogEntry>;
  [[nodiscard]] auto splice(LogIndex from, LogIndex to) const -> immer::flex_vector<LogEntry>;

  immer::flex_vector<LogEntry> _log{};
};

}  // namespace arangodb::replication2::replicated_log
