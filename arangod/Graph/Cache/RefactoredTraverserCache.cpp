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

#include "RefactoredTraverserCache.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Aql/AqlValue.h"
#include "Aql/Query.h"
#include "Aql/TraversalStats.h"
#include "Basics/StaticStrings.h"
#include "Basics/StringHeap.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ServerState.h"
#include "Graph/BaseOptions.h"
#include "Graph/EdgeDocumentToken.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/QueryRegistryFeature.h"
#include "StorageEngine/PhysicalCollection.h"
#include "StorageEngine/TransactionState.h"
#include "Transaction/Helpers.h"
#include "Transaction/Methods.h"
#include "Transaction/Options.h"
#include "VocBase/LogicalCollection.h"

#include <absl/strings/str_cat.h>
#include <velocypack/Builder.h>
#include <velocypack/HashedStringRef.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::graph;

namespace {
bool isWithClauseMissing(arangodb::basics::Exception const& ex) {
  if (ServerState::instance()->isDBServer() &&
      ex.code() == TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND) {
    // on a DB server, we could have got here only in the OneShard case.
    // in this case turn the rather misleading "collection or view not found"
    // error into a nicer "collection not known to traversal, please add WITH"
    // message, so users know what to do
    return true;
  }
  if (ServerState::instance()->isSingleServer() &&
      ex.code() == TRI_ERROR_TRANSACTION_UNREGISTERED_COLLECTION) {
    return true;
  }

  return false;
}
}  // namespace

RefactoredTraverserCache::RefactoredTraverserCache(
    transaction::Methods* trx, aql::QueryContext* query,
    ResourceMonitor& resourceMonitor, aql::TraversalStats& stats,
    MonitoredCollectionToShardMap const& collectionToShardMap,
    arangodb::aql::Projections const& vertexProjections,
    arangodb::aql::Projections const& edgeProjections, bool produceVertices)
    : _query(query),
      _trx(trx),
      _stringHeap(
          resourceMonitor,
          4096), /* arbitrary block-size may be adjusted for performance */
      _collectionToShardMap(collectionToShardMap),
      _resourceMonitor(resourceMonitor),
      _produceVertices(produceVertices),
      _allowImplicitCollections(ServerState::instance()->isSingleServer() &&
                                !_query->vocbase()
                                     .server()
                                     .getFeature<QueryRegistryFeature>()
                                     .requireWith()),
      _vertexProjections(vertexProjections),
      _edgeProjections(edgeProjections) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
}

RefactoredTraverserCache::~RefactoredTraverserCache() { clear(); }

void RefactoredTraverserCache::clear() {
  _resourceMonitor.decreaseMemoryUsage(
      _persistedStrings.size() * sizeof(arangodb::velocypack::HashedStringRef));
  _persistedStrings.clear();
  _stringHeap.clear();
}

template<typename ResultType>
bool RefactoredTraverserCache::appendEdge(EdgeDocumentToken const& idToken,
                                          EdgeReadType readType,
                                          ResultType& result) {
  if constexpr (std::is_same_v<ResultType, std::string>) {
    // Only Ids can be extracted into a std::string
    TRI_ASSERT(readType == EdgeReadType::ONLYID);
  }
  auto col = _trx->vocbase().lookupCollection(idToken.cid());

  if (ADB_UNLIKELY(col == nullptr)) {
    // collection gone... should not happen
    LOG_TOPIC("c4d78", ERR, arangodb::Logger::GRAPHS)
        << "Could not extract indexed edge document. collection not found";
    TRI_ASSERT(col != nullptr);  // for maintainer mode
    return false;
  }
  auto cb = [&](LocalDocumentId, aql::DocumentData&& data, VPackSlice edge) {
    if (readType == EdgeReadType::ONLYID) {
      if constexpr (std::is_same_v<ResultType, std::string>) {
        // If we want to expose the ID, we need to translate the
        // custom type Unfortunately we cannot do this in slice only
        // manner, as there is no complete slice with the _id.
        result = transaction::helpers::extractIdString(_trx->resolver(), edge,
                                                       VPackSlice::noneSlice());
        return true;
      }
      edge = edge.get(StaticStrings::IdString).translate();
    } else if (readType == EdgeReadType::ID_DOCUMENT) {
      if constexpr (std::is_same_v<ResultType, velocypack::Builder>) {
        TRI_ASSERT(result.isOpenObject());
        TRI_ASSERT(edge.isObject());
        // Extract and Translate the _key value
        result.add(VPackValue(transaction::helpers::extractIdString(
            _trx->resolver(), edge, VPackSlice::noneSlice())));
        if (!_edgeProjections.empty()) {
          VPackObjectBuilder guard(&result);
          _edgeProjections.toVelocyPackFromDocument(result, edge, _trx);
        } else {
          result.add(edge);
        }
        return true;
      } else {
        // We can only inject key_value pairs into velocypack
        TRI_ASSERT(false);
      }
    }
    // NOTE: Do not count this as Primary Index Scan, we
    // counted it in the edge Index before copying...
    if constexpr (std::is_same_v<ResultType, aql::AqlValue>) {
      if (!_edgeProjections.empty()) {
        // TODO: This does one unnecessary copy.
        // We should be able to move the Projection into the
        // AQL value.
        transaction::BuilderLeaser builder(_trx);
        {
          VPackObjectBuilder guard(builder.get());
          _edgeProjections.toVelocyPackFromDocument(*builder, edge, _trx);
        }
        result = aql::AqlValue(builder->slice());
      } else if (data) {
        result = aql::AqlValue(data);
      } else {
        result = aql::AqlValue(edge);
      }
    } else if constexpr (std::is_same_v<ResultType, velocypack::Builder>) {
      if (!_edgeProjections.empty()) {
        VPackObjectBuilder guard(&result);
        _edgeProjections.toVelocyPackFromDocument(result, edge, _trx);
      } else {
        result.add(edge);
      }
    }
    return true;
  };
  auto res =
      col->getPhysical()->lookup(_trx, idToken.localDocumentId(), cb, {}).ok();
  if (ADB_UNLIKELY(!res)) {
    // We already had this token, inconsistent state. Return NULL in Production
    LOG_TOPIC("daac5", ERR, arangodb::Logger::GRAPHS)
        << "Could not extract indexed edge document, return 'null' instead. "
        << "This is most likely a caching issue. Try: 'db." << col->name()
        << ".unload(); db." << col->name()
        << ".load()' in arangosh to fix this.";
  }
  return res;
}

ResultT<std::pair<std::string, size_t>>
RefactoredTraverserCache::extractCollectionName(
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
  return std::make_pair(colName, pos);
}

template<typename ResultType>
bool RefactoredTraverserCache::appendVertex(
    aql::TraversalStats& stats, velocypack::HashedStringRef const& id,
    ResultType& result) {
  auto collectionNameResult = extractCollectionName(id);
  if (collectionNameResult.fail()) {
    THROW_ARANGO_EXCEPTION(collectionNameResult.result());
  }

  auto findDocumentInCollection = [&](std::string const& shardId) -> bool {
    if (!_produceVertices) {
      // we don't need any vertex data, return quickly
      result.add(VPackSlice::nullSlice());
      return true;
    }
    try {
      transaction::AllowImplicitCollectionsSwitcher disallower(
          _trx->state()->options(), _allowImplicitCollections);

      auto cb = [&](LocalDocumentId, aql::DocumentData&& data, VPackSlice doc) {
        stats.incrScannedIndex(1);
        // copying...
        if constexpr (std::is_same_v<ResultType, aql::AqlValue>) {
          if (!_vertexProjections.empty()) {
            // TODO: This does one unnecessary copy.
            // We should be able to move the Projection into the
            // AQL value.
            transaction::BuilderLeaser builder(_trx);
            {
              VPackObjectBuilder guard(builder.get());
              _vertexProjections.toVelocyPackFromDocument(*builder, doc, _trx);
            }
            result = aql::AqlValue(builder->slice());
          } else if (data) {
            result = aql::AqlValue(data);
          } else {
            result = aql::AqlValue(doc);
          }
        } else if constexpr (std::is_same_v<ResultType, velocypack::Builder>) {
          if (!_vertexProjections.empty()) {
            VPackObjectBuilder guard(&result);
            _vertexProjections.toVelocyPackFromDocument(result, doc, _trx);
          } else {
            result.add(doc);
          }
        }
        return true;
      };
      Result res =
          _trx->documentFastPathLocal(
                  shardId,
                  id.substr(collectionNameResult.get().second + 1).stringView(),
                  cb)
              .waitAndGet();
      if (res.ok()) {
        return true;
      }

      if (!res.is(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND)) {
        // ok we are in a rather bad state. Better throw and abort.
        THROW_ARANGO_EXCEPTION(res);
      }
    } catch (basics::Exception const& ex) {
      if (isWithClauseMissing(ex)) {
        // turn the error into a different error
        auto message = absl::StrCat("collection not known to traversal: '",
                                    shardId, "'. please add 'WITH ", shardId,
                                    "' as the first line in your AQL");
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
                                       message);
      }
      // rethrow original error
      throw;
    }
    return false;
  };

  std::string const& collectionName = collectionNameResult.get().first;
  if (_collectionToShardMap.empty()) {
    TRI_ASSERT(!ServerState::instance()->isDBServer());
    if (findDocumentInCollection(collectionName)) {
      return true;
    }
  } else {
    auto it = _collectionToShardMap.find(collectionName);
    if (it == _collectionToShardMap.end()) {
      // Connected to a vertex where we do not know the Shard to.
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
          "collection not known to traversal: '" + collectionName +
              "'. please add 'WITH " + collectionName +
              "' as the first line in your AQL");
    }
    for (auto const& shard : it->second) {
      if (findDocumentInCollection(shard)) {
        // Short circuit, as soon as one shard contains this document
        // we can return it.
        return true;
      }
    }
  }

  // Register a warning. It is okay though but helps the user
  std::string msg = "vertex '" + id.toString() + "' not found";
  _query->warnings().registerWarning(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                     msg.c_str());
  // This is expected, we may have dangling edges. Interpret as NULL
  return false;
}

void RefactoredTraverserCache::insertEdgeIntoResult(
    EdgeDocumentToken const& idToken, VPackBuilder& builder) {
  if (!appendEdge(idToken, EdgeReadType::DOCUMENT, builder)) {
    builder.add(VPackSlice::nullSlice());
  }
}

void RefactoredTraverserCache::insertEdgeIdIntoResult(
    EdgeDocumentToken const& idToken, VPackBuilder& builder) {
  if (!appendEdge(idToken, EdgeReadType::ONLYID, builder)) {
    builder.add(VPackSlice::nullSlice());
  }
}

void RefactoredTraverserCache::insertEdgeIntoLookupMap(
    EdgeDocumentToken const& idToken, VPackBuilder& builder) {
  if (!appendEdge(idToken, EdgeReadType::ID_DOCUMENT, builder)) {
    // The IDToken has been expanded by an index used on for the edges.
    // The invariant is that an index only delivers existing edges so this
    // case should never happen in production. If it shows up we have
    // inconsistencies in index and data.
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "GraphEngine attempt to read details of a non-existing edge. This "
        "indicates index inconsistency.");
  }
}

std::string RefactoredTraverserCache::getEdgeId(
    EdgeDocumentToken const& idToken) {
  std::string res;
  if (!appendEdge(idToken, EdgeReadType::ONLYID, res)) {
    res = "null";
  }
  return res;
}

void RefactoredTraverserCache::insertVertexIntoResult(
    aql::TraversalStats& stats,
    arangodb::velocypack::HashedStringRef const& idString,
    VPackBuilder& builder, bool writeIdIfNotFound) {
  if (!appendVertex(stats, idString, builder)) {
    if (writeIdIfNotFound) {
      builder.add(VPackValue(idString.toString()));
    } else {
      builder.add(VPackSlice::nullSlice());
    }
  }
}

arangodb::velocypack::HashedStringRef RefactoredTraverserCache::persistString(
    arangodb::velocypack::HashedStringRef idString) {
  auto it = _persistedStrings.find(idString);
  if (it != _persistedStrings.end()) {
    return *it;
  }
  auto res = _stringHeap.registerString(idString);

  ResourceUsageScope guard(_resourceMonitor, sizeof(res));
  _persistedStrings.emplace(res);

  guard.steal();
  return res;
}
