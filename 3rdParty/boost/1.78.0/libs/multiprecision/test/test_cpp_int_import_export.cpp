// Copyright John Maddock 2015.

// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef _MSC_VER
#define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/cpp_int.hpp>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include "test.hpp"
#include <iostream>
#include <iomanip>

#ifdef BOOST_MSVC
#pragma warning(disable : 4127)
#endif
template <class T>
struct unchecked_type
{
   typedef T type;
};

template <unsigned MinBits, unsigned MaxBits, boost::multiprecision::cpp_integer_type SignType, boost::multiprecision::cpp_int_check_type Checked, class Allocator, boost::multiprecision::expression_template_option ExpressionTemplates>
struct unchecked_type<boost::multiprecision::number<boost::multiprecision::cpp_int_backend<MinBits, MaxBits, SignType, Checked, Allocator>, ExpressionTemplates> >
{
   typedef boost::multiprecision::number<boost::multiprecision::cpp_int_backend<MinBits, MaxBits, SignType, boost::multiprecision::unchecked, Allocator>, ExpressionTemplates> type;
};

template <class T>
T generate_random()
{
   typedef typename unchecked_type<T>::type unchecked_T;

   static const unsigned limbs = std::numeric_limits<T>::is_specialized && std::numeric_limits<T>::is_bounded ? std::numeric_limits<T>::digits / std::numeric_limits<unsigned>::digits + 3 : 20;

   static boost::random::uniform_int_distribution<unsigned> ui(0, limbs);
   static boost::random::mt19937                            gen;
   unchecked_T                                              val = gen();
   unsigned                                                 lim = ui(gen);
   for (unsigned i = 0; i < lim; ++i)
   {
      val *= (gen.max)();
      val += gen();
   }
   return val;
}

template <class T>
void test_round_trip_neg(T val, const std::integral_constant<bool, true>&)
{
   // Try some negative values:
   std::vector<unsigned char> cv;
   T                          newval;
   val = -val;
   export_bits(val, std::back_inserter(cv), 8, false);
   import_bits(newval, cv.begin(), cv.end(), 8, false);
   BOOST_CHECK_EQUAL(-val, newval);
}

template <class T>
void test_round_trip_neg(const T&, const std::integral_constant<bool, false>&)
{
}

template <class T>
void test_round_trip(T val)
{
   std::vector<unsigned char> cv;
   export_bits(val, std::back_inserter(cv), 8);
   T newval;
   import_bits(newval, cv.begin(), cv.end());
   BOOST_CHECK_EQUAL(val, newval);
   // Should get the same value if we reverse the bytes:
   std::reverse(cv.begin(), cv.end());
   newval = 0;
   import_bits(newval, cv.begin(), cv.end(), 8, false);
   BOOST_CHECK_EQUAL(val, newval);
   // Also try importing via pointers as these may memcpy:
   newval = 0;
   import_bits(newval, &cv[0], &cv[0] + cv.size(), 8, false);
   BOOST_CHECK_EQUAL(val, newval);

   cv.clear();
   export_bits(val, std::back_inserter(cv), 8, false);
   import_bits(newval, cv.begin(), cv.end(), 8, false);
   BOOST_CHECK_EQUAL(val, newval);
   std::reverse(cv.begin(), cv.end());
   newval = 0;
   import_bits(newval, cv.begin(), cv.end(), 8, true);
   BOOST_CHECK_EQUAL(val, newval);

   std::vector<std::uintmax_t> bv;
   export_bits(val, std::back_inserter(bv), std::numeric_limits<std::uintmax_t>::digits);
   import_bits(newval, bv.begin(), bv.end());
   BOOST_CHECK_EQUAL(val, newval);
   // Should get the same value if we reverse the values:
   std::reverse(bv.begin(), bv.end());
   newval = 0;
   import_bits(newval, bv.begin(), bv.end(), std::numeric_limits<std::uintmax_t>::digits, false);
   BOOST_CHECK_EQUAL(val, newval);
   // Also try importing via pointers as these may memcpy:
   newval = 0;
   import_bits(newval, &bv[0], &bv[0] + bv.size(), std::numeric_limits<std::uintmax_t>::digits, false);
   BOOST_CHECK_EQUAL(val, newval);

   bv.clear();
   export_bits(val, std::back_inserter(bv), std::numeric_limits<std::uintmax_t>::digits, false);
   import_bits(newval, bv.begin(), bv.end(), std::numeric_limits<std::uintmax_t>::digits, false);
   BOOST_CHECK_EQUAL(val, newval);
   //
   // Try with an unconventional number of bits, to model some machine with guard bits:
   //
   bv.clear();
   export_bits(val, std::back_inserter(bv), std::numeric_limits<std::uintmax_t>::digits - 3);
   import_bits(newval, bv.begin(), bv.end(), std::numeric_limits<std::uintmax_t>::digits - 3);
   BOOST_CHECK_EQUAL(val, newval);

   bv.clear();
   export_bits(val, std::back_inserter(bv), std::numeric_limits<std::uintmax_t>::digits - 3, false);
   import_bits(newval, bv.begin(), bv.end(), std::numeric_limits<std::uintmax_t>::digits - 3, false);
   BOOST_CHECK_EQUAL(val, newval);

   cv.clear();
   export_bits(val, std::back_inserter(cv), 6);
   import_bits(newval, cv.begin(), cv.end(), 6);
   BOOST_CHECK_EQUAL(val, newval);

   cv.clear();
   export_bits(val, std::back_inserter(cv), 6, false);
   import_bits(newval, cv.begin(), cv.end(), 6, false);
   BOOST_CHECK_EQUAL(val, newval);

   test_round_trip_neg(val, std::integral_constant<bool, std::numeric_limits<T>::is_signed>());
}

template <class T>
void test_round_trip()
{
   std::cout << std::hex;
   std::cerr << std::hex;
   for (unsigned i = 0; i < 1000; ++i)
   {
      T val = generate_random<T>();
      test_round_trip(val);
   }
   //
   // Bug cases.
   // See https://github.com/boostorg/multiprecision/issues/21
   T bug(1);
   bug << std::numeric_limits<T>::digits - 1;
   --bug;
   test_round_trip(bug);
}

int main()
{
   test_round_trip<boost::multiprecision::cpp_int>();
   test_round_trip<boost::multiprecision::checked_int1024_t>();
   test_round_trip<boost::multiprecision::checked_uint512_t>();
   test_round_trip<boost::multiprecision::number<boost::multiprecision::cpp_int_backend<64, 64, boost::multiprecision::unsigned_magnitude, boost::multiprecision::checked, void> > >();
   test_round_trip<boost::multiprecision::number<boost::multiprecision::cpp_int_backend<23, 23, boost::multiprecision::unsigned_magnitude, boost::multiprecision::checked, void> > >();
   return boost::report_errors();
}
