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
/// @author Lars Maier
////////////////////////////////////////////////////////////////////////////////

#pragma once
#include "Replication2/Streams/StreamInformationBlock.h"
#include "Replication2/Streams/Streams.h"

namespace arangodb::replication2::streams {

template <StreamId Id, typename Type, typename Tags>
auto StreamInformationBlock<stream_descriptor<Id, Type, Tags>>::getTransientContainer()
    -> TransientType& {
  if (!std::holds_alternative<TransientType>(_container)) {
    _container = std::get<ContainerType>(_container).transient();
  }
  return std::get<TransientType>(_container);
}

template <StreamId Id, typename Type, typename Tags>
auto StreamInformationBlock<stream_descriptor<Id, Type, Tags>>::getPersistentContainer()
    -> ContainerType& {
  if (!std::holds_alternative<ContainerType>(_container)) {
    _container = std::get<TransientType>(_container).persistent();
  }
  return std::get<ContainerType>(_container);
}

template <StreamId Id, typename Type, typename Tags>
auto StreamInformationBlock<stream_descriptor<Id, Type, Tags>>::appendEntry(LogIndex index,
                                                                            Type t) {
  getTransientContainer().push_back(EntryType{index, std::move(t)});
}

template <StreamId Id, typename Type, typename Tags>
auto StreamInformationBlock<stream_descriptor<Id, Type, Tags>>::getWaitForResolveSet(LogIndex commitIndex)
    -> std::multimap<LogIndex, futures::Promise<WaitForResult>> {
  WaitForQueue toBeResolved;
  auto const end = _waitForQueue.upper_bound(commitIndex);
  for (auto it = _waitForQueue.begin(); it != end;) {
    toBeResolved.insert(_waitForQueue.extract(it++));
  }
  return toBeResolved;
}

template <StreamId Id, typename Type, typename Tags>
auto StreamInformationBlock<stream_descriptor<Id, Type, Tags>>::registerWaitFor(LogIndex index)
    -> futures::Future<WaitForResult> {
  return _waitForQueue.emplace(index, futures::Promise<WaitForResult>{})->second.getFuture();
}

template <StreamId Id, typename Type, typename Tags>
auto StreamInformationBlock<stream_descriptor<Id, Type, Tags>>::getIterator()
    -> std::unique_ptr<Iterator> {
  auto log = getPersistentContainer();

  struct Iterator : TypedLogRangeIterator<StreamEntryView<Type>> {
    ContainerType log;
    typename ContainerType::iterator current;

    auto next() -> std::optional<StreamEntryView<Type>> override {
      if (current != std::end(log)) {
        auto view = std::make_pair(current->first, std::cref(current->second));
        ++current;
        return view;
      }
      return std::nullopt;
    }

    [[nodiscard]] auto range() const noexcept -> std::pair<LogIndex, LogIndex> override {
      abort();
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }

    explicit Iterator(ContainerType log)
        : log(std::move(log)), current(this->log.begin()) {}
  };

  return std::make_unique<Iterator>(std::move(log));
}

template <StreamId Id, typename Type, typename Tags>
auto StreamInformationBlock<stream_descriptor<Id, Type, Tags>>::getIteratorRange(LogIndex start, LogIndex stop)
    -> std::unique_ptr<Iterator> {
  TRI_ASSERT(stop >= start);

  auto const log = getPersistentContainer();

  using ContainerIterator = typename ContainerType::iterator;

  struct Iterator : TypedLogRangeIterator<StreamEntryView<Type>> {
    ContainerType _log;
    ContainerIterator current;
    LogIndex start, stop;

    auto next() -> std::optional<StreamEntryView<Type>> override {
      if (current != std::end(_log) && current->first < stop) {
        auto view = std::make_pair(current->first, std::cref(current->second));
        ++current;
        return view;
      }
      return std::nullopt;
    }
    [[nodiscard]] auto range() const noexcept -> std::pair<LogIndex, LogIndex> override {
      return {start, stop};
    }

    explicit Iterator(ContainerType log, LogIndex start, LogIndex stop)
        : _log(std::move(log)),
          current(std::lower_bound(std::begin(_log), std::end(_log), start,
                                   [](StreamEntry<Type> const& left, LogIndex index) {
                                     return left.first < index;
                                   })),
          start(start),
          stop(stop) {}
  };
  return std::make_unique<Iterator>(std::move(log), start, stop);
}

}  // namespace arangodb::replication2::streams
