////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/types.h"
#include "Graph/Traverser.h"
#include "Graph/TraverserOptions.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/HashedStringRef.h>

namespace arangodb {
namespace traverser {
class ClusterEdgeCursor;

class ClusterTraverser final : public Traverser {
  friend class ClusterEdgeCursor;

 public:
  ClusterTraverser(TraverserOptions* opts,
                   std::unordered_map<ServerID, aql::EngineId> const* engines,
                   std::string const& dbname);

  ~ClusterTraverser() = default;

  void setStartVertex(std::string const& id) override;

 protected:
  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions
  ///        Also apppends the _id value of the vertex in the given vector

  bool getVertex(arangodb::velocypack::Slice, arangodb::traverser::EnumeratedPath& path) override;

  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions
  bool getSingleVertex(arangodb::velocypack::Slice edge, arangodb::velocypack::StringRef sourceVertexId,
                       uint64_t depth, arangodb::velocypack::StringRef& targetVertexId) override;

  bool getVertex(arangodb::velocypack::StringRef vertex, size_t depth) override;

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
  void clear() override;

 private:
  void fetchVertices();

  /// @brief build the (single) path enumerator of this traverser
  void createEnumerator();

  std::unordered_map<arangodb::velocypack::HashedStringRef, VPackSlice> _vertices;

  std::string const _dbname;

  std::unordered_map<ServerID, aql::EngineId> const* _engines;

  std::unordered_set<arangodb::velocypack::HashedStringRef> _verticesToFetch;
};

}  // namespace traverser
}  // namespace arangodb

#endif
