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

#ifndef ARANGOD_VOC_BASE_TRAVERSER_CACHE_H
#define ARANGOD_VOC_BASE_TRAVERSER_CACHE_H 1

#include "Basics/Common.h"
#include "Basics/StringRef.h"

namespace arangodb {
class ManagedDocumentResult;
class StringHeap;

namespace cache {
class Cache;
class Finding;
}

namespace transaction {
class Methods;
}

namespace velocypack {
class Builder;
class Slice;
}

namespace aql {
  struct AqlValue;
}
  
namespace traverser {
class TraverserCache {

  public:
   explicit TraverserCache(transaction::Methods* trx);

   ~TraverserCache();

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Inserts the real document stored within the token
   ///        into the given builder.
   ///        The document will be taken from the hash-cache.
   ///        If it is not cached it will be looked up in the StorageEngine
   //////////////////////////////////////////////////////////////////////////////

   void insertIntoResult(StringRef idString,
                         arangodb::velocypack::Builder& builder);

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Return AQL value containing the result
   ///        The document will be taken from the hash-cache.
   ///        If it is not cached it will be looked up in the StorageEngine
   //////////////////////////////////////////////////////////////////////////////
  
   aql::AqlValue fetchAqlResult(StringRef idString);

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Insert value into store
   //////////////////////////////////////////////////////////////////////////////
   void insertDocument(StringRef idString,
                       arangodb::velocypack::Slice const& document);

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Throws the document referenced by the token into the filter
   ///        function and returns it result.
   ///        The document will be taken from the hash-cache.
   ///        If it is not cached it will be looked up in the StorageEngine
   //////////////////////////////////////////////////////////////////////////////

   bool validateFilter(StringRef idString,
       std::function<bool(arangodb::velocypack::Slice const&)> filterFunc);
  
  size_t getAndResetInsertedDocuments() {
    size_t tmp = _insertedDocuments;
    _insertedDocuments = 0;
    return tmp;
  }

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Persist the given id string. The return value is guaranteed to
   ///        stay valid as long as this cache is valid
   //////////////////////////////////////////////////////////////////////////////
   StringRef persistString(StringRef const idString);

  private:

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Lookup a document by token in the cache.
   ///        As long as finding is retained it is guaranteed that the result
   ///        stays valid. Finding should not be retained very long, if it is
   ///        needed for longer, copy the value.
   //////////////////////////////////////////////////////////////////////////////
   cache::Finding lookup(StringRef idString);

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Lookup a document from the database and insert it into the cache.
   ///        The Slice returned here is only valid until the NEXT call of this
   ///        function.
   //////////////////////////////////////////////////////////////////////////////

   arangodb::velocypack::Slice lookupInCollection(
       StringRef idString);

   //////////////////////////////////////////////////////////////////////////////
   /// @brief The hash-cache that saves documents found in the Database
   //////////////////////////////////////////////////////////////////////////////
   std::shared_ptr<arangodb::cache::Cache> _cache;

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Reusable ManagedDocumentResult that temporarily takes
   ///        responsibility for one document.
   //////////////////////////////////////////////////////////////////////////////
   std::unique_ptr<ManagedDocumentResult> _mmdr;

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Transaction to access data, This class is NOT responsible for it.
   //////////////////////////////////////////////////////////////////////////////
   arangodb::transaction::Methods* _trx;
  
   //////////////////////////////////////////////////////////////////////////////
   /// @brief Documents inserted in this cache
   //////////////////////////////////////////////////////////////////////////////
   size_t _insertedDocuments;

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Stringheap to take care of _id strings, s.t. they stay valid
   ///        during the entire traversal.
   //////////////////////////////////////////////////////////////////////////////
   std::unique_ptr<arangodb::StringHeap> _stringHeap;

   //////////////////////////////////////////////////////////////////////////////
   /// @brief Set of all strings persisted in the stringHeap. So we can save some
   ///        memory by not storing them twice.
   //////////////////////////////////////////////////////////////////////////////
   std::unordered_set<arangodb::StringRef> _persistedStrings;
};

}
}

#endif
