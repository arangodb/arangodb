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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "QueryCache.h"
#include "Basics/Exceptions.h"
#include "Basics/MutexLocker.h"
#include "Basics/ReadLocker.h"
#include "Basics/tryEmplaceHelper.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/WriteLocker.h"
#include "Basics/conversions.h"
#include "Basics/fasthash.h"
#include "Basics/system-functions.h"
#include "Utils/ExecContext.h"
#include "VocBase/LogicalDataSource.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb::aql;

namespace {
/// @brief singleton instance of the query cache
static arangodb::aql::QueryCache instance;

/// @brief whether or not the cache is enabled
static std::atomic<arangodb::aql::QueryCacheMode> mode(CACHE_ON_DEMAND);

/// @brief maximum number of results in each per-database cache
static std::atomic<size_t> maxResultsCount(128);  // default value. can be changed later

/// @brief maximum cumulated size of results in each per-database cache
static std::atomic<size_t> maxResultsSize(256 * 1024 * 1024);  // default value. can be changed later

/// @brief maximum size of an individual cache entry
static std::atomic<size_t> maxEntrySize(16 * 1024 * 1024);  // default value. can be changed later

/// @brief whether or not to include results of system collections
static std::atomic<bool> includeSystem(false);  // default value. can be changed later

/// @brief whether or not the query cache will return bind vars in its list of
/// cached results
static bool showBindVars = true;  // will be set once on startup. cannot be changed at runtime
}  // namespace

/// @brief create a cache entry
QueryCacheResultEntry::QueryCacheResultEntry(uint64_t hash, QueryString const& queryString,
                                             std::shared_ptr<VPackBuilder> const& queryResult,
                                             std::shared_ptr<VPackBuilder> const& bindVars,
                                             std::unordered_map<std::string, std::string>&& dataSources
)
    : _hash(hash),
      _queryString(queryString.data(), queryString.size()),
      _queryResult(queryResult),
      _bindVars(bindVars),
      _dataSources(std::move(dataSources)),
      _size(_queryString.size()),
      _rows(0),
      _hits(0),
      _stamp(0.0),
      _prev(nullptr),
      _next(nullptr) {
  // add result size
  try {
    if (_queryResult) {
      _size += _queryResult->size();
      _rows = _queryResult->slice().length();
    }
    if (_bindVars) {
      _size += _bindVars->size();
    }
  } catch (...) {
  }
}

double QueryCacheResultEntry::executionTime() const {
  if (!_stats) {
    return -1.0;
  }

  try {
    VPackSlice s = _stats->slice();
    if (!s.isObject()) {
      return -1.0;
    }
    s = s.get("stats");
    if (!s.isObject()) {
      return -1.0;
    }
    s = s.get("executionTime");
    if (!s.isNumber()) {
      return -1.0;
    }
    return s.getNumericValue<double>();
  } catch (...) {
  }

  return -1.0;
}

void QueryCacheResultEntry::toVelocyPack(VPackBuilder& builder) const {
  builder.openObject();

  builder.add("hash", VPackValue(std::to_string(_hash)));
  builder.add("query", VPackValue(_queryString));

  if (::showBindVars) {
    if (_bindVars && !_bindVars->slice().isNone()) {
      builder.add("bindVars", _bindVars->slice());
    } else {
      builder.add("bindVars", VPackSlice::emptyObjectSlice());
    }
  }

  builder.add("size", VPackValue(_size));
  builder.add("results", VPackValue(_rows));
  builder.add("hits", VPackValue(_hits.load()));

  double executionTime = this->executionTime();

  if (executionTime < 0.0) {
    builder.add("runTime", VPackValue(VPackValueType::Null));
  } else {
    builder.add("runTime", VPackValue(executionTime));
  }

  auto timeString = TRI_StringTimeStamp(_stamp, false);

  builder.add("started", VPackValue(timeString));

  builder.add("dataSources", VPackValue(VPackValueType::Array));

  // emit all datasource names
  for (auto const& it : _dataSources) {
    builder.add(arangodb::velocypack::Value(it.second));
  }

  builder.close();

  builder.close();
}

bool QueryCacheResultEntry::currentUserHasPermissions() const {
  ExecContext const& exec = ExecContext::current();

  // got a result from the query cache
  if (!exec.isSuperuser()) {
    for (auto& dataSource : _dataSources) {
      auto const& dataSourceName = dataSource.second;

      if (!exec.canUseCollection(dataSourceName, auth::Level::RO)) {
        // cannot use query cache result because of permissions
        return false;
      }
    }
  }
  
  return true;
}

/// @brief create a database-specific cache
QueryCacheDatabaseEntry::QueryCacheDatabaseEntry()
    : _entriesByHash(), _head(nullptr), _tail(nullptr), _numResults(0), _sizeResults(0) {
  _entriesByHash.reserve(128);
  _entriesByDataSourceGuid.reserve(16);
}

/// @brief destroy a database-specific cache
QueryCacheDatabaseEntry::~QueryCacheDatabaseEntry() {
  _entriesByHash.clear();
  _entriesByDataSourceGuid.clear();
}

/// @brief return the query cache contents
void QueryCacheDatabaseEntry::queriesToVelocyPack(VPackBuilder& builder) const {
  for (auto const& it : _entriesByHash) {
    QueryCacheResultEntry const* entry = it.second.get();
    TRI_ASSERT(entry != nullptr);

    entry->toVelocyPack(builder);
  }
}

/// @brief lookup a query result in the database-specific cache
std::shared_ptr<QueryCacheResultEntry> QueryCacheDatabaseEntry::lookup(
    uint64_t hash, QueryString const& queryString,
    std::shared_ptr<VPackBuilder> const& bindVars) const {
  auto it = _entriesByHash.find(hash);

  if (it == _entriesByHash.end()) {
    // not found in cache
    return nullptr;
  }

  // found some result in cache
  auto entry = (*it).second.get();

  if (queryString.size() != entry->_queryString.size() ||
      memcmp(queryString.data(), entry->_queryString.data(), queryString.size()) != 0) {
    // found something, but obviously the result of a different query with the
    // same hash
    return nullptr;
  }

  // compare bind variables
  VPackSlice entryBindVars = VPackSlice::emptyObjectSlice();
  if (entry->_bindVars != nullptr) {
    entryBindVars = entry->_bindVars->slice();
  }
  VPackValueLength entryLength = entryBindVars.length();

  VPackSlice lookupBindVars = VPackSlice::emptyObjectSlice();
  if (bindVars != nullptr) {
    lookupBindVars = bindVars->slice();
  }
  VPackValueLength lookupLength = lookupBindVars.length();

  if (entryLength > 0 || lookupLength > 0) {
    if (entryLength != lookupLength) {
      // different number of bind variables
      return nullptr;
    }

    if (!basics::VelocyPackHelper::equal(entryBindVars, lookupBindVars, false)) {
      // different bind variables
      return nullptr;
    }
  }

  // all equal -> hit!
  entry->increaseHits();

  // found an entry
  return (*it).second;
}

/// @brief store a query result in the database-specific cache
void QueryCacheDatabaseEntry::store(std::shared_ptr<QueryCacheResultEntry>&& entry,
                                    size_t allowedMaxResultsCount,
                                    size_t allowedMaxResultsSize) {
  auto* e = entry.get();

  // make room in the cache so the new entry will definitely fit
  enforceMaxResults(allowedMaxResultsCount - 1, allowedMaxResultsSize - e->_size);

  // insert entry into the cache
  uint64_t hash = e->_hash;
  auto result = _entriesByHash.insert({hash, entry});
  if (!result.second) {
    // remove previous entry
    auto& previous = result.first->second;
    removeDatasources(previous.get());
    unlink(previous.get());

    // update with the new entry
    result.first->second = std::move(entry);
  }

  try {
    for (auto const& it : e->_dataSources) {
      auto& ref = _entriesByDataSourceGuid[it.first];
      ref.first = TRI_vocbase_t::IsSystemName(it.second);
      ref.second.emplace(hash);
    }
  } catch (...) {
    // rollback

    // remove from data sources
    for (auto const& it : e->_dataSources) {
      auto itr2 = _entriesByDataSourceGuid.find(it.first);

      if (itr2 != _entriesByDataSourceGuid.end()) {
        itr2->second.second.erase(hash);
      }
    }

    // finally remove entry itself from hash table
    _entriesByHash.erase(hash);
    throw;
  }

  link(e);

  TRI_ASSERT(_numResults <= allowedMaxResultsCount);
  TRI_ASSERT(_sizeResults <= allowedMaxResultsSize);
  TRI_ASSERT(_head != nullptr);
  TRI_ASSERT(_tail != nullptr);
  TRI_ASSERT(_tail == e);
  TRI_ASSERT(e->_next == nullptr);
}

/// @brief invalidate all entries for the given data sources in the
/// database-specific cache
void QueryCacheDatabaseEntry::invalidate(std::vector<std::string> const& dataSourceGuids) {
  for (auto const& it: dataSourceGuids) {
    invalidate(it);
  }
}

/// @brief invalidate all entries for a data source in the database-specific
/// cache
void QueryCacheDatabaseEntry::invalidate(std::string const& dataSourceGuid) {
  auto itr = _entriesByDataSourceGuid.find(dataSourceGuid);

  if (itr == _entriesByDataSourceGuid.end()) {
    return;
  }

  for (auto& it2 : itr->second.second) {
    auto it3 = _entriesByHash.find(it2);

    if (it3 != _entriesByHash.end()) {
      // remove entry from the linked list
      auto entry = (*it3).second;
      unlink(entry.get());

      // erase it from hash table
      _entriesByHash.erase(it3);
    }
  }

  _entriesByDataSourceGuid.erase(itr);
}

/// @brief enforce maximum number of results
/// must be called under the shard's lock
void QueryCacheDatabaseEntry::enforceMaxResults(size_t numResults, size_t sizeResults) {
  while (_numResults > numResults || _sizeResults > sizeResults) {
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
  for (auto it = _entriesByHash.begin(); it != _entriesByHash.end();
       /* no hoisting */) {
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

/// @brief exclude all data from system collections
/// must be called under the shard's lock
void QueryCacheDatabaseEntry::excludeSystem() {
  for (auto itr = _entriesByDataSourceGuid.begin(); // setup
       itr != _entriesByDataSourceGuid.end(); // condition
       /* no hoisting */) {
    if (!itr->second.first) {
      // not a system collection
      ++itr;
    } else {
      for (auto const& hash : itr->second.second) {
        auto it2 = _entriesByHash.find(hash);

        if (it2 != _entriesByHash.end()) {
          auto* entry = (*it2).second.get();
          unlink(entry);
          _entriesByHash.erase(it2);
        }
      }

      itr = _entriesByDataSourceGuid.erase(itr);
    }
  }
}

void QueryCacheDatabaseEntry::removeDatasources(QueryCacheResultEntry const* e) {
  for (auto const& it : e->_dataSources) {
    auto itr = _entriesByDataSourceGuid.find(it.first);

    if (itr != _entriesByDataSourceGuid.end()) {
      itr->second.second.erase(e->_hash);
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

  TRI_ASSERT(_numResults > 0);
  --_numResults;

  TRI_ASSERT(_sizeResults >= e->_size);
  _sizeResults -= e->_size;
}

/// @brief link the result entry to the end of the list
void QueryCacheDatabaseEntry::link(QueryCacheResultEntry* e) {
  ++_numResults;
  _sizeResults += e->_size;

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
QueryCache::QueryCache() = default;

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
  builder.add("maxResults", VPackValue(::maxResultsCount.load()));
  builder.add("maxResultsSize", VPackValue(::maxResultsSize.load()));
  builder.add("maxEntrySize", VPackValue(::maxEntrySize.load()));
  builder.add("includeSystem", VPackValue(::includeSystem.load()));
  builder.close();
}

/// @brief return the query cache properties
QueryCacheProperties QueryCache::properties() const {
  MUTEX_LOCKER(mutexLocker, _propertiesLock);

  return QueryCacheProperties{::mode.load(),           ::maxResultsCount.load(),
                              ::maxResultsSize.load(), ::maxEntrySize.load(),
                              ::includeSystem.load(),  ::showBindVars};
}

/// @brief set the cache properties
void QueryCache::properties(QueryCacheProperties const& properties) {
  MUTEX_LOCKER(mutexLocker, _propertiesLock);

  setMode(properties.mode);
  setMaxResults(properties.maxResultsCount, properties.maxResultsSize);
  setMaxEntrySize(properties.maxEntrySize);
  setIncludeSystem(properties.includeSystem);
  ::showBindVars = properties.showBindVars;
}

/// @brief set the cache properties
void QueryCache::properties(VPackSlice const& properties) {
  if (!properties.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER, "expecting Object for query cache properties");
  }

  MUTEX_LOCKER(mutexLocker, _propertiesLock);

  auto mode = ::mode.load();
  auto maxResultsCount = ::maxResultsCount.load();
  auto maxResultsSize = ::maxResultsSize.load();
  auto maxEntrySize = ::maxEntrySize.load();
  auto includeSystem = ::includeSystem.load();

  VPackSlice v = properties.get("mode");
  if (v.isString()) {
    mode = modeString(v.copyString());
  }

  v = properties.get("maxResults");
  if (v.isNumber()) {
    int64_t value = v.getNumericValue<int64_t>();
    if (value <= 0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid value for maxResults");
    }
    maxResultsCount = v.getNumericValue<size_t>();
  }

  v = properties.get("maxResultsSize");
  if (v.isNumber()) {
    int64_t value = v.getNumericValue<int64_t>();
    if (value <= 0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid value for maxResultsSize");
    }
    maxResultsSize = v.getNumericValue<size_t>();
  }

  v = properties.get("maxEntrySize");
  if (v.isNumber()) {
    int64_t value = v.getNumericValue<int64_t>();
    if (value <= 0) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "invalid value for maxEntrySize");
    }
    maxEntrySize = v.getNumericValue<size_t>();
  }

  v = properties.get("includeSystem");
  if (v.isBoolean()) {
    includeSystem = v.getBoolean();
  }

  setMode(mode);
  setMaxResults(maxResultsCount, maxResultsSize);
  setMaxEntrySize(maxEntrySize);
  setIncludeSystem(includeSystem);
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

/// @brief return the internal type for a mode string
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
std::shared_ptr<QueryCacheResultEntry> QueryCache::lookup(
    TRI_vocbase_t* vocbase, uint64_t hash, QueryString const& queryString,
    std::shared_ptr<VPackBuilder> const& bindVars) const {
  auto const part = getPart(vocbase);
  READ_LOCKER(readLocker, _entriesLock[part]);

  auto& entry = _entries[part];
  auto it = entry.find(vocbase);

  if (it == entry.end()) {
    // no entry found for the requested database
    return nullptr;
  }

  return (*it).second->lookup(hash, queryString, bindVars);
}

/// @brief store a query in the cache
void QueryCache::store(TRI_vocbase_t* vocbase, std::shared_ptr<QueryCacheResultEntry> entry) {
  TRI_ASSERT(entry != nullptr);
  auto* e = entry.get();

  if (e->_size > ::maxEntrySize.load()) {
    // entry is too big
    return;
  }

  if (!::includeSystem.load()) {
    // check if we need to exclude the entry because it refers to system
    // collections
    for (auto const& it : e->_dataSources) {
      if (TRI_vocbase_t::IsSystemName(it.second)) {
        // refers to a system collection...
        return;
      }
    }
  }

  size_t const allowedMaxResultsCount = ::maxResultsCount.load();
  size_t const allowedMaxResultsSize = ::maxResultsSize.load();

  if (allowedMaxResultsCount == 0) {
    // cache has only space for 0 entries...
    return;
  }

  if (e->_size > allowedMaxResultsSize) {
    // entry is too big
    return;
  }

  // set insertion time (wall-clock time, only used for displaying it later)
  e->_stamp = TRI_microtime();

  // get the right part of the cache to store the result in
  auto const part = getPart(vocbase);
  WRITE_LOCKER(writeLocker, _entriesLock[part]);
  auto it = _entries[part].try_emplace(
      vocbase, 
      arangodb::lazyConstruct([&]{
        return std::make_unique<QueryCacheDatabaseEntry>();
      })
  ).first;
  // store cache entry
  (*it).second->store(std::move(entry), allowedMaxResultsCount, allowedMaxResultsSize);
}

/// @brief invalidate all queries for the given data sources
void QueryCache::invalidate(TRI_vocbase_t* vocbase, std::vector<std::string> const& dataSourceGuids) {
  auto const part = getPart(vocbase);
  WRITE_LOCKER(writeLocker, _entriesLock[part]);

  auto& entry = _entries[part];
  auto it = entry.find(vocbase);

  if (it == entry.end()) {
    return;
  }

  // invalidate while holding the lock
  it->second->invalidate(dataSourceGuids);
}

/// @brief invalidate all queries for a particular data source
void QueryCache::invalidate(TRI_vocbase_t* vocbase, std::string const& dataSourceGuid) {
  auto const part = getPart(vocbase);
  WRITE_LOCKER(writeLocker, _entriesLock[part]);

  auto& entry = _entries[part];
  auto it = entry.find(vocbase);

  if (it == entry.end()) {
    return;
  }

  // invalidate while holding the lock
  it->second->invalidate(dataSourceGuid);
}

/// @brief invalidate all queries for a particular database
void QueryCache::invalidate(TRI_vocbase_t* vocbase) {
  std::unique_ptr<QueryCacheDatabaseEntry> databaseQueryCache;

  {
    auto const part = getPart(vocbase);
    WRITE_LOCKER(writeLocker, _entriesLock[part]);

    auto& entry = _entries[part];
    auto it = entry.find(vocbase);

    if (it == entry.end()) {
      return;
    }

    databaseQueryCache = std::move((*it).second);
    entry.erase(it);
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
void QueryCache::queriesToVelocyPack(TRI_vocbase_t* vocbase, VPackBuilder& builder) const {
  builder.openArray();

  {
    auto const part = getPart(vocbase);
    READ_LOCKER(readLocker, _entriesLock[part]);

    auto& entry = _entries[part];
    auto it = entry.find(vocbase);

    if (it != entry.end()) {
      (*it).second->queriesToVelocyPack(builder);
    }
  }

  builder.close();
}

/// @brief get the query cache instance
QueryCache* QueryCache::instance() { return &::instance; }

/// @brief enforce maximum number of elements in each database-specific cache
void QueryCache::enforceMaxResults(size_t numResults, size_t sizeResults) {
  for (unsigned int i = 0; i < numberOfParts; ++i) {
    WRITE_LOCKER(writeLocker, _entriesLock[i]);

    for (auto& it : _entries[i]) {
      it.second->enforceMaxResults(numResults, sizeResults);
    }
  }
}

/// @brief enforce maximum size of individual elements in each database-specific
/// cache
void QueryCache::enforceMaxEntrySize(size_t value) {
  for (unsigned int i = 0; i < numberOfParts; ++i) {
    WRITE_LOCKER(writeLocker, _entriesLock[i]);

    for (auto& it : _entries[i]) {
      it.second->enforceMaxEntrySize(value);
    }
  }
}

/// @brief exclude all data from system collections
void QueryCache::excludeSystem() {
  for (unsigned int i = 0; i < numberOfParts; ++i) {
    WRITE_LOCKER(writeLocker, _entriesLock[i]);

    for (auto& it : _entries[i]) {
      it.second->excludeSystem();
    }
  }
}

/// @brief determine which lock to use for the cache entries
unsigned int QueryCache::getPart(TRI_vocbase_t const* vocbase) const {
  uint64_t v = uintptr_t(vocbase);
  return static_cast<int>(fasthash64_uint64(v, 0xf12345678abcdef) % numberOfParts);
}

/// @brief invalidate all entries in the cache part
/// note that the caller of this method must hold the write lock
void QueryCache::invalidate(unsigned int part) { _entries[part].clear(); }

/// @brief sets the maximum number of results in each per-database cache
void QueryCache::setMaxResults(size_t numResults, size_t sizeResults) {
  if (numResults == 0 || sizeResults == 0) {
    return;
  }

  size_t mr = ::maxResultsCount.load();
  size_t ms = ::maxResultsSize.load();

  if (numResults < mr || sizeResults < ms) {
    enforceMaxResults(numResults, sizeResults);
  }

  ::maxResultsCount.store(numResults);
  ::maxResultsSize.store(sizeResults);
}

/// @brief sets the maximum size of individual result entries in each
/// per-database cache
void QueryCache::setMaxEntrySize(size_t value) {
  if (value == 0) {
    return;
  }

  size_t v = ::maxEntrySize.load();

  if (value < v) {
    enforceMaxEntrySize(value);
  }

  ::maxEntrySize.store(value);
}

/// @brief sets the "include-system" flag
void QueryCache::setIncludeSystem(bool value) {
  if (!value && ::includeSystem.load()) {
    excludeSystem();
  }

  ::includeSystem.store(value);
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
