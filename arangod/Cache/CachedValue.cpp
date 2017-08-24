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

CachedValue* CachedValue::copy() const {
  uint8_t* buf = new uint8_t[size()];
  memcpy(buf, this, size());
  CachedValue* value = reinterpret_cast<CachedValue*>(buf);
  value->_refCount = 0;
  return value;
}

CachedValue* CachedValue::construct(void const* k, size_t kSize,
                                    void const* v, size_t vSize) {
  if (kSize == 0 || k == nullptr || (vSize > 0 && v == nullptr) ||
      kSize > maxKeySize || vSize > maxValueSize) {
    return nullptr;
  }

  uint8_t* buf = new uint8_t[sizeof(CachedValue) + kSize + vSize];
  CachedValue* cv = reinterpret_cast<CachedValue*>(buf);

  cv->_refCount = 0;
  cv->_keySize = static_cast<uint32_t>(kSize);
  cv->_valueSize = static_cast<uint64_t>(vSize);
  std::memcpy(const_cast<uint8_t*>(cv->key()), k, kSize);
  if (vSize > 0) {
    std::memcpy(const_cast<uint8_t*>(cv->value()), v, vSize);
  }

  return cv;
}

void CachedValue::operator delete(void* ptr) {
  delete[] reinterpret_cast<uint8_t*>(ptr);
}
