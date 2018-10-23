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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "shared.hpp"
#include "data_input.hpp"
#include "data_output.hpp"

#include "utils/std.hpp"
#include "utils/unicode_utils.hpp"
#include "utils/bytes_utils.hpp"

#include <memory>

NS_ROOT

/* -------------------------------------------------------------------
* data_output
* ------------------------------------------------------------------*/

data_output::~data_output() {}

/* -------------------------------------------------------------------
* index_output
* ------------------------------------------------------------------*/

index_output::~index_output() {}

/* -------------------------------------------------------------------
* output_buf
* ------------------------------------------------------------------*/

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

/* -------------------------------------------------------------------
* buffered_index_output
* ------------------------------------------------------------------*/

buffered_index_output::buffered_index_output(size_t buf_size)
  : buf(memory::make_unique<byte_type[]>(buf_size)),
    start(0),
    pos(buf.get()),
    end(pos + buf_size),
    buf_size(buf_size) {
}

buffered_index_output::~buffered_index_output() {}

void buffered_index_output::write_int(int32_t value) {
  if (remain() < sizeof(uint32_t)) {
    index_output::write_int(value);
  } else {
    irs::write<uint32_t>(pos, value);
  }
}

void buffered_index_output::write_long(int64_t value) {
  if (remain() < sizeof(uint64_t)) {
    index_output::write_long(value);
  } else {
    irs::write<uint64_t>(pos, value);
  }
}

void buffered_index_output::write_vint(uint32_t v) {
  if (remain() < bytes_io<uint32_t>::const_max_vsize) {
    index_output::write_vint(v);
  } else {
    irs::vwrite<uint32_t>(pos, v);
  }
}

void buffered_index_output::write_vlong(uint64_t v) {
  if (remain() < bytes_io<uint64_t>::const_max_vsize) {
    index_output::write_vlong(v);
  } else {
    irs::vwrite<uint64_t>(pos, v);
  }
}

void buffered_index_output::write_byte(byte_type b) {
  if (pos >= end) {
    flush();
  }

  *pos++ = b;
}

void buffered_index_output::write_bytes(const byte_type* b, size_t length) {
  size_t left = std::distance(pos, end);

  // is there enough space in the buffer?
  if (left >= length) {
    // we add the data to the end of the buffer
    std::memcpy(pos, b, length);
    pos += length;

    // if the buffer is full, flush it
    if (end == pos) {
      flush();
    }
  } else {
    // is data larger then buffer?
    if (length > buf_size) {
      // we flush the buffer
      if (pos > buf.get()) {
        flush();
      }

      // and write data at once
      flush_buffer(b, length);
      start += length;
    } else {
      // we fill/flush the buffer (until the input is written)
      size_t slice_pos = 0; // position in the input data

      while (slice_pos < length) {
        auto slice_len = std::min(length - slice_pos, left);

        std::memcpy(pos, b + slice_pos, slice_len);
        slice_pos += slice_len;
        pos += slice_len;

        // if the buffer is full, flush it
        left -= slice_len;
        if (pos == end) {
          flush();
          left = buf_size;
        }
      }
    }
  }
}

size_t buffered_index_output::file_pointer() const { 
  return start + std::distance(buf.get(), pos);
}

void buffered_index_output::flush() {
  const size_t size = std::distance(buf.get(), pos);
  flush_buffer(buf.get(), size);
  start += size;
  pos = buf.get();
}

void buffered_index_output::close() {
  flush();
  start = 0;
  pos = buf.get();
}

NS_END
