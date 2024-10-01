// Copyright 2000 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

//
//

#include "s2/util/coding/coder.h"

#include <cassert>

#include <algorithm>
#include <cstdint>
#include <utility>

#include "absl/utility/utility.h"

#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "s2/base/port.h"

Encoder::Encoder(Encoder&& other)
    : buf_(absl::exchange(other.buf_, nullptr)),
      limit_(absl::exchange(other.limit_, nullptr)),
      underlying_buffer_(absl::exchange(other.underlying_buffer_, nullptr)),
      orig_(absl::exchange(other.orig_, nullptr)) {}

Encoder& Encoder::operator=(Encoder&& other) {
  if (this == &other) return *this;
  if (ensure_allowed()) DeleteBuffer(underlying_buffer_, capacity());
  buf_ = absl::exchange(other.buf_, nullptr);
  limit_ = absl::exchange(other.limit_, nullptr);
  underlying_buffer_ = absl::exchange(other.underlying_buffer_, nullptr);
  orig_ = absl::exchange(other.orig_, nullptr);
  return *this;
}

Encoder::~Encoder() {
  S2_CHECK_LE(buf_, limit_);  // Catch the buffer overflow.
  if (ensure_allowed()) DeleteBuffer(underlying_buffer_, capacity());
}

int Encoder::varint32_length(uint32 v) { return Varint::Length32(v); }

int Encoder::varint64_length(uint64 v) { return Varint::Length64(v); }

std::pair<unsigned char*, size_t> Encoder::NewBuffer(size_t size) {
  auto* p = std::allocator<unsigned char>().allocate(size);
  return {p, size};
}

void Encoder::DeleteBuffer(unsigned char* buf, size_t size) {
  std::allocator<unsigned char>().deallocate(buf, size);
}

void Encoder::EnsureSlowPath(size_t N) {
  S2_CHECK(ensure_allowed());
  assert(avail() < N);
  assert(length() == 0 || orig_ == underlying_buffer_);

  // Double buffer size, but make sure we always have at least N extra bytes
  const size_t current_len = length();
  // Used in opensource; avoid structured bindings (a C++17 feature) to
  // remain C++11-compatible.  b/210097200
  unsigned char* new_buffer;
  size_t new_capacity;
  std::tie(new_buffer, new_capacity) =
      NewBuffer(std::max(current_len + N, 2 * current_len));

  if (underlying_buffer_) {
    memcpy(new_buffer, underlying_buffer_, current_len);
    DeleteBuffer(underlying_buffer_, capacity());
  }
  underlying_buffer_ = new_buffer;

  orig_ = new_buffer;
  limit_ = new_buffer + new_capacity;
  buf_ = orig_ + current_len;
  S2_CHECK(avail() >= N);
}

void Encoder::Resize(size_t N) {
  S2_CHECK(length() >= N);
  buf_ = orig_ + N;
  assert(length() == N);
}
