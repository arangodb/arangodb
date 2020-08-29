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

#include <cstdint>
#include <cstring>

#include "Cache/CachedValue.h"

#include "Basics/debugging.h"

namespace arangodb::cache {

const std::size_t CachedValue::_headerAllocSize = sizeof(CachedValue) + CachedValue::_padding;

CachedValue* CachedValue::copy() const {
  // cppcheck detects a memory leak here for "buf", but this is a false positive.
  // cppcheck-suppress *
  std::uint8_t* buf = new std::uint8_t[size()];
  CachedValue* value = nullptr;
  try {
    value = new (buf + offset()) CachedValue(*this);
  } catch (...) {
    delete[] buf;
    return nullptr;
  }

  // cppcheck-suppress *
  return value;
}

CachedValue* CachedValue::construct(void const* k, std::size_t kSize,
                                    void const* v, std::size_t vSize) {
  if (kSize == 0 || k == nullptr || (vSize > 0 && v == nullptr) ||
      kSize > maxKeySize || vSize > maxValueSize) {
    return nullptr;
  }

  std::uint8_t* buf = new std::uint8_t[_headerAllocSize + kSize + vSize];
  std::uint8_t* aligned = reinterpret_cast<std::uint8_t*>(
      (reinterpret_cast<std::size_t>(buf) + _headerAllocOffset) & _headerAllocMask);
  std::size_t offset = buf - aligned;
  // ctor of CachedValue is noexcept
  return new (aligned) CachedValue(offset, k, kSize, v, vSize);
}

void CachedValue::operator delete(void* ptr) {
  CachedValue* cv = reinterpret_cast<CachedValue*>(ptr);
  std::size_t offset = cv->offset();
  cv->~CachedValue();
  delete[](reinterpret_cast<std::uint8_t*>(ptr) - offset);
}

CachedValue::CachedValue(std::size_t off, void const* k, std::size_t kSize,
                         void const* v, std::size_t vSize) noexcept
    : _refCount(0),
      _keySize(static_cast<std::uint32_t>(kSize + (off << _offsetShift))),
      _valueSize(static_cast<std::uint32_t>(vSize)) {
  std::memcpy(const_cast<std::uint8_t*>(key()), k, kSize);
  if (vSize > 0) {
    std::memcpy(const_cast<std::uint8_t*>(value()), v, vSize);
  }
}

CachedValue::CachedValue(CachedValue const& other) noexcept
    : _refCount(0), _keySize(other._keySize), _valueSize(other._valueSize) {
  std::memcpy(const_cast<std::uint8_t*>(key()), other.key(), keySize() + valueSize());
}

}  // namespace arangodb::cache
