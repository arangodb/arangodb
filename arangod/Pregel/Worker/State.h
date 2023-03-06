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

#include "Actor/ActorPID.h"
#include "Pregel/CollectionSpecifications.h"
#include "Pregel/PregelOptions.h"

namespace arangodb::pregel::worker {

struct WorkerState {
  WorkerState(actor::ActorPID conductor,
              ExecutionSpecifications executionSpecifications,
              CollectionSpecifications collectionSpecifications)
      : conductor{std::move(conductor)},
        executionSpecifications{std::move(executionSpecifications)},
        collectionSpecifications{std::move(collectionSpecifications)} {};
  actor::ActorPID conductor;
  const ExecutionSpecifications executionSpecifications;
  const CollectionSpecifications collectionSpecifications;
};
template<typename Inspector>
auto inspect(Inspector& f, WorkerState& x) {
  return f.object(x).fields(
      f.field("conductor", x.conductor),
      f.field("executionSpecifications", x.executionSpecifications),
      f.field("collectionSpecifications", x.collectionSpecifications));
}

}  // namespace arangodb::pregel::worker

template<>
struct fmt::formatter<arangodb::pregel::worker::WorkerState>
    : arangodb::inspection::inspection_formatter {};
