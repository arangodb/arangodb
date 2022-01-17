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

#include <gtest/gtest.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <Replication2/Streams/LogMultiplexer.h>
#include <Replication2/Streams/StreamSpecification.h>

#include <Replication2/Mocks/FakeReplicatedLog.h>
#include <Replication2/Mocks/PersistedLog.h>
#include <Replication2/Mocks/ReplicatedLogMetricsMock.h>
#include <Mocks/LogLevels.h>

namespace arangodb::replication2::test {

struct LogMultiplexerTestBase
    : ::testing::Test,
      public ::arangodb::tests::LogSuppressor<Logger::REPLICATION2,
                                              LogLevel::ERR> {
  static auto createReplicatedLog(LogId id = LogId{0})
      -> std::shared_ptr<replication2::replicated_log::ReplicatedLog> {
    return createReplicatedLogImpl<replicated_log::ReplicatedLog,
                                   test::MockLog>(id);
  }

  static auto createAsyncReplicatedLog(LogId id = LogId{0})
      -> std::shared_ptr<replication2::replicated_log::ReplicatedLog> {
    return createReplicatedLogImpl<replicated_log::ReplicatedLog,
                                   test::AsyncMockLog>(id);
  }

  static auto createFakeReplicatedLog(LogId id = LogId{0})
      -> std::shared_ptr<replication2::test::TestReplicatedLog> {
    return createReplicatedLogImpl<replication2::test::TestReplicatedLog,
                                   test::MockLog>(id);
  }

 private:
  template<typename Impl, typename MockLog>
  static auto createReplicatedLogImpl(LogId id) -> std::shared_ptr<Impl> {
    auto persisted = std::make_shared<MockLog>(id);
    auto core = std::make_unique<replicated_log::LogCore>(persisted);
    auto metrics = std::make_shared<ReplicatedLogMetricsMock>();
    auto options = std::make_shared<ReplicatedLogGlobalSettings>();

    return std::make_shared<Impl>(std::move(core), metrics, options,
                                  LoggerContext(Logger::REPLICATION2));
  }
};

struct default_deserializer {
  template<typename T>
  auto operator()(streams::serializer_tag_t<T>, velocypack::Slice s) -> T {
    return s.extract<T>();
  }
};

struct default_serializer {
  template<typename T>
  void operator()(streams::serializer_tag_t<T>, T const& t,
                  velocypack::Builder& b) {
    b.add(velocypack::Value(t));
  }
};

inline constexpr auto my_int_stream_id = streams::StreamId{1};
inline constexpr auto my_string_stream_id = streams::StreamId{8};
inline constexpr auto my_string2_stream_id = streams::StreamId{9};

inline constexpr auto my_int_stream_tag = streams::StreamTag{12};
inline constexpr auto my_string_stream_tag = streams::StreamTag{55};
inline constexpr auto my_string2_stream_tag = streams::StreamTag{56};
inline constexpr auto my_string2_stream_tag2 = streams::StreamTag{58};

/* clang-format off */

using MyTestSpecification = streams::stream_descriptor_set<
      streams::stream_descriptor<my_int_stream_id, int, streams::tag_descriptor_set<
        streams::tag_descriptor<my_int_stream_tag, default_deserializer, default_serializer>
      >>,
      streams::stream_descriptor<my_string_stream_id, std::string, streams::tag_descriptor_set<
        streams::tag_descriptor<my_string_stream_tag, default_deserializer, default_serializer>
      >>,
      streams::stream_descriptor<my_string2_stream_id, std::string, streams::tag_descriptor_set<
        streams::tag_descriptor<my_string2_stream_tag, default_deserializer, default_serializer>,
        streams::tag_descriptor<my_string2_stream_tag2, default_deserializer, default_serializer>
      >>
    >;

/* clang-format on */

}  // namespace arangodb::replication2::test

extern template struct arangodb::replication2::streams::LogMultiplexer<
    arangodb::replication2::test::MyTestSpecification>;
extern template struct arangodb::replication2::streams::LogDemultiplexer<
    arangodb::replication2::test::MyTestSpecification>;
