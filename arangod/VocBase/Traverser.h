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

#ifndef ARANGOD_VOC_BASE_TRAVERSER_H
#define ARANGOD_VOC_BASE_TRAVERSER_H 1

#include "Basics/Common.h"
#include "Basics/hashes.h"
#include "Basics/ShortestPathFinder.h"
#include "Aql/AqlValue.h"
#include "Aql/AstNode.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Transaction.h"
#include "VocBase/PathEnumerator.h"
#include "VocBase/voc-types.h"

namespace arangodb {

namespace velocypack {
class Builder;
class Slice;
}

namespace aql {
struct AstNode;
class Expression;
class Query;
}
namespace traverser {

struct TraverserOptions;

//TODO Deprecated
class TraverserExpression {
 public:
  bool isEdgeAccess;
  arangodb::aql::AstNodeType comparisonType;
  arangodb::aql::AstNode const* varAccess;
  std::unique_ptr<arangodb::velocypack::Builder> compareTo;

  TraverserExpression(bool pisEdgeAccess,
                      arangodb::aql::AstNodeType pcomparisonType,
                      arangodb::aql::AstNode const* pvarAccess)
      : isEdgeAccess(pisEdgeAccess),
        comparisonType(pcomparisonType),
        varAccess(pvarAccess),
        compareTo(nullptr) {}

  explicit TraverserExpression(arangodb::velocypack::Slice const& slice);

  virtual ~TraverserExpression() {
    // no need to destroy varAccess here. Its memory is managed via the
    // _nodeRegister variable in this class

    for (auto& it : _stringRegister) {
      delete it;
    }
  }

  void toVelocyPack(arangodb::velocypack::Builder& builder) const;

  bool matchesCheck(arangodb::Transaction*,
                    arangodb::velocypack::Slice const& element) const;

 protected:
  TraverserExpression()
      : isEdgeAccess(false),
        comparisonType(arangodb::aql::NODE_TYPE_ROOT),
        varAccess(nullptr),
        compareTo(nullptr) {}

 private:
  bool recursiveCheck(arangodb::aql::AstNode const*,
                      arangodb::velocypack::Slice& value,
                      arangodb::velocypack::Slice& base) const;

  // Required when creating this expression without AST
  std::vector<std::unique_ptr<arangodb::aql::AstNode const>> _nodeRegister;
  std::vector<std::string*> _stringRegister;
};

class ShortestPath {
  friend class basics::DynamicDistanceFinder<arangodb::velocypack::Slice,
                                             arangodb::velocypack::Slice,
                                             size_t, ShortestPath>;
  friend class basics::DynamicDistanceFinder<arangodb::velocypack::Slice,
                                             arangodb::velocypack::Slice,
                                             double, ShortestPath>;
  friend class arangodb::basics::ConstDistanceFinder<
      arangodb::velocypack::Slice, arangodb::velocypack::Slice,
      arangodb::basics::VelocyPackHelper::VPackStringHash,
      arangodb::basics::VelocyPackHelper::VPackStringEqual, ShortestPath>;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Constructor. This is an abstract only class.
  //////////////////////////////////////////////////////////////////////////////

  ShortestPath() : _readDocuments(0) {}

  ~ShortestPath() {}

  /// @brief Clears the path
  void clear();

  /// @brief Builds only the last edge pointing to the vertex at position as
  /// VelocyPack

  void edgeToVelocyPack(Transaction*, size_t, arangodb::velocypack::Builder&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the vertex at position as VelocyPack
  //////////////////////////////////////////////////////////////////////////////

  void vertexToVelocyPack(Transaction*, size_t, arangodb::velocypack::Builder&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Gets the amount of read documents
  //////////////////////////////////////////////////////////////////////////////

  size_t getReadDocuments() const { return _readDocuments; }

  /// @brief Gets the length of the path. (Number of vertices)

  size_t length() { return _vertices.size(); };

 private:
  /// @brief Count how many documents have been read
  size_t _readDocuments;

  // Convention _vertices.size() -1 === _edges.size()
  // path is _vertices[0] , _edges[0], _vertices[1] etc.

  /// @brief vertices
  std::deque<arangodb::velocypack::Slice> _vertices;

  /// @brief edges
  std::deque<arangodb::velocypack::Slice> _edges;
};

class TraversalPath {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Constructor. This is an abstract only class.
  //////////////////////////////////////////////////////////////////////////////

  TraversalPath() : _readDocuments(0) {}

  virtual ~TraversalPath() {}

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds the complete path as VelocyPack
  ///        Has the format:
  ///        {
  ///           vertices: [<vertex-as-velocypack>],
  ///           edges: [<edge-as-velocypack>]
  ///        }
  //////////////////////////////////////////////////////////////////////////////

  virtual void pathToVelocyPack(Transaction*,
                                arangodb::velocypack::Builder&) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the last edge on the path as VelocyPack
  //////////////////////////////////////////////////////////////////////////////

  virtual void lastEdgeToVelocyPack(Transaction*,
                                    arangodb::velocypack::Builder&) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the last vertex as VelocyPack
  //////////////////////////////////////////////////////////////////////////////

  virtual aql::AqlValue lastVertexToAqlValue(Transaction*) = 0;

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
  friend class BreadthFirstEnumerator;
  friend class DepthFirstEnumerator;

 protected:

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Class to read vertices.
  /////////////////////////////////////////////////////////////////////////////

  class VertexGetter {
   public:
    explicit VertexGetter(Traverser* traverser)
        : _traverser(traverser) {}

    virtual ~VertexGetter() = default;

    virtual bool getVertex(arangodb::velocypack::Slice,
                           std::vector<arangodb::velocypack::Slice>&);

    virtual bool getSingleVertex(arangodb::velocypack::Slice,
                                 arangodb::velocypack::Slice, size_t,
                                 arangodb::velocypack::Slice&);

    virtual void reset(arangodb::velocypack::Slice);

   protected:
    Traverser* _traverser;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Class to read vertices. Will return each vertex exactly once!
  /////////////////////////////////////////////////////////////////////////////

  class UniqueVertexGetter : public VertexGetter {
   public:
    explicit UniqueVertexGetter(Traverser* traverser)
        : VertexGetter(traverser) {}

    ~UniqueVertexGetter() = default;

    bool getVertex(arangodb::velocypack::Slice,
                   std::vector<arangodb::velocypack::Slice>&) override;

    bool getSingleVertex(arangodb::velocypack::Slice,
                         arangodb::velocypack::Slice, size_t,
                         arangodb::velocypack::Slice&) override;

    void reset(arangodb::velocypack::Slice) override;

   private:
    std::unordered_set<arangodb::velocypack::Slice> _returnedVertices;
  };


 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Constructor. This is an abstract only class.
  //////////////////////////////////////////////////////////////////////////////

  Traverser(TraverserOptions* opts, Transaction* trx);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Destructor
  //////////////////////////////////////////////////////////////////////////////

  virtual ~Traverser() {}

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

  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions
  ///        Also appends the _id value of the vertex in the given vector

 protected:
  virtual bool getVertex(arangodb::velocypack::Slice,
                         std::vector<arangodb::velocypack::Slice>&) = 0;

  /// @brief Function to load the other sides vertex of an edge
  ///        Returns true if the vertex passes filtering conditions

  virtual bool getSingleVertex(arangodb::velocypack::Slice,
                               arangodb::velocypack::Slice, size_t,
                               arangodb::velocypack::Slice&) = 0;
 public:
 
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the last vertex as AQLValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue lastVertexToAqlValue();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the last edge as AQLValue
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue lastEdgeToAqlValue();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Builds the complete path as AQLValue
  ///        Has the format:
  ///        {
  ///           vertices: [<vertex-as-velocypack>],
  ///           edges: [<edge-as-velocypack>]
  ///        }
  ///        NOTE: Will clear the given buffer and will leave the path in it.
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue pathToAqlValue(arangodb::velocypack::Builder&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the number of filtered paths
  //////////////////////////////////////////////////////////////////////////////

  size_t getAndResetFilteredPaths() {
    size_t tmp = _filteredPaths;
    _filteredPaths = 0;
    return tmp;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the number of documents loaded
  //////////////////////////////////////////////////////////////////////////////

  size_t getAndResetReadDocuments() {
    size_t tmp = _readDocuments;
    _readDocuments = 0;
    return tmp;
  }
  
  TraverserOptions const* options() { return _opts; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Simple check if there are potentially more paths.
  ///        It might return true although there are no more paths available.
  ///        If it returns false it is guaranteed that there are no more paths.
  //////////////////////////////////////////////////////////////////////////////

  bool hasMore() { return !_done; }

  bool edgeMatchesConditions(arangodb::velocypack::Slice,
                             arangodb::velocypack::Slice, size_t, size_t);

  bool vertexMatchesConditions(arangodb::velocypack::Slice, size_t);

 protected:

  /// @brief Outer top level transaction
  Transaction* _trx;

  /// @brief internal cursor to enumerate the paths of a graph
  std::unique_ptr<arangodb::traverser::PathEnumerator> _enumerator;

  /// @brief internal getter to extract an edge
  std::unique_ptr<VertexGetter> _vertexGetter;

  /// @brief Builder for the start value slice. Leased from transaction
  TransactionBuilderLeaser _startIdBuilder;

  /// @brief counter for all read documents
  size_t _readDocuments;

  /// @brief counter for all filtered paths
  size_t _filteredPaths;

  /// @brief toggle if this path should be pruned on next step
  bool _pruneNext;

  /// @brief indicator if this traversal is done
  bool _done;

  /// @brief options for traversal
  TraverserOptions* _opts;

  /// @brief Function to fetch the real data of a vertex into an AQLValue
  virtual aql::AqlValue fetchVertexData(arangodb::velocypack::Slice) = 0;

  /// @brief Function to fetch the real data of an edge into an AQLValue
  virtual aql::AqlValue fetchEdgeData(arangodb::velocypack::Slice) = 0;

  /// @brief Function to add the real data of a vertex into a velocypack builder
  virtual void addVertexToVelocyPack(arangodb::velocypack::Slice,
                                     arangodb::velocypack::Builder&) = 0;

  /// @brief Function to add the real data of an edge into a velocypack builder
  virtual void addEdgeToVelocyPack(arangodb::velocypack::Slice,
                                   arangodb::velocypack::Builder&) = 0;
};
}  // traverser
}  // arangodb

#endif
