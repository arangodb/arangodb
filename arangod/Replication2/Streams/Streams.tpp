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
 * @tparam Implementation Implementor Top Class
 * @tparam Descriptor Stream Descriptor
 * @tparam StreamInterface Stream<T> or ProducerStream<T>
 */
template<typename Implementation, typename Descriptor,
         template<typename> typename StreamInterface>
struct StreamGenericImplementationBase
    : virtual StreamGenericBase<Descriptor, StreamInterface> {
  static_assert(is_stream_descriptor_v<Descriptor>);

  using ValueType = stream_descriptor_type_t<Descriptor>;
  using Iterator = TypedLogRangeIterator<StreamEntryView<ValueType>>;
  using WaitForResult = typename StreamInterface<ValueType>::WaitForResult;

  auto waitForIterator(LogIndex index)
      -> futures::Future<std::unique_ptr<Iterator>> override final {
    return implementation().template waitForIteratorInternal<Descriptor>(index);
  }
  auto waitFor(LogIndex index)
      -> futures::Future<WaitForResult> override final {
    return implementation().template waitForInternal<Descriptor>(index);
  }
  auto release(LogIndex index) -> void override final {
    return implementation().template releaseInternal<Descriptor>(index);
  }
  auto getAllEntriesIterator() -> std::unique_ptr<Iterator> override final {
    return implementation().template getIteratorInternal<Descriptor>();
  }

 private:
  auto implementation() -> Implementation& {
    return static_cast<Implementation&>(*this);
  }
};

/**
 * Wrapper about StreamGenericImplementationBase, that adds depending on the
 * StreamInterface more methods. Is specialized for ProducerStream<T>.
 * @tparam Implementation Implementor Top Class
 * @tparam Descriptor Stream Descriptor
 * @tparam StreamInterface Stream<T> or ProducerStream<T>
 */
template<typename Implementation, typename Descriptor,
         template<typename> typename StreamInterface>
struct StreamGenericImplementation
    : StreamGenericImplementationBase<Implementation, Descriptor,
                                      StreamInterface> {};
template<typename Implementation, typename Descriptor>
struct StreamGenericImplementation<Implementation, Descriptor, ProducerStream>
    : StreamGenericImplementationBase<Implementation, Descriptor,
                                      ProducerStream> {
  using ValueType = stream_descriptor_type_t<Descriptor>;
  auto insert(ValueType const& t) -> LogIndex override {
    return static_cast<Implementation*>(this)
        ->template insertInternal<Descriptor>(t);
  }
};

template<typename Implementation, typename Descriptor>
using StreamImplementation =
    StreamGenericImplementation<Implementation, Descriptor, Stream>;
template<typename Implementation, typename Descriptor>
using ProducerStreamImplementation =
    StreamGenericImplementation<Implementation, Descriptor, ProducerStream>;

template<typename, typename, template<typename> typename>
struct ProxyStreamDispatcher;

/**
 * Class that implements all streams as virtual base classes.
 * @tparam Implementation
 * @tparam Streams
 * @tparam StreamInterface
 */
template<typename Implementation, typename... Streams,
         template<typename> typename StreamInterface>
struct ProxyStreamDispatcher<Implementation, stream_descriptor_set<Streams...>,
                             StreamInterface>
    : StreamGenericImplementation<Implementation, Streams, StreamInterface>... {
};

}  // namespace arangodb::replication2::streams
