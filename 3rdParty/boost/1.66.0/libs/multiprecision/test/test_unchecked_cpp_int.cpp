///////////////////////////////////////////////////////////////
//  Copyright 2017 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_

//
// Check results of truncated overflow.
//

#ifdef _MSC_VER
#  define _SCL_SECURE_NO_WARNINGS
#endif

#include <boost/multiprecision/cpp_int.hpp>
#include "test.hpp"
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_int.hpp>

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


template <class Number>
void test()
{
   using namespace boost::multiprecision;
   typedef Number test_type;


   for(unsigned i = 30; i < std::numeric_limits<test_type>::digits; ++i)
   {
      for(unsigned j = std::numeric_limits<test_type>::digits - i - 1; j < std::numeric_limits<test_type>::digits; ++j)
      {
         for(unsigned k = 0; k < 10; ++k)
         {
            test_type a = static_cast<test_type>(generate_random<cpp_int>(i));
            test_type b = static_cast<test_type>(generate_random<cpp_int>(j));
            test_type c = static_cast<test_type>(cpp_int(a) * cpp_int(b));
            test_type d = a * b;
            BOOST_CHECK_EQUAL(c, d);

            if((k == 0) && (j == 0))
            {
               for(unsigned s = 1; s < std::numeric_limits<test_type>::digits; ++s)
                  BOOST_CHECK_EQUAL(a << s, test_type(cpp_int(a) << s));
            }
         }
      }
   }
}

int main()
{
   using namespace boost::multiprecision;

   test<int512_t>();
   test<uint512_t>();

   //
   // We also need to test type with "odd" bit counts in order to ensure full code coverage:
   //
   test<number<cpp_int_backend<528, 528, signed_magnitude, unchecked, void> > >();
   test<number<cpp_int_backend<528, 528, unsigned_magnitude, unchecked, void> > >();
   test<number<cpp_int_backend<48, 48, signed_magnitude, unchecked, void> > >();
   test<number<cpp_int_backend<48, 48, unsigned_magnitude, unchecked, void> > >();
   return boost::report_errors();
}



