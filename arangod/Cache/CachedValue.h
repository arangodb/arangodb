////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CACHE_CACHED_VALUE_H
#define ARANGODB_CACHE_CACHED_VALUE_H

#include <atomic>
#include <cstdint>
#include <cstring>

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
  static constexpr std::size_t maxKeySize = 0x00FFFFFFULL;
  // value size must fit in 4 bytes
  static constexpr std::size_t maxValueSize = 0xFFFFFFFFULL;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Reference count (to avoid premature deletion)
  //////////////////////////////////////////////////////////////////////////////
  inline std::uint32_t refCount() const noexcept { return _refCount.load(); }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Size of the key in bytes
  //////////////////////////////////////////////////////////////////////////////
  inline std::size_t keySize() const noexcept {
    return static_cast<std::size_t>(_keySize & _keyMask);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Size of the value in bytes
  //////////////////////////////////////////////////////////////////////////////
  inline std::size_t valueSize() const noexcept {
    return static_cast<std::size_t>(_valueSize);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns a pointer offset to the key
  //////////////////////////////////////////////////////////////////////////////
  inline std::uint8_t const* key() const noexcept {
    return (reinterpret_cast<std::uint8_t const*>(this) + sizeof(CachedValue));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns a pointer offset to the value
  //////////////////////////////////////////////////////////////////////////////
  inline std::uint8_t const* value() const noexcept {
    return (_valueSize == 0) ? nullptr
                             : reinterpret_cast<std::uint8_t const*>(this) +
                                   sizeof(CachedValue) + keySize();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Returns the allocated size of bytes including the key and value
  //////////////////////////////////////////////////////////////////////////////
  inline std::size_t size() const noexcept {
    return _headerAllocSize + keySize() + valueSize();
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Utility method to compare underlying key to external key
  //////////////////////////////////////////////////////////////////////////////
  inline bool sameKey(void const* k, std::size_t kSize) const noexcept {
    return (keySize() == kSize) && (0 == memcmp(key(), k, kSize));
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Increase reference count
  //////////////////////////////////////////////////////////////////////////////
  inline void lease() noexcept { ++_refCount; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Decrease reference count
  //////////////////////////////////////////////////////////////////////////////
  inline void release() noexcept { --_refCount; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Checks whether value can be freed (i.e. no references to it)
  //////////////////////////////////////////////////////////////////////////////
  inline bool isFreeable() const noexcept { return _refCount.load() == 0; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Create a copy of this CachedValue object
  //////////////////////////////////////////////////////////////////////////////
  CachedValue* copy() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Construct a CachedValue object from a given key and value
  //////////////////////////////////////////////////////////////////////////////
  static CachedValue* construct(void const* k, std::size_t kSize, void const* v,
                                std::size_t vSize);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief Custom deleter to handle casting issues
  //////////////////////////////////////////////////////////////////////////////
  static void operator delete(void* ptr);

 private:
  static constexpr std::size_t _padding = alignof(std::atomic<std::uint32_t>) - 1;
  static const std::size_t _headerAllocSize;
  static constexpr std::size_t _headerAllocMask = ~_padding;
  static constexpr std::size_t _headerAllocOffset = _padding;
  static constexpr std::uint32_t _keyMask = 0x00FFFFFF;
  static constexpr std::uint32_t _offsetMask = 0xFF000000;
  static constexpr std::size_t _offsetShift = 24;

  std::atomic<std::uint32_t> _refCount;
  std::uint32_t _keySize;
  std::uint32_t _valueSize;

 private:
  CachedValue(std::size_t off, void const* k, std::size_t kSize, void const* v,
              std::size_t vSize) noexcept;
  CachedValue(CachedValue const& other) noexcept;

  inline std::size_t offset() const {
    return ((_keySize & _offsetMask) >> _offsetShift);
  }
};

};  // end namespace cache
};  // end namespace arangodb

#endif
