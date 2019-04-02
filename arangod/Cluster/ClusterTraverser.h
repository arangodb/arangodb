////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_CLUSTER_TRAVERSER_H
#define ARANGOD_CLUSTER_CLUSTER_TRAVERSER_H 1

#include "Cluster/TraverserEngineRegistry.h"
#include "Graph/Traverser.h"
#include "Graph/TraverserOptions.h"
#include "VocBase/LogicalCollection.h"

namespace arangodb {
class CollectionNameResolver;
namespace transaction {
class Methods;
}

namespace traverser {
class ClusterEdgeCursor;

class PathEnumerator;

class ClusterTraverser final : public Traverser {
  friend class ClusterEdgeCursor;

 public:
  ClusterTraverser(TraverserOptions* opts,
                   std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines,
                   std::string const& dbname, transaction::Methods* trx);

  ~ClusterTraverser() {}

  void setStartVertex(std::string const& id) override;
  
 protected:
  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions
  ///        Also apppends the _id value of the vertex in the given vector

  bool getVertex(arangodb::velocypack::Slice, std::vector<arangodb::velocypack::StringRef>&) override;

  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions
  bool getSingleVertex(arangodb::velocypack::Slice edge, arangodb::velocypack::StringRef const sourceVertexId,
                       uint64_t depth, arangodb::velocypack::StringRef& targetVertexId) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to fetch the real data of a vertex into an AQLValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue fetchVertexData(arangodb::velocypack::StringRef) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to add the real data of a vertex into a velocypack builder
  //////////////////////////////////////////////////////////////////////////////

  void addVertexToVelocyPack(arangodb::velocypack::StringRef, arangodb::velocypack::Builder&) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Destroy DBServer Traverser Engines
  //////////////////////////////////////////////////////////////////////////////

  void destroyEngines() override;

 private:
  void fetchVertices();

  std::unordered_map<arangodb::velocypack::StringRef, std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>> _vertices;

  std::string _dbname;

  std::unordered_map<ServerID, traverser::TraverserEngineID> const* _engines;

  std::unordered_set<arangodb::velocypack::StringRef> _verticesToFetch;
};

}  // namespace traverser
}  // namespace arangodb

#endif
