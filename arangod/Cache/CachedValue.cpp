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

uint8_t const* CachedValue::key() const {
  uint8_t const* buf = reinterpret_cast<uint8_t const*>(this);
  return (buf + sizeof(CachedValue));
}

uint8_t const* CachedValue::value() const {
  if (valueSize == 0) {
    return nullptr;
  }

  uint8_t const* buf = reinterpret_cast<uint8_t const*>(this);
  return (buf + sizeof(CachedValue) + keySize);
}

uint64_t CachedValue::size() const {
  uint64_t size = sizeof(CachedValue);
  size += keySize;
  size += valueSize;
  return size;
}

bool CachedValue::sameKey(void const* k, uint32_t kSize) const {
  if (keySize != kSize) {
    return false;
  }

  return (0 == memcmp(key(), k, keySize));
}

void CachedValue::lease() { ++refCount; }

void CachedValue::release() { 
  if (--refCount == UINT32_MAX) {
    TRI_ASSERT(false);
  }
}

bool CachedValue::isFreeable() { return (refCount.load() == 0); }

CachedValue* CachedValue::copy() const {
  uint8_t* buf = new uint8_t[size()];
  memcpy(buf, this, size());
  CachedValue* value = reinterpret_cast<CachedValue*>(buf);
  value->refCount = 0;
  return value;
}

CachedValue* CachedValue::construct(void const* k, uint32_t kSize,
                                    void const* v, uint64_t vSize) {
  if (kSize == 0 || k == nullptr || (vSize > 0 && v == nullptr) ||
      kSize > maxKeySize || vSize > maxValueSize) {
    return nullptr;
  }

  uint8_t* buf = new uint8_t[sizeof(CachedValue) + kSize + vSize];
  CachedValue* cv = reinterpret_cast<CachedValue*>(buf);

  cv->refCount = 0;
  cv->keySize = kSize;
  cv->valueSize = vSize;
  std::memcpy(const_cast<uint8_t*>(cv->key()), k, kSize);
  if (vSize > 0) {
    std::memcpy(const_cast<uint8_t*>(cv->value()), v, vSize);
  }

  return cv;
}

void CachedValue::operator delete(void* ptr) {
  delete[] reinterpret_cast<uint8_t*>(ptr);
}
