////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Aql/QueryCache.h"
#include "Basics/fasthash.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/tri-strings.h"
#include "Basics/WriteLocker.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

/// @brief singleton instance of the query cache
static arangodb::aql::QueryCache Instance;

/// @brief maximum number of results in each per-database cache
static size_t MaxResults = 128;  // default value. can be changed later

/// @brief whether or not the cache is enabled
static std::atomic<arangodb::aql::QueryCacheMode> Mode(CACHE_ON_DEMAND);

/// @brief create a cache entry
QueryCacheResultEntry::QueryCacheResultEntry(
    uint64_t hash, char const* queryString, size_t queryStringLength,
    std::shared_ptr<VPackBuilder> queryResult, std::vector<std::string> const& collections)
    : _hash(hash),
      _queryString(nullptr),
      _queryStringLength(queryStringLength),
      _queryResult(queryResult),
      _collections(collections),
      _prev(nullptr),
      _next(nullptr),
      _refCount(0),
      _deletionRequested(0) {
  _queryString =
      TRI_DuplicateString(TRI_UNKNOWN_MEM_ZONE, queryString, queryStringLength);

  if (_queryString == nullptr) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_OUT_OF_MEMORY);
  }
}

/// @brief destroy a cache entry
QueryCacheResultEntry::~QueryCacheResultEntry() {
  TRI_FreeString(TRI_UNKNOWN_MEM_ZONE, _queryString);
}

/// @brief check whether the element can be destroyed, and delete it if yes
void QueryCacheResultEntry::tryDelete() {
  _deletionRequested = 1;

  if (_refCount == 0) {
    delete this;
  }
}

/// @brief use the element, so it cannot be deleted meanwhile
void QueryCacheResultEntry::use() { ++_refCount; }

/// @brief unuse the element, so it can be deleted if required
void QueryCacheResultEntry::unuse() {
  TRI_ASSERT(_refCount > 0);

  if (--_refCount == 0) {
    if (_deletionRequested == 1) {
      // trigger the deletion
      delete this;
    }
  }
}

/// @brief create a database-specific cache
QueryCacheDatabaseEntry::QueryCacheDatabaseEntry()
    : _entriesByHash(),
      _entriesByCollection(),
      _head(nullptr),
      _tail(nullptr),
      _numElements(0) {
  _entriesByHash.reserve(128);
  _entriesByCollection.reserve(16);
}

/// @brief destroy a database-specific cache
QueryCacheDatabaseEntry::~QueryCacheDatabaseEntry() {
  for (auto& it : _entriesByHash) {
    tryDelete(it.second);
  }

  _entriesByHash.clear();
  _entriesByCollection.clear();
}

/// @brief lookup a query result in the database-specific cache
QueryCacheResultEntry* QueryCacheDatabaseEntry::lookup(
    uint64_t hash, char const* queryString, size_t queryStringLength) {
  auto it = _entriesByHash.find(hash);

  if (it == _entriesByHash.end()) {
    // not found in cache
    return nullptr;
  }

  // found some result in cache

  if (queryStringLength != (*it).second->_queryStringLength ||
      memcmp(queryString, (*it).second->_queryString, queryStringLength) != 0) {
    // found something, but obviously the result of a different query with the
    // same hash
    return nullptr;
  }

  // found an entry
  auto entry = (*it).second;

  // mark the entry as being used so noone else can delete it while it is in use
  entry->use();

  return entry;
}

/// @brief store a query result in the database-specific cache
void QueryCacheDatabaseEntry::store(uint64_t hash,
                                    QueryCacheResultEntry* entry) {
  // insert entry into the cache
  if (!_entriesByHash.emplace(hash, entry).second) {
    // remove previous entry
    auto it = _entriesByHash.find(hash);
    TRI_ASSERT(it != _entriesByHash.end());
    auto previous = (*it).second;
    unlink(previous);
    _entriesByHash.erase(it);
    tryDelete(previous);

    // and insert again
    _entriesByHash.emplace(hash, entry);
  }

  try {
    for (auto const& it : entry->_collections) {
      auto it2 = _entriesByCollection.find(it);

      if (it2 == _entriesByCollection.end()) {
        // no entry found for collection. now create it
        _entriesByCollection.emplace(it, std::unordered_set<uint64_t>{hash});
      } else {
        // there already was an entry for this collection
        (*it2).second.emplace(hash);
      }
    }
  } catch (...) {
    // rollback

    // remove from collections
    for (auto const& it : entry->_collections) {
      auto it2 = _entriesByCollection.find(it);

      if (it2 != _entriesByCollection.end()) {
        (*it2).second.erase(hash);
      }
    }

    // finally remove entry itself from hash table
    auto it = _entriesByHash.find(hash);
    TRI_ASSERT(it != _entriesByHash.end());
    auto previous = (*it).second;
    _entriesByHash.erase(it);
    unlink(previous);
    tryDelete(previous);
    throw;
  }

  link(entry);

  enforceMaxResults(MaxResults);

  TRI_ASSERT(_numElements <= MaxResults);
  TRI_ASSERT(_head != nullptr);
  TRI_ASSERT(_tail != nullptr);
  TRI_ASSERT(_tail == entry);
  TRI_ASSERT(entry->_next == nullptr);
}

/// @brief invalidate all entries for the given collections in the
/// database-specific cache
void QueryCacheDatabaseEntry::invalidate(
    std::vector<std::string> const& collections) {
  for (auto const& it : collections) {
    invalidate(it);
  }
}

/// @brief invalidate all entries for a collection in the database-specific
/// cache
void QueryCacheDatabaseEntry::invalidate(std::string const& collection) {
  auto it = _entriesByCollection.find(collection);

  if (it == _entriesByCollection.end()) {
    return;
  }

  for (auto& it2 : (*it).second) {
    auto it3 = _entriesByHash.find(it2);

    if (it3 != _entriesByHash.end()) {
      // remove entry from the linked list
      auto entry = (*it3).second;
      unlink(entry);

      // erase it from hash table
      _entriesByHash.erase(it3);

      // delete the object itself
      tryDelete(entry);
    }
  }

  _entriesByCollection.erase(it);
}

/// @brief enforce maximum number of results
void QueryCacheDatabaseEntry::enforceMaxResults(size_t value) {
  while (_numElements > value) {
    // too many elements. now wipe the first element from the list

    // copy old _head value as unlink() will change it...
    auto head = _head;
    unlink(head);
    auto it = _entriesByHash.find(head->_hash);
    TRI_ASSERT(it != _entriesByHash.end());
    _entriesByHash.erase(it);
    tryDelete(head);
  }
}

/// @brief check whether the element can be destroyed, and delete it if yes
void QueryCacheDatabaseEntry::tryDelete(QueryCacheResultEntry* e) {
  e->tryDelete();
}

/// @brief unlink the result entry from the list
void QueryCacheDatabaseEntry::unlink(QueryCacheResultEntry* e) {
  if (e->_prev != nullptr) {
    e->_prev->_next = e->_next;
  }
  if (e->_next != nullptr) {
    e->_next->_prev = e->_prev;
  }

  if (_head == e) {
    _head = e->_next;
  }
  if (_tail == e) {
    _tail = e->_prev;
  }

  e->_prev = nullptr;
  e->_next = nullptr;

  TRI_ASSERT(_numElements > 0);
  --_numElements;
}

/// @brief link the result entry to the end of the list
void QueryCacheDatabaseEntry::link(QueryCacheResultEntry* e) {
  ++_numElements;

  if (_head == nullptr) {
    // list is empty
    TRI_ASSERT(_tail == nullptr);
    // set list head and tail to the element
    _head = e;
    _tail = e;
    return;
  }

  if (_tail != nullptr) {
    // adjust list tail
    _tail->_next = e;
  }

  e->_prev = _tail;
  _tail = e;
}

/// @brief create the query cache
QueryCache::QueryCache() : _propertiesLock(), _entriesLock(), _entries() {}

/// @brief destroy the query cache
QueryCache::~QueryCache() { 
  for (unsigned int i = 0; i < NumberOfParts; ++i) {
    invalidate(i);
  }
}

/// @brief return the query cache properties
VPackBuilder QueryCache::properties() {
  MUTEX_LOCKER(mutexLocker, _propertiesLock);

  VPackBuilder builder;
  builder.openObject();
  builder.add("mode", VPackValue(modeString(mode())));
  builder.add("maxResults", VPackValue(MaxResults));
  builder.close();

  return builder;
}

/// @brief return the cache properties
void QueryCache::properties(std::pair<std::string, size_t>& result) {
  MUTEX_LOCKER(mutexLocker, _propertiesLock);

  result.first = modeString(mode());
  result.second = MaxResults;
}

/// @brief set the cache properties
void QueryCache::setProperties(
    std::pair<std::string, size_t> const& properties) {
  MUTEX_LOCKER(mutexLocker, _propertiesLock);

  setMode(properties.first);
  setMaxResults(properties.second);
}

/// @brief test whether the cache might be active
/// this is a quick test that may save the caller from further bothering
/// about the query cache if case it returns `false`
bool QueryCache::mayBeActive() const { return (mode() != CACHE_ALWAYS_OFF); }

/// @brief return whether or not the query cache is enabled
QueryCacheMode QueryCache::mode() const {
  return Mode.load(std::memory_order_relaxed);
}

/// @brief return a string version of the mode
std::string QueryCache::modeString(QueryCacheMode mode) {
  switch (mode) {
    case CACHE_ALWAYS_OFF:
      return "off";
    case CACHE_ALWAYS_ON:
      return "on";
    case CACHE_ON_DEMAND:
      return "demand";
  }

  TRI_ASSERT(false);
  return "off";
}

/// @brief lookup a query result in the cache
QueryCacheResultEntry* QueryCache::lookup(TRI_vocbase_t* vocbase, uint64_t hash,
                                          char const* queryString,
                                          size_t queryStringLength) {
  auto const part = getPart(vocbase);
  READ_LOCKER(readLocker, _entriesLock[part]);

  auto it = _entries[part].find(vocbase);

  if (it == _entries[part].end()) {
    // no entry found for the requested database
    return nullptr;
  }

  return (*it).second->lookup(hash, queryString, queryStringLength);
}

/// @brief store a query in the cache
/// if the call is successful, the cache has taken over ownership for the
/// query result!
QueryCacheResultEntry* QueryCache::store(
    TRI_vocbase_t* vocbase, uint64_t hash, char const* queryString,
    size_t queryStringLength, std::shared_ptr<VPackBuilder> result,
    std::vector<std::string> const& collections) {


  if (!result->slice().isArray()) {
    return nullptr;
  }

  // get the right part of the cache to store the result in
  auto const part = getPart(vocbase);

  // create the cache entry outside the lock
  auto entry = std::make_unique<QueryCacheResultEntry>(
      hash, queryString, queryStringLength, result, collections);

  WRITE_LOCKER(writeLocker, _entriesLock[part]);

  auto it = _entries[part].find(vocbase);

  if (it == _entries[part].end()) {
    // create entry for the current database
    auto db = std::make_unique<QueryCacheDatabaseEntry>();
    it = _entries[part].emplace(vocbase, db.get()).first;
    db.release();
  }

  // store cache entry
  (*it).second->store(hash, entry.get());
  return entry.release();
}

/// @brief invalidate all queries for the given collections
void QueryCache::invalidate(TRI_vocbase_t* vocbase,
                            std::vector<std::string> const& collections) {
  auto const part = getPart(vocbase);
  WRITE_LOCKER(writeLocker, _entriesLock[part]);

  auto it = _entries[part].find(vocbase);

  if (it == _entries[part].end()) {
    return;
  }

  // invalidate while holding the lock
  (*it).second->invalidate(collections);
}

/// @brief invalidate all queries for a particular collection
void QueryCache::invalidate(TRI_vocbase_t* vocbase, std::string const& collection) {
  auto const part = getPart(vocbase);
  WRITE_LOCKER(writeLocker, _entriesLock[part]);

  auto it = _entries[part].find(vocbase);

  if (it == _entries[part].end()) {
    return;
  }

  // invalidate while holding the lock
  (*it).second->invalidate(collection);
}

/// @brief invalidate all queries for a particular database
void QueryCache::invalidate(TRI_vocbase_t* vocbase) {
  QueryCacheDatabaseEntry* databaseQueryCache = nullptr;

  {
    auto const part = getPart(vocbase);
    WRITE_LOCKER(writeLocker, _entriesLock[part]);

    auto it = _entries[part].find(vocbase);

    if (it == _entries[part].end()) {
      return;
    }

    databaseQueryCache = (*it).second;
    _entries[part].erase(it);
  }

  // delete without holding the lock
  TRI_ASSERT(databaseQueryCache != nullptr);
  delete databaseQueryCache;
}

/// @brief invalidate all queries
void QueryCache::invalidate() {
  for (unsigned int i = 0; i < NumberOfParts; ++i) {
    WRITE_LOCKER(writeLocker, _entriesLock[i]);

    // must invalidate all entries now because disabling the cache will turn off
    // cache invalidation when modifying data. turning on the cache later would
    // then
    // lead to invalid results being returned. this can all be prevented by
    // fully
    // clearing the cache
    invalidate(i);
  }
}

/// @brief hashes a query string
uint64_t QueryCache::hashQueryString(char const* queryString,
                                     size_t queryLength) const {
  TRI_ASSERT(queryString != nullptr);

  return fasthash64(queryString, queryLength, 0x3123456789abcdef);
}

/// @brief get the query cache instance
QueryCache* QueryCache::instance() { return &Instance; }

/// @brief enforce maximum number of elements in each database-specific cache
void QueryCache::enforceMaxResults(size_t value) {
  for (unsigned int i = 0; i < NumberOfParts; ++i) {
    WRITE_LOCKER(writeLocker, _entriesLock[i]);

    for (auto& it : _entries[i]) {
      it.second->enforceMaxResults(value);
    }
  }
}

/// @brief determine which lock to use for the cache entries
unsigned int QueryCache::getPart(TRI_vocbase_t const* vocbase) const {
  return static_cast<int>(
      fasthash64(vocbase, sizeof(decltype(vocbase)), 0xf12345678abcdef) %
      NumberOfParts);
}

/// @brief invalidate all entries in the cache part
/// note that the caller of this method must hold the write lock
void QueryCache::invalidate(unsigned int part) {
  for (auto& it : _entries[part]) {
    delete it.second;
  }

  _entries[part].clear();
}

/// @brief sets the maximum number of results in each per-database cache
void QueryCache::setMaxResults(size_t value) {
  if (value == 0) {
    return;
  }

  if (value > MaxResults) {
    enforceMaxResults(value);
  }

  MaxResults = value;
}

/// @brief sets the caching mode
void QueryCache::setMode(QueryCacheMode value) {
  if (value == mode()) {
    // actually no mode change
    return;
  }

  invalidate();

  Mode.store(value, std::memory_order_release);
}

/// @brief enable or disable the query cache
void QueryCache::setMode(std::string const& value) {
  if (value == "demand") {
    setMode(CACHE_ON_DEMAND);
  } else if (value == "on") {
    setMode(CACHE_ALWAYS_ON);
  } else {
    setMode(CACHE_ALWAYS_OFF);
  }
}
