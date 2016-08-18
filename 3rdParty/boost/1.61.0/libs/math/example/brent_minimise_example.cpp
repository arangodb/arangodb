//! \file
//! \brief Brent_minimise_example.cpp

// Copyright Paul A. Bristow 2015.

// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Note that this file contains Quickbook mark-up as well as code
// and comments, don't change any of the special comment mark-ups!

// For some diagnostic information:
//#define BOOST_MATH_INSTRUMENT
// If quadmath float128 is available:
//#define BOOST_HAVE_QUADMATH

// Example of finding minimum of a function with Brent's method.
//[brent_minimise_include_1
#include <boost/math/tools/minima.hpp>
//] [/brent_minimise_include_1]

#include <boost/math/special_functions/next.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp>
#include <boost/math/special_functions/pow.hpp>
#include <boost/math/constants/constants.hpp>
#include <boost/test/floating_point_comparison.hpp> // For is_close_to and is_small

//[brent_minimise_mp_include_0
#include <boost/multiprecision/cpp_dec_float.hpp> // For decimal boost::multiprecision::cpp_dec_float_50.
#include <boost/multiprecision/cpp_bin_float.hpp> // For binary boost::multiprecision::cpp_bin_float_50;
//] [/brent_minimise_mp_include_0]

//#ifndef _MSC_VER  // float128 is not yet supported by Microsoft compiler at 2013.
#ifdef BOOST_HAVE_QUADMATH  // Define only if GCC or Intel, and have quadmath.lib or .dll library available.
#  include <boost/multiprecision/float128.hpp>
#endif

#include <iostream>
// using std::cout; using std::endl;
#include <iomanip>
// using std::setw; using std::setprecision;
#include <limits>
using std::numeric_limits;
#include <tuple>
#include <utility> // pair, make_pair
#include <type_traits>
#include <typeinfo>

 //typedef boost::multiprecision::number<boost::multiprecision::cpp_dec_float<50>,
 //   boost::multiprecision::et_off>
 //   cpp_dec_float_50_et_off;
 //
 // typedef boost::multiprecision::number<boost::multiprecision::cpp_bin_float<50>,
 //   boost::multiprecision::et_off>
 //   cpp_bin_float_50_et_off;

// http://en.wikipedia.org/wiki/Brent%27s_method Brent's method

double f(double x)
{
  return (x + 3) * (x - 1) * (x - 1);
}

//[brent_minimise_double_functor
struct funcdouble
{
  double operator()(double const& x)
  { //
    return (x + 3) * (x - 1) * (x - 1); // (x + 3)(x - 1)^2
  }
};
//] [/brent_minimise_double_functor]

//[brent_minimise_T_functor
struct func
{
  template <class T>
  T operator()(T const& x)
  { //
    return (x + 3) * (x - 1) * (x - 1); //
  }
};
//] [/brent_minimise_T_functor]

//[brent_minimise_close
//
template <class T = double>
bool close(T expect, T got, T tolerance)
{
  using boost::math::fpc::is_close_to;
  using boost::math::fpc::is_small;

  if (is_small<T>(expect, tolerance))
  {
    return is_small<T>(got, tolerance);
  }
  else
  {
    return is_close_to<T>(expect, got, tolerance);
  }
}

//] [/brent_minimise_close]

//[brent_minimise_T_show

template <class T>
void show_minima()
{
  using boost::math::tools::brent_find_minima;
  try
  { // Always use try'n'catch blocks with Boost.Math to get any error messages.

    int bits = std::numeric_limits<T>::digits/2; // Maximum is digits/2;
    std::streamsize prec = static_cast<int>(2 + sqrt(bits));  // Number of significant decimal digits.
    std::streamsize precision = std::cout.precision(prec); // Save.

    std::cout << "\n\nFor type  " << typeid(T).name()
      << ",\n  epsilon = " << std::numeric_limits<T>::epsilon()
      // << ", precision of " << bits << " bits"
      << ",\n  the maximum theoretical precision from Brent minimization is " << sqrt(std::numeric_limits<T>::epsilon())
      << "\n  Displaying to std::numeric_limits<T>::digits10 " << prec << " significant decimal digits."
      << std::endl;

    const boost::uintmax_t maxit = 20;
    boost::uintmax_t it = maxit;
    // Construct using string, not double, avoids loss of precision.
    //T bracket_min = static_cast<T>("-4");
    //T bracket_max = static_cast<T>("1.3333333333333333333333333333333333333333333333333");

    //  Construction from double may cause loss of precision for multiprecision types like cpp_bin_float.
    // but brackets values are good enough for using Brent minimization.
    T bracket_min = static_cast<T>(-4);
    T bracket_max = static_cast<T>(1.3333333333333333333333333333333333333333333333333);

    std::pair<T, T> r = brent_find_minima<func, T>(func(), bracket_min, bracket_max, bits, it);

    std::cout << "  x at minimum = " << r.first << ", f(" << r.first << ") = " << r.second;
    if (it < maxit)
    {
      std::cout << ",\n  met " << bits << " bits precision" << ", after " << it << " iterations." << std::endl;
    }
    else
    {
      std::cout << ",\n  did NOT meet " << bits << " bits precision" << " after " << it << " iterations!" << std::endl;
    }
    // Check that result is that expected (compared to theoretical uncertainty).
    T uncertainty = sqrt(std::numeric_limits<T>::epsilon());
    //std::cout << std::boolalpha << "x == 1 (compared to uncertainty " << uncertainty << ") is " << close(static_cast<T>(1), r.first, uncertainty) << std::endl;
    //std::cout << std::boolalpha << "f(x) == (0 compared to uncertainty " << uncertainty << ") is " << close(static_cast<T>(0), r.second, uncertainty) << std::endl;
    // Problems with this using multiprecision with expression template on?
    std::cout.precision(precision);  // Restore.
  }
  catch (const std::exception& e)
  { // Always useful to include try & catch blocks because default policies
    // are to throw exceptions on arguments that cause errors like underflow, overflow.
    // Lacking try & catch blocks, the program will abort without a message below,
    // which may give some helpful clues as to the cause of the exception.
    std::cout <<
      "\n""Message from thrown exception was:\n   " << e.what() << std::endl;
  }
} // void show_minima()

//] [/brent_minimise_T_show]

int main()
{
  std::cout << "Brent's minimisation example." << std::endl;
  std::cout << std::boolalpha << std::endl;

  // Tip - using
  // std::cout.precision(std::numeric_limits<T>::digits10);
  // during debugging is wise because it shows if construction of multiprecision involves conversion from double
  // by finding random or zero digits after 17.

  // Specific type double - unlimited iterations.
  using boost::math::tools::brent_find_minima;

  //[brent_minimise_double_1
  int bits = std::numeric_limits<double>::digits;

  std::pair<double, double> r = brent_find_minima(funcdouble(), -4., 4. / 3, bits);

  std::cout.precision(std::numeric_limits<double>::digits10);
  std::cout << "x at minimum = " << r.first << ", f(" << r.first << ") = " << r.second << std::endl;
  // x at minimum = 1.00000000112345, f(1.00000000112345) = 5.04852568272458e-018
  //] [/brent_minimise_double_1]

  std::cout << "x at minimum = " << (r.first - 1.) /r.first << std::endl;

  double uncertainty = sqrt(std::numeric_limits<double>::epsilon());
  std::cout << "Uncertainty sqrt(epsilon) =  " << uncertainty << std::endl;
  // sqrt(epsilon) =  1.49011611938477e-008
  // (epsilon is always > 0, so no need to take abs value).

  using boost::math::fpc::is_close_to;
  using boost::math::fpc::is_small;

  std::cout << is_close_to(1., r.first, uncertainty) << std::endl;
  std::cout << is_small(r.second, uncertainty) << std::endl;

  std::cout << std::boolalpha << "x == 1 (compared to uncertainty " << uncertainty << ") is " << close(1., r.first, uncertainty) << std::endl;
  std::cout << std::boolalpha << "f(x) == (0 compared to uncertainty " << uncertainty << ") is " << close(0., r.second, uncertainty) << std::endl;

  // Specific type double - limit maxit to 20 iterations.
  std::cout << "Precision bits = " << bits << std::endl;
//[brent_minimise_double_2
  const boost::uintmax_t maxit = 20;
  boost::uintmax_t it = maxit;
  r = brent_find_minima(funcdouble(), -4., 4. / 3, bits, it);
  std::cout << "x at minimum = " << r.first << ", f(" << r.first << ") = " << r.second
  << " after " << it << " iterations. " << std::endl;
//] [/brent_minimise_double_2]
  // x at minimum = 1.00000000112345, f(1.00000000112345) = 5.04852568272458e-018

//[brent_minimise_double_3

  std::streamsize prec = static_cast<int>(2 + sqrt(bits));  // Number of significant decimal digits.
  std::cout << "Showing " << bits << " bits precision with " << prec
    << " decimal digits from tolerance " << sqrt(std::numeric_limits<double>::epsilon())
    << std::endl;
  std::streamsize precision = std::cout.precision(prec); // Save.

  std::cout << "x at minimum = " << r.first << ", f(" << r.first << ") = " << r.second
    << " after " << it << " iterations. " << std::endl;

//] [/brent_minimise_double_3]
  // Showing 53 bits precision with 9 decimal digits from tolerance 1.49011611938477e-008
  //  x at minimum = 1, f(1) = 5.04852568e-018

  {
//[brent_minimise_double_4
  bits /= 2; // Half digits precision (effective maximum).
  double epsilon_2 = boost::math::pow<-(std::numeric_limits<double>::digits/2 - 1), double>(2);

  std::cout << "Showing " << bits << " bits precision with " << prec
    << " decimal digits from tolerance " << sqrt(epsilon_2)
    << std::endl;
  std::streamsize precision = std::cout.precision(prec); // Save.

  boost::uintmax_t it = maxit;
  r = brent_find_minima(funcdouble(), -4., 4. / 3, bits, it);
  std::cout << "x at minimum = " << r.first << ", f(" << r.first << ") = " << r.second << std::endl;
  std::cout << it << " iterations. " << std::endl;
//] [/brent_minimise_double_4]
  }
  // x at minimum = 1, f(1) = 5.04852568e-018

  {
  //[brent_minimise_double_5
    bits /= 2; // Quarter precision.
    double epsilon_4 = boost::math::pow<-(std::numeric_limits<double>::digits / 4 - 1), double>(2);

    std::cout << "Showing " << bits << " bits precision with " << prec
      << " decimal digits from tolerance " << sqrt(epsilon_4)
      << std::endl;
    std::streamsize precision = std::cout.precision(prec); // Save.

    boost::uintmax_t it = maxit;
    r = brent_find_minima(funcdouble(), -4., 4. / 3, bits, it);
    std::cout << "x at minimum = " << r.first << ", f(" << r.first << ") = " << r.second
    << ", after " << it << " iterations. " << std::endl;
  //] [/brent_minimise_double_5]
  }

  // Showing 13 bits precision with 9 decimal digits from tolerance 0.015625
  // x at minimum = 0.9999776, f(0.9999776) = 2.0069572e-009
  //  7 iterations.

  {
//[brent_minimise_template_1
    std::cout.precision(std::numeric_limits<long double>::digits10);
    long double bracket_min = -4.;
    long double bracket_max = 4. / 3;
    int bits = std::numeric_limits<long double>::digits;
    const boost::uintmax_t maxit = 20;
    boost::uintmax_t it = maxit;

    std::pair<long double, long double> r = brent_find_minima(func(), bracket_min, bracket_max, bits, it);
    std::cout << "x at minimum = " << r.first << ", f(" << r.first << ") = " << r.second
      << ", after " << it << " iterations. " << std::endl;
//] [/brent_minimise_template_1]
  }

  // Show use of built-in type Template versions.
  // (Will not work if construct bracket min and max from string).

//[brent_minimise_template_fd
  show_minima<float>();
  show_minima<double>();
  show_minima<long double>();
//] [/brent_minimise_template_fd]

  using boost::multiprecision::cpp_bin_float_50; // binary.

//[brent_minimise_mp_include_1
#ifdef BOOST_HAVE_QUADMATH  // Define only if GCC or Intel and have quadmath.lib or .dll library available.
  using boost::multiprecision::float128;
#endif
//] [/brent_minimise_mp_include_1]

//[brent_minimise_template_quad
// #ifndef _MSC_VER
#ifdef BOOST_HAVE_QUADMATH  // Define only if GCC or Intel and have quadmath.lib or .dll library available.
  show_minima<float128>(); // Needs quadmath_snprintf, sqrtQ, fabsq that are in in quadmath library.
#endif
//] [/brent_minimise_template_quad

  // User-defined floating-point template.

//[brent_minimise_mp_typedefs
  using boost::multiprecision::cpp_bin_float_50; // binary.

  typedef boost::multiprecision::number<boost::multiprecision::cpp_bin_float<50>,
    boost::multiprecision::et_on>
    cpp_bin_float_50_et_on;  // et_on is default so is same as cpp_bin_float_50.

  typedef boost::multiprecision::number<boost::multiprecision::cpp_bin_float<50>,
    boost::multiprecision::et_off>
    cpp_bin_float_50_et_off;

  using boost::multiprecision::cpp_dec_float_50; // decimal.

  typedef boost::multiprecision::number<boost::multiprecision::cpp_dec_float<50>,
    boost::multiprecision::et_on> // et_on is default so is same as cpp_dec_float_50.
    cpp_dec_float_50_et_on;

  typedef boost::multiprecision::number<boost::multiprecision::cpp_dec_float<50>,
    boost::multiprecision::et_off>
    cpp_dec_float_50_et_off;
//] [/brent_minimise_mp_typedefs]

  { // binary ET on by default.
//[brent_minimise_mp_1
    std::cout.precision(std::numeric_limits<cpp_bin_float_50>::digits10);

    cpp_bin_float_50 fpv("-1.2345");
    cpp_bin_float_50 absv;

    absv = fpv < static_cast<cpp_bin_float_50>(0) ? -fpv : fpv;
    std::cout << fpv << ' ' << absv << std::endl;


    int bits = std::numeric_limits<cpp_bin_float_50>::digits / 2 - 2;

    cpp_bin_float_50 bracket_min = static_cast<cpp_bin_float_50>("-4");
    cpp_bin_float_50 bracket_max = static_cast<cpp_bin_float_50>("1.3333333333333333333333333333333333333333333333333");

    std::cout << bracket_min << " " << bracket_max << std::endl;
    const boost::uintmax_t maxit = 20;
    boost::uintmax_t it = maxit;
    std::pair<cpp_bin_float_50, cpp_bin_float_50> r = brent_find_minima(func(), bracket_min, bracket_max, bits, it);

    std::cout << "x at minimum = " << r.first << ", f(" << r.first << ") = " << r.second
    // x at minimum = 1, f(1) = 5.04853e-018
      << ", after " << it << " iterations. " << std::endl;

    close(static_cast<cpp_bin_float_50>(1), r.first, sqrt(std::numeric_limits<cpp_bin_float_50>::epsilon()));

//] [/brent_minimise_mp_1]

/*
//[brent_minimise_mp_output_1
    For type  class boost::multiprecision::number<class boost::multiprecision::backends::cpp_bin_float<50, 10, void, int, 0, 0>, 1>,
      epsilon = 5.3455294202e-51,
      the maximum theoretical precision from Brent minimization is 7.311312755e-26
      Displaying to std::numeric_limits<T>::digits10 11 significant decimal digits.
      x at minimum = 1, f(1) = 5.6273022713e-58,
      met 84 bits precision, after 14 iterations.
//] [/brent_minimise_mp_output_1]
*/
//[brent_minimise_mp_2
    show_minima<cpp_bin_float_50_et_on>(); //
//] [/brent_minimise_mp_2]

/*
//[brent_minimise_mp_output_2
    For type  class boost::multiprecision::number<class boost::multiprecision::backends::cpp_bin_float<50, 10, void, int, 0, 0>, 1>,

//] [/brent_minimise_mp_output_1]
*/
  }

  { // binary ET on explicit
    std::cout.precision(std::numeric_limits<cpp_bin_float_50_et_on>::digits10);

    int bits = std::numeric_limits<cpp_bin_float_50_et_on>::digits / 2 - 2;

    cpp_bin_float_50_et_on bracket_min = static_cast<cpp_bin_float_50_et_on>("-4");
    cpp_bin_float_50_et_on bracket_max = static_cast<cpp_bin_float_50_et_on>("1.3333333333333333333333333333333333333333333333333");

    std::cout << bracket_min << " " << bracket_max << std::endl;
    const boost::uintmax_t maxit = 20;
    boost::uintmax_t it = maxit;
    std::pair<cpp_bin_float_50_et_on, cpp_bin_float_50_et_on> r = brent_find_minima(func(), bracket_min, bracket_max, bits, it);

    std::cout << "x at minimum = " << r.first << ", f(" << r.first << ") = " << r.second << std::endl;
    // x at minimum = 1, f(1) = 5.04853e-018
    std::cout << it << " iterations. " << std::endl;

    show_minima<cpp_bin_float_50_et_on>(); //

  }
  return 0;

  { // binary ET off
    std::cout.precision(std::numeric_limits<cpp_bin_float_50_et_off>::digits10);

    int bits = std::numeric_limits<cpp_bin_float_50_et_off>::digits / 2 - 2;
    cpp_bin_float_50_et_off bracket_min = static_cast<cpp_bin_float_50_et_off>("-4");
    cpp_bin_float_50_et_off bracket_max = static_cast<cpp_bin_float_50_et_off>("1.3333333333333333333333333333333333333333333333333");

    std::cout << bracket_min << " " << bracket_max << std::endl;
    const boost::uintmax_t maxit = 20;
    boost::uintmax_t it = maxit;
    std::pair<cpp_bin_float_50_et_off, cpp_bin_float_50_et_off> r = brent_find_minima(func(), bracket_min, bracket_max, bits, it);

    std::cout << "x at minimum = " << r.first << ", f(" << r.first << ") = " << r.second << std::endl;
    // x at minimum = 1, f(1) = 5.04853e-018
    std::cout << it << " iterations. " << std::endl;

    show_minima<cpp_bin_float_50_et_off>(); //
  }

  { // decimal ET on by default
    std::cout.precision(std::numeric_limits<cpp_dec_float_50>::digits10);

    int bits = std::numeric_limits<cpp_dec_float_50>::digits / 2 - 2;

    cpp_dec_float_50 bracket_min = static_cast<cpp_dec_float_50>("-4");
    cpp_dec_float_50 bracket_max = static_cast<cpp_dec_float_50>("1.3333333333333333333333333333333333333333333333333");

    std::cout << bracket_min << " " << bracket_max << std::endl;
    const boost::uintmax_t maxit = 20;
    boost::uintmax_t it = maxit;
    std::pair<cpp_dec_float_50, cpp_dec_float_50> r = brent_find_minima(func(), bracket_min, bracket_max, bits, it);

    std::cout << "x at minimum = " << r.first << ", f(" << r.first << ") = " << r.second << std::endl;
    // x at minimum = 1, f(1) = 5.04853e-018
    std::cout << it << " iterations. " << std::endl;

    show_minima<cpp_dec_float_50>();
  }

  { // decimal ET on
    std::cout.precision(std::numeric_limits<cpp_dec_float_50_et_on>::digits10);

    int bits = std::numeric_limits<cpp_dec_float_50_et_on>::digits / 2 - 2;

    cpp_dec_float_50_et_on bracket_min = static_cast<cpp_dec_float_50_et_on>("-4");
    cpp_dec_float_50_et_on bracket_max = static_cast<cpp_dec_float_50_et_on>("1.3333333333333333333333333333333333333333333333333");
    std::cout << bracket_min << " " << bracket_max << std::endl;
    const boost::uintmax_t maxit = 20;
    boost::uintmax_t it = maxit;
    std::pair<cpp_dec_float_50_et_on, cpp_dec_float_50_et_on> r = brent_find_minima(func(), bracket_min, bracket_max, bits, it);

    std::cout << "x at minimum = " << r.first << ", f(" << r.first << ") = " << r.second << std::endl;
    // x at minimum = 1, f(1) = 5.04853e-018
    std::cout << it << " iterations. " << std::endl;

    show_minima<cpp_dec_float_50_et_on>();

  }

  { // decimal ET off
    std::cout.precision(std::numeric_limits<cpp_dec_float_50_et_off>::digits10);

    int bits = std::numeric_limits<cpp_dec_float_50_et_off>::digits / 2 - 2;

    cpp_dec_float_50_et_off bracket_min = static_cast<cpp_dec_float_50_et_off>("-4");
    cpp_dec_float_50_et_off bracket_max = static_cast<cpp_dec_float_50_et_off>("1.3333333333333333333333333333333333333333333333333");

    std::cout << bracket_min << " " << bracket_max << std::endl;
    const boost::uintmax_t maxit = 20;
    boost::uintmax_t it = maxit;
    std::pair<cpp_dec_float_50_et_off, cpp_dec_float_50_et_off> r = brent_find_minima(func(), bracket_min, bracket_max, bits, it);

    std::cout << "x at minimum = " << r.first << ", f(" << r.first << ") = " << r.second << std::endl;
    // x at minimum = 1, f(1) = 5.04853e-018
    std::cout << it << " iterations. " << std::endl;

    show_minima<cpp_dec_float_50_et_off>();
  }

  return 0;
} // int main()


/*



 // GCC 4.9.1 with quadmath

 Brent's minimisation example.
x at minimum = 1.00000000112345, f(1.00000000112345) = 5.04852568272458e-018
x at minimum = 1.12344622367552e-009
Uncertainty sqrt(epsilon) =  1.49011611938477e-008
x == 1 (compared to uncertainty 1.49011611938477e-008) is true
f(x) == (0 compared to uncertainty 1.49011611938477e-008) is true
Precision bits = 53
x at minimum = 1.00000000112345, f(1.00000000112345) = 5.04852568272458e-018 after 10 iterations.
Showing 53 bits precision with 9 decimal digits from tolerance 1.49011611938477e-008
x at minimum = 1, f(1) = 5.04852568e-018 after 10 iterations.
Showing 26 bits precision with 9 decimal digits from tolerance 0.000172633492
x at minimum = 1, f(1) = 5.04852568e-018
10 iterations.
Showing 13 bits precision with 9 decimal digits from tolerance 0.015625
x at minimum = 0.9999776, f(0.9999776) = 2.0069572e-009, after 7 iterations.
x at minimum = 1.00000000000137302, f(1.00000000000137302) = 7.5407901369731193e-024, after 10 iterations.


For type  f,
  epsilon = 1.1921e-007,
  the maximum theoretical precision from Brent minimization is 0.00034527
  Displaying to std::numeric_limits<T>::digits10 5 significant decimal digits.
  x at minimum = 1.0002, f(1.0002) = 1.9017e-007,
  met 12 bits precision, after 7 iterations.
x == 1 (compared to uncertainty 0.00034527) is true
f(x) == (0 compared to uncertainty 0.00034527) is true


For type  d,
  epsilon = 2.220446e-016,
  the maximum theoretical precision from Brent minimization is 1.490116e-008
  Displaying to std::numeric_limits<T>::digits10 7 significant decimal digits.
  x at minimum = 1, f(1) = 5.048526e-018,
  met 26 bits precision, after 10 iterations.
x == 1 (compared to uncertainty 1.490116e-008) is true
f(x) == (0 compared to uncertainty 1.490116e-008) is true


For type  e,
  epsilon = 1.084202e-019,
  the maximum theoretical precision from Brent minimization is 3.292723e-010
  Displaying to std::numeric_limits<T>::digits10 7 significant decimal digits.
  x at minimum = 1, f(1) = 7.54079e-024,
  met 32 bits precision, after 10 iterations.
x == 1 (compared to uncertainty 3.292723e-010) is true
f(x) == (0 compared to uncertainty 3.292723e-010) is true


For type  N5boost14multiprecision6numberINS0_8backends16float128_backendELNS0_26expression_template_optionE0EEE,
  epsilon = 1.92592994e-34,
  the maximum theoretical precision from Brent minimization is 1.38777878e-17
  Displaying to std::numeric_limits<T>::digits10 9 significant decimal digits.
  x at minimum = 1, f(1) = 1.48695468e-43,
  met 56 bits precision, after 12 iterations.
x == 1 (compared to uncertainty 1.38777878e-17) is true
f(x) == (0 compared to uncertainty 1.38777878e-17) is true
-4 1.3333333333333333333333333333333333333333333333333
x at minimum = 0.99999999999999999999999999998813903221565569205253, f(0.99999999999999999999999999998813903221565569205253) = 5.6273022712501408640665300316078046703496236636624e-58, after 14 iterations.


For type  N5boost14multiprecision6numberINS0_8backends13cpp_bin_floatILj50ELNS2_15digit_base_typeE10EviLi0ELi0EEELNS0_26expression_template_optionE1EEE,
  epsilon = 5.3455294202e-51,
  the maximum theoretical precision from Brent minimization is 7.311312755e-26
  Displaying to std::numeric_limits<T>::digits10 11 significant decimal digits.
  x at minimum = 1, f(1) = 5.6273022713e-58,
  met 84 bits precision, after 14 iterations.
x == 1 (compared to uncertainty 7.311312755e-26) is true
f(x) == (0 compared to uncertainty 7.311312755e-26) is true
-4 1.3333333333333333333333333333333333333333333333333
x at minimum = 0.99999999999999999999999999998813903221565569205253, f(0.99999999999999999999999999998813903221565569205253) = 5.6273022712501408640665300316078046703496236636624e-58
14 iterations.


For type  N5boost14multiprecision6numberINS0_8backends13cpp_bin_floatILj50ELNS2_15digit_base_typeE10EviLi0ELi0EEELNS0_26expression_template_optionE1EEE,
  epsilon = 5.3455294202e-51,
  the maximum theoretical precision from Brent minimization is 7.311312755e-26
  Displaying to std::numeric_limits<T>::digits10 11 significant decimal digits.
  x at minimum = 1, f(1) = 5.6273022713e-58,
  met 84 bits precision, after 14 iterations.
x == 1 (compared to uncertainty 7.311312755e-26) is true
f(x) == (0 compared to uncertainty 7.311312755e-26) is true

RUN SUCCESSFUL (total time: 90ms)


*/
