//Copyright (c) 2006-2009 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#define BOOST_NORETURN
#include <boost/config.hpp>

#if !defined( BOOST_NO_EXCEPTIONS )
#   error This program requires exception handling disabled.
#endif

#include <boost/exception_ptr.hpp>
#include <boost/exception/get_error_info.hpp>
#include <boost/detail/lightweight_test.hpp>

typedef boost::error_info<struct tag_answer,int> answer;

int exc_count;

struct
err:
    virtual boost::exception,
    virtual std::exception
    {
    err()
        {
        ++exc_count;
        }

    err( err const & )
        {
        ++exc_count;
        }

    virtual
    ~err() BOOST_NOEXCEPT_OR_NOTHROW
        {
        --exc_count;
        }

    private:

    err & operator=( err const & );
    };


namespace
    {
    bool throw_exception_called;
    }

// It is not valid to return to the caller but we do for testing purposes.
namespace
boost
    {
    void
    throw_exception( std::exception const & e )
        {
        throw_exception_called = true;
        BOOST_TEST(dynamic_cast<err const *>(&e)!=0);
        int const * ans=boost::get_error_info<answer>(e);
        BOOST_TEST(ans && *ans==42);
        }

    struct source_location;
    void
    throw_exception( std::exception const & e, boost::source_location const & )
        {
        throw_exception_called = true;
        BOOST_TEST(dynamic_cast<err const *>(&e)!=0);
        int const * ans=boost::get_error_info<answer>(e);
        BOOST_TEST(ans && *ans==42);
        }
    }

void
simple_test()
    {
    boost::exception_ptr p = boost::copy_exception(err() << answer(42));
    throw_exception_called = false;
    boost::exception_detail::rethrow_exception_(p);
    BOOST_TEST(throw_exception_called);
    }

int
main()
    {
    BOOST_TEST(++exc_count==1);
    simple_test();
    BOOST_TEST(!--exc_count);
    return boost::report_errors();
    }
