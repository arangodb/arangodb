///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

//
// Compare results of truncated left shift to gmp, see:
// https://svn.boost.org/trac/boost/ticket/12790
//

#ifdef _MSC_VER
#  define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/gmp.hpp>
#include <boost/multiprecision/cpp_int.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>
#include <boost/timer.hpp>
#include "test.hpp"


#if !defined(TEST1) && !defined(TEST2) && !defined(TEST3)
#define TEST1
#define TEST2
#define TEST3
#endif

template <class T>
T generate_random(unsigned bits_wanted)
{
   static boost::random::mt19937 gen;
   typedef boost::random::mt19937::result_type random_type;

   T max_val;
   unsigned digits;
   if(std::numeric_limits<T>::is_bounded && (bits_wanted == (unsigned)std::numeric_limits<T>::digits))
   {
      max_val = (std::numeric_limits<T>::max)();
      digits = std::numeric_limits<T>::digits;
   }
   else
   {
      max_val = T(1) << bits_wanted;
      digits = bits_wanted;
   }

   unsigned bits_per_r_val = std::numeric_limits<random_type>::digits - 1;
   while((random_type(1) << bits_per_r_val) > (gen.max)()) --bits_per_r_val;

   unsigned terms_needed = digits / bits_per_r_val + 1;

   T val = 0;
   for(unsigned i = 0; i < terms_needed; ++i)
   {
      val *= (gen.max)();
      val += gen();
   }
   val %= max_val;
   return val;
}

template <class T>
void test_value(const T& val)
{
   boost::multiprecision::mpz_int z(val.str()), mask(1);
   mask <<= std::numeric_limits<T>::digits;
   --mask;

   for(unsigned i = 0; i <= std::numeric_limits<T>::digits + 2; ++i)
   {
      BOOST_CHECK_EQUAL((val << i).str(), boost::multiprecision::mpz_int(((z << i) & mask)).str());
   }
}

void test(const boost::mpl::int_<200>&) {}

template <int N>
void test(boost::mpl::int_<N> const&)
{
   test(boost::mpl::int_<N + 4>());

   typedef boost::multiprecision::number < boost::multiprecision::cpp_int_backend<N, N, boost::multiprecision::unsigned_magnitude>, boost::multiprecision::et_off> mp_type;

   std::cout << "Running tests for precision: " << N << std::endl;

   mp_type mp(-1);
   test_value(mp);

   for(unsigned i = 0; i < 1000; ++i)
      test_value(generate_random<mp_type>(std::numeric_limits<mp_type>::digits));
}


int main()
{
   test(boost::mpl::int_<24>());
   return boost::report_errors();
}



