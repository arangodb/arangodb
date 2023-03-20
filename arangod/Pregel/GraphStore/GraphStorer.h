////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include <functional>
#include <memory>

#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Pregel/GraphStore/GraphStorerBase.h"
#include "Pregel/GraphStore/Quiver.h"
#include "Cluster/ClusterTypes.h"

namespace arangodb::pregel {

class WorkerConfig;

template<class V, class E>
struct GraphFormat;

template<typename V, typename E>
struct GraphStorer : GraphStorerBase<V, E> {
  explicit GraphStorer(std::shared_ptr<WorkerConfig> config,
                       std::shared_ptr<GraphFormat<V, E> const> graphFormat,
                       std::vector<ShardID> globalShards,
                       std::function<void()> statusUpdateCallback)
      : graphFormat(graphFormat),
        globalShards(globalShards),
        config(config),
        statusUpdateCallback(statusUpdateCallback) {}

  auto store(std::shared_ptr<Quiver<V, E>> quiver) -> void override;

  std::shared_ptr<GraphFormat<V, E> const> graphFormat;
  std::vector<ShardID> globalShards;
  std::shared_ptr<WorkerConfig const> config;
  std::function<void()> statusUpdateCallback;
};

}  // namespace arangodb::pregel
