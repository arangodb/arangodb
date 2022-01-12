// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/result.hpp>
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/pred.hpp>
#endif

#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

template <int> struct info { int value; };

template <class T>
leaf::result<T> test()
{
    leaf::result<T> r1 = leaf::new_error(info<42>{40});
    leaf::result<T> r2 = r1.load(info<1>{});
    leaf::result<T> r3 = r2.load(info<2>{2}, []{ return info<3>{3}; });
    leaf::result<T> r4 = r3.load([](info<42> & x){ ++x.value; });
    leaf::result<T> r5 = r4.load([](info<42> & x){ ++x.value; }, [](info<1> & x){ ++x.value; });
    return r5;
}

int main()
{
    {
        int r = leaf::try_handle_all(
            []
            {
                return test<int>();
            },
            []( leaf::match_value<info<42>, 42>, leaf::match_value<info<1>, 1>, leaf::match_value<info<2>, 2>, leaf::match_value<info<3>, 3> )
            {
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                BOOST_LEAF_CHECK(test<void>());
                return 0;
            },
            []( leaf::match_value<info<42>, 42>, leaf::match_value<info<1>, 1>, leaf::match_value<info<2>, 2>, leaf::match_value<info<3>, 3> )
            {
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 1);
    }
    return boost::report_errors();
}
