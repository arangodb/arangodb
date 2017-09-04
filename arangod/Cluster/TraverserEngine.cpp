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

#include "TraverserEngine.h"
#include "Aql/AqlTransaction.h"
#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "Aql/QueryString.h"
#include "Basics/Exceptions.h"
#include "Graph/EdgeCursor.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/TraverserCache.h"
#include "Transaction/Context.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/ManagedDocumentResult.h"
#include "VocBase/TraverserOptions.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

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

BaseEngine::BaseEngine(TRI_vocbase_t* vocbase, VPackSlice info)
    : _query(nullptr), _trx(nullptr), _collections(vocbase) {
  VPackSlice shardsSlice = info.get(SHARDS);
  if (shardsSlice.isNone() || !shardsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The body requires a " + SHARDS + " attribute.");
  }

  VPackSlice edgesSlice = shardsSlice.get(EDGES);

  if (edgesSlice.isNone() || !edgesSlice.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The " + SHARDS + " object requires an " + EDGES + " attribute.");
  }

  VPackSlice vertexSlice = shardsSlice.get(VERTICES);

  if (vertexSlice.isNone() || !vertexSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The " + SHARDS + " object requires a " + VERTICES + " attribute.");
  }

  // Add all Edge shards to the transaction
  for (VPackSlice const shardList : VPackArrayIterator(edgesSlice)) {
    TRI_ASSERT(shardList.isArray());
    for (VPackSlice const shard : VPackArrayIterator(shardList)) {
      TRI_ASSERT(shard.isString());
      _collections.add(shard.copyString(), AccessMode::Type::READ);
    }
  }

  // Add all Vertex shards to the transaction
  for (auto const& collection : VPackObjectIterator(vertexSlice)) {
    std::vector<std::string> shards;
    for (VPackSlice const shard : VPackArrayIterator(collection.value)) {
      TRI_ASSERT(shard.isString());
      std::string name = shard.copyString();
      _collections.add(name, AccessMode::Type::READ);
      shards.emplace_back(std::move(name));
    }
    _vertexShards.emplace(collection.key.copyString(), shards);
  }

  // FIXME: in the future this needs to be replaced with the new cluster
  // wide transactions
  transaction::Options trxOpts;
#ifdef USE_ENTERPRISE
  if (info.hasKey(INACCESSIBLE)) {
    trxOpts.skipInaccessibleCollections = true;
    std::unordered_set<ShardID> inaccessible;
    for (VPackSlice const& shard : VPackArrayIterator(info.get(INACCESSIBLE))) {
      TRI_ASSERT(shard.isString());
      inaccessible.insert(shard.copyString());
    }
    _trx = aql::AqlTransaction::create(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        _collections.collections(), trxOpts, true, inaccessible);
  } else {
    _trx = aql::AqlTransaction::create(
        arangodb::transaction::StandaloneContext::Create(vocbase),
        _collections.collections(), trxOpts, true);
  }
#else
  _trx = aql::AqlTransaction::create(
       arangodb::transaction::StandaloneContext::Create(vocbase),
       _collections.collections(), trxOpts, true);
#endif

  // true here as last argument is crucial: it leads to the fact that the
  // created transaction is considered a "MAIN" part and will not switch
  // off collection locking completely!
  auto params = std::make_shared<VPackBuilder>();
  auto opts = std::make_shared<VPackBuilder>();
  _query = new aql::Query(false, vocbase, aql::QueryString(), params, opts,
                          aql::PART_DEPENDENT);
  _query->injectTransaction(_trx);

  VPackSlice variablesSlice = info.get(VARIABLES);
  if (!variablesSlice.isNone()) {
    if (!variablesSlice.isArray()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "The optional " + VARIABLES + " has to be an array.");
    }
    for (auto v : VPackArrayIterator(variablesSlice)) {
      _query->ast()->variables()->createVariable(v);
    }
  }

  _trx->begin();  // We begin the transaction before we lock.
                  // We also setup indexes before we lock.
}

BaseEngine::~BaseEngine() {
  if (_trx) {
    try {
      _trx->commit();
    } catch (...) {
      // If we could not commit
      // we are in a bad state.
      // This is a READ-ONLY trx
    }
  }
  delete _query;
}

bool BaseEngine::lockCollection(std::string const& shard) {
  auto resolver = _trx->resolver();
  TRI_voc_cid_t cid = resolver->getCollectionIdLocal(shard);
  if (cid == 0) {
    return false;
  }
  _trx->pinData(cid);  // will throw when it fails
  Result res = _trx->lock(_trx->trxCollection(cid), AccessMode::Type::READ);
  if (!res.ok()) {
    LOG_TOPIC(ERR, arangodb::Logger::FIXME)
        << "Logging Shard " << shard << " lead to exception '"
        << res.errorNumber() << "' (" << res.errorMessage() << ") ";
    return false;
  }
  return true;
}

std::shared_ptr<transaction::Context> BaseEngine::context() const {
  return _trx->transactionContext();
}

void BaseEngine::getVertexData(VPackSlice vertex, VPackBuilder& builder) {
  // We just hope someone has locked the shards properly. We have no clue...
  // Thanks locking
  TRI_ASSERT(ServerState::instance()->isDBServer());
  TRI_ASSERT(vertex.isString() || vertex.isArray());
  
  ManagedDocumentResult mmdr;
  builder.openObject();
  auto workOnOneDocument = [&](VPackSlice v) {
    StringRef id(v);
    size_t pos = id.find('/');
    if (pos == std::string::npos || pos + 1 == id.size()) {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_EDGE,
                                     "edge contains invalid value " + id.toString());
    }
    ShardID shardName = id.substr(0, pos).toString();
    auto shards = _vertexShards.find(shardName);
    if (shards == _vertexShards.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
                                     "collection not known to traversal: '" +
                                     shardName + "'. please add 'WITH " + shardName +
                                     "' as the first line in your AQL");
      // The collection is not known here!
      // Maybe handle differently
    }
    
    StringRef vertex = id.substr(pos+1);
    for (std::string const& shard : shards->second) {
      Result res = _trx->documentFastPathLocal(shard, vertex, mmdr, false);
      if (res.ok()) {
        // FOUND short circuit.
        builder.add(v);
        mmdr.addToBuilder(builder, false);
        break;
      } else if (res.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
        // We are in a very bad condition here...
        THROW_ARANGO_EXCEPTION(res);
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

BaseTraverserEngine::BaseTraverserEngine(TRI_vocbase_t* vocbase,
                                         VPackSlice info)
    : BaseEngine(vocbase, info), _opts(nullptr) {}

BaseTraverserEngine::~BaseTraverserEngine() {}

void BaseTraverserEngine::getEdges(VPackSlice vertex, size_t depth,
                                   VPackBuilder& builder) {
  // We just hope someone has locked the shards properly. We have no clue...
  // Thanks locking
  TRI_ASSERT(vertex.isString() || vertex.isArray());
  ManagedDocumentResult mmdr;
  builder.openObject();
  builder.add(VPackValue("edges"));
  builder.openArray();
  if (vertex.isArray()) {
    for (VPackSlice v : VPackArrayIterator(vertex)) {
      TRI_ASSERT(v.isString());
      // result.clear();
      StringRef vertexId(v);
      std::unique_ptr<arangodb::graph::EdgeCursor> edgeCursor(
          _opts->nextCursor(&mmdr, vertexId, depth));

      edgeCursor->readAll([&](std::unique_ptr<EdgeDocumentToken>&& eid,
                              VPackSlice edge, size_t cursorId) {
        if (edge.isString()) {
          edge = _opts->cache()->lookupToken(eid.get());
        }
        if (_opts->evaluateEdgeExpression(edge, StringRef(v), depth,
                                          cursorId)) {
          builder.add(edge);
        }
      });
      // Result now contains all valid edges, probably multiples.
    }
  } else if (vertex.isString()) {
    std::unique_ptr<arangodb::graph::EdgeCursor> edgeCursor(
        _opts->nextCursor(&mmdr, StringRef(vertex), depth));
    edgeCursor->readAll([&](std::unique_ptr<EdgeDocumentToken>&& eid,
                            VPackSlice edge, size_t cursorId) {
      if (edge.isString()) {
        edge = _opts->cache()->lookupToken(eid.get());
      }
      if (_opts->evaluateEdgeExpression(edge, StringRef(vertex), depth,
                                        cursorId)) {
        builder.add(edge);
      }
    });
    // Result now contains all valid edges, probably multiples.
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  builder.close();
  builder.add("readIndex",
              VPackValue(_opts->cache()->getAndResetInsertedDocuments()));
  builder.add("filtered",
              VPackValue(_opts->cache()->getAndResetFilteredDocuments()));
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
    StringRef id(v);
    size_t pos = id.find('/');
    if (pos == std::string::npos || pos + 1 == id.size()) {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_GRAPH_INVALID_EDGE,
                                     "edge contains invalid value " + id.toString());
    }
    ShardID shardName = id.substr(0, pos).toString();
    auto shards = _vertexShards.find(shardName);
    if (shards == _vertexShards.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
                                     "collection not known to traversal: '" +
                                     shardName + "'. please add 'WITH " + shardName +
                                     "' as the first line in your AQL");
      // The collection is not known here!
      // Maybe handle differently
    }
    
    StringRef vertex = id.substr(pos+1);
    for (std::string const& shard : shards->second) {
      Result res = _trx->documentFastPathLocal(shard, vertex, mmdr, false);
      if (res.ok()) {
        // FOUND short circuit.
        read++;
        builder.add(v);
        mmdr.addToBuilder(builder, false);
        break;
      } else if (res.isNot(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
        // We are in a very bad condition here...
        THROW_ARANGO_EXCEPTION(res);
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

ShortestPathEngine::ShortestPathEngine(TRI_vocbase_t* vocbase,
                                       arangodb::velocypack::Slice info)
    : BaseEngine(vocbase, info) {
  VPackSlice optsSlice = info.get(OPTIONS);
  if (optsSlice.isNone() || !optsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The body requires an " + OPTIONS + " attribute.");
  }
  VPackSlice shardsSlice = info.get(SHARDS);
  VPackSlice edgesSlice = shardsSlice.get(EDGES);
  VPackSlice type = optsSlice.get(TYPE);
  if (!type.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The " + OPTIONS + " require a " + TYPE + " attribute.");
  }
  TRI_ASSERT(type.isEqualString("shortestPath"));
  _opts.reset(new ShortestPathOptions(_query, optsSlice, edgesSlice));
  TRI_ASSERT(_opts != nullptr);
  if (_opts == nullptr) {
    // It seems we could not generate the options
    // without throwing an error. Must by OOM.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  // We create the cache, but we do not need any engines.
  _opts->activateCache(false, nullptr);
}

ShortestPathEngine::~ShortestPathEngine() {}

void ShortestPathEngine::getEdges(VPackSlice vertex, bool backward,
                                  VPackBuilder& builder) {
  // We just hope someone has locked the shards properly. We have no clue...
  // Thanks locking
  TRI_ASSERT(vertex.isString() || vertex.isArray());

  std::unique_ptr<arangodb::graph::EdgeCursor> edgeCursor;

  ManagedDocumentResult mmdr;
  builder.openObject();
  builder.add(VPackValue("edges"));
  builder.openArray();
  if (vertex.isArray()) {
    for (VPackSlice v : VPackArrayIterator(vertex)) {
      TRI_ASSERT(v.isString());
      // result.clear();
      StringRef vertexId(v);
      if (backward) {
        edgeCursor.reset(_opts->nextReverseCursor(&mmdr, vertexId));
      } else {
        edgeCursor.reset(_opts->nextCursor(&mmdr, vertexId));
      }

      edgeCursor->readAll([&](std::unique_ptr<EdgeDocumentToken>&& eid,
                              VPackSlice edge, size_t cursorId) {
        if (edge.isString()) {
          edge = _opts->cache()->lookupToken(eid.get());
        }
        builder.add(edge);
      });
      // Result now contains all valid edges, probably multiples.
    }
  } else if (vertex.isString()) {
    StringRef vertexId(vertex);
    if (backward) {
      edgeCursor.reset(_opts->nextReverseCursor(&mmdr, vertexId));
    } else {
      edgeCursor.reset(_opts->nextCursor(&mmdr, vertexId));
    }
    edgeCursor->readAll([&](std::unique_ptr<EdgeDocumentToken>&& eid,
                            VPackSlice edge, size_t cursorId) {
      if (edge.isString()) {
        edge = _opts->cache()->lookupToken(eid.get());
      }
      builder.add(edge);
    });
    // Result now contains all valid edges, probably multiples.
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  builder.close();
  builder.add("readIndex",
              VPackValue(_opts->cache()->getAndResetInsertedDocuments()));
  builder.add("filtered", VPackValue(0));
  builder.close();
}

TraverserEngine::TraverserEngine(TRI_vocbase_t* vocbase,
                                 arangodb::velocypack::Slice info)
    : BaseTraverserEngine(vocbase, info) {
  VPackSlice optsSlice = info.get(OPTIONS);
  if (!optsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The body requires an " + OPTIONS + " attribute.");
  }
  VPackSlice shardsSlice = info.get(SHARDS);
  VPackSlice edgesSlice = shardsSlice.get(EDGES);
  VPackSlice type = optsSlice.get(TYPE);
  if (!type.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The " + OPTIONS + " require a " + TYPE + " attribute.");
  }
  TRI_ASSERT(type.isEqualString("traversal"));
  _opts.reset(new TraverserOptions(_query, optsSlice, edgesSlice));
  TRI_ASSERT(_opts != nullptr);
  if (_opts == nullptr) {
    // It seems we could not generate the options
    // without throwing an error. Must by OOM.
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
  // We create the cache, but we do not need any engines.
  _opts->activateCache(false, nullptr);
}

TraverserEngine::~TraverserEngine() {}

void TraverserEngine::smartSearch(VPackSlice, VPackBuilder&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ONLY_ENTERPRISE);
}

void TraverserEngine::smartSearchBFS(VPackSlice, VPackBuilder&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ONLY_ENTERPRISE);
}
