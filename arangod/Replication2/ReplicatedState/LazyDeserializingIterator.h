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

#pragma once

#include "Replication2/ReplicatedLog/LogEntryView.h"
#include "Replication2/Streams/Streams.h"

namespace arangodb::replication2 {

template<typename To, typename Deserializer>
struct LazyDeserializingIterator
    : TypedLogRangeIterator<streams::StreamEntryView<To>> {
  // TODO Take `From` as a template parameter, and add some sensible way of
  //      deserializing it generically.
  using From = LogEntryView;
  explicit LazyDeserializingIterator(
      std::unique_ptr<TypedLogRangeIterator<From>> iterator)
      : _iterator(std::move(iterator)) {}

  auto next() -> std::optional<streams::StreamEntryView<To>> override {
    if (auto&& current = _iterator->next(); current.has_value()) {
      auto slice = current->logPayload();
      auto value = std::invoke(
          Deserializer{}, streams::serializer_tag<std::decay_t<To>>, slice);
      _current.emplace(std::move(value));
      return {{current->logIndex(), std::cref(*_current)}};
    } else {
      _current.reset();
      return std::nullopt;
    }
  }

  auto range() const noexcept -> LogRange override {
    return _iterator->range();
  }

 private:
  std::unique_ptr<TypedLogRangeIterator<From>> _iterator;
  std::optional<std::remove_reference_t<To>> _current;
};

}  // namespace arangodb::replication2
