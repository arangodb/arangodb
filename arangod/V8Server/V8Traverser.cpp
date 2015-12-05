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
#include "Utils/ShapedJsonTransformer.h"
#include "Utils/SingleCollectionReadOnlyTransaction.h"
#include "VocBase/document-collection.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/VocShaper.h"

using namespace std;
using namespace triagens::basics;
using namespace triagens::arango;
using namespace triagens::arango::traverser;

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
// --SECTION--                                   class SingleServerTraversalPath
// -----------------------------------------------------------------------------

Json* SingleServerTraversalPath::pathToJson (Transaction* trx,
                                             CollectionNameResolver* resolver) {
  std::unique_ptr<Json> path(new Json(Json::Object, 2));
  Json vertices(Json::Array);
  for (size_t i = 0; i < _path.vertices.size(); ++i) {
    auto v = vertexToJson(trx, resolver, _path.vertices[i]);
    try {
      vertices(*v);
      delete v;
    }
    catch (...) {
      delete v;
      throw;
    }
  }
  Json edges(Json::Array);
  for (size_t i = 0; i < _path.edges.size(); ++i) {
    auto e = edgeToJson(trx, resolver, _path.edges[i]);
    try {
      edges(*e);
      delete e;
    }
    catch (...) {
      delete e;
      throw;
    }
  }
  (*path)("vertices", vertices)
         ("edges", edges);

  return path.release();
}


Json* SingleServerTraversalPath::lastEdgeToJson (Transaction* trx,
                                                 CollectionNameResolver* resolver) {
  return edgeToJson(trx, resolver, _path.edges.back());
}

Json* SingleServerTraversalPath::lastVertexToJson (Transaction* trx,
                                                   CollectionNameResolver* resolver) {
  return vertexToJson(trx, resolver, _path.vertices.back());
}

Json* SingleServerTraversalPath::edgeToJson (Transaction* trx,
                                             CollectionNameResolver* resolver,
                                             EdgeInfo const& e) {
  auto collection = trx->trxCollection(e.cid);
  TRI_shaped_json_t shapedJson;
  TRI_EXTRACT_SHAPED_JSON_MARKER(shapedJson, &e.mptr);
  return new Json(TRI_ExpandShapedJson(
    collection->_collection->_collection->getShaper(),
    resolver,
    e.cid,
    &e.mptr
  ));
}

Json* SingleServerTraversalPath::vertexToJson (Transaction* trx,
                                               CollectionNameResolver* resolver,
                                               VertexId const& v) {
  auto collection = trx->trxCollection(v.cid);
  if (collection == nullptr) {
    int res = TRI_AddCollectionTransaction(trx->getInternals(), 
                                           v.cid,
                                           TRI_TRANSACTION_READ,
                                           trx->nestingLevel(),
                                           true,
                                           true);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }

    TRI_EnsureCollectionsTransaction(trx->getInternals());
    collection = trx->trxCollection(v.cid);

    if (collection == nullptr) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "collection is a nullptr");
    }
  }
  TRI_doc_mptr_copy_t mptr;
  int res = trx->readSingle(collection, &mptr, v.key);
  ++_readDocuments;
  if (res != TRI_ERROR_NO_ERROR) {
    if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
      return new Json(Json::Null);
    }
    THROW_ARANGO_EXCEPTION(res);
  }
  return new Json(TRI_ExpandShapedJson(
    collection->_collection->_collection->getShaper(),
    resolver,
    v.cid,
    &mptr
  ));
}

// -----------------------------------------------------------------------------
// --SECTION--                                         class DepthFirstTraverser
// -----------------------------------------------------------------------------

DepthFirstTraverser::DepthFirstTraverser (
  std::vector<TRI_document_collection_t*> const& edgeCollections,
  TraverserOptions& opts,
  CollectionNameResolver* resolver,
  Transaction* trx,
  std::unordered_map<size_t, std::vector<TraverserExpression*>> const* expressions
) : Traverser(opts, expressions),
    _resolver(resolver),
    _edgeCols(edgeCollections),
    _trx(trx) {
  _defInternalFunctions();
}

bool DepthFirstTraverser::edgeMatchesConditions (TRI_doc_mptr_t& e, size_t& eColIdx, size_t depth) {
  auto it = _expressions->find(depth);
  if (it != _expressions->end()) {
    for (auto const& exp : it->second) {
      if (exp->isEdgeAccess && ! exp->matchesCheck(e, _edgeCols.at(eColIdx), _resolver)) {
        ++_filteredPaths;
        return false;
      }
    }
  }
  return true;
}

bool DepthFirstTraverser::vertexMatchesConditions (VertexId const& v, size_t depth) {
  auto it = _expressions->find(depth);
  if (it != _expressions->end()) {
    TRI_doc_mptr_copy_t mptr;
    TRI_document_collection_t* docCol;
    bool fetchVertex = true;
    for (auto const& exp : it->second) {
      if (! exp->isEdgeAccess) {
        if (fetchVertex) {
          fetchVertex = false;
          auto collection = _trx->trxCollection(v.cid);
          if (collection == nullptr) {
            int res = TRI_AddCollectionTransaction(_trx->getInternals(), 
                                                   v.cid,
                                                   TRI_TRANSACTION_READ,
                                                   _trx->nestingLevel(),
                                                   true,
                                                   true);
            if (res != TRI_ERROR_NO_ERROR) {
              THROW_ARANGO_EXCEPTION(res);
            }
            TRI_EnsureCollectionsTransaction(_trx->getInternals());
            collection = _trx->trxCollection(v.cid);

            if (collection == nullptr) {
              THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "collection is a nullptr");
            }
          }

         int res = _trx->readSingle(collection, &mptr, v.key); 
          ++_readDocuments;
          if (res != TRI_ERROR_NO_ERROR) {
            if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
              // Vertex does not exist. Do not try filter
              triagens::basics::Json tmp(triagens::basics::Json::Null);
              // This needs a different check method now.
              // Innerloop here
              for (auto const& exp2 : it->second) {
                if (! exp2->isEdgeAccess) {
                  if (! exp2->matchesCheck(tmp.json())) {
                    ++_filteredPaths;
                    return false;
                  }
                }
              }
              return true;
            }
            THROW_ARANGO_EXCEPTION(res);
          }
          docCol = collection->_collection->_collection;
        }
        if (! exp->matchesCheck(mptr, docCol, _resolver)) {
          ++_filteredPaths;
          return false;
        }
      }
    }
  }
  return true;
}
          

void DepthFirstTraverser::_defInternalFunctions () {
  _getVertex = [] (EdgeInfo const& edge, VertexId const& vertex, size_t depth, VertexId& result) -> bool {
    auto mptr = edge.mptr;
    if (strcmp(TRI_EXTRACT_MARKER_FROM_KEY(&mptr), vertex.key) == 0 &&
        TRI_EXTRACT_MARKER_FROM_CID(&mptr) == vertex.cid) {
      result = VertexId(TRI_EXTRACT_MARKER_TO_CID(&mptr), TRI_EXTRACT_MARKER_TO_KEY(&mptr));
    }
    else {
      result = VertexId(TRI_EXTRACT_MARKER_FROM_CID(&mptr), TRI_EXTRACT_MARKER_FROM_KEY(&mptr));
    }
    return true;
  };

  if (_opts.direction == TRI_EDGE_ANY) {
    _getEdge = [&] (VertexId const& startVertex, std::vector<EdgeInfo>& edges, TRI_doc_mptr_copy_t*& last, size_t& eColIdx, bool& dir) {
      std::vector<TRI_doc_mptr_copy_t> tmp;
      TRI_ASSERT(eColIdx < _edgeCols.size());
      triagens::arango::EdgeIndex* edgeIndex = _edgeCols.at(eColIdx)->edgeIndex();
      if (dir) {
        TRI_edge_index_iterator_t it(TRI_EDGE_OUT, startVertex.cid, startVertex.key);
        edgeIndex->lookup(&it, tmp, last, 1);

        while (last == nullptr) {
          // Switch back direction
          dir = false;
          ++eColIdx;
          if (_edgeCols.size() == eColIdx) {
            // No more to do, stop here
            return;
          }
          TRI_edge_index_iterator_t it(TRI_EDGE_IN, startVertex.cid, startVertex.key);
          edgeIndex = _edgeCols.at(eColIdx)->edgeIndex();
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
            if (_edgeCols.size() == eColIdx) {
              // No more to do, stop here
              return;
            }
            TRI_edge_index_iterator_t it(TRI_EDGE_IN, startVertex.cid, startVertex.key);
            edgeIndex = _edgeCols.at(eColIdx)->edgeIndex();
            edgeIndex->lookup(&it, tmp, last, 1);
          }
        }
      }
      if (last != nullptr) {
        ++_readDocuments;
        // sth is stored in tmp. Now push it on edges
        TRI_ASSERT(tmp.size() == 1);
        if (! edgeMatchesConditions(tmp.back(), eColIdx, edges.size())) {
          // Retry with the next element
          _getEdge(startVertex, edges, last, eColIdx, dir);
          return;
        }
        EdgeInfo e(
          _edgeCols.at(eColIdx)->_info._cid,
          tmp.back()
        );
        VertexId other;
        // This always returns true and third parameter is ignored
        _getVertex(e, startVertex, 0, other);
        if (! vertexMatchesConditions(other, edges.size() + 1)) {
          // Retry with the next element
          _getEdge(startVertex, edges, last, eColIdx, dir);
          return;
        }
        edges.push_back(e);
      }
    };
  } else {
    _getEdge = [&] (VertexId const& startVertex, std::vector<EdgeInfo>& edges, TRI_doc_mptr_copy_t*& last, size_t& eColIdx, bool& dir) {
      std::vector<TRI_doc_mptr_copy_t> tmp;
      TRI_ASSERT(eColIdx < _edgeCols.size());
      // Do not touch the bool parameter, as long as it is default the first encountered nullptr is final
      TRI_edge_index_iterator_t it(_opts.direction, startVertex.cid, startVertex.key);
      triagens::arango::EdgeIndex* edgeIndex = _edgeCols.at(eColIdx)->edgeIndex();
      edgeIndex->lookup(&it, tmp, last, 1);
      while (last == nullptr) {
        // This edge collection does not have any more edges for this vertex. Check the next one
        ++eColIdx;
        if (_edgeCols.size() == eColIdx) {
          // No more to do, stop here
          return;
        }
        edgeIndex = _edgeCols.at(eColIdx)->edgeIndex();
        edgeIndex->lookup(&it, tmp, last, 1);
      }
      if (last != nullptr) {
        // sth is stored in tmp. Now push it on edges
        ++_readDocuments;
        TRI_ASSERT(tmp.size() == 1);
        if (! edgeMatchesConditions(tmp.back(), eColIdx, edges.size())) {
          // Retry with the next element
          _getEdge(startVertex, edges, last, eColIdx, dir);
          return;
        }
        EdgeInfo e(
          _edgeCols.at(eColIdx)->_info._cid,
          tmp.back()
        );
        VertexId other;
        // This always returns true and third parameter is ignored
        _getVertex(e, startVertex, 0, other);
        if (! vertexMatchesConditions(other, edges.size() + 1)) {
          // Retry with the next element
          _getEdge(startVertex, edges, last, eColIdx, dir);
          return;
        }
        edges.push_back(e);
      }
    };
  }
}

void DepthFirstTraverser::setStartVertex (triagens::arango::traverser::VertexId const& v) {
  auto it = _expressions->find(0);
  if (it != _expressions->end()) {
    if (! it->second.empty()) {
      TRI_doc_mptr_copy_t mptr;
      TRI_document_collection_t* docCol;
      bool fetchVertex = true;
      for (auto const& exp : it->second) {
        if (! exp->isEdgeAccess) {
          if (fetchVertex) {
            fetchVertex = false;
            auto collection = _trx->trxCollection(v.cid);
            if (collection == nullptr) {
              int res = TRI_AddCollectionTransaction(_trx->getInternals(), 
                                                     v.cid,
                                                     TRI_TRANSACTION_READ,
                                                     _trx->nestingLevel(),
                                                     true,
                                                     true);
              if (res != TRI_ERROR_NO_ERROR) {
                THROW_ARANGO_EXCEPTION(res);
              }
              TRI_EnsureCollectionsTransaction(_trx->getInternals());
              collection = _trx->trxCollection(v.cid);

              if (collection == nullptr) {
                THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, "collection is a nullptr");
              }
            }

            int res = _trx->readSingle(collection, &mptr, v.key); 
            ++_readDocuments;
            if (res != TRI_ERROR_NO_ERROR) {
              // Vertex does not exist
              _done = true;
              return;
            }
            docCol = collection->_collection->_collection;
          }
          if (! exp->matchesCheck(mptr, docCol, _resolver)) {
            ++_filteredPaths;
            _done = true;
            return;
          }
        }
      }
    }
  }
  _enumerator.reset(new PathEnumerator<EdgeInfo, VertexId, TRI_doc_mptr_copy_t>(_getEdge, _getVertex, v));
  _done = false;
}

TraversalPath* DepthFirstTraverser::next () {
  TRI_ASSERT(!_done);
  if (_pruneNext) {
    _pruneNext = false;
    _enumerator->prune();
  }
  TRI_ASSERT(!_pruneNext);
  const EnumeratedPath<EdgeInfo, VertexId>& path = _enumerator->next();
  size_t countEdges = path.edges.size();
  if (countEdges == 0) {
    _done = true;
    // Done traversing
    return nullptr;
  }
  std::unique_ptr<SingleServerTraversalPath> p(new SingleServerTraversalPath(path));
  if (_opts.shouldPrunePath(p.get())) {
    _enumerator->prune();
    return next();
  }
  if (countEdges >= _opts.maxDepth) {
    _pruneNext = true;
  }
  if (countEdges < _opts.minDepth) {
    return next();
  }
  return p.release();
}
