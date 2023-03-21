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

#include <memory>
#include <functional>

#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"
#include "Cluster/ClusterTypes.h"
#include "Pregel/GraphStore/GraphLoaderBase.h"
#include "Pregel/IndexHelpers.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/vocbase.h"

namespace arangodb::pregel {

class WorkerConfig;
template<class V, class E>
struct GraphFormat;

template<typename V, typename E>
struct GraphLoader : GraphLoaderBase<V, E> {
  explicit GraphLoader(std::shared_ptr<WorkerConfig const> config,
                       std::shared_ptr<GraphFormat<V, E> const> graphFormat,
                       std::function<void()> statusUpdateCallback)
      : result(std::make_shared<Quiver<V, E>>()),
        graphFormat(graphFormat),
        resourceMonitor(GlobalResourceMonitor::instance()),
        config(config),
        statusUpdateCallback(statusUpdateCallback) {}

  auto load() -> std::shared_ptr<Quiver<V, E>> override;

  auto loadVertices(ShardID const& vertexShard,
                    std::vector<ShardID> const& edgeShards) -> void;
  auto loadEdges(transaction::Methods& trx, Vertex<V, E>& vertex,
                 std::string_view documentID,
                 traverser::EdgeCollectionInfo& info) -> void;

  uint64_t determineVertexIdRangeStart(uint64_t numVertices);

  std::shared_ptr<Quiver<V, E>> result;

  std::shared_ptr<GraphFormat<V, E> const> graphFormat;
  ResourceMonitor resourceMonitor;
  std::shared_ptr<WorkerConfig const> config;
  std::function<void()> const statusUpdateCallback;

  uint64_t const batchSize = 10000;
};
}  // namespace arangodb::pregel
