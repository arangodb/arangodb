//  deprecated_test.cpp  ---------------------------------------------------------------//

//  Copyright Beman Dawes 2014, 2015

//  Distributed under the Boost Software License, Version 1.0.
//  See http://www.boost.org/LICENSE_1_0.txt

//  See library home page at http://www.boost.org/libs/endian

//--------------------------------------------------------------------------------------//

#include <boost/endian/detail/disable_warnings.hpp>

#define BOOST_ENDIAN_DEPRECATED_NAMES
#include <boost/endian/endian.hpp>
#include <boost/detail/lightweight_main.hpp>
#include <boost/core/lightweight_test.hpp>
#include <iostream>
#include <sstream>

using namespace boost::endian;
using std::cout;
using std::endl;

namespace
{

  //  check_size  ----------------------------------------------------------------------//

  void check_size()
  {
    BOOST_TEST_EQ(sizeof(big8_t), 1);
    BOOST_TEST_EQ(sizeof(big16_t), 2);
    BOOST_TEST_EQ(sizeof(big24_t), 3);
    BOOST_TEST_EQ(sizeof(big32_t), 4);
    BOOST_TEST_EQ(sizeof(big40_t), 5);
    BOOST_TEST_EQ(sizeof(big48_t), 6);
    BOOST_TEST_EQ(sizeof(big56_t), 7);
    BOOST_TEST_EQ(sizeof(big64_t), 8);

    BOOST_TEST_EQ(sizeof(ubig8_t), 1);
    BOOST_TEST_EQ(sizeof(ubig16_t), 2);
    BOOST_TEST_EQ(sizeof(ubig24_t), 3);
    BOOST_TEST_EQ(sizeof(ubig32_t), 4);
    BOOST_TEST_EQ(sizeof(ubig40_t), 5);
    BOOST_TEST_EQ(sizeof(ubig48_t), 6);
    BOOST_TEST_EQ(sizeof(ubig56_t), 7);
    BOOST_TEST_EQ(sizeof(ubig64_t), 8);

    BOOST_TEST_EQ(sizeof(little8_t), 1);
    BOOST_TEST_EQ(sizeof(little16_t), 2);
    BOOST_TEST_EQ(sizeof(little24_t), 3);
    BOOST_TEST_EQ(sizeof(little32_t), 4);
    BOOST_TEST_EQ(sizeof(little40_t), 5);
    BOOST_TEST_EQ(sizeof(little48_t), 6);
    BOOST_TEST_EQ(sizeof(little56_t), 7);
    BOOST_TEST_EQ(sizeof(little64_t), 8);

    BOOST_TEST_EQ(sizeof(ulittle8_t), 1);
    BOOST_TEST_EQ(sizeof(ulittle16_t), 2);
    BOOST_TEST_EQ(sizeof(ulittle24_t), 3);
    BOOST_TEST_EQ(sizeof(ulittle32_t), 4);
    BOOST_TEST_EQ(sizeof(ulittle40_t), 5);
    BOOST_TEST_EQ(sizeof(ulittle48_t), 6);
    BOOST_TEST_EQ(sizeof(ulittle56_t), 7);
    BOOST_TEST_EQ(sizeof(ulittle64_t), 8);

    BOOST_TEST_EQ(sizeof(native8_t), 1);
    BOOST_TEST_EQ(sizeof(native16_t), 2);
    BOOST_TEST_EQ(sizeof(native24_t), 3);
    BOOST_TEST_EQ(sizeof(native32_t), 4);
    BOOST_TEST_EQ(sizeof(native40_t), 5);
    BOOST_TEST_EQ(sizeof(native48_t), 6);
    BOOST_TEST_EQ(sizeof(native56_t), 7);
    BOOST_TEST_EQ(sizeof(native64_t), 8);

    BOOST_TEST_EQ(sizeof(unative8_t), 1);
    BOOST_TEST_EQ(sizeof(unative16_t), 2);
    BOOST_TEST_EQ(sizeof(unative24_t), 3);
    BOOST_TEST_EQ(sizeof(unative32_t), 4);
    BOOST_TEST_EQ(sizeof(unative40_t), 5);
    BOOST_TEST_EQ(sizeof(unative48_t), 6);
    BOOST_TEST_EQ(sizeof(unative56_t), 7);
    BOOST_TEST_EQ(sizeof(unative64_t), 8);

    BOOST_TEST_EQ(sizeof(aligned_big16_t), 2);
    BOOST_TEST_EQ(sizeof(aligned_big32_t), 4);
    BOOST_TEST_EQ(sizeof(aligned_big64_t), 8);

    BOOST_TEST_EQ(sizeof(aligned_ubig16_t), 2);
    BOOST_TEST_EQ(sizeof(aligned_ubig32_t), 4);
    BOOST_TEST_EQ(sizeof(aligned_ubig64_t), 8);

    BOOST_TEST_EQ(sizeof(aligned_little16_t), 2);
    BOOST_TEST_EQ(sizeof(aligned_little32_t), 4);
    BOOST_TEST_EQ(sizeof(aligned_little64_t), 8);

    BOOST_TEST_EQ(sizeof(aligned_ulittle16_t), 2);
    BOOST_TEST_EQ(sizeof(aligned_ulittle32_t), 4);
    BOOST_TEST_EQ(sizeof(aligned_ulittle64_t), 8);

# ifndef BOOST_NO_CXX11_TEMPLATE_ALIASES
    BOOST_TEST_EQ(sizeof(endian<endianness::big, int_least16_t, 16>), 2);
    BOOST_TEST_EQ(sizeof(endian<endianness::big,
      int_least16_t, 16, alignment::unaligned>), 2);
# endif
  } // check_size

  //  test_inserter_and_extractor  -----------------------------------------------------//

  void test_inserter_and_extractor()
  {
    std::cout << "test inserter and extractor..." << std::endl;

    ubig64_t bu64(0x010203040506070ULL);
    ulittle64_t lu64(0x010203040506070ULL);

    uint64_t x;

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
    ubig64_t bu64z(0);
    ss >> bu64z;
    BOOST_TEST_EQ(bu64z, bu64);

    ss.clear();
    ss << 0x010203040506070ULL;
    ulittle64_t lu64z(0);
    ss >> lu64z;
    BOOST_TEST_EQ(lu64z, lu64);

    std::cout << "test inserter and extractor complete" << std::endl;

  }

}  // unnamed namespace

   //--------------------------------------------------------------------------------------//

int cpp_main(int, char *[])
{
  cout << "byte swap intrinsics: " BOOST_ENDIAN_INTRINSIC_MSG << endl;

  cout << "  construct big endian aligned" << endl;
  big32_t x(1122334455);

  cout << "  assign to buffer from built-in integer" << endl;
  x = 1234567890;

  cout << "  operator==(buffer.value(), built-in)" << endl;
  bool b1(x == 1234567890);
  BOOST_TEST(b1);

  cout << "  construct little endian unaligned" << endl;
  little32_t x2(1122334455);

  cout << "  assign to buffer from built-in integer" << endl;
  x2 = 1234567890;

  cout << "  operator==(buffer.value(), built-in)" << endl;
  bool b2(x2 == 1234567890);
  BOOST_TEST(b2);

  check_size();
  test_inserter_and_extractor();

  cout << "  done" << endl;

  return ::boost::report_errors();
}

#include <boost/endian/detail/disable_warnings_pop.hpp>
