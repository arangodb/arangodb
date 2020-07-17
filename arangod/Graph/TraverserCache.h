////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017-2017 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_GRAPH_TRAVERSER_CACHE_H
#define ARANGOD_GRAPH_TRAVERSER_CACHE_H 1

#include "Basics/Common.h"
#include "Basics/StringHeap.h"
#include "VocBase/ManagedDocumentResult.h"

#include <velocypack/StringRef.h>

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
}  // namespace aql

namespace graph {

struct EdgeDocumentToken;

/// Small wrapper around the actual datastore in
/// which edges and vertices are stored. The cluster can overwrite this
/// with an implementation which caches entire documents,
/// the single server / db server can just work with raw
/// document tokens and retrieve documents as needed
struct BaseOptions;


class TraverserCache {
 public:
  explicit TraverserCache(aql::QueryContext& query, BaseOptions* opts);

  virtual ~TraverserCache();

  /// @brief clears all allocated memory in the underlying StringHeap
  void clear();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts the real document stored within the token
  ///        into the given builder.
  //////////////////////////////////////////////////////////////////////////////
  virtual void insertEdgeIntoResult(graph::EdgeDocumentToken const& etkn,
                                    velocypack::Builder& builder);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts the real document identified by the _id string
  //////////////////////////////////////////////////////////////////////////////
  virtual void insertVertexIntoResult(arangodb::velocypack::StringRef idString, velocypack::Builder& builder);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Return AQL value containing the result
  ///        The document will be looked up in the StorageEngine
  //////////////////////////////////////////////////////////////////////////////
  virtual aql::AqlValue fetchEdgeAqlResult(graph::EdgeDocumentToken const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Return AQL value containing the result
  ///        The document will be looked up in the StorageEngine
  //////////////////////////////////////////////////////////////////////////////
  virtual aql::AqlValue fetchVertexAqlResult(arangodb::velocypack::StringRef idString);

  size_t getAndResetInsertedDocuments() {
    size_t tmp = _insertedDocuments;
    _insertedDocuments = 0;
    return tmp;
  }

  size_t getAndResetFilteredDocuments() {
    size_t tmp = _filteredDocuments;
    _filteredDocuments = 0;
    return tmp;
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Persist the given id string. The return value is guaranteed to
  ///        stay valid as long as this cache is valid
  //////////////////////////////////////////////////////////////////////////////
  arangodb::velocypack::StringRef persistString(arangodb::velocypack::StringRef idString);

  void increaseFilterCounter() { _filteredDocuments++; }

  void increaseCounter() { _insertedDocuments++; }

  /// Only valid until the next call to this class
  virtual velocypack::Slice lookupToken(EdgeDocumentToken const& token);

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Lookup a document from the database.
  ///        The Slice returned here is only valid until the NEXT call of this
  ///        function.
  //////////////////////////////////////////////////////////////////////////////
  arangodb::velocypack::Slice lookupVertexInCollection(arangodb::velocypack::StringRef idString);

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reusable ManagedDocumentResult that temporarily takes
  ///        responsibility for one document.
  //////////////////////////////////////////////////////////////////////////////
  ManagedDocumentResult _mmdr;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Query used to register warnings to.
  //////////////////////////////////////////////////////////////////////////////
  arangodb::aql::QueryContext& _query;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Transaction to access data, This class is NOT responsible for it.
  //////////////////////////////////////////////////////////////////////////////
  arangodb::transaction::Methods* _trx;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Documents inserted in this cache
  //////////////////////////////////////////////////////////////////////////////
  size_t _insertedDocuments;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Documents filtered
  //////////////////////////////////////////////////////////////////////////////
  size_t _filteredDocuments;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Stringheap to take care of _id strings, s.t. they stay valid
  ///        during the entire traversal.
  //////////////////////////////////////////////////////////////////////////////
  arangodb::StringHeap _stringHeap;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Set of all strings persisted in the stringHeap. So we can save some
  ///        memory by not storing them twice.
  //////////////////////////////////////////////////////////////////////////////
  std::unordered_set<arangodb::velocypack::StringRef> _persistedStrings;

  BaseOptions const* _baseOptions;
};

}  // namespace graph
}  // namespace arangodb

#endif
