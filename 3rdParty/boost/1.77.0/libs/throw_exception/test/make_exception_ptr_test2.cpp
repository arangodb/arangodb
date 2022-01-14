// Copyright 2020 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt

#if defined(_MSC_VER)
# pragma warning(disable: 4702) // unreachable code
#endif

#define BOOST_EXCEPTION_DISABLE

#include <boost/exception_ptr.hpp>
#include <boost/core/lightweight_test.hpp>

class my_exception: public std::exception {};

int main()
{
    BOOST_TEST_THROWS( boost::rethrow_exception( boost::make_exception_ptr( my_exception() ) ), my_exception );
    return boost::report_errors();
}
