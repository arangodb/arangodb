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

namespace leaf = boost::leaf;

template <int> struct info { int value; };

enum class my_error_code
{
    ok,
    error1,
    error2,
    error3
};

struct e_my_error_code { my_error_code value; };

struct e_std_error_code { std::error_code value; };

template <class R>
leaf::result<R> f( my_error_code ec )
{
    if( ec==my_error_code::ok )
        return R(42);
    else
        return leaf::new_error(ec, e_my_error_code{ec}, info<1>{1}, info<2>{2}, info<3>{3});
}

template <class R, class Errc>
leaf::result<R> f_errc( Errc ec )
{
    return leaf::new_error(make_error_code(ec), info<1>{1}, info<2>{2}, info<3>{3});
}

template <class R, class Errc>
leaf::result<R> f_errc_wrapped( Errc ec )
{
    return leaf::new_error(e_std_error_code{make_error_code(ec)}, info<1>{1}, info<2>{2}, info<3>{3});
}

int main()
{
    // void, try_handle_some (success)
    {
        int c=0;
        leaf::result<void> r = leaf::try_handle_some(
            [&c]() -> leaf::result<void>
            {
                BOOST_LEAF_AUTO(answer, f<int>(my_error_code::ok));
                c = answer;
                return { };
            },
            [&c]( leaf::error_info const & unmatched )
            {
                BOOST_TEST_EQ(c, 0);
                c = 1;
                return unmatched.error();
            } );
        BOOST_TEST(r);
        BOOST_TEST_EQ(c, 42);
    }

    // void, try_handle_some (failure, matched)
    {
        int c=0;
        leaf::result<void> r = leaf::try_handle_some(
            [&c]() -> leaf::result<void>
            {
                BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                c = answer;
                return { };
            },
            [&c]( my_error_code ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 1;
            } );
        BOOST_TEST_EQ(c, 1);
        BOOST_TEST(r);
    }

    // void, try_handle_some (failure, matched), match cond_x (single enum value)
    {
        int c=0;
        leaf::result<void> r = leaf::try_handle_some(
            [&c]() -> leaf::result<void>
            {
                BOOST_LEAF_AUTO(answer, f_errc<int>(errc_a::a0));
                c = answer;
                return { };
            },
            [&c]( leaf::match<leaf::condition<cond_x>, cond_x::x00> ec, info<1> const & x, info<2> const & y )
            {
                BOOST_TEST_EQ(ec.matched, make_error_code(errc_a::a0));
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 1;
            } );
        BOOST_TEST_EQ(c, 1);
        BOOST_TEST(r);
    }

    // void, try_handle_some (failure, matched), match cond_x (wrapped std::error_code)
    {
        int c=0;
        leaf::result<void> r = leaf::try_handle_some(
            [&c]() -> leaf::result<void>
            {
                BOOST_LEAF_AUTO(answer, f_errc_wrapped<int>(errc_a::a0));
                c = answer;
                return { };
            },
            [&c]( leaf::match_value<leaf::condition<e_std_error_code, cond_x>, cond_x::x00> ec, info<1> const & x, info<2> const & y )
            {
                BOOST_TEST_EQ(ec.matched.value, make_error_code(errc_a::a0));
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 1;
            } );
        BOOST_TEST_EQ(c, 1);
        BOOST_TEST(r);
    }

    // void, try_handle_some (failure, matched), match enum (single enum value)
    {
        int c=0;
        leaf::result<void> r = leaf::try_handle_some(
            [&c]() -> leaf::result<void>
            {
                BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                c = answer;
                return { };
            },
            [&c]( leaf::match<my_error_code, my_error_code::error2> )
            {
                BOOST_TEST_EQ(c, 0);
                c = 1;
            },
            [&c]( leaf::match<my_error_code, my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            } );
        BOOST_TEST_EQ(c, 2);
        BOOST_TEST(r);
    }

    // void, try_handle_some (failure, matched), match enum (multiple enum values)
    {
        int c=0;
        leaf::result<void> r = leaf::try_handle_some(
            [&c]() -> leaf::result<void>
            {
                BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                c = answer;
                return { };
            },
            [&c]( leaf::match<my_error_code, my_error_code::error2> )
            {
                BOOST_TEST_EQ(c, 0);
                c = 1;
            },
            [&c]( leaf::match<my_error_code, my_error_code::error2, my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            } );
        BOOST_TEST_EQ(c, 2);
        BOOST_TEST(r);
    }

    // void, try_handle_some (failure, matched), match value (single value)
    {
        int c=0;
        leaf::result<void> r = leaf::try_handle_some(
            [&c]() -> leaf::result<void>
            {
                BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                c = answer;
                return { };
            },
            [&c]( leaf::match_value<e_my_error_code, my_error_code::error2> )
            {
                BOOST_TEST_EQ(c, 0);
                c = 1;
            },
            [&c]( leaf::match_value<e_my_error_code, my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched.value==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            } );
        BOOST_TEST_EQ(c, 2);
        BOOST_TEST(r);
    }

    // void, try_handle_some (failure, matched), match value (multiple values)
    {
        int c=0;
        leaf::result<void> r = leaf::try_handle_some(
            [&c]() -> leaf::result<void>
            {
                BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                c = answer;
                return { };
            },
            [&c]( leaf::match_value<e_my_error_code, my_error_code::error2> )
            {
                BOOST_TEST_EQ(c, 0);
                c = 1;
            },
            [&c]( leaf::match_value<e_my_error_code, my_error_code::error2, my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched.value==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            } );
        BOOST_TEST_EQ(c, 2);
        BOOST_TEST(r);
    }

    // void, try_handle_some (failure, initially not matched)
    {
        int c=0;
        leaf::try_handle_all(
            [&c]
            {
                leaf::result<void> r = leaf::try_handle_some(
                    [&c]() -> leaf::result<void>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        c = answer;
                        return { };
                    },
                    [&c]( info<4> )
                    {
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(!r);
                BOOST_TEST_EQ(c, 0);
                return r;
            },
            [&c]( my_error_code ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]()
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    // void, try_handle_some (failure, initially not matched), match cond_x (single enum value)
    {
        int c=0;
        leaf::try_handle_all(
            [&c]
            {
                leaf::result<void> r = leaf::try_handle_some(
                    [&c]() -> leaf::result<void>
                    {
                        BOOST_LEAF_AUTO(answer, f_errc<int>(errc_a::a0));
                        c = answer;
                        return { };
                    },
                    [&c]( leaf::match<leaf::condition<cond_x>, cond_x::x11> )
                    {
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(!r);
                BOOST_TEST_EQ(c, 0);
                return r;
            },
            [&c]( leaf::match<leaf::condition<cond_x>, cond_x::x00> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(ec.matched, make_error_code(errc_a::a0));
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]()
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    // void, try_handle_some (failure, initially not matched), match cond_x (wrapped std::error_code)
    {
        int c=0;
        leaf::try_handle_all(
            [&c]
            {
                leaf::result<void> r = leaf::try_handle_some(
                    [&c]() -> leaf::result<void>
                    {
                        BOOST_LEAF_AUTO(answer, f_errc_wrapped<int>(errc_a::a0));
                        c = answer;
                        return { };
                    },
                    [&c]( leaf::match_value<leaf::condition<e_std_error_code, cond_x>, cond_x::x11> )
                    {
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(!r);
                BOOST_TEST_EQ(c, 0);
                return r;
            },
            [&c]( leaf::match_value<leaf::condition<e_std_error_code, cond_x>, cond_x::x00> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(ec.matched.value, make_error_code(errc_a::a0));
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]()
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    // void, try_handle_some (failure, initially not matched), match enum (single enum value)
    {
        int c=0;
        leaf::try_handle_all(
            [&c]
            {
                leaf::result<void> r = leaf::try_handle_some(
                    [&c]() -> leaf::result<void>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        c = answer;
                        return { };
                    },
                    [&c]( leaf::match<my_error_code, my_error_code::error2> )
                    {
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(!r);
                BOOST_TEST_EQ(c, 0);
                return r;
            },
            [&c]( leaf::match<my_error_code, my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]()
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    // void, try_handle_some (failure, initially not matched), match enum (multiple enum values)
    {
        int c=0;
        leaf::try_handle_all(
            [&c]
            {
                leaf::result<void> r = leaf::try_handle_some(
                    [&c]() -> leaf::result<void>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        c = answer;
                        return { };
                    },
                    [&c]( leaf::match<my_error_code, my_error_code::error2> )
                    {
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(!r);
                BOOST_TEST_EQ(c, 0);
                return r;
            },
            [&c]( leaf::match<my_error_code, my_error_code::error2, my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]()
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    // void, try_handle_some (failure, initially not matched), match value (single value)
    {
        int c=0;
        leaf::try_handle_all(
            [&c]
            {
                leaf::result<void> r = leaf::try_handle_some(
                    [&c]() -> leaf::result<void>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        c = answer;
                        return { };
                    },
                    [&c]( leaf::match_value<e_my_error_code, my_error_code::error2> )
                    {
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(!r);
                BOOST_TEST_EQ(c, 0);
                return r;
            },
            [&c]( leaf::match_value<e_my_error_code, my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched.value==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]()
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    // void, try_handle_some (failure, initially not matched), match value (multiple values)
    {
        int c=0;
        leaf::try_handle_all(
            [&c]
            {
                leaf::result<void> r = leaf::try_handle_some(
                    [&c]() -> leaf::result<void>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        c = answer;
                        return { };
                    },
                    [&c]( leaf::match_value<e_my_error_code, my_error_code::error2> )
                    {
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(!r);
                BOOST_TEST_EQ(c, 0);
                return r;
            },
            [&c]( leaf::match_value<e_my_error_code, my_error_code::error2, my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched.value==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]()
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 2);
    }

    // void, try_handle_some (failure, initially matched)
    {
        int c=0;
        leaf::try_handle_all(
            [&c]
            {
                leaf::result<void> r = leaf::try_handle_some(
                    [&c]() -> leaf::result<void>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        c = answer;
                        return { };
                    },
                    [&c]( my_error_code ec, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST(ec==my_error_code::error1);
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(r);
                BOOST_TEST_EQ(c, 1);
                return r;
            },
            [&c]( info<4> )
            {
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]()
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 1);
    }

    // void, try_handle_some (failure, initially matched), match cond_x (single enum value)
    {
        int c=0;
        leaf::try_handle_all(
            [&c]
            {
                leaf::result<void> r = leaf::try_handle_some(
                    [&c]() -> leaf::result<void>
                    {
                        BOOST_LEAF_AUTO(answer, f_errc<int>(errc_a::a0));
                        c = answer;
                        return { };
                    },
                    [&c]( leaf::match<leaf::condition<cond_x>, cond_x::x00> ec, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST_EQ(ec.matched, make_error_code(errc_a::a0));
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(r);
                BOOST_TEST_EQ(c, 1);
                return r;
            },
            [&c]( leaf::match<leaf::condition<cond_x>, cond_x::x11> )
            {
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]()
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 1);
    }

    // void, try_handle_some (failure, initially matched), match cond_x (wrapped std::error_code)
    {
        int c=0;
        leaf::try_handle_all(
            [&c]
            {
                leaf::result<void> r = leaf::try_handle_some(
                    [&c]() -> leaf::result<void>
                    {
                        BOOST_LEAF_AUTO(answer, f_errc_wrapped<int>(errc_a::a0));
                        c = answer;
                        return { };
                    },
                    [&c]( leaf::match_value<leaf::condition<e_std_error_code, cond_x>, cond_x::x00> ec, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST_EQ(ec.matched.value, make_error_code(errc_a::a0));
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(r);
                BOOST_TEST_EQ(c, 1);
                return r;
            },
            [&c]( leaf::match_value<leaf::condition<e_std_error_code, cond_x>, cond_x::x11> )
            {
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]()
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 1);
    }
    // void, try_handle_some (failure, initially matched), match enum (single enum value)
    {
        int c=0;
        leaf::try_handle_all(
            [&c]
            {
                leaf::result<void> r = leaf::try_handle_some(
                    [&c]() -> leaf::result<void>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        c = answer;
                        return { };
                    },
                    [&c]( leaf::match<my_error_code, my_error_code::error1> ec, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST(ec.matched==my_error_code::error1);
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(r);
                BOOST_TEST_EQ(c, 1);
                return r;
            },
            [&c]( leaf::match<my_error_code, my_error_code::error2> )
            {
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]()
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 1);
    }

    // void, try_handle_some (failure, initially matched), match enum (multiple enum values)
    {
        int c=0;
        leaf::try_handle_all(
            [&c]
            {
                leaf::result<void> r = leaf::try_handle_some(
                    [&c]() -> leaf::result<void>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        c = answer;
                        return { };
                    },
                    [&c]( leaf::match<my_error_code, my_error_code::error2, my_error_code::error1> ec, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST(ec.matched==my_error_code::error1);
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(r);
                BOOST_TEST_EQ(c, 1);
                return r;
            },
            [&c]( leaf::match<my_error_code, my_error_code::error2> )
            {
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]()
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 1);
    }

    // void, try_handle_some (failure, initially matched), match value (single value)
    {
        int c=0;
        leaf::try_handle_all(
            [&c]
            {
                leaf::result<void> r = leaf::try_handle_some(
                    [&c]() -> leaf::result<void>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        c = answer;
                        return { };
                    },
                    [&c]( leaf::match_value<e_my_error_code, my_error_code::error1> ec, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST(ec.matched.value==my_error_code::error1);
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(r);
                BOOST_TEST_EQ(c, 1);
                return r;
            },
            [&c]( leaf::match_value<e_my_error_code, my_error_code::error2> )
            {
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]()
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 1);
    }

    // void, try_handle_some (failure, initially matched), match value (multiple values)
    {
        int c=0;
        leaf::try_handle_all(
            [&c]
            {
                leaf::result<void> r = leaf::try_handle_some(
                    [&c]() -> leaf::result<void>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        c = answer;
                        return { };
                    },
                    [&c]( leaf::match_value<e_my_error_code, my_error_code::error2,my_error_code::error1> ec, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST(ec.matched.value==my_error_code::error1);
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        BOOST_TEST_EQ(c, 0);
                        c = 1;
                    } );
                BOOST_TEST(r);
                BOOST_TEST_EQ(c, 1);
                return r;
            },
            [&c]( leaf::match_value<e_my_error_code, my_error_code::error2> )
            {
                BOOST_TEST_EQ(c, 0);
                c = 2;
            },
            [&c]()
            {
                BOOST_TEST_EQ(c, 0);
                c = 3;
            } );
        BOOST_TEST_EQ(c, 1);
    }

    //////////////////////////////////////

    // int, try_handle_some (success)
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                BOOST_LEAF_AUTO(answer, f<int>(my_error_code::ok));
                return answer;
            },
            []( leaf::error_info const & unmatched )
            {
                return unmatched.error();
            } );
        BOOST_TEST_EQ(*r, 42);
    }

    // int, try_handle_some (failure, matched)
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                return answer;
            },
            []( my_error_code ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 1;
            } );
        BOOST_TEST(r);
        BOOST_TEST_EQ(*r, 1);
    }

    // int, try_handle_some (failure, matched), match cond_x (single enum value)
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                BOOST_LEAF_AUTO(answer, f_errc<int>(errc_a::a0));
                return answer;
            },
            []( leaf::match<leaf::condition<cond_x>, cond_x::x11> )
            {
                return 1;
            },
            []( leaf::match<leaf::condition<cond_x>, cond_x::x00> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(ec.matched, make_error_code(errc_a::a0));
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            } );
        BOOST_TEST(r);
        BOOST_TEST_EQ(*r, 2);
    }

    // int, try_handle_some (failure, matched), match enum (single enum value)
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                return answer;
            },
            []( leaf::match<my_error_code,my_error_code::error2> )
            {
                return 1;
            },
            []( leaf::match<my_error_code,my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            } );
        BOOST_TEST(r);
        BOOST_TEST_EQ(*r, 2);
    }

    // int, try_handle_some (failure, matched), match enum (multiple enum values)
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                return answer;
            },
            []( leaf::match<my_error_code,my_error_code::error2> )
            {
                return 1;
            },
            []( leaf::match<my_error_code,my_error_code::error2,my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            } );
        BOOST_TEST(r);
        BOOST_TEST_EQ(*r, 2);
    }

    // int, try_handle_some (failure, matched), match value (single value)
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                return answer;
            },
            []( leaf::match_value<e_my_error_code,my_error_code::error2> )
            {
                return 1;
            },
            []( leaf::match_value<e_my_error_code,my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched.value==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            } );
        BOOST_TEST(r);
        BOOST_TEST_EQ(*r, 2);
    }

    // int, try_handle_some (failure, matched), match value (multiple values)
    {
        leaf::result<int> r = leaf::try_handle_some(
            []() -> leaf::result<int>
            {
                BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                return answer;
            },
            []( leaf::match_value<e_my_error_code,my_error_code::error2> )
            {
                return 1;
            },
            []( leaf::match_value<e_my_error_code,my_error_code::error2,my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched.value==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            } );
        BOOST_TEST(r);
        BOOST_TEST_EQ(*r, 2);
    }

    // int, try_handle_some (failure, initially not matched)
    {
        int r = leaf::try_handle_all(
            []
            {
                leaf::result<int> r = leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        return answer;
                    },
                    []( info<4> )
                    {
                        return 1;
                    } );
                BOOST_TEST(!r);
                return r;
            },
            []( my_error_code ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    // int, try_handle_some (failure, initially not matched), match cond_x (single enum value)
    {
        int r = leaf::try_handle_all(
            []
            {
                leaf::result<int> r = leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        BOOST_LEAF_AUTO(answer, f_errc<int>(errc_a::a0));
                        return answer;
                    },
                    []( leaf::match<leaf::condition<cond_x>, cond_x::x11> )
                    {
                        return 1;
                    } );
                BOOST_TEST(!r);
                return r;
            },
            []( leaf::match<leaf::condition<cond_x>, cond_x::x00> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST_EQ(ec.matched, make_error_code(errc_a::a0));
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    // int, try_handle_some (failure, initially not matched), match enum (single enum value)
    {
        int r = leaf::try_handle_all(
            []
            {
                leaf::result<int> r = leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        return answer;
                    },
                    []( leaf::match<my_error_code,my_error_code::error2> )
                    {
                        return 1;
                    } );
                BOOST_TEST(!r);
                return r;
            },
            []( leaf::match<my_error_code,my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    // int, try_handle_some (failure, initially not matched), match enum (multiple enum values)
    {
        int r = leaf::try_handle_all(
            []
            {
                leaf::result<int> r = leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        return answer;
                    },
                    []( leaf::match<my_error_code,my_error_code::error2> )
                    {
                        return 1;
                    } );
                BOOST_TEST(!r);
                return r;
            },
            []( leaf::match<my_error_code,my_error_code::error2,my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    // int, try_handle_some (failure, initially not matched), match value (single value)
    {
        int r = leaf::try_handle_all(
            []
            {
                leaf::result<int> r = leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        return answer;
                    },
                    []( leaf::match_value<e_my_error_code,my_error_code::error2> )
                    {
                        return 1;
                    } );
                BOOST_TEST(!r);
                return r;
            },
            []( leaf::match_value<e_my_error_code,my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched.value==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    // int, try_handle_some (failure, initially not matched), match value (multiple values)
    {
        int r = leaf::try_handle_all(
            []
            {
                leaf::result<int> r = leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        return answer;
                    },
                    []( leaf::match_value<e_my_error_code,my_error_code::error2> )
                    {
                        return 1;
                    } );
                BOOST_TEST(!r);
                return r;
            },
            []( leaf::match_value<e_my_error_code,my_error_code::error2,my_error_code::error1> ec, info<1> const & x, info<2> y )
            {
                BOOST_TEST(ec.matched.value==my_error_code::error1);
                BOOST_TEST_EQ(x.value, 1);
                BOOST_TEST_EQ(y.value, 2);
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 2);
    }

    // int, try_handle_some (failure, initially matched)
    {
        int r = leaf::try_handle_all(
            []
            {
                leaf::result<int> r = leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        return answer;
                    },
                    []( my_error_code ec, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST(ec==my_error_code::error1);
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        return 1;
                    } );
                BOOST_TEST(r);
                return r;
            },
            []( info<4> )
            {
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 1);
    }

    // int, try_handle_some (failure, initially matched), match enum (single enum value)
    {
        int r = leaf::try_handle_all(
            []
            {
                leaf::result<int> r = leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        return answer;
                    },
                    []( leaf::match<my_error_code,my_error_code::error1> ec, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST(ec.matched==my_error_code::error1);
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        return 1;
                    } );
                BOOST_TEST(r);
                return r;
            },
            []( leaf::match<my_error_code,my_error_code::error2> )
            {
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 1);
    }

    // int, try_handle_some (failure, initially matched), match enum (multiple enum values)
    {
        int r = leaf::try_handle_all(
            []
            {
                leaf::result<int> r = leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        return answer;
                    },
                    []( leaf::match<my_error_code,my_error_code::error2,my_error_code::error1> ec, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST(ec.matched==my_error_code::error1);
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        return 1;
                    } );
                BOOST_TEST(r);
                return r;
            },
            []( leaf::match<my_error_code,my_error_code::error2> )
            {
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 1);
    }

    // int, try_handle_some (failure, initially matched), match value (single value)
    {
        int r = leaf::try_handle_all(
            []
            {
                leaf::result<int> r = leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        return answer;
                    },
                    []( leaf::match_value<e_my_error_code,my_error_code::error1> ec, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST(ec.matched.value==my_error_code::error1);
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        return 1;
                    } );
                BOOST_TEST(r);
                return r;
            },
            []( leaf::match_value<e_my_error_code,my_error_code::error2> )
            {
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 1);
    }

    // int, try_handle_some (failure, initially matched), match value (multiple values)
    {
        int r = leaf::try_handle_all(
            []
            {
                leaf::result<int> r = leaf::try_handle_some(
                    []() -> leaf::result<int>
                    {
                        BOOST_LEAF_AUTO(answer, f<int>(my_error_code::error1));
                        return answer;
                    },
                    []( leaf::match_value<e_my_error_code,my_error_code::error2,my_error_code::error1> ec, info<1> const & x, info<2> y )
                    {
                        BOOST_TEST(ec.matched.value==my_error_code::error1);
                        BOOST_TEST_EQ(x.value, 1);
                        BOOST_TEST_EQ(y.value, 2);
                        return 1;
                    } );
                BOOST_TEST(r);
                return r;
            },
            []( leaf::match_value<e_my_error_code,my_error_code::error2> )
            {
                return 2;
            },
            []
            {
                return 3;
            } );
        BOOST_TEST_EQ(r, 1);
    }

    return boost::report_errors();
}
