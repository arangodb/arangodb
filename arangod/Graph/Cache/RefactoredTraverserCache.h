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

#ifndef ARANGOD_GRAPH_CACHE_REFACTORED_TRAVERSER_CACHE_H
#define ARANGOD_GRAPH_CACHE_REFACTORED_TRAVERSER_CACHE_H 1

#include "Basics/Common.h"
#include "Basics/ResourceUsage.h"
#include "Basics/ResultT.h"
#include "Basics/StringHeap.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/HashedStringRef.h>

#include <map>
#include <unordered_set>

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
  explicit RefactoredTraverserCache(arangodb::transaction::Methods* trx,
                                    aql::QueryContext* query,
                                    arangodb::ResourceMonitor& resourceMonitor,
                                    arangodb::aql::TraversalStats& stats,
                                    std::map<std::string, std::string> const& collectionToShardMap);
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
  /// @brief Inserts the real document identified by the _id string
  //////////////////////////////////////////////////////////////////////////////
  void insertVertexIntoResult(aql::TraversalStats& stats,
                              arangodb::velocypack::HashedStringRef const& idString,
                              velocypack::Builder& builder);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Persist the given id string. The return value is guaranteed to
  ///        stay valid as long as this cache is valid
  //////////////////////////////////////////////////////////////////////////////
  arangodb::velocypack::HashedStringRef persistString(arangodb::velocypack::HashedStringRef idString);

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Lookup a document from the database.
  ///        if this returns false the result is unmodified
  //////////////////////////////////////////////////////////////////////////////

  template <typename ResultType>
  bool appendVertex(aql::TraversalStats& stats,
                    arangodb::velocypack::HashedStringRef const& idString,
                    ResultType& result);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Lookup an edge document from the database.
  ///        if this returns false the result is unmodified
  //////////////////////////////////////////////////////////////////////////////

  template <typename ResultType>
  bool appendEdge(graph::EdgeDocumentToken const& etkn, ResultType& result);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Helper Method to extract collection Name from given VertexIdentifier
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
  std::unordered_set<arangodb::velocypack::HashedStringRef> _persistedStrings;

 private:
  std::map<std::string, std::string> const& _collectionToShardMap;
  arangodb::ResourceMonitor& _resourceMonitor;

};

}  // namespace graph
}  // namespace arangodb

#endif
