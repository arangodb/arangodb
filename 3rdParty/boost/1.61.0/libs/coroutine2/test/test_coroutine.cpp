
//          Copyright Oliver Kowalke 2014.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

#include <boost/assert.hpp>
#include <boost/test/unit_test.hpp>

#include <boost/coroutine2/coroutine.hpp>

namespace coro = boost::coroutines2;

int value1 = 0;
std::string value2 = "";
bool value3 = false;
double value4 = .0;
int * value5 = 0;
int& value6 = value1;
int& value7 = value1;
int value8 = 0;
int value9 = 0;

struct X
{
    X() { value1 = 7; }
    ~X() { value1 = 0; }

    X( X const&) = delete;
    X & operator=( X const&) = delete;
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

    void operator()( coro::coroutine< int >::push_type &)
    { value3 = state; }
};

class moveable
{
public:
    bool    state;

    moveable() :
        state( false)
    {}

    moveable( int) :
        state( true)
    {}

    moveable( moveable const&) = delete;
    moveable & operator=( moveable const&) = delete;

    moveable( moveable && other) :
        state( false)
    { std::swap( state, other.state); }

    moveable & operator=( moveable && other)
    {
        if ( this != & other) {
            state = other.state;
            other.state = false;
        }
        return * this;
    }

    void operator()( coro::coroutine< int >::push_type &)
    { value3 = state; }
};

class movedata
{
public:
    int     i;

    movedata( int i_) :
        i( i_)
    {}

    movedata( movedata const&) = delete;
    movedata & operator=( movedata const&) = delete;

    movedata( movedata && other) :
        i( 0)
    { std::swap( i, other.i); }

    movedata & operator=( movedata && other)
    {
        if ( this != & other) {
            i = other.i;
            other.i = 0;
        }
        return * this;
    }
};

struct my_exception {};

void f1( coro::coroutine< void >::push_type & c)
{
    while ( c)
        c();
}

void f2( coro::coroutine< void >::push_type &)
{ ++value1; }

void f3( coro::coroutine< void >::push_type & c)
{
    ++value1;
    c();
    ++value1;
}

void f4( coro::coroutine< int >::push_type & c)
{
    c( 3);
    c( 7);
}

void f5( coro::coroutine< std::string >::push_type & c)
{
    std::string res("abc");
    c( res);
    res = "xyz";
    c( res);
}

void f6( coro::coroutine< int >::pull_type & c)
{ value1 = c.get(); }

void f7( coro::coroutine< std::string >::pull_type & c)
{ value2 = c.get(); }

void f8( coro::coroutine< std::tuple< double, double > >::pull_type & c)
{
    double x = 0, y = 0;
    std::tie( x, y) = c.get();
    value4 = x + y;
    c();
    std::tie( x, y) = c.get();
    value4 = x + y;
}

void f9( coro::coroutine< int * >::pull_type & c)
{ value5 = c.get(); }

void f91( coro::coroutine< int const* >::pull_type & c)
{ value5 = const_cast< int * >( c.get() ); }

void f10( coro::coroutine< int & >::pull_type & c)
{
    int const& i = c.get();
    value5 = const_cast< int * >( & i);
}

void f101( coro::coroutine< int const& >::pull_type & c)
{
    int const& i = c.get();
    value5 = const_cast< int * >( & i);
}

void f11( coro::coroutine< std::tuple< int, int > >::pull_type & c)
{
    std::tie( value8, value9) = c.get();
}

void f12( coro::coroutine< void >::pull_type & c)
{
    X x_;
    c();
    c();
}

void f16( coro::coroutine< int >::push_type & c)
{
    c( 1);
    c( 2);
    c( 3);
    c( 4);
    c( 5);
}

void f17( coro::coroutine< int >::pull_type & c, std::vector< int > & vec)
{
    int x = c.get();
    while ( 5 > x)
    {
        vec.push_back( x);
        x = c().get();
    }
}

void f20( coro::coroutine< int >::push_type &)
{}

void f21( coro::coroutine< int >::pull_type & c)
{
    while ( c)
    {
        value1 = c.get();
        c();
    }
}

void f22( coro::coroutine< movedata >::pull_type & c)
{
    movedata mv( c.get() );
    value1 = mv.i;
}

void test_move()
{
    {
        coro::coroutine< int >::pull_type coro1( f20);
        coro::coroutine< int >::pull_type coro2( f16);
        BOOST_CHECK( ! coro1);
        BOOST_CHECK( coro2);
        BOOST_CHECK_EQUAL( 1, coro2.get() );
        coro2();
        BOOST_CHECK_EQUAL( 2, coro2.get() );
        coro1 = std::move( coro2);
        BOOST_CHECK( coro1);
        BOOST_CHECK( ! coro2);
        coro1();
        BOOST_CHECK_EQUAL( 3, coro1.get() );
        BOOST_CHECK( ! coro2);
    }

    {
        value3 = false;
        copyable cp( 3);
        BOOST_CHECK( cp.state);
        BOOST_CHECK( ! value3);
        coro::coroutine< int >::pull_type coro( cp);
        BOOST_CHECK( cp.state);
        BOOST_CHECK( value3);
    }

    {
        value3 = false;
        moveable mv( 7);
        BOOST_CHECK( mv.state);
        BOOST_CHECK( ! value3);
        coro::coroutine< int >::pull_type coro( std::move( mv) );
        BOOST_CHECK( ! mv.state);
        BOOST_CHECK( value3);
    }

    {
        value1 = 0;
        movedata mv( 7);
        BOOST_CHECK_EQUAL( 0, value1);
        BOOST_CHECK_EQUAL( 7, mv.i);
        coro::coroutine< movedata >::push_type coro( f22);
        coro( std::move( mv) );
        BOOST_CHECK_EQUAL( 7, value1);
        BOOST_CHECK_EQUAL( 0, mv.i);
    }
}

void test_complete()
{
    value1 = 0;

    coro::coroutine< void >::pull_type coro( f2);
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( ( int)1, value1);
}

void test_jump()
{
    value1 = 0;

    coro::coroutine< void >::pull_type coro( f3);
    BOOST_CHECK( coro);
    BOOST_CHECK_EQUAL( ( int)1, value1);
    coro();
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( ( int)2, value1);
}

void test_result_int()
{
    coro::coroutine< int >::pull_type coro( f4);
    BOOST_CHECK( coro);
    int result = coro.get();
    BOOST_CHECK( coro);
    BOOST_CHECK_EQUAL( 3, result);
    result = coro().get();
    BOOST_CHECK( coro);
    BOOST_CHECK_EQUAL( 7, result);
    coro();
    BOOST_CHECK( ! coro);
}

void test_result_string()
{
    coro::coroutine< std::string >::pull_type coro( f5);
    BOOST_CHECK( coro);
    std::string result = coro.get();
    BOOST_CHECK( coro);
    BOOST_CHECK_EQUAL( std::string("abc"), result);
    result = coro().get();
    BOOST_CHECK( coro);
    BOOST_CHECK_EQUAL( std::string("xyz"), result);
    coro();
    BOOST_CHECK( ! coro);
}

void test_arg_int()
{
    value1 = 0;

    coro::coroutine< int >::push_type coro( f6);
    BOOST_CHECK( coro);
    coro( 3);
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( 3, value1);
}

void test_arg_string()
{
    value2 = "";

    coro::coroutine< std::string >::push_type coro( f7);
    BOOST_CHECK( coro);
    coro( std::string("abc") );
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( std::string("abc"), value2);
}

void test_fp()
{
    value4 = 0;

    coro::coroutine< std::tuple< double, double > >::push_type coro( f8);
    BOOST_CHECK( coro);
    coro( std::make_tuple( 7.35, 3.14) );
    BOOST_CHECK( coro);
    BOOST_CHECK_EQUAL( ( double) 10.49, value4);

    value4 = 0;
    coro( std::make_tuple( 1.15, 3.14) );
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( ( double) 4.29, value4);
}

void test_ptr()
{
    value5 = 0;

    int a = 3;
    coro::coroutine< int * >::push_type coro( f9);
    BOOST_CHECK( coro);
    coro( & a);
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( & a, value5);
}

void test_const_ptr()
{
    value5 = 0;

    int a = 3;
    coro::coroutine< int const* >::push_type coro( f91);
    BOOST_CHECK( coro);
    coro( & a);
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( & a, value5);
}

void test_ref()
{
    value5 = 0;

    int a = 3;
    coro::coroutine< int & >::push_type coro( f10);
    BOOST_CHECK( coro);
    coro( a);
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( & a, value5);
}

void test_const_ref()
{
    value5 = 0;

    int a = 3;
    coro::coroutine< int const& >::push_type coro( f101);
    BOOST_CHECK( coro);
    coro( a);
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( & a, value5);
}

void test_no_result()
{
    coro::coroutine< int >::pull_type coro( f20);
    BOOST_CHECK( ! coro);
}

void test_tuple()
{
    value8 = 0;
    value9 = 0;

    int a = 3, b = 7;
    std::tuple< int, int > tpl( a, b);
    BOOST_CHECK_EQUAL( a, std::get< 0 >( tpl) );
    BOOST_CHECK_EQUAL( b, std::get< 1 >( tpl) );
    coro::coroutine< std::tuple< int, int > >::push_type coro( f11);
    BOOST_CHECK( coro);
    coro( tpl);
    BOOST_CHECK( ! coro);
    BOOST_CHECK_EQUAL( a, value8);
    BOOST_CHECK_EQUAL( b, value9);
}

void test_unwind()
{
    value1 = 0;
    {
        coro::coroutine< void >::push_type coro( f12);
        BOOST_CHECK( coro);
        BOOST_CHECK_EQUAL( ( int) 0, value1);
        coro();
        BOOST_CHECK( coro);
        BOOST_CHECK_EQUAL( ( int) 7, value1);
        coro();
        BOOST_CHECK_EQUAL( ( int) 7, value1);
    }
    BOOST_CHECK_EQUAL( ( int) 0, value1);
}

void test_exceptions()
{
    std::string msg("abc"), value;
    std::runtime_error ex( msg);
    try
    {
        coro::coroutine< void >::push_type coro(
                [&msg]( coro::coroutine< void >::pull_type &) {
                    throw std::runtime_error( msg);
                });
        BOOST_CHECK( coro);
        coro();
        BOOST_CHECK( ! coro);
        BOOST_CHECK( false);
    }
    catch ( std::runtime_error const& ex)
    { value = ex.what(); }
    BOOST_CHECK_EQUAL( value, msg);
}

void test_input_iterator()
{
    {
        using std::begin;
        using std::end;

        std::vector< int > vec;
        coro::coroutine< int >::pull_type coro( f16);
        coro::coroutine< int >::pull_type::iterator e = end( coro);
        for (
            coro::coroutine< int >::pull_type::iterator i = begin( coro);
            i != e; ++i)
        { vec.push_back( * i); }
        BOOST_CHECK_EQUAL( ( std::size_t)5, vec.size() );
        BOOST_CHECK_EQUAL( ( int)1, vec[0] );
        BOOST_CHECK_EQUAL( ( int)2, vec[1] );
        BOOST_CHECK_EQUAL( ( int)3, vec[2] );
        BOOST_CHECK_EQUAL( ( int)4, vec[3] );
        BOOST_CHECK_EQUAL( ( int)5, vec[4] );
    }
    {
        std::vector< int > vec;
        coro::coroutine< int >::pull_type coro( f16);
        for ( auto i : coro)
        { vec.push_back( i); }
        BOOST_CHECK_EQUAL( ( std::size_t)5, vec.size() );
        BOOST_CHECK_EQUAL( ( int)1, vec[0] );
        BOOST_CHECK_EQUAL( ( int)2, vec[1] );
        BOOST_CHECK_EQUAL( ( int)3, vec[2] );
        BOOST_CHECK_EQUAL( ( int)4, vec[3] );
        BOOST_CHECK_EQUAL( ( int)5, vec[4] );
    }
    {
        int i1 = 1, i2 = 2, i3 = 3;
        coro::coroutine< int& >::pull_type coro(
            [&i1,&i2,&i3](coro::coroutine< int& >::push_type & c){
                c( i1);
                c( i2);
                c( i3);
            });

        int counter = 1;
        for ( int & i : coro) {
            switch ( counter) {
            case 1:
                BOOST_CHECK_EQUAL( & i1, & i);
                break;
            case 2:
                BOOST_CHECK_EQUAL( & i2, & i);
                break;
            case 3:
                BOOST_CHECK_EQUAL( & i3, & i);
                break;
            default:
                BOOST_ASSERT( false);
            }
            ++counter;
        }
    }
}

void test_output_iterator()
{
    using std::begin;
    using std::end;

    int counter = 0;
    std::vector< int > vec;
    coro::coroutine< int >::push_type coro(
        [&vec]( coro::coroutine< int >::pull_type & c) {
            int x = c.get();
            while ( 5 > x)
            {
                vec.push_back( x);
                x = c().get();
            }
        });
//  coro::coroutine< int >::push_type coro(
//      std::bind( f17, std::placeholders::_1, std::ref( vec) ) );
    coro::coroutine< int >::push_type::iterator e( end( coro) );
    for ( coro::coroutine< int >::push_type::iterator i( begin( coro) );
          i != e; ++i)
    {
        i = ++counter;
    }
    BOOST_CHECK_EQUAL( ( std::size_t)4, vec.size() );
    BOOST_CHECK_EQUAL( ( int)1, vec[0] );
    BOOST_CHECK_EQUAL( ( int)2, vec[1] );
    BOOST_CHECK_EQUAL( ( int)3, vec[2] );
    BOOST_CHECK_EQUAL( ( int)4, vec[3] );
}

std::vector< int > vec;
coro::coroutine< void >::pull_type * child = nullptr;

void start_child_coroutine() {
    child = new coro::coroutine< void >::pull_type([](coro::coroutine< void >::push_type & yield) {
            vec.push_back( 2);
            yield();
            vec.push_back( 2);
            yield();
            vec.push_back( 2);
            yield();
            vec.push_back( 2);
            yield();
            vec.push_back( 2);
            yield();
            vec.push_back( 2);
            });
}

coro::coroutine< void >::pull_type start_parent_coroutine() {
    return coro::coroutine< void >::pull_type([=](coro::coroutine< void >::push_type & yield) {
            vec.push_back( 1);
            start_child_coroutine();
            yield();
            vec.push_back( 1);
            });
}

void test_chaining()
{
    auto parent = start_parent_coroutine();
    while ( * child) {
        ( * child)();
    }
    BOOST_CHECK_EQUAL( 7, vec.size() );
    BOOST_CHECK_EQUAL( 1, vec[0]);
    BOOST_CHECK_EQUAL( 2, vec[1]);
    BOOST_CHECK_EQUAL( 2, vec[2]);
    BOOST_CHECK_EQUAL( 2, vec[3]);
    BOOST_CHECK_EQUAL( 2, vec[4]);
    BOOST_CHECK_EQUAL( 2, vec[5]);
    BOOST_CHECK_EQUAL( 2, vec[6]);
}

boost::unit_test::test_suite * init_unit_test_suite( int, char* [])
{
    boost::unit_test::test_suite * test =
        BOOST_TEST_SUITE("Boost.Coroutine2: coroutine test suite");

    test->add( BOOST_TEST_CASE( & test_move) );
    test->add( BOOST_TEST_CASE( & test_complete) );
    test->add( BOOST_TEST_CASE( & test_jump) );
    test->add( BOOST_TEST_CASE( & test_result_int) );
    test->add( BOOST_TEST_CASE( & test_result_string) );
    test->add( BOOST_TEST_CASE( & test_arg_int) );
    test->add( BOOST_TEST_CASE( & test_arg_string) );
    test->add( BOOST_TEST_CASE( & test_fp) );
    test->add( BOOST_TEST_CASE( & test_ptr) );
    test->add( BOOST_TEST_CASE( & test_const_ptr) );
    test->add( BOOST_TEST_CASE( & test_no_result) );
    test->add( BOOST_TEST_CASE( & test_ref) );
    test->add( BOOST_TEST_CASE( & test_const_ref) );
    test->add( BOOST_TEST_CASE( & test_tuple) );
    test->add( BOOST_TEST_CASE( & test_unwind) );
    test->add( BOOST_TEST_CASE( & test_exceptions) );
    test->add( BOOST_TEST_CASE( & test_input_iterator) );
    test->add( BOOST_TEST_CASE( & test_output_iterator) );
    test->add( BOOST_TEST_CASE( & test_chaining) );

    return test;
}
