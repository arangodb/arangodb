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
/// @author Daniel H. Larkin
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_TYPES_H
#define ARANGO_ROCKSDB_ROCKSDB_TYPES_H 1

#include "Basics/Common.h"

#include <rocksdb/slice.h>

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// If these values change, make sure to reflect the changes in
/// RocksDBPrefixExtractor as well.
////////////////////////////////////////////////////////////////////////////////
enum class RocksDBEntryType : char {
  Database = '0',
  Collection = '1',
  CounterValue = '2',
  Document = '3',
  PrimaryIndexValue = '4',
  EdgeIndexValue = '5',
  VPackIndexValue = '6',
  UniqueVPackIndexValue = '7',
  SettingsValue = '8',
  ReplicationApplierConfig = '9',
  FulltextIndexValue = ':',
  GeoIndexValue = ';',
  IndexEstimateValue = '<',
  KeyGeneratorValue = '=',
  View = '>'
};

char const* rocksDBEntryTypeName(RocksDBEntryType);

enum class RocksDBLogType : char {
  Invalid = 0,
  DatabaseCreate = '1',
  DatabaseDrop = '2',
  CollectionCreate = '3',
  CollectionDrop = '4',
  CollectionRename = '5',
  CollectionChange = '6',
  IndexCreate = '7',
  IndexDrop = '8',
  ViewCreate = '9',
  ViewDrop = ':',
  ViewChange = ';',
  BeginTransaction = '<',
  DocumentOperationsPrologue = '=',
  DocumentRemove = '>',
  SinglePut = '?',
  SingleRemove = '@'
};
  
enum class RocksDBSettingsType : char {
  Invalid = 0,
  Version = 'V',
  ServerTick = 'S'
};

char const* rocksDBLogTypeName(RocksDBLogType);
rocksdb::Slice const& rocksDBSlice(RocksDBEntryType const& type);
char rocksDBFormatVersion();
}  // namespace arangodb

#endif
