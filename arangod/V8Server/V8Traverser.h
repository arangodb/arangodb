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
#include "Traverser.h"
#include "VocBase/edge-collection.h"
#include "VocBase/ExampleMatcher.h"

////////////////////////////////////////////////////////////////////////////////
/// @brief Template for a vertex id. Is simply a pair of cid and key
////////////////////////////////////////////////////////////////////////////////

struct VertexId {
  TRI_voc_cid_t cid;
  char const*   key;

  VertexId() : cid(0), key(nullptr) {
  }

  VertexId( TRI_voc_cid_t cid, char const* key) 
    : cid(cid), key(key) {
  }

  // Find unnecessary copies
  //   VertexId(const VertexId&) = delete;
  // VertexId(const VertexId& v) : first(v.first), second(v.second) { std::cout << "move failed!\n";}
  // VertexId(VertexId&& v) : first(v.first), second(std::move(v.second)) {}
};

// EdgeId and VertexId are similar here. both have a key and a cid
typedef VertexId EdgeId; 


////////////////////////////////////////////////////////////////////////////////
/// @brief typedef the template instanciation of the PathFinder
////////////////////////////////////////////////////////////////////////////////

typedef triagens::basics::PathFinder<VertexId, EdgeId, double> 
        ArangoDBPathFinder;



namespace triagens {
  namespace basics {
    namespace traverser {
      struct ShortestPathOptions {

        private: 
          std::unordered_map<TRI_voc_cid_t, triagens::arango::ExampleMatcher*> _vertexFilter;
          std::unordered_map<TRI_voc_cid_t, triagens::arango::ExampleMatcher*> _edgeFilter;

        public:
          std::string direction;
          bool useWeight;
          std::string weightAttribute;
          double defaultWeight;
          bool bidirectional;
          bool multiThreaded;
          bool useVertexFilter;
          bool useEdgeFilter;

          ShortestPathOptions() :
            direction("outbound"),
            useWeight(false),
            weightAttribute(""),
            defaultWeight(1),
            bidirectional(true),
            multiThreaded(true),
            useVertexFilter(false),
            useEdgeFilter(false) {
          }

          void addEdgeFilter(
            v8::Isolate* isolate,
            v8::Handle<v8::Object> const& example,
            TRI_shaper_t* shaper,
            TRI_voc_cid_t const& cid,
            std::string& errorMessage
          );
          bool matchesEdge(EdgeId& e, TRI_doc_mptr_copy_t* edge) const;

      };
      struct NeighborsOptions {
        std::string direction;
        bool distinct;

        NeighborsOptions() :
          direction("outbound"),
          distinct(false) {
        }

      };
 
    }
  }
}


    
////////////////////////////////////////////////////////////////////////////////
/// @brief callback to weight an edge
////////////////////////////////////////////////////////////////////////////////

typedef std::function<double(TRI_doc_mptr_copy_t& edge)> WeightCalculatorFunction;

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
/// @brief Wrapper for the shortest path computation
////////////////////////////////////////////////////////////////////////////////
std::unique_ptr<ArangoDBPathFinder::Path> TRI_RunShortestPathSearch (
    std::vector<EdgeCollectionInfo*>& collectionInfos,
    std::string const& startVertex,
    std::string const& targetVertex,
    triagens::arango::CollectionNameResolver const* resolver,
    triagens::basics::traverser::ShortestPathOptions& opts
);

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the neighbors computation
////////////////////////////////////////////////////////////////////////////////

std::vector<VertexId> TRI_RunNeighborsSearch (
  v8::Isolate* isolate,
  TRI_vocbase_t* vocbase,
  std::string const& vertexCollectionName,
  std::string const& edgeCollectionName,
  std::string const& startVertex,
  triagens::arango::CollectionNameResolver const* resolver,
  TRI_document_collection_t* ecol,
  triagens::basics::traverser::NeighborsOptions& opts
);

#endif
