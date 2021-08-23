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

template <typename Derived, typename Spec, template <typename> typename StreamInterface, typename Interface>
struct LogMultiplexerImplementationBase {
  explicit LogMultiplexerImplementationBase(std::shared_ptr<Interface> const& interface)
      : _guardedData(static_cast<Derived&>(*this)), _interface(interface) {}

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename E = StreamEntryView<T>>
  auto waitForIteratorInternal(LogIndex first)
      -> futures::Future<std::unique_ptr<TypedLogRangeIterator<E>>> {
    return waitForInternal<StreamDescriptor>(first).thenValue(
        [that = shared_from_self(), first](auto&&) {
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

 protected:
  template <typename>
  struct MultiplexerData;
  template <typename... Descriptors>
  struct MultiplexerData<stream_descriptor_set<Descriptors...>> {
    std::tuple<StreamInformationBlock<Descriptors>...> _blocks;
    LogIndex _firstUncommittedIndex{1};
    LogIndex _lastIndex;
    bool _pendingWaitFor{false};

    Derived& _self;

    explicit MultiplexerData(Derived& self) : _self(self) {}
    void digestIterator(LogRangeIterator& iter) {
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

    auto getWaitForResolveSetAll(LogIndex commitIndex) {
      return std::make_tuple(std::make_pair(
          std::get<StreamInformationBlock<Descriptors>>(_blocks).getWaitForResolveSet(commitIndex),
          typename StreamInformationBlock<Descriptors>::WaitForResult{})...);
    }

    // returns a LogIndex to wait for (if necessary)
    auto checkWaitFor() -> std::optional<LogIndex> {
      if (!_pendingWaitFor && _lastIndex >= _firstUncommittedIndex) {
        // we have to trigger a waitFor operation
        // and wait for the next index
        _pendingWaitFor = true;
        return _firstUncommittedIndex;
      }
      return std::nullopt;
    }
  };

  auto shared_from_self() -> std::shared_ptr<Derived> {
    return std::static_pointer_cast<Derived>(static_cast<Derived&>(*this).shared_from_this());
  }

  Guarded<MultiplexerData<Spec>, basics::UnshackledMutex> _guardedData{};
  std::shared_ptr<Interface> const _interface;
};

template <typename Spec, typename Interface>
struct LogDemultiplexerImplementation
    : LogDemultiplexer<Spec>,  // implement the actual class
      ProxyStreamDispatcher<LogDemultiplexerImplementation<Spec, Interface>, Spec, Stream>,  // use a proxy stream dispatcher
      LogMultiplexerImplementationBase<LogDemultiplexerImplementation<Spec, Interface>, Spec, Stream, Interface> {
  using SelfClass = LogDemultiplexerImplementation<Spec, Interface>;
  explicit LogDemultiplexerImplementation(std::shared_ptr<Interface> interface)
      : LogMultiplexerImplementationBase<LogDemultiplexerImplementation<Spec, Interface>, Spec, Stream, Interface>(
            std::move(interface)) {}

  auto digestIterator(LogRangeIterator& iter) -> void override {
    this->_guardedData.getLockedGuard()->digestIterator(iter);
  }

  auto listen() -> void override {
    auto nextIndex =
        this->_guardedData.doUnderLock([](auto& self) -> std::optional<LogIndex> {
          if (!self._pendingWaitFor) {
            self._pendingWaitFor = true;
            return self._firstUncommittedIndex;
          }
          return std::nullopt;
        });
    if (nextIndex.has_value()) {
      triggerWaitFor(*nextIndex);
    }
  }

 private:
  void triggerWaitFor(LogIndex waitForIndex) {
    this->_interface->waitForIterator(waitForIndex)
        .thenValue([weak = this->weak_from_this()](std::unique_ptr<LogRangeIterator>&& iter) {
          if (auto locked = weak.lock(); locked) {
            auto that = std::static_pointer_cast<SelfClass>(locked);
            auto [nextIndex, promiseSets] = that->_guardedData.doUnderLock([&](auto& self) {
              self._firstUncommittedIndex = iter->range().second;
              self.digestIterator(*iter);
              return std::make_tuple(self._firstUncommittedIndex,
                                     self.getWaitForResolveSetAll(
                                         self._firstUncommittedIndex.saturatedDecrement()));
            });

            that->triggerWaitFor(nextIndex);
            resolvePromiseSets(Spec{}, promiseSets);
          }
        });
  }
};

template <typename Spec, typename Interface>
struct LogMultiplexerImplementation
    : LogMultiplexer<Spec>,
      ProxyStreamDispatcher<LogMultiplexerImplementation<Spec, Interface>, Spec, ProducerStream>,
      LogMultiplexerImplementationBase<LogMultiplexerImplementation<Spec, Interface>, Spec, ProducerStream, Interface> {
  using SelfClass = LogMultiplexerImplementation<Spec, Interface>;

  explicit LogMultiplexerImplementation(std::shared_ptr<Interface> interface)
      : LogMultiplexerImplementationBase<LogMultiplexerImplementation<Spec, Interface>, Spec, ProducerStream, Interface>(
            std::move(interface)) {}

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
    auto [index, waitForIndex] = this->_guardedData.doUnderLock([&](auto& self) {
      // First write to replicated log
      auto insertIndex = this->_interface->insert(LogPayload(std::move(serialized)));
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

 private:
  void triggerWaitForIndex(LogIndex waitForIndex) {
    auto f = this->_interface->waitFor(waitForIndex);
    std::move(f).thenValue([weak = this->weak_from_this()](
                               replicated_log::WaitForResult&& result) noexcept {
      // First lock the shared pointer
      if (auto locked = weak.lock(); locked) {
        auto that = std::static_pointer_cast<SelfClass>(locked);
        // now acquire the mutex
        auto [resolveSets, nextIndex] = that->_guardedData.doUnderLock([&](auto& self) {
          self._pendingWaitFor = false;

          // find out what the commit index is
          self._firstUncommittedIndex = result.commitIndex + 1;
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
};

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
