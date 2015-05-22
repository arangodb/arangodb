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

#include "v8.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "V8Server/v8-wrapshapedjson.h"
#include "V8Server/v8-vocindex.h"
#include "V8Server/v8-collection.h"
#include "Utils/transactions.h"
#include "Utils/V8ResolverGuard.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/document-collection.h"
#include "V8Traverser.h"
#include "VocBase/key-generator.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::basics::traverser;
using namespace triagens::arango;

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
////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _from Id out of mptr, we return an RValue reference
/// to explicitly allow move semantics.
////////////////////////////////////////////////////////////////////////////////

static VertexId extractFromId(TRI_doc_mptr_copy_t& ptr) {
  return VertexId(TRI_EXTRACT_MARKER_FROM_CID(&ptr),
                  TRI_EXTRACT_MARKER_FROM_KEY(&ptr));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _to Id out of mptr, we return an RValue reference
/// to explicitly allow move semantics.
////////////////////////////////////////////////////////////////////////////////

static VertexId extractToId(TRI_doc_mptr_copy_t& ptr) {
  return VertexId(TRI_EXTRACT_MARKER_TO_CID(&ptr),
                  TRI_EXTRACT_MARKER_TO_KEY(&ptr));
};


////////////////////////////////////////////////////////////////////////////////
/// @brief Expander for Multiple edge collections
////////////////////////////////////////////////////////////////////////////////

class MultiCollectionEdgeExpander {

////////////////////////////////////////////////////////////////////////////////
/// @brief Edge direction for this expander
////////////////////////////////////////////////////////////////////////////////

    TRI_edge_direction_e _direction;

////////////////////////////////////////////////////////////////////////////////
/// @brief all info required for edge collection
////////////////////////////////////////////////////////////////////////////////

    vector<EdgeCollectionInfo*> _edgeCollections;

////////////////////////////////////////////////////////////////////////////////
/// @brief function to check if the edge passes the filter
////////////////////////////////////////////////////////////////////////////////

    function<bool (EdgeId&, TRI_doc_mptr_copy_t*)> _isAllowed;

////////////////////////////////////////////////////////////////////////////////
/// @brief function to check if the vertex passes the filter
////////////////////////////////////////////////////////////////////////////////

    function<bool (VertexId&)> _isAllowedVertex;

  public: 

    MultiCollectionEdgeExpander(TRI_edge_direction_e const& direction,
                       vector<EdgeCollectionInfo*> const& edgeCollections,
                       function<bool (EdgeId&, TRI_doc_mptr_copy_t*)> isAllowed,
                       function<bool (VertexId&)> isAllowedVertex
                     )
      : _direction(direction),
        _edgeCollections(edgeCollections),
        _isAllowed(isAllowed),
        _isAllowedVertex(isAllowedVertex) {
      }


    void operator() (VertexId& source,
                     vector<ArangoDBPathFinder::Step*>& result) {
      TransactionBase fake(true); // Fake a transaction to please checks. 
                                  // This is due to multi-threading

      equal_to<VertexId> eq;
      for (auto edgeCollection : _edgeCollections) { 
        auto edges = edgeCollection->getEdges(_direction, source); 

        unordered_map<VertexId, size_t> candidates;
        for (size_t j = 0;  j < edges.size(); ++j) {
          EdgeId edgeId = edgeCollection->extractEdgeId(edges[j]);
          if (!_isAllowed(edgeId, &edges[j])) {
            continue;
          }
          VertexId from = extractFromId(edges[j]);
          VertexId to = extractToId(edges[j]);
          double currentWeight = edgeCollection->weightEdge(edges[j]);
          auto inserter = [&](VertexId& s, VertexId& t) {
            if (_isAllowedVertex(t)) {
              auto cand = candidates.find(t);
              if (cand == candidates.end()) {
                // Add weight
                result.push_back(new ArangoDBPathFinder::Step(t, s, currentWeight,
                                 edgeId));
                candidates.emplace(t, result.size() - 1);
              } else {
                // Compare weight
                auto oldWeight = result[cand->second]->weight();
                if (currentWeight < oldWeight) {
                  result[cand->second]->setWeight(currentWeight);
                }
              }
            }
          };
          if (!eq(from, source)) {
            inserter(to, from);
          } 
          else if (!eq(to, source)) {
            inserter(from, to);
          }
        }
      }
    } 
};

class SimpleEdgeExpander {

////////////////////////////////////////////////////////////////////////////////
/// @brief The direction used for edges in this expander
////////////////////////////////////////////////////////////////////////////////

    TRI_edge_direction_e _direction;

////////////////////////////////////////////////////////////////////////////////
/// @brief all info required for edge collection
////////////////////////////////////////////////////////////////////////////////

    EdgeCollectionInfo* _edgeCollection;

  public: 

    SimpleEdgeExpander(TRI_edge_direction_e& direction,
                       EdgeCollectionInfo* edgeCollection)
      : _direction(direction),
        _edgeCollection(edgeCollection) {
    };

    void operator() (VertexId& source,
                     vector<ArangoDBPathFinder::Step*>& result) {
      TransactionBase fake(true); // Fake a transaction to please checks. 
                                  // This is due to multi-threading
      auto edges = _edgeCollection->getEdges(_direction, source); 

      equal_to<VertexId> eq;
      unordered_map<VertexId, size_t> candidates;
      for (size_t j = 0;  j < edges.size(); ++j) {
        VertexId from = extractFromId(edges[j]);
        VertexId to = extractToId(edges[j]);
        double currentWeight = _edgeCollection->weightEdge(edges[j]);
        auto inserter = [&](VertexId& s, VertexId& t) {
          auto cand = candidates.find(t);
          if (cand == candidates.end()) {
            // Add weight
            result.push_back(new ArangoDBPathFinder::Step(t, s, currentWeight, 
                             _edgeCollection->extractEdgeId(edges[j])));
            candidates.emplace(t, result.size() - 1);
          } else {
            // Compare weight
            auto oldWeight = result[cand->second]->weight();
            if (currentWeight < oldWeight) {
              result[cand->second]->setWeight(currentWeight);
            }
          }
        };
        if (!eq(from, source)) {
          inserter(to, from);
        } 
        else if (!eq(to, source)) {
          inserter(from, to);
        }
      }
    } 
};

// -----------------------------------------------------------------------------
// --SECTION--                                     ShortestPathOptions FUNCTIONS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if an edge matches to given examples
////////////////////////////////////////////////////////////////////////////////

bool ShortestPathOptions::matchesEdge(EdgeId& e, TRI_doc_mptr_copy_t* edge) const {
  if (!useEdgeFilter) {
    // Nothing to do
    return true;
  }
  auto it = _edgeFilter.find(e.cid);
  if (it == _edgeFilter.end()) {
    // This collection does not have any object of this shape.
    // Short circuit.
    return false;
  }
  return it->second->matches(edge);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Insert a new edge matcher object
////////////////////////////////////////////////////////////////////////////////

void ShortestPathOptions::addEdgeFilter( 
          v8::Isolate* isolate,
          v8::Handle<v8::Object> const& example,
          TRI_shaper_t* shaper,
          TRI_voc_cid_t const& cid,
          string& errorMessage) {
  auto it = _edgeFilter.find(cid);
  if (it == _edgeFilter.end()) {
    _edgeFilter.emplace(cid, new ExampleMatcher(isolate, example, shaper, errorMessage));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Insert a new vertex matcher object
////////////////////////////////////////////////////////////////////////////////

void ShortestPathOptions::addVertexFilter( 
          v8::Isolate* isolate,
          v8::Handle<v8::Object> const& example,
          ExplicitTransaction* trx,
          TRI_transaction_collection_t* col,
          TRI_shaper_t* shaper,
          TRI_voc_cid_t const& cid,
          string& errorMessage) {
  auto it = _vertexFilter.find(cid);
  if (it == _vertexFilter.end()) {
    _vertexFilter.emplace(cid, VertexFilterInfo(
      trx,
      col,
      new ExampleMatcher(isolate, example, shaper, errorMessage)
    ));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if a vertex matches to given examples
////////////////////////////////////////////////////////////////////////////////

bool ShortestPathOptions::matchesVertex(VertexId& v) const {
  if (!useVertexFilter) {
    // Nothing to do
    return true;
  }
  if (start == v || end == v) {
    return true;
  }

  auto it = _vertexFilter.find(v.cid);
  if (it == _vertexFilter.end()) {
    // This collection does not have any object of this shape.
    // Short circuit.
    return false;
  }

  TRI_doc_mptr_copy_t vertex;
  int res = it->second.trx->readSingle(it->second.col, &vertex, v.key);
  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }
  return it->second.matcher->matches(&vertex);
}

// -----------------------------------------------------------------------------
// --SECTION--                                          private Helper Functions
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the shortest path computation
////////////////////////////////////////////////////////////////////////////////

unique_ptr<ArangoDBPathFinder::Path> TRI_RunShortestPathSearch (
    vector<EdgeCollectionInfo*>& collectionInfos,
    ShortestPathOptions& opts) {

  TRI_edge_direction_e forward;
  TRI_edge_direction_e backward;
  if (opts.direction == "outbound") {
    forward = TRI_EDGE_OUT;
    backward = TRI_EDGE_IN;
  } else if (opts.direction == "inbound") {
    forward = TRI_EDGE_IN;
    backward = TRI_EDGE_OUT;
  } else {
    forward = TRI_EDGE_ANY;
    backward = TRI_EDGE_ANY;
  }

  auto edgeFilterClosure = [&opts](EdgeId& e, TRI_doc_mptr_copy_t* edge) -> bool {return opts.matchesEdge(e, edge);};

  auto vertexFilterClosure = [&opts](VertexId& v) -> bool { return opts.matchesVertex(v); };

  MultiCollectionEdgeExpander forwardExpander(forward, collectionInfos, edgeFilterClosure, vertexFilterClosure);
  MultiCollectionEdgeExpander backwardExpander(backward, collectionInfos, edgeFilterClosure, vertexFilterClosure);

  ArangoDBPathFinder pathFinder(forwardExpander, 
                              backwardExpander,
                              opts.bidirectional);
  unique_ptr<ArangoDBPathFinder::Path> path;
  if (opts.multiThreaded) {
    path.reset(pathFinder.shortestPathTwoThreads(opts.start, opts.end));
  }
  else {
    path.reset(pathFinder.shortestPath(opts.start, opts.end));
  } 
  return path;
}

vector<VertexId> TRI_RunNeighborsSearch (
  vector<EdgeCollectionInfo*>& collectionInfos,
  NeighborsOptions& opts
) {
  vector<VertexId> result;

  if (opts.distinct) {
    unordered_set<VertexId> distinct;
    if (opts.direction == TRI_EDGE_IN || opts.direction == TRI_EDGE_ANY) {
      TRI_edge_direction_e dir = TRI_EDGE_IN;
      for (auto col : collectionInfos) {
        auto edges = col->getEdges(dir, opts.start);
        for (size_t j = 0;  j < edges.size(); ++j) {
          distinct.insert(extractFromId(edges[j]));
        }
      }
    }
    if (opts.direction == TRI_EDGE_OUT || opts.direction == TRI_EDGE_ANY) {
      TRI_edge_direction_e dir = TRI_EDGE_OUT;
      for (auto col : collectionInfos) {
        auto edges = col->getEdges(dir, opts.start);
        for (size_t j = 0;  j < edges.size(); ++j) {
          distinct.insert(extractToId(edges[j]));
        }
      }
    }
    copy(distinct.begin(), distinct.end(), back_inserter(result));
  } else {
    if (opts.direction == TRI_EDGE_IN || opts.direction == TRI_EDGE_ANY) {
      TRI_edge_direction_e dir = TRI_EDGE_IN;
      for (auto col : collectionInfos) {
        auto edges = col->getEdges(dir, opts.start);
        for (size_t j = 0;  j < edges.size(); ++j) {
          result.push_back(extractFromId(edges[j]));
        }
      }
    }
    if (opts.direction == TRI_EDGE_OUT || opts.direction == TRI_EDGE_ANY) {
      TRI_edge_direction_e dir = TRI_EDGE_OUT;
      for (auto col : collectionInfos) {
        auto edges = col->getEdges(dir, opts.start);
        for (size_t j = 0;  j < edges.size(); ++j) {
          result.push_back(extractToId(edges[j]));
        }
      }
    }
  }
  return result;
};
