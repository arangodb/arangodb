// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Copyright Paul A. Bristow 2019.
// Copyright Christopher Kormanyos 2012.
// Copyright John Maddock 2012.

// This file is written to be included from a Quickbook .qbk document.
// It can be compiled by the C++ compiler, and run. Any output can
// also be added here as comment or included or pasted in elsewhere.
// Caution: this file contains Quickbook markup as well as code
// and comments: don't change any of the special comment markups!

#ifdef _MSC_VER
#  pragma warning (disable : 4512) // assignment operator could not be generated.
#  pragma warning (disable : 4996)
#endif

//[big_seventh_example_1

/*`[h5 Using Boost.Multiprecision `cpp_float` types for numerical calculations with higher precision than __fundamental `long double`.]

The Boost.Multiprecision library can be used for computations requiring precision
exceeding that of standard __fundamental types such as `float`, `double`
and `long double`. For extended-precision calculations, Boost.Multiprecision
supplies several template data types called `cpp_bin_float_`.

The number of decimal digits of precision is fixed at compile-time via template parameter.

To use these floating-point types and
[@https://www.boost.org/doc/libs/release/libs/math/doc/html/constants.html Boost.Math collection of high-precision constants],
we need some includes:
*/

#include <boost/math/constants/constants.hpp>

#include <boost/multiprecision/cpp_bin_float.hpp>
// that includes some predefined typedefs like:
// using boost::multiprecision::cpp_bin_float_quad;
// using boost::multiprecision::cpp_bin_float_50;
// using boost::multiprecision::cpp_bin_float_100;

#include <iostream>
#include <limits>

/*` So now we can demonstrate with some trivial calculations:
*/

//] //[big_seventh_example_1]

int main()
{

//[big_seventh_example_2
/*`Using a `typedef` like `cpp_bin_float_50` hides the complexity of multiprecision, and
allows us to define variables with 50 decimal digit precision just like __fundamental `double`.
*/
  using boost::multiprecision::cpp_bin_float_50;

  cpp_bin_float_50 seventh = cpp_bin_float_50(1) / 7;  // 1 / 7

/*`By default, output would only show the standard 6 decimal digits,
 so set precision to show all 50 significant digits, including any trailing zeros (to show the full implied 50 digit precision).
*/
  std::cout.precision(std::numeric_limits<cpp_bin_float_50>::digits10); // Show 50 decimal digit precision.
  std::cout << std::showpoint << std::endl; // Append any trailing zeros.
  std::cout << seventh << std::endl;
/*`which outputs:

  0.14285714285714285714285714285714285714285714285714

We can also use Boost.Math __math_constants like [pi],
guaranteed to be initialized with the very last bit of precision for the floating-point type.
*/
   std::cout << "pi = " << boost::math::constants::pi<cpp_bin_float_50>() << std::endl;
   cpp_bin_float_50 circumference = boost::math::constants::pi<cpp_bin_float_50>() * 2 * seventh;
   std::cout << "c =  "<< circumference << std::endl;

/*`which outputs

  pi = 3.1415926535897932384626433832795028841971693993751

  c =  0.89759790102565521098932668093700082405633411410717
*/
//]  [/big_seventh_example_2]

//[big_seventh_example_3
/*`So using `cpp_bin_float_50` looks like a simple 'drop-in' for the __fundamental_type like 'double',
but beware of less-than-expected precision from construction or conversion from `double` or other lower precision types.
This is a mistake that is very easy to make,
and very difficult to detect because the difference in precision is only visible after about the 17th decimal digit.

We can show this by constructing a fraction one seventh from `double`:
*/
   cpp_bin_float_50 seventh_0 = cpp_bin_float_50(1/7);  // Avoid the schoolboy-error `double d7 = 1 / 7;` giving zero!
   std::cout << "seventh_0 = "  << seventh_0 << std::endl;
   // seventh_double0 = 0.0000000000000000000000000000000000000000000000000

   cpp_bin_float_50 seventh_double = cpp_bin_float_50(1./7);  // Construct from double! (0.14285714285714)
   std::cout << "seventh_double = "  << seventh_double << std::endl; // Boost.Multiprecision post-school error!
   // seventh_double = 0.14285714285714284921269268124888185411691665649414

/*`Did you spot the probably-unintended difference?  After the 17th decimal digit, result is apparently random
and not the expected recurring pattern 14285714285714...

The 'random' digits after digit 17 are from the `cpp_bin_float_50` representation of the `double` value 0.14285714285714
which is different from the 'better' `cpp_bin_float_50` representation of the fraction 1/7
*/

   cpp_bin_float_50 seventh_big(1);  // 1
   seventh_big /= 7;
   std::cout << "seventh_big = " << seventh_big << std::endl; //
   // seventh_big     = 0.14285714285714285714285714285714285714285714285714
//`Note the recurring 14285714285714 pattern as expected.

//`We can also construct a `const` version (but not yet `constexpr`) and evaluate in a single expression:

   const cpp_bin_float_50 seventh_const (cpp_bin_float_50(1) / 7);
   std::cout << "seventh_const = " << seventh_const << std::endl; //
   // seventh_const = 0.14285714285714285714285714285714285714285714285714
//]  [/big_seventh_example_3]

//[big_seventh_example_constexpr

   // Sadly we cannot (yet) write:
   // constexpr cpp_bin_float_50 any_constexpr(0);

   // constexpr cpp_bin_float_50 seventh_constexpr (cpp_bin_float_50(1) / 7);
   // std::cout << "seventh_constexpr = " << seventh_constexpr << std::endl; //
   // nor use the macro constexpr unless it returns `const`
   // constexpr cpp_bin_float_50 seventh_constexpr(seventh_const);

//]  [/big_seventh_example_constexpr]

//[big_seventh_example_4
   /*`For some purposes, this difference in precision may be insignificant,
but if one is implementing a formula involving a fraction from integers,
including decimal fractions like 1/10, 1/100, then comparison with other computations like __WolframAlpha
will reveal differences whose cause may be perplexing.

To get as precise-as-possible decimal fractions like 1.234, we can write
*/

  const cpp_bin_float_50 f1(cpp_bin_float_50(1234) / 1000);  // Construct from a fraction.
  std::cout << "cpp_bin_float_50 f1(cpp_bin_float_50(1234) / 1000) = " << f1 << std::endl; // cpp_bin_float_50 f1(cpp_bin_float_50(1234) / 1000) = 1.2340000000000000000000000000000000000000000000000
/*`or
*/
  const cpp_bin_float_50 f2("1.234");  // Construct from decimal digit string.
  std::cout << "cpp_bin_float_50 f2(\"1.234\") = " << f2 << std::endl; // cpp_bin_float_50 f2("1.234") = 1.2340000000000000000000000000000000000000000000000

/*`that are different from constructing from a `double` with value 1.234
*/
  const cpp_bin_float_50 f3(cpp_bin_float_50(1.234));
  std::cout << "cpp_bin_float_50 f3(cpp_bin_float_50(1.234)) = " << f3 << std::endl; // 1.2339999999999999857891452847979962825775146484375

//]  [/big_seventh_example_4]

    return 0;
} // int main()


/*
//[big_seventh_example_output

0.14285714285714285714285714285714285714285714285714
pi = 3.1415926535897932384626433832795028841971693993751
c =  0.89759790102565521098932668093700082405633411410717
seventh_0 = 0.0000000000000000000000000000000000000000000000000
seventh_double = 0.14285714285714284921269268124888185411691665649414
seventh_big = 0.14285714285714285714285714285714285714285714285714
seventh_const = 0.14285714285714285714285714285714285714285714285714
cpp_bin_float_50 f1(cpp_bin_float_50(1234) / 100) = 12.340000000000000000000000000000000000000000000000
cpp_bin_float_50 f2("1.234") = 1.2340000000000000000000000000000000000000000000000
cpp_bin_float_50 f3(cpp_bin_float_50(1.234)) = 1.2339999999999999857891452847979962825775146484375

//] //[/big_seventh_example_output]

*/

