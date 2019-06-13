//  buffer_test.cpp  -------------------------------------------------------------------//

//  Copyright Beman Dawes 2014

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

//  See library home page at http://www.boost.org/libs/endian

//--------------------------------------------------------------------------------------//

#include <boost/endian/detail/disable_warnings.hpp>

//#define BOOST_ENDIAN_LOG
#include <boost/endian/buffers.hpp>
#include <boost/detail/lightweight_main.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/cstdint.hpp>
#include <iostream>
#include <sstream>
#include <limits>

using namespace boost::endian;
using std::cout;
using std::endl;

namespace
{

  //  check_size  ------------------------------------------------------------//

  void check_size()
  {

    BOOST_TEST_EQ(sizeof(big_int8_buf_t), 1u);
    BOOST_TEST_EQ(sizeof(big_int16_buf_t), 2u);
    BOOST_TEST_EQ(sizeof(big_int24_buf_t), 3u);
    BOOST_TEST_EQ(sizeof(big_int32_buf_t), 4u);
    BOOST_TEST_EQ(sizeof(big_int40_buf_t), 5u);
    BOOST_TEST_EQ(sizeof(big_int48_buf_t), 6u);
    BOOST_TEST_EQ(sizeof(big_int56_buf_t), 7u);
    BOOST_TEST_EQ(sizeof(big_int64_buf_t), 8u);

    BOOST_TEST_EQ(sizeof(big_uint8_buf_t), 1u);
    BOOST_TEST_EQ(sizeof(big_uint16_buf_t), 2u);
    BOOST_TEST_EQ(sizeof(big_uint24_buf_t), 3u);
    BOOST_TEST_EQ(sizeof(big_uint32_buf_t), 4u);
    BOOST_TEST_EQ(sizeof(big_uint40_buf_t), 5u);
    BOOST_TEST_EQ(sizeof(big_uint48_buf_t), 6u);
    BOOST_TEST_EQ(sizeof(big_uint56_buf_t), 7u);
    BOOST_TEST_EQ(sizeof(big_uint64_buf_t), 8u);

    BOOST_TEST_EQ(sizeof(little_int8_buf_t), 1u);
    BOOST_TEST_EQ(sizeof(little_int16_buf_t), 2u);
    BOOST_TEST_EQ(sizeof(little_int24_buf_t), 3u);
    BOOST_TEST_EQ(sizeof(little_int32_buf_t), 4u);
    BOOST_TEST_EQ(sizeof(little_int40_buf_t), 5u);
    BOOST_TEST_EQ(sizeof(little_int48_buf_t), 6u);
    BOOST_TEST_EQ(sizeof(little_int56_buf_t), 7u);
    BOOST_TEST_EQ(sizeof(little_int64_buf_t), 8u);

    BOOST_TEST_EQ(sizeof(little_uint8_buf_t), 1u);
    BOOST_TEST_EQ(sizeof(little_uint16_buf_t), 2u);
    BOOST_TEST_EQ(sizeof(little_uint24_buf_t), 3u);
    BOOST_TEST_EQ(sizeof(little_uint32_buf_t), 4u);
    BOOST_TEST_EQ(sizeof(little_uint40_buf_t), 5u);
    BOOST_TEST_EQ(sizeof(little_uint48_buf_t), 6u);
    BOOST_TEST_EQ(sizeof(little_uint56_buf_t), 7u);
    BOOST_TEST_EQ(sizeof(little_uint64_buf_t), 8u);

    BOOST_TEST_EQ(sizeof(native_int8_buf_t), 1u);
    BOOST_TEST_EQ(sizeof(native_int16_buf_t), 2u);
    BOOST_TEST_EQ(sizeof(native_int24_buf_t), 3u);
    BOOST_TEST_EQ(sizeof(native_int32_buf_t), 4u);
    BOOST_TEST_EQ(sizeof(native_int40_buf_t), 5u);
    BOOST_TEST_EQ(sizeof(native_int48_buf_t), 6u);
    BOOST_TEST_EQ(sizeof(native_int56_buf_t), 7u);
    BOOST_TEST_EQ(sizeof(native_int64_buf_t), 8u);

    BOOST_TEST_EQ(sizeof(native_uint8_buf_t), 1u);
    BOOST_TEST_EQ(sizeof(native_uint16_buf_t), 2u);
    BOOST_TEST_EQ(sizeof(native_uint24_buf_t), 3u);
    BOOST_TEST_EQ(sizeof(native_uint32_buf_t), 4u);
    BOOST_TEST_EQ(sizeof(native_uint40_buf_t), 5u);
    BOOST_TEST_EQ(sizeof(native_uint48_buf_t), 6u);
    BOOST_TEST_EQ(sizeof(native_uint56_buf_t), 7u);
    BOOST_TEST_EQ(sizeof(native_uint64_buf_t), 8u);

    BOOST_TEST_EQ(sizeof(big_int8_buf_at), 1u);
    BOOST_TEST_EQ(sizeof(big_int16_buf_at), 2u);
    BOOST_TEST_EQ(sizeof(big_int32_buf_at), 4u);
    BOOST_TEST_EQ(sizeof(big_int64_buf_at), 8u);

    BOOST_TEST_EQ(sizeof(big_uint8_buf_at), 1u);
    BOOST_TEST_EQ(sizeof(big_uint16_buf_at), 2u);
    BOOST_TEST_EQ(sizeof(big_uint32_buf_at), 4u);
    BOOST_TEST_EQ(sizeof(big_uint64_buf_at), 8u);

    BOOST_TEST_EQ(sizeof(little_int8_buf_at), 1u);
    BOOST_TEST_EQ(sizeof(little_int16_buf_at), 2u);
    BOOST_TEST_EQ(sizeof(little_int32_buf_at), 4u);
    BOOST_TEST_EQ(sizeof(little_int64_buf_at), 8u);

    BOOST_TEST_EQ(sizeof(little_uint8_buf_at), 1u);
    BOOST_TEST_EQ(sizeof(little_uint16_buf_at), 2u);
    BOOST_TEST_EQ(sizeof(little_uint32_buf_at), 4u);
    BOOST_TEST_EQ(sizeof(little_uint64_buf_at), 8u);
  } // check_size

  //  test_inserter_and_extractor  -----------------------------------------------------//

  void test_inserter_and_extractor()
  {
    std::cout << "test inserter and extractor..." << std::endl;

    big_uint64_buf_t bu64(0x010203040506070ULL);
    little_uint64_buf_t lu64(0x010203040506070ULL);

    boost::uint64_t x;

    std::stringstream ss;

    ss << bu64;
    ss >> x;
    BOOST_TEST_EQ(x, 0x010203040506070ULL);

    ss.clear();
    ss << lu64;
    ss >> x;
    BOOST_TEST_EQ(x, 0x010203040506070ULL);

    ss.clear();
    ss << 0x010203040506070ULL;
    big_uint64_buf_t bu64z(0);
    ss >> bu64z;
    BOOST_TEST_EQ(bu64z.value(), bu64.value());

    ss.clear();
    ss << 0x010203040506070ULL;
    little_uint64_buf_t lu64z(0);
    ss >> lu64z;
    BOOST_TEST_EQ(lu64z.value(), lu64.value());

    std::cout << "test inserter and extractor complete" << std::endl;

  }

  template<class T> struct unaligned
  {
    char x;
    T y;
  };

  template<class T> void test_buffer_type( typename T::value_type v1, typename T::value_type v2 )
  {
    T buffer( v1 );
    BOOST_TEST_EQ( buffer.value(), v1 );

    buffer = v2;
    BOOST_TEST_EQ( buffer.value(), v2 );

    unaligned<T> buffer2 = { 0, T( v1 ) };
    BOOST_TEST_EQ( buffer2.y.value(), v1 );

    buffer2.y = v2;
    BOOST_TEST_EQ( buffer2.y.value(), v2 );
  }

  void test_construction_and_assignment()
  {
    std::cout << "test construction and assignment..." << std::endl;

    test_buffer_type< big_int8_buf_at>( 0x01, -0x01 );
    test_buffer_type<big_int16_buf_at>( 0x0102, -0x0102 );
    test_buffer_type<big_int32_buf_at>( 0x01020304, -0x01020304 );
    test_buffer_type<big_int64_buf_at>( 0x0102030405060708LL, -0x0102030405060708LL );

    test_buffer_type< big_uint8_buf_at>( 0x01, 0xFE );
    test_buffer_type<big_uint16_buf_at>( 0x0102, 0xFE02 );
    test_buffer_type<big_uint32_buf_at>( 0x01020304, 0xFE020304 );
    test_buffer_type<big_uint64_buf_at>( 0x0102030405060708ULL, 0xFE02030405060708LL );

    test_buffer_type< little_int8_buf_at>( 0x01, -0x01 );
    test_buffer_type<little_int16_buf_at>( 0x0102, -0x0102 );
    test_buffer_type<little_int32_buf_at>( 0x01020304, -0x01020304 );
    test_buffer_type<little_int64_buf_at>( 0x0102030405060708LL, -0x0102030405060708LL );

    test_buffer_type< little_uint8_buf_at>( 0x01, 0xFE );
    test_buffer_type<little_uint16_buf_at>( 0x0102, 0xFE02 );
    test_buffer_type<little_uint32_buf_at>( 0x01020304, 0xFE020304 );
    test_buffer_type<little_uint64_buf_at>( 0x0102030405060708ULL, 0xFE02030405060708LL );

    test_buffer_type< big_int8_buf_t>( 0x01, -0x01 );
    test_buffer_type<big_int16_buf_t>( 0x0102, -0x0102 );
    test_buffer_type<big_int24_buf_t>( 0x010203, -0x010203 );
    test_buffer_type<big_int32_buf_t>( 0x01020304, -0x01020304 );
    test_buffer_type<big_int40_buf_t>( 0x0102030405LL, -0x0102030405LL );
    test_buffer_type<big_int48_buf_t>( 0x010203040506LL, -0x010203040506LL );
    test_buffer_type<big_int56_buf_t>( 0x01020304050607LL, -0x01020304050607LL );
    test_buffer_type<big_int64_buf_t>( 0x0102030405060708LL, -0x0102030405060708LL );

    test_buffer_type< little_uint8_buf_t>( 0x01, 0xFE );
    test_buffer_type<little_uint16_buf_t>( 0x0102, 0xFE02 );
    test_buffer_type<little_uint24_buf_t>( 0x010203, 0xFE0203 );
    test_buffer_type<little_uint32_buf_t>( 0x01020304, 0xFE020304 );
    test_buffer_type<little_uint40_buf_t>( 0x0102030405ULL, 0xFE02030405ULL );
    test_buffer_type<little_uint48_buf_t>( 0x010203040506ULL, 0xFE0203040506ULL );
    test_buffer_type<little_uint56_buf_t>( 0x01020304050607ULL, 0xFE020304050607ULL );
    test_buffer_type<little_uint64_buf_t>( 0x0102030405060708ULL, 0xFE02030405060708LL );

    std::cout << "test construction and assignment complete" << std::endl;
  }

  template <typename Int>
  void test_boundary_values_()
  {
    test_buffer_type< endian_buffer<order::big, Int, sizeof(Int) * CHAR_BIT, align::no> >( std::numeric_limits<Int>::min(), std::numeric_limits<Int>::max() );
    test_buffer_type< endian_buffer<order::big, Int, sizeof(Int) * CHAR_BIT, align::no> >( std::numeric_limits<Int>::min(), std::numeric_limits<Int>::max() );
    test_buffer_type< endian_buffer<order::big, Int, sizeof(Int) * CHAR_BIT, align::yes> >( std::numeric_limits<Int>::min(), std::numeric_limits<Int>::max() );
    test_buffer_type< endian_buffer<order::big, Int, sizeof(Int) * CHAR_BIT, align::yes> >( std::numeric_limits<Int>::min(), std::numeric_limits<Int>::max() );
  }

  void test_boundary_values()
  {
    std::cout << "test boundary values..." << std::endl;

    test_boundary_values_<signed int>();
    test_boundary_values_<unsigned int>();
    test_boundary_values_<signed short>();
    test_boundary_values_<unsigned short>();
    test_boundary_values_<signed char>();
    test_boundary_values_<unsigned char>();

    std::cout << "test boundary values complete" << std::endl;
  }

}  // unnamed namespace

//--------------------------------------------------------------------------------------//

int cpp_main(int, char *[])
{
  cout << "byte swap intrinsics: " BOOST_ENDIAN_INTRINSIC_MSG << endl;

  cout << "  construct big endian aligned" << endl;
  big_int32_buf_at x(1122334455);

  cout << "  assign to buffer from built-in integer" << endl;
  x = 1234567890;

  cout << "  operator==(buffer.value(), built-in)" << endl;
  bool b1(x.value() == 1234567890);
  BOOST_TEST(b1);

  cout << "  construct little endian unaligned" << endl;
  little_int32_buf_t x2(1122334455);

  cout << "  assign to buffer from built-in integer" << endl;
  x2 = 1234567890;

  cout << "  operator==(buffer.value(), built-in)" << endl;
  bool b2(x2.value() == 1234567890);
  BOOST_TEST(b2);

  check_size();
  test_inserter_and_extractor();
  test_construction_and_assignment();
  test_boundary_values();

  cout << "  done" << endl;

  return ::boost::report_errors();
}

#include <boost/endian/detail/disable_warnings_pop.hpp>
