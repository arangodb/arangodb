////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include <algorithm>
#include <array>
#include <cstring>
#include <string_view>
#include <type_traits>

#include "Basics/Endian.h"
#include "Basics/StringUtils.h"
#include "Basics/debugging.h"

namespace arangodb {

// small utility class providing a fixed-size string.
// the maximum length of the string is provided at compile time.
// data can be added to the string until the fixed capacity is exceeded.
// from that point on, all further append operations will do nothing
// (i.e. not append to the buffer or overrun it), nor throw an exception.
// all operations of this class are noexcept.
template<size_t N>
class SizeLimitedString {
  static_assert(N > 0);

 public:
  SizeLimitedString(SizeLimitedString const&) = delete;
  SizeLimitedString& operator=(SizeLimitedString const&) = delete;

  SizeLimitedString() noexcept { clear(); }

  std::string_view view() const noexcept {
    TRI_ASSERT(_offset <= N);
    return std::string_view(&_buffer[0], _offset);
  }

  void clear() noexcept {
    memset(&_buffer[0], 0, sizeof(_buffer));
    _offset = 0;
    _full = false;
  }

  bool empty() const noexcept { return _offset == 0; }

  size_t capacity() const noexcept { return N; }

  size_t size() const noexcept {
    TRI_ASSERT(N >= _offset);
    return _offset;
  }

  void push_back(char c) noexcept {
    if (_full) {
      return;
    }
    _buffer[_offset] = c;
    ++_offset;
    recalculateState();
  }

  SizeLimitedString& append(std::string_view data) noexcept {
    if (!_full) {
      // append as much as is possible (potentially only a prefix)
      size_t length = std::min(data.size(), remaining());
      memcpy(_buffer + _offset, data.data(), length);
      _offset += length;
      recalculateState();
    }
    return *this;
  }

  // append a uint64_t value as a string. will only append the value if
  // there is enough capacity in the buffer for the maximum possible
  // uint64_t value (which is 21 bytes long).
  SizeLimitedString& appendUInt64(uint64_t value) noexcept {
    if (!_full) {
      if (basics::maxUInt64StringSize > remaining()) {
        _full = true;
      } else {
        _offset += basics::StringUtils::itoa(value, _buffer + _offset);
        recalculateState();
      }
    }
    return *this;
  }

  // append a hex-encoded value.
  // will append up to as many bytes as there is remaining space in the buffer.
  template<typename T>
  SizeLimitedString& appendHexValue(T value, bool stripLeadingZeros) noexcept {
    static_assert(std::is_integral_v<T> || std::is_pointer_v<T>);
    // copy value into local buffer
    std::array<unsigned char, sizeof(T)> buffer;
    memcpy(reinterpret_cast<void*>(buffer.data()), &value, sizeof(T));
    if constexpr (basics::isLittleEndian()) {
      std::reverse(buffer.begin(), buffer.end());
    }

    // stringify value. if string is already full, this may do some
    // iterations, but not append to the buffer. we are not optimizing for
    // this case.
    constexpr char chars[] = "0123456789abcdef";
    for (auto c : buffer) {
      if (!stripLeadingZeros || (c >> 4U) != 0) {
        push_back(chars[c >> 4U]);
        stripLeadingZeros = false;
      }
      if (!stripLeadingZeros || (c & 0xfU) != 0) {
        push_back(chars[c & 0xfU]);
        stripLeadingZeros = false;
      }
    }
    if (stripLeadingZeros) {
      push_back('0');
    }

    return *this;
  }

 private:
  size_t remaining() const noexcept {
    TRI_ASSERT(N >= _offset);
    return N - _offset;
  }

  void recalculateState() noexcept {
    _full |= (_offset >= N);
    TRI_ASSERT(_offset <= N);
  }

  char _buffer[N];
  size_t _offset;
  bool _full;
};
}  // namespace arangodb
