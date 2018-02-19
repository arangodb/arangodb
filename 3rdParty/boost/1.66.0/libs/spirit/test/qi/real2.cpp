/*=============================================================================
    Copyright (c) 2001-2011 Joel de Guzman
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
    //  signed real number tests
    ///////////////////////////////////////////////////////////////////////////
    {
        using boost::spirit::qi::double_;
        using boost::spirit::qi::float_;
        using boost::spirit::qi::parse;
        double  d;
        float f;

        BOOST_TEST(test("-1234", double_));
        BOOST_TEST(test_attr("-1234", double_, d) && compare(d, -1234));

        BOOST_TEST(test("-1.2e3", double_));
        BOOST_TEST(test_attr("-1.2e3", double_, d) && compare(d, -1.2e3));

        BOOST_TEST(test("+1.2e3", double_));
        BOOST_TEST(test_attr("+1.2e3", double_, d) && compare(d, 1.2e3));

        BOOST_TEST(test("-0.1", double_));
        BOOST_TEST(test_attr("-0.1", double_, d) && compare(d, -0.1));

        BOOST_TEST(test("-1.2e-3", double_));
        BOOST_TEST(test_attr("-1.2e-3", double_, d) && compare(d, -1.2e-3));

        BOOST_TEST(test("-1.e2", double_));
        BOOST_TEST(test_attr("-1.e2", double_, d) && compare(d, -1.e2));

        BOOST_TEST(test("-.2e3", double_));
        BOOST_TEST(test_attr("-.2e3", double_, d) && compare(d, -.2e3));

        BOOST_TEST(test("-2e3", double_));
        BOOST_TEST(test_attr("-2e3", double_, d) && compare(d, -2e3));

        BOOST_TEST(!test("-e3", double_));
        BOOST_TEST(!test_attr("-e3", double_, d));

        BOOST_TEST(!test("-1.2e", double_));
        BOOST_TEST(!test_attr("-1.2e", double_, d));

        BOOST_TEST(test_attr("-5.7222349715140557e+307", double_, d));
        BOOST_TEST(d == -5.7222349715140557e+307); // exact!

        BOOST_TEST(test_attr("2.0332938517515416e-308", double_, d));
        BOOST_TEST(d == 2.0332938517515416e-308); // exact!

        BOOST_TEST(test_attr("20332938517515416e291", double_, d));
        BOOST_TEST(d == 20332938517515416e291); // exact!

        BOOST_TEST(test_attr("2.0332938517515416e307", double_, d));
        BOOST_TEST(d == 2.0332938517515416e307); // exact!

        BOOST_TEST(test_attr("1.7976931348623157e+308", double_, d));   // DBL_MAX
        BOOST_TEST(d == 1.7976931348623157e+308); // exact!

        BOOST_TEST(test_attr("2.2204460492503131e-16", double_, d));    // DBL_EPSILON
        BOOST_TEST(d == 2.2204460492503131e-16); // exact!

        BOOST_TEST(test_attr("2.2250738585072014e-308", double_, d));   // DBL_MIN
        BOOST_TEST(d == 2.2250738585072014e-308); // exact!

        BOOST_TEST(test_attr("4.9406564584124654e-324", double_, d));   // DBL_DENORM_MIN
        BOOST_TEST(d == 4.9406564584124654e-324); // exact!
        
        BOOST_TEST(test_attr("219721.03839999999", double_, d));
        BOOST_TEST(d == 219721.03839999999); // exact!

        BOOST_TEST(test_attr("-5.7222349715140557e+307", double_, d));
        BOOST_TEST(d == -5.7222349715140557e+307); // exact!

        BOOST_TEST(test_attr("2.2204460492503131e-16", double_, d));
        BOOST_TEST(d == 2.2204460492503131e-16); // exact!

        // floats...

        BOOST_TEST(test_attr("3.402823466e+38", float_, f));            // FLT_MAX
        BOOST_TEST(f == 3.402823466e+38F); // exact!

        BOOST_TEST(test_attr("1.192092896e-07", float_, f));            // FLT_EPSILON
        BOOST_TEST(f == 1.192092896e-07F); // exact!

        BOOST_TEST(test_attr("1.175494351e-38", float_, f));            // FLT_MIN
        BOOST_TEST(f == 1.175494351e-38F); // exact!

        BOOST_TEST(test_attr("219721.03839999999", float_, f));
        BOOST_TEST(f == 219721.03839999999f); // inexact
        
        BOOST_TEST(test_attr("2.2204460492503131e-16", float_, f));
        BOOST_TEST(f == 2.2204460492503131e-16f); // inexact
        
        // big exponents!
        // fail, but do not assert!
        BOOST_TEST(!test_attr("123e1234000000", double_, d));
        BOOST_TEST(!test_attr("123e-1234000000", double_, d));

        using boost::math::fpclassify;
        using boost::spirit::detail::signbit;   // Boost version is broken

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
    }

    return boost::report_errors();
}
