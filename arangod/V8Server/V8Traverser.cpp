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

#include "V8Traverser.h"
#include "Indexes/EdgeIndex.h"
#include "Utils/transactions.h"
#include "Utils/V8ResolverGuard.h"
#include "Utils/CollectionNameResolver.h"
#include "V8/v8-conv.h"
#include "V8/v8-utils.h"
#include "V8Server/v8-vocbaseprivate.h"
#include "V8Server/v8-wrapshapedjson.h"
#include "V8Server/v8-vocindex.h"
#include "V8Server/v8-collection.h"
#include "VocBase/document-collection.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/VocShaper.h"

#include <v8.h>

using namespace std;
using namespace triagens::basics;
using namespace triagens::basics::traverser;
using namespace triagens::arango;

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _from Id out of mptr, we return an RValue reference
/// to explicitly allow move semantics.
////////////////////////////////////////////////////////////////////////////////

static VertexId ExtractFromId (TRI_doc_mptr_copy_t const& ptr) {
  return VertexId(TRI_EXTRACT_MARKER_FROM_CID(&ptr),
                  TRI_EXTRACT_MARKER_FROM_KEY(&ptr));
};

////////////////////////////////////////////////////////////////////////////////
/// @brief extract the _to Id out of mptr, we return an RValue reference
/// to explicitly allow move semantics.
////////////////////////////////////////////////////////////////////////////////

static VertexId ExtractToId (TRI_doc_mptr_copy_t const& ptr) {
  return VertexId(TRI_EXTRACT_MARKER_TO_CID(&ptr),
                  TRI_EXTRACT_MARKER_TO_KEY(&ptr));
};


////////////////////////////////////////////////////////////////////////////////
/// @brief Helper to transform a vertex _id string to VertexId struct.
/// NOTE:  Make sure the given string is not freed as long as the resulting
///        VertexId is in use
////////////////////////////////////////////////////////////////////////////////

VertexId triagens::basics::traverser::IdStringToVertexId (
    CollectionNameResolver const* resolver,
    string const& vertex
  ) {
  size_t split;
  char const* str = vertex.c_str();

  if (! TRI_ValidateDocumentIdKeyGenerator(str, &split)) {
    throw TRI_ERROR_ARANGO_INVALID_KEY_GENERATOR;
  }

  string const collectionName = vertex.substr(0, split);
  auto coli = resolver->getCollectionStruct(collectionName);

  if (coli == nullptr) {
    throw TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND;
  }
  return VertexId(coli->_cid, const_cast<char *>(str + split + 1));
}


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
      for (auto const& edgeCollection : _edgeCollections) { 
        auto edges = edgeCollection->getEdges(_direction, source); 

        unordered_map<VertexId, size_t> candidates;
        for (size_t j = 0;  j < edges.size(); ++j) {
          EdgeId edgeId = edgeCollection->extractEdgeId(edges[j]);
          if (! _isAllowed(edgeId, &edges[j])) {
            continue;
          }
          VertexId from = ExtractFromId(edges[j]);
          VertexId to = ExtractToId(edges[j]);
          double currentWeight = edgeCollection->weightEdge(edges[j]);
          auto inserter = [&](VertexId& s, VertexId& t) {
            if (_isAllowedVertex(t)) {
              auto cand = candidates.find(t);
              if (cand == candidates.end()) {
                // Add weight
                result.emplace_back(new ArangoDBPathFinder::Step(t, s, currentWeight,
                                    edgeId));
                candidates.emplace(t, result.size() - 1);
              } 
              else {
                // Compare weight
                auto oldWeight = result[cand->second]->weight();
                if (currentWeight < oldWeight) {
                  result[cand->second]->setWeight(currentWeight);
                }
              }
            }
          };
          if (! eq(from, source)) {
            inserter(to, from);
          } 
          else if (! eq(to, source)) {
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
        VertexId from = ExtractFromId(edges[j]);
        VertexId to = ExtractToId(edges[j]);
        double currentWeight = _edgeCollection->weightEdge(edges[j]);
        auto inserter = [&](VertexId& s, VertexId& t) {
          auto cand = candidates.find(t);
          if (cand == candidates.end()) {
            // Add weight
            result.emplace_back(new ArangoDBPathFinder::Step(t, s, currentWeight, 
                                _edgeCollection->extractEdgeId(edges[j])));
            candidates.emplace(t, result.size() - 1);
          } 
          else {
            // Compare weight
            auto oldWeight = result[cand->second]->weight();
            if (currentWeight < oldWeight) {
              result[cand->second]->setWeight(currentWeight);
            }
          }
        };
        if (! eq(from, source)) {
          inserter(to, from);
        } 
        else if (! eq(to, source)) {
          inserter(from, to);
        }
      }
    } 
};

// -----------------------------------------------------------------------------
// --SECTION--                                            BasicOptions FUNCTIONS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Insert a new vertex matcher object
////////////////////////////////////////////////////////////////////////////////

void BasicOptions::addVertexFilter (v8::Isolate* isolate,
                                    v8::Handle<v8::Value> const& example,
                                    ExplicitTransaction* trx,
                                    TRI_transaction_collection_t* col,
                                    VocShaper* shaper,
                                    TRI_voc_cid_t const& cid,
                                    string& errorMessage) {

  auto it = _vertexFilter.find(cid);

  if (example->IsArray()) {
    if (it == _vertexFilter.end()) {
      _vertexFilter.emplace(cid, VertexFilterInfo(
        trx,
        col,
        new ExampleMatcher(isolate, v8::Handle<v8::Array>::Cast(example), shaper, errorMessage)
      ));
    }
  } 
  else {
    // Has to be Object
    if (it == _vertexFilter.end()) {
      _vertexFilter.emplace(cid, VertexFilterInfo(
        trx,
        col,
        new ExampleMatcher(isolate, v8::Handle<v8::Array>::Cast(example), shaper, errorMessage)
      ));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if a vertex matches to given examples
////////////////////////////////////////////////////////////////////////////////

bool BasicOptions::matchesVertex (VertexId const& v) const {
  if (! useVertexFilter) {
    // Nothing to do
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

  return it->second.matcher->matches(v.cid, &vertex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Insert a new edge matcher object
////////////////////////////////////////////////////////////////////////////////

void BasicOptions::addEdgeFilter (v8::Isolate* isolate,
                                  v8::Handle<v8::Value> const& example,
                                  VocShaper* shaper,
                                  TRI_voc_cid_t const& cid,
                                  string& errorMessage) {
  useEdgeFilter = true;
  auto it = _edgeFilter.find(cid);

  if (example->IsArray()) {
    if (it == _edgeFilter.end()) {
      _edgeFilter.emplace(cid, new ExampleMatcher(isolate, v8::Handle<v8::Array>::Cast(example), shaper, errorMessage));
    }
  } 
  else {
    // Has to be Object
    if (it == _edgeFilter.end()) {
      _edgeFilter.emplace(cid, new ExampleMatcher(isolate, v8::Handle<v8::Object>::Cast(example), shaper, errorMessage));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Insert a new edge matcher object
////////////////////////////////////////////////////////////////////////////////

void BasicOptions::addEdgeFilter (Json const& example,
                                  VocShaper* shaper,
                                  TRI_voc_cid_t const& cid,
                                  CollectionNameResolver const* resolver) {
  useEdgeFilter = true;
  auto it = _edgeFilter.find(cid);
  if (it == _edgeFilter.end()) {
    _edgeFilter.emplace(cid, new ExampleMatcher(example.json(), shaper, resolver));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if an edge matches to given examples
////////////////////////////////////////////////////////////////////////////////

bool BasicOptions::matchesEdge (EdgeId& e, TRI_doc_mptr_copy_t* edge) const {
  if (! useEdgeFilter) {
    // Nothing to do
    return true;
  }

  auto it = _edgeFilter.find(e.cid);

  if (it == _edgeFilter.end()) {
    // This collection does not have any object of this shape.
    // Short circuit.
    return false;
  }

  return it->second->matches(e.cid, edge);
}

// -----------------------------------------------------------------------------
// --SECTION--                                     ShortestPathOptions FUNCTIONS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if a vertex matches to given examples
////////////////////////////////////////////////////////////////////////////////

bool ShortestPathOptions::matchesVertex (VertexId const& v) const {
  if (start == v || end == v) {
    return true;
  }

  return BasicOptions::matchesVertex(v);
}

// -----------------------------------------------------------------------------
// --SECTION--                                        NeighborsOptions FUNCTIONS
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if a vertex matches to given examples
////////////////////////////////////////////////////////////////////////////////

bool NeighborsOptions::matchesVertex (VertexId const& v) const {
  // If there are explicitly marked collections check them.
  if (! _explicitCollections.empty()) {
    // If the current collection is not stored the result is invalid
    if (_explicitCollections.find(v.cid) == _explicitCollections.end()) {
      return false;
    }
  }
  return BasicOptions::matchesVertex(v);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Inserts one explicitly allowed collection. As soon as one is explicitly
/// allowed all others are implicitly disallowed. If there is no explicitly allowed
/// collection all are implicitly allowed.
////////////////////////////////////////////////////////////////////////////////

void NeighborsOptions::addCollectionRestriction (TRI_voc_cid_t cid) {
  _explicitCollections.emplace(cid);
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
  } 
  else if (opts.direction == "inbound") {
    forward = TRI_EDGE_IN;
    backward = TRI_EDGE_OUT;
  } 
  else {
    forward = TRI_EDGE_ANY;
    backward = TRI_EDGE_ANY;
  }

  auto edgeFilterClosure = [&opts] (EdgeId& e, TRI_doc_mptr_copy_t* edge) -> bool { 
    return opts.matchesEdge(e, edge); 
  };

  auto vertexFilterClosure = [&opts] (VertexId& v) -> bool { 
    return opts.matchesVertex(v); 
  };

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

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the shortest path computation
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ArangoDBConstDistancePathFinder::Path> TRI_RunSimpleShortestPathSearch ( 
    vector<EdgeCollectionInfo*>& collectionInfos,
    ShortestPathOptions& opts) {

  TRI_edge_direction_e forward;
  TRI_edge_direction_e backward;

  if (opts.direction == "outbound") {
    forward = TRI_EDGE_OUT;
    backward = TRI_EDGE_IN;
  } 
  else if (opts.direction == "inbound") {
    forward = TRI_EDGE_IN;
    backward = TRI_EDGE_OUT;
  } 
  else {
    forward = TRI_EDGE_ANY;
    backward = TRI_EDGE_ANY;
  }

  auto fwExpander = [&collectionInfos, forward] (VertexId& v, vector<EdgeId>& res_edges, vector<VertexId>& neighbors) {
    equal_to<VertexId> eq;
    for (auto const& edgeCollection : collectionInfos) { 
      auto edges = edgeCollection->getEdges(forward, v); 
      for (size_t j = 0;  j < edges.size(); ++j) {
        EdgeId edgeId = edgeCollection->extractEdgeId(edges[j]);
        VertexId from = ExtractFromId(edges[j]);
        if (! eq(from, v)) {
          res_edges.emplace_back(edgeId);
          neighbors.emplace_back(from);
        } else {
          VertexId to = ExtractToId(edges[j]);
          if (! eq(to, v)) {
            res_edges.emplace_back(edgeId);
            neighbors.emplace_back(to);
          }
        }
      }
    }
  };
  auto bwExpander = [&collectionInfos, backward] (VertexId& v, vector<EdgeId>& res_edges, vector<VertexId>& neighbors) {
    equal_to<VertexId> eq;
    for (auto const& edgeCollection : collectionInfos) { 
      auto edges = edgeCollection->getEdges(backward, v); 
      for (size_t j = 0;  j < edges.size(); ++j) {
        EdgeId edgeId = edgeCollection->extractEdgeId(edges[j]);
        VertexId from = ExtractFromId(edges[j]);
        if (! eq(from, v)) {
          res_edges.emplace_back(edgeId);
          neighbors.emplace_back(from);
        } else {
          VertexId to = ExtractToId(edges[j]);
          if (! eq(to, v)) {
            res_edges.emplace_back(edgeId);
            neighbors.emplace_back(to);
          }
        }
      }
    }
  };

  ArangoDBConstDistancePathFinder pathFinder(fwExpander, bwExpander);
  std::unique_ptr<ArangoDBConstDistancePathFinder::Path> path;
  path.reset(pathFinder.search(opts.start, opts.end));
  return path;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief search for distinct inbound neighbors
////////////////////////////////////////////////////////////////////////////////

static void InboundNeighbors (vector<EdgeCollectionInfo*>& collectionInfos,
                              NeighborsOptions& opts,
                              unordered_set<VertexId>& startVertices,
                              unordered_set<VertexId>& visited,
                              unordered_set<VertexId>& distinct,
                              uint64_t depth = 1) {

  TRI_edge_direction_e dir = TRI_EDGE_IN;
  unordered_set<VertexId> nextDepth;

  for (auto const& col : collectionInfos) {
    for (VertexId const& start : startVertices) {
      auto edges = col->getEdges(dir, start);
      for (size_t j = 0;  j < edges.size(); ++j) {
        EdgeId edgeId = col->extractEdgeId(edges[j]);

        if (opts.matchesEdge(edgeId, &edges[j])) {
          VertexId v = ExtractFromId(edges[j]);
          if (visited.find(v) != visited.end()) {
            // We have already visited this vertex
            continue;
          }
          visited.emplace(v);
          if (depth >= opts.minDepth) {
            if (opts.matchesVertex(v)) {
              distinct.emplace(v);
            }
          }
          if (depth < opts.maxDepth) {
            nextDepth.emplace(v);
          }
        }
      }
    }
  }

  if (! nextDepth.empty()) {
    InboundNeighbors(collectionInfos, opts, nextDepth, visited, distinct, depth + 1);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief search for distinct outbound neighbors
////////////////////////////////////////////////////////////////////////////////

static void OutboundNeighbors (vector<EdgeCollectionInfo*>& collectionInfos,
                               NeighborsOptions& opts,
                               unordered_set<VertexId>& startVertices,
                               unordered_set<VertexId>& visited,
                               unordered_set<VertexId>& distinct,
                               uint64_t depth = 1) {

  TRI_edge_direction_e dir = TRI_EDGE_OUT;
  unordered_set<VertexId> nextDepth;

  for (auto const& col : collectionInfos) {
    for (VertexId const& start : startVertices) {
      auto edges = col->getEdges(dir, start);

      for (size_t j = 0;  j < edges.size(); ++j) {
        EdgeId edgeId = col->extractEdgeId(edges[j]);
        if (opts.matchesEdge(edgeId, &edges[j])) {
          VertexId v = ExtractToId(edges[j]);
          if (visited.find(v) != visited.end()) {
            // We have already visited this vertex
            continue;
          }
          visited.emplace(v);
          if (depth >= opts.minDepth) {
            if (opts.matchesVertex(v)) {
              distinct.emplace(v);
            }
          }
          if (depth < opts.maxDepth) {
            nextDepth.emplace(v);
          }
        }
      }
    }
  }
  if (! nextDepth.empty()) {
    OutboundNeighbors(collectionInfos, opts, nextDepth, visited, distinct, depth + 1);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief search for distinct in- and outbound neighbors
////////////////////////////////////////////////////////////////////////////////

static void AnyNeighbors (vector<EdgeCollectionInfo*>& collectionInfos,
                          NeighborsOptions& opts,
                          unordered_set<VertexId>& startVertices,
                          unordered_set<VertexId>& visited,
                          unordered_set<VertexId>& distinct,
                          uint64_t depth = 1) {

  TRI_edge_direction_e dir = TRI_EDGE_OUT;
  unordered_set<VertexId> nextDepth;

  for (auto const& col : collectionInfos) {
    for (VertexId const& start : startVertices) {
      dir = TRI_EDGE_OUT;
      auto edges = col->getEdges(dir, start);

      for (size_t j = 0;  j < edges.size(); ++j) {
        EdgeId edgeId = col->extractEdgeId(edges[j]);
        if (opts.matchesEdge(edgeId, &edges[j])) {
          VertexId v = ExtractToId(edges[j]);
          if (visited.find(v) != visited.end()) {
            // We have already visited this vertex
            continue;
          }
          visited.emplace(v);
          if (depth >= opts.minDepth) {
            if (opts.matchesVertex(v)) {
              distinct.emplace(v);
            }
          }
          if (depth < opts.maxDepth) {
            nextDepth.emplace(v);
          }
        }
      }
      dir = TRI_EDGE_IN;
      edges = col->getEdges(dir, start);
      for (size_t j = 0;  j < edges.size(); ++j) {
        EdgeId edgeId = col->extractEdgeId(edges[j]);
        if (opts.matchesEdge(edgeId, &edges[j])) {
          VertexId v = ExtractFromId(edges[j]);
          if (visited.find(v) != visited.end()) {
            // We have already visited this vertex
            continue;
          }
          visited.emplace(v);
          if (depth >= opts.minDepth) {
            if (opts.matchesVertex(v)) {
              distinct.emplace(v);
            }
          }
          if (depth < opts.maxDepth) {
            nextDepth.emplace(v);
          }
        }
      }
    }
  }
  if (! nextDepth.empty()) {
    AnyNeighbors(collectionInfos, opts, nextDepth, visited, distinct, depth + 1);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Execute a search for neighboring vertices
////////////////////////////////////////////////////////////////////////////////

void TRI_RunNeighborsSearch (
    vector<EdgeCollectionInfo*>& collectionInfos,
    NeighborsOptions& opts,
    unordered_set<VertexId>& result) {
  unordered_set<VertexId> startVertices;
  unordered_set<VertexId> visited;
  startVertices.emplace(opts.start);
  visited.emplace(opts.start);

  switch (opts.direction) {
    case TRI_EDGE_IN:
      InboundNeighbors(collectionInfos, opts, startVertices, visited, result);
      break;
    case TRI_EDGE_OUT:
      OutboundNeighbors(collectionInfos, opts, startVertices, visited, result);
      break;
    case TRI_EDGE_ANY:
      AnyNeighbors(collectionInfos, opts, startVertices, visited, result);
      break;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                         class DepthFirstTraverser
// -----------------------------------------------------------------------------

DepthFirstTraverser::DepthFirstTraverser (
    TRI_document_collection_t* edgeCollection,
    TRI_edge_direction_e& direction,
    uint64_t minDepth,
    uint64_t maxDepth
  ) : _done(false),
      _pruneNext(false)
       {
  _opts.minDepth = minDepth;
  _opts.maxDepth = maxDepth;
  _opts.direction = direction;
  _defInternalFunctions();
}

DepthFirstTraverser::DepthFirstTraverser (
  std::vector<TRI_document_collection_t*> edgeCollections,
  TraverserOptions& opts
) : _done(false),
    _opts(opts),
    _pruneNext(false),
    _edgeCols(edgeCollections) {
  _defInternalFunctions();
}

void DepthFirstTraverser::_defInternalFunctions () {
  _getVertex = [] (EdgeInfo& edge, VertexId& vertex) -> VertexId {
    auto mptr = edge.mptr;
    // TODO fill Statistics
    if (strcmp(TRI_EXTRACT_MARKER_FROM_KEY(&mptr), vertex.key) == 0 &&
        TRI_EXTRACT_MARKER_FROM_CID(&mptr) == vertex.cid) {
      return VertexId(TRI_EXTRACT_MARKER_TO_CID(&mptr), TRI_EXTRACT_MARKER_TO_KEY(&mptr));
    }
    return VertexId(TRI_EXTRACT_MARKER_FROM_CID(&mptr), TRI_EXTRACT_MARKER_FROM_KEY(&mptr));
  };

  auto direction = _opts.direction;
  auto edgeCols = _edgeCols;
  if (direction == TRI_EDGE_ANY) {
    _getEdge = [edgeCols] (VertexId& startVertex, std::vector<EdgeInfo>& edges, void*& last, size_t& eColIdx, bool& dir) {
      std::vector<TRI_doc_mptr_copy_t> tmp;
      TRI_ASSERT(eColIdx < edgeCols.size());
      // TODO fill Statistics
      // TODO Self referencing edges
      triagens::arango::EdgeIndex* edgeIndex = edgeCols.at(eColIdx)->edgeIndex();
      if (dir) {
        TRI_edge_index_iterator_t it(TRI_EDGE_OUT, startVertex.cid, startVertex.key);
        edgeIndex->lookup(&it, tmp, last, 1);
        while (last == nullptr) {
          // Switch back direction
          dir = false;
          ++eColIdx;
          if (edgeCols.size() == eColIdx) {
            // No more to do, stop here
            return;
          }
          TRI_edge_index_iterator_t it(TRI_EDGE_IN, startVertex.cid, startVertex.key);
          edgeIndex = edgeCols.at(eColIdx)->edgeIndex();
          edgeIndex->lookup(&it, tmp, last, 1);
          if (last == nullptr) {
            dir = true;
            TRI_edge_index_iterator_t it(TRI_EDGE_OUT, startVertex.cid, startVertex.key);
            edgeIndex->lookup(&it, tmp, last, 1);
          }
        }
      } else {
        TRI_edge_index_iterator_t it(TRI_EDGE_IN, startVertex.cid, startVertex.key);
        edgeIndex->lookup(&it, tmp, last, 1);
        while (last == nullptr) {
          // now change direction
          dir = true;
          TRI_edge_index_iterator_t it(TRI_EDGE_OUT, startVertex.cid, startVertex.key);
          edgeIndex->lookup(&it, tmp, last, 1);
          if (last == nullptr) {
            // The other direction also has no further edges
            dir = false;
            ++eColIdx;
            if (edgeCols.size() == eColIdx) {
              // No more to do, stop here
              return;
            }
            TRI_edge_index_iterator_t it(TRI_EDGE_IN, startVertex.cid, startVertex.key);
            edgeIndex = edgeCols.at(eColIdx)->edgeIndex();
            edgeIndex->lookup(&it, tmp, last, 1);
          }
        }
      }
      if (last != nullptr) {
        // sth is stored in tmp. Now push it on edges
        TRI_ASSERT(tmp.size() == 1);
        EdgeInfo e(
          edgeCols.at(eColIdx)->_info._cid,
          tmp.back()
        );
        edges.push_back(e);
      }
    };
  } else {
    _getEdge = [edgeCols, direction] (VertexId& startVertex, std::vector<EdgeInfo>& edges, void*& last, size_t& eColIdx, bool&) {
      std::vector<TRI_doc_mptr_copy_t> tmp;
      TRI_ASSERT(eColIdx < edgeCols.size());
      // Do not touch the bool parameter, as long as it is default the first encountered nullptr is final
      // TODO fill Statistics
      TRI_edge_index_iterator_t it(direction, startVertex.cid, startVertex.key);
      triagens::arango::EdgeIndex* edgeIndex = edgeCols.at(eColIdx)->edgeIndex();
      edgeIndex->lookup(&it, tmp, last, 1);
      while (last == nullptr) {
        // This edge collection does not have any more edges for this vertex. Check the next one
        ++eColIdx;
        if (edgeCols.size() == eColIdx) {
          // No more to do, stop here
          return;
        }
        edgeIndex = edgeCols.at(eColIdx)->edgeIndex();
        edgeIndex->lookup(&it, tmp, last, 1);
      }
      if (last != nullptr) {
        // sth is stored in tmp. Now push it on edges
        TRI_ASSERT(tmp.size() == 1);
        edges.push_back(EdgeInfo(
          edgeCols.at(eColIdx)->_info._cid,
          tmp.back()
        ));
      }
    };
  }
}

void DepthFirstTraverser::setStartVertex (VertexId& v) {
  _enumerator.reset(new PathEnumerator<EdgeInfo, VertexId>(_getEdge, _getVertex, v));
  _done = false;
}

size_t DepthFirstTraverser::skip (size_t amount) {
  size_t skipped = 0;
  TraversalPath<EdgeInfo, VertexId> p;
  for (size_t i = 0; i < amount; ++i) {
    p = next();
    if (p.edges.size() == 0) {
      break;
    }
    ++skipped;
  }
  return skipped;
}

bool DepthFirstTraverser::hasMore () {
  return !_done;
}

const TraversalPath<EdgeInfo, VertexId>& DepthFirstTraverser::next () {
  TRI_ASSERT(!_done);
  if (_pruneNext) {
    _pruneNext = false;
    _enumerator->prune();
  }
  TRI_ASSERT(!_pruneNext);
  const TraversalPath<EdgeInfo, VertexId>& p = _enumerator->next();
  size_t countEdges = p.edges.size();
  if (countEdges == 0) {
    _done = true;
    // Done traversing
    return p;
  }
  if (_opts.shouldPrunePath(p)) {
    _enumerator->prune();
    return next();
  }
  if (countEdges >= _opts.maxDepth) {
    _pruneNext = true;
  }
  if (countEdges < _opts.minDepth) {
    return next();
  }
  return p;
}

void DepthFirstTraverser::prune () {
  _pruneNext = true;
}
