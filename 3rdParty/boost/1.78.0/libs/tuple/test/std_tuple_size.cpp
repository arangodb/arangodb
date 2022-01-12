// Copyright 2017 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/tuple/tuple.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/config.hpp>
#include <boost/config/pragma_message.hpp>

#if defined(BOOST_NO_CXX11_HDR_TUPLE)

BOOST_PRAGMA_MESSAGE("Skipping std::tuple_size tests for lack of <tuple>")
int main() {}

#else

#include <tuple>

template<class Tp> void test( std::size_t x )
{
    BOOST_TEST_EQ( std::tuple_size< Tp >::value, x );
    BOOST_TEST_EQ( std::tuple_size< typename Tp::inherited >::value, x );
}

struct V
{
};

int main()
{
    test< boost::tuple<> >( 0 );
    test< boost::tuple<V> >( 1 );
    test< boost::tuple<V, V> >( 2 );
    test< boost::tuple<V, V, V> >( 3 );
    test< boost::tuple<V, V, V, V> >( 4 );
    test< boost::tuple<V, V, V, V, V> >( 5 );
    test< boost::tuple<V, V, V, V, V, V> >( 6 );
    test< boost::tuple<V, V, V, V, V, V, V> >( 7 );
    test< boost::tuple<V, V, V, V, V, V, V, V> >( 8 );
    test< boost::tuple<V, V, V, V, V, V, V, V, V> >( 9 );
    test< boost::tuple<V, V, V, V, V, V, V, V, V, V> >( 10 );

#if !defined(BOOST_NO_CXX11_DECLTYPE)

    BOOST_TEST_EQ( std::tuple_size<decltype(boost::make_tuple())>::value, 0 );
    BOOST_TEST_EQ( std::tuple_size<decltype(boost::make_tuple(1))>::value, 1 );
    BOOST_TEST_EQ( std::tuple_size<decltype(boost::make_tuple(1, 2))>::value, 2 );
    BOOST_TEST_EQ( std::tuple_size<decltype(boost::make_tuple(1, 2, 3))>::value, 3 );
    BOOST_TEST_EQ( std::tuple_size<decltype(boost::make_tuple(1, 2, 3, 4))>::value, 4 );
    BOOST_TEST_EQ( std::tuple_size<decltype(boost::make_tuple(1, 2, 3, 4, 5))>::value, 5 );
    BOOST_TEST_EQ( std::tuple_size<decltype(boost::make_tuple(1, 2, 3, 4, 5, 6))>::value, 6 );
    BOOST_TEST_EQ( std::tuple_size<decltype(boost::make_tuple(1, 2, 3, 4, 5, 6, 7))>::value, 7 );
    BOOST_TEST_EQ( std::tuple_size<decltype(boost::make_tuple(1, 2, 3, 4, 5, 6, 7, 8))>::value, 8 );
    BOOST_TEST_EQ( std::tuple_size<decltype(boost::make_tuple(1, 2, 3, 4, 5, 6, 7, 8, 9))>::value, 9 );
    BOOST_TEST_EQ( std::tuple_size<decltype(boost::make_tuple(1, 2, 3, 4, 5, 6, 7, 8, 9, 10))>::value, 10 );

#endif

    return boost::report_errors();
}

#endif
