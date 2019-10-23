/* 
   Copyright (c) T. Zachary Laine 2018.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/
#include <iostream>

#include <boost/algorithm/find_not.hpp>

#define BOOST_TEST_MAIN
#include <boost/test/unit_test.hpp>

#include <vector>
#include <list>


namespace ba = boost::algorithm;

template <typename Container>
struct dist_t
{
    dist_t(Container & cont) : cont_(cont) {}
    template<typename Iter>
    std::ptrdiff_t operator()(Iter it) const
    {
        return std::distance(cont_.begin(), it);
    }

    Container & cont_;
};

BOOST_CXX14_CONSTEXPR bool check_constexpr()
{
    int in_data[] = {2, 2, 3, 4, 5};
    bool res = true;

    const int* from = in_data;
    const int* to = in_data + 5;

    const int* start = ba::find_not(from, to, 1); // stops on first
    res = (res && start == from);

    start = ba::find_not(in_data, 1); // stops on first
    res = (res && start == from);

    int in_data_2[] = {6, 6, 6, 6, 6};
    const int* end = ba::find_not(in_data_2, in_data_2 + 5, 6); // stops on the end
    res = (res && end == in_data_2 + 5);

    end = ba::find_not(in_data_2, 6); // stops on the end
    res = (res && end == in_data_2 + 5);

    const int* three = ba::find_not(from, to, 2); // stops on third element
    res = (res && three == in_data + 2);

    three = ba::find_not(in_data, 2); // stops on third element
    res = (res && three == in_data + 2);

    return res;
}

void test_sequence()
{
    {
        std::vector<int> v1;
        const dist_t<std::vector<int> > dist(v1);

        for (int i = 5; i < 15; ++i)
            v1.push_back(i);
        BOOST_CHECK_EQUAL(dist(ba::find_not(v1.begin(), v1.end(), 0)), 0);
        BOOST_CHECK_EQUAL(
            dist(ba::find_not(v1.begin(), v1.end(), v1.back())), 0);
        BOOST_CHECK_EQUAL(
            dist(ba::find_not(v1.begin(), v1.end(), v1.front())), 1);

        BOOST_CHECK_EQUAL(dist(ba::find_not(v1, 0)), 0);
        BOOST_CHECK_EQUAL(dist(ba::find_not(v1, v1.back())), 0);
        BOOST_CHECK_EQUAL(dist(ba::find_not(v1, v1.front())), 1);

        v1 = std::vector<int>(10, 2);
        BOOST_CHECK_EQUAL(dist(ba::find_not(v1.begin(), v1.end(), 0)), 0);
        BOOST_CHECK_EQUAL(
            dist(ba::find_not(v1.begin(), v1.end(), v1.back())), v1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_not(v1.begin(), v1.end(), v1.front())), v1.size());

        BOOST_CHECK_EQUAL(dist(ba::find_not(v1, 0)), 0);
        BOOST_CHECK_EQUAL(dist(ba::find_not(v1, v1.back())), v1.size());
        BOOST_CHECK_EQUAL(dist(ba::find_not(v1, v1.front())), v1.size());
    }

    //  With bidirectional iterators.
    {
        std::list<int> l1;
        const dist_t<std::list<int> > dist(l1);

        for (int i = 5; i < 15; ++i)
            l1.push_back(i);
        BOOST_CHECK_EQUAL(dist(ba::find_not(l1.begin(), l1.end(), 0)), 0);
        BOOST_CHECK_EQUAL(
            dist(ba::find_not(l1.begin(), l1.end(), l1.back())), 0);
        BOOST_CHECK_EQUAL(
            dist(ba::find_not(l1.begin(), l1.end(), l1.front())), 1);

        BOOST_CHECK_EQUAL(dist(ba::find_not(l1, 0)), 0);
        BOOST_CHECK_EQUAL(dist(ba::find_not(l1, l1.back())), 0);
        BOOST_CHECK_EQUAL(dist(ba::find_not(l1, l1.front())), 1);

        l1.clear();
        for (int i = 0; i < 10; ++i)
            l1.push_back(2);
        BOOST_CHECK_EQUAL(dist(ba::find_not(l1.begin(), l1.end(), 0)), 0);
        BOOST_CHECK_EQUAL(
            dist(ba::find_not(l1.begin(), l1.end(), l1.back())), l1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_not(l1.begin(), l1.end(), l1.front())), l1.size());

        BOOST_CHECK_EQUAL(dist(ba::find_not(l1, 0)), 0);
        BOOST_CHECK_EQUAL(dist(ba::find_not(l1, l1.back())), l1.size());
        BOOST_CHECK_EQUAL(dist(ba::find_not(l1, l1.front())), l1.size());
    }

    BOOST_CXX14_CONSTEXPR bool ce_result = check_constexpr();
    BOOST_CHECK(ce_result);
}


BOOST_AUTO_TEST_CASE(test_main)
{
    test_sequence();
}
