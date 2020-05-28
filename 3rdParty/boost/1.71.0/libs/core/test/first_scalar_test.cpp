/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/first_scalar.hpp>
#include <boost/core/lightweight_test.hpp>

int main()
{
    int i1 = 0;
    BOOST_TEST_EQ(boost::first_scalar(&i1), &i1);
    int i2[4] = { };
    BOOST_TEST_EQ(boost::first_scalar(i2), &i2[0]);
    int i3[2][4][6] = { };
    BOOST_TEST_EQ(boost::first_scalar(i3), &i3[0][0][0]);
    return boost::report_errors();
}
