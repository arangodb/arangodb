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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Graph/EdgeDocumentToken.h"
#include <deque>

namespace arangodb {

namespace aql {
struct AqlValue;
}

namespace graph {

class AttributeWeightShortestPathFinder;
class ConstantWeightShortestPathFinder;
class KShortestPathsFinder;
class TraverserCache;

class ShortestPathResult {
  friend class arangodb::graph::AttributeWeightShortestPathFinder;
  friend class arangodb::graph::ConstantWeightShortestPathFinder;
  friend class arangodb::graph::KShortestPathsFinder;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Constructor. This is an abstract only class.
  //////////////////////////////////////////////////////////////////////////////

  ShortestPathResult();

  ~ShortestPathResult();

  /// @brief Clears the path
  void clear();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the last edge pointing to the vertex at position as
  /// AqlValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue edgeToAqlValue(TraverserCache* cache, size_t depth) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the vertex at position as AqlValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue vertexToAqlValue(TraverserCache* cache, size_t depth) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Gets the amount of read documents
  //////////////////////////////////////////////////////////////////////////////

  size_t getReadDocuments() const { return _readDocuments; }

  /// @brief Gets the length of the path. (Number of vertices)

  size_t length() { return _vertices.size(); }

  void addVertex(std::string_view v);
  void addEdge(arangodb::graph::EdgeDocumentToken e);

  static constexpr size_t resultItemMemoryUsage() {
    return sizeof(typename decltype(_vertices)::value_type) +
           sizeof(typename decltype(_edges)::value_type);
  }

 private:
  /// @brief Count how many documents have been read
  size_t _readDocuments;

  // Convention _vertices.size() -1 === _edges.size()
  // path is _vertices[0] , _edges[0], _vertices[1] etc.

  /// @brief vertices
  std::deque<std::string_view> _vertices;

  /// @brief edges
  std::deque<arangodb::graph::EdgeDocumentToken> _edges;
};

}  // namespace graph
}  // namespace arangodb
