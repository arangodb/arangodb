//
// Created by lars on 16/08/2021.
//

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <immer/flex_vector.hpp>
#include <immer/flex_vector_transient.hpp>

#include <Basics/Exceptions.h>
#include <Basics/Guarded.h>
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
  auto waitFor(LogIndex index) -> futures::Future<futures::Unit> final {
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

  template <typename StreamDescriptor>
  auto waitForInternal(LogIndex) -> futures::Future<futures::Unit> {
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

    LogIndex _releaseIndex{0};
    VariantType _container;

    auto getTransientContainer() -> TransientType&;
    auto getPersistentContainer() -> ContainerType&;

    auto appendEntry(LogIndex index, Type t);
    template <StreamTag StreamTag, typename Deserializer, typename Serializer>
    auto appendValueBySlice(tag_descriptor<StreamTag, Deserializer, Serializer>,
                            LogIndex index, velocypack::Slice value);
  };

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

template <typename Spec>
template <StreamId Id, typename Type, typename Tags>
auto LogDemultiplexerImplementation<Spec>::StreamInformationBlock<stream_descriptor<Id, Type, Tags>>::getTransientContainer()
    -> TransientType& {
  if (!std::holds_alternative<TransientType>(_container)) {
    _container = std::get<ContainerType>(_container).transient();
  }
  return std::get<TransientType>(_container);
}

template <typename Spec>
template <StreamId Id, typename Type, typename Tags>
auto LogDemultiplexerImplementation<Spec>::StreamInformationBlock<stream_descriptor<Id, Type, Tags>>::getPersistentContainer()
    -> ContainerType& {
  if (!std::holds_alternative<ContainerType>(_container)) {
    _container = std::get<TransientType>(_container).persistent();
  }
  return std::get<ContainerType>(_container);
}

template <typename Spec>
template <StreamId Id, typename Type, typename Tags>
auto LogDemultiplexerImplementation<Spec>::StreamInformationBlock<stream_descriptor<Id, Type, Tags>>::appendEntry(
    LogIndex index, Type t) {
  getTransientContainer().push_back(EntryType{index, std::move(t)});
}

template <typename Spec>
template <StreamId Id, typename Type, typename Tags>
template <StreamTag StreamTag, typename Deserializer, typename Serializer>
auto LogDemultiplexerImplementation<Spec>::StreamInformationBlock<stream_descriptor<Id, Type, Tags>>::appendValueBySlice(
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

template <typename Spec>
struct LogMultiplexerImplementation
    : LogMultiplexer<Spec>,
      StreamDispatcher<LogMultiplexerImplementation<Spec>, Spec, ProducerStream> {
  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename E = typename Stream<T>::EntryViewType>
  auto waitForIteratorInternal(LogIndex)
      -> futures::Future<std::unique_ptr<TypedLogRangeIterator<E>>> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  template <typename StreamDescriptor>
  auto waitForInternal(LogIndex) -> futures::Future<futures::Unit> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  template <typename StreamDescriptor>
  auto releaseInternal(LogIndex) -> void {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>>
  auto insertInternal(T const& t) -> LogIndex {
    using PrimaryTag = stream_descriptor_primary_tag_t<StreamDescriptor>;
    using Serializer = typename PrimaryTag::serializer;
    static_assert(std::is_invocable_r_v < void, Serializer, serializer_tag_t<T>,
                  std::add_lvalue_reference_t<std::add_const_t<T>>,
                  std::add_lvalue_reference_t<velocypack::Builder>>);

    auto serialized = std::invoke([&]{
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

    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  template <typename StreamDescriptor, typename T = stream_descriptor_type_t<StreamDescriptor>,
            typename E = typename Stream<T>::EntryViewType>
  auto getIteratorInternal() -> std::unique_ptr<TypedLogRangeIterator<E>> {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }
};

}  // namespace arangodb::replication2::streams

template <typename Spec>
auto LogDemultiplexer2<Spec>::construct() -> std::shared_ptr<LogDemultiplexer2> {
  return std::make_shared<streams::LogDemultiplexerImplementation<Spec>>();
}

template <typename Spec>
auto LogMultiplexer<Spec>::construct() -> std::shared_ptr<LogMultiplexer> {
  return std::make_shared<streams::LogMultiplexerImplementation<Spec>>();
}
