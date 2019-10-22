// Copyright David Abrahams, Daniel Wallin 2003.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter.hpp>
#include <boost/parameter/macros.hpp>
#include <boost/bind.hpp>
#include "basics.hpp"

namespace test {

    BOOST_PARAMETER_FUN(int, f, 2, 4, f_parameters)
    {
        p[test::_tester](
            p[test::_name]
          , p[test::_value || boost::bind(&test::value_default)]
          , p[test::_index | 999]
        );

        return 1;
    }

    BOOST_PARAMETER_NAME(foo)
    BOOST_PARAMETER_NAME(bar)

    struct baz_parameters
      : boost::parameter::parameters<
            boost::parameter::optional<test::tag::foo>
          , boost::parameter::optional<test::tag::bar>
        >
    {
    };

    BOOST_PARAMETER_FUN(int, baz, 0, 2, baz_parameters)
    {
        return 1;
    }
} // namespace test

#include <boost/ref.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>

int main()
{
    test::f(
        test::values(
            std::string("foo")
          , std::string("bar")
          , std::string("baz")
        )
      , std::string("foo")
      , std::string("bar")
      , std::string("baz")
    );
    BOOST_TEST_EQ(1, test::baz());

    int x = 56;
    test::f(
        test::values(std::string("foo"), 666.222, 56)
      , test::_index = boost::ref(x)
      , test::_name = std::string("foo")
    );

    return boost::report_errors();
}

