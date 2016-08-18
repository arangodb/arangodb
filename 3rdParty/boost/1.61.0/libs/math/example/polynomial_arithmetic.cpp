// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Copyright Jeremy W. Murphy 2015.

// This file is written to be included from a Quickbook .qbk document.
// It can be compiled by the C++ compiler, and run. Any output can
// also be added here as comment or included or pasted in elsewhere.
// Caution: this file contains Quickbook markup as well as code
// and comments: don't change any of the special comment markups!

//[polynomial_arithmetic_0
/*`First include the essential polynomial header (and others) to make the example:
*/
#include <boost/math/tools/polynomial.hpp>
//] [polynomial_arithmetic_0

#include <boost/array.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/assert.hpp>

#include <iostream>
#include <stdexcept>
#include <cmath>
#include <string>
#include <utility>

//[polynomial_arithmetic_1
/*`and some using statements are convenient:
*/

using std::string;
using std::exception;
using std::cout;
using std::abs;
using std::pair;

using namespace boost::math;
using namespace boost::math::tools; // for polynomial
using boost::lexical_cast;

//] [/polynomial_arithmetic_1]

template <typename T>
string sign_str(T const &x)
{
  return x < 0 ? "-" : "+";
}

template <typename T>
string inner_coefficient(T const &x)
{
  string result(" " + sign_str(x) + " ");
  if (abs(x) != T(1))
      result += lexical_cast<string>(abs(x));
  return result;
}

/*! Output in formula format.
For example: from a polynomial in Boost container storage  [ 10, -6, -4, 3 ]
show as human-friendly formula notation: 3x^3 - 4x^2 - 6x + 10.
*/
template <typename T>
string formula_format(polynomial<T> const &a)
{
  string result;
  if (a.size() == 0)
      result += lexical_cast<string>(T(0));
  else
  {
    // First one is a special case as it may need unary negate.
    unsigned i = a.size() - 1;
    if (a[i] < 0)
        result += "-";
    if (abs(a[i]) != T(1))
        result += lexical_cast<string>(abs(a[i]));

    if (i > 0)
    {
      result += "x";
      if (i > 1)
      {
          result += "^" + lexical_cast<string>(i);
          i--;
          for (; i != 1; i--)
              result += inner_coefficient(a[i]) + "x^" + lexical_cast<string>(i);

          result += inner_coefficient(a[i]) + "x";
      }
      i--;

      result += " " + sign_str(a[i]) + " " + lexical_cast<string>(abs(a[i]));
    }
  }
  return result;
} // string formula_format(polynomial<T> const &a)


int main()
{
  cout << "Example: Polynomial arithmetic.\n\n";

  try
  {
//[polynomial_arithmetic_2
/*`Store the coefficients in a convenient way to access them,
then create some polynomials using construction from an iterator range,
and finally output in a 'pretty' formula format.

[tip Although we might conventionally write a polynomial from left to right
in descending order of degree, Boost.Math stores in [*ascending order of degree].]

  Read/write for humans:    3x^3 - 4x^2 - 6x + 10
  Boost polynomial storage: [ 10, -6, -4, 3 ]
*/
  boost::array<double, 4> const d3a = {{10, -6, -4, 3}};
  polynomial<double> const a(d3a.begin(), d3a.end());

  // With C++11 and later, you can also use initializer_list construction.
  polynomial<double> const b{{-2.0, 1.0}};

  // formula_format() converts from Boost storage to human notation.
  cout << "a = " << formula_format(a)
  << "\nb = " << formula_format(b) << "\n\n";

//] [/polynomial_arithmetic_2]

//[polynomial_arithmetic_3
  // Now we can do arithmetic with the usual infix operators: + - * / and %.
  polynomial<double> s = a + b;
  cout << "a + b = " << formula_format(s) << "\n";
  polynomial<double> d = a - b;
  cout << "a - b = " << formula_format(d) << "\n";
  polynomial<double> p = a * b;
  cout << "a * b = " << formula_format(p) << "\n";
  polynomial<double> q = a / b;
  cout << "a / b = " << formula_format(q) << "\n";
  polynomial<double> r = a % b;
  cout << "a % b = " << formula_format(r) << "\n";
//] [/polynomial_arithmetic_3]

//[polynomial_arithmetic_4
/*`
Division is a special case where you can calculate two for the price of one.

Actually, quotient and remainder are always calculated together due to the nature
of the algorithm: the infix operators return one result and throw the other
away.

If you are doing a lot of division and want both the quotient and remainder, then
you don't want to do twice the work necessary.

In that case you can call the underlying function, [^quotient_remainder],
to get both results together as a pair.
*/
  pair< polynomial<double>, polynomial<double> > result;
  result = quotient_remainder(a, b);
// Reassure ourselves that the result is the same.
  BOOST_ASSERT(result.first == q);
  BOOST_ASSERT(result.second == r);
//] [/polynomial_arithmetic_4]
}
catch (exception const &e)
{
  cout << "\nMessage from thrown exception was:\n   " << e.what() << "\n";
}
} // int main()

/*
//[polynomial_output_1

a = 3x^3 - 4x^2 - 6x + 10
b = x - 2

//] [/polynomial_output_1]


//[polynomial_output_2

a + b = 3x^3 - 4x^2 - 5x + 8
a - b = 3x^3 - 4x^2 - 7x + 12
a * b = 3x^4 - 10x^3 + 2x^2 + 22x - 20
a / b = 3x^2 + 2x - 2
a % b = 6

//] [/polynomial_output_2]

*/
