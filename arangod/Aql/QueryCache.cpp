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
#include "Basics/tri-strings.h"
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
                                              TRI_json_t* queryResult,
                                              std::vector<std::string> const& collections)
  : _queryString(nullptr),
    _queryStringLength(queryStringLength),
    _queryResult(queryResult),
    _collections(collections) {

  _queryString = TRI_DuplicateString2Z(TRI_UNKNOWN_MEM_ZONE, queryString, queryStringLength);

  if (_queryString == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a cache entry
////////////////////////////////////////////////////////////////////////////////

QueryCacheResultEntry::~QueryCacheResultEntry () {
  TRI_FreeJson(TRI_UNKNOWN_MEM_ZONE, _queryResult);
  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, _queryString);
}

// -----------------------------------------------------------------------------
// --SECTION--                                    struct QueryCacheDatabaseEntry
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief create a database-specific cache
////////////////////////////////////////////////////////////////////////////////

QueryCacheDatabaseEntry::QueryCacheDatabaseEntry (TRI_vocbase_t* vocbase)
  : _vocbase(vocbase),
    _entriesByHash(),
    _entriesByCollection() {
  
  _entriesByHash.reserve(128);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief destroy a database-specific cache
////////////////////////////////////////////////////////////////////////////////

QueryCacheDatabaseEntry::~QueryCacheDatabaseEntry () {  
  _entriesByCollection.clear();
}

////////////////////////////////////////////////////////////////////////////////
/// @brief lookup a query result in the database-specific cache
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<QueryCacheResultEntry> QueryCacheDatabaseEntry::lookup (uint64_t hash,
                                                                        char const* queryString,
                                                                        size_t queryStringLength) const {
  auto it = _entriesByHash.find(hash);

  if (it == _entriesByHash.end()) {
    // not found in cache
    return std::shared_ptr<QueryCacheResultEntry>();
  }

  // found some result in cache
  
  if (queryStringLength != (*it).second->_queryStringLength ||
      strcmp(queryString, (*it).second->_queryString) != 0) {
    // found something, but obviously the result of a different query with the same hash
    return std::shared_ptr<QueryCacheResultEntry>();
  }

  return (*it).second;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief store a query result in the database-specific cache
////////////////////////////////////////////////////////////////////////////////

void QueryCacheDatabaseEntry::store (uint64_t hash,
                                     std::shared_ptr<QueryCacheResultEntry>& entry) {
  if (! _entriesByHash.emplace(hash, entry).second) {
    // remove previous entry
    _entriesByHash.erase(hash);
    _entriesByHash.emplace(hash, entry);
  }

  try {
    for (auto const& it : entry.get()->_collections) {
      auto it2 = _entriesByCollection.find(it);
    
      if (it2 == _entriesByCollection.end()) {
        it2 = _entriesByCollection.emplace(it, std::unordered_set<uint64_t>()).first;
      }

      (*it2).second.emplace(hash);
    }
  }
  catch (...) {
    // rollback

    // remove from collections
    for (auto const& it : entry.get()->_collections) {
      auto it2 = _entriesByCollection.find(it);

      if (it2 != _entriesByCollection.end()) {
        (*it2).second.erase(hash);
      }
    }

    // remove from hash table
    _entriesByHash.erase(hash);
    throw;
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all entries for the given collections in the 
/// database-specific cache
////////////////////////////////////////////////////////////////////////////////

void QueryCacheDatabaseEntry::invalidate (std::vector<char const*> const& collections) {
  for (auto const& it : collections) {
    invalidate(it);
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all entries for a collection in the database-specific 
/// cache
////////////////////////////////////////////////////////////////////////////////

void QueryCacheDatabaseEntry::invalidate (char const* collection) {
  auto it = _entriesByCollection.find(std::string(collection));

  if (it == _entriesByCollection.end()) {
    return;
  }
  
  for (auto& it2 : (*it).second) {
    _entriesByHash.erase(it2);
  }

  _entriesByCollection.erase(it);
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
  : _lock(),
    _entries(),
    _active(true) {

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
/// @brief test whether the cache might be active
////////////////////////////////////////////////////////////////////////////////

bool QueryCache::mayBeActive () const {
  return (_active && (mode() != CACHE_ALWAYS_OFF));
}

////////////////////////////////////////////////////////////////////////////////
/// @brief return whether or not the query cache is enabled
////////////////////////////////////////////////////////////////////////////////

QueryCacheMode QueryCache::mode () const {
  if (! _active) {
    // override whatever was stored in mode
    return CACHE_ALWAYS_OFF;
  }

  return Mode.load(std::memory_order_relaxed);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disables the query cache and flushes entries
////////////////////////////////////////////////////////////////////////////////

void QueryCache::mode (QueryCacheMode value) {
  // serialize all accesses to this method to prevent races
  WRITE_LOCKER(_lock);

  // must invalidate all entries now because disabling the cache will turn off
  // cache invalidation when modifying data. turning on the cache later would then
  // lead to invalid results being returned. this can all be prevented by fully
  // clearing the cache 
  invalidate();

  Mode.store(value, std::memory_order_release);
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

  std::shared_ptr<QueryCacheResultEntry> entry(new QueryCacheResultEntry(queryString, queryStringLength, result, collections));

  WRITE_LOCKER(_lock);

  auto it = _entries.find(vocbase);

  if (it == _entries.end()) {
    std::unique_ptr<QueryCacheDatabaseEntry> db(new QueryCacheDatabaseEntry(vocbase));
    it = _entries.emplace(vocbase, db.get()).first;
    db.release();
  }

  (*it).second->store(hash, entry);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all queries for the given collections
/// the lock is already acquired externally so we don't lock again
////////////////////////////////////////////////////////////////////////////////

void QueryCache::invalidate (triagens::basics::ReadWriteLock& lock,
                             TRI_vocbase_t* vocbase,
                             std::vector<char const*> const& collections) {
  TRI_ASSERT(&lock == &_lock); // this should be our lock

  try {
    auto it = _entries.find(vocbase);

    if (it == _entries.end()) { 
      return;
    } 

    // invalidate while holding the lock
    (*it).second->invalidate(collections);
  }
  catch (...) {
    // something is really wrong. now disable ourselves
    disable();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all queries for the given collections
////////////////////////////////////////////////////////////////////////////////

void QueryCache::invalidate (TRI_vocbase_t* vocbase,
                             std::vector<char const*> const& collections) {
  try {
    WRITE_LOCKER(_lock);

    auto it = _entries.find(vocbase);

    if (it == _entries.end()) { 
      return;
    } 

    // invalidate while holding the lock
    (*it).second->invalidate(collections);
  }
  catch (...) {
    // something is really wrong. now disable ourselves
    disable();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all queries for a particular collection
////////////////////////////////////////////////////////////////////////////////

void QueryCache::invalidate (TRI_vocbase_t* vocbase,
                             char const* collection) {
  try {
    WRITE_LOCKER(_lock);

    auto it = _entries.find(vocbase);

    if (it == _entries.end()) { 
      return;
    } 

    // invalidate while holding the lock
    (*it).second->invalidate(collection);
  }
  catch (...) {
    // something is really wrong. now disable ourselves
    disable();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all queries for a particular collection
/// the lock is already acquired externally so we don't lock again
////////////////////////////////////////////////////////////////////////////////

void QueryCache::invalidate (triagens::basics::ReadWriteLock& lock,
                             TRI_vocbase_t* vocbase,
                             char const* collection) {
  TRI_ASSERT(&lock == &_lock); // this should be our lock

  try {
    auto it = _entries.find(vocbase);

    if (it == _entries.end()) { 
      return;
    } 

    // invalidate while holding the lock
    (*it).second->invalidate(collection);
  }
  catch (...) {
    // something is really wrong. now disable ourselves
    disable();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all queries for a particular database
////////////////////////////////////////////////////////////////////////////////

void QueryCache::invalidate (TRI_vocbase_t* vocbase) {
  QueryCacheDatabaseEntry* databaseQueryCache = nullptr;

  try {
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
  catch (...) {
    // something is really wrong. now disable ourselves
    disable();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all queries for a particular database
/// the lock is already acquired externally so we don't lock again
////////////////////////////////////////////////////////////////////////////////

void QueryCache::invalidate (triagens::basics::ReadWriteLock& lock,
                             TRI_vocbase_t* vocbase) {
  TRI_ASSERT(&lock == &_lock); // this should be our lock

  try {
    QueryCacheDatabaseEntry* databaseQueryCache = nullptr;

    auto it = _entries.find(vocbase);

    if (it == _entries.end()) { 
      return;
    } 

    databaseQueryCache = (*it).second;
    _entries.erase(it);

    // delete without holding the lock
    TRI_ASSERT(databaseQueryCache != nullptr);
    delete databaseQueryCache;
  }
  catch (...) {
    // something is really wrong. now disable ourselves
    disable();
  }
}

////////////////////////////////////////////////////////////////////////////////
/// @brief disable ourselves in case of emergency
////////////////////////////////////////////////////////////////////////////////

void QueryCache::disable () {
  _active = false;
  mode(CACHE_ALWAYS_OFF);
}

////////////////////////////////////////////////////////////////////////////////
/// @brief get the query cache instance
////////////////////////////////////////////////////////////////////////////////

QueryCache* QueryCache::instance () {
  return &Instance;
}

// -----------------------------------------------------------------------------
// --SECTION--                                                   private methods
// -----------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////
/// @brief invalidate all entries in cache
/// note that the caller of this method must hold the write lock
////////////////////////////////////////////////////////////////////////////////

void QueryCache::invalidate () {
  for (auto& it : _entries) {
    delete it.second;
  }

  _entries.clear();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------

// Local Variables:
// mode: outline-minor
// outline-regexp: "/// @brief\\|/// {@inheritDoc}\\|/// @page\\|// --SECTION--\\|/// @\\}"
// End:
