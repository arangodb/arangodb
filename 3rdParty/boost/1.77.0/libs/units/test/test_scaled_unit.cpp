// Boost.Units - A C++ library for zero-overhead dimensional analysis and 
// unit/quantity manipulation and conversion
//
// Copyright (C) 2003-2008 Matthias Christian Schabel
// Copyright (C) 2007-2008 Steven Watanabe
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

/** 
\file
    
\brief test_scaled_conversion.cpp

\details
Test unit scaling

Output:
@verbatim
@endverbatim
**/

#include <boost/units/systems/si/prefixes.hpp>
#include <boost/units/systems/si/time.hpp>
#include <boost/units/quantity.hpp>
#include <boost/units/io.hpp>

#include <sstream>

#include "test_close.hpp"

namespace bu = boost::units;
namespace si = boost::units::si;

void test_scaled_to_plain() {
    BOOST_CONSTEXPR_OR_CONST bu::quantity<si::time> s1 = 12.5 * si::seconds;
    BOOST_CONSTEXPR_OR_CONST bu::quantity<si::time> s2(si::nano * s1);
    BOOST_UNITS_TEST_CLOSE(1e-9 * s1.value(), s2.value(), 0.000000001);
}

void test_plain_to_scaled() {
    BOOST_CONSTEXPR_OR_CONST bu::quantity<si::time> s1 = 12.5 * si::seconds;
    typedef bu::multiply_typeof_helper<si::nano_type, si::time>::type time_unit;
    BOOST_CONSTEXPR_OR_CONST bu::quantity<time_unit> s2(s1);
    BOOST_UNITS_TEST_CLOSE(1e9 * s1.value(), s2.value(), 0.000000001);
}

void test_scaled_to_scaled() {
    typedef bu::multiply_typeof_helper<si::mega_type, si::time>::type mega_time_unit;
    typedef bu::multiply_typeof_helper<si::micro_type, si::time>::type micro_time_unit;
    BOOST_CONSTEXPR_OR_CONST bu::quantity<mega_time_unit> s1(12.5 * si::seconds);
    BOOST_CONSTEXPR_OR_CONST bu::quantity<micro_time_unit> s2(s1);
    BOOST_UNITS_TEST_CLOSE(1e12 * s1.value(), s2.value(), 0.000000001);
}

void test_conversion_factor() {
    BOOST_UNITS_TEST_CLOSE(conversion_factor(si::nano*si::seconds, si::seconds), 1e-9, 0.000000001);
    BOOST_UNITS_TEST_CLOSE(conversion_factor(si::seconds, si::nano*si::seconds), 1e9, 0.000000001);
    BOOST_UNITS_TEST_CLOSE(conversion_factor(si::mega*si::seconds, si::micro*si::seconds), 1e12, 0.000000001);
}

void test_output() {
    std::stringstream stream;
    stream << si::nano * 12.5 * si::seconds;
    BOOST_TEST_EQ(stream.str(), "12.5 ns");
}

void test_output_name() {
    std::stringstream stream;
    stream << bu::name_format << si::nano * 12.5 * si::seconds;
    BOOST_TEST_EQ(stream.str(), "12.5 nanosecond");
}

int main()
{
    test_scaled_to_plain();
    test_plain_to_scaled();
    test_scaled_to_scaled();
    test_conversion_factor();
    test_output();
    test_output_name();
    return boost::report_errors();
}
