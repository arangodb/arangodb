//  Unit test for boost::lexical_cast.
//
//  See http://www.boost.org for most recent version, including documentation.
//
//  Copyright Antony Polukhin, 2014-2019.
//
//  Distributed under the Boost
//  Software License, Version 1.0. (See accompanying file
//  LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt).

#include <boost/config.hpp>

#include <boost/lexical_cast/try_lexical_convert.hpp>
#include <boost/test/unit_test.hpp>

using namespace boost::conversion;

void try_uncommon_cases()
{
    std::string sres;
    const bool res1 = try_lexical_convert(std::string("Test string"), sres);
    BOOST_CHECK(res1);
    BOOST_CHECK_EQUAL(sres, "Test string");

    volatile int vires;
    const bool res2 = try_lexical_convert(100, vires);
    BOOST_CHECK(res2);
    BOOST_CHECK_EQUAL(vires, 100);

    const bool res3 = try_lexical_convert("Test string", sres);
    BOOST_CHECK(res3);
    BOOST_CHECK_EQUAL(sres, "Test string");

    const bool res4 = try_lexical_convert("Test string", sizeof("Test string") - 1, sres);
    BOOST_CHECK(res4);
    BOOST_CHECK_EQUAL(sres, "Test string");

    int ires;
    BOOST_CHECK(!try_lexical_convert("Test string", ires));
    BOOST_CHECK(!try_lexical_convert(1.1, ires));
    BOOST_CHECK(!try_lexical_convert(-1.9, ires));
    BOOST_CHECK(!try_lexical_convert("1.1", ires));
    BOOST_CHECK(!try_lexical_convert("1000000000000000000000000000000000000000", ires));
}


void try_common_cases()
{
    int ires = 0;
    const bool res1 = try_lexical_convert(std::string("100"), ires);
    BOOST_CHECK(res1);
    BOOST_CHECK_EQUAL(ires, 100);

    ires = 0;
    const bool res2 = try_lexical_convert("-100", ires);
    BOOST_CHECK(res2);
    BOOST_CHECK_EQUAL(ires, -100);

    float fres = 1.0f;
    const bool res3 = try_lexical_convert("0.0", fres);
    BOOST_CHECK(res3);
    BOOST_CHECK_EQUAL(fres, 0.0f);

    fres = 1.0f;
    const bool res4 = try_lexical_convert("0.0", sizeof("0.0") - 1, fres);
    BOOST_CHECK(res4);
    BOOST_CHECK_EQUAL(fres, 0.0f);
}

boost::unit_test::test_suite *init_unit_test_suite(int, char *[])
{
    boost::unit_test::test_suite *suite =
        BOOST_TEST_SUITE("Tests for try_lexical_convert");
    suite->add(BOOST_TEST_CASE(&try_uncommon_cases));
    suite->add(BOOST_TEST_CASE(&try_common_cases));

    return suite;
}
