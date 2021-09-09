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
#include <immer/flex_vector_transient.hpp>
#if (_MSC_VER >= 1)
#pragma warning(pop)
#endif

#include "Replication2/ReplicatedLog/LogCommon.h"
#include "Replication2/ReplicatedLog/PersistedLog.h"

namespace arangodb::replication2::replicated_log {

class ReplicatedLogIterator : public LogRangeIterator {
 public:
  using log_type = ::immer::flex_vector<InMemoryLogEntry, arangodb::immer::arango_memory_policy>;

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
      return {_container.front().entry().logIndex(), _container.back().entry().logIndex() + 1};
    }
  }

 private:
  log_type _container;
  log_type::const_iterator _begin;
  log_type::const_iterator _end;
};

class InMemoryPersistedLogIterator : public PersistedLogIterator {
 public:
  using log_type = ::immer::flex_vector<InMemoryLogEntry, arangodb::immer::arango_memory_policy>;

  explicit InMemoryPersistedLogIterator(log_type container)
      : _container(std::move(container)),
        _begin(_container.begin()),
        _end(_container.end()) {}

  auto next() -> std::optional<PersistingLogEntry> override {
    if (_begin != _end) {
      auto const& it = *_begin;
      ++_begin;
      return it.entry();
    }
    return std::nullopt;
  }

 private:
  log_type _container;
  log_type::const_iterator _begin;
  log_type::const_iterator _end;
};

}  // namespace arangodb::replication2::replicated_log
