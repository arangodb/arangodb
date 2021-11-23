
// Copyright 2018 Peter Dimov.
// Distributed under the Boost Software License, Version 1.0.

#include <boost/function.hpp>
#include <boost/core/lightweight_test.hpp>

void throw_bad_function_call();

int main()
{
    BOOST_TEST_THROWS( throw_bad_function_call(), boost::bad_function_call );
    return boost::report_errors();
}
