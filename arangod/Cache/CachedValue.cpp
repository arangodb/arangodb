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

#include "Cache/CachedValue.h"

#include <stdint.h>
#include <cstring>

using namespace arangodb::cache;

const size_t CachedValue::_headerAllocSize = sizeof(CachedValue) +
                                       CachedValue::_padding;

CachedValue* CachedValue::copy() const {
  uint8_t* buf = new uint8_t[size()];
  CachedValue* value = nullptr;
  try {
    value = new (buf + offset()) CachedValue(*this);
  } catch (...) {
    delete[] buf;
    return nullptr;
  }

  return value;
}

CachedValue* CachedValue::construct(void const* k, size_t kSize,
                                    void const* v, size_t vSize) {
  if (kSize == 0 || k == nullptr || (vSize > 0 && v == nullptr) ||
      kSize > maxKeySize || vSize > maxValueSize) {
    return nullptr;
  }

  uint8_t* buf = new uint8_t[_headerAllocSize + kSize + vSize];
  CachedValue* cv = nullptr;
  try {
    uint8_t* aligned = reinterpret_cast<uint8_t*>(
      (reinterpret_cast<size_t>(buf) + _headerAllocOffset) &
      _headerAllocMask);
    size_t offset = buf - aligned;
    cv = new (aligned) CachedValue(offset, k, kSize, v, vSize);
  } catch (...) {
    delete[] buf;
    return nullptr;
  }

  return cv;
}

void CachedValue::operator delete(void* ptr) {
  CachedValue* cv = reinterpret_cast<CachedValue*>(ptr);
  size_t offset = cv->offset();
  cv->~CachedValue();
  delete[] (reinterpret_cast<uint8_t*>(ptr) - offset);
}

CachedValue::CachedValue(size_t off, void const* k, size_t kSize,
                         void const* v, size_t vSize)
  : _refCount(0),
    _keySize(static_cast<uint32_t>(kSize + (off << _offsetShift))),
    _valueSize(vSize) {
  std::memcpy(const_cast<uint8_t*>(key()), k, kSize);
  if (vSize > 0) {
    std::memcpy(const_cast<uint8_t*>(value()), v, vSize);
  }
}

CachedValue::CachedValue(CachedValue const& other)
  : _refCount(0),
    _keySize(other._keySize),
    _valueSize(other._valueSize) {
  std::memcpy(const_cast<uint8_t*>(key()), other.key(),
              keySize() + valueSize());
}
