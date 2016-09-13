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

#ifndef ARANGOD_SINGLE_SERVER_TRAVERSER_H
#define ARANGOD_SINGLE_SERVER_TRAVERSER_H 1

#include "VocBase/Traverser.h"
#include "Aql/AqlValue.h"
#include "Utils/OperationCursor.h"
#include "VocBase/TraverserOptions.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class EdgeIndex;
class LogicalCollection;

namespace traverser {

class PathEnumerator;

class SingleServerEdgeCursor : public EdgeCursor {
 private:
  std::vector<std::vector<OperationCursor*>> _cursors;
  size_t _currentCursor;
  size_t _currentSubCursor;
  std::vector<TRI_doc_mptr_t*> _cache;
  size_t _cachePos;

 public:
  explicit SingleServerEdgeCursor(size_t);

  ~SingleServerEdgeCursor() {
    for (auto& it : _cursors) {
      for (auto& it2 : it) {
        delete it2;
      }
    }
  }

  bool next(std::vector<arangodb::velocypack::Slice>&, size_t&) override;

  bool readAll(std::unordered_set<arangodb::velocypack::Slice>&, size_t&) override;

  std::vector<std::vector<OperationCursor*>>& getCursors() {
    return _cursors;
  }
};

class SingleServerTraverser final : public Traverser {

 public:
  SingleServerTraverser(TraverserOptions*, Transaction*);

  ~SingleServerTraverser();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reset the traverser to use another start vertex
  //////////////////////////////////////////////////////////////////////////////

  void setStartVertex(std::string const& v) override;


 protected:
  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions
  ///        Adds the _id of the vertex into the given vector

  bool getVertex(arangodb::velocypack::Slice,
                 std::vector<arangodb::velocypack::Slice>&) override;

  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions

  bool getSingleVertex(arangodb::velocypack::Slice, arangodb::velocypack::Slice,
                       size_t depth, arangodb::velocypack::Slice&) override;

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

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Cache for vertex documents, points from _id to start of 
  /// document VPack value (in datafiles)
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<arangodb::velocypack::Slice, uint8_t const*> _vertices;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Cache for edge documents, points from _id to start of edge
  /// VPack value (in datafiles)
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<std::string, uint8_t const*> _edges;

};
} // namespace traverser
} // namespace arangodb

#endif
