///////////////////////////////////////////////////////////////
//  Copyright 2012 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

#ifdef TEST_VLD
#include <vld.h>
#endif

#include <boost/math/special_functions/pow.hpp>
#include <boost/integer/common_factor_rt.hpp>
#include <boost/functional/hash.hpp>
#include <functional>
#include "test.hpp"

template <class T>
struct is_boost_rational : public std::integral_constant<bool, false>
{};
template <class T>
struct is_checked_cpp_int : public std::integral_constant<bool, false>
{};

#ifdef BOOST_MSVC
// warning C4127: conditional expression is constant
#pragma warning(disable : 4127)
#endif

template <class Target, class Source>
Target checked_lexical_cast(const Source& val)
{
#ifndef BOOST_NO_EXCEPTIONS
   try
   {
#endif
      return boost::lexical_cast<Target>(val);
#ifndef BOOST_NO_EXCEPTIONS
   }
   catch (...)
   {
      std::cerr << "Error in lexical cast\nSource type = " << typeid(Source).name() << " \"" << val << "\"\n";
      std::cerr << "Target type = " << typeid(Target).name() << std::endl;
      throw;
   }
#endif
}

bool isfloat(float) { return true; }
bool isfloat(double) { return true; }
bool isfloat(long double) { return true; }
template <class T>
bool isfloat(T) { return false; }

namespace detail {

template <class tag, class Arg1, class Arg2, class Arg3, class Arg4>
typename boost::multiprecision::detail::expression<tag, Arg1, Arg2, Arg3, Arg4>::result_type
abs(boost::multiprecision::detail::expression<tag, Arg1, Arg2, Arg3, Arg4> const& v)
{
   typedef typename boost::multiprecision::detail::expression<tag, Arg1, Arg2, Arg3, Arg4>::result_type result_type;
   return v < 0 ? result_type(-v) : result_type(v);
}

} // namespace detail

template <class T>
struct is_twos_complement_integer : public std::integral_constant<bool, true>
{};

template <class T>
struct related_type
{
   typedef T type;
};

template <class Real, class Val>
void test_comparisons(Val, Val, const std::integral_constant<bool, false>&)
{}

int normalize_compare_result(int r)
{
   return r > 0 ? 1 : r < 0 ? -1 : 0;
}

enum unscoped_enum
{
   one = 1,
   two = 2,
   three = 3,
};

enum struct scoped_enum
{
   four = 4,
   five = 5,
   six = 6,
};

template <class Real>
typename std::enable_if<boost::multiprecision::is_number<Real>::value>::type test_enum_conversions()
{
   Real r1(one);
   BOOST_CHECK_EQUAL(r1, 1);
   Real r2(scoped_enum::four);
   BOOST_CHECK_EQUAL(r2, 4);
   r1 = two;
   BOOST_CHECK_EQUAL(r1, 2);
   r1.assign(scoped_enum::five);
   BOOST_CHECK_EQUAL(r1, 5);

   r1.assign(two);

   BOOST_CHECK_EQUAL(static_cast<unscoped_enum>(r1), two);
   BOOST_CHECK(static_cast<scoped_enum>(r2) == scoped_enum::four);
}

template <class Real>
typename std::enable_if<!boost::multiprecision::is_number<Real>::value>::type test_enum_conversions() 
{}

template <class Real, class Val>
typename std::enable_if<boost::multiprecision::number_category<Real>::value != boost::multiprecision::number_kind_complex>::type
test_comparisons(Val a, Val b, const std::integral_constant<bool, true>&)
{
   Real r1(a);
   Real r2(b);
   Real z(1);

   int cr = a < b ? -1 : a > b ? 1 : 0;

   BOOST_CHECK_EQUAL(r1 == r2, a == b);
   BOOST_CHECK_EQUAL(r1 != r2, a != b);
   BOOST_CHECK_EQUAL(r1 <= r2, a <= b);
   BOOST_CHECK_EQUAL(r1 < r2, a < b);
   BOOST_CHECK_EQUAL(r1 >= r2, a >= b);
   BOOST_CHECK_EQUAL(r1 > r2, a > b);

   BOOST_CHECK_EQUAL(r1 == b, a == b);
   BOOST_CHECK_EQUAL(r1 != b, a != b);
   BOOST_CHECK_EQUAL(r1 <= b, a <= b);
   BOOST_CHECK_EQUAL(r1 < b, a < b);
   BOOST_CHECK_EQUAL(r1 >= b, a >= b);
   BOOST_CHECK_EQUAL(r1 > b, a > b);

   BOOST_CHECK_EQUAL(a == r2, a == b);
   BOOST_CHECK_EQUAL(a != r2, a != b);
   BOOST_CHECK_EQUAL(a <= r2, a <= b);
   BOOST_CHECK_EQUAL(a < r2, a < b);
   BOOST_CHECK_EQUAL(a >= r2, a >= b);
   BOOST_CHECK_EQUAL(a > r2, a > b);

   BOOST_CHECK_EQUAL(r1 * z == r2, a == b);
   BOOST_CHECK_EQUAL(r1 * z != r2, a != b);
   BOOST_CHECK_EQUAL(r1 * z <= r2, a <= b);
   BOOST_CHECK_EQUAL(r1 * z < r2, a < b);
   BOOST_CHECK_EQUAL(r1 * z >= r2, a >= b);
   BOOST_CHECK_EQUAL(r1 * z > r2, a > b);

   BOOST_CHECK_EQUAL(r1 == r2 * z, a == b);
   BOOST_CHECK_EQUAL(r1 != r2 * z, a != b);
   BOOST_CHECK_EQUAL(r1 <= r2 * z, a <= b);
   BOOST_CHECK_EQUAL(r1 < r2 * z, a < b);
   BOOST_CHECK_EQUAL(r1 >= r2 * z, a >= b);
   BOOST_CHECK_EQUAL(r1 > r2 * z, a > b);

   BOOST_CHECK_EQUAL(r1 * z == r2 * z, a == b);
   BOOST_CHECK_EQUAL(r1 * z != r2 * z, a != b);
   BOOST_CHECK_EQUAL(r1 * z <= r2 * z, a <= b);
   BOOST_CHECK_EQUAL(r1 * z < r2 * z, a < b);
   BOOST_CHECK_EQUAL(r1 * z >= r2 * z, a >= b);
   BOOST_CHECK_EQUAL(r1 * z > r2 * z, a > b);

   BOOST_CHECK_EQUAL(r1 * z == b, a == b);
   BOOST_CHECK_EQUAL(r1 * z != b, a != b);
   BOOST_CHECK_EQUAL(r1 * z <= b, a <= b);
   BOOST_CHECK_EQUAL(r1 * z < b, a < b);
   BOOST_CHECK_EQUAL(r1 * z >= b, a >= b);
   BOOST_CHECK_EQUAL(r1 * z > b, a > b);

   BOOST_CHECK_EQUAL(a == r2 * z, a == b);
   BOOST_CHECK_EQUAL(a != r2 * z, a != b);
   BOOST_CHECK_EQUAL(a <= r2 * z, a <= b);
   BOOST_CHECK_EQUAL(a < r2 * z, a < b);
   BOOST_CHECK_EQUAL(a >= r2 * z, a >= b);
   BOOST_CHECK_EQUAL(a > r2 * z, a > b);

   BOOST_CHECK_EQUAL(normalize_compare_result(r1.compare(r2)), cr);
   BOOST_CHECK_EQUAL(normalize_compare_result(r2.compare(r1)), -cr);
   BOOST_CHECK_EQUAL(normalize_compare_result(r1.compare(b)), cr);
   BOOST_CHECK_EQUAL(normalize_compare_result(r2.compare(a)), -cr);
}

template <class Real, class Val>
typename std::enable_if<boost::multiprecision::number_category<Real>::value == boost::multiprecision::number_kind_complex>::type
test_comparisons(Val a, Val b, const std::integral_constant<bool, true>&)
{
   Real r1(a);
   Real r2(b);
   Real z(1);

   int cr = a < b ? -1 : a > b ? 1 : 0;
   (void)cr;

   BOOST_CHECK_EQUAL(r1 == r2, a == b);
   BOOST_CHECK_EQUAL(r1 != r2, a != b);

   BOOST_CHECK_EQUAL(r1 == b, a == b);
   BOOST_CHECK_EQUAL(r1 != b, a != b);

   BOOST_CHECK_EQUAL(a == r2, a == b);
   BOOST_CHECK_EQUAL(a != r2, a != b);

   BOOST_CHECK_EQUAL(r1 * z == r2, a == b);
   BOOST_CHECK_EQUAL(r1 * z != r2, a != b);

   BOOST_CHECK_EQUAL(r1 == r2 * z, a == b);
   BOOST_CHECK_EQUAL(r1 != r2 * z, a != b);

   BOOST_CHECK_EQUAL(r1 * z == r2 * z, a == b);
   BOOST_CHECK_EQUAL(r1 * z != r2 * z, a != b);

   BOOST_CHECK_EQUAL(r1 * z == b, a == b);
   BOOST_CHECK_EQUAL(r1 * z != b, a != b);

   BOOST_CHECK_EQUAL(a == r2 * z, a == b);
   BOOST_CHECK_EQUAL(a != r2 * z, a != b);

   if (r1 == r2)
   {
      BOOST_CHECK_EQUAL(normalize_compare_result(r1.compare(r2)), 0);
      BOOST_CHECK_EQUAL(normalize_compare_result(r2.compare(r1)), 0);
      BOOST_CHECK_EQUAL(normalize_compare_result(r1.compare(b)), 0);
      BOOST_CHECK_EQUAL(normalize_compare_result(r2.compare(a)), 0);
   }
   else
   {
      BOOST_CHECK_NE(normalize_compare_result(r1.compare(r2)), 0);
      BOOST_CHECK_NE(normalize_compare_result(r2.compare(r1)), 0);
      BOOST_CHECK_NE(normalize_compare_result(r1.compare(b)), 0);
      BOOST_CHECK_NE(normalize_compare_result(r2.compare(a)), 0);
   }
}

template <class Real, class Exp>
void test_conditional(Real v, Exp e)
{
   //
   // Verify that Exp is usable in Boolean contexts, and has the same value as v:
   //
   if (e)
   {
      BOOST_CHECK(v);
   }
   else
   {
      BOOST_CHECK(!v);
   }
   if (!e)
   {
      BOOST_CHECK(!v);
   }
   else
   {
      BOOST_CHECK(v);
   }
}

template <class Real>
void test_complement(Real a, Real b, Real c, const std::integral_constant<bool, true>&)
{
   int i         = 1020304;
   int j         = 56789123;
   int sign_mask = ~0;
   if (std::numeric_limits<Real>::is_signed)
   {
      BOOST_CHECK_EQUAL(~a, (~i & sign_mask));
      c = a & ~b;
      BOOST_CHECK_EQUAL(c, (i & (~j & sign_mask)));
      c = ~(a | b);
      BOOST_CHECK_EQUAL(c, (~(i | j) & sign_mask));
   }
   else
   {
      BOOST_CHECK_EQUAL((~a & a), 0);
   }
}

template <class Real>
void test_complement(Real, Real, Real, const std::integral_constant<bool, false>&)
{
}

template <class Real, class T>
void test_integer_ops(const T&) {}

template <class Real>
void test_rational(const std::integral_constant<bool, true>&)
{
   Real a(2);
   a /= 3;
   BOOST_CHECK_EQUAL(numerator(a), 2);
   BOOST_CHECK_EQUAL(denominator(a), 3);
   Real b(4);
   b /= 6;
   BOOST_CHECK_EQUAL(a, b);

   //
   // Check IO code:
   //
   std::stringstream ss;
   ss << a;
   ss >> b;
   BOOST_CHECK_EQUAL(a, b);
}

template <class Real>
void test_rational(const std::integral_constant<bool, false>&)
{
   Real a(2);
   a /= 3;
   BOOST_CHECK_EQUAL(numerator(a), 2);
   BOOST_CHECK_EQUAL(denominator(a), 3);
   Real b(4);
   b /= 6;
   BOOST_CHECK_EQUAL(a, b);

#ifndef BOOST_NO_EXCEPTIONS
   BOOST_CHECK_THROW(Real(a / 0), std::overflow_error);
   BOOST_CHECK_THROW(Real("3.14"), std::runtime_error);
#endif
   b = Real("2/3");
   BOOST_CHECK_EQUAL(a, b);
   //
   // Check IO code:
   //
   std::stringstream ss;
   ss << a;
   ss >> b;
   BOOST_CHECK_EQUAL(a, b);
   //
   // Conversion to integer, see https://github.com/boostorg/multiprecision/issues/342.
   //
   Real three(10, 3);
   BOOST_CHECK_EQUAL(static_cast<std::uint16_t>(three), 3);
   BOOST_CHECK_EQUAL(static_cast<std::uint32_t>(three), 3);
   BOOST_CHECK_EQUAL(static_cast<std::uint64_t>(three), 3);
   three = -three;
   BOOST_CHECK_EQUAL(static_cast<std::int16_t>(three), -3);
   BOOST_CHECK_EQUAL(static_cast<std::int32_t>(three), -3);
   BOOST_CHECK_EQUAL(static_cast<std::int64_t>(three), -3);
}

template <class Real>
void test_integer_ops(const std::integral_constant<int, boost::multiprecision::number_kind_rational>&)
{
   test_rational<Real>(is_boost_rational<Real>());
}

template <class Real>
void test_signed_integer_ops(const std::integral_constant<bool, true>&)
{
   Real a(20);
   Real b(7);
   Real c(5);
   BOOST_CHECK_EQUAL(-a % c, 0);
   BOOST_CHECK_EQUAL(-a % b, -20 % 7);
   BOOST_CHECK_EQUAL(-a % -b, -20 % -7);
   BOOST_CHECK_EQUAL(a % -b, 20 % -7);
   BOOST_CHECK_EQUAL(-a % 7, -20 % 7);
   BOOST_CHECK_EQUAL(-a % -7, -20 % -7);
   BOOST_CHECK_EQUAL(a % -7, 20 % -7);
   BOOST_CHECK_EQUAL(-a % 7u, -20 % 7);
   BOOST_CHECK_EQUAL(-a % a, 0);
   BOOST_CHECK_EQUAL(-a % 5, 0);
   BOOST_CHECK_EQUAL(-a % -5, 0);
   BOOST_CHECK_EQUAL(a % -5, 0);

   b = -b;
   BOOST_CHECK_EQUAL(a % b, 20 % -7);
   a = -a;
   BOOST_CHECK_EQUAL(a % b, -20 % -7);
   BOOST_CHECK_EQUAL(a % -7, -20 % -7);
   b = 7;
   BOOST_CHECK_EQUAL(a % b, -20 % 7);
   BOOST_CHECK_EQUAL(a % 7, -20 % 7);
   BOOST_CHECK_EQUAL(a % 7u, -20 % 7);

   a = 20;
   a %= b;
   BOOST_CHECK_EQUAL(a, 20 % 7);
   a = -20;
   a %= b;
   BOOST_CHECK_EQUAL(a, -20 % 7);
   a = 20;
   a %= -b;
   BOOST_CHECK_EQUAL(a, 20 % -7);
   a = -20;
   a %= -b;
   BOOST_CHECK_EQUAL(a, -20 % -7);
   a = 5;
   a %= b - a;
   BOOST_CHECK_EQUAL(a, 5 % (7 - 5));
   a = -20;
   a %= 7;
   BOOST_CHECK_EQUAL(a, -20 % 7);
   a = 20;
   a %= -7;
   BOOST_CHECK_EQUAL(a, 20 % -7);
   a = -20;
   a %= -7;
   BOOST_CHECK_EQUAL(a, -20 % -7);
#ifndef BOOST_NO_LONG_LONG
   a = -20;
   a %= 7uLL;
   BOOST_CHECK_EQUAL(a, -20 % 7);
   a = 20;
   a %= -7LL;
   BOOST_CHECK_EQUAL(a, 20 % -7);
   a = -20;
   a %= -7LL;
   BOOST_CHECK_EQUAL(a, -20 % -7);
#endif
   a = 400;
   b = 45;
   BOOST_CHECK_EQUAL(gcd(a, -45), boost::integer::gcd(400, 45));
   BOOST_CHECK_EQUAL(lcm(a, -45), boost::integer::lcm(400, 45));
   BOOST_CHECK_EQUAL(gcd(-400, b), boost::integer::gcd(400, 45));
   BOOST_CHECK_EQUAL(lcm(-400, b), boost::integer::lcm(400, 45));
   a = -20;
   BOOST_CHECK_EQUAL(abs(a), 20);
   BOOST_CHECK_EQUAL(abs(-a), 20);
   BOOST_CHECK_EQUAL(abs(+a), 20);
   a = 20;
   BOOST_CHECK_EQUAL(abs(a), 20);
   BOOST_CHECK_EQUAL(abs(-a), 20);
   BOOST_CHECK_EQUAL(abs(+a), 20);
   a = -400;
   b = 45;
   BOOST_CHECK_EQUAL(gcd(a, b), boost::integer::gcd(-400, 45));
   BOOST_CHECK_EQUAL(lcm(a, b), boost::integer::lcm(-400, 45));
   BOOST_CHECK_EQUAL(gcd(a, 45), boost::integer::gcd(-400, 45));
   BOOST_CHECK_EQUAL(lcm(a, 45), boost::integer::lcm(-400, 45));
   BOOST_CHECK_EQUAL(gcd(-400, b), boost::integer::gcd(-400, 45));
   BOOST_CHECK_EQUAL(lcm(-400, b), boost::integer::lcm(-400, 45));
   Real r;
   divide_qr(a, b, c, r);
   BOOST_CHECK_EQUAL(c, a / b);
   BOOST_CHECK_EQUAL(r, a % b);
   BOOST_CHECK_EQUAL(integer_modulus(a, 57), abs(a % 57));
   b = -57;
   divide_qr(a, b, c, r);
   BOOST_CHECK_EQUAL(c, a / b);
   BOOST_CHECK_EQUAL(r, a % b);
   BOOST_CHECK_EQUAL(integer_modulus(a, -57), abs(a % -57));
   a = 458;
   divide_qr(a, b, c, r);
   BOOST_CHECK_EQUAL(c, a / b);
   BOOST_CHECK_EQUAL(r, a % b);
   BOOST_CHECK_EQUAL(integer_modulus(a, -57), abs(a % -57));
#ifndef TEST_CHECKED_INT
   if (is_checked_cpp_int<Real>::value)
   {
      a = -1;
#ifndef BOOST_NO_EXCEPTIONS
      BOOST_CHECK_THROW(a << 2, std::range_error);
      BOOST_CHECK_THROW(a >> 2, std::range_error);
      BOOST_CHECK_THROW(a <<= 2, std::range_error);
      BOOST_CHECK_THROW(a >>= 2, std::range_error);
#endif
   }
   else
   {
      a = -1;
      BOOST_CHECK_EQUAL(a << 10, -1024);
      a = -23;
      BOOST_CHECK_EQUAL(a << 10, -23552);
      a = -23456;
      BOOST_CHECK_EQUAL(a >> 10, -23);
      a = -3;
      BOOST_CHECK_EQUAL(a >> 10, -1);
   }
#endif
}
template <class Real>
void test_signed_integer_ops(const std::integral_constant<bool, false>&)
{
}

template <class Real>
inline Real negate_if_signed(Real r, const std::integral_constant<bool, true>&)
{
   return -r;
}
template <class Real>
inline Real negate_if_signed(Real r, const std::integral_constant<bool, false>&)
{
   return r;
}

template <class Real, class Int>
void test_integer_overflow()
{
   if (std::numeric_limits<Real>::digits > std::numeric_limits<Int>::digits)
   {
      Real m((std::numeric_limits<Int>::max)());
      Int  r;
      ++m;
      if (is_checked_cpp_int<Real>::value)
      {
         BOOST_CHECK_THROW(m.template convert_to<Int>(), std::overflow_error);
      }
      else if (boost::multiprecision::detail::is_signed<Int>::value)
      {
         r = m.template convert_to<Int>();
         BOOST_CHECK_EQUAL(r, (std::numeric_limits<Int>::max)());
      }
      else
      {
         r = m.template convert_to<Int>();
         BOOST_CHECK_EQUAL(r, 0);
      }
      // Again with much larger value:
      m = 1u;
      m <<= (std::min)(std::numeric_limits<Real>::digits - 1, 1000);
      if (is_checked_cpp_int<Real>::value)
      {
         BOOST_CHECK_THROW(m.template convert_to<Int>(), std::overflow_error);
      }
      else if (boost::multiprecision::detail::is_signed<Int>::value && boost::multiprecision::detail::is_integral<Int>::value)
      {
         r = m.template convert_to<Int>();
         BOOST_CHECK_EQUAL(r, (std::numeric_limits<Int>::max)());
      }
      else
      {
         r = m.template convert_to<Int>();
         BOOST_CHECK_EQUAL(r, 0);
      }

      if (std::numeric_limits<Real>::is_signed && (boost::multiprecision::detail::is_signed<Int>::value))
      {
         m = (std::numeric_limits<Int>::min)();
         --m;
         if (is_checked_cpp_int<Real>::value)
         {
            BOOST_CHECK_THROW(m.template convert_to<Int>(), std::overflow_error);
         }
         else
         {
            r = m.template convert_to<Int>();
            BOOST_CHECK_EQUAL(r, (std::numeric_limits<Int>::min)());
         }
         // Again with much larger value:
         m = 2u;
         m = pow(m, (std::min)(std::numeric_limits<Real>::digits - 1, 1000));
         ++m;
         m = negate_if_signed(m, std::integral_constant<bool, std::numeric_limits<Real>::is_signed>());
         if (is_checked_cpp_int<Real>::value)
         {
            BOOST_CHECK_THROW(m.template convert_to<Int>(), std::overflow_error);
         }
         else
         {
            r = m.template convert_to<Int>();
            BOOST_CHECK_EQUAL(r, (std::numeric_limits<Int>::min)());
         }
      }
      else if (std::numeric_limits<Real>::is_signed && !boost::multiprecision::detail::is_signed<Int>::value)
      {
         // signed to unsigned converison with overflow, it's really not clear what should happen here!
         m = (std::numeric_limits<Int>::max)();
         ++m;
         m = negate_if_signed(m, std::integral_constant<bool, std::numeric_limits<Real>::is_signed>());
         BOOST_CHECK_THROW(m.template convert_to<Int>(), std::range_error);
         // Again with much larger value:
         m = 2u;
         m = pow(m, (std::min)(std::numeric_limits<Real>::digits - 1, 1000));
         m = negate_if_signed(m, std::integral_constant<bool, std::numeric_limits<Real>::is_signed>());
         BOOST_CHECK_THROW(m.template convert_to<Int>(), std::range_error);
      }
   }
}

template <class Real, class Int>
void test_integer_round_trip()
{
   if (std::numeric_limits<Real>::digits >= std::numeric_limits<Int>::digits)
   {
      Real m((std::numeric_limits<Int>::max)());
      Int  r = m.template convert_to<Int>();
      BOOST_CHECK_EQUAL(m, r);
      if (std::numeric_limits<Real>::is_signed && (std::numeric_limits<Real>::digits > std::numeric_limits<Int>::digits))
      {
         m = (std::numeric_limits<Int>::min)();
         r = m.template convert_to<Int>();
         BOOST_CHECK_EQUAL(m, r);
      }
   }
   test_integer_overflow<Real, Int>();
}

template <class Real>
void test_integer_ops(const std::integral_constant<int, boost::multiprecision::number_kind_integer>&)
{
   test_signed_integer_ops<Real>(std::integral_constant<bool, std::numeric_limits<Real>::is_signed>());

   Real a(20);
   Real b(7);
   Real c(5);
   BOOST_CHECK_EQUAL(a % b, 20 % 7);
   BOOST_CHECK_EQUAL(a % 7, 20 % 7);
   BOOST_CHECK_EQUAL(a % 7u, 20 % 7);
   BOOST_CHECK_EQUAL(a % a, 0);
   BOOST_CHECK_EQUAL(a % c, 0);
   BOOST_CHECK_EQUAL(a % 5, 0);
   a = a % (b + 0);
   BOOST_CHECK_EQUAL(a, 20 % 7);
   a = 20;
   c = (a + 2) % (a - 1);
   BOOST_CHECK_EQUAL(c, 22 % 19);
   c = 5;
   a = b % (a - 15);
   BOOST_CHECK_EQUAL(a, 7 % 5);
   a = 20;

   a = 20;
   a %= 7;
   BOOST_CHECK_EQUAL(a, 20 % 7);
#ifndef BOOST_NO_LONG_LONG
   a = 20;
   a %= 7uLL;
   BOOST_CHECK_EQUAL(a, 20 % 7);
#endif
   a = 20;
   ++a;
   BOOST_CHECK_EQUAL(a, 21);
   --a;
   BOOST_CHECK_EQUAL(a, 20);
   BOOST_CHECK_EQUAL(a++, 20);
   BOOST_CHECK_EQUAL(a, 21);
   BOOST_CHECK_EQUAL(a--, 21);
   BOOST_CHECK_EQUAL(a, 20);
   a = 2000;
   a <<= 20;
   BOOST_CHECK_EQUAL(a, 2000L << 20);
   a >>= 20;
   BOOST_CHECK_EQUAL(a, 2000);
   a <<= 20u;
   BOOST_CHECK_EQUAL(a, 2000L << 20);
   a >>= 20u;
   BOOST_CHECK_EQUAL(a, 2000);
#ifndef BOOST_NO_EXCEPTIONS
   BOOST_CHECK_THROW(a <<= -20, std::out_of_range);
   BOOST_CHECK_THROW(a >>= -20, std::out_of_range);
   BOOST_CHECK_THROW(Real(a << -20), std::out_of_range);
   BOOST_CHECK_THROW(Real(a >> -20), std::out_of_range);
#endif
#ifndef BOOST_NO_LONG_LONG
   if (sizeof(long long) > sizeof(std::size_t))
   {
      // extreme values should trigger an exception:
#ifndef BOOST_NO_EXCEPTIONS
      BOOST_CHECK_THROW(a >>= (1uLL << (sizeof(long long) * CHAR_BIT - 2)), std::out_of_range);
      BOOST_CHECK_THROW(a <<= (1uLL << (sizeof(long long) * CHAR_BIT - 2)), std::out_of_range);
      BOOST_CHECK_THROW(a >>= -(1LL << (sizeof(long long) * CHAR_BIT - 2)), std::out_of_range);
      BOOST_CHECK_THROW(a <<= -(1LL << (sizeof(long long) * CHAR_BIT - 2)), std::out_of_range);
      BOOST_CHECK_THROW(a >>= (1LL << (sizeof(long long) * CHAR_BIT - 2)), std::out_of_range);
      BOOST_CHECK_THROW(a <<= (1LL << (sizeof(long long) * CHAR_BIT - 2)), std::out_of_range);
#endif
      // Unless they fit within range:
      a = 2000L;
      a <<= 20uLL;
      BOOST_CHECK_EQUAL(a, (2000L << 20));
      a = 2000;
      a <<= 20LL;
      BOOST_CHECK_EQUAL(a, (2000L << 20));

#ifndef BOOST_NO_EXCEPTIONS
      BOOST_CHECK_THROW(Real(a >> (1uLL << (sizeof(long long) * CHAR_BIT - 2))), std::out_of_range);
      BOOST_CHECK_THROW(Real(a <<= (1uLL << (sizeof(long long) * CHAR_BIT - 2))), std::out_of_range);
      BOOST_CHECK_THROW(Real(a >>= -(1LL << (sizeof(long long) * CHAR_BIT - 2))), std::out_of_range);
      BOOST_CHECK_THROW(Real(a <<= -(1LL << (sizeof(long long) * CHAR_BIT - 2))), std::out_of_range);
      BOOST_CHECK_THROW(Real(a >>= (1LL << (sizeof(long long) * CHAR_BIT - 2))), std::out_of_range);
      BOOST_CHECK_THROW(Real(a <<= (1LL << (sizeof(long long) * CHAR_BIT - 2))), std::out_of_range);
#endif
      // Unless they fit within range:
      a = 2000L;
      BOOST_CHECK_EQUAL(Real(a << 20uLL), (2000L << 20));
      a = 2000;
      BOOST_CHECK_EQUAL(Real(a << 20LL), (2000L << 20));
   }
#endif
   a = 20;
   b = a << 20;
   BOOST_CHECK_EQUAL(b, (20 << 20));
   b = a >> 2;
   BOOST_CHECK_EQUAL(b, (20 >> 2));
   b = (a + 2) << 10;
   BOOST_CHECK_EQUAL(b, (22 << 10));
   b = (a + 3) >> 3;
   BOOST_CHECK_EQUAL(b, (23 >> 3));
   //
   // Bit fiddling:
   //
   int i = 1020304;
   int j = 56789123;
   int k = 4523187;
   a     = i;
   b     = j;
   c     = a;
   c &= b;
   BOOST_CHECK_EQUAL(c, (i & j));
   c = a;
   c &= j;
   BOOST_CHECK_EQUAL(c, (i & j));
   c = a;
   c &= a + b;
   BOOST_CHECK_EQUAL(c, (i & (i + j)));
   BOOST_CHECK_EQUAL((a & b), (i & j));
   c = k;
   a = a & (b + k);
   BOOST_CHECK_EQUAL(a, (i & (j + k)));
   a = i;
   a = (b + k) & a;
   BOOST_CHECK_EQUAL(a, (i & (j + k)));
   a = i;
   c = a & b & k;
   BOOST_CHECK_EQUAL(c, (i & j & k));
   c = a;
   c &= (c + b);
   BOOST_CHECK_EQUAL(c, (i & (i + j)));
   c = a & (b | 1);
   BOOST_CHECK_EQUAL(c, (i & (j | 1)));

   test_complement<Real>(a, b, c, typename is_twos_complement_integer<Real>::type());

   a = i;
   b = j;
   c = a;
   c |= b;
   BOOST_CHECK_EQUAL(c, (i | j));
   c = a;
   c |= j;
   BOOST_CHECK_EQUAL(c, (i | j));
   c = a;
   c |= a + b;
   BOOST_CHECK_EQUAL(c, (i | (i + j)));
   BOOST_CHECK_EQUAL((a | b), (i | j));
   c = k;
   a = a | (b + k);
   BOOST_CHECK_EQUAL(a, (i | (j + k)));
   a = i;
   a = (b + k) | a;
   BOOST_CHECK_EQUAL(a, (i | (j + k)));
   a = i;
   c = a | b | k;
   BOOST_CHECK_EQUAL(c, (i | j | k));
   c = a;
   c |= (c + b);
   BOOST_CHECK_EQUAL(c, (i | (i + j)));
   c = a | (b | 1);
   BOOST_CHECK_EQUAL(c, (i | (j | 1)));

   a = i;
   b = j;
   c = a;
   c ^= b;
   BOOST_CHECK_EQUAL(c, (i ^ j));
   c = a;
   c ^= j;
   BOOST_CHECK_EQUAL(c, (i ^ j));
   c = a;
   c ^= a + b;
   BOOST_CHECK_EQUAL(c, (i ^ (i + j)));
   BOOST_CHECK_EQUAL((a ^ b), (i ^ j));
   c = k;
   a = a ^ (b + k);
   BOOST_CHECK_EQUAL(a, (i ^ (j + k)));
   a = i;
   a = (b + k) ^ a;
   BOOST_CHECK_EQUAL(a, (i ^ (j + k)));
   a = i;
   c = a ^ b ^ k;
   BOOST_CHECK_EQUAL(c, (i ^ j ^ k));
   c = a;
   c ^= (c + b);
   BOOST_CHECK_EQUAL(c, (i ^ (i + j)));
   c = a ^ (b | 1);
   BOOST_CHECK_EQUAL(c, (i ^ (j | 1)));

   a = i;
   b = j;
   c = k;
   //
   // Non-member functions:
   //
   a = 400;
   b = 45;
   BOOST_CHECK_EQUAL(gcd(a, b), boost::integer::gcd(400, 45));
   BOOST_CHECK_EQUAL(lcm(a, b), boost::integer::lcm(400, 45));
   BOOST_CHECK_EQUAL(gcd(a, 45), boost::integer::gcd(400, 45));
   BOOST_CHECK_EQUAL(lcm(a, 45), boost::integer::lcm(400, 45));
   BOOST_CHECK_EQUAL(gcd(a, 45u), boost::integer::gcd(400, 45));
   BOOST_CHECK_EQUAL(lcm(a, 45u), boost::integer::lcm(400, 45));
   BOOST_CHECK_EQUAL(gcd(400, b), boost::integer::gcd(400, 45));
   BOOST_CHECK_EQUAL(lcm(400, b), boost::integer::lcm(400, 45));
   BOOST_CHECK_EQUAL(gcd(400u, b), boost::integer::gcd(400, 45));
   BOOST_CHECK_EQUAL(lcm(400u, b), boost::integer::lcm(400, 45));

   if (std::numeric_limits<Real>::is_bounded)
   {
      // Fixed precision integer:
      a = (std::numeric_limits<Real>::max)() - 1;
      b = (std::numeric_limits<Real>::max)() / 35;
      Real div = gcd(a, b);
      BOOST_CHECK_EQUAL(a % div, 0);
      BOOST_CHECK_EQUAL(b % div, 0);
   }

   //
   // Conditionals involving 2 arg functions:
   //
   test_conditional(Real(gcd(a, b)), gcd(a, b));

   Real r;
   divide_qr(a, b, c, r);
   BOOST_CHECK_EQUAL(c, a / b);
   BOOST_CHECK_EQUAL(r, a % b);
   divide_qr(a + 0, b, c, r);
   BOOST_CHECK_EQUAL(c, a / b);
   BOOST_CHECK_EQUAL(r, a % b);
   divide_qr(a, b + 0, c, r);
   BOOST_CHECK_EQUAL(c, a / b);
   BOOST_CHECK_EQUAL(r, a % b);
   divide_qr(a + 0, b + 0, c, r);
   BOOST_CHECK_EQUAL(c, a / b);
   BOOST_CHECK_EQUAL(r, a % b);
   BOOST_CHECK_EQUAL(integer_modulus(a, 57), a % 57);
   for (i = 0; i < 20; ++i)
   {
      if (std::numeric_limits<Real>::is_specialized && (!std::numeric_limits<Real>::is_bounded || ((int)i * 17 < std::numeric_limits<Real>::digits)))
      {
         BOOST_CHECK_EQUAL(lsb(Real(1) << (i * 17)), static_cast<unsigned>(i * 17));
         BOOST_CHECK_EQUAL(msb(Real(1) << (i * 17)), static_cast<unsigned>(i * 17));
         BOOST_CHECK(bit_test(Real(1) << (i * 17), i * 17));
         BOOST_CHECK(!bit_test(Real(1) << (i * 17), i * 17 + 1));
         if (i)
         {
            BOOST_CHECK(!bit_test(Real(1) << (i * 17), i * 17 - 1));
         }
         Real zero(0);
         BOOST_CHECK(bit_test(bit_set(zero, i * 17), i * 17));
         zero = 0;
         BOOST_CHECK_EQUAL(bit_flip(zero, i * 17), Real(1) << i * 17);
         zero = Real(1) << i * 17;
         BOOST_CHECK_EQUAL(bit_flip(zero, i * 17), 0);
         zero = Real(1) << i * 17;
         BOOST_CHECK_EQUAL(bit_unset(zero, i * 17), 0);
      }
   }
   //
   // pow, powm:
   //
   BOOST_CHECK_EQUAL(pow(Real(3), 4u), 81);
   BOOST_CHECK_EQUAL(pow(Real(3) + Real(0), 4u), 81);
   BOOST_CHECK_EQUAL(powm(Real(3), Real(4), Real(13)), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3), Real(4), 13), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3), Real(4), Real(13) + 0), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3), Real(4) + 0, Real(13)), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3), Real(4) + 0, 13), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3), Real(4) + 0, Real(13) + 0), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3), 4 + 0, Real(13)), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3), 4 + 0, 13), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3), 4 + 0, Real(13) + 0), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3) + 0, Real(4), Real(13)), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3) + 0, Real(4), 13), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3) + 0, Real(4), Real(13) + 0), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3) + 0, Real(4) + 0, Real(13)), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3) + 0, Real(4) + 0, 13), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3) + 0, Real(4) + 0, Real(13) + 0), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3) + 0, 4 + 0, Real(13)), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3) + 0, 4 + 0, 13), 81 % 13);
   BOOST_CHECK_EQUAL(powm(Real(3) + 0, 4 + 0, Real(13) + 0), 81 % 13);
   //
   // Conditionals involving 3 arg functions:
   //
   test_conditional(Real(powm(Real(3), Real(4), Real(13))), powm(Real(3), Real(4), Real(13)));

#ifndef BOOST_NO_EXCEPTIONS
   //
   // Things that are expected errors:
   //
   BOOST_CHECK_THROW(Real("3.14"), std::runtime_error);
   BOOST_CHECK_THROW(Real("3L"), std::runtime_error);
   BOOST_CHECK_THROW(Real(Real(20) / 0u), std::overflow_error);
#endif
   //
   // Extra tests added for full coverage:
   //
   a = 20;
   b = 7;
   c = 20 % b;
   BOOST_CHECK_EQUAL(c, (20 % 7));
   c = 20 % (b + 0);
   BOOST_CHECK_EQUAL(c, (20 % 7));
   c = a & 10;
   BOOST_CHECK_EQUAL(c, (20 & 10));
   c = 10 & a;
   BOOST_CHECK_EQUAL(c, (20 & 10));
   c = (a + 0) & (b + 0);
   BOOST_CHECK_EQUAL(c, (20 & 7));
   c = 10 & (a + 0);
   BOOST_CHECK_EQUAL(c, (20 & 10));
   c = 10 | a;
   BOOST_CHECK_EQUAL(c, (20 | 10));
   c = (a + 0) | (b + 0);
   BOOST_CHECK(c == (20 | 7))
   c = 20 | (b + 0);
   BOOST_CHECK_EQUAL(c, (20 | 7));
   c = a ^ 7;
   BOOST_CHECK_EQUAL(c, (20 ^ 7));
   c = 20 ^ b;
   BOOST_CHECK_EQUAL(c, (20 ^ 7));
   c = (a + 0) ^ (b + 0);
   BOOST_CHECK_EQUAL(c, (20 ^ 7));
   c = 20 ^ (b + 0);
   BOOST_CHECK_EQUAL(c, (20 ^ 7));
   //
   // RValue ref tests:
   //
   c = Real(20) % b;
   BOOST_CHECK_EQUAL(c, (20 % 7));
   c = a % Real(7);
   BOOST_CHECK_EQUAL(c, (20 % 7));
   c = Real(20) % Real(7);
   BOOST_CHECK_EQUAL(c, (20 % 7));
   c = Real(20) % 7;
   BOOST_CHECK_EQUAL(c, (20 % 7));
   c = 20 % Real(7);
   BOOST_CHECK_EQUAL(c, (20 % 7));
   c = Real(20) % (b * 1);
   BOOST_CHECK_EQUAL(c, (20 % 7));
   c = (a * 1 + 0) % Real(7);
   BOOST_CHECK_EQUAL(c, (20 % 7));
   c = Real(20) >> 2;
   BOOST_CHECK_EQUAL(c, (20 >> 2));
   c = Real(20) & b;
   BOOST_CHECK_EQUAL(c, (20 & 7));
   c = a & Real(7);
   BOOST_CHECK_EQUAL(c, (20 & 7));
   c = Real(20) & Real(7);
   BOOST_CHECK_EQUAL(c, (20 & 7));
   c = Real(20) & 7;
   BOOST_CHECK_EQUAL(c, (20 & 7));
   c = 20 & Real(7);
   BOOST_CHECK_EQUAL(c, (20 & 7));
   c = Real(20) & (b * 1 + 0);
   BOOST_CHECK_EQUAL(c, (20 & 7));
   c = (a * 1 + 0) & Real(7);
   BOOST_CHECK_EQUAL(c, (20 & 7));
   c = Real(20) | b;
   BOOST_CHECK_EQUAL(c, (20 | 7));
   c = a | Real(7);
   BOOST_CHECK_EQUAL(c, (20 | 7));
   c = Real(20) | Real(7);
   BOOST_CHECK_EQUAL(c, (20 | 7));
   c = Real(20) | 7;
   BOOST_CHECK_EQUAL(c, (20 | 7));
   c = 20 | Real(7);
   BOOST_CHECK_EQUAL(c, (20 | 7));
   c = Real(20) | (b * 1 + 0);
   BOOST_CHECK_EQUAL(c, (20 | 7));
   c = (a * 1 + 0) | Real(7);
   BOOST_CHECK_EQUAL(c, (20 | 7));
   c = Real(20) ^ b;
   BOOST_CHECK_EQUAL(c, (20 ^ 7));
   c = a ^ Real(7);
   BOOST_CHECK_EQUAL(c, (20 ^ 7));
   c = Real(20) ^ Real(7);
   BOOST_CHECK_EQUAL(c, (20 ^ 7));
   c = Real(20) ^ 7;
   BOOST_CHECK_EQUAL(c, (20 ^ 7));
   c = 20 ^ Real(7);
   BOOST_CHECK_EQUAL(c, (20 ^ 7));
   c = Real(20) ^ (b * 1 + 0);
   BOOST_CHECK_EQUAL(c, (20 ^ 7));
   c = (a * 1 + 0) ^ Real(7);
   BOOST_CHECK_EQUAL(c, (20 ^ 7));

   //
   // Round tripping of built in integers:
   //
   test_integer_round_trip<Real, short>();
   test_integer_round_trip<Real, unsigned short>();
   test_integer_round_trip<Real, int>();
   test_integer_round_trip<Real, unsigned int>();
   test_integer_round_trip<Real, long>();
   test_integer_round_trip<Real, unsigned long>();
#ifndef BOOST_NO_LONG_LONG
   test_integer_round_trip<Real, long long>();
   test_integer_round_trip<Real, unsigned long long>();
#endif
}

template <class Real, class T>
void test_float_funcs(const T&) {}

template <class Real>
void test_float_funcs(const std::integral_constant<bool, true>&)
{
   if (boost::multiprecision::is_interval_number<Real>::value)
      return;
   //
   // Test variable reuse in function calls, see https://svn.boost.org/trac/boost/ticket/8326
   //
   Real a(2), b(10), c, d;
   a = pow(a, b);
   BOOST_CHECK_EQUAL(a, 1024);
   a = 2;
   b = pow(a, b);
   BOOST_CHECK_EQUAL(b, 1024);
   b = 10;
   a = pow(a, 10);
   BOOST_CHECK_EQUAL(a, 1024);
   a = -2;
   a = abs(a);
   BOOST_CHECK_EQUAL(a, 2);
   a = -2;
   a = fabs(a);
   BOOST_CHECK_EQUAL(a, 2);
   a = 2.5;
   a = floor(a);
   BOOST_CHECK_EQUAL(a, 2);
   a = 2.5;
   a = ceil(a);
   BOOST_CHECK_EQUAL(a, 3);
   a = 2.5;
   a = trunc(a);
   BOOST_CHECK_EQUAL(a, 2);
   a = 2.25;
   a = round(a);
   BOOST_CHECK_EQUAL(a, 2);
   a = 2;
   a = ldexp(a, 1);
   BOOST_CHECK_EQUAL(a, 4);
   int i;
   a = frexp(a, &i);
   BOOST_CHECK_EQUAL(a, 0.5);

   Real tol = std::numeric_limits<Real>::epsilon() * 3;
   a        = 4;
   a        = sqrt(a);
   BOOST_CHECK_CLOSE_FRACTION(a, 2, tol);
   a = 3;
   a = exp(a);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(exp(Real(3))), tol);
   a = 3;
   a = log(a);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(log(Real(3))), tol);
   a = 3;
   a = log10(a);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(log10(Real(3))), tol);

   a = 0.5;
   a = sin(a);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(sin(Real(0.5))), tol);
   a = 0.5;
   a = cos(a);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(cos(Real(0.5))), tol);
   a = 0.5;
   a = tan(a);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(tan(Real(0.5))), tol);
   a = 0.5;
   a = asin(a);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(asin(Real(0.5))), tol);
   a = 0.5;
   a = acos(a);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(acos(Real(0.5))), tol);
   a = 0.5;
   a = atan(a);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(atan(Real(0.5))), tol);
   a = 0.5;
   a = sinh(a);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(sinh(Real(0.5))), tol);
   a = 0.5;
   a = cosh(a);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(cosh(Real(0.5))), tol);
   a = 0.5;
   a = tanh(a);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(tanh(Real(0.5))), tol);
   // fmod, need to check all the sign permutations:
   a = 4;
   b = 2;
   a = fmod(a, b);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(fmod(Real(4), Real(2))), tol);
   a = 4;
   b = fmod(a, b);
   BOOST_CHECK_CLOSE_FRACTION(b, Real(fmod(Real(4), Real(2))), tol);
   a = 4;
   b = 2;
   a = fmod(-a, b);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(fmod(-Real(4), Real(2))), tol);
   a = 4;
   b = fmod(-a, b);
   BOOST_CHECK_CLOSE_FRACTION(b, Real(-fmod(Real(4), Real(2))), tol);
   a = 4;
   b = 2;
   a = fmod(a, -b);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(fmod(Real(4), -Real(2))), tol);
   a = 4;
   b = fmod(a, -b);
   BOOST_CHECK_CLOSE_FRACTION(b, Real(fmod(Real(4), -Real(2))), tol);
   a = 4;
   b = 2;
   a = fmod(-a, -b);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(fmod(-Real(4), -Real(2))), tol);
   a = 4;
   b = fmod(-a, -b);
   BOOST_CHECK_CLOSE_FRACTION(b, Real(fmod(-Real(4), -Real(2))), tol);
   // modf:
   a = 5;
   a /= 2;
   b = modf(a, &c);
   BOOST_CHECK_EQUAL(b + c, a);
   BOOST_CHECK_EQUAL(b > 0, a > 0);
   BOOST_CHECK_EQUAL(c > 0, a > 0);
   a = -a;
   b = modf(a, &c);
   BOOST_CHECK_EQUAL(b + c, a);
   BOOST_CHECK_EQUAL(b > 0, a > 0);
   BOOST_CHECK_EQUAL(c > 0, a > 0);
   b = modf(a, &c);
   c = 0;
   modf(a, &c);
   BOOST_CHECK_EQUAL(b + c, a);
   BOOST_CHECK_EQUAL(b > 0, a > 0);
   BOOST_CHECK_EQUAL(c > 0, a > 0);
   a = -a;
   b = modf(a, &c);
   c = 0;
   modf(a, &c);
   BOOST_CHECK_EQUAL(b + c, a);
   BOOST_CHECK_EQUAL(b > 0, a > 0);
   BOOST_CHECK_EQUAL(c > 0, a > 0);

   if (std::numeric_limits<Real>::has_infinity)
   {
      a = std::numeric_limits<Real>::infinity();
      b = modf(a, &c);
      BOOST_CHECK_EQUAL(a, c);
      BOOST_CHECK_EQUAL(b, 0);
      a = -std::numeric_limits<Real>::infinity();
      b = modf(a, &c);
      BOOST_CHECK_EQUAL(a, c);
      BOOST_CHECK_EQUAL(b, 0);
   }
   if (std::numeric_limits<Real>::has_quiet_NaN)
   {
      a = std::numeric_limits<Real>::quiet_NaN();
      b = modf(a, &c);
      BOOST_CHECK((boost::math::isnan)(b));
      BOOST_CHECK((boost::math::isnan)(c));
   }

   a = 4;
   b = 2;
   a = atan2(a, b);
   BOOST_CHECK_CLOSE_FRACTION(a, Real(atan2(Real(4), Real(2))), tol);
   a = 4;
   b = atan2(a, b);
   BOOST_CHECK_CLOSE_FRACTION(b, Real(atan2(Real(4), Real(2))), tol);

   // fma:
   a = 2;
   b = 4;
   c = 6;
   BOOST_CHECK_EQUAL(fma(a, b, c), 14);
   BOOST_CHECK_EQUAL(fma(a, 4, c), 14);
   BOOST_CHECK_EQUAL(fma(a, b, 6), 14);
   BOOST_CHECK_EQUAL(fma(a, 4, 6), 14);
   BOOST_CHECK_EQUAL(fma(a + 0, b, c), 14);
   BOOST_CHECK_EQUAL(fma(a - 0, 4, c), 14);
   BOOST_CHECK_EQUAL(fma(a * 1, b, 6), 14);
   BOOST_CHECK_EQUAL(fma(a / 1, 4, 6), 14);
   BOOST_CHECK_EQUAL(fma(2, b, c), 14);
   BOOST_CHECK_EQUAL(fma(2, b, 6), 14);
   BOOST_CHECK_EQUAL(fma(2, b * 1, c), 14);
   BOOST_CHECK_EQUAL(fma(2, b + 0, 6), 14);
   BOOST_CHECK_EQUAL(fma(2, 4, c), 14);
   BOOST_CHECK_EQUAL(fma(2, 4, c + 0), 14);

   // Default construct, for consistency with native floats, default constructed values are zero:
   Real zero;
   BOOST_CHECK_EQUAL(zero, 0);

   //
   // Complex number functions on scalars:
   //
   a = 40;
   BOOST_CHECK_EQUAL(Real(arg(a)), 0);
   BOOST_CHECK_EQUAL(Real(arg(a + 0)), 0);
   a - 20;
   BOOST_CHECK_EQUAL(Real(arg(a)), 0);
   BOOST_CHECK_EQUAL(Real(arg(a - 20)), 0);
}

template <class T, class U>
void compare_NaNs(const T& a, const U& b)
{
   BOOST_CHECK_EQUAL(a == b, false);
   BOOST_CHECK_EQUAL(a != b, true);
   BOOST_CHECK_EQUAL(a <= b, false);
   BOOST_CHECK_EQUAL(a >= b, false);
   BOOST_CHECK_EQUAL(a > b, false);
   BOOST_CHECK_EQUAL(a < b, false);
   //
   // Again where LHS may be an expression template:
   //
   BOOST_CHECK_EQUAL(1 * a == b, false);
   BOOST_CHECK_EQUAL(1 * a != b, true);
   BOOST_CHECK_EQUAL(1 * a <= b, false);
   BOOST_CHECK_EQUAL(1 * a >= b, false);
   BOOST_CHECK_EQUAL(1 * a > b, false);
   BOOST_CHECK_EQUAL(1 * a < b, false);
   //
   // Again where RHS may be an expression template:
   //
   BOOST_CHECK_EQUAL(a == b * 1, false);
   BOOST_CHECK_EQUAL(a != b * 1, true);
   BOOST_CHECK_EQUAL(a <= b * 1, false);
   BOOST_CHECK_EQUAL(a >= b * 1, false);
   BOOST_CHECK_EQUAL(a > b * 1, false);
   BOOST_CHECK_EQUAL(a < b * 1, false);
   //
   // Again where LHS and RHS may be an expression templates:
   //
   BOOST_CHECK_EQUAL(1 * a == b * 1, false);
   BOOST_CHECK_EQUAL(1 * a != b * 1, true);
   BOOST_CHECK_EQUAL(1 * a <= b * 1, false);
   BOOST_CHECK_EQUAL(1 * a >= b * 1, false);
   BOOST_CHECK_EQUAL(1 * a > b * 1, false);
   BOOST_CHECK_EQUAL(1 * a < b * 1, false);
}

template <class Real, class T>
void test_float_ops(const T&) {}

template <class Real>
void test_float_ops(const std::integral_constant<int, boost::multiprecision::number_kind_floating_point>&)
{
   BOOST_CHECK_EQUAL(abs(Real(2)), 2);
   BOOST_CHECK_EQUAL(abs(Real(-2)), 2);
   BOOST_CHECK_EQUAL(fabs(Real(2)), 2);
   BOOST_CHECK_EQUAL(fabs(Real(-2)), 2);
   BOOST_CHECK_EQUAL(floor(Real(5) / 2), 2);
   BOOST_CHECK_EQUAL(ceil(Real(5) / 2), 3);
   BOOST_CHECK_EQUAL(floor(Real(-5) / 2), -3);
   BOOST_CHECK_EQUAL(ceil(Real(-5) / 2), -2);
   BOOST_CHECK_EQUAL(trunc(Real(5) / 2), 2);
   BOOST_CHECK_EQUAL(trunc(Real(-5) / 2), -2);
   //
   // ldexp and frexp, these pretty much have to be implemented by each backend:
   //
   typedef typename Real::backend_type::exponent_type e_type;
   BOOST_CHECK_EQUAL(ldexp(Real(2), 5), 64);
   BOOST_CHECK_EQUAL(ldexp(Real(2), -5), Real(2) / 32);
   Real   v(512);
   e_type exponent;
   Real   r = frexp(v, &exponent);
   BOOST_CHECK_EQUAL(r, 0.5);
   BOOST_CHECK_EQUAL(exponent, 10);
   BOOST_CHECK_EQUAL(v, 512);
   v = 1 / v;
   r = frexp(v, &exponent);
   BOOST_CHECK_EQUAL(r, 0.5);
   BOOST_CHECK_EQUAL(exponent, -8);
   BOOST_CHECK_EQUAL(ldexp(Real(2), e_type(5)), 64);
   BOOST_CHECK_EQUAL(ldexp(Real(2), e_type(-5)), Real(2) / 32);
   v = 512;
   e_type exp2;
   r = frexp(v, &exp2);
   BOOST_CHECK_EQUAL(r, 0.5);
   BOOST_CHECK_EQUAL(exp2, 10);
   BOOST_CHECK_EQUAL(v, 512);
   v = 1 / v;
   r = frexp(v, &exp2);
   BOOST_CHECK_EQUAL(r, 0.5);
   BOOST_CHECK_EQUAL(exp2, -8);
   //
   // scalbn and logb, these are the same as ldexp and frexp unless the radix is
   // something other than 2:
   //
   if (std::numeric_limits<Real>::is_specialized && std::numeric_limits<Real>::radix)
   {
      BOOST_CHECK_EQUAL(scalbn(Real(2), 5), 2 * pow(double(std::numeric_limits<Real>::radix), 5));
      BOOST_CHECK_EQUAL(scalbn(Real(2), -5), Real(2) / pow(double(std::numeric_limits<Real>::radix), 5));
      v        = 512;
      exponent = ilogb(v);
      r        = scalbn(v, -exponent);
      BOOST_CHECK(r >= 1);
      BOOST_CHECK(r < std::numeric_limits<Real>::radix);
      BOOST_CHECK_EQUAL(exponent, logb(v));
      BOOST_CHECK_EQUAL(v, scalbn(r, exponent));
      v        = 1 / v;
      exponent = ilogb(v);
      r        = scalbn(v, -exponent);
      BOOST_CHECK(r >= 1);
      BOOST_CHECK(r < std::numeric_limits<Real>::radix);
      BOOST_CHECK_EQUAL(exponent, logb(v));
      BOOST_CHECK_EQUAL(v, scalbn(r, exponent));
   }
   //
   // pow and exponent:
   //
   v = 3.25;
   r = pow(v, 0);
   BOOST_CHECK_EQUAL(r, 1);
   r = pow(v, 1);
   BOOST_CHECK_EQUAL(r, 3.25);
   r = pow(v, 2);
   BOOST_CHECK_EQUAL(r, boost::math::pow<2>(3.25));
   r = pow(v, 3);
   BOOST_CHECK_EQUAL(r, boost::math::pow<3>(3.25));
   r = pow(v, 4);
   BOOST_CHECK_EQUAL(r, boost::math::pow<4>(3.25));
   r = pow(v, 5);
   BOOST_CHECK_EQUAL(r, boost::math::pow<5>(3.25));
   r = pow(v, 6);
   BOOST_CHECK_EQUAL(r, boost::math::pow<6>(3.25));
   r = pow(v, 25);
   BOOST_CHECK_EQUAL(r, boost::math::pow<25>(Real(3.25)));

#ifndef BOOST_NO_EXCEPTIONS
   //
   // Things that are expected errors:
   //
   BOOST_CHECK_THROW(Real("3.14L"), std::runtime_error);
   if (std::numeric_limits<Real>::is_specialized)
   {
      if (std::numeric_limits<Real>::has_infinity)
      {
         BOOST_CHECK((boost::math::isinf)(Real(20) / 0u));
      }
      else
      {
         BOOST_CHECK_THROW(r = Real(Real(20) / 0u), std::overflow_error);
      }
   }
#endif
   //
   // Comparisons of NaN's should always fail:
   //
   if (std::numeric_limits<Real>::has_quiet_NaN)
   {
      r = v = std::numeric_limits<Real>::quiet_NaN();
      compare_NaNs(r, v);
      v = 0;
      compare_NaNs(r, v);
      r.swap(v);
      compare_NaNs(r, v);
      //
      // Conmpare NaN to int:
      //
      compare_NaNs(v, 0);
      compare_NaNs(0, v);
      //
      // Compare to floats:
      //
      compare_NaNs(v, 0.5);
      compare_NaNs(0.5, v);
      if (std::numeric_limits<double>::has_quiet_NaN)
      {
         compare_NaNs(r, std::numeric_limits<double>::quiet_NaN());
         compare_NaNs(std::numeric_limits<double>::quiet_NaN(), r);
      }
   }

   //
   // Operations involving NaN's as one argument:
   //
   if (std::numeric_limits<Real>::has_quiet_NaN)
   {
      v = 20.25;
      r = std::numeric_limits<Real>::quiet_NaN();
      BOOST_CHECK((boost::math::isnan)(v + r));
      BOOST_CHECK((boost::math::isnan)(r + v));
      BOOST_CHECK((boost::math::isnan)(r - v));
      BOOST_CHECK((boost::math::isnan)(v - r));
      BOOST_CHECK((boost::math::isnan)(r * v));
      BOOST_CHECK((boost::math::isnan)(v * r));
      BOOST_CHECK((boost::math::isnan)(r / v));
      BOOST_CHECK((boost::math::isnan)(v / r));
      Real t = v;
      BOOST_CHECK((boost::math::isnan)(t += r));
      t = r;
      BOOST_CHECK((boost::math::isnan)(t += v));
      t = r;
      BOOST_CHECK((boost::math::isnan)(t -= v));
      t = v;
      BOOST_CHECK((boost::math::isnan)(t -= r));
      t = r;
      BOOST_CHECK((boost::math::isnan)(t *= v));
      t = v;
      BOOST_CHECK((boost::math::isnan)(t *= r));
      t = r;
      BOOST_CHECK((boost::math::isnan)(t /= v));
      t = v;
      BOOST_CHECK((boost::math::isnan)(t /= r));
   }
   //
   // Operations involving infinities as one argument:
   //
   if (std::numeric_limits<Real>::has_infinity)
   {
      v = 20.25;
      r = std::numeric_limits<Real>::infinity();
      BOOST_CHECK((boost::math::isinf)(v + r));
      BOOST_CHECK((boost::math::isinf)(r + v));
      BOOST_CHECK((boost::math::isinf)(r - v));
      BOOST_CHECK((boost::math::isinf)(v - r));
      BOOST_CHECK_LT(v - r, 0);
      BOOST_CHECK((boost::math::isinf)(r * v));
      BOOST_CHECK((boost::math::isinf)(v * r));
      BOOST_CHECK((boost::math::isinf)(r / v));
      BOOST_CHECK_EQUAL(v / r, 0);
      Real t = v;
      BOOST_CHECK((boost::math::isinf)(t += r));
      t = r;
      BOOST_CHECK((boost::math::isinf)(t += v));
      t = r;
      BOOST_CHECK((boost::math::isinf)(t -= v));
      t = v;
      BOOST_CHECK((boost::math::isinf)(t -= r));
      t = v;
      BOOST_CHECK(t -= r < 0);
      t = r;
      BOOST_CHECK((boost::math::isinf)(t *= v));
      t = v;
      BOOST_CHECK((boost::math::isinf)(t *= r));
      t = r;
      BOOST_CHECK((boost::math::isinf)(t /= v));
      t = v;
      BOOST_CHECK((t /= r) == 0);
   }
   //
   // Operations that should produce NaN as a result:
   //
   if (std::numeric_limits<Real>::has_quiet_NaN)
   {
      v = r  = 0;
      Real t = v / r;
      BOOST_CHECK((boost::math::isnan)(t));
      v /= r;
      BOOST_CHECK((boost::math::isnan)(v));
      t = v / 0;
      BOOST_CHECK((boost::math::isnan)(v));
      if (std::numeric_limits<Real>::has_infinity)
      {
         v = 0;
         r = std::numeric_limits<Real>::infinity();
         t = v * r;
         if (!boost::multiprecision::is_interval_number<Real>::value)
         {
            BOOST_CHECK((boost::math::isnan)(t));
            t = r * 0;
            BOOST_CHECK((boost::math::isnan)(t));
         }
         v = r;
         t = r / v;
         BOOST_CHECK((boost::math::isnan)(t));
      }
   }

   test_float_funcs<Real>(std::integral_constant<bool, std::numeric_limits<Real>::is_specialized>());
}

template <class T>
struct lexical_cast_target_type
{
   typedef typename std::conditional<
       boost::multiprecision::detail::is_signed<T>::value && boost::multiprecision::detail::is_integral<T>::value,
       std::intmax_t,
       typename std::conditional<
           boost::multiprecision::detail::is_unsigned<T>::value,
           std::uintmax_t,
           T>::type>::type type;
};

template <class Real, class Num>
void test_negative_mixed_minmax(std::integral_constant<bool, true> const&)
{
   if (!std::numeric_limits<Real>::is_bounded || (std::numeric_limits<Real>::digits >= std::numeric_limits<Num>::digits))
   {
      Real mx1((std::numeric_limits<Num>::max)() - 1);
      ++mx1;
      Real mx2((std::numeric_limits<Num>::max)());
      BOOST_CHECK_EQUAL(mx1, mx2);
      mx1 = (std::numeric_limits<Num>::max)() - 1;
      ++mx1;
      mx2 = (std::numeric_limits<Num>::max)();
      BOOST_CHECK_EQUAL(mx1, mx2);

      if (!std::numeric_limits<Real>::is_bounded || (std::numeric_limits<Real>::digits > std::numeric_limits<Num>::digits))
      {
         Real mx3((std::numeric_limits<Num>::min)() + 1);
         --mx3;
         Real mx4((std::numeric_limits<Num>::min)());
         BOOST_CHECK_EQUAL(mx3, mx4);
         mx3 = (std::numeric_limits<Num>::min)() + 1;
         --mx3;
         mx4 = (std::numeric_limits<Num>::min)();
         BOOST_CHECK_EQUAL(mx3, mx4);
      }
   }
}
template <class Real, class Num>
void test_negative_mixed_minmax(std::integral_constant<bool, false> const&)
{
}

template <class Real, class Num>
void test_negative_mixed_numeric_limits(std::integral_constant<bool, true> const&)
{
   typedef typename lexical_cast_target_type<Num>::type target_type;
#if defined(TEST_MPFR)
   Num tol = 10 * std::numeric_limits<Num>::epsilon();
#else
   Num tol = 0;
#endif
   static const int        left_shift      = std::numeric_limits<Num>::digits - 1;
   Num                     n1              = -static_cast<Num>(1uLL << ((left_shift < 63) && (left_shift > 0) ? left_shift : 10));
   Num                     n2              = -1;
   Num                     n3              = 0;
   Num                     n4              = -20;
   std::ios_base::fmtflags f               = std::is_floating_point<Num>::value ? std::ios_base::scientific : std::ios_base::fmtflags(0);
   int                     digits_to_print = std::is_floating_point<Num>::value && std::numeric_limits<Num>::is_specialized
                             ? std::numeric_limits<Num>::digits10 + 5
                             : 0;
   if (std::numeric_limits<target_type>::digits <= std::numeric_limits<Real>::digits)
   {
      BOOST_CHECK_CLOSE(n1, checked_lexical_cast<target_type>(Real(n1).str(digits_to_print, f)), tol);
   }
   BOOST_CHECK_CLOSE(n2, checked_lexical_cast<target_type>(Real(n2).str(digits_to_print, f)), 0);
   BOOST_CHECK_CLOSE(n3, checked_lexical_cast<target_type>(Real(n3).str(digits_to_print, f)), 0);
   BOOST_CHECK_CLOSE(n4, checked_lexical_cast<target_type>(Real(n4).str(digits_to_print, f)), 0);
}

template <class Real, class Num>
void test_negative_mixed_numeric_limits(std::integral_constant<bool, false> const&) {}

template <class Real, class Num>
void test_negative_mixed(std::integral_constant<bool, true> const&)
{
   typedef typename std::conditional<
       std::is_convertible<Num, Real>::value,
       typename std::conditional<boost::multiprecision::detail::is_integral<Num>::value && (sizeof(Num) < sizeof(int)), int, Num>::type,
       Real>::type cast_type;
   typedef typename std::conditional<
       std::is_convertible<Num, Real>::value,
       Num,
       Real>::type simple_cast_type;
   std::cout << "Testing mixed arithmetic with type: " << typeid(Real).name() << " and " << typeid(Num).name() << std::endl;
   static const int left_shift = std::numeric_limits<Num>::digits - 1;
   Num              n1         = -static_cast<Num>(1uLL << ((left_shift < 63) && (left_shift > 0) ? left_shift : 10));
   Num              n2         = -1;
   Num              n3         = 0;
   Num              n4         = -20;
   Num              n5         = -8;

   test_comparisons<Real>(n1, n2, std::is_convertible<Num, Real>());
   test_comparisons<Real>(n1, n3, std::is_convertible<Num, Real>());
   test_comparisons<Real>(n3, n1, std::is_convertible<Num, Real>());
   test_comparisons<Real>(n2, n1, std::is_convertible<Num, Real>());
   test_comparisons<Real>(n1, n1, std::is_convertible<Num, Real>());
   test_comparisons<Real>(n3, n3, std::is_convertible<Num, Real>());

   // Default construct:
   BOOST_CHECK_EQUAL(Real(n1), static_cast<cast_type>(n1));
   BOOST_CHECK_EQUAL(Real(n2), static_cast<cast_type>(n2));
   BOOST_CHECK_EQUAL(Real(n3), static_cast<cast_type>(n3));
   BOOST_CHECK_EQUAL(Real(n4), static_cast<cast_type>(n4));
   BOOST_CHECK_EQUAL(static_cast<cast_type>(n1), Real(n1));
   BOOST_CHECK_EQUAL(static_cast<cast_type>(n2), Real(n2));
   BOOST_CHECK_EQUAL(static_cast<cast_type>(n3), Real(n3));
   BOOST_CHECK_EQUAL(static_cast<cast_type>(n4), Real(n4));
   BOOST_CHECK_EQUAL(Real(n1).template convert_to<Num>(), n1);
   BOOST_CHECK_EQUAL(Real(n2).template convert_to<Num>(), n2);
   BOOST_CHECK_EQUAL(Real(n3).template convert_to<Num>(), n3);
   BOOST_CHECK_EQUAL(Real(n4).template convert_to<Num>(), n4);
   BOOST_CHECK_EQUAL(static_cast<Num>(Real(n1)), n1);
   BOOST_CHECK_EQUAL(static_cast<Num>(Real(n2)), n2);
   BOOST_CHECK_EQUAL(static_cast<Num>(Real(n3)), n3);
   BOOST_CHECK_EQUAL(static_cast<Num>(Real(n4)), n4);
   // Conversions when source is an expression template:
   BOOST_CHECK_EQUAL((Real(n1) + 0).template convert_to<Num>(), n1);
   BOOST_CHECK_EQUAL((Real(n2) + 0).template convert_to<Num>(), n2);
   BOOST_CHECK_EQUAL((Real(n3) + 0).template convert_to<Num>(), n3);
   BOOST_CHECK_EQUAL((Real(n4) + 0).template convert_to<Num>(), n4);
   BOOST_CHECK_EQUAL(static_cast<Num>((Real(n1) + 0)), n1);
   BOOST_CHECK_EQUAL(static_cast<Num>((Real(n2) + 0)), n2);
   BOOST_CHECK_EQUAL(static_cast<Num>((Real(n3) + 0)), n3);
   BOOST_CHECK_EQUAL(static_cast<Num>((Real(n4) + 0)), n4);
   test_negative_mixed_numeric_limits<Real, Num>(std::integral_constant<bool, std::numeric_limits<Real>::is_specialized>());
   // Assignment:
   Real r(0);
   BOOST_CHECK(r != static_cast<cast_type>(n1));
   r = static_cast<simple_cast_type>(n1);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n1));
   r = static_cast<simple_cast_type>(n2);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n2));
   r = static_cast<simple_cast_type>(n3);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n3));
   r = static_cast<simple_cast_type>(n4);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n4));
   // Addition:
   r = static_cast<simple_cast_type>(n2);
   BOOST_CHECK_EQUAL(r + static_cast<simple_cast_type>(n4), static_cast<cast_type>(n2 + n4));
   BOOST_CHECK_EQUAL(Real(r + static_cast<simple_cast_type>(n4)), static_cast<cast_type>(n2 + n4));
   r += static_cast<simple_cast_type>(n4);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n2 + n4));
   // subtraction:
   r = static_cast<simple_cast_type>(n4);
   BOOST_CHECK_EQUAL(r - static_cast<simple_cast_type>(n5), static_cast<cast_type>(n4 - n5));
   BOOST_CHECK_EQUAL(Real(r - static_cast<simple_cast_type>(n5)), static_cast<cast_type>(n4 - n5));
   r -= static_cast<simple_cast_type>(n5);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n4 - n5));
   // Multiplication:
   r = static_cast<simple_cast_type>(n2);
   BOOST_CHECK_EQUAL(r * static_cast<simple_cast_type>(n4), static_cast<cast_type>(n2 * n4));
   BOOST_CHECK_EQUAL(Real(r * static_cast<simple_cast_type>(n4)), static_cast<cast_type>(n2 * n4));
   r *= static_cast<simple_cast_type>(n4);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n2 * n4));
   // Division:
   r = static_cast<simple_cast_type>(n1);
   BOOST_CHECK_EQUAL(r / static_cast<simple_cast_type>(n5), static_cast<cast_type>(n1 / n5));
   BOOST_CHECK_EQUAL(Real(r / static_cast<simple_cast_type>(n5)), static_cast<cast_type>(n1 / n5));
   r /= static_cast<simple_cast_type>(n5);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n1 / n5));
   //
   // Extra cases for full coverage:
   //
   r = Real(n4) + static_cast<simple_cast_type>(n5);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n4 + n5));
   r = static_cast<simple_cast_type>(n4) + Real(n5);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n4 + n5));
   r = Real(n4) - static_cast<simple_cast_type>(n5);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n4 - n5));
   r = static_cast<simple_cast_type>(n4) - Real(n5);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n4 - n5));
   r = static_cast<simple_cast_type>(n4) * Real(n5);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n4 * n5));
   r = static_cast<cast_type>(Num(4) * n4) / Real(4);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n4));

   Real a, b, c;
   a = 20;
   b = 30;
   c = -a + b;
   BOOST_CHECK_EQUAL(c, 10);
   c = b + -a;
   BOOST_CHECK_EQUAL(c, 10);
   n4 = 30;
   c  = -a + static_cast<cast_type>(n4);
   BOOST_CHECK_EQUAL(c, 10);
   c = static_cast<cast_type>(n4) + -a;
   BOOST_CHECK_EQUAL(c, 10);
   c = -a + -b;
   BOOST_CHECK_EQUAL(c, -50);
   n4 = 4;
   c  = -(a + b) + static_cast<cast_type>(n4);
   BOOST_CHECK_EQUAL(c, -50 + 4);
   n4 = 50;
   c  = (a + b) - static_cast<cast_type>(n4);
   BOOST_CHECK_EQUAL(c, 0);
   c = (a + b) - static_cast<cast_type>(n4);
   BOOST_CHECK_EQUAL(c, 0);
   c = a - -(b + static_cast<cast_type>(n4));
   BOOST_CHECK_EQUAL(c, 20 - -(30 + 50));
   c = -(b + static_cast<cast_type>(n4)) - a;
   BOOST_CHECK_EQUAL(c, -(30 + 50) - 20);
   c = a - -b;
   BOOST_CHECK_EQUAL(c, 50);
   c = -a - b;
   BOOST_CHECK_EQUAL(c, -50);
   c = -a - static_cast<cast_type>(n4);
   BOOST_CHECK_EQUAL(c, -20 - 50);
   c = static_cast<cast_type>(n4) - -a;
   BOOST_CHECK_EQUAL(c, 50 + 20);
   c = -(a + b) - Real(n4);
   BOOST_CHECK_EQUAL(c, -(20 + 30) - 50);
   c = static_cast<cast_type>(n4) - (a + b);
   BOOST_CHECK_EQUAL(c, 0);
   c = (a + b) * static_cast<cast_type>(n4);
   BOOST_CHECK_EQUAL(c, 50 * 50);
   c = static_cast<cast_type>(n4) * (a + b);
   BOOST_CHECK_EQUAL(c, 50 * 50);
   c = a * -(b + static_cast<cast_type>(n4));
   BOOST_CHECK_EQUAL(c, 20 * -(30 + 50));
   c = -(b + static_cast<cast_type>(n4)) * a;
   BOOST_CHECK_EQUAL(c, 20 * -(30 + 50));
   c = a * -b;
   BOOST_CHECK_EQUAL(c, 20 * -30);
   c = -a * b;
   BOOST_CHECK_EQUAL(c, 20 * -30);
   c = -a * static_cast<cast_type>(n4);
   BOOST_CHECK_EQUAL(c, -20 * 50);
   c = static_cast<cast_type>(n4) * -a;
   BOOST_CHECK_EQUAL(c, -20 * 50);
   c = -(a + b) + a;
   BOOST_CHECK(-50 + 20);
   c = static_cast<cast_type>(n4) - (a + b);
   BOOST_CHECK_EQUAL(c, 0);
   Real d = 10;
   c      = (a + b) / d;
   BOOST_CHECK_EQUAL(c, 5);
   c = (a + b) / (d + 0);
   BOOST_CHECK_EQUAL(c, 5);
   c = (a + b) / static_cast<cast_type>(n4);
   BOOST_CHECK_EQUAL(c, 1);
   c = static_cast<cast_type>(n4) / (a + b);
   BOOST_CHECK_EQUAL(c, 1);
   d = 50;
   c = d / -(a + b);
   BOOST_CHECK_EQUAL(c, -1);
   c = -(a + b) / d;
   BOOST_CHECK_EQUAL(c, -1);
   d = 2;
   c = a / -d;
   BOOST_CHECK_EQUAL(c, 20 / -2);
   c = -a / d;
   BOOST_CHECK_EQUAL(c, 20 / -2);
   d = 50;
   c = -d / static_cast<cast_type>(n4);
   BOOST_CHECK_EQUAL(c, -1);
   c = static_cast<cast_type>(n4) / -d;
   BOOST_CHECK_EQUAL(c, -1);
   c = static_cast<cast_type>(n4) + a;
   BOOST_CHECK_EQUAL(c, 70);
   c = static_cast<cast_type>(n4) - a;
   BOOST_CHECK_EQUAL(c, 30);
   c = static_cast<cast_type>(n4) * a;
   BOOST_CHECK_EQUAL(c, 50 * 20);

   n1 = -2;
   n2 = -3;
   n3 = -4;
   a  = static_cast<cast_type>(n1);
   b  = static_cast<cast_type>(n2);
   c  = static_cast<cast_type>(n3);
   d  = a + b * c;
   BOOST_CHECK_EQUAL(d, -2 + -3 * -4);
   d = static_cast<cast_type>(n1) + b * c;
   BOOST_CHECK_EQUAL(d, -2 + -3 * -4);
   d = a + static_cast<cast_type>(n2) * c;
   BOOST_CHECK_EQUAL(d, -2 + -3 * -4);
   d = a + b * static_cast<cast_type>(n3);
   BOOST_CHECK_EQUAL(d, -2 + -3 * -4);
   d = static_cast<cast_type>(n1) + static_cast<cast_type>(n2) * c;
   BOOST_CHECK_EQUAL(d, -2 + -3 * -4);
   d = static_cast<cast_type>(n1) + b * static_cast<cast_type>(n3);
   BOOST_CHECK_EQUAL(d, -2 + -3 * -4);
   a += static_cast<cast_type>(n2) * c;
   BOOST_CHECK_EQUAL(a, -2 + -3 * -4);
   a = static_cast<cast_type>(n1);
   a += b * static_cast<cast_type>(n3);
   BOOST_CHECK_EQUAL(a, -2 + -3 * -4);
   a = static_cast<cast_type>(n1);

   d = b * c + a;
   BOOST_CHECK_EQUAL(d, -2 + -3 * -4);
   d = b * c + static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, -2 + -3 * -4);
   d = static_cast<cast_type>(n2) * c + a;
   BOOST_CHECK_EQUAL(d, -2 + -3 * -4);
   d = b * static_cast<cast_type>(n3) + a;
   BOOST_CHECK_EQUAL(d, -2 + -3 * -4);
   d = static_cast<cast_type>(n2) * c + static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, -2 + -3 * -4);
   d = b * static_cast<cast_type>(n3) + static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, -2 + -3 * -4);

   a = -20;
   d = a - b * c;
   BOOST_CHECK_EQUAL(d, -20 - -3 * -4);
   n1 = -20;
   d  = static_cast<cast_type>(n1) - b * c;
   BOOST_CHECK_EQUAL(d, -20 - -3 * -4);
   d = a - static_cast<cast_type>(n2) * c;
   BOOST_CHECK_EQUAL(d, -20 - -3 * -4);
   d = a - b * static_cast<cast_type>(n3);
   BOOST_CHECK_EQUAL(d, -20 - -3 * -4);
   d = static_cast<cast_type>(n1) - static_cast<cast_type>(n2) * c;
   BOOST_CHECK_EQUAL(d, -20 - -3 * -4);
   d = static_cast<cast_type>(n1) - b * static_cast<cast_type>(n3);
   BOOST_CHECK_EQUAL(d, -20 - -3 * -4);
   a -= static_cast<cast_type>(n2) * c;
   BOOST_CHECK_EQUAL(a, -20 - -3 * -4);
   a = static_cast<cast_type>(n1);
   a -= b * static_cast<cast_type>(n3);
   BOOST_CHECK_EQUAL(a, -20 - -3 * -4);

   a = -2;
   d = b * c - a;
   BOOST_CHECK_EQUAL(d, -3 * -4 - -2);
   n1 = -2;
   d  = b * c - static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, -3 * -4 - -2);
   d = static_cast<cast_type>(n2) * c - a;
   BOOST_CHECK_EQUAL(d, -3 * -4 - -2);
   d = b * static_cast<cast_type>(n3) - a;
   BOOST_CHECK_EQUAL(d, -3 * -4 - -2);
   d = static_cast<cast_type>(n2) * c - static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, -3 * -4 - -2);
   d = b * static_cast<cast_type>(n3) - static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, -3 * -4 - -2);
   //
   // Conversion from min and max values:
   //
   test_negative_mixed_minmax<Real, Num>(std::integral_constant<bool, std::numeric_limits<Real>::is_integer && std::numeric_limits<Num>::is_integer > ());
   //
   // RValue ref overloads:
   //
   a  = 2;
   n1 = 3;
   d  = -a + static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, 1);
   d = static_cast<cast_type>(n1) + -a;
   BOOST_CHECK_EQUAL(d, 1);
   d = -a - static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, -5);
   d = static_cast<cast_type>(n1) - -a;
   BOOST_CHECK_EQUAL(d, 5);
   d = -a * static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, -6);
   d = static_cast<cast_type>(n1) * -a;
   BOOST_CHECK_EQUAL(d, -6);
   n1 = 4;
   d  = -static_cast<cast_type>(n1) / a;
   BOOST_CHECK_EQUAL(d, -2);
   d = static_cast<cast_type>(n1) / -a;
   BOOST_CHECK_EQUAL(d, -2);
}

template <class Real, class Num>
void test_negative_mixed(std::integral_constant<bool, false> const&)
{
}

template <class Real, class Num>
void test_mixed(const std::integral_constant<bool, false>&)
{
}

template <class Real>
inline bool check_is_nan(const Real& val, const std::integral_constant<bool, true>&)
{
   return (boost::math::isnan)(val);
}
template <class Real>
inline bool check_is_nan(const Real&, const std::integral_constant<bool, false>&)
{
   return false;
}
template <class Real>
inline Real negate_value(const Real& val, const std::integral_constant<bool, true>&)
{
   return -val;
}
template <class Real>
inline Real negate_value(const Real& val, const std::integral_constant<bool, false>&)
{
   return val;
}

template <class Real, class Num>
void test_mixed_numeric_limits(const std::integral_constant<bool, true>&)
{
   typedef typename lexical_cast_target_type<Num>::type target_type;
#if defined(TEST_MPFR)
   Num tol = 10 * std::numeric_limits<Num>::epsilon();
#else
   Num tol = 0;
#endif

   Real d;
   if (std::numeric_limits<Real>::has_infinity && std::numeric_limits<Num>::has_infinity)
   {
      d = static_cast<Real>(std::numeric_limits<Num>::infinity());
      BOOST_CHECK_GT(d, (std::numeric_limits<Real>::max)());
      d = static_cast<Real>(negate_value(std::numeric_limits<Num>::infinity(), std::integral_constant<bool, std::numeric_limits<Num>::is_signed>()));
      BOOST_CHECK_LT(d, negate_value((std::numeric_limits<Real>::max)(), std::integral_constant<bool, std::numeric_limits<Real>::is_signed>()));
   }
   if (std::numeric_limits<Real>::has_quiet_NaN && std::numeric_limits<Num>::has_quiet_NaN)
   {
      d = static_cast<Real>(std::numeric_limits<Num>::quiet_NaN());
      BOOST_CHECK(check_is_nan(d, std::integral_constant<bool, std::numeric_limits<Real>::has_quiet_NaN>()));
      d = static_cast<Real>(negate_value(std::numeric_limits<Num>::quiet_NaN(), std::integral_constant<bool, std::numeric_limits<Num>::is_signed>()));
      BOOST_CHECK(check_is_nan(d, std::integral_constant<bool, std::numeric_limits<Real>::has_quiet_NaN>()));
   }

   static const int left_shift = std::numeric_limits<Num>::digits - 1;
   Num              n1         = static_cast<Num>(1uLL << ((left_shift < 63) && (left_shift > 0) ? left_shift : 10));
   Num              n2         = 1;
   Num              n3         = 0;
   Num              n4         = 20;

   std::ios_base::fmtflags f               = std::is_floating_point<Num>::value ? std::ios_base::scientific : std::ios_base::fmtflags(0);
   int                     digits_to_print = std::is_floating_point<Num>::value && std::numeric_limits<Num>::is_specialized
                             ? std::numeric_limits<Num>::digits10 + 5
                             : 0;
   if (std::numeric_limits<target_type>::digits <= std::numeric_limits<Real>::digits)
   {
      BOOST_CHECK_CLOSE(n1, checked_lexical_cast<target_type>(Real(n1).str(digits_to_print, f)), tol);
   }
   BOOST_CHECK_CLOSE(n2, checked_lexical_cast<target_type>(Real(n2).str(digits_to_print, f)), 0);
   BOOST_CHECK_CLOSE(n3, checked_lexical_cast<target_type>(Real(n3).str(digits_to_print, f)), 0);
   BOOST_CHECK_CLOSE(n4, checked_lexical_cast<target_type>(Real(n4).str(digits_to_print, f)), 0);
}
template <class Real, class Num>
void test_mixed_numeric_limits(const std::integral_constant<bool, false>&)
{
}

template <class Real, class Num>
void test_mixed(const std::integral_constant<bool, true>&)
{
   typedef typename std::conditional<
       std::is_convertible<Num, Real>::value,
       typename std::conditional<boost::multiprecision::detail::is_integral<Num>::value && (sizeof(Num) < sizeof(int)), int, Num>::type,
       Real>::type cast_type;
   typedef typename std::conditional<
       std::is_convertible<Num, Real>::value,
       Num,
       Real>::type simple_cast_type;

   if (std::numeric_limits<Real>::is_specialized && std::numeric_limits<Real>::is_bounded && std::numeric_limits<Real>::digits < std::numeric_limits<Num>::digits)
      return;

   std::cout << "Testing mixed arithmetic with type: " << typeid(Real).name() << " and " << typeid(Num).name() << std::endl;
   static const int left_shift = std::numeric_limits<Num>::digits - 1;
   Num              n1         = static_cast<Num>(1uLL << ((left_shift < 63) && (left_shift > 0) ? left_shift : 10));
   Num              n2         = 1;
   Num              n3         = 0;
   Num              n4         = 20;
   Num              n5         = 8;

   test_comparisons<Real>(n1, n2, std::is_convertible<Num, Real>());
   test_comparisons<Real>(n1, n3, std::is_convertible<Num, Real>());
   test_comparisons<Real>(n1, n1, std::is_convertible<Num, Real>());
   test_comparisons<Real>(n3, n1, std::is_convertible<Num, Real>());
   test_comparisons<Real>(n2, n1, std::is_convertible<Num, Real>());
   test_comparisons<Real>(n3, n3, std::is_convertible<Num, Real>());

   // Default construct:
   BOOST_CHECK_EQUAL(Real(n1), static_cast<cast_type>(n1));
   BOOST_CHECK_EQUAL(Real(n2), static_cast<cast_type>(n2));
   BOOST_CHECK_EQUAL(Real(n3), static_cast<cast_type>(n3));
   BOOST_CHECK_EQUAL(Real(n4), static_cast<cast_type>(n4));
   BOOST_CHECK_EQUAL(Real(n1).template convert_to<Num>(), n1);
   BOOST_CHECK_EQUAL(Real(n2).template convert_to<Num>(), n2);
   BOOST_CHECK_EQUAL(Real(n3).template convert_to<Num>(), n3);
   BOOST_CHECK_EQUAL(Real(n4).template convert_to<Num>(), n4);
   BOOST_CHECK_EQUAL(static_cast<Num>(Real(n1)), n1);
   BOOST_CHECK_EQUAL(static_cast<Num>(Real(n2)), n2);
   BOOST_CHECK_EQUAL(static_cast<Num>(Real(n3)), n3);
   BOOST_CHECK_EQUAL(static_cast<Num>(Real(n4)), n4);
   // Again with expression templates:
   BOOST_CHECK_EQUAL((Real(n1) + 0).template convert_to<Num>(), n1);
   BOOST_CHECK_EQUAL((Real(n2) + 0).template convert_to<Num>(), n2);
   BOOST_CHECK_EQUAL((Real(n3) + 0).template convert_to<Num>(), n3);
   BOOST_CHECK_EQUAL((Real(n4) + 0).template convert_to<Num>(), n4);
   BOOST_CHECK_EQUAL(static_cast<Num>(Real(n1) + 0), n1);
   BOOST_CHECK_EQUAL(static_cast<Num>(Real(n2) + 0), n2);
   BOOST_CHECK_EQUAL(static_cast<Num>(Real(n3) + 0), n3);
   BOOST_CHECK_EQUAL(static_cast<Num>(Real(n4) + 0), n4);
   BOOST_CHECK_EQUAL(static_cast<cast_type>(n1), Real(n1));
   BOOST_CHECK_EQUAL(static_cast<cast_type>(n2), Real(n2));
   BOOST_CHECK_EQUAL(static_cast<cast_type>(n3), Real(n3));
   BOOST_CHECK_EQUAL(static_cast<cast_type>(n4), Real(n4));
   // Assignment:
   Real r(0);
   BOOST_CHECK(r != static_cast<cast_type>(n1));
   r = static_cast<simple_cast_type>(n1);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n1));
   r = static_cast<simple_cast_type>(n2);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n2));
   r = static_cast<simple_cast_type>(n3);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n3));
   r = static_cast<simple_cast_type>(n4);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n4));
   // Addition:
   r = static_cast<simple_cast_type>(n2);
   BOOST_CHECK_EQUAL(r + static_cast<simple_cast_type>(n4), static_cast<cast_type>(n2 + n4));
   BOOST_CHECK_EQUAL(Real(r + static_cast<simple_cast_type>(n4)), static_cast<cast_type>(n2 + n4));
   r += static_cast<simple_cast_type>(n4);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n2 + n4));
   // subtraction:
   r = static_cast<simple_cast_type>(n4);
   BOOST_CHECK_EQUAL(r - static_cast<simple_cast_type>(n5), static_cast<cast_type>(n4 - n5));
   BOOST_CHECK_EQUAL(Real(r - static_cast<simple_cast_type>(n5)), static_cast<cast_type>(n4 - n5));
   r -= static_cast<simple_cast_type>(n5);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n4 - n5));
   // Multiplication:
   r = static_cast<simple_cast_type>(n2);
   BOOST_CHECK_EQUAL(r * static_cast<simple_cast_type>(n4), static_cast<cast_type>(n2 * n4));
   BOOST_CHECK_EQUAL(Real(r * static_cast<simple_cast_type>(n4)), static_cast<cast_type>(n2 * n4));
   r *= static_cast<simple_cast_type>(n4);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n2 * n4));
   // Division:
   r = static_cast<simple_cast_type>(n1);
   BOOST_CHECK_EQUAL(r / static_cast<simple_cast_type>(n5), static_cast<cast_type>(n1 / n5));
   BOOST_CHECK_EQUAL(Real(r / static_cast<simple_cast_type>(n5)), static_cast<cast_type>(n1 / n5));
   r /= static_cast<simple_cast_type>(n5);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n1 / n5));
   //
   // special cases for full coverage:
   //
   r = static_cast<simple_cast_type>(n5) + Real(n4);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n4 + n5));
   r = static_cast<simple_cast_type>(n4) - Real(n5);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n4 - n5));
   r = static_cast<simple_cast_type>(n4) * Real(n5);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n4 * n5));
   r = static_cast<cast_type>(Num(4) * n4) / Real(4);
   BOOST_CHECK_EQUAL(r, static_cast<cast_type>(n4));

   typedef std::integral_constant<bool, 
       (!std::numeric_limits<Num>::is_specialized || std::numeric_limits<Num>::is_signed) && (!std::numeric_limits<Real>::is_specialized || std::numeric_limits<Real>::is_signed)>
       signed_tag;

   test_negative_mixed<Real, Num>(signed_tag());

   n1 = 2;
   n2 = 3;
   n3 = 4;
   Real a(n1), b(n2), c(n3), d;
   d = a + b * c;
   BOOST_CHECK_EQUAL(d, 2 + 3 * 4);
   d = static_cast<cast_type>(n1) + b * c;
   BOOST_CHECK_EQUAL(d, 2 + 3 * 4);
   d = a + static_cast<cast_type>(n2) * c;
   BOOST_CHECK_EQUAL(d, 2 + 3 * 4);
   d = a + b * static_cast<cast_type>(n3);
   BOOST_CHECK_EQUAL(d, 2 + 3 * 4);
   d = static_cast<cast_type>(n1) + static_cast<cast_type>(n2) * c;
   BOOST_CHECK_EQUAL(d, 2 + 3 * 4);
   d = static_cast<cast_type>(n1) + b * static_cast<cast_type>(n3);
   BOOST_CHECK_EQUAL(d, 2 + 3 * 4);
   a += static_cast<cast_type>(n2) * c;
   BOOST_CHECK_EQUAL(a, 2 + 3 * 4);
   a = static_cast<cast_type>(n1);
   a += b * static_cast<cast_type>(n3);
   BOOST_CHECK_EQUAL(a, 2 + 3 * 4);
   a = static_cast<cast_type>(n1);

   d = b * c + a;
   BOOST_CHECK_EQUAL(d, 2 + 3 * 4);
   d = b * c + static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, 2 + 3 * 4);
   d = static_cast<cast_type>(n2) * c + a;
   BOOST_CHECK_EQUAL(d, 2 + 3 * 4);
   d = b * static_cast<cast_type>(n3) + a;
   BOOST_CHECK_EQUAL(d, 2 + 3 * 4);
   d = static_cast<cast_type>(n2) * c + static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, 2 + 3 * 4);
   d = b * static_cast<cast_type>(n3) + static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, 2 + 3 * 4);

   a = 20;
   d = a - b * c;
   BOOST_CHECK_EQUAL(d, 20 - 3 * 4);
   n1 = 20;
   d  = static_cast<cast_type>(n1) - b * c;
   BOOST_CHECK_EQUAL(d, 20 - 3 * 4);
   d = a - static_cast<cast_type>(n2) * c;
   BOOST_CHECK_EQUAL(d, 20 - 3 * 4);
   d = a - b * static_cast<cast_type>(n3);
   BOOST_CHECK_EQUAL(d, 20 - 3 * 4);
   d = static_cast<cast_type>(n1) - static_cast<cast_type>(n2) * c;
   BOOST_CHECK_EQUAL(d, 20 - 3 * 4);
   d = static_cast<cast_type>(n1) - b * static_cast<cast_type>(n3);
   BOOST_CHECK_EQUAL(d, 20 - 3 * 4);
   a -= static_cast<cast_type>(n2) * c;
   BOOST_CHECK_EQUAL(a, 20 - 3 * 4);
   a = static_cast<cast_type>(n1);
   a -= b * static_cast<cast_type>(n3);
   BOOST_CHECK_EQUAL(a, 20 - 3 * 4);

   a = 2;
   d = b * c - a;
   BOOST_CHECK_EQUAL(d, 3 * 4 - 2);
   n1 = 2;
   d  = b * c - static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, 3 * 4 - 2);
   d = static_cast<cast_type>(n2) * c - a;
   BOOST_CHECK_EQUAL(d, 3 * 4 - 2);
   d = b * static_cast<cast_type>(n3) - a;
   BOOST_CHECK_EQUAL(d, 3 * 4 - a);
   d = static_cast<cast_type>(n2) * c - static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, 3 * 4 - 2);
   d = b * static_cast<cast_type>(n3) - static_cast<cast_type>(n1);
   BOOST_CHECK_EQUAL(d, 3 * 4 - 2);

   test_mixed_numeric_limits<Real, Num>(std::integral_constant<bool, std::numeric_limits<Real>::is_specialized>());
}

template <class Real>
typename std::enable_if<boost::multiprecision::number_category<Real>::value == boost::multiprecision::number_kind_complex>::type test_members(Real)
{
   //
   // Test sign and zero functions:
   //
   Real a = 20;
   Real b = 30;
   BOOST_CHECK(!a.is_zero());
   a = -20;
   BOOST_CHECK(!a.is_zero());
   a = 0;
   BOOST_CHECK(a.is_zero());

   a = 20;
   b = 30;
   a.swap(b);
   BOOST_CHECK_EQUAL(a, 30);
   BOOST_CHECK_EQUAL(b, 20);

   Real c(2, 3);

   BOOST_CHECK_EQUAL(a.real(), 30);
   BOOST_CHECK_EQUAL(a.imag(), 0);
   BOOST_CHECK_EQUAL(c.real(), 2);
   BOOST_CHECK_EQUAL(c.imag(), 3);

   //
   // try some more 2-argument constructors:
   //
   {
      Real d(40.5, 2);
      BOOST_CHECK_EQUAL(d.real(), 40.5);
      BOOST_CHECK_EQUAL(d.imag(), 2);
   }
   {
      Real d("40.5", "2");
      BOOST_CHECK_EQUAL(d.real(), 40.5);
      BOOST_CHECK_EQUAL(d.imag(), 2);
   }
   {
      Real d("40.5", std::string("2"));
      BOOST_CHECK_EQUAL(d.real(), 40.5);
      BOOST_CHECK_EQUAL(d.imag(), 2);
   }
#ifndef BOOST_NO_CXX17_HDR_STRING_VIEW
   {
      std::string      sx("40.550"), sy("222");
      std::string_view vx(sx.c_str(), 4), vy(sy.c_str(), 1);
      Real             d(vx, vy);
      BOOST_CHECK_EQUAL(d.real(), 40.5);
      BOOST_CHECK_EQUAL(d.imag(), 2);
   }
#endif
   {
      typename Real::value_type x(40.5), y(2);
      Real                      d(x, y);
      BOOST_CHECK_EQUAL(d.real(), 40.5);
      BOOST_CHECK_EQUAL(d.imag(), 2);
   }
#ifdef TEST_MPC
   {
      typename Real::value_type x(40.5), y(2);
      Real                      d(x.backend().data(), y.backend().data());
      BOOST_CHECK_EQUAL(d.real(), 40.5);
      BOOST_CHECK_EQUAL(d.imag(), 2);
   }
#endif
   {
      typename Real::value_type x(40.5);
      Real                      d(x, 2);
      BOOST_CHECK_EQUAL(d.real(), 40.5);
      BOOST_CHECK_EQUAL(d.imag(), 2);
   }
   {
      typename Real::value_type x(40.5);
      Real                      d(2, x);
      BOOST_CHECK_EQUAL(d.imag(), 40.5);
      BOOST_CHECK_EQUAL(d.real(), 2);
   }
   {
      typename Real::value_type x(real(a) * real(b) + imag(a) * imag(b)), y(imag(a) * real(b) - real(a) * imag(b));
      Real                      d(real(a) * real(b) + imag(a) * imag(b), imag(a) * real(b) - real(a) * imag(b));
      Real                      e(x, y);
      BOOST_CHECK_EQUAL(d, e);
   }
   //
   // real and imag setters:
   //
   c.real(4);
   BOOST_CHECK_EQUAL(real(c), 4);
   c.imag(-55);
   BOOST_CHECK_EQUAL(imag(c), -55);
   typename Real::value_type z(20);
   c.real(z);
   BOOST_CHECK_EQUAL(real(c), 20);
   c.real(21L);
   BOOST_CHECK_EQUAL(real(c), 21);
   c.real(22L);
   BOOST_CHECK_EQUAL(real(c), 22);
   c.real(23UL);
   BOOST_CHECK_EQUAL(real(c), 23);
   c.real(24U);
   BOOST_CHECK_EQUAL(real(c), 24);
   c.real(25.0f);
   BOOST_CHECK_EQUAL(real(c), 25);
   c.real(26.0);
   BOOST_CHECK_EQUAL(real(c), 26);
   c.real(27.0L);
   BOOST_CHECK_EQUAL(real(c), 27);
#if defined(BOOST_HAS_LONG_LONG)
   c.real(28LL);
   BOOST_CHECK_EQUAL(real(c), 28);
   c.real(29ULL);
   BOOST_CHECK_EQUAL(real(c), 29);
#endif
   c.imag(z);
   BOOST_CHECK_EQUAL(imag(c), 20);
   c.imag(21L);
   BOOST_CHECK_EQUAL(imag(c), 21);
   c.imag(22L);
   BOOST_CHECK_EQUAL(imag(c), 22);
   c.imag(23UL);
   BOOST_CHECK_EQUAL(imag(c), 23);
   c.imag(24U);
   BOOST_CHECK_EQUAL(imag(c), 24);
   c.imag(25.0f);
   BOOST_CHECK_EQUAL(imag(c), 25);
   c.imag(26.0);
   BOOST_CHECK_EQUAL(imag(c), 26);
   c.imag(27.0L);
   BOOST_CHECK_EQUAL(imag(c), 27);
#if defined(BOOST_HAS_LONG_LONG)
   c.imag(28LL);
   BOOST_CHECK_EQUAL(imag(c), 28);
   c.imag(29ULL);
   BOOST_CHECK_EQUAL(imag(c), 29);
#endif

   c.real(2).imag(3);

   BOOST_CHECK_EQUAL(real(a), 30);
   BOOST_CHECK_EQUAL(imag(a), 0);
   BOOST_CHECK_EQUAL(real(c), 2);
   BOOST_CHECK_EQUAL(imag(c), 3);
   BOOST_CHECK_EQUAL(real(a + 0), 30);
   BOOST_CHECK_EQUAL(imag(a + 0), 0);
   BOOST_CHECK_EQUAL(real(c + 0), 2);
   BOOST_CHECK_EQUAL(imag(c + 0), 3);

   // string construction:
   a = Real("2");
   BOOST_CHECK_EQUAL(real(a), 2);
   BOOST_CHECK_EQUAL(imag(a), 0);
   a = Real("(2)");
   BOOST_CHECK_EQUAL(real(a), 2);
   BOOST_CHECK_EQUAL(imag(a), 0);
   a = Real("(,2)");
   BOOST_CHECK_EQUAL(real(a), 0);
   BOOST_CHECK_EQUAL(imag(a), 2);
   a = Real("(2,3)");
   BOOST_CHECK_EQUAL(real(a), 2);
   BOOST_CHECK_EQUAL(imag(a), 3);

   typedef typename boost::multiprecision::component_type<Real>::type real_type;

   real_type r(3);
   real_type tol = std::numeric_limits<real_type>::epsilon() * 30;

   a = r;
   BOOST_CHECK_EQUAL(real(a), 3);
   BOOST_CHECK_EQUAL(imag(a), 0);

   a += r;
   BOOST_CHECK_EQUAL(real(a), 6);
   BOOST_CHECK_EQUAL(imag(a), 0);

   a *= r;
   BOOST_CHECK_EQUAL(real(a), 18);
   BOOST_CHECK_EQUAL(imag(a), 0);

   a = a / r;
   BOOST_CHECK_EQUAL(real(a), 6);
   BOOST_CHECK_EQUAL(imag(a), 0);
   a = a - r;
   BOOST_CHECK_EQUAL(real(a), 3);
   BOOST_CHECK_EQUAL(imag(a), 0);
   a = r + a;
   BOOST_CHECK_EQUAL(real(a), 6);
   BOOST_CHECK_EQUAL(imag(a), 0);

   r = abs(c);
   BOOST_CHECK_CLOSE_FRACTION(real_type("3.60555127546398929311922126747049594625129657384524621271045305622716694829301044520461908201849071767351418202406"), r, tol);
   r = arg(c);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.98279372324732906798571061101466601449687745363162855676142508831798807154979603538970653437281731110816513970201"), r, tol);
   r = norm(c);
   BOOST_CHECK_CLOSE_FRACTION(real_type(13), r, tol);
   a = conj(c);
   BOOST_CHECK_EQUAL(real(a), 2);
   BOOST_CHECK_EQUAL(imag(a), -3);
   a = proj(c);
   BOOST_CHECK_EQUAL(real(a), 2);
   BOOST_CHECK_EQUAL(imag(a), 3);
   a = polar(real_type(3), real_type(-10));
   BOOST_CHECK_CLOSE_FRACTION(real_type("-2.517214587229357356776591843472194503559790495399505640507861193146377760598812305202801138281266416782353163216"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.63206333266810944021424298555413184505092903874867167472255203785027162892148027712122702168494964847488147271478"), imag(a), tol);
   a = polar(real_type(3) + 0, real_type(-10));
   BOOST_CHECK_CLOSE_FRACTION(real_type("-2.517214587229357356776591843472194503559790495399505640507861193146377760598812305202801138281266416782353163216"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.63206333266810944021424298555413184505092903874867167472255203785027162892148027712122702168494964847488147271478"), imag(a), tol);
   a = polar(real_type(3), real_type(-10) + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-2.517214587229357356776591843472194503559790495399505640507861193146377760598812305202801138281266416782353163216"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.63206333266810944021424298555413184505092903874867167472255203785027162892148027712122702168494964847488147271478"), imag(a), tol);
   a = polar(real_type(3) + 0, real_type(-10) + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-2.517214587229357356776591843472194503559790495399505640507861193146377760598812305202801138281266416782353163216"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.63206333266810944021424298555413184505092903874867167472255203785027162892148027712122702168494964847488147271478"), imag(a), tol);
   a = polar(3, real_type(-10));
   BOOST_CHECK_CLOSE_FRACTION(real_type("-2.517214587229357356776591843472194503559790495399505640507861193146377760598812305202801138281266416782353163216"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.63206333266810944021424298555413184505092903874867167472255203785027162892148027712122702168494964847488147271478"), imag(a), tol);
   a = polar(3.0, real_type(-10) + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-2.517214587229357356776591843472194503559790495399505640507861193146377760598812305202801138281266416782353163216"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.63206333266810944021424298555413184505092903874867167472255203785027162892148027712122702168494964847488147271478"), imag(a), tol);

   a = polar(real_type(3));
   BOOST_CHECK_EQUAL(3, real(a));
   BOOST_CHECK_EQUAL(0, imag(a));
   a = polar(real_type(3) + 0);
   BOOST_CHECK_EQUAL(3, real(a));
   BOOST_CHECK_EQUAL(0, imag(a));

   r = abs(c + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("3.60555127546398929311922126747049594625129657384524621271045305622716694829301044520461908201849071767351418202406"), r, tol);
   r = arg(c + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.98279372324732906798571061101466601449687745363162855676142508831798807154979603538970653437281731110816513970201"), r, tol);
   r = norm(c + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type(13), r, tol);
   a = conj(c + 0);
   BOOST_CHECK_EQUAL(real(a), 2);
   BOOST_CHECK_EQUAL(imag(a), -3);
   a = proj(c + 0);
   BOOST_CHECK_EQUAL(real(a), 2);
   BOOST_CHECK_EQUAL(imag(a), 3);

   a = exp(c);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-7.3151100949011025174865361510507893218698794489446322367845159660828327860599907104337742108443234172141249777"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.0427436562359044141015039404625521939183300604422348975424523449538886779880818796291971422701951470533151185"), imag(a), tol);

   a = log(c);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.282474678730768368026743720782659302402633972380103558209522755331732333662205089699787331720244744384629096046"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.9827937232473290679857106110146660144968774536316285567614250883179880715497960353897065343728173111081651397020"), imag(a), tol);

   a = log10(c);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.556971676153418384603252578971164215414864594193534135900595487498776545815097120403823727129449829836488977743"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.426821890855466638944275673291166123449562356934437957244904971730668088711719757900679614536803436424488603794"), imag(a), tol);

   a = exp(c + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-7.3151100949011025174865361510507893218698794489446322367845159660828327860599907104337742108443234172141249777"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.0427436562359044141015039404625521939183300604422348975424523449538886779880818796291971422701951470533151185"), imag(a), tol);

   a = log(c + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.282474678730768368026743720782659302402633972380103558209522755331732333662205089699787331720244744384629096046"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.9827937232473290679857106110146660144968774536316285567614250883179880715497960353897065343728173111081651397020"), imag(a), tol);

   a = log10(c + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.556971676153418384603252578971164215414864594193534135900595487498776545815097120403823727129449829836488977743"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.426821890855466638944275673291166123449562356934437957244904971730668088711719757900679614536803436424488603794"), imag(a), tol);

   // Powers where one arg is an integer.
   b = Real(5, -2);
   a = pow(c, b);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-3053.8558566606567369633610140423321260211388217942246293871310470377722279440084474789529228008638668934381183"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("3097.9975862915005132449772136982559285192410496951232473245540634244845290672745578327467396750607773968246915"), imag(a), tol);
   a = pow(c, 3);
   BOOST_CHECK_CLOSE_FRACTION(real_type(-46), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type(9), imag(a), tol);
   a = pow(3, c);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-8.8931513442797186948734782808862447235385767991868219480917324534839621090167050538805196124711247247992169338"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-1.3826999557878897572499699021550296885662132089951379549068064961882821777067532977546360861176011175070188118"), imag(a), tol * 3);
   a = pow(c + 0, b);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-3053.8558566606567369633610140423321260211388217942246293871310470377722279440084474789529228008638668934381183"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("3097.9975862915005132449772136982559285192410496951232473245540634244845290672745578327467396750607773968246915"), imag(a), tol);
   a = pow(c + 0, 3);
   BOOST_CHECK_CLOSE_FRACTION(real_type(-46), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type(9), imag(a), tol);
   a = pow(3, c + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-8.8931513442797186948734782808862447235385767991868219480917324534839621090167050538805196124711247247992169338"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-1.3826999557878897572499699021550296885662132089951379549068064961882821777067532977546360861176011175070188118"), imag(a), tol * 3);

   r = 3;
   // Powers where one arg is a real_type.
   a = pow(c, r);
   BOOST_CHECK_CLOSE_FRACTION(real_type(-46), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type(9), imag(a), tol);
   a = pow(r, c);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-8.8931513442797186948734782808862447235385767991868219480917324534839621090167050538805196124711247247992169338"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-1.3826999557878897572499699021550296885662132089951379549068064961882821777067532977546360861176011175070188118"), imag(a), tol * 3);
   a = pow(c + 0, r);
   BOOST_CHECK_CLOSE_FRACTION(real_type(-46), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type(9), imag(a), tol);
   a = pow(r, c + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-8.8931513442797186948734782808862447235385767991868219480917324534839621090167050538805196124711247247992169338"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-1.3826999557878897572499699021550296885662132089951379549068064961882821777067532977546360861176011175070188118"), imag(a), tol * 3);
   a = pow(c, r + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type(-46), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type(9), imag(a), tol);
   a = pow(r + 0, c);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-8.8931513442797186948734782808862447235385767991868219480917324534839621090167050538805196124711247247992169338"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-1.3826999557878897572499699021550296885662132089951379549068064961882821777067532977546360861176011175070188118"), imag(a), tol * 3);

   // Powers where one arg is an float.
   a = pow(c, 3.0);
   BOOST_CHECK_CLOSE_FRACTION(real_type(-46), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type(9), imag(a), tol);
   a = pow(3.0, c);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-8.8931513442797186948734782808862447235385767991868219480917324534839621090167050538805196124711247247992169338"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-1.3826999557878897572499699021550296885662132089951379549068064961882821777067532977546360861176011175070188118"), imag(a), tol * 3);
   a = pow(c + 0, 3.0);
   BOOST_CHECK_CLOSE_FRACTION(real_type(-46), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type(9), imag(a), tol);
   a = pow(3.0, c + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-8.8931513442797186948734782808862447235385767991868219480917324534839621090167050538805196124711247247992169338"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-1.3826999557878897572499699021550296885662132089951379549068064961882821777067532977546360861176011175070188118"), imag(a), tol * 3);

   a = c;
   a = sqrt(a);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.674149228035540040448039300849051821674708677883920366727287836003399240343274891876712629708287692163156802065"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.8959774761298381247157337552900434410433241995549314932449006989874470582160955817053273057885402621549320588976"), imag(a), tol);
   a = c;
   a = sin(a);
   BOOST_CHECK_CLOSE_FRACTION(real_type("9.154499146911429573467299544609832559158860568765182977899828142590020335321896403936690014669532606510294425039"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-4.168906959966564350754813058853754843573565604758055889965478710592666260138453299795649308385497563475115931624"), imag(a), tol);
   a = c;
   a = cos(a);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-4.1896256909688072301325550196159737286219454041279210357407905058369727912162626993926269783331491034500484583"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-9.1092278937553365979791972627788621213326202389201695649104967309554222940748568716960841549279996556547993373"), imag(a), tol);
   a = c;
   a = tan(a);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-0.0037640256415042482927512211303226908396306202016580864328644932511249097100916559688254811519914564480500042311"), real(a), tol * 5);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.0032386273536098014463585978219272598077897241071003399272426939850671219193120708438426543945017427085738411"), imag(a), tol);
   a = c;
   a = asin(a);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.5706527843210994007102838796856696501828032450960401365302732598209740064262509342420347149436326252483895113827"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.983387029916535432347076902894039565014248302909345356125267430944752731616095111727103650117987412058949254132"), imag(a), tol);
   a = c;
   a = acos(a);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.000143542473797218521037811954081791915781454591512773957199036332934196716853565071982697727425908742684531873"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-1.983387029916535432347076902894039565014248302909345356125267430944752731616095111727103650117987412058949254132"), imag(a), tol);
   a = c;
   a = atan(a);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.409921049596575522530619384460420782588207051908724814771070766475530084440199227135813201495737846771570458568"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.2290726829685387662958818029420027678625253049770656169479919704951963414344907622560676377741902308144912055002"), imag(a), tol);
   a = c;
   a = sqrt(a + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.674149228035540040448039300849051821674708677883920366727287836003399240343274891876712629708287692163156802065"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.8959774761298381247157337552900434410433241995549314932449006989874470582160955817053273057885402621549320588976"), imag(a), tol);
   a = c;
   a = sin(a + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("9.154499146911429573467299544609832559158860568765182977899828142590020335321896403936690014669532606510294425039"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-4.168906959966564350754813058853754843573565604758055889965478710592666260138453299795649308385497563475115931624"), imag(a), tol);
   a = c;
   a = cos(a + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-4.1896256909688072301325550196159737286219454041279210357407905058369727912162626993926269783331491034500484583"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-9.1092278937553365979791972627788621213326202389201695649104967309554222940748568716960841549279996556547993373"), imag(a), tol);
   a = c;
   a = tan(a + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-0.0037640256415042482927512211303226908396306202016580864328644932511249097100916559688254811519914564480500042311"), real(a), tol * 5);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.0032386273536098014463585978219272598077897241071003399272426939850671219193120708438426543945017427085738411"), imag(a), tol);
   a = c;
   a = asin(a + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.5706527843210994007102838796856696501828032450960401365302732598209740064262509342420347149436326252483895113827"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.983387029916535432347076902894039565014248302909345356125267430944752731616095111727103650117987412058949254132"), imag(a), tol);
   a = c;
   a = acos(a + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.000143542473797218521037811954081791915781454591512773957199036332934196716853565071982697727425908742684531873"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-1.983387029916535432347076902894039565014248302909345356125267430944752731616095111727103650117987412058949254132"), imag(a), tol);
   a = c;
   a = atan(a + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.409921049596575522530619384460420782588207051908724814771070766475530084440199227135813201495737846771570458568"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.2290726829685387662958818029420027678625253049770656169479919704951963414344907622560676377741902308144912055002"), imag(a), tol);

   a = c;
   a = sinh(a);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-3.5905645899857799520125654477948167931949136757293015099986213974178826801534614215227593814301490087307920223"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.53092108624851980526704009066067655967277345095149103008706855371803528753067068552935673000832252607835087747"), imag(a), tol);
   a = c;
   a = cosh(a);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-3.7245455049153225654739707032559725286749657732153307267858945686649501059065292889110148294141744084833329553"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.51182256998738460883446384980187563424555660949074386745538379123585339045741119409984041226187262097496424111"), imag(a), tol);
   a = c;
   a = tanh(a);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.965385879022133124278480269394560685879729650005757773636908240066639772853967550095754361348005358178253777920"), real(a), tol * 5);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-0.00988437503832249372031403430350121097961813353467039031861010606115560355679254344335582852193041894874685555114"), imag(a), tol);
   a = c;
   a = asinh(a);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.968637925793096291788665095245498189520731012682010573842811017352748255492485345887875752070076230641308014923"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.9646585044076027920454110594995323555197773725073316527132580297155508786089335572049608301897631767195194427315"), imag(a), tol);
   a = c;
   a = acosh(a);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.983387029916535432347076902894039565014248302909345356125267430944752731616095111727103650117987412058949254132"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.000143542473797218521037811954081791915781454591512773957199036332934196716853565071982697727425908742684531873"), imag(a), tol);
   a = c;
   a = atanh(a);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.1469466662255297520474327851547159424423449403442452953891851939502023996823900422792744078835711416939934387775"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.338972522294493561124193575909144241084316172544492778582005751793809271060233646663717270678614587712809117131"), imag(a), tol);
   a = c;
   a = sinh(a + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-3.5905645899857799520125654477948167931949136757293015099986213974178826801534614215227593814301490087307920223"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.53092108624851980526704009066067655967277345095149103008706855371803528753067068552935673000832252607835087747"), imag(a), tol);
   a = c;
   a = cosh(a + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-3.7245455049153225654739707032559725286749657732153307267858945686649501059065292889110148294141744084833329553"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.51182256998738460883446384980187563424555660949074386745538379123585339045741119409984041226187262097496424111"), imag(a), tol);
   a = c;
   a = tanh(a + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.965385879022133124278480269394560685879729650005757773636908240066639772853967550095754361348005358178253777920"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("-0.00988437503832249372031403430350121097961813353467039031861010606115560355679254344335582852193041894874685555114"), imag(a), tol);
   a = c;
   a = asinh(a + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.968637925793096291788665095245498189520731012682010573842811017352748255492485345887875752070076230641308014923"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.9646585044076027920454110594995323555197773725073316527132580297155508786089335572049608301897631767195194427315"), imag(a), tol);
   a = c;
   a = acosh(a + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.983387029916535432347076902894039565014248302909345356125267430944752731616095111727103650117987412058949254132"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.000143542473797218521037811954081791915781454591512773957199036332934196716853565071982697727425908742684531873"), imag(a), tol);
   a = c;
   a = atanh(a + 0);
   BOOST_CHECK_CLOSE_FRACTION(real_type("0.1469466662255297520474327851547159424423449403442452953891851939502023996823900422792744078835711416939934387775"), real(a), tol);
   BOOST_CHECK_CLOSE_FRACTION(real_type("1.338972522294493561124193575909144241084316172544492778582005751793809271060233646663717270678614587712809117131"), imag(a), tol);
}

template <class Real>
typename std::enable_if<boost::multiprecision::number_category<Real>::value != boost::multiprecision::number_kind_complex>::type test_members(Real)
{
   //
   // Test sign and zero functions:
   //
   Real a = 20;
   Real b = 30;
   BOOST_CHECK(a.sign() > 0);
   BOOST_CHECK(!a.is_zero());
   if (std::numeric_limits<Real>::is_signed)
   {
      a = -20;
      BOOST_CHECK(a.sign() < 0);
      BOOST_CHECK(!a.is_zero());
   }
   a = 0;
   BOOST_CHECK_EQUAL(a.sign(), 0);
   BOOST_CHECK(a.is_zero());

   a = 20;
   b = 30;
   a.swap(b);
   BOOST_CHECK_EQUAL(a, 30);
   BOOST_CHECK_EQUAL(b, 20);
   //
   // Test complex number functions which are also overloaded for scalar type:
   //
   BOOST_CHECK_EQUAL(real(a), a);
   BOOST_CHECK_EQUAL(imag(a), 0);
   BOOST_CHECK_EQUAL(real(a + 0), a);
   BOOST_CHECK_EQUAL(imag(a + 2), 0);
   BOOST_CHECK_EQUAL(norm(a), a * a);
   BOOST_CHECK_EQUAL(norm(a * 1), a * a);
   BOOST_CHECK_EQUAL(conj(a), a);
   BOOST_CHECK_EQUAL(conj(a * 1), a);
   BOOST_CHECK_EQUAL(proj(a), a);
   BOOST_CHECK_EQUAL(proj(a * 1), a);
   BOOST_CHECK_EQUAL(a.real(), a);
   BOOST_CHECK_EQUAL(a.imag(), 0);
   a.real(55);
   BOOST_CHECK_EQUAL(a, 55);
}

template <class Real>
void test_members(boost::rational<Real>)
{
}

template <class Real>
void test_signed_ops(const std::integral_constant<bool, true>&)
{
   Real a(8);
   Real b(64);
   Real c(500);
   Real d(1024);
   Real ac;
   BOOST_CHECK_EQUAL(-a, -8);
   ac = a;
   ac = ac - b;
   BOOST_CHECK_EQUAL(ac, 8 - 64);
   ac = a;
   ac -= a + b;
   BOOST_CHECK_EQUAL(ac, -64);
   ac = a;
   ac -= b - a;
   BOOST_CHECK_EQUAL(ac, 16 - 64);
   ac = -a;
   BOOST_CHECK_EQUAL(ac, -8);
   ac = a;
   ac -= -a;
   BOOST_CHECK_EQUAL(ac, 16);
   ac = a;
   ac += -a;
   BOOST_CHECK_EQUAL(ac, 0);
   ac = b;
   ac /= -a;
   BOOST_CHECK_EQUAL(ac, -8);
   ac = a;
   ac *= -a;
   BOOST_CHECK_EQUAL(ac, -64);
   ac = a + -b;
   BOOST_CHECK_EQUAL(ac, 8 - 64);
   ac = -a + b;
   BOOST_CHECK_EQUAL(ac, -8 + 64);
   ac = -a + -b;
   BOOST_CHECK_EQUAL(ac, -72);
   ac = a + -+-b; // lots of unary operators!!
   BOOST_CHECK_EQUAL(ac, 72);
   test_conditional(Real(-a), -a);

   //
   // RValue ref tests:
   //
   a = 3;
   b = 4;
   c = Real(20) + -(a + b);
   BOOST_CHECK_EQUAL(c, 13);
   c = Real(20) + -a;
   BOOST_CHECK_EQUAL(c, 17);
   c = -a + Real(20);
   BOOST_CHECK_EQUAL(c, 17);
   c = -a + b;
   BOOST_CHECK_EQUAL(c, 1);
   c = b + -a;
   BOOST_CHECK_EQUAL(c, 1);
   a = 2;
   b = 3;
   c = Real(10) - a;
   BOOST_CHECK_EQUAL(c, 8);
   c = a - Real(2);
   BOOST_CHECK_EQUAL(c, 0);
   c = Real(3) - Real(2);
   BOOST_CHECK_EQUAL(c, 1);
   a = 20;
   c = a - (a + b);
   BOOST_CHECK_EQUAL(c, -3);
   a = 2;
   c = (a * b) - (a + b);
   BOOST_CHECK_EQUAL(c, 1);
   c = Real(20) - -(a + b);
   BOOST_CHECK_EQUAL(c, 25);
   c = Real(20) - (-a);
   BOOST_CHECK_EQUAL(c, 22);
   c = (-b) - Real(-5);
   BOOST_CHECK_EQUAL(c, 2);
   c = (-b) - a;
   BOOST_CHECK_EQUAL(c, -5);
   c = b - (-a);
   BOOST_CHECK_EQUAL(c, 5);
   c = Real(3) * -(a + b);
   BOOST_CHECK_EQUAL(c, -15);
   c = -(a + b) * Real(3);
   BOOST_CHECK_EQUAL(c, -15);
   c = Real(2) * -a;
   BOOST_CHECK_EQUAL(c, -4);
   c = -a * Real(2);
   BOOST_CHECK_EQUAL(c, -4);
   c = -a * b;
   BOOST_CHECK_EQUAL(c, -6);
   a = 2;
   b = 4;
   c = Real(4) / -a;
   BOOST_CHECK_EQUAL(c, -2);
   c = -b / Real(2);
   BOOST_CHECK_EQUAL(c, -2);
   c = Real(4) / -(2 * a);
   BOOST_CHECK_EQUAL(c, -1);
   c = b / -(2 * a);
   BOOST_CHECK_EQUAL(c, -1);
   c = -(2 * a) / Real(2);
   BOOST_CHECK_EQUAL(c, -2);
}
template <class Real>
void test_signed_ops(const std::integral_constant<bool, false>&)
{
}

template <class Real>
void test_basic_conditionals(Real a, Real b)
{
   if (a)
   {
      BOOST_ERROR("Unexpected non-zero result");
   }
   if (!a)
   {
   }
   else
   {
      BOOST_ERROR("Unexpected zero result");
   }
   b = 2;
   if (!b)
   {
      BOOST_ERROR("Unexpected zero result");
   }
   if (b)
   {
   }
   else
   {
      BOOST_ERROR("Unexpected non-zero result");
   }
   if (a && b)
   {
      BOOST_ERROR("Unexpected zero result");
   }
   if (!(a || b))
   {
      BOOST_ERROR("Unexpected zero result");
   }
   if (a + b)
   {
   }
   else
   {
      BOOST_ERROR("Unexpected zero result");
   }
   if (b - 2)
   {
      BOOST_ERROR("Unexpected non-zero result");
   }
}

template <class T>
typename std::enable_if<boost::multiprecision::number_category<T>::value == boost::multiprecision::number_kind_complex>::type
test_relationals(T a, T b)
{
   BOOST_CHECK_EQUAL((a == b), false);
   BOOST_CHECK_EQUAL((a != b), true);

   BOOST_CHECK_EQUAL((a + b == b), false);
   BOOST_CHECK_EQUAL((a + b != b), true);

   BOOST_CHECK_EQUAL((a == b + a), false);
   BOOST_CHECK_EQUAL((a != b + a), true);

   BOOST_CHECK_EQUAL((a + b == b + a), true);
   BOOST_CHECK_EQUAL((a + b != b + a), false);

   BOOST_CHECK_EQUAL((8 == b + a), false);
   BOOST_CHECK_EQUAL((8 != b + a), true);
   BOOST_CHECK_EQUAL((800 == b + a), false);
   BOOST_CHECK_EQUAL((800 != b + a), true);
   BOOST_CHECK_EQUAL((72 == b + a), true);
   BOOST_CHECK_EQUAL((72 != b + a), false);

   BOOST_CHECK_EQUAL((b + a == 8), false);
   BOOST_CHECK_EQUAL((b + a != 8), true);
   BOOST_CHECK_EQUAL((b + a == 800), false);
   BOOST_CHECK_EQUAL((b + a != 800), true);
   BOOST_CHECK_EQUAL((b + a == 72), true);
   BOOST_CHECK_EQUAL((b + a != 72), false);
}

template <class T>
typename std::enable_if<boost::multiprecision::number_category<T>::value != boost::multiprecision::number_kind_complex>::type
test_relationals(T a, T b)
{
   BOOST_CHECK_EQUAL((a == b), false);
   BOOST_CHECK_EQUAL((a != b), true);
   BOOST_CHECK_EQUAL((a <= b), true);
   BOOST_CHECK_EQUAL((a < b), true);
   BOOST_CHECK_EQUAL((a >= b), false);
   BOOST_CHECK_EQUAL((a > b), false);

   BOOST_CHECK_EQUAL((a + b == b), false);
   BOOST_CHECK_EQUAL((a + b != b), true);
   BOOST_CHECK_EQUAL((a + b >= b), true);
   BOOST_CHECK_EQUAL((a + b > b), true);
   BOOST_CHECK_EQUAL((a + b <= b), false);
   BOOST_CHECK_EQUAL((a + b < b), false);

   BOOST_CHECK_EQUAL((a == b + a), false);
   BOOST_CHECK_EQUAL((a != b + a), true);
   BOOST_CHECK_EQUAL((a <= b + a), true);
   BOOST_CHECK_EQUAL((a < b + a), true);
   BOOST_CHECK_EQUAL((a >= b + a), false);
   BOOST_CHECK_EQUAL((a > b + a), false);

   BOOST_CHECK_EQUAL((a + b == b + a), true);
   BOOST_CHECK_EQUAL((a + b != b + a), false);
   BOOST_CHECK_EQUAL((a + b <= b + a), true);
   BOOST_CHECK_EQUAL((a + b < b + a), false);
   BOOST_CHECK_EQUAL((a + b >= b + a), true);
   BOOST_CHECK_EQUAL((a + b > b + a), false);

   BOOST_CHECK_EQUAL((8 == b + a), false);
   BOOST_CHECK_EQUAL((8 != b + a), true);
   BOOST_CHECK_EQUAL((8 <= b + a), true);
   BOOST_CHECK_EQUAL((8 < b + a), true);
   BOOST_CHECK_EQUAL((8 >= b + a), false);
   BOOST_CHECK_EQUAL((8 > b + a), false);
   BOOST_CHECK_EQUAL((800 == b + a), false);
   BOOST_CHECK_EQUAL((800 != b + a), true);
   BOOST_CHECK_EQUAL((800 >= b + a), true);
   BOOST_CHECK_EQUAL((800 > b + a), true);
   BOOST_CHECK_EQUAL((800 <= b + a), false);
   BOOST_CHECK_EQUAL((800 < b + a), false);
   BOOST_CHECK_EQUAL((72 == b + a), true);
   BOOST_CHECK_EQUAL((72 != b + a), false);
   BOOST_CHECK_EQUAL((72 <= b + a), true);
   BOOST_CHECK_EQUAL((72 < b + a), false);
   BOOST_CHECK_EQUAL((72 >= b + a), true);
   BOOST_CHECK_EQUAL((72 > b + a), false);

   BOOST_CHECK_EQUAL((b + a == 8), false);
   BOOST_CHECK_EQUAL((b + a != 8), true);
   BOOST_CHECK_EQUAL((b + a >= 8), true);
   BOOST_CHECK_EQUAL((b + a > 8), true);
   BOOST_CHECK_EQUAL((b + a <= 8), false);
   BOOST_CHECK_EQUAL((b + a < 8), false);
   BOOST_CHECK_EQUAL((b + a == 800), false);
   BOOST_CHECK_EQUAL((b + a != 800), true);
   BOOST_CHECK_EQUAL((b + a <= 800), true);
   BOOST_CHECK_EQUAL((b + a < 800), true);
   BOOST_CHECK_EQUAL((b + a >= 800), false);
   BOOST_CHECK_EQUAL((b + a > 800), false);
   BOOST_CHECK_EQUAL((b + a == 72), true);
   BOOST_CHECK_EQUAL((b + a != 72), false);
   BOOST_CHECK_EQUAL((b + a >= 72), true);
   BOOST_CHECK_EQUAL((b + a > 72), false);
   BOOST_CHECK_EQUAL((b + a <= 72), true);
   BOOST_CHECK_EQUAL((b + a < 72), false);

   T c;
   //
   // min and max overloads:
   //
#if !defined(min) && !defined(max)
   //   using std::max;
   //   using std::min;
   // This works, but still causes complaints from inspect.exe, so use brackets to prevent macrosubstitution,
   // and to explicitly specify type T seems necessary, for reasons unclear.
   a = 2;
   b = 5;
   c = 6;
   BOOST_CHECK_EQUAL((std::min<T>)(a, b), a);
   BOOST_CHECK_EQUAL((std::min<T>)(b, a), a);
   BOOST_CHECK_EQUAL((std::max<T>)(a, b), b);
   BOOST_CHECK_EQUAL((std::max<T>)(b, a), b);
   BOOST_CHECK_EQUAL((std::min<T>)(a, b + c), a);
   BOOST_CHECK_EQUAL((std::min<T>)(b + c, a), a);
   BOOST_CHECK_EQUAL((std::min<T>)(a, c - b), 1);
   BOOST_CHECK_EQUAL((std::min<T>)(c - b, a), 1);
   BOOST_CHECK_EQUAL((std::max<T>)(a, b + c), 11);
   BOOST_CHECK_EQUAL((std::max<T>)(b + c, a), 11);
   BOOST_CHECK_EQUAL((std::max<T>)(a, c - b), a);
   BOOST_CHECK_EQUAL((std::max<T>)(c - b, a), a);
   BOOST_CHECK_EQUAL((std::min<T>)(a + b, b + c), 7);
   BOOST_CHECK_EQUAL((std::min<T>)(b + c, a + b), 7);
   BOOST_CHECK_EQUAL((std::max<T>)(a + b, b + c), 11);
   BOOST_CHECK_EQUAL((std::max<T>)(b + c, a + b), 11);
   BOOST_CHECK_EQUAL((std::min<T>)(a + b, c - a), 4);
   BOOST_CHECK_EQUAL((std::min<T>)(c - a, a + b), 4);
   BOOST_CHECK_EQUAL((std::max<T>)(a + b, c - a), 7);
   BOOST_CHECK_EQUAL((std::max<T>)(c - a, a + b), 7);

   long l1(2), l2(3), l3;
   l3 = (std::min)(l1, l2) + (std::max)(l1, l2) + (std::max<long>)(l1, l2) + (std::min<long>)(l1, l2);
   BOOST_CHECK_EQUAL(l3, 10);

#endif
}

template <class T>
const T& self(const T& a) { return a; }

template <class Real>
void test()
{
#if !defined(NO_MIXED_OPS) && !defined(SLOW_COMPILER)
   boost::multiprecision::is_number<Real> tag;
   test_mixed<Real, unsigned char>(tag);
   test_mixed<Real, signed char>(tag);
   test_mixed<Real, char>(tag);
   test_mixed<Real, short>(tag);
   test_mixed<Real, unsigned short>(tag);
   test_mixed<Real, int>(tag);
   test_mixed<Real, unsigned int>(tag);
   test_mixed<Real, long>(tag);
   test_mixed<Real, unsigned long>(tag);
#ifdef BOOST_HAS_LONG_LONG
   test_mixed<Real, long long>(tag);
   test_mixed<Real, unsigned long long>(tag);
#endif
   test_mixed<Real, float>(tag);
   test_mixed<Real, double>(tag);
   test_mixed<Real, long double>(tag);

   typedef typename related_type<Real>::type                                                                      related_type;
   std::integral_constant<bool, boost::multiprecision::is_number<Real>::value && !std::is_same<related_type, Real>::value> tag2;

   test_mixed<Real, related_type>(tag2);

   std::integral_constant<bool, boost::multiprecision::is_number<Real>::value && (boost::multiprecision::number_category<Real>::value == boost::multiprecision::number_kind_complex)> complex_tag;
   test_mixed<Real, std::complex<float> >(complex_tag);
   test_mixed<Real, std::complex<double> >(complex_tag);
   test_mixed<Real, std::complex<long double> >(complex_tag);

   test_enum_conversions<Real>();

#endif
#ifndef MIXED_OPS_ONLY
   //
   // Integer only functions:
   //
   test_integer_ops<Real>(typename boost::multiprecision::number_category<Real>::type());
   //
   // Real number only functions:
   //
   test_float_ops<Real>(typename boost::multiprecision::number_category<Real>::type());
   //
   // Test basic arithmetic:
   //
   Real a(8);
   Real b(64);
   Real c(500);
   Real d(1024);
   BOOST_CHECK_EQUAL(a + b, 72);
   a += b;
   BOOST_CHECK_EQUAL(a, 72);
   BOOST_CHECK_EQUAL(a - b, 8);
   a -= b;
   BOOST_CHECK_EQUAL(a, 8);
   BOOST_CHECK_EQUAL(a * b, 8 * 64L);
   a *= b;
   BOOST_CHECK_EQUAL(a, 8 * 64L);
   BOOST_CHECK_EQUAL(a / b, 8);
   a /= b;
   BOOST_CHECK_EQUAL(a, 8);
   Real ac(a);
   BOOST_CHECK_EQUAL(ac, a);
   ac = a * c;
   BOOST_CHECK_EQUAL(ac, 8 * 500L);
   ac = 8 * 500L;
   ac = ac + b + c;
   BOOST_CHECK_EQUAL(ac, 8 * 500L + 64 + 500);
   ac = a;
   ac = b + c + ac;
   BOOST_CHECK_EQUAL(ac, 8 + 64 + 500);
   ac = ac - b + c;
   BOOST_CHECK_EQUAL(ac, 8 + 64 + 500 - 64 + 500);
   ac = a;
   ac = b + c - ac;
   BOOST_CHECK_EQUAL(ac, -8 + 64 + 500);
   ac = a;
   ac = ac * b;
   BOOST_CHECK_EQUAL(ac, 8 * 64);
   ac = a;
   ac *= b * ac;
   BOOST_CHECK_EQUAL(ac, 8 * 8 * 64);
   ac = b;
   ac = ac / a;
   BOOST_CHECK_EQUAL(ac, 64 / 8);
   ac = b;
   ac /= ac / a;
   BOOST_CHECK_EQUAL(ac, 64 / (64 / 8));
   ac = a;
   ac = b + ac * a;
   BOOST_CHECK_EQUAL(ac, 64 * 2);
   ac = a;
   ac = b - ac * a;
   BOOST_CHECK_EQUAL(ac, 0);
   ac = a;
   ac = b * (ac + a);
   BOOST_CHECK_EQUAL(ac, 64 * (16));
   ac = a;
   ac = b / (ac * 1);
   BOOST_CHECK_EQUAL(ac, 64 / 8);
   ac = a;
   ac = ac + b;
   BOOST_CHECK_EQUAL(ac, 8 + 64);
   ac = a;
   ac = a + ac;
   BOOST_CHECK_EQUAL(ac, 16);
   ac = a;
   ac = a - ac;
   BOOST_CHECK_EQUAL(ac, 0);
   ac = a;
   ac += a + b;
   BOOST_CHECK_EQUAL(ac, 80);
   ac = a;
   ac += b + a;
   BOOST_CHECK_EQUAL(ac, 80);
   ac = +a;
   BOOST_CHECK_EQUAL(ac, 8);
   ac = 8;
   ac = a * ac;
   BOOST_CHECK_EQUAL(ac, 8 * 8);
   ac = a;
   ac = a;
   ac += +a;
   BOOST_CHECK_EQUAL(ac, 16);
   ac = a;
   ac += b - a;
   BOOST_CHECK_EQUAL(ac, 8 + 64 - 8);
   ac = a;
   ac += b * c;
   BOOST_CHECK_EQUAL(ac, 8 + 64 * 500);
   ac = a;
   ac = a;
   ac -= +a;
   BOOST_CHECK_EQUAL(ac, 0);
   ac = a;
   if (std::numeric_limits<Real>::is_signed || is_twos_complement_integer<Real>::value)
   {
      ac = a;
      ac -= c - b;
      BOOST_CHECK_EQUAL(ac, 8 - (500 - 64));
      ac = a;
      ac -= b * c;
      BOOST_CHECK_EQUAL(ac, 8 - 500 * 64);
   }
   ac = a;
   ac += ac * b;
   BOOST_CHECK_EQUAL(ac, 8 + 8 * 64);
   if (std::numeric_limits<Real>::is_signed || is_twos_complement_integer<Real>::value)
   {
      ac = a;
      ac -= ac * b;
      BOOST_CHECK_EQUAL(ac, 8 - 8 * 64);
   }
   ac = a * 8;
   ac *= +a;
   BOOST_CHECK_EQUAL(ac, 64 * 8);
   ac = a;
   ac *= b * c;
   BOOST_CHECK_EQUAL(ac, 8 * 64 * 500);
   ac = a;
   ac *= b / a;
   BOOST_CHECK_EQUAL(ac, 8 * 64 / 8);
   ac = a;
   ac *= b + c;
   BOOST_CHECK_EQUAL(ac, 8 * (64 + 500));
   ac = b;
   ac /= +a;
   BOOST_CHECK_EQUAL(ac, 8);
   ac = b;
   ac /= b / a;
   BOOST_CHECK_EQUAL(ac, 64 / (64 / 8));
   ac = b;
   ac /= a + Real(0);
   BOOST_CHECK_EQUAL(ac, 8);
   //
   // simple tests with immediate values, these calls can be optimised in many backends:
   //
   ac = a + b;
   BOOST_CHECK_EQUAL(ac, 72);
   ac = a + +b;
   BOOST_CHECK_EQUAL(ac, 72);
   ac = +a + b;
   BOOST_CHECK_EQUAL(ac, 72);
   ac = +a + +b;
   BOOST_CHECK_EQUAL(ac, 72);
   ac = a;
   ac = b / ac;
   BOOST_CHECK_EQUAL(ac, b / a);
   //
   // Comparisons:
   //
   test_relationals(a, b);
   test_members(a);
   //
   // Use in Boolean context:
   //
   a = 0;
   b = 2;
   test_basic_conditionals(a, b);
   //
   // Test iostreams:
   //
   std::stringstream ss;
   a = 20;
   b = 2;
   ss << a;
   ss >> c;
   BOOST_CHECK_EQUAL(a, c);
   ss.clear();
   ss << a + b;
   ss >> c;
   BOOST_CHECK_EQUAL(c, 22);
   BOOST_CHECK_EQUAL(c, a + b);
   //
   // More cases for complete code coverage:
   //
   a = 20;
   b = 30;
   swap(a, b);
   BOOST_CHECK_EQUAL(a, 30);
   BOOST_CHECK_EQUAL(b, 20);
   a = 20;
   b = 30;
   std::swap(a, b);
   BOOST_CHECK_EQUAL(a, 30);
   BOOST_CHECK_EQUAL(b, 20);
   a = 20;
   b = 30;
   a = a + b * 2;
   BOOST_CHECK_EQUAL(a, 20 + 30 * 2);
   a = 100;
   a = a - b * 2;
   BOOST_CHECK_EQUAL(a, 100 - 30 * 2);
   a = 20;
   a = a * (b + 2);
   BOOST_CHECK_EQUAL(a, 20 * (32));
   a = 20;
   a = (b + 2) * a;
   BOOST_CHECK_EQUAL(a, 20 * (32));
   a = 90;
   b = 2;
   a = a / (b + 0);
   BOOST_CHECK_EQUAL(a, 45);
   a = 20;
   b = 30;
   c = (a * b) + 22;
   BOOST_CHECK_EQUAL(c, 20 * 30 + 22);
   c = 22 + (a * b);
   BOOST_CHECK_EQUAL(c, 20 * 30 + 22);
   c  = 10;
   ac = a + b * c;
   BOOST_CHECK_EQUAL(ac, 20 + 30 * 10);
   ac = b * c + a;
   BOOST_CHECK_EQUAL(ac, 20 + 30 * 10);
   a = a + b * c;
   BOOST_CHECK_EQUAL(a, 20 + 30 * 10);
   a = 20;
   b = a + b * c;
   BOOST_CHECK_EQUAL(b, 20 + 30 * 10);
   b = 30;
   c = a + b * c;
   BOOST_CHECK_EQUAL(c, 20 + 30 * 10);
   c = 10;
   c = a + b / c;
   BOOST_CHECK_EQUAL(c, 20 + 30 / 10);
   //
   // Additional tests for rvalue ref overloads:
   //
   a = 3;
   b = 4;
   c = Real(2) + a;
   BOOST_CHECK_EQUAL(c, 5);
   c = a + Real(2);
   BOOST_CHECK_EQUAL(c, 5);
   c = Real(3) + Real(2);
   BOOST_CHECK_EQUAL(c, 5);
   c = Real(2) + (a + b);
   BOOST_CHECK_EQUAL(c, 9);
   c = (a + b) + Real(2);
   BOOST_CHECK_EQUAL(c, 9);
   c = (a + b) + (a + b);
   BOOST_CHECK_EQUAL(c, 14);
   c = a * Real(4);
   BOOST_CHECK_EQUAL(c, 12);
   c = Real(3) * Real(4);
   BOOST_CHECK_EQUAL(c, 12);
   c = (a + b) * (a + b);
   BOOST_CHECK_EQUAL(c, 49);
   a = 2;
   c = b / Real(2);
   BOOST_CHECK_EQUAL(c, 2);
   c = Real(4) / a;
   BOOST_CHECK_EQUAL(c, 2);
   c = Real(4) / Real(2);
   BOOST_CHECK_EQUAL(c, 2);
   //
   // Test conditionals:
   //
   a = 20;
   test_conditional(a, +a);
   test_conditional(a, (a + 0));

   test_signed_ops<Real>(std::integral_constant<bool, std::numeric_limits<Real>::is_signed>());
   //
   // Test hashing:
   //
   boost::hash<Real> hasher;
   std::size_t       s = hasher(a);
   BOOST_CHECK_NE(s, 0);
   std::hash<Real> hasher2;
   s = hasher2(a);
   BOOST_CHECK_NE(s, 0);

   //
   // Test move:
   //
   Real m(static_cast<Real&&>(a));
   BOOST_CHECK_EQUAL(m, 20);
   // Move from already moved from object:
   Real m2(static_cast<Real&&>(a));
   // assign from moved from object
   // (may result in "a" being left in valid state as implementation artifact):
   c = static_cast<Real&&>(a);
   // assignment to moved-from objects:
   c = static_cast<Real&&>(m);
   BOOST_CHECK_EQUAL(c, 20);
   m2 = c;
   BOOST_CHECK_EQUAL(c, 20);
   // Destructor of "a" checks destruction of moved-from-object...
   Real m3(static_cast<Real&&>(a));
#ifndef BOOST_MP_NOT_TESTING_NUMBER
   //
   // string and string_view:
   //
   {
      std::string s1("2");
      Real        x(s1);
      BOOST_CHECK_EQUAL(x, 2);
      s1 = "3";
      x.assign(s1);
      BOOST_CHECK_EQUAL(x, 3);
#ifndef BOOST_NO_CXX17_HDR_STRING_VIEW
      s1 = "20";
      std::string_view v(s1.c_str(), 1);
      Real             y(v);
      BOOST_CHECK_EQUAL(y, 2);
      std::string_view v2(s1.c_str(), 2);
      y.assign(v2);
      BOOST_CHECK_EQUAL(y, 20);
#endif
   }
#endif
   //
   // Bug cases, self assignment first:
   //
   a = 20;
   a = self(a);
   BOOST_CHECK_EQUAL(a, 20);

   a = 2;
   a = a * a * a;
   BOOST_CHECK_EQUAL(a, 8);
   a = 2;
   a = a + a + a;
   BOOST_CHECK_EQUAL(a, 6);
   a = 2;
   a = a - a + a;
   BOOST_CHECK_EQUAL(a, 2);
   a = 2;
   a = a + a - a;
   BOOST_CHECK_EQUAL(a, 2);
   a = 2;
   a = a * a - a;
   BOOST_CHECK_EQUAL(a, 2);
   a = 2;
   a = a + a * a;
   BOOST_CHECK_EQUAL(a, 6);
   a = 2;
   a = (a + a) * a;
   BOOST_CHECK_EQUAL(a, 8);
#endif
}
