// Boost.Range library
//
//  Copyright Neil Groves 2014. Use, modification and
//  distribution is subject to the Boost Software License, Version
//  1.0. (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt)
//
// For more information, see http://www.boost.org/libs/range/
//

#include <boost/detail/workaround.hpp>

#include <boost/range/iterator_range.hpp>
#include <boost/range/functions.hpp>
#include <boost/range/as_literal.hpp>
#include <boost/variant.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include <string>

namespace
{
    enum E
    {
        e1, e2, e3
    };

    void test_variant_report()
    {
        typedef boost::mpl::vector<
            E,
            std::string,
            boost::iterator_range<std::string::iterator>
        >::type args;

        typedef boost::make_variant_over<args>::type variant_t;

        variant_t v;
        std::string s;
        v = boost::iterator_range<std::string::iterator>(s.begin(), s.end());
        v = e2;
        v = std::string();

        // Rationale:
        // This is cast to const char* to guard against ambiguity in the case
        // where std::string::iterator it a char*
        v = static_cast<const char*>("");
    }
}

boost::unit_test::test_suite* init_unit_test_suite( int argc, char* argv[] )
{
    boost::unit_test::test_suite* test =
        BOOST_TEST_SUITE("iterator range and variant interoperability");

    test->add(BOOST_TEST_CASE(&test_variant_report));

    return test;
}
