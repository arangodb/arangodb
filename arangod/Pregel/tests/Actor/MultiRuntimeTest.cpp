////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Julia Volmer
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <unordered_set>

#include "Actor/ActorPID.h"
#include "Actor/Dispatcher.h"
#include "Actor/Runtime.h"

#include "TrivialActor.h"
#include "PingPongActors.h"

using namespace arangodb;

using namespace arangodb::pregel::actor;
using namespace arangodb::pregel::actor::test;

struct MockScheduler {
  auto operator()(auto fn) { fn(); }
};

TEST(MultiRuntimeTest, ping_pong_game) {
  std::unordered_map<ServerID, std::unique_ptr<Runtime<MockScheduler>>>
      runtimes;
  auto externalDispatcher = ExternalDispatcher{
      .send = [&runtimes](ActorPID sender, ActorPID receiver,
                          velocypack::SharedSlice msg) -> void {
        if (runtimes.contains(receiver.server)) {
          runtimes[receiver.server]->process(sender, receiver, msg);
        } else {
          std::cerr << "cannot find server " << receiver.server << std::endl;
          std::abort();
        }
      }};

  auto scheduler = std::make_shared<MockScheduler>();

  // Runtime 1
  auto serverID1 = ServerID{"A"};
  runtimes.emplace(serverID1, std::make_unique<Runtime<MockScheduler>>(
                                  serverID1, "RuntimeTest-1", scheduler,
                                  externalDispatcher));

  auto pong_actor = runtimes[serverID1]->spawn<pong_actor::Actor>(
      pong_actor::State{}, pong_actor::Start{});

  // Runtime 2
  auto serverID2 = ServerID{"B"};
  runtimes.emplace(serverID2, std::make_unique<Runtime<MockScheduler>>(
                                  serverID2, "RuntimeTest-2", scheduler,
                                  externalDispatcher));
  auto ping_actor = runtimes[serverID2]->spawn<ping_actor::Actor>(
      ping_actor::State{},
      ping_actor::Start{.pongActor =
                            ActorPID{.server = serverID1, .id = pong_actor}});

  auto ping_actor_state =
      runtimes[serverID2]->getActorStateByID<ping_actor::Actor>(ping_actor);
  ASSERT_EQ(ping_actor_state,
            (ping_actor::State{.called = 2, .message = "hello world"}));
  auto pong_actor_state =
      runtimes[serverID1]->getActorStateByID<pong_actor::Actor>(pong_actor);
  ASSERT_EQ(pong_actor_state, (pong_actor::State{.called = 1}));
}
