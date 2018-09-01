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
/// @author Dan Larkin-York
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_TYPES_H
#define ARANGO_ROCKSDB_ROCKSDB_TYPES_H 1

#include "Basics/Common.h"

#include <rocksdb/slice.h>

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// Used to keep track of current key type in RocksDBKey and RocksDBKeyBounds
/// Should not be written to disk from 3.2 milestone 1 onwards
////////////////////////////////////////////////////////////////////////////////
enum class RocksDBEntryType : char {
  Placeholder = '\0',
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
  LegacyGeoIndexValue = ';',
  IndexEstimateValue = '<',
  KeyGeneratorValue = '=',
  View = '>',
  GeoIndexValue = '?'
};

char const* rocksDBEntryTypeName(RocksDBEntryType);

////////////////////////////////////////////////////////////////////////////////
/// Used to for various metadata in the write-ahead-log
////////////////////////////////////////////////////////////////////////////////
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
  DocumentOperationsPrologue = '=',  // <- deprecated
  DocumentRemove = '>',              // <- deprecated
  SinglePut = '?',
  SingleRemove = '@',                  // <- deprecated
  DocumentRemoveAsPartOfUpdate = 'A',  // <- deprecated
#ifdef USE_IRESEARCH
  IResearchLinkDrop = 'C',
#endif
  CommitTransaction = 'D',
  DocumentRemoveV2 = 'E',
  SingleRemoveV2 = 'F',
  CollectionTruncate = 'G'
};

/// @brief settings keys
enum class RocksDBSettingsType : char {
  Invalid = 0,
  Version = 'V',
  ServerTick = 'S',
  Endianness = 'E'
};
  
/// @brief endianess value
enum class RocksDBEndianness : char {
  Invalid = 0,
  Little = 'L',
  Big = 'B'
};
  
/// @brief rocksdb format version
char rocksDBFormatVersion();

char const* rocksDBLogTypeName(RocksDBLogType);
rocksdb::Slice const& rocksDBSlice(RocksDBEntryType const& type);
}  // namespace arangodb

#endif
