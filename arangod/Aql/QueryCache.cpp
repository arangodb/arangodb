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

#include "QueryCache.h"
#include "Basics/fasthash.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

namespace {
/// @brief singleton instance of the query cache
static arangodb::aql::QueryCache instance;

/// @brief maximum number of results in each per-database cache
static std::atomic<size_t> maxResults(128);  // default value. can be changed later

static std::atomic<size_t> maxEntrySize(16 * 1024 * 1024);  // default value. can be changed later

/// @brief whether or not the cache is enabled
static std::atomic<arangodb::aql::QueryCacheMode> mode(CACHE_ON_DEMAND);
}

/// @brief create a cache entry
QueryCacheResultEntry::QueryCacheResultEntry(
    uint64_t hash, QueryString const& queryString,
    std::shared_ptr<VPackBuilder> const& queryResult, 
    std::vector<std::string> const& dataSources)
    : _hash(hash),
      _queryString(queryString.data(), queryString.size()),
      _queryResult(queryResult),
      _dataSources(dataSources),
      _size(_queryString.size()),
      _hits(0),
      _prev(nullptr),
      _next(nullptr) {
  // add result size
  try {
    if (_queryResult) {
      _size += _queryResult->size();
    }
  } catch (...) {}
}

/// @brief create a database-specific cache
QueryCacheDatabaseEntry::QueryCacheDatabaseEntry()
    : _entriesByHash(),
      _head(nullptr),
      _tail(nullptr),
      _numElements(0) {
  _entriesByHash.reserve(128);
  _entriesByDataSource.reserve(16);
}

/// @brief destroy a database-specific cache
QueryCacheDatabaseEntry::~QueryCacheDatabaseEntry() {
  _entriesByHash.clear();
  _entriesByDataSource.clear();
}

/// @brief return the query cache contents
void QueryCacheDatabaseEntry::queriesToVelocyPack(VPackBuilder& builder) const {
  for (auto const& it : _entriesByHash) {
    builder.openObject();
    builder.add("query", VPackValue(it.second->_queryString));
    builder.add("size", VPackValue(it.second->_size));
    builder.add("hits", VPackValue(it.second->_hits.load()));
   
    builder.add("dataSources", VPackValue(VPackValueType::Array));
    for (auto const& ds : it.second->_dataSources) {
      builder.add(VPackValue(ds));
    }
    builder.close();
   
    builder.close();
  }
}

/// @brief lookup a query result in the database-specific cache
std::shared_ptr<QueryCacheResultEntry> QueryCacheDatabaseEntry::lookup(
    uint64_t hash, QueryString const& queryString) {
  auto it = _entriesByHash.find(hash);

  if (it == _entriesByHash.end()) {
    // not found in cache
    return nullptr;
  }

  // found some result in cache

  if (queryString.size() != (*it).second->_queryString.size() ||
      memcmp(queryString.data(), (*it).second->_queryString.data(), queryString.size()) != 0) {
    // found something, but obviously the result of a different query with the
    // same hash
    return nullptr;
  }

  (*it).second->increaseHits();

  // found an entry
  return (*it).second;
}

/// @brief store a query result in the database-specific cache
void QueryCacheDatabaseEntry::store(uint64_t hash,
                                    std::shared_ptr<QueryCacheResultEntry> entry) {
  // insert entry into the cache
  if (!_entriesByHash.emplace(hash, entry).second) {
    // remove previous entry
    auto it = _entriesByHash.find(hash);
    TRI_ASSERT(it != _entriesByHash.end());
    auto previous = (*it).second;
    removeDatasources(previous.get());
    unlink(previous.get());
    _entriesByHash.erase(it);

    // and insert again
    _entriesByHash.emplace(hash, entry);
  }

  try {
    for (auto const& it : entry->_dataSources) {
      auto it2 = _entriesByDataSource.find(it);

      if (it2 == _entriesByDataSource.end()) {
        // no entry found for data source. now create it
        _entriesByDataSource.emplace(it, std::unordered_set<uint64_t>{hash});
      } else {
        // there already was an entry for this data source
        (*it2).second.emplace(hash);
      }
    }
  } catch (...) {
    // rollback

    // remove from data sources
    for (auto const& it : entry->_dataSources) {
      auto it2 = _entriesByDataSource.find(it);

      if (it2 != _entriesByDataSource.end()) {
        (*it2).second.erase(hash);
      }
    }

    // finally remove entry itself from hash table
    auto it = _entriesByHash.find(hash);
    TRI_ASSERT(it != _entriesByHash.end());
    auto previous = (*it).second;
    _entriesByHash.erase(it);
    unlink(previous.get());
    throw;
  }

  link(entry.get());

  size_t mr = ::maxResults.load();
  enforceMaxResults(mr);

  TRI_ASSERT(_numElements <= mr);
  TRI_ASSERT(_head != nullptr);
  TRI_ASSERT(_tail != nullptr);
  TRI_ASSERT(_tail == entry.get());
  TRI_ASSERT(entry->_next == nullptr);
}

/// @brief invalidate all entries for the given data sources in the
/// database-specific cache
void QueryCacheDatabaseEntry::invalidate(
    std::vector<std::string> const& dataSources) {
  for (auto const& it : dataSources) {
    invalidate(it);
  }
}

/// @brief invalidate all entries for a data source in the database-specific
/// cache
void QueryCacheDatabaseEntry::invalidate(std::string const& dataSource) {
  auto it = _entriesByDataSource.find(dataSource);

  if (it == _entriesByDataSource.end()) {
    return;
  }

  for (auto& it2 : (*it).second) {
    auto it3 = _entriesByHash.find(it2);

    if (it3 != _entriesByHash.end()) {
      // remove entry from the linked list
      auto entry = (*it3).second;
      unlink(entry.get());

      // erase it from hash table
      _entriesByHash.erase(it3);
    }
  }

  _entriesByDataSource.erase(it);
}

/// @brief enforce maximum number of results
/// must be called under the shard's lock
void QueryCacheDatabaseEntry::enforceMaxResults(size_t value) {
  while (_numElements > value) {
    // too many elements. now wipe the first element from the list

    // copy old _head value as unlink() will change it...
    auto head = _head;
    removeDatasources(head);
    unlink(head);
    auto it = _entriesByHash.find(head->_hash);
    TRI_ASSERT(it != _entriesByHash.end());
    _entriesByHash.erase(it);
  }
}

/// @brief enforce maximum per-entry size
/// must be called under the shard's lock
void QueryCacheDatabaseEntry::enforceMaxEntrySize(size_t value) {
  for (auto it = _entriesByHash.begin(); it != _entriesByHash.end(); /* no hoisting */) {
    auto const& entry = (*it).second.get();

    if (entry->_size > value) {
      removeDatasources(entry);
      unlink(entry);
      it = _entriesByHash.erase(it);
    } else {
      // keep the entry
      ++it;
    }
  }
}

void QueryCacheDatabaseEntry::removeDatasources(QueryCacheResultEntry const* e) {
  for (auto const& ds : e->_dataSources) {
    auto it = _entriesByDataSource.find(ds);

    if (it != _entriesByDataSource.end()) {
      (*it).second.erase(e->_hash);
    }
  }
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
  for (unsigned int i = 0; i < numberOfParts; ++i) {
    invalidate(i);
  }
}

/// @brief return the query cache properties
void QueryCache::toVelocyPack(VPackBuilder& builder) const {
  MUTEX_LOCKER(mutexLocker, _propertiesLock);

  builder.openObject();
  builder.add("mode", VPackValue(modeString(mode())));
  builder.add("maxResults", VPackValue(::maxResults.load()));
  builder.add("maxEntrySize", VPackValue(::maxEntrySize.load()));
  builder.close();
}
  
/// @brief return the query cache properties
QueryCacheProperties QueryCache::properties() const {
  MUTEX_LOCKER(mutexLocker, _propertiesLock);

  return QueryCacheProperties{ ::mode.load(), ::maxResults.load(), ::maxEntrySize.load() }; 
}

/// @brief set the cache properties
void QueryCache::properties(QueryCacheProperties const& properties) {
  MUTEX_LOCKER(mutexLocker, _propertiesLock);

  setMode(properties.mode);
  setMaxResults(properties.maxEntries);
  setMaxEntrySize(properties.maxEntrySize);
}

/// @brief test whether the cache might be active
/// this is a quick test that may save the caller from further bothering
/// about the query cache if case it returns `false`
bool QueryCache::mayBeActive() const { return (mode() != CACHE_ALWAYS_OFF); }

/// @brief return whether or not the query cache is enabled
QueryCacheMode QueryCache::mode() const {
  return ::mode.load(std::memory_order_relaxed);
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

/// @brief return a string version of the mode
QueryCacheMode QueryCache::modeString(std::string const& mode) {
  if (mode == "on") {
    return CACHE_ALWAYS_ON;
  }
  if (mode == "demand") {
    return CACHE_ON_DEMAND;
  }

  return CACHE_ALWAYS_OFF;
}

/// @brief lookup a query result in the cache
std::shared_ptr<QueryCacheResultEntry> QueryCache::lookup(TRI_vocbase_t* vocbase, uint64_t hash,
                                                          QueryString const& queryString) {
  auto const part = getPart(vocbase);
  READ_LOCKER(readLocker, _entriesLock[part]);

  auto it = _entries[part].find(vocbase);

  if (it == _entries[part].end()) {
    // no entry found for the requested database
    return nullptr;
  }

  return (*it).second->lookup(hash, queryString);
}

/// @brief store a query in the cache
void QueryCache::store(TRI_vocbase_t* vocbase, std::shared_ptr<QueryCacheResultEntry> entry) {
  TRI_ASSERT(entry != nullptr);

  if (entry->_size > ::maxEntrySize.load()) {
    // entry is too big
    return;
  }
  
  // get the right part of the cache to store the result in
  auto const part = getPart(vocbase);
  WRITE_LOCKER(writeLocker, _entriesLock[part]);

  auto it = _entries[part].find(vocbase);

  if (it == _entries[part].end()) {
    // create entry for the current database
    auto db = std::make_unique<QueryCacheDatabaseEntry>();
    it = _entries[part].emplace(vocbase, std::move(db)).first;
  }

  // store cache entry
  (*it).second->store(entry->_hash, entry);
}

/// @brief invalidate all queries for the given data sources
void QueryCache::invalidate(TRI_vocbase_t* vocbase,
                            std::vector<std::string> const& dataSources) {
  auto const part = getPart(vocbase);
  WRITE_LOCKER(writeLocker, _entriesLock[part]);

  auto it = _entries[part].find(vocbase);

  if (it == _entries[part].end()) {
    return;
  }

  // invalidate while holding the lock
  (*it).second->invalidate(dataSources);
}

/// @brief invalidate all queries for a particular data source
void QueryCache::invalidate(TRI_vocbase_t* vocbase, std::string const& dataSource) {
  auto const part = getPart(vocbase);
  WRITE_LOCKER(writeLocker, _entriesLock[part]);

  auto it = _entries[part].find(vocbase);

  if (it == _entries[part].end()) {
    return;
  }

  // invalidate while holding the lock
  (*it).second->invalidate(dataSource);
}

/// @brief invalidate all queries for a particular database
void QueryCache::invalidate(TRI_vocbase_t* vocbase) {
  std::unique_ptr<QueryCacheDatabaseEntry> databaseQueryCache;

  {
    auto const part = getPart(vocbase);
    WRITE_LOCKER(writeLocker, _entriesLock[part]);

    auto it = _entries[part].find(vocbase);

    if (it == _entries[part].end()) {
      return;
    }

    databaseQueryCache = std::move((*it).second);
    _entries[part].erase(it);
  }

  // delete without holding the lock
  TRI_ASSERT(databaseQueryCache != nullptr);
}

/// @brief invalidate all queries
void QueryCache::invalidate() {
  for (unsigned int i = 0; i < numberOfParts; ++i) {
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

/// @brief return the query cache contents
void QueryCache::queriesToVelocyPack(VPackBuilder& builder) const {
  builder.openArray();

  for (unsigned int i = 0; i < numberOfParts; ++i) {
    READ_LOCKER(writeLocker, _entriesLock[i]);

    for (auto& it : _entries[i]) {
      it.second->queriesToVelocyPack(builder);
    }
  }

  builder.close();
}

/// @brief get the query cache instance
QueryCache* QueryCache::instance() { return &::instance; }

/// @brief enforce maximum number of elements in each database-specific cache
void QueryCache::enforceMaxResults(size_t value) {
  for (unsigned int i = 0; i < numberOfParts; ++i) {
    WRITE_LOCKER(writeLocker, _entriesLock[i]);

    for (auto& it : _entries[i]) {
      it.second->enforceMaxResults(value);
    }
  }
}

/// @brief enforce maximum size of individual elements in each database-specific cache
void QueryCache::enforceMaxEntrySize(size_t value) {
  for (unsigned int i = 0; i < numberOfParts; ++i) {
    WRITE_LOCKER(writeLocker, _entriesLock[i]);

    for (auto& it : _entries[i]) {
      it.second->enforceMaxEntrySize(value);
    }
  }
}

/// @brief determine which lock to use for the cache entries
unsigned int QueryCache::getPart(TRI_vocbase_t const* vocbase) const {
  return static_cast<int>(
      fasthash64(&vocbase, sizeof(TRI_vocbase_t const*), 0xf12345678abcdef) %
      numberOfParts);
}

/// @brief invalidate all entries in the cache part
/// note that the caller of this method must hold the write lock
void QueryCache::invalidate(unsigned int part) {
  _entries[part].clear();
}

/// @brief sets the maximum number of results in each per-database cache
void QueryCache::setMaxResults(size_t value) {
  if (value == 0) {
    return;
  }

  size_t mr = ::maxResults.load();

  if (value < mr) {
    enforceMaxResults(value);
  }

  ::maxResults.store(value);
}

/// @brief sets the maximum size of individual result entries in each per-database cache
void QueryCache::setMaxEntrySize(size_t value) {
  if (value == 0) {
    return;
  }

  size_t mr = ::maxEntrySize.load();

  if (value < mr) {
    enforceMaxEntrySize(value);
  }
  
  ::maxEntrySize.store(value);
}

/// @brief sets the caching mode
void QueryCache::setMode(QueryCacheMode value) {
  if (value == mode()) {
    // actually no mode change
    return;
  }

  invalidate();

  ::mode.store(value, std::memory_order_release);
}
