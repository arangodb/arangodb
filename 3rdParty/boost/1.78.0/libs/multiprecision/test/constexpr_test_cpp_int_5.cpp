//  (C) Copyright John Maddock 2019.
//  Use, modification and distribution are subject to the
//  Boost Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Contains Quickbook markup, using in Boost.Multiprecision.qbk section on Literals and constexpr, penultimate section on factorials.

#include "constexpr_arithmetric_test.hpp"
#include "boost/multiprecision/cpp_int.hpp"
#include "boost/multiprecision/integer.hpp"
#include "test.hpp"

template <class F, class V>
decltype(std::declval<F>()(std::declval<V>())) non_constexpr_invoke(F f, V v)
{
   return f(v);
}

//[factorial_decl
template <class T>
constexpr T factorial(const T& a)
{
   return a ? a * factorial(a - 1) : 1;
}
//]

template <class T, class U>
constexpr T big_mul(const U& a, const U& b)
{
   using boost::multiprecision::multiply;
   T result = T();
   multiply(result, a, b);
   return result;
}
template <class T, class U>
constexpr T big_add(const U& a, const U& b)
{
   using boost::multiprecision::add;
   T result = T();
   add(result, a, b);
   return result;
}
template <class T, class U>
constexpr T big_sub(const U& a, const U& b)
{
   using boost::multiprecision::subtract;
   T result = T();
   subtract(result, a, b);
   return result;
}
template <class U>
constexpr U div_qr_d(const U& a, const U& b)
{
   using boost::multiprecision::divide_qr;
   U result = U();
   U r      = U();
   divide_qr(a, b, result, r);
   return result;
}
template <class U>
constexpr U div_qr_r(const U& a, const U& b)
{
   using boost::multiprecision::divide_qr;
   U result = U();
   U r      = U();
   divide_qr(a, b, result, r);
   return r;
}
template <class T>
constexpr T do_bit_set(T val, unsigned pos)
{
   using boost::multiprecision::bit_set;
   bit_set(val, pos);
   return val;
}
template <class T>
constexpr T do_bit_unset(T val, unsigned pos)
{
   using boost::multiprecision::bit_unset;
   bit_unset(val, pos);
   return val;
}
template <class T>
constexpr T do_bit_flip(T val, unsigned pos)
{
   using boost::multiprecision::bit_flip;
   bit_flip(val, pos);
   return val;
}
template <class T>
constexpr T test_swap(T a, T b)
{
   swap(a, b);
   a.swap(b);
   return a;
}

int main()
{
   using namespace boost::multiprecision::literals;

   typedef boost::multiprecision::checked_int1024_t  int_backend;
   typedef boost::multiprecision::checked_int512_t   small_int_backend;
   typedef boost::multiprecision::checked_uint1024_t unsigned_backend;

   constexpr int_backend f1 = factorial(int_backend(31));
   static_assert(f1 == 0x1956ad0aae33a4560c5cd2c000000_cppi);
   constexpr unsigned_backend f2 = factorial(unsigned_backend(31));
   static_assert(f2 == 0x1956ad0aae33a4560c5cd2c000000_cppui);

   //
   // Test integer non-member functions:
   //
   constexpr small_int_backend si1 = (std::numeric_limits<small_int_backend>::max)();
   constexpr small_int_backend si2 = 239876;
   constexpr std::int32_t i        = (std::numeric_limits<int>::max)();
   constexpr std::int32_t j        = 239876;
   // Multiply:
   {
      constexpr int_backend i1 = big_mul<int_backend>(si1, si2);
      int_backend           nc;
      multiply(nc, si1, si2);
      BOOST_CHECK_EQUAL(nc, i1);

      constexpr std::int64_t k = big_mul<std::int64_t>(i, j);
      std::int64_t           ii;
      boost::multiprecision::multiply(ii, i, j);
      BOOST_CHECK_EQUAL(ii, k);
   }
   // Add:
   {
      constexpr int_backend i1 = big_add<int_backend>(si1, si2);
      int_backend           nc;
      add(nc, si1, si2);
      BOOST_CHECK_EQUAL(nc, i1);

      constexpr std::int64_t k = big_add<std::int64_t>(i, j);
      std::int64_t           ii;
      boost::multiprecision::add(ii, i, j);
      BOOST_CHECK_EQUAL(ii, k);
   }
   // Subtract:
   {
      constexpr int_backend i1 = big_sub<int_backend>(si1, -si2);
      int_backend           nc;
      subtract(nc, si1, -si2);
      BOOST_CHECK_EQUAL(nc, i1);

      constexpr std::int64_t k = big_sub<std::int64_t>(i, -j);
      std::int64_t           ii;
      boost::multiprecision::subtract(ii, i, -j);
      BOOST_CHECK_EQUAL(ii, k);
   }
   // divide_qr:
   {
      constexpr small_int_backend i1 = div_qr_d(si1, si2);
      small_int_backend           nc, nc2;
      divide_qr(si1, si2, nc, nc2);
      BOOST_CHECK_EQUAL(nc, i1);

      constexpr std::int64_t k = div_qr_d(i, j);
      std::int32_t           ii, ij;
      boost::multiprecision::divide_qr(i, j, ii, ij);
      BOOST_CHECK_EQUAL(ii, k);
   }
   // divide_qr:
   {
      constexpr small_int_backend i1 = div_qr_r(si1, si2);
      small_int_backend           nc, nc2;
      divide_qr(si1, si2, nc, nc2);
      BOOST_CHECK_EQUAL(nc2, i1);

      constexpr std::int64_t k = div_qr_r(i, j);
      std::int32_t           ii, ij;
      boost::multiprecision::divide_qr(i, j, ii, ij);
      BOOST_CHECK_EQUAL(ij, k);
   }
   // integer_modulus:
   {
      constexpr int     i1 = integer_modulus(si1, 67);
      small_int_backend nc(si1);
      int               r = integer_modulus(nc, 67);
      BOOST_CHECK_EQUAL(r, i1);

      constexpr std::int32_t k = boost::multiprecision::integer_modulus(i, j);
      std::int32_t           ii(i);
      r = boost::multiprecision::integer_modulus(ii, j);
      BOOST_CHECK_EQUAL(r, k);
   }
   // powm:
   {
      constexpr small_int_backend i1 = powm(si1, si2, si2);
      small_int_backend           nc(si1);
      nc = powm(nc, si2, si2);
      BOOST_CHECK_EQUAL(nc, i1);

      constexpr std::int32_t k = boost::multiprecision::powm(i, j, j);
      std::int32_t           ii(i);
      ii = boost::multiprecision::powm(ii, j, j);
      BOOST_CHECK_EQUAL(ii, k);
   }
   // lsb:
   {
      constexpr int     i1 = lsb(si1);
      small_int_backend nc(si1);
      int               nci = lsb(nc);
      BOOST_CHECK_EQUAL(nci, i1);

      constexpr std::int32_t k = boost::multiprecision::lsb(i);
      std::int32_t           ii(i);
      ii = boost::multiprecision::lsb(ii);
      BOOST_CHECK_EQUAL(ii, k);
   }
   // msb:
   {
      constexpr int     i1 = msb(si1);
      small_int_backend nc(si1);
      int               nci = msb(nc);
      BOOST_CHECK_EQUAL(nci, i1);

      constexpr std::int32_t k = boost::multiprecision::msb(i);
      std::int32_t           ii(i);
      ii = boost::multiprecision::msb(ii);
      BOOST_CHECK_EQUAL(ii, k);
   }
   // bit_test:
   {
      constexpr bool b = bit_test(si1, 1);
      static_assert(b);

      constexpr bool k = boost::multiprecision::bit_test(i, 1);
      static_assert(k);
   }
   // bit_set:
   {
      constexpr int_backend i(0);
      constexpr int_backend j = do_bit_set(i, 20);
      static_assert(bit_test(j, 20));

      constexpr int ii(0);
      constexpr int jj = do_bit_set(ii, 20);
      static_assert(boost::multiprecision::bit_test(jj, 20));
   }
   // bit_unset:
   {
      constexpr int_backend r = do_bit_unset(si1, 20);
      static_assert(bit_test(r, 20) == false);

      constexpr int jj = do_bit_unset(i, 20);
      static_assert(boost::multiprecision::bit_test(jj, 20) == false);
   }
   // bit_unset:
   {
      constexpr int_backend r = do_bit_flip(si1, 20);
      static_assert(bit_test(r, 20) == false);

      constexpr int jj = do_bit_flip(i, 20);
      static_assert(boost::multiprecision::bit_test(jj, 20) == false);
   }
   // sqrt:
   {
      constexpr int_backend r = sqrt(si1);
      small_int_backend     nc(si1);
      nc = sqrt(nc);
      BOOST_CHECK_EQUAL(nc, r);
      constexpr int_backend r2 = sqrt(si1 * 1);
      BOOST_CHECK_EQUAL(nc, r);

      constexpr int jj = boost::multiprecision::sqrt(i);
      int           k  = i;
      k                = boost::multiprecision::sqrt(k);
      BOOST_CHECK_EQUAL(jj, k);
   }
   {
      // swap:
      constexpr small_int_backend r = test_swap(si1, si2);
      static_assert(si1 == r);
   }
   {
      // gcd:
      constexpr int_backend i(si1), j(si1 / 3);
      constexpr int_backend k = gcd(i, j);

      int_backend ii(i), jj(j);
      BOOST_CHECK_EQUAL(k, gcd(ii, jj));

      constexpr unsigned_backend ui(i), uj(j);
      constexpr unsigned_backend uk = gcd(ui, uj);
      unsigned_backend           uii(ui), ujj(uj);
      BOOST_CHECK_EQUAL(uk, gcd(uii, ujj));

      constexpr int_backend l = lcm(i, j);
      BOOST_CHECK_EQUAL(l, lcm(ii, jj));
      constexpr unsigned_backend ul = lcm(ui, uj);
      BOOST_CHECK_EQUAL(ul, lcm(uii, ujj));
   }
   return boost::report_errors();
}
