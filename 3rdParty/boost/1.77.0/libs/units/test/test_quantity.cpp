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

#include "test_header.hpp"

#include <boost/units/pow.hpp>

namespace bu = boost::units;

BOOST_STATIC_CONSTEXPR double E_ = 2.718281828459045235360287471352662497757;

int main()
{
    // default constructor
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::energy>          E1; 
    BOOST_TEST(E1.value() == double());
    
    // value_type constructor
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::energy>          E2(E_*bu::joules);
    BOOST_TEST(E2.value() == E_);

    // copy constructor
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::energy>          E3(E2);
    BOOST_TEST(E3.value() == E_);

    // operator=
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::energy>          E4 = E2;
    BOOST_TEST(E4.value() == E_);

    // implicit copy constructor value_type conversion
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::energy,float>    E5(E2);
    BOOST_UNITS_CHECK_CLOSE(E5.value(),float(E_));

    // implicit operator= value_type conversion
    //BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::energy,float>    E7 = E2;
    //BOOST_UNITS_CHECK_CLOSE(E7.value(),float(E_));
    
    //BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::energy,long>     E8 = E2;
    //BOOST_TEST(E8.value() == long(E_));
    
    // const construction
    bu::quantity<bu::energy>                E9(E2); 
    BOOST_TEST(E9.value() == E_);
    
    // value assignment
    bu::quantity_cast<double&>(E9) = 1.5;
    BOOST_TEST(E9.value() == 1.5);
    
    // value assignment with implicit value_type conversion
    bu::quantity_cast<double&>(E9) = 2;
    BOOST_TEST(E9.value() == double(2));
    
    // operator+=(this_type)
    E9 = 2.0*bu::joules;
    E9 += E9;
    BOOST_TEST(E9.value() == 4.0);
    
    // operator-=(this_type)
    E9 = 2.0*bu::joules;
    E9 -= E9;
    BOOST_TEST(E9.value() == 0.0);
    
    // operator*=(value_type)
    E9 = 2.0*bu::joules;
    E9 *= 2.0;
    BOOST_TEST(E9.value() == 4.0);
    
    // operator/=(value_type)
    E9 = 2.0*bu::joules;
    E9 /= 2.0;
    BOOST_TEST(E9.value() == 1.0);
    
    // static construct quantity from value_type
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::energy>      E(bu::quantity<bu::energy>::from_value(2.5));
    BOOST_TEST(E.value() == 2.5);
    
    // quantity_cast
        
    // unit * scalar
    BOOST_TEST(bu::joules*2.0 == bu::quantity<bu::energy>::from_value(2.0));
    
    // unit / scalar
    BOOST_TEST(bu::joules/2.0 == bu::quantity<bu::energy>::from_value(0.5));
    
    // scalar * unit
    BOOST_TEST(2.0*bu::joules == bu::quantity<bu::energy>::from_value(2.0));
    
    // scalar / unit
    BOOST_TEST(2.0/bu::joules == bu::quantity<bu::inverse_energy>::from_value(2.0));

    //  quantity * scalar
    BOOST_TEST(E*2.0 == bu::quantity<bu::energy>::from_value(5.0));

    //  quantity / scalar
    BOOST_TEST(E/2.0 == bu::quantity<bu::energy>::from_value(1.25));
    
    // scalar * quantity
    BOOST_TEST(2.0*E == bu::quantity<bu::energy>::from_value(5.0));

    // scalar / quantity
    BOOST_TEST(2.0/E == bu::quantity<bu::inverse_energy>::from_value(0.8));

    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::length>      L(1.0*bu::meters);
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::mass>        M(2.0*bu::kilograms);
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::time>        T(3.0*bu::seconds);
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::velocity>    V(bu::quantity<bu::velocity>::from_value(4.0));
    
    // unit * quantity
    BOOST_TEST(bu::seconds*V == 4.0*bu::meters);
    
    // unit / quantity
    BOOST_TEST(bu::meters/V == 0.25*bu::seconds);
    
    // quantity * unit
    BOOST_TEST(V*bu::seconds == 4.0*bu::meters);
    
    // quantity / unit
    BOOST_TEST(V/bu::meters == 4.0/bu::seconds);
    
    // +quantity
    BOOST_TEST(+V == 4.0*bu::meters_per_second);
    
    // -quantity
    BOOST_TEST(-V == -4.0*bu::meters_per_second);
    
    // quantity + quantity
    BOOST_TEST(V+V == 8.0*bu::meters_per_second);
    
    // quantity - quantity
    BOOST_TEST(V-V == 0.0*bu::meters_per_second);
    
    // quantity * quantity
    BOOST_TEST(V*T == 12.0*bu::meters);
    
    // quantity / quantity
    BOOST_TEST(L/V == 0.25*bu::seconds);
    
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::area>    A(2.0*bu::square_meters);
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::volume>  VL(1.0*bu::cubic_meters);
    
    // integer power of quantity
    BOOST_TEST(2.0*bu::pow<2>(L) == A);
    
    // rational power of quantity
    BOOST_TEST((bu::pow< bu::static_rational<2,3> >(VL) == 0.5*A));
    
    // integer root of quantity
    BOOST_TEST(bu::root<2>(A) == std::sqrt(2.0)*L);
    
    // rational root of quantity
    BOOST_TEST((bu::root< bu::static_rational<3,2> >(VL) == 0.5*A));
    
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::area> A1(0.0*bu::square_meters),
                                                    A2(0.0*bu::square_meters),
                                                    A3(1.0*bu::square_meters),
                                                    A4(-1.0*bu::square_meters);
                                    
    // operator==
    BOOST_TEST((A1 == A2) == true);
    BOOST_TEST((A1 == A3) == false);
    
    // operator!=
    BOOST_TEST((A1 != A2) == false);
    BOOST_TEST((A1 != A3) == true);
    
    // operator<
    BOOST_TEST((A1 < A2) == false);
    BOOST_TEST((A1 < A3) == true);
    
    // operator<=
    BOOST_TEST((A1 <= A2) == true);
    BOOST_TEST((A1 <= A3) == true);
    
    // operator>
    BOOST_TEST((A1 > A2) == false);
    BOOST_TEST((A1 > A4) == true);
    
    // operator>=
    BOOST_TEST((A1 >= A2) == true);
    BOOST_TEST((A1 >= A4) == true);
        
    return boost::report_errors();
}
