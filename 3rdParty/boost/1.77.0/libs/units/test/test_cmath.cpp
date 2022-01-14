// Boost.Units - A C++ library for zero-overhead dimensional analysis and 
// unit/quantity manipulation and conversion
//
// Copyright (C) 2003-2008 Matthias Christian Schabel
// Copyright (C) 2008 Steven Watanabe
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/** 
\file
    
\brief test_units_1.cpp

\details
Test unit class.

Output:
@verbatim
@endverbatim
**/

#include <limits>

#include <boost/units/cmath.hpp>

#include "test_header.hpp"

namespace bu = boost::units;

int main()
{
    BOOST_CONSTEXPR_OR_CONST double inf = std::numeric_limits<double>::infinity(),
                                    nan = std::numeric_limits<double>::quiet_NaN();
    
    // default constructor
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::energy>   E1(0.0*bu::joules),
                                                        E2(inf*bu::joules),
                                                        E3(nan*bu::joules); 
    
    BOOST_TEST((bu::isfinite)(E1) == true);
    BOOST_TEST((bu::isfinite)(E2) == false);
    BOOST_TEST((bu::isfinite)(E3) == false);

    BOOST_TEST((bu::isinf)(E1) == false);
    BOOST_TEST((bu::isinf)(E2) == true);
    BOOST_TEST((bu::isinf)(E3) == false);
    
    BOOST_TEST((bu::isnan)(E1) == false);
    BOOST_TEST((bu::isnan)(E2) == false);
    BOOST_TEST((bu::isnan)(E3) == true);
    
    BOOST_TEST((bu::isnormal)(E1) == false);
    BOOST_TEST((bu::isnormal)(E2) == false);
    BOOST_TEST((bu::isnormal)(E3) == false);

    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::energy>   E4(-2.5*bu::joules),
                                                        E5(2.5*bu::joules); 
                                            
    BOOST_TEST((bu::isgreater)(E4,E5) == false);
    BOOST_TEST((bu::isgreater)(E5,E4) == true);
    BOOST_TEST((bu::isgreater)(E4,E4) == false);
    BOOST_TEST((bu::isgreater)(E3,E4) == false);
    BOOST_TEST((bu::isgreater)(E4,E3) == false);

    BOOST_TEST((bu::isgreaterequal)(E4,E5) == false);
    BOOST_TEST((bu::isgreaterequal)(E5,E4) == true);
    BOOST_TEST((bu::isgreaterequal)(E4,E4) == true);
    BOOST_TEST((bu::isgreaterequal)(E3,E4) == false);
    BOOST_TEST((bu::isgreaterequal)(E4,E3) == false);

    BOOST_TEST((bu::isless)(E4,E5) == true);
    BOOST_TEST((bu::isless)(E5,E4) == false);
    BOOST_TEST((bu::isless)(E4,E4) == false);
    BOOST_TEST((bu::isless)(E3,E4) == false);
    BOOST_TEST((bu::isless)(E4,E3) == false);

    BOOST_TEST((bu::islessequal)(E4,E5) == true);
    BOOST_TEST((bu::islessequal)(E5,E4) == false);
    BOOST_TEST((bu::islessequal)(E4,E4) == true);
    BOOST_TEST((bu::islessequal)(E3,E4) == false);
    BOOST_TEST((bu::islessequal)(E4,E3) == false);

    BOOST_TEST((bu::islessgreater)(E4,E5) == true);
    BOOST_TEST((bu::islessgreater)(E5,E4) == true);
    BOOST_TEST((bu::islessgreater)(E4,E4) == false);
    BOOST_TEST((bu::islessgreater)(E3,E4) == false);
    BOOST_TEST((bu::islessgreater)(E4,E3) == false);

    BOOST_TEST((bu::isunordered)(E4,E5) == false);
    BOOST_TEST((bu::isunordered)(E5,E4) == false);
    BOOST_TEST((bu::isunordered)(E4,E4) == false);
    BOOST_TEST((bu::isunordered)(E3,E4) == true);
    BOOST_TEST((bu::isunordered)(E4,E3) == true);

    BOOST_TEST((bu::abs)(E4) == E5);
    BOOST_TEST((bu::ceil)(E4) == -2.0*bu::joules);
    BOOST_TEST((bu::copysign)(E4,E5) == E5);
    BOOST_TEST((bu::fabs)(E4) == E5);
    BOOST_TEST((bu::floor)(E4) == -3.0*bu::joules);
    BOOST_TEST((bu::fdim)(E4,E5) == 0.0*bu::joules);
    BOOST_TEST((bu::fdim)(E5,E4) == E5-E4);
    
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::length>   L1(3.0*bu::meters),
                                                        L2(4.0*bu::meters);
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::area>     A1(4.0*bu::square_meters),
                                                        A2(L1*L2+A1);

#if 0
    BOOST_TEST((bu::fma)(L1,L2,A1) == A2);
#endif
       
    BOOST_TEST((bu::fmax)(E4,E5) == E5);
    BOOST_TEST((bu::fmin)(E4,E5) == E4);
    
    // need to test fpclassify
    
    BOOST_TEST(bu::hypot(L1,L2) == 5.0*bu::meters);
    
#if 0

//    BOOST_TEST(bu::llrint(E4).value() == bu::detail::llrint(E4.value()));
//    BOOST_TEST(bu::llround(E4).value() == bu::detail::llround(E4.value()));
    BOOST_TEST((bu::nearbyint)(E4).value() == (bu::detail::nearbyint)(E4.value()));
    BOOST_TEST((bu::rint)(E4).value() == (bu::detail::rint)(E4.value()));

#endif

    BOOST_TEST((bu::nextafter)(E4,E5).value() == (boost::math::nextafter)(E4.value(),E5.value()));
    BOOST_TEST((bu::nextafter)(E5,E4).value() == (boost::math::nextafter)(E5.value(),E4.value()));

    BOOST_TEST((bu::nexttoward)(E4,E5).value() == (boost::math::nextafter)(E4.value(),E5.value()));
    BOOST_TEST((bu::nexttoward)(E5,E4).value() == (boost::math::nextafter)(E5.value(),E4.value()));

    BOOST_TEST((bu::round)(E4 - 0.00000000001 * bu::joules) == -3.0*bu::joules);
    BOOST_TEST((bu::round)(E5 + 0.00000000001 * bu::joules) == 3.0*bu::joules);    
    BOOST_TEST((bu::signbit)(E4) != 0);
    BOOST_TEST((bu::signbit)(E5) == 0);
    BOOST_TEST((bu::trunc)(E4) == -2.0*bu::joules);
    BOOST_TEST((bu::trunc)(E5) == 2.0*bu::joules);

    BOOST_TEST((bu::fmod)(E4,E5) == -0.0*bu::joules);

    bu::quantity<bu::energy>    pint;
    
    BOOST_TEST((bu::modf)(E4,&pint) == -0.5*bu::joules);
    BOOST_TEST(pint == -2.0*bu::joules);
    
    int ex;
    const bu::quantity<bu::energy>  E6((bu::frexp)(E4,&ex));
    
    BOOST_TEST(E6 == -0.625*bu::joules);
    BOOST_TEST(ex == 2);
    BOOST_TEST((bu::ldexp)(E6,ex) == E4);
    
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::dimensionless>   E7(1.0);
    
    BOOST_TEST(bu::pow(E7,E7) == 1.0*1.0);
    
    const bu::quantity<bu::dimensionless>   E8((bu::exp)(E7));
    
    BOOST_TEST(std::abs(E8 - std::exp(1.0)) < .000001);
    BOOST_TEST(bu::log(E8) == E7);
    
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::dimensionless>   E9(100.0);
    
    BOOST_TEST(bu::log10(E9) == 2.0);
    
    BOOST_TEST(bu::sqrt(A1) == 2.0*bu::meters);
    
    return boost::report_errors();
}
