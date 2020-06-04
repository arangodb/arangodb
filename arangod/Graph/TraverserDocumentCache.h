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

#ifndef ARANGOD_GRAPH_TRAVERSER_DOCUMENT_CACHE_H
#define ARANGOD_GRAPH_TRAVERSER_DOCUMENT_CACHE_H 1

#include "Graph/TraverserCache.h"

namespace arangodb {

namespace cache {
class Cache;
class Finding;
}  // namespace cache

namespace graph {

class TraverserDocumentCache final : public TraverserCache {
 public:
  TraverserDocumentCache(aql::QueryContext& query, 
                         std::shared_ptr<arangodb::cache::Cache> cache, 
                         BaseOptions*);

  ~TraverserDocumentCache();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Inserts the real document stored within the token
  ///        into the given builder.
  ///        The document will be taken from the hash-cache.
  ///        If it is not cached it will be looked up in the StorageEngine
  //////////////////////////////////////////////////////////////////////////////
  void insertEdgeIntoResult(EdgeDocumentToken const& etkn,
                            arangodb::velocypack::Builder& builder) override;

  /// Looks up the document and inserts it into the builder
  void insertVertexIntoResult(arangodb::velocypack::StringRef idString, arangodb::velocypack::Builder& builder) override;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Return AQL value containing the result
  ///        The document will be taken from the hash-cache.
  ///        If it is not cached it will be looked up in the StorageEngine
  //////////////////////////////////////////////////////////////////////////////

  aql::AqlValue fetchEdgeAqlResult(graph::EdgeDocumentToken const&) override;

  aql::AqlValue fetchVertexAqlResult(arangodb::velocypack::StringRef idString) override;

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Lookup a document by token in the cache.
  ///        As long as finding is retained it is guaranteed that the result
  ///        stays valid. Finding should not be retained very long, if it is
  ///        needed for longer, copy the value.
  //////////////////////////////////////////////////////////////////////////////
  cache::Finding lookup(arangodb::velocypack::StringRef idString);

 private:
  void insertIntoCache(arangodb::velocypack::StringRef id,
                       arangodb::velocypack::Slice const& document);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Lookup a document from the database and insert it into the cache.
  ///        The Slice returned here is only valid until the NEXT call of this
  ///        function.
  //////////////////////////////////////////////////////////////////////////////

  arangodb::velocypack::Slice lookupAndCache(arangodb::velocypack::StringRef idString);
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief The hash-cache that saves documents found in the Database
  //////////////////////////////////////////////////////////////////////////////
  std::shared_ptr<arangodb::cache::Cache> _cache;
};
}  // namespace graph
}  // namespace arangodb

#endif
