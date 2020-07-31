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

#ifndef ARANGO_ROCKSDB_ROCKSDB_KEY_BOUNDS_H
#define ARANGO_ROCKSDB_ROCKSDB_KEY_BOUNDS_H 1

#include "Basics/Common.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "VocBase/vocbase.h"

#include <rocksdb/slice.h>
#include <velocypack/Slice.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

#include <iosfwd>

namespace rocksdb {
class ColumnFamilyHandle;
}

namespace arangodb {

class RocksDBKeyBounds {
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief empty bounds
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds Empty();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for list of all databases
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds Databases();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all collections belonging to a specified database
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds DatabaseCollections(TRI_voc_tick_t databaseId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all documents belonging to a specified collection
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds CollectionDocuments(uint64_t objectId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all documents belonging to a specified collection within
  /// a certain range
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds CollectionDocumentRange(uint64_t objectId,
                                                  std::size_t min, std::size_t max);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all index-entries- belonging to a specified primary
  /// index
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds PrimaryIndex(uint64_t indexId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all index-entries- within a range belonging to a
  ///  specified primary index
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds PrimaryIndex(uint64_t indexId, std::string const& lower,
                                       std::string const& upper);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all index-entries belonging to a specified edge index
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds EdgeIndex(uint64_t indexId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all index-entries belonging to a specified edge index
  /// related to the specified vertex
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds EdgeIndexVertex(uint64_t indexId,
                                          arangodb::velocypack::StringRef const& vertexId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all index-entries belonging to a specified non-unique
  /// index (hash, skiplist and permanent)
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds VPackIndex(uint64_t indexId, bool reverse);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all entries belonging to a specified unique index
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds UniqueVPackIndex(uint64_t indexId, bool reverse);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all entries of a fulltext index
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds FulltextIndex(uint64_t indexId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all entries belonging to specified legacy geo index
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds LegacyGeoIndex(uint64_t indexId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all entries in geo index
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds GeoIndex(uint64_t indexId);
  static RocksDBKeyBounds GeoIndex(uint64_t indexId, uint64_t minCell, uint64_t maxCell);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all index-entries within a value range belonging to a
  /// specified non-unique index (skiplist and permanent)
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds VPackIndex(uint64_t indexId, VPackSlice const& left,
                                     VPackSlice const& right);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all documents within a value range belonging to a
  /// specified unique index
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds UniqueVPackIndex(uint64_t indexId, VPackSlice const& left,
                                           VPackSlice const& right);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all documents within a value range belonging to a
  /// specified unique index. this method is used for point lookups
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds UniqueVPackIndex(uint64_t indexId, VPackSlice const& left);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all views belonging to a specified database
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds DatabaseViews(TRI_voc_tick_t databaseId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all counter values
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds CounterValues();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all index estimate values
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds IndexEstimateValues();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all key generators
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds KeyGenerators();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all entries of a fulltext index, matching prefixes
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds FulltextIndexPrefix(uint64_t, arangodb::velocypack::StringRef const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Bounds for all entries of a fulltext index, matching the word
  //////////////////////////////////////////////////////////////////////////////
  static RocksDBKeyBounds FulltextIndexComplete(uint64_t, arangodb::velocypack::StringRef const&);

 public:
  RocksDBKeyBounds(RocksDBKeyBounds const& other);
  RocksDBKeyBounds(RocksDBKeyBounds&& other) noexcept;
  RocksDBKeyBounds& operator=(RocksDBKeyBounds const& other);
  RocksDBKeyBounds& operator=(RocksDBKeyBounds&& other) noexcept;

  RocksDBEntryType type() const { return _type; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the left bound slice.
  ///
  /// Forward iterators may use it->Seek(bound.start()) and reverse iterators
  /// may check that the current key is greater than this value.
  //////////////////////////////////////////////////////////////////////////////
  rocksdb::Slice start() const { return _internals.start(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the right bound slice.
  ///
  /// Reverse iterators may use it->SeekForPrev(bound.end()) and forward
  /// iterators may check that the current key is less than this value.
  //////////////////////////////////////////////////////////////////////////////
  rocksdb::Slice end() const { return _internals.end(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the column family from this Bound
  ///
  /// All bounds iterators need to iterate over the correct column families
  /// with this helper function it is made sure that correct column family
  /// for bound is used.
  //////////////////////////////////////////////////////////////////////////////
  rocksdb::ColumnFamilyHandle* columnFamily() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the object ID for these bounds
  ///
  /// This method is only valid for certain types of bounds: Documents and
  /// Index entries.
  //////////////////////////////////////////////////////////////////////////////
  uint64_t objectId() const;

 private:
  RocksDBKeyBounds();
  explicit RocksDBKeyBounds(RocksDBEntryType type);
  RocksDBKeyBounds(RocksDBEntryType type, uint64_t first);
  RocksDBKeyBounds(RocksDBEntryType type, uint64_t first, bool second);
  RocksDBKeyBounds(RocksDBEntryType type, uint64_t first, arangodb::velocypack::StringRef const& second);
  RocksDBKeyBounds(RocksDBEntryType type, uint64_t first, VPackSlice const& second);
  RocksDBKeyBounds(RocksDBEntryType type, uint64_t first,
                   VPackSlice const& second, VPackSlice const& third);
  RocksDBKeyBounds(RocksDBEntryType type, uint64_t first, uint64_t second, uint64_t third);
  RocksDBKeyBounds(RocksDBEntryType type, uint64_t id, std::string const& lower,
                   std::string const& upper);

 private:
  // private class that will hold both bounds in a single buffer (with only one
  // allocation)
  class BoundsBuffer {
    friend class RocksDBKeyBounds;

   public:
    BoundsBuffer() : _separatorPosition(0) {}

    BoundsBuffer(BoundsBuffer const& other)
        : _buffer(other._buffer), _separatorPosition(other._separatorPosition) {}

    BoundsBuffer(BoundsBuffer&& other) noexcept
        : _buffer(std::move(other._buffer)),
          _separatorPosition(other._separatorPosition) {
      other._separatorPosition = 0;
    }

    BoundsBuffer& operator=(BoundsBuffer const& other) {
      if (this != &other) {
        _buffer = other._buffer;
        _separatorPosition = other._separatorPosition;
      }
      return *this;
    }

    BoundsBuffer& operator=(BoundsBuffer&& other) noexcept {
      if (this != &other) {
        _buffer = std::move(other._buffer);
        _separatorPosition = other._separatorPosition;
        other._separatorPosition = 0;
      }
      return *this;
    }

    // reserve space for bounds
    void reserve(size_t length) {
      TRI_ASSERT(_separatorPosition == 0);
      TRI_ASSERT(_buffer.empty());
      _buffer.reserve(length);
    }

    // mark the end of the start buffer
    void separate() {
      TRI_ASSERT(_separatorPosition == 0);
      TRI_ASSERT(!_buffer.empty());
      _separatorPosition = _buffer.size();
    }

    // append a character
    void push_back(char c) { _buffer.push_back(c); }

    // return the internal buffer for modification or reading
    std::string& buffer() { return _buffer; }
    std::string const& buffer() const { return _buffer; }

    // return a slice to the start buffer
    rocksdb::Slice start() const {
      TRI_ASSERT(_separatorPosition != 0);
      return rocksdb::Slice(_buffer.data(), _separatorPosition);
    }

    // return a slice to the end buffer
    rocksdb::Slice end() const {
      TRI_ASSERT(_separatorPosition != 0);
      return rocksdb::Slice(_buffer.data() + _separatorPosition,
                            _buffer.size() - _separatorPosition);
    }

   private:
    std::string _buffer;
    size_t _separatorPosition;
  };

  BoundsBuffer& internals() { return _internals; }
  BoundsBuffer const& internals() const { return _internals; }

  static const char _stringSeparator;
  RocksDBEntryType _type;
  BoundsBuffer _internals;
};

std::ostream& operator<<(std::ostream&, RocksDBKeyBounds const&);

}  // namespace arangodb

#endif
