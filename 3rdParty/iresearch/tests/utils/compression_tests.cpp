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

#ifndef IRESEARCH_COMPRESSION_TESTS_H
#define IRESEARCH_COMPRESSION_TESTS_H

#include "tests_shared.hpp"
#include "utils/compression.hpp"

namespace tests {
namespace detail {

using namespace iresearch;

void compress_decompress_core( const bytes_ref& src) {
  compressor cmp( src.size());
  cmp.compress( src);

  decompressor dcmp( src.size());
  dcmp.decompress( cmp.data());

  tests::assert_equal(src, dcmp.data());
}

} // detail
} // tests

TEST( compression_tests, compress_decompress) {
  using namespace iresearch;

  const bytes_ref alphabet( 
    reinterpret_cast< const byte_type* >(alphabet::ENGLISH),
    strlen(alphabet::ENGLISH)
  );

  tests::detail::compress_decompress_core( generate<bytes>( alphabet, 12314U));
  tests::detail::compress_decompress_core( generate<bytes>( alphabet, 1024U));
  tests::detail::compress_decompress_core( generate<bytes>( alphabet, 512U));
  tests::detail::compress_decompress_core( generate<bytes>( alphabet, 128U));
  tests::detail::compress_decompress_core( bytes_ref::nil);
}

#endif
