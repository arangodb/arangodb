#pragma once

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
      bool allowImplicitVertexCollections, bool produceVertices)
      : _trx(std::move(trx)),
        _queryCtx(queryCtx),
        _projections(projections),
        _collectionToShardMap(collectionToShardMap),
        _allowImplicitVertexCollections(allowImplicitVertexCollections),
        _produceVertices(produceVertices){};
  VertexLookup(VertexLookup const&) = delete;
  VertexLookup(VertexLookup&&) = default;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Lookup a document from the database.
  ///        if this returns false the result is unmodified
  //////////////////////////////////////////////////////////////////////////////
  auto appendVertex(aql::TraversalStats& stats,
                    velocypack::HashedStringRef const& id,
                    velocypack::Builder& result) const -> bool;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts the real document identified by the _id string
  //////////////////////////////////////////////////////////////////////////////
  auto insertVertexIntoResult(
      aql::TraversalStats& stats,
      arangodb::velocypack::HashedStringRef const& idString,
      VPackBuilder& builder, bool writeIdIfNotFound) const -> void;

 private:
  transaction::Methods* _trx;
  aql::QueryContext* _queryCtx;
  aql::Projections const& _projections;
  MonitoredCollectionToShardMap const& _collectionToShardMap;
  bool _allowImplicitVertexCollections;
  bool _produceVertices;
};

}  // namespace arangodb::graph
