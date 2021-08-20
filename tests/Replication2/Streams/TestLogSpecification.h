////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020-2021 ArangoDB GmbH, Cologne, Germany
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

#include <velocypack/Slice.h>
#include <velocypack/Builder.h>

#include <Replication2/Streams/StreamSpecification.h>
#include <Replication2/Streams/LogMultiplexer.h>

namespace arangodb::replication2::test {

inline constexpr auto my_int_stream_tag = streams::StreamTag{12};
inline constexpr auto my_string_stream_id = streams::StreamId{8};

inline constexpr auto my_string_stream_tag = streams::StreamTag{55};
inline constexpr auto my_int_stream_id = streams::StreamId{1};


struct default_deserializer {
  template <typename T>
  auto operator()(streams::serializer_tag_t<T>, velocypack::Slice s) -> T {
    return s.extract<T>();
  }
};

struct default_serializer {
  template <typename T>
  void operator()(streams::serializer_tag_t<T>, T const& t, velocypack::Builder& b) {
    b.add(velocypack::Value(t));
  }
};


/* clang-format off */

using MyTestSpecification = streams::stream_descriptor_set<
    streams::stream_descriptor<my_int_stream_id, int, streams::tag_descriptor_set<
    streams::tag_descriptor<my_int_stream_tag, default_deserializer, default_serializer>
    >>,
    streams::stream_descriptor<my_string_stream_id, std::string, streams::tag_descriptor_set<
    streams::tag_descriptor<my_string_stream_tag, default_deserializer, default_serializer>
    >>
    >;

/* clang-format on */

}  // namespace arangodb::replication2::test

extern template struct arangodb::replication2::streams::LogMultiplexer<arangodb::replication2::test::MyTestSpecification>;
extern template struct arangodb::replication2::streams::LogDemultiplexer<arangodb::replication2::test::MyTestSpecification>;
