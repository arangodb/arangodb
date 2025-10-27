#include "Graph/Providers/SingleServer/VertexLookup.h"

#include "Graph/Providers/SingleServer/VertexLookup.h"
#include "velocypack/HashedStringRef.h"
#include "Logger/LogMacros.h"
#include "Basics/ResultT.h"
#include "Basics/Exceptions.h"
#include "Cluster/ServerState.h"
#include "Transaction/Options.h"
#include "Transaction/Methods.h"
#include "StorageEngine/TransactionState.h"

#include <string>

#include <velocypack/Slice.h>

using namespace arangodb;

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

auto splitDocumentId(velocypack::HashedStringRef idHashed)
    -> std::pair<velocypack::HashedStringRef, velocypack::HashedStringRef> {
  size_t pos = idHashed.find('/');

  if (pos == std::string::npos) {
    // Invalid input. If we get here somehow we managed to store invalid
    // _from/_to values or the traverser did a let an illegal start through
    TRI_ASSERT(false);
    auto res = Result(TRI_ERROR_GRAPH_INVALID_EDGE,
                      "edge contains invalid value " + idHashed.toString());
    THROW_ARANGO_EXCEPTION(res);
  }

  return std::make_pair(idHashed.substr(0, pos),
                        idHashed.substr(pos + 1, std::string::npos));
}

}  // namespace

namespace arangodb::graph {

auto VertexLookup::findDocumentInCollection(velocypack::HashedStringRef shardId,
                                            velocypack::HashedStringRef key,
                                            velocypack::Builder& result)
    -> bool {
  try {
    transaction::AllowImplicitCollectionsSwitcher disallower(
        _trx->state()->options(), _allowImplicitVertexCollections);

    Result res =
        _trx->documentFastPathLocal(
                shardId.stringView(), key.stringView(),  //
                [&](LocalDocumentId, aql::DocumentData&& data, VPackSlice doc) {
                  _stats->incrScannedIndex(1);

                  // copying...
                  _projections.toVelocyPackFromDocumentFull(result, doc, _trx);

                  return true;
                })
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
      auto message =
          absl::StrCat("collection not known to traversal: '",
                       shardId.stringView(), "'. please add 'WITH ",
                       shardId.stringView(), "' as the first line in your AQL");
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
                                     message);
    }
    // rethrow original error
    throw;
  }
  return false;
}

auto VertexLookup::appendVertex(velocypack::HashedStringRef id,
                                velocypack::Builder& result) -> bool {
  // TODO: kann weg. if we do not produce vertices, we always add null slice
  if (!_produceVertices) {
    result.add(VPackSlice::nullSlice());
    return true;
  }

  auto [collectionName, key] = splitDocumentId(id);

  if (_collectionToShardMap.empty()) {
    TRI_ASSERT(!ServerState::instance()->isDBServer());
    if (findDocumentInCollection(collectionName, key, result)) {
      return true;
    }
  } else {
    auto it = _collectionToShardMap.find(collectionName.toString());
    if (it == _collectionToShardMap.end()) {
      // Connected to a vertex where we do not know the Shard to.
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_QUERY_COLLECTION_LOCK_FAILED,
          "collection not known to traversal: '" + collectionName.toString() +
              "'. please add 'WITH " + collectionName.toString() +
              "' as the first line in your AQL");
    }
    for (auto const& shard : it->second) {
      auto str = std::string(shard);
      if (findDocumentInCollection(
              velocypack::HashedStringRef(str.c_str(), str.size()), key,
              result)) {
        // Short circuit, as soon as one shard contains this document
        // we can return it.
        return true;
      }
    }
  }

  // Register a warning. It is okay though but helps the user
  std::string msg = "vertex '" + id.toString() + "' not found";
  _queryCtx->warnings().registerWarning(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND,
                                        msg.c_str());
  // This is expected, we may have dangling edges. Interpret as NULL
  return false;
}

auto VertexLookup::insertVertexIntoResult(
    arangodb::velocypack::HashedStringRef idString, VPackBuilder& builder,
    bool writeIdIfNotFound) -> void {
  if (!_produceVertices) {
    builder.add(VPackSlice::nullSlice());
  }

  if (!appendVertex(idString, builder)) {
    if (writeIdIfNotFound) {
      builder.add(VPackValue(idString.toString()));
    } else {
      builder.add(VPackSlice::nullSlice());
    }
  }
}

}  // namespace arangodb::graph
