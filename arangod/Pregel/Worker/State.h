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
#include "Pregel/Algorithm.h"
#include "Basics/Guarded.h"
#include "Pregel/CollectionSpecifications.h"
#include "Pregel/PregelOptions.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/vocbase.h"
#include "Pregel/Status/Status.h"

namespace arangodb::pregel::worker {

template<typename V, typename E, typename M>
struct WorkerState {
  WorkerState(actor::ActorPID conductor,
              ExecutionSpecifications executionSpecifications,
              CollectionSpecifications collectionSpecifications,
              std::unique_ptr<Algorithm<V, E, M>> algorithm,
              TRI_vocbase_t& vocbase)
      : conductor{std::move(conductor)},
        executionSpecifications{std::move(executionSpecifications)},
        collectionSpecifications{std::move(collectionSpecifications)},
        algorithm{std::move(algorithm)},
        vocbaseGuard{vocbase} {};

  auto observeStatus() -> Status const {
    auto currentGss = currentGssObservables.observe();
    auto fullGssStatus = allGssStatus.copy();

    if (!currentGss.isDefault()) {
      fullGssStatus.gss.emplace_back(currentGss);
    }
    return Status{
        .graphStoreStatus =
            GraphStoreStatus{},  // TODO GORDO-1546 graphStore->status(),
        .allGssStatus = fullGssStatus.gss.size() > 0
                            ? std::optional{fullGssStatus}
                            : std::nullopt};
  }

  actor::ActorPID conductor;
  const ExecutionSpecifications executionSpecifications;
  const CollectionSpecifications collectionSpecifications;
  std::unique_ptr<Algorithm<V, E, M>> algorithm;
  const DatabaseGuard vocbaseGuard;
  // TODO GOROD-1546
  // GraphStore graphStore;
  GssObservables currentGssObservables;
  Guarded<AllGssStatus> allGssStatus;
};
template<typename V, typename E, typename M, typename Inspector>
auto inspect(Inspector& f, WorkerState<V, E, M>& x) {
  return f.object(x).fields(
      f.field("conductor", x.conductor),
      f.field("executionSpecifications", x.executionSpecifications),
      f.field("collectionSpecifications", x.collectionSpecifications),
      f.field("algorithm", x.algorithm->name()));
}

}  // namespace arangodb::pregel::worker

template<typename V, typename E, typename M>
struct fmt::formatter<arangodb::pregel::worker::WorkerState<V, E, M>>
    : arangodb::inspection::inspection_formatter {};
