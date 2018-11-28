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

#ifndef IRESEARCH_DATAINPUT_H
#define IRESEARCH_DATAINPUT_H

#include <memory>

#include "error/error.hpp"

#include "utils/bit_utils.hpp"
#include "utils/bytes_utils.hpp"
#include "utils/io_utils.hpp"
#include "utils/string.hpp"
#include "utils/noncopyable.hpp"

#include <iterator>

NS_ROOT

//////////////////////////////////////////////////////////////////////////////
/// @struct data_input
/// @brief base interface for all low-level input data streams
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API data_input
    : std::iterator<std::forward_iterator_tag, byte_type, void, void, void> {

  virtual ~data_input();

  virtual byte_type read_byte() = 0;

  virtual size_t read_bytes(byte_type* b, size_t count) = 0;

  virtual size_t file_pointer() const = 0;

  virtual size_t length() const = 0;

  virtual bool eof() const = 0;

  int16_t read_short() {
    // important to read as unsigned
    return irs::read<uint16_t>(*this);
  }

  virtual int32_t read_int() {
    // important to read as unsigned
    return irs::read<uint32_t>(*this);
  }

  virtual int64_t read_long() {
    // important to read as unsigned
    return irs::read<uint64_t>(*this);
  }

  virtual uint32_t read_vint() {
    return irs::vread<uint32_t>(*this);
  }

  virtual uint64_t read_vlong() {
    return irs::vread<uint64_t>(*this);
  }

  byte_type operator*() { return read_byte(); }
  data_input& operator++() NOEXCEPT { return *this; }
  data_input& operator++(int) NOEXCEPT { return *this; }
}; // data_input

//////////////////////////////////////////////////////////////////////////////
/// @struct index_input
//////////////////////////////////////////////////////////////////////////////
struct IRESEARCH_API index_input : public data_input {
 public:
  DECLARE_UNIQUE_PTR(index_input);
  DEFINE_FACTORY_INLINE(index_input);

  virtual ~index_input();
  virtual ptr dup() const = 0; // non-thread-safe fd copy (offset preserved)
  virtual ptr reopen() const = 0; // thread-safe new low-level-fd (offset preserved)
  virtual void seek(size_t pos) = 0;

  // returns checksum from the current position to a
  // specified offset without changing current position
  virtual int64_t checksum(size_t offset) const = 0;

 private:
  index_input& operator=( const index_input& ) = delete;
}; // index_input

//////////////////////////////////////////////////////////////////////////////
/// @class input_buf
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API input_buf final : public std::streambuf, util::noncopyable {
 public:
  typedef std::streambuf::char_type char_type;
  typedef std::streambuf::int_type int_type;

  explicit input_buf(index_input* in);

  virtual std::streamsize showmanyc() override;

  virtual std::streamsize xsgetn(char_type* s, std::streamsize size) override;

  virtual int_type uflow() override;

  virtual int_type underflow() override;

  operator index_input&() { return *in_; }

 private:
  index_input* in_;
}; // input_buf

//////////////////////////////////////////////////////////////////////////////
/// @class buffered_index_input
//////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API buffered_index_input : public index_input {
 public:
  virtual ~buffered_index_input();

  virtual byte_type read_byte() override final;

  virtual size_t read_bytes(byte_type* b, size_t count) override final;

  virtual size_t file_pointer() const override final {
    return start_ + offset();
  }

  virtual bool eof() const override final {
    return file_pointer() >= length();
  }

  virtual void seek(size_t pos) override final;

  size_t buffer_size() const { return buf_size_; }

  virtual int32_t read_int() override final;

  virtual int64_t read_long() override final;

  virtual uint32_t read_vint() override final;

  virtual uint64_t read_vlong() override final;

 protected:
  explicit buffered_index_input(size_t buf_size = 1024);

  buffered_index_input(const buffered_index_input& rhs);

  virtual void seek_internal(size_t pos) = 0;

  // returns number of bytes read
  virtual size_t read_internal(byte_type* b, size_t count) = 0;

 private:
  // returns number of bytes between begin_ & end_
  size_t refill();

  // returns number of elements between current position and beginning of the buffer
  FORCE_INLINE size_t offset() const { 
    return std::distance(buf_.get(), begin_); 
  }

  // returns number of reamining bytes in the buffer 
  FORCE_INLINE size_t remain() const { 
    return std::distance(begin_, end_); 
  }

  // returns number of valid bytes in the buffer 
  FORCE_INLINE size_t size() const { 
    return std::distance(buf_.get(), end_); 
  }

  IRESEARCH_API_PRIVATE_VARIABLES_BEGIN
  std::unique_ptr< byte_type[] > buf_; // buffer itself
  byte_type* begin_{ buf_.get() }; // current position in the buffer
  byte_type* end_{ buf_.get() }; // end of the valid bytes in the buffer
  size_t start_{}; // position of the buffer in file
  size_t buf_size_; // size of the buffer in bytes
  IRESEARCH_API_PRIVATE_VARIABLES_END
}; // buffered_index_input

NS_END

#endif // IRESEARCH_DATAINPUT_H
