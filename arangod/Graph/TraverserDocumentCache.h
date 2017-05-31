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
}

namespace graph {

class TraverserDocumentCache : public TraverserCache {

  public:
   explicit TraverserDocumentCache(transaction::Methods* trx);

   ~TraverserDocumentCache();


   //////////////////////////////////////////////////////////////////////////////
   /// @brief Inserts the real document stored within the token
   ///        into the given builder.
   ///        The document will be taken from the hash-cache.
   ///        If it is not cached it will be looked up in the StorageEngine
   //////////////////////////////////////////////////////////////////////////////

   void insertIntoResult(StringRef idString,
                         arangodb::velocypack::Builder& builder) override;

   void insertIntoResult(EdgeDocumentToken const* etkn,
                         arangodb::velocypack::Builder& builder) override;

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Return AQL value containing the result
   ///        The document will be taken from the hash-cache.
   ///        If it is not cached it will be looked up in the StorageEngine
   //////////////////////////////////////////////////////////////////////////////
  
   aql::AqlValue fetchAqlResult(StringRef idString) override;

   aql::AqlValue fetchAqlResult(arangodb::graph::EdgeDocumentToken const*) override;

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Insert value into store
   //////////////////////////////////////////////////////////////////////////////

   void insertDocument(StringRef idString,
                       arangodb::velocypack::Slice const& document) override;

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Throws the document referenced by the token into the filter
   ///        function and returns it result.
   ///        The document will be taken from the hash-cache.
   ///        If it is not cached it will be looked up in the StorageEngine
   //////////////////////////////////////////////////////////////////////////////

   bool validateFilter(StringRef idString,
       std::function<bool(arangodb::velocypack::Slice const&)> filterFunc) override;
 
  protected:

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Lookup a document by token in the cache.
   ///        As long as finding is retained it is guaranteed that the result
   ///        stays valid. Finding should not be retained very long, if it is
   ///        needed for longer, copy the value.
   //////////////////////////////////////////////////////////////////////////////
   cache::Finding lookup(StringRef idString);

  protected:

   //////////////////////////////////////////////////////////////////////////////
   /// @brief The hash-cache that saves documents found in the Database
   //////////////////////////////////////////////////////////////////////////////
   std::shared_ptr<arangodb::cache::Cache> _cache;
 
   //////////////////////////////////////////////////////////////////////////////
   /// @brief Lookup a document from the database and insert it into the cache.
   ///        The Slice returned here is only valid until the NEXT call of this
   ///        function.
   //////////////////////////////////////////////////////////////////////////////

   arangodb::velocypack::Slice lookupAndCache(
       StringRef idString);


};
} // namespace traverser
} // namespace arangodb

#endif
