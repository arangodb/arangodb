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
/// @author Simon Grätzer
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#include "RocksDBColumnFamilyManager.h"

#include <rocksdb/db.h>

#include "Basics/debugging.h"

namespace {
std::array<char const*, arangodb::RocksDBColumnFamilyManager::numberOfColumnFamilies> const
    internalNames = {rocksdb::kDefaultColumnFamilyName.c_str(),
                     "Documents",
                     "PrimaryIndex",
                     "EdgeIndex",
                     "VPackIndex",
                     "GeoIndex",
                     "FulltextIndex"};
std::array<char const*, arangodb::RocksDBColumnFamilyManager::numberOfColumnFamilies> const externalNames = {
    "definitions", "documents", "primary", "edge", "vpack", "geo", "fulltext"};

static_assert(internalNames.size() == externalNames.size());
static_assert(internalNames.size() == arangodb::RocksDBColumnFamilyManager::numberOfColumnFamilies);
}  // namespace

namespace arangodb {

std::array<rocksdb::ColumnFamilyHandle*, RocksDBColumnFamilyManager::numberOfColumnFamilies>
    RocksDBColumnFamilyManager::_handles = {nullptr, nullptr, nullptr, nullptr,
                                            nullptr, nullptr, nullptr};

rocksdb::ColumnFamilyHandle* RocksDBColumnFamilyManager::get(Family family) {
  if (family == Family::Invalid) {
    // use the default
    family = Family::Definitions;
  }

  std::size_t index = static_cast<std::underlying_type<Family>::type>(family);
  TRI_ASSERT(index < _handles.size());

  return _handles[index];
}

void RocksDBColumnFamilyManager::set(Family family, rocksdb::ColumnFamilyHandle* handle) {
  if (family == Family::Invalid) {
    // use the default
    family = Family::Definitions;
  }

  std::size_t index = static_cast<std::underlying_type<Family>::type>(family);
  TRI_ASSERT(index < _handles.size());

  _handles[index] = handle;
}

char const* RocksDBColumnFamilyManager::name(Family family, NameMode mode) {
  if (family == Family::Invalid) {
    // use the default
    family = Family::Definitions;
  }

  std::size_t index = static_cast<std::underlying_type<Family>::type>(family);
  TRI_ASSERT(index < ::internalNames.size());

  if (mode == NameMode::Internal) {
    return ::internalNames[index];
  }
  return ::externalNames[index];
}

char const* RocksDBColumnFamilyManager::name(rocksdb::ColumnFamilyHandle* handle,
                                             NameMode mode) {
  for (std::size_t i = 0; i < _handles.size(); ++i) {
    if (_handles[i] == handle) {
      if (mode == NameMode::Internal) {
        return ::internalNames[i];
      }
      return ::externalNames[i];
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
