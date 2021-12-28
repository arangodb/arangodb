//
//  sp_bml_unique_ptr_test.cpp
//
//  Copyright (c) 2012, 2015 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/core/lightweight_test.hpp>
#include <boost/move/unique_ptr.hpp>
#include <boost/type_traits/remove_reference.hpp>
#include <memory>
#include <utility>

struct X: public boost::enable_shared_from_this< X >
{
    static int instances;

    X()
    {
        ++instances;
    }

    ~X()
    {
        --instances;
    }

private:

    X( X const & );
    X & operator=( X const & );
};

int X::instances = 0;

struct Y
{
    static int instances;

    bool deleted_;

    Y(): deleted_( false )
    {
        ++instances;
    }

    ~Y()
    {
        BOOST_TEST( deleted_ );
        --instances;
    }

private:

    Y( Y const & );
    Y & operator=( Y const & );
};

int Y::instances = 0;

struct YD
{
    void operator()( Y* p ) const
    {
        if( p )
        {
            p->deleted_ = true;
            delete p;
        }
        else
        {
            BOOST_ERROR( "YD::operator()(0) called" );
        }
    }
};

template<class U, class T, class D> static void test_null_unique_ptr( boost::movelib::unique_ptr<T, D> p1, boost::movelib::unique_ptr<T, D> p2 )
{
    BOOST_TEST( T::instances == 0 );

    boost::shared_ptr<U> sp( boost::move( p1 ) );

    BOOST_TEST( sp.get() == 0 );
    BOOST_TEST( sp.use_count() == 0 );

    sp.reset( new T, typename boost::remove_reference<D>::type() );

    BOOST_TEST( sp.get() != 0 );
    BOOST_TEST( sp.use_count() == 1 );

    BOOST_TEST( T::instances == 1 );

    sp = boost::move( p2 );

    BOOST_TEST( sp.get() == 0 );
    BOOST_TEST( sp.use_count() == 0 );

    BOOST_TEST( T::instances == 0 );
}

int main()
{
    {
        BOOST_TEST( X::instances == 0 );

        boost::movelib::unique_ptr<X> p( new X );
        BOOST_TEST( X::instances == 1 );

        boost::shared_ptr<X> p2( boost::move( p ) );
        BOOST_TEST( X::instances == 1 );
        BOOST_TEST( p.get() == 0 );

        boost::shared_ptr<X> p3 = p2->shared_from_this();
        BOOST_TEST( p2 == p3 );
        BOOST_TEST( !(p2 < p3) && !(p3 < p2) );

        p2.reset();
        p3.reset();
        BOOST_TEST( X::instances == 0 );

        p2 = boost::movelib::unique_ptr<X>( new X );
        BOOST_TEST( X::instances == 1 );

        p2 = boost::movelib::unique_ptr<X>( new X );
        BOOST_TEST( X::instances == 1 );

        p3 = p2->shared_from_this();
        BOOST_TEST( p2 == p3 );
        BOOST_TEST( !(p2 < p3) && !(p3 < p2) );

        p2.reset();
        p3.reset();
        BOOST_TEST( X::instances == 0 );
    }

    {
        BOOST_TEST( X::instances == 0 );

        boost::movelib::unique_ptr<X> p( new X );
        BOOST_TEST( X::instances == 1 );

        boost::shared_ptr<X const> p2( boost::move( p ) );
        BOOST_TEST( X::instances == 1 );
        BOOST_TEST( p.get() == 0 );

        boost::shared_ptr<X const> p3 = p2->shared_from_this();
        BOOST_TEST( p2 == p3 );
        BOOST_TEST( !(p2 < p3) && !(p3 < p2) );

        p2.reset();
        p3.reset();
        BOOST_TEST( X::instances == 0 );

        p2 = boost::movelib::unique_ptr<X>( new X );
        BOOST_TEST( X::instances == 1 );

        p2 = boost::movelib::unique_ptr<X>( new X );
        BOOST_TEST( X::instances == 1 );

        p3 = p2->shared_from_this();
        BOOST_TEST( p2 == p3 );
        BOOST_TEST( !(p2 < p3) && !(p3 < p2) );

        p2.reset();
        p3.reset();
        BOOST_TEST( X::instances == 0 );
    }

    {
        BOOST_TEST( X::instances == 0 );

        boost::movelib::unique_ptr<X> p( new X );
        BOOST_TEST( X::instances == 1 );

        boost::shared_ptr<void> p2( boost::move( p ) );
        BOOST_TEST( X::instances == 1 );
        BOOST_TEST( p.get() == 0 );

        p2.reset();
        BOOST_TEST( X::instances == 0 );

        p2 = boost::movelib::unique_ptr<X>( new X );
        BOOST_TEST( X::instances == 1 );

        p2 = boost::movelib::unique_ptr<X>( new X );
        BOOST_TEST( X::instances == 1 );

        p2.reset();
        BOOST_TEST( X::instances == 0 );
    }

    {
        BOOST_TEST( Y::instances == 0 );

        boost::movelib::unique_ptr<Y, YD> p( new Y, YD() );
        BOOST_TEST( Y::instances == 1 );

        boost::shared_ptr<Y> p2( boost::move( p ) );
        BOOST_TEST( Y::instances == 1 );
        BOOST_TEST( p.get() == 0 );

        p2.reset();
        BOOST_TEST( Y::instances == 0 );

        p2 = boost::movelib::unique_ptr<Y, YD>( new Y, YD() );
        BOOST_TEST( Y::instances == 1 );

        p2 = boost::movelib::unique_ptr<Y, YD>( new Y, YD() );
        BOOST_TEST( Y::instances == 1 );

        p2.reset();
        BOOST_TEST( Y::instances == 0 );
    }

    {
        BOOST_TEST( Y::instances == 0 );

        YD yd;

        boost::movelib::unique_ptr<Y, YD&> p( new Y, yd );
        BOOST_TEST( Y::instances == 1 );

        boost::shared_ptr<Y> p2( boost::move( p ) );
        BOOST_TEST( Y::instances == 1 );
        BOOST_TEST( p.get() == 0 );

        p2.reset();
        BOOST_TEST( Y::instances == 0 );

        p2 = boost::movelib::unique_ptr<Y, YD&>( new Y, yd );
        BOOST_TEST( Y::instances == 1 );

        p2 = boost::movelib::unique_ptr<Y, YD&>( new Y, yd );
        BOOST_TEST( Y::instances == 1 );

        p2.reset();
        BOOST_TEST( Y::instances == 0 );
    }

    {
        BOOST_TEST( Y::instances == 0 );

        YD yd;

        boost::movelib::unique_ptr<Y, YD const&> p( new Y, yd );
        BOOST_TEST( Y::instances == 1 );

        boost::shared_ptr<Y> p2( boost::move( p ) );
        BOOST_TEST( Y::instances == 1 );
        BOOST_TEST( p.get() == 0 );

        p2.reset();
        BOOST_TEST( Y::instances == 0 );

        p2 = boost::movelib::unique_ptr<Y, YD const&>( new Y, yd );
        BOOST_TEST( Y::instances == 1 );

        p2 = boost::movelib::unique_ptr<Y, YD const&>( new Y, yd );
        BOOST_TEST( Y::instances == 1 );

        p2.reset();
        BOOST_TEST( Y::instances == 0 );
    }

    {
        test_null_unique_ptr<X>( boost::movelib::unique_ptr<X>(), boost::movelib::unique_ptr<X>() );
        test_null_unique_ptr<X const>( boost::movelib::unique_ptr<X>(), boost::movelib::unique_ptr<X>() );
        test_null_unique_ptr<void>( boost::movelib::unique_ptr<X>(), boost::movelib::unique_ptr<X>() );
        test_null_unique_ptr<void const>( boost::movelib::unique_ptr<X>(), boost::movelib::unique_ptr<X>() );
    }

    {
        test_null_unique_ptr<Y>( boost::movelib::unique_ptr<Y, YD>( 0, YD() ), boost::movelib::unique_ptr<Y, YD>( 0, YD() ) );
        test_null_unique_ptr<Y const>( boost::movelib::unique_ptr<Y, YD>( 0, YD() ), boost::movelib::unique_ptr<Y, YD>( 0, YD() ) );
        test_null_unique_ptr<void>( boost::movelib::unique_ptr<Y, YD>( 0, YD() ), boost::movelib::unique_ptr<Y, YD>( 0, YD() ) );
        test_null_unique_ptr<void const>( boost::movelib::unique_ptr<Y, YD>( 0, YD() ), boost::movelib::unique_ptr<Y, YD>( 0, YD() ) );
    }

    {
        YD yd;

        test_null_unique_ptr<Y>( boost::movelib::unique_ptr<Y, YD&>( 0, yd ), boost::movelib::unique_ptr<Y, YD&>( 0, yd ) );
        test_null_unique_ptr<Y const>( boost::movelib::unique_ptr<Y, YD&>( 0, yd ), boost::movelib::unique_ptr<Y, YD&>( 0, yd ) );
        test_null_unique_ptr<void>( boost::movelib::unique_ptr<Y, YD&>( 0, yd ), boost::movelib::unique_ptr<Y, YD&>( 0, yd ) );
        test_null_unique_ptr<void const>( boost::movelib::unique_ptr<Y, YD&>( 0, yd ), boost::movelib::unique_ptr<Y, YD&>( 0, yd ) );
    }

    return boost::report_errors();
}
