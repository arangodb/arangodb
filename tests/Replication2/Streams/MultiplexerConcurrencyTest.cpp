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

#include <gtest/gtest.h>

#include <Basics/VelocyPackHelper.h>
#include <Replication2/Mocks/AsyncFollower.h>
#include <utility>

#include "Replication2/ReplicatedLog/LogFollower.h"
#include "Replication2/ReplicatedLog/LogLeader.h"
#include "Replication2/Streams/LogMultiplexer.h"

#include "Replication2/Streams/TestLogSpecification.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;
using namespace arangodb::replication2::test;

struct LogMultiplexerConcurrencyTest : LogMultiplexerTestBase {
  using Spec = test::MyTestSpecification;

  template <streams::StreamId StreamId>
  struct StateMachine : std::enable_shared_from_this<StateMachine<StreamId>> {
    using ValueType = streams::stream_type_by_id_t<StreamId, Spec>;

    explicit StateMachine(std::shared_ptr<streams::Stream<ValueType>> stream)
        : _stream(std::move(stream)) {}

    void start() { waitForStream(LogIndex{1}); }

    void waitForStream(LogIndex next) {
      _stream->waitForIterator(next).thenValue([weak = this->weak_from_this()](auto&& iter) {
        if (auto self = weak.lock(); self) {
          auto [start, stop] = iter->range();
          TRI_ASSERT(start != stop);
          while (auto memtry = iter->next()) {
            self->_observedLog.emplace(*memtry);
          }
          self->waitForStream(stop);
        } else {
          TRI_ASSERT(false);
        }
      });
    }

    std::map<LogIndex, ValueType> _observedLog;
    std::shared_ptr<streams::Stream<ValueType>> _stream;
  };

  template <typename>
  struct StateCombiner;
  template <typename... Descriptors>
  struct StateCombiner<streams::stream_descriptor_set<Descriptors...>> {
    std::tuple<std::shared_ptr<StateMachine<Descriptors::id>>...> _states;

    template <typename Mux>
    explicit StateCombiner(std::shared_ptr<Mux> const& demux)
        : _states(std::make_shared<StateMachine<Descriptors::id>>(
              demux->template getStreamById<Descriptors::id>())...) {
      ((std::get<std::shared_ptr<StateMachine<Descriptors::id>>>(_states)->start()), ...);
    }
  };

  struct FollowerInstance {
    explicit FollowerInstance(std::shared_ptr<LogFollower> const& follower)
        : _follower(follower),
          _demux(streams::LogDemultiplexer<Spec>::construct(follower)),
          combiner(_demux) {
      _demux->listen();
    }

    std::shared_ptr<LogFollower> _follower;
    std::shared_ptr<streams::LogDemultiplexer<test::MyTestSpecification>> _demux;
    StateCombiner<Spec> combiner;
  };

  struct LeaderInstance {
    explicit LeaderInstance(std::shared_ptr<LogLeader> const& leader)
        : _leader(leader),
          _mux(streams::LogMultiplexer<Spec>::construct(leader)),
          combiner(_mux) {}

    std::shared_ptr<LogLeader> _leader;
    std::shared_ptr<streams::LogMultiplexer<test::MyTestSpecification>> _mux;
    StateCombiner<Spec> combiner;
  };
};

TEST_F(LogMultiplexerConcurrencyTest, test) {
  auto followerLog = createAsyncReplicatedLog(LogId{1});
  auto leaderLog = createAsyncReplicatedLog(LogId{2});

  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");
  auto asyncFollower = std::make_shared<AsyncFollower>(follower);

  auto leader = leaderLog->becomeLeader(LogConfig(2, 2, false), "leader",
                                        LogTerm{1}, {asyncFollower});

  auto followerInstance = std::make_shared<FollowerInstance>(follower);
  auto leaderInstance = std::make_shared<LeaderInstance>(leader);

  auto producer = leaderInstance->_mux->getStreamById<test::my_int_stream_id>();

  constexpr std::size_t num_threads = 8;
  constexpr std::size_t num_inserts_per_thread = 10000;
  constexpr auto lastIndex = LogIndex{num_threads * num_inserts_per_thread + 1};

  std::vector<std::thread> threads;
  std::generate_n(std::back_inserter(threads), num_threads, [&] {
    return std::thread([&, producer] {
      auto index = LogIndex{0};
      for (std::size_t i = 0; i < num_inserts_per_thread; i++) {
        index = producer->insert((int)i);
      }
      producer->waitFor(index).wait();
    });
  });

  std::for_each(std::begin(threads), std::end(threads),
                [](std::thread& t) { t.join(); });
  asyncFollower->waitFor(lastIndex).wait();
  asyncFollower->stop();

  auto iterA = follower->waitForIterator(LogIndex{1}).get();
  auto iterB = leader->waitForIterator(LogIndex{1}).get();

  EXPECT_EQ(iterA->range(), iterB->range());
  while (auto A = iterA->next()) {
    auto B = iterB->next();
    ASSERT_TRUE(B.has_value());
    EXPECT_EQ(A->logIndex(), B->logIndex());
    bool equal = basics::VelocyPackHelper::equal(A->logPayload(), B->logPayload(), true);
    EXPECT_TRUE(equal) << A->logPayload().toJson() << " " << B->logPayload().toJson();
  }
  EXPECT_FALSE(iterB->next().has_value());

  MyTestSpecification::for_each_descriptor([&](auto p) {
    using Descriptor = decltype(p);
    auto streamA = leaderInstance->_mux->getStreamByDescriptor<Descriptor>();
    auto streamB = followerInstance->_demux->getStreamByDescriptor<Descriptor>();

    auto iterA = streamA->waitForIterator(LogIndex{1}).get();
    auto iterB = streamB->waitForIterator(LogIndex{1}).get();

    EXPECT_EQ(iterA->range(), iterB->range());
    while (auto A = iterA->next()) {
      ASSERT_EQ(A, iterB->next());
    }
    EXPECT_FALSE(iterB->next().has_value());
  });
}
