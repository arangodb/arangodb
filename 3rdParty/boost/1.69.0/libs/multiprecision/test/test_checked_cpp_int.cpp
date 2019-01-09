///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_

//
// Compare arithmetic results using fixed_int to GMP results.
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
void test_signed_overflow(Number a, Number b, const boost::mpl::true_&)
{
   a = -a;
   BOOST_CHECK_THROW(Number(a * b), std::overflow_error);
   ++a;
   BOOST_CHECK(Number(a * b) >= (std::numeric_limits<Number>::min)());
}
template <class Number>
void test_signed_overflow(Number a, Number b, const boost::mpl::false_&)
{
}

template <class Number>
void test()
{
   using namespace boost::multiprecision;
   typedef Number test_type;

   if(std::numeric_limits<test_type>::is_bounded)
   {
      test_type val = (std::numeric_limits<test_type>::max)();
#ifndef BOOST_NO_EXCEPTIONS
      BOOST_CHECK_THROW(++val, std::overflow_error);
      val = (std::numeric_limits<test_type>::max)();
      BOOST_CHECK_THROW(test_type(1 + val), std::overflow_error);
      BOOST_CHECK_THROW(test_type(val + 1), std::overflow_error);
      BOOST_CHECK_THROW(test_type(2 * val), std::overflow_error);
      val /= 2;
      val += 1;
      BOOST_CHECK_THROW(test_type(2 * val), std::overflow_error);

      if(std::numeric_limits<test_type>::is_signed)
      {
         val = (std::numeric_limits<test_type>::min)();
         BOOST_CHECK_THROW(--val, std::overflow_error);
         val = (std::numeric_limits<test_type>::min)();
         BOOST_CHECK_THROW(test_type(val - 1), std::overflow_error);
         BOOST_CHECK_THROW(test_type(2 * val), std::overflow_error);
         val /= 2;
         val -= 1;
         BOOST_CHECK_THROW(test_type(2 * val), std::overflow_error);
      }
      else
      {
         val = (std::numeric_limits<test_type>::min)();
         BOOST_CHECK_THROW(--val, std::range_error);
         val = (std::numeric_limits<test_type>::min)();
         BOOST_CHECK_THROW(test_type(val - 1), std::range_error);
      }
#endif
      //
      // Test overflow in random values:
      //
      for(unsigned bits = 30; bits < std::numeric_limits<test_type>::digits; bits += 30)
      {
         for(unsigned i = 0; i < 100; ++i)
         {
            val = static_cast<test_type>(generate_random<cpp_int>(bits));
            test_type val2 = 1 + (std::numeric_limits<test_type>::max)() / val;
            BOOST_CHECK_THROW(test_type(val2 * val), std::overflow_error);
            test_signed_overflow(val2, val, boost::mpl::bool_<std::numeric_limits<test_type>::is_signed>());
            --val2;
            BOOST_CHECK(cpp_int(val2) * cpp_int(val) <= cpp_int((std::numeric_limits<test_type>::max)()));
            BOOST_CHECK(val2 * val <= (std::numeric_limits<test_type>::max)());
            val2 = (std::numeric_limits<test_type>::max)() - val;
            ++val2;
            BOOST_CHECK_THROW(test_type(val2 + val), std::overflow_error);
            BOOST_CHECK((val2 - 1) + val == (std::numeric_limits<test_type>::max)());
            if(std::numeric_limits<test_type>::is_signed)
            {
               val2 = (std::numeric_limits<test_type>::min)() + val;
               --val2;
               BOOST_CHECK_THROW(test_type(val2 - val), std::overflow_error);
               ++val2;
               BOOST_CHECK(val2 - val == (std::numeric_limits<test_type>::min)());
            }
            unsigned shift = std::numeric_limits<test_type>::digits - msb(val);
            BOOST_CHECK_THROW((val << shift) > 0, std::overflow_error);
         }
      }
   }

#ifndef BOOST_NO_EXCEPTIONS
   if(std::numeric_limits<test_type>::is_signed)
   {
      test_type a = -1;
      test_type b = 1;
      BOOST_CHECK_THROW(test_type(a | b), std::range_error);
      BOOST_CHECK_THROW(test_type(a & b), std::range_error);
      BOOST_CHECK_THROW(test_type(a ^ b), std::range_error);
   }
   else
   {
      // Constructing from a negative value is not allowed:
      BOOST_CHECK_THROW(test_type(-2), std::range_error);
      BOOST_CHECK_THROW(test_type("-2"), std::range_error);
   }
   if(std::numeric_limits<test_type>::digits < std::numeric_limits<long long>::digits)
   {
      long long llm = (std::numeric_limits<long long>::max)();
      test_type t;
      BOOST_CHECK_THROW(t = llm, std::range_error);
      BOOST_CHECK_THROW(t = static_cast<test_type>(llm), std::range_error);
      unsigned long long ullm = (std::numeric_limits<unsigned long long>::max)();
      BOOST_CHECK_THROW(t = ullm, std::range_error);
      BOOST_CHECK_THROW(t = static_cast<test_type>(ullm), std::range_error);

      static const checked_uint512_t big = (std::numeric_limits<checked_uint512_t>::max)();
      BOOST_CHECK_THROW(t = static_cast<test_type>(big), std::range_error);
   }
   //
   // String errors:
   //
   BOOST_CHECK_THROW(test_type("12A"), std::runtime_error);
   BOOST_CHECK_THROW(test_type("0658"), std::runtime_error);

   if(std::numeric_limits<test_type>::is_signed)
   {
      BOOST_CHECK_THROW(test_type(-2).str(0, std::ios_base::hex), std::runtime_error);
      BOOST_CHECK_THROW(test_type(-2).str(0, std::ios_base::oct), std::runtime_error);
   }
#endif
}

int main()
{
   using namespace boost::multiprecision;

   test<number<cpp_int_backend<0, 0, signed_magnitude, checked> > >();
   test<checked_int512_t>();
   test<checked_uint512_t>();
   test<number<cpp_int_backend<32, 32, signed_magnitude, checked, void> > >();
   test<number<cpp_int_backend<32, 32, unsigned_magnitude, checked, void> > >();

   //
   // We also need to test type with "odd" bit counts in order to ensure full code coverage:
   //
   test<number<cpp_int_backend<528, 528, signed_magnitude, checked, void> > >();
   test<number<cpp_int_backend<528, 528, unsigned_magnitude, checked, void> > >();
   test<number<cpp_int_backend<48, 48, signed_magnitude, checked, void> > >();
   test<number<cpp_int_backend<48, 48, unsigned_magnitude, checked, void> > >();
   return boost::report_errors();
}



