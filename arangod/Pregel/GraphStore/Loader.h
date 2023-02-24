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
/// @author Julia Volmer
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Pregel/GraphStore/Quiver.h"

#include <memory>

namespace arangodb::pregel {
template<typename V, typename E>
struct Loader {
  Loader() {}

  // ====================== NOT THREAD SAFE ===========================
  void loadShards(WorkerConfig* config,
                  std::function<void()> const& statusUpdateCallback,
                  std::function<void()> const& finishedLoadingCallback);
  void loadDocument(WorkerConfig* config, std::string const& documentID);
  void loadDocument(WorkerConfig* config, PregelShard sourceShard,
                    std::string_view key);

  auto loadVertices(ShardID const& vertexShard,
                    std::vector<ShardID> const& edgeShards,
                    std::function<void()> const& statusUpdateCallback)
      -> std::vector<Vertex<V, E>>;
  void loadEdges(transaction::Methods& trx, Vertex<V, E>& vertex,
                 ShardID const& edgeShard, std::string_view documentID,
                 uint64_t numVertices, traverser::EdgeCollectionInfo& info);

  std::unique_ptr<Quiver<V, E>> quiver;
};
}  // namespace arangodb::pregel
