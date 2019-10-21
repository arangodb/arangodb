/* 
   Copyright (c) T. Zachary Laine 2018.

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

    For more information, see http://www.boost.org
*/
#include <iostream>

#include <boost/algorithm/find_backward.hpp>

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

BOOST_CXX14_CONSTEXPR bool check_constexpr_backward()
{
    int in_data[] = {1, 2, 3, 4, 5};
    bool res = true;

    const int* from = in_data;
    const int* to = in_data + 5;

    const int* start = ba::find_backward(from, to, 1); // stops on first
    res = (res && start == from);

    start = ba::find_backward(in_data, 1); // stops on first
    res = (res && start == from);

    const int* end = ba::find_backward(from, to, 6); // stops on the end
    res = (res && end == to);

    end = ba::find_backward(in_data, 6); // stops on the end
    res = (res && end == to);

    const int* three = ba::find_backward(from, to, 3); // stops on third element
    res = (res && three == in_data + 2);

    three = ba::find_backward(in_data, 3); // stops on third element
    res = (res && three == in_data + 2);

    return res;
}

void test_find_backward()
{
    {
        std::vector<int> v1;
        const dist_t<std::vector<int> > dist(v1);

        for (int i = 5; i < 15; ++i)
            v1.push_back(i);
        BOOST_CHECK_EQUAL(
            dist(ba::find_backward(v1.begin(), v1.end(), 0)), v1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_backward(v1.begin(), v1.end(), 100)), v1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_backward(v1.begin(), v1.end(), v1.back())),
            v1.size() - 1);
        BOOST_CHECK_EQUAL(
            dist(ba::find_backward(v1.begin(), v1.end(), v1.front())), 0);

        BOOST_CHECK_EQUAL(dist(ba::find_backward(v1, 0)), v1.size());
        BOOST_CHECK_EQUAL(dist(ba::find_backward(v1, 100)), v1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_backward(v1, v1.back())), v1.size() - 1);
        BOOST_CHECK_EQUAL(dist(ba::find_backward(v1, v1.front())), 0);
    }

    //  With bidirectional iterators.
    {
        std::list<int> l1;
        const dist_t<std::list<int> > dist(l1);

        for (int i = 5; i < 15; ++i)
            l1.push_back(i);
        BOOST_CHECK_EQUAL(
            dist(ba::find_backward(l1.begin(), l1.end(), 0)), l1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_backward(l1.begin(), l1.end(), 100)), l1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_backward(l1.begin(), l1.end(), l1.back())),
            l1.size() - 1);
        BOOST_CHECK_EQUAL(
            dist(ba::find_backward(l1.begin(), l1.end(), l1.front())), 0);

        BOOST_CHECK_EQUAL(dist(ba::find_backward(l1, 0)), l1.size());
        BOOST_CHECK_EQUAL(dist(ba::find_backward(l1, 100)), l1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_backward(l1, l1.back())), l1.size() - 1);
        BOOST_CHECK_EQUAL(dist(ba::find_backward(l1, l1.front())), 0);
    }

    BOOST_CXX14_CONSTEXPR bool ce_result = check_constexpr_backward();
    BOOST_CHECK(ce_result);
}

struct equals
{
    BOOST_CXX14_CONSTEXPR equals(int n) : n_(n) {}
    BOOST_CXX14_CONSTEXPR bool operator()(int i) { return i == n_; }
    int n_;
};

BOOST_CXX14_CONSTEXPR bool check_constexpr_if_backward()
{
    int in_data[] = {1, 2, 3, 4, 5};
    bool res = true;

    const int* from = in_data;
    const int* to = in_data + 5;

    const int* start = ba::find_if_backward(from, to, equals(1)); // stops on first
    res = (res && start == from);

    start = ba::find_if_backward(in_data, equals(1)); // stops on first
    res = (res && start == from);

    const int* end = ba::find_if_backward(from, to, equals(6)); // stops on the end
    res = (res && end == to);

    end = ba::find_if_backward(in_data, equals(6)); // stops on the end
    res = (res && end == to);

    const int* three = ba::find_if_backward(from, to, equals(3)); // stops on third element
    res = (res && three == in_data + 2);

    three = ba::find_if_backward(in_data, equals(3)); // stops on third element
    res = (res && three == in_data + 2);

    return res;
}

void test_find_if_backward()
{
    {
        std::vector<int> v1;
        const dist_t<std::vector<int> > dist(v1);

        for (int i = 5; i < 15; ++i)
            v1.push_back(i);
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_backward(v1.begin(), v1.end(), equals(0))),
            v1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_backward(v1.begin(), v1.end(), equals(100))),
            v1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_backward(v1.begin(), v1.end(), equals(v1.back()))),
            v1.size() - 1);
        BOOST_CHECK_EQUAL(
            dist(
                ba::find_if_backward(v1.begin(), v1.end(), equals(v1.front()))),
            0);

        BOOST_CHECK_EQUAL(dist(ba::find_if_backward(v1, equals(0))), v1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_backward(v1, equals(100))), v1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_backward(v1, equals(v1.back()))), v1.size() - 1);
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_backward(v1, equals(v1.front()))), 0);
    }

    //  With bidirectional iterators.
    {
        std::list<int> l1;
        const dist_t<std::list<int> > dist(l1);

        for (int i = 5; i < 15; ++i)
            l1.push_back(i);
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_backward(l1.begin(), l1.end(), equals(0))),
            l1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_backward(l1.begin(), l1.end(), equals(100))),
            l1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_backward(l1.begin(), l1.end(), equals(l1.back()))),
            l1.size() - 1);
        BOOST_CHECK_EQUAL(
            dist(
                ba::find_if_backward(l1.begin(), l1.end(), equals(l1.front()))),
            0);

        BOOST_CHECK_EQUAL(dist(ba::find_if_backward(l1, equals(0))), l1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_backward(l1, equals(100))), l1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_backward(l1, equals(l1.back()))), l1.size() - 1);
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_backward(l1, equals(l1.front()))), 0);
    }

    BOOST_CXX14_CONSTEXPR bool ce_result = check_constexpr_if_backward();
    BOOST_CHECK(ce_result);
}

struct not_equals
{
    BOOST_CXX14_CONSTEXPR not_equals(int n) : n_(n) {}
    BOOST_CXX14_CONSTEXPR bool operator()(int i) { return i != n_; }
    int n_;
};

BOOST_CXX14_CONSTEXPR bool check_constexpr_if_not_backward()
{
    int in_data[] = {1, 2, 3, 4, 5};
    bool res = true;

    const int* from = in_data;
    const int* to = in_data + 5;

    const int* start = ba::find_if_not_backward(from, to, not_equals(1)); // stops on first
    res = (res && start == from);

    start = ba::find_if_not_backward(in_data, not_equals(1)); // stops on first
    res = (res && start == from);

    const int* end = ba::find_if_not_backward(from, to, not_equals(6)); // stops on the end
    res = (res && end == to);

    end = ba::find_if_not_backward(in_data, not_equals(6)); // stops on the end
    res = (res && end == to);

    const int* three = ba::find_if_not_backward(from, to, not_equals(3)); // stops on third element
    res = (res && three == in_data + 2);

    three = ba::find_if_not_backward(in_data, not_equals(3)); // stops on third element
    res = (res && three == in_data + 2);

    return res;
}

void test_find_if_not_backward()
{
    {
        std::vector<int> v1;
        const dist_t<std::vector<int> > dist(v1);

        for (int i = 5; i < 15; ++i)
            v1.push_back(i);
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(v1.begin(), v1.end(), not_equals(0))),
            v1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(
                v1.begin(), v1.end(), not_equals(100))),
            v1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(
                v1.begin(), v1.end(), not_equals(v1.back()))),
            v1.size() - 1);
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(
                v1.begin(), v1.end(), not_equals(v1.front()))),
            0);

        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(v1, not_equals(0))), v1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(v1, not_equals(100))), v1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(v1, not_equals(v1.back()))),
            v1.size() - 1);
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(v1, not_equals(v1.front()))), 0);
    }

    //  With bidirectional iterators.
    {
        std::list<int> l1;
        const dist_t<std::list<int> > dist(l1);

        for (int i = 5; i < 15; ++i)
            l1.push_back(i);
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(l1.begin(), l1.end(), not_equals(0))),
            l1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(
                l1.begin(), l1.end(), not_equals(100))),
            l1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(
                l1.begin(), l1.end(), not_equals(l1.back()))),
            l1.size() - 1);
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(
                l1.begin(), l1.end(), not_equals(l1.front()))),
            0);

        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(l1, not_equals(0))), l1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(l1, not_equals(100))), l1.size());
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(l1, not_equals(l1.back()))),
            l1.size() - 1);
        BOOST_CHECK_EQUAL(
            dist(ba::find_if_not_backward(l1, not_equals(l1.front()))), 0);
    }

    BOOST_CXX14_CONSTEXPR bool ce_result = check_constexpr_if_not_backward();
    BOOST_CHECK(ce_result);
}

BOOST_CXX14_CONSTEXPR bool check_constexpr_not_backward()
{
    int in_data[] = {1, 5, 5, 5, 5};
    bool res = true;

    const int* from = in_data;
    const int* to = in_data + 5;

    const int* start = ba::find_not_backward(from, to, 5); // stops on first
    res = (res && start == from);

    start = ba::find_not_backward(in_data, 5); // stops on first
    res = (res && start == from);

    const int in_data_2[] = {6, 6, 6, 6, 6};
    const int* end = ba::find_not_backward(in_data_2, in_data_2 + 5, 6); // stops on the end
    res = (res && end == in_data_2 + 5);

    end = ba::find_not_backward(in_data_2, 6); // stops on the end
    res = (res && end == in_data_2 + 5);

    return res;
}

void test_find_not_backward()
{
    {
        std::vector<int> v1;
        const dist_t<std::vector<int> > dist(v1);

        for (int i = 0; i < 5; ++i)
            v1.push_back(0);
        for (int i = 0; i < 5; ++i)
            v1.push_back(1);
        BOOST_CHECK_EQUAL(
            dist(ba::find_not_backward(v1.begin(), v1.end(), 1)), 4);
        BOOST_CHECK_EQUAL(
            dist(ba::find_not_backward(v1.begin(), v1.end(), 0)),
            v1.size() - 1);
        BOOST_CHECK_EQUAL(
            dist(ba::find_not_backward(v1.begin(), v1.end(), 2)),
            v1.size() - 1);

        BOOST_CHECK_EQUAL(dist(ba::find_not_backward(v1, 1)), 4);
        BOOST_CHECK_EQUAL(dist(ba::find_not_backward(v1, 0)), v1.size() - 1);
        BOOST_CHECK_EQUAL(dist(ba::find_not_backward(v1, 2)), v1.size() - 1);

        v1.resize(5);
        BOOST_CHECK_EQUAL(
            dist(ba::find_not_backward(v1.begin(), v1.end(), 0)), v1.size());

        BOOST_CHECK_EQUAL(dist(ba::find_not_backward(v1, 0)), v1.size());
    }

    //  With bidirectional iterators.
    {
        std::list<int> l1;
        const dist_t<std::list<int> > dist(l1);

        for (int i = 0; i < 5; ++i)
            l1.push_back(0);
        for (int i = 0; i < 5; ++i)
            l1.push_back(1);
        BOOST_CHECK_EQUAL(
            dist(ba::find_not_backward(l1.begin(), l1.end(), 1)), 4);
        BOOST_CHECK_EQUAL(
            dist(ba::find_not_backward(l1.begin(), l1.end(), 0)),
            l1.size() - 1);
        BOOST_CHECK_EQUAL(
            dist(ba::find_not_backward(l1.begin(), l1.end(), 2)),
            l1.size() - 1);

        BOOST_CHECK_EQUAL(dist(ba::find_not_backward(l1, 1)), 4);
        BOOST_CHECK_EQUAL(dist(ba::find_not_backward(l1, 0)), l1.size() - 1);
        BOOST_CHECK_EQUAL(dist(ba::find_not_backward(l1, 2)), l1.size() - 1);

        l1.resize(5);
        BOOST_CHECK_EQUAL(
            dist(ba::find_not_backward(l1.begin(), l1.end(), 0)), l1.size());

        BOOST_CHECK_EQUAL(dist(ba::find_not_backward(l1, 0)), l1.size());
    }

    BOOST_CXX14_CONSTEXPR bool ce_result = check_constexpr_not_backward();
    BOOST_CHECK(ce_result);
}

BOOST_AUTO_TEST_CASE(test_main)
{
    test_find_backward();
    test_find_if_backward();
    test_find_if_not_backward();
    test_find_not_backward();
}
