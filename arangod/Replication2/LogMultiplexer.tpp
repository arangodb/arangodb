//
// Created by lars on 16/08/2021.
//

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

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::streams;

namespace {

template <StreamId Id, typename Type, typename... Tags>
constexpr auto stream_descriptor_contains_tag(
    stream_descriptor<Id, Type, tag_descriptor_set<Tags...>>, StreamTag wanted) -> bool {
  return ((Tags::tag == wanted) || ...);
}

template <typename Tag, StreamId>
struct tag_stream_pair {};

template <typename...>
struct tag_stream_pair_list {};

template <typename...>
struct tag_stream_pair_list_merge;
template <typename... Ds, typename... Bs, typename... As>
struct tag_stream_pair_list_merge<tag_stream_pair_list<Ds...>, tag_stream_pair_list<Bs...>, As...> {
  using type =
      typename tag_stream_pair_list_merge<tag_stream_pair_list<Ds..., Bs...>, As...>::type;
};
template <typename T>
struct tag_stream_pair_list_merge<T> {
  using type = T;
};

template <typename>
struct tag_stream_pair_from_descriptor;
template <StreamId Id, typename Type, typename... Tags>
struct tag_stream_pair_from_descriptor<streams::stream_descriptor<Id, Type, streams::tag_descriptor_set<Tags...>>> {
  using type = tag_stream_pair_list<tag_stream_pair<Tags, Id>...>;
};

template <typename>
struct tag_stream_pair_list_from_spec;
template <typename... Ds>
struct tag_stream_pair_list_from_spec<streams::stream_descriptor_set<Ds...>> {
  using type =
      typename tag_stream_pair_list_merge<typename tag_stream_pair_from_descriptor<Ds>::type...>::type;
};

}  // namespace

namespace arangodb::replication2::streams {

template <typename Descriptor, typename Type = stream_descriptor_type_t<Descriptor>>
struct DescriptorValueTag {
  using DescriptorType = Descriptor;
  explicit DescriptorValueTag(Type value) : value(std::move(value)) {}
  Type value;
};

template <typename... Descriptors>
struct MultiplexedVariant {
  using VariantType = std::variant<DescriptorValueTag<Descriptors>...>;

  auto variant() -> VariantType& { return _value; }
  auto variant() const -> VariantType& { return _value; }

  template <typename... Args>
  explicit MultiplexedVariant(std::in_place_t, Args&&... args)
      : _value(std::forward<Args>(args)...) {}

 private:
  explicit MultiplexedVariant(VariantType value) : _value(std::move(value)) {}
  VariantType _value;
};

struct MultiplexedValues {
  template <typename Descriptor, typename Type = stream_descriptor_type_t<Descriptor>>
  static void toVelocyPack(Type const& v, velocypack::Builder& builder) {
    using PrimaryTag = stream_descriptor_primary_tag_t<Descriptor>;
    using Serializer = typename PrimaryTag::serializer;
    velocypack::ArrayBuilder ab(&builder);
    builder.add(velocypack::Value(PrimaryTag::tag));
    std::invoke(Serializer{}, serializer_tag<Type>, v, builder);
  }

  template <typename... Descriptors>
  static auto fromVelocyPack(velocypack::Slice slice)
      -> MultiplexedVariant<Descriptors...> {
    TRI_ASSERT(slice.isArray());
    auto [tag, valueSlice] = slice.unpackTuple<StreamTag, velocypack::Slice>();
    // Now find the descriptor that fits this tag
    return FromVelocyPackHelper<MultiplexedVariant<Descriptors...>, Descriptors...>::extract(tag, valueSlice);
  }

 private:
  template <typename ValueType, typename Descriptor, typename... Other>
  struct FromVelocyPackHelper {
    static auto extract(StreamTag tag, velocypack::Slice slice) -> ValueType {
      return extractTags(stream_descriptor_tags_t<Descriptor>{}, tag, slice);
    }

    template <typename Tag, typename... Tags>
    static auto extractTags(tag_descriptor_set<Tag, Tags...>, StreamTag tag,
                            velocypack::Slice slice) -> ValueType {
      if (Tag::tag == tag) {
        return extractValue<typename Tag::deserializer>(slice);
      } else if constexpr (sizeof...(Tags) > 0) {
        return extractTags(tag_descriptor_set<Tags...>{}, tag, slice);
      } else if constexpr (sizeof...(Other) > 0) {
        return FromVelocyPackHelper<ValueType, Other...>::extract(tag, slice);
      } else {
        std::abort();
      }
    }

    template <typename Deserializer, typename Type = stream_descriptor_type_t<Descriptor>>
    static auto extractValue(velocypack::Slice slice) -> ValueType {
      auto value = std::invoke(Deserializer{}, serializer_tag<Type>, slice);
      return ValueType(std::in_place, std::in_place_type<DescriptorValueTag<Descriptor>>,
                       std::move(value));
    }
  };
};

template <typename T>
struct StreamEntry {
  LogIndex index;
  T value;
  auto intoView() const -> StreamEntryView<T> { return {index, value}; }
};

/**
 * This is the implementation of the stream interfaces. They are just proxy
 * objects that static_cast the this pointer to the respective implementor and
 * forward the call, annotated with the stream descriptor.
 * @tparam Self Implementor Top Class
 * @tparam Descriptor Stream Descriptor
 * @tparam StreamTypeTemplate Stream<T> or ProducerStream<T>
 */
template <typename Self, typename Descriptor, template <typename> typename StreamTypeTemplate>
struct StreamGenericImplementationBase
    : virtual StreamGenericBase<Descriptor, StreamTypeTemplate> {
  static_assert(is_stream_descriptor_v<Descriptor>);

  using ValueType = stream_descriptor_type_t<Descriptor>;
  using StreamType = streams::Stream<ValueType>;
  using EntryViewType = StreamEntryView<ValueType>;
  using Iterator = TypedLogRangeIterator<EntryViewType>;
  using WaitForResult = typename Stream<ValueType>::WaitForResult;

  auto waitForIterator(LogIndex index) -> futures::Future<std::unique_ptr<Iterator>> final {
    return self().template waitForIteratorInternal<Descriptor>(index);
  }
  auto waitFor(LogIndex index) -> futures::Future<WaitForResult> final {
    return self().template waitForInternal<Descriptor>(index);
  }
  auto release(LogIndex index) -> void final {
    return self().template releaseInternal<Descriptor>(index);
  }
  auto getIterator() -> std::unique_ptr<Iterator> final {
    return self().template getIteratorInternal<Descriptor>();
  }

 protected:
  auto self() -> Self& { return static_cast<Self&>(*this); }
};

/**
 * Wrapper about StreamGenericImplementationBase, that adds depending on the
 * StreamType more methods. Is specialized for ProducerStream<T>.
 * @tparam Self Implementor Top Class
 * @tparam Descriptor Stream Descriptor
 * @tparam StreamTypeTemplate Stream<T> or ProducerStream<T>
 */
template <typename Self, typename Descriptor, template <typename> typename StreamTypeTemplate>
struct StreamGenericImplementation
    : StreamGenericImplementationBase<Self, Descriptor, StreamTypeTemplate> {};
template <typename Self, typename Descriptor>
struct StreamGenericImplementation<Self, Descriptor, ProducerStream>
    : StreamGenericImplementationBase<Self, Descriptor, ProducerStream> {
  using ValueType = stream_descriptor_type_t<Descriptor>;
  auto insert(ValueType const& t) -> LogIndex override {
    return this->self().template insertInternal<Descriptor>(t);
  }
};

template <typename Self, typename Descriptor>
using StreamImplementation = StreamGenericImplementation<Self, Descriptor, Stream>;
template <typename Self, typename Descriptor>
using ProducerStreamImplementation =
    StreamGenericImplementation<Self, Descriptor, ProducerStream>;

template <typename, typename, template <typename> typename>
struct StreamDispatcher;

/**
 * Class that implements all streams as virtual base classes.
 * @tparam Self
 * @tparam Streams
 * @tparam StreamType
 */
template <typename Self, typename... Streams, template <typename> typename StreamType>
struct StreamDispatcher<Self, stream_descriptor_set<Streams...>, StreamType>
    : StreamGenericImplementation<Self, Streams, StreamType>... {};

template <typename Descriptor>
struct StreamInformationBlock;
template <StreamId Id, typename Type, typename Tags>
struct StreamInformationBlock<stream_descriptor<Id, Type, Tags>> {
  using StreamType = streams::Stream<Type>;
  using EntryType = StreamEntry<Type>;
  using Iterator = TypedLogRangeIterator<StreamEntryView<Type>>;

  using ContainerType = ::immer::flex_vector<EntryType>;
  using TransientType = typename ContainerType::transient_type;
  using VariantType = std::variant<ContainerType, TransientType>;

  using WaitForResult = typename StreamType::WaitForResult;
  using WaitForPromise = futures::Promise<WaitForResult>;
  using WaitForQueue = std::multimap<LogIndex, WaitForPromise>;

  LogIndex _releaseIndex{0};
  VariantType _container;
  WaitForQueue _waitForQueue;

  auto getTransientContainer() -> TransientType&;
  auto getPersistentContainer() -> ContainerType&;

  auto appendEntry(LogIndex index, Type t);
  template <StreamTag StreamTag, typename Deserializer, typename Serializer>
  auto appendValueBySlice(tag_descriptor<StreamTag, Deserializer, Serializer>,
                          LogIndex index, velocypack::Slice value);

  auto getWaitForResolveSet(LogIndex commitIndex) -> WaitForQueue;
  auto registerWaitFor(LogIndex index) -> futures::Future<WaitForResult>;
  auto getIterator() -> std::unique_ptr<Iterator>;
};

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
template <StreamTag StreamTag, typename Deserializer, typename Serializer>
auto StreamInformationBlock<stream_descriptor<Id, Type, Tags>>::appendValueBySlice(
    tag_descriptor<StreamTag, Deserializer, Serializer>, LogIndex index,
    velocypack::Slice value) {
  static_assert(std::is_invocable_r_v<Type, Deserializer, serializer_tag_t<Type>, velocypack::Slice>);
  appendEntry(index, std::invoke(Deserializer{}, serializer_tag<Type>, value));
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
        auto view = current->intoView();
        ++current;
        return view;
      }
      return std::nullopt;
    }
    auto range() const noexcept -> std::pair<LogIndex, LogIndex> override {
      abort();
      THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
    }

    explicit Iterator(ContainerType log)
        : log(std::move(log)), current(this->log.begin()) {}
  };

  return std::make_unique<Iterator>(std::move(log));
}

namespace {
template <typename Descriptor, typename Queue, typename Result, typename Block = StreamInformationBlock<Descriptor>>
auto resolvePromiseSet(std::pair<Queue, Result>& q) {
  std::for_each(std::begin(q.first), std::end(q.first),
                [&](auto& pair) { pair.second.setValue(q.second); });
}

template <typename... Descriptors, typename... Pairs>
auto resolvePromiseSets(stream_descriptor_set<Descriptors...>, std::tuple<Pairs...>& pairs) {
  (resolvePromiseSet<Descriptors>(std::get<Pairs>(pairs)), ...);
}

}  // namespace

template <typename Spec, typename Store>
struct LogMultiplexerImplementationBase {
  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename E = StreamEntryView<T>>
  auto getIteratorInternal() -> std::unique_ptr<TypedLogRangeIterator<E>> {
    using BlockType = StreamInformationBlock<StreamDescriptor>;

    return _guardedData.template doUnderLock([](Store& self) {
      auto& block = std::get<BlockType>(self._blocks);
      return block.getIterator();
    });
  }

  Guarded<Store, basics::UnshackledMutex> _guardedData;
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
  auto waitForIteratorInternal(LogIndex)
      -> futures::Future<std::unique_ptr<TypedLogRangeIterator<E>>> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

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
    void digestEntry(LogEntryView entry);

    auto getWaitForResolveSetAll(LogIndex commitIndex) {
      return std::make_tuple(std::make_pair(
          std::get<StreamInformationBlock<Descriptors>>(_blocks).getWaitForResolveSet(commitIndex),
          typename StreamInformationBlock<Descriptors>::WaitForResult{})...);
    };

   private:
    template <typename... TagDescriptors, StreamId... StreamIds>
    void digestEntryInternal(tag_stream_pair_list<tag_stream_pair<TagDescriptors, StreamIds>...>,
                             LogEntryView entry);
  };

  Guarded<MultiplexerData<Spec>, basics::UnshackledMutex> _guardedData{};
  std::shared_ptr<Interface> const _interface;
};

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
template <typename... Descriptors>
void LogDemultiplexerImplementation<Spec, Interface>::MultiplexerData<
    stream_descriptor_set<Descriptors...>>::digestEntry(LogEntryView entry) {
  using StreamTagPairs = typename tag_stream_pair_list_from_spec<Spec>::type;
  digestEntryInternal(StreamTagPairs{}, entry);
}

template <typename Spec, typename Interface>
template <typename... Descriptors>
template <typename... TagDescriptors, StreamId... StreamIds>
void LogDemultiplexerImplementation<Spec, Interface>::MultiplexerData<stream_descriptor_set<Descriptors...>>::digestEntryInternal(
    tag_stream_pair_list<tag_stream_pair<TagDescriptors, StreamIds>...>, LogEntryView entry) {
  // now lookup the tag in the pairs
  // The compiler will translate this into a jump table
  auto const dispatchEntryByTag = [this](StreamTag tag, LogIndex index,
                                         velocypack::Slice slice) {
    return ([&] {
      constexpr const auto StreamIndex = stream_index_by_id_v<StreamIds, Spec>;

      if (tag == TagDescriptors::tag) {
        // This tag was recognised - now deserialize the entry
        std::get<StreamIndex>(_blocks).appendValueBySlice(TagDescriptors{}, index, slice);
        return true;
      }
      return false;
    }() || ...);
  };

  auto const slice = entry.logPayload();
  auto const entryTag = slice.get("tag").extract<StreamTag>();
  auto const valueSlice = slice.get("value");

  if (auto const wasDispatched = dispatchEntryByTag(entryTag, entry.logIndex(), valueSlice);
      !wasDispatched) {
    LOG_TOPIC("9b7ab", FATAL, Logger::REPLICATION2)
        << "Log-Multiplexer could not dispatch value with tag (" << entryTag << ").";
    FATAL_ERROR_EXIT();
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
  auto waitForIteratorInternal(LogIndex)
      -> futures::Future<std::unique_ptr<TypedLogRangeIterator<E>>> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename W = typename Stream<T>::WaitForResult>
  auto waitForInternal(LogIndex index) -> futures::Future<W> {
    return _guarded.doUnderLock([&](MultiplexerData<Spec>& self) {
      if (self._commitIndex >= index) {
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
      auto index = _interface->insert(LogPayload(std::move(serialized)));
      TRI_ASSERT(index > self._lastIndex);
      self._lastIndex = index;

      // Now we insert the value T into the StreamsLog,
      // but it is not yet visible because of the commitIndex
      auto& block = std::get<StreamInformationBlock<StreamDescriptor>>(self._blocks);
      block.appendEntry(index, t);
      return std::make_pair(index, self.checkWaitFor());
    });

    if (waitForIndex.has_value()) {
      triggerWaitForIndex(*waitForIndex);
    }
    return index;
  }

  void triggerWaitForIndex(LogIndex waitForIndex) {
    auto f = _interface->waitFor(waitForIndex);
    std::move(f).thenValue([weak = this->weak_from_this()](replicated_log::WaitForResult&& result) {
      // First lock the shared pointer
      if (auto that = std::static_pointer_cast<SelfClass>(weak.lock()); that) {
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

}  // namespace arangodb::replication2::streams

template <typename Spec>
auto LogDemultiplexer<Spec>::construct(std::shared_ptr<replicated_log::LogFollower> interface)
    -> std::shared_ptr<LogDemultiplexer> {
  return std::make_shared<streams::LogDemultiplexerImplementation<Spec, replicated_log::LogFollower>>(
      std::move(interface));
}

template <typename Spec>
auto LogMultiplexer<Spec>::construct(std::shared_ptr<arangodb::replication2::replicated_log::LogLeader> leader)
    -> std::shared_ptr<LogMultiplexer> {
  return std::make_shared<streams::LogMultiplexerImplementation<Spec, replicated_log::LogLeader>>(
      std::move(leader));
}
