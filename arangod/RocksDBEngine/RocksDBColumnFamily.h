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

#include "RocksDBEngine/RocksDBCommon.h"

#include <rocksdb/db.h>

namespace arangodb {

/// Globally defined column families. If you do change the number of column
/// families
/// consider if there is a need for an upgrade script. Added column families can
/// be
/// created automatically by rocksdb. Do check the RocksDB WAL tailing code and
/// the
/// counter manager. Maybe the the number of families in the shouldHandle method
/// needs
/// to be changed
struct RocksDBColumnFamily {
  friend class RocksDBEngine;
  friend class RocksDBWrapper;

  static constexpr size_t minNumberOfColumnFamilies = 7;
  static constexpr size_t numberOfColumnFamilies = 7;

  static RocksDBWrapperCFHandle* definitions() { return _definitions; }

  static RocksDBWrapperCFHandle* documents() { return _documents; }

  static RocksDBWrapperCFHandle* primary() { return _primary; }

  static RocksDBWrapperCFHandle* edge() { return _edge; }

  /// unique and non unique vpack indexes (skiplist, permanent indexes)
  static RocksDBWrapperCFHandle* vpack() { return _vpack; }

  static RocksDBWrapperCFHandle* geo() { return _geo; }

  static RocksDBWrapperCFHandle* fulltext() { return _fulltext; }

  static RocksDBWrapperCFHandle* invalid() { return _invalid; }

  static char const* columnFamilyName(RocksDBWrapperCFHandle* cf) {
    if (cf == _definitions) {
      return "definitions";
    }
    if (cf == _documents) {
      return "documents";
    }
    if (cf == _primary) {
      return "primary";
    }
    if (cf == _edge) {
      return "edge";
    }
    if (cf == _vpack) {
      return "vpack";
    }
    if (cf == _geo) {
      return "geo";
    }
    if (cf == _fulltext) {
      return "fulltext";
    }
    if (cf == _invalid) {
      return "invalid";
    }
    TRI_ASSERT(false);
    return "unknown";
  }

 private:
  // static variables for all existing column families
  // note that these are initialized in RocksDBEngine.cpp
  // as there is no RocksDBColumnFamily.cpp
  static RocksDBWrapperCFHandle* _definitions;
  static RocksDBWrapperCFHandle* _documents;
  static RocksDBWrapperCFHandle* _primary;
  static RocksDBWrapperCFHandle* _edge;
  static RocksDBWrapperCFHandle* _vpack;
  static RocksDBWrapperCFHandle* _geo;
  static RocksDBWrapperCFHandle* _fulltext;
  static RocksDBWrapperCFHandle* _invalid;
  static std::vector<RocksDBWrapperCFHandle*> _allHandles;
};

}  // namespace arangodb

#endif
