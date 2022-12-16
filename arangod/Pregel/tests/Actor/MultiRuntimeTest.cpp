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
#include "Actor/Runtime.h"

#include "TrivialActor.h"
#include "PingPongActors.h"

using namespace arangodb::pregel::actor;
using namespace arangodb::pregel::actor::test;

struct MockScheduler {
  auto operator()(auto fn) { fn(); }
};

TEST(MultiRuntimeTest, ping_pong_game) {


  auto scheduler = std::make_shared<MockScheduler>();

  // Runtime 1
  auto serverID1 = ServerID{"A"};
  Runtime runtime1(serverID1, "RuntimeTest-1", scheduler);

  auto pong_actor = runtime1.spawn<pong_actor::Actor>(pong_actor::State{},
                                                     pong_actor::Start{});



  // Runtime 2
  auto serverID2 = ServerID{"B"};
  Runtime runtime2(serverID2, "RuntimeTest-2", scheduler);
  auto ping_actor = runtime2.spawn<ping_actor::Actor>(
      ping_actor::State{},
      ping_actor::Start{.pongActor =
                            ActorPID{.id = pong_actor, .server = serverID1}});


  auto ping_actor_state =
      runtime2.getActorStateByID<ping_actor::Actor>(ping_actor);
  ASSERT_EQ(ping_actor_state,
            (ping_actor::State{.called = 2, .message = "hello world"}));
  auto pong_actor_state =
      runtime1.getActorStateByID<pong_actor::Actor>(pong_actor);
  ASSERT_EQ(pong_actor_state, (pong_actor::State{.called = 1}));
}
