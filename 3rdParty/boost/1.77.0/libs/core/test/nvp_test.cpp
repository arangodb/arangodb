/*
Copyright 2019 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/core/nvp.hpp>
#include <boost/core/lightweight_test.hpp>

void test()
{
    const char* n = "name";
    int v = 1;
    boost::nvp<int> p(n, v);
    BOOST_TEST_EQ(p.name(), n);
    BOOST_TEST_EQ(p.value(), 1);
    BOOST_TEST_EQ(&p.value(), &v);
}

void test_factory()
{
    const char* n = "name";
    int v = 1;
    boost::nvp<int> p = boost::make_nvp(n, v);
    BOOST_TEST_EQ(p.name(), n);
    BOOST_TEST_EQ(p.value(), 1);
    BOOST_TEST_EQ(&p.value(), &v);
}

void test_macro()
{
    int v = 1;
    boost::nvp<int> p = BOOST_NVP(v);
    BOOST_TEST_CSTR_EQ(p.name(), "v");
    BOOST_TEST_EQ(p.value(), 1);
    BOOST_TEST_EQ(&p.value(), &v);
}

int main()
{
    test();
    test_factory();
    test_macro();
    return boost::report_errors();
}
