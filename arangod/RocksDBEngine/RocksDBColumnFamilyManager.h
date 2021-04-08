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

#ifndef ARANGOD_ROCKSDB_ENGINE_ROCKSDB_COLUMN_FAMILY_MANAGER_H
#define ARANGOD_ROCKSDB_ENGINE_ROCKSDB_COLUMN_FAMILY_MANAGER_H 1

#include "RocksDBEngine/RocksDBCommon.h"
#include <rocksdb/db.h>

namespace arangodb {

/// Globally defined column families. If you do change the number of
/// column-families consider if there
/// is a need for an upgrade script. Added column families
/// can be created automatically by rocksdb.
/// Do check the RocksDB WAL tailing code and the
/// counter manager. Maybe the the number of families in the shouldHandle method
/// needs to be changed
struct RocksDBColumnFamilyManager {
  enum class Family : std::size_t {
    Definitions = 0,
    Documents = 1,
    PrimaryIndex = 2,
    EdgeIndex = 3,
    VPackIndex = 4,  // persistent, "skiplist", "hash"
    GeoIndex = 5,
    FulltextIndex = 6,

    Invalid = 1024  // special placeholder
  };

  enum class NameMode {
    Internal,  // for use within RocksDB
    External   // for display to users
  };

  static constexpr size_t minNumberOfColumnFamilies = 7;
  static constexpr size_t numberOfColumnFamilies = 7;

  static void initialize();

  static rocksdb::ColumnFamilyHandle* get(Family family);
  static void set(Family family, rocksdb::ColumnFamilyHandle* handle);

  static char const* name(Family family, NameMode mode = NameMode::Internal);
  static char const* name(rocksdb::ColumnFamilyHandle* handle,
                          NameMode mode = NameMode::External);

  static std::array<rocksdb::ColumnFamilyHandle*, numberOfColumnFamilies> const& allHandles();

 private:
  static std::array<char const*, numberOfColumnFamilies> _internalNames;
  static std::array<char const*, numberOfColumnFamilies> _externalNames;
  static std::array<rocksdb::ColumnFamilyHandle*, numberOfColumnFamilies> _handles;
  static rocksdb::ColumnFamilyHandle* _defaultHandle;
};

}  // namespace arangodb

#endif
