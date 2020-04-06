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
/// @author Dan Larkin-York
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGO_ROCKSDB_ROCKSDB_VALUE_H
#define ARANGO_ROCKSDB_ROCKSDB_VALUE_H 1

#include "Basics/Common.h"
#include "Basics/debugging.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "VocBase/Identifiers/LocalDocumentId.h"

#include <rocksdb/slice.h>
#include <s2/s2point.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
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
  static RocksDBValue PrimaryIndexValue(LocalDocumentId const& docId, TRI_voc_rid_t revision);
  static RocksDBValue EdgeIndexValue(arangodb::velocypack::StringRef const& vertexId);
  static RocksDBValue VPackIndexValue();
  static RocksDBValue UniqueVPackIndexValue(LocalDocumentId const& docId);
  static RocksDBValue View(VPackSlice const& data);
  static RocksDBValue ReplicationApplierConfig(VPackSlice const& data);
  static RocksDBValue KeyGeneratorValue(VPackSlice const& data);
  static RocksDBValue S2Value(S2Point const& c);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Used to construct an empty value of the given type for retrieval
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBValue Empty(RocksDBEntryType type);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Extracts the LocalDocumentId from a value
  ///
  /// May be called only on PrimaryIndexValue values. Other types will throw.
  //////////////////////////////////////////////////////////////////////////////

  static LocalDocumentId documentId(RocksDBValue const&);
  static LocalDocumentId documentId(rocksdb::Slice const&);
  static LocalDocumentId documentId(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Extracts the revisionId from a value
  ///
  /// May be called only on PrimaryIndexValue values. Other types will throw.
  //////////////////////////////////////////////////////////////////////////////
  static TRI_voc_rid_t revisionId(RocksDBValue const&);    // throwing
  static TRI_voc_rid_t revisionId(rocksdb::Slice const&);  // throwing
  static bool revisionId(rocksdb::Slice const&, TRI_voc_rid_t& id);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Extracts the vertex _to or _from ID (`_key`) from a value
  ///
  /// May be called only on EdgeIndexValue values. Other types will throw.
  //////////////////////////////////////////////////////////////////////////////
  static arangodb::velocypack::StringRef vertexId(rocksdb::Slice const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Extracts the VelocyPack data from a value
  ///
  /// May be called only values of the following types: Database, Collection,
  /// Document, and View. Other types will throw.
  //////////////////////////////////////////////////////////////////////////////
  static VPackSlice data(RocksDBValue const&);
  static VPackSlice data(rocksdb::Slice const&);
  static VPackSlice data(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Extracts the numeric value from the key field of a VPackSlice
  //////////////////////////////////////////////////////////////////////////////
  static uint64_t keyValue(rocksdb::Slice const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Centroid of shape or point on the sphere surface in degrees
  //////////////////////////////////////////////////////////////////////////////
  static S2Point centroid(rocksdb::Slice const&);

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns a reference to the underlying string buffer.
  //////////////////////////////////////////////////////////////////////////////
  std::string const& string() const { return _buffer; }  // to be used with put
  std::string* buffer() { return &_buffer; }             // to be used with get
  VPackSlice slice() const {
    return VPackSlice(reinterpret_cast<uint8_t const*>(_buffer.data()));
  }  // return a slice

  RocksDBValue(RocksDBEntryType type, rocksdb::Slice slice)
      : _type(type), _buffer(slice.data(), slice.size()) {}

  RocksDBValue(RocksDBValue const&) = delete;
  RocksDBValue& operator=(RocksDBValue const&) = delete;

  RocksDBValue(RocksDBValue&& other) noexcept
      : _type(other._type), _buffer(std::move(other._buffer)) {}

  RocksDBValue& operator=(RocksDBValue&& other) noexcept {
    TRI_ASSERT(_type == other._type || _type == RocksDBEntryType::Placeholder);
    _type = other._type;
    _buffer = std::move(other._buffer);
    return *this;
  }

 private:
  RocksDBValue();
  explicit RocksDBValue(RocksDBEntryType type);
  RocksDBValue(RocksDBEntryType type, LocalDocumentId const& docId, TRI_voc_rid_t revision);
  RocksDBValue(RocksDBEntryType type, VPackSlice const& data);
  RocksDBValue(RocksDBEntryType type, arangodb::velocypack::StringRef const& data);
  explicit RocksDBValue(S2Point const&);

 private:
  static RocksDBEntryType type(char const* data, size_t size);
  static LocalDocumentId documentId(char const* data, uint64_t size);
  static arangodb::velocypack::StringRef vertexId(char const* data, size_t size);
  static VPackSlice data(char const* data, size_t size);
  static uint64_t keyValue(char const* data, size_t size);

 private:
  RocksDBEntryType _type;
  std::string _buffer;
};

}  // namespace arangodb

#endif
