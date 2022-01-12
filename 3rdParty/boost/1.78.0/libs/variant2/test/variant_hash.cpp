
// Copyright 2020 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning( disable: 4244 ) // conversion from int to float, possible loss of data
#endif

#include <boost/variant2/variant.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/core/lightweight_test_trait.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/config/workaround.hpp>
#include <vector>

using namespace boost::variant2;

template<template<class...> class Hash, class T1, class T2, class T3> void test()
{
    variant<T1, T2, T3> v1( in_place_index_t<0>{} );
    std::size_t h1 = Hash<decltype(v1)>()( v1 );

    variant<T1, T2, T3> v2( in_place_index_t<1>{} );
    std::size_t h2 = Hash<decltype(v2)>()( v2 );

    variant<T1, T2, T3> v3( in_place_index_t<2>{} );
    std::size_t h3 = Hash<decltype(v3)>()( v3 );

    BOOST_TEST_NE( h1, h2 );
    BOOST_TEST_NE( h1, h3 );
    BOOST_TEST_NE( h2, h3 );
}

template<template<class...> class Hash, class T> void test2()
{
    variant<T> v1( 0 );
    std::size_t h1 = Hash<decltype(v1)>()( v1 );

    variant<T> v2( 1 );
    std::size_t h2 = Hash<decltype(v2)>()( v2 );

    variant<T> v3( 2 );
    std::size_t h3 = Hash<decltype(v3)>()( v3 );

    BOOST_TEST_NE( h1, h2 );
    BOOST_TEST_NE( h1, h3 );
    BOOST_TEST_NE( h2, h3 );
}

struct X
{
    int m = 0;
};

std::size_t hash_value( X const& x )
{
    return boost::hash<int>()( x.m );
}

struct Y {}; // no hash support

int main()
{
    test<std::hash, monostate, monostate, monostate>();
    test<std::hash, int, int, float>();

    test<boost::hash, monostate, monostate, monostate>();
    test<boost::hash, int, int, float>();

    test<boost::hash, monostate, X, std::vector<X>>();

    test2<std::hash, int>();
    test2<std::hash, float>();

    test2<boost::hash, int>();
    test2<boost::hash, float>();

#if !BOOST_WORKAROUND(BOOST_MSVC, < 1910) && ( !defined(_LIBCPP_STD_VER) || _LIBCPP_STD_VER > 11 )

    BOOST_TEST_TRAIT_FALSE(( detail::is_hash_enabled<Y> ));

#endif

    return boost::report_errors();
}
