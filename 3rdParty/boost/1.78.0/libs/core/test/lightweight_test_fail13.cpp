/*
Copyright 2020 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/lightweight_test.hpp>

void f()
{
    throw 5;
}

int main()
{
    BOOST_TEST_NO_THROW(f());
    return boost::report_errors();
}
