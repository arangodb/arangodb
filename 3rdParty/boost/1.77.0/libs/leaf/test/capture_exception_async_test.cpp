// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/leaf/detail/config.hpp>
#if defined(BOOST_LEAF_NO_EXCEPTIONS) || defined(BOOST_LEAF_NO_THREADS)

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
#include <vector>
#include <future>
#include <iterator>
#include <algorithm>

namespace leaf = boost::leaf;

template <int> struct info { int value; };

struct fut_info
{
    int a;
    int b;
    int result;
    std::future<int> fut;
};

template <class H, class F>
std::vector<fut_info> launch_tasks( int task_count, F f )
{
    BOOST_LEAF_ASSERT(task_count>0);
    std::vector<fut_info> fut;
    std::generate_n( std::back_inserter(fut), task_count,
        [=]
        {
            int const a = rand();
            int const b = rand();
            int const res = (rand()%10) - 5;
            return fut_info { a, b, res, std::async( std::launch::async,
                [=]
                {
                    return leaf::capture(leaf::make_shared_context<H>(), f, a, b, res);
                } ) };
        } );
    return fut;
}

int main()
{
    int received_a, received_b;
    auto error_handlers = std::make_tuple(
        [&received_a, &received_b]( info<1> const & x1, info<2> const & x2, info<4> const & )
        {
            received_a = x1.value;
            received_b = x2.value;
            return -1;
        },
        []
        {
            return -2;
        } );

    {
        std::vector<fut_info> fut = launch_tasks<decltype(error_handlers)>(
            100,
            []( int a, int b, int res )
            {
                if( res >= 0 )
                    return res;
                else
                    throw leaf::exception(info<1>{a}, info<2>{b}, info<3>{});
            } );

        for( auto & f : fut )
        {
            f.fut.wait();
            received_a = received_b = 0;
            int r = leaf::try_catch(
                [&]
                {
                    auto load = leaf::on_error( info<4>{} );

                    // Calling future_get is required in order to make the on_error (above) work.
                    return leaf::future_get(f.fut);
                },
                error_handlers );
            if( f.result>=0 )
                BOOST_TEST_EQ(r, f.result);
            else
            {
                BOOST_TEST_EQ(r, -1);
                BOOST_TEST_EQ(received_a, f.a);
                BOOST_TEST_EQ(received_b, f.b);
            }
        }
    }

    {
        std::vector<fut_info> fut = launch_tasks<decltype(error_handlers)>(
            100,
            []( int a, int b, int res )
            {
                if( res >= 0 )
                    return res;
                else
                    throw leaf::exception(info<1>{a}, info<2>{b}, info<3>{});
            } );

        for( auto & f : fut )
        {
            f.fut.wait();
            received_a = received_b = 0;
            int r = leaf::try_catch(
                [&]
                {
                    auto load = leaf::on_error( info<4>{} );

                    return leaf::try_catch(
                        [&]
                        {
                            // Not calling future_get, a on_error in this scope won't work correctly.
                            // This is to verify that the on_error in the outer scope (above) works.
                            return f.fut.get();
                        },
                        []() -> int
                        {
                            throw;
                        } );
                },
                error_handlers );
            if( f.result>=0 )
                BOOST_TEST_EQ(r, f.result);
            else
            {
                BOOST_TEST_EQ(r, -1);
                BOOST_TEST_EQ(received_a, f.a);
                BOOST_TEST_EQ(received_b, f.b);
            }
        }
    }

    return boost::report_errors();
}

#endif
