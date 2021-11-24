//  (C) Copyright John Maddock 2019.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "constexpr_arithmetric_test.hpp"
#include "boost/multiprecision/cpp_int.hpp"
#include "test.hpp"

template <class F, class V>
decltype(std::declval<F>()(std::declval<V>())) non_constexpr_invoke(F f, V v)
{
   return f(v);
}

int main()
{
   typedef boost::multiprecision::checked_int128_t  int_backend;
   typedef boost::multiprecision::checked_uint128_t unsigned_backend;

   {
      constexpr int_backend a(22);
      constexpr unsigned_backend c(22);
      constexpr int_backend b      = test_constexpr_add_subtract(a);
      constexpr unsigned_backend d = test_constexpr_add_subtract(c);

      constexpr long long llv = (long long)b;

      static_assert(b == -108);
      static_assert(d == 554);
      static_assert(llv == -108);

      BOOST_CHECK_EQUAL(b, non_constexpr_invoke(test_constexpr_add_subtract<int_backend>, a));
      BOOST_CHECK_EQUAL(d, non_constexpr_invoke(test_constexpr_add_subtract<unsigned_backend>, c));
   }
   {
      constexpr int_backend a(22);
      constexpr unsigned_backend c(22);
      constexpr int_backend b      = test_constexpr_mul_divide(a);
      constexpr unsigned_backend d = test_constexpr_mul_divide(c);
      static_assert(b == 22);
      static_assert(d == 22);

      BOOST_CHECK_EQUAL(b, non_constexpr_invoke(test_constexpr_mul_divide<int_backend>, a));
      BOOST_CHECK_EQUAL(d, non_constexpr_invoke(test_constexpr_mul_divide<unsigned_backend>, c));
   }
   {
      constexpr int_backend a(22);
      constexpr unsigned_backend c(22);
      constexpr int_backend b      = test_constexpr_bitwise(a);
      constexpr unsigned_backend d = test_constexpr_bitwise(c);
#ifdef BOOST_HAS_INT128
      static_assert(b == 230);
      static_assert(d == 120);
#else
      static_assert(b == 210);
      static_assert(d == 106);
#endif

      BOOST_CHECK_EQUAL(b, non_constexpr_invoke(test_constexpr_bitwise<int_backend>, a));
      BOOST_CHECK_EQUAL(d, non_constexpr_invoke(test_constexpr_bitwise<unsigned_backend>, c));
   }
   {
      constexpr int_backend a(22);
      constexpr unsigned_backend c(22);
      constexpr int_backend b = test_constexpr_logical(a);
      constexpr unsigned_backend d = test_constexpr_logical(c);
#ifdef BOOST_HAS_INT128
      //static_assert(b == 95);
      //static_assert(d == 95);
#else
      static_assert(b == 82);
      static_assert(d == 82);
#endif
      BOOST_CHECK_EQUAL(b, non_constexpr_invoke(test_constexpr_logical<int_backend>, a));
      BOOST_CHECK_EQUAL(d, non_constexpr_invoke(test_constexpr_logical<unsigned_backend>, c));
   }
   {
      constexpr int_backend a(22);
      constexpr unsigned_backend c(22);
      constexpr int_backend b      = test_constexpr_compare(a);
      constexpr unsigned_backend d = test_constexpr_compare(c);
#ifdef BOOST_HAS_INT128
      static_assert(b == 95);
      static_assert(d == 95);
#else
      static_assert(b == 95);
      static_assert(d == 95);
#endif
      BOOST_CHECK_EQUAL(b, non_constexpr_invoke(test_constexpr_compare<int_backend>, a));
      BOOST_CHECK_EQUAL(d, non_constexpr_invoke(test_constexpr_compare<unsigned_backend>, c));
   }
   return boost::report_errors();
}
