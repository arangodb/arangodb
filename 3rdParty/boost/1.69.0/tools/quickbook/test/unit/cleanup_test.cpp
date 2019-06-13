
/*=============================================================================
    Copyright (c) 2017 Daniel James

    Use, modification and distribution is subject to the Boost Software
    License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
    http://www.boost.org/LICENSE_1_0.txt)
=============================================================================*/

#include <vector>
#include <boost/core/lightweight_test.hpp>
#include "cleanup.hpp"

struct counted
{
    static int count;
    static std::vector<int> destroyed;
    static void reset()
    {
        count = 0;
        destroyed.clear();
    }

    int value;

    counted(int v) : value(v)
    {
        BOOST_TEST(value != -1);
        ++count;
    }

    counted(counted const& x) : value(x.value)
    {
        BOOST_TEST(value != -1);
        ++count;
    }

    ~counted()
    {
        BOOST_TEST(value != -1);
        destroyed.push_back(value);
        value = -1;
        BOOST_TEST(count > 0);
        --count;
    }
};

int counted::count = 0;
std::vector<int> counted::destroyed;

int main()
{
    counted::reset();
    {
        quickbook::cleanup c;
    }
    BOOST_TEST(counted::count == 0);

    counted::reset();
    {
        quickbook::cleanup c;
        counted& v1 = c.add(new counted(1));
        counted& v2 = c.add(new counted(2));
        BOOST_TEST(v1.value == 1);
        BOOST_TEST(v2.value == 2);
    }
    BOOST_TEST(counted::count == 0);
    BOOST_TEST(counted::destroyed.size() == 2);
    BOOST_TEST(counted::destroyed[0] == 2);
    BOOST_TEST(counted::destroyed[1] == 1);

    counted::reset();
    {
        quickbook::cleanup c;
        int& x = c.add(new int(10));
        BOOST_TEST(x == 10);
    }

    return boost::report_errors();
}
