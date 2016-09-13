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

#ifndef ARANGOD_AQL_GRAPHS_H
#define ARANGOD_AQL_GRAPHS_H 1

#include "Basics/Common.h"

namespace arangodb {

namespace velocypack {
class Builder;
class Slice;
}

namespace aql {

class Graph {
 public:
  explicit Graph(arangodb::velocypack::Slice const&);

  virtual ~Graph() {}

 private:
  /// @brief the cids of all vertexCollections
  std::unordered_set<std::string> _vertexColls;

  /// @brief the cids of all edgeCollections
  std::unordered_set<std::string> _edgeColls;

  /// @brief Graph collection edge definition attribute name
  static char const* _attrEdgeDefs;

  /// @brief Graph collection orphan list arribute name
  static char const* _attrOrphans;

 public:
  /// @brief Graph collection name
  static std::string const _graphs;

  /// @brief Add Collections to the object
  void insertVertexCollections(arangodb::velocypack::Slice& arr);

 public:
  /// @brief get the cids of all vertexCollections
  std::unordered_set<std::string> const& vertexCollections() const;

  /// @brief get the cids of all edgeCollections
  std::unordered_set<std::string> const& edgeCollections() const;

  /// @brief Add an edge collection to this graphs definition
  void addEdgeCollection(std::string const&);

  /// @brief Add a vertex collection to this graphs definition
  void addVertexCollection(std::string const&);

  /// @brief return a VelocyPack representation of the graph
  void toVelocyPack(arangodb::velocypack::Builder&, bool) const;

  virtual void enhanceEngineInfo(arangodb::velocypack::Builder&) const;
};

}  // namespace aql
}  // namespace arangodb

#endif
