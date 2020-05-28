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

#include "TraverserDocumentCache.h"

#include "Aql/AqlValue.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/debugging.h"
#include "Cache/Cache.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/Common.h"
#include "Cache/Finding.h"
#include "Cluster/ServerState.h"
#include "Graph/EdgeDocumentToken.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

TraverserDocumentCache::TraverserDocumentCache(aql::QueryContext& query,
                                               std::shared_ptr<arangodb::cache::Cache> cache,
                                               BaseOptions* options)
    : TraverserCache(query, options), 
      _cache(std::move(cache)) {
  TRI_ASSERT(_cache != nullptr);
}

TraverserDocumentCache::~TraverserDocumentCache() {
  TRI_ASSERT(_cache != nullptr);
  auto cacheManager = CacheManagerFeature::MANAGER;
  if (cacheManager != nullptr) {
    try {
      cacheManager->destroyCache(_cache);
    } catch (...) {
      // no exceptions allowed here
    }
  }
}

// @brief Only for internal use, Cache::Finding prevents
// the cache from removing this specific object. Should not be retained
// for a longer period of time.
// DO NOT give it to a caller.
cache::Finding TraverserDocumentCache::lookup(arangodb::velocypack::StringRef idString) {
  return _cache->find(idString.data(), static_cast<uint32_t>(idString.length()));
}

VPackSlice TraverserDocumentCache::lookupAndCache(arangodb::velocypack::StringRef id) {
  VPackSlice result = lookupVertexInCollection(id);
  insertIntoCache(id, result);
  return result;
}

// These two do not use the cache.
void TraverserDocumentCache::insertEdgeIntoResult(EdgeDocumentToken const& idToken,
                                                  VPackBuilder& builder) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  builder.add(lookupToken(idToken));
}

void TraverserDocumentCache::insertVertexIntoResult(arangodb::velocypack::StringRef idString, VPackBuilder& builder) {
  auto finding = lookup(idString);
  if (finding.found()) {
    auto val = finding.value();
    VPackSlice slice(val->value());
    // finding makes sure that slice contant stays valid.
    builder.add(slice);
    return;
  }
  // Not in cache. Fetch and insert.
  builder.add(lookupAndCache(idString));
}

aql::AqlValue TraverserDocumentCache::fetchEdgeAqlResult(EdgeDocumentToken const& idToken) {
  TRI_ASSERT(!ServerState::instance()->isCoordinator());
  return aql::AqlValue(lookupToken(idToken));
}

aql::AqlValue TraverserDocumentCache::fetchVertexAqlResult(arangodb::velocypack::StringRef idString) {
  auto finding = lookup(idString);
  if (finding.found()) {
    auto val = finding.value();
    VPackSlice slice(val->value());
    // finding makes sure that slice contant stays valid.
    return aql::AqlValue(slice);
  }
  // Not in cache. Fetch and insert.
  return aql::AqlValue(lookupAndCache(idString));
}

void TraverserDocumentCache::insertIntoCache(arangodb::velocypack::StringRef id,
                                             arangodb::velocypack::Slice const& document) {
  void const* key = id.data();
  auto keySize = static_cast<uint32_t>(id.length());

  void const* resVal = document.begin();
  uint64_t resValSize = static_cast<uint64_t>(document.byteSize());
  std::unique_ptr<cache::CachedValue> value(
      cache::CachedValue::construct(key, keySize, resVal, resValSize));

  if (value) {
    auto result = _cache->insert(value.get());
    if (!result.ok()) {
      LOG_TOPIC("9de3a", DEBUG, Logger::GRAPHS) << "Insert failed";
    } else {
      // Cache is responsible.
      // If this failed, well we do not store it and read it again next time.
      value.release();
    }
  }
}
