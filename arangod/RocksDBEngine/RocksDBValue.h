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

#ifndef ARANGO_ROCKSDB_ROCKSDB_VALUE_H
#define ARANGO_ROCKSDB_ROCKSDB_VALUE_H 1

#include "Basics/Common.h"
#include "Basics/StringRef.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "VocBase/vocbase.h"

#include <rocksdb/slice.h>

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

class RocksDBValue {
 public:
  //----------------------------------------------------------------------------
  // SECTION Constructors
  // Each of these simply specifies the correct type and copies the input
  // parameter in an appropriate format into the underlying string buffer.
  //----------------------------------------------------------------------------

  static RocksDBValue Database(VPackSlice const& data);
  static RocksDBValue Collection(VPackSlice const& data);
  static RocksDBValue Document(VPackSlice const& data);
  static RocksDBValue PrimaryIndexValue(TRI_voc_rid_t revisionId);
  static RocksDBValue EdgeIndexValue();
  static RocksDBValue IndexValue();
  static RocksDBValue UniqueIndexValue(TRI_voc_rid_t revisionId);
  static RocksDBValue View(VPackSlice const& data);
  static RocksDBValue ReplicationApplierConfig(VPackSlice const& data);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Used to construct an empty value of the given type for retrieval
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBValue Empty(RocksDBEntryType type);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Extracts the revisionId from a value
  ///
  /// May be called only on PrimaryIndexValue values. Other types will throw.
  //////////////////////////////////////////////////////////////////////////////

  static TRI_voc_rid_t revisionId(RocksDBValue const&);
  static TRI_voc_rid_t revisionId(rocksdb::Slice const&);
  static TRI_voc_rid_t revisionId(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Extracts the primary key (`_key`) from a value
  ///
  /// May be called only on UniqueIndexValue values. Other types will throw.
  //////////////////////////////////////////////////////////////////////////////
  static StringRef primaryKey(RocksDBValue const&);
  static StringRef primaryKey(rocksdb::Slice const&);
  static StringRef primaryKey(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Extracts the VelocyPack data from a value
  ///
  /// May be called only values of the following types: Database, Collection,
  /// Document, and View. Other types will throw.
  //////////////////////////////////////////////////////////////////////////////
  static VPackSlice data(RocksDBValue const&);
  static VPackSlice data(rocksdb::Slice const&);
  static VPackSlice data(std::string const&);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns a reference to the underlying string buffer.
  //////////////////////////////////////////////////////////////////////////////
  std::string const& string() { return _buffer; }  // to be used with put
  std::string* buffer() { return &_buffer; }       // to be used with get
  VPackSlice slice() const {
    return VPackSlice(reinterpret_cast<uint8_t const*>(_buffer.data()));
  }  // return a slice

  RocksDBValue(RocksDBEntryType type, rocksdb::Slice slice)
      : _type(type), _buffer(slice.data(), slice.size()) {}
 
  RocksDBValue(RocksDBValue&& other) 
      : _type(other._type), _buffer(std::move(other._buffer)) {}
 
 private:
  RocksDBValue();
  explicit RocksDBValue(RocksDBEntryType type);
  RocksDBValue(RocksDBEntryType type, uint64_t data);
  RocksDBValue(RocksDBEntryType type, VPackSlice const& data);

 private:
  static RocksDBEntryType type(char const* data, size_t size);
  static TRI_voc_rid_t revisionId(char const* data, uint64_t size);
  static StringRef primaryKey(char const* data, size_t size);
  static VPackSlice data(char const* data, size_t size);

 private:
  RocksDBEntryType const _type;
  std::string _buffer;
};

}  // namespace arangodb

#endif
