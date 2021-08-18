
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
  auto operator()(serializer_tag_t<int>, velocypack::Slice s) -> int {
    return s.getNumericValue<int>();
  }
  auto operator()(serializer_tag_t<std::string>, velocypack::Slice s) -> std::string {
    return s.copyString();
  }
};

struct default_serializer {

  void operator()(serializer_tag_t<int>, int t, velocypack::Builder& b) {
    b.add(velocypack::Value(t));
  }
  void operator()(serializer_tag_t<std::string>, std::string const& t,
                  velocypack::Builder& b) {
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
template struct streams::LogDemultiplexer2<MyTestSpecification>;
template struct streams::LogMultiplexer<MyTestSpecification>;

struct TestInsertInterfaceImpl : TestInsertInterface {
  auto insert(LogPayload p) -> LogIndex override {
    auto index = LogIndex{_logEntries.size() + 1};
    _logEntries.push_back(std::move(p));
    return index;
  }

  auto waitFor(LogIndex index) -> futures::Future<replicated_log::WaitForResult> override {
    return _promises
        .emplace(index, futures::Promise<replicated_log::WaitForResult>{})
        ->second.getFuture();
  }

  auto resolvePromises(LogIndex index) {
    auto const end = _promises.upper_bound(index);
    for (auto it = _promises.begin(); it != end;) {
      it->second.setValue(replicated_log::WaitForResult(index, nullptr));
      it = _promises.erase(it);
    }
  }

  std::vector<LogPayload> _logEntries;
  std::multimap<LogIndex, futures::Promise<replicated_log::WaitForResult>> _promises;
};

TEST_F(LogDemuxTest, leader_follower_test) {
  auto leaderLog = createReplicatedLog();
  auto followerLog = createReplicatedLog();

  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto leader =
      leaderLog->becomeLeader(LogConfig(2, false), "leader", LogTerm{1}, {follower});

  auto mux = LogMultiplexer<MyTestSpecification>::construct(leader);
  auto demux = LogDemultiplexer2<MyTestSpecification>::construct(follower);

  auto leaderStreamA = mux->getStreamById<my_int_stream_id>();
  auto leaderStreamB = mux->getStreamById<my_string_stream_id>();

  leaderStreamA->insert(12);
  leaderStreamB->insert("foo");
  leaderStreamA->insert(13);
  leaderStreamB->insert("bar");
  leaderStreamA->insert(14);
  leaderStreamB->insert("baz");
  leaderStreamA->insert(15);
  leaderStreamB->insert("fuz");
  leaderStreamA->insert(16);

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
    auto iter = followerStreamA->getIterator();
    for (auto x : {12, 13, 14, 15, 16}) {
      auto entry = iter->next();
      ASSERT_TRUE(entry.has_value());
      EXPECT_EQ(entry->value, x);
    }
    EXPECT_EQ(iter->next(), std::nullopt);
  }
  {
    auto iter = followerStreamB->getIterator();
    for (auto x : {"foo", "bar", "baz", "fuz"}) {
      auto entry = iter->next();
      ASSERT_TRUE(entry.has_value());
      EXPECT_EQ(entry->value, x);
    }
    EXPECT_EQ(iter->next(), std::nullopt);
  }
}
