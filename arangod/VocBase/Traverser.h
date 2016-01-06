////////////////////////////////////////////////////////////////////////////////
/// @brief Traverser
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_VOC_BASE_TRAVERSER_H
#define ARANGOD_VOC_BASE_TRAVERSER_H 1

#include "Basics/Common.h"
#include "Basics/Traverser.h"
#include "Aql/AstNode.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/Transaction.h"
#include "VocBase/DocumentAccessor.h"
#include "VocBase/voc-types.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace triagens {
namespace arango {
namespace traverser {


////////////////////////////////////////////////////////////////////////////////
/// @brief Template for a vertex id. Is simply a pair of cid and key
/// NOTE: This struct will never free the value asigned to char const* key
///       The environment has to make sure that the string it points to is
///       not freed as long as this struct is in use!
////////////////////////////////////////////////////////////////////////////////

struct VertexId {
  TRI_voc_cid_t cid;
  char const* key;

  VertexId() : cid(0), key("") {}

  VertexId(TRI_voc_cid_t cid, char const* key) : cid(cid), key(key) {}

  bool operator==(VertexId const& other) const {
    if (cid == other.cid) {
      return strcmp(key, other.key) == 0;
    }
    return false;
  }

  std::string toString(
      triagens::arango::CollectionNameResolver const* resolver) const {
    return resolver->getCollectionNameCluster(cid) + "/" + std::string(key);
  }
};

// EdgeId and VertexId are similar here. both have a key and a cid
typedef VertexId EdgeId;

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper function to convert an _id string into a VertexId
////////////////////////////////////////////////////////////////////////////////

VertexId IdStringToVertexId(
    triagens::arango::CollectionNameResolver const* resolver,
    std::string const& vertex);


class TraverserExpression {
 public:
  bool isEdgeAccess;
  triagens::aql::AstNodeType comparisonType;
  triagens::aql::AstNode const* varAccess;
  std::unique_ptr<triagens::basics::Json> compareTo;

  TraverserExpression(bool pisEdgeAccess,
                      triagens::aql::AstNodeType pcomparisonType,
                      triagens::aql::AstNode const* pvarAccess)
      : isEdgeAccess(pisEdgeAccess),
        comparisonType(pcomparisonType),
        varAccess(pvarAccess),
        compareTo(nullptr) {}

  explicit TraverserExpression(VPackSlice const& slice);

  virtual ~TraverserExpression() {
    // no need to destroy varAccess here. Its memory is managed via the
    // _nodeRegister variable in this class

    for (auto& it : _stringRegister) {
      delete it;
    }
  }

  void toJson(triagens::basics::Json& json, TRI_memory_zone_t* zone) const;

  bool matchesCheck(TRI_doc_mptr_t& element,
                    TRI_document_collection_t* collection,
                    CollectionNameResolver const* resolver) const;

  bool matchesCheck(TRI_json_t const* element) const;

  bool matchesCheck(VPackSlice const& element) const;

  bool matchesCheck(DocumentAccessor& accessor) const;

 protected:
  TraverserExpression()
      : isEdgeAccess(false),
        comparisonType(triagens::aql::NODE_TYPE_ROOT),
        varAccess(nullptr),
        compareTo(nullptr) {}

 private:
  bool recursiveCheck(triagens::aql::AstNode const*, DocumentAccessor&) const;

  // Required when creating this expression without AST
  std::vector<std::unique_ptr<triagens::aql::AstNode const>> _nodeRegister;
  std::vector<std::string*> _stringRegister;
};


class TraversalPath {
 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Constructor. This is an abstract only class.
  ////////////////////////////////////////////////////////////////////////////////

  TraversalPath() : _readDocuments(0) {}

  virtual ~TraversalPath() {}

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Builds the complete path as Json
  ///        Has the format:
  ///        {
  ///           vertices: [<vertex-as-json>],
  ///           edges: [<edge-as-json>]
  ///        }
  ////////////////////////////////////////////////////////////////////////////////

  virtual triagens::basics::Json* pathToJson(Transaction*,
                                             CollectionNameResolver*) = 0;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the last edge on the path as Json
  ////////////////////////////////////////////////////////////////////////////////

  virtual triagens::basics::Json* lastEdgeToJson(Transaction*,
                                                 CollectionNameResolver*) = 0;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Builds only the last vertex as Json
  ////////////////////////////////////////////////////////////////////////////////

  virtual triagens::basics::Json* lastVertexToJson(Transaction*,
                                                   CollectionNameResolver*) = 0;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Gets the amount of read documents
  ////////////////////////////////////////////////////////////////////////////////

  size_t getReadDocuments() const { return _readDocuments; }

 protected:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Count how many documents have been read
  ////////////////////////////////////////////////////////////////////////////////

  size_t _readDocuments;
};


struct TraverserOptions {
 private:
  std::function<bool(const TraversalPath* path)> pruningFunction;

 public:
  TRI_edge_direction_e direction;

  uint64_t minDepth;

  uint64_t maxDepth;

  bool usesPrune;

  TraverserOptions()
      : direction(TRI_EDGE_OUT), minDepth(1), maxDepth(1), usesPrune(false) {}

  void setPruningFunction(
      std::function<bool(TraversalPath const* path)> const& callback) {
    pruningFunction = callback;
    usesPrune = true;
  }

  bool shouldPrunePath(TraversalPath const* path) {
    if (!usesPrune) {
      return false;
    }
    return pruningFunction(path);
  }
};


class Traverser {
 public:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Constructor. This is an abstract only class.
  ////////////////////////////////////////////////////////////////////////////////

  Traverser()
      : _readDocuments(0),
        _filteredPaths(0),
        _pruneNext(false),
        _done(true),
        _expressions(nullptr) {}

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Constructor. This is an abstract only class.
  ////////////////////////////////////////////////////////////////////////////////

  Traverser(TraverserOptions& opts,
            std::unordered_map<size_t, std::vector<TraverserExpression*>> const*
                expressions)
      : _readDocuments(0),
        _filteredPaths(0),
        _pruneNext(false),
        _done(true),
        _opts(opts),
        _expressions(expressions) {}

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Destructor
  ////////////////////////////////////////////////////////////////////////////////

  virtual ~Traverser() {}

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Reset the traverser to use another start vertex
  ////////////////////////////////////////////////////////////////////////////////

  virtual void setStartVertex(VertexId const& v) = 0;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Skip amount many paths of the graph.
  ////////////////////////////////////////////////////////////////////////////////

  size_t skip(size_t amount) {
    size_t skipped = 0;
    for (size_t i = 0; i < amount; ++i) {
      std::unique_ptr<TraversalPath> p(next());
      if (p == nullptr) {
        _done = true;
        break;
      }
      ++skipped;
    }
    return skipped;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next possible path in the graph.
  ////////////////////////////////////////////////////////////////////////////////

  virtual TraversalPath* next() = 0;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Get the number of filtered paths
  ////////////////////////////////////////////////////////////////////////////////

  size_t getAndResetFilteredPaths() {
    size_t tmp = _filteredPaths;
    _filteredPaths = 0;
    return tmp;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Get the number of documents loaded
  ////////////////////////////////////////////////////////////////////////////////

  size_t getAndResetReadDocuments() {
    size_t tmp = _readDocuments;
    _readDocuments = 0;
    return tmp;
  }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Prune the current path prefix. Do not evaluate it any further.
  ////////////////////////////////////////////////////////////////////////////////

  void prune() { _pruneNext = true; }

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief Simple check if there potentially more paths.
  ///        It might return true although there are no more paths available.
  ///        If it returns false it is guaranteed that there are no more paths.
  ////////////////////////////////////////////////////////////////////////////////

  bool hasMore() { return !_done; }

 protected:
  ////////////////////////////////////////////////////////////////////////////////
  /// @brief counter for all read documents
  ////////////////////////////////////////////////////////////////////////////////

  size_t _readDocuments;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief counter for all filtered paths
  ////////////////////////////////////////////////////////////////////////////////

  size_t _filteredPaths;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief toggle if this path should be pruned on next step
  ////////////////////////////////////////////////////////////////////////////////

  bool _pruneNext;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief indicator if this traversal is done
  ////////////////////////////////////////////////////////////////////////////////

  bool _done;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief options for traversal
  ////////////////////////////////////////////////////////////////////////////////

  TraverserOptions _opts;

  ////////////////////////////////////////////////////////////////////////////////
  /// @brief a vector containing all information for early pruning
  ////////////////////////////////////////////////////////////////////////////////

  std::unordered_map<size_t, std::vector<TraverserExpression*>> const*
      _expressions;
};

}  // traverser
}  // arango
}  // triagens


namespace std {
template <>
struct hash<triagens::arango::traverser::VertexId> {
 public:
  size_t operator()(triagens::arango::traverser::VertexId const& s) const {
    size_t h1 = std::hash<TRI_voc_cid_t>()(s.cid);
    size_t h2 = TRI_FnvHashString(s.key);
    return h1 ^ (h2 << 1);
  }
};

template <>
struct equal_to<triagens::arango::traverser::VertexId> {
 public:
  bool operator()(triagens::arango::traverser::VertexId const& s,
                  triagens::arango::traverser::VertexId const& t) const {
    return s.cid == t.cid && strcmp(s.key, t.key) == 0;
  }
};

template <>
struct less<triagens::arango::traverser::VertexId> {
 public:
  bool operator()(triagens::arango::traverser::VertexId const& lhs,
                  triagens::arango::traverser::VertexId const& rhs) {
    if (lhs.cid != rhs.cid) {
      return lhs.cid < rhs.cid;
    }
    return strcmp(lhs.key, rhs.key) < 0;
  }
};
}

#endif
