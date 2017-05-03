//Copyright (c) 2006-2013 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

//This file tests the N3757 syntax for adding error info to std::exception, which is
//different from the syntax used by Boost Exception.

#include <boost/exception/N3757.hpp>
#include <boost/exception/diagnostic_information.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <ostream>

struct tag1 { typedef int type; };
struct tag2 { typedef std::string type; };
struct tag3 { typedef int type; };

struct my_error_code { int ec; my_error_code(int ec):ec(ec){ } };
struct tag_error_code { typedef my_error_code type; };

bool my_error_code_to_string_called=false;

std::ostream &
operator<<( std::ostream & s, my_error_code const & x )
{
    my_error_code_to_string_called=true;
    return s << "my_error_code(" << x.ec << ')';
}

struct my_exception: virtual std::exception, virtual boost::exception { };

int
main()
    {
    try
        {
        throw my_exception();
        }
    catch(
	boost::exception & e )
        {
        e.set<tag1>(42);
        e.set<tag2>("42");
        e.set<tag_error_code>(42); //Implicit conversion
        try
            {
            throw;
            }
        catch(
        my_exception & e )
            {
            BOOST_TEST(e.get<tag1>() && *e.get<tag1>()==42);
            BOOST_TEST(e.get<tag2>() && *e.get<tag2>()=="42");
            BOOST_TEST(!e.get<tag3>());
            BOOST_TEST(e.get<tag_error_code>() && e.get<tag_error_code>()->ec==42);

            //Below we're verifying that an error code wrapped in a user-defined type
            //invokes the correct op<< to convert the error code to string.
            //Note that N3757 diagnostic_information uses different syntax, it is a
            //member of of std::exception.
            std::string di=boost::diagnostic_information(e);
            BOOST_TEST(!di.empty());
            BOOST_TEST(my_error_code_to_string_called);
            }
        }
    return boost::report_errors();
    }
