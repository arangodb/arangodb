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

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

class RocksDBEntry {
 public:
  enum class Type : char {
    Database = '0',
    Collection = '1',
    Index = '2',
    Document = '3',
    IndexValue = '4',
    UniqueIndexValue = '5',
    View = '6',
    CrossReference = '9'
  };

 public:
  RocksDBEntry() = delete;

  static RocksDBEntry Database(uint64_t id, VPackSlice const& data);
  static RocksDBEntry Collection(uint64_t id, VPackSlice const& data);
  static RocksDBEntry Index(uint64_t id, VPackSlice const& data);
  static RocksDBEntry Document(uint64_t collectionId, uint64_t revisionId,
                               VPackSlice const& data);
  static RocksDBEntry IndexValue(uint64_t indexId, uint64_t revisionId,
                                 VPackSlice const& indexValues);
  static RocksDBEntry UniqueIndexValue(uint64_t indexId, uint64_t revisionId,
                                       VPackSlice const& indexValues);
  static RocksDBEntry View(uint64_t id, VPackSlice const& data);

  static RocksDBEntry CrossReferenceCollection(uint64_t databaseId,
                                               uint64_t collectionId);
  static RocksDBEntry CrossReferenceIndex(uint64_t databaseId,
                                          uint64_t collectionId,
                                          uint64_t indexId);
  static RocksDBEntry CrossReferenceView(uint64_t databaseId, uint64_t viewId);

 public:
  Type type() const;

  uint64_t databaseId() const;
  uint64_t collectionId() const;
  uint64_t indexId() const;
  uint64_t viewId() const;
  uint64_t revisionId() const;

  VPackSlice const indexedValues() const;
  VPackSlice const data() const;

  std::string const& key() const;
  std::string const& value() const;
  std::string& valueBuffer();

 private:
  RocksDBEntry(Type type, Type subtype, uint64_t first, uint64_t second = 0,
               uint64_t third = 0);
  RocksDBEntry(Type type, uint64_t first, uint64_t second = 0,
               VPackSlice const& slice = VPackSlice());

 private:
  const Type _type;
  std::string _keyBuffer;
  std::string _valueBuffer;
};

}  // namespace arangodb

#endif
