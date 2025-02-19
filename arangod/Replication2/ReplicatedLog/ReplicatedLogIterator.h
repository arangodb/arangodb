////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
#include <optional>

#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/InMemoryLogEntry.h"
#include "Replication2/ReplicatedLog/LogEntryView.h"

namespace arangodb::replication2::replicated_log {

class ReplicatedLogIterator : public LogViewRangeIterator {
 public:
  using log_type = ::immer::flex_vector<InMemoryLogEntry,
                                        arangodb::immer::arango_memory_policy>;

  explicit ReplicatedLogIterator(log_type container)
      : _container(std::move(container)),
        _begin(_container.begin()),
        _end(_container.end()) {}

  auto next() -> std::optional<LogEntryView> override {
    while (_begin != _end) {
      auto const& it = *_begin;
      ++_begin;
      auto const& entry = it.entry();
      if (entry.logPayload()) {
        return LogEntryView(entry.logIndex(), *entry.logPayload());
      }
    }
    return std::nullopt;
  }

  auto range() const noexcept -> LogRange override {
    if (_container.empty()) {
      return {LogIndex{0}, LogIndex{0}};
    } else {
      return {_container.front().entry().logIndex(),
              _container.back().entry().logIndex() + 1};
    }
  }

 private:
  log_type _container;
  log_type::const_iterator _begin;
  log_type::const_iterator _end;
};

struct InMemoryLogIteratorImpl : InMemoryLogIterator {
  using log_type = ::immer::flex_vector<InMemoryLogEntry,
                                        arangodb::immer::arango_memory_policy>;

  explicit InMemoryLogIteratorImpl(log_type container)
      : _container(std::move(container)),
        _begin(_container.begin()),
        _end(_container.end()) {}

  auto next() -> std::optional<InMemoryLogEntry> override {
    if (_begin != _end) {
      auto const& it = *_begin;
      ++_begin;
      return it;
    }
    return std::nullopt;
  }

 private:
  log_type _container;
  log_type::const_iterator _begin;
  log_type::const_iterator _end;
};

}  // namespace arangodb::replication2::replicated_log
