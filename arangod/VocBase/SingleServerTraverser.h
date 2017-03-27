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

#include "Aql/AqlValue.h"
#include "Utils/OperationCursor.h"
#include "VocBase/Traverser.h"
#include "VocBase/TraverserOptions.h"
#include "VocBase/voc-types.h"

namespace arangodb {

class LogicalCollection;
class ManagedDocumentResult;

namespace traverser {

class PathEnumerator;
  
class SingleServerEdgeCursor : public EdgeCursor {
 private:
  TraverserOptions* _opts;
  transaction::Methods* _trx;
  ManagedDocumentResult* _mmdr;
  std::vector<std::vector<OperationCursor*>> _cursors;
  size_t _currentCursor;
  size_t _currentSubCursor;
  std::vector<DocumentIdentifierToken> _cache;
  size_t _cachePos;
  std::vector<size_t> const* _internalCursorMapping;

 public:
  SingleServerEdgeCursor(ManagedDocumentResult* mmdr, TraverserOptions* options, size_t, std::vector<size_t> const* mapping = nullptr);

  ~SingleServerEdgeCursor() {
    for (auto& it : _cursors) {
      for (auto& it2 : it) {
        delete it2;
      }
    }
  }

  bool next(std::function<void(arangodb::StringRef const&, VPackSlice, size_t)> callback) override;

  void readAll(std::function<void(arangodb::StringRef const&, arangodb::velocypack::Slice, size_t&)>) override;

  std::vector<std::vector<OperationCursor*>>& getCursors() {
    return _cursors;
  }
};

class SingleServerTraverser final : public Traverser {

 public:
  SingleServerTraverser(TraverserOptions*, transaction::Methods*, ManagedDocumentResult*);

  ~SingleServerTraverser();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reset the traverser to use another start vertex
  //////////////////////////////////////////////////////////////////////////////

  void setStartVertex(std::string const& v) override;
  
  size_t getAndResetReadDocuments() override;

 protected:
  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions
  ///        Adds the _id of the vertex into the given vector

  bool getVertex(arangodb::velocypack::Slice edge,
                 std::vector<arangodb::StringRef>&) override;

  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions
  bool getSingleVertex(arangodb::velocypack::Slice edge,
                       arangodb::StringRef const sourceVertexId,
                       uint64_t depth,
                       arangodb::StringRef& targetVertexId) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to fetch the real data of a vertex into an AQLValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue fetchVertexData(StringRef) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to fetch the real data of an edge into an AQLValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue fetchEdgeData(StringRef) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to add the real data of a vertex into a velocypack builder
  //////////////////////////////////////////////////////////////////////////////

  void addVertexToVelocyPack(StringRef,
                             arangodb::velocypack::Builder&) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Function to add the real data of an edge into a velocypack builder
  //////////////////////////////////////////////////////////////////////////////

  void addEdgeToVelocyPack(StringRef,
                           arangodb::velocypack::Builder&) override;

 private:
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Cache for vertex documents, points from _id to start of 
  /// document VPack value (in datafiles)
  //////////////////////////////////////////////////////////////////////////////

  //std::unordered_map<arangodb::velocypack::Slice, uint8_t const*> _vertices;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Cache for edge documents, points from _id to start of edge
  /// VPack value (in datafiles)
  //////////////////////////////////////////////////////////////////////////////

  //std::unordered_map<std::string, uint8_t const*> _edges;

};
} // namespace traverser
} // namespace arangodb

#endif
