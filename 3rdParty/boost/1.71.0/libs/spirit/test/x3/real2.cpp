/*=============================================================================
    Copyright (c) 2001-2015 Joel de Guzman
    Copyright (c) 2001-2011 Hartmut Kaiser
    Copyright (c) 2011      Bryce Lelbach

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include "real.hpp"

template <typename T, typename P>
void basic_real_parser_test(P parser)
{
    using spirit_test::test;
    using spirit_test::test_attr;

    T attr;

    BOOST_TEST(test("-1234", parser));
    BOOST_TEST(test_attr("-1234", parser, attr) && compare(attr, T(-1234l)));

    BOOST_TEST(test("-1.2e3", parser));
    BOOST_TEST(test_attr("-1.2e3", parser, attr) && compare(attr, T(-1.2e3l)));

    BOOST_TEST(test("+1.2e3", parser));
    BOOST_TEST(test_attr("+1.2e3", parser, attr) && compare(attr, T(1.2e3l)));

    BOOST_TEST(test("-0.1", parser));
    BOOST_TEST(test_attr("-0.1", parser, attr) && compare(attr, T(-0.1l)));

    BOOST_TEST(test("-1.2e-3", parser));
    BOOST_TEST(test_attr("-1.2e-3", parser, attr) && compare(attr, T(-1.2e-3l)));

    BOOST_TEST(test("-1.e2", parser));
    BOOST_TEST(test_attr("-1.e2", parser, attr) && compare(attr, T(-1.e2l)));

    BOOST_TEST(test("-.2e3", parser));
    BOOST_TEST(test_attr("-.2e3", parser, attr) && compare(attr, T(-.2e3l)));

    BOOST_TEST(test("-2e3", parser));
    BOOST_TEST(test_attr("-2e3", parser, attr) && compare(attr, T(-2e3l)));

    BOOST_TEST(!test("-e3", parser));
    BOOST_TEST(!test_attr("-e3", parser, attr));

    BOOST_TEST(!test("-1.2e", parser));
    BOOST_TEST(!test_attr("-1.2e", parser, attr));
}

int
main()
{
    using spirit_test::test;
    using spirit_test::test_attr;
    ///////////////////////////////////////////////////////////////////////////
    //  signed real number tests
    ///////////////////////////////////////////////////////////////////////////
    {
        basic_real_parser_test<float>(boost::spirit::x3::float_);
        basic_real_parser_test<double>(boost::spirit::x3::double_);
        basic_real_parser_test<long double>(boost::spirit::x3::long_double);
    }

    {
        using boost::spirit::x3::double_;
        double  d;

#if defined(BOOST_SPIRIT_TEST_REAL_PRECISION)
        BOOST_TEST(test_attr("-5.7222349715140557e+307", double_, d));
        BOOST_TEST(d == -5.7222349715140557e+307); // exact!

        BOOST_TEST(test_attr("2.0332938517515416e-308", double_, d));
        BOOST_TEST(d == 2.0332938517515416e-308); // exact!

        BOOST_TEST(test_attr("20332938517515416e291", double_, d));
        BOOST_TEST(d == 20332938517515416e291); // exact!

        BOOST_TEST(test_attr("2.0332938517515416e307", double_, d));
        BOOST_TEST(d == 2.0332938517515416e307); // exact!
#endif

        using boost::math::fpclassify;
        using boost::spirit::x3::signbit;   // Boost version is broken

        BOOST_TEST(test("-inf", double_));
        BOOST_TEST(test("-infinity", double_));
        BOOST_TEST(test_attr("-inf", double_, d) &&
            FP_INFINITE == fpclassify(d) && signbit(d));
        BOOST_TEST(test_attr("-infinity", double_, d) &&
            FP_INFINITE == fpclassify(d) && signbit(d));
        BOOST_TEST(test("-INF", double_));
        BOOST_TEST(test("-INFINITY", double_));
        BOOST_TEST(test_attr("-INF", double_, d) &&
            FP_INFINITE == fpclassify(d) && signbit(d));
        BOOST_TEST(test_attr("-INFINITY", double_, d) &&
            FP_INFINITE == fpclassify(d) && signbit(d));

        BOOST_TEST(test("-nan", double_));
        BOOST_TEST(test_attr("-nan", double_, d) &&
            FP_NAN == fpclassify(d) && signbit(d));
        BOOST_TEST(test("-NAN", double_));
        BOOST_TEST(test_attr("-NAN", double_, d) &&
            FP_NAN == fpclassify(d) && signbit(d));

        BOOST_TEST(test("-nan(...)", double_));
        BOOST_TEST(test_attr("-nan(...)", double_, d) &&
            FP_NAN == fpclassify(d) && signbit(d));
        BOOST_TEST(test("-NAN(...)", double_));
        BOOST_TEST(test_attr("-NAN(...)", double_, d) &&
            FP_NAN == fpclassify(d) && signbit(d));

        BOOST_TEST(!test("1e999", double_));
        BOOST_TEST(!test("1e-999", double_));
        BOOST_TEST(test_attr("2.1111111e-303", double_, d) &&
            compare(d, 2.1111111e-303));
        BOOST_TEST(!test_attr("1.1234e", double_, d) && compare(d, 1.1234));

        // https://svn.boost.org/trac10/ticket/11608
        BOOST_TEST(test_attr("1267650600228229401496703205376", double_, d) &&
            compare(d, 1.2676506002282291E+30));    // Note Qi has better precision

        BOOST_TEST(test_attr("12676506.00228229401496703205376", double_, d) &&
            compare(d, 1.2676506002282292E7));      // Note Qi has better precision

        BOOST_TEST(test_attr("12676506.00228229401496703205376E6", double_, d) &&
            compare(d, 1.2676506002282291016E13));  // Note Qi has better precision
    }

    return boost::report_errors();
}
