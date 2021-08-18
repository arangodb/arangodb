
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
    return T::fromVelocyPack(s);
  }
  auto operator()(serializer_tag_t<int>, velocypack::Slice s) -> int {
    return s.getNumericValue<int>();
  }
};

struct default_serializer {
  template <typename T, typename = void>
  void operator()(serializer_tag_t<T>, T const& t, velocypack::Builder& b) {
    t.toVelocyPack(b);
  }

  void operator()(serializer_tag_t<int>, int t, velocypack::Builder& b) {
    b.add(velocypack::Value(t));
  }
};

struct MyEntryType {
  static auto fromVelocyPack(velocypack::Slice s) -> MyEntryType {
    return MyEntryType{s.get("value").copyString()};
  }
  void toVelocyPack(velocypack::Builder& b) const {
    velocypack::ObjectBuilder ob(&b);
    b.add("value", velocypack::Value(value));
  }

  std::string value;
};

/* clang-format off */

inline constexpr auto my_stream_id = StreamId{1};
inline constexpr auto my_int_stream_tag = StreamTag{12};
inline constexpr auto my_old_int_stream_tag = StreamTag{13};


inline constexpr auto my_foo_stream_id = StreamId{8};
inline constexpr auto my_foo_tag = StreamTag{55};


using MyTestSpecification = stream_descriptor_set<
  stream_descriptor<my_stream_id, int, tag_descriptor_set<
    tag_descriptor<my_int_stream_tag, default_deserializer, default_serializer>,
    tag_descriptor<my_old_int_stream_tag, default_deserializer, default_serializer>
  >>,
  stream_descriptor<my_foo_stream_id, MyEntryType, tag_descriptor_set<
    tag_descriptor<my_foo_tag, default_deserializer, default_serializer>
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

TEST_F(LogDemuxTest, simple_test) {
  auto interface = std::make_shared<TestInsertInterfaceImpl>();

  auto mux = streams::LogMultiplexer<MyTestSpecification>::construct(interface);
  std::shared_ptr<ProducerStream<int>> streamA = mux->getStreamBaseById<my_stream_id>();
  auto streamB = mux->getStreamBaseById<my_foo_stream_id>();

  auto index = streamA->insert(12);
  EXPECT_EQ(interface->_promises.size(), 1);

  auto fA = streamA->waitFor(index);
  ASSERT_FALSE(fA.isReady());

  auto fB = streamB->waitFor(index + 1);
  ASSERT_FALSE(fB.isReady());

  interface->resolvePromises(index);
  ASSERT_TRUE(fA.isReady());
  ASSERT_FALSE(fB.isReady());

  auto index2 = streamB->insert(MyEntryType{"hello world"});

  interface->resolvePromises(index2);
  ASSERT_TRUE(fB.isReady());
}
