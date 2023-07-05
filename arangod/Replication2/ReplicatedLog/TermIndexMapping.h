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

#pragma once

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/Storage/IteratorPosition.h"

#include <optional>
#include <map>

namespace arangodb::replication2 {

struct LogTerm;
struct LogRange;
struct LogIndex;
struct TermIndexPair;

namespace replicated_log {

struct TermIndexMapping {
  auto getFirstIndexOfTerm(LogTerm term) const noexcept
      -> std::optional<LogIndex>;
  auto getTermRange(LogTerm) const noexcept -> std::optional<LogRange>;
  auto getIndexRange() const noexcept -> LogRange;
  auto getTermOfIndex(LogIndex) const noexcept -> std::optional<LogTerm>;
  auto getLastIndex() const noexcept -> std::optional<TermIndexPair>;
  auto getFirstIndex() const noexcept -> std::optional<TermIndexPair>;
  auto empty() const noexcept -> bool;

  void removeFront(LogIndex stop) noexcept;
  void removeBack(LogIndex start) noexcept;
  void insert(LogRange, LogTerm) noexcept;
  void insert(LogIndex, LogTerm) noexcept;

  void append(TermIndexMapping const&) noexcept;

 private:
  struct TermInfo {
    LogRange range;
    storage::IteratorPosition startPosition;
  };
  using ContainerType = std::map<LogTerm, TermInfo>;
  ContainerType _mapping;
};

}  // namespace replicated_log
}  // namespace arangodb::replication2
