
#include <gtest/gtest.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include <Replication2/Mocks/PersistedLog.h>
#include <Replication2/Mocks/ReplicatedLogMetricsMock.h>

#include <Replication2/ReplicatedLog/ReplicatedLog.h>

#include <Basics/Exceptions.h>
#include <Replication2/LogMultiplexer.h>

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::streams;

struct LogDemuxTest : ::testing::Test {
  static auto createReplicatedLog()
      -> std::shared_ptr<replication2::replicated_log::ReplicatedLog> {
    auto persisted = std::make_shared<test::MockLog>(LogId{0});
    auto core = std::make_unique<replicated_log::LogCore>(persisted);
    auto metrics = std::make_shared<ReplicatedLogMetricsMock>();
    return std::make_shared<replicated_log::ReplicatedLog>(std::move(core), metrics,
                                                           LoggerContext(Logger::REPLICATION2));
  }
};

struct default_deserializer {
  template <typename T>
  auto operator()(serializer_tag_t<T>, velocypack::Slice s) -> T {
    return s.extract<T>();
  }
};

struct default_serializer {
  template <typename T>
  void operator()(serializer_tag_t<T>, T const& t, velocypack::Builder& b) {
    b.add(velocypack::Value(t));
  }
};

/* clang-format off */

inline constexpr auto my_int_stream_id = StreamId{1};
inline constexpr auto my_int_stream_tag = StreamTag{12};

inline constexpr auto my_string_stream_id = StreamId{8};
inline constexpr auto my_string_stream_tag = StreamTag{55};


using MyTestSpecification = stream_descriptor_set<
  stream_descriptor<my_int_stream_id, int, tag_descriptor_set<
    tag_descriptor<my_int_stream_tag, default_deserializer, default_serializer>
  >>,
  stream_descriptor<my_string_stream_id, std::string, tag_descriptor_set<
    tag_descriptor<my_string_stream_tag, default_deserializer, default_serializer>
  >>
>;

/* clang-format on */

#include <Replication2/LogMultiplexer.tpp>
template struct streams::LogDemultiplexer<MyTestSpecification>;
template struct streams::LogMultiplexer<MyTestSpecification>;

TEST_F(LogDemuxTest, leader_follower_test) {
  auto ints = {12, 13, 14, 15, 16};
  auto strings = {"foo", "bar", "baz", "fuz"};

  auto leaderLog = createReplicatedLog();
  auto followerLog = createReplicatedLog();

  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto leader =
      leaderLog->becomeLeader(LogConfig(2, false), "leader", LogTerm{1}, {follower});

  auto mux = LogMultiplexer<MyTestSpecification>::construct(leader);
  auto demux = LogDemultiplexer<MyTestSpecification>::construct(follower);

  auto leaderStreamA = mux->getStreamBaseById<my_int_stream_id>();
  auto leaderStreamB = mux->getStreamBaseById<my_string_stream_id>();

  {
    auto iterA = ints.begin();
    auto iterB = strings.begin();
    while (iterA != ints.end() || iterB != strings.end()) {
      if (iterA != ints.end()) {
        leaderStreamA->insert(*iterA);
        ++iterA;
      }
      if (iterB != strings.end()) {
        leaderStreamB->insert(*iterB);
        ++iterB;
      }
    }
  }

  auto followerStreamA = demux->getStreamBaseById<my_int_stream_id>();
  auto followerStreamB = demux->getStreamBaseById<my_string_stream_id>();

  auto futureA = followerStreamA->waitFor(LogIndex{2});
  auto futureB = followerStreamB->waitFor(LogIndex{1});
  ASSERT_FALSE(futureA.isReady());
  ASSERT_FALSE(futureB.isReady());

  demux->listen();
  ASSERT_TRUE(futureA.isReady());
  ASSERT_TRUE(futureB.isReady());

  {
    auto iter = followerStreamA->getAllEntriesIterator();
    for (auto x : ints) {
      auto entry = iter->next();
      ASSERT_TRUE(entry.has_value()) << "expected value " << x;
      auto const &[index, value] = *entry;
      EXPECT_EQ(value, x);
    }
    EXPECT_EQ(iter->next(), std::nullopt);
  }
  {
    auto iter = followerStreamB->getAllEntriesIterator();
    for (auto x : strings) {
      auto entry = iter->next();
      ASSERT_TRUE(entry.has_value());
      auto const &[index, value] = *entry;
      EXPECT_EQ(value, x);
    }
    EXPECT_EQ(iter->next(), std::nullopt);
  }

  LOG_DEVEL << leader->copyInMemoryLog().dump();
}
