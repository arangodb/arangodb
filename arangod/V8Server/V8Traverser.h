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

#include "Basics/Common.h"
#include "Basics/Traverser.h"
#include "Utils/ExplicitTransaction.h"
#include "VocBase/edge-collection.h"
#include "VocBase/ExampleMatcher.h"

class VocShaper;

////////////////////////////////////////////////////////////////////////////////
/// @brief Template for a vertex id. Is simply a pair of cid and key
/// NOTE: This struct will never free the value asigned to char const* key
///       The environment has to make sure that the string it points to is
///       not freed as long as this struct is in use!
////////////////////////////////////////////////////////////////////////////////

struct VertexId {
  TRI_voc_cid_t cid;
  char const* key;

  VertexId () 
    : cid(0), 
      key(nullptr) {
  }

  VertexId (TRI_voc_cid_t cid, char const* key) 
    : cid(cid),
      key(key) {
  }

  bool operator== (const VertexId& other) const {
    if (cid == other.cid) {
      return strcmp(key, other.key) == 0;
    }
    return false;
  }

};

struct EdgeInfo {
  TRI_voc_cid_t cid;
  TRI_doc_mptr_copy_t mptr;

  EdgeInfo (
    TRI_voc_cid_t& pcid,
    TRI_doc_mptr_copy_t& pmptr
  ) : cid(pcid), mptr(pmptr) { }
};

namespace std {
  template<>
  struct hash<VertexId> {
    public:
      size_t operator() (VertexId const& s) const {
        size_t h1 = std::hash<TRI_voc_cid_t>()(s.cid);
        size_t h2 = TRI_FnvHashString(s.key);
        return h1 ^ ( h2 << 1 );
      }
  };

  template<>
  struct equal_to<VertexId> {
    public:
      bool operator() (VertexId const& s, VertexId const& t) const {
        return s.cid == t.cid && strcmp(s.key, t.key) == 0;
      }
  };

  template<>
    struct less<VertexId> {
      public:
        bool operator() (const VertexId& lhs, const VertexId& rhs) {
          if (lhs.cid != rhs.cid) {
            return lhs.cid < rhs.cid;
          }
          return strcmp(lhs.key, rhs.key) < 0;
        }
    };

}

// EdgeId and VertexId are similar here. both have a key and a cid
typedef VertexId EdgeId; 

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
/// @brief typedef the template instanciation of the PathFinder
////////////////////////////////////////////////////////////////////////////////

typedef triagens::basics::PathFinder<VertexId, EdgeId, double> 
        ArangoDBPathFinder;

typedef triagens::basics::ConstDistanceFinder<VertexId, EdgeId> 
        ArangoDBConstDistancePathFinder;

namespace triagens {
  namespace basics {
    namespace traverser {

////////////////////////////////////////////////////////////////////////////////
/// @brief Helper function to convert an _id string into a VertexId
////////////////////////////////////////////////////////////////////////////////
      VertexId IdStringToVertexId (triagens::arango::CollectionNameResolver const* resolver,
                                    std::string const& vertex);

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

          void addEdgeFilter (Json const& example,
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
// --SECTION--                                                 Traverser options
// -----------------------------------------------------------------------------

      struct TraverserOptions {

        private:
          std::function<bool (const TraversalPath<EdgeInfo, VertexId>& path)> pruningFunction;

        public:
          TRI_edge_direction_e direction;

          uint64_t minDepth;

          uint64_t maxDepth;

          bool usesPrune;


          TraverserOptions () : 
            direction(TRI_EDGE_OUT),
            minDepth(1),
            maxDepth(1),
            usesPrune(false)
          { };

          void setPruningFunction (
            std::function<bool (const TraversalPath<EdgeInfo, VertexId>& path)> callback
          ) {
            pruningFunction = callback;
            usesPrune = true;
          }

          bool shouldPrunePath (
            const TraversalPath<EdgeInfo, VertexId>& path
          ) {
            if (!usesPrune) {
              return false;
            }
            return pruningFunction(path);
          }

      };

// -----------------------------------------------------------------------------
// --SECTION--                                         class DepthFirstTraverser
// -----------------------------------------------------------------------------
      class DepthFirstTraverser {

        private:

////////////////////////////////////////////////////////////////////////////////
/// @brief options for traversal
////////////////////////////////////////////////////////////////////////////////

          bool _done;

////////////////////////////////////////////////////////////////////////////////
/// @brief options for traversal
////////////////////////////////////////////////////////////////////////////////

          TraverserOptions _opts;

////////////////////////////////////////////////////////////////////////////////
/// @brief toggle if this path should be pruned on next step
////////////////////////////////////////////////////////////////////////////////

          bool _pruneNext;

////////////////////////////////////////////////////////////////////////////////
/// @brief internal cursor to enumerate the paths of a graph
////////////////////////////////////////////////////////////////////////////////

          std::unique_ptr<PathEnumerator<EdgeInfo, VertexId>> _enumerator;

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to extract an edge
////////////////////////////////////////////////////////////////////////////////

          std::function<void(VertexId&, std::vector<EdgeInfo>&, void*&, size_t&, bool&)> _getEdge;

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to extract vertex information
////////////////////////////////////////////////////////////////////////////////

          std::function<VertexId (EdgeInfo&, VertexId&)> _getVertex;

////////////////////////////////////////////////////////////////////////////////
/// @brief a vector containing all required edge collection structures
////////////////////////////////////////////////////////////////////////////////

          std::vector<TRI_document_collection_t*> _edgeCols;

////////////////////////////////////////////////////////////////////////////////
/// @brief internal function to define the _getVertex and _getEdge functions
////////////////////////////////////////////////////////////////////////////////

          void _defInternalFunctions ();

        public:

          DepthFirstTraverser (
            TRI_document_collection_t* edgeCollection,
            TRI_edge_direction_e& direction,
            uint64_t minDepth,
            uint64_t maxDepth
          );

          DepthFirstTraverser (
            std::vector<TRI_document_collection_t*> edgeCollections,
            TraverserOptions& _opts
          );

////////////////////////////////////////////////////////////////////////////////
/// @brief Reset the traverser to use another start vertex
////////////////////////////////////////////////////////////////////////////////

          void setStartVertex (VertexId& v);

////////////////////////////////////////////////////////////////////////////////
/// @brief Skip amount many paths of the graph.
////////////////////////////////////////////////////////////////////////////////

          size_t skip (size_t amount);

////////////////////////////////////////////////////////////////////////////////
/// @brief Get the next possible path in the graph.
////////////////////////////////////////////////////////////////////////////////

          const TraversalPath<EdgeInfo, VertexId>&  next ();

////////////////////////////////////////////////////////////////////////////////
/// @brief Prune the current path prefix. Do not evaluate it any further.
////////////////////////////////////////////////////////////////////////////////

          void prune ();

////////////////////////////////////////////////////////////////////////////////
/// @brief Simple check if there potentially more paths.
///        It might return true although there are no more paths available.
///        If it returns false it is guaranteed that there are no more paths.
////////////////////////////////////////////////////////////////////////////////

          bool hasMore ();

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

    EdgeCollectionInfo (TRI_voc_cid_t& edgeCollectionCid,
                        TRI_document_collection_t* edgeCollection,
                        WeightCalculatorFunction weighter)
      : _edgeCollectionCid(edgeCollectionCid),
        _edgeCollection(edgeCollection),
        _weighter(weighter) {
    }

    EdgeId extractEdgeId (TRI_doc_mptr_copy_t& ptr) {
      return EdgeId(_edgeCollectionCid, TRI_EXTRACT_MARKER_KEY(&ptr));
    }

    std::vector<TRI_doc_mptr_copy_t> getEdges (TRI_edge_direction_e direction,
                                               VertexId const& vertexId) const {
      return TRI_LookupEdgesDocumentCollection(_edgeCollection,
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
    triagens::basics::traverser::ShortestPathOptions& opts
);


std::unique_ptr<ArangoDBConstDistancePathFinder::Path> TRI_RunSimpleShortestPathSearch (
    std::vector<EdgeCollectionInfo*>& collectionInfos,
    triagens::basics::traverser::ShortestPathOptions& opts
);

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the neighbors computation
////////////////////////////////////////////////////////////////////////////////

void TRI_RunNeighborsSearch (std::vector<EdgeCollectionInfo*>& collectionInfos,
                             triagens::basics::traverser::NeighborsOptions& opts,
                             std::unordered_set<VertexId>& distinct);

#endif
