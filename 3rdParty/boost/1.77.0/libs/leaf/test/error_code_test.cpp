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

#include "_test_res.hpp"
#include "lightweight_test.hpp"

namespace leaf = boost::leaf;

struct e_wrapped_error_code { std::error_code value; };

template <class R>
void test()
{
#if __cplusplus >= 201703L
    {
        int r = leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_a::a0);
            },
            []( leaf::match<std::error_code, leaf::category<errc_a>, leaf::category<errc_b>> code )
            {
                std::error_code const & ec = code.matched;
                BOOST_TEST_EQ(&ec.category(), &cat_errc_a());
                BOOST_TEST_EQ(ec, errc_a::a0);
                return 42;
            },
            []
            {
                return -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
    {
        int r = leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_b::b0);
            },
            []( leaf::match<std::error_code, leaf::category<errc_a>, leaf::category<errc_b>> code )
            {
                std::error_code const & ec = code.matched;
                BOOST_TEST_EQ(&ec.category(), &cat_errc_b());
                BOOST_TEST_EQ(ec, errc_b::b0);
                return 42;
            },
            []
            {
                return -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
    {
        int r = leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_b::b0);
            },
            []( leaf::match<std::error_code, leaf::category<errc_a>, errc_b::b0> code )
            {
                std::error_code const & ec = code.matched;
                BOOST_TEST_EQ(&ec.category(), &cat_errc_b());
                BOOST_TEST_EQ(ec, errc_b::b0);
                return 42;
            },
            []
            {
                return -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#endif

    {
        int r = leaf::try_handle_all(
            []() -> R
            {
                return errc_a::a0; // testing without make_error_code
            },
            []( std::error_code const & ec )
            {
                BOOST_TEST(!leaf::is_error_id(ec));
                BOOST_TEST_EQ(ec, errc_a::a0);
                return 42;
            },
            []
            {
                return -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
    {
        int r = leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_a::a0);
            },
            []( leaf::match<leaf::condition<errc_a>, errc_a::a0> code )
            {
                std::error_code const & ec = code.matched;
                BOOST_TEST_EQ(ec, errc_a::a0);
                return 42;
            },
            []
            {
                return -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#if __cplusplus >= 201703L
    {
        int r = leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_a::a0);
            },
            []( leaf::match<std::error_code, errc_a::a0> code )
            {
                std::error_code const & ec = code.matched;
                BOOST_TEST_EQ(ec, errc_a::a0);
                return 42;
            },
            []
            {
                return -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#endif
    {
        int r = leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_a::a0);
            },
            []( leaf::match<leaf::condition<errc_a>, errc_a::a0> code )
            {
                std::error_code const & ec = code.matched;
                BOOST_TEST_EQ(ec, errc_a::a0);
                return 42;
            },
            []
            {
                return -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
    {
        int r = leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_a::a0);
            },
            []( leaf::match<leaf::condition<cond_x>, cond_x::x00> cond )
            {
                std::error_code const & ec = cond.matched;
                BOOST_TEST_EQ(ec, errc_a::a0);
                BOOST_TEST(ec==make_error_condition(cond_x::x00));
                return 42;
            },
            []
            {
                return -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#if __cplusplus >= 201703L
    {
        int r = leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_a::a0);
            },
            []( leaf::match<std::error_code, cond_x::x00> cond )
            {
                std::error_code const & ec = cond.matched;
                BOOST_TEST_EQ(ec, errc_a::a0);
                BOOST_TEST(ec==make_error_condition(cond_x::x00));
                return 42;
            },
            []
            {
                return -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#endif

    {
        int r = leaf::try_handle_all(
            []() -> R
            {
                return leaf::new_error( e_wrapped_error_code { make_error_code(errc_a::a0) } ).to_error_code();
            },
            []( e_wrapped_error_code const & wec )
            {
                std::error_code const & ec = wec.value;
                BOOST_TEST_EQ(ec, errc_a::a0);
                return 42;
            },
            []
            {
                return -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
    {
        int r = leaf::try_handle_all(
            []() -> R
            {
                return leaf::new_error( e_wrapped_error_code { make_error_code(errc_a::a0) } ).to_error_code();
            },
            []( leaf::match_value<leaf::condition<e_wrapped_error_code, errc_a>, errc_a::a0> code )
            {
                e_wrapped_error_code const & wec = code.matched;
                std::error_code const & ec = wec.value;
                BOOST_TEST_EQ(ec, errc_a::a0);
                return 42;
            },
            []
            {
                return -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#if __cplusplus >= 201703L
    {
        int r = leaf::try_handle_all(
            []() -> R
            {
                return leaf::new_error( e_wrapped_error_code { make_error_code(errc_a::a0) } ).to_error_code();
            },
            []( leaf::match_value<e_wrapped_error_code, errc_a::a0> code )
            {
                e_wrapped_error_code const & wec = code.matched;
                std::error_code const & ec = wec.value;
                BOOST_TEST_EQ(ec, errc_a::a0);
                return 42;
            },
            []
            {
                return -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#endif
    {
        int r = leaf::try_handle_all(
            []() -> R
            {
                return leaf::new_error( e_wrapped_error_code { make_error_code(errc_a::a0) } ).to_error_code();
            },
            []( leaf::match_value<leaf::condition<e_wrapped_error_code, cond_x>, cond_x::x00> cond )
            {
                e_wrapped_error_code const & wec = cond.matched;
                std::error_code const & ec = wec.value;
                BOOST_TEST_EQ(ec, errc_a::a0);
                BOOST_TEST(ec==make_error_condition(cond_x::x00));
                return 42;
            },
            []
            {
                return -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#if __cplusplus >= 201703L
    {
        int r = leaf::try_handle_all(
            []() -> R
            {
                return leaf::new_error( e_wrapped_error_code { make_error_code(errc_a::a0) } ).to_error_code();
            },
            []( leaf::match_value<e_wrapped_error_code, cond_x::x00> cond )
            {
                e_wrapped_error_code const & wec = cond.matched;
                std::error_code const & ec = wec.value;
                BOOST_TEST_EQ(ec, errc_a::a0);
                BOOST_TEST(ec==make_error_condition(cond_x::x00));
                return 42;
            },
            []
            {
                return -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#endif
}

template <class R>
void test_void()
{
#if __cplusplus >= 201703L
    {
        int r = 0;
        leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_a::a0);
            },
            [&]( leaf::match<std::error_code, leaf::category<errc_a>, leaf::category<errc_b>> code )
            {
                std::error_code const & ec = code.matched;
                BOOST_TEST_EQ(&ec.category(), &cat_errc_a());
                BOOST_TEST_EQ(ec, errc_a::a0);
                r = 42;
            },
            [&]
            {
                r = -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
    {
        int r = 0;
        leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_b::b0);
            },
            [&]( leaf::match<std::error_code, leaf::category<errc_a>, leaf::category<errc_b>> code )
            {
                std::error_code const & ec = code.matched;
                BOOST_TEST_EQ(&ec.category(), &cat_errc_b());
                BOOST_TEST_EQ(ec, errc_b::b0);
                r = 42;
            },
            [&]
            {
                r = -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
    {
        int r = 0;
        leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_b::b0);
            },
            [&]( leaf::match<std::error_code, leaf::category<errc_a>, errc_b::b0> code )
            {
                std::error_code const & ec = code.matched;
                BOOST_TEST_EQ(&ec.category(), &cat_errc_b());
                BOOST_TEST_EQ(ec, errc_b::b0);
                r = 42;
            },
            [&]
            {
                r = -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#endif

    {
        int r = 0;
        leaf::try_handle_all(
            [&]() -> R
            {
                return errc_a::a0; // testing without make_error_code
            },
            [&]( std::error_code const & ec )
            {
                BOOST_TEST(!leaf::is_error_id(ec));
                BOOST_TEST_EQ(ec, errc_a::a0);
                r = 42;
            },
            [&]
            {
                r = -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
    {
        int r = 0;
        leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_a::a0);
            },
            [&]( leaf::match<leaf::condition<errc_a>, errc_a::a0> code )
            {
                std::error_code const & ec = code.matched;
                BOOST_TEST_EQ(ec, errc_a::a0);
                r = 42;
            },
            [&]
            {
                r = -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#if __cplusplus >= 201703L
    {
        int r = 0;
        leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_a::a0);
            },
            [&]( leaf::match<std::error_code, errc_a::a0> code )
            {
                std::error_code const & ec = code.matched;
                BOOST_TEST_EQ(ec, errc_a::a0);
                r = 42;
            },
            [&]
            {
                r = -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#endif
    {
        int r = 0;
        leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_a::a0);
            },
            [&]( leaf::match<leaf::condition<errc_a>, errc_a::a0> code )
            {
                std::error_code const & ec = code.matched;
                BOOST_TEST_EQ(ec, errc_a::a0);
                r = 42;
            },
            [&]
            {
                r = -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
    {
        int r = 0;
        leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_a::a0);
            },
            [&]( leaf::match<leaf::condition<cond_x>, cond_x::x00> cond )
            {
                std::error_code const & ec = cond.matched;
                BOOST_TEST_EQ(ec, errc_a::a0);
                BOOST_TEST(ec==make_error_condition(cond_x::x00));
                r = 42;
            },
            [&]
            {
                r = -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#if __cplusplus >= 201703L
    {
        int r = 0;
        leaf::try_handle_all(
            []() -> R
            {
                return make_error_code(errc_a::a0);
            },
            [&]( leaf::match<std::error_code, cond_x::x00> cond )
            {
                std::error_code const & ec = cond.matched;
                BOOST_TEST_EQ(ec, errc_a::a0);
                BOOST_TEST(ec==make_error_condition(cond_x::x00));
                r = 42;
            },
            [&]
            {
                r = -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#endif

    {
        int r = 0;
        leaf::try_handle_all(
            []() -> R
            {
                return leaf::new_error( e_wrapped_error_code { make_error_code(errc_a::a0) } ).to_error_code();
            },
            [&]( e_wrapped_error_code const & wec )
            {
                std::error_code const & ec = wec.value;
                BOOST_TEST_EQ(ec, errc_a::a0);
                r = 42;
            },
            [&]
            {
                r = -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
    {
        int r = 0;
        leaf::try_handle_all(
            []() -> R
            {
                return leaf::new_error( e_wrapped_error_code { make_error_code(errc_a::a0) } ).to_error_code();
            },
            [&]( leaf::match_value<leaf::condition<e_wrapped_error_code, errc_a>, errc_a::a0> code )
            {
                e_wrapped_error_code const & wec = code.matched;
                std::error_code const & ec = wec.value;
                BOOST_TEST_EQ(ec, errc_a::a0);
                r = 42;
            },
            [&]
            {
                r = -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#if __cplusplus >= 201703L
    {
        int r = 0;
        leaf::try_handle_all(
            []() -> R
            {
                return leaf::new_error( e_wrapped_error_code { make_error_code(errc_a::a0) } ).to_error_code();
            },
            [&]( leaf::match_value<e_wrapped_error_code, errc_a::a0> code )
            {
                e_wrapped_error_code const & wec = code.matched;
                std::error_code const & ec = wec.value;
                BOOST_TEST_EQ(ec, errc_a::a0);
                r = 42;
            },
            [&]
            {
                r = -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#endif
    {
        int r = 0;
        leaf::try_handle_all(
            []() -> R
            {
                return leaf::new_error( e_wrapped_error_code { make_error_code(errc_a::a0) } ).to_error_code();
            },
            [&]( leaf::match_value<leaf::condition<e_wrapped_error_code, cond_x>, cond_x::x00> cond )
            {
                e_wrapped_error_code const & wec = cond.matched;
                std::error_code const & ec = wec.value;
                BOOST_TEST_EQ(ec, errc_a::a0);
                BOOST_TEST(ec==make_error_condition(cond_x::x00));
                r = 42;
            },
            [&]
            {
                r = -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#if __cplusplus >= 201703L
    {
        int r = 0;
        leaf::try_handle_all(
            []() -> R
            {
                return leaf::new_error( e_wrapped_error_code { make_error_code(errc_a::a0) } ).to_error_code();
            },
            [&]( leaf::match_value<e_wrapped_error_code, cond_x::x00> cond )
            {
                e_wrapped_error_code const & wec = cond.matched;
                std::error_code const & ec = wec.value;
                BOOST_TEST_EQ(ec, errc_a::a0);
                BOOST_TEST(ec==make_error_condition(cond_x::x00));
                r = 42;
            },
            [&]
            {
                r = -42;
            } );
        BOOST_TEST_EQ(r, 42);
    }
#endif
}

int main()
{
    test<leaf::result<int>>();
    test<test_res<int, std::error_code>>();
    test_void<leaf::result<void>>();
    test_void<test_res<void, std::error_code>>();
    return boost::report_errors();
}
