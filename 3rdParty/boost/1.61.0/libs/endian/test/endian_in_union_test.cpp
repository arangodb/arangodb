//  endian_in_union_test.cpp  -------------------------------------------------//

//  Copyright Beman Dawes 2008

//  Distributed under the Boost Software License, Version 1.0. (See accompanying
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//  See library home page at http://www.boost.org/libs/endian

//----------------------------------------------------------------------------//

#define BOOST_ENDIAN_FORCE_PODNESS

#include <boost/endian/detail/disable_warnings.hpp>

#include <boost/endian/arithmetic.hpp>
#include <boost/detail/lightweight_main.hpp>
#include <cassert>

using namespace boost::endian;

union U
{
  big_int8_t           big_8;
  big_int16_t          big_16;
  big_int24_t          big_24;
  big_int32_t          big_32;
  big_int40_t          big_40;
  big_int48_t          big_48;
  big_int56_t          big_56;
  big_int64_t          big_64;
                   
  big_uint8_t          big_u8;
  big_uint16_t         big_u16;
  big_uint24_t         big_u24;
  big_uint32_t         big_u32;
  big_uint40_t         big_u40;
  big_uint48_t         big_u48;
  big_uint56_t         big_u56;
  big_uint64_t         big_u64;
                   
  little_int8_t        little_8;
  little_int16_t       little_16;
  little_int24_t       little_24;
  little_int32_t       little_32;
  little_int40_t       little_40;
  little_int48_t       little_48;
  little_int56_t       little_56;
  little_int64_t       little_64;
                   
  little_uint8_t       little_u8;
  little_uint16_t      little_u16;
  little_uint24_t      little_u24;
  little_uint32_t      little_u32;
  little_uint40_t      little_u40;
  little_uint48_t      little_u48;
  little_uint56_t      little_u56;
  little_uint64_t      little_u64;
                   
  native_int8_t        native_8;
  native_int16_t       native_16;
  native_int24_t       native_24;
  native_int32_t       native_32;
  native_int40_t       native_40;
  native_int48_t       native_48;
  native_int56_t       native_56;
  native_int64_t       native_64;
                   
  native_uint8_t       native_u8;
  native_uint16_t      native_u16;
  native_uint24_t      native_u24;
  native_uint32_t      native_u32;
  native_uint40_t      native_u40;
  native_uint48_t      native_u48;
  native_uint56_t      native_u56;
  native_uint64_t      native_u64;
};

U foo;

int cpp_main(int, char * [])
{

  return 0;
}

