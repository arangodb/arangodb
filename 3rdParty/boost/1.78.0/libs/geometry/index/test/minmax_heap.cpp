// Boost.Geometry

// Copyright (c) 2021, Oracle and/or its affiliates.
// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <boost/test/included/test_exec_monitor.hpp>
#include <boost/test/impl/execution_monitor.ipp>

#include <algorithm>
#include <vector>
#include <map>

#include <boost/geometry/index/detail/minmax_heap.hpp>
#include <boost/geometry/index/detail/maxmin_heap.hpp>

using namespace boost::geometry::index::detail;

struct noncopyable
{
    noncopyable(int i_) : i(i_) {}
    noncopyable(noncopyable const&) = delete;
    noncopyable& operator=(noncopyable const&) = delete;
    noncopyable(noncopyable&&) = default;
    noncopyable& operator=(noncopyable&&) = default;
    bool operator<(noncopyable const& other) const { return i < other.i; }
    operator int() const { return i; }
    int i;
};

template <typename T>
struct minmax_default
{
    minmax_default() = default;
    minmax_default(std::vector<int> const& vec)
        : heap(vec.begin(), vec.end())
    {
        make_minmax_heap(heap.begin(), heap.end());
    }
    T const& top() const
    {
        return heap[0];
    }
    T const& bottom() const
    {
        return bottom_minmax_heap(heap.begin(), heap.end());
    }
    void push(int i)
    {
        heap.push_back(T(i));
        push_minmax_heap(heap.begin(), heap.end());
    }
    void pop_top()
    {
        pop_top_minmax_heap(heap.begin(), heap.end());
        heap.pop_back();
    }
    void pop_bottom()
    {
        pop_bottom_minmax_heap(heap.begin(), heap.end());
        heap.pop_back();
    }
    bool is_heap() const
    {
        return is_minmax_heap(heap.begin(), heap.end());
    }
    bool empty() const
    {
        return heap.empty();
    }
    std::vector<T> heap;
};

template <typename T>
struct minmax_less
{
    minmax_less() = default;
    minmax_less(std::vector<int> const& vec)
        : heap(vec.begin(), vec.end())
    {
        make_minmax_heap(heap.begin(), heap.end(), std::less<>());
    }
    T const& top() const
    {
        return heap[0];
    }
    T const& bottom() const
    {
        return bottom_minmax_heap(heap.begin(), heap.end(), std::less<>());
    }
    void push(int i)
    {
        heap.push_back(T(i));
        push_minmax_heap(heap.begin(), heap.end(), std::less<>());
    }
    void pop_top()
    {
        pop_top_minmax_heap(heap.begin(), heap.end(), std::less<>());
        heap.pop_back();
    }
    void pop_bottom()
    {
        pop_bottom_minmax_heap(heap.begin(), heap.end(), std::less<>());
        heap.pop_back();
    }
    bool is_heap() const
    {
        return is_minmax_heap(heap.begin(), heap.end(), std::less<>());
    }
    bool empty() const
    {
        return heap.empty();
    }
    std::vector<T> heap;
};

template <typename T>
struct maxmin_greater
{
    maxmin_greater() = default;
    maxmin_greater(std::vector<int> const& vec)
        : heap(vec.begin(), vec.end())
    {
        make_maxmin_heap(heap.begin(), heap.end(), std::greater<>());
    }
    T const& top() const
    {
        return heap[0];
    }
    T const& bottom() const
    {
        return bottom_maxmin_heap(heap.begin(), heap.end(), std::greater<>());
    }
    void push(int i)
    {
        heap.push_back(T(i));
        push_maxmin_heap(heap.begin(), heap.end(), std::greater<>());
    }
    void pop_top()
    {
        pop_top_maxmin_heap(heap.begin(), heap.end(), std::greater<>());
        heap.pop_back();
    }
    void pop_bottom()
    {
        pop_bottom_maxmin_heap(heap.begin(), heap.end(), std::greater<>());
        heap.pop_back();
    }
    bool is_heap() const
    {
        return is_maxmin_heap(heap.begin(), heap.end(), std::greater<>());
    }
    bool empty() const
    {
        return heap.empty();
    }
    std::vector<T> heap;
};

template <typename T>
struct maxmin_default_switch
{
    maxmin_default_switch() = default;
    maxmin_default_switch(std::vector<int> const& vec)
        : heap(vec.begin(), vec.end())
    {
        make_maxmin_heap(heap.begin(), heap.end());
    }
    T const& bottom() const
    {
        return heap[0];
    }
    T const& top() const
    {
        return bottom_maxmin_heap(heap.begin(), heap.end());
    }
    void push(int i)
    {
        heap.push_back(T(i));
        push_maxmin_heap(heap.begin(), heap.end());
    }
    void pop_top()
    {
        pop_bottom_maxmin_heap(heap.begin(), heap.end());
        heap.pop_back();
    }
    void pop_bottom()
    {
        pop_top_maxmin_heap(heap.begin(), heap.end());
        heap.pop_back();
    }
    bool is_heap() const
    {
        return is_maxmin_heap(heap.begin(), heap.end());
    }
    bool empty() const
    {
        return heap.empty();
    }
    std::vector<T> heap;
};

template <typename Heap>
void test()
{
    std::vector<int> vec;
    int const n = 20;
    for (int i = 0; i < n; ++i)
    {
        vec.push_back(rand() % n);
    }

    {
        std::map<int, int> map;
        Heap heap;
        for (int i : vec)
        {
            heap.push(i);
            BOOST_CHECK(heap.is_heap());
            
            map[i]++;
            BOOST_CHECK_EQUAL(heap.top(), map.begin()->first);
            BOOST_CHECK_EQUAL(heap.bottom(), (--map.end())->first);
        }

        while (! heap.empty())
        {
            int i = heap.top();
            BOOST_CHECK_EQUAL(i, map.begin()->first);
            BOOST_CHECK_EQUAL(heap.bottom(), (--map.end())->first);
            BOOST_CHECK(map[i] > 0);
            map[i]--;
            if (map[i] <= 0)
                map.erase(i);

            heap.pop_top();
            BOOST_CHECK(heap.is_heap());
        }

        BOOST_CHECK(map.empty());
    }

    {
        Heap heap(vec);
        BOOST_CHECK(heap.is_heap());
        
        std::map<int, int> map;
        for (int i : vec)
            map[i]++;
        BOOST_CHECK_EQUAL(heap.top(), map.begin()->first);
        BOOST_CHECK_EQUAL(heap.bottom(), (--map.end())->first);

        while (! heap.empty())
        {
            int i = heap.bottom();
            BOOST_CHECK_EQUAL(heap.top(), map.begin()->first);
            BOOST_CHECK_EQUAL(i, (--map.end())->first);
            BOOST_CHECK(map[i] > 0);
            map[i]--;
            if (map[i] <= 0)
                map.erase(i);

            heap.pop_bottom();
            BOOST_CHECK(heap.is_heap());
        }

        BOOST_CHECK(map.empty());
    }
}

int test_main(int, char* [])
{
    test<minmax_default<int>>();
    test<minmax_default<noncopyable>>();
    test<minmax_less<int>>();
    test<maxmin_greater<int>>();
    test<maxmin_default_switch<int>>();

    return 0;
}
