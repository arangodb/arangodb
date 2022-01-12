//  Copyright (c) 2001-2020 Hartmut Kaiser
//  Copyright (c) 2011      Bryce Lelbach
// 
//  Distributed under the Boost Software License, Version 1.0. (See accompanying 
//  file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include "real.hpp"

#include <boost/spirit/include/version.hpp>
#include <boost/spirit/include/karma_phoenix_attributes.hpp>
#include <boost/phoenix/core.hpp>
#include <boost/phoenix/operator.hpp>

///////////////////////////////////////////////////////////////////////////////
#ifndef BOOST_SPIRIT_NO_MATH_REAL_CONCEPT
// does not provide proper std::numeric_limits, we need to roll our own
namespace boost { namespace spirit { namespace traits
{
    template <>
    struct is_nan<boost::math::concepts::real_concept>
    {
        static bool call(boost::math::concepts::real_concept n) 
        { 
            return test_nan(n.value()); 
        }
    };

    template <>
    struct is_infinite<boost::math::concepts::real_concept>
    {
        static bool call(boost::math::concepts::real_concept n) 
        { 
            return test_infinite(n.value()); 
        }
    };
}}}
#endif

///////////////////////////////////////////////////////////////////////////////
int main()
{
    using namespace boost::spirit;

    {
        using namespace boost::spirit::ascii;

        ///////////////////////////////////////////////////////////////////////
        typedef karma::real_generator<double, fixed_policy<double> > fixed_type;
        fixed_type const fixed = fixed_type();

        BOOST_TEST(test("0.0", fixed, 0.0));
        BOOST_TEST(test("1.0", fixed, 1.0));

        BOOST_TEST(test("0.0", fixed, 0.000012345));
        BOOST_TEST(test("0.0", fixed, 0.00012345));
        BOOST_TEST(test("0.001", fixed, 0.0012345));
        BOOST_TEST(test("0.012", fixed, 0.012345));
        BOOST_TEST(test("0.123", fixed, 0.12345));
        BOOST_TEST(test("1.234", fixed, 1.2345));
        BOOST_TEST(test("12.345", fixed, 12.345));
        BOOST_TEST(test("123.45", fixed, 123.45));
        BOOST_TEST(test("1234.5", fixed, 1234.5));
        BOOST_TEST(test("12342.0", fixed, 12342.));
        BOOST_TEST(test("123420.0", fixed, 123420.));
        BOOST_TEST(test("123420000000000000000.0", fixed, 1.23420e20));

        BOOST_TEST(test("0.0", fixed, -0.000012345));
        BOOST_TEST(test("0.0", fixed, -0.00012345));
        BOOST_TEST(test("-0.001", fixed, -0.0012345));
        BOOST_TEST(test("-0.012", fixed, -0.012345));
        BOOST_TEST(test("-0.123", fixed, -0.12345));
        BOOST_TEST(test("-1.234", fixed, -1.2345));
        BOOST_TEST(test("-12.346", fixed, -12.346));
        BOOST_TEST(test("-123.46", fixed, -123.46));
        BOOST_TEST(test("-1234.5", fixed, -1234.5));
        BOOST_TEST(test("-12342.0", fixed, -12342.));
        BOOST_TEST(test("-123420.0", fixed, -123420.));
        BOOST_TEST(test("-123420000000000000000.0", fixed, -1.23420e20));
    }

#ifndef BOOST_SPIRIT_NO_MATH_REAL_CONCEPT
    {
        using boost::math::concepts::real_concept;
        typedef karma::real_generator<real_concept> custom_type;
        custom_type const custom = custom_type();

        BOOST_TEST(test("0.0", custom, real_concept(0.0)));
        BOOST_TEST(test("1.0", custom, real_concept(1.0)));
        BOOST_TEST(test("1.0", custom, real_concept(1.0001)));
        BOOST_TEST(test("1.001", custom, real_concept(1.001)));
        BOOST_TEST(test("1.01", custom, real_concept(1.010)));
        BOOST_TEST(test("1.1", custom, real_concept(1.100)));

        BOOST_TEST(test("1.234e-04", custom, real_concept(0.00012345)));
        BOOST_TEST(test("0.001", custom, real_concept(0.0012345)));
        BOOST_TEST(test("0.012", custom, real_concept(0.012345)));
        BOOST_TEST(test("0.123", custom, real_concept(0.12345)));
        BOOST_TEST(test("1.234", custom, real_concept(1.2345)));
        BOOST_TEST(test("12.346", custom, real_concept(12.346)));
        BOOST_TEST(test("123.46", custom, real_concept(123.46)));
        BOOST_TEST(test("1234.5", custom, real_concept(1234.5)));
        BOOST_TEST(test("12342.0", custom, real_concept(12342.)));
        BOOST_TEST(test("1.234e05", custom, real_concept(123420.)));

        BOOST_TEST(test("-1.0", custom, real_concept(-1.0)));
        BOOST_TEST(test("-1.234", custom, real_concept(-1.2345)));
        BOOST_TEST(test("-1.235", custom, real_concept(-1.2346)));
        BOOST_TEST(test("-1234.2", custom, real_concept(-1234.2)));

        BOOST_TEST(test("1.0", custom(real_concept(1.0))));
        BOOST_TEST(test("1.0", custom(real_concept(1.0001))));
        BOOST_TEST(test("1.001", custom(real_concept(1.001))));
        BOOST_TEST(test("1.01", custom(real_concept(1.010))));
        BOOST_TEST(test("1.1", custom(real_concept(1.100))));

        BOOST_TEST(test("1.234e-04", custom(real_concept(0.00012345))));
        BOOST_TEST(test("0.001", custom(real_concept(0.0012345))));
        BOOST_TEST(test("0.012", custom(real_concept(0.012345))));
        BOOST_TEST(test("0.123", custom(real_concept(0.12345))));
        BOOST_TEST(test("1.234", custom(real_concept(1.2345))));
        BOOST_TEST(test("12.346", custom(real_concept(12.346))));
        BOOST_TEST(test("123.46", custom(real_concept(123.46))));
        BOOST_TEST(test("1234.5", custom(real_concept(1234.5))));
        BOOST_TEST(test("12342.0", custom(real_concept(12342.))));
        BOOST_TEST(test("1.234e05", custom(real_concept(123420.))));
    }
#endif

// this appears to be broken on Apple Tiger x86 with gcc4.0.1
#if (__GNUC__*10000 + __GNUC_MINOR__*100 + __GNUC_PATCHLEVEL__ != 40001) || \
    !defined(__APPLE__)
    {
        ///////////////////////////////////////////////////////////////////////
        typedef karma::real_generator<double, bordercase_policy<double> > 
            bordercase_type;
        bordercase_type const bordercase = bordercase_type();

//         BOOST_TEST(test("-5.7222349715140557e307", 
//             bordercase(-5.7222349715140557e307)));

        BOOST_TEST(test("1.7976931348623158e308", 
            bordercase(1.7976931348623158e308)));       // DBL_MAX
        BOOST_TEST(test("-1.7976931348623158e308", 
            bordercase(-1.7976931348623158e308)));      // -DBL_MAX
        BOOST_TEST(test("2.2250738585072014e-308", 
            bordercase(2.2250738585072014e-308)));      // DBL_MIN
        BOOST_TEST(test("-2.2250738585072014e-308", 
            bordercase(-2.2250738585072014e-308)));     // -DBL_MIN
    }
#endif

    {
        boost::optional<double> v;
        BOOST_TEST(!test("", double_, v));
        BOOST_TEST(!test("", double_(1.0), v));

        v = 1.0;
        BOOST_TEST(test("1.0", double_, v));
        BOOST_TEST(test("1.0", double_(1.0), v));
    }

    {   // Phoenix expression tests (requires to include 
        // karma_phoenix_attributes.hpp)
        namespace phoenix = boost::phoenix;

        BOOST_TEST(test("1.0", double_, phoenix::val(1.0)));

        double d = 1.2;
        BOOST_TEST(test("1.2", double_, phoenix::ref(d)));
        BOOST_TEST(test("2.2", double_, ++phoenix::ref(d)));
    }

    // test for denormalized numbers
    {
        BOOST_TEST(test("4.941e-324", double_, std::numeric_limits<double>::denorm_min()));
    }

    // test for #628: spirit::karma::generate generates 10.0e-04, but expecting 1.0e-03
    {
        BOOST_TEST(test("1.0", double_, 0.99999999999999829));
        BOOST_TEST(test("0.1", double_, 0.099999999999999829));
        BOOST_TEST(test("0.01", double_, 0.0099999999999999829));
        BOOST_TEST(test("1.0e-03", double_, 0.00099999999999999829));
        BOOST_TEST(test("1.0e-04", double_, 0.00009999999999999982));
        BOOST_TEST(test("1.0e-05", double_, 0.00000999999999999998));
    }

    return boost::report_errors();
}

