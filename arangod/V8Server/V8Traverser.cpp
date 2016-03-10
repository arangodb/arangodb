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

#include "V8Traverser.h"
#include "Indexes/EdgeIndex.h"
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationCursor.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/document-collection.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/VocShaper.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::traverser;


EdgeCollectionInfo::EdgeCollectionInfo(arangodb::Transaction* trx,
                                       std::string const& collectionName,
                                       WeightCalculatorFunction weighter)
    : _trx(trx), _collectionName(collectionName), _weighter(weighter) {
  TRI_voc_cid_t cid = trx->resolver()->getCollectionIdLocal(collectionName);

  if (cid == 0) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, "'%s'",
                                  collectionName.c_str());
  }
  trx->addCollectionAtRuntime(cid);

  if (!trx->isEdgeCollection(collectionName)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
  }

  TRI_document_collection_t* documentCollection = trx->documentCollection(cid);
  arangodb::EdgeIndex* edgeIndex = documentCollection->edgeIndex();
  TRI_ASSERT(edgeIndex !=
             nullptr);  // Checked because collection is edge Collection.
  _indexId = arangodb::basics::StringUtils::itoa(edgeIndex->id());
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get edges for the given direction and start vertex.
////////////////////////////////////////////////////////////////////////////////

OperationCursor EdgeCollectionInfo::getEdges(TRI_edge_direction_e direction,
                                             std::string const& vertexId) {

  _searchBuilder.clear();
  EdgeIndex::buildSearchValue(direction, vertexId, _searchBuilder);
  return _trx->indexScan(_collectionName,
                         arangodb::Transaction::CursorType::INDEX, _indexId,
                         _searchBuilder.slice(), 0, UINT64_MAX, 1000, false);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Compute the weight of an edge
////////////////////////////////////////////////////////////////////////////////

double EdgeCollectionInfo::weightEdge(VPackSlice const edge) {
  return _weighter(edge);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Return name of the wrapped collection
////////////////////////////////////////////////////////////////////////////////

std::string const& EdgeCollectionInfo::getName() {
  return _collectionName;
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Expander for Multiple edge collections
////////////////////////////////////////////////////////////////////////////////

class MultiCollectionEdgeExpander {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Edge direction for this expander
  //////////////////////////////////////////////////////////////////////////////

  TRI_edge_direction_e _direction;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief all info required for edge collection
  //////////////////////////////////////////////////////////////////////////////

  std::vector<EdgeCollectionInfo*> _edgeCollections;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief function to check if the edge passes the filter
  //////////////////////////////////////////////////////////////////////////////

  std::function<bool(VPackSlice)> _isAllowed;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief function to check if the vertex passes the filter
  //////////////////////////////////////////////////////////////////////////////

  std::function<bool(std::string const&)> _isAllowedVertex;

 public:
  MultiCollectionEdgeExpander(
      TRI_edge_direction_e const& direction,
      std::vector<EdgeCollectionInfo*> const& edgeCollections,
      std::function<bool(VPackSlice const)> isAllowed,
      std::function<bool(std::string const&)> isAllowedVertex)
      : _direction(direction),
        _edgeCollections(edgeCollections),
        _isAllowed(isAllowed),
        _isAllowedVertex(isAllowedVertex) {}

  void operator()(std::string const& source,
                  std::vector<ArangoDBPathFinder::Step*>& result) {
    for (auto const& edgeCollection : _edgeCollections) {
      TRI_ASSERT(edgeCollection != nullptr);

      auto edgeCursor = edgeCollection->getEdges(_direction, source);
      std::unordered_map<std::string, size_t> candidates;

      auto inserter = [&](std::string const& s, std::string const& t,
                          double currentWeight, VPackSlice edge) {
        if (_isAllowedVertex(t)) {
          auto cand = candidates.find(t);
          if (cand == candidates.end()) {
            // Add weight
#warning The third parameter has to be replaced by _id content. We need to extract the internal attribute here. Waiting for JAN
            result.emplace_back(new ArangoDBPathFinder::Step(
                t, s, currentWeight,
                edge.get(TRI_VOC_ATTRIBUTE_ID).copyString()));
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

      while (edgeCursor.hasMore()) {
        int res = edgeCursor.getMore();
        if (res != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(res);
        }
        VPackSlice edges = edgeCursor.slice();

        for (auto const& edge : VPackArrayIterator(edges)) {
          if (!_isAllowed(edge)) {
            continue;
          }
          std::string const from = edge.get(TRI_VOC_ATTRIBUTE_FROM).copyString();
          std::string const to = edge.get(TRI_VOC_ATTRIBUTE_TO).copyString();
          double currentWeight = edgeCollection->weightEdge(edge);
          if (from == source) {
            inserter(from, to, currentWeight, edge);
          } else {
            inserter(to, from, currentWeight, edge);
          }
        }
      }
    }
  }
};

class SimpleEdgeExpander {
  //////////////////////////////////////////////////////////////////////////////
  /// @brief The direction used for edges in this expander
  //////////////////////////////////////////////////////////////////////////////

  TRI_edge_direction_e _direction;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief all info required for edge collection
  //////////////////////////////////////////////////////////////////////////////

  EdgeCollectionInfo* _edgeCollection;

 public:
  SimpleEdgeExpander(TRI_edge_direction_e& direction,
                     EdgeCollectionInfo* edgeCollection)
      : _direction(direction), _edgeCollection(edgeCollection) {}

  void operator()(std::string const& source,
                  std::vector<ArangoDBPathFinder::Step*>& result) {
    TRI_ASSERT(_edgeCollection != nullptr);

    std::unordered_map<std::string, size_t> candidates;

    auto inserter = [&](std::string const& s, std::string const& t,
                        double currentWeight, VPackSlice edge) {
      auto cand = candidates.find(t);
      if (cand == candidates.end()) {
        // Add weight
        result.emplace_back(new ArangoDBPathFinder::Step(
#warning The third parameter has to be replaced by _id content. We need to extract the internal attribute here. Waiting for JAN
            t, s, currentWeight, edge.get(TRI_VOC_ATTRIBUTE_ID).copyString()));
        candidates.emplace(t, result.size() - 1);
      } else {
        // Compare weight
        auto oldWeight = result[cand->second]->weight();
        if (currentWeight < oldWeight) {
          result[cand->second]->setWeight(currentWeight);
        }
      }
    };

    auto edgeCursor = _edgeCollection->getEdges(_direction, source);
    while (edgeCursor.hasMore()) {
      int res = edgeCursor.getMore();
      if (res != TRI_ERROR_NO_ERROR) {
        THROW_ARANGO_EXCEPTION(res);
      }
      VPackSlice edges = edgeCursor.slice();
      for (auto const& edge : VPackArrayIterator(edges)) {
        std::string const from = edge.get(TRI_VOC_ATTRIBUTE_FROM).copyString();
        std::string const to = edge.get(TRI_VOC_ATTRIBUTE_TO).copyString();
        double currentWeight = _edgeCollection->weightEdge(edge);
        if (from == source) {
          inserter(from, to, currentWeight, edge);
        } else {
          inserter(to, from, currentWeight, edge);
        }
      }
    }
  }
};

////////////////////////////////////////////////////////////////////////////////
/// @brief Insert a new vertex matcher object
////////////////////////////////////////////////////////////////////////////////

void BasicOptions::addVertexFilter(v8::Isolate* isolate,
                                   v8::Handle<v8::Value> const& example,
                                   ExplicitTransaction* trx,
                                   TRI_document_collection_t* col,
                                   VocShaper* shaper, TRI_voc_cid_t const& cid,
                                   std::string& errorMessage) {
  auto it = _vertexFilter.find(cid);

  if (example->IsArray()) {
    if (it == _vertexFilter.end()) {
      _vertexFilter.emplace(
          cid, VertexFilterInfo(
                   trx, col, new ExampleMatcher(
                                 isolate, v8::Handle<v8::Array>::Cast(example),
                                 errorMessage)));
    }
  } else {
    // Has to be Object
    if (it == _vertexFilter.end()) {
      _vertexFilter.emplace(
          cid, VertexFilterInfo(
                   trx, col, new ExampleMatcher(
                                 isolate, v8::Handle<v8::Array>::Cast(example),
                                 errorMessage)));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if a vertex matches to given examples
////////////////////////////////////////////////////////////////////////////////

bool BasicOptions::matchesVertex(std::string const& v) const {
  if (!useVertexFilter) {
    // Nothing to do
    return true;
  }

#warning We need to find a solution for this. cid => matcher, is not available any more
  return true;
  /*
  auto it = _vertexFilter.find(v.cid);

  if (it == _vertexFilter.end()) {
    // This collection does not have any object of this shape.
    // Short circuit.
    return false;
  }

  OperationOptions options;
  VPackSlice slice;
#warning fill slice from v.key
#warning pass trx into this function!
//  OperationResult opRes = trx->document(it->second.col, slice, options);
  OperationResult opRes(TRI_ERROR_INTERNAL);
#warning fill vertex

  if (!opRes.successful()) {
    return false;
  }

  return it->second.matcher->matches(opRes.slice());
  */
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Insert a new edge matcher object
////////////////////////////////////////////////////////////////////////////////

void BasicOptions::addEdgeFilter(v8::Isolate* isolate,
                                 v8::Handle<v8::Value> const& example,
                                 std::string const& cName,
                                 std::string& errorMessage) {
  useEdgeFilter = true;
  auto it = _edgeFilter.find(cName);

  if (it != _edgeFilter.end()) {
    return;
  }

  if (example->IsArray()) {
    _edgeFilter.emplace(
        cName, new ExampleMatcher(isolate, v8::Handle<v8::Array>::Cast(example),
                                errorMessage));
  } else {
    // Has to be Object
    _edgeFilter.emplace(
        cName, new ExampleMatcher(isolate, v8::Handle<v8::Object>::Cast(example),
                                errorMessage));
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Insert a new edge matcher object
////////////////////////////////////////////////////////////////////////////////

void BasicOptions::addEdgeFilter(VPackSlice const& example,
                                 std::string const& cName) {
  useEdgeFilter = true;
  auto it = _edgeFilter.find(cName);
  if (it == _edgeFilter.end()) {
    _edgeFilter.emplace(cName, new ExampleMatcher(example, true));
  }
}



////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if an edge matches to given examples
////////////////////////////////////////////////////////////////////////////////

bool BasicOptions::matchesEdge(VPackSlice edge) const {
  if (!useEdgeFilter) {
    // Nothing to do
    return true;
  }

#warning We need to find a solution for this. cid => matcher, is not available any more
  return true;
  /*
  auto it = _edgeFilter.find(e.cid);

  if (it == _edgeFilter.end()) {
    // This collection does not have any object that can match.
    // Short circuit.
    return false;
  }

  return it->second->matches(VPackSlice(edge->vpack()));
  */
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if a vertex matches to given examples
////////////////////////////////////////////////////////////////////////////////

bool ShortestPathOptions::matchesVertex(std::string const& v) const {
  if (start == v || end == v) {
    return true;
  }

  return BasicOptions::matchesVertex(v);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if a vertex matches to given examples
////////////////////////////////////////////////////////////////////////////////

bool NeighborsOptions::matchesVertex(std::string const& v) const {
  // If there are explicitly marked collections check them.
  if (!_explicitCollections.empty()) {
#warning find a solution for this.VertexId
    return false;
    // If the current collection is not stored the result is invalid
    /*
    if (_explicitCollections.find(v.cid) == _explicitCollections.end()) {
      return false;
    }
    */
  }
  return BasicOptions::matchesVertex(v);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Inserts one explicitly allowed collection. As soon as one is
/// explicitly
/// allowed all others are implicitly disallowed. If there is no explicitly
/// allowed
/// collection all are implicitly allowed.
////////////////////////////////////////////////////////////////////////////////

void NeighborsOptions::addCollectionRestriction(TRI_voc_cid_t cid) {
  _explicitCollections.emplace(cid);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the shortest path computation
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ArangoDBPathFinder::Path> TRI_RunShortestPathSearch(
    std::vector<EdgeCollectionInfo*>& collectionInfos,
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

  auto edgeFilterClosure = [&opts](VPackSlice edge)
                               -> bool { return opts.matchesEdge(edge); };

  auto vertexFilterClosure =
      [&opts](std::string const& v) -> bool { return opts.matchesVertex(v); };

  MultiCollectionEdgeExpander forwardExpander(
      forward, collectionInfos, edgeFilterClosure, vertexFilterClosure);
  MultiCollectionEdgeExpander backwardExpander(
      backward, collectionInfos, edgeFilterClosure, vertexFilterClosure);

  ArangoDBPathFinder pathFinder(forwardExpander, backwardExpander,
                                opts.bidirectional);
  std::unique_ptr<ArangoDBPathFinder::Path> path;
  if (opts.multiThreaded) {
    path.reset(pathFinder.shortestPathTwoThreads(opts.start, opts.end));
  } else {
    path.reset(pathFinder.shortestPath(opts.start, opts.end));
  }
  return path;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the shortest path computation
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ArangoDBConstDistancePathFinder::Path>
TRI_RunSimpleShortestPathSearch(
    std::vector<EdgeCollectionInfo*>& collectionInfos,
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

  auto fwExpander =
      [&collectionInfos, forward](std::string const& v, std::vector<std::string>& res_edges,
                                  std::vector<std::string>& neighbors) {
        for (auto const& edgeCollection : collectionInfos) {
          TRI_ASSERT(edgeCollection != nullptr);
          OperationCursor edgeCursor = edgeCollection->getEdges(forward, v);
          while (edgeCursor.hasMore()) {
            int res = edgeCursor.getMore();
            if (res != TRI_ERROR_NO_ERROR) {
              THROW_ARANGO_EXCEPTION(res);
            }
            VPackSlice edges = edgeCursor.slice();

            for (auto const& edge : VPackArrayIterator(edges)) {
#warning fucking custom type
              std::string edgeId = edge.get(TRI_VOC_ATTRIBUTE_ID).copyString();
              std::string from = edge.get(TRI_VOC_ATTRIBUTE_FROM).copyString();
              if (from == v) {
                std::string to = edge.get(TRI_VOC_ATTRIBUTE_TO).copyString();
                if (to == v) {
                  res_edges.emplace_back(edgeId);
                  neighbors.emplace_back(to);
                }
              } else {
                res_edges.emplace_back(edgeId);
                neighbors.emplace_back(from);
              }

            }
          }
        }
      };
  auto bwExpander =
      [&collectionInfos, backward](std::string const& v, std::vector<std::string>& res_edges,
                                   std::vector<std::string>& neighbors) {
        for (auto const& edgeCollection : collectionInfos) {
          TRI_ASSERT(edgeCollection != nullptr);
          OperationCursor edgeCursor = edgeCollection->getEdges(backward, v);
          while (edgeCursor.hasMore()) {
            int res = edgeCursor.getMore();
            if (res != TRI_ERROR_NO_ERROR) {
              THROW_ARANGO_EXCEPTION(res);
            }
            VPackSlice edges = edgeCursor.slice();

            for (auto const& edge : VPackArrayIterator(edges)) {
#warning fucking custom type
              std::string edgeId = edge.get(TRI_VOC_ATTRIBUTE_ID).copyString();
              std::string from = edge.get(TRI_VOC_ATTRIBUTE_FROM).copyString();
              if (from == v) {
                std::string to = edge.get(TRI_VOC_ATTRIBUTE_TO).copyString();
                if (to == v) {
                  res_edges.emplace_back(edgeId);
                  neighbors.emplace_back(to);
                }
              } else {
                res_edges.emplace_back(edgeId);
                neighbors.emplace_back(from);
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

static void InboundNeighbors(std::vector<EdgeCollectionInfo*>& collectionInfos,
                             NeighborsOptions& opts,
                             std::unordered_set<std::string>& startVertices,
                             std::unordered_set<std::string>& visited,
                             std::unordered_set<std::string>& distinct,
                             uint64_t depth = 1) {
  TRI_edge_direction_e dir = TRI_EDGE_IN;
  std::unordered_set<std::string> nextDepth;

  for (auto const& col : collectionInfos) {
    TRI_ASSERT(col != nullptr);

    for (auto const& start : startVertices) {
      auto edgeCursor = col->getEdges(dir, start);
      while (edgeCursor.hasMore()) {
        int res = edgeCursor.getMore();
        if (res != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(res);
        }
        VPackSlice edges = edgeCursor.slice();
        for (auto const& edge : VPackArrayIterator(edges)) {
          if (opts.matchesEdge(edge)) {
            std::string v = edge.get(TRI_VOC_ATTRIBUTE_FROM).copyString();
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
  }

  if (!nextDepth.empty()) {
    InboundNeighbors(collectionInfos, opts, nextDepth, visited, distinct,
                     depth + 1);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief search for distinct outbound neighbors
////////////////////////////////////////////////////////////////////////////////

static void OutboundNeighbors(std::vector<EdgeCollectionInfo*>& collectionInfos,
                              NeighborsOptions& opts,
                              std::unordered_set<std::string>& startVertices,
                              std::unordered_set<std::string>& visited,
                              std::unordered_set<std::string>& distinct,
                              uint64_t depth = 1) {
  TRI_edge_direction_e dir = TRI_EDGE_OUT;
  std::unordered_set<std::string> nextDepth;

  for (auto const& col : collectionInfos) {
    TRI_ASSERT(col != nullptr);

    for (auto const& start : startVertices) {
      auto edgeCursor = col->getEdges(dir, start);
      while (edgeCursor.hasMore()) {
        int res = edgeCursor.getMore();
        if (res != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(res);
        }
        VPackSlice edges = edgeCursor.slice();
        for (auto const& edge : VPackArrayIterator(edges)) {
          if (opts.matchesEdge(edge)) {
            std::string v = edge.get(TRI_VOC_ATTRIBUTE_TO).copyString();
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
  }

  if (!nextDepth.empty()) {
    OutboundNeighbors(collectionInfos, opts, nextDepth, visited, distinct,
                      depth + 1);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief search for distinct in- and outbound neighbors
////////////////////////////////////////////////////////////////////////////////

static void AnyNeighbors(std::vector<EdgeCollectionInfo*>& collectionInfos,
                         NeighborsOptions& opts,
                         std::unordered_set<std::string>& startVertices,
                         std::unordered_set<std::string>& visited,
                         std::unordered_set<std::string>& distinct,
                         uint64_t depth = 1) {

  TRI_edge_direction_e dir = TRI_EDGE_ANY;
  std::unordered_set<std::string> nextDepth;

  for (auto const& col : collectionInfos) {
    TRI_ASSERT(col != nullptr);

    for (auto const& start : startVertices) {
      auto edgeCursor = col->getEdges(dir, start);
      while (edgeCursor.hasMore()) {
        int res = edgeCursor.getMore();
        if (res != TRI_ERROR_NO_ERROR) {
          THROW_ARANGO_EXCEPTION(res);
        }
        VPackSlice edges = edgeCursor.slice();
        for (auto const& edge : VPackArrayIterator(edges)) {
          if (opts.matchesEdge(edge)) {
            std::string v = edge.get(TRI_VOC_ATTRIBUTE_TO).copyString();
            if (visited.find(v) == visited.end()) {
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
            v = edge.get(TRI_VOC_ATTRIBUTE_FROM).copyString();
            if (visited.find(v) == visited.end()) {
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
    }
  }

  if (!nextDepth.empty()) {
    AnyNeighbors(collectionInfos, opts, nextDepth, visited, distinct,
                 depth + 1);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Execute a search for neighboring vertices
////////////////////////////////////////////////////////////////////////////////

void TRI_RunNeighborsSearch(std::vector<EdgeCollectionInfo*>& collectionInfos,
                            NeighborsOptions& opts,
                            std::unordered_set<std::string>& result) {
  std::unordered_set<std::string> startVertices;
  std::unordered_set<std::string> visited;
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

void SingleServerTraversalPath::getDocumentByIdentifier(Transaction* trx,
                                                        std::string const& identifier,
                                                        VPackBuilder& result) {
  // TODO Check if we can get away with using ONLY VPackSlices referencing externals instead of std::string.
  // I am afaid that they may run out of scope.
  _searchBuilder.clear();
  _searchBuilder.openObject();
  _searchBuilder.add(VPackValue(TRI_VOC_ATTRIBUTE_KEY));

  std::vector<std::string> parts =
      arangodb::basics::StringUtils::split(identifier, "/");

  TRI_ASSERT(parts.size() == 2);
  _searchBuilder.add(VPackValue(parts[1]));
  _searchBuilder.close();

  TRI_voc_cid_t cid = trx->resolver()->getCollectionIdLocal(parts[0]);

  if (cid == 0) {
    THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND, "'%s'",
                                  parts[0].c_str());
  }
  trx->addCollectionAtRuntime(cid);

  OperationOptions options;
  OperationResult opRes = trx->document(parts[0], _searchBuilder.slice(), options);
      
  if (opRes.failed()) {
    THROW_ARANGO_EXCEPTION(opRes.code);
  }

  result.add(opRes.slice());
}

void SingleServerTraversalPath::pathToVelocyPack(Transaction* trx,
                                                 VPackBuilder& result) {
  result.openObject();
  result.add(VPackValue("edges"));
  result.openArray();
  for (auto const& it : _path.edges) {
    getDocumentByIdentifier(trx, it, result);
  }
  result.close();
  result.add(VPackValue("vertices"));
  result.openArray();
  for (auto const& it : _path.vertices) {
    getDocumentByIdentifier(trx, it, result);
  }
  result.close();
  result.close();
}

void SingleServerTraversalPath::lastEdgeToVelocyPack(Transaction* trx, VPackBuilder& result) {
  getDocumentByIdentifier(trx, _path.edges.back(), result);
}

void SingleServerTraversalPath::lastVertexToVelocyPack(Transaction* trx, VPackBuilder& result) {
  getDocumentByIdentifier(trx, _path.vertices.back(), result);
}

DepthFirstTraverser::DepthFirstTraverser(
    std::vector<TRI_document_collection_t*> const& edgeCollections,
    TraverserOptions& opts, CollectionNameResolver* resolver, Transaction* trx,
    std::unordered_map<size_t, std::vector<TraverserExpression*>> const*
        expressions)
    : Traverser(opts, expressions),
      _resolver(resolver),
      _edgeGetter(this, opts, resolver, trx),
      _edgeCols(edgeCollections),
      _trx(trx) {
  _defInternalFunctions();
}

bool DepthFirstTraverser::edgeMatchesConditions(TRI_doc_mptr_t& e,
                                                size_t& eColIdx, size_t depth) {
  TRI_ASSERT(_expressions != nullptr);

  auto it = _expressions->find(depth);

  if (it != _expressions->end()) {
    for (auto const& exp : it->second) {
      TRI_ASSERT(exp != nullptr);

      if (exp->isEdgeAccess &&
          !exp->matchesCheck(e, _edgeCols.at(eColIdx), _resolver)) {
        ++_filteredPaths;
        return false;
      }
    }
  }
  return true;
}

bool DepthFirstTraverser::vertexMatchesConditions(VPackSlice const& v,
                                                  size_t depth) {
  TRI_ASSERT(_expressions != nullptr);

  auto it = _expressions->find(depth);

  if (it != _expressions->end()) {
    /* TODO FIXME
    // This has to be replaced by new Transaction API
    TRI_doc_mptr_t mptr;
    TRI_document_collection_t* docCol = nullptr;
    bool fetchVertex = true;
    for (auto const& exp : it->second) {
      TRI_ASSERT(exp != nullptr);

      if (!exp->isEdgeAccess) {
        if (fetchVertex) {
          fetchVertex = false;
          auto collection = _trx->trxCollection(v.cid);
          if (collection == nullptr) {
            int res = TRI_AddCollectionTransaction(
                _trx->getInternals(), v.cid, TRI_TRANSACTION_READ,
                _trx->nestingLevel(), true, true);
            if (res != TRI_ERROR_NO_ERROR) {
              THROW_ARANGO_EXCEPTION(res);
            }
            TRI_EnsureCollectionsTransaction(_trx->getInternals());
            collection = _trx->trxCollection(v.cid);

            if (collection == nullptr) {
              THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                             "collection is a nullptr");
            }
            auto trxCollection = _trx->trxCollection(v.cid);
            if (trxCollection != nullptr) {
              _trx->orderDitch(trxCollection);
            }
          }

          int res = _trx->document(collection, &mptr, v.key);
          ++_readDocuments;
          if (res != TRI_ERROR_NO_ERROR) {
            if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
              // Vertex does not exist. Do not try filter
              arangodb::basics::Json tmp(arangodb::basics::Json::Null);
              // This needs a different check method now.
              // Innerloop here
              for (auto const& exp2 : it->second) {
                TRI_ASSERT(exp2 != nullptr);

                if (!exp2->isEdgeAccess) {
                  if (!exp2->matchesCheck(tmp.json())) {
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
        TRI_ASSERT(docCol != nullptr);
        if (!exp->matchesCheck(mptr, docCol, _resolver)) {
          ++_filteredPaths;
          return false;
        }
      }
    }
    */
  }
  return true;
}

void DepthFirstTraverser::_defInternalFunctions() {
  _getVertex = [](std::string const& edge, std::string const& vertex, size_t depth,
                  std::string& result) -> bool {
    return false;
    // TODO FIX THIS
    /* Do we still use mptr here or do we switch to VPack?
    auto mptr = edge.mptr;
    if (strcmp(TRI_EXTRACT_MARKER_FROM_KEY(&mptr), vertex.key) == 0 &&
        TRI_EXTRACT_MARKER_FROM_CID(&mptr) == vertex.cid) {
      result = VertexId(TRI_EXTRACT_MARKER_TO_CID(&mptr),
                        TRI_EXTRACT_MARKER_TO_KEY(&mptr));
    } else {
      result = VertexId(TRI_EXTRACT_MARKER_FROM_CID(&mptr),
                        TRI_EXTRACT_MARKER_FROM_KEY(&mptr));
    }
    return true;
    */
  };
}

void DepthFirstTraverser::setStartVertex(
    VPackSlice const& v) {
  TRI_ASSERT(_expressions != nullptr);

  auto it = _expressions->find(0);

  if (it != _expressions->end()) {
    if (!it->second.empty()) {
      TRI_doc_mptr_t mptr;
      TRI_document_collection_t* docCol = nullptr;
      bool fetchVertex = true;
      for (auto const& exp : it->second) {
        TRI_ASSERT(exp != nullptr);

        if (!exp->isEdgeAccess) {
          if (fetchVertex) {
            fetchVertex = false;
            /* TODO Add Collection to Transaction!
            auto collection = _trx->trxCollection(v.cid);
            if (collection == nullptr) {
              int res = TRI_AddCollectionTransaction(
                  _trx->getInternals(), v.cid, TRI_TRANSACTION_READ,
                  _trx->nestingLevel(), true, true);
              if (res != TRI_ERROR_NO_ERROR) {
                THROW_ARANGO_EXCEPTION(res);
              }
              TRI_EnsureCollectionsTransaction(_trx->getInternals());
              collection = _trx->trxCollection(v.cid);

              if (collection == nullptr) {
                THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                               "collection is a nullptr");
              }
              auto trxCollection = _trx->trxCollection(v.cid);
              if (trxCollection != nullptr) {
                _trx->orderDitch(trxCollection);
              }
            }

            int res = _trx->document(collection, &mptr, v.key);
            ++_readDocuments;
            if (res != TRI_ERROR_NO_ERROR) {
              // Vertex does not exist
              _done = true;
              return;
            }
            docCol = collection->_collection->_collection;
            */
            return;
          }
          TRI_ASSERT(docCol != nullptr);
          if (!exp->matchesCheck(mptr, docCol, _resolver)) {
            ++_filteredPaths;
            _done = true;
            return;
          }
        }
      }
    }
  }
  std::string tmp = v.get(TRI_VOC_ATTRIBUTE_ID).copyString();
  _enumerator.reset(new PathEnumerator<std::string, std::string, TRI_doc_mptr_t>(
      _edgeGetter, _getVertex, tmp));
  _done = false;
}

TraversalPath* DepthFirstTraverser::next() {
  TRI_ASSERT(!_done);
  if (_pruneNext) {
    _pruneNext = false;
    _enumerator->prune();
  }
  TRI_ASSERT(!_pruneNext);
  const EnumeratedPath<std::string, std::string>& path = _enumerator->next();
  size_t countEdges = path.edges.size();
  if (countEdges == 0) {
    _done = true;
    // Done traversing
    return nullptr;
  }

  auto p = std::make_unique<SingleServerTraversalPath>(path);
  if (countEdges >= _opts.maxDepth) {
    _pruneNext = true;
  }
  if (countEdges < _opts.minDepth) {
    return next();
  }
  return p.release();
}

EdgeIndex* DepthFirstTraverser::EdgeGetter::getEdgeIndex(
    std::string const& eColName,
    TRI_voc_cid_t& cid) {
  auto it = _indexCache.find(eColName);

  if (it == _indexCache.end()) {
    cid = _resolver->getCollectionIdLocal(eColName);
    TRI_document_collection_t* documentCollection = _trx->documentCollection(cid);
    arangodb::EdgeIndex* edgeIndex = documentCollection->edgeIndex();
    _indexCache.emplace(eColName, std::make_pair(cid, edgeIndex));
    return edgeIndex;
  }

  cid = it->second.first;
  return it->second.second;
}

void DepthFirstTraverser::EdgeGetter::operator()(
    std::string const& startVertex,
    std::vector<std::string>& edges, TRI_doc_mptr_t*& last, size_t& eColIdx,
    bool& dir) {
#warning use new EdgeIndex VPACK lookup here.
  // builderSearchValue(dir, startVertex, builder);
  // trx->indexScan();
  /*
  std::string eColName;
  TRI_edge_direction_e direction;
  while (true) {
    if (!_opts.getCollection(eColIdx, eColName, direction)) {
      // We are done traversing.
      return;
    }
    TRI_voc_cid_t cid;
    arangodb::EdgeIndex* edgeIndex = getEdgeIndex(eColName, cid);
    std::vector<TRI_doc_mptr_t> tmp;
    if (direction == TRI_EDGE_ANY) {
      TRI_edge_direction_e currentDir = dir ? TRI_EDGE_OUT : TRI_EDGE_IN;
      TRI_edge_index_iterator_t it(currentDir, startVertex);
      edgeIndex->lookup(_trx, &it, tmp, last, 1);
      if (last == nullptr) {
        // Could not find next edge.
        // Change direction and increase collectionId
        if (dir) {
          ++eColIdx;
        }
        dir = !dir;
        continue;
      }
    }
    else {
      TRI_edge_index_iterator_t it(direction, startVertex);
      edgeIndex->lookup(_trx, &it, tmp, last, 1);
      if (last == nullptr) {
        // Could not find next edge.
        // Set direction to false and continue with next collection
        dir = false;
        ++eColIdx;
        continue;
      }
    }
    // If we get here we have found the next edge.
    // Now validate expression checks
    ++_traverser->_readDocuments;
    // sth is stored in tmp. Now push it on edges
    TRI_ASSERT(tmp.size() == 1);
    if (!_traverser->edgeMatchesConditions(tmp.back(), eColIdx, edges.size())) {
      // Retry with the next element
      continue;
    }
    EdgeInfo e(cid, tmp.back());
    VPackSlice other;
    // This always returns true and third parameter is ignored
    _traverser->_getVertex(e, startVertex, 0, other);
    if (!_traverser->vertexMatchesConditions(other, edges.size() + 1)) {
      // Retry with the next element
      continue;
    }
    auto search = std::find(edges.begin(), edges.end(), e);
    if (search != edges.end()) {
      // The edge is included twice. Go on with the next
      continue;
    }
    edges.push_back(e);
    return;
  }
  */
}
