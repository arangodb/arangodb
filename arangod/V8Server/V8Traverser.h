////////////////////////////////////////////////////////////////////////////////
/// @brief V8 Traverser
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

#ifndef ARANGODB_V8_TRAVERSER_H
#define ARANGODB_V8_TRAVERSER_H 1

#include "Utils/ExplicitTransaction.h"
#include "VocBase/edge-collection.h"
#include "VocBase/ExampleMatcher.h"
#include "VocBase/Traverser.h"

namespace triagens {
  namespace arango {
    class Transaction;
  }
}

class VocShaper;

struct EdgeInfo {
  TRI_voc_cid_t cid;
  TRI_doc_mptr_copy_t mptr;

  EdgeInfo (
    TRI_voc_cid_t pcid,
    TRI_doc_mptr_copy_t& pmptr
  ) : cid(pcid), mptr(pmptr) { }

  bool operator== (EdgeInfo const& other) const {
    if (cid == other.cid &&
        mptr._hash == other.mptr._hash) {
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
  triagens::arango::ExplicitTransaction* trx;
  TRI_transaction_collection_t* col;
  triagens::arango::ExampleMatcher* matcher;

  VertexFilterInfo (triagens::arango::ExplicitTransaction* trx,
                    TRI_transaction_collection_t* col,
                    triagens::arango::ExampleMatcher* matcher) 
    : trx(trx), 
      col(col), 
      matcher(matcher) {
    }

};

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef the template instantiation of the PathFinder
////////////////////////////////////////////////////////////////////////////////

typedef triagens::basics::PathFinder<triagens::arango::traverser::VertexId,
                                     triagens::arango::traverser::EdgeId,
                                     double> 
        ArangoDBPathFinder;

typedef triagens::basics::ConstDistanceFinder<triagens::arango::traverser::VertexId,
                                              triagens::arango::traverser::EdgeId>
        ArangoDBConstDistancePathFinder;

namespace triagens {
  namespace arango {
    namespace traverser {

      // A collection of shared options used in several functions.
      // Should not be used directly, use specialization instead.
      struct BasicOptions {

        protected:

          std::unordered_map<TRI_voc_cid_t, triagens::arango::ExampleMatcher*> _edgeFilter;
          std::unordered_map<TRI_voc_cid_t, VertexFilterInfo> _vertexFilter;

          BasicOptions () :
            useEdgeFilter(false),
            useVertexFilter(false) {
          }

          ~BasicOptions () {
            // properly clean up the mess
            for (auto& it : _edgeFilter) {
              delete it.second;
            }
            for (auto& it: _vertexFilter) {
              delete it.second.matcher;
              it.second.matcher = nullptr;
            }
          }

        public:
          VertexId start;
          bool useEdgeFilter;
          bool useVertexFilter;

          void addEdgeFilter (v8::Isolate* isolate,
                              v8::Handle<v8::Value> const& example,
                              VocShaper* shaper,
                              TRI_voc_cid_t const& cid,
                              std::string& errorMessage);

          void addEdgeFilter (triagens::basics::Json const& example,
                              VocShaper* shaper,
                              TRI_voc_cid_t const& cid,
                              triagens::arango::CollectionNameResolver const* resolver);

          void addVertexFilter (v8::Isolate* isolate,
                                v8::Handle<v8::Value> const& example,
                                triagens::arango::ExplicitTransaction* trx,
                                TRI_transaction_collection_t* col,
                                VocShaper* shaper,
                                TRI_voc_cid_t const& cid,
                                std::string& errorMessage);

          bool matchesEdge (EdgeId& e, TRI_doc_mptr_copy_t* edge) const;

          bool matchesVertex (VertexId const& v) const;

      };
 
      struct NeighborsOptions : BasicOptions {

        private:
          std::unordered_set<TRI_voc_cid_t> _explicitCollections; 

        public:
          TRI_edge_direction_e direction;
          uint64_t minDepth;
          uint64_t maxDepth;

          NeighborsOptions () 
            : direction(TRI_EDGE_OUT),
              minDepth(1),
              maxDepth(1) {
          }

          bool matchesVertex (VertexId const&) const;

          void addCollectionRestriction (TRI_voc_cid_t cid);
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

          ShortestPathOptions () 
            : direction("outbound"),
              useWeight(false),
              weightAttribute(""),
              defaultWeight(1),
              bidirectional(true),
              multiThreaded(true) {
          }
          
          bool matchesVertex (VertexId const&) const;

      };

// -----------------------------------------------------------------------------
// --SECTION--                                   class SingleServerTraversalPath
// -----------------------------------------------------------------------------

      class SingleServerTraversalPath : public TraversalPath {
        
// -----------------------------------------------------------------------------
// --SECTION--                                                  public functions
// -----------------------------------------------------------------------------

        public:

          explicit SingleServerTraversalPath (triagens::basics::EnumeratedPath<EdgeInfo, VertexId> const& path) 
            : _path(path) {
          }

          ~SingleServerTraversalPath () {
          }

          triagens::basics::Json* pathToJson (triagens::arango::Transaction*,
                                              triagens::arango::CollectionNameResolver*) override;

          triagens::basics::Json* lastEdgeToJson (triagens::arango::Transaction*,
                                                  triagens::arango::CollectionNameResolver*) override;

          triagens::basics::Json* lastVertexToJson (triagens::arango::Transaction*,
                                                    triagens::arango::CollectionNameResolver*) override;

        private:

// -----------------------------------------------------------------------------
// --SECTION--                                                 private functions
// -----------------------------------------------------------------------------

          triagens::basics::Json* edgeToJson (Transaction* trx,
                                              CollectionNameResolver* resolver,
                                              EdgeInfo const& e);

          triagens::basics::Json* vertexToJson (Transaction* trx,
                                                CollectionNameResolver* resolver,
                                                VertexId const& v);

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

          triagens::basics::EnumeratedPath<EdgeInfo, VertexId> _path;

      };

// -----------------------------------------------------------------------------
// --SECTION--                                         class DepthFirstTraverser
// -----------------------------------------------------------------------------
      class DepthFirstTraverser : public Traverser {

        private:

////////////////////////////////////////////////////////////////////////////////
/// @brief collection name resolver
////////////////////////////////////////////////////////////////////////////////

          CollectionNameResolver* _resolver;

////////////////////////////////////////////////////////////////////////////////
/// @brief internal cursor to enumerate the paths of a graph
////////////////////////////////////////////////////////////////////////////////

          std::unique_ptr<triagens::basics::PathEnumerator<EdgeInfo,
                                                           VertexId,
                                                           TRI_doc_mptr_copy_t>> _enumerator;

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to extract an edge
////////////////////////////////////////////////////////////////////////////////

          std::function<void(VertexId const&,
                             std::vector<EdgeInfo>&,
                             TRI_doc_mptr_copy_t*&,
                             size_t&,
                             bool&)>              _getEdge;

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to extract vertex information
////////////////////////////////////////////////////////////////////////////////

          std::function<bool (EdgeInfo const&, VertexId const&, size_t, VertexId&)> _getVertex;

////////////////////////////////////////////////////////////////////////////////
/// @brief a vector containing all required edge collection structures
////////////////////////////////////////////////////////////////////////////////

          std::vector<TRI_document_collection_t*> _edgeCols;

////////////////////////////////////////////////////////////////////////////////
/// @brief Outer top level transaction
////////////////////////////////////////////////////////////////////////////////

          Transaction* _trx;

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to define the _getVertex and _getEdge functions
////////////////////////////////////////////////////////////////////////////////

          void _defInternalFunctions ();

        public:

          DepthFirstTraverser (
            std::vector<TRI_document_collection_t*> const&,
            TraverserOptions&,
            CollectionNameResolver*,
            Transaction*,
            std::unordered_map<size_t, std::vector<TraverserExpression*>> const*
          );

////////////////////////////////////////////////////////////////////////////////
/// @brief Reset the traverser to use another start vertex
////////////////////////////////////////////////////////////////////////////////

          void setStartVertex (VertexId const& v) override;

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the next possible path in the graph.
////////////////////////////////////////////////////////////////////////////////

          TraversalPath* next () override;

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

        private:

          bool edgeMatchesConditions (TRI_doc_mptr_t&,
                                      size_t&,
                                      size_t);

          bool vertexMatchesConditions (VertexId const&,
                                        size_t);
      };

    }
  }
}


////////////////////////////////////////////////////////////////////////////////
/// @brief callback to weight an edge
////////////////////////////////////////////////////////////////////////////////

typedef std::function<double(TRI_doc_mptr_copy_t& edge)> WeightCalculatorFunction;

////////////////////////////////////////////////////////////////////////////////
/// @brief Information required internally of the traverser.
///        Used to easily pass around collections.
///        Also offer abstraction to extract edges.
////////////////////////////////////////////////////////////////////////////////

class EdgeCollectionInfo {

  private:

////////////////////////////////////////////////////////////////////////////////
/// @brief the underlying transaction
////////////////////////////////////////////////////////////////////////////////

    triagens::arango::Transaction* _trx;
        
////////////////////////////////////////////////////////////////////////////////
/// @brief edge collection
////////////////////////////////////////////////////////////////////////////////

    TRI_voc_cid_t _edgeCollectionCid;

////////////////////////////////////////////////////////////////////////////////
/// @brief edge collection
////////////////////////////////////////////////////////////////////////////////

    TRI_document_collection_t* _edgeCollection;

////////////////////////////////////////////////////////////////////////////////
/// @brief weighter functions
////////////////////////////////////////////////////////////////////////////////

    WeightCalculatorFunction _weighter;

  public:

    EdgeCollectionInfo (triagens::arango::Transaction* trx,
                        TRI_voc_cid_t& edgeCollectionCid,
                        TRI_document_collection_t* edgeCollection,
                        WeightCalculatorFunction weighter)
      : _trx(trx),
        _edgeCollectionCid(edgeCollectionCid),
        _edgeCollection(edgeCollection),
        _weighter(weighter) {
    }

    triagens::arango::traverser::EdgeId extractEdgeId (TRI_doc_mptr_copy_t& ptr) {
      return triagens::arango::traverser::EdgeId(_edgeCollectionCid, TRI_EXTRACT_MARKER_KEY(&ptr));
    }

    std::vector<TRI_doc_mptr_copy_t> getEdges (TRI_edge_direction_e direction,
                                               triagens::arango::traverser::VertexId const& vertexId) const {
      return TRI_LookupEdgesDocumentCollection(_trx, _edgeCollection,
                   direction, vertexId.cid, const_cast<char*>(vertexId.key));
    }

    TRI_voc_cid_t getCid () {
      return _edgeCollectionCid;
    }

    VocShaper* getShaper () {
      return _edgeCollection->getShaper();
    }

    double weightEdge (TRI_doc_mptr_copy_t& ptr) {
      return _weighter(ptr);
    }

};

////////////////////////////////////////////////////////////////////////////////
/// @brief Information required internally of the traverser.
///        Used to easily pass around collections.
////////////////////////////////////////////////////////////////////////////////

class VertexCollectionInfo {

  private:
    
////////////////////////////////////////////////////////////////////////////////
/// @brief vertex collection
////////////////////////////////////////////////////////////////////////////////

    TRI_voc_cid_t _vertexCollectionCid;

////////////////////////////////////////////////////////////////////////////////
/// @brief vertex collection
////////////////////////////////////////////////////////////////////////////////

    TRI_transaction_collection_t* _vertexCollection;

  public:

    VertexCollectionInfo (TRI_voc_cid_t& vertexCollectionCid,
                          TRI_transaction_collection_t* vertexCollection) 
      : _vertexCollectionCid(vertexCollectionCid),
        _vertexCollection(vertexCollection) {
    }

    TRI_voc_cid_t getCid () {
      return _vertexCollectionCid;
    }

    TRI_transaction_collection_t* getCollection () {
      return _vertexCollection;
    }

    VocShaper* getShaper () {
      return _vertexCollection->_collection->_collection->getShaper();
    }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the shortest path computation
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ArangoDBPathFinder::Path> TRI_RunShortestPathSearch (
    std::vector<EdgeCollectionInfo*>& collectionInfos,
    triagens::arango::traverser::ShortestPathOptions& opts
);

std::unique_ptr<ArangoDBConstDistancePathFinder::Path> TRI_RunSimpleShortestPathSearch (
    std::vector<EdgeCollectionInfo*>& collectionInfos,
    triagens::arango::traverser::ShortestPathOptions& opts
);

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the neighbors computation
////////////////////////////////////////////////////////////////////////////////

void TRI_RunNeighborsSearch (std::vector<EdgeCollectionInfo*>& collectionInfos,
                             triagens::arango::traverser::NeighborsOptions& opts,
                             std::unordered_set<triagens::arango::traverser::VertexId>& distinct);

#endif
