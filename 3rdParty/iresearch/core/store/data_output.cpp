////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
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
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "shared.hpp"
#include "data_input.hpp"
#include "data_output.hpp"

#include "utils/std.hpp"
#include "utils/bytes_utils.hpp"

#include <memory>

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                         output_buf implementation
// -----------------------------------------------------------------------------

output_buf::output_buf(index_output* out) : out_(out) {
  assert(out_);
}

std::streamsize output_buf::xsputn(const char_type* c, std::streamsize size) {
  out_->write_bytes(reinterpret_cast< const byte_type* >(c), size);
  return size;
}

output_buf::int_type output_buf::overflow(int_type c) {
  out_->write_byte(traits_type::to_char_type(c));
  return c;
}

// -----------------------------------------------------------------------------
// --SECTION--                              buffered_index_output implementation
// -----------------------------------------------------------------------------

void buffered_index_output::write_int(int32_t value) {
  if (remain() < sizeof(uint32_t)) {
    index_output::write_int(value);
  } else {
    irs::write<uint32_t>(pos_, value);
  }
}

void buffered_index_output::write_long(int64_t value) {
  if (remain() < sizeof(uint64_t)) {
    index_output::write_long(value);
  } else {
    irs::write<uint64_t>(pos_, value);
  }
}

void buffered_index_output::write_vint(uint32_t v) {
  if (remain() < bytes_io<uint32_t>::const_max_vsize) {
    index_output::write_vint(v);
  } else {
    irs::vwrite<uint32_t>(pos_, v);
  }
}

void buffered_index_output::write_vlong(uint64_t v) {
  if (remain() < bytes_io<uint64_t>::const_max_vsize) {
    index_output::write_vlong(v);
  } else {
    irs::vwrite<uint64_t>(pos_, v);
  }
}

void buffered_index_output::write_byte(byte_type b) {
  if (pos_ >= end_) {
    flush();
  }

  *pos_++ = b;
}

void buffered_index_output::write_bytes(const byte_type* b, size_t length) {
  assert(pos_ <= end_);
  auto left = size_t(std::distance(pos_, end_));

  // is there enough space in the buffer?
  if (left > length) {
    // we add the data to the end of the buffer
    std::memcpy(pos_, b, length);
    pos_ += length;
  } else {
    // is data larger or equal than buffer?
    if (length >= buf_size_) {
      // we flush the buffer
      if (pos_ > buf_) {
        flush();
      }

      // and write data at once
      flush_buffer(b, length);
      start_ += length;
    } else {
      // we fill/flush the buffer (until the input is written)
      size_t slice_pos_ = 0; // pos_ition in the input data

      while (slice_pos_ < length) {
        auto slice_len = std::min(length - slice_pos_, left);

        std::memcpy(pos_, b + slice_pos_, slice_len);
        slice_pos_ += slice_len;
        pos_ += slice_len;

        // if the buffer is full, flush it
        left -= slice_len;
        if (pos_ == end_) {
          flush();
          left = buf_size_;
        }
      }
    }
  }
}

size_t buffered_index_output::file_pointer() const {
  assert(buf_ <= pos_);
  return start_ + size_t(std::distance(buf_, pos_));
}

void buffered_index_output::flush() {
  assert(buf_ <= pos_);
  const auto size = size_t(std::distance(buf_, pos_));
  flush_buffer(buf_, size);
  start_ += size;
  pos_ = buf_;
}

void buffered_index_output::close() {
  if (pos_ > buf_) {
    flush();
  }
  start_ = 0;
  pos_ = buf_;
}

}
