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

#include <boost/units/cmath.hpp>
#include <boost/units/scale.hpp>
#include <boost/units/make_scaled_unit.hpp>

namespace bu = boost::units;

static const double E_ = 2.718281828459045235360287471352662497757;

typedef bu::make_scaled_unit<bu::length,
                             bu::scale<10, bu::static_rational<-3> > >::type milli_meter_unit;

typedef bu::make_scaled_unit<bu::area,
                             bu::scale<10, bu::static_rational<-6> > >::type micro_meter2_unit;

int main()
{
    const bu::quantity<micro_meter2_unit> E1 = E_*micro_meter2_unit();
    const bu::quantity<milli_meter_unit>  E2 = sqrt(E1);

    BOOST_TEST(E1.value() == E_);
    BOOST_TEST(E2.value() == sqrt(E_));
    return boost::report_errors();
}
