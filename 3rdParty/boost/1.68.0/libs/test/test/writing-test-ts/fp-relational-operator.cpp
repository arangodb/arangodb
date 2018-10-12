//  (C) Copyright Raffi Enficiaud 2017.
//  Distributed under the Boost Software License, Version 1.0.
//  (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)

// See ticket #13011

#define BOOST_TEST_MODULE fp_check_relational_op
#include <boost/test/unit_test.hpp>
namespace utf = boost::unit_test;
namespace tt = boost::test_tools;

//
// with zeros
//

BOOST_AUTO_TEST_CASE(EQ_zero_on_left, * utf::expected_failures(2))
{
    BOOST_TEST(0.0 == 1.0);
    BOOST_TEST(0.0 == 1.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(NE_zero_on_left)
{
    BOOST_TEST(0.0 != 1.0);
    BOOST_TEST(0.0 != 1.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(EQ_zero_on_right, * utf::expected_failures(2))
{
    BOOST_TEST(1.0 == 0.0);
    BOOST_TEST(1.0 == 0.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(NE_zero_on_right)
{
    BOOST_TEST(1.0 != 0.0);
    BOOST_TEST(1.0 != 0.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(EQ_zero_on_left_right)
{
    BOOST_TEST(0.0 == 0.0);
    BOOST_TEST(0.0 == 0.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(NE_zero_on_left_right, * utf::expected_failures(2))
{
    BOOST_TEST(0.0 != 0.0);
    BOOST_TEST(0.0 != 0.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(LT_zero_on_left)
{
    BOOST_TEST(0.0 < 1.0);
    BOOST_TEST(0.0 < 1.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(GT_zero_on_left, * utf::expected_failures(2) )
{
    BOOST_TEST(0.0 > 1.0);
    BOOST_TEST(0.0 > 1.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(GT_zero_on_right)
{
    BOOST_TEST(1.0 > 0.0);
    BOOST_TEST(1.0 > 0.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(LT_zero_on_right, * utf::expected_failures(2) )
{
    BOOST_TEST(1.0 < 0.0);
    BOOST_TEST(1.0 < 0.0, tt::tolerance(0.001));
}

// with equality

BOOST_AUTO_TEST_CASE(LE_zero_on_left)
{
    BOOST_TEST(0.0 <= 1.0);
    BOOST_TEST(0.0 <= 1.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(GE_zero_on_left, * utf::expected_failures(2) )
{
    BOOST_TEST(0.0 >= 1.0);
    BOOST_TEST(0.0 >= 1.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(GE_zero_on_right)
{
    BOOST_TEST(1.0 >= 0.0);
    BOOST_TEST(1.0 >= 0.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(LE_zero_on_right, * utf::expected_failures(2) )
{
    BOOST_TEST(1.0 <= 0.0);
    BOOST_TEST(1.0 <= 0.0, tt::tolerance(0.001));
}

//
// with numbers
//

BOOST_AUTO_TEST_CASE(LT_smaller_on_left)
{
    BOOST_TEST(1.0 < 2.0);
    BOOST_TEST(1.0 < 2.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(GT_smaller_on_left, * utf::expected_failures(2) )
{
    BOOST_TEST(1.0 > 2.0);
    BOOST_TEST(1.0 > 2.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(GT_smaller_on_right)
{
    BOOST_TEST(2.0 > 1.0);
    BOOST_TEST(2.0 > 1.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(LT_smaller_on_right, * utf::expected_failures(2) )
{
    BOOST_TEST(2.0 < 1.0);
    BOOST_TEST(2.0 < 1.0, tt::tolerance(0.001));
}

// with equality

BOOST_AUTO_TEST_CASE(LE_smaller_on_left)
{
    BOOST_TEST(1.0 <= 2.0);
    BOOST_TEST(1.0 <= 2.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(GE_smaller_on_left, * utf::expected_failures(2) )
{
    BOOST_TEST(1.0 >= 2.0);
    BOOST_TEST(1.0 >= 2.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(GE_smaller_on_right)
{
    BOOST_TEST(2.0 >= 1.0);
    BOOST_TEST(2.0 >= 1.0, tt::tolerance(0.001));
}

BOOST_AUTO_TEST_CASE(LE_smaller_on_right, * utf::expected_failures(2) )
{
    BOOST_TEST(2.0 <= 1.0);
    BOOST_TEST(2.0 <= 1.0, tt::tolerance(0.001));
}
