////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GRAPH_GRAPH_H
#define ARANGOD_GRAPH_GRAPH_H

#include "Aql/VariableGenerator.h"

namespace arangodb {
namespace graph {

class Graph {
 public:
  explicit Graph(arangodb::velocypack::Slice const &);

  virtual ~Graph() {}

 private:
  /// @brief the cids of all vertexCollections
  std::unordered_set<std::__cxx11::string> _vertexColls;

  /// @brief the cids of all edgeCollections
  std::unordered_set<std::__cxx11::string> _edgeColls;

  /// @brief Graph collection edge definition attribute name
  static char const *_attrEdgeDefs;

  /// @brief Graph collection orphan list arribute name
  static char const *_attrOrphans;

 public:
  /// @brief Graph collection name
  static std::__cxx11::string const _graphs;

  /// @brief Add Collections to the object
  void insertVertexCollections(arangodb::velocypack::Slice &arr);

 public:
  /// @brief get the cids of all vertexCollections
  std::unordered_set<std::__cxx11::string> const &vertexCollections() const;

  /// @brief get the cids of all edgeCollections
  std::unordered_set<std::__cxx11::string> const &edgeCollections() const;

  /// @brief Add an edge collection to this graphs definition
  void addEdgeCollection(std::__cxx11::string const &);

  /// @brief Add a vertex collection to this graphs definition
  void addVertexCollection(std::__cxx11::string const &);

  /// @brief return a VelocyPack representation of the graph
  void toVelocyPack(arangodb::velocypack::Builder &, bool) const;

  virtual void enhanceEngineInfo(arangodb::velocypack::Builder &) const;
};

}
}

#endif // ARANGOD_GRAPH_GRAPH_H
