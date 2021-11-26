// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/smart_ptr/owner_hash.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/core/lightweight_test.hpp>

template<class T> std::size_t hash_( T const& t )
{
    return boost::owner_hash<T>()( t );
}

int main()
{
    boost::shared_ptr<int> p1( new int );
    boost::shared_ptr<int> p2( p1 );

    BOOST_TEST_EQ( hash_( p1 ), hash_( p2 ) );

    boost::shared_ptr<int> p3( new int );

    BOOST_TEST_NE( hash_( p1 ), hash_( p3 ) );

    boost::shared_ptr<int> p4;
    boost::shared_ptr<int> p5;

    BOOST_TEST_EQ( hash_( p4 ), hash_( p5 ) );
    BOOST_TEST_NE( hash_( p4 ), hash_( p3 ) );

    boost::shared_ptr<int> p6( static_cast<int*>(0) );

    BOOST_TEST_NE( hash_( p4 ), hash_( p6 ) );

    boost::shared_ptr<void> p7( p1 );

    BOOST_TEST_EQ( hash_( p1 ), hash_( p7 ) );

    boost::shared_ptr<void> p8;

    BOOST_TEST_NE( hash_( p1 ), hash_( p8 ) );
    BOOST_TEST_EQ( hash_( p4 ), hash_( p8 ) );

    boost::weak_ptr<int> q1( p1 );

    BOOST_TEST_EQ( hash_( p1 ), hash_( q1 ) );

    boost::weak_ptr<int> q2( p1 );

    BOOST_TEST_EQ( hash_( q1 ), hash_( q2 ) );

    boost::weak_ptr<int> q3( p3 );

    BOOST_TEST_NE( hash_( p1 ), hash_( q3 ) );
    BOOST_TEST_NE( hash_( q1 ), hash_( q3 ) );

    boost::weak_ptr<int> q4;

    BOOST_TEST_EQ( hash_( p4 ), hash_( q4 ) );
    BOOST_TEST_NE( hash_( q1 ), hash_( q4 ) );

    boost::weak_ptr<void> q5;

    BOOST_TEST_EQ( hash_( q4 ), hash_( q5 ) );

    boost::weak_ptr<void> q7( p7 );

    BOOST_TEST_EQ( hash_( p1 ), hash_( q7 ) );
    BOOST_TEST_EQ( hash_( q1 ), hash_( q7 ) );

    p1.reset();
    p2.reset();
    p3.reset();
    p7.reset();

    BOOST_TEST( q1.expired() );
    BOOST_TEST( q2.expired() );
    BOOST_TEST( q3.expired() );
    BOOST_TEST( q7.expired() );

    BOOST_TEST_EQ( hash_( q1 ), hash_( q2 ) );
    BOOST_TEST_EQ( hash_( q1 ), hash_( q7 ) );
    BOOST_TEST_NE( hash_( q1 ), hash_( q3 ) );
    BOOST_TEST_NE( hash_( q1 ), hash_( q4 ) );

    return boost::report_errors();
}
