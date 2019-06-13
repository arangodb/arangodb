///////////////////////////////////////////////////////////////////////////////
//  Copyright 2018 John Maddock. Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_MP_CPP_COMPLEX_HPP
#define BOOST_MP_CPP_COMPLEX_HPP

#include <boost/multiprecision/cpp_bin_float.hpp>
#include <boost/multiprecision/complex_adaptor.hpp>

namespace boost {
   namespace multiprecision {

#ifndef BOOST_NO_CXX11_TEMPLATE_ALIASES

      template <unsigned Digits, backends::digit_base_type DigitBase = backends::digit_base_10, class Allocator = void, class Exponent = int, Exponent MinExponent = 0, Exponent MaxExponent = 0>
      using cpp_complex_backend = complex_adaptor<cpp_bin_float<Digits, DigitBase, Allocator, Exponent, MinExponent, MaxExponent> >;

      template <unsigned Digits, backends::digit_base_type DigitBase = digit_base_10, class Allocator = void, class Exponent = int, Exponent MinExponent = 0, Exponent MaxExponent = 0, expression_template_option ExpressionTemplates = et_off>
      using cpp_complex = number<complex_adaptor<cpp_bin_float<Digits, DigitBase, Allocator, Exponent, MinExponent, MaxExponent> >, ExpressionTemplates>;

      typedef cpp_complex<50> cpp_complex_50;
      typedef cpp_complex<100> cpp_complex_100;

      typedef cpp_complex<24, backends::digit_base_2, void, boost::int16_t, -126, 127> cpp_complex_single;
      typedef cpp_complex<53, backends::digit_base_2, void, boost::int16_t, -1022, 1023> cpp_complex_double;
      typedef cpp_complex<64, backends::digit_base_2, void, boost::int16_t, -16382, 16383> cpp_complex_extended;
      typedef cpp_complex<113, backends::digit_base_2, void, boost::int16_t, -16382, 16383> cpp_complex_quad;
      typedef cpp_complex<237, backends::digit_base_2, void, boost::int32_t, -262142, 262143> cpp_complex_oct;

#else

      typedef number<complex_adaptor<cpp_bin_float<50> >, et_off> cpp_complex_50;
      typedef number<complex_adaptor<cpp_bin_float<100> >, et_off> cpp_complex_100;

      typedef number<complex_adaptor<cpp_bin_float<24, backends::digit_base_2, void, boost::int16_t, -126, 127> >, et_off> cpp_complex_single;
      typedef number<complex_adaptor<cpp_bin_float<53, backends::digit_base_2, void, boost::int16_t, -1022, 1023> >, et_off> cpp_complex_double;
      typedef number<complex_adaptor<cpp_bin_float<64, backends::digit_base_2, void, boost::int16_t, -16382, 16383> >, et_off> cpp_complex_extended;
      typedef number<complex_adaptor<cpp_bin_float<113, backends::digit_base_2, void, boost::int16_t, -16382, 16383> >, et_off> cpp_complex_quad;
      typedef number<complex_adaptor<cpp_bin_float<237, backends::digit_base_2, void, boost::int32_t, -262142, 262143> >, et_off> cpp_complex_oct;

#endif

   }
}

#endif
