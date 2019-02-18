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

#ifndef IRESEARCH_CIPHER_UTILS_H
#define IRESEARCH_CIPHER_UTILS_H

#include "store/data_output.hpp"
#include "store/data_input.hpp"
#include "store/directory_attributes.hpp"
#include "utils/math_utils.hpp"
#include "utils/noncopyable.hpp"

NS_ROOT

// -----------------------------------------------------------------------------
// --SECTION--                                                           helpers
// -----------------------------------------------------------------------------

inline const irs::cipher* get_cipher(const attribute_store& attrs) NOEXCEPT {
  auto cipher = attrs.get<irs::cipher>();

  return cipher ? cipher.get() : nullptr;
}

/// @returns padding required by a specified cipher for a given size
inline size_t ceil(const cipher& cipher, size_t size) NOEXCEPT {
  assert(cipher.block_size());

  return math::math_traits<size_t>::ceil(size, cipher.block_size());
}

IRESEARCH_API void append_padding(const cipher& cipher, index_output& out);

IRESEARCH_API bool encrypt(const irs::cipher& cipher, byte_type* data, size_t length);
IRESEARCH_API bool decrypt(const irs::cipher& cipher, byte_type* data, size_t length);

//////////////////////////////////////////////////////////////////////////////
/// @class encrypted_output
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API encrypted_output final : public irs::index_output, util::noncopyable {
 public:
  encrypted_output(
    index_output::ptr&& out,
    const irs::cipher& cipher,
    size_t buf_size
  );

  virtual void flush() override;

  virtual void close() override;

  virtual size_t file_pointer() const override;

  virtual void write_byte(byte_type b) override final;

  virtual void write_bytes(const byte_type* b, size_t length) override final;

  virtual void write_vint(uint32_t v) override final;

  virtual void write_vlong(uint64_t v) override final;

  virtual void write_int(int32_t v) override final;

  virtual void write_long(int64_t v) override final;

  virtual int64_t checksum() const override final {
    return out_->checksum();
  }

  void append_and_flush() {
    append_padding(*cipher_, *this);
    flush();
  }

  size_t buffer_size() const NOEXCEPT { return buf_size_; }

  index_output::ptr release() NOEXCEPT {
    return std::move(out_);
  }

  const index_output& stream() const NOEXCEPT { return *out_; }

 private:
  // returns number of reamining bytes in the buffer
  FORCE_INLINE size_t remain() const {
    return std::distance(pos_, end_);
  }

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  index_output::ptr out_;
  const irs::cipher* cipher_;
  const size_t buf_size_;
  std::unique_ptr<byte_type[]> buf_;
  size_t start_; // position of buffer in a file
  byte_type* pos_; // position in buffer
  byte_type* end_;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // encrypted_output

class IRESEARCH_API encrypted_input final : public buffered_index_input, util::noncopyable {
 public:
  encrypted_input(
    irs::index_input::ptr&& in,
    const irs::cipher& cipher,
    size_t buf_size,
    size_t padding = 0
  );

  virtual index_input::ptr dup() const override final;

  virtual index_input::ptr reopen() const override final;

  virtual size_t length() const override final {
    return length_;
  }

  virtual int64_t checksum(size_t offset) const override final {
    return in_->checksum(offset);
  }

  const index_input& stream() const NOEXCEPT {
    return *in_;
  }

  index_input::ptr release() NOEXCEPT {
    return std::move(in_);
  }

 protected:
  virtual void seek_internal(size_t pos) override {
    if (pos != file_pointer()) {
      throw not_supported();
    }
  }

  virtual bool read_internal(
    byte_type* b, size_t count, size_t& read
  ) override;

 private:
  index_input::ptr in_;
  const irs::cipher* cipher_;
  const size_t block_size_;
  const size_t length_;
}; // encrypted_input

NS_END // ROOT

#endif
