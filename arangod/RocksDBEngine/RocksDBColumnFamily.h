////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_ROCKSDB_ENGINE_COLUMN_FAMILY_H
#define ARANGOD_ROCKSDB_ENGINE_COLUMN_FAMILY_H 1

#include <rocksdb/db.h>

namespace arangodb {

/// Globally defined column families. If you do change the number of column families
/// consider if there is a need for an upgrade script. Added column families can be
/// created automatically by rocksdb. Do check the RocksDB WAL tailing code and the
/// counter manager. Maybe the the number of families in the shouldHandle method needs
/// to be changed
struct RocksDBColumnFamily {
  friend class RocksDBEngine;

  static constexpr size_t minNumberOfColumnFamilies = 8;
  static constexpr size_t numberOfColumnFamilies = 9;
  
  static rocksdb::ColumnFamilyHandle* other() { return _other; }
  
  static rocksdb::ColumnFamilyHandle* documents() { return _documents; }

  static rocksdb::ColumnFamilyHandle* primary() { return _primary; }

  static rocksdb::ColumnFamilyHandle* edge() { return _edge; }

  static rocksdb::ColumnFamilyHandle* geo() { return _geo; }

  static rocksdb::ColumnFamilyHandle* fulltext() { return _fulltext; }

  static rocksdb::ColumnFamilyHandle* index() { return _index; }

  static rocksdb::ColumnFamilyHandle* uniqueIndex() { return _uniqueIndex; }

  static rocksdb::ColumnFamilyHandle* views() { return _views; }

 private:
  static rocksdb::ColumnFamilyHandle* _other;
  static rocksdb::ColumnFamilyHandle* _documents;
  static rocksdb::ColumnFamilyHandle* _primary;
  static rocksdb::ColumnFamilyHandle* _edge;
  static rocksdb::ColumnFamilyHandle* _geo;
  static rocksdb::ColumnFamilyHandle* _fulltext;
  static rocksdb::ColumnFamilyHandle* _index;
  static rocksdb::ColumnFamilyHandle* _uniqueIndex;
  static rocksdb::ColumnFamilyHandle* _views;
  static std::vector<rocksdb::ColumnFamilyHandle*> _allHandles;
};

}  // namespace arangodb

#endif
