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
#include "Basics/Guarded.h"
#include "Basics/ResourceUsage.h"
#include "Cluster/ClusterTypes.h"
#include "Pregel/GraphStore/GraphLoaderBase.h"
#include "Pregel/GraphStore/LoadableVertexShard.h"
#include "Pregel/IndexHelpers.h"
#include "Pregel/StatusMessages.h"
#include "Utils/DatabaseGuard.h"
#include "VocBase/vocbase.h"

namespace arangodb::pregel {

class WorkerConfig;
template<class V, class E>
struct GraphFormat;

struct OldLoadingUpdate {
  std::function<void()> fn;
};
struct ActorLoadingUpdate {
  std::function<void(pregel::message::GraphLoadingUpdate)> fn;
};
using LoadingUpdateCallback =
    std::variant<OldLoadingUpdate, ActorLoadingUpdate>;

struct VertexIdRange {
  uint64_t current = 0;
  uint64_t maxId = 0;
};
template<typename Inspector>
auto inspect(Inspector& f, VertexIdRange& r) {
  return f.object(r).fields(f.field("current", r.current),
                            f.field("maxId", r.maxId));
}

template<typename V, typename E>
struct GraphLoader : GraphLoaderBase<V, E> {
  explicit GraphLoader(std::shared_ptr<WorkerConfig const> config,
                       std::shared_ptr<GraphFormat<V, E> const> graphFormat,
                       LoadingUpdateCallback updateCallback)
      : graphFormat(graphFormat),
        resourceMonitor(GlobalResourceMonitor::instance()),
        config(config),
        updateCallback(updateCallback) {}
  auto load() -> futures::Future<Magazine<V, E>> override;

  auto loadVertices(LoadableVertexShard const& loadableVertexShard)
      -> std::shared_ptr<Quiver<V, E>>;
  auto loadEdges(transaction::Methods& trx, Vertex<V, E>& vertex,
                 std::string_view documentID,
                 traverser::EdgeCollectionInfo& info) -> void;

  auto requestVertexIds(uint64_t numVertices) -> VertexIdRange;
  auto computeLoadableVertexShards()
      -> std::shared_ptr<std::vector<LoadableVertexShard>>;
  std::shared_ptr<GraphFormat<V, E> const> graphFormat;
  ResourceMonitor resourceMonitor;
  std::shared_ptr<WorkerConfig const> config;
  LoadingUpdateCallback updateCallback;

  std::atomic<uint64_t> currentIdBase;

  uint64_t const batchSize = 10000;
};
}  // namespace arangodb::pregel
