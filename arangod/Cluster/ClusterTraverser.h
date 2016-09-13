////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "VocBase/LogicalCollection.h"
#include "VocBase/Traverser.h"
#include "VocBase/TraverserOptions.h"

namespace arangodb {
class CollectionNameResolver;
class Transaction;

namespace traverser {
class ClusterEdgeCursor;

class PathEnumerator;

class ClusterTraverser final : public Traverser {
  friend class ClusterEdgeCursor;

 public:
  ClusterTraverser(
      TraverserOptions* opts,
      std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines,
      std::string const& dbname, Transaction* trx);

  ~ClusterTraverser() {}

  void setStartVertex(std::string const& id) override;

 protected:
  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions
  ///        Also apppends the _id value of the vertex in the given vector

  bool getVertex(arangodb::velocypack::Slice,
                 std::vector<arangodb::velocypack::Slice>&) override;

  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions

  bool getSingleVertex(arangodb::velocypack::Slice, arangodb::velocypack::Slice,
                       size_t, arangodb::velocypack::Slice&) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to fetch the real data of a vertex into an AQLValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue fetchVertexData(arangodb::velocypack::Slice) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to fetch the real data of an edge into an AQLValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue fetchEdgeData(arangodb::velocypack::Slice) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to add the real data of a vertex into a velocypack builder
  //////////////////////////////////////////////////////////////////////////////

  void addVertexToVelocyPack(arangodb::velocypack::Slice,
                             arangodb::velocypack::Builder&) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to add the real data of an edge into a velocypack builder
  //////////////////////////////////////////////////////////////////////////////

  void addEdgeToVelocyPack(arangodb::velocypack::Slice,
                           arangodb::velocypack::Builder&) override;

 private:

  void fetchVertices();

  std::unordered_map<arangodb::velocypack::Slice, arangodb::velocypack::Slice>
      _edges;

  std::unordered_map<arangodb::velocypack::Slice,
                     std::shared_ptr<arangodb::velocypack::Buffer<uint8_t>>>
      _vertices;

  std::string _dbname;

  std::unordered_map<ServerID, traverser::TraverserEngineID> const* _engines;

  std::unordered_set<arangodb::velocypack::Slice> _verticesToFetch;

  std::vector<std::shared_ptr<arangodb::velocypack::Builder>> _datalake;

};

}  // traverser
}  // arangodb

#endif
