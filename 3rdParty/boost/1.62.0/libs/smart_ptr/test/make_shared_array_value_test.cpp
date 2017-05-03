/*
(c) 2012-2015 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#include <boost/detail/lightweight_test.hpp>
#include <boost/smart_ptr/make_shared.hpp>

int main()
{
    {
        boost::shared_ptr<int[]> a1 = boost::make_shared<int[]>(4, 1);
        BOOST_TEST(a1[0] == 1);
        BOOST_TEST(a1[1] == 1);
        BOOST_TEST(a1[2] == 1);
        BOOST_TEST(a1[3] == 1);
    }
    {
        boost::shared_ptr<int[4]> a1 = boost::make_shared<int[4]>(1);
        BOOST_TEST(a1[0] == 1);
        BOOST_TEST(a1[1] == 1);
        BOOST_TEST(a1[2] == 1);
        BOOST_TEST(a1[3] == 1);
    }
    {
        boost::shared_ptr<const int[]> a1 = boost::make_shared<const int[]>(4, 1);
        BOOST_TEST(a1[0] == 1);
        BOOST_TEST(a1[1] == 1);
        BOOST_TEST(a1[2] == 1);
        BOOST_TEST(a1[3] == 1);
    }
    {
        boost::shared_ptr<const int[4]> a1 = boost::make_shared<const int[4]>(1);
        BOOST_TEST(a1[0] == 1);
        BOOST_TEST(a1[1] == 1);
        BOOST_TEST(a1[2] == 1);
        BOOST_TEST(a1[3] == 1);
    }
    return boost::report_errors();
}
