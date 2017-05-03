/*
(c) 2014 Glen Joseph Fernandes
<glenjofe -at- gmail.com>

Distributed under the Boost Software
License, Version 1.0.
http://boost.org/LICENSE_1_0.txt
*/
#include <boost/config.hpp>
#if !defined(BOOST_NO_CXX11_SMART_PTR)
#include <boost/detail/lightweight_test.hpp>
#include <boost/smart_ptr/make_unique.hpp>

struct type {
    int x;
    int y;
};

int main()
{
    {
        std::unique_ptr<type> a1 = boost::make_unique<type>();
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1->x == 0);
        BOOST_TEST(a1->y == 0);
    }

    {
        std::unique_ptr<const type> a1 = boost::make_unique<const type>();
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1->x == 0);
        BOOST_TEST(a1->y == 0);
    }

#if !defined(BOOST_NO_CXX11_UNIFIED_INITIALIZATION_SYNTAX)
    {
        std::unique_ptr<type> a1 = boost::make_unique<type>({ 1, 2 });
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1->x == 1);
        BOOST_TEST(a1->y == 2);
    }

    {
        std::unique_ptr<const type> a1 = boost::make_unique<const type>({ 1, 2 });
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1->x == 1);
        BOOST_TEST(a1->y == 2);
    }
#endif

    return boost::report_errors();
}
#else

int main()
{
    return 0;
}

#endif
