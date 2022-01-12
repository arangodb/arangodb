// Copyright (c) 2018-2021 Emil Dotchevski and Reverge Studios, Inc.

// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

#ifdef BOOST_LEAF_TEST_SINGLE_HEADER
#   include "leaf.hpp"
#else
#   include <boost/leaf/error.hpp>
#endif

#include "lightweight_test.hpp"
#include <future>
#include <vector>
#include <algorithm>
#include <iterator>

namespace leaf = boost::leaf;

constexpr int ids_per_thread = 10000;

std::vector<int> generate_ids()
{
    std::vector<int> ids;
    ids.reserve(ids_per_thread);
    for(int i=0; i!=ids_per_thread-1; ++i)
    {
        int id = leaf::leaf_detail::new_id();
        BOOST_TEST_NE(id&1, 0);
        int last = leaf::leaf_detail::current_id();
        BOOST_TEST_EQ(last, leaf::leaf_detail::current_id());
        BOOST_TEST_NE(last&1, 0);
        BOOST_TEST_EQ(last, id);
        ids.push_back(id);
    }
    return ids;
}

int main()
{
    {
        leaf::error_id e1;
        leaf::error_id e2;
        BOOST_TEST(!e1);
        BOOST_TEST_EQ(e1.value(), 0);
        BOOST_TEST(!e2);
        BOOST_TEST_EQ(e2.value(), 0);
        BOOST_TEST(e1==e2);
        BOOST_TEST(!(e1!=e2));
        BOOST_TEST(!(e1<e2));
        BOOST_TEST(!(e2<e1));
    }
    {
        leaf::error_id e1;
        leaf::error_id e2 = leaf::new_error();
        BOOST_TEST(!e1);
        BOOST_TEST_EQ(e1.value(), 0);
        BOOST_TEST(e2);
        BOOST_TEST_EQ(e2.value(), 1);
        BOOST_TEST(!(e1==e2));
        BOOST_TEST(e1!=e2);
        BOOST_TEST(e1<e2);
        BOOST_TEST(!(e2<e1));
    }
    {
        leaf::error_id e1 = leaf::new_error();
        leaf::error_id e2 = leaf::new_error();
        BOOST_TEST(e1);
        BOOST_TEST_EQ(e1.value(), 5);
        BOOST_TEST(e2);
        BOOST_TEST_EQ(e2.value(), 9);
        BOOST_TEST(!(e1==e2));
        BOOST_TEST(e1!=e2);
        BOOST_TEST(e1<e2);
        BOOST_TEST(!(e2<e1));
    }
    {
        leaf::error_id e1 = leaf::new_error();
        leaf::error_id e2 = e1;
        BOOST_TEST(e1);
        BOOST_TEST_EQ(e1.value(), 13);
        BOOST_TEST(e2);
        BOOST_TEST_EQ(e2.value(), 13);
        BOOST_TEST(e1==e2);
        BOOST_TEST(!(e1!=e2));
        BOOST_TEST(!(e1<e2));
        BOOST_TEST(!(e2<e1));
    }
    {
        leaf::error_id e1 = leaf::new_error();
        leaf::error_id e2; e2 = e1;
        BOOST_TEST(e1);
        BOOST_TEST_EQ(e1.value(), 17);
        BOOST_TEST(e2);
        BOOST_TEST_EQ(e2.value(), 17);
        BOOST_TEST(e1==e2);
        BOOST_TEST(!(e1!=e2));
        BOOST_TEST(!(e1<e2));
        BOOST_TEST(!(e2<e1));
    }
#ifdef BOOST_LEAF_NO_THREADS
    std::vector<int> all_ids = generate_ids();
#else
    constexpr int thread_count = 100;
    using thread_ids = std::future<std::vector<int>>;
    std::vector<thread_ids> fut;
    fut.reserve(thread_count);
    std::generate_n(
        std::back_inserter(fut),
        thread_count,
        [=]
        {
            return std::async(std::launch::async, &generate_ids);
        });
    std::vector<int> all_ids;
    for(auto & f : fut)
    {
        auto fv = f.get();
        all_ids.insert(all_ids.end(), fv.begin(), fv.end());
    }
#endif
    std::sort(all_ids.begin(), all_ids.end());
    auto u = std::unique(all_ids.begin(), all_ids.end());
    BOOST_TEST(u==all_ids.end());

    return boost::report_errors();
}
