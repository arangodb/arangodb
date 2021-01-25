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
////////////////////////////////////////////////////////////////////////////////

#include "shared.hpp"
#include "ctr_encryption.hpp"

#include <chrono>

namespace {

void decode_ctr_header(
    const irs::bytes_ref& header,
    size_t block_size,
    uint64_t& base_counter,
    irs::bytes_ref& iv
) {
  assert(
    header.size() >= irs::ctr_encryption::MIN_HEADER_LENGTH
    && header.size() >= 2*block_size
  );

  const auto* begin = header.c_str();
  base_counter = irs::read<uint64_t>(begin);
  iv = irs::bytes_ref(header.c_str() + block_size, block_size);
}

}

namespace iresearch {

////////////////////////////////////////////////////////////////////////////////
///// @class ctr_cipher_stream
////////////////////////////////////////////////////////////////////////////////
class ctr_cipher_stream : public encryption::stream {
 public:
  explicit ctr_cipher_stream(
      const cipher& cipher,
      const bytes_ref& iv,
      uint64_t counter_base
  ) noexcept
    : cipher_(&cipher),
      iv_(iv),
      counter_base_(counter_base) {
  }

  virtual size_t block_size() const noexcept override {
    return cipher_->block_size();
  }

  virtual bool encrypt(uint64_t offset, byte_type* data, size_t size) override;

  virtual bool decrypt(uint64_t offset, byte_type* data, size_t size) override;

 private:
  bool encrypt_block(uint64_t block_index, byte_type* data, byte_type* scratch);

  bool decrypt_block(uint64_t block_index, byte_type* data, byte_type* scratch);

  const cipher* cipher_;
  bstring iv_;
  uint64_t counter_base_;
}; // ctr_cipher_stream

bool ctr_cipher_stream::encrypt(uint64_t offset, byte_type* data, size_t size) {
  const auto block_size = this->block_size();
  assert(block_size);
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
  assert(block_size);
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
  auto* begin = scratch;
  irs::write<uint64_t>(begin, counter_base_ + block_index);

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

  const auto header_length = this->header_length();

  if (header_length < MIN_HEADER_LENGTH) {
    IR_FRMT_ERROR(
      "failed to initialize encryption header of size " IR_SIZE_T_SPECIFIER ", need at least " IR_SIZE_T_SPECIFIER ", path '%s'",
      header_length, MIN_HEADER_LENGTH, filename.c_str()
    );

    return false;
  }

  if (header_length < 2*block_size) {
    IR_FRMT_ERROR(
      "failed to initialize encryption header of size " IR_SIZE_T_SPECIFIER ", need at least " IR_SIZE_T_SPECIFIER ", path '%s'",
      header_length, 2*block_size, filename.c_str()
    );

    return false;
  }

  const auto duration = std::chrono::system_clock::now().time_since_epoch();
  const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  ::srand(uint32_t(millis));

  std::for_each(
    header, header + header_length,
    [](byte_type& b) {
      b = static_cast<byte_type>(::rand() & 0xFF);
  });

  uint64_t base_counter;
  bytes_ref iv;
  decode_ctr_header(bytes_ref(header, header_length), block_size, base_counter, iv);

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
    byte_type* header
) {
  assert(header);

  const auto block_size = cipher_->block_size();

  if (!block_size) {
    IR_FRMT_ERROR(
      "failed to instantiate encryption stream with block of size 0, path '%s'",
      filename.c_str()
    );

    return nullptr;
  }

  const auto header_length = this->header_length();

  if (header_length < MIN_HEADER_LENGTH) {
    IR_FRMT_ERROR(
      "failed to instantiate encryption stream with header of size " IR_SIZE_T_SPECIFIER ", need at least " IR_SIZE_T_SPECIFIER ", path '%s'",
      header_length, MIN_HEADER_LENGTH, filename.c_str()
    );

    return nullptr;
  }

  if (header_length < 2*block_size) {
    IR_FRMT_ERROR(
      "failed to instantiate encryption stream with header of size " IR_SIZE_T_SPECIFIER ", need at least " IR_SIZE_T_SPECIFIER ", path '%s'",
      header_length, 2*block_size, filename.c_str()
    );

    return nullptr;
  }

  uint64_t base_counter;
  bytes_ref iv;
  decode_ctr_header(bytes_ref(header, header_length), block_size, base_counter, iv);

  // decrypt the encrypted part of the header
  ctr_cipher_stream stream(*cipher_, iv, base_counter);
  if (!stream.decrypt(0, header + 2*block_size, header_length - 2*block_size)) {
    IR_FRMT_ERROR(
      "failed to decrypt encryption header for instantiation of encryption stream, path '%s'",
      filename.c_str()
    );

    return nullptr;
  }

  return memory::make_unique<ctr_cipher_stream>(*cipher_, bytes_ref(iv.c_str(), block_size), base_counter);
}

}
