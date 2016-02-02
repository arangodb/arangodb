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

#ifndef ARANGOD_V8_SERVER_V8_TRAVERSER_H
#define ARANGOD_V8_SERVER_V8_TRAVERSER_H 1

#include "Utils/ExplicitTransaction.h"
#include "VocBase/edge-collection.h"
#include "VocBase/ExampleMatcher.h"
#include "VocBase/Traverser.h"

namespace arangodb {
class Transaction;
}

class VocShaper;

struct EdgeInfo {
  TRI_voc_cid_t cid;
  TRI_doc_mptr_copy_t mptr;

  EdgeInfo(TRI_voc_cid_t pcid, TRI_doc_mptr_copy_t& pmptr)
      : cid(pcid), mptr(pmptr) {}

  bool operator==(EdgeInfo const& other) const {
    if (cid == other.cid && mptr._hash == other.mptr._hash) {
      // We have to look into the key now. The only source of truth.
      char const* l = TRI_EXTRACT_MARKER_KEY(&mptr);
      char const* r = TRI_EXTRACT_MARKER_KEY(&other.mptr);
      return strcmp(l, r) == 0;
    }
    return false;
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Template for information required by vertex filter.
///        Contains transaction, barrier and the Matcher Class.
////////////////////////////////////////////////////////////////////////////////

struct VertexFilterInfo {
  arangodb::ExplicitTransaction* trx;
  TRI_transaction_collection_t* col;
  arangodb::ExampleMatcher* matcher;

  VertexFilterInfo(arangodb::ExplicitTransaction* trx,
                   TRI_transaction_collection_t* col,
                   arangodb::ExampleMatcher* matcher)
      : trx(trx), col(col), matcher(matcher) {}
};

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef the template instantiation of the PathFinder
////////////////////////////////////////////////////////////////////////////////

typedef arangodb::basics::PathFinder<arangodb::traverser::VertexId,
                                     arangodb::traverser::EdgeId,
                                     double> ArangoDBPathFinder;

typedef arangodb::basics::ConstDistanceFinder<arangodb::traverser::VertexId,
                                              arangodb::traverser::EdgeId>
    ArangoDBConstDistancePathFinder;

namespace arangodb {
namespace traverser {

// A collection of shared options used in several functions.
// Should not be used directly, use specialization instead.
struct BasicOptions {
 protected:
  std::unordered_map<TRI_voc_cid_t, arangodb::ExampleMatcher*> _edgeFilter;
  std::unordered_map<TRI_voc_cid_t, VertexFilterInfo> _vertexFilter;

  BasicOptions() : useEdgeFilter(false), useVertexFilter(false) {}

  ~BasicOptions() {
    // properly clean up the mess
    for (auto& it : _edgeFilter) {
      delete it.second;
    }
    for (auto& it : _vertexFilter) {
      delete it.second.matcher;
      it.second.matcher = nullptr;
    }
  }

 public:
  VertexId start;
  bool useEdgeFilter;
  bool useVertexFilter;

  void addEdgeFilter(v8::Isolate* isolate, v8::Handle<v8::Value> const& example,
                     VocShaper* shaper, TRI_voc_cid_t const& cid,
                     std::string& errorMessage);

  void addEdgeFilter(arangodb::basics::Json const& example, VocShaper* shaper,
                     TRI_voc_cid_t const& cid,
                     arangodb::CollectionNameResolver const* resolver);

  void addVertexFilter(v8::Isolate* isolate,
                       v8::Handle<v8::Value> const& example,
                       arangodb::ExplicitTransaction* trx,
                       TRI_transaction_collection_t* col, VocShaper* shaper,
                       TRI_voc_cid_t const& cid, std::string& errorMessage);

  bool matchesEdge(EdgeId& e, TRI_doc_mptr_copy_t* edge) const;

  bool matchesVertex(VertexId const& v) const;
};

struct NeighborsOptions : BasicOptions {
 private:
  std::unordered_set<TRI_voc_cid_t> _explicitCollections;

 public:
  TRI_edge_direction_e direction;
  uint64_t minDepth;
  uint64_t maxDepth;

  NeighborsOptions() : direction(TRI_EDGE_OUT), minDepth(1), maxDepth(1) {}

  bool matchesVertex(VertexId const&) const;

  void addCollectionRestriction(TRI_voc_cid_t cid);
};

struct ShortestPathOptions : BasicOptions {
 public:
  std::string direction;
  bool useWeight;
  std::string weightAttribute;
  double defaultWeight;
  bool bidirectional;
  bool multiThreaded;
  VertexId end;

  ShortestPathOptions()
      : direction("outbound"),
        useWeight(false),
        weightAttribute(""),
        defaultWeight(1),
        bidirectional(true),
        multiThreaded(true) {}

  bool matchesVertex(VertexId const&) const;
};

class SingleServerTraversalPath : public TraversalPath {
 public:
  explicit SingleServerTraversalPath(
      arangodb::basics::EnumeratedPath<EdgeInfo, VertexId> const& path)
      : _path(path) {}

  ~SingleServerTraversalPath() {}

  arangodb::basics::Json* pathToJson(
      arangodb::Transaction*, arangodb::CollectionNameResolver*) override;

  arangodb::basics::Json* lastEdgeToJson(
      arangodb::Transaction*, arangodb::CollectionNameResolver*) override;

  arangodb::basics::Json* lastVertexToJson(
      arangodb::Transaction*, arangodb::CollectionNameResolver*) override;

 private:
  arangodb::basics::Json* edgeToJson(Transaction* trx,
                                     CollectionNameResolver* resolver,
                                     EdgeInfo const& e);

  arangodb::basics::Json* vertexToJson(Transaction* trx,
                                       CollectionNameResolver* resolver,
                                       VertexId const& v);

  arangodb::basics::EnumeratedPath<EdgeInfo, VertexId> _path;
};

class DepthFirstTraverser : public Traverser {

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief callable class to load edges based on opts.
  //////////////////////////////////////////////////////////////////////////////

  class EdgeGetter {
   public:
    EdgeGetter(DepthFirstTraverser* traverser,
                        TraverserOptions const& opts,
                        CollectionNameResolver* resolver, Transaction* trx)
        : _traverser(traverser), _resolver(resolver), _opts(opts), _trx(trx) {}

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Function to fill the list of edges properly.
    //////////////////////////////////////////////////////////////////////////////

    void operator()(VertexId const&, std::vector<EdgeInfo>&,
                    TRI_doc_mptr_t*&, size_t&, bool&);

   private:

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Get an edge index for the given collection by name
    //////////////////////////////////////////////////////////////////////////////
    
    EdgeIndex* getEdgeIndex(std::string const&, TRI_voc_cid_t&);

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Collection name resolver
    //////////////////////////////////////////////////////////////////////////////
    DepthFirstTraverser* _traverser;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Collection name resolver
    //////////////////////////////////////////////////////////////////////////////
    CollectionNameResolver* _resolver;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Cache for indexes. Maps collectionName to Index
    //////////////////////////////////////////////////////////////////////////////
    std::unordered_map<std::string, std::pair<TRI_voc_cid_t, EdgeIndex*>>
        _indexCache;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Traverser options
    //////////////////////////////////////////////////////////////////////////////
    TraverserOptions _opts;

    //////////////////////////////////////////////////////////////////////////////
    /// @brief Pointer to active transaction
    ///        All Edge Collections have to be properly locked before traversing!
    //////////////////////////////////////////////////////////////////////////////
    Transaction* _trx;
  };

  //////////////////////////////////////////////////////////////////////////////
  /// @brief collection name resolver
  //////////////////////////////////////////////////////////////////////////////

  CollectionNameResolver* _resolver;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal cursor to enumerate the paths of a graph
  //////////////////////////////////////////////////////////////////////////////

  std::unique_ptr<arangodb::basics::PathEnumerator<
      EdgeInfo, VertexId, TRI_doc_mptr_t>> _enumerator;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal getter to extract an edge
  //////////////////////////////////////////////////////////////////////////////

  EdgeGetter _edgeGetter;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal function to extract vertex information
  //////////////////////////////////////////////////////////////////////////////

  std::function<bool(EdgeInfo const&, VertexId const&, size_t, VertexId&)>
      _getVertex;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief a vector containing all required edge collection structures
  //////////////////////////////////////////////////////////////////////////////

  std::vector<TRI_document_collection_t*> _edgeCols;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Outer top level transaction
  //////////////////////////////////////////////////////////////////////////////

  Transaction* _trx;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief internal function to define the _getVertex and _getEdge functions
  //////////////////////////////////////////////////////////////////////////////

  void _defInternalFunctions();

 public:
  DepthFirstTraverser(
      std::vector<TRI_document_collection_t*> const&, TraverserOptions&,
      CollectionNameResolver*, Transaction*,
      std::unordered_map<size_t, std::vector<TraverserExpression*>> const*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reset the traverser to use another start vertex
  //////////////////////////////////////////////////////////////////////////////

  void setStartVertex(VertexId const& v) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Get the next possible path in the graph.
  //////////////////////////////////////////////////////////////////////////////

  TraversalPath* next() override;

 private:
  bool edgeMatchesConditions(TRI_doc_mptr_t&, size_t&, size_t);

  bool vertexMatchesConditions(VertexId const&, size_t);
};
}
}

////////////////////////////////////////////////////////////////////////////////
/// @brief callback to weight an edge
////////////////////////////////////////////////////////////////////////////////

typedef std::function<double(TRI_doc_mptr_copy_t& edge)>
    WeightCalculatorFunction;

////////////////////////////////////////////////////////////////////////////////
/// @brief Information required internally of the traverser.
///        Used to easily pass around collections.
///        Also offer abstraction to extract edges.
////////////////////////////////////////////////////////////////////////////////

class EdgeCollectionInfo {
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the underlying transaction
  //////////////////////////////////////////////////////////////////////////////

  arangodb::Transaction* _trx;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief edge collection
  //////////////////////////////////////////////////////////////////////////////

  TRI_voc_cid_t _edgeCollectionCid;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief edge collection
  //////////////////////////////////////////////////////////////////////////////

  TRI_document_collection_t* _edgeCollection;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief weighter functions
  //////////////////////////////////////////////////////////////////////////////

  WeightCalculatorFunction _weighter;

 public:
  EdgeCollectionInfo(arangodb::Transaction* trx,
                     TRI_voc_cid_t& edgeCollectionCid,
                     TRI_document_collection_t* edgeCollection,
                     WeightCalculatorFunction weighter)
      : _trx(trx),
        _edgeCollectionCid(edgeCollectionCid),
        _edgeCollection(edgeCollection),
        _weighter(weighter) {}

  arangodb::traverser::EdgeId extractEdgeId(TRI_doc_mptr_copy_t& ptr) {
    return arangodb::traverser::EdgeId(_edgeCollectionCid,
                                       TRI_EXTRACT_MARKER_KEY(&ptr));
  }

  std::vector<TRI_doc_mptr_copy_t> getEdges(
      TRI_edge_direction_e direction,
      arangodb::traverser::VertexId const& vertexId) const {
    return TRI_LookupEdgesDocumentCollection(_trx, _edgeCollection, direction,
                                             vertexId.cid,
                                             const_cast<char*>(vertexId.key));
  }

  TRI_voc_cid_t getCid() { return _edgeCollectionCid; }

  VocShaper* getShaper() { return _edgeCollection->getShaper(); }

  double weightEdge(TRI_doc_mptr_copy_t& ptr) { return _weighter(ptr); }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Information required internally of the traverser.
///        Used to easily pass around collections.
////////////////////////////////////////////////////////////////////////////////

class VertexCollectionInfo {
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief vertex collection
  //////////////////////////////////////////////////////////////////////////////

  TRI_voc_cid_t _vertexCollectionCid;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief vertex collection
  //////////////////////////////////////////////////////////////////////////////

  TRI_transaction_collection_t* _vertexCollection;

 public:
  VertexCollectionInfo(TRI_voc_cid_t& vertexCollectionCid,
                       TRI_transaction_collection_t* vertexCollection)
      : _vertexCollectionCid(vertexCollectionCid),
        _vertexCollection(vertexCollection) {}

  TRI_voc_cid_t getCid() { return _vertexCollectionCid; }

  TRI_transaction_collection_t* getCollection() { return _vertexCollection; }

  VocShaper* getShaper() {
    return _vertexCollection->_collection->_collection->getShaper();
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the shortest path computation
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ArangoDBPathFinder::Path> TRI_RunShortestPathSearch(
    std::vector<EdgeCollectionInfo*>& collectionInfos,
    arangodb::traverser::ShortestPathOptions& opts);

std::unique_ptr<ArangoDBConstDistancePathFinder::Path>
TRI_RunSimpleShortestPathSearch(
    std::vector<EdgeCollectionInfo*>& collectionInfos,
    arangodb::traverser::ShortestPathOptions& opts);

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the neighbors computation
////////////////////////////////////////////////////////////////////////////////

void TRI_RunNeighborsSearch(
    std::vector<EdgeCollectionInfo*>& collectionInfos,
    arangodb::traverser::NeighborsOptions& opts,
    std::unordered_set<arangodb::traverser::VertexId>& distinct);

#endif
