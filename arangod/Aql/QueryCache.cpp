////////////////////////////////////////////////////////////////////////////////
/// @brief Aql, query cache
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2014, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Aql/QueryCache.h"
#include "Basics/json.h"
#include "Basics/Exceptions.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "VocBase/vocbase.h"

using namespace triagens::aql;

// -----------------------------------------------------------------------------
// --SECTION--                                                 private variables
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief singleton instance of the query cache
////////////////////////////////////////////////////////////////////////////////

static triagens::aql::QueryCache Instance;

////////////////////////////////////////////////////////////////////////////////
/// @brief whether or not the cache is enabled
////////////////////////////////////////////////////////////////////////////////

static std::atomic<triagens::aql::QueryCacheMode> Mode(CACHE_ON_DEMAND);

// -----------------------------------------------------------------------------
// --SECTION--                                      struct QueryCacheResultEntry
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a cache entry
////////////////////////////////////////////////////////////////////////////////

QueryCacheResultEntry::QueryCacheResultEntry (char const* queryString,
                                              size_t queryStringLength,
                                              TRI_json_t* queryResult)
  : queryStringLength(queryStringLength),
    queryResult(queryResult) {
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a cache entry
////////////////////////////////////////////////////////////////////////////////

QueryCacheResultEntry::~QueryCacheResultEntry () {
  TRI_Free(TRI_UNKNOWN_MEM_ZONE, queryResult);
}

// -----------------------------------------------------------------------------
// --SECTION--                                    struct QueryCacheDatabaseEntry
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a database-specific cache
////////////////////////////////////////////////////////////////////////////////

QueryCacheDatabaseEntry::QueryCacheDatabaseEntry (TRI_vocbase_t* vocbase)
  : vocbase(vocbase),
    entriesByHash(),
    entriesByCollection() {
  
  entriesByHash.reserve(128);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a database-specific cache
////////////////////////////////////////////////////////////////////////////////

QueryCacheDatabaseEntry::~QueryCacheDatabaseEntry () {  
  entriesByCollection.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a query result in the database-specific cache
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<QueryCacheResultEntry> QueryCacheDatabaseEntry::lookup (uint64_t hash,
                                                                        char const* queryString,
                                                                        size_t queryStringLength) const {
  auto it = entriesByHash.find(hash);

  if (it == entriesByHash.end()) {
    // not found in cache
    return std::shared_ptr<QueryCacheResultEntry>();
  }

  // found result in cache

  if (queryStringLength != (*it).second->queryStringLength) {
    // found something, but obviously the result of a different query with the same hash
    return std::shared_ptr<QueryCacheResultEntry>();
  }

  return (*it).second;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief store a query result in the database-specific cache
////////////////////////////////////////////////////////////////////////////////

void QueryCacheDatabaseEntry::store (uint64_t hash,
                                     char const* queryString, 
                                     size_t queryStringLength,
                                     TRI_json_t* result,
                                     std::vector<std::string> const& collections) {
  entriesByHash.erase(hash);
  
  std::shared_ptr<QueryCacheResultEntry> entry(new QueryCacheResultEntry(queryString, queryStringLength, result));
  entriesByHash.emplace(hash, entry);

  try {
    for (auto const& it : collections) {
      auto it2 = entriesByCollection.find(it);
    
      if (it2 == entriesByCollection.end()) {
        it2 = entriesByCollection.emplace(it, std::unordered_set<uint64_t>()).first;
      }

      (*it2).second.emplace(hash);
    }
  }
  catch (...) {
    // rollback

    // remove from collections
    for (auto const& it : collections) {
      auto it2 = entriesByCollection.find(it);

      if (it2 != entriesByCollection.end()) {
        (*it2).second.erase(hash);
      }
    }

    // remove from hash table
    entriesByHash.erase(hash);
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all entries for a collection in the database-specific 
/// cache
////////////////////////////////////////////////////////////////////////////////

void QueryCacheDatabaseEntry::invalidate (std::string const& collection) {
  auto it = entriesByCollection.find(collection);

  if (it == entriesByCollection.end()) {
    return;
  }
  
  for (auto& it2 : (*it).second) {
    entriesByHash.erase(it2);
  }

  entriesByCollection.erase(it);
}

// -----------------------------------------------------------------------------
// --SECTION--                                                  class QueryCache
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// --SECTION--                                        constructors / destructors
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create the query cache
////////////////////////////////////////////////////////////////////////////////

QueryCache::QueryCache () 
  : _lock() {

}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy the query cache
////////////////////////////////////////////////////////////////////////////////

QueryCache::~QueryCache () {
}

// -----------------------------------------------------------------------------
// --SECTION--                                                    public methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether or not the query cache is enabled
////////////////////////////////////////////////////////////////////////////////

QueryCacheMode QueryCache::mode () const {
  return Mode.load(std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enable or disable the query cache
////////////////////////////////////////////////////////////////////////////////

void QueryCache::mode (QueryCacheMode value) {
  return Mode.store(value, std::memory_order_release);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief enable or disable the query cache
////////////////////////////////////////////////////////////////////////////////

void QueryCache::mode (std::string const& value) {
  if (value == "demand") {
    mode(CACHE_ON_DEMAND);
  }
  else if (value == "on") {
    mode(CACHE_ALWAYS_ON);
  }
  else {
    mode(CACHE_ALWAYS_OFF);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a query result in the cache
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<QueryCacheResultEntry> QueryCache::lookup (TRI_vocbase_t* vocbase,
                                                           uint64_t hash,
                                                           char const* queryString,
                                                           size_t queryStringLength) {
  READ_LOCKER(_lock);

  auto it = _entries.find(vocbase);

  if (it == _entries.end()) {
    // no entry found for the requested database
    return nullptr;
  } 

  return (*it).second->lookup(hash, queryString, queryStringLength);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief store a query in the cache
/// if the call is successful, the cache has taken over ownership for the 
/// query result!
////////////////////////////////////////////////////////////////////////////////

void QueryCache::store (TRI_vocbase_t* vocbase,
                        uint64_t hash,
                        char const* queryString,
                        size_t queryStringLength,
                        TRI_json_t* result,
                        std::vector<std::string> const& collections) {
  WRITE_LOCKER(_lock);

  auto it = _entries.find(vocbase);

  if (it == _entries.end()) {
    std::unique_ptr<QueryCacheDatabaseEntry> db(new QueryCacheDatabaseEntry(vocbase));
    it = _entries.emplace(vocbase, db.get()).first;
    db.release();
  }

  (*it).second->store(hash, queryString, queryStringLength, result, collections);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all queries for a particular collection
////////////////////////////////////////////////////////////////////////////////

void QueryCache::invalidate (TRI_vocbase_t* vocbase,
                             std::string const& collection) {
  WRITE_LOCKER(_lock);

  auto it = _entries.find(vocbase);

  if (it == _entries.end()) { 
    return;
  } 

  (*it).second->invalidate(collection);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all queries for a particular database
////////////////////////////////////////////////////////////////////////////////

void QueryCache::invalidate (TRI_vocbase_t* vocbase) {
  QueryCacheDatabaseEntry* databaseQueryCache = nullptr;

  {
    WRITE_LOCKER(_lock);

    auto it = _entries.find(vocbase);

    if (it == _entries.end()) { 
      return;
    } 

    databaseQueryCache = (*it).second;
    _entries.erase(it);
  }

  // delete without holding the lock
  TRI_ASSERT(databaseQueryCache != nullptr);
  delete databaseQueryCache;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the query cache instance
////////////////////////////////////////////////////////////////////////////////

QueryCache* QueryCache::instance () {
  return &Instance;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
