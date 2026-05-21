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

#include "ctr_encryption.hpp"

#include <chrono>

#include "absl/strings/str_cat.h"
#include "shared.hpp"

namespace {

void decode_ctr_header(irs::bytes_view header, size_t block_size,
                       uint64_t& base_counter, irs::bytes_view& iv) {
  IRS_ASSERT(header.size() >= irs::ctr_encryption::MIN_HEADER_LENGTH &&
             header.size() >= 2 * block_size);

  const auto* begin = header.data();
  base_counter = irs::read<uint64_t>(begin);
  iv = irs::bytes_view(header.data() + block_size, block_size);
}

}  // namespace

namespace irs {

////////////////////////////////////////////////////////////////////////////////
///// @class ctr_cipher_stream
////////////////////////////////////////////////////////////////////////////////
class ctr_cipher_stream : public encryption::stream {
 public:
  explicit ctr_cipher_stream(const cipher& cipher, bytes_view iv,
                             uint64_t counter_base) noexcept
    : cipher_(&cipher), iv_(iv), counter_base_(counter_base) {}

  size_t block_size() const noexcept final { return cipher_->block_size(); }

  bool encrypt(uint64_t offset, byte_type* data, size_t size) final;

  bool decrypt(uint64_t offset, byte_type* data, size_t size) final;

 private:
  // for CTR decryption and encryption are the same
  bool crypt_block(uint64_t block_index, byte_type* IRS_RESTRICT data,
                   byte_type* IRS_RESTRICT scratch);

  const cipher* cipher_;
  bstring iv_;
  uint64_t counter_base_;
};

bool ctr_cipher_stream::encrypt(uint64_t offset, byte_type* data, size_t size) {
  const auto block_size = this->block_size();
  IRS_ASSERT(block_size);
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
      block = block_buf.data();
      std::memmove(block + block_offset, data, n);
    }

    if (!crypt_block(block_index, block, scratch.data())) {
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
  IRS_ASSERT(block_size);
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
      block = block_buf.data();
      std::memmove(block + block_offset, data, n);
    }

    if (!crypt_block(block_index, block, scratch.data())) {
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

bool ctr_cipher_stream::crypt_block(uint64_t block_index,
                                    byte_type* IRS_RESTRICT data,
                                    byte_type* IRS_RESTRICT scratch) {
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

// -----------------------------------------------------------------------------
// --SECTION--                                     ctr_encryption implementation
// -----------------------------------------------------------------------------

bool ctr_encryption::create_header(std::string_view filename,
                                   byte_type* header) {
  IRS_ASSERT(header);

  const auto block_size = cipher_->block_size();

  if (!block_size) {
    IRS_LOG_ERROR(absl::StrCat(
      "failed to initialize encryption header with block of size 0, path '",
      filename, "'"));

    return false;
  }

  const auto header_length = this->header_length();

  if (header_length < MIN_HEADER_LENGTH) {
    IRS_LOG_ERROR(absl::StrCat(
      "failed to initialize encryption header of size ", header_length,
      ", need at least ", MIN_HEADER_LENGTH, ", path '", filename, "'"));

    return false;
  }

  if (header_length < 2 * block_size) {
    IRS_LOG_ERROR(absl::StrCat(
      "failed to initialize encryption header of size ", header_length,
      ", need at least ", 2 * block_size, ", path '", filename, "'"));

    return false;
  }

  const auto duration = std::chrono::system_clock::now().time_since_epoch();
  const auto millis =
    std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
  ::srand(uint32_t(millis));

  std::for_each(header, header + header_length, [](byte_type& b) {
    b = static_cast<byte_type>(::rand() & 0xFF);
  });

  uint64_t base_counter;
  bytes_view iv;
  decode_ctr_header(bytes_view(header, header_length), block_size, base_counter,
                    iv);

  // encrypt header starting from 2nd block
  ctr_cipher_stream stream(*cipher_, iv, base_counter);
  if (!stream.encrypt(0, header + 2 * block_size,
                      header_length - 2 * block_size)) {
    IRS_LOG_ERROR(
      absl::StrCat("failed to encrypt header, path '", filename, "'"));

    return false;
  }

  return true;
}

encryption::stream::ptr ctr_encryption::create_stream(std::string_view filename,
                                                      byte_type* header) {
  IRS_ASSERT(header);

  const auto block_size = cipher_->block_size();

  if (!block_size) {
    IRS_LOG_ERROR(absl::StrCat(
      "failed to instantiate encryption stream with block of size 0, path '",
      filename, "'"));

    return nullptr;
  }

  const auto header_length = this->header_length();

  if (header_length < MIN_HEADER_LENGTH) {
    IRS_LOG_ERROR(absl::StrCat(
      "failed to instantiate encryption stream with header of size ",
      header_length, ", need at least ", MIN_HEADER_LENGTH, ", path '",
      filename, "'"));

    return nullptr;
  }

  if (header_length < 2 * block_size) {
    IRS_LOG_ERROR(absl::StrCat(
      "failed to instantiate encryption stream with header of size ",
      header_length, ", need at least ", 2 * block_size, ", path '", filename,
      "'"));

    return nullptr;
  }

  uint64_t base_counter;
  bytes_view iv;
  decode_ctr_header(bytes_view(header, header_length), block_size, base_counter,
                    iv);

  // decrypt the encrypted part of the header
  ctr_cipher_stream stream(*cipher_, iv, base_counter);
  if (!stream.decrypt(0, header + 2 * block_size,
                      header_length - 2 * block_size)) {
    IRS_LOG_ERROR(
      absl::StrCat("failed to decrypt encryption header for instantiation of "
                   "encryption stream, path '",
                   filename, "'"));

    return nullptr;
  }

  return std::make_unique<ctr_cipher_stream>(
    *cipher_, bytes_view(iv.data(), block_size), base_counter);
}

}  // namespace irs
