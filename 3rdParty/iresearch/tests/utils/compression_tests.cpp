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
  tests::detail::compress_decompress_core( bytes_ref::NIL);
}

#endif
