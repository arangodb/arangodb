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

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::traverser;

////////////////////////////////////////////////////////////////////////////////
/// @brief Get a document by it's ID. Also lazy locks the collection.
///        If DOCUMENT_NOT_FOUND this function will return normally
///        with a OperationResult.failed() == true.
///        On all other cases this function throws.
////////////////////////////////////////////////////////////////////////////////

static OperationResult FetchDocumentById(arangodb::Transaction* trx,
                                         std::string const& id,
                                         VPackBuilder& builder,
                                         OperationOptions& options) {
  std::vector<std::string> parts =
          arangodb::basics::StringUtils::split(id, "/");
  TRI_ASSERT(parts.size() == 2);
  trx->addCollectionAtRuntime(parts[0]);
  builder.clear();
  builder.openObject();
  builder.add(VPackValue(TRI_VOC_ATTRIBUTE_KEY));
  builder.add(VPackValue(parts[1]));
  builder.close();

  OperationResult opRes = trx->document(parts[0], builder.slice(), options);

  if (opRes.failed() && opRes.code != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
    THROW_ARANGO_EXCEPTION(opRes.code);
  }
  return opRes;
}

EdgeCollectionInfo::EdgeCollectionInfo(arangodb::Transaction* trx,
                                       std::string const& collectionName,
                                       WeightCalculatorFunction weighter)
    : _trx(trx), _collectionName(collectionName), _weighter(weighter) {

  trx->addCollectionAtRuntime(collectionName);

  if (!trx->isEdgeCollection(collectionName)) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_COLLECTION_TYPE_INVALID);
  }
  _indexId = trx->edgeIndexHandle(collectionName);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get edges for the given direction and start vertex.
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<OperationCursor> EdgeCollectionInfo::getEdges(
    TRI_edge_direction_e direction, std::string const& vertexId) {
#warning Make this thread safe s.t. we only need 2 builders.
  VPackBuilder searchBuilder;
  EdgeIndex::buildSearchValue(direction, vertexId, searchBuilder);
  return _trx->indexScan(_collectionName,
                         arangodb::Transaction::CursorType::INDEX, _indexId,
                         searchBuilder.slice(), 0, UINT64_MAX, 1000, false);
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
            result.emplace_back(new ArangoDBPathFinder::Step(
                t, s, currentWeight, edgeCollection->trx()->extractIdString(edge)));
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

      auto opRes = std::make_shared<OperationResult>(TRI_ERROR_NO_ERROR);
      while (edgeCursor->hasMore()) {
        edgeCursor->getMore(opRes);
        if (opRes->failed()) {
          THROW_ARANGO_EXCEPTION(opRes->code);
        }
        VPackSlice edges = opRes->slice();

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
            t, s, currentWeight, _edgeCollection->trx()->extractIdString(edge)));
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
    auto opRes = std::make_shared<OperationResult>(TRI_ERROR_NO_ERROR);
    while (edgeCursor->hasMore()) {
      edgeCursor->getMore(opRes);
      if (opRes->failed()) {
        THROW_ARANGO_EXCEPTION(opRes->code);
      }
      VPackSlice edges = opRes->slice();
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
                                   arangodb::Transaction* trx,
                                   std::string const& name,
                                   std::string& errorMessage) {
  auto it = _vertexFilter.find(name);

  if (it == _vertexFilter.end()) {
    if (example->IsArray()) {
      _vertexFilter.emplace(name, new ExampleMatcher(
          isolate, v8::Handle<v8::Array>::Cast(example), errorMessage));
    } else {
      // Has to be Object
      _vertexFilter.emplace(name, new ExampleMatcher(
          isolate, v8::Handle<v8::Object>::Cast(example), errorMessage));
    }
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if a vertex matches to given examples
////////////////////////////////////////////////////////////////////////////////

bool BasicOptions::matchesVertex(std::string const& collectionName,
                                 std::string const& key,
                                 VPackSlice vertex) const {
  if (!useVertexFilter) {
    // Nothing to do
    return true;
  }
  auto it = _vertexFilter.find(collectionName);

  if (it == _vertexFilter.end()) {
    // This collection does not have any object of this shape.
    // Short circuit.
    return false;
  }
  return it->second->matches(vertex);
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

  auto id = _trx->extractIdString(edge);
  std::vector<std::string> parts = arangodb::basics::StringUtils::split(id, "/");
  TRI_ASSERT(parts.size() == 2); // We have a real ID
  auto it = _edgeFilter.find(parts[0]);
  
  if (it == _edgeFilter.end()) {
    // This collection does not have any object that can match.
    // Short circuit.
    return false;
  }

  return it->second->matches(edge);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if a vertex matches to given examples
////////////////////////////////////////////////////////////////////////////////

bool ShortestPathOptions::matchesVertex(std::string const& collectionName,
                                        std::string const& key,
                                        VPackSlice vertex) const {
  std::string v = collectionName + "/" + key;
  if (start == v || end == v) {
    return true;
  }

  return BasicOptions::matchesVertex(collectionName, key, vertex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if a vertex matches to given examples
////////////////////////////////////////////////////////////////////////////////

bool NeighborsOptions::matchesVertex(std::string const& collectionName,
                                     std::string const& key,
                                     VPackSlice vertex) const {
  // If there are explicitly marked collections check them.
  if (!_explicitCollections.empty()) {
    // If the current collection is not stored the result is invalid
    if (_explicitCollections.find(collectionName) == _explicitCollections.end()) {
      return false;
    }
  }
  return BasicOptions::matchesVertex(collectionName, key, vertex);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Checks if a vertex matches to given examples. Also fetches the vertex.
////////////////////////////////////////////////////////////////////////////////

bool NeighborsOptions::matchesVertex(std::string const& id) const {
  std::vector<std::string> parts =
      arangodb::basics::StringUtils::split(id, "/");
  TRI_ASSERT(parts.size() == 2);
  // If there are explicitly marked collections check them.
  if (!_explicitCollections.empty()) {
    // If the current collection is not stored the result is invalid
    if (_explicitCollections.find(parts[0]) == _explicitCollections.end()) {
      return false;
    }
  }
  VPackBuilder tmp;
  tmp.openObject();
  tmp.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(parts[1]));
  tmp.close();
  OperationOptions opOpts;
  OperationResult opRes = _trx->document(parts[0], tmp.slice(), opOpts);
  if (opRes.failed()) {
    return false;
  }
  return BasicOptions::matchesVertex(parts[0], parts[1], opRes.slice());
}


////////////////////////////////////////////////////////////////////////////////
/// @brief Inserts one explicitly allowed collection. As soon as one is
/// explicitly
/// allowed all others are implicitly disallowed. If there is no explicitly
/// allowed
/// collection all are implicitly allowed.
////////////////////////////////////////////////////////////////////////////////

void NeighborsOptions::addCollectionRestriction(std::string const& collectionName) {
  _explicitCollections.emplace(collectionName);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Wrapper for the shortest path computation
////////////////////////////////////////////////////////////////////////////////

std::unique_ptr<ArangoDBPathFinder::Path> TRI_RunShortestPathSearch(
    std::vector<EdgeCollectionInfo*> const& collectionInfos,
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
      [&opts](std::string const& v) -> bool {
#warning This closure needs to be optimized
        std::vector<std::string> parts = arangodb::basics::StringUtils::split(v, "/");
        VPackBuilder tmp;
        tmp.openObject();
        tmp.add(TRI_VOC_ATTRIBUTE_KEY, VPackValue(parts[1]));
        tmp.close();
        OperationOptions opOpts;
        OperationResult opRes = opts.trx()->document(parts[0], tmp.slice(), opOpts);
        if (opRes.failed()) {
          return false;
        }
        return opts.matchesVertex(parts[0], parts[1], opRes.slice());
      };

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
    std::vector<EdgeCollectionInfo*> const& collectionInfos,
    arangodb::Transaction* trx,
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
      [&collectionInfos, forward, trx](std::string const& v, std::vector<std::string>& res_edges,
                                  std::vector<std::string>& neighbors) {
        for (auto const& edgeCollection : collectionInfos) {
          TRI_ASSERT(edgeCollection != nullptr);
          std::shared_ptr<OperationCursor> edgeCursor = edgeCollection->getEdges(forward, v);
          auto opRes = std::make_shared<OperationResult>(TRI_ERROR_NO_ERROR);
          while (edgeCursor->hasMore()) {
            edgeCursor->getMore(opRes);
            if (opRes->failed()) {
              THROW_ARANGO_EXCEPTION(opRes->code);
            }
            VPackSlice edges = opRes->slice();

            for (auto const& edge : VPackArrayIterator(edges)) {
              std::string edgeId = trx->extractIdString(edge);
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
      [&collectionInfos, backward, trx](std::string const& v, std::vector<std::string>& res_edges,
                                   std::vector<std::string>& neighbors) {
        auto opRes = std::make_shared<OperationResult>(TRI_ERROR_NO_ERROR);
        for (auto const& edgeCollection : collectionInfos) {
          TRI_ASSERT(edgeCollection != nullptr);
          std::shared_ptr<OperationCursor> edgeCursor = edgeCollection->getEdges(backward, v);
          while (edgeCursor->hasMore()) {
            edgeCursor->getMore(opRes);
            if (opRes->failed()) {
              THROW_ARANGO_EXCEPTION(opRes->code);
            }
            VPackSlice edges = opRes->slice();

            for (auto const& edge : VPackArrayIterator(edges)) {
              std::string edgeId = trx->extractIdString(edge);
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
  auto path = std::make_unique<ArangoDBConstDistancePathFinder::Path>();
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

  auto opRes = std::make_shared<OperationResult>(TRI_ERROR_NO_ERROR);
  for (auto const& col : collectionInfos) {
    TRI_ASSERT(col != nullptr);

    for (auto const& start : startVertices) {
      auto edgeCursor = col->getEdges(dir, start);
      while (edgeCursor->hasMore()) {
        edgeCursor->getMore(opRes);
        if (opRes->failed()) {
          THROW_ARANGO_EXCEPTION(opRes->code);
        }
        VPackSlice edges = opRes->slice();
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
  auto opRes = std::make_shared<OperationResult>(TRI_ERROR_NO_ERROR);

  for (auto const& col : collectionInfos) {
    TRI_ASSERT(col != nullptr);

    for (auto const& start : startVertices) {
      auto edgeCursor = col->getEdges(dir, start);
      while (edgeCursor->hasMore()) {
        edgeCursor->getMore(opRes);
        if (opRes->failed()) {
          THROW_ARANGO_EXCEPTION(opRes->code);
        }
        VPackSlice edges = opRes->slice();
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
  auto opRes = std::make_shared<OperationResult>(TRI_ERROR_NO_ERROR);

  for (auto const& col : collectionInfos) {
    TRI_ASSERT(col != nullptr);

    for (auto const& start : startVertices) {
      auto edgeCursor = col->getEdges(dir, start);
      while (edgeCursor->hasMore()) {
        edgeCursor->getMore(opRes);
        if (opRes->failed()) {
          THROW_ARANGO_EXCEPTION(opRes->code);
        }
        VPackSlice edges = opRes->slice();
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
  OperationOptions options;
  OperationResult opRes = FetchDocumentById(trx, identifier, _searchBuilder, options);

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
    TraverserOptions& opts, Transaction* trx,
    std::unordered_map<size_t, std::vector<TraverserExpression*>> const*
        expressions)
    : Traverser(opts, expressions), _edgeGetter(this, opts, trx), _trx(trx) {
  _defInternalFunctions();
}

bool DepthFirstTraverser::edgeMatchesConditions(VPackSlice e, size_t depth) {
  TRI_ASSERT(_expressions != nullptr);

  auto it = _expressions->find(depth);

  if (it != _expressions->end()) {
    for (auto const& exp : it->second) {
      TRI_ASSERT(exp != nullptr);

      if (exp->isEdgeAccess && !exp->matchesCheck(e)) {
        ++_filteredPaths;
        return false;
      }
    }
  }
  return true;
}

bool DepthFirstTraverser::vertexMatchesConditions(std::string const& v,
                                                  size_t depth) {
  TRI_ASSERT(_expressions != nullptr);

  auto it = _expressions->find(depth);

  if (it != _expressions->end()) {
    bool fetchVertex = true;
    std::shared_ptr<VPackBuffer<uint8_t>> vertex;
    for (auto const& exp : it->second) {
      TRI_ASSERT(exp != nullptr);

      if (!exp->isEdgeAccess) {
        if (fetchVertex) {
          fetchVertex = false;
          auto it = _vertices.find(v);
          if (it == _vertices.end()) {
            OperationResult opRes =
                FetchDocumentById(_trx, v, _builder, _operationOptions);
            ++_readDocuments;
            if (opRes.failed()) {
              TRI_ASSERT(opRes.code == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
              VPackBuilder tmp;
              tmp.add(VPackValue(VPackValueType::Null));
              vertex = tmp.steal();
            } else {
              vertex = opRes.buffer;
            }
              _vertices.emplace(v, vertex);
          } else {
            vertex = it->second;
          }
        }
        if (!exp->matchesCheck(VPackSlice(vertex->data()))) {
          ++_filteredPaths;
          return false;
        }
      }
    }
  }
  return true;
}

void DepthFirstTraverser::_defInternalFunctions() {
  _getVertex = [this](std::string const& edge, std::string const& vertex, size_t depth,
                  std::string& result) -> bool {
    auto const& it = _edges.find(edge);
    TRI_ASSERT(it != _edges.end());
    VPackSlice v(it->second->data());
    // NOTE: We assume that we only have valid edges.
    result = v.get(TRI_VOC_ATTRIBUTE_FROM).copyString();
    if (result == vertex) {
      result = v.get(TRI_VOC_ATTRIBUTE_TO).copyString();
    }
    return true;
  };
}

void DepthFirstTraverser::setStartVertex(
    std::string const& v) {
  TRI_ASSERT(_expressions != nullptr);

  auto it = _expressions->find(0);

  if (it != _expressions->end()) {
    if (!it->second.empty()) {
      std::shared_ptr<VPackBuffer<uint8_t>> vertex;
      bool fetchVertex = true;
      for (auto const& exp : it->second) {
        TRI_ASSERT(exp != nullptr);

        if (!exp->isEdgeAccess) {
          if (fetchVertex) {
            fetchVertex = false;
            OperationResult result = FetchDocumentById(_trx, v, _builder, _operationOptions);
            ++_readDocuments;
            if (result.failed()) {
              // Vertex does not exist
              _done = true;
              return;
            }
            vertex = result.buffer;
            _vertices.emplace(v, vertex);
          }
          if (!exp->matchesCheck(VPackSlice(vertex->data()))) {
            ++_filteredPaths;
            _done = true;
            return;
          }
        }
      }
    }
  }
  _enumerator.reset(new PathEnumerator<std::string, std::string, VPackValueLength>(
      _edgeGetter, _getVertex, v));
  _done = false;
}

TraversalPath* DepthFirstTraverser::next() {
  TRI_ASSERT(!_done);
  if (_pruneNext) {
    _pruneNext = false;
    _enumerator->prune();
  }
  TRI_ASSERT(!_pruneNext);
  EnumeratedPath<std::string, std::string> const& path = _enumerator->next();
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

bool DepthFirstTraverser::EdgeGetter::nextCursor(std::string const& startVertex,
                                                 size_t& eColIdx,
                                                 VPackValueLength*& last) {
  while (true) {
    std::string eColName;
    std::string indexHandle;
    if (last != nullptr) {
      // The cursor is empty clean up
      last = nullptr;
      _posInCursor.pop();
      _cursors.pop();
      _results.pop();
    }
    if (!_opts.getCollectionAndSearchValue(eColIdx, startVertex, eColName, indexHandle,
                                           _builder)) {
      // If we get here there are no valid edges at all
      return false;
    }
    std::shared_ptr<OperationCursor> cursor = _trx->indexScan(
        eColName, arangodb::Transaction::CursorType::INDEX, indexHandle,
        _builder.slice(), 0, UINT64_MAX, TRI_DEFAULT_BATCH_SIZE, false);
    if (cursor->failed()) {
      // Some error, ignore and go to next
      eColIdx++;
      continue;
    }
    TRI_ASSERT(_posInCursor.size() == _cursors.size());
    _cursors.push(cursor);
    _results.emplace(std::make_shared<OperationResult>(TRI_ERROR_NO_ERROR));
    return true;
  }
}

void DepthFirstTraverser::EdgeGetter::nextEdge(
    std::string const& startVertex, size_t& eColIdx, VPackValueLength*& last,
    std::vector<std::string>& edges) {
  auto cursor = _cursors.top();
  auto opRes = _results.top();
  if (last == nullptr) {
    _posInCursor.push(0);
    last = &_posInCursor.top();
  } else {
    ++(*last);
  }
  while (true) {
    VPackSlice edge = opRes->slice();
    if (!edge.isArray() || edge.length() <= *last) {
      if (cursor->hasMore()) {
        // Fetch next and try again
        cursor->getMore(opRes);
        TRI_ASSERT(last != nullptr);
        *last = 0;
        continue;
      }
      eColIdx++;
      if (!nextCursor(startVertex, eColIdx, last)) {
        // No further edges.
        TRI_ASSERT(last == nullptr);
        TRI_ASSERT(_cursors.size() == _posInCursor.size());
        TRI_ASSERT(_cursors.size() == _results.size());
        return;
      }
      // There is a new Cursor on top of the stack, try it
      _posInCursor.push(0);
      last = &_posInCursor.top();
      continue;
    }
    edge = edge.at(*last);
    if (!_traverser->edgeMatchesConditions(edge, edges.size())) {
      ++_traverser->_filteredPaths;
      TRI_ASSERT(last != nullptr);
      (*last)++;
      continue;
    }
    std::string id = _trx->extractIdString(edge);
    VPackBuilder tmpBuilder = VPackBuilder::clone(edge);
    _traverser->_edges.emplace(id, tmpBuilder.steal());
    edges.emplace_back(id);
    return;
  }
}

void DepthFirstTraverser::EdgeGetter::operator()(
    std::string const& startVertex,
    std::vector<std::string>& edges, VPackValueLength*& last, size_t& eColIdx,
    bool& dir) {
  if (last == nullptr) {
    eColIdx = 0;
    if (!nextCursor(startVertex, eColIdx, last)) {
      // We were not able to find any edge
      return;
    }
  }
  nextEdge(startVertex, eColIdx, last, edges);
}
