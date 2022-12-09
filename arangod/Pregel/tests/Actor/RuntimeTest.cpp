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

#include "Actor/Runtime.h"

#include "TrivialActor.h"

using namespace arangodb::pregel::actor;
using namespace arangodb::pregel::actor::test;

struct MockScheduler {
  auto operator()(auto fn) { fn(); }
};

struct MockSendingMechanism {

};

TEST(RuntimeTest, gives_back_stuff_pushed) {
  auto scheduler = std::make_shared<MockScheduler>();
  auto sendingMechanism = std::make_shared<MockSendingMechanism>();

  Runtime runtime("PRMR-1234", "RuntimeTest", scheduler, sendingMechanism);

  auto p = TrivialMessage{};
  p.message = TrivialMessage1("foo");

  runtime.spawn<TrivialState, TrivialMessage, TrivialHandler>(TrivialState("foo"), std::move(p));

//  std::visit(TrivialHandler{std::make_unique<TrivialState>("foo")}, p.message);
}
