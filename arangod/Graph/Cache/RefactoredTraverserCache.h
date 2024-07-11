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

#pragma once

#include "Aql/Projections.h"
#include "Basics/ResourceUsage.h"
#include "Basics/ResultT.h"
#include "Basics/StringHeap.h"
#include "Basics/MemoryTypes/MemoryTypes.h"
#include "Containers/FlatHashSet.h"

#include <velocypack/HashedStringRef.h>

namespace arangodb {

namespace transaction {
class Methods;
}

namespace velocypack {
class Builder;
class Slice;
}  // namespace velocypack

namespace aql {
struct AqlValue;
class QueryContext;
class TraversalStats;
}  // namespace aql

namespace graph {

struct EdgeDocumentToken;

/// Small wrapper around the actual datastore in
/// which edges and vertices are stored.
/// This db server variant can just work with raw
/// document tokens and retrieve documents as needed

class RefactoredTraverserCache {
 public:
  enum EdgeReadType { ONLYID, DOCUMENT, ID_DOCUMENT };

  RefactoredTraverserCache(
      arangodb::transaction::Methods* trx, aql::QueryContext* query,
      arangodb::ResourceMonitor& resourceMonitor,
      arangodb::aql::TraversalStats& stats,
      MonitoredCollectionToShardMap const& collectionToShardMap,
      arangodb::aql::Projections const& vertexProjections,
      arangodb::aql::Projections const& edgeProjections, bool produceVertices);

  ~RefactoredTraverserCache();

  RefactoredTraverserCache(RefactoredTraverserCache const&) = delete;
  RefactoredTraverserCache(RefactoredTraverserCache&&) = default;

  RefactoredTraverserCache& operator=(RefactoredTraverserCache const&) = delete;

  /// @brief clears all allocated memory in the underlying StringHeap
  void clear();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts the real document stored within the token
  ///        into the given builder.
  //////////////////////////////////////////////////////////////////////////////
  void insertEdgeIntoResult(graph::EdgeDocumentToken const& etkn,
                            velocypack::Builder& builder);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts only the edges _id value into the given builder.
  //////////////////////////////////////////////////////////////////////////////
  void insertEdgeIdIntoResult(graph::EdgeDocumentToken const& etkn,
                              velocypack::Builder& builder);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts { [...], _id : edge, [...] } into the given builder.
  /// The builder has to be an open Object.
  //////////////////////////////////////////////////////////////////////////////
  void insertEdgeIntoLookupMap(graph::EdgeDocumentToken const& etkn,
                               velocypack::Builder& builder);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the Translated Edge _id value, translating the custom type
  //////////////////////////////////////////////////////////////////////////////
  std::string getEdgeId(EdgeDocumentToken const& idToken);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts the real document identified by the _id string
  //////////////////////////////////////////////////////////////////////////////
  void insertVertexIntoResult(
      aql::TraversalStats& stats,
      arangodb::velocypack::HashedStringRef const& idString,
      velocypack::Builder& builder, bool writeIdIfNotFound);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Persist the given id string. The return value is guaranteed to
  ///        stay valid as long as this cache is valid
  //////////////////////////////////////////////////////////////////////////////
  arangodb::velocypack::HashedStringRef persistString(
      arangodb::velocypack::HashedStringRef idString);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Lookup a document from the database.
  ///        if this returns false the result is unmodified
  //////////////////////////////////////////////////////////////////////////////

  template<typename ResultType>
  bool appendVertex(aql::TraversalStats& stats,
                    arangodb::velocypack::HashedStringRef const& idString,
                    ResultType& result);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Lookup an edge document from the database.
  ///        if this returns false the result is unmodified
  ///        if readType is set to ONLYID: the result will only contain the _Id
  ///        if readType is set to DOCUMENT: the result will contain the data
  ///        if readType is set to ID_DOCUMENT: the result will contain {id:
  ///        data}
  //////////////////////////////////////////////////////////////////////////////

  template<typename ResultType>
  bool appendEdge(graph::EdgeDocumentToken const& etkn, EdgeReadType readType,
                  ResultType& result);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Helper Method to extract collection Name from given
  /// VertexIdentifier
  ///        Will translate to ShardName in case of Satellite Graphs
  //////////////////////////////////////////////////////////////////////////////

  ResultT<std::pair<std::string, size_t>> extractCollectionName(
      velocypack::HashedStringRef const& idHashed) const;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Query used to register warnings to.
  //////////////////////////////////////////////////////////////////////////////
  arangodb::aql::QueryContext* _query;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Transaction to access data, This class is NOT responsible for it.
  //////////////////////////////////////////////////////////////////////////////
  arangodb::transaction::Methods* _trx;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Stringheap to take care of _id strings, s.t. they stay valid
  ///        during the entire traversal.
  //////////////////////////////////////////////////////////////////////////////
  arangodb::StringHeap _stringHeap;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Set of all strings persisted in the stringHeap. So we can save some
  ///        memory by not storing them twice.
  //////////////////////////////////////////////////////////////////////////////
  containers::FlatHashSet<arangodb::velocypack::HashedStringRef>
      _persistedStrings;

 private:
  MonitoredCollectionToShardMap const& _collectionToShardMap;
  arangodb::ResourceMonitor& _resourceMonitor;

  /// @brief whether or not to produce vertices
  bool const _produceVertices;

  /// @brief whether or not to allow adding of previously unknown collections
  /// during the traversal
  bool const _allowImplicitCollections;

  /// @brief Projections on vertex data, responsibility is with BaseOptions
  aql::Projections const& _vertexProjections;

  /// @brief Projections on edge data, responsibility is with BaseOptions
  aql::Projections const& _edgeProjections;
};

}  // namespace graph
}  // namespace arangodb
