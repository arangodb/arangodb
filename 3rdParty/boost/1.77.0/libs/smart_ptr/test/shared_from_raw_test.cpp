//
//  shared_from_raw_test - based on shared_from_this_test
//
//  Copyright (c) 2002, 2003, 2014 Peter Dimov
//
//  Distributed under the Boost Software License, Version 1.0.
//
//  See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
//

#if defined(__GNUC__) && __GNUC__ > 4
# pragma GCC diagnostic ignored "-Wdelete-non-virtual-dtor"
#endif

#include <boost/smart_ptr/enable_shared_from_raw.hpp>
#include <boost/shared_ptr.hpp>

#include <boost/core/lightweight_test.hpp>

//

class X
{
public:

    virtual void f() = 0;

protected:

    ~X() {}
};

class Y
{
public:

    virtual boost::shared_ptr<X> getX() = 0;

protected:

    ~Y() {}
};

boost::shared_ptr<Y> createY();

void test()
{
    boost::shared_ptr<Y> py = createY();
    BOOST_TEST(py.get() != 0);
    BOOST_TEST(py.use_count() == 1);

    try
    {
        boost::shared_ptr<X> px = py->getX();
        BOOST_TEST(px.get() != 0);
        BOOST_TEST(py.use_count() == 2);

        px->f();

#if !defined( BOOST_NO_RTTI )
        boost::shared_ptr<Y> py2 = boost::dynamic_pointer_cast<Y>(px);
        BOOST_TEST(py.get() == py2.get());
        BOOST_TEST(!(py < py2 || py2 < py));
        BOOST_TEST(py.use_count() == 3);
#endif
    }
    catch( boost::bad_weak_ptr const& )
    {
        BOOST_ERROR( "py->getX() failed" );
    }
}

void test2();
void test3();

int main()
{
    test();
    test2();
    test3();
    return boost::report_errors();
}

// virtual inheritance to stress the implementation
// (prevents Y* -> impl*, enable_shared_from_raw* -> impl* casts)

class impl: public X, public virtual Y, public virtual boost::enable_shared_from_raw
{
public:

    virtual void f()
    {
    }

    virtual boost::shared_ptr<X> getX()
    {
        boost::shared_ptr<impl> pi = boost::shared_from_raw( this );
        BOOST_TEST( pi.get() == this );
        return pi;
    }
};

// intermediate impl2 to stress the implementation

class impl2: public impl
{
};

boost::shared_ptr<Y> createY()
{
    boost::shared_ptr<Y> pi(new impl2);
    return pi;
}

void test2()
{
    boost::shared_ptr<Y> pi(static_cast<impl2*>(0));
}

//

struct V: public boost::enable_shared_from_raw
{
};

void test3()
{
    boost::shared_ptr<V> p( new V );

    try
    {
        boost::shared_ptr<V> q = boost::shared_from_raw( p.get() );
        BOOST_TEST( p == q );
        BOOST_TEST( !(p < q) && !(q < p) );
    }
    catch( boost::bad_weak_ptr const & )
    {
        BOOST_ERROR( "shared_from_this( p.get() ) failed" );
    }

    V v2( *p );

    try
    {
        // shared_from_raw differs from shared_from_this;
        // it will not throw here and will create a shared_ptr

        boost::shared_ptr<V> r = boost::shared_from_raw( &v2 );

        // check if the shared_ptr is correct and that it does
        // not share ownership with p

        BOOST_TEST( r.get() == &v2 );
        BOOST_TEST( p != r );
        BOOST_TEST( (p < r) || (r < p) );
    }
    catch( boost::bad_weak_ptr const & )
    {
        BOOST_ERROR("shared_from_raw( &v2 ) failed");
    }

    try
    {
        *p = V();
        boost::shared_ptr<V> r = boost::shared_from_raw( p.get() );
        BOOST_TEST( p == r );
        BOOST_TEST( !(p < r) && !(r < p) );
    }
    catch( boost::bad_weak_ptr const & )
    {
        BOOST_ERROR("shared_from_raw( p.get() ) threw bad_weak_ptr after *p = V()");
    }
}
