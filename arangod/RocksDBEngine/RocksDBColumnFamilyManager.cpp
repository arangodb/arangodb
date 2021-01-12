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
/// @author Simon Grätzer
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBColumnFamilyManager.h"

#include <rocksdb/db.h>

#include "Basics/debugging.h"

namespace arangodb {

std::array<char const*, arangodb::RocksDBColumnFamilyManager::numberOfColumnFamilies>
    RocksDBColumnFamilyManager::_internalNames = {"default",      "Documents",
                                                  "PrimaryIndex", "EdgeIndex",
                                                  "VPackIndex",   "GeoIndex",
                                                  "FulltextIndex"};
std::array<char const*, arangodb::RocksDBColumnFamilyManager::numberOfColumnFamilies> RocksDBColumnFamilyManager::_externalNames =
    {"definitions", "documents", "primary", "edge", "vpack", "geo", "fulltext"};

std::array<rocksdb::ColumnFamilyHandle*, RocksDBColumnFamilyManager::numberOfColumnFamilies>
    RocksDBColumnFamilyManager::_handles = {nullptr, nullptr, nullptr, nullptr,
                                            nullptr, nullptr, nullptr};

rocksdb::ColumnFamilyHandle* RocksDBColumnFamilyManager::_defaultHandle = nullptr;

void RocksDBColumnFamilyManager::initialize() {
  std::size_t index =
      static_cast<std::underlying_type<RocksDBColumnFamilyManager::Family>::type>(
          Family::Definitions);
  _internalNames[index] = rocksdb::kDefaultColumnFamilyName.c_str();
}

rocksdb::ColumnFamilyHandle* RocksDBColumnFamilyManager::get(Family family) {
  if (family == Family::Invalid) {
    return _defaultHandle;
  }

  std::size_t index = static_cast<std::underlying_type<Family>::type>(family);
  TRI_ASSERT(index < _handles.size());

  return _handles[index];
}

void RocksDBColumnFamilyManager::set(Family family, rocksdb::ColumnFamilyHandle* handle) {
  if (family == Family::Invalid) {
    _defaultHandle = handle;
    return;
  }

  std::size_t index = static_cast<std::underlying_type<Family>::type>(family);
  TRI_ASSERT(index < _handles.size());

  _handles[index] = handle;
}

char const* RocksDBColumnFamilyManager::name(Family family, NameMode mode) {
  if (family == Family::Invalid) {
    return rocksdb::kDefaultColumnFamilyName.c_str();
  }

  std::size_t index = static_cast<std::underlying_type<Family>::type>(family);
  TRI_ASSERT(index < _internalNames.size());

  if (mode == NameMode::Internal) {
    return _internalNames[index];
  }
  return _externalNames[index];
}

char const* RocksDBColumnFamilyManager::name(rocksdb::ColumnFamilyHandle* handle,
                                             NameMode mode) {
  for (std::size_t i = 0; i < _handles.size(); ++i) {
    if (_handles[i] == handle) {
      if (mode == NameMode::Internal) {
        return _internalNames[i];
      }
      return _externalNames[i];
    }
  }

  // didn't find it in the list; we should never get here
  TRI_ASSERT(false);
  return "unknown";
}

std::array<rocksdb::ColumnFamilyHandle*, RocksDBColumnFamilyManager::numberOfColumnFamilies> const&
RocksDBColumnFamilyManager::allHandles() {
  return _handles;
}

}  // namespace arangodb
