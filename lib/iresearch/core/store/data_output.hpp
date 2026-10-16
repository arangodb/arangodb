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

#pragma once

#include <streambuf>

#include "utils/bytes_utils.hpp"
#include "utils/io_utils.hpp"
#include "utils/noncopyable.hpp"
#include "utils/string.hpp"

namespace irs {

//////////////////////////////////////////////////////////////////////////////
/// @struct data_output
/// @brief base interface for all low-level output data streams
//////////////////////////////////////////////////////////////////////////////
struct data_output {
  using iterator_category = std::output_iterator_tag;
  using value_type = void;
  using pointer = void;
  using reference = void;
  using difference_type = void;

  virtual ~data_output() = default;

  virtual void write_byte(byte_type b) = 0;

  virtual void write_bytes(const byte_type* b, size_t len) = 0;

  virtual void write_short(int16_t i) { irs::write<uint16_t>(*this, i); }

  virtual void write_int(int32_t i) { irs::write<uint32_t>(*this, i); }

  virtual void write_long(int64_t i) { irs::write<uint64_t>(*this, i); }

  virtual void write_vint(uint32_t i) { irs::vwrite<uint32_t>(*this, i); }

  virtual void write_vlong(uint64_t i) { irs::vwrite<uint64_t>(*this, i); }

  data_output& operator=(byte_type b) {
    write_byte(b);
    return *this;
  }
  data_output& operator*() noexcept { return *this; }
  data_output& operator++() noexcept { return *this; }
  data_output& operator++(int) noexcept { return *this; }
};

class index_output : public data_output {
 public:
  using OnClose = std::function<void(size_t)>;

  DECLARE_IO_PTR(index_output, close);
  DEFINE_FACTORY_INLINE(index_output);

  virtual void flush() = 0;

  void close() {
    const auto size = CloseImpl();
    if (callback_ && *callback_) {
      (*callback_)(size);
    }
  }

  virtual size_t file_pointer() const = 0;

  virtual int64_t checksum() const = 0;

  const OnClose* SetCallback(const OnClose* callback) noexcept {
    return std::exchange(callback_, callback);
  }

 private:
  virtual size_t CloseImpl() = 0;

  const OnClose* callback_{};
};

//////////////////////////////////////////////////////////////////////////////
/// @class output_buf
//////////////////////////////////////////////////////////////////////////////
class output_buf final : public std::streambuf, util::noncopyable {
 public:
  typedef std::streambuf::char_type char_type;
  typedef std::streambuf::int_type int_type;

  explicit output_buf(index_output* out);

  std::streamsize xsputn(const char_type* c, std::streamsize size) final;

  int_type overflow(int_type c) final;

  index_output* internal() const { return out_; }

 private:
  index_output* out_;
};

//////////////////////////////////////////////////////////////////////////////
/// @class buffered_index_output
//////////////////////////////////////////////////////////////////////////////
class buffered_index_output : public index_output, util::noncopyable {
 public:
  void flush() final;

  size_t file_pointer() const final;

  void write_byte(byte_type b) final;

  void write_bytes(const byte_type* b, size_t length) final;

  void write_short(int16_t value) final;

  void write_vint(uint32_t v) final;

  void write_vlong(uint64_t v) final;

  void write_int(int32_t v) final;

  void write_long(int64_t v) final;

  buffered_index_output& operator=(byte_type b) {
    write_byte(b);
    return *this;
  }
  buffered_index_output& operator*() noexcept { return *this; }
  buffered_index_output& operator++() noexcept { return *this; }
  buffered_index_output& operator++(int) noexcept { return *this; }

 protected:
  void reset(byte_type* buf, size_t size) noexcept {
    buf_ = buf;
    pos_ = buf;
    end_ = buf + size;
    buf_size_ = size;
  }

  size_t CloseImpl() override;

  virtual void flush_buffer(const byte_type* b, size_t len) = 0;

  byte_type* buffer() const noexcept { return buf_; }

  size_t buffer_offset() const noexcept { return start_; }

  // returns number of reamining bytes in the buffer
  IRS_FORCE_INLINE size_t remain() const { return std::distance(pos_, end_); }

 private:
  byte_type* buf_{};
  byte_type* pos_{};  // position in buffer
  byte_type* end_{};
  size_t start_{};  // position of buffer in a file
  size_t buf_size_{};
};

}  // namespace irs
