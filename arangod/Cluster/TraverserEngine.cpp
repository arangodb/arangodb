////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#include "TraverserEngine.h"
#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "Basics/Exceptions.h"
#include "Basics/MemoryTypes/MemoryTypes.h"
#include "Basics/Result.h"
#include "Basics/StaticStrings.h"
#include "Graph/Cursors/EdgeCursor.h"
#include "Graph/EdgeDocumentToken.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserOptions.h"
#include "Transaction/Context.h"
#include "Utils/CollectionNameResolver.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#ifdef USE_ENTERPRISE
#include "Enterprise/Transaction/IgnoreNoAccessMethods.h"
#endif

#include <string_view>

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::traverser;

static const std::string SHARDS = "shards";
static const std::string TYPE = "type";

#ifndef USE_ENTERPRISE
/*static*/ std::unique_ptr<BaseEngine> BaseEngine::buildEngine(
    TRI_vocbase_t& vocbase, aql::QueryContext& query, VPackSlice info) {
  VPackSlice type = info.get(std::initializer_list<std::string_view>(
      {StaticStrings::GraphOptions, TYPE}));

  if (!type.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The body requires an 'options.type' attribute.");
  }

  if (type.isEqualString("traversal")) {
    return std::make_unique<TraverserEngine>(vocbase, query, info);
  } else if (type.isEqualString("shortestPath")) {
    return std::make_unique<ShortestPathEngine>(vocbase, query, info);
  }
  THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                 "The 'options.type' attribute either has to "
                                 "be traversal or shortestPath");
}
#endif

auto EdgeCursorForMultipleVertices::rearm() -> bool {
  if (_nextVertex == _vertices.end()) {
    return false;
  }
  _cursor->rearm(*_nextVertex, _depth);
  _nextVertex++;
  return true;
}
auto EdgeCursorForMultipleVertices::hasMore() -> bool {
  return _cursor->hasMore() || rearm();
}

BaseEngine::BaseEngine(TRI_vocbase_t& vocbase, aql::QueryContext& query,
                       VPackSlice info)
    : _engineId(TRI_NewTickServer()),
      _query(query),
      _vertexShards{_query.resourceMonitor()} {
  VPackSlice shardsSlice = info.get(SHARDS);

  if (!shardsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("The body requires a ", SHARDS, " attribute."));
  }

  VPackSlice edgesSlice = shardsSlice.get(StaticStrings::GraphQueryEdges);

  if (!edgesSlice.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("The ", SHARDS, " object requires an ",
                     StaticStrings::GraphQueryEdges, " attribute."));
  }

  VPackSlice vertexSlice = shardsSlice.get(StaticStrings::GraphQueryVertices);

  if (!vertexSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("The ", SHARDS, " object requires a ",
                     StaticStrings::GraphQueryVertices, " attribute."));
  }

  // Add all Edge shards to the transaction
  TRI_ASSERT(edgesSlice.isArray());
  for (auto shardList : VPackArrayIterator(edgesSlice)) {
    TRI_ASSERT(shardList.isArray());
    for (auto shard : VPackArrayIterator(shardList)) {
      TRI_ASSERT(shard.isString());
      _query.collections().add(shard.copyString(), AccessMode::Type::READ,
                               aql::Collection::Hint::Shard);
    }
  }

  // Add all Vertex shards to the transaction
  TRI_ASSERT(vertexSlice.isObject());
  for (auto collection : VPackObjectIterator(vertexSlice)) {
    ResourceUsageAllocator<MonitoredCollectionToShardMap, ResourceMonitor>
        alloc = {_query.resourceMonitor()};
    MonitoredShardIDVector shards{alloc};
    TRI_ASSERT(collection.value.isArray());
    for (auto shard : VPackArrayIterator(collection.value)) {
      TRI_ASSERT(shard.isString());
      auto shardString = shard.copyString();
      auto maybeShardID = ShardID::shardIdFromString(shardString);
      _query.collections().add(shardString, AccessMode::Type::READ,
                               aql::Collection::Hint::Shard);
      TRI_ASSERT(maybeShardID.ok())
          << "Parsed a list of shards contianing an invalid name: "
          << shardString;
      shards.emplace_back(std::move(maybeShardID.get()));
    }
    _vertexShards.emplace(collection.key.copyString(), std::move(shards));
  }

#ifdef USE_ENTERPRISE
  if (_query.queryOptions().transactionOptions.skipInaccessibleCollections) {
    _trx = std::make_unique<transaction::IgnoreNoAccessMethods>(
        _query.newTrxContext(), _query.queryOptions().transactionOptions);
  }
#endif
  if (_trx == nullptr) {
    _trx = std::make_unique<transaction::Methods>(
        _query.newTrxContext(), _query.queryOptions().transactionOptions);
  }
}

BaseEngine::~BaseEngine() = default;

std::shared_ptr<transaction::Context> BaseEngine::context() const {
  return _trx->transactionContext();
}

void BaseEngine::getVertexData(VPackSlice vertex, VPackBuilder& builder,
                               bool nestedOutput) {
  TRI_ASSERT(ServerState::instance()->isDBServer());
  TRI_ASSERT(vertex.isString() || vertex.isArray());

  std::uint64_t read = 0;
  bool shouldProduceVertices = this->produceVertices();

  auto workOnOneDocument = [&](VPackSlice v) {
    if (v.isNull()) {
      return;
    }

    std::string_view id = v.stringView();
    size_t pos = id.find('/');
    if (pos == std::string::npos || pos + 1 == id.size()) {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_GRAPH_INVALID_EDGE,
          absl::StrCat("edge contains invalid value ", id));
    }
    std::string shardName = std::string(id.substr(0, pos));
    auto shards = _vertexShards.find(shardName);
    if (shards == _vertexShards.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
          absl::StrCat("collection not known to traversal: '", shardName,
                       "'. please add 'WITH ", shardName,
                       "' as the first line in your AQL"));
      // The collection is not known here!
      // Maybe handle differently
    }

    if (!shouldProduceVertices) {
      return;
    }
    std::string_view vertex = id.substr(pos + 1);
    auto cb = [&](LocalDocumentId, aql::DocumentData&&, VPackSlice doc) {
      // FOUND. short circuit.
      read++;
      builder.add(v);
      if (!options().getVertexProjections().empty()) {
        VPackObjectBuilder guard(&builder);
        options().getVertexProjections().toVelocyPackFromDocument(builder, doc,
                                                                  _trx.get());
      } else {
        builder.add(doc);
      }
      return true;
    };
    for (auto const& shard : shards->second) {
      Result res = _trx->documentFastPathLocal(std::string{shard}, vertex, cb)
                       .waitAndGet();
      if (res.ok()) {
        break;
      }
      if (res.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
        // We are in a very bad condition here...
        THROW_ARANGO_EXCEPTION(res);
      }
    }
  };

  builder.openObject();

  if (nestedOutput) {
    builder.add(VPackValue(StaticStrings::GraphQueryVertices));

    if (vertex.isArray()) {
      builder.openArray(true);
    }
  }

  if (vertex.isArray()) {
    for (VPackSlice v : VPackArrayIterator(vertex)) {
      workOnOneDocument(v);
    }
  } else {
    workOnOneDocument(vertex);
  }

  if (nestedOutput) {
    if (vertex.isArray()) {
      builder.close();
    }
    // statistics
    builder.add("readIndex", VPackValue(read));
    builder.add("filtered", VPackValue(0));
    // TODO: wire these counters
    builder.add("cacheHits", VPackValue(0));
    builder.add("cacheMisses", VPackValue(0));
    // intentionally not sending "cursorsCreated" and "cursorsRearmed"
    // here, as we are not using a cursor. the caller can handle that.
  }
  builder.close();
}

BaseTraverserEngine::BaseTraverserEngine(TRI_vocbase_t& vocbase,
                                         aql::QueryContext& query,
                                         VPackSlice info)
    : BaseEngine(vocbase, query, info), _variables(query.ast()->variables()) {}

BaseTraverserEngine::~BaseTraverserEngine() = default;

void BaseTraverserEngine::rearm(size_t depth, uint64_t batchSize,
                                std::vector<std::string> vertices,
                                VPackSlice variables) {
  injectVariables(variables);
  _cursor = EdgeCursorForMultipleVertices{
      _nextCursorId, depth, batchSize, std::move(vertices), getCursor(depth)};
  _nextCursorId++;
}

graph::EdgeCursor* BaseTraverserEngine::getCursor(uint64_t currentDepth) {
  graph::EdgeCursor* cursor = nullptr;
  if (_opts->hasSpecificCursorForDepth(currentDepth)) {
    auto it = _depthSpecificCursors.find(currentDepth);
    if (it == _depthSpecificCursors.end()) {
      it = _depthSpecificCursors
               .emplace(currentDepth, _opts->buildCursor(currentDepth))
               .first;
    }
    TRI_ASSERT(it != _depthSpecificCursors.end());
    cursor = it->second.get();
  } else {
    if (_generalCursor == nullptr) {
      _generalCursor = _opts->buildCursor(currentDepth);
    }
    TRI_ASSERT(_generalCursor != nullptr);
    cursor = _generalCursor.get();
  }
  return cursor;
}

void BaseTraverserEngine::allEdges(std::vector<std::string> const& vertices,
                                   size_t depth, VPackBuilder& builder) {
  auto outputVertex = [this](VPackBuilder& builder, std::string_view vertex,
                             size_t depth) {
    graph::EdgeCursor* cursor = getCursor(depth);
    cursor->rearm(vertex, depth);
    _nextCursorId++;

    cursor->readAll(
        [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorId) {
          if (edge.isString()) {
            edge = _opts->cache()->lookupToken(eid);
          }
          if (edge.isNull()) {
            return;
          }
          if (_opts->evaluateEdgeExpression(edge, vertex, depth, cursorId)) {
            if (!options().getEdgeProjections().empty()) {
              VPackObjectBuilder guard(&builder);
              options().getEdgeProjections().toVelocyPackFromDocument(
                  builder, edge, _trx.get());
            } else {
              builder.add(edge);
            }
          }
        });
  };

  builder.openObject();
  builder.add(VPackValue(StaticStrings::GraphQueryEdges));
  builder.openArray(true);
  for (auto const& v : vertices) {
    outputVertex(builder, v, depth);
  }
  builder.close();
  // statistics
  addAndClearStatistics(builder);
  builder.close();
}

Result BaseTraverserEngine::nextEdgeBatch(size_t batchId,
                                          VPackBuilder& builder) {
  TRI_ASSERT(_cursor.has_value());
  if (_cursor->_nextBatch != batchId) {
    return Result{TRI_ERROR_HTTP_BAD_PARAMETER, ""};
  }
  uint64_t count = 0;
  auto batchSize = _cursor->_batchSize;

  builder.add(VPackValue(StaticStrings::GraphQueryEdges));
  builder.openArray(true);
  while (count != batchSize && _cursor->hasMore()) {
    auto vertex = _cursor->_cursor->currentVertex();
    auto depth = _cursor->_cursor->currentDepth();

    _cursor->_cursor->nextBatch(
        [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorId) {
          if (edge.isString()) {
            edge = _opts->cache()->lookupToken(eid);
          }
          if (edge.isNull()) {
            return;
          }
          if (_opts->evaluateEdgeExpression(edge, *vertex, *depth, cursorId)) {
            if (!options().getEdgeProjections().empty()) {
              VPackObjectBuilder guard(&builder);
              options().getEdgeProjections().toVelocyPackFromDocument(
                  builder, edge, _trx.get());
            } else {
              builder.add(edge);
            }
          }
          count++;
        },
        batchSize);
  }
  builder.close();
  builder.add("done", VPackValue(not _cursor->hasMore()));
  builder.add("cursorId", VPackValue(_cursor->_cursorId));
  builder.add("batchId", VPackValue(_cursor->_nextBatch));

  if (count > 0) {
    _cursor->_nextBatch++;
  }
  return {};
}

void BaseTraverserEngine::addAndClearStatistics(VPackBuilder& builder) {
  // Copy & clear is intentional
  auto stats = *_opts->stats();
  _opts->stats()->clear();
  builder.add("readIndex", VPackValue(stats.getScannedIndex()));
  builder.add("filtered", VPackValue(stats.getFiltered()));
  builder.add("cacheHits", VPackValue(stats.getCacheHits()));
  builder.add("cacheMisses", VPackValue(stats.getCacheMisses()));
  builder.add("cursorsCreated", VPackValue(stats.getCursorsCreated()));
  builder.add("cursorsRearmed", VPackValue(stats.getCursorsRearmed()));
}

bool BaseTraverserEngine::produceVertices() const {
  return _opts->produceVertices();
}

aql::VariableGenerator const* BaseTraverserEngine::variables() const {
  return _variables;
}

graph::BaseOptions const& BaseTraverserEngine::options() const {
  TRI_ASSERT(_opts != nullptr);
  return *_opts;
}

void BaseTraverserEngine::injectVariables(VPackSlice variableSlice) {
  if (variableSlice.isArray()) {
    _opts->clearVariableValues();
    for (auto pair : VPackArrayIterator(variableSlice)) {
      if ((!pair.isArray()) || pair.length() != 2) {
        // Invalid communication. Skip
        TRI_ASSERT(false);
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                       "Internal Traverser communication "
                                       "broken. Failed to inject variables.");
      }
      auto varId =
          arangodb::basics::VelocyPackHelper::getNumericValue<aql::VariableId>(
              pair.at(0), "id", 0);
      aql::Variable* var = variables()->getVariable(varId);
      TRI_ASSERT(var != nullptr);
      // register temporary variables in expression context
      _opts->setVariableValue(
          var, aql::AqlValue{aql::AqlValueHintSliceNoCopy{pair.at(1)}});
    }
    _opts->calculateIndexExpressions(_query.ast());
  }
}

ShortestPathEngine::ShortestPathEngine(TRI_vocbase_t& vocbase,
                                       aql::QueryContext& query,
                                       arangodb::velocypack::Slice info)
    : BaseEngine(vocbase, query, info) {
  VPackSlice optsSlice = info.get(StaticStrings::GraphOptions);
  if (optsSlice.isNone() || !optsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("The body requires an ", StaticStrings::GraphOptions,
                     " attribute."));
  }
  VPackSlice shardsSlice = info.get(SHARDS);
  VPackSlice edgesSlice = shardsSlice.get(StaticStrings::GraphQueryEdges);
  VPackSlice type = optsSlice.get(TYPE);
  if (!type.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("The ", StaticStrings::GraphOptions, " require a ", TYPE,
                     " attribute."));
  }
  TRI_ASSERT(type.isEqualString("shortestPath"));
  _opts = std::make_unique<ShortestPathOptions>(_query, optsSlice, edgesSlice);
  // We create the cache, but we do not need any engines.
  _opts->activateCache(nullptr);

  _forwardCursor = _opts->buildCursor(false);
  _backwardCursor = _opts->buildCursor(true);
}

ShortestPathEngine::~ShortestPathEngine() = default;

void ShortestPathEngine::getEdges(VPackSlice vertex, bool backward,
                                  VPackBuilder& builder) {
  TRI_ASSERT(vertex.isString() || vertex.isArray());

  builder.openObject();
  builder.add(StaticStrings::GraphQueryEdges,
              VPackValue(VPackValueType::Array));
  if (vertex.isArray()) {
    for (VPackSlice v : VPackArrayIterator(vertex)) {
      if (!v.isString()) {
        continue;
      }
      addEdgeData(builder, backward, v.stringView());
      // Result now contains all valid edges, probably multiples.
    }
  } else if (vertex.isString()) {
    addEdgeData(builder, backward, vertex.stringView());
    // Result now contains all valid edges, probably multiples.
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  builder.close();

  // intentional; copy & clear
  auto stats = *_opts->stats();
  _opts->stats()->clear();

  // statistics
  builder.add("readIndex", VPackValue(stats.getScannedIndex()));
  builder.add("filtered", VPackValue(0));
  builder.add("cacheHits", VPackValue(stats.getCacheHits()));
  builder.add("cacheMisses", VPackValue(stats.getCacheMisses()));
  builder.add("cursorsCreated", VPackValue(stats.getCursorsCreated()));
  builder.add("cursorsRearmed", VPackValue(stats.getCursorsRearmed()));

  builder.close();
}

graph::BaseOptions const& ShortestPathEngine::options() const {
  TRI_ASSERT(_opts != nullptr);
  return *_opts;
}

void ShortestPathEngine::addEdgeData(VPackBuilder& builder, bool backward,
                                     std::string_view v) {
  graph::EdgeCursor* cursor =
      backward ? _backwardCursor.get() : _forwardCursor.get();
  cursor->rearm(v, 0);

  cursor->readAll(
      [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t /*cursorId*/) {
        if (edge.isString()) {
          edge = _opts->cache()->lookupToken(eid);
        }
        if (edge.isNull()) {
          return;
        }
        if (!options().getEdgeProjections().empty()) {
          VPackObjectBuilder guard(&builder);
          options().getEdgeProjections().toVelocyPackFromDocument(builder, edge,
                                                                  _trx.get());
        } else {
          builder.add(edge);
        }
      });
}

TraverserEngine::TraverserEngine(TRI_vocbase_t& vocbase,
                                 aql::QueryContext& query,
                                 arangodb::velocypack::Slice info)
    : BaseTraverserEngine(vocbase, query, info) {
  VPackSlice optsSlice = info.get(StaticStrings::GraphOptions);
  if (!optsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("The body requires an ", StaticStrings::GraphOptions,
                     " attribute."));
  }
  VPackSlice shardsSlice = info.get(SHARDS);
  VPackSlice edgesSlice = shardsSlice.get(StaticStrings::GraphQueryEdges);
  VPackSlice type = optsSlice.get(TYPE);
  if (!type.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        absl::StrCat("The ", StaticStrings::GraphOptions, " require a ", TYPE,
                     " attribute."));
  }
  TRI_ASSERT(type.isEqualString("traversal"));
  _opts = std::make_unique<TraverserOptions>(_query, optsSlice, edgesSlice);
  // We create the cache, but we do not need any engines.
  _opts->activateCache(nullptr);
}

TraverserEngine::~TraverserEngine() = default;

void TraverserEngine::smartSearch(VPackSlice, VPackBuilder&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ONLY_ENTERPRISE);
}

void TraverserEngine::smartSearchUnified(VPackSlice, VPackBuilder&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ONLY_ENTERPRISE);
}
