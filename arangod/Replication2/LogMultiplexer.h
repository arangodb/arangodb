#pragma once
#include <utility>

#include <Futures/Future.h>

#include <Replication2/ReplicatedLog/LogCommon.h>
#include <Replication2/ReplicatedLog/types.h>

namespace arangodb::replication2::streams {

using StreamId = std::uint64_t;
using StreamTag = std::uint64_t;

template <typename T>
struct serializer_tag_t {};
template <typename T>
inline constexpr auto serializer_tag = serializer_tag_t<T>{};

template <StreamTag Tag, typename D, typename S>
struct tag_descriptor {
  using deserializer = D;
  using serializer = S;
  static constexpr auto tag = Tag;
};

template <typename>
struct is_tag_descriptor : std::false_type {};
template <typename D, typename S, StreamTag Tag>
struct is_tag_descriptor<tag_descriptor<Tag, D, S>> : std::true_type {};
template <typename T>
inline constexpr bool is_tag_descriptor_v = is_tag_descriptor<T>::value;

template <typename... Ts>
struct tag_descriptor_set {
  static_assert((is_tag_descriptor_v<Ts> && ...));
};

template <typename>
struct tag_descriptor_set_primary;
template <typename D, typename... Ds>
struct tag_descriptor_set_primary<tag_descriptor_set<D, Ds...>> {
  using type = D;
};
template <typename Ds>
using tag_descriptor_set_primary_t = tag_descriptor_set_primary<Ds>;

template <StreamId, typename, typename>
struct stream_descriptor;
template <StreamId StreamId, typename Type, typename... TDs>
struct stream_descriptor<StreamId, Type, tag_descriptor_set<TDs...>> {
  static constexpr auto id = StreamId;
  using tags = tag_descriptor_set<TDs...>;
  using type = Type;

  // Check that all deserializers are invocable with (serializer_tag<Type>{},
  // slice) and return Type.
  static_assert((std::is_invocable_r_v<Type, typename TDs::deserializer, serializer_tag_t<Type>, velocypack::Slice> &&
                 ...));

  // Check that all serializers are invocable with (serializer_tag<Type>{}, T
  // const&, Builder) and return void.
  static_assert((std::is_invocable_r_v<void, typename TDs::serializer, serializer_tag_t<Type>,
                                       std::add_lvalue_reference_t<std::add_const_t<Type>>,
                                       std::add_lvalue_reference_t<velocypack::Builder>> &&
                 ...));
};

template <typename>
struct is_stream_descriptor : std::false_type {};
template <StreamId StreamId, typename Type, typename Tags>
struct is_stream_descriptor<stream_descriptor<StreamId, Type, Tags>> : std::true_type {
};
template <typename T>
inline constexpr auto is_stream_descriptor_v = is_stream_descriptor<T>::value;

template <typename... Descriptors>
struct stream_descriptor_set {
  static_assert((is_stream_descriptor_v<Descriptors> && ...));

  static constexpr auto length = sizeof...(Descriptors);
};

template <typename T>
struct stream_descriptor_type {
  static_assert(is_stream_descriptor_v<T>);
  using type = typename T::type;
};
template <typename T>
using stream_descriptor_type_t = typename stream_descriptor_type<T>::type;
template <typename T>
struct stream_descriptor_id {
  static inline constexpr auto value = T::id;
};
template <typename T>
inline constexpr auto stream_descriptor_id_v = stream_descriptor_id<T>::value;
template <typename T>
struct stream_descriptor_tags {
  using type = typename T::tags;
};
template <typename T>
using stream_descriptor_tags_t = typename stream_descriptor_tags<T>::type;

template <typename T>
using stream_descriptor_primary_tag_t =
    typename tag_descriptor_set_primary_t<stream_descriptor_tags_t<T>>::type;

namespace detail {
template <StreamId StreamId, typename... Ds>
struct stream_descriptor_by_id_impl;
template <StreamId StreamId, typename D, typename... Ds>
struct stream_descriptor_by_id_impl<StreamId, D, Ds...>
    : std::conditional<StreamId == stream_descriptor_id_v<D>, D,
                       typename stream_descriptor_by_id_impl<StreamId, Ds...>::type> {};
template <unsigned StreamId, typename D>
struct stream_descriptor_by_id_impl<StreamId, D> {
  // static_assert(StreamId == stream_descriptor_id_v<D>);
  using type = D;
};

}  // namespace detail

template <StreamId StreamId, typename Ds>
struct stream_descriptor_by_id;
template <StreamId StreamId, typename... Ds>
struct stream_descriptor_by_id<StreamId, stream_descriptor_set<Ds...>> {
  static_assert(((stream_descriptor_id_v<Ds> == StreamId) || ...));
  using type = typename detail::stream_descriptor_by_id_impl<StreamId, Ds...>::type;
};
template <StreamId StreamId, typename Ds>
using stream_descriptor_by_id_t = typename stream_descriptor_by_id<StreamId, Ds>::type;

template <StreamId StreamId, typename Ds>
using stream_type_by_id_t =
    stream_descriptor_type_t<stream_descriptor_by_id_t<StreamId, Ds>>;

namespace detail {
template <std::size_t I, StreamId StreamId, typename... Ds>
struct stream_index_by_id_impl;
template <std::size_t I, StreamId StreamId, typename D, typename... Ds>
struct stream_index_by_id_impl<I, StreamId, D, Ds...>
    : std::conditional_t<StreamId == stream_descriptor_id_v<D>, std::integral_constant<std::size_t, I>,
                         stream_index_by_id_impl<I + 1, StreamId, Ds...>> {};
template <std::size_t I, unsigned StreamId, typename D>
struct stream_index_by_id_impl<I, StreamId, D>
    : std::integral_constant<std::size_t, I> {};
}  // namespace detail

template <StreamId StreamId, typename Ds>
struct stream_index_by_id;
template <StreamId StreamId, typename... Ds>
struct stream_index_by_id<StreamId, stream_descriptor_set<Ds...>> {
  static_assert(((stream_descriptor_id_v<Ds> == StreamId) || ...));
  static inline constexpr std::size_t value =
      detail::stream_index_by_id_impl<0, StreamId, Ds...>::value;
};

template <StreamId StreamId, typename Ds>
inline constexpr auto stream_index_by_id_v = stream_index_by_id<StreamId, Ds>::value;

template<typename T>
struct StreamEntryView {
  LogIndex index;
  T const& value;
};

template<typename T>
struct StreamEntry {
  LogIndex index;
  T value;

  auto intoView() const -> StreamEntryView<T> {
    return {index, value};
  }
};

template <typename T>
struct Stream {
  using EntryViewType = StreamEntryView<T>;
  using EntryType = StreamEntry<T>;
  using Iterator = TypedLogRangeIterator<EntryViewType>;

  virtual ~Stream() = default;
  virtual auto waitForIterator(LogIndex)
      -> futures::Future<std::unique_ptr<Iterator>> = 0;
  virtual auto waitFor(LogIndex) -> futures::Future<futures::Unit> = 0;
  virtual auto release(LogIndex) -> void = 0;
};

template<typename T>
struct ProducerStream : Stream<T> {
  virtual auto insert(T const&) -> LogIndex = 0;
};

template <typename Ds, template<typename> typename StreamType, typename Type = stream_descriptor_type_t<Ds>>
    struct StreamBase : StreamType<Type> {
  static_assert(is_stream_descriptor_v<Ds>);
  using Iterator = typename StreamType<Type>::Iterator;
  virtual auto getIterator() -> std::unique_ptr<Iterator> = 0;
};

template <typename, typename, template<typename> typename>
struct StreamDispatcherBase;
template <typename Self, typename... Streams, template<typename> typename StreamType>
struct StreamDispatcherBase<Self, stream_descriptor_set<Streams...>, StreamType>
    : virtual StreamBase<Streams, StreamType>... {
 protected:
  template <typename Descriptor>
  auto getStreamByIdInternal() {
    return std::static_pointer_cast<StreamBase<Descriptor, StreamType>>(
        self().shared_from_this());
  }

 private:
  auto self() -> Self& { return static_cast<Self&>(*this); }
};

template <typename Self, typename Spec, template <typename> typename StreamType>
struct LogDemultiplexerBase : std::enable_shared_from_this<Self>,
                              StreamDispatcherBase<Self, Spec, StreamType> {
  template <StreamId Id, typename Descriptor = stream_descriptor_by_id_t<Id, Spec>,
            typename Type = stream_descriptor_type_t<Descriptor>>
  auto getStreamBaseById()
      -> std::shared_ptr<StreamBase<Descriptor, StreamType>> {
    return this->template getStreamByIdInternal<Descriptor>();
  }

  template <StreamId Id, typename Descriptor = stream_descriptor_by_id_t<Id, Spec>,
      typename Type = stream_descriptor_type_t<Descriptor>>
  auto getStreamById() -> std::shared_ptr<Stream<Type>> {
    return this->template getStreamByIdInternal<Descriptor>();
  }
};

template <typename Spec>
struct LogDemultiplexer2 : LogDemultiplexerBase<LogDemultiplexer2<Spec>, Spec, Stream> {
  virtual auto digestIterator(LogRangeIterator& iter) -> void = 0;
  static auto construct() -> std::shared_ptr<LogDemultiplexer2>;
};

template <typename Spec>
struct LogMultiplexer : LogDemultiplexerBase<LogMultiplexer<Spec>, Spec, ProducerStream> {
  static auto construct() -> std::shared_ptr<LogMultiplexer>;
};

}  // namespace arangodb::replication2::streams
