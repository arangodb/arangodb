#pragma once

#include "Aql/Projections.h"
#include "Graph/EdgeDocumentToken.h"
#include "velocypack/Builder.h"
#include "Indexes/IndexIterator.h"

namespace arangodb::graph {

struct EdgeDocumentToken;

struct EdgeLookup {
  explicit EdgeLookup(transaction::Methods* trx,
                      aql::Projections const& projections)
      : _trx(std::move(trx)), _projections(projections){};
  EdgeLookup(EdgeLookup const&) = delete;
  EdgeLookup(EdgeLookup&&) = default;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts the real document stored within the token
  ///        into the given builder.
  //////////////////////////////////////////////////////////////////////////////
  auto insertEdgeIntoResult(graph::EdgeDocumentToken const& etkn,
                            velocypack::Builder& builder) const -> void;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts only the edges _id value into the given builder.
  //////////////////////////////////////////////////////////////////////////////
  auto insertEdgeIdIntoResult(graph::EdgeDocumentToken const& etkn,
                              velocypack::Builder& builder) const -> void;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts { [...], _id : edge, [...] } into the given builder.
  /// The builder has to be an open Object.
  //////////////////////////////////////////////////////////////////////////////
  auto insertEdgeIntoLookupMap(graph::EdgeDocumentToken const& etkn,
                               velocypack::Builder& builder) const -> void;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the Translated Edge _id value, translating the custom type
  //////////////////////////////////////////////////////////////////////////////
  auto getEdgeId(EdgeDocumentToken const& idToken) const -> std::string;

 private:
  auto doAppendEdge(EdgeDocumentToken const& idToken,
                    IndexIterator::DocumentCallback const& cb) const -> bool;

 private:
  transaction::Methods* _trx;
  aql::Projections const& _projections;
};

}  // namespace arangodb::graph
