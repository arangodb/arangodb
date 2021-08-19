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
#include "Replication2/Streams/Streams.h"

namespace arangodb::replication2::streams {

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

}
