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

class RocksDBKey {
 public:
  RocksDBKey() = delete;

  static RocksDBKey Database(TRI_voc_tick_t databaseId, VPackSlice const& data);
  static RocksDBKey Collection(TRI_voc_tick_t databaseId,
                               TRI_voc_cid_t collectionId,
                               VPackSlice const& data);
  static RocksDBKey Index(TRI_voc_tick_t databaseId, TRI_voc_cid_t collectionId,
                          TRI_idx_iid_t indexId, VPackSlice const& data);
  static RocksDBKey Document(uint64_t collectionId, TRI_voc_rid_t revisionId,
                             VPackSlice const& data);
  static RocksDBKey PrimaryIndexValue(uint64_t indexId,
                                      std::string const& primaryKey);
  static RocksDBKey EdgeIndexValue(uint64_t indexId,
                                   std::string const& vertexId,
                                   std::string const& primaryKey);
  static RocksDBKey IndexValue(uint64_t indexId, std::string const& primaryKey,
                               VPackSlice const& indexValues);
  static RocksDBKey UniqueIndexValue(uint64_t indexId,
                                     VPackSlice const& indexValues);
  static RocksDBKey View(TRI_voc_tick_t databaseId, TRI_voc_cid_t viewId,
                         VPackSlice const& data);

 public:
  static RocksDBEntryType type(RocksDBKey const&);
  static RocksDBEntryType type(rocksdb::Slice const&);

  static TRI_voc_tick_t databaseId(RocksDBKey const&);
  static TRI_voc_tick_t databaseId(rocksdb::Slice const&);

  static TRI_voc_cid_t collectionId(RocksDBKey const&);
  static TRI_voc_cid_t collectionId(rocksdb::Slice const&);

  static TRI_idx_iid_t indexId(RocksDBKey const&);
  static TRI_idx_iid_t indexId(rocksdb::Slice const&);

  static TRI_voc_cid_t viewId(RocksDBKey const&);
  static TRI_voc_cid_t viewId(rocksdb::Slice const&);

  static TRI_voc_rid_t revisionId(RocksDBKey const&);
  static TRI_voc_rid_t revisionId(rocksdb::Slice const&);

  static std::string primaryKey(RocksDBKey const&);
  static std::string primaryKey(rocksdb::Slice const&);

  static std::string vertexId(RocksDBKey const&);
  static std::string vertexId(rocksdb::Slice const&);

  static VPackSlice indexedVPack(RocksDBKey const&);
  static VPackSlice indexedVPack(rocksdb::Slice const&);

 public:
  std::string const& key() const;

  static bool isSameDatabase(RocksDBEntryType type, TRI_voc_tick_t id,
                             rocksdb::Slice const& slice);

 private:
  RocksDBKey(RocksDBEntryType type, uint64_t first);
  RocksDBKey(RocksDBEntryType type, uint64_t first, uint64_t second);
  RocksDBKey(RocksDBEntryType type, uint64_t first, uint64_t second,
             uint64_t third);
  RocksDBKey(RocksDBEntryType type, uint64_t first, VPackSlice const& slice);
  RocksDBKey(RocksDBEntryType type, uint64_t first, std::string const& second,
             VPackSlice const& slice);
  RocksDBKey(RocksDBEntryType type, uint64_t first, std::string const& second);
  RocksDBKey(RocksDBEntryType type, uint64_t first, std::string const& second,
             std::string const& third);

 private:
 public:
  static RocksDBEntryType type(char const* data, size_t size);
  static TRI_voc_tick_t databaseId(char const* data, size_t size);
  static TRI_voc_cid_t collectionId(char const* data, size_t size);
  static TRI_idx_iid_t indexId(char const* data, size_t size);
  static TRI_voc_cid_t viewId(char const* data, size_t size);
  static TRI_voc_rid_t revisionId(char const* data, size_t size);
  static std::string primaryKey(char const* data, size_t size);
  static std::string vertexId(char const* data, size_t size);
  static VPackSlice indexedVPack(char const* data, size_t size);

 private:
  static const char _stringSeparator;
  RocksDBEntryType const _type;
  std::string _buffer;
};

}  // namespace arangodb

#endif
