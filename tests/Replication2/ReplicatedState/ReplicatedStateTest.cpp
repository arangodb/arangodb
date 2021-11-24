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

#include <string>
#include <unordered_map>
#include <utility>

#include <gtest/gtest.h>

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>

#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Basics/voc-errors.h"
#include "Futures/Future.h"

#include "Mocks/LogLevels.h"

#include "Replication2/ReplicatedLog/ReplicatedLog.h"
#include "Replication2/ReplicatedLog/TestHelper.h"
#include "Replication2/Streams/LogMultiplexer.h"
#include "Replication2/Streams/StreamSpecification.h"
#include "Replication2/Streams/Streams.h"

using namespace arangodb;
using namespace arangodb::replication2;

template <typename T>
struct EntryDeserializer {};
template <typename T>
struct EntrySerializer {};

template <typename S>
struct ReplicatedStateTraits {
  using FactoryType = typename S::FactoryType;
  using LeaderType = typename S::LeaderType;
  using FollowerType = typename S::FollowerType;
  using EntryType = typename S::EntryType;
  using Deserializer = EntryDeserializer<EntryType>;
  using Serializer = EntrySerializer<EntryType>;
};

template <typename S>
using ReplicatedStateStreamSpec = streams::stream_descriptor_set<streams::stream_descriptor<
    streams::StreamId{1}, typename ReplicatedStateTraits<S>::EntryType,
    streams::tag_descriptor_set<streams::tag_descriptor<1, typename ReplicatedStateTraits<S>::Deserializer, typename ReplicatedStateTraits<S>::Serializer>>>>;

template <typename S>
struct ReplicatedLeaderState;

template <typename S>
struct ReplicatedFollowerState;

struct ReplicatedStateBase {
  virtual ~ReplicatedStateBase() = default;
};

template <typename S, typename Factory = typename ReplicatedStateTraits<S>::FactoryType>
struct ReplicatedState : ReplicatedStateBase,
                         std::enable_shared_from_this<ReplicatedState<S, Factory>> {
  explicit ReplicatedState(std::shared_ptr<replicated_log::ReplicatedLog> log,
                           std::shared_ptr<Factory> factory)
      : log(std::move(log)), factory(std::move(factory)) {}

  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using FollowerType = typename ReplicatedStateTraits<S>::FollowerType;

  void flush() {
    auto participant = log->getParticipant();
    if (auto leader = std::dynamic_pointer_cast<replicated_log::LogLeader>(participant); leader) {
      runLeader(std::move(leader));
    } else if (auto follower = std::dynamic_pointer_cast<replicated_log::LogFollower>(participant);
               follower) {
      runFollower(std::move(follower));
    } else {
      // unconfigured
      std::abort();
    }
  }

  auto getFollower() const -> std::shared_ptr<FollowerType> {
    if (auto machine = std::dynamic_pointer_cast<FollowerState>(currentState); machine) {
      return std::static_pointer_cast<FollowerType>(machine->state);
    }
    return nullptr;
  }

 private:
  friend struct ReplicatedFollowerState<S>;
  friend struct ReplicatedLeaderState<S>;

  struct StateBase {
    virtual ~StateBase() = default;
  };

  struct LeaderState;

  void runLeader(std::shared_ptr<replicated_log::LogLeader> logLeader);

  struct FollowerState;

  void runFollower(std::shared_ptr<replicated_log::LogFollower> logFollower);

  std::shared_ptr<StateBase> currentState;
  std::shared_ptr<replicated_log::ReplicatedLog> log;
  std::shared_ptr<Factory> factory;
};

template <typename S>
struct ReplicatedLeaderState {
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Stream = streams::Stream<EntryType>;
  using EntryIterator = typename Stream::Iterator;
  virtual ~ReplicatedLeaderState() = default;

 protected:
  friend struct ReplicatedState<S>::LeaderState;
  virtual auto installSnapshot(ParticipantId const& destination)
      -> futures::Future<Result> = 0;
  virtual auto recoverEntries(std::unique_ptr<EntryIterator>)
      -> futures::Future<Result> = 0;
};

template <typename S>
struct ReplicatedFollowerState {
  using EntryType = typename ReplicatedStateTraits<S>::EntryType;
  using Stream = streams::Stream<EntryType>;
  using EntryIterator = typename Stream::Iterator;

  virtual ~ReplicatedFollowerState() = default;

 protected:
  friend struct ReplicatedState<S>::FollowerState;
  virtual auto applyEntries(std::unique_ptr<EntryIterator>)
      -> futures::Future<Result> = 0;
};

template <typename S, typename F>
struct ReplicatedState<S, F>::LeaderState : StateBase,
                                            std::enable_shared_from_this<LeaderState> {
  explicit LeaderState(std::shared_ptr<ReplicatedState> const& parent,
                       std::shared_ptr<replicated_log::LogLeader> leader)
      : _parent(parent), _leader(std::move(leader)) {}

  using Stream = streams::Stream<EntryType>;
  using Iterator = typename Stream::Iterator;

  void run() {
    // 1. wait for leadership established
    // 1.2. digest available entries into multiplexer
    // 2. construct leader state
    // 2.2 apply all log entries of the previous term
    // 3. make leader state available
    // 2. on configuration change, update
    LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
        << "LeaderState waiting for leadership to be established";
    _leader->waitForLeadership()
        .thenValue([self = this->shared_from_this()](auto&& result) {
          LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
              << "LeaderState established";
          self->_mux = Multiplexer::construct(self->_leader);
          self->_mux->digestAvailableEntries();
          if (auto parent = self->_parent.lock(); parent) {
            self->_machine = parent->factory->constructLeader();
            auto stream = self->_mux->template getStreamById<1>();
            LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
                << "receiving committed entries for recovery";
            return stream->waitForIterator(LogIndex{0}).thenValue([self](std::unique_ptr<Iterator>&& result) {
              LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
                  << "starting recovery";
              return self->_machine->recoverEntries(std::move(result));
            });
          }
          return futures::Future<Result>{TRI_ERROR_REPLICATION_LEADER_CHANGE};
        })
        .thenValue([self = this->shared_from_this()](Result&& result) {
          LOG_TOPIC("fb593", TRACE, Logger::REPLICATED_STATE)
              << "recovery completed";
          LOG_TOPIC("fb593", TRACE, Logger::REPLICATED_STATE)
              << "starting leader service";
        });
  }

  using Multiplexer = streams::LogMultiplexer<ReplicatedStateStreamSpec<S>>;

  std::shared_ptr<ReplicatedLeaderState<S>> _machine;
  std::shared_ptr<Multiplexer> _mux;
  std::weak_ptr<ReplicatedState> _parent;
  std::shared_ptr<replicated_log::LogLeader> _leader;
};

template <typename S, typename F>
struct ReplicatedState<S, F>::FollowerState
    : StateBase,
      std::enable_shared_from_this<FollowerState> {
  using Stream = streams::Stream<EntryType>;
  using Iterator = typename Stream::Iterator;

  LogIndex _nextEntry{0};
  FollowerState(std::shared_ptr<Stream> stream,
                std::shared_ptr<ReplicatedFollowerState<S>> state,
                std::weak_ptr<ReplicatedState> parent)
      : stream(std::move(stream)), state(std::move(state)), parent(std::move(parent)) {}

  std::shared_ptr<Stream> stream;
  std::shared_ptr<ReplicatedFollowerState<S>> state;
  std::weak_ptr<ReplicatedState> parent;

  void run() {
    LOG_TOPIC("53ba0", TRACE, Logger::REPLICATED_STATE)
        << "FollowerState wait for iterator starting at " << _nextEntry;
    stream->waitForIterator(_nextEntry)
        .thenValue([this](std::unique_ptr<Iterator> iter) {
          auto [from, to] = iter->range();
          LOG_TOPIC("6626f", TRACE, Logger::REPLICATED_STATE)
              << "State machine received log range " << iter->range();
          // call into state machine to apply entries
          return state->applyEntries(std::move(iter)).thenValue([to = to](Result&& res) {
            return std::make_pair(to, res);
          });
        })
        .thenFinal([self = this->shared_from_this()](
                       futures::Try<std::pair<LogIndex, Result>>&& tryResult) {
          try {
            auto& [index, res] = tryResult.get();
            if (res.ok()) {
              self->_nextEntry = index;  // commit that index
              self->stream->release(index.saturatedDecrement());
              LOG_TOPIC("ab737", TRACE, Logger::REPLICATED_STATE)
                  << "State machine accepted log entries, next entry is " << index;
            } else {
              LOG_TOPIC("e8777", ERR, Logger::REPLICATED_STATE)
                  << "Follower state machine returned result " << res;
            }
            self->run();
          } catch (basics::Exception const& e) {
            if (e.code() == TRI_ERROR_REPLICATION_LEADER_CHANGE) {
              if (auto ptr = self->parent.lock(); ptr) {
                ptr->flush();
              } else {
                LOG_TOPIC("15cb4", DEBUG, Logger::REPLICATED_STATE)
                    << "LogFollower resigned, but Replicated State already "
                       "gone";
              }
            } else {
              LOG_TOPIC("e73bc", ERR, Logger::REPLICATED_STATE)
                  << "Unexpected exception in applyEntries: " << e.what();
            }
          } catch (std::exception const& e) {
            LOG_TOPIC("e73bc", ERR, Logger::REPLICATED_STATE)
                << "Unexpected exception in applyEntries: " << e.what();
          } catch (...) {
            LOG_TOPIC("4d2b7", ERR, Logger::REPLICATED_STATE)
                << "Unexpected exception";
          }
        });
  }
};

template <typename S, typename Factory>
void ReplicatedState<S, Factory>::runFollower(std::shared_ptr<replicated_log::LogFollower> logFollower) {
  // 1. construct state follower
  // 2. wait for new entries, call applyEntries
  // 3. forward release index after the entries have been applied
  LOG_TOPIC("95b9d", DEBUG, Logger::REPLICATED_STATE)
      << "create follower state";
  auto state = factory->constructFollower();
  auto demux = streams::LogDemultiplexer<ReplicatedStateStreamSpec<S>>::construct(logFollower);
  auto stream = demux->template getStreamById<1>();
  auto machine = std::make_shared<FollowerState>(stream, state, this->weak_from_this());
  machine->run();
  demux->listen();
  currentState = machine;
}

template <typename S, typename Factory>
void ReplicatedState<S, Factory>::runLeader(std::shared_ptr<replicated_log::LogLeader> logLeader) {
  LOG_TOPIC("95b9d", DEBUG, Logger::REPLICATED_STATE) << "create leader state";
  auto machine = std::make_shared<LeaderState>(this->shared_from_this(), logLeader);
  machine->run();
  currentState = machine;
}

template <typename Char>
struct TransparentBasicStringHash {
  using is_transparent = void;
  using hash_type = std::hash<std::basic_string_view<Char>>;
  auto operator()(std::basic_string<Char> const& str) const noexcept {
    return hash_type{}(str);
  }
  auto operator()(std::basic_string_view<Char> const& str) const noexcept {
    return hash_type{}(str);
  }
  auto operator()(const Char* str) const noexcept { return hash_type{}(str); }
};

using TransparentStringHash = TransparentBasicStringHash<char>;

struct ReplicatedStateFeature {
  template <typename S, typename... Args>
  void registerStateType(std::string name, Args&&... args) {
    using Factory = typename ReplicatedStateTraits<S>::FactoryType;
    static_assert(std::is_constructible_v<Factory, Args...>);
    auto factory =
        std::make_shared<InternalFactory<S>>(std::in_place, std::forward<Args>(args)...);
    factories.emplace(std::move(name), std::move(factory));
  }

  auto createReplicatedState(std::string_view name,
                             std::shared_ptr<replicated_log::ReplicatedLog> log)
      -> std::shared_ptr<ReplicatedStateBase> {
    if (auto iter = factories.find(name); iter != std::end(factories)) {
      return iter->second->createReplicatedState(std::move(log));
    }
    return nullptr;
  }

 private:
  struct InternalFactoryBase : std::enable_shared_from_this<InternalFactoryBase> {
    virtual ~InternalFactoryBase() = default;
    virtual auto createReplicatedState(std::shared_ptr<replicated_log::ReplicatedLog>)
        -> std::shared_ptr<ReplicatedStateBase> = 0;
  };

  template <typename S, typename Factory = typename ReplicatedStateTraits<S>::FactoryType>
  struct InternalFactory : InternalFactoryBase, private Factory {
    template <typename... Args>
    explicit InternalFactory(std::in_place_t, Args&&... args)
        : Factory(std::forward<Args>(args)...) {}

    auto createReplicatedState(std::shared_ptr<replicated_log::ReplicatedLog> log)
        -> std::shared_ptr<ReplicatedStateBase> override {
      return std::make_shared<ReplicatedState<S>>(std::move(log), getStateFactory());
    }

    auto getStateFactory() -> std::shared_ptr<Factory> {
      return {shared_from_this(), static_cast<Factory*>(this)};
    }
  };

  std::unordered_map<std::string, std::shared_ptr<InternalFactoryBase>, TransparentStringHash, std::equal_to<void>> factories;
};

#include "Replication2/Streams/LogMultiplexer.tpp"

struct MyFactory;
struct MyEntryType;
struct MyLeaderState;
struct MyFollowerState;

struct MyState {
  using LeaderType = MyLeaderState;
  using FollowerType = MyFollowerState;
  using EntryType = MyEntryType;
  using FactoryType = MyFactory;
};

struct MyEntryType {
  std::string key, value;
};

template <>
struct EntryDeserializer<MyEntryType> {
  auto operator()(streams::serializer_tag_t<MyEntryType>, velocypack::Slice s) const
      -> MyEntryType {
    auto key = s.get("key").copyString();
    auto value = s.get("value").copyString();
    return MyEntryType{.key = key, .value = value};
  }
};

template <>
struct EntrySerializer<MyEntryType> {
  auto operator()(streams::serializer_tag_t<MyEntryType>, MyEntryType const& e,
                  velocypack::Builder& b) const {
    velocypack::ObjectBuilder ob(&b);
    b.add("key", velocypack::Value(e.key));
    b.add("value", velocypack::Value(e.value));
  }
};

struct MyStateBase {
  virtual ~MyStateBase() = default;
  std::unordered_map<std::string, std::string> store;
};

struct MyLeaderState : MyStateBase, ReplicatedLeaderState<MyState> {
 protected:
  auto installSnapshot(ParticipantId const& destination) -> futures::Future<Result> override {
    return futures::Future<Result>{TRI_ERROR_NO_ERROR};
  }

  auto recoverEntries(std::unique_ptr<EntryIterator> ptr) -> futures::Future<Result> override {
    LOG_DEVEL << "recover from log";
    return futures::Future<Result>{TRI_ERROR_NO_ERROR};
  }
};

struct MyFollowerState : MyStateBase, ReplicatedFollowerState<MyState> {
 protected:
  auto applyEntries(std::unique_ptr<EntryIterator> ptr) -> futures::Future<Result> override {
    while (auto entry = ptr->next()) {
      auto& [idx, modification] = *entry;
      store[modification.key] = modification.value;
    }

    return futures::Future<Result>{TRI_ERROR_NO_ERROR};
  }
};

struct MyFactory {
  auto constructFollower() -> std::shared_ptr<MyFollowerState> {
    return std::make_shared<MyFollowerState>();
  }
  auto constructLeader() -> std::shared_ptr<MyLeaderState> {
    return std::make_shared<MyLeaderState>();
  }
};

struct ReplicatedStateTest
    : test::ReplicatedLogTest,
      tests::LogSuppressor<Logger::REPLICATED_STATE, LogLevel::TRACE> {
  std::shared_ptr<ReplicatedStateFeature> feature =
      std::make_shared<ReplicatedStateFeature>();
};

TEST_F(ReplicatedStateTest, simple_become_follower_test) {
  feature->registerStateType<MyState>("my-state");
  auto log = makeReplicatedLog(LogId{1});
  auto follower = log->becomeFollower("follower", LogTerm{1}, "leader");
  auto state = std::dynamic_pointer_cast<ReplicatedState<MyState>>(
      feature->createReplicatedState("my-state", log));
  ASSERT_NE(state, nullptr);

  state->flush();

  auto leaderLog = makeReplicatedLog(LogId{1});
  auto leader = leaderLog->becomeLeader(LogConfig(2, 2, false), "leader",
                                        LogTerm{1}, {follower});
  auto mux = streams::LogMultiplexer<ReplicatedStateStreamSpec<MyState>>::construct(leader);
  auto inputStream = mux->getStreamById<1>();

  inputStream->insert({.key = "hello", .value = "world"});
  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  auto followerState = state->getFollower();
  ASSERT_NE(followerState, nullptr);
  auto& store = followerState->store;
  EXPECT_EQ(store.size(), 1);
  EXPECT_EQ(store["hello"], "world");
}

TEST_F(ReplicatedStateTest, recreate_follower_on_new_term) {
  feature->registerStateType<MyState>("my-state");
  auto log = makeReplicatedLog(LogId{1});
  auto follower = log->becomeFollower("follower", LogTerm{1}, "leader");
  auto state = std::dynamic_pointer_cast<ReplicatedState<MyState>>(
      feature->createReplicatedState("my-state", log));
  ASSERT_NE(state, nullptr);

  // create a leader in term 1
  auto leaderLog = makeReplicatedLog(LogId{1});
  auto leader = leaderLog->becomeLeader(LogConfig(2, 2, false), "leader",
                                        LogTerm{1}, {follower});
  auto mux = streams::LogMultiplexer<ReplicatedStateStreamSpec<MyState>>::construct(leader);
  auto inputStream = mux->getStreamById<1>();
  inputStream->insert({.key = "hello", .value = "world"});

  state->flush();

  // recreate follower
  follower = log->becomeFollower("follower", LogTerm{2}, "leader");

  // create a leader in term 2
  leader = leaderLog->becomeLeader(LogConfig(2, 2, false), "leader", LogTerm{2}, {follower});
  mux = streams::LogMultiplexer<ReplicatedStateStreamSpec<MyState>>::construct(leader);
  inputStream = mux->getStreamById<1>();
  inputStream->insert({.key = "hello", .value = "world"});

  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

  auto followerState = state->getFollower();
  ASSERT_NE(followerState, nullptr);
  auto& store = followerState->store;
  EXPECT_EQ(store.size(), 1);
  EXPECT_EQ(store["hello"], "world");
}

TEST_F(ReplicatedStateTest, simple_become_leader_test) {
  feature->registerStateType<MyState>("my-state");

  auto followerLog = makeReplicatedLog(LogId{1});
  auto follower = followerLog->becomeFollower("follower", LogTerm{1}, "leader");

  auto log = makeReplicatedLog(LogId{1});
  auto leader = log->becomeLeader(LogConfig(2, 2, false), "leader",
                                        LogTerm{1}, {follower});
  leader->triggerAsyncReplication();
  auto state = std::dynamic_pointer_cast<ReplicatedState<MyState>>(
      feature->createReplicatedState("my-state", log));
  ASSERT_NE(state, nullptr);

  state->flush();
  while (follower->hasPendingAppendEntries()) {
    follower->runAsyncAppendEntries();
  }

}
