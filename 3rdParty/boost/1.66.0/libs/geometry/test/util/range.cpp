// Boost.Geometry
// Unit Test

// Copyright (c) 2014-2015 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Use, modification and distribution is subject to the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)


#include <geometry_test_common.hpp>

#include <iterator>
#include <vector>

#include <boost/range/iterator_range.hpp>

#include <boost/geometry/util/range.hpp>

namespace bgt {

template <bool MutableIterator>
struct beginner
{
    template <typename Range>
    typename boost::range_iterator<Range>::type
    operator()(Range & rng)
    {
        return boost::begin(rng);
    }
};
template <>
struct beginner<false>
{
    template <typename Range>
    typename boost::range_iterator<Range const>::type
    operator()(Range & rng)
    {
        return boost::const_begin(rng);
    }
};

template <bool MutableIterator>
struct ender
{
    template <typename Range>
    typename boost::range_iterator<Range>::type
    operator()(Range & rng)
    {
        return boost::end(rng);
    }
};
template <>
struct ender<false>
{
    template <typename Range>
    typename boost::range_iterator<Range const>::type
    operator()(Range & rng)
    {
        return boost::const_end(rng);
    }
};

struct NonMovable
{
    NonMovable(int ii = 0) : i(ii) {}
    NonMovable(NonMovable const& ii) : i(ii.i) {}
    NonMovable & operator=(NonMovable const& ii) { i = ii.i; return *this; }
    bool operator==(NonMovable const& ii) const { return i == ii.i; }
    int i;
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
private:
    NonMovable(NonMovable && ii);
    NonMovable & operator=(NonMovable && ii);
#endif
};

struct CopyableAndMovable
{
    CopyableAndMovable(int ii = 0) : i(ii) {}
    CopyableAndMovable(CopyableAndMovable const& ii) : i(ii.i) {}
    CopyableAndMovable & operator=(CopyableAndMovable const& ii) { i = ii.i; return *this; }
    bool operator==(CopyableAndMovable const& ii) const { return i == ii.i; }
    int i;
#ifndef BOOST_NO_CXX11_RVALUE_REFERENCES
    CopyableAndMovable(CopyableAndMovable && ii) : i(std::move(ii.i)) {}
    CopyableAndMovable & operator=(CopyableAndMovable && ii) { i = std::move(ii.i); return *this; }
#endif
};

} // namespace bgt

namespace bgr = bg::range;

template <typename T, bool MutableIterator>
void test_all()
{
    bgt::beginner<MutableIterator> begin;
    bgt::ender<MutableIterator> end;

    std::vector<T> v;
    for (int i = 0 ; i < 20 ; ++i)
    {
        bgr::push_back(v, i);
    }

    for (int i = 0 ; i < 20 ; ++i)
    {
        BOOST_CHECK(bgr::at(v, i) == i);
    }

    {
        std::vector<T> w;
        std::copy(v.begin(), v.end(), bgr::back_inserter(w));
        BOOST_CHECK(v.size() == w.size() && std::equal(v.begin(), v.end(), w.begin()));
    }

    BOOST_CHECK(bgr::front(v) == 0);
    BOOST_CHECK(bgr::back(v) == 19);

    BOOST_CHECK(boost::size(v) == 20); // [0,19]
    bgr::resize(v, 15);
    BOOST_CHECK(boost::size(v) == 15); // [0,14]
    BOOST_CHECK(bgr::back(v) == 14);

    bgr::pop_back(v);
    BOOST_CHECK(boost::size(v) == 14); // [0,13]
    BOOST_CHECK(bgr::back(v) == 13);
    
    typename std::vector<T>::iterator
        it = bgr::erase(v, end(v) - 1);
    BOOST_CHECK(boost::size(v) == 13); // [0,12]
    BOOST_CHECK(bgr::back(v) == 12);
    BOOST_CHECK(it == end(v));

    it = bgr::erase(v, end(v) - 3, end(v));
    BOOST_CHECK(boost::size(v) == 10); // [0,9]
    BOOST_CHECK(bgr::back(v) == 9);
    BOOST_CHECK(it == end(v));

    it = bgr::erase(v, begin(v) + 2);
    BOOST_CHECK(boost::size(v) == 9); // {0,1,3..9}
    BOOST_CHECK(bgr::at(v, 1) == 1);
    BOOST_CHECK(bgr::at(v, 2) == 3);
    BOOST_CHECK(bgr::back(v) == 9);
    BOOST_CHECK(it == bgr::pos(v, 2));

    it = bgr::erase(v, begin(v) + 2, begin(v) + 2);
    BOOST_CHECK(boost::size(v) == 9); // {0,1,3..9}
    BOOST_CHECK(bgr::at(v, 1) == 1);
    BOOST_CHECK(bgr::at(v, 2) == 3);
    BOOST_CHECK(bgr::back(v) == 9);
    BOOST_CHECK(it == bgr::pos(v, 2));

    it = bgr::erase(v, begin(v) + 2, begin(v) + 5);
    BOOST_CHECK(boost::size(v) == 6); // {0,1,6..9}
    BOOST_CHECK(bgr::at(v, 1) == 1);
    BOOST_CHECK(bgr::at(v, 2) == 6);
    BOOST_CHECK(bgr::back(v) == 9);
    BOOST_CHECK(it == bgr::pos(v, 2));

    it = bgr::erase(v, begin(v));
    BOOST_CHECK(boost::size(v) == 5); // {1,6..9}
    BOOST_CHECK(bgr::at(v, 0) == 1);
    BOOST_CHECK(bgr::at(v, 1) == 6);
    BOOST_CHECK(bgr::back(v) == 9);
    BOOST_CHECK(it == bgr::pos(v, 0));

    it = bgr::erase(v, begin(v), begin(v) + 3);
    BOOST_CHECK(boost::size(v) == 2); // {8,9}
    BOOST_CHECK(bgr::at(v, 0) == 8);
    BOOST_CHECK(bgr::at(v, 1) == 9);
    BOOST_CHECK(bgr::back(v) == 9);
    BOOST_CHECK(it == bgr::pos(v, 0));

    it = bgr::erase(v, begin(v), end(v));
    BOOST_CHECK(boost::size(v) == 0);
    BOOST_CHECK(it == end(v));
}

void test_detail()
{
    int arr[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    bgr::detail::copy_or_move(arr + 1, arr + 10, arr);
    BOOST_CHECK(arr[0] == 1);

    std::vector<int> v(10, 0);
    bgr::detail::copy_or_move(v.begin() + 1, v.begin() + 10, v.begin());
    BOOST_CHECK(boost::size(v) == 10);
    bgr::erase(v, v.begin() + 1);
    BOOST_CHECK(boost::size(v) == 9);

    bgt::NonMovable * arr2[10] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    bgt::NonMovable foo;
    arr2[1] = &foo;
    bgr::detail::copy_or_move(arr2 + 1, arr2 + 10, arr2);
    BOOST_CHECK(arr2[0] == &foo);

    // Storing pointers in a std::vector is not possible in MinGW C++98
#if __cplusplus >= 201103L
    std::vector<bgt::NonMovable*> v2(10, (bgt::NonMovable*)NULL);
    bgr::detail::copy_or_move(v2.begin() + 1, v2.begin() + 10, v2.begin());
    BOOST_CHECK(boost::size(v2) == 10);
    bgr::erase(v2, v2.begin() + 1);
    BOOST_CHECK(boost::size(v2) == 9);
#endif
}

template <class Iterator>
void test_pointers()
{
    int arr[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

    boost::iterator_range<Iterator> r1(arr, arr + 10);
    std::pair<Iterator, Iterator> r2(arr, arr + 10);

    BOOST_CHECK(bgr::front(r1) == 0);
    BOOST_CHECK(bgr::front(r2) == 0);
    BOOST_CHECK(bgr::back(r1) == 9);
    BOOST_CHECK(bgr::back(r2) == 9);
    BOOST_CHECK(bgr::at(r1, 5) == 5);
    BOOST_CHECK(bgr::at(r2, 5) == 5);
}

int test_main(int, char* [])
{
    test_all<int, true>();
    test_all<int, false>();
    // Storing non-movable elements in a std::vector is not possible in some implementations of STD lib
#ifdef BOOST_GEOMETRY_TEST_NONMOVABLE_ELEMENTS
    test_all<bgt::NonMovable, true>();
    test_all<bgt::NonMovable, false>();
#endif
    test_all<bgt::CopyableAndMovable, true>();
    test_all<bgt::CopyableAndMovable, false>();

    test_detail();
    test_pointers<int*>();
    test_pointers<int const*>();

    return 0;
}
