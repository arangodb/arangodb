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
#include "VocBase/LocalDocumentId.h"
#include "VocBase/vocbase.h"
#include "VocBase/voc-types.h"

namespace arangodb {
class MMFilesIndexLookupContext;

namespace velocypack {
class Slice;
}

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
  void fill(velocypack::Slice const value, uint32_t offset) {
    velocypack::ValueLength len = value.byteSize();
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
  velocypack::Slice slice(MMFilesIndexLookupContext* context) const;
  
  inline bool isOffset() const noexcept {
    return !isInline();
  }

  inline bool isInline() const noexcept {
    return value.data[maxValueLength()] == 1;
  }

 private:
  void setOffset(uint32_t offset) {
    value.offset = offset;
    value.data[maxValueLength()] = 0; // type = offset
  }
    
  void setInline(uint8_t const* data, size_t length) noexcept {
    TRI_ASSERT(length > 0);
    TRI_ASSERT(length <= maxValueLength());
    memcpy(&value.data[0], data, length);
    value.data[maxValueLength()] = 1; // type = inline
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

static_assert(sizeof(MMFilesIndexElementValue) == 12, "invalid size of MMFilesIndexElementValue");

/// @brief hash index element. Do not directly construct it.
struct MMFilesHashIndexElement {
  // Do not use new for this struct, use create()!
 private:
  MMFilesHashIndexElement(LocalDocumentId const& documentId, std::vector<std::pair<arangodb::velocypack::Slice, uint32_t>> const& values);

  MMFilesHashIndexElement() = delete;
  MMFilesHashIndexElement(MMFilesHashIndexElement const&) = delete;
  MMFilesHashIndexElement& operator=(MMFilesHashIndexElement const&) = delete;
  ~MMFilesHashIndexElement() = delete;

 public:
  inline bool isSet() const { return _localDocumentId.isSet(); }

  /// @brief get the local document id
  inline LocalDocumentId localDocumentId() const { return _localDocumentId; }
  inline LocalDocumentId::BaseType localDocumentIdValue() const { return _localDocumentId.id(); }
  inline uint64_t hash() const { return _hash & 0xFFFFFFFFULL; }
  
  inline operator bool() const { return _localDocumentId.isSet(); }
  inline bool operator==(MMFilesHashIndexElement const& other) const {
    return _localDocumentId == other._localDocumentId && _hash == other._hash;
  }
  
  inline bool operator<(MMFilesHashIndexElement const& other) const {
    return _localDocumentId < other._localDocumentId;
  }

  /// @brief base memory usage of an index element
  static constexpr size_t baseMemoryUsage(size_t numSubs) {
    return sizeof(LocalDocumentId) + sizeof(uint32_t) + (sizeof(MMFilesIndexElementValue) * numSubs);
  }
  
  inline MMFilesIndexElementValue const* subObject(size_t position) const {
    char const* p = reinterpret_cast<char const*>(this) + baseMemoryUsage(position);
    return reinterpret_cast<MMFilesIndexElementValue const*>(p);
  }
  
  arangodb::velocypack::Slice slice(MMFilesIndexLookupContext* context, size_t position) const;
  
  static uint64_t hash(arangodb::velocypack::Slice const& values);
  static uint64_t hash(std::vector<arangodb::velocypack::Slice> const& values);
  static uint64_t hash(std::vector<std::pair<arangodb::velocypack::Slice, uint32_t>> const& values);
  
  /// @brief allocate a new index element from a vector of slices
  static MMFilesHashIndexElement* initialize(MMFilesHashIndexElement* memory, 
                                             LocalDocumentId const& localDocumentId,
                                             std::vector<std::pair<arangodb::velocypack::Slice, uint32_t>> const& values);
  
 private:
  inline MMFilesIndexElementValue* subObject(size_t position) {
    char* p = reinterpret_cast<char*>(this) + sizeof(LocalDocumentId) + sizeof(uint32_t) + (sizeof(MMFilesIndexElementValue) * position);
    return reinterpret_cast<MMFilesIndexElementValue*>(p);
  }
  
 private:
  LocalDocumentId _localDocumentId;
  uint32_t _hash;
};

/// @brief skiplist index element. Do not directly construct it.
struct MMFilesSkiplistIndexElement {
  // Do not use new for this struct, use create()!
 private:
  MMFilesSkiplistIndexElement(LocalDocumentId const& documentId, std::vector<std::pair<arangodb::velocypack::Slice, uint32_t>> const& values);

  MMFilesSkiplistIndexElement() = delete;
  MMFilesSkiplistIndexElement(MMFilesSkiplistIndexElement const&) = delete;
  MMFilesSkiplistIndexElement& operator=(MMFilesSkiplistIndexElement const&) = delete;
  ~MMFilesSkiplistIndexElement() = delete;

 public:
  inline bool isSet() const { return _localDocumentId.isSet(); }

  /// @brief get the local document id
  inline LocalDocumentId localDocumentId() const { return _localDocumentId; }
  inline LocalDocumentId::BaseType localDocumentIdValue() const { return _localDocumentId.id(); }
  
  inline operator bool() const { return _localDocumentId.isSet(); }
  inline bool operator==(MMFilesSkiplistIndexElement const& other) const {
    return _localDocumentId == other._localDocumentId;
  }

  /// @brief base memory usage of an index element
  static constexpr size_t baseMemoryUsage(size_t numSubs) {
    return sizeof(LocalDocumentId) + (sizeof(MMFilesIndexElementValue) * numSubs);
  }
  
  inline MMFilesIndexElementValue const* subObject(size_t position) const {
    char const* p = reinterpret_cast<char const*>(this) + baseMemoryUsage(position);
    return reinterpret_cast<MMFilesIndexElementValue const*>(p);
  }
  
  arangodb::velocypack::Slice slice(MMFilesIndexLookupContext* context, size_t position) const;
  
  /// @brief allocate a new index element from a vector of slices
  static MMFilesSkiplistIndexElement* initialize(MMFilesSkiplistIndexElement* element,
                                                 LocalDocumentId const& documentId,
                                                 std::vector<std::pair<arangodb::velocypack::Slice, uint32_t>> const& values);
  
 private:
  inline MMFilesIndexElementValue* subObject(size_t position) {
    char* p = reinterpret_cast<char*>(this) + sizeof(LocalDocumentId) + (sizeof(MMFilesIndexElementValue) * position);
    return reinterpret_cast<MMFilesIndexElementValue*>(p);
  }
  
 private:
  LocalDocumentId _localDocumentId;
};

struct MMFilesSimpleIndexElement {
 public:
  // clang does not like:
  // constexpr MMFilesSimpleIndexElement() : _localDocumentId(LocalDocumentId::none()), _hashAndOffset(0) {}
  MMFilesSimpleIndexElement() : _localDocumentId(LocalDocumentId::none()), _hashAndOffset(0) {}

  MMFilesSimpleIndexElement(LocalDocumentId const& documentId, arangodb::velocypack::Slice const& value, uint32_t offset); 

  MMFilesSimpleIndexElement(MMFilesSimpleIndexElement const& other) noexcept : _localDocumentId(other._localDocumentId), _hashAndOffset(other._hashAndOffset) {}

  MMFilesSimpleIndexElement& operator=(MMFilesSimpleIndexElement const& other) noexcept {
    _localDocumentId = other._localDocumentId;
    _hashAndOffset = other._hashAndOffset;
    return *this;
  }

  inline bool isSet() const { return _localDocumentId.isSet(); }

  /// @brief get the local document id
  inline LocalDocumentId localDocumentId() const { return _localDocumentId; }
  
  inline LocalDocumentId::BaseType localDocumentIdValue() const { return _localDocumentId.id(); }
  
  inline uint64_t hash() const noexcept { return _hashAndOffset & 0xFFFFFFFFULL; }
  
  inline uint32_t offset() const noexcept { return static_cast<uint32_t>((_hashAndOffset & 0xFFFFFFFF00000000ULL) >> 32); }
  
  arangodb::velocypack::Slice slice(MMFilesIndexLookupContext*) const;
  
  inline operator bool() const noexcept { return _localDocumentId.isSet(); }
  
  inline bool operator==(MMFilesSimpleIndexElement const& other) const noexcept {
    return _localDocumentId == other._localDocumentId && _hashAndOffset == other._hashAndOffset;
  }
  
  inline bool operator<(MMFilesSimpleIndexElement const& other) const noexcept {
    return _localDocumentId < other._localDocumentId;
  }
  
  static uint64_t hash(arangodb::velocypack::Slice const& value);
  
  inline void updateLocalDocumentId(LocalDocumentId const& documentId, uint32_t offset) { 
    _localDocumentId = documentId; 
    _hashAndOffset &= 0xFFFFFFFFULL; 
    _hashAndOffset |= (static_cast<uint64_t>(offset) << 32);
  }
  
 private:
  LocalDocumentId _localDocumentId;
  uint64_t _hashAndOffset;
};

}

#endif
