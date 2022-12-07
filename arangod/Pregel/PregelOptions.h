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

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>
#include "Basics/ResultT.h"
#include "Cluster/ClusterTypes.h"
#include "Inspection/Types.h"
#include "VocBase/vocbase.h"
#include "velocypack/Builder.h"

namespace arangodb::pregel {

struct GraphCollectionNames {
  std::vector<std::string> vertexCollections;
  std::vector<std::string> edgeCollections;
};

struct GraphName {
  std::string graph;
};

using VertexCollectionID = CollectionID;
using EdgeCollectionID = CollectionID;
struct EdgeCollectionRestrictions {
  // maps from vertex collection name to a list of edge collections that this
  // vertex collection is restricted to. only use for a collection if there is
  // at least one entry for the collection!
  std::unordered_map<VertexCollectionID, std::vector<EdgeCollectionID>> items;
  auto add(EdgeCollectionRestrictions others) const
      -> EdgeCollectionRestrictions;
};

struct GraphDataSource {
  GraphDataSource(
      std::variant<GraphCollectionNames, GraphName> graphOrCollections,
      EdgeCollectionRestrictions restrictions)
      : graphOrCollections{std::move(graphOrCollections)},
        edgeCollectionRestrictions{std::move(restrictions)} {}
  auto collectionNames(TRI_vocbase_t& vocbase) -> ResultT<GraphCollectionNames>;
  auto restrictions(TRI_vocbase_t& vocbase)
      -> ResultT<EdgeCollectionRestrictions>;

 private:
  std::variant<GraphCollectionNames, GraphName> graphOrCollections;
  EdgeCollectionRestrictions edgeCollectionRestrictions;
  auto graphRestrictions(TRI_vocbase_t& vocbase)
      -> ResultT<EdgeCollectionRestrictions>;
};

struct PregelOptions {
  std::string algorithm;
  VPackBuilder userParameters;
  GraphDataSource graphDataSource;
};

}  // namespace arangodb::pregel
