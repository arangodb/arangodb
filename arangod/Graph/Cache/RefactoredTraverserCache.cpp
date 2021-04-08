////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#include "RefactoredTraverserCache.h"

#include "Aql/AqlValue.h"
#include "Aql/Query.h"
#include "Aql/TraversalStats.h"
#include "Basics/StringHeap.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Graph/BaseOptions.h"
#include "Graph/EdgeDocumentToken.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "StorageEngine/PhysicalCollection.h"
#include "Transaction/Methods.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

RefactoredTraverserCache::RefactoredTraverserCache(
    arangodb::transaction::Methods* trx, aql::QueryContext* query,
    arangodb::ResourceMonitor& resourceMonitor, arangodb::aql::TraversalStats& stats,
    std::map<std::string, std::string> const& collectionToShardMap)
    : _query(query),
      _trx(trx),
      _stringHeap(resourceMonitor, 4096), /* arbitrary block-size may be adjusted for performance */
      _collectionToShardMap(collectionToShardMap),
      _resourceMonitor(resourceMonitor) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
}

RefactoredTraverserCache::~RefactoredTraverserCache() { clear(); }

void RefactoredTraverserCache::clear() {
  _resourceMonitor.decreaseMemoryUsage(
      _persistedStrings.size() * sizeof(arangodb::velocypack::HashedStringRef));
  _persistedStrings.clear();
  _stringHeap.clear();
}

template <typename ResultType>
bool RefactoredTraverserCache::appendEdge(EdgeDocumentToken const& idToken,
                                          ResultType& result) {
  auto col = _trx->vocbase().lookupCollection(idToken.cid());

  if (ADB_UNLIKELY(col == nullptr)) {
    // collection gone... should not happen
    LOG_TOPIC("c4d78", ERR, arangodb::Logger::GRAPHS)
        << "Could not extract indexed edge document. collection not found";
    TRI_ASSERT(col != nullptr);  // for maintainer mode
    return false;
  }

  auto res = col->getPhysical()->read(
      _trx, idToken.localDocumentId(), [&](LocalDocumentId const&, VPackSlice edge) -> bool {
        // NOTE: Do not count this as Primary Index Scan, we counted it in the
        // edge Index before copying...
        if constexpr (std::is_same_v<ResultType, aql::AqlValue>) {
          result = aql::AqlValue(edge);
        } else if constexpr (std::is_same_v<ResultType, velocypack::Builder>) {
          result.add(edge);
        }
        return true;
      });
  if (ADB_UNLIKELY(!res)) {
    // We already had this token, inconsistent state. Return NULL in Production
    LOG_TOPIC("daac5", ERR, arangodb::Logger::GRAPHS)
        << "Could not extract indexed edge document, return 'null' instead. "
        << "This is most likely a caching issue. Try: 'db." << col->name()
        << ".unload(); db." << col->name() << ".load()' in arangosh to fix this.";
  }
  return res;
}

ResultT<std::pair<std::string, size_t>> RefactoredTraverserCache::extractCollectionName(
    velocypack::HashedStringRef const& idHashed) const {
  size_t pos = idHashed.find('/');
  if (pos == std::string::npos) {
    // Invalid input. If we get here somehow we managed to store invalid
    // _from/_to values or the traverser did a let an illegal start through
    TRI_ASSERT(false);
    return Result{TRI_ERROR_GRAPH_INVALID_EDGE,
                  "edge contains invalid value " + idHashed.toString()};
  }

  std::string colName = idHashed.substr(0, pos).toString();
  if (_collectionToShardMap.empty()) {
    TRI_ASSERT(!ServerState::instance()->isDBServer());
    return std::make_pair(colName, pos);
  }
  auto it = _collectionToShardMap.find(colName);
  if (it == _collectionToShardMap.end()) {
    // Connected to a vertex where we do not know the Shard to.
    return Result{TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
                  "collection not known to traversal: '" + colName +
                      "'. please add 'WITH " + colName +
                      "' as the first line in your AQL"};
  }
  // We have translated to a Shard
  return std::make_pair(it->second, pos);
}

template <typename ResultType>
bool RefactoredTraverserCache::appendVertex(aql::TraversalStats& stats,
                                            velocypack::HashedStringRef const& id,
                                            ResultType& result) {
  auto collectionNameResult = extractCollectionName(id);
  if (collectionNameResult.fail()) {
    THROW_ARANGO_EXCEPTION(collectionNameResult.result());
  }

  Result res = _trx->documentFastPathLocal(
      collectionNameResult.get().first,
      id.substr(collectionNameResult.get().second + 1).stringRef(),
      [&](LocalDocumentId const&, VPackSlice doc) -> bool {
        stats.addScannedIndex(1);
        // copying...
        if constexpr (std::is_same_v<ResultType, aql::AqlValue>) {
          result = aql::AqlValue(doc);
        } else if constexpr (std::is_same_v<ResultType, velocypack::Builder>) {
          result.add(doc);
        }
        return true;
      });
  if (res.ok()) {
    return true;
  }

  if (!res.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
    // ok we are in a rather bad state. Better throw and abort.
    THROW_ARANGO_EXCEPTION(res);
  }

  // Register a warning. It is okay though but helps the user
  std::string msg = "vertex '" + id.toString() + "' not found";
  _query->warnings().registerWarning(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, msg.c_str());
  // This is expected, we may have dangling edges. Interpret as NULL
  return false;
}

void RefactoredTraverserCache::insertEdgeIntoResult(EdgeDocumentToken const& idToken,
                                                    VPackBuilder& builder) {
  if (!appendEdge(idToken, builder)) {
    builder.add(VPackSlice::nullSlice());
  }
}

void RefactoredTraverserCache::insertVertexIntoResult(aql::TraversalStats& stats,
                                                      arangodb::velocypack::HashedStringRef const& idString,
                                                      VPackBuilder& builder) {
  if (!appendVertex(stats, idString, builder)) {
    builder.add(VPackSlice::nullSlice());
  }
}

arangodb::velocypack::HashedStringRef RefactoredTraverserCache::persistString(
    arangodb::velocypack::HashedStringRef idString) {
  auto it = _persistedStrings.find(idString);
  if (it != _persistedStrings.end()) {
    return *it;
  }
  auto res = _stringHeap.registerString(idString);
  _persistedStrings.emplace(res);
  _resourceMonitor.increaseMemoryUsage(sizeof(res));
  return res;
}
