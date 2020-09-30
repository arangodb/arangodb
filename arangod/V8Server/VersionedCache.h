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

#ifndef ARANGOD_VERSIONED_CACHE_H
#define ARANGOD_VERSIONED_CACHE_H 1

#include "Basics/ReadWriteLock.h"

#include <memory>
#include <unordered_map>

namespace arangodb {
namespace velocypack {
class Builder;
}

class VersionedCache {
 public:
  VersionedCache();
  
  ~VersionedCache();
  
  /// @brief return the current version number
  uint64_t currentVersion() const;

  /// @brief bump the internal version number and return it
  uint64_t bumpVersion();
  
  /// @brief gets value for a given key from the cache. 
  std::pair<std::shared_ptr<arangodb::velocypack::Builder>, uint64_t> get(std::string const& key) const;

  /// @brief get the version for a given key from the cache. 
  uint64_t getVersion(std::string const& key) const;
  
  /// @brief sets value for a given key in the cache. 
  bool set(std::string const& key, std::shared_ptr<arangodb::velocypack::Builder> value, uint64_t version);
  
  /// @brief unconditionally sets value for a given key in the cache. 
  void set(std::string const& key, std::shared_ptr<arangodb::velocypack::Builder> value);
  
  /// @brief unconditionally removes the key from the cache
  void remove(std::string const& key);

  /// @brief removes all key from the cache with the given prefix.
  void removePrefix(std::string const& prefix);

  /// @brief builds a key from two components
  static std::string buildKey(std::string const& prefix, std::string const& suffix);
  
 private:
  struct CacheValue {
    /// @brief a Builder containing the value for the key
    std::shared_ptr<arangodb::velocypack::Builder> data;

    uint64_t version;

    CacheValue();
    CacheValue(std::shared_ptr<arangodb::velocypack::Builder> data, uint64_t version);
    ~CacheValue();
  };

  /// @brief read/write lock for accessing keys
  mutable basics::ReadWriteLock _lock;
  
  /// @brief key => value map
  /// protected by _lock
  std::unordered_map<std::string, CacheValue> _keys;

  /// @brief internal version number for cache state. starts at 1
  uint64_t _currentVersion;
};

}  // namespace arangodb

#endif
