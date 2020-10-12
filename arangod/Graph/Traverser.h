////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_GRAPH_TRAVERSER_H
#define ARANGOD_GRAPH_TRAVERSER_H 1

#include "Aql/AqlValue.h"
#include "Aql/AstNode.h"
#include "Basics/Common.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/hashes.h"
#include "Graph/AttributeWeightShortestPathFinder.h"
#include "Graph/ConstantWeightShortestPathFinder.h"
#include "Graph/PathEnumerator.h"
#include "Graph/ShortestPathFinder.h"
#include "Transaction/Helpers.h"
#include "VocBase/voc-types.h"

#include <velocypack/StringRef.h>

namespace arangodb {

namespace transaction {
class Methods;
}

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
struct AstNode;
class Expression;
class Query;
}  // namespace aql

namespace graph {
class WeightedEnumerator;
class BreadthFirstEnumerator;
class NeighborsEnumerator;
class TraverserCache;
}  // namespace graph

namespace traverser {

struct TraverserOptions;

class TraversalPath {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Constructor. This is an abstract only class.
  //////////////////////////////////////////////////////////////////////////////

  TraversalPath() : _readDocuments(0) {}

  virtual ~TraversalPath() = default;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds the complete path as VelocyPack
  ///        Has the format:
  ///        {
  ///           vertices: [<vertex-as-velocypack>],
  ///           edges: [<edge-as-velocypack>]
  ///        }
  //////////////////////////////////////////////////////////////////////////////

  virtual void pathToVelocyPack(transaction::Methods*, arangodb::velocypack::Builder&) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the last edge on the path as VelocyPack
  //////////////////////////////////////////////////////////////////////////////

  virtual void lastEdgeToVelocyPack(transaction::Methods*,
                                    arangodb::velocypack::Builder&) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the last vertex as VelocyPack
  //////////////////////////////////////////////////////////////////////////////

  virtual aql::AqlValue lastVertexToAqlValue(transaction::Methods*) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Gets the amount of read documents
  //////////////////////////////////////////////////////////////////////////////

  virtual size_t getReadDocuments() const { return _readDocuments; }

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Count how many documents have been read
  //////////////////////////////////////////////////////////////////////////////

  size_t _readDocuments;
};

class Traverser {
  friend class arangodb::graph::BreadthFirstEnumerator;
  friend class arangodb::graph::WeightedEnumerator;
  friend class DepthFirstEnumerator;
  friend class arangodb::graph::NeighborsEnumerator;
#ifdef USE_ENTERPRISE
  friend class SmartDepthFirstPathEnumerator;
  friend class SmartBreadthFirstPathEnumerator;
#endif

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Class to read vertices.
  /////////////////////////////////////////////////////////////////////////////

  class VertexGetter {
   public:
    explicit VertexGetter(Traverser* traverser) : _traverser(traverser) {}

    virtual ~VertexGetter() = default;

    virtual bool getVertex(arangodb::velocypack::Slice,
                           std::vector<arangodb::velocypack::StringRef>&);

    virtual bool getSingleVertex(arangodb::velocypack::Slice, arangodb::velocypack::StringRef,
                                 uint64_t, arangodb::velocypack::StringRef&);

    virtual bool getVertex(arangodb::velocypack::StringRef vertex, size_t depth);

    virtual void reset(arangodb::velocypack::StringRef const&);

   protected:
    Traverser* _traverser;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Class to read vertices. Will return each vertex exactly once!
  /////////////////////////////////////////////////////////////////////////////

  class UniqueVertexGetter final : public VertexGetter {
   public:
    explicit UniqueVertexGetter(Traverser* traverser)
        : VertexGetter(traverser) {}

    ~UniqueVertexGetter() = default;

    bool getVertex(arangodb::velocypack::Slice,
                   std::vector<arangodb::velocypack::StringRef>&) override;

    bool getSingleVertex(arangodb::velocypack::Slice, arangodb::velocypack::StringRef,
                         uint64_t, arangodb::velocypack::StringRef&) override;

    bool getVertex(arangodb::velocypack::StringRef vertex, size_t depth) override;

    void reset(arangodb::velocypack::StringRef const&) override;

   private:
    std::unordered_set<arangodb::velocypack::StringRef> _returnedVertices;
  };

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Constructor. This is an abstract only class.
  //////////////////////////////////////////////////////////////////////////////

  Traverser(TraverserOptions* opts);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Destructor
  //////////////////////////////////////////////////////////////////////////////

  virtual ~Traverser();

  void done() { _done = true; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reset the traverser to use another start vertex
  //////////////////////////////////////////////////////////////////////////////

  virtual void setStartVertex(std::string const& value) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Skip amount many paths of the graph.
  //////////////////////////////////////////////////////////////////////////////

  size_t skip(size_t amount) {
    size_t skipped = 0;
    for (size_t i = 0; i < amount; ++i) {
      if (!next()) {
        _done = true;
        break;
      }
      ++skipped;
    }
    return skipped;
  }

  /// @brief Get the next possible path in the graph.
  bool next();

  /// @brief Function to clear all used caches properly
  virtual void clear() = 0;

  graph::TraverserCache* traverserCache();

 protected:
  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions
  ///        Also appends the _id value of the vertex in the given vector
  virtual bool getVertex(arangodb::velocypack::Slice,
                         std::vector<arangodb::velocypack::StringRef>&) = 0;

  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions
  virtual bool getSingleVertex(arangodb::velocypack::Slice edge,
                               arangodb::velocypack::StringRef sourceVertexId,
                               uint64_t depth,
                               arangodb::velocypack::StringRef& targetVertexId) = 0;

  virtual bool getVertex(arangodb::velocypack::StringRef vertex, size_t depth) = 0;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the last vertex as AQLValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue lastVertexToAqlValue() {
    return _enumerator->lastVertexToAqlValue();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the last edge as AQLValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue lastEdgeToAqlValue() {
    return _enumerator->lastEdgeToAqlValue();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds the complete path as AQLValue
  ///        Has the format:
  ///        {
  ///           vertices: [<vertex-as-velocypack>],
  ///           edges: [<edge-as-velocypack>]
  ///        }
  ///        NOTE: Will clear the given buffer and will leave the path in it.
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder& builder) {
    return _enumerator->pathToAqlValue(builder);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the number of filtered paths
  //////////////////////////////////////////////////////////////////////////////

  size_t getAndResetFilteredPaths();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the number of documents loaded
  //////////////////////////////////////////////////////////////////////////////

  size_t getAndResetReadDocuments();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the number of HTTP requests made
  //////////////////////////////////////////////////////////////////////////////

  size_t getAndResetHttpRequests();

  TraverserOptions* options() { return _opts; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Simple check if there are potentially more paths.
  ///        It might return true although there are no more paths available.
  ///        If it returns false it is guaranteed that there are no more paths.
  //////////////////////////////////////////////////////////////////////////////

  bool hasMore() const { return !_done; }

  bool edgeMatchesConditions(arangodb::velocypack::Slice edge,
                             arangodb::velocypack::StringRef vid,
                             uint64_t depth, size_t cursorId);

  bool vertexMatchesConditions(arangodb::velocypack::StringRef vid, uint64_t depth);

  transaction::Methods* trx() const { return _trx; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Destroy DBServer Traverser Engines
  //////////////////////////////////////////////////////////////////////////////

  virtual void destroyEngines() = 0;

 protected:
  /// @brief Outer top level transaction
  transaction::Methods* _trx;

  /// @brief internal cursor to enumerate the paths of a graph
  std::unique_ptr<traverser::PathEnumerator> _enumerator;

  /// @brief internal getter to extract an edge
  std::unique_ptr<VertexGetter> _vertexGetter;

  /// @brief indicator if this traversal is done
  bool _done;

  /// @brief options for traversal
  TraverserOptions* _opts;

  /// @brief Function to fetch the real data of a vertex into an AQLValue
  virtual aql::AqlValue fetchVertexData(arangodb::velocypack::StringRef vid) = 0;

  /// @brief Function to add the real data of a vertex into a velocypack builder
  virtual void addVertexToVelocyPack(arangodb::velocypack::StringRef vid,
                                     arangodb::velocypack::Builder&) = 0;
};
}  // namespace traverser
}  // namespace arangodb

#endif
