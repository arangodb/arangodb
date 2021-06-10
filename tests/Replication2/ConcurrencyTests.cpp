//
// Created by lars on 30/04/2021.
//
#include "Replication2/ReplicatedLog/LogParticipantI.h"
#include "Replication2/ReplicatedLog/types.h"
#include "TestHelper.h"

using namespace arangodb;
using namespace arangodb::replication2;
using namespace arangodb::replication2::replicated_log;


struct ReplicatedLogConcurrentTest : ReplicatedLogTest {
  using ThreadIdx = uint16_t;
  using IterIdx = uint32_t;
  constexpr static auto maxIter = std::numeric_limits<IterIdx>::max();

  struct alignas(128) ThreadCoordinationData {
    // the testee
    std::shared_ptr<LogParticipantI> log;

    // only when set to true, all client threads start
    std::atomic<bool> go = false;
    // when set to true, client threads will stop after the current iteration,
    // whatever that means for them.
    std::atomic<bool> stopClientThreads = false;
    // when set to true, the replication thread stops. should be done only
    // after all client threads stopped to avoid them hanging while waiting on
    // replication.
    std::atomic<bool> stopReplicationThreads = false;
    // every thread increases this by one when it's ready to start
    std::atomic<std::size_t> threadsReady = 0;
    // every thread increases this by one when it's done a certain minimal
    // amount of work. This is to guarantee that all threads are running long
    // enough side by side.
    std::atomic<std::size_t> threadsSatisfied = 0;
  };

  // Used to generate payloads that are unique across threads.
  constexpr static auto genPayload = [](ThreadIdx thread, IterIdx i) {
    // This should fit into short string optimization
    auto str = std::string(16, ' ');

    auto p = str.begin() + 5;
    // up to 5 digits for thread
    static_assert(std::numeric_limits<ThreadIdx>::max() <= 99'999);
    // 1 digit for ':'
    *p = ':';
    // up to 10 digits for i
    static_assert(std::numeric_limits<IterIdx>::max() <= 99'999'999'999);

    do {
      --p;
      *p = static_cast<char>('0' + (thread % 10));
      thread /= 10;
    } while (thread > 0);
    p = str.end();
    do {
      --p;
      *p = static_cast<char>('0' + (i % 10));
      i /= 10;
    } while (i > 0);

    return str;
  };

  constexpr static auto alternatinglyInsertAndRead = [](ThreadIdx const threadIdx,
                                                        ThreadCoordinationData& data) {
    using namespace std::chrono_literals;
    auto log = std::dynamic_pointer_cast<DelayedLogLeader>(data.log);
    ASSERT_NE(log, nullptr);
    data.threadsReady.fetch_add(1);
    while (!data.go.load()) {
    }

    for (auto i = std::uint32_t{0}; i < maxIter && !data.stopClientThreads.load(); ++i) {
      auto const payload = LogPayload{genPayload(threadIdx, i)};
      auto const idx = log->insert(payload);
      std::this_thread::sleep_for(1ns);
      auto fut = log->waitFor(idx);
      fut.get();
      auto snapshot = log->getReplicatedLogSnapshot();
      ASSERT_LT(0, idx.value);
      ASSERT_LE(idx.value, snapshot.size());
      auto const& entry = snapshot[idx.value - 1];
      EXPECT_EQ(idx, entry.logIndex());
      EXPECT_EQ(payload, entry.logPayload());
      if (i == 1000) {
        // we should have done at least a few iterations before finishing
        data.threadsSatisfied.fetch_add(1, std::memory_order_relaxed);
      }
    }
  };

  constexpr static auto insertManyThenRead = [](ThreadIdx const threadIdx,
                                                ThreadCoordinationData& data) {
    using namespace std::chrono_literals;
    auto log = std::dynamic_pointer_cast<DelayedLogLeader>(data.log);
    ASSERT_NE(log, nullptr);
    data.threadsReady.fetch_add(1);
    while (!data.go.load()) {
    }

    constexpr auto batch = 100;
    auto idxs = std::vector<LogIndex>{};
    idxs.resize(batch);

    for (auto i = std::uint32_t{0};
         i < maxIter && !data.stopClientThreads.load(); i += batch) {
      for (auto k = 0; k < batch && i + k < maxIter; ++k) {
        auto const payload = LogPayload{genPayload(threadIdx, i + k)};
        idxs[k] = log->insert(payload);
      }
      std::this_thread::sleep_for(1ns);
      auto fut = log->waitFor(idxs.back());
      fut.get();
      auto snapshot = log->getReplicatedLogSnapshot();
      for (auto k = 0; k < batch && i + k < maxIter; ++k) {
        auto const payload = LogPayload{genPayload(threadIdx, i + k)};
        auto const idx = idxs[k];
        ASSERT_LT(0, idx.value);
        ASSERT_LE(idx.value, snapshot.size());
        auto const& entry = snapshot[idx.value - 1];
        EXPECT_EQ(idx, entry.logIndex());
        EXPECT_EQ(payload, entry.logPayload()) << VPackSlice(payload.dummy.data()).toJson() << " " << VPackSlice(entry.logPayload().dummy.data()).toJson();
      }
      if (i == 10 * batch) {
        // we should have done at least a few iterations before finishing
        data.threadsSatisfied.fetch_add(1, std::memory_order_relaxed);
      }
    }
  };

  constexpr static auto runReplicationWithIntermittentPauses =
      [](ThreadCoordinationData& data) {
        using namespace std::chrono_literals;
        auto log = std::dynamic_pointer_cast<DelayedLogLeader>(data.log);
        ASSERT_NE(log, nullptr);
        for (auto i = 0;; ++i) {
          log->runAsyncStep();
          if (i % 16) {
            std::this_thread::sleep_for(100ns);
            if (data.stopReplicationThreads.load()) {
              return;
            }
          }
        }
      };

  constexpr static auto runFollowerReplicationWithIntermittentPauses =
      // NOLINTNEXTLINE(performance-unnecessary-value-param)
      [](std::vector<DelayedFollowerLog*> followers, ThreadCoordinationData& data) {
        using namespace std::chrono_literals;
        for (auto i = 0;;) {
          for (auto* follower : followers) {
            follower->runAsyncAppendEntries();
            if (i % 17) {
              std::this_thread::sleep_for(100ns);
              if (data.stopReplicationThreads.load()) {
                return;
              }
            }
            ++i;
          }
        }
      };
};

TEST_F(ReplicatedLogConcurrentTest, genPayloadTest) {
  EXPECT_EQ("    0:         0", genPayload(0, 0));
  EXPECT_EQ("   11:        42", genPayload(11, 42));
  EXPECT_EQ("65535:4294967295", genPayload(65535, 4294967295));
}

TEST_F(ReplicatedLogConcurrentTest, lonelyLeader) {
  using namespace std::chrono_literals;

  auto replicatedLog = makeReplicatedLog(LogId{1});
  // TODO this test hangs because there is not local follower currently
  auto leaderLog = replicatedLog->becomeLeader("leader", LogTerm{1}, {}, 1);

  auto data = ThreadCoordinationData{leaderLog};

  // start replication
  auto replicationThread =
      std::thread{runReplicationWithIntermittentPauses, std::ref(data)};

  std::vector<std::thread> clientThreads;
  auto threadCounter = uint16_t{0};
  clientThreads.emplace_back(alternatinglyInsertAndRead, threadCounter++, std::ref(data));
  clientThreads.emplace_back(insertManyThenRead, threadCounter++, std::ref(data));

  ASSERT_EQ(clientThreads.size(), threadCounter);
  while (data.threadsReady.load() < clientThreads.size()) {
  }
  data.go.store(true);
  while (data.threadsSatisfied.load() < clientThreads.size()) {
    std::this_thread::sleep_for(100us);
  }
  data.stopClientThreads.store(true);

  for (auto& thread : clientThreads) {
    thread.join();
  }

  // stop replication only after all client threads joined, so we don't block
  // them in some intermediate state
  data.stopReplicationThreads.store(true);
  replicationThread.join();

  auto stats = std::get<LeaderStatus>(data.log->getStatus()).local;
  EXPECT_LE(LogIndex{8000}, stats.commitIndex);
  EXPECT_LE(stats.commitIndex, stats.spearHead.index);
}

TEST_F(ReplicatedLogConcurrentTest, leaderWithFollowers) {
  using namespace std::chrono_literals;

  auto leaderLog = makeReplicatedLog(LogId{1});
  auto follower1Log = makeReplicatedLog(LogId{2});
  auto follower2Log = makeReplicatedLog(LogId{3});

  auto follower1 = follower1Log->becomeFollower("follower1", LogTerm{1}, "leader");
  auto follower2 = follower2Log->becomeFollower("follower2", LogTerm{1}, "leader");
  auto leader =
      leaderLog->becomeLeader("leader", LogTerm{1},
                              {std::static_pointer_cast<AbstractFollower>(follower1),
                               std::static_pointer_cast<AbstractFollower>(follower2)},
                              2);

  auto data = ThreadCoordinationData{leader};

  // start replication
  auto replicationThread =
      std::thread{runReplicationWithIntermittentPauses, std::ref(data)};
  auto followerReplicationThread =
      std::thread{runFollowerReplicationWithIntermittentPauses,
                  std::vector{follower1.get(), follower2.get()}, std::ref(data)};

  std::vector<std::thread> clientThreads;
  auto threadCounter = uint16_t{0};
  clientThreads.emplace_back(alternatinglyInsertAndRead, threadCounter++, std::ref(data));
  clientThreads.emplace_back(insertManyThenRead, threadCounter++, std::ref(data));

  ASSERT_EQ(clientThreads.size(), threadCounter);
  while (data.threadsReady.load() < clientThreads.size()) {
  }
  data.go.store(true);
  while (data.threadsSatisfied.load() < clientThreads.size()) {
    std::this_thread::sleep_for(100us);
  }
  data.stopClientThreads.store(true);

  for (auto& thread : clientThreads) {
    thread.join();
  }

  // stop replication only after all client threads joined, so we don't block
  // them in some intermediate state
  data.stopReplicationThreads.store(true);
  replicationThread.join();
  followerReplicationThread.join();

  auto stats = std::get<LeaderStatus>(data.log->getStatus()).local;
  EXPECT_LE(LogIndex{8000}, stats.commitIndex);
  EXPECT_LE(stats.commitIndex, stats.spearHead.index);
}
