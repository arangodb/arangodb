///////////////////////////////////////////////////////////////
//  Copyright 2013 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at https://www.boost.org/LICENSE_1_0.txt

// Demonstrations of using Boost.Multiprecision float128 quad type.
// (Only available using GCC compiler).

// Contains Quickbook markup in comments.

//[float128_eg
#include <boost/multiprecision/float128.hpp>
#include <boost/math/special_functions/gamma.hpp>
#include <iostream>

int main()
{
   using namespace boost::multiprecision; // Potential to cause name collisions?
   // using boost::multiprecision::float128; // is safer.

/*`The type float128 provides operations at 128-bit precision with
[@https://en.wikipedia.org/wiki/Quadruple-precision_floating-point_format#IEEE_754_quadruple-precision_binary_floating-point_format:_binary128 Quadruple-precision floating-point format]
and have full `std::numeric_limits` support:
*/
   float128 b = 2;
//` There are  15 bits of (biased) binary exponent and 113-bits of significand precision
   std::cout << std::numeric_limits<float128>::digits << std::endl;
//` or 33 decimal places:
   std::cout << std::numeric_limits<float128>::digits10 << std::endl;
   //` We can use any C++ std library function, so let's show all the at-most 36 potentially significant digits, and any trailing zeros, as well:
    std::cout.setf(std::ios_base::showpoint); // Include any trailing zeros.
   std::cout << std::setprecision(std::numeric_limits<float128>::max_digits10)
      << log(b) << std::endl; // Shows log(2) = 0.693147180559945309417232121458176575
   //` We can also use any function from Boost.Math, for example, the 'true gamma' function `tgamma`:
   std::cout << boost::math::tgamma(b) << std::endl;
   /*` And since we have an extended exponent range, we can generate some really large
    numbers here (4.02387260077093773543702433923004111e+2564):
   */
   std::cout << boost::math::tgamma(float128(1000)) << std::endl;
   /*` We can declare constants using GCC or Intel's native types, and literals with the Q suffix, and these can be declared `constexpr` if required:
   */
   // std::numeric_limits<float128>::max_digits10 = 36
   constexpr float128 pi = 3.14159265358979323846264338327950288Q;
   std::cout.precision(std::numeric_limits<float128>::max_digits10);
   std::cout << "pi = " << pi << std::endl; //pi = 3.14159265358979323846264338327950280
//] [/float128_eg]
   return 0;
}

/*
//[float128_numeric_limits

GCC 8.1.0

Type name is float128_t:
Type is g
std::is_fundamental<> = true
boost::multiprecision::detail::is_signed<> = true
boost::multiprecision::detail::is_unsigned<> = false
boost::multiprecision::detail::is_integral<> = false
boost::multiprecision::detail::is_arithmetic<> = true
std::is_const<> = false
std::is_trivial<> = true
std::is_standard_layout<> = true
std::is_pod<> = true
std::numeric_limits::<>is_exact = false
std::numeric_limits::<>is bounded = true
std::numeric_limits::<>is_modulo = false
std::numeric_limits::<>is_iec559 = true
std::numeric_limits::<>traps = false
std::numeric_limits::<>tinyness_before = false
std::numeric_limits::<>max() = 1.18973149535723176508575932662800702e+4932
std::numeric_limits::<>min() = 3.36210314311209350626267781732175260e-4932
std::numeric_limits::<>lowest() = -1.18973149535723176508575932662800702e+4932
std::numeric_limits::<>min_exponent = -16381
std::numeric_limits::<>max_exponent = 16384
std::numeric_limits::<>epsilon() = 1.92592994438723585305597794258492732e-34
std::numeric_limits::<>radix = 2
std::numeric_limits::<>digits = 113
std::numeric_limits::<>digits10 = 33
std::numeric_limits::<>max_digits10 = 36
std::numeric_limits::<>has denorm = true
std::numeric_limits::<>denorm min = 6.47517511943802511092443895822764655e-4966
std::denorm_loss = false
limits::has_signaling_NaN == false
std::numeric_limits::<>quiet_NaN = nan
std::numeric_limits::<>infinity = inf

//] [/float128_numeric_limits]
*/


