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

#include "shared.hpp"
#include "data_input.hpp"

#include "error/error.hpp"
#include "utils/memory.hpp"
#include "utils/std.hpp"

#include <memory>

NS_ROOT

/* -------------------------------------------------------------------
* data_input
* ------------------------------------------------------------------*/

data_input::~data_input() { }

int16_t data_input::read_short() {
  uint16_t b = static_cast<uint16_t>(read_byte()) << 8U;
  return b | static_cast<uint16_t>(read_byte());
}

int32_t data_input::read_int() {
  uint32_t b = static_cast<uint32_t>(read_byte()) << 24U;
  b |= static_cast<uint32_t>(read_byte()) << 16U;
  b |= static_cast<uint32_t>(read_byte()) << 8U;
  return b | static_cast<uint32_t>(read_byte());
}

uint32_t data_input::read_vint() {
  uint32_t out = read_byte(); if (!(out & 0x80)) return out;

  uint32_t b;
  out -= 0x80;
  b = read_byte(); out += b << 7; if (!(b & 0x80)) return out;
  out -= 0x80 << 7;
  b = read_byte(); out += b << 14; if (!(b & 0x80)) return out;
  out -= 0x80 << 14;
  b = read_byte(); out += b << 21; if (!(b & 0x80)) return out;
  out -= 0x80 << 21;
  b = read_byte(); out += b << 28;
  // last byte always has MSB == 0, so we don't need to check and subtract 0x80

  return out;
}

int64_t data_input::read_long() {
  uint64_t i = static_cast< uint64_t >( read_int() ) << 32U;
  return i | ( static_cast< uint64_t >( read_int() ) & 0xFFFFFFFFL );
}

uint64_t data_input::read_vlong() {
  const uint64_t MASK = 0x80;
  uint64_t out = read_byte(); if (!(out & MASK)) return out;

  uint64_t b;
  out -= MASK;
  b = read_byte(); out += b << 7; if (!(b & MASK)) return out;
  out -= MASK << 7;
  b = read_byte(); out += b << 14; if (!(b & MASK)) return out;
  out -= MASK << 14;
  b = read_byte(); out += b << 21; if (!(b & MASK)) return out;
  out -= MASK << 21;
  b = read_byte(); out += b << 28; if (!(b & MASK)) return out;
  out -= MASK << 28;
  b = read_byte(); out += b << 35; if (!(b & MASK)) return out;
  out -= MASK << 35;
  b = read_byte(); out += b << 42; if (!(b & MASK)) return out;
  out -= MASK << 42;
  b = read_byte(); out += b << 49; if (!(b & MASK)) return out;
  out -= MASK << 49;
  b = read_byte(); out += b << 56; if (!(b & MASK)) return out;
  out -= MASK << 56;
  b = read_byte(); out += b << 63;
  // last byte always has MSB == 0, so we don't need to check and subtract 0x80

  return out;
}

/* -------------------------------------------------------------------
* index_input
* ------------------------------------------------------------------*/

index_input::~index_input() { }

/* -------------------------------------------------------------------
* input_buf
* ------------------------------------------------------------------*/

input_buf::input_buf( index_input* in )
  : in_( in ) {
  assert( in_ );
}

std::streamsize input_buf::xsgetn(input_buf::char_type* c, std::streamsize size) {
#ifdef IRESEARCH_DEBUG
  const auto read = in_->read_bytes(reinterpret_cast<byte_type*>(c), size);
  assert(read == size_t(size));
#else 
  in_->read_bytes(reinterpret_cast<byte_type*>(c), size);
#endif // IRESEARCH_DEBUG
  return size;
}

input_buf::int_type input_buf::uflow() {
  return in_->read_int();
}

std::streamsize input_buf::showmanyc() {
  return in_->length() - in_->file_pointer();
}

/* -------------------------------------------------------------------
* buffered_index_input
* ------------------------------------------------------------------*/

buffered_index_input::buffered_index_input(size_t buf_size /* = 1024 */)
  : buf_size_(buf_size) {
}

buffered_index_input::buffered_index_input(const buffered_index_input& rhs)
  : index_input(),
    start_(rhs.start_ + rhs.offset()),
    buf_size_(rhs.buf_size_) {
}

buffered_index_input::~buffered_index_input() { }

byte_type buffered_index_input::read_byte() {
  if (begin_ >= end_) {
    refill();
  }

  return *begin_++;
}

size_t buffered_index_input::read_bytes(byte_type* b, size_t count) {
  assert(begin_ <= end_);

  // read remaining data from buffer
  size_t read = std::min(count, remain());
  if (read) {
    std::memcpy(b, begin_, sizeof(byte_type) * read);
    begin_ += read;
    count -= read;
  }

  if (!count) {
    return read; // it's enuogh for us
  }

  size_t size;
  b += read;
  if (count < buf_size_) { // refill buffer & copy
    size = std::min(count, refill());
    std::memcpy(b, begin_, sizeof(byte_type) * size);
    begin_ += size;
  } else { // read directly to output buffer
    size = read_internal(b, count);
    start_ += (offset() + size);
    begin_ = end_ = buf_.get(); // will trigger refill on the next read
  }

  return read += size;
}

size_t buffered_index_input::refill() {
  const auto data_start = this->file_pointer();
  const auto data_end = std::min(data_start + buf_size_, length());

  const ptrdiff_t data_size = data_end - data_start;
  if (data_size <= 0) {
    throw eof_error(); // read past eof
  }

  if (!buf_) {
    buf_ = memory::make_unique<byte_type[]>(buf_size_);
    seek_internal(start_);
  }

  begin_ = buf_.get();
  end_ = begin_ + read_internal(buf_.get(), data_size);
  start_ = data_start;

  return data_size;
}

void buffered_index_input::seek(size_t p) {
  if (p >= start_ && p < (start_ + size())) {
    begin_ = buf_.get() + p - start_;
  } else {
    begin_ = end_ = buf_.get();
    start_ = p;
    seek_internal(p);
  }
}

NS_END
