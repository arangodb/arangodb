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

// TODO: these includes are here because of lookupToken below.
//       This method should probably be somewhere else?
#include "VocBase/vocbase.h"
#include "VocBase/LogicalCollection.h"
#include "StorageEngine/PhysicalCollection.h"

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
  if (_nextVertex == _vertices.size()) {
    return false;
  }
  _cursor->rearm(_vertices[_nextVertex], _depth);
  _nextVertex++;
  return true;
}
auto EdgeCursorForMultipleVertices::hasMore() -> bool {
  return _cursor->hasMore() || _nextVertex != _vertices.size();
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

// TODO: lookupToken goes directly to the physical collection to lookup
// an edge document. In the past this function was part of TraverserCache, but
// there was no caching functionality involved whatsoever.
//
// Maybe the TraverserEngine is not the correct place to do this lookup.
VPackSlice BaseEngine::lookupToken(EdgeDocumentToken const& idToken) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());

  auto col = _trx->vocbase().lookupCollection(idToken.cid());

  if (col == nullptr) {
    // collection gone... should not happen
    LOG_TOPIC("3b2ba", ERR, arangodb::Logger::GRAPHS)
        << "Could not extract indexed edge document. collection not found";
    TRI_ASSERT(col != nullptr);  // for maintainer mode
    return arangodb::velocypack::Slice::nullSlice();
  }

  _docBuilder.clear();
  auto cb = IndexIterator::makeDocumentCallback(_docBuilder);
  if (col->getPhysical()
          ->lookup(_trx.get(), idToken.localDocumentId(), cb,
                   {.countBytes = true})
          .fail()) {
    // We already had this token, inconsistent state. Return NULL in Production
    LOG_TOPIC("3acb3", ERR, arangodb::Logger::GRAPHS)
        << "Could not extract indexed edge document, return 'null' instead. "
        << "This is most likely a caching issue. Try: 'db." << col->name()
        << ".unload(); db." << col->name()
        << ".load()' in arangosh to fix this.";
    TRI_ASSERT(false);  // for maintainer mode
    return arangodb::velocypack::Slice::nullSlice();
  }

  return _docBuilder.slice();
}

BaseTraverserEngine::BaseTraverserEngine(TRI_vocbase_t& vocbase,
                                         aql::QueryContext& query,
                                         VPackSlice info)
    : BaseEngine(vocbase, query, info), _variables(query.ast()->variables()) {}

BaseTraverserEngine::~BaseTraverserEngine() = default;

size_t BaseTraverserEngine::createNewCursor(size_t depth, uint64_t batchSize,
                                            std::vector<std::string> vertices,
                                            VPackSlice variables) {
  injectVariables(variables);
  _cursors.emplace_back(EdgeCursorForMultipleVertices{
      _nextCursorId, depth, batchSize, std::move(vertices), getCursor(depth),
      VPackBuilder{variables}});
  auto id = _nextCursorId;
  _nextCursorId++;
  return id;
}

std::unique_ptr<graph::EdgeCursor> BaseTraverserEngine::getCursor(
    uint64_t currentDepth) {
  return _opts->buildCursor(currentDepth);
}

void BaseTraverserEngine::allEdges(std::vector<std::string> const& vertices,
                                   size_t depth, VPackSlice variables,
                                   VPackBuilder& builder) {
  auto outputVertex = [this](VPackBuilder& builder, std::string_view vertex,
                             size_t depth) {
    auto cursor = getCursor(depth);
    cursor->rearm(vertex, depth);
    _nextCursorId++;

    cursor->readAll(
        [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorId) {
          if (edge.isString()) {
            edge = lookupToken(eid);
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

  injectVariables(variables);
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

Result BaseTraverserEngine::nextEdgeBatch(size_t cursorId, size_t batchId,
                                          VPackBuilder& builder) {
  if (_cursors.empty()) {
    return Result{
        TRI_ERROR_HTTP_BAD_PARAMETER,
        fmt::format("cursor id {} does not exist in traverser engine {}",
                    cursorId, engineId())};
  }
  auto& cursor = _cursors.back();
  if (cursorId != cursor._cursorId) {
    return Result{
        TRI_ERROR_HTTP_BAD_PARAMETER,
        fmt::format(
            "cursor id {} is not on top of cursor stack in traverser engine {}",
            cursorId, engineId())};
  }
  if (cursor._nextBatch != batchId) {
    return Result{TRI_ERROR_HTTP_BAD_PARAMETER,
                  fmt::format("batch id {} is not next batch for cursor id {} "
                              "in traverser engine {}",
                              batchId, cursorId, engineId())};
  }
  uint64_t count = 0;
  auto batchSize = cursor._batchSize;

  // TODO not sure if this is necessary here or if it suffices if
  // variables are injected on cursor creation (in
  // BaseTraverserEngine::createNewCursor)
  injectVariables(cursor._variables.slice());

  builder.add(VPackValue(StaticStrings::GraphQueryEdges));
  builder.openArray(true);
  while (count != batchSize && cursor.hasMore()) {
    auto vertex = cursor._cursor->currentVertex();
    auto depth = cursor._cursor->currentDepth();

    cursor._cursor->nextBatch(
        [&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorId) {
          if (edge.isString()) {
            edge = lookupToken(eid);
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
    if (!cursor._cursor->hasMore()) {
      cursor.rearm();
    }
  }
  builder.close();
  builder.add("done", VPackValue(not cursor.hasMore()));
  builder.add("cursorId", VPackValue(cursor._cursorId));
  builder.add("batchId", VPackValue(cursor._nextBatch));

  if (count > 0) {
    cursor._nextBatch++;
  }
  if (not cursor.hasMore()) {
    _cursors.pop_back();
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
          edge = lookupToken(eid);
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
}

TraverserEngine::~TraverserEngine() = default;

void TraverserEngine::smartSearch(VPackSlice, VPackBuilder&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ONLY_ENTERPRISE);
}

void TraverserEngine::smartSearchUnified(VPackSlice, VPackBuilder&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ONLY_ENTERPRISE);
}
