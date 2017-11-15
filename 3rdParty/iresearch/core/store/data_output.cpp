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

#include "shared.hpp"
#include "data_input.hpp"
#include "data_output.hpp"

#include "utils/std.hpp"
#include "utils/unicode_utils.hpp"

#include <memory>

NS_ROOT

/* -------------------------------------------------------------------
* data_output
* ------------------------------------------------------------------*/

data_output::~data_output() {}

void data_output::write_short( int16_t i ) {
  write_byte( static_cast< byte_type >( i >> 8 ) );
  write_byte( static_cast< byte_type >( i ) );
}

void data_output::write_int( int32_t i ) {
  write_byte( static_cast< byte_type >( i >> 24 ) );
  write_byte( static_cast< byte_type >( i >> 16 ) );
  write_byte( static_cast< byte_type >( i >> 8 ) );
  write_byte( static_cast< byte_type >( i ) );
}

void data_output::write_vint(uint32_t i) {
  while (i >= 0x80) {
    write_byte(static_cast<byte_type>(i | 0x80));
    i >>= 7;
  }

  write_byte(static_cast<byte_type>(i));
}

void data_output::write_long( int64_t i ) {
  write_int( static_cast< int32_t >( i >> 32 ) );
  write_int( static_cast< int32_t >( i ) );
}

void data_output::write_vlong( uint64_t i ) {
  while (i >= uint64_t(0x80)) {
    write_byte(static_cast<byte_type>(i | uint64_t(0x80)));
    i >>= 7;
  }

  write_byte(static_cast<byte_type>(i));
}

/* -------------------------------------------------------------------
* index_output
* ------------------------------------------------------------------*/

index_output::~index_output() {}

/* -------------------------------------------------------------------
* output_buf
* ------------------------------------------------------------------*/

output_buf::output_buf( index_output* out ) : out_( out ) {
  assert( out_ );
}

std::streamsize output_buf::xsputn( const char_type* c, std::streamsize size ) {
  out_->write_bytes( reinterpret_cast< const byte_type* >( c ), size );
  return size;
}

output_buf::int_type output_buf::overflow( int_type c ) {
  out_->write_int( c );
  return c;
}

/* -------------------------------------------------------------------
* buffered_index_output
* ------------------------------------------------------------------*/

buffered_index_output::buffered_index_output( size_t buf_size )
  : buf(memory::make_unique<byte_type[]>(buf_size)),
    start( 0 ),
    pos( 0 ),
    buf_size( buf_size ) { 
}

buffered_index_output::~buffered_index_output() {}

void buffered_index_output::write_byte( byte_type b ) {
  if ( pos >= buf_size ) {
    flush();
  }

  buf[pos++] = b;
}

void buffered_index_output::write_bytes( const byte_type* b, size_t length ) {
  auto left = buf_size - pos;

  // is there enough space in the buffer?
  if ( left >= length ) {
    // we add the data to the end of the buffer
    std::memcpy( buf.get() + pos, b, length );
    pos += length;

    // if the buffer is full, flush it
    if ( buf_size == pos ) {
      flush();
    }
  } else {
    // is data larger then buffer?
    if ( length > buf_size ) {
      // we flush the buffer
      if ( pos > 0 ) {
        flush();
      }

      // and write data at once
      flush_buffer( b, length );
      start += length;
    } else {
      // we fill/flush the buffer (until the input is written)
      size_t slice_pos = 0; // position in the input data
      while ( slice_pos < length ) {

        auto slice_len = std::min( length - slice_pos, left );

        std::memcpy( buf.get() + pos, b + slice_pos, slice_len );
        slice_pos += slice_len;
        pos += slice_len;

        // if the buffer is full, flush it
        left = buf_size - pos;
        if ( left == 0 ) {
          flush();
          left = buf_size;
        }
      }
    }
  }
}

size_t buffered_index_output::file_pointer() const { 
  return start + pos; 
}

void buffered_index_output::flush() {
  flush_buffer( buf.get(), pos );
  start += pos;
  pos = 0U;
}

void buffered_index_output::close() {
  flush();
  start = 0;
  pos = 0;
}

NS_END
