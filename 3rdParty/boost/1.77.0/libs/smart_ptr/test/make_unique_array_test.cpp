/*
Copyright 2014 Glen Joseph Fernandes
(glenjofe@gmail.com)

Distributed under the Boost Software License, Version 1.0.
(http://www.boost.org/LICENSE_1_0.txt)
*/
#include <boost/config.hpp>
#if !defined(BOOST_NO_CXX11_SMART_PTR)
#include <boost/core/lightweight_test.hpp>
#include <boost/smart_ptr/make_unique.hpp>

class type {
public:
    static unsigned instances;

    type() {
        ++instances;
    }

    ~type() {
        --instances;
    }

private:
    type(const type&);
    type& operator=(const type&);
};

unsigned type::instances = 0;

int main()
{
    {
        std::unique_ptr<int[]> result = boost::make_unique<int[]>(3);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result[0] == 0);
        BOOST_TEST(result[1] == 0);
        BOOST_TEST(result[2] == 0);
    }
    {
        std::unique_ptr<int[][2]> result =
            boost::make_unique<int[][2]>(2);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result[0][0] == 0);
        BOOST_TEST(result[0][1] == 0);
        BOOST_TEST(result[1][0] == 0);
        BOOST_TEST(result[1][1] == 0);
    }
    {
        std::unique_ptr<const int[]> result =
            boost::make_unique<const int[]>(3);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result[0] == 0);
        BOOST_TEST(result[1] == 0);
        BOOST_TEST(result[2] == 0);
    }
    {
        std::unique_ptr<const int[][2]> result =
            boost::make_unique<const int[][2]>(2);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(result[0][0] == 0);
        BOOST_TEST(result[0][1] == 0);
        BOOST_TEST(result[1][0] == 0);
        BOOST_TEST(result[1][1] == 0);
    }
    BOOST_TEST(type::instances == 0);
    {
        std::unique_ptr<type[]> result =
            boost::make_unique<type[]>(3);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(type::instances == 3);
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    BOOST_TEST(type::instances == 0);
    {
        std::unique_ptr<type[][2]> result =
            boost::make_unique<type[][2]>(2);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(type::instances == 4);
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    BOOST_TEST(type::instances == 0);
    {
        std::unique_ptr<const type[]> result =
            boost::make_unique<const type[]>(3);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(type::instances == 3);
        result.reset();
        BOOST_TEST(type::instances == 0);
    }
    BOOST_TEST(type::instances == 0);
    {
        std::unique_ptr<const type[][2]> result =
            boost::make_unique<const type[][2]>(2);
        BOOST_TEST(result.get() != 0);
        BOOST_TEST(type::instances == 4);
        result.reset();
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
