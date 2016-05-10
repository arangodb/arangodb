////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_SLICE_CONTAINER_H
#define VELOCYPACK_SLICE_CONTAINER_H 1

#include <cstring>
#include <string>

#include "velocypack/velocypack-common.h"
#include "velocypack/Options.h"
#include "velocypack/Slice.h"

namespace arangodb {
namespace velocypack {

class SliceContainer {
 public:
  SliceContainer() = delete;

  SliceContainer(uint8_t const* data, ValueLength length) 
    : _data(nullptr) {

    VELOCYPACK_ASSERT(data != nullptr);
    VELOCYPACK_ASSERT(length > 0);

    _data = new uint8_t[checkOverflow(length)];
    memcpy(_data, data, checkOverflow(length));
  }

  SliceContainer(char const* data, ValueLength length) 
    : SliceContainer(reinterpret_cast<uint8_t const*>(data), length) {
  }

  SliceContainer(Slice const& slice)
    : SliceContainer(slice.begin(), slice.byteSize()) {
  }

  SliceContainer(Slice const* slice)
    : SliceContainer(*slice) {
  }

  // copy a slim buffer
  SliceContainer(SliceContainer const& that) : _data(nullptr) {
    VELOCYPACK_ASSERT(that._data != nullptr);

    ValueLength const length = that.length();
    _data = new uint8_t[checkOverflow(length)];
    memcpy(_data, that._data, checkOverflow(length));
  }

  // copy-assign a slim buffer
  SliceContainer& operator=(SliceContainer const& that) {
    if (this != &that) {
      VELOCYPACK_ASSERT(that._data != nullptr);

      ValueLength const length = that.length();
      auto data = new uint8_t[checkOverflow(length)];
      memcpy(data, that._data, checkOverflow(length));

      delete[] _data;
      _data = data;
    }

    return *this;
  }

  // move a slim buffer
  SliceContainer(SliceContainer&& that) : _data(nullptr) {
    VELOCYPACK_ASSERT(that._data != nullptr);

    _data = that._data;
    that._data = nullptr;
  }

  // move assign a slim buffer
  SliceContainer& operator=(SliceContainer&& that) {
    if (this != &that) {
      VELOCYPACK_ASSERT(that._data != nullptr);

      delete[] _data; // delete our own data first
      _data = that._data;
      that._data = nullptr;
    }

    return *this;
  }

  ~SliceContainer() {
    delete[] _data;
  }

 public:
  inline Slice slice() const {
    if (_data == nullptr) {
      return Slice();
    }
    return Slice(_data);
  } 
  
  inline uint8_t const* begin() const { return _data; }
  inline uint8_t const* data() const { return _data; }

  inline ValueLength size() const { return slice().byteSize(); }
  inline ValueLength length() const { return slice().byteSize(); }
  inline ValueLength byteSize() const { return slice().byteSize(); }
  
 private:
  uint8_t* _data;
  
};

// class should not be different to a raw pointer size-wise
static_assert(sizeof(SliceContainer) == sizeof(void*), "invalid size for SliceContainer");

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
