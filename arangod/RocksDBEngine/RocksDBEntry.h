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
/// @author Daniel H. Larkin
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_ENTRY_H
#define ARANGO_ROCKSDB_ROCKSDB_ENTRY_H 1

#include "Basics/Common.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "VocBase/vocbase.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

class RocksDBEntry {

 public:
  RocksDBEntry() = delete;

  static RocksDBEntry Database(TRI_voc_tick_t databaseId, VPackSlice const& data);
  static RocksDBEntry Collection(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId, VPackSlice const& data);
  static RocksDBEntry Index(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId, TRI_idx_iid_t indexId, VPackSlice const& data);
  static RocksDBEntry Document(uint64_t collectionId, TRI_voc_rid_t revisionId,
                               VPackSlice const& data);
  static RocksDBEntry IndexValue(uint64_t indexId, TRI_voc_rid_t revisionId,
                                 VPackSlice const& indexValues);
  static RocksDBEntry UniqueIndexValue(uint64_t indexId, TRI_voc_rid_t revisionId,
                                       VPackSlice const& indexValues);
  static RocksDBEntry View(TRI_voc_tick_t databaseId, TRI_voc_cid_t viewId, VPackSlice const& data);

 public:
  RocksDBEntryType type() const;

  TRI_voc_tick_t databaseId() const;
  TRI_voc_cid_t collectionId() const;
  TRI_idx_iid_t indexId() const;
  TRI_voc_cid_t viewId() const;
  TRI_voc_rid_t revisionId() const;

  VPackSlice indexedValues() const;
  VPackSlice data() const;

  std::string const& key() const;
  std::string const& value() const;
  std::string& valueBuffer();
    
  static bool isSameDatabase(RocksDBEntryType type, TRI_voc_tick_t id, rocksdb::Slice const& slice);
  static uint64_t uint64FromPersistent(char const* p);
  static void uint64ToPersistent(char* p, uint64_t value);
  static void uint64ToPersistent(std::string& out, uint64_t value);

 private:
  RocksDBEntry(RocksDBEntryType type, uint64_t first, VPackSlice const& slice);
  RocksDBEntry(RocksDBEntryType type, uint64_t first, uint64_t second, VPackSlice const& slice);
  RocksDBEntry(RocksDBEntryType type, uint64_t first, uint64_t second, uint64_t third, VPackSlice const& slice);

 private:
  RocksDBEntryType const _type;
  std::string _keyBuffer;
  std::string _valueBuffer;
};

}  // namespace arangodb

#endif
