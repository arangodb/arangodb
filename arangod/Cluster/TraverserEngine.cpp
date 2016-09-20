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

#include "Basics/Exceptions.h"
#include "Aql/Ast.h"
#include "Aql/Query.h"
#include "Utils/AqlTransaction.h"
#include "Utils/CollectionNameResolver.h"
#include "VocBase/TraverserOptions.h"

#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::traverser;

static const std::string OPTIONS = "options";
static const std::string SHARDS = "shards";
static const std::string EDGES = "edges";
static const std::string VARIABLES = "variables";
static const std::string VERTICES = "vertices";

BaseTraverserEngine::BaseTraverserEngine(TRI_vocbase_t* vocbase,
                                         arangodb::velocypack::Slice info)
    : _opts(nullptr), _query(nullptr), _trx(nullptr), _collections(vocbase) {
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
        "The " + SHARDS + " object requires an " + VERTICES + " attribute.");
  }

  // Add all Edge shards to the transaction
  for (VPackSlice const shardList : VPackArrayIterator(edgesSlice)) {
    TRI_ASSERT(shardList.isArray());
    for (VPackSlice const shard : VPackArrayIterator(shardList)) {
      TRI_ASSERT(shard.isString());
      _collections.add(shard.copyString(), TRI_TRANSACTION_READ); 
    }
  }

  // Add all Vertex shards to the transaction
  for (auto const& collection : VPackObjectIterator(vertexSlice)) {
    std::vector<std::string> shards;
    for (VPackSlice const shard : VPackArrayIterator(collection.value)) {
      TRI_ASSERT(shard.isString());
      std::string name = shard.copyString();
      _collections.add(name, TRI_TRANSACTION_READ); 
      shards.emplace_back(std::move(name));
    }
    _vertexShards.emplace(collection.key.copyString(), shards);
  }

  _trx = new arangodb::AqlTransaction(
      arangodb::StandaloneTransactionContext::Create(vocbase),
      _collections.collections(), false);
  auto params = std::make_shared<VPackBuilder>();
  auto opts = std::make_shared<VPackBuilder>();
  _query = new aql::Query(true, vocbase, "", 0, params, opts, aql::PART_DEPENDENT);
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


  _trx->begin(); // We begin the transaction before we lock.
                 // We also setup indexes before we lock.
}

BaseTraverserEngine::~BaseTraverserEngine() {
  /*
  auto resolver = _trx->resolver();
  // TODO Do we need this or will delete trx do this already?
  for (auto const& shard : _locked) {
    TRI_voc_cid_t cid = resolver->getCollectionIdLocal(shard);
    if (cid == 0) {
      LOG(ERR) << "Failed to unlock shard " << shard << ": not found";
      continue;
    }
    int res = _trx->unlock(_trx->trxCollection(cid), TRI_TRANSACTION_READ);
    if (res != TRI_ERROR_NO_ERROR) {
      LOG(ERR) << "Failed to unlock shard " << shard << ": "
               << TRI_errno_string(res);
    }
  }
  */
  if (_trx) {
    _trx->commit();
  }
  if (_query != nullptr) {
    delete _query;
  }
}

void BaseTraverserEngine::getEdges(VPackSlice vertex, size_t depth, VPackBuilder& builder) {
  // We just hope someone has locked the shards properly. We have no clue... Thanks locking

  TRI_ASSERT(vertex.isString() || vertex.isArray());
  size_t cursorId = 0;
  size_t read = 0;
  size_t filtered = 0;
  std::vector<VPackSlice> result;
  builder.openObject();
  builder.add(VPackValue("edges"));
  builder.openArray();
  if (vertex.isArray()) {
    for (VPackSlice v : VPackArrayIterator(vertex)) {
      TRI_ASSERT(v.isString());
      result.clear();
      auto edgeCursor = _opts->nextCursor(v, depth);
      while(edgeCursor->next(result, cursorId)) {
        if (!_opts->evaluateEdgeExpression(result.back(), v, depth, cursorId)) {
          filtered++;
          result.pop_back();
        }
      }
      for (auto const& it : result) {
        builder.add(it);
      }
      // Result now contains all valid edges, probably multiples.
    }
  } else if (vertex.isString()) {
    std::unique_ptr<arangodb::traverser::EdgeCursor> edgeCursor(_opts->nextCursor(vertex, depth));

    while(edgeCursor->next(result, cursorId)) {
      if (!_opts->evaluateEdgeExpression(result.back(), vertex, depth, cursorId)) {
        filtered++;
        result.pop_back();
      }
    }
    for (auto const& it : result) {
      builder.add(it);
    }
    // Result now contains all valid edges, probably multiples.
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  builder.close();
  builder.add("readIndex", VPackValue(read));
  builder.add("filtered", VPackValue(filtered));
  builder.close();
}

void BaseTraverserEngine::getVertexData(VPackSlice vertex, VPackBuilder& builder) {
  // We just hope someone has locked the shards properly. We have no clue...
  // Thanks locking
  TRI_ASSERT(vertex.isString() || vertex.isArray());
  builder.openObject();
  bool found;
  auto workOnOneDocument = [&](VPackSlice v) {
    found = false;
    StringRef id(v);
    std::string name = id.substr(0, id.find('/')).toString();
    auto shards = _vertexShards.find(name);
    if (shards == _vertexShards.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
                                     "Collection not known to Traversal " +
                                         name + " please add 'WITH " + name +
                                         "' as the first line in your AQL");
      // The collection is not known here!
      // Maybe handle differently
    }
    builder.add(v);
    for (std::string const& shard : shards->second) {
      int res = _trx->documentFastPath(shard, v, builder, false);
      if (res == TRI_ERROR_NO_ERROR) {
        found = true;
        // FOUND short circuit.
        break;
      }
      if (res != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // We are in a very bad condition here...
        THROW_ARANGO_EXCEPTION(res);
      }
    }
    if (!found) {
      builder.add(arangodb::basics::VelocyPackHelper::NullValue());
      builder.removeLast();
    }
  };

  if (vertex.isArray()) {
    for (VPackSlice v : VPackArrayIterator(vertex)) {
      workOnOneDocument(v);
    }
  } else {
    workOnOneDocument(vertex);
  }
  builder.close(); // The outer object
}

void BaseTraverserEngine::getVertexData(VPackSlice vertex, size_t depth,
                                    VPackBuilder& builder) {
  // We just hope someone has locked the shards properly. We have no clue...
  // Thanks locking
  TRI_ASSERT(vertex.isString() || vertex.isArray());
  size_t read = 0;
  size_t filtered = 0;
  bool found = false;
  builder.openObject();
  builder.add(VPackValue("vertices"));

  auto workOnOneDocument = [&](VPackSlice v) {
    found = false;
    StringRef id(v);
    std::string name = id.substr(0, id.find('/')).toString();
    auto shards = _vertexShards.find(name);
    if (shards == _vertexShards.end()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
                                     "Collection not known to Traversal " +
                                         name + " please add 'WITH " + name +
                                         "' as the first line in your AQL");
    }
    builder.add(v);
    for (std::string const& shard : shards->second) {
      int res = _trx->documentFastPath(shard, v, builder, false);
      if (res == TRI_ERROR_NO_ERROR) {
        read++;
        found = true;
        // FOUND short circuit.
        break;
      }
      if (res != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
        // We are in a very bad condition here...
        THROW_ARANGO_EXCEPTION(res);
      }
    }
    // TODO FILTERING!
    // HOWTO Distinguish filtered vs NULL?
    if (!found) {
      builder.removeLast();
    }
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
  builder.add("filtered", VPackValue(filtered));
  builder.close();
}

bool BaseTraverserEngine::lockCollection(std::string const& shard) {
  if (_locked.find(shard) != _locked.end()) {
    return false;
  }
  auto resolver = _trx->resolver();
  TRI_voc_cid_t cid = resolver->getCollectionIdLocal(shard);
  if (cid == 0) {
    return false;
  }
  _trx->orderDitch(cid); // will throw when it fails 
  int res = _trx->lock(_trx->trxCollection(cid), TRI_TRANSACTION_READ);
  if (res != TRI_ERROR_NO_ERROR) {
    LOG(ERR) << "Logging Shard " << shard << " lead to exception '"
             << TRI_errno_string(res) << "' (" << res << ") ";
    return false;
  }
  return true;
}

std::shared_ptr<arangodb::TransactionContext> BaseTraverserEngine::context() const {
  return _trx->transactionContext();
}

TraverserEngine::TraverserEngine(TRI_vocbase_t* vocbase,
                                 arangodb::velocypack::Slice info)
    : BaseTraverserEngine(vocbase, info) {
  VPackSlice optsSlice = info.get(OPTIONS);
  if (optsSlice.isNone() || !optsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The body requires a " + OPTIONS + " attribute.");
  }
  VPackSlice shardsSlice = info.get(SHARDS);
  VPackSlice edgesSlice = shardsSlice.get(EDGES);

  _opts.reset(new TraverserOptions(_query, optsSlice, edgesSlice));
}


TraverserEngine::~TraverserEngine() {
}

void TraverserEngine::smartSearch(VPackSlice, VPackBuilder&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ONLY_ENTERPRISE);
}

void TraverserEngine::smartSearchBFS(VPackSlice, VPackBuilder&) {
  THROW_ARANGO_EXCEPTION(TRI_ERROR_ONLY_ENTERPRISE);
}
