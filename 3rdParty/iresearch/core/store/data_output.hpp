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

#ifndef IRESEARCH_DATAOUTPUT_H
#define IRESEARCH_DATAOUTPUT_H

#include "utils/string.hpp"
#include "utils/io_utils.hpp"
#include "utils/bytes_utils.hpp"
#include "utils/noncopyable.hpp"

#include <streambuf>

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @struct data_output
/// @brief base interface for all low-level output data streams
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API data_output
    : std::iterator<std::output_iterator_tag, byte_type, void, void, void> {

  virtual ~data_output();

  // TODO: remove close method
  virtual void close() = 0;

  virtual void write_byte(byte_type b) = 0;

  virtual void write_bytes(const byte_type* b, size_t len) = 0;

  void write_short(int16_t i) {
    irs::write<uint16_t>(*this, i);
  }

  virtual void write_int(int32_t i) {
    irs::write<uint32_t>(*this, i);
  }

  virtual void write_long(int64_t i) {
    irs::write<uint64_t>(*this, i);
  }

  virtual void write_vint(uint32_t i) {
    irs::vwrite<uint32_t>(*this, i);
  }

  virtual void write_vlong(uint64_t i) {
    irs::vwrite<uint64_t>(*this, i);
  }

  data_output& operator=(byte_type b) {
    write_byte(b);
    return *this;
  }
  data_output& operator*() NOEXCEPT { return *this; }
  data_output& operator++() NOEXCEPT { return *this; }
  data_output& operator++(int) NOEXCEPT { return *this; }
}; // data_output

//////////////////////////////////////////////////////////////////////////////
/// @struct index_output
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API index_output : public data_output {
 public:
  DECLARE_IO_PTR(index_output, close);
  DEFINE_FACTORY_INLINE(index_output);

  virtual ~index_output();

  /* deprecated */
  virtual void flush() = 0;

  virtual size_t file_pointer() const = 0;

  virtual int64_t checksum() const = 0;
}; // index_output

//////////////////////////////////////////////////////////////////////////////
/// @class output_buf
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API output_buf final : public std::streambuf, util::noncopyable {
 public:
  typedef std::streambuf::char_type char_type;
  typedef std::streambuf::int_type int_type;

  explicit output_buf(index_output* out);

  virtual std::streamsize xsputn(
    const char_type* c,
    std::streamsize size
  ) override;

  virtual int_type overflow(int_type c) override;

 private:
  index_output* out_;
}; // output_buf

//////////////////////////////////////////////////////////////////////////////
/// @class buffered_index_output
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API buffered_index_output : public index_output, util::noncopyable {
 public:
  static const size_t DEF_BUFFER_SIZE = 1024;

  buffered_index_output(size_t buf_size = DEF_BUFFER_SIZE);

  virtual ~buffered_index_output();

  virtual void flush() override;

  virtual void close() override;

  virtual size_t file_pointer() const override;

  virtual void write_byte(byte_type b) override final;

  virtual void write_bytes(const byte_type* b, size_t length) override final;

  virtual void write_vint(uint32_t v) override final;

  virtual void write_vlong(uint64_t v) override final;

  virtual void write_int(int32_t v) override final;

  virtual void write_long(int64_t v) override final;

 protected:
  virtual void flush_buffer(const byte_type* b, size_t len) = 0;

 private:
  // returns number of reamining bytes in the buffer
  FORCE_INLINE size_t remain() const {
    return std::distance(pos, end);
  }

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::unique_ptr<byte_type[]> buf;
  size_t start; // position of buffer in a file
  byte_type* pos;   // position in buffer
  byte_type* end;
  size_t buf_size;
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // buffered_index_output

NS_END

#endif // IRESEARCH_DATAOUTPUT_H
