
// weak_from_test.cpp
//
// Copyright 2019 Peter Dimov
//
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/smart_ptr/enable_shared_from.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/core/lightweight_test.hpp>

//

class X
{
private:

    int m_;

public:

    X(): m_() {}
};

class Y: public boost::enable_shared_from
{
};

class Z: public X, public Y
{
public:

    boost::weak_ptr<Z> weak_from_this()
    {
        return boost::weak_from( this );
    }
};

void null_deleter( void const* )
{
}

int main()
{
    boost::shared_ptr<Z> sp( new Z );
    boost::weak_ptr<Z> p( sp );

    {
        boost::weak_ptr<Z> q = sp->weak_from_this();

        BOOST_TEST_EQ( p.lock(), q.lock() );
        BOOST_TEST( !( p < q ) && !( q < p ) );
    }

    Z v2( *sp );

    {
        boost::weak_ptr<Z> q = v2.weak_from_this();
        BOOST_TEST( q.expired() );
    }

    *sp = Z();

    {
        boost::weak_ptr<Z> q = sp->weak_from_this();

        BOOST_TEST_EQ( p.lock(), q.lock() );
        BOOST_TEST( !( p < q ) && !( q < p ) );
    }

    {
        boost::shared_ptr<Z> sp2( sp.get(), null_deleter );
    }

    {
        boost::weak_ptr<Z> q = sp->weak_from_this();

        BOOST_TEST_EQ( p.lock(), q.lock() );
        BOOST_TEST( !( p < q ) && !( q < p ) );
    }

    {
        boost::weak_ptr<Z> p2 = sp->weak_from_this();

        BOOST_TEST( !p.expired() );
        BOOST_TEST( !p2.expired() );

        sp.reset();

        BOOST_TEST( p.expired() );
        BOOST_TEST( p2.expired() );
    }

    return boost::report_errors();
}
