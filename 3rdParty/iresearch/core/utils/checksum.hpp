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

#ifndef IRESEARCH_UTILS_CHECKSUM_H
#define IRESEARCH_UTILS_CHECKSUM_H

#include "shared.hpp"

#include <memory>
#include <cassert>

NS_ROOT

template< typename Checksum >
class buffered_checksum {
 public:
  typedef typename Checksum::value_type value_type;

  static const size_t DEFAULT_BUF_SIZE = 128;

  explicit buffered_checksum(size_t buf_size = DEFAULT_BUF_SIZE)
    : buf_(new byte_type[buf_size]),
      buf_size_(buf_size),
      size_(0) {
    assert(buf_size_);
  }

  buffered_checksum( const buffered_checksum& rhs )
    : impl_( rhs.impl_ ),
      buf_( new byte_type[rhs.buf_size_] ),
      buf_size_( rhs.buf_size_ ),
      size_( rhs.size_ ) {
    assert( buf_size_ );
    std::memcpy( buf_.get(), rhs.buf_.get(), rhs.size_ );
  }

  void process_byte( byte_type b ) {
    if ( size_ == buf_size_ ) {
      flush();
    }

    buf_[size_++] = b;
  }

  void process_bytes( const void* buffer, size_t count ) {
    if ( count >= buf_size_ ) {
      flush();
      impl_.process_bytes( buffer, count );
    } else {
      if ( size_ + count > buf_size_ ) {
        flush();
      }
      memcpy( buf_.get() + size_, buffer, sizeof( byte_type ) * count );
      size_ += count;
    }
  }

  inline void reset() {
    size_ = 0;
    impl_.reset();
  }

  value_type checksum() const {
    const_cast< buffered_checksum* >( this )->flush();
    return impl_.checksum();
  }

  void swap( buffered_checksum& rhs ) {
    std::swap( impl_, rhs.impl_ );
    std::swap( buf_, rhs.buf_ );
    std::swap( buf_size_, rhs.buf_size_ );
    std::swap( size_, rhs.size_ );
  }

 private:
  buffered_checksum& operator=( const buffered_checksum& rhs ) = delete;

  inline void flush() {
    if ( size_ > 0) {
      impl_.process_bytes( buf_.get(), size_ );
      size_ = 0;
    }
  }

  Checksum impl_;
  std::unique_ptr< byte_type[] > buf_;
  size_t buf_size_;
  size_t size_;
};

NS_END

#endif
