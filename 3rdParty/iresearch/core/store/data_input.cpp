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

#include "error/error.hpp"
#include "utils/memory.hpp"
#include "utils/numeric_utils.hpp"
#include "utils/std.hpp"

#include <memory>

namespace iresearch {

// -----------------------------------------------------------------------------
// --SECTION--                                          input_buf implementation
// -----------------------------------------------------------------------------

input_buf::input_buf(index_input* in)
  : in_(in) {
  assert(in_);
}

std::streamsize input_buf::xsgetn(input_buf::char_type* c, std::streamsize size) {
#ifdef IRESEARCH_DEBUG
  const auto read = in_->read_bytes(reinterpret_cast<byte_type*>(c), size);
  assert(read == size_t(size));
  UNUSED(read);
#else
  in_->read_bytes(reinterpret_cast<byte_type*>(c), size);
#endif // IRESEARCH_DEBUG
  return size;
}

input_buf::int_type input_buf::underflow() {
  // FIXME add 'peek()' function to 'index_input'
  const auto ch = uflow();
  in_->seek(in_->file_pointer() - 1);
  return ch;
}

input_buf::int_type input_buf::uflow() {
  return traits_type::to_int_type(in_->read_byte());
}

std::streamsize input_buf::showmanyc() {
  return in_->length() - in_->file_pointer();
}

// -----------------------------------------------------------------------------
// --SECTION--                               buffered_index_input implementation
// -----------------------------------------------------------------------------

buffered_index_input::buffered_index_input(
    size_t buf_size/*= DEFAULT_BUFFER_SIZE*/
) noexcept
  : start_(0),
    buf_size_(buf_size) {
}

buffered_index_input::buffered_index_input(
    const buffered_index_input& rhs
) noexcept
  : index_input(),
    start_(rhs.start_ + rhs.offset()),
    buf_size_(rhs.buf_size_) {
}

byte_type buffered_index_input::read_byte() {
  if (begin_ >= end_) {
    refill();
  }

  return *begin_++;
}

int32_t buffered_index_input::read_int() {
  return remain() < sizeof(uint32_t)
    ? data_input::read_int()
    : irs::read<uint32_t>(begin_);
}

int64_t buffered_index_input::read_long() {
  return remain() < sizeof(uint64_t)
    ? data_input::read_long()
    : irs::read<uint64_t>(begin_);
}

uint32_t buffered_index_input::read_vint() {
  return remain() < bytes_io<uint32_t>::const_max_vsize
    ? data_input::read_vint()
    : irs::vread<uint32_t>(begin_);
}

uint64_t buffered_index_input::read_vlong() {
  return remain() < bytes_io<uint64_t>::const_max_vsize
    ? data_input::read_vlong()
    : irs::vread<uint64_t>(begin_);
}

const byte_type* buffered_index_input::read_buffer(size_t size, BufferHint hint) noexcept {
  if (hint == BufferHint::PERSISTENT) {
    // don't support persistent buffers
    return nullptr;
  }

  auto* begin = begin_ + size;

  if (begin > end_) {
    return nullptr;
  }

  std::swap(begin, begin_);
  return begin;
}

size_t buffered_index_input::read_bytes(byte_type* b, size_t count) {
  assert(begin_ <= end_);

  // read remaining data from buffer
  size_t read = std::min(count, remain());
  if (read) {
    std::memcpy(b, begin_, sizeof(byte_type) * read);
    begin_ += read;
  }

  if (read == count) {
    return read; // it's enough for us
  }

  size_t size = count - read;
  b += read;
  if (size < buf_size_) { // refill buffer & copy
    size = std::min(size, refill());
    std::memcpy(b, begin_, sizeof(byte_type) * size);
    begin_ += size;
  } else { // read directly to output buffer if possible
    size  = read_internal(b, size);
    start_ += (offset() + size);
    begin_ = end_ = buf_.get(); // will trigger refill on the next read
  }

  return read += size;
}

size_t buffered_index_input::refill() {
  const auto data_start = this->file_pointer();
  const auto data_end = std::min(data_start + buf_size_, length());

  const ptrdiff_t data_size = data_end - data_start;
  if (data_size <= 0) {
    throw eof_error(); // read past eof
  }

  if (!buf_) {
    buf_ = memory::make_unique<byte_type[]>(buf_size_);
    begin_ = end_ = buf_.get();
    seek_internal(start_);
  }

  begin_ = buf_.get();
  end_ = begin_ + read_internal(buf_.get(), data_size);
  start_ = data_start;

  return data_size;
}

void buffered_index_input::seek(size_t p) {
  if (p >= start_ && p < (start_ + size())) {
    begin_ = buf_.get() + p - start_;
  } else {
    seek_internal(p);
    begin_ = end_ = buf_.get();
    start_ = p;
  }
}

}
