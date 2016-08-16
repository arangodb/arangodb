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

    explicit type() {
        instances++;
    }

    ~type() {
        instances--;
    }

private:
    type(const type&);
    type& operator=(const type&);
};

unsigned int type::instances = 0;

int main()
{
    {
        std::unique_ptr<int[]> a1 = boost::make_unique<int[]>(3);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1[0] == 0);
        BOOST_TEST(a1[1] == 0);
        BOOST_TEST(a1[2] == 0);
    }

    {
        std::unique_ptr<int[][2]> a1 = boost::make_unique<int[][2]>(2);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1[0][0] == 0);
        BOOST_TEST(a1[0][1] == 0);
        BOOST_TEST(a1[1][0] == 0);
        BOOST_TEST(a1[1][1] == 0);
    }

    {
        std::unique_ptr<const int[]> a1 = boost::make_unique<const int[]>(3);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1[0] == 0);
        BOOST_TEST(a1[1] == 0);
        BOOST_TEST(a1[2] == 0);
    }

    {
        std::unique_ptr<const int[][2]> a1 = boost::make_unique<const int[][2]>(2);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(a1[0][0] == 0);
        BOOST_TEST(a1[0][1] == 0);
        BOOST_TEST(a1[1][0] == 0);
        BOOST_TEST(a1[1][1] == 0);
    }

    BOOST_TEST(type::instances == 0);
    {
        std::unique_ptr<type[]> a1 = boost::make_unique<type[]>(3);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 3);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }

    BOOST_TEST(type::instances == 0);
    {
        std::unique_ptr<type[][2]> a1 = boost::make_unique<type[][2]>(2);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 4);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }

    BOOST_TEST(type::instances == 0);
    {
        std::unique_ptr<const type[]> a1 = boost::make_unique<const type[]>(3);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 3);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }

    BOOST_TEST(type::instances == 0);
    {
        std::unique_ptr<const type[][2]> a1 = boost::make_unique<const type[][2]>(2);
        BOOST_TEST(a1.get() != 0);
        BOOST_TEST(type::instances == 4);
        a1.reset();
        BOOST_TEST(type::instances == 0);
    }

    return boost::report_errors();
}
#else

int main()
{
    return 0;
}

#endif
