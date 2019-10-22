
//          Copyright Oliver Kowalke 2009.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <cstdio>

#include <boost/assert.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/move/move.hpp>
#include <boost/range.hpp>
#include <boost/ref.hpp>
#include <boost/test/unit_test.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/utility.hpp>

#include <boost/coroutine/symmetric_coroutine.hpp>

namespace coro = boost::coroutines;

bool value1 = false;
int value2 = 0;
std::string value3;

typedef void( * coro_fn_void)(coro::symmetric_coroutine< void* >::yield_type &);

coro::symmetric_coroutine< void >::call_type * term_coro = 0;

struct X
{
    int i;

    X() :
        i( 0)
    {}

    X( int i_) :
        i( i_)
    {}
};

X * p = 0;

struct Y
{
    Y() 
    { value2 = 7; }

    ~Y() 
    { value2 = 0; }
};
   
template< typename X, typename I >
void trampoline( coro::symmetric_coroutine< void* >::yield_type & yield)
{
    void * vp = yield.get();
    X * x = static_cast< X * >( vp);
    I i( yield);
    x->d = & i;
    i.suspend();
    i.run();
}

struct B
{
    virtual ~B() {}

    virtual void foo() = 0;
};

class D : public B
{
public:
    int                                                 count;
    coro::symmetric_coroutine< void* >::call_type       call;
    coro::symmetric_coroutine< void* >::yield_type  *   yield;

    D( coro::symmetric_coroutine< void* >::yield_type & yield_) :
        B(),
        count( 0),
        call(),
        yield( & yield_)
    {}

    void foo() {}

    void resume()
    { call( 0); }

    void suspend()
    { ( *yield)(); }

    void run()
    {
        while ( yield && * yield)
        {
            ++count;
            suspend();
        }
    }
};

struct T
{
    D   *   d;

    T() :
        d( 0)
    {}
};

class copyable
{
public:
    bool    state;

    copyable() :
        state( false)
    {}

    copyable( int) :
        state( true)
    {}

    void operator()( coro::symmetric_coroutine< bool >::yield_type & yield)
    {
        if ( yield)
            value1 = yield.get();
    }
};

class moveable
{
private:
    BOOST_MOVABLE_BUT_NOT_COPYABLE( moveable)

public:
    bool    state;

    moveable() :
        state( false)
    {}

    moveable( int) :
        state( true)
    {}

    moveable( BOOST_RV_REF( moveable) other) :
        state( false)
    { std::swap( state, other.state); }

    moveable & operator=( BOOST_RV_REF( moveable) other)
    {
        if ( this == & other) return * this;
        moveable tmp( boost::move( other) );
        std::swap( state, tmp.state);
        return * this;
    }

    void operator()( coro::symmetric_coroutine< int >::yield_type &)
    { value1 = state; }
};

void empty( coro::symmetric_coroutine< void >::yield_type &) {}

void f2( coro::symmetric_coroutine< void >::yield_type &)
{ ++value2; }

void f3( coro::symmetric_coroutine< X >::yield_type & yield)
{ value2 = yield.get().i; }

void f4( coro::symmetric_coroutine< X& >::yield_type & yield)
{
    X & x = yield.get();
    p = & x;
}

void f5( coro::symmetric_coroutine< X* >::yield_type & yield)
{ p = yield.get(); }

void f6( coro::symmetric_coroutine< void >::yield_type & yield)
{
    Y y;
    yield( *term_coro);
}

void f7( coro::symmetric_coroutine< int >::yield_type & yield)
{
    value2 = yield.get();
    yield( *term_coro);
    value2 = yield.get();
}

template< typename E >
void f9( coro::symmetric_coroutine< void >::yield_type &, E const& e)
{ throw e; }

void f10( coro::symmetric_coroutine< int >::yield_type & yield,
          coro::symmetric_coroutine< int >::call_type & other)
{
    int i = yield.get();
    yield( other, i);
    value2 = yield.get();
}

void f101( coro::symmetric_coroutine< int >::yield_type & yield)
{ value2 = yield.get(); }

void f11( coro::symmetric_coroutine< void >::yield_type & yield,
          coro::symmetric_coroutine< void >::call_type & other)
{
    yield( other);
    value2 = 7;
}

void f111( coro::symmetric_coroutine< void >::yield_type &)
{ value2 = 3; }

void f12( coro::symmetric_coroutine< X& >::yield_type & yield,
          coro::symmetric_coroutine< X& >::call_type & other)
{
    yield( other, yield.get());
    p = & yield.get();
}

void f121( coro::symmetric_coroutine< X& >::yield_type & yield)
{ p = & yield.get(); }

void f14( coro::symmetric_coroutine< int >::yield_type & yield,
          coro::symmetric_coroutine< std::string >::call_type & other)
{
    std::string str( boost::lexical_cast< std::string >( yield.get() ) );
    yield( other, str);
    value2 = yield.get();
}

void f141( coro::symmetric_coroutine< std::string >::yield_type & yield)
{ value3 = yield.get(); }

void f15( coro::symmetric_coroutine< int >::yield_type & yield,
          int offset,
          coro::symmetric_coroutine< int >::call_type & other)
{
    int x = yield.get();
    value2 += x + offset;
    yield( other, x);
    x = yield.get();
    value2 += x + offset;
    yield( other, x);
}

void f151( coro::symmetric_coroutine< int >::yield_type & yield,
          int offset)
{
    int x = yield.get();
    value2 += x + offset;
    yield();
    x = yield.get();
    value2 += x + offset;
}

void f16( coro::symmetric_coroutine< int >::yield_type & yield)
{
    while ( yield)
    {
        value2 = yield.get();
        yield();
    }
}

void test_move()
{
    {
        coro::symmetric_coroutine< void >::call_type coro1;
        coro::symmetric_coroutine< void >::call_type coro2( empty);
        BOOST_CHECK( ! coro1);
        BOOST_CHECK( coro2);
        coro1 = boost::move( coro2);
        BOOST_CHECK( coro1);
        BOOST_CHECK( ! coro2);
    }

    {
        value1 = false;
        copyable cp( 3);
        BOOST_CHECK( cp.state);
        BOOST_CHECK( ! value1);
        coro::symmetric_coroutine< bool >::call_type coro( cp);
        coro( true);
        BOOST_CHECK( cp.state);
        BOOST_CHECK( value1);
    }

    {
        value1 = false;
        moveable mv( 7);
        BOOST_CHECK( mv.state);
        BOOST_CHECK( ! value1);
        coro::symmetric_coroutine< int >::call_type coro( boost::move( mv) );
        coro( 7);
        BOOST_CHECK( ! mv.state);
        BOOST_CHECK( value1);
    }
}

void test_complete()
{
    value2 = 0;

    coro::symmetric_coroutine< void >::call_type coro( f2);
    BOOST_CHECK( coro);
    coro();
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( ( int)1, value2);
}

void test_yield()
{
    value2 = 0;

    coro::symmetric_coroutine< int >::call_type coro3(
        boost::bind( f151, _1, 3) );
    BOOST_CHECK( coro3);
    coro::symmetric_coroutine< int >::call_type coro2(
        boost::bind( f15, _1, 2, boost::ref( coro3) ) );
    BOOST_CHECK( coro2);
    coro::symmetric_coroutine< int >::call_type coro1(
        boost::bind( f15, _1, 1, boost::ref( coro2) ) );
    BOOST_CHECK( coro1);

    BOOST_CHECK_EQUAL( ( int)0, value2);
    coro1( 1);
    BOOST_CHECK( coro3);
    BOOST_CHECK( coro2);
    BOOST_CHECK( coro1);
    BOOST_CHECK_EQUAL( ( int)9, value2);
    coro1( 2);
    BOOST_CHECK( ! coro3);
    BOOST_CHECK( coro2);
    BOOST_CHECK( coro1);
    BOOST_CHECK_EQUAL( ( int)21, value2);
}

void test_pass_value()
{
    value2 = 0;

    X x(7);
    BOOST_CHECK_EQUAL( ( int)7, x.i);
    BOOST_CHECK_EQUAL( 0, value2);
    coro::symmetric_coroutine< X >::call_type coro( f3);
    BOOST_CHECK( coro);
    coro(7);
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( ( int)7, x.i);
    BOOST_CHECK_EQUAL( 7, value2);
}

void test_pass_reference()
{
    p = 0;

    X x;
    coro::symmetric_coroutine< X& >::call_type coro( f4);
    BOOST_CHECK( coro);
    coro( x);
    BOOST_CHECK( ! coro);
    BOOST_CHECK( p == & x);
}

void test_pass_pointer()
{
    p = 0;

    X x;
    coro::symmetric_coroutine< X* >::call_type coro( f5);
    BOOST_CHECK( coro);
    coro( & x);
    BOOST_CHECK( ! coro);
    BOOST_CHECK( p == & x);
}

void test_unwind()
{
    value2 = 0;
    {
        coro::symmetric_coroutine< void >::call_type coro( f6);
        coro::symmetric_coroutine< void >::call_type coro_e( empty);
        BOOST_CHECK( coro);
        BOOST_CHECK( coro_e);
        term_coro = & coro_e;
        BOOST_CHECK_EQUAL( ( int) 0, value2);
        coro();
        BOOST_CHECK( coro);
        BOOST_CHECK_EQUAL( ( int) 7, value2);
    }
    BOOST_CHECK_EQUAL( ( int) 0, value2);
}

void test_no_unwind()
{
    value2 = 0;
    {
        coro::symmetric_coroutine< void >::call_type coro( f6,
            coro::attributes(
                coro::stack_allocator::traits_type::default_size(),
                coro::no_stack_unwind) );
        coro::symmetric_coroutine< void >::call_type coro_e( empty);
        BOOST_CHECK( coro);
        BOOST_CHECK( coro_e);
        term_coro = & coro_e;
        BOOST_CHECK_EQUAL( ( int) 0, value2);
        coro();
        BOOST_CHECK( coro);
        BOOST_CHECK_EQUAL( ( int) 7, value2);
    }
    BOOST_CHECK_EQUAL( ( int) 7, value2);
}

void test_termination()
{
    value2 = 0;

    coro::symmetric_coroutine< int >::call_type coro( f7);
    coro::symmetric_coroutine< void >::call_type coro_e( empty);
    BOOST_CHECK( coro);
    BOOST_CHECK( coro_e);
    term_coro = & coro_e;
    BOOST_CHECK_EQUAL( ( int) 0, value2);
    coro(3);
    BOOST_CHECK( coro);
    BOOST_CHECK_EQUAL( ( int) 3, value2);
    coro(7);
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( ( int) 7, value2);
}

void test_yield_to_void()
{
    value2 = 0;

    coro::symmetric_coroutine< void >::call_type coro_other( f111);
    coro::symmetric_coroutine< void >::call_type coro( boost::bind( f11, _1, boost::ref( coro_other) ) );
    BOOST_CHECK( coro_other);
    BOOST_CHECK( coro);
    BOOST_CHECK_EQUAL( ( int) 0, value2);
    coro();
    BOOST_CHECK( ! coro_other);
    BOOST_CHECK( coro);
    BOOST_CHECK_EQUAL( ( int) 3, value2);
    coro();
    BOOST_CHECK( ! coro_other);
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( ( int) 7, value2);
}

void test_yield_to_int()
{
    value2 = 0;

    coro::symmetric_coroutine< int >::call_type coro_other( f101);
    coro::symmetric_coroutine< int >::call_type coro( boost::bind( f10, _1, boost::ref( coro_other) ) );
    BOOST_CHECK( coro_other);
    BOOST_CHECK( coro);
    BOOST_CHECK_EQUAL( ( int) 0, value2);
    coro(3);
    BOOST_CHECK( ! coro_other);
    BOOST_CHECK( coro);
    BOOST_CHECK_EQUAL( ( int) 3, value2);
    coro(7);
    BOOST_CHECK( ! coro_other);
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( ( int) 7, value2);
}

void test_yield_to_ref()
{
    p = 0;

    coro::symmetric_coroutine< X& >::call_type coro_other( f121);
    coro::symmetric_coroutine< X& >::call_type coro( boost::bind( f12, _1, boost::ref( coro_other) ) );
    BOOST_CHECK( coro_other);
    BOOST_CHECK( coro);
    BOOST_CHECK( 0 == p);
    X x1(3);
    coro( x1);
    BOOST_CHECK( ! coro_other);
    BOOST_CHECK( coro);
    BOOST_CHECK_EQUAL( p->i, x1.i);
    BOOST_CHECK( p == & x1);
    X x2(7);
    coro(x2);
    BOOST_CHECK( ! coro_other);
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( p->i, x2.i);
    BOOST_CHECK( p == & x2);
}

void test_yield_to_different()
{
    value2 = 0;
    value3 = "";

    coro::symmetric_coroutine< std::string >::call_type coro_other( f141);
    coro::symmetric_coroutine< int >::call_type coro( boost::bind( f14, _1, boost::ref( coro_other) ) );
    BOOST_CHECK( coro_other);
    BOOST_CHECK( coro);
    BOOST_CHECK_EQUAL( ( int) 0, value2);
    BOOST_CHECK( value3.empty() );
    coro(3);
    BOOST_CHECK( ! coro_other);
    BOOST_CHECK( coro);
    BOOST_CHECK_EQUAL( "3", value3);
    coro(7);
    BOOST_CHECK( ! coro_other);
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( ( int) 7, value2);
}

void test_move_coro()
{
    value2 = 0;

    coro::symmetric_coroutine< int >::call_type coro1( f16);
    coro::symmetric_coroutine< int >::call_type coro2;
    BOOST_CHECK( coro1);
    BOOST_CHECK( ! coro2);

    coro1( 1);
    BOOST_CHECK_EQUAL( ( int)1, value2);

    coro2 = boost::move( coro1);
    BOOST_CHECK( ! coro1);
    BOOST_CHECK( coro2);

    coro2( 2);
    BOOST_CHECK_EQUAL( ( int)2, value2);

    coro1 = boost::move( coro2);
    BOOST_CHECK( coro1);
    BOOST_CHECK( ! coro2);

    coro1( 3);
    BOOST_CHECK_EQUAL( ( int)3, value2);

    coro2 = boost::move( coro1);
    BOOST_CHECK( ! coro1);
    BOOST_CHECK( coro2);

    coro2( 4);
    BOOST_CHECK_EQUAL( ( int)4, value2);
}

void test_vptr()
{
    D * d = 0;
    T t;
    coro_fn_void fn = trampoline< T, D >;
    coro::symmetric_coroutine< void* >::call_type call( fn);
    call( & t);
    d = t.d;
    BOOST_CHECK( 0 != d);
    d->call = boost::move( call);

    BOOST_CHECK_EQUAL( ( int) 0, d->count);
    d->resume();
    BOOST_CHECK_EQUAL( ( int) 1, d->count);
    d->resume();
    BOOST_CHECK_EQUAL( ( int) 2, d->count);
}

boost::unit_test::test_suite * init_unit_test_suite( int, char* [])
{
    boost::unit_test::test_suite * test =
        BOOST_TEST_SUITE("Boost.coroutine: symmetric coroutine test suite");

    test->add( BOOST_TEST_CASE( & test_move) );
    test->add( BOOST_TEST_CASE( & test_complete) );
    test->add( BOOST_TEST_CASE( & test_yield) );
    test->add( BOOST_TEST_CASE( & test_pass_value) );
    test->add( BOOST_TEST_CASE( & test_pass_reference) );
    test->add( BOOST_TEST_CASE( & test_pass_pointer) );
    test->add( BOOST_TEST_CASE( & test_termination) );
    test->add( BOOST_TEST_CASE( & test_unwind) );
    test->add( BOOST_TEST_CASE( & test_no_unwind) );
    test->add( BOOST_TEST_CASE( & test_yield_to_void) );
    test->add( BOOST_TEST_CASE( & test_yield_to_int) );
    test->add( BOOST_TEST_CASE( & test_yield_to_ref) );
    test->add( BOOST_TEST_CASE( & test_yield_to_different) );
    test->add( BOOST_TEST_CASE( & test_move_coro) );
    test->add( BOOST_TEST_CASE( & test_vptr) );

    return test;
}
