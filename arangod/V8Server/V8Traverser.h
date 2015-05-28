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
#include "VocBase/edge-collection.h"
#include "VocBase/ExampleMatcher.h"
#include "Utils/ExplicitTransaction.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief Template for a vertex id. Is simply a pair of cid and key
////////////////////////////////////////////////////////////////////////////////

struct VertexId {
  TRI_voc_cid_t cid;
  char const*   key;

  VertexId () : cid(0), key(nullptr) {
  }

  VertexId (TRI_voc_cid_t cid, char const* key) 
    : cid(cid), key(key) {
  }
  
  bool operator== (const VertexId& other) const {
    if (cid == other.cid) {
      return strcmp(key, other.key) == 0;
    }
    return false;
  }

  // Find unnecessary copies
  //   VertexId(const VertexId&) = delete;
  // VertexId(const VertexId& v) : first(v.first), second(v.second) { std::cout << "move failed!\n";}
  // VertexId(VertexId&& v) : first(v.first), second(std::move(v.second)) {}
};

namespace std {
  template<>
  struct hash<VertexId> {
    public:
      size_t operator()(VertexId const& s) const {
        size_t h1 = std::hash<TRI_voc_cid_t>()(s.cid);
        size_t h2 = TRI_FnvHashString(s.key);
        return h1 ^ ( h2 << 1 );
      }
  };

  template<>
  struct equal_to<VertexId> {
    public:
      bool operator()(VertexId const& s, VertexId const& t) const {
        return s.cid == t.cid && strcmp(s.key, t.key) == 0;
      }
  };

  template<>
    struct less<VertexId> {
      public:
        bool operator()(const VertexId& lhs, const VertexId& rhs) {
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

  VertexFilterInfo (
    triagens::arango::ExplicitTransaction* trx,
    TRI_transaction_collection_t* col,
    triagens::arango::ExampleMatcher* matcher
  ) : trx(trx), col(col), matcher(matcher) {
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief typedef the template instanciation of the PathFinder
////////////////////////////////////////////////////////////////////////////////

typedef triagens::basics::PathFinder<VertexId, EdgeId, double> 
        ArangoDBPathFinder;



namespace triagens {
  namespace basics {
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

        public:
          VertexId start;
          bool useEdgeFilter;
          bool useVertexFilter;


          void addEdgeFilter (
            v8::Isolate* isolate,
            v8::Handle<v8::Value> const& example,
            TRI_shaper_t* shaper,
            TRI_voc_cid_t const& cid,
            std::string& errorMessage
          );

          void addVertexFilter (
            v8::Isolate* isolate,
            v8::Handle<v8::Value> const& example,
            triagens::arango::ExplicitTransaction* trx,
            TRI_transaction_collection_t* col,
            TRI_shaper_t* shaper,
            TRI_voc_cid_t const& cid,
            std::string& errorMessage
          );

          bool matchesEdge (EdgeId& e, TRI_doc_mptr_copy_t* edge) const;

          bool matchesVertex (VertexId& v) const;

      };
 
      struct NeighborsOptions : BasicOptions {

        private:
          std::unordered_set<TRI_voc_cid_t> _explicitCollections; 

        public:
          TRI_edge_direction_e direction;
          uint64_t minDepth;
          uint64_t maxDepth;

          NeighborsOptions () :
            direction(TRI_EDGE_OUT),
            minDepth(1),
            maxDepth(1) {
          }

          bool matchesVertex (VertexId& v) const;

          void addCollectionRestriction (TRI_voc_cid_t& cid);
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

          ShortestPathOptions() :
            direction("outbound"),
            useWeight(false),
            weightAttribute(""),
            defaultWeight(1),
            bidirectional(true),
            multiThreaded(true) {
          }

          
          bool matchesVertex (VertexId& v) const;

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

    EdgeCollectionInfo(
      TRI_voc_cid_t& edgeCollectionCid,
      TRI_document_collection_t* edgeCollection,
      WeightCalculatorFunction weighter
     ) : _edgeCollectionCid(edgeCollectionCid),
       _edgeCollection(edgeCollection),
       _weighter(weighter) {
    }

    EdgeId extractEdgeId(TRI_doc_mptr_copy_t& ptr) {
      return EdgeId(_edgeCollectionCid, TRI_EXTRACT_MARKER_KEY(&ptr));
    }

    std::vector<TRI_doc_mptr_copy_t> getEdges (TRI_edge_direction_e& direction,
        VertexId& vertexId) {
      return TRI_LookupEdgesDocumentCollection(_edgeCollection,
                   direction, vertexId.cid, const_cast<char*>(vertexId.key));
    }

    TRI_voc_cid_t getCid() {
      return _edgeCollectionCid;
    }

    TRI_shaper_t* getShaper() {
      return _edgeCollection->getShaper();
    }

    double weightEdge(TRI_doc_mptr_copy_t& ptr) {
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

    VertexCollectionInfo(
      TRI_voc_cid_t& vertexCollectionCid,
      TRI_transaction_collection_t* vertexCollection
     ) : _vertexCollectionCid(vertexCollectionCid),
       _vertexCollection(vertexCollection) {
    }

    TRI_voc_cid_t getCid() {
      return _vertexCollectionCid;
    }

    TRI_transaction_collection_t* getCollection() {
      return _vertexCollection;
    }

    TRI_shaper_t* getShaper() {
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

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the neighbors computation
////////////////////////////////////////////////////////////////////////////////

void TRI_RunNeighborsSearch (
  std::vector<EdgeCollectionInfo*>& collectionInfos,
  triagens::basics::traverser::NeighborsOptions& opts,
  std::unordered_set<VertexId>& distinct,
  std::vector<VertexId>& result
);

#endif
