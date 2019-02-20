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
#include "utils/attributes.hpp"
#include "utils/math_utils.hpp"
#include "utils/noncopyable.hpp"

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @struct encryption
/// @brief directory encryption provider
//////////////////////////////////////////////////////////////////////////////
struct encryption : public stored_attribute {
  DECLARE_ATTRIBUTE_TYPE();

  ////////////////////////////////////////////////////////////////////////////
  /// @struct stream
  ////////////////////////////////////////////////////////////////////////////
  struct stream {
    DECLARE_UNIQUE_PTR(stream);

    virtual ~stream() = default;

    virtual size_t block_size() const = 0;

    virtual bool decrypt(uint64_t offset, byte_type* data, size_t size) = 0;

    virtual bool encrypt(uint64_t offset, byte_type* data, size_t size) = 0;
  };

   /// @returns the length of the header that is added to every file
   ///          and used for storing encryption options
  virtual size_t header_length() = 0;

  /// @brief an allocated block of header memory for a new file
  virtual bool create_header(
    const std::string& filename,
    byte_type* header
  ) = 0;

  /// @returns a cipher stream for a file given file name
  virtual stream::ptr create_stream(
    const std::string& filename,
    const bytes_ref& header
  ) = 0;
};

// -----------------------------------------------------------------------------
// --SECTION--                                                           helpers
// -----------------------------------------------------------------------------

inline irs::encryption* get_encryption(const attribute_store& attrs) NOEXCEPT {
  auto enc = attrs.get<irs::encryption>();

  return enc ? enc.get() : nullptr;
}

bool write_encryption_header(
  const std::string& filename,
  index_output& out,
  encryption* enc,
  bstring& buf
);

/// @brief initialize an encryption header and create corresponding cipher stream
/// @returns true if cipher stream was initialized, false if encryption is not
///          appliclabe
/// @throws index_error in case of error on header or stream creation
bool encrypt(
  const std::string& filename,
  encryption* enc,
  bstring& header,
  encryption::stream::ptr& cipher
);

/// @brief create corresponding cipher stream from a specified header
/// @returns true if cipher stream was initialized, false if encryption is not
///          appliclabe
/// @throws index_error in case of error on stream creation
bool decrypt(
  const std::string& filename,
  encryption* enc,
  const bstring& header,
  encryption::stream::ptr& cipher
);

/// @returns padding required by a specified cipher for a given size
inline size_t ceil(const encryption::stream& cipher, size_t size) NOEXCEPT {
  assert(cipher.block_size());

  return math::math_traits<size_t>::ceil(size, cipher.block_size());
}

IRESEARCH_API void append_padding(const encryption::stream& cipher, index_output& out);

////////////////////////////////////////////////////////////////////////////////
///// @class cipher
////////////////////////////////////////////////////////////////////////////////
struct cipher {
  virtual ~cipher() = default;

  virtual size_t block_size() const = 0;

  virtual bool encrypt(byte_type* data) const = 0;

  virtual bool decrypt(byte_type* data) const = 0;
}; // cipher

////////////////////////////////////////////////////////////////////////////////
///// @class ctr_cipher_stream
////////////////////////////////////////////////////////////////////////////////
class ctr_cipher_stream : public encryption::stream {
 public:
  explicit ctr_cipher_stream(
      const cipher& cipher,
      const bytes_ref& iv,
      uint64_t counter_base = 0
  ) NOEXCEPT
    : cipher_(&cipher),
      iv_(iv),
      counter_base_(counter_base) {
  }

  virtual size_t block_size() const NOEXCEPT override { return cipher_->block_size(); }

  virtual bool encrypt(uint64_t offset, byte_type* data, size_t size) override;

  virtual bool decrypt(uint64_t offset, byte_type* data, size_t size) override;

 private:
  bool encrypt_block(uint64_t block_index, byte_type* data, byte_type* scratch);

  bool decrypt_block(uint64_t block_index, byte_type* data, byte_type* scratch);

  const cipher* cipher_;
  bstring iv_;
  uint64_t counter_base_;
}; // ctr_cipher_stream

////////////////////////////////////////////////////////////////////////////////
///// @class ctr_encryption
////////////////////////////////////////////////////////////////////////////////
class ctr_encryption : public encryption {
 public:
  static const size_t DEFAULT_HEADER_LENGTH = 4096;

  explicit ctr_encryption(const cipher& cipher) NOEXCEPT
   : cipher_(&cipher) {
  }

  virtual size_t header_length() NOEXCEPT override {
    return DEFAULT_HEADER_LENGTH;
  }

  virtual bool create_header(
    const std::string& filename,
    byte_type* header
  ) override;

  virtual stream::ptr create_stream(
    const std::string& filename,
    const bytes_ref& heder
  ) override;

 private:
  const cipher* cipher_;
}; // ctr_encryption

////////////////////////////////////////////////////////////////////////////////
///// @class encrypted_output
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API encrypted_output final : public irs::index_output, util::noncopyable {
 public:
  encrypted_output(
    index_output::ptr&& out,
    encryption::stream& cipher,
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
  encryption::stream* cipher_;
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
    index_input::ptr&& in,
    encryption::stream& cipher,
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
  encryption::stream* cipher_;
  const size_t block_size_;
  const size_t length_;
}; // encrypted_input

NS_END // ROOT

#endif
