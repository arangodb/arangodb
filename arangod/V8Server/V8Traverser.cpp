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
#include "Utils/CollectionNameResolver.h"
#include "Utils/OperationCursor.h"
#include "Utils/SingleCollectionTransaction.h"
#include "VocBase/SingleServerTraverser.h"
#include "VocBase/document-collection.h"
#include "VocBase/EdgeCollectionInfo.h"

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
/// @brief search for distinct inbound neighbors
////////////////////////////////////////////////////////////////////////////////

static void InboundNeighbors(std::vector<EdgeCollectionInfo*> const& collectionInfos,
                             NeighborsOptions const& opts,
                             std::vector<VPackSlice> const& startVertices,
                             std::unordered_set<VPackSlice, VPackStringHash, VPackStringEqual>& visited,
                             std::vector<VPackSlice>& distinct,
                             uint64_t depth = 1) {
  std::vector<VPackSlice> nextDepth;

  std::vector<TRI_doc_mptr_t*> cursor;
  for (auto const& col : collectionInfos) {
    TRI_ASSERT(col != nullptr);

    for (auto const& start : startVertices) {
      cursor.clear();
      auto edgeCursor = col->getEdges(start);
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
  std::vector<VPackSlice> nextDepth;
  std::vector<TRI_doc_mptr_t*> cursor;

  for (auto const& col : collectionInfos) {
    TRI_ASSERT(col != nullptr);

    for (auto const& start : startVertices) {
      cursor.clear();
      auto edgeCursor = col->getEdges(start);
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

  std::vector<VPackSlice> nextDepth;
  std::vector<TRI_doc_mptr_t*> cursor;

  for (auto const& col : collectionInfos) {
    TRI_ASSERT(col != nullptr);

    for (auto const& start : startVertices) {
      cursor.clear();
      auto edgeCursor = col->getEdges(start);
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

