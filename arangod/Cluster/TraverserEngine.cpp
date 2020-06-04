////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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

#include "TraverserEngine.h"
#include "Aql/AqlTransaction.h"
#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Basics/Exceptions.h"
#include "Basics/Result.h"
#include "Graph/EdgeCursor.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserOptions.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Transaction/Context.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

#ifdef USE_ENTERPRISE
#include "Enterprise/Transaction/IgnoreNoAccessMethods.h"
#endif

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::traverser;

static const std::string OPTIONS = "options";
static const std::string INACCESSIBLE = "inaccessible";
static const std::string SHARDS = "shards";
static const std::string EDGES = "edges";
static const std::string TYPE = "type";
static const std::string VARIABLES = "variables";
static const std::string VERTICES = "vertices";

#ifndef USE_ENTERPRISE
/*static*/ std::unique_ptr<BaseEngine> BaseEngine::BuildEngine(
    TRI_vocbase_t& vocbase, aql::QueryContext& query,
    VPackSlice info) {
  VPackSlice type = info.get(std::vector<std::string>({OPTIONS, TYPE}));

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

BaseEngine::BaseEngine(TRI_vocbase_t& vocbase,
                       aql::QueryContext& query,
                       VPackSlice info)
    : _query(query), _trx(nullptr) {
  VPackSlice shardsSlice = info.get(SHARDS);

  if (shardsSlice.isNone() || !shardsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The body requires a " + SHARDS +
                                       " attribute.");
  }

  VPackSlice edgesSlice = shardsSlice.get(EDGES);

  if (edgesSlice.isNone() || !edgesSlice.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The " + SHARDS + " object requires an " +
                                       EDGES + " attribute.");
  }

  VPackSlice vertexSlice = shardsSlice.get(VERTICES);

  if (vertexSlice.isNone() || !vertexSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The " + SHARDS + " object requires a " +
                                       VERTICES + " attribute.");
  }

  // Add all Edge shards to the transaction
  TRI_ASSERT(edgesSlice.isArray());
  for (VPackSlice const shardList : VPackArrayIterator(edgesSlice)) {
    TRI_ASSERT(shardList.isArray());
    for (VPackSlice const shard : VPackArrayIterator(shardList)) {
      TRI_ASSERT(shard.isString());
      _query.collections().add(shard.copyString(), AccessMode::Type::READ, aql::Collection::Hint::Collection);
    }
  }

  // Add all Vertex shards to the transaction
  TRI_ASSERT(vertexSlice.isObject());
  for (auto const& collection : VPackObjectIterator(vertexSlice)) {
    std::vector<std::string> shards;
    TRI_ASSERT(collection.value.isArray());
    for (VPackSlice const shard : VPackArrayIterator(collection.value)) {
      TRI_ASSERT(shard.isString());
      std::string name = shard.copyString();
      _query.collections().add(name, AccessMode::Type::READ, aql::Collection::Hint::Shard);
      shards.emplace_back(std::move(name));
    }
    _vertexShards.try_emplace(collection.key.copyString(), std::move(shards));
  }

#ifdef USE_ENTERPRISE
  if (_query.queryOptions().transactionOptions.skipInaccessibleCollections) {
    _trx = new transaction::IgnoreNoAccessMethods(_query.newTrxContext(), _query.queryOptions().transactionOptions);
  } else {
    _trx = new transaction::Methods(_query.newTrxContext(), _query.queryOptions().transactionOptions);
  }
#else
  _trx = new transaction::Methods(_query.newTrxContext(), _query.queryOptions().transactionOptions);
#endif
}

BaseEngine::~BaseEngine() {
  delete _trx;
}

std::shared_ptr<transaction::Context> BaseEngine::context() const {
  return _trx->transactionContext();
}

void BaseEngine::getVertexData(VPackSlice vertex, VPackBuilder& builder) {
  // We just hope someone has locked the shards properly. We have no clue...
  // Thanks locking
  TRI_ASSERT(ServerState::instance()->isDBServer());
  TRI_ASSERT(vertex.isString() || vertex.isArray());

  bool const shouldProduceVertices = this->produceVertices(); 
  ManagedDocumentResult mmdr;
  builder.openObject();
  auto workOnOneDocument = [&](VPackSlice v) {
    arangodb::velocypack::StringRef id(v);
    size_t pos = id.find('/');
    if (pos == std::string::npos || pos + 1 == id.size()) {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_EDGE,
                                     "edge contains invalid value " + id.toString());
    }
    std::string shardName = id.substr(0, pos).toString();
    auto shards = _vertexShards.find(shardName);
    if (shards == _vertexShards.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
                                     "collection not known to traversal: '" +
                                         shardName + "'. please add 'WITH " + shardName +
                                         "' as the first line in your AQL");
      // The collection is not known here!
      // Maybe handle differently
    }

    if (shouldProduceVertices) {
      arangodb::velocypack::StringRef vertex = id.substr(pos + 1);
      for (std::string const& shard : shards->second) {
        Result res = _trx->documentFastPathLocal(shard, vertex, mmdr);
        if (res.ok()) {
          // FOUND short circuit.
          builder.add(v);
          mmdr.addToBuilder(builder);
          break;
        } else if (res.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
          // We are in a very bad condition here...
          THROW_ARANGO_EXCEPTION(res);
        }
      }
    }
  };

  if (vertex.isArray()) {
    for (VPackSlice v : VPackArrayIterator(vertex)) {
      workOnOneDocument(v);
    }
  } else {
    workOnOneDocument(vertex);
  }
  builder.close();  // The outer object
}

BaseTraverserEngine::BaseTraverserEngine(TRI_vocbase_t& vocbase,
                                         aql::QueryContext& query,
                                         VPackSlice info)
    : BaseEngine(vocbase, query, info), _variables(query.ast()->variables()) {}

BaseTraverserEngine::~BaseTraverserEngine() = default;

graph::EdgeCursor* BaseTraverserEngine::getCursor(arangodb::velocypack::StringRef nextVertex, uint64_t currentDepth) {
  if (currentDepth >= _cursors.size()) {
    _cursors.emplace_back(_opts->buildCursor(currentDepth));
  }
  graph::EdgeCursor* cursor = _cursors.at(currentDepth).get();
  cursor->rearm(nextVertex, currentDepth);
  return cursor;
}

void BaseTraverserEngine::getEdges(VPackSlice vertex, size_t depth, VPackBuilder& builder) {
  // We just hope someone has locked the shards properly. We have no clue...
  // Thanks locking
    
  auto outputVertex = [this, depth](VPackBuilder& builder, VPackSlice vertex) {
    TRI_ASSERT(vertex.isString());

    graph::EdgeCursor* cursor = getCursor(arangodb::velocypack::StringRef(vertex), depth);

    cursor->readAll([&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorId) {
      if (edge.isString()) {
        edge = _opts->cache()->lookupToken(eid);
      }
      if (edge.isNull()) {
        return;
      }
      if (_opts->evaluateEdgeExpression(edge, arangodb::velocypack::StringRef(vertex), depth, cursorId)) {
        builder.add(edge);
      }
    });
  };

  TRI_ASSERT(vertex.isString() || vertex.isArray());
  builder.openObject();
  builder.add("edges", VPackValue(VPackValueType::Array));
  if (vertex.isArray()) {
    for (VPackSlice v : VPackArrayIterator(vertex)) {
      outputVertex(builder, v);
    }
  } else if (vertex.isString()) {
    outputVertex(builder, vertex);
    // Result now contains all valid edges, probably multiples.
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  builder.close();
  builder.add("readIndex", VPackValue(_opts->cache()->getAndResetInsertedDocuments()));
  builder.add("filtered", VPackValue(_opts->cache()->getAndResetFilteredDocuments()));
  builder.close();
}

void BaseTraverserEngine::getVertexData(VPackSlice vertex, size_t depth,
                                        VPackBuilder& builder) {
  // We just hope someone has locked the shards properly. We have no clue...
  // Thanks locking
  TRI_ASSERT(ServerState::instance()->isDBServer());
  TRI_ASSERT(vertex.isString() || vertex.isArray());

  size_t read = 0;
  ManagedDocumentResult mmdr;
  builder.openObject();
  builder.add(VPackValue("vertices"));

  auto workOnOneDocument = [&](VPackSlice v) {
    if (v.isNull()) {
      return;
    }
    arangodb::velocypack::StringRef id(v);
    size_t pos = id.find('/');
    if (pos == std::string::npos || pos + 1 == id.size()) {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_EDGE,
                                     "edge contains invalid value " + id.toString());
    }

    std::string shardName = id.substr(0, pos).toString();
    auto shards = _vertexShards.find(shardName);
    if (shards == _vertexShards.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
                                     "collection not known to traversal: '" +
                                         shardName + "'. please add 'WITH " + shardName +
                                         "' as the first line in your AQL");
      // The collection is not known here!
      // Maybe handle differently
    }

    if (_opts->produceVertices()) {
      arangodb::velocypack::StringRef vertex = id.substr(pos + 1);
      for (std::string const& shard : shards->second) {
        Result res = _trx->documentFastPathLocal(shard, vertex, mmdr);
        if (res.ok()) {
          // FOUND short circuit.
          read++;
          builder.add(v);
          mmdr.addToBuilder(builder);
          break;
        } else if (res.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
          // We are in a very bad condition here...
          THROW_ARANGO_EXCEPTION(res);
        }
      }
    }

    // TODO FILTERING!
    // HOWTO Distinguish filtered vs NULL?
  };

  if (vertex.isArray()) {
    builder.openArray();
    for (VPackSlice v : VPackArrayIterator(vertex)) {
      workOnOneDocument(v);
    }
    builder.close();
  } else {
    workOnOneDocument(vertex);
  }
  builder.add("readIndex", VPackValue(read));
  builder.add("filtered", VPackValue(0));
  builder.close();
}
  
bool BaseTraverserEngine::produceVertices() const {
  return _opts->produceVertices();
}

aql::VariableGenerator const* BaseTraverserEngine::variables() const {
  return _variables;
}

void BaseTraverserEngine::injectVariables(VPackSlice variableSlice) {
  if (variableSlice.isArray()) {
    _opts->clearVariableValues();
    for (auto const& pair : VPackArrayIterator(variableSlice)) {
      if ((!pair.isArray()) || pair.length() != 2) {
        // Invalid communication. Skip
        TRI_ASSERT(false);
        continue;
      }
      auto varId =
          arangodb::basics::VelocyPackHelper::getNumericValue<aql::VariableId>(pair.at(0),
                                                                          "id", 0);
      aql::Variable* var = variables()->getVariable(varId);
      TRI_ASSERT(var != nullptr);
      aql::AqlValue val(pair.at(1).start());
      _opts->setVariableValue(var, val);
    }
  }
}

ShortestPathEngine::ShortestPathEngine(TRI_vocbase_t& vocbase,
                                       aql::QueryContext& query,
                                       arangodb::velocypack::Slice info)
    : BaseEngine(vocbase, query, info) {

  VPackSlice optsSlice = info.get(OPTIONS);
  if (optsSlice.isNone() || !optsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The body requires an " + OPTIONS +
                                       " attribute.");
  }
  VPackSlice shardsSlice = info.get(SHARDS);
  VPackSlice edgesSlice = shardsSlice.get(EDGES);
  VPackSlice type = optsSlice.get(TYPE);
  if (!type.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The " + OPTIONS + " require a " + TYPE +
                                       " attribute.");
  }
  TRI_ASSERT(type.isEqualString("shortestPath"));
  _opts.reset(new ShortestPathOptions(_query, optsSlice, edgesSlice));
  // We create the cache, but we do not need any engines.
  _opts->activateCache(false, nullptr);
  
  _forwardCursor = _opts->buildCursor(false);
  _backwardCursor = _opts->buildCursor(true);
}

ShortestPathEngine::~ShortestPathEngine() = default;

void ShortestPathEngine::getEdges(VPackSlice vertex, bool backward, VPackBuilder& builder) {
  // We just hope someone has locked the shards properly. We have no clue...
  // Thanks locking
  TRI_ASSERT(vertex.isString() || vertex.isArray());

  builder.openObject();
  builder.add("edges", VPackValue(VPackValueType::Array));
  if (vertex.isArray()) {
    for (VPackSlice v : VPackArrayIterator(vertex)) {
      if (!v.isString()) {
        continue;
      }
      addEdgeData(builder, backward, arangodb::velocypack::StringRef(v));
      // Result now contains all valid edges, probably multiples.
    }
  } else if (vertex.isString()) {
    addEdgeData(builder, backward, arangodb::velocypack::StringRef(vertex));
    // Result now contains all valid edges, probably multiples.
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  builder.close();
  builder.add("readIndex", VPackValue(_opts->cache()->getAndResetInsertedDocuments()));
  builder.add("filtered", VPackValue(0));
  builder.close();
}

void ShortestPathEngine::addEdgeData(VPackBuilder& builder, bool backward, arangodb::velocypack::StringRef v) {
  graph::EdgeCursor* cursor = backward ? _backwardCursor.get() : _forwardCursor.get();
  cursor->rearm(v, 0);

  cursor->readAll([&](EdgeDocumentToken&& eid, VPackSlice edge, size_t cursorId) {
    if (edge.isString()) {
      edge = _opts->cache()->lookupToken(eid);
    }
    if (edge.isNull()) {
      return;
    }
    builder.add(edge);
  });
}

TraverserEngine::TraverserEngine(TRI_vocbase_t& vocbase,
                                 aql::QueryContext& query,
                                 arangodb::velocypack::Slice info)
    : BaseTraverserEngine(vocbase, query, info) {
  VPackSlice optsSlice = info.get(OPTIONS);
  if (!optsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The body requires an " + OPTIONS +
                                       " attribute.");
  }
  VPackSlice shardsSlice = info.get(SHARDS);
  VPackSlice edgesSlice = shardsSlice.get(EDGES);
  VPackSlice type = optsSlice.get(TYPE);
  if (!type.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The " + OPTIONS + " require a " + TYPE +
                                       " attribute.");
  }
  TRI_ASSERT(type.isEqualString("traversal"));
  _opts.reset(new TraverserOptions(_query, optsSlice, edgesSlice));
  // We create the cache, but we do not need any engines.
  _opts->activateCache(false, nullptr);
}

TraverserEngine::~TraverserEngine() = default;

void TraverserEngine::smartSearch(VPackSlice, VPackBuilder&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ONLY_ENTERPRISE);
}

void TraverserEngine::smartSearchBFS(VPackSlice, VPackBuilder&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ONLY_ENTERPRISE);
}
