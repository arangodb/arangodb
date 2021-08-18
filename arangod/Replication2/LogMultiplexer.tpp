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

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::streams;

namespace {

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

template <typename Self, typename Descriptor, template <typename> typename StreamTypeTemplate>
struct StreamImplementationBase : virtual StreamBase<Descriptor, StreamTypeTemplate> {
  static_assert(is_stream_descriptor_v<Descriptor>);

  using ValueType = stream_descriptor_type_t<Descriptor>;
  using StreamType = streams::Stream<ValueType>;
  using EntryViewType = typename StreamType::EntryViewType;
  using Iterator = TypedLogRangeIterator<EntryViewType>;

  auto waitForIterator(LogIndex index) -> futures::Future<std::unique_ptr<Iterator>> final {
    return self().template waitForIteratorInternal<Descriptor>(index);
  }
  auto waitFor(LogIndex index) -> futures::Future<typename StreamType::WaitForResult> final {
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

template <typename Self, typename Descriptor, template <typename> typename StreamTypeTemplate>
struct StreamImplementation
    : StreamImplementationBase<Self, Descriptor, StreamTypeTemplate> {};
template <typename Self, typename Descriptor>
struct StreamImplementation<Self, Descriptor, ProducerStream>
    : StreamImplementationBase<Self, Descriptor, ProducerStream> {
  using ValueType = stream_descriptor_type_t<Descriptor>;

  auto insert(ValueType const& t) -> LogIndex override {
    return this->self().template insertInternal<Descriptor>(t);
  }
};

template <typename, typename, template <typename> typename>
struct StreamDispatcher;
template <typename Self, typename... Streams, template <typename> typename StreamType>
struct StreamDispatcher<Self, stream_descriptor_set<Streams...>, StreamType>
    : StreamImplementation<Self, Streams, StreamType>... {};

template <typename Descriptor>
struct StreamInformationBlock;
template <StreamId Id, typename Type, typename Tags>
struct StreamInformationBlock<stream_descriptor<Id, Type, Tags>> {
  using StreamType = streams::Stream<Type>;
  using EntryType = typename StreamType::EntryType;
  using Iterator = TypedLogRangeIterator<EntryType>;

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

  auto getWaitForResolveSet(LogIndex commitIndex) -> WaitForQueue {
    WaitForQueue toBeResolved;
    auto const end = _waitForQueue.upper_bound(commitIndex);
    for (auto it = _waitForQueue.begin(); it != end;) {
      toBeResolved.insert(_waitForQueue.extract(it++));
    }
    return toBeResolved;
  }
};

template <typename Spec>
struct LogDemultiplexerImplementation
    : LogDemultiplexer2<Spec>,
      StreamDispatcher<LogDemultiplexerImplementation<Spec>, Spec, Stream> {
  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename E = typename Stream<T>::EntryViewType>
  auto waitForIteratorInternal(LogIndex)
      -> futures::Future<std::unique_ptr<TypedLogRangeIterator<E>>> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename W = typename Stream<T>::WaitForResult>
  auto waitForInternal(LogIndex) -> futures::Future<W> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  template <typename StreamDescriptor>
  auto releaseInternal(LogIndex) -> void {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename E = typename Stream<T>::EntryViewType>
  auto getIteratorInternal() -> std::unique_ptr<TypedLogRangeIterator<E>>;

  auto digestIterator(LogRangeIterator& iter) -> void override;

  template <typename>
  struct MultiplexerData;
  template <typename... Descriptors>
  struct MultiplexerData<stream_descriptor_set<Descriptors...>> {
    std::tuple<StreamInformationBlock<Descriptors>...> _blocks;

    void digestIterator(LogRangeIterator& iter);
    void digestEntry(LogEntryView entry);

   private:
    template <typename... TagDescriptors, StreamId... StreamIds>
    void digestEntryInternal(tag_stream_pair_list<tag_stream_pair<TagDescriptors, StreamIds>...>,
                             LogEntryView entry);
  };

  Guarded<MultiplexerData<Spec>, basics::UnshackledMutex> _guardedData{};
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

template <typename Spec>
template <typename... Descriptors>
void LogDemultiplexerImplementation<Spec>::MultiplexerData<stream_descriptor_set<Descriptors...>>::digestIterator(
    LogRangeIterator& iter) {
  while (auto memtry = iter.next()) {
    digestEntry(memtry.value());
  }
}

template <typename Spec>
template <typename... Descriptors>
void LogDemultiplexerImplementation<Spec>::MultiplexerData<stream_descriptor_set<Descriptors...>>::digestEntry(
    LogEntryView entry) {
  using StreamTagPairs = typename tag_stream_pair_list_from_spec<Spec>::type;
  digestEntryInternal(StreamTagPairs{}, entry);
}

template <typename Spec>
template <typename... Descriptors>
template <typename... TagDescriptors, StreamId... StreamIds>
void LogDemultiplexerImplementation<Spec>::MultiplexerData<stream_descriptor_set<Descriptors...>>::digestEntryInternal(
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

template <typename Spec>
void LogDemultiplexerImplementation<Spec>::digestIterator(LogRangeIterator& iter) {
  _guardedData.getLockedGuard()->digestIterator(iter);
}

template <typename Spec>
template <typename StreamDescriptor, typename T, typename E>
auto LogDemultiplexerImplementation<Spec>::getIteratorInternal()
    -> std::unique_ptr<TypedLogRangeIterator<E>> {
  using BlockType = StreamInformationBlock<StreamDescriptor>;

  return _guardedData.template doUnderLock([](MultiplexerData<Spec>& self) {
    auto& block = std::get<BlockType>(self._blocks);
    auto log = block.getPersistentContainer();

    struct Iterator : TypedLogRangeIterator<E> {
      using ContainerType = typename BlockType::ContainerType;
      using ContainerIterator = typename ContainerType::iterator;
      ContainerType log;
      ContainerIterator current;

      auto next() -> std::optional<E> override {
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
            typename E = typename Stream<T>::EntryViewType>
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
      return block._waitForQueue.emplace(index, futures::Promise<W>{})->second.getFuture();
    });
  }

  template <typename StreamDescriptor>
  auto releaseInternal(LogIndex) -> void {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>>
  auto insertInternal(T const& t) -> LogIndex {
    using PrimaryTag = stream_descriptor_primary_tag_t<StreamDescriptor>;
    using Serializer = typename PrimaryTag::serializer;
    static_assert(
        std::is_invocable_r_v<void, Serializer, serializer_tag_t<T>,
                              std::add_lvalue_reference_t<std::add_const_t<T>>,
                              std::add_lvalue_reference_t<velocypack::Builder>>);

    auto serialized = std::invoke([&] {
      velocypack::UInt8Buffer buffer;
      {
        velocypack::Builder builder(buffer);
        velocypack::ObjectBuilder ob(&builder);
        builder.add("tag", velocypack::Value(PrimaryTag::tag));
        builder.add(velocypack::Value("value"));
        std::invoke(Serializer{}, serializer_tag<T>, t, builder);
      }
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

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename E = typename Stream<T>::EntryViewType>
  auto getIteratorInternal() -> std::unique_ptr<TypedLogRangeIterator<E>> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  template <typename... Descriptors, typename... Queues>
  static auto resolvePromiseSets(stream_descriptor_set<Descriptors...>,
                                 std::tuple<Queues...>& queues) {
    (resolvePromiseSet<Descriptors>(std::get<Queues>(queues)), ...);
  }

  template <typename Descriptor, typename Block = StreamInformationBlock<Descriptor>, typename Queue = typename Block::WaitForQueue>
  static auto resolvePromiseSet(Queue& q) {
    using WaitForResult = typename Block::WaitForResult;
    std::for_each(std::begin(q), std::end(q),
                  [](auto& pair) { pair.second.setValue(WaitForResult{}); });
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

    auto getWaitForResolveSetAll(LogIndex commitIndex)
        -> std::tuple<typename StreamInformationBlock<Descriptors>::WaitForQueue...> {
      return std::make_tuple(
          std::get<StreamInformationBlock<Descriptors>>(_blocks).getWaitForResolveSet(
              commitIndex)...);
    };
  };

  Guarded<MultiplexerData<Spec>, basics::UnshackledMutex> _guarded;
  std::shared_ptr<Interface> const _interface;
};

}  // namespace arangodb::replication2::streams

template <typename Spec>
auto LogDemultiplexer2<Spec>::construct() -> std::shared_ptr<LogDemultiplexer2> {
  return std::make_shared<streams::LogDemultiplexerImplementation<Spec>>();
}

template <typename Spec>
auto LogMultiplexer<Spec>::construct(std::shared_ptr<arangodb::replication2::replicated_log::LogLeader> leader)
-> std::shared_ptr<LogMultiplexer> {
  return std::make_shared<streams::LogMultiplexerImplementation<Spec, replicated_log::LogLeader>>(
      std::move(leader));
}

template <typename Spec>
auto LogMultiplexer<Spec>::construct(std::shared_ptr<TestInsertInterface> leader)
    -> std::shared_ptr<LogMultiplexer> {
  return std::make_shared<streams::LogMultiplexerImplementation<Spec, TestInsertInterface>>(
      std::move(leader));
}
