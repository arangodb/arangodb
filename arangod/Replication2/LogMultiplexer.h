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

  // Check that all deserializers are invocable with (serializer_tag<Type>{}, slice)
  // and return Type.
  static_assert((std::is_invocable_r_v<Type, typename TDs::deserializer, serializer_tag_t<Type>, velocypack::Slice> &&
                 ...));

  // Check that all serializers are invocable with (serializer_tag<Type>{}, T const&, Builder)
  // and return void.
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

template <typename T>
struct Stream {

  struct EntryView {
    LogIndex index;
    T const& value;
  };

  using Iterator = TypedLogRangeIterator<EntryView>;

  virtual ~Stream() = default;
  virtual auto waitForIterator(LogIndex)
  -> futures::Future<std::unique_ptr<Iterator>> = 0;
  virtual auto waitFor(LogIndex) -> futures::Future<futures::Unit> = 0;
  virtual auto release(LogIndex) -> void = 0;
};

// TODO we should replace this interface with ILogParticipant or some entity
//      from Replication2/ReplicatedLogs should implement this. Otherwise we
//      always have two indirections instead of one.
struct LogInterface {
  virtual ~LogInterface() = default;
  virtual auto insert(LogPayload) -> LogIndex = 0;
  virtual auto waitFor(LogIndex) -> futures::Future<std::unique_ptr<LogRangeIterator>> = 0;
  virtual auto release(LogIndex) -> void = 0;
};

template <typename Spec>
struct LogDemultiplexer {
  explicit LogDemultiplexer(std::shared_ptr<LogInterface> const&);

  template <unsigned StreamId, typename D = stream_descriptor_by_id_t<StreamId, Spec>>
  auto getStream() -> std::shared_ptr<Stream<stream_descriptor_type_t<D>>> {
    return std::dynamic_pointer_cast<StreamImplementationBase<D>>(_container);
  }

  void digestIterator(LogRangeIterator& iter);

 private:
  template <typename Ds>
  struct StreamImplementationBase : Stream<stream_descriptor_type_t<Ds>> {
    static_assert(is_stream_descriptor_v<Ds>);
  };

  template <typename>
  struct StreamContainerBase;
  template <typename... Ds>
  struct StreamContainerBase<stream_descriptor_set<Ds...>>
      : virtual StreamImplementationBase<Ds>... {
    static_assert((is_stream_descriptor_v<Ds> && ...));
    virtual ~StreamContainerBase() = default;
    virtual void listen() = 0;
  };

  template <typename, typename>
  struct StreamImplementation;
  template <typename, typename>
  struct StreamContainer;

  using ContainerType = StreamContainer<Spec, std::make_index_sequence<Spec::length>>;

  auto getContainer() -> ContainerType&;

  std::shared_ptr<StreamContainerBase<Spec>> const _container;
};
}  // namespace arangodb::replication2::streams
