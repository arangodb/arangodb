//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#ifndef IRESEARCH_DATAINPUT_H
#define IRESEARCH_DATAINPUT_H

#include <memory>

#include "error/error.hpp"

#include "utils/bit_utils.hpp"
#include "utils/io_utils.hpp"
#include "utils/checksum.hpp"
#include "utils/string.hpp"
#include "utils/noncopyable.hpp"

NS_ROOT

/* -------------------------------------------------------------------
* data_input
* ------------------------------------------------------------------*/

struct IRESEARCH_API data_input {
  virtual ~data_input();

  virtual uint8_t read_byte() = 0;

  virtual size_t read_bytes(byte_type* b, size_t count) = 0;

  virtual size_t file_pointer() const = 0;
  
  virtual size_t length() const = 0;

  virtual bool eof() const = 0;

  // TODO: make functions virtual and override it in subclass in 
  // order to achieve better performance

  int16_t read_short();

  int32_t read_int();

  uint32_t read_vint();

  int64_t read_long();

  uint64_t read_vlong();
};

/* -------------------------------------------------------------------
* index_input
* ------------------------------------------------------------------*/

struct IRESEARCH_API index_input : public data_input {
 public:
  DECLARE_PTR(index_input);
  DECLARE_FACTORY(index_input);

  virtual ~index_input();
  virtual ptr dup() const NOEXCEPT = 0; // non-thread-safe fd copy (offset preserved)
  virtual ptr reopen() const NOEXCEPT = 0; // thread-safe new low-level-fd (offset preserved)
  virtual void seek(size_t pos) = 0;

 private:
  index_input& operator=( const index_input& ) = delete;
};

/* -------------------------------------------------------------------
* input_buf
* ------------------------------------------------------------------*/

class IRESEARCH_API input_buf: public std::streambuf, util::noncopyable {
 public:
  typedef std::streambuf::char_type char_type;
  typedef std::streambuf::int_type int_type;

  input_buf( index_input* in );

  virtual std::streamsize showmanyc() override;

  virtual std::streamsize xsgetn(char_type* s, std::streamsize size) override;

  virtual int_type uflow() override;

  operator index_input&() { return *in_; }

 private:
  index_input* in_;
};

/* -------------------------------------------------------------------
* buffered_index_input
* ------------------------------------------------------------------*/

class IRESEARCH_API buffered_index_input : public index_input {
 public:
  virtual ~buffered_index_input();

  virtual byte_type read_byte() final;

  virtual size_t read_bytes(byte_type* b, size_t count) final;

  virtual size_t file_pointer() const final {
    return start_ + offset();
  }

  virtual bool eof() const final {
    return file_pointer() >= length();
  }

  virtual void seek(size_t pos) final;

  size_t buffer_size() const { return buf_size_; }

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
};

NS_END

#endif // IRESEARCH_DATAINPUT_H
