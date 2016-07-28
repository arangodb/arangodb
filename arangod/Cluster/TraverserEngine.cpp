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
#include "Aql/Query.h"
#include "Utils/AqlTransaction.h"
#include "VocBase/Traverser.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::traverser;

static const std::string OPTIONS = "options";
static const std::string SHARDS = "shards";
static const std::string EDGES = "edges";
static const std::string VERTICES = "vertices";
static const std::string TOLOCK = "tolock";

TraverserEngine::TraverserEngine(TRI_vocbase_t* vocbase,
                                 arangodb::velocypack::Slice info)
    : _opts(nullptr),
      _query(nullptr),
      _trx(nullptr),
      _collections(vocbase),
      _didLock(false) {
  VPackSlice optsSlice = info.get(OPTIONS);
  if (optsSlice.isNone() || !optsSlice.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The body requires a " + OPTIONS + " attribute.");
  }

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

  if (vertexSlice.isNone() || !vertexSlice.isArray()) {
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
  for (VPackSlice const shard : VPackArrayIterator(vertexSlice)) {
    TRI_ASSERT(shard.isString());
    std::string name = shard.copyString();
    _collections.add(name, TRI_TRANSACTION_READ); 
    _vertexShards.emplace_back(std::move(name));
  }

  VPackSlice toLockSlice = shardsSlice.get(TOLOCK);

  if (toLockSlice.isNone() || !toLockSlice.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "The " + SHARDS + " object requires an " + TOLOCK + " attribute.");
  }

  // List all that we have to lock
  for (VPackSlice const shard : VPackArrayIterator(toLockSlice)) {
    TRI_ASSERT(shard.isString());
    _toLock.emplace_back(shard.copyString());
  }
  _trx = new arangodb::AqlTransaction(
      arangodb::StandaloneTransactionContext::Create(vocbase),
      _collections.collections(), false);
  auto params = std::make_shared<VPackBuilder>();
  auto opts = std::make_shared<VPackBuilder>();
  _query = new aql::Query(true, vocbase, "", 0, params, opts, aql::PART_DEPENDENT);
  _query->injectTransaction(_trx);
  _opts.reset(new TraverserOptions(_query, optsSlice, edgesSlice));
}

TraverserEngine::~TraverserEngine() {
}

void TraverserEngine::getEdges(VPackSlice vertex, size_t depth, VPackBuilder& builder) {
  if (!_didLock) {
    // We try to read the edges before the collections are locked. Invalid access
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED);
  }
  TRI_ASSERT(vertex.isString() || vertex.isArray());
  size_t cursorId = 0;
  size_t read = 0;
  size_t filtered = 0;
  std::vector<VPackSlice> result;
  builder.openObject();
  if (vertex.isArray()) {
    builder.add(VPackValue("edges"));
    builder.openArray();
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
      builder.openArray();
      for (auto const& it : result) {
        builder.add(it);
      }
      builder.close();
      // Result now contains all valid edges, probably multiples.
    }
    builder.close();
  } else if (vertex.isString()) {
    auto edgeCursor = _opts->nextCursor(vertex, depth);
    while(edgeCursor->next(result, cursorId)) {
      if (!_opts->evaluateEdgeExpression(result.back(), vertex, depth, cursorId)) {
        filtered++;
        result.pop_back();
      }
    }
    builder.add(VPackValue("edges"));
    builder.openArray();
    for (auto const& it : result) {
      builder.add(it);
    }
    builder.close();
    // Result now contains all valid edges, probably multiples.
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  builder.add("readIndex", VPackValue(read));
  builder.add("filtered", VPackValue(filtered));
  builder.close();
}

void TraverserEngine::getVertexData(VPackSlice vertex, size_t depth, VPackBuilder& builder) {
  if (!_didLock) {
    // We try to read the edges before the collections are locked. Invalid access
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED);
  }
  // TODO use external or internal builder for result
  // Alternative: Dump directly into body.
  TRI_ASSERT(vertex.isString() || vertex.isArray());
  size_t read = 0;
  size_t filtered = 0;
  int res = TRI_ERROR_NO_ERROR;
  bool found = false;
  builder.openObject();
  builder.add(VPackValue("vertices"));
  if (vertex.isArray()) {
    builder.openArray();
    for (VPackSlice v : VPackArrayIterator(vertex)) {
      found = false;
      for (std::string const& shard : _vertexShards) {
        res = _trx->documentFastPath(shard, v, builder);
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
        builder.add(arangodb::basics::VelocyPackHelper::NullValue());
      }
    }
    builder.close();
  } else if (vertex.isString()) {
    found = false;
    for (std::string const& shard : _vertexShards) {
      res = _trx->documentFastPath(shard, vertex, builder);
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
      builder.add(arangodb::basics::VelocyPackHelper::NullValue());
    }
  } else {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_BAD_PARAMETER);
  }
  builder.add("readIndex", VPackValue(read));
  builder.add("filtered", VPackValue(filtered));
  builder.close();
}

void TraverserEngine::lockCollections() {
  auto resolver = _trx->resolver();
  for (auto const& it : _vertexShards) {
    TRI_voc_cid_t cid = resolver->getCollectionIdLocal(it);
    if (cid == 0) {
      THROW_ARANGO_EXCEPTION_FORMAT(TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND,
                                    "'%s'", it.c_str());
    }
    _trx->orderDitch(cid); // will throw when it fails 
    int res = _trx->lock(_trx->trxCollection(cid), TRI_TRANSACTION_READ);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
  }
  _didLock = true;
}
