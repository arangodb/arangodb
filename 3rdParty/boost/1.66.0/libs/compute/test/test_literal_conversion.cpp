//---------------------------------------------------------------------------//
// Copyright (c) 2016 Jason Rhinelander <jason@imaginary.ca>
//
// Distributed under the Boost Software License, Version 1.0
// See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt
//
// See http://boostorg.github.com/compute for more information.
//---------------------------------------------------------------------------//

#define BOOST_TEST_MODULE TestLiteralConversion
#include <boost/test/unit_test.hpp>

#include <string>
#include <sstream>
#include <vector>

#include <boost/compute/detail/literal.hpp>

BOOST_AUTO_TEST_CASE(literal_conversion_float)
{
    std::vector<float> values, roundtrip;
    values.push_back(1.2345679f);
    values.push_back(1.2345680f);
    values.push_back(1.2345681f);
    for (size_t i = 0; i < values.size(); i++) {
        std::string literal = boost::compute::detail::make_literal(values[i]);
        // Check that we got something in the literal, and that at least the first
        // 6 characters (5 digits) look good
        BOOST_CHECK_EQUAL(literal.substr(0, 6), "1.2345");

        // libc++ stream extraction fails on a value such as "1.2f" rather than extracting 1.2
        // and leaving the f; so check the "f", then slice it off for the extraction below:
        BOOST_CHECK_EQUAL(literal.substr(literal.length()-1), "f");

        std::istringstream iss(literal.substr(0, literal.length()-1));
        float x;
        iss >> x;
        roundtrip.push_back(x);
    }
    BOOST_CHECK_EQUAL(values[0], roundtrip[0]);
    BOOST_CHECK_EQUAL(values[1], roundtrip[1]);
    BOOST_CHECK_EQUAL(values[2], roundtrip[2]);
}

BOOST_AUTO_TEST_CASE(literal_conversion_double)
{
    std::vector<double> values, roundtrip;
    values.push_back(1.2345678901234567);
    values.push_back(1.2345678901234569);
    values.push_back(1.2345678901234571);
    for (size_t i = 0; i < values.size(); i++) {
        std::string literal = boost::compute::detail::make_literal(values[i]);
        // Check that we got something in the literal, and that at least the first
        // 11 characters (10 digits) look good
        BOOST_CHECK_EQUAL(literal.substr(0, 11), "1.234567890");

        // Stuff the literal into a stream then extract it, and make sure we get a numerically
        // identical result.  (Since there is no suffix, we don't have to worry about removing the
        // suffix like we have to above, for float--but we also check to make sure there is no
        // (unextracted) suffix).
        std::istringstream iss(literal);
        double x;
        std::string suffix;
        iss >> x >> suffix;
        BOOST_CHECK_EQUAL(suffix, "");
        roundtrip.push_back(x);
    }
    BOOST_CHECK_EQUAL(values[0], roundtrip[0]);
    BOOST_CHECK_EQUAL(values[1], roundtrip[1]);
    BOOST_CHECK_EQUAL(values[2], roundtrip[2]);
}
