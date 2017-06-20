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

#ifndef IRESEARCH_FORMATS_UTILS_H
#define IRESEARCH_FORMATS_UTILS_H

#include "store/data_output.hpp"
#include "store/data_input.hpp"
#include "store/checksum_io.hpp"

#include "index/field_meta.hpp"

NS_ROOT

void IRESEARCH_API validate_footer(iresearch::index_input& in);

template<typename Type, typename Reader, typename Visitor>
size_t read_all(const Visitor& visitor, Reader& reader, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    Type value;

    reader.read(value);

    if (!visitor(value)) {
      return count - i + 1; // +1 for current iteration
    }
  }

  return count;
}

template<typename Iterator, typename Writer>
Iterator write_all(Writer& writer, Iterator begin, Iterator end) {
  for(; begin != end; ++begin) {
    writer.write(*begin);
  }

  return begin;
}

NS_BEGIN(format_utils)

const int32_t FORMAT_MAGIC = 0x3fd76c17;

const int32_t FOOTER_MAGIC = -FORMAT_MAGIC;

const uint32_t FOOTER_LEN = 2 * sizeof( int32_t ) + sizeof( int64_t );

void IRESEARCH_API write_header(index_output& out, const string_ref& format, int32_t ver);

void IRESEARCH_API write_footer(index_output& out);

int64_t IRESEARCH_API read_checksum(index_input& in);

int32_t IRESEARCH_API check_header(
  index_input& in, const string_ref& format, int32_t min_ver, int32_t max_ver
);

template< typename Checksum >
int64_t check_footer( checksum_index_input< Checksum >& in ) {
  validate_footer( in );

  const int64_t req_checksum = in.checksum();
  const int64_t checksum = in.read_long();

  if ( checksum != req_checksum ) {
    // invalid checksum
    throw index_error();
  }

  return req_checksum;
}

template< typename Checksum >
int64_t check_checksum(const index_input& in) {
  auto clone = in.dup();

  clone->seek(0);

  checksum_index_input<Checksum> check_in(std::move(clone));
  check_in.seek(check_in.length() - FOOTER_LEN);
  return check_footer(check_in);
}

NS_END
NS_END

#endif