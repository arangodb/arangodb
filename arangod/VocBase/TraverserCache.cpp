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

#include "TraverserCache.h"

#include "Basics/StringHeap.h"
#include "Basics/StringRef.h"
#include "Basics/VelocyPackHelper.h"

#include "Cache/Common.h"
#include "Cache/Cache.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/Finding.h"

#include "Logger/Logger.h"
#include "Transaction/Methods.h"
#include "VocBase/ManagedDocumentResult.h"
#include "Aql/AqlValue.h"

#include <velocypack/Builder.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::traverser;

TraverserCache::TraverserCache(transaction::Methods* trx)
    : _cache(nullptr), _mmdr(new ManagedDocumentResult{}),
      _trx(trx), _insertedDocuments(0),
      _stringHeap(new StringHeap{4096}) /* arbitrary block-size may be adjusted for perforamnce */ {
  auto cacheManager = CacheManagerFeature::MANAGER;
  TRI_ASSERT(cacheManager != nullptr);
  _cache = cacheManager->createCache(cache::CacheType::Plain);
}

TraverserCache::~TraverserCache() {
  auto cacheManager = CacheManagerFeature::MANAGER;
  cacheManager->destroyCache(_cache);
}

// @brief Only for internal use, Cache::Finding prevents
// the cache from removing this specific object. Should not be retained
// for a longer period of time.
// DO NOT give it to a caller.
cache::Finding TraverserCache::lookup(StringRef idString) {
  VPackValueLength keySize = idString.length();
  void const* key = idString.data();
  //uint32_t keySize = static_cast<uint32_t>(idString.byteSize());
  return _cache->find(key, (uint32_t)keySize);
}

VPackSlice TraverserCache::lookupInCollection(StringRef id) {
  size_t pos = id.find('/');
  if (pos == std::string::npos) {
    // Invalid input. If we get here somehow we managed to store invalid _from/_to
    // values or the traverser did a let an illegal start through
    TRI_ASSERT(false);
    return basics::VelocyPackHelper::NullValue();
  }
  int res = _trx->documentFastPathLocal(id.substr(0, pos).toString(),
                                        id.substr(pos + 1).toString(), *_mmdr);

  if (res != TRI_ERROR_NO_ERROR && res != TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
    // ok we are in a rather bad state. Better throw and abort.
    THROW_ARANGO_EXCEPTION(res);
  }

  VPackSlice result;
  if (res == TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND) {
    // This is expected, we may have dangling edges. Interpret as NULL
    result = basics::VelocyPackHelper::NullValue();
  } else {
    result = VPackSlice(_mmdr->vpack());
  }

  void const* key = id.begin();
  VPackValueLength keySize = id.length();

  void const* resVal = result.begin();
  uint64_t resValSize = static_cast<uint64_t>(result.byteSize());
  std::unique_ptr<cache::CachedValue> value(
      cache::CachedValue::construct(key, (uint32_t)keySize, resVal, resValSize));

  if (value) {
    bool success = _cache->insert(value.get());
    if (!success) {
      LOG_TOPIC(DEBUG, Logger::GRAPHS) << "Insert failed";
    } else {
      // Cache is responsible.
      // If this failed, well we do not store it and read it again next time.
      value.release();
    }
  }
  ++_insertedDocuments;
  return result;
}

void TraverserCache::insertIntoResult(StringRef idString,
                                      VPackBuilder& builder) {
  auto finding = lookup(idString);
  if (finding.found()) {
    auto val = finding.value();
    VPackSlice slice(val->value());
    // finding makes sure that slice contant stays valid.
    builder.add(slice);
  } else {
    // Not in cache. Fetch and insert.
    builder.add(lookupInCollection(idString));
  }
}

aql::AqlValue TraverserCache::fetchAqlResult(StringRef idString) {
  auto finding = lookup(idString);
  if (finding.found()) {
    auto val = finding.value();
    // finding makes sure that slice content stays valid.
    return aql::AqlValue(VPackSlice(val->value()));
  }
  // Not in cache. Fetch and insert.
  return aql::AqlValue(lookupInCollection(idString));
}

void TraverserCache::insertDocument(StringRef idString, arangodb::velocypack::Slice const& document) {
  auto finding = lookup(idString);
  if (!finding.found()) {
    VPackValueLength keySize = idString.length();
    void const* key = idString.data();
    
    void const* resVal = document.begin();
    uint64_t resValSize = static_cast<uint64_t>(document.byteSize());
    std::unique_ptr<cache::CachedValue> value(cache::CachedValue::construct(key, (uint32_t)keySize,
                                                                            resVal, resValSize));
    
    if (value) {
      bool success = _cache->insert(value.get());
      if (!success) {
        LOG_TOPIC(DEBUG, Logger::GRAPHS) << "Insert document into cache failed";
      } else {
        // Cache is responsible.
        // If this failed, well we do not store it and read it again next time.
        value.release();
      }
    }
    ++_insertedDocuments;
  }
}

bool TraverserCache::validateFilter(
    StringRef idString,
    std::function<bool(VPackSlice const&)> filterFunc) {
  auto finding = lookup(idString);
  if (finding.found()) {
    auto val = finding.value();
    VPackSlice slice(val->value());
    // finding makes sure that slice contant stays valid.
    return filterFunc(slice);
  }
  // Not in cache. Fetch and insert.
  VPackSlice slice = lookupInCollection(idString);
  return filterFunc(slice);
}

StringRef TraverserCache::persistString(
    StringRef const idString) {
  auto it = _persistedStrings.find(idString);
  if (it != _persistedStrings.end()) {
    return *it;
  }
  StringRef res = _stringHeap->registerString(idString.begin(), idString.length());
  _persistedStrings.emplace(res);
  return res;
}
