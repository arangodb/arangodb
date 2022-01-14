/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman
    Copyright (c) 2001-2011 Hartmut Kaiser
    Copyright (c) 2011      Bryce Lelbach

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "real.hpp"

int
main()
{
    using spirit_test::test;
    using spirit_test::test_attr;

    ///////////////////////////////////////////////////////////////////////////
    //  thousand separated numbers
    ///////////////////////////////////////////////////////////////////////////
    {
        using boost::spirit::x3::uint_parser;
        using boost::spirit::x3::parse;

        uint_parser<unsigned, 10, 1, 3> uint3;
        uint_parser<unsigned, 10, 3, 3> uint3_3;

    #define r (uint3 >> *(',' >> uint3_3))

        BOOST_TEST(test("1,234,567,890", r));
        BOOST_TEST(test("12,345,678,900", r));
        BOOST_TEST(test("123,456,789,000", r));
        BOOST_TEST(!test("1000,234,567,890", r));
        BOOST_TEST(!test("1,234,56,890", r));
        BOOST_TEST(!test("1,66", r));
    }

    ///////////////////////////////////////////////////////////////////////////
    //  unsigned real number tests
    ///////////////////////////////////////////////////////////////////////////
    {
        using boost::spirit::x3::real_parser;
        using boost::spirit::x3::parse;
        using boost::spirit::x3::ureal_policies;

        constexpr real_parser<double, ureal_policies<double> > udouble;
        double d;

        BOOST_SPIRIT_ASSERT_CONSTEXPR_CTORS(udouble);

        BOOST_TEST(test("1234", udouble));
        BOOST_TEST(test_attr("1234", udouble, d) && compare(d, 1234));

        BOOST_TEST(test("1.2e3", udouble));
        BOOST_TEST(test_attr("1.2e3", udouble, d) && compare(d, 1.2e3));

        BOOST_TEST(test("1.2e-3", udouble));
        BOOST_TEST(test_attr("1.2e-3", udouble, d) && compare(d, 1.2e-3));

        BOOST_TEST(test("1.e2", udouble));
        BOOST_TEST(test_attr("1.e2", udouble, d) && compare(d, 1.e2));

        BOOST_TEST(test("1.", udouble));
        BOOST_TEST(test_attr("1.", udouble, d) && compare(d, 1.));

        BOOST_TEST(test(".2e3", udouble));
        BOOST_TEST(test_attr(".2e3", udouble, d) && compare(d, .2e3));

        BOOST_TEST(test("2e3", udouble));
        BOOST_TEST(test_attr("2e3", udouble, d) && compare(d, 2e3));

        BOOST_TEST(test("2", udouble));
        BOOST_TEST(test_attr("2", udouble, d) && compare(d, 2));

        BOOST_TEST(test("inf", udouble));
        BOOST_TEST(test("infinity", udouble));
        BOOST_TEST(test("INF", udouble));
        BOOST_TEST(test("INFINITY", udouble));

        BOOST_TEST(test_attr("inf", udouble, d)
                && std::isinf(d));
        BOOST_TEST(test_attr("INF", udouble, d)
                && std::isinf(d));
        BOOST_TEST(test_attr("infinity", udouble, d)
                && std::isinf(d));
        BOOST_TEST(test_attr("INFINITY", udouble, d)
                && std::isinf(d));

        BOOST_TEST(test("nan", udouble));
        BOOST_TEST(test_attr("nan", udouble, d)
                && std::isnan(d));
        BOOST_TEST(test("NAN", udouble));
        BOOST_TEST(test_attr("NAN", udouble, d)
                && std::isnan(d));
        BOOST_TEST(test("nan(...)", udouble));
        BOOST_TEST(test_attr("nan(...)", udouble, d)
                && std::isnan(d));
        BOOST_TEST(test("NAN(...)", udouble));
        BOOST_TEST(test_attr("NAN(...)", udouble, d)
                && std::isnan(d));

        BOOST_TEST(!test("e3", udouble));
        BOOST_TEST(!test_attr("e3", udouble, d));

        BOOST_TEST(!test("-1.2e3", udouble));
        BOOST_TEST(!test_attr("-1.2e3", udouble, d));

        BOOST_TEST(!test("+1.2e3", udouble));
        BOOST_TEST(!test_attr("+1.2e3", udouble, d));

        BOOST_TEST(!test("1.2e", udouble));
        BOOST_TEST(!test_attr("1.2e", udouble, d));

        BOOST_TEST(!test("-.3", udouble));
        BOOST_TEST(!test_attr("-.3", udouble, d));
    }

    return boost::report_errors();
}
