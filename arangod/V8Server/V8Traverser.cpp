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
#include "Basics/StaticStrings.h"
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

using VPackStringHash = arangodb::basics::VelocyPackHelper::VPackStringHash;
using VPackStringEqual = arangodb::basics::VelocyPackHelper::VPackStringEqual;

ShortestPathOptions::ShortestPathOptions(arangodb::Transaction* trx)
    : BasicOptions(trx),
      direction("outbound"),
      useWeight(false),
      weightAttribute(""),
      defaultWeight(1),
      bidirectional(true),
      multiThreaded(true) {
}

void ShortestPathOptions::setStart(std::string const& id) {
  start = id;
  startBuilder.clear();
  startBuilder.add(VPackValue(id));
}

void ShortestPathOptions::setEnd(std::string const& id) {
  end = id;
  endBuilder.clear();
  endBuilder.add(VPackValue(id));
}

VPackSlice ShortestPathOptions::getStart() const {
  return startBuilder.slice();
}

VPackSlice ShortestPathOptions::getEnd() const {
  return endBuilder.slice();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Get a document by it's ID. Also lazy locks the collection.
///        If DOCUMENT_NOT_FOUND this function will return normally
///        with a OperationResult.failed() == true.
///        On all other cases this function throws.
////////////////////////////////////////////////////////////////////////////////

static int FetchDocumentById(arangodb::Transaction* trx,
                             std::string const& id,
                             VPackBuilder& builder,
                             VPackBuilder& result) {
  size_t pos = id.find('/');
  if (pos == std::string::npos) {
    TRI_ASSERT(false);
    return TRI_ERROR_INTERNAL;
  }
  if (id.find('/', pos + 1) != std::string::npos) {
    TRI_ASSERT(false);
    return TRI_ERROR_INTERNAL;
  }

  std::string col = id.substr(0, pos);
  trx->addCollectionAtRuntime(col);
  builder.clear();
  builder.openObject();
  builder.add(StaticStrings::KeyString, VPackValue(id.substr(pos + 1)));
  builder.close();

  int res = trx->documentFastPath(col, builder.slice(), result);

  if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
    THROW_ARANGO_EXCEPTION(res);
  }
  return res;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief Local class to expand edges.
///        Will be handed over to the path finder
////////////////////////////////////////////////////////////////////////////////

struct BasicExpander {
 private:
  std::vector<EdgeCollectionInfo*> const _colls;
  arangodb::Transaction* _trx;
  TRI_edge_direction_e _dir;
  std::vector<TRI_doc_mptr_t*> _cursor;

 public:
  BasicExpander(std::vector<EdgeCollectionInfo*> const& colls,
                arangodb::Transaction* trx, TRI_edge_direction_e dir)
      : _colls(colls),
        _trx(trx),
        _dir(dir) {}

  void operator()(VPackSlice const& v, std::vector<VPackSlice>& res_edges,
                  std::vector<VPackSlice>& neighbors) {
    for (auto const& edgeCollection : _colls) {
      _cursor.clear();
      TRI_ASSERT(edgeCollection != nullptr);
      std::shared_ptr<OperationCursor> edgeCursor = edgeCollection->getEdges(_dir, v);
      while (edgeCursor->hasMore()) {
        edgeCursor->getMoreMptr(_cursor, UINT64_MAX);
        for (auto const& mptr : _cursor) {
          VPackSlice edge(mptr->vpack());
          VPackSlice from = Transaction::extractFromFromDocument(edge);
          if (from == v) {
            VPackSlice to = Transaction::extractToFromDocument(edge);
            if (to != v) {
              res_edges.emplace_back(edge);
              neighbors.emplace_back(to);
            }
          } else {
            res_edges.emplace_back(edge);
            neighbors.emplace_back(from);
          }

        }
      }
    }
  }
};


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
  _searchBuilder.clear();
  EdgeIndex::buildSearchValue(direction, vertexId, _searchBuilder);
  return _trx->indexScan(_collectionName,
                         arangodb::Transaction::CursorType::INDEX, _indexId,
                         _searchBuilder.slice(), 0, UINT64_MAX, 1000, false);
}

std::shared_ptr<OperationCursor> EdgeCollectionInfo::getEdges(
    TRI_edge_direction_e direction, VPackSlice const& vertexId) {
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

  std::function<bool(VPackSlice const&)> _isAllowedVertex;

 public:
  MultiCollectionEdgeExpander(
      TRI_edge_direction_e const& direction,
      std::vector<EdgeCollectionInfo*> const& edgeCollections,
      std::function<bool(VPackSlice const)> isAllowed,
      std::function<bool(VPackSlice const&)> isAllowedVertex)
      : _direction(direction),
        _edgeCollections(edgeCollections),
        _isAllowed(isAllowed),
        _isAllowedVertex(isAllowedVertex) {}

  void operator()(VPackSlice const& source,
                  std::vector<ArangoDBPathFinder::Step*>& result) {
    std::vector<TRI_doc_mptr_t*> cursor;
    for (auto const& edgeCollection : _edgeCollections) {
      TRI_ASSERT(edgeCollection != nullptr);

      auto edgeCursor = edgeCollection->getEdges(_direction, source);
      std::unordered_map<VPackSlice, size_t> candidates;

      auto inserter = [&](VPackSlice const& s, VPackSlice const& t,
                          double currentWeight, VPackSlice edge) {
        if (_isAllowedVertex(t)) {
          auto cand = candidates.find(t);
          if (cand == candidates.end()) {
            // Add weight
            auto step = std::make_unique<ArangoDBPathFinder::Step>(
                t, s, currentWeight, std::move(edge));
            result.emplace_back(step.release());
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
      
      cursor.clear();
      while (edgeCursor->hasMore()) {
        edgeCursor->getMoreMptr(cursor, UINT64_MAX);
        for (auto const& mptr : cursor) {
          VPackSlice edge(mptr->vpack());
          if (!_isAllowed(edge)) {
            continue;
          }
          VPackSlice from = Transaction::extractFromFromDocument(edge);
          VPackSlice to = Transaction::extractToFromDocument(edge);
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

  std::unordered_map<VPackSlice, size_t> _candidates;

 public:
  SimpleEdgeExpander(TRI_edge_direction_e& direction,
                     EdgeCollectionInfo* edgeCollection)
      : _direction(direction), _edgeCollection(edgeCollection) {}

  void operator()(VPackSlice const& source,
                  std::vector<ArangoDBPathFinder::Step*>& result) {
    TRI_ASSERT(_edgeCollection != nullptr);

    _candidates.clear();

    auto inserter = [&](VPackSlice const s, VPackSlice const t,
                        double currentWeight, VPackSlice edge) {
      auto cand = _candidates.find(t);
      if (cand == _candidates.end()) {
        // Add weight
        auto step = std::make_unique<ArangoDBPathFinder::Step>(
            std::move(t), std::move(s), currentWeight, std::move(edge));
        result.emplace_back(step.release());
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
      edgeCursor->getMore(opRes, UINT64_MAX, false);
      if (opRes->failed()) {
        THROW_ARANGO_EXCEPTION(opRes->code);
      }
      VPackSlice edges = opRes->slice();
      for (auto const& edge : VPackArrayIterator(edges)) {
        VPackSlice const from = Transaction::extractFromFromDocument(edge);
        VPackSlice const to = Transaction::extractToFromDocument(edge);
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
      auto matcher = std::make_unique<ExampleMatcher>(
          isolate, v8::Handle<v8::Array>::Cast(example), errorMessage);
      _vertexFilter.emplace(name, matcher.release());
    } else {
      // Has to be Object
      auto matcher = std::make_unique<ExampleMatcher>(
          isolate, v8::Handle<v8::Object>::Cast(example), errorMessage);
      _vertexFilter.emplace(name, matcher.release());
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
    auto matcher = std::make_unique<ExampleMatcher>(
        isolate, v8::Handle<v8::Array>::Cast(example), errorMessage);
    _edgeFilter.emplace(cName, matcher.release());
  } else {
    // Has to be Object
    auto matcher = std::make_unique<ExampleMatcher>(
        isolate, v8::Handle<v8::Object>::Cast(example), errorMessage);
    _edgeFilter.emplace(cName, matcher.release());
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
    auto matcher = std::make_unique<ExampleMatcher>(example, true);
    _edgeFilter.emplace(cName, matcher.release());
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

  std::string id = _trx->extractIdString(edge);
  size_t pos = id.find('/');

  if (pos == std::string::npos) {
    // no / contained in _id!
    return false;
  } 
  if (id.find('/', pos + 1) != std::string::npos) {
    // multiple / contained in _id!
    return false;
  }

  auto it = _edgeFilter.find(id.substr(0, pos));

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

bool NeighborsOptions::matchesVertex(VPackSlice const& id) const {
  if (!useVertexFilter && _explicitCollections.empty()) {
    // Nothing to do
    return true;
  }
  // TODO Optimize
  return matchesVertex(id.copyString());
}

bool NeighborsOptions::matchesVertex(std::string const& id) const {
  if (!useVertexFilter && _explicitCollections.empty()) {
    // Nothing to do
    return true;
  }

  size_t pos = id.find('/');
  if (pos == std::string::npos) {
    TRI_ASSERT(false);
    return false;
  }
  if (id.find('/', pos + 1) != std::string::npos) {
    TRI_ASSERT(false);
    return false;
  }
  
  std::string col = id.substr(0, pos);
  // If there are explicitly marked collections check them.
  if (!_explicitCollections.empty()) {
    // If the current collection is not stored the result is invalid
    if (_explicitCollections.find(col) == _explicitCollections.end()) {
      return false;
    }
  }
  std::string key = id.substr(pos + 1);
  TransactionBuilderLeaser tmp(_trx);
  tmp->openObject();
  tmp->add(StaticStrings::KeyString, VPackValue(key));
  tmp->close();

  TransactionBuilderLeaser result(_trx);
  int res = _trx->documentFastPath(col, tmp->slice(), *(result.get()));
  if (res != TRI_ERROR_NO_ERROR) {
    return false;
  }
  return BasicOptions::matchesVertex(col, key, result->slice());
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

void NeighborsOptions::setStart(std::string const& id) {
  start = id;
  startBuilder.clear();
  startBuilder.add(VPackValue(id));
}

VPackSlice NeighborsOptions::getStart() const {
  return startBuilder.slice();
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

  VPackBuilder tmpBuilder;
  auto vertexFilterClosure = [&opts, &tmpBuilder](VPackSlice const& vertex) -> bool {
    std::string v = vertex.copyString();
    size_t pos = v.find('/');

    if (pos == std::string::npos) {
      // no / contained in _id!
      return false;
    }
    if (v.find('/', pos + 1) != std::string::npos) {
      // multiple / contained in _id!
      return false;
    }
    std::string col = v.substr(0, pos);
    std::string key = v.substr(pos + 1);

    tmpBuilder.clear();
    tmpBuilder.openObject();
    tmpBuilder.add(StaticStrings::KeyString, VPackValue(key));
    tmpBuilder.close();
    OperationOptions opOpts;
    OperationResult opRes = opts.trx()->document(col, tmpBuilder.slice(), opOpts);
    if (opRes.failed()) {
      return false;
    }
    return opts.matchesVertex(col, key, opRes.slice());
  };

  MultiCollectionEdgeExpander forwardExpander(
      forward, collectionInfos, edgeFilterClosure, vertexFilterClosure);
  MultiCollectionEdgeExpander backwardExpander(
      backward, collectionInfos, edgeFilterClosure, vertexFilterClosure);

  ArangoDBPathFinder pathFinder(forwardExpander, backwardExpander,
                                opts.bidirectional);
  std::unique_ptr<ArangoDBPathFinder::Path> path;
  // New trx api is not thread safe. Two threads only give little performance
  // gain. Maybe reactivate this in the future (MVCC).
  opts.multiThreaded = false;
  VPackSlice start = opts.getStart();
  VPackSlice end = opts.getEnd();
  if (opts.multiThreaded) {
    path.reset(pathFinder.shortestPathTwoThreads(start, end));
  } else {
    path.reset(pathFinder.shortestPath(start, end));
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

  auto fwExpander = BasicExpander(collectionInfos, trx, forward);
  auto bwExpander = BasicExpander(collectionInfos, trx, backward);

  ArangoDBConstDistancePathFinder pathFinder(fwExpander, bwExpander);
  VPackSlice start = opts.getStart();
  VPackSlice end = opts.getEnd();

  auto path = std::unique_ptr<ArangoDBConstDistancePathFinder::Path>(
      pathFinder.search(start, end));
  return path;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief search for distinct inbound neighbors
////////////////////////////////////////////////////////////////////////////////

static void InboundNeighbors(std::vector<EdgeCollectionInfo*> const& collectionInfos,
                             NeighborsOptions const& opts,
                             std::vector<VPackSlice> const& startVertices,
                             std::unordered_set<VPackSlice, VPackStringHash, VPackStringEqual>& visited,
                             std::vector<VPackSlice>& distinct,
                             uint64_t depth = 1) {
  TRI_edge_direction_e dir = TRI_EDGE_IN;
  std::vector<VPackSlice> nextDepth;

  std::vector<TRI_doc_mptr_t*> cursor;
  for (auto const& col : collectionInfos) {
    TRI_ASSERT(col != nullptr);

    for (auto const& start : startVertices) {
      cursor.clear();
      auto edgeCursor = col->getEdges(dir, start);
      while (edgeCursor->hasMore()) {
        edgeCursor->getMoreMptr(cursor, UINT64_MAX);
        for (auto const& mptr : cursor) {
          VPackSlice edge(mptr->vpack());
          if (opts.matchesEdge(edge)) {
            VPackSlice v = Transaction::extractFromFromDocument(edge);
            if (visited.find(v) != visited.end()) {
              // We have already visited this vertex
              continue;
            }
            if (depth >= opts.minDepth) {
              if (opts.matchesVertex(v)) {
                distinct.emplace_back(v);
              }
            }
            if (depth < opts.maxDepth) {
              nextDepth.emplace_back(v);
            }
            visited.emplace(v);
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

static void OutboundNeighbors(std::vector<EdgeCollectionInfo*> const& collectionInfos,
                              NeighborsOptions const& opts,
                              std::vector<VPackSlice> const& startVertices,
                              std::unordered_set<VPackSlice, VPackStringHash, VPackStringEqual>& visited,
                              std::vector<VPackSlice>& distinct,
                              uint64_t depth = 1) {
  TRI_edge_direction_e dir = TRI_EDGE_OUT;
  std::vector<VPackSlice> nextDepth;
  std::vector<TRI_doc_mptr_t*> cursor;

  for (auto const& col : collectionInfos) {
    TRI_ASSERT(col != nullptr);

    for (auto const& start : startVertices) {
      cursor.clear();
      auto edgeCursor = col->getEdges(dir, start);
      while (edgeCursor->hasMore()) {
        edgeCursor->getMoreMptr(cursor, UINT64_MAX);
        for (auto const& mptr : cursor) {
          VPackSlice edge(mptr->vpack());
          if (opts.matchesEdge(edge)) {
            VPackSlice v = Transaction::extractToFromDocument(edge);
            if (visited.find(v) != visited.end()) {
              // We have already visited this vertex
              continue;
            }
            if (depth >= opts.minDepth) {
              if (opts.matchesVertex(v)) {
                distinct.emplace_back(v);
              }
            }
            if (depth < opts.maxDepth) {
              nextDepth.emplace_back(v);
            }
            visited.emplace(std::move(v));
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

static void AnyNeighbors(std::vector<EdgeCollectionInfo*> const& collectionInfos,
                         NeighborsOptions const& opts,
                         std::vector<VPackSlice> const& startVertices,
                         std::unordered_set<VPackSlice, VPackStringHash, VPackStringEqual>& visited,
                         std::vector<VPackSlice>& distinct,
                         uint64_t depth = 1) {

  TRI_edge_direction_e dir = TRI_EDGE_ANY;
  std::vector<VPackSlice> nextDepth;
  std::vector<TRI_doc_mptr_t*> cursor;

  for (auto const& col : collectionInfos) {
    TRI_ASSERT(col != nullptr);

    for (auto const& start : startVertices) {
      cursor.clear();
      auto edgeCursor = col->getEdges(dir, start);
      while (edgeCursor->hasMore()) {
        edgeCursor->getMoreMptr(cursor, UINT64_MAX);
        for (auto const& mptr : cursor) {
          VPackSlice edge(mptr->vpack());
          if (opts.matchesEdge(edge)) {
            VPackSlice v = Transaction::extractToFromDocument(edge);
            if (visited.find(v) == visited.end()) {
              if (depth >= opts.minDepth) {
                if (opts.matchesVertex(v)) {
                  distinct.emplace_back(v);
                }
              }
              if (depth < opts.maxDepth) {
                nextDepth.emplace_back(v);
              }
              visited.emplace(std::move(v));
              continue;
            }
            v = Transaction::extractFromFromDocument(edge);
            if (visited.find(v) == visited.end()) {
              if (depth >= opts.minDepth) {
                if (opts.matchesVertex(v)) {
                  distinct.emplace_back(v);
                }
              }
              if (depth < opts.maxDepth) {
                nextDepth.emplace_back(v);
              }
              visited.emplace(v);
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

void TRI_RunNeighborsSearch(std::vector<EdgeCollectionInfo*> const& collectionInfos,
                            NeighborsOptions const& opts,
                            std::unordered_set<VPackSlice, VPackStringHash, VPackStringEqual>& visited,
                            std::vector<VPackSlice>& result) {
  std::vector<VPackSlice> startVertices;
  startVertices.emplace_back(opts.getStart());
  visited.emplace(opts.getStart());

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
  int res = FetchDocumentById(trx, identifier, _searchBuilder, result);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }
}

void SingleServerTraversalPath::pathToVelocyPack(Transaction* trx,
                                                 VPackBuilder& result) {
  result.openObject();
  result.add(VPackValue("edges"));
  result.openArray();
  for (auto const& it : _path.edges) {
    auto cached = _traverser->_edges.find(it);
    // All edges are cached!!
    TRI_ASSERT(cached != _traverser->_edges.end());
    result.add(VPackSlice(cached->second->data()));
  }
  result.close();
  result.add(VPackValue("vertices"));
  result.openArray();
  for (auto const& it : _path.vertices) {
    std::shared_ptr<VPackBuffer<uint8_t>> vertex =
        _traverser->fetchVertexData(it);
    result.add(VPackSlice(vertex->data()));
  }
  result.close();
  result.close();
}

void SingleServerTraversalPath::lastEdgeToVelocyPack(Transaction* trx, VPackBuilder& result) {
  auto cached = _traverser->_edges.find(_path.edges.back());
  // All edges are cached!!
  TRI_ASSERT(cached != _traverser->_edges.end());
  result.add(VPackSlice(cached->second->data()));
}

void SingleServerTraversalPath::lastVertexToVelocyPack(Transaction* trx, VPackBuilder& result) {
  std::shared_ptr<VPackBuffer<uint8_t>> vertex =
      _traverser->fetchVertexData(_path.vertices.back());
  result.add(VPackSlice(vertex->data()));
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

      if (exp->isEdgeAccess && !exp->matchesCheck(_trx, e)) {
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
          vertex = fetchVertexData(v);
        }
        if (!exp->matchesCheck(_trx, VPackSlice(vertex->data()))) {
          ++_filteredPaths;
          return false;
        }
      }
    }
  }
  return true;
}

std::shared_ptr<VPackBuffer<uint8_t>> DepthFirstTraverser::fetchVertexData(
    std::string const& id) {
  auto it = _vertices.find(id);
  if (it == _vertices.end()) {
    VPackBuilder tmp;
    int res = FetchDocumentById(_trx, id, _builder, tmp);
    ++_readDocuments;
    if (res != TRI_ERROR_NO_ERROR) {
      TRI_ASSERT(res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND);
      tmp.add(VPackValue(VPackValueType::Null));
      return tmp.steal();
    }
    auto shared_buffer = tmp.steal();
    _vertices.emplace(id, shared_buffer);
    return shared_buffer;
  }
  return it->second;
}

void DepthFirstTraverser::_defInternalFunctions() {
  _getVertex = [this](std::string const& edge, std::string const& vertex, size_t depth,
                  std::string& result) -> bool {
    auto const& it = _edges.find(edge);
    TRI_ASSERT(it != _edges.end());
    VPackSlice v(it->second->data());
    // NOTE: We assume that we only have valid edges.
    result = Transaction::extractFromFromDocument(v).copyString();
    // result = v.get(StaticStrings::FromString).copyString();
    if (result == vertex) {
      result = Transaction::extractToFromDocument(v).copyString();
      // result = v.get(StaticStrings::ToString).copyString();
    }
    return true;
  };
}

void DepthFirstTraverser::setStartVertex(std::string const& v) {
  _pruneNext = false;

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
            VPackBuilder tmp;
            int result = FetchDocumentById(_trx, v, _builder, tmp);
            ++_readDocuments;
            if (result != TRI_ERROR_NO_ERROR) {
              // Vertex does not exist
              _done = true;
              return;
            }
            vertex = tmp.steal();
            _vertices.emplace(v, vertex);
          }
          if (!exp->matchesCheck(_trx, VPackSlice(vertex->data()))) {
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
  if (path.vertices.empty()) {
    _done = true;
    // Done traversing
    return nullptr;
  }
  size_t countEdges = path.edges.size();

  auto p = std::make_unique<SingleServerTraversalPath>(path, this);
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
    arangodb::Transaction::IndexHandle indexHandle;
    if (last != nullptr) {
      // The cursor is empty clean up
      last = nullptr;
      TRI_ASSERT(!_posInCursor.empty());
      TRI_ASSERT(!_cursors.empty());
      TRI_ASSERT(!_results.empty());
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
        _builder.slice(), 0, UINT64_MAX, Transaction::defaultBatchSize(), false);
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
  if (last == nullptr) {
    _posInCursor.push(0);
    last = &_posInCursor.top();
  } else {
    ++(*last);
  }
  while (true) {
    TRI_ASSERT(!_cursors.empty());
    auto cursor = _cursors.top();
    TRI_ASSERT(!_results.empty());
    auto opRes = _results.top();
    TRI_ASSERT(opRes != nullptr);
    // note: we need to check *first* that there is actually something in the buffer
    // before we access its internals. otherwise, the buffer contents are uninitialized
    // and non-deterministic behavior will be the consequence
    VPackSlice edge = opRes->slice();
    if (opRes->buffer->empty() || !edge.isArray() || edge.length() <= *last) {
      if (cursor->hasMore()) {
        // Fetch next and try again
        cursor->getMore(opRes);
        TRI_ASSERT(last != nullptr);
        *last = 0;
        edge = opRes->slice();
        TRI_ASSERT(edge.isArray());
        _traverser->_readDocuments += static_cast<size_t>(edge.length());
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
      TRI_ASSERT(last != nullptr);
      (*last)++;
      continue;
    }
    std::string id = _trx->extractIdString(edge);
    // test if edge is already on this path
    auto found = std::find(edges.begin(), edges.end(), id);
    if (found != edges.end()) {
      // This edge is already on the path, next
      TRI_ASSERT(last != nullptr);
      (*last)++;
      continue;
    }

    VPackBuilder tmpBuilder = VPackBuilder::clone(edge);
    _traverser->_edges.emplace(id, tmpBuilder.steal());

    std::string other;
    _traverser->_getVertex(id, startVertex, 0, other);
    if (!_traverser->vertexMatchesConditions(other, edges.size() + 1)) {
      // Retry with the next element
      TRI_ASSERT(last != nullptr);
      (*last)++;
      continue;
    }

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
