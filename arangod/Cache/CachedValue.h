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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_CACHED_VALUE_H
#define ARANGODB_CACHE_CACHED_VALUE_H

#include "Basics/Common.h"

#include <stdint.h>
#include <atomic>

namespace arangodb {
namespace cache {

////////////////////////////////////////////////////////////////////////////////
/// @brief This is the beginning of a cache data entry.
///
/// It will be allocated using new uint8_t[] with the correct size for header,
/// key and value. The key and value reside directly behind the header entries
/// contained in this struct. The reference count is used to lend CachedValues
/// to clients.
////////////////////////////////////////////////////////////////////////////////
struct CachedValue {
  // key size must fit in 3 bytes
  static constexpr size_t maxKeySize = 0x00FFFFFFULL;
  // value size must fit in 4 bytes
  static constexpr size_t maxValueSize = 0xFFFFFFFFULL;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reference count (to avoid premature deletion)
  //////////////////////////////////////////////////////////////////////////////
  inline uint32_t refCount() const { return _refCount.load(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Size of the key in bytes
  //////////////////////////////////////////////////////////////////////////////
  inline size_t keySize() const {
    return static_cast<size_t>(_keySize & _keyMask);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Size of the value in bytes
  //////////////////////////////////////////////////////////////////////////////
  inline size_t valueSize() const { return static_cast<size_t>(_valueSize); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns a pointer offset to the key
  //////////////////////////////////////////////////////////////////////////////
  inline uint8_t const* key() const {
    return (reinterpret_cast<uint8_t const*>(this) + sizeof(CachedValue));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns a pointer offset to the value
  //////////////////////////////////////////////////////////////////////////////
  inline uint8_t const* value() const {
    return (_valueSize == 0)
      ? nullptr
      : reinterpret_cast<uint8_t const*>(this) + sizeof(CachedValue) + 
          keySize();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the allocated size of bytes including the key and value
  //////////////////////////////////////////////////////////////////////////////
  inline size_t size() const {
    return _headerAllocSize + keySize() + valueSize();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Utility method to compare underlying key to external key
  //////////////////////////////////////////////////////////////////////////////
  inline bool sameKey(void const* k, size_t kSize) const {
    return (keySize() == kSize) &&
           (0 == memcmp(key(), k, kSize));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Increase reference count
  //////////////////////////////////////////////////////////////////////////////
  inline void lease() { ++_refCount; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Decrease reference count
  //////////////////////////////////////////////////////////////////////////////
  inline void release() { --_refCount; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks whether value can be freed (i.e. no references to it)
  //////////////////////////////////////////////////////////////////////////////
  inline bool isFreeable() const { return _refCount.load() == 0; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Create a copy of this CachedValue object
  //////////////////////////////////////////////////////////////////////////////
  CachedValue* copy() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Construct a CachedValue object from a given key and value
  //////////////////////////////////////////////////////////////////////////////
  static CachedValue* construct(void const* k, size_t kSize, void const* v,
                                size_t vSize);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Custom deleter to handle casting issues
  //////////////////////////////////////////////////////////////////////////////
  static void operator delete(void* ptr);

 private:
  static constexpr size_t _padding = alignof(std::atomic<uint32_t>) - 1;
  static const size_t _headerAllocSize;
  static constexpr size_t _headerAllocMask = ~_padding;
  static constexpr size_t _headerAllocOffset = _padding;
  static constexpr uint32_t _keyMask = 0x00FFFFFF;
  static constexpr uint32_t _offsetMask = 0xFF000000;
  static constexpr size_t _offsetShift = 24;

  std::atomic<uint32_t> _refCount;
  uint32_t _keySize;
  uint32_t _valueSize;

 private:
  CachedValue(size_t off, void const* k, size_t kSize,
              void const* v, size_t vSize);
  CachedValue(CachedValue const& other);

  inline size_t offset() const {
    return ((_keySize & _offsetMask) >> _offsetShift);
  }
};

};  // end namespace cache
};  // end namespace arangodb

#endif
