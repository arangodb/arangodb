// Copyright Daniel Wallin 2006.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/parameter/name.hpp>

namespace test {

    BOOST_PARAMETER_NAME(kw)
    BOOST_PARAMETER_NAME(unused)

    template <typename Args>
    int f(Args const& args)
    {
        return args[test::_kw | 1.f];
    }
} // namespace test

#include <boost/parameter/aux_/maybe.hpp>
#include <boost/core/lightweight_test.hpp>

int main()
{
    BOOST_TEST_EQ(0, test::f((test::_kw = 0, test::_unused = 0)));
    BOOST_TEST_EQ(1, test::f(test::_unused = 0));
    BOOST_TEST_EQ(
        1
      , test::f((
            test::_kw = boost::parameter::aux::maybe<int>()
          , test::_unused = 0
        ))
    );
    BOOST_TEST_EQ(
        2
      , test::f((
            test::_kw = boost::parameter::aux::maybe<int>(2)
          , test::_unused = 0
        ))
    );
    return boost::report_errors();
}

