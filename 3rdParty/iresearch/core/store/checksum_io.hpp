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

#ifndef IRESEARCH_CHECKSUM_IO_H
#define IRESEARCH_CHECKSUM_IO_H

#include "shared.hpp"
#include "store_utils.hpp"

#include "utils/checksum.hpp"

NS_ROOT

/* -------------------------------------------------------------------
 * checksum_index_output
 *   adapter for index_output implementations that
 *   have no ability to calculate checksum
 * ------------------------------------------------------------------*/

template< typename Checksum >
class checksum_index_output final : public index_output {
 public:
  checksum_index_output(index_output::ptr&& impl) NOEXCEPT
    : impl_(std::move(impl)) {
    assert( impl_ != nullptr ); 
  }

  virtual	~checksum_index_output() {
    release();
  }

  // data_output

  virtual void close() override {
    release();
  }

  virtual void write_byte( byte_type b ) override {
    impl_->write_byte( b );
    crc_.process_byte( b );
  }

  virtual void write_bytes( const byte_type* b, size_t len ) override {
    impl_->write_bytes( b, len );
    crc_.process_bytes( b, len );
  }

  // index_output

  virtual void flush() override {
    impl_->flush();
  }

  virtual size_t file_pointer() const override {
    return impl_->file_pointer();
  }

  virtual int64_t checksum() const override {
    return crc_.checksum();
  }

 private:
  inline void release() {
    if ( !impl_ ) {
      return;
    }

    try {
      impl_->close();
    } catch ( ... ) {}
    impl_.reset( nullptr );
  }

  buffered_checksum< Checksum > crc_;
  std::unique_ptr< index_output > impl_;
};

/* -------------------------------------------------------------------
* checksum_index_input
*   adapter for index_input implementations that
*   provides ability to calculate checksums
* ------------------------------------------------------------------*/

template< typename Checksum >
class checksum_index_input final : public index_input {
 public:
  checksum_index_input() NOEXCEPT { }

  explicit checksum_index_input(index_input::ptr&& impl) NOEXCEPT
      : impl_(std::move(impl)) { 
    assert(impl_);
  }

  virtual ~checksum_index_input() { }

  /* data_input */

  virtual byte_type read_byte() override {
    byte_type b = impl_->read_byte();
    crc_.process_byte(b);
    return b;
  }

  virtual size_t read_bytes(byte_type* b, size_t len) override {
    const auto read = impl_->read_bytes(b, len);
    crc_.process_bytes(b, read);
    return read;
  }

  /* index_input */

  virtual ptr dup() const NOEXCEPT override {
    PTR_NAMED(checksum_index_input, ptr, impl_->dup(), crc_);
    return ptr;
  }

  virtual size_t length() const override {
    return impl_->length();
  }

  virtual size_t file_pointer() const override {
    return impl_->file_pointer();
  }

  virtual ptr reopen() const NOEXCEPT override {
    PTR_NAMED(checksum_index_input, ptr, impl_->reopen(), crc_);
    return ptr;
  }

  virtual void seek(size_t pos) override {
    const auto ptr = impl_->file_pointer();
    if (pos < ptr) {
      // seek backward unsupported
      throw not_supported();
    } else if (pos == ptr) {
      // nothing to do
      return;
    }

    if (!skip_buf_) {
      // lazy skip buffer initialization
      skip_buf_ = memory::make_unique< byte_type[] >(SKIP_BUFFER_SIZE);
    }
    
    iresearch::skip(*this, pos - ptr, skip_buf_.get(), SKIP_BUFFER_SIZE);
  }

  virtual bool eof() const override {
    return impl_->eof();
  }

  int64_t checksum() const {
    return crc_.checksum();
  }

  void swap(checksum_index_input& rhs) NOEXCEPT {
    std::swap(impl_, rhs.impl_);
    crc_.swap(rhs.crc_);
  }

 private:
  checksum_index_input(index_input::ptr&& impl, const buffered_checksum< Checksum >& crc)
    : crc_(crc),
      impl_(std::move(impl)) {
    assert(impl_);
  }

  buffered_checksum< Checksum > crc_;
  std::unique_ptr< byte_type[] > skip_buf_;
  index_input::ptr impl_;
};

NS_END

#endif