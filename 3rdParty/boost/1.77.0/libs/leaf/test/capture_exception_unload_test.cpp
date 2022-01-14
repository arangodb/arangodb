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
#   include <boost/leaf/capture.hpp>
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/exception.hpp>
#   include <boost/leaf/on_error.hpp>
#endif

#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

template <int> struct info { int value; };

template <class... E, class F>
void test( F f_ )
{
    auto f =
        [=]
        {
            try
            {
                leaf::capture( std::make_shared<leaf::leaf_detail::polymorphic_context_impl<leaf::context<info<1>, info<2>, info<3>>>>(), f_);
                BOOST_TEST(false);
                return std::exception_ptr();
            }
            catch(...)
            {
                return std::current_exception();
            }
        };

    {
        int c=0;
        auto ep = f();
        leaf::try_catch(
            [&ep]
            {
                return std::rethrow_exception(ep);
            },
            [&c]( info<1> const & x )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(c, 0);
                c = 1;
            },
            [&c]
            {
                BOOST_TEST_EQ(c, 0);
                c = 2;
            } );
        BOOST_TEST_EQ(c, 1);
    }

    {
        int c=0;
        auto ep = f();
        leaf::try_catch(
            [&ep]
            {
                return std::rethrow_exception(ep);
            },
            [&c]( info<2> const & x )
            {
                BOOST_TEST_EQ(x.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 1;
            },
            [&c]
            {
                BOOST_TEST_EQ(c, 0);
                c = 2;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    {
        auto ep = f();
        int what = leaf::try_catch(
            [&ep]
            {
                std::rethrow_exception(ep); return 0;
            },
            []( info<1> const & x )
            {
                BOOST_TEST_EQ(x.value, 1);
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(what, 1);
    }

    {
        auto ep = f();
        int what = leaf::try_catch(
            [&ep]
            {
                std::rethrow_exception(ep); return 0;
            },
            []( info<2> const & x )
            {
                BOOST_TEST_EQ(x.value, 2);
                return 1;
            },
            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(what, 2);
    }

    {
        auto ep = f();
        bool what = leaf::try_catch(
            [&ep]
            {
                std::rethrow_exception(ep); return true;
            },
            []( info<1> const & x, info<2> const & )
            {
                return true;
            },
            []( info<1> const & x, info<3> const & y )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 3);
                return false;
            },
            []( info<1> const & x )
            {
                return true;
            },
            []
            {
                return true;
            } );
        BOOST_TEST(!what);
    }

    {
        auto ep = f();
        bool what = leaf::try_catch(
            [&ep]
            {
                std::rethrow_exception(ep); return false;
            },
            []( info<1> const & x, info<2> const & )
            {
                return false;
            },
            []( info<1> const & x, info<3> const & y )
            {
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 3);
                return true;
            },
            []( info<1> const & x )
            {
                return false;
            },
            []
            {
                return false;
            } );
        BOOST_TEST(what);
    }
}

int main()
{
    test<info<1>, info<2>, info<3>>(
        []
        {
            throw leaf::exception(info<1>{1}, info<3>{3}); // Derives from leaf::leaf::error_id
        } );
    test<info<1>, info<2>, info<3>>(
        []
        {
            auto load = leaf::on_error( info<1>{1}, info<3>{3} );
            throw leaf::exception(); // Derives from leaf::leaf::error_id
        } );
    test<info<1>, info<2>, info<3>>(
        []
        {
            auto load = leaf::on_error( info<1>{1}, info<3>{3} );
            throw std::exception(); // Does not derive from leaf::leaf::error_id
        } );
    return boost::report_errors();
}

#endif
