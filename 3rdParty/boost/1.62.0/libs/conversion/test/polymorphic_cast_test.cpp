//
// Test boost::polymorphic_cast, boost::polymorphic_downcast and 
// boost::polymorphic_pointer_cast, boost::polymorphic_pointer_downcast
//
// Copyright 1999 Beman Dawes
// Copyright 1999 Dave Abrahams
// Copyright 2014 Peter Dimov
// Copyright 2014 Boris Rasin, Antony Polukhin
//
// Distributed under the Boost Software License, Version 1.0.
//
// See accompanying file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt
//

#define BOOST_ENABLE_ASSERT_HANDLER
#include <boost/polymorphic_cast.hpp>
#include <boost/polymorphic_pointer_cast.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <boost/smart_ptr/intrusive_ref_counter.hpp>
#include <boost/core/lightweight_test.hpp>
#include <string>
#include <memory>

static bool expect_assertion = false;
static int assertion_failed_count = 0;

// BOOST_ASSERT custom handler
void boost::assertion_failed( char const * expr, char const * function, char const * file, long line )
{
    if( expect_assertion )
    {
        ++assertion_failed_count;
    }
    else
    {
        BOOST_ERROR( "unexpected assertion" );

        BOOST_LIGHTWEIGHT_TEST_OSTREAM
          << file << "(" << line << "): assertion '" << expr << "' failed in function '"
          << function << "'" << std::endl;
    }
}

//

struct Base : boost::intrusive_ref_counter<Base>
{
    virtual ~Base() {}
    virtual std::string kind() { return "Base"; }
};

struct Base2
{
    virtual ~Base2() {}
    virtual std::string kind2() { return "Base2"; }
};

struct Derived : public Base, Base2
{
    virtual std::string kind() { return "Derived"; }
};

static void test_polymorphic_cast()
{
    Base * base = new Derived;

    Derived * derived;
    
    try
    {
        derived = boost::polymorphic_cast<Derived*>( base );

        BOOST_TEST( derived != 0 );

        if( derived != 0 )
        {
            BOOST_TEST_EQ( derived->kind(), "Derived" );
        }
    }
    catch( std::bad_cast const& )
    {
        BOOST_ERROR( "boost::polymorphic_cast<Derived*>( base ) threw std::bad_cast" );
    }

    Base2 * base2;

    try
    {
        base2 = boost::polymorphic_cast<Base2*>( base ); // crosscast

        BOOST_TEST( base2 != 0 );

        if( base2 != 0 )
        {
            BOOST_TEST_EQ( base2->kind2(), "Base2" );
        }
    }
    catch( std::bad_cast const& )
    {
        BOOST_ERROR( "boost::polymorphic_cast<Base2*>( base ) threw std::bad_cast" );
    }

    delete base;
}

static void test_polymorphic_pointer_cast()
{
    Base * base = new Derived;

    Derived * derived;

    try
    {
        derived = boost::polymorphic_pointer_cast<Derived>( base );

        BOOST_TEST( derived != 0 );

        if( derived != 0 )
        {
            BOOST_TEST_EQ( derived->kind(), "Derived" );
        }
    }
    catch( std::bad_cast const& )
    {
        BOOST_ERROR( "boost::polymorphic_pointer_cast<Derived>( base ) threw std::bad_cast" );
    }

    Base2 * base2;

    try
    {
        base2 = boost::polymorphic_pointer_cast<Base2>( base ); // crosscast

        BOOST_TEST( base2 != 0 );

        if( base2 != 0 )
        {
            BOOST_TEST_EQ( base2->kind2(), "Base2" );
        }
    }
    catch( std::bad_cast const& )
    {
        BOOST_ERROR( "boost::polymorphic_pointer_cast<Base2>( base ) threw std::bad_cast" );
    }

    boost::shared_ptr<Base> sp_base( base );
    boost::shared_ptr<Base2> sp_base2;
    try
    {
        sp_base2 = boost::polymorphic_pointer_cast<Base2>( sp_base ); // crosscast

        BOOST_TEST( sp_base2 != 0 );

        if( sp_base2 != 0 )
        {
            BOOST_TEST_EQ( sp_base2->kind2(), "Base2" );
        }
    }
    catch( std::bad_cast const& )
    {
        BOOST_ERROR( "boost::polymorphic_pointer_cast<Base2>( sp_base ) threw std::bad_cast" );
    }

    // we do not `delete base;` because sahred_ptr is holding base
}

static void test_polymorphic_downcast()
{
    Base * base = new Derived;

    Derived * derived = boost::polymorphic_downcast<Derived*>( base );

    BOOST_TEST( derived != 0 );

    if( derived != 0 )
    {
        BOOST_TEST_EQ( derived->kind(), "Derived" );
    }

    // polymorphic_downcast can't do crosscasts

    delete base;
}

static void test_polymorphic_pointer_downcast_builtin()
{
    Base * base = new Derived;

    Derived * derived = boost::polymorphic_pointer_downcast<Derived>( base );

    BOOST_TEST( derived != 0 );

    if( derived != 0 )
    {
        BOOST_TEST_EQ( derived->kind(), "Derived" );
    }

    // polymorphic_pointer_downcast can't do crosscasts

    delete base;
}

static void test_polymorphic_pointer_downcast_boost_shared()
{
    boost::shared_ptr<Base> base (new Derived);

    boost::shared_ptr<Derived> derived = boost::polymorphic_pointer_downcast<Derived>( base );

    BOOST_TEST( derived != 0 );

    if( derived != 0 )
    {
        BOOST_TEST_EQ( derived->kind(), "Derived" );
    }
}

static void test_polymorphic_pointer_downcast_intrusive()
{
    boost::intrusive_ptr<Base> base (new Derived);

    boost::intrusive_ptr<Derived> derived = boost::polymorphic_pointer_downcast<Derived>( base );

    BOOST_TEST( derived != 0 );

    if( derived != 0 )
    {
        BOOST_TEST_EQ( derived->kind(), "Derived" );
    }
}

#ifndef BOOST_NO_CXX11_SMART_PTR

static void test_polymorphic_pointer_downcast_std_shared()
{
    std::shared_ptr<Base> base (new Derived);

    std::shared_ptr<Derived> derived = boost::polymorphic_pointer_downcast<Derived>( base );

    BOOST_TEST( derived != 0 );

    if( derived != 0 )
    {
        BOOST_TEST_EQ( derived->kind(), "Derived" );
    }
}

#endif

static void test_polymorphic_cast_fail()
{
    Base * base = new Base;

    BOOST_TEST_THROWS( boost::polymorphic_cast<Derived*>( base ), std::bad_cast );

    delete base;
}

static void test_polymorphic_pointer_cast_fail()
{
    Base * base = new Base;
    BOOST_TEST_THROWS( boost::polymorphic_pointer_cast<Derived>( base ), std::bad_cast );
    delete base;

    BOOST_TEST_THROWS( boost::polymorphic_pointer_cast<Derived>( boost::shared_ptr<Base>(new Base) ), std::bad_cast );

#ifndef BOOST_NO_CXX11_SMART_PTR
    BOOST_TEST_THROWS( boost::polymorphic_pointer_cast<Derived>( std::shared_ptr<Base>(new Base) ), std::bad_cast );
#endif

    BOOST_TEST_THROWS( boost::polymorphic_pointer_cast<Derived>( boost::intrusive_ptr<Base>(new Base) ), std::bad_cast );
}

static void test_polymorphic_downcast_fail()
{
    Base * base = new Base;

    int old_count = assertion_failed_count;
    expect_assertion = true;

    boost::polymorphic_downcast<Derived*>( base ); // should assert

    BOOST_TEST_EQ( assertion_failed_count, old_count + 1 );
    expect_assertion = false;

    delete base;
}

static void test_polymorphic_pointer_downcast_builtin_fail()
{
    Base * base = new Base;

    int old_count = assertion_failed_count;
    expect_assertion = true;

    boost::polymorphic_pointer_downcast<Derived>( base ); // should assert

    BOOST_TEST_EQ( assertion_failed_count, old_count + 1 );
    expect_assertion = false;

    delete base;
}

static void test_polymorphic_pointer_downcast_boost_shared_fail()
{
    boost::shared_ptr<Base> base (new Base);

    int old_count = assertion_failed_count;
    expect_assertion = true;

    boost::polymorphic_pointer_downcast<Derived>( base ); // should assert

    BOOST_TEST_EQ( assertion_failed_count, old_count + 1 );
    expect_assertion = false;
}

#ifndef BOOST_NO_CXX11_SMART_PTR

static void test_polymorphic_pointer_downcast_std_shared_fail()
{
    std::shared_ptr<Base> base (new Base);

    int old_count = assertion_failed_count;
    expect_assertion = true;

    boost::polymorphic_pointer_downcast<Derived>( base ); // should assert

    BOOST_TEST_EQ( assertion_failed_count, old_count + 1 );
    expect_assertion = false;
}

#endif

static void test_polymorphic_pointer_downcast_intrusive_fail()
{
    boost::intrusive_ptr<Base> base (new Base);

    int old_count = assertion_failed_count;
    expect_assertion = true;

    boost::polymorphic_pointer_downcast<Derived>( base ); // should assert

    BOOST_TEST_EQ( assertion_failed_count, old_count + 1 );
    expect_assertion = false;
}

int main()
{
    test_polymorphic_cast();
    test_polymorphic_pointer_cast();
    test_polymorphic_downcast();
    test_polymorphic_pointer_downcast_builtin();
    test_polymorphic_pointer_downcast_boost_shared();
    test_polymorphic_pointer_downcast_intrusive();
    test_polymorphic_cast_fail();
    test_polymorphic_pointer_cast_fail();
    test_polymorphic_downcast_fail();
    test_polymorphic_pointer_downcast_builtin_fail();
    test_polymorphic_pointer_downcast_boost_shared_fail();
    test_polymorphic_pointer_downcast_intrusive_fail();

#ifndef BOOST_NO_CXX11_SMART_PTR
    test_polymorphic_pointer_downcast_std_shared();
    test_polymorphic_pointer_downcast_std_shared_fail();
#endif

    return boost::report_errors();
}
