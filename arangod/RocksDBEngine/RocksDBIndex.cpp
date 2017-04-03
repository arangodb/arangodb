////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2017 ArangoDB GmbH, Cologne, Germany
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

#include "RocksDBIndex.h"
#include "Cache/CacheManagerFeature.h"
#include "Cache/Common.h"
#include "Cache/Manager.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/ticks.h"

using namespace arangodb;

RocksDBIndex::RocksDBIndex(
    TRI_idx_iid_t id, LogicalCollection* collection,
    std::vector<std::vector<arangodb::basics::AttributeName>> const& attributes,
    bool unique, bool sparse, uint64_t objectId)
    : Index(id, collection, attributes, unique, sparse),
      _objectId(TRI_NewTickServer()),  // TODO!
      _cmp(static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE)->cmp()),
      _cacheManager(CacheManagerFeature::MANAGER),
      _cache(nullptr),
      _useCache(false) {}

RocksDBIndex::RocksDBIndex(TRI_idx_iid_t id, LogicalCollection* collection,
                           VPackSlice const& info)
    : Index(id, collection, info),
      _objectId(TRI_NewTickServer()),  // TODO!
      _cmp(static_cast<RocksDBEngine*>(EngineSelectorFeature::ENGINE)->cmp()) {}

RocksDBIndex::~RocksDBIndex() {
  if (_useCache) {
    TRI_ASSERT(_cache != nullptr);
    _cacheManager->destroyCache(_cache);
  }
}

void RocksDBIndex::createCache() {
  TRI_ASSERT(_cacheManager != nullptr);
  TRI_ASSERT(_useCache);
  TRI_ASSERT(_cache.get() == nullptr);
  _cache = _cacheManager->createCache(cache::CacheType::Transactional);
  if (_cache.get() == nullptr) {
    _useCache = false;
  }
}
