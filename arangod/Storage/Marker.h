////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_STORAGE_MARKER_H
#define ARANGOD_STORAGE_MARKER_H 1

#include "Basics/Common.h"
#include "Basics/hashes.h"
#include "Storage/Options.h"

#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

namespace arangodb {

////////////////////////////////////////////////////////////////////////////////
/// @brief available marker types. values must be < 128
////////////////////////////////////////////////////////////////////////////////

enum MarkerType : uint8_t {
  MarkerTypeHeader = 1,
  MarkerTypeFooter = 2,

  MarkerTypeDocumentPreface = 10,
  MarkerTypeDocument = 11,
  MarkerTypeDocumentDeletion = 12,

  MarkerTypeTransactionBegin = 20,
  MarkerTypeTransactionCommit = 21,
  MarkerTypeTransactionAbort = 22,

  MarkerTypeCollectionCreate = 30,
  MarkerTypeCollectionDrop = 31,
  MarkerTypeCollectionRename = 32,
  MarkerTypeCollectionProperties = 33,

  MarkerTypeIndexCreate = 40,
  MarkerTypeIndexDrop = 41,

  MarkerTypeDatabaseCreate = 50,
  MarkerTypeDatabaseDrop = 51,

  MarkerMax = 127
};

static_assert(MarkerMax < 128, "invalid maximum marker type value");

struct MarkerHelper {
  uint32_t alignedSize(uint32_t value) { return ((value + 7) / 8) * 8; }

  uint64_t alignedSize(uint64_t value) { return ((value + 7) / 8) * 8; }

  template <typename T>
  static inline uint32_t calculateNumberLength(T value) throw() {
    uint32_t len = 1;
    while (value > 0) {
      ++len;
      value >>= 8;
      ++len;
    }
    return len;
  }

  template <typename T>
  static inline T readNumber(uint8_t const* source, uint32_t length) {
    T result = 0;
    uint8_t const* end = source + length;
    do {
      result <<= 8;
      result += static_cast<T>(*source++);
    } while (source < end);
    return result;
  }

  template <typename T>
  static inline void storeNumber(uint8_t* dest, T value, uint32_t length) {
    uint8_t* end = dest + length;
    do {
      *dest++ = static_cast<uint8_t>(value & 0xff);
      value >>= 8;
    } while (dest < end);
  }

  // returns a type name for a marker
  static char const* typeName(MarkerType type);

  // returns the static length for the marker type
  // the static length is the total length of the marker's static data fields,
  // excluding the base marker's fields and excluding the marker's dynamic
  // VPack data values
  static uint64_t staticLength(MarkerType type);

  // calculate the required length for a marker of the specified type, given a
  // payload of the specified length
  static uint64_t calculateMarkerLength(MarkerType type,
                                        uint64_t payloadLength);

  // calculate the required length for the header of a marker, given a body
  // of the specified length
  static uint64_t calculateHeaderLength(uint64_t bodyLength);
};

/* the base layout for all markers is:
   uint32_t   type and length information (first byte contains marker type,
              following 3 bytes contain length information)
   uint32_t   crc checksum
   uint64_t   tick value
   (uint64_t) optional length information
   char[]     payload

   if the highest bit in the first byte (type) is set, then the length of
   the marker is coded in the uint64_t length value at offset 0x10.
   if the highest bit in the first byte (type) is not set, then the length
   of the marker is coded in bytes from offset 1 to (including) 3.
*/

class MarkerReader {
 public:
  static uint64_t const MinMarkerLength = 16;

  MarkerReader(uint8_t* begin) : _begin(begin), _length(calculateLength()) {
    TRI_ASSERT(_length >= MinMarkerLength);
  }

 public:
  char* data() const { return reinterpret_cast<char*>(_begin); }

  uint8_t* begin() const { return _begin; }

  uint8_t* end() const { return _begin + _length; }

  MarkerType type() const {
    // read lowest 7 bits of head byte
    uint8_t type = *_begin & 0x7f;
    return static_cast<MarkerType>(type);
  }

  uint32_t length() const { return static_cast<uint32_t>(_length); }

  uint32_t headerLength() const {
    if (*_begin & 0x80) {
      return 24;
    }
    return 16;
  }

  // gets the currently persisted CRC value of the marker
  uint32_t persistedCrc() const {
    return readAlignedNumber<uint32_t>(_begin + 4, 4);
  }

  // recalculates the actual CRC value of the marker
  uint32_t actualCrc() const {
    static uint32_t const empty = 0;

    uint32_t crc = TRI_InitialCrc32();
    crc = TRI_BlockCrc32(crc, data(), 4);  // calculate crc for first 4 bytes
    crc = TRI_BlockCrc32(crc, reinterpret_cast<char const*>(&empty),
                         sizeof(empty));
    crc = TRI_BlockCrc32(crc, data() + 8,
                         _length - 8);  // calculate crc for rest of marker
    crc = TRI_FinalCrc32(crc);
    return crc;
  }

  uint64_t tick() const { return readAlignedNumber<uint64_t>(_begin + 8, 8); }

  uint8_t* payload() const { return _begin + headerLength(); }

  template <typename T>
  T readNumber(uint8_t const* start, uint64_t length) const {
    return MarkerHelper::readNumber<T>(start, static_cast<uint32_t>(length));
  }

  template <typename T>
  T readAlignedNumber(uint8_t const* start, uint64_t length) const {
    TRI_ASSERT(reinterpret_cast<uintptr_t>(start) % sizeof(T) == 0);
    // TODO: create an optimized version for aligned data access
    return readNumber<T>(start, length);
  }

 protected:
  uint64_t calculateLength() const {
    if (*_begin & 0x80) {
      return readAlignedNumber<uint64_t>(_begin + 8, 8);
    }
    return readNumber<uint64_t>(_begin + 1, 3);
  }

 protected:
  uint8_t* _begin;
  uint64_t _length;
};

class MarkerWriter : public MarkerReader {
 public:
  MarkerWriter(uint8_t* begin) : MarkerReader(begin) {}

 public:
  // calculates the marker's CRC values and stores it
  uint32_t storeCrc() {
    // invalidate crc data in marker
    MarkerHelper::storeNumber(_begin + 4, 0, 4);
    // recalculate crc
    uint32_t crc = TRI_InitialCrc32();
    crc = TRI_BlockCrc32(crc, data(), _length);
    crc = TRI_FinalCrc32(crc);
    MarkerHelper::storeNumber(_begin + 4, crc, 4);
    return crc;
  }

  template <typename T>
  void storeNumber(uint8_t* start, T value, uint64_t length) const {
    MarkerHelper::storeNumber<T>(start, value, static_cast<uint32_t>(length));
  }

  template <typename T>
  void storeAlignedNumber(uint8_t* start, T value, uint64_t length) const {
    TRI_ASSERT(reinterpret_cast<uintptr_t>(start) % sizeof(T) == 0);
    // TODO: create an optimized version for aligned data access
    storeNumber<T>(start, value, length);
  }
};

template <typename T>
class MarkerAccessorMeta : public T {
  /* this is a marker for meta data (header, footer etc)
     its layout is:

     BaseMarker      base (16 or 24 bytes)
  */

 public:
  MarkerAccessorMeta(uint8_t* begin) : T(begin) {}

  static uint64_t staticLength() { return 0; }
};

typedef MarkerAccessorMeta<MarkerReader> MarkerReaderMeta;

class MarkerWriterMeta : public MarkerAccessorMeta<MarkerWriter> {
 public:
  MarkerWriterMeta(uint8_t* begin) : MarkerAccessorMeta<MarkerWriter>(begin) {}
};

template <typename T>
class MarkerAccessorDocumentPreface : public T {
  /* this is a preface marker for documents operations
     its layout is:

     BaseMarker      base (16 or 24 bytes)
     uint64_t        database id
     uint64_t        collection id
  */

 public:
  MarkerAccessorDocumentPreface(uint8_t* begin) : T(begin) {}

 public:
  uint64_t database() const {
    return MarkerReader::readAlignedNumber<uint64_t>(MarkerReader::payload(),
                                                     8);
  }

  uint64_t collection() const {
    return MarkerReader::readAlignedNumber<uint64_t>(
        MarkerReader::payload() + 8, 8);
  }

  static uint64_t staticLength() { return 16; }
};

typedef MarkerAccessorDocumentPreface<MarkerReader> MarkerReaderDocumentPreface;

class MarkerWriterDocumentPreface
    : public MarkerAccessorDocumentPreface<MarkerWriter> {
 public:
  MarkerWriterDocumentPreface(uint8_t* begin)
      : MarkerAccessorDocumentPreface<MarkerWriter>(begin) {}

 public:
  void database(uint64_t id) {
    MarkerWriter::storeAlignedNumber<uint64_t>(MarkerWriter::payload(), id, 8);
  }

  void collection(uint64_t id) {
    MarkerWriter::storeAlignedNumber<uint64_t>(MarkerWriter::payload() + 8, id,
                                               8);
  }
};

template <typename T>
class MarkerAccessorDocument : public T {
  /* this is a combined marker for documents / edges and deletions.
     its layout is:

     BaseMarker      base (16 or 24 bytes)
     uint64_t        transaction id
     VersionedVPack  VPack with document value

     VersionedVPack is one byte for the VPack version, followed
     by the actual VPack value
  */

 public:
  MarkerAccessorDocument(uint8_t* begin) : T(begin) {}

 public:
  uint64_t transaction() const {
    return MarkerReader::readAlignedNumber<uint64_t>(MarkerReader::payload(),
                                                     8);
  }

  uint8_t* versionedVPackValue() const { return this->payload() + 8; }

  uint8_t* vPackValue() const { return versionedVPackValue() + 1; }

  VPackSlice slice() const { return VPackSlice(this->vpackValue(), StorageOptions::getOptions()); }

  static uint64_t staticLength() { return 8; }
};

typedef MarkerAccessorDocument<MarkerReader> MarkerReaderDocument;

class MarkerWriterDocument : public MarkerAccessorDocument<MarkerWriter> {
 public:
  MarkerWriterDocument(uint8_t* begin)
      : MarkerAccessorDocument<MarkerWriter>(begin) {}

 public:
  void transaction(uint64_t tid) const {
    MarkerWriter::storeAlignedNumber<uint64_t>(MarkerReader::payload(), tid, 8);
  }
};

template <typename T>
class MarkerAccessorTransaction : public T {
  /* this is a marker accessor for transaction handling.
     its layout is:

     BaseMarker      base (16 or 24 bytes)
     uint64_t        transaction id
  */

 public:
  MarkerAccessorTransaction(uint8_t* begin) : T(begin) {}

 public:
  uint64_t transaction() const {
    return MarkerReader::readAlignedNumber<uint64_t>(MarkerReader::payload(),
                                                     8);
  }

  static uint64_t staticLength() { return 0; }
};

typedef MarkerAccessorTransaction<MarkerReader> MarkerReaderTransaction;

class MarkerWriterTransaction : public MarkerAccessorTransaction<MarkerWriter> {
  /* this is a marker accessor for transaction handling.
     its layout is:

     BaseMarker      base (16 or 24 bytes)
     uint64_t        transaction id
  */

 public:
  MarkerWriterTransaction(uint8_t* begin)
      : MarkerAccessorTransaction<MarkerWriter>(begin) {}

 public:
  void transaction(uint64_t tid) const {
    MarkerWriter::storeAlignedNumber<uint64_t>(MarkerReader::payload(), tid, 8);
  }
};

template <typename T>
class MarkerAccessorStructural : public T {
  /* this is a marker accessor for structural data
     (i.e. collections, indexes, databases)
     its layout is:

     BaseMarker      base (16 or 24 bytes)
     VersionedVPack  VPack with document value

     VersionedVPack is one byte for the VPack version, followed
     by the actual VPack value
  */

 public:
  MarkerAccessorStructural(uint8_t* begin) : T(begin) {}

 public:
  uint8_t* versionedVPackValue() const { return MarkerReader::payload() + 8; }

  uint8_t* vPackValue() const { return versionedVPackValue() + 1; }
  
  VPackSlice slice() const { return VPackSlice(this->vpackValue(), StorageOptions::getOptions()); }

  static uint64_t staticLength() { return 0; }
};

template <typename T>
class MarkerAccessorDatabase : public MarkerAccessorStructural<T> {
  /* this is a marker accessor for databases.
     its layout is:

     BaseMarker      base (16 or 24 bytes)
     VersionedVPack  VPack with document value

     VersionedVPack is one byte for the VPack version, followed
     by the actual VPack value
  */

 public:
  MarkerAccessorDatabase(uint8_t* begin) : MarkerAccessorStructural<T>(begin) {}

  static uint64_t staticLength() { return 0; }
};

typedef MarkerAccessorDatabase<MarkerReader> MarkerReaderDatabase;

class MarkerWriterDatabase : public MarkerAccessorDatabase<MarkerWriter> {
 public:
  MarkerWriterDatabase(uint8_t* begin)
      : MarkerAccessorDatabase<MarkerWriter>(begin) {}
};

template <typename T>
class MarkerAccessorCollection : public MarkerAccessorStructural<T> {
  /* this is a marker accessor for collections.
     its layout is:

     BaseMarker      base (16 or 24 bytes)
     VersionedVPack  VPack with more data value

     VersionedVPack is one byte for the VPack version, followed
     by the actual VPack value
  */

 public:
  MarkerAccessorCollection(uint8_t* begin)
      : MarkerAccessorStructural<T>(begin) {}

  static uint64_t staticLength() { return 0; }
};

typedef MarkerAccessorCollection<MarkerReader> MarkerReaderCollection;

class MarkerWriterCollection : public MarkerAccessorCollection<MarkerWriter> {
 public:
  MarkerWriterCollection(uint8_t* begin)
      : MarkerAccessorCollection<MarkerWriter>(begin) {}
};

template <typename T>
class MarkerAccessorIndex : public MarkerAccessorStructural<T> {
  /* this is a marker accessor for indexes.
     its layout is:

     BaseMarker      base (16 or 24 bytes)
     uint64_t        transaction id
  */

 public:
  MarkerAccessorIndex(uint8_t* begin) : MarkerAccessorStructural<T>(begin) {}

  static uint64_t staticLength() { return 0; }
};

typedef MarkerAccessorIndex<MarkerReader> MarkerReaderIndex;

class MarkerWriterIndex : public MarkerReaderIndex {
 public:
  MarkerWriterIndex(uint8_t* begin) : MarkerReaderIndex(begin) {}
};
}

std::ostream& operator<<(std::ostream&, arangodb::MarkerReader const*);

#endif
