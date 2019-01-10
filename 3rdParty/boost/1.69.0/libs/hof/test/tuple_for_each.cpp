/*=============================================================================
    Copyright (c) 2017 Paul Fultz II
    tuple_for_each.cpp
    Distributed under the Boost Software License, Version 1.0. (See accompanying
    file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
==============================================================================*/
#include <boost/hof/unpack.hpp>
#include <boost/hof/proj.hpp>
#include <boost/hof/function.hpp>
#include <boost/hof/reveal.hpp>
#include "test.hpp"

struct tuple_for_each_f
{
    template<class Sequence, class F>
    constexpr auto operator()(Sequence&& s, F && f) const BOOST_HOF_RETURNS
    (
        boost::hof::unpack(boost::hof::proj(boost::hof::forward<F>(f)))(boost::hof::forward<Sequence>(s)), boost::hof::forward<F>(f)
    );
};

BOOST_HOF_STATIC_FUNCTION(tuple_for_each) = tuple_for_each_f{};

BOOST_HOF_TEST_CASE()
{
    std::tuple<int, short, char> tp{ 1, 2, 3 };

    {
        int s = 0;

        tuple_for_each( tp, [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 123 );
    }

    {
        int s = 0;

        tuple_for_each( std::move(tp), [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 123 );
    }
}
BOOST_HOF_TEST_CASE()
{
    std::tuple<int, short, char> const tp{ 1, 2, 3 };

    {
        int s = 0;

        tuple_for_each( tp, [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 123 );
    }

    {
        int s = 0;

        tuple_for_each( std::move(tp), [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 123 );
    }
}

// #if defined( __clang_major__ ) && __clang_major__ == 3 && __clang_minor__ < 8
// #else
BOOST_HOF_TEST_CASE()
{
    std::tuple<std::unique_ptr<int>, std::unique_ptr<int>, std::unique_ptr<int>> tp{ std::unique_ptr<int>(new int(1)), std::unique_ptr<int>(new int(2)), std::unique_ptr<int>(new int(3)) };

    int s = 0;

    tuple_for_each( std::move(tp), [&]( std::unique_ptr<int> p ){ s = s * 10 + *p; } );

    BOOST_HOF_TEST_CHECK( s == 123 );
}

BOOST_HOF_TEST_CASE()
{
    auto tp = boost::hof::pack(1, 2, 3);

    {
        int s = 0;

        tuple_for_each( tp, [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 123 );
    }

    {
        int s = 0;

        tuple_for_each( std::move(tp), [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 123 );
    }
}
BOOST_HOF_TEST_CASE()
{
    const auto tp = boost::hof::pack(1, 2, 3);

    {
        int s = 0;

        tuple_for_each( tp, [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 123 );
    }

    {
        int s = 0;

        tuple_for_each( std::move(tp), [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 123 );
    }
}
// #endif
BOOST_HOF_TEST_CASE()
{
    std::pair<int, short> tp{ 1, 2 };

    {
        int s = 0;

        tuple_for_each( tp, [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 12 );
    }

    {
        int s = 0;

        tuple_for_each( std::move(tp), [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 12 );
    }
}  
BOOST_HOF_TEST_CASE()
{
    std::pair<int, short> const tp{ 1, 2 };

    {
        int s = 0;

        tuple_for_each( tp, [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 12 );
    }

    {
        int s = 0;

        tuple_for_each( std::move(tp), [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 12 );
    }
}
BOOST_HOF_TEST_CASE()
{
    std::array<int, 3> tp{{ 1, 2, 3 }};

    {
        int s = 0;

        tuple_for_each( tp, [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 123 );
    }

    {
        int s = 0;

        tuple_for_each( std::move(tp), [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 123 );
    }
}
BOOST_HOF_TEST_CASE()
{
    std::array<int, 3> const tp{{ 1, 2, 3 }};

    {
        int s = 0;

        tuple_for_each( tp, [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 123 );
    }

    {
        int s = 0;

        tuple_for_each( std::move(tp), [&]( int x ){ s = s * 10 + x; } );

        BOOST_HOF_TEST_CHECK( s == 123 );
    }
}
BOOST_HOF_TEST_CASE()
{
    std::tuple<> tp;

    BOOST_HOF_TEST_CHECK( tuple_for_each( tp, 11 ) == 11 );
    BOOST_HOF_TEST_CHECK( tuple_for_each( std::move( tp ), 12 ) == 12 );
}

BOOST_HOF_TEST_CASE()
{
    BOOST_HOF_TEST_CHECK( tuple_for_each( boost::hof::pack(), 11 ) == 11 );
    BOOST_HOF_STATIC_TEST_CHECK( tuple_for_each( boost::hof::pack(), 11 ) == 11 );
}
BOOST_HOF_TEST_CASE()
{
    std::array<int, 0> tp;

    BOOST_HOF_TEST_CHECK( tuple_for_each( tp, 11 ) == 11 );
    BOOST_HOF_TEST_CHECK( tuple_for_each( std::move( tp ), 12 ) == 12 );
}

struct assert_is_integral
{
    template<class T> constexpr bool operator()( T ) const
    {
        BOOST_HOF_STATIC_TEST_CHECK( std::is_integral<T>::value );
        return true;
    }
};

BOOST_HOF_TEST_CASE()
{
#if !BOOST_HOF_HAS_CONSTEXPR_TUPLE
    auto r = tuple_for_each( std::tuple<int, short, char>{1, 2, 3}, assert_is_integral() );
#else
    constexpr auto r = tuple_for_each( std::tuple<int, short, char>{1, 2, 3}, assert_is_integral() );
#endif
    (void)r;
}

BOOST_HOF_TEST_CASE()
{
#if !BOOST_HOF_HAS_CONSTEXPR_TUPLE
    auto r = tuple_for_each( boost::hof::pack(1, 2, 3), assert_is_integral() );
#else
    constexpr auto r = tuple_for_each( boost::hof::pack(1, 2, 3), assert_is_integral() );
#endif
    (void)r;
}
