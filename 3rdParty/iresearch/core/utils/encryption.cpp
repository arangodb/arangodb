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

#include "encryption.hpp"
#include "string_utils.hpp"
#include "error/error.hpp"
#include "store/data_output.hpp"
#include "store/store_utils.hpp"
#include "store/directory_attributes.hpp"
#include "utils/bytes_utils.hpp"

NS_LOCAL

bool decode_ctr_header(
    const irs::bytes_ref& header,
    size_t block_size,
    uint64_t& base_counter,
    irs::bytes_ref& iv
) {
  // FIXME need at least sizeof(uint64_t)
  if (header.size() < 2*block_size) {
    return false;
  }

  const auto* begin = header.c_str();
  base_counter = irs::read<uint64_t>(begin);
  iv = irs::bytes_ref(header.c_str() + block_size, block_size);
  return true;
}

const irs::byte_type PADDING[32] { };

NS_END

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                        encryption
// -----------------------------------------------------------------------------

DEFINE_ATTRIBUTE_TYPE(encryption)

// -----------------------------------------------------------------------------
// --SECTION--                                                           helpers
// -----------------------------------------------------------------------------

bool write_encryption_header(
    const std::string& filename,
    index_output& out,
    encryption* enc,
    bstring& buf
) {
  const size_t size = enc ? enc->header_length() : 0;
  buf.resize(size);

  if (size) {
    assert(enc);
    if (!enc->create_header(filename, &buf[0])) {
      return false;
    }
  }

  irs::write_string(out, buf);
  return true;
}

bool encrypt(
    const std::string& filename,
    encryption* enc,
    bstring& header,
    encryption::stream::ptr& cipher
) {
  const size_t size = enc ? enc->header_length() : 0;

  if (!size) {
    // no encryption
    return true;
  }

  assert(enc);
  header.resize(size);

  if (!enc->create_header(filename, &header[0])) {
    throw index_error(string_utils::to_string(
      "failed to initialize encryption header, path '%s'",
      filename.c_str()
    ));
  }

  cipher = enc->create_stream(filename, header);

  if (!cipher) {
    throw index_error(string_utils::to_string(
      "failed to instantiate encryption stream, path '%s'",
      filename.c_str()
    ));
  }


  const auto block_size = cipher->block_size();

  if (!block_size) {
    IR_FRMT_WARN(
      "failed to instantiate encryption stream with block of size 0, fallback to unencrypted, path '%s'",
      filename.c_str()
    );

    cipher = nullptr;
  }

  return true;
}

bool decrypt(
    const std::string& filename,
    encryption* enc,
    const bstring& header,
    encryption::stream::ptr& cipher
) {
  if (header.empty()) {
    // no encryption
    return true;
  }

  if (!enc) {
    throw index_error(string_utils::to_string(
      "failed to open encrypted file without cipher, path '%s'",
      filename.c_str()
    ));
  }

  if (header.size() != enc->header_length()) {
    throw index_error(string_utils::to_string(
      "failed to open encrypted file, expect encryption header of size " IR_SIZE_T_SPECIFIER ", got " IR_SIZE_T_SPECIFIER ", path '%s'",
      enc->header_length(), header.size(), filename.c_str()
    ));
  }

  cipher = enc->create_stream(filename, header);

  if (!cipher) {
    throw index_error(string_utils::to_string(
      "failed to open encrypted file, path '%s'",
      filename.c_str(), enc->header_length(), header.size()
    ));
  }

  const auto block_size = cipher->block_size();

  if (!block_size) {
    throw index_error(string_utils::to_string(
      "invalid block size 0 specified for encrypted file, path '%s'",
      filename.c_str(), enc->header_length(), header.size()
    ));
  }

  return true;
}

void append_padding(const encryption::stream& cipher, index_output& out) {
  const auto begin = out.file_pointer();

  for (auto padding = ceil(cipher, begin) - begin; padding; ) {
    const auto to_write = std::min(padding, sizeof(PADDING));
    out.write_bytes(PADDING, to_write);
    padding -= to_write;
  }
}

// -----------------------------------------------------------------------------
// --SECTION--                                  ctr_cipher_stream implementation
// -----------------------------------------------------------------------------

bool ctr_cipher_stream::encrypt(uint64_t offset, byte_type* data, size_t size) {
  const auto block_size = this->block_size();
  uint64_t block_index = offset / block_size;
  size_t block_offset = offset % block_size;

  bstring block_buf;
  bstring scratch(block_size, 0);

  // encrypt block by block
  while (true) {
    byte_type* block = data;
    const size_t n = std::min(size, block_size - block_offset);

    if (n != block_size) {
      block_buf.resize(block_size);
      block = &block_buf[0];
      std::memmove(block + block_offset, data, n);
    }

    if (!encrypt_block(block_index, block, &scratch[0])) {
      return false;
    }

    if (block != data) {
      std::memmove(data, block + block_offset, n);
    }

    size -= n;

    if (!size) {
      return true;
    }

    data += n;
    block_offset = 0;
    ++block_index;
  }
}

bool ctr_cipher_stream::decrypt(uint64_t offset, byte_type* data, size_t size) {
  const auto block_size = this->block_size();
  uint64_t block_index = offset / block_size;
  size_t block_offset = offset % block_size;

  bstring block_buf;
  bstring scratch(block_size, 0);

  // decrypt block by block
  while (true) {
    byte_type* block = data;
    const size_t n = std::min(size, block_size - block_offset);

    if (n != block_size) {
      block_buf.resize(block_size, 0);
      block = &block_buf[0];
      std::memmove(block + block_offset, data, n);
    }

    if (!decrypt_block(block_index, block, &scratch[0])) {
      return false;
    }

    if (block != data) {
      std::memmove(data, block + block_offset, n);
    }

    size -= n;

    if (!size) {
      return true;
    }

    data += n;
    block_offset = 0;
    ++block_index;
  }
}

bool ctr_cipher_stream::encrypt_block(
    uint64_t block_index,
    byte_type* data,
    byte_type* scratch
) {
  // init nonce + counter
  const auto block_size = this->block_size();
  std::memmove(scratch, iv_.c_str(), block_size);
  //auto* begin = scratch;
  //irs::write<uint64_t>(begin, 0);//counter_base_ + block_index);

  // encrypt nonce + counter
  if (!cipher_->encrypt(scratch)) {
    return false;
  }

  // XOR data with ciphertext
  for (size_t i = 0; i < block_size; ++i) {
    data[i] ^= scratch[i];
  }

  return true;
}

bool ctr_cipher_stream::decrypt_block(
    uint64_t block_index,
    byte_type* data,
    byte_type* scratch
) {
  // for CTR decryption and encryption are the same
  return encrypt_block(block_index, data, scratch);
}

// -----------------------------------------------------------------------------
// --SECTION--                                     ctr_encryption implementation
// -----------------------------------------------------------------------------

bool ctr_encryption::create_header(
    const std::string& filename,
    byte_type* header
) {
  assert(header);

  const auto block_size = cipher_->block_size();

  if (!block_size) {
    IR_FRMT_ERROR(
      "failed to initialize encryption header with block of size 0, path '%s'",
      filename.c_str()
    );

    return false;
  }

  const auto duration = std::chrono::system_clock::now().time_since_epoch();
  const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();

  ::srand(uint32_t(millis));
  const auto header_length = this->header_length();

  if (header_length < 2*block_size) {
    IR_FRMT_ERROR(
      "failed to initialize encryption header of size " IR_SIZE_T_SPECIFIER ", need at least " IR_SIZE_T_SPECIFIER ", path '%s'",
      header_length, 2*block_size, filename.c_str()
    );

    return false;
  }

  std::for_each(
    header, header + header_length,
    [](byte_type& b) {
      b = static_cast<byte_type>(::rand() & 0xFF);
  });

  uint64_t base_counter;
  bytes_ref iv;
  if (!decode_ctr_header(bytes_ref(header, header_length), block_size, base_counter, iv)) {
    IR_FRMT_ERROR(
      "failed to initialize encryption header of size " IR_SIZE_T_SPECIFIER ", path '%s'",
      header_length, filename.c_str()
    );

    return false;
  }

  // encrypt header starting from 2nd block
  ctr_cipher_stream stream(*cipher_, iv, base_counter);
  if (!stream.encrypt(0, header + 2*block_size, header_length - 2*block_size)) {
    IR_FRMT_ERROR(
      "failed to encrypt header, path '%s'",
      filename.c_str()
    );

    return false;
  }

  return true;
}

encryption::stream::ptr ctr_encryption::create_stream(
    const std::string& filename,
    const bytes_ref& header
) {
  const auto block_size = cipher_->block_size();

  if (!block_size) {
    IR_FRMT_ERROR(
      "failed to instantiate encryption stream with block of size 0, path '%s'",
      filename.c_str()
    );

    return nullptr;
  }

  uint64_t base_counter;
  bytes_ref iv;

  if (!decode_ctr_header(header, block_size, base_counter, iv)) {
    IR_FRMT_ERROR(
      "failed to initialize encryption header of size " IR_SIZE_T_SPECIFIER " for instantiation of encryption stream, path '%s'",
      header.size(), filename.c_str()
    );

    return nullptr;
  }

  // decrypt the encrypted part of the header
  ctr_cipher_stream stream(*cipher_, iv, base_counter);
  if (!stream.decrypt(0, const_cast<byte_type*>(header.c_str()) + 2*block_size, header.size() - 2*block_size)) {
    IR_FRMT_ERROR(
      "failed to decrypt encryption header for instantiation of encryption stream, path '%s'",
      filename.c_str()
    );

    return nullptr;
  }

  return memory::make_unique<ctr_cipher_stream>(*cipher_, bytes_ref(iv.c_str(), block_size), base_counter);
}

// -----------------------------------------------------------------------------
// --SECTION--                                   encrypted_output implementation
// -----------------------------------------------------------------------------

encrypted_output::encrypted_output(
    index_output::ptr&& out,
    encryption::stream& cipher,
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

  if (!cipher_->encrypt(out_->file_pointer(), buf_.get(), size)) {
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
    index_input::ptr&& in,
    encryption::stream& cipher,
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
  const auto offset = in_->file_pointer();

  read = in_->read_bytes(b, count);

  if (!cipher_->decrypt(offset, b, read)) {
    throw io_error(string_utils::to_string(
      "buffer size " IR_SIZE_T_SPECIFIER " is not multiple of cipher block size " IR_SIZE_T_SPECIFIER,
      read, block_size_
    ));
  }

  return true;
}

NS_END
