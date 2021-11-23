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
    
\brief test_dimensionless_quantity.cpp

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
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::dimensionless>           E1; 
    BOOST_TEST(E1.value() == double());
    
    // value_type constructor
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::dimensionless>           E2(E_);
    BOOST_TEST(E2.value() == E_);

    // copy constructor
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::dimensionless>           E3(E2);
    BOOST_TEST(E3.value() == E_);

    // operator=
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::dimensionless>           E4 = E2;
    BOOST_TEST(E4.value() == E_);

    // implicit copy constructor value_type conversion
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::dimensionless,float>     E5(E2);
    BOOST_UNITS_CHECK_CLOSE(E5.value(), float(E_));

    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::dimensionless,long>      E6(E2);
    BOOST_TEST(E6.value() == long(E_));

    // implicit operator= value_type conversion
    // narrowing conversion disallowed
//    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::dimensionless,float>     E7 = E2;
//    BOOST_UNITS_CHECK_CLOSE(E7.value(),float(E_));
    
    // narrowing conversion disallowed
//    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::dimensionless,long>      E8 = E2;
//    BOOST_TEST(E8.value() == long(E_));
    
    // const construction
    bu::quantity<bu::dimensionless>                 E9(E2); 
    BOOST_TEST(E9.value() == E_);
    
//    // value assignment
//    E9.value() = 1.5*bu::dimensionless();
//    BOOST_TEST(E9.value() == 1.5);
//    
//    // value assignment with implicit conversion
//    E9.value() = 1.5;
//    BOOST_TEST(E9.value() == 1.5);
//    
//    // value assignment with implicit value_type conversion
//    E9.value() = 2*bu::dimensionless();
//    BOOST_TEST(E9.value() == double(2));
//
//    // value assignment with implicit value_type conversion
//    E9.value() = 2;
//    BOOST_TEST(E9.value() == double(2));
    
    // operator+=(this_type)
    E9 = 2.0;
    E9 += E9;
    BOOST_TEST(E9.value() == 4.0);
    
    // operator-=(this_type)
    E9 = 2.0;
    E9 -= E9;
    BOOST_TEST(E9.value() == 0.0);
    
    // operator*=(value_type)
    E9 = 2.0;
    E9 *= 2.0;
    BOOST_TEST(E9.value() == 4.0);
    
    // operator/=(value_type)
    E9 = 2.0;
    E9 /= 2.0;
    BOOST_TEST(E9.value() == 1.0);
    
    // static construct quantity from value_type
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::dimensionless>           E(bu::quantity<bu::dimensionless>::from_value(2.5));
    BOOST_TEST(E.value() == 2.5);
    
    // implicit conversion to value_type
    const double    V1(E9);
    BOOST_TEST(V1 == E9.value());
    
    const double    V2 = E9;
    BOOST_TEST(V2 == E9.value());
            
    // unit * scalar
    BOOST_TEST(bu::dimensionless()*2.0 == bu::quantity<bu::dimensionless>::from_value(2.0));
    
    // unit / scalar
    BOOST_TEST(bu::dimensionless()/2.0 == bu::quantity<bu::dimensionless>::from_value(0.5));
    
    // scalar * unit
    BOOST_TEST(2.0*bu::dimensionless() == bu::quantity<bu::dimensionless>::from_value(2.0));
    
    // scalar / unit
    BOOST_TEST(2.0/bu::dimensionless() == bu::quantity<bu::dimensionless>::from_value(2.0));

    //  quantity * scalar
    BOOST_TEST(E*2.0 == bu::quantity<bu::dimensionless>::from_value(5.0));

    //  quantity / scalar
    BOOST_TEST(E/2.0 == bu::quantity<bu::dimensionless>::from_value(1.25));
    
    // scalar * quantity
    BOOST_TEST(2.0*E == bu::quantity<bu::dimensionless>::from_value(5.0));

    // scalar / quantity
    BOOST_TEST(2.0/E == bu::quantity<bu::dimensionless>::from_value(0.8));

    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::dimensionless>    D1(1.0),
                                                                D2(2.0);
    
    // unit * quantity
    BOOST_TEST(bu::dimensionless()*D1 == D1);
    
    // unit / quantity
    BOOST_TEST(bu::dimensionless()/D1 == D1);
    
    // quantity * unit
    BOOST_TEST(D1*bu::dimensionless() == D1);
    
    // quantity / unit
    BOOST_TEST(D1*bu::dimensionless() == D1);
    
    // +quantity
    BOOST_TEST(+D1 == 1.0*bu::dimensionless());
    
    // -quantity
    BOOST_TEST(-D1 == -1.0*bu::dimensionless());
    
    // quantity + quantity
    BOOST_TEST(D2+D1 == 3.0*bu::dimensionless());
    
    // quantity - quantity
    BOOST_TEST(D2-D1 == 1.0*bu::dimensionless());
    
    // quantity * quantity
    BOOST_TEST(D1*D2 == 2.0*bu::dimensionless());
    
    // quantity / quantity
    BOOST_TEST(D2/D1 == 2.0*bu::dimensionless());
    
    // integer power of quantity
    BOOST_TEST(2.0*bu::pow<2>(D2) == 2.0*std::pow(2.0,2.0)*bu::dimensionless());
    
    // rational power of quantity
    BOOST_TEST((2.0*bu::pow< bu::static_rational<2,3> >(D2) == 2.0*std::pow(2.0,2.0/3.0)*bu::dimensionless()));
    
    // integer root of quantity
    BOOST_TEST(2.0*bu::root<2>(D2) == 2.0*std::pow(2.0,1.0/2.0)*bu::dimensionless());
    
    // rational root of quantity
    BOOST_TEST((2.0*bu::root< bu::static_rational<3,2> >(D2) == 2.0*std::pow(2.0,2.0/3.0)*bu::dimensionless()));
    
    BOOST_CONSTEXPR_OR_CONST bu::quantity<bu::dimensionless>    A1(0.0),
                                                                A2(0.0),
                                                                A3(1.0),
                                                                A4(-1.0);
                                    
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
