////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

// Note: Array indices must match Family enum values.
// FulltextIndex (index 6) is kept as a placeholder even though it was removed
// in 4.0. "ZkdIndex" is the old name for MdiIndex, kept for backwards compat.
std::array<char const*,
           arangodb::RocksDBColumnFamilyManager::numberOfColumnFamilies>
    RocksDBColumnFamilyManager::_internalNames = {
        "default",        "Documents", "PrimaryIndex", "EdgeIndex",
        "VPackIndex",     "GeoIndex",
        "FulltextIndex",  // removed in 4.0, kept for index alignment
        "ReplicatedLogs", "ZkdIndex",  "MdiPrefixed",  "VectorIndex"};

std::array<char const*,
           arangodb::RocksDBColumnFamilyManager::numberOfColumnFamilies>
    RocksDBColumnFamilyManager::_externalNames = {
        "definitions",     "documents", "primary",      "edge",  "vpack", "geo",
        "fulltext",  // removed in 4.0, kept for index alignment
        "replicated-logs", "mdi",       "mdi-prefixed", "vector"};

std::array<rocksdb::ColumnFamilyHandle*,
           RocksDBColumnFamilyManager::numberOfColumnFamilies>
    RocksDBColumnFamilyManager::_handles = {nullptr, nullptr, nullptr, nullptr,
                                            nullptr, nullptr, nullptr, nullptr,
                                            nullptr, nullptr, nullptr};

rocksdb::ColumnFamilyHandle* RocksDBColumnFamilyManager::_defaultHandle =
    nullptr;

void RocksDBColumnFamilyManager::initialize() {
  std::size_t index = static_cast<
      std::underlying_type<RocksDBColumnFamilyManager::Family>::type>(
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

void RocksDBColumnFamilyManager::set(Family family,
                                     rocksdb::ColumnFamilyHandle* handle) {
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

char const* RocksDBColumnFamilyManager::name(
    rocksdb::ColumnFamilyHandle* handle, NameMode mode) {
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

std::vector<rocksdb::ColumnFamilyHandle*>
RocksDBColumnFamilyManager::allHandles() {
  // Some column families are null, we need to filter them out.
  std::vector<rocksdb::ColumnFamilyHandle*> result;
  result.reserve(_handles.size());
  for (auto* handle : _handles) {
    if (handle != nullptr) {
      result.push_back(handle);
    }
  }
  return result;
}

RocksDBColumnFamilyManager::Family RocksDBColumnFamilyManager::fromString(
    std::string_view name) {
  // Check for default column family
  if (name == "default" || name == rocksdb::kDefaultColumnFamilyName) {
    return Family::Definitions;
  }

  // Search in internal names array
  for (std::size_t i = 0; i < _internalNames.size(); ++i) {
    if (name == _internalNames[i]) {
      return static_cast<Family>(i);
    }
  }

  return Family::Invalid;
}

}  // namespace arangodb
