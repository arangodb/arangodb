////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <shared.hpp>

#include <absl/crc/crc32c.h>

namespace irs {

class crc32c {
  // TODO(MBkkt) kCrc32Xor == 0 is incorrect, we ignore all first zero bytes,
  //  We can fix it with new directory format and kCrc32Xor = 0xffffffff
  static constexpr uint32_t kCrc32Xor = 0;

 public:
  explicit crc32c(uint32_t seed = 0) noexcept : value_{seed ^ kCrc32Xor} {}

  IRS_FORCE_INLINE void process_bytes(const void* data, size_t size) noexcept {
    value_ = absl::ExtendCrc32c(
      value_, std::string_view{static_cast<const char*>(data), size}, 0);
  }

  IRS_FORCE_INLINE void process_block(const void* begin,
                                      const void* end) noexcept {
    process_bytes(begin, std::distance(static_cast<const char*>(begin),
                                       static_cast<const char*>(end)));
  }

  IRS_FORCE_INLINE uint32_t checksum() const noexcept {
    return static_cast<uint32_t>(value_) ^ kCrc32Xor;
  }

  absl::crc32c_t value_;
};

}  // namespace irs
