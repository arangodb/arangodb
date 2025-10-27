#pragma once

#include <velocypack/HashedStringRef.h>
#include "Aql/Projections.h"
#include "Basics/MemoryTypes/MemoryTypes.h"
#include "velocypack/Builder.h"
#include "Indexes/IndexIterator.h"
#include "Aql/TraversalStats.h"
#include "Aql/QueryContext.h"

namespace arangodb::graph {

struct VertexLookup {
  explicit VertexLookup(
      transaction::Methods* trx, aql::QueryContext* queryCtx,
      aql::Projections const& projections,
      MonitoredCollectionToShardMap const& collectionToShardMap,
      std::shared_ptr<aql::TraversalStats> stats,
      bool allowImplicitVertexCollections, bool produceVertices)
      : _trx(std::move(trx)),
        _queryCtx(queryCtx),
        _projections(projections),
        _collectionToShardMap(collectionToShardMap),
        _stats(std::move(stats)),
        _allowImplicitVertexCollections(allowImplicitVertexCollections),
        _produceVertices(produceVertices) {}
  VertexLookup(VertexLookup const&) = delete;
  VertexLookup(VertexLookup&&) = default;

  auto findDocumentInCollection(velocypack::HashedStringRef shardId,
                                velocypack::HashedStringRef key,
                                velocypack::Builder& result) -> bool;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Lookup a document from the database.
  ///        if this returns false the result is unmodified
  //////////////////////////////////////////////////////////////////////////////
  auto appendVertex(velocypack::HashedStringRef id, velocypack::Builder& result)
      -> bool;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts the real document identified by the _id string
  //////////////////////////////////////////////////////////////////////////////
  auto insertVertexIntoResult(velocypack::HashedStringRef idString,
                              VPackBuilder& builder, bool writeIdIfNotFound)
      -> void;

 private:
  transaction::Methods* _trx;
  aql::QueryContext* _queryCtx;
  aql::Projections const& _projections;
  MonitoredCollectionToShardMap const& _collectionToShardMap;
  std::shared_ptr<aql::TraversalStats> _stats;
  bool _allowImplicitVertexCollections;
  bool _produceVertices;
};

}  // namespace arangodb::graph
