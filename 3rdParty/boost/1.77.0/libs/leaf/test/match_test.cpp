// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/handle_errors.hpp>
#   include <boost/leaf/pred.hpp>
#   include <boost/leaf/result.hpp>
#endif

#include "_test_ec.hpp"
#include "lightweight_test.hpp"
#include <exception>

namespace leaf = boost::leaf;

enum class my_error { e1=1, e2, e3 };

struct e_my_error { int value; };

#if __cplusplus >= 201703L
template <my_error value>
constexpr bool cmp_my_error( my_error const & e ) noexcept
{
    return e == value;
};

template <int S>
constexpr bool e_my_error_gt( e_my_error const & e ) noexcept
{
  return e.value > S;
}
#endif

struct my_exception: std::exception
{
    int value;
    bool operator==(int);
};

template <class M, class E>
bool test(E const & e )
{
    if( M::evaluate(e) )
    {
        M m{e};
        BOOST_TEST(e == m.matched);
        return true;
    }
    else
        return false;
}

int main()
{
    {
        int e = 42;

        BOOST_TEST(( test<leaf::match<int, 42>>(e) ));
        BOOST_TEST(( !test<leaf::match<int, 41>>(e) ));
        BOOST_TEST(( test<leaf::match<int, 42, 41>>(e) ));

        BOOST_TEST(( !test<leaf::if_not<leaf::match<int, 42>>>(e) ));
        BOOST_TEST(( test<leaf::if_not<leaf::match<int, 41>>>(e) ));
        BOOST_TEST(( !test<leaf::if_not<leaf::match<int, 42, 41>>>(e) ));
    }

    {
        my_error e = my_error::e1;

        BOOST_TEST(( test<leaf::match<my_error, my_error::e1>>(e) ));
        BOOST_TEST(( !test<leaf::match<my_error, my_error::e2>>(e) ));
        BOOST_TEST(( test<leaf::match<my_error, my_error::e2, my_error::e1>>(e) ));

        BOOST_TEST(( !test<leaf::if_not<leaf::match<my_error, my_error::e1>>>(e) ));
        BOOST_TEST(( test<leaf::if_not<leaf::match<my_error, my_error::e2>>>(e) ));
        BOOST_TEST(( !test<leaf::if_not<leaf::match<my_error, my_error::e2, my_error::e1>>>(e) ));
    }

    {
        std::error_code e = errc_a::a0;

        BOOST_TEST(( test<leaf::match<leaf::condition<cond_x>, cond_x::x00>>(e) ));
        BOOST_TEST(( !test<leaf::match<leaf::condition<cond_x>, cond_x::x11>>(e) ));
        BOOST_TEST(( test<leaf::match<leaf::condition<cond_x>, cond_x::x11, cond_x::x00>>(e) ));


        BOOST_TEST(( !test<leaf::if_not<leaf::match<leaf::condition<cond_x>, cond_x::x00>>>(e) ));
        BOOST_TEST(( test<leaf::if_not<leaf::match<leaf::condition<cond_x>, cond_x::x11>>>(e) ));
        BOOST_TEST(( !test<leaf::if_not<leaf::match<leaf::condition<cond_x>, cond_x::x11, cond_x::x00>>>(e) ));

#if __cplusplus >= 201703L
        BOOST_TEST(( test<leaf::match<std::error_code, errc_a::a0>>(e) ));
        BOOST_TEST(( !test<leaf::match<std::error_code, errc_a::a2>>(e) ));
        BOOST_TEST(( test<leaf::match<std::error_code, errc_a::a2, errc_a::a0>>(e) ));

        BOOST_TEST(( !test<leaf::if_not<leaf::match<std::error_code, errc_a::a0>>>(e) ));
        BOOST_TEST(( test<leaf::if_not<leaf::match<std::error_code, errc_a::a2>>>(e) ));
        BOOST_TEST(( !test<leaf::if_not<leaf::match<std::error_code, errc_a::a2, errc_a::a0>>>(e) ));
#endif
    }

#if __cplusplus >= 201703L
    {
        my_error e = my_error::e1;

        BOOST_TEST(( test<leaf::match<my_error, cmp_my_error<my_error::e1>>>(e) ));
        BOOST_TEST(( !test<leaf::match<my_error, cmp_my_error<my_error::e2>>>(e) ));
    }
#endif

    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::new_error(my_error::e1);
            },

            []( leaf::match<my_error, my_error::e1> )
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
                return leaf::new_error(my_error::e1);
            },

            []( leaf::match<my_error, my_error::e2> )
            {
                return 1;
            },

            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::new_error(my_error::e1);
            },

            []( leaf::if_not<leaf::match<my_error, my_error::e1>> )
            {
                return 1;
            },

            []( my_error e )
            {
                return 2;
            },

            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::new_error();
            },

            []( leaf::if_not<leaf::match<my_error, my_error::e1>> )
            {
                return 1;
            },

            []( my_error e )
            {
                return 2;
            },

            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 3);
    }

#if __cplusplus >= 201703L
    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::new_error(my_error::e1);
            },

            []( leaf::match<my_error, cmp_my_error<my_error::e1>> )
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
                return leaf::new_error(my_error::e1);
            },

            []( leaf::match<my_error, cmp_my_error<my_error::e2>> )
            {
                return 1;
            },

            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    {
        int r = leaf::try_handle_all(
            []() -> leaf::result<int>
            {
                return leaf::new_error(e_my_error{42});
            },

            []( leaf::match<e_my_error, e_my_error_gt<41>> m )
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
                return leaf::new_error(e_my_error{42});
            },

            []( leaf::match<e_my_error, e_my_error_gt<42>> m )
            {
                return 1;
            },

            []
            {
                return 2;
            } );
        BOOST_TEST_EQ(r, 2);
    }
#endif

    return boost::report_errors();
}
