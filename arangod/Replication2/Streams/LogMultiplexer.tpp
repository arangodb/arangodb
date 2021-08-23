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

#include <map>
#include <memory>
#include <tuple>
#include <type_traits>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>

#include <Basics/Exceptions.h>
#include <Basics/Guarded.h>
#include <Basics/UnshackledMutex.h>
#include <Basics/application-exit.h>

#include "LogMultiplexer.h"

#include <Replication2/ReplicatedLog/LogFollower.h>
#include <Replication2/ReplicatedLog/LogLeader.h>

#include <Replication2/Streams/MultiplexedValues.h>
#include <Replication2/Streams/StreamInformationBlock.h>
#include <Replication2/Streams/Streams.h>

#include <Replication2/Streams/StreamInformationBlock.tpp>
#include <Replication2/Streams/Streams.tpp>

namespace arangodb::replication2::streams {

namespace {
template <typename Queue, typename Result>
auto allUnresolved(std::pair<Queue, Result>& q) {
  return std::all_of(std::begin(q.first), std::end(q.first),
                     [&](auto const& pair) { return !pair.second.isFulfilled(); });
}
template <typename Descriptor, typename Queue, typename Result, typename Block = StreamInformationBlock<Descriptor>>
auto resolvePromiseSet(std::pair<Queue, Result>& q) {
  TRI_ASSERT(allUnresolved(q));
  std::for_each(std::begin(q.first), std::end(q.first), [&](auto& pair) {
    TRI_ASSERT(!pair.second.isFulfilled());
    if (!pair.second.isFulfilled()) {
      pair.second.setValue(q.second);
    }
  });
}

template <typename... Descriptors, typename... Pairs, std::size_t... Idxs>
auto resolvePromiseSets(stream_descriptor_set<Descriptors...>,
                        std::index_sequence<Idxs...>, std::tuple<Pairs...>& pairs) {
  (resolvePromiseSet<Descriptors>(std::get<Idxs>(pairs)), ...);
}

template <typename... Descriptors, typename... Pairs>
auto resolvePromiseSets(stream_descriptor_set<Descriptors...>, std::tuple<Pairs...>& pairs) {
  resolvePromiseSets(stream_descriptor_set<Descriptors...>{},
                     std::index_sequence_for<Descriptors...>{}, pairs);
}
}  // namespace

template <typename Derived, typename Base, typename Spec, template <typename> typename StreamInterface, typename Interface>
struct LogMultiplexerBase : Base, StreamDispatcher<Derived, Spec, StreamInterface> {
  explicit LogMultiplexerBase(std::shared_ptr<Interface> const& interface)
      : _interface(interface) {}

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename E = StreamEntryView<T>>
  auto waitForIteratorInternal(LogIndex first)
      -> futures::Future<std::unique_ptr<TypedLogRangeIterator<E>>> {
    return waitForInternal<StreamDescriptor>(first).thenValue(
        [that = std::static_pointer_cast<Derived>(this->shared_from_this()), first](auto&&) {
          return that->_guardedData.doUnderLock([&](MultiplexerData<Spec>& self) {
            auto& block = std::get<StreamInformationBlock<StreamDescriptor>>(self._blocks);
            return block.getIteratorRange(first, self._firstUncommittedIndex);
          });
        });
  }

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename W = typename Stream<T>::WaitForResult>
  auto waitForInternal(LogIndex index) -> futures::Future<W> {
    return _guardedData.doUnderLock([&](MultiplexerData<Spec>& self) {
      if (self._firstUncommittedIndex > index) {
        return futures::Future<W>{std::in_place};
      }
      auto& block = std::get<StreamInformationBlock<StreamDescriptor>>(self._blocks);
      return block.registerWaitFor(index);
    });
  }

  template <typename StreamDescriptor>
  auto releaseInternal(LogIndex) -> void {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename E = StreamEntryView<T>>
  auto getIteratorInternal() -> std::unique_ptr<TypedLogRangeIterator<E>> {
    using BlockType = StreamInformationBlock<StreamDescriptor>;

    return _guardedData.template doUnderLock([](MultiplexerData<Spec>& self) {
      auto& block = std::get<BlockType>(self._blocks);
      return block.getIterator();
    });
  }

 private:
  template <typename>
  struct MultiplexerData;
  template <typename... Descriptors>
  struct MultiplexerData<stream_descriptor_set<Descriptors...>> {
    std::tuple<StreamInformationBlock<Descriptors>...> _blocks;
    LogIndex _firstUncommittedIndex{1};
    bool _pendingWaitFor{false};

    void digestIterator(LogRangeIterator& iter);

    auto getWaitForResolveSetAll(LogIndex commitIndex);
  };

  Guarded<MultiplexerData<Spec>, basics::UnshackledMutex> _guardedData{};
  std::shared_ptr<Interface> const _interface;

  auto self() -> Derived& { return static_cast<Derived&>(*this); }
};

template <typename Spec, typename Interface>
struct LogDemultiplexerImplementation
    : LogDemultiplexer<Spec>,
      StreamDispatcher<LogDemultiplexerImplementation<Spec, Interface>, Spec, Stream> {
  using SelfClass = LogDemultiplexerImplementation<Spec, Interface>;
  explicit LogDemultiplexerImplementation(std::shared_ptr<Interface> interface)
      : _interface(std::move(interface)) {}

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename E = StreamEntryView<T>>
  auto waitForIteratorInternal(LogIndex first)
      -> futures::Future<std::unique_ptr<TypedLogRangeIterator<E>>>;

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename W = typename Stream<T>::WaitForResult>
  auto waitForInternal(LogIndex index) -> futures::Future<W> {
    return _guardedData.doUnderLock([&](MultiplexerData<Spec>& self) {
      if (self._nextIndex > index) {
        return futures::Future<W>{std::in_place};
      }
      auto& block = std::get<StreamInformationBlock<StreamDescriptor>>(self._blocks);
      return block.registerWaitFor(index);
    });
  }

  template <typename StreamDescriptor>
  auto releaseInternal(LogIndex) -> void {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename E = StreamEntryView<T>>
  auto getIteratorInternal() -> std::unique_ptr<TypedLogRangeIterator<E>>;

  auto digestIterator(LogRangeIterator& iter) -> void override;

  auto listen() -> void override {
    auto nextIndex =
        _guardedData.doUnderLock([](MultiplexerData<Spec>& self) -> std::optional<LogIndex> {
          if (!self._pendingWaitFor) {
            self._pendingWaitFor = true;
            return self._nextIndex;
          }
          return std::nullopt;
        });
    if (nextIndex.has_value()) {
      triggerWaitFor(*nextIndex);
    }
  }

  void triggerWaitFor(LogIndex waitForIndex) {
    _interface->waitForIterator(waitForIndex)
        .thenValue([weak = this->weak_from_this()](std::unique_ptr<LogRangeIterator>&& iter) {
          if (auto locked = weak.lock(); locked) {
            auto that = std::static_pointer_cast<SelfClass>(locked);
            auto [nextIndex, promiseSets] =
                that->_guardedData.doUnderLock([&](MultiplexerData<Spec>& self) {
                  self._nextIndex = iter->range().second;
                  self.digestIterator(*iter);
                  return std::make_tuple(self._nextIndex,
                                         self.getWaitForResolveSetAll(
                                             self._nextIndex.saturatedDecrement()));
                });

            that->triggerWaitFor(nextIndex);
            resolvePromiseSets(Spec{}, promiseSets);
          }
        });
  }

  template <typename>
  struct MultiplexerData;
  template <typename... Descriptors>
  struct MultiplexerData<stream_descriptor_set<Descriptors...>> {
    std::tuple<StreamInformationBlock<Descriptors>...> _blocks;
    LogIndex _nextIndex{1};
    bool _pendingWaitFor{false};

    void digestIterator(LogRangeIterator& iter);

    auto getWaitForResolveSetAll(LogIndex commitIndex);
  };

  Guarded<MultiplexerData<Spec>, basics::UnshackledMutex> _guardedData{};
  std::shared_ptr<Interface> const _interface;
};

template <typename Spec, typename Interface>
template <typename... Descriptors>
auto LogDemultiplexerImplementation<Spec, Interface>::MultiplexerData<
    stream_descriptor_set<Descriptors...>>::getWaitForResolveSetAll(LogIndex commitIndex) {
  return std::make_tuple(std::make_pair(
      std::get<StreamInformationBlock<Descriptors>>(_blocks).getWaitForResolveSet(commitIndex),
      typename StreamInformationBlock<Descriptors>::WaitForResult{})...);
}

template <typename Spec, typename Interface>
template <typename... Descriptors>
void LogDemultiplexerImplementation<Spec, Interface>::MultiplexerData<
    stream_descriptor_set<Descriptors...>>::digestIterator(LogRangeIterator& iter) {
  while (auto memtry = iter.next()) {
    auto muxedValue =
        MultiplexedValues::fromVelocyPack<Descriptors...>(memtry->logPayload());
    std::visit(
        [&](auto&& value) {
          using ValueTag = std::decay_t<decltype(value)>;
          using Descriptor = typename ValueTag::DescriptorType;
          std::get<StreamInformationBlock<Descriptor>>(_blocks).appendEntry(
              memtry->logIndex(), std::move(value.value));
        },
        std::move(muxedValue.variant()));
  }
}

template <typename Spec, typename Interface>
void LogDemultiplexerImplementation<Spec, Interface>::digestIterator(LogRangeIterator& iter) {
  _guardedData.getLockedGuard()->digestIterator(iter);
}

template <typename Spec, typename Interface>
template <typename StreamDescriptor, typename T, typename E>
auto LogDemultiplexerImplementation<Spec, Interface>::getIteratorInternal()
    -> std::unique_ptr<TypedLogRangeIterator<E>> {
  using BlockType = StreamInformationBlock<StreamDescriptor>;

  return _guardedData.template doUnderLock([](MultiplexerData<Spec>& self) {
    auto& block = std::get<BlockType>(self._blocks);
    return block.getIterator();
  });
}

template <typename Spec, typename Interface>
struct LogMultiplexerImplementation
    : LogMultiplexer<Spec>,
      StreamDispatcher<LogMultiplexerImplementation<Spec, Interface>, Spec, ProducerStream> {
  using SelfClass = LogMultiplexerImplementation<Spec, Interface>;

  explicit LogMultiplexerImplementation(std::shared_ptr<Interface> interface)
      : _guarded(*this), _interface(std::move(interface)) {}

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename E = StreamEntryView<T>>
  auto waitForIteratorInternal(LogIndex first)
      -> futures::Future<std::unique_ptr<TypedLogRangeIterator<E>>>;

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename W = typename Stream<T>::WaitForResult>
  auto waitForInternal(LogIndex index) -> futures::Future<W> {
    return _guarded.doUnderLock([&](MultiplexerData<Spec>& self) {
      if (self._commitIndex >= index) {
        return futures::Future<W>{std::in_place};
      }
      auto& block = std::get<StreamInformationBlock<StreamDescriptor>>(self._blocks);
      return block.registerWaitFor(index).thenValue([](auto&& value) {
        return std::forward<decltype(value)>(value);
      });
    });
  }

  template <typename StreamDescriptor>
  auto releaseInternal(LogIndex) -> void {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>>
  auto insertInternal(T const& t) -> LogIndex {
    auto serialized = std::invoke([&] {
      velocypack::UInt8Buffer buffer;
      velocypack::Builder builder(buffer);
      MultiplexedValues::toVelocyPack<StreamDescriptor>(t, builder);
      return buffer;
    });

    // we have to lock before we insert, otherwise we could mess up the order
    // or log entries for this stream
    auto [index, waitForIndex] = _guarded.doUnderLock([&](MultiplexerData<Spec>& self) {
      // First write to replicated log
      auto insertIndex = _interface->insert(LogPayload(std::move(serialized)));
      TRI_ASSERT(insertIndex > self._lastIndex);
      self._lastIndex = insertIndex;

      // Now we insert the value T into the StreamsLog,
      // but it is not yet visible because of the commitIndex
      auto& block = std::get<StreamInformationBlock<StreamDescriptor>>(self._blocks);
      block.appendEntry(insertIndex, t);
      return std::make_pair(insertIndex, self.checkWaitFor());
    });

    if (waitForIndex.has_value()) {
      triggerWaitForIndex(*waitForIndex);
    }
    return index;
  }

  void triggerWaitForIndex(LogIndex waitForIndex) {
    auto f = _interface->waitFor(waitForIndex);
    std::move(f).thenValue([weak = this->weak_from_this()](
                               replicated_log::WaitForResult&& result) noexcept {
      // First lock the shared pointer
      if (auto locked = weak.lock(); locked) {
        auto that = std::static_pointer_cast<SelfClass>(locked);
        // now acquire the mutex
        auto [resolveSets, nextIndex] =
            that->_guarded.doUnderLock([&](MultiplexerData<Spec>& self) {
              self.pendingWaitFor = false;

              // find out what the commit index is
              self._commitIndex = result.commitIndex;
              return std::make_pair(self.getWaitForResolveSetAll(result.commitIndex),
                                    self.checkWaitFor());
            });

        resolvePromiseSets(Spec{}, resolveSets);
        if (nextIndex.has_value()) {
          that->triggerWaitForIndex(*nextIndex);
        }
      }
    });
  }

  template <typename>
  struct MultiplexerData;
  template <typename... Descriptors>
  struct MultiplexerData<stream_descriptor_set<Descriptors...>> {
    explicit MultiplexerData(SelfClass& self) : _self(self) {}

    SelfClass& _self;
    std::tuple<StreamInformationBlock<Descriptors>...> _blocks;
    bool pendingWaitFor{false};
    LogIndex _lastIndex;
    LogIndex _commitIndex;

    // returns a LogIndex to wait for (if necessary)
    auto checkWaitFor() -> std::optional<LogIndex> {
      if (!pendingWaitFor && _lastIndex != _commitIndex) {
        // we have to trigger a waitFor operation
        // and wait for the next index
        TRI_ASSERT(_lastIndex > _commitIndex);
        auto waitForIndex = _commitIndex + 1;
        pendingWaitFor = true;
        return waitForIndex;
      }
      return std::nullopt;
    }

    auto getWaitForResolveSetAll(LogIndex commitIndex) {
      return std::make_tuple(std::make_pair(
          std::get<StreamInformationBlock<Descriptors>>(_blocks).getWaitForResolveSet(commitIndex),
          typename StreamInformationBlock<Descriptors>::WaitForResult{})...);
    };
  };

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename E = StreamEntryView<T>>
  auto getIteratorInternal() -> std::unique_ptr<TypedLogRangeIterator<E>> {
    using BlockType = StreamInformationBlock<StreamDescriptor>;

    return _guarded.template doUnderLock([](MultiplexerData<Spec>& self) {
      auto& block = std::get<BlockType>(self._blocks);
      return block.getIterator();
    });
  }

  Guarded<MultiplexerData<Spec>, basics::UnshackledMutex> _guarded;
  std::shared_ptr<Interface> const _interface;
};

template <typename Spec, typename Interface>
template <typename StreamDescriptor, typename T, typename E>
auto LogDemultiplexerImplementation<Spec, Interface>::waitForIteratorInternal(LogIndex first)
    -> futures::Future<std::unique_ptr<TypedLogRangeIterator<E>>> {
  return waitForInternal<StreamDescriptor>(first).thenValue(
      [that = std::static_pointer_cast<SelfClass>(this->shared_from_this()), first](auto&&) {
        return that->_guardedData.doUnderLock([&](MultiplexerData<Spec>& self) {
          auto& block = std::get<StreamInformationBlock<StreamDescriptor>>(self._blocks);
          return block.getIteratorRange(first, self._nextIndex);
        });
      });
}

template <typename Spec, typename Interface>
template <typename StreamDescriptor, typename T, typename E>
auto LogMultiplexerImplementation<Spec, Interface>::waitForIteratorInternal(LogIndex first)
    -> futures::Future<std::unique_ptr<TypedLogRangeIterator<E>>> {
  return waitForInternal<StreamDescriptor>(first).thenValue(
      [that = std::static_pointer_cast<SelfClass>(this->shared_from_this()), first](auto&&) {
        return that->_guarded.doUnderLock([&](MultiplexerData<Spec>& self) {
          auto& block = std::get<StreamInformationBlock<StreamDescriptor>>(self._blocks);
          return block.getIteratorRange(first, self._commitIndex + 1);
        });
      });
}

template <typename Spec>
auto LogDemultiplexer<Spec>::construct(std::shared_ptr<replicated_log::ILogParticipant> interface)
    -> std::shared_ptr<LogDemultiplexer> {
  return std::make_shared<streams::LogDemultiplexerImplementation<Spec, replicated_log::ILogParticipant>>(
      std::move(interface));
}

template <typename Spec>
auto LogMultiplexer<Spec>::construct(std::shared_ptr<arangodb::replication2::replicated_log::LogLeader> leader)
    -> std::shared_ptr<LogMultiplexer> {
  return std::make_shared<streams::LogMultiplexerImplementation<Spec, replicated_log::LogLeader>>(
      std::move(leader));
}

}  // namespace arangodb::replication2::streams
