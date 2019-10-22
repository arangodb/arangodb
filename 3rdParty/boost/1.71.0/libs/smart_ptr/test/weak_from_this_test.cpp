#include <boost/config.hpp>

//
//  weak_from_this_test.cpp
//
//  Copyright (c) 2002, 2003, 2015 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//


#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/detail/lightweight_test.hpp>

class V: public boost::enable_shared_from_this<V>
{
};

void test()
{
    boost::shared_ptr<V> p( new V );

    boost::weak_ptr<V> q = p;
    BOOST_TEST( !q.expired() );

    boost::weak_ptr<V> q2 = p->weak_from_this();
    BOOST_TEST( !q2.expired() );
    BOOST_TEST( !(q < q2) && !(q2 < q) );

    V v2( *p );

    boost::weak_ptr<V> q3 = v2.weak_from_this();
    BOOST_TEST( q3.expired() );

    *p = V();

    boost::weak_ptr<V> q4 = p->weak_from_this();
    BOOST_TEST( !q4.expired() );
    BOOST_TEST( !(q < q4) && !(q4 < q) );
    BOOST_TEST( !(q2 < q4) && !(q4 < q2) );
}

int main()
{
    test();
    return boost::report_errors();
}
