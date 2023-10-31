////////////////////////////////////////////////////////////////////////////////
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
/// @author Julia Volmer
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <variant>
#include "Actor/ActorPID.h"
#include "Inspection/Types.h"
#include "Pregel/Worker/Messages.h"

namespace arangodb::pregel::message {

struct SpawnStart {};
template<typename Inspector>
auto inspect(Inspector& f, SpawnStart& x) {
  return f.object(x).fields();
}

struct SpawnWorker {
  actor::ServerID destinationServer;
  actor::ActorPID conductor;
  actor::ActorPID resultActorOnCoordinator;
  actor::ActorPID statusActor;
  actor::ActorPID metricsActor;
  TTL ttl;
  worker::message::CreateWorker message;
};
template<typename Inspector>
auto inspect(Inspector& f, SpawnWorker& x) {
  return f.object(x).fields(
      f.field("destinationServer", x.destinationServer),
      f.field("conductor", x.conductor), f.field("statusActor", x.statusActor),
      f.field("resultActorOnCoordinator", x.resultActorOnCoordinator),
      f.field("ttl", x.ttl), f.field("message", x.message));
}

struct SpawnCleanup {};
template<typename Inspector>
auto inspect(Inspector& f, SpawnCleanup& x) {
  return f.object(x).fields();
}

struct SpawnMessages : std::variant<SpawnStart, SpawnWorker, SpawnCleanup> {
  using std::variant<SpawnStart, SpawnWorker, SpawnCleanup>::variant;
};
template<typename Inspector>
auto inspect(Inspector& f, SpawnMessages& x) {
  return f.variant(x).unqualified().alternatives(
      arangodb::inspection::type<SpawnStart>("Start"),
      arangodb::inspection::type<SpawnWorker>("SpawnWorker"),
      arangodb::inspection::type<SpawnCleanup>("SpawnCleanup"));
}

}  // namespace arangodb::pregel::message
