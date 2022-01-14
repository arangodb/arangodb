//Copyright (c) 2006-2009 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

#if defined( BOOST_NO_EXCEPTIONS )
#   error This program requires exception handling.
#endif

#include <boost/exception_ptr.hpp>
#include <boost/exception/get_error_info.hpp>
#include <boost/exception/info.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <boost/detail/workaround.hpp>

#if BOOST_WORKAROUND(BOOST_CODEGEARC, BOOST_TESTED_AT(0x610))
struct tag_test {};
#endif

typedef boost::error_info<struct tag_test,int> test;

struct
test_boost_exception:
    boost::exception
    {
    };

void
throw_boost_exception()
    {
    throw test_boost_exception() << test(42);
    }

void
throw_unknown_exception()
    {
    struct
    test_exception:
        std::exception
        {
        };
    throw test_exception();
    }

struct
user_defined_exception
    {
        user_defined_exception(int d):data(d){}
        int data;
    };

void
throw_user_defined_exception()
    {
    throw user_defined_exception(42);
    }

void
throw_builtin_exception()
    {
    throw 42;
    }

int
main()
    {
    try
        {
        throw_boost_exception();
        }
    catch(
    ... )
        {
        boost::exception_ptr ep=boost::current_exception();
        try
            {
            rethrow_exception(ep);
            }
        catch(
        boost::unknown_exception & x )
            {
            if( int const * d=boost::get_error_info<test>(x) )
                BOOST_TEST( 42==*d );
            else
                BOOST_TEST(false);
            }
        catch(
        boost::exception & x )
            {
            //Yay! Non-intrusive cloning supported!
            if( int const * d=boost::get_error_info<test>(x) )
                BOOST_TEST( 42==*d );
            else
                BOOST_TEST(false);
            }
        catch(
        ... )
            {
            BOOST_TEST(false);
            }
        try
            {
            rethrow_exception(ep);
            }
        catch(
        boost::exception & x )
            {
            if( int const * d=boost::get_error_info<test>(x) )
                BOOST_TEST( 42==*d );
            else
                BOOST_TEST(false);
            }
        catch(
        ... )
            {
            BOOST_TEST(false);
            }
        }
    try
        {
        throw_unknown_exception();
        }
    catch(
    ... )
        {
        boost::exception_ptr ep=boost::current_exception();
        try
            {
            rethrow_exception(ep);
            }
        catch(
        boost::unknown_exception & )
            {
            }
        catch(
        std::exception & )
            {
            //Yay! Non-intrusive cloning supported!
            }
        catch(
        ... )
            {
            BOOST_TEST(false);
            }
        try
            {
            rethrow_exception(ep);
            }
        catch(
        boost::exception & )
            {
            }
        catch(
        std::exception & )
            {
            //Yay! Non-intrusive cloning supported!
            }
        catch(
        ... )
            {
            BOOST_TEST(false);
            }
        }
    try
        {
        throw_user_defined_exception();
        }
    catch(
    ... )
        {
        boost::exception_ptr ep=boost::current_exception();
        try
            {
            rethrow_exception(ep);
            }
#ifndef BOOST_NO_CXX11_HDR_EXCEPTION
        catch(
        user_defined_exception & x)
            {
            //Yay! std::current_exception to the rescue!
            BOOST_TEST( 42==x.data );
            }
#else
        catch(
        boost::unknown_exception & )
            {
            //Boo! user defined exception was transported as a boost::unknown_exception
            }
#endif
        catch(
        ... )
            {
            BOOST_TEST(false);
            }
        try
            {
            rethrow_exception(ep);
            }
#ifndef BOOST_NO_CXX11_HDR_EXCEPTION
        catch(
        user_defined_exception & x)
            {
            //Yay! std::current_exception to the rescue!
            BOOST_TEST( 42==x.data );
            }
#else
        catch(
        boost::unknown_exception & )
            {
            //Boo! user defined exception was transported as a boost::unknown_exception
            }
#endif
        catch(
        ... )
            {
            BOOST_TEST(false);
            }
        }
    try
        {
        throw_builtin_exception();
        }
    catch(
    ... )
        {
        boost::exception_ptr ep=boost::current_exception();
        try
            {
            rethrow_exception(ep);
            }
#ifndef BOOST_NO_CXX11_HDR_EXCEPTION
        catch(
        int & x)
            {
            //Yay! std::current_exception to the rescue!
            BOOST_TEST( 42==x );
            }
#else
        catch(
        boost::unknown_exception & )
            {
            //Boo! builtin exception was transported as a boost::unknown_exception
            }
#endif
        catch(
        ... )
            {
            BOOST_TEST(false);
            }
        try
            {
            rethrow_exception(ep);
            }
#ifndef BOOST_NO_CXX11_HDR_EXCEPTION
        catch(
        int & x)
            {
            //Yay! std::current_exception to the rescue!
            BOOST_TEST( 42==x );
            }
#else
        catch(
        boost::unknown_exception & )
            {
            //Boo! builtin exception was transported as a boost::unknown_exception
            }
#endif
        catch(
        ... )
            {
            BOOST_TEST(false);
            }
        }
    return boost::report_errors();
    }
