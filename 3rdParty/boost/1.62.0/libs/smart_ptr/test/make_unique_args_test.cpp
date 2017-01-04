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

class type {
public:
    static unsigned int instances;

    explicit type(int v1 = 0, 
                  int v2 = 0, 
                  int v3 = 0, 
                  int v4 = 0, 
                  int v5 = 0, 
                  int v6 = 0, 
                  int v7 = 0, 
                  int v8 = 0, 
                  int v9 = 0) 
        : sum(v1 + v2 + v3 + v4 + v5 + v6 + v7 + v8 + v9) {
        instances++;
    }

    ~type() {
        instances--;
    }

    const int sum;

private:
    type(const type&);
    type& operator=(const type&);
};

unsigned int type::instances = 0;

int main()
{
    BOOST_TEST(type::instances == 0);
    {
        std::unique_ptr<type> a1 = boost::make_unique<type>();
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 1);
        BOOST_TEST(a1->sum == 0);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }

#if !defined( BOOST_NO_CXX11_VARIADIC_TEMPLATES )
    {
        std::unique_ptr<type> a1 = boost::make_unique<type>(1);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 1);
        BOOST_TEST(a1->sum == 1);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }

    {
        std::unique_ptr<type> a1 = boost::make_unique<type>(1, 2);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 1);
        BOOST_TEST(a1->sum == 1 + 2);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }

    {
        std::unique_ptr<type> a1 = boost::make_unique<type>(1, 2, 3);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 1);
        BOOST_TEST(a1->sum == 1 + 2 + 3);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }

    {
        std::unique_ptr<type> a1 = boost::make_unique<type>(1, 2, 3, 4);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 1);
        BOOST_TEST(a1->sum == 1 + 2 + 3 + 4);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }

    {
        std::unique_ptr<type> a1 = boost::make_unique<type>(1, 2, 3, 4, 5);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 1);
        BOOST_TEST(a1->sum == 1 + 2 + 3 + 4 + 5);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }

    {
        std::unique_ptr<type> a1 = boost::make_unique<type>(1, 2, 3, 4, 5, 6);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 1);
        BOOST_TEST(a1->sum == 1 + 2 + 3 + 4 + 5 + 6);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }

    {
        std::unique_ptr<type> a1 = boost::make_unique<type>(1, 2, 3, 4, 5, 6, 7);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 1);
        BOOST_TEST(a1->sum == 1 + 2 + 3 + 4 + 5 + 6 + 7);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }

    {
        std::unique_ptr<type> a1 = boost::make_unique<type>(1, 2, 3, 4, 5, 6, 7, 8);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 1);
        BOOST_TEST(a1->sum == 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }

    {
        std::unique_ptr<type> a1 = boost::make_unique<type>(1, 2, 3, 4, 5, 6, 7, 8, 9);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 1);
        BOOST_TEST(a1->sum == 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9);
        a1.reset();
        BOOST_TEST(type::instances == 0);
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
