// Copyright Paul A. Bristow 2013
// Copyright John Maddock 2013
// Copyright Christopher Kormanyos

// Use, modification and distribution are subject to the
// Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt
// or copy at http://www.boost.org/LICENSE_1_0.txt)

// Examples of numeric_limits usage as snippets for multiprecision documentation.

// Includes text as Quickbook comments.

#include <iostream>
#include <iomanip>
#include <string>
#include <sstream>
#include <limits> // numeric_limits
#include <iomanip>
#include <locale>
#include <boost/assert.hpp>

#include <boost/math/constants/constants.hpp>
#include <boost/math/special_functions/nonfinite_num_facets.hpp>

#include <boost/math/special_functions/factorials.hpp>
#include <boost/math/special_functions/next.hpp>
#include <boost/math/tools/precision.hpp>
#include <boost/multiprecision/cpp_dec_float.hpp> // is decimal.
#include <boost/multiprecision/cpp_bin_float.hpp> // is binary.

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp> // Boost.Test
#include <boost/test/floating_point_comparison.hpp> 

static long double const log10Two = 0.30102999566398119521373889472449L; // log10(2.)

template <typename T>
int max_digits10()
{
   int significand_digits = std::numeric_limits<T>::digits;
  // BOOST_CONSTEXPR_OR_CONST int significand_digits = std::numeric_limits<T>::digits;
   return static_cast<int>(ceil(1 + significand_digits * log10Two));
} // template <typename T> int max_digits10()

// Used to test max_digits10<>() function below.
//#define BOOST_NO_CXX11_NUMERIC_LIMITS

BOOST_AUTO_TEST_CASE(test_numeric_limits_snips)
{
  try
  {

// Example of portable way to get `std::numeric_limits<T>::max_digits10`.
//[max_digits10_1

/*`For example, to be portable (including obselete platforms) for type `T` where `T` may be:
 `float`, `double`, `long double`, `128-bit quad type`, `cpp_bin_float_50` ...
*/

  typedef float T;

#if defined BOOST_NO_CXX11_NUMERIC_LIMITS
   // No max_digits10 implemented.
    std::cout.precision(max_digits10<T>());
#else
  #if(_MSC_VER <= 1600) 
   //  Wrong value for std::numeric_limits<float>::max_digits10.
    std::cout.precision(max_digits10<T>());
  #else // Use the C++11 max_digits10.
     std::cout.precision(std::numeric_limits<T>::max_digits10);
  #endif
#endif

  std::cout << "std::cout.precision(max_digits10) = " << std::cout.precision() << std::endl; // 9

  double x = 1.2345678901234567889;

  std::cout << "x = " << x << std::endl; //

/*`which should output:

  std::cout.precision(max_digits10) = 9
  x = 1.23456789
*/

//] [/max_digits10_1]

  {
//[max_digits10_2

  double write = 2./3; // Any arbitrary value that cannot be represented exactly.
  double read = 0;
  std::stringstream s;
  s.precision(std::numeric_limits<double>::digits10); // or `float64_t` for 64-bit IEE754 double.
  s << write;
  s >> read;
  if(read != write)
  {
    std::cout <<  std::setprecision(std::numeric_limits<double>::digits10)
      << read << " != " << write << std::endl;
  }

//] [/max_digits10_2]
  // 0.666666666666667 != 0.666666666666667
  }

  {
//[max_digits10_3

  double pi = boost::math::double_constants::pi;
  std::cout.precision(std::numeric_limits<double>::max_digits10);
  std::cout << pi << std::endl; // 3.1415926535897931

//] [/max_digits10_3]
  }
  {
//[max_digits10_4
/*`and similarly for a much higher precision type:
*/

  using namespace boost::multiprecision;

  typedef number<cpp_dec_float<50> > cpp_dec_float_50; // 50 decimal digits.

  using boost::multiprecision::cpp_dec_float_50;

  cpp_dec_float_50 pi = boost::math::constants::pi<cpp_dec_float_50>();
  std::cout.precision(std::numeric_limits<cpp_dec_float_50>::max_digits10);
  std::cout << pi << std::endl;
  // 3.141592653589793238462643383279502884197169399375105820974944592307816406
//] [/max_digits10_4]
  }

  {
//[max_digits10_5

  for (int i = 2; i < 15; i++)
  {
    std::cout << std::setw(std::numeric_limits<int>::max_digits10)
      << boost::math::factorial<double>(i)  << std::endl;
  }

//] [/max_digits10_5]
  }

  }
  catch(std::exception ex)
  {
    std::cout << "Caught Exception " << ex.what() << std::endl;
  }

  {
//[max_digits10_6

  typedef double T;

  bool denorm = std::numeric_limits<T>::denorm_min() < std::numeric_limits<T>::min();
  BOOST_ASSERT(denorm);

//] [/max_digits10_6]
  }

  {
    unsigned char c = 255;
    std::cout << "char c = " << (int)c << std::endl;
  }

  {
//[digits10_1
    std::cout
      << std::setw(std::numeric_limits<short>::digits10 +1 +1) // digits10+1, and +1 for sign.
      << std::showpos << (std::numeric_limits<short>::max)() // +32767
      << std::endl
      << std::setw(std::numeric_limits<short>::digits10 +1 +1)
      << (std::numeric_limits<short>::min)() << std::endl;   // -32767
//] [/digits10_1]
  }

  {
//[digits10_2
    std::cout
      << std::setw(std::numeric_limits<unsigned short>::digits10 +1 +1) // digits10+1, and +1 for sign.
      << std::showpos << (std::numeric_limits<unsigned short>::max)() //  65535
      << std::endl
      << std::setw(std::numeric_limits<unsigned short>::digits10 +1 +1) // digits10+1, and +1 for sign.
      << (std::numeric_limits<unsigned short>::min)() << std::endl;   //      0
//] [/digits10_2]
  }

  std::cout <<std::noshowpos << std::endl;

  {
//[digits10_3
  std::cout.precision(std::numeric_limits<double>::max_digits10);
  double d =  1e15;
  double dp1 = d+1;
  std::cout << d << "\n" << dp1 << std::endl;
  // 1000000000000000
  // 1000000000000001
  std::cout <<  dp1 - d << std::endl; // 1
//] [/digits10_3]
  }

  {
//[digits10_4
  std::cout.precision(std::numeric_limits<double>::max_digits10);
  double d =  1e16;
  double dp1 = d+1;
  std::cout << d << "\n" << dp1 << std::endl;
  // 10000000000000000
  // 10000000000000000
    std::cout << dp1 - d << std::endl; // 0 !!!
//] [/digits10_4]
  }

  {
//[epsilon_1
  std::cout.precision(std::numeric_limits<double>::max_digits10);
  double d = 1.;
  double eps = std::numeric_limits<double>::epsilon();
  double dpeps = d+eps;
  std::cout << std::showpoint // Ensure all trailing zeros are shown.
    << d << "\n"           // 1.0000000000000000
    << dpeps << std::endl; // 2.2204460492503131e-016
  std::cout << dpeps - d   // 1.0000000000000002
    << std::endl;
//] [epsilon_1]
  }

  {
//[epsilon_2
  double one = 1.;
  double nad = boost::math::float_next(one);
  std::cout << nad << "\n"  //  1.0000000000000002
    << nad - one // 2.2204460492503131e-016
    << std::endl;
//] [epsilon_2]
  }
  {
//[epsilon_3
  std::cout.precision(std::numeric_limits<double>::max_digits10);
  double d = 1.;
  double eps = std::numeric_limits<double>::epsilon();
  double dpeps = d + eps/2;

  std::cout << std::showpoint // Ensure all trailing zeros are shown.
    << dpeps << "\n"       // 1.0000000000000000
    << eps/2 << std::endl; // 1.1102230246251565e-016
  std::cout << dpeps - d   // 0.00000000000000000
    << std::endl;
//] [epsilon_3]
  }

  {
    typedef double RealType;
//[epsilon_4
/*`A tolerance might be defined using this version of epsilon thus:
*/
    RealType tolerance = boost::math::tools::epsilon<RealType>() * 2;
//] [epsilon_4]
  }

  {
//[digits10_5
    -(std::numeric_limits<double>::max)() == std::numeric_limits<double>::lowest();
//] [/digits10_5]
  }

  {
//[denorm_min_1
  std::cout.precision(std::numeric_limits<double>::max_digits10);
  if (std::numeric_limits<double>::has_denorm == std::denorm_present)
  {
    double d = std::numeric_limits<double>::denorm_min();

      std::cout << d << std::endl; //  4.9406564584124654e-324

      int exponent;

      double significand = frexp(d, &exponent);
      std::cout << "exponent = " << std::hex << exponent << std::endl; //  fffffbcf
      std::cout << "significand = " << std::hex << significand << std::endl; // 0.50000000000000000
  }
  else
  {
    std::cout << "No denormalization. " << std::endl;
  }
//] [denorm_min_1]
  }

  {
//[round_error_1
    double round_err = std::numeric_limits<double>::epsilon() // 2.2204460492503131e-016
                     * std::numeric_limits<double>::round_error(); // 1/2
    std::cout << round_err << std::endl; // 1.1102230246251565e-016
//] [/round_error_1]
  }

  {
    typedef double T;
//[tolerance_1
/*`For example, if we want a tolerance that might suit about 9 arithmetical operations,
say sqrt(9) = 3,  we could define:
*/

    T tolerance =  3 * std::numeric_limits<T>::epsilon();

/*`This is very widely used in Boost.Math testing
with Boost.Test's macro `BOOST_CHECK_CLOSE_FRACTION`
*/

    T expected = 1.0;
    T calculated = 1.0 + std::numeric_limits<T>::epsilon();

    BOOST_CHECK_CLOSE_FRACTION(expected, calculated, tolerance);

//] [/tolerance_1]
  }

  {
//[tolerance_2

  using boost::multiprecision::number;
  using boost::multiprecision::cpp_dec_float;
  using boost::multiprecision::et_off;

  typedef number<cpp_dec_float<50>, et_off > cpp_dec_float_50; // 50 decimal digits.
/*`[note that Boost.Test does not yet allow floating-point comparisons with expression templates on,
so the default expression template parameter has been replaced by `et_off`.]
*/

  cpp_dec_float_50 tolerance =  3 * std::numeric_limits<cpp_dec_float_50>::epsilon();
  cpp_dec_float_50 expected = boost::math::constants::two_pi<cpp_dec_float_50>();
  cpp_dec_float_50 calculated = 2 * boost::math::constants::pi<cpp_dec_float_50>();

  BOOST_CHECK_CLOSE_FRACTION(expected, calculated, tolerance);

//] [/tolerance_2]
  }

  {
//[tolerance_3

  using boost::multiprecision::cpp_bin_float_quad;

  cpp_bin_float_quad tolerance =  3 * std::numeric_limits<cpp_bin_float_quad>::epsilon();
  cpp_bin_float_quad expected = boost::math::constants::two_pi<cpp_bin_float_quad>();
  cpp_bin_float_quad calculated = 2 * boost::math::constants::pi<cpp_bin_float_quad>();

  BOOST_CHECK_CLOSE_FRACTION(expected, calculated, tolerance);

//] [/tolerance_3]
  }

  {
//[nan_1]

/*`NaN can be used with binary multiprecision types like `cpp_bin_float_quad`:
*/
  using boost::multiprecision::cpp_bin_float_quad;

  if (std::numeric_limits<cpp_bin_float_quad>::has_quiet_NaN == true)
  {
    cpp_bin_float_quad tolerance =  3 * std::numeric_limits<cpp_bin_float_quad>::epsilon();

    cpp_bin_float_quad NaN =  std::numeric_limits<cpp_bin_float_quad>::quiet_NaN();
    std::cout << "cpp_bin_float_quad NaN is "  << NaN << std::endl; //   cpp_bin_float_quad NaN is nan

    cpp_bin_float_quad expected = NaN;
    cpp_bin_float_quad calculated = 2 * NaN;
    // Comparisons of NaN's always fail:
    bool b = expected == calculated;
    std::cout << b << std::endl;
    BOOST_CHECK_NE(expected, expected);
    BOOST_CHECK_NE(expected, calculated);
  }
  else
  {
    std::cout << "Type " << typeid(cpp_bin_float_quad).name() << " does not have NaNs!" << std::endl;
  }

//] [/nan_1]
  }

  {
//[facet_1]

/*`
See [@boost:/libs/math/example/nonfinite_facet_sstream.cpp]
and we also need

  #include <boost/math/special_functions/nonfinite_num_facets.hpp>

Then we can equally well use a multiprecision type cpp_bin_float_quad:

*/
  using boost::multiprecision::cpp_bin_float_quad;

  typedef cpp_bin_float_quad T;

  using boost::math::nonfinite_num_put;
  using boost::math::nonfinite_num_get;
  {
    std::locale old_locale;
    std::locale tmp_locale(old_locale, new nonfinite_num_put<char>);
    std::locale new_locale(tmp_locale, new nonfinite_num_get<char>);
    std::stringstream ss;
    ss.imbue(new_locale);
    T inf = std::numeric_limits<T>::infinity();
    ss << inf; // Write out.
    assert(ss.str() == "inf");
    T r;
    ss >> r; // Read back in.
    assert(inf == r); // Confirms that the floating-point values really are identical.
    std::cout << "infinity output was " << ss.str() << std::endl;
    std::cout << "infinity input was " << r << std::endl;
  }

/*`
  infinity output was inf
  infinity input was inf

Similarly we can do the same with NaN (except that we cannot use `assert`)
*/
  {
    std::locale old_locale;
    std::locale tmp_locale(old_locale, new nonfinite_num_put<char>);
    std::locale new_locale(tmp_locale, new nonfinite_num_get<char>);
    std::stringstream ss;
    ss.imbue(new_locale);
    T n;
    T NaN = std::numeric_limits<T>::quiet_NaN();
    ss << NaN; // Write out.
    assert(ss.str() == "nan");
    std::cout << "NaN output was " << ss.str() << std::endl;
    ss >> n; // Read back in.
    std::cout << "NaN input was " << n << std::endl;
  }
/*`
  NaN output was nan
  NaN input was nan

*/
//] [/facet_1]
  }


} // BOOST_AUTO_TEST_CASE(test_numeric_limits_snips)

