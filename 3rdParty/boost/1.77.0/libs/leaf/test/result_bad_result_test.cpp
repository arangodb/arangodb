// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/detail/config.hpp>
#ifdef BOOST_LEAF_NO_EXCEPTIONS

#include <iostream>

int main()
{
    std::cout << "Unit test not applicable." << std::endl;
    return 0;
}

#else

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/pred.hpp>
#   include <boost/leaf/result.hpp>
#endif

#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

struct e_test { int value; };

int check( leaf::bad_result const &, leaf::match_value<e_test, 42> )
{
    return 1;
}

struct res { int val; };

int main()
{
    {
        int r = leaf::try_catch(
            []
            {
                leaf::result<int> r = leaf::new_error(e_test{42});
                (void) r.value();
                return 0;
            },
            check );
        BOOST_TEST_EQ(r, 1);
    }
    {
        int r = leaf::try_catch(
            []
            {
                leaf::result<int> const r = leaf::new_error(e_test{42});
                (void) r.value();
                return 0;
            },
            check );
        BOOST_TEST_EQ(r, 1);
    }
    {
        int r = leaf::try_catch(
            []
            {
                leaf::result<int> r = leaf::new_error(e_test{42});
                (void) *r;
                return 0;
            },
            check );
        BOOST_TEST_EQ(r, 1);
    }
    {
        int r = leaf::try_catch(
            []
            {
                leaf::result<int> const r = leaf::new_error(e_test{42});
                (void) *r;
                return 0;
            },
            check );
        BOOST_TEST_EQ(r, 1);
    }
    {
        int r = leaf::try_catch(
            []
            {
                leaf::result<res> r = leaf::new_error(e_test{42});
                (void) r->val;
                return 0;
            },
            check );
        BOOST_TEST_EQ(r, 1);
    }
    {
        int r = leaf::try_catch(
            []
            {
                leaf::result<res> const r = leaf::new_error(e_test{42});
                (void) r->val;
                return 0;
            },
            check );
        BOOST_TEST_EQ(r, 1);
    }
    return boost::report_errors();
}

#endif
