////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "cipher_utils.hpp"
#include "string_utils.hpp"
#include "error/error.hpp"
#include "store/data_output.hpp"
#include "store/directory_attributes.hpp"

NS_LOCAL

bool decrypt(
    const irs::cipher& cipher,
    irs::byte_type* data,
    size_t length,
    size_t block_size
) {
  assert(block_size == cipher.block_size());
  bool result = true;

  for (auto* end = data + length; data != end; data += block_size) {
    result &= cipher.decrypt(data);
  }

  return result;
}

const irs::byte_type PADDING[32] { };

NS_END

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                           helpers
// -----------------------------------------------------------------------------

void append_padding(const cipher& cipher, index_output& out) {
  const auto begin = out.file_pointer();

  for (auto padding = ceil(cipher, begin) - begin; padding; ) {
    const auto to_write = std::min(padding, sizeof(PADDING));
    out.write_bytes(PADDING, to_write);
    padding -= to_write;
  }
}

bool encrypt(const irs::cipher& cipher, byte_type* data, size_t length) {
  const auto block_size = cipher.block_size();
  assert(block_size);

  if (length % block_size) {
    // can't encrypt unaligned data
    return false;
  }

  bool result = true;

  for (auto* end = data + length; data != end; data += block_size) {
    result &= cipher.encrypt(data);
  }

  return result;
}

bool decrypt(const irs::cipher& cipher, byte_type* data, size_t length) {
  const auto block_size = cipher.block_size();
  assert(block_size);

  if (length % block_size) {
    // can't decrypt unaligned data
    return false;
  }

  return ::decrypt(cipher, data, length, block_size);
}

// -----------------------------------------------------------------------------
// --SECTION--                                   encrypted_output implementation
// -----------------------------------------------------------------------------

encrypted_output::encrypted_output(
    index_output::ptr&& out,
    const irs::cipher& cipher,
    size_t buf_size)
  : out_(std::move(out)),
    cipher_(&cipher),
    buf_size_(cipher.block_size() * std::max(size_t(1), buf_size)),
    buf_(memory::make_unique<byte_type[]>(buf_size_)),
    start_(0),
    pos_(buf_.get()),
    end_(pos_ + buf_size_) {
  assert(buf_size_);
}

void encrypted_output::write_int(int32_t value) {
  if (remain() < sizeof(uint32_t)) {
    index_output::write_int(value);
  } else {
    irs::write<uint32_t>(pos_, value);
  }
}

void encrypted_output::write_long(int64_t value) {
  if (remain() < sizeof(uint64_t)) {
    index_output::write_long(value);
  } else {
    irs::write<uint64_t>(pos_, value);
  }
}

void encrypted_output::write_vint(uint32_t v) {
  if (remain() < bytes_io<uint32_t>::const_max_vsize) {
    index_output::write_vint(v);
  } else {
    irs::vwrite<uint32_t>(pos_, v);
  }
}

void encrypted_output::write_vlong(uint64_t v) {
  if (remain() < bytes_io<uint64_t>::const_max_vsize) {
    index_output::write_vlong(v);
  } else {
    irs::vwrite<uint64_t>(pos_, v);
  }
}

void encrypted_output::write_byte(byte_type b) {
  if (pos_ >= end_) {
    flush();
  }

  *pos_++ = b;
}

void encrypted_output::write_bytes(const byte_type* b, size_t length) {
  assert(pos_ <= end_);
  auto left = size_t(std::distance(pos_, end_));

  // is there enough space in the buffer?
  if (left >= length) {
    // we add the data to the end of the buffer
    std::memcpy(pos_, b, length);
    pos_ += length;

    // if the buffer is full, flush it
    if (end_ == pos_) {
      flush();
    }
  } else {
    // we fill/flush the buffer (until the input is written)
    size_t slice_pos = 0; // position in the input data

    while (slice_pos < length) {
      auto slice_len = std::min(length - slice_pos, left);

      std::memcpy(pos_, b + slice_pos, slice_len);
      slice_pos += slice_len;
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

size_t encrypted_output::file_pointer() const {
  assert(buf_.get() <= pos_);
  return start_ + size_t(std::distance(buf_.get(), pos_));
}

void encrypted_output::flush() {
  if (!out_) {
    return;
  }

  assert(buf_.get() <= pos_);
  const auto size = size_t(std::distance(buf_.get(), pos_));

  if (!encrypt(*cipher_, buf_.get(), size)) {
    throw io_error(string_utils::to_string(
      "buffer size " IR_SIZE_T_SPECIFIER " is not multiple of cipher block size " IR_SIZE_T_SPECIFIER,
      size, cipher_->block_size()
    ));
  }

  out_->write_bytes(buf_.get(), size);
  start_ += size;
  pos_ = buf_.get();
}

void encrypted_output::close() {
  flush();
  start_ = 0;
  pos_ = buf_.get();
}

// -----------------------------------------------------------------------------
// --SECTION--                                    encrypted_input implementation
// -----------------------------------------------------------------------------

encrypted_input::encrypted_input(
    irs::index_input::ptr&& in,
    const irs::cipher& cipher,
    size_t buf_size,
    size_t padding /* = 0*/
) : buffered_index_input(cipher.block_size() * std::max(size_t(1), buf_size)),
    in_(std::move(in)),
    cipher_(&cipher),
    block_size_(cipher.block_size()),
    length_(in_->length() - in_->file_pointer() - padding) {
  assert(block_size_ && buffer_size());
  assert(in_ && in_->length() >= in_->file_pointer() + padding);
}

index_input::ptr encrypted_input::dup() const {
  assert(cipher_->block_size());

  return index_input::make<encrypted_input>(
    in_->dup(), *cipher_, buffer_size() / cipher_->block_size()
  );
}

index_input::ptr encrypted_input::reopen() const {
  assert(cipher_->block_size());

  return index_input::make<encrypted_input>(
    in_->reopen(), *cipher_, buffer_size() / cipher_->block_size()
  );
}

bool encrypted_input::read_internal(byte_type* b, size_t count, size_t& read) {
  if (count % block_size_) {
    // count is not multiple to a block size
    return false;
  }

  read = in_->read_bytes(b, count);

  if (!::decrypt(*cipher_, b, read, block_size_)) {
    throw io_error(string_utils::to_string(
      "buffer size " IR_SIZE_T_SPECIFIER " is not multiple of cipher block size " IR_SIZE_T_SPECIFIER,
      read, block_size_
    ));
  }

  return true;
}

NS_END
