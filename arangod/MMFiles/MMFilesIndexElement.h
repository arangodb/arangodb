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

#ifndef ARANGOD_MMFILES_INDEX_ELEMENT_H
#define ARANGOD_MMFILES_INDEX_ELEMENT_H 1

#include "Basics/Common.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

namespace arangodb {

namespace velocypack {
class Slice;
}

class IndexLookupContext;

/// @brief velocypack sub-object (for indexes, as part of IndexElement,
/// if the last byte in data[] is 0, then the VelocyPack data is managed
/// by the datafile the element is in. If the last byte in data[] is 1, then
/// value.data contains the actual VelocyPack data in place.
struct MMFilesIndexElementValue {
  friend struct MMFilesHashIndexElement;
  friend struct MMFilesSkiplistIndexElement;

 public:
  MMFilesIndexElementValue() {}
  ~MMFilesIndexElementValue() {}

  /// @brief fill an MMFilesIndexElementValue structure with a subvalue
  void fill(VPackSlice const value, uint32_t offset) {
    VPackValueLength len = value.byteSize();
    if (len <= maxValueLength()) {
      setInline(value.start(), static_cast<size_t>(len));
    } else {
      setOffset(offset);
    }
  }

  /// @brief velocypack sub-object (for indexes, as part of IndexElement,
  /// if offset is non-zero, then it is an offset into the VelocyPack data in
  /// the data or WAL file. If offset is 0, then data contains the actual data
  /// in place.
  arangodb::velocypack::Slice slice(IndexLookupContext* context) const;

  inline bool isOffset() const noexcept { return !isInline(); }

  inline bool isInline() const noexcept {
    return value.data[maxValueLength()] == 1;
  }

 private:
  void setOffset(uint32_t offset) {
    value.offset = offset;
    value.data[maxValueLength()] = 0;  // type = offset
  }

  void setInline(uint8_t const* data, size_t length) noexcept {
    TRI_ASSERT(length > 0);
    TRI_ASSERT(length <= maxValueLength());
    memcpy(&value.data[0], data, length);
    value.data[maxValueLength()] = 1;  // type = inline
  }

  static constexpr size_t maxValueLength() noexcept {
    return sizeof(value.data) - 1;
  }

 private:
  union {
    uint8_t data[12];
    uint32_t offset;
  } value;
};

static_assert(sizeof(MMFilesIndexElementValue) == 12,
              "invalid size of MMFilesIndexElementValue");

/// @brief hash index element. Do not directly construct it.
struct MMFilesHashIndexElement {
  // Do not use new for this struct, use create()!
 private:
  MMFilesHashIndexElement(TRI_voc_rid_t revisionId,
                          std::vector<std::pair<VPackSlice, uint32_t>> const& values);

  MMFilesHashIndexElement() = delete;
  MMFilesHashIndexElement(MMFilesHashIndexElement const&) = delete;
  MMFilesHashIndexElement& operator=(MMFilesHashIndexElement const&) = delete;
  ~MMFilesHashIndexElement() = delete;

 public:
  /// @brief get the revision id of the document
  inline TRI_voc_rid_t revisionId() const { return _revisionId; }
  inline uint64_t hash() const { return _hash & 0xFFFFFFFFULL; }

  inline operator bool() const { return _revisionId != 0; }
  inline bool operator==(MMFilesHashIndexElement const& other) const {
    return _revisionId == other._revisionId && _hash == other._hash;
  }

  inline bool operator<(MMFilesHashIndexElement const& other) const {
    return _revisionId < other._revisionId;
  }

  /// @brief base memory usage of an index element
  static constexpr size_t baseMemoryUsage(size_t numSubs) {
    return sizeof(TRI_voc_rid_t) + sizeof(uint32_t) +
           (sizeof(MMFilesIndexElementValue) * numSubs);
  }

  inline MMFilesIndexElementValue const* subObject(size_t position) const {
    char const* p = reinterpret_cast<char const*>(this) + baseMemoryUsage(position);
    return reinterpret_cast<MMFilesIndexElementValue const*>(p);
  }

  arangodb::velocypack::Slice slice(IndexLookupContext* context, size_t position) const;

  static uint64_t hash(arangodb::velocypack::Slice const& values);
  static uint64_t hash(std::vector<arangodb::velocypack::Slice> const& values);
  static uint64_t hash(std::vector<std::pair<arangodb::velocypack::Slice, uint32_t>> const& values);

  /// @brief allocate a new index element from a vector of slices
  static MMFilesHashIndexElement* initialize(
      MMFilesHashIndexElement* memory, TRI_voc_rid_t revisionId,
      std::vector<std::pair<arangodb::velocypack::Slice, uint32_t>> const& values);

 private:
  inline MMFilesIndexElementValue* subObject(size_t position) {
    char* p = reinterpret_cast<char*>(this) + sizeof(TRI_voc_rid_t) +
              sizeof(uint32_t) + (sizeof(MMFilesIndexElementValue) * position);
    return reinterpret_cast<MMFilesIndexElementValue*>(p);
  }

 private:
  TRI_voc_rid_t _revisionId;
  uint32_t _hash;
};

/// @brief skiplist index element. Do not directly construct it.
struct MMFilesSkiplistIndexElement {
  // Do not use new for this struct, use create()!
 private:
  MMFilesSkiplistIndexElement(TRI_voc_rid_t revisionId,
                              std::vector<std::pair<VPackSlice, uint32_t>> const& values);

  MMFilesSkiplistIndexElement() = delete;
  MMFilesSkiplistIndexElement(MMFilesSkiplistIndexElement const&) = delete;
  MMFilesSkiplistIndexElement& operator=(MMFilesSkiplistIndexElement const&) = delete;
  ~MMFilesSkiplistIndexElement() = delete;

 public:
  /// @brief get the revision id of the document
  inline TRI_voc_rid_t revisionId() const { return _revisionId; }

  inline operator bool() const { return _revisionId != 0; }
  inline bool operator==(MMFilesSkiplistIndexElement const& other) const {
    return _revisionId == other._revisionId;
  }

  /// @brief base memory usage of an index element
  static constexpr size_t baseMemoryUsage(size_t numSubs) {
    return sizeof(TRI_voc_rid_t) + (sizeof(MMFilesIndexElementValue) * numSubs);
  }

  inline MMFilesIndexElementValue const* subObject(size_t position) const {
    char const* p = reinterpret_cast<char const*>(this) + baseMemoryUsage(position);
    return reinterpret_cast<MMFilesIndexElementValue const*>(p);
  }

  arangodb::velocypack::Slice slice(IndexLookupContext* context, size_t position) const;

  /// @brief allocate a new index element from a vector of slices
  static MMFilesSkiplistIndexElement* initialize(
      MMFilesSkiplistIndexElement* element, TRI_voc_rid_t revisionId,
      std::vector<std::pair<arangodb::velocypack::Slice, uint32_t>> const& values);

 private:
  inline MMFilesIndexElementValue* subObject(size_t position) {
    char* p = reinterpret_cast<char*>(this) + sizeof(TRI_voc_rid_t) +
              (sizeof(MMFilesIndexElementValue) * position);
    return reinterpret_cast<MMFilesIndexElementValue*>(p);
  }

 private:
  TRI_voc_rid_t _revisionId;
};

struct MMFilesSimpleIndexElement {
 public:
  constexpr MMFilesSimpleIndexElement() : _revisionId(0), _hashAndOffset(0) {}
  MMFilesSimpleIndexElement(TRI_voc_rid_t revisionId,
                            arangodb::velocypack::Slice const& value, uint32_t offset);
  MMFilesSimpleIndexElement(MMFilesSimpleIndexElement const& other)
      : _revisionId(other._revisionId), _hashAndOffset(other._hashAndOffset) {}
  MMFilesSimpleIndexElement& operator=(MMFilesSimpleIndexElement const& other) noexcept {
    _revisionId = other._revisionId;
    _hashAndOffset = other._hashAndOffset;
    return *this;
  }

  /// @brief get the revision id of the document
  inline TRI_voc_rid_t revisionId() const { return _revisionId; }
  inline uint64_t hash() const { return _hashAndOffset & 0xFFFFFFFFULL; }
  inline uint32_t offset() const {
    return static_cast<uint32_t>((_hashAndOffset & 0xFFFFFFFF00000000ULL) >> 32);
  }
  arangodb::velocypack::Slice slice(IndexLookupContext*) const;

  inline operator bool() const { return _revisionId != 0; }
  inline bool operator==(MMFilesSimpleIndexElement const& other) const {
    return _revisionId == other._revisionId && _hashAndOffset == other._hashAndOffset;
  }
  inline bool operator<(MMFilesSimpleIndexElement const& other) const {
    return _revisionId < other._revisionId;
  }

  static uint64_t hash(arangodb::velocypack::Slice const& value);
  inline void updateRevisionId(TRI_voc_rid_t revisionId, uint32_t offset) {
    _revisionId = revisionId;
    _hashAndOffset &= 0xFFFFFFFFULL;
    _hashAndOffset |= (static_cast<uint64_t>(offset) << 32);
  }

 private:
  TRI_voc_rid_t _revisionId;
  uint64_t _hashAndOffset;
};

}  // namespace arangodb

#endif
