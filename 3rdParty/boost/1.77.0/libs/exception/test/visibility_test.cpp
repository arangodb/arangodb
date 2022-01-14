//Copyright (c) 2006-2009 Emil Dotchevski and Reverge Studios, Inc.

//Distributed under the Boost Software License, Version 1.0. (See accompanying
//file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#include <boost/config.hpp>

#if defined( BOOST_NO_EXCEPTIONS )
#   error This program requires exception handling.
#endif

#include "visibility_test_lib.hpp"
#include <boost/exception/get_error_info.hpp>
#include <boost/detail/lightweight_test.hpp>

void BOOST_SYMBOL_IMPORT hidden_throw();

int
main()
    {
    try
        {
        hidden_throw();
        BOOST_TEST(false);
        }
    catch(
    my_exception & e )
        {
        BOOST_TEST(boost::get_error_info<my_info>(e) && *boost::get_error_info<my_info>(e)==42);
        }
    return boost::report_errors();
    }
