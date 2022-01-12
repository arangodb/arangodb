//  (C) Copyright John Maddock 2019.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "constexpr_arithmetric_test.hpp"
#include "../performance/arithmetic_backend.hpp"

int main()
{
   typedef boost::multiprecision::number<boost::multiprecision::backends::arithmetic_backend<long long>, boost::multiprecision::et_off>          int_backend;
   typedef boost::multiprecision::number<boost::multiprecision::backends::arithmetic_backend<unsigned long long>, boost::multiprecision::et_off> unsigned_backend;

   typedef boost::multiprecision::number<boost::multiprecision::backends::arithmetic_backend<long long>, boost::multiprecision::et_on>          int_backend_et;
   typedef boost::multiprecision::number<boost::multiprecision::backends::arithmetic_backend<unsigned long long>, boost::multiprecision::et_on> unsigned_backend_et;

   {
      constexpr int_backend a(22);
      constexpr unsigned_backend c(22);
      constexpr int_backend b      = test_constexpr_add_subtract(a);
      constexpr unsigned_backend d = test_constexpr_add_subtract(c);

      constexpr long long llv = (long long)b;

      static_assert(b == -108);
      static_assert(llv == -108);
      static_assert(d == 554);
   }
   {
      constexpr int_backend a(22);
      constexpr unsigned_backend c(22);
      constexpr int_backend b      = test_constexpr_mul_divide(a);
      constexpr unsigned_backend d = test_constexpr_mul_divide(c);
      static_assert(b == 22);
      static_assert(d == 22);
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
   }
   {
      constexpr int_backend a(22);
      constexpr unsigned_backend c(22);
      constexpr int_backend b      = test_constexpr_logical(a);
      constexpr unsigned_backend d = test_constexpr_logical(c);
      static_assert(b == 82);
      static_assert(d == 82);
   }
   {
      constexpr int_backend a(22);
      constexpr unsigned_backend c(22);
      constexpr int_backend b      = test_constexpr_compare(a);
      constexpr unsigned_backend d = test_constexpr_compare(c);
      static_assert(b == 95);
      static_assert(d == 95);
   }
   //
   // Over again with expression templates turned on:
   //
   {
      constexpr int_backend_et a(22);
      constexpr unsigned_backend_et c(22);
      constexpr int_backend_et b      = test_constexpr_add_subtract(a);
      constexpr unsigned_backend_et d = test_constexpr_add_subtract(c);

      static_assert(b == -108);
      static_assert(d == 554);
   }
   {
      constexpr int_backend_et a(22);
      constexpr unsigned_backend_et c(22);
      constexpr int_backend_et b      = test_constexpr_mul_divide(a);
      constexpr unsigned_backend_et d = test_constexpr_mul_divide(c);
      static_assert(b == 22);
      static_assert(d == 22);
   }
   {
      constexpr int_backend_et a(22);
      constexpr unsigned_backend_et c(22);
      constexpr int_backend_et b      = test_constexpr_bitwise(a);
      constexpr unsigned_backend_et d = test_constexpr_bitwise(c);
#ifdef BOOST_HAS_INT128
      static_assert(b == 230);
      static_assert(d == 120);
#else
      static_assert(b == 210);
      static_assert(d == 106);
#endif
   }
   {
      constexpr int_backend_et a(22);
      constexpr unsigned_backend_et c(22);
      constexpr int_backend_et b      = test_constexpr_logical(a);
      constexpr unsigned_backend_et d = test_constexpr_logical(c);
      static_assert(b == 82);
      static_assert(d == 82);
   }
   {
      constexpr int_backend_et a(22);
      constexpr unsigned_backend_et c(22);
      constexpr int_backend_et b      = test_constexpr_compare(a);
      constexpr unsigned_backend_et d = test_constexpr_compare(c);
      static_assert(b == 95);
      static_assert(d == 95);
   }
   std::cout << "Done!" << std::endl;
}
