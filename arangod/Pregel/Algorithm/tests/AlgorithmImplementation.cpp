////////////////////////////////////////////////////////////////////////////////
///
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#include <velocypack/vpack.h>

#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

#include <Basics/VelocyPackStringLiteral.h>
#include <Inspection/VPack.h>

#include <velocypack/vpack.h>

#include <fmt/core.h>
#include <fmt/format.h>

#include <ActorFramework/Actor.h>
#include <ActorFramework/Runtime.h>

using namespace arangodb::velocypack;
using namespace arangodb::actor_framework;

struct Msg : MsgPayloadBase {};

struct Worker : ActorBase {
  Worker(PidT pid, Runtime& rt) : ActorBase(pid, rt) {
    fmt::print("Worker created {}\n", pid.pid);
  }
  virtual void process(PidT from,
                       std::unique_ptr<MsgPayloadBase> msg) override {
    fmt::print("Hello, world, this is your worker speaking {} \n", from.pid);
    send<Msg>(from);
  }

  std::vector<PidT> workers;
};

struct MsgPayload : MsgPayloadBase {};
struct Conductor : ActorBase {
  Conductor(PidT pid, Runtime& rt) : ActorBase(pid, rt) {}
  virtual void process(PidT from,
                       std::unique_ptr<MsgPayloadBase> msg) override {
    fmt::print("Hello, world, this is your conductor speaking\n");
    for (std::size_t i = 0; i < 5; i++) {
      auto p = rt.spawn<Worker>();
      workers.emplace_back(p);
      send<Msg>(p);
    }
  }

  std::vector<PidT> workers;
};

auto main(int argc, char** argv) -> int {
  fmt::print("GWEN testbed\n");

  Runtime rt;

  auto pid = rt.spawn<Conductor>();
  rt.send(pid, pid, std::make_unique<Msg>());
  rt.run();

  return 0;
}
