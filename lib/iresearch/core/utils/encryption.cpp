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

#include "encryption.hpp"

#include "error/error.hpp"
#include "store/data_output.hpp"
#include "store/directory_attributes.hpp"
#include "store/store_utils.hpp"
#include "utils/bytes_utils.hpp"
#include "utils/crc.hpp"
#include "utils/misc.hpp"

#include <absl/strings/str_cat.h>

namespace irs {

bool encrypt(std::string_view filename, index_output& out, encryption* enc,
             bstring& header, encryption::stream::ptr& cipher) {
  header.resize(enc ? enc->header_length() : 0);

  if (header.empty()) {
    // no encryption
    irs::write_string(out, header);
    return false;
  }

  IRS_ASSERT(enc);

  if (!enc->create_header(filename, header.data())) {
    throw index_error{absl::StrCat(
      "failed to initialize encryption header, path '", filename, "'")};
  }

  // header is encrypted here
  irs::write_string(out, header);

  cipher = enc->create_stream(filename, header.data());

  if (!cipher) {
    throw index_error{absl::StrCat(
      "Failed to instantiate encryption stream, path '", filename, "'")};
  }

  if (!cipher->block_size()) {
    throw index_error{absl::StrCat(
      "Failed to instantiate encryption stream with block of size 0, path '",
      filename, "'")};
  }

  // header is decrypted here, write checksum
  crc32c crc;
  crc.process_bytes(header.c_str(), header.size());
  out.write_vlong(crc.checksum());

  return true;
}

bool decrypt(std::string_view filename, index_input& in, encryption* enc,
             encryption::stream::ptr& cipher) {
  auto header = irs::read_string<bstring>(in);

  if (header.empty()) {
    // no encryption
    return false;
  }

  if (!enc) {
    throw index_error{absl::StrCat(
      "Failed to open encrypted file without cipher, path '", filename, "'")};
  }

  if (header.size() != enc->header_length()) {
    throw index_error{
      absl::StrCat("failed to open encrypted file, expect encryption header of "
                   "size ",
                   enc->header_length(), ", got ", header.size(), ", path '",
                   filename, "'")};
  }

  cipher = enc->create_stream(filename, header.data());

  if (!cipher) {
    throw index_error{
      absl::StrCat("Failed to open encrypted file, path '", filename, "'")};
  }

  const auto block_size = cipher->block_size();

  if (!block_size) {
    throw index_error{
      absl::StrCat("Invalid block size 0 specified for encrypted file, path '",
                   filename, "'")};
  }

  // header is decrypted here, check checksum
  crc32c crc;
  crc.process_bytes(header.c_str(), header.size());

  if (crc.checksum() != in.read_vlong()) {
    throw index_error{
      absl::StrCat("Invalid ecryption header, path '", filename, "'")};
  }

  return true;
}

encrypted_output::encrypted_output(index_output& out,
                                   encryption::stream& cipher,
                                   size_t num_buffers)
  : out_(&out),
    cipher_(&cipher),
    buf_size_(cipher.block_size() * std::max(size_t(1), num_buffers)),
    buf_(std::make_unique<byte_type[]>(buf_size_)),
    start_(0),
    pos_(buf_.get()),
    end_(pos_ + buf_size_) {
  IRS_ASSERT(buf_size_);
}

encrypted_output::encrypted_output(index_output::ptr&& out,
                                   encryption::stream& cipher,
                                   size_t num_buffers)
  : encrypted_output(*out, cipher, num_buffers) {
  managed_out_ = std::move(out);
}

void encrypted_output::write_int(int32_t value) {
  if (remain() < sizeof(uint32_t)) {
    irs::write<uint32_t>(*this, value);
  } else {
    irs::write<uint32_t>(pos_, value);
  }
}

void encrypted_output::write_long(int64_t value) {
  if (remain() < sizeof(uint64_t)) {
    irs::write<uint64_t>(*this, value);
  } else {
    irs::write<uint64_t>(pos_, value);
  }
}

void encrypted_output::write_vint(uint32_t v) {
  if (remain() < bytes_io<uint32_t>::const_max_vsize) {
    irs::vwrite<uint32_t>(*this, v);
  } else {
    irs::vwrite<uint32_t>(pos_, v);
  }
}

void encrypted_output::write_vlong(uint64_t v) {
  if (remain() < bytes_io<uint64_t>::const_max_vsize) {
    irs::vwrite<uint64_t>(*this, v);
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
  IRS_ASSERT(pos_ <= end_);
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
    size_t slice_pos = 0;  // position in the input data

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

size_t encrypted_output::file_pointer() const noexcept {
  IRS_ASSERT(buf_.get() <= pos_);
  return start_ + size_t(std::distance(buf_.get(), pos_));
}

void encrypted_output::flush() {
  if (!out_) {
    return;
  }

  IRS_ASSERT(buf_.get() <= pos_);
  const auto size = size_t(std::distance(buf_.get(), pos_));

  if (!cipher_->encrypt(out_->file_pointer(), buf_.get(), size)) {
    throw io_error{absl::StrCat("Buffer size ", size,
                                " is not multiple of cipher block size ",
                                cipher_->block_size())};
  }

  out_->write_bytes(buf_.get(), size);
  start_ += size;
  pos_ = buf_.get();
}

size_t encrypted_output::CloseImpl() {
  flush();
  const auto size = file_pointer();
  start_ = 0;
  pos_ = buf_.get();
  return size;
}

encrypted_input::encrypted_input(index_input& in, encryption::stream& cipher,
                                 size_t num_buffers, size_t padding /* = 0*/)
  : buf_size_(cipher.block_size() * std::max(size_t(1), num_buffers)),
    buf_(std::make_unique<byte_type[]>(buf_size_)),
    in_(&in),
    cipher_(&cipher),
    start_(in_->file_pointer()),
    length_(in_->length() - start_ - padding) {
  IRS_ASSERT(cipher.block_size() && buf_size_);
  IRS_ASSERT(in_ && in_->length() >= in_->file_pointer() + padding);
  buffered_index_input::reset(buf_.get(), buf_size_, 0);
}

encrypted_input::encrypted_input(index_input::ptr&& in,
                                 encryption::stream& cipher, size_t num_buffers,
                                 size_t padding /* = 0*/)
  : encrypted_input(*in, cipher, num_buffers, padding) {
  managed_in_ = std::move(in);
}

encrypted_input::encrypted_input(const encrypted_input& rhs,
                                 index_input::ptr&& in) noexcept
  : buf_size_(rhs.buf_size_),
    buf_(std::make_unique<byte_type[]>(buf_size_)),
    managed_in_(std::move(in)),
    in_(managed_in_.get()),
    cipher_(rhs.cipher_),
    start_(rhs.start_),
    length_(rhs.length_) {
  IRS_ASSERT(cipher_->block_size());
  buffered_index_input::reset(buf_.get(), buf_size_, rhs.file_pointer());
}

int64_t encrypted_input::checksum(size_t offset) const {
  const auto begin = file_pointer();
  const auto end = (std::min)(begin + offset, this->length());

  Finally restore_position = [begin, this]() noexcept {
    // FIXME make me noexcept as I'm begin called from within ~finally()
    const_cast<encrypted_input*>(this)->seek_internal(begin);
  };

  const_cast<encrypted_input*>(this)->seek_internal(begin);

  crc32c crc;
  byte_type buf[DEFAULT_ENCRYPTION_BUFFER_SIZE];

  for (auto pos = begin; pos < end;) {
    const auto to_read = (std::min)(end - pos, sizeof buf);
    pos += const_cast<encrypted_input*>(this)->read_internal(buf, to_read);
    crc.process_bytes(buf, to_read);
  }

  return crc.checksum();
}

index_input::ptr encrypted_input::dup() const {
  auto dup = in_->dup();

  if (!dup) {
    throw io_error{
      absl::StrCat("Failed to duplicate input file, error: ", errno)};
  }

  return index_input::ptr{new encrypted_input{*this, std::move(dup)}};
}

index_input::ptr encrypted_input::reopen() const {
  auto reopened = in_->reopen();

  if (!reopened) {
    throw io_error(absl::StrCat("Failed to reopen input file, error: ", errno));
  }

  return index_input::ptr{new encrypted_input{*this, std::move(reopened)}};
}

void encrypted_input::seek_internal(size_t pos) {
  pos += start_;

  if (pos != in_->file_pointer()) {
    in_->seek(pos);
  }
}

size_t encrypted_input::read_internal(byte_type* b, size_t count) {
  const auto offset = in_->file_pointer();

  const auto read = in_->read_bytes(b, count);

  if (!cipher_->decrypt(offset, b, read)) {
    throw io_error{absl::StrCat("Buffer size ", read,
                                " is not multiple of cipher block size ",
                                cipher_->block_size())};
  }

  return read;
}

}  // namespace irs
