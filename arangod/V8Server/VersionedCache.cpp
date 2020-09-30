////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "VersionedCache.h"

#include "Basics/ReadLocker.h"
#include "Basics/WriteLocker.h"

#include <velocypack/Builder.h>

using namespace arangodb;

VersionedCache::VersionedCache()
    : _currentVersion(1) {}

VersionedCache::~VersionedCache() = default;

uint64_t VersionedCache::currentVersion() const {
  READ_LOCKER(guard, _lock);
  TRI_ASSERT(_currentVersion > 0);
  return _currentVersion;
}

uint64_t VersionedCache::bumpVersion() {
  WRITE_LOCKER(guard, _lock);
  TRI_ASSERT(_currentVersion > 0);
  return ++_currentVersion;
}

std::pair<std::shared_ptr<arangodb::velocypack::Builder>, uint64_t> VersionedCache::get(std::string const& key) const {
  READ_LOCKER(guard, _lock);

  auto it = _keys.find(key);
  if (it != _keys.end()) {
    auto const& entry = (*it).second;
    return { entry.data, entry.version };
  }
  return { nullptr, 0 };
}

uint64_t VersionedCache::getVersion(std::string const& key) const {
  READ_LOCKER(guard, _lock);

  auto it = _keys.find(key);
  if (it != _keys.end()) {
    auto const& entry = (*it).second;
    return entry.version;
  }
  return 0;
}

bool VersionedCache::set(std::string const& key, std::shared_ptr<arangodb::velocypack::Builder> value, uint64_t version) {
  if (version > 0) {
    WRITE_LOCKER(guard, _lock);

    // will create entry if it is not yet present
    auto& entry = _keys[key];
    if (entry.data == nullptr || entry.version <= version) {
      entry.data = value;
      entry.version = version;
      return true;
    }
  }
  // existing cache entry is newer than what we tried to insert
  return false;
}

void VersionedCache::set(std::string const& key, std::shared_ptr<arangodb::velocypack::Builder> value) {
  WRITE_LOCKER(guard, _lock);

  // unconditionally set cache entry
  _keys[key] = { std::move(value), UINT64_MAX };
}
  
void VersionedCache::remove(std::string const& key) {
  WRITE_LOCKER(guard, _lock);
  _keys.erase(key);
}

void VersionedCache::removePrefix(std::string const& prefix) {
  WRITE_LOCKER(guard, _lock);

  auto it = _keys.begin();
  while (it != _keys.end()) {
    arangodb::velocypack::StringRef key((*it).first);
    if (key.substr(0, prefix.size()) == prefix) {
      // prefix match
      it = _keys.erase(it);
    } else {
      // no match
      ++it;
    }
  }
}
  
/// @brief builds a key from two components
/*static*/ std::string VersionedCache::buildKey(std::string const& prefix, std::string const& suffix) {
  std::string result;
  result.reserve(prefix.size() + suffix.size() + 1);
  result.append(prefix);
  result.push_back('-');
  result.append(suffix);
  return result;
}
  
VersionedCache::CacheValue::CacheValue()
  : CacheValue(nullptr, 0) {} 

VersionedCache::CacheValue::CacheValue(std::shared_ptr<arangodb::velocypack::Builder> data, uint64_t version)
  : data(std::move(data)), version(version) {}

VersionedCache::CacheValue::~CacheValue() = default;
