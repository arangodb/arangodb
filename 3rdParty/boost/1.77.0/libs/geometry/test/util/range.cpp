// Boost.Geometry
// Unit Test

// Copyright (c) 2014-2021 Oracle and/or its affiliates.

// Contributed and/or modified by Adam Wulkiewicz, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#include <geometry_test_common.hpp>

#include <iterator>
#include <vector>

#include <boost/range/iterator_range.hpp>

#include <boost/geometry/util/range.hpp>

namespace bgt {

struct NonCopyable
{
    NonCopyable(int ii = 0) : i(ii) {}
    NonCopyable(NonCopyable && ii) : i(ii.i) {}
    NonCopyable & operator=(NonCopyable && ii) { i = ii.i; return *this; }
    bool operator==(NonCopyable const& ii) const { return i == ii.i; }
    int i;
    NonCopyable(NonCopyable const& ii) = delete;
    NonCopyable & operator=(NonCopyable const& ii) = delete;
};

struct CopyableAndMovable
{
    CopyableAndMovable(int ii = 0) : i(ii) {}
    CopyableAndMovable(CopyableAndMovable const& ii) : i(ii.i) {}
    CopyableAndMovable & operator=(CopyableAndMovable const& ii) { i = ii.i; return *this; }
    bool operator==(CopyableAndMovable const& ii) const { return i == ii.i; }
    int i;
    CopyableAndMovable(CopyableAndMovable && ii) : i(std::move(ii.i)) {}
    CopyableAndMovable & operator=(CopyableAndMovable && ii) { i = std::move(ii.i); return *this; }
};

template <typename Container>
struct proxy
{
    using iterator = typename boost::range_iterator<Container>::type;
    using const_iterator = typename boost::range_iterator<Container const>::type;

    explicit proxy(Container & container) : m_ptr(&container) {}
    template <typename T>
    void push_back(T&& v) { m_ptr->push_back(std::forward<T>(v)); }
    template <typename ...Ts>
    void emplace_back(Ts&&... vs) { m_ptr->emplace_back(std::forward<Ts>(vs)...); }
    void clear() { m_ptr->clear(); }
    void resize(size_t n) { m_ptr->resize(n); }

    iterator begin() { return m_ptr->begin(); }
    const_iterator begin() const { return m_ptr->begin(); }
    iterator end() { return m_ptr->end(); }
    const_iterator end() const { return m_ptr->end(); }

private:
    Container * m_ptr;
};

template <typename Container>
bgt::proxy<Container> make_proxy(Container & container)
{
    return bgt::proxy<Container>(container);
}

} // namespace bgt

namespace bgr = bg::range;

template <typename T>
void fill(std::vector<T> & v, int n)
{
    for (int i = 0; i < n; ++i)
    {
        bgr::push_back(v, i);
    }
}

template <typename T>
void test_push_emplace(std::vector<T> & v)
{
    bgr::push_back(v, 0);
    bgr::push_back(bgt::make_proxy(v), 1);
    bgr::emplace_back(v, 2);
    bgr::emplace_back(bgt::make_proxy(v), 3);

    typename std::vector<T>::value_type val = 4;
    bgr::push_back(v, std::move(val));
    val = 5;
    bgr::push_back(bgt::make_proxy(v), std::move(val));
    val = 6;
    bgr::emplace_back(v, std::move(val));
    val = 7;
    bgr::emplace_back(bgt::make_proxy(v), std::move(val));

    for (int i = (int)v.size(); i < 20; ++i)
    {
        bgr::push_back(v, i);
    }
}

template <typename T>
void test_at_front_back(std::vector<T> const& v)
{
    BOOST_CHECK(bgr::at(v, 1) == 1);
    BOOST_CHECK(bgr::at(bgt::make_proxy(v), 2) == 2);
    BOOST_CHECK(bgr::at(std::make_pair(v.begin(), v.end()), 3) == 3);

    BOOST_CHECK(bgr::front(v) == 0);
    BOOST_CHECK(bgr::front(bgt::make_proxy(v)) == 0);
    BOOST_CHECK(bgr::front(std::make_pair(v.begin(), v.end())) == 0);

    BOOST_CHECK(bgr::back(v) == 19);
    BOOST_CHECK(bgr::back(bgt::make_proxy(v)) == 19);
    BOOST_CHECK(bgr::back(std::make_pair(v.begin(), v.end())) == 19);
}

template <typename T>
void test_at_front_back(std::vector<T> & v)
{
    bgr::at(v, 1) = 101;
    bgr::at(bgt::make_proxy(v), 2) = 102;
    bgr::at(std::make_pair(v.begin(), v.end()), 3) = 103;

    BOOST_CHECK(bgr::at(v, 1) == 101);
    BOOST_CHECK(bgr::at(bgt::make_proxy(v), 2) == 102);
    BOOST_CHECK(bgr::at(std::make_pair(v.begin(), v.end()), 3) == 103);

    bgr::at(v, 1) = 1;
    bgr::at(bgt::make_proxy(v), 2) = 2;
    bgr::at(std::make_pair(v.begin(), v.end()), 3) = 3;

    bgr::front(v) = 100;
    BOOST_CHECK(bgr::front(v) == 100);
    bgr::front(bgt::make_proxy(v)) = 200;
    BOOST_CHECK(bgr::front(v) == 200);
    bgr::front(std::make_pair(v.begin(), v.end())) = 0;
    BOOST_CHECK(bgr::front(v) == 0);

    bgr::back(v) = 119;
    BOOST_CHECK(bgr::back(v) == 119);
    bgr::back(bgt::make_proxy(v)) = 219;
    BOOST_CHECK(bgr::back(v) == 219);
    bgr::back(std::make_pair(v.begin(), v.end())) = 19;
    BOOST_CHECK(bgr::back(v) == 19);
}

template <typename T>
void test_clear(std::vector<T> & v)
{
    std::vector<T> w;
    std::copy(v.begin(), v.end(), bgr::back_inserter(w));
    BOOST_CHECK(w.size() == 20 && std::equal(v.begin(), v.end(), w.begin()));
    bgr::clear(w);
    BOOST_CHECK(w.size() == 0);
    w = v;
    BOOST_CHECK(w.size() == 20);
    bgr::clear(bgt::make_proxy(w));
    BOOST_CHECK(w.size() == 0);
}

void test_clear(std::vector<bgt::NonCopyable> & )
{}

template <typename T>
void test_resize_pop(std::vector<T> & v)
{
    BOOST_CHECK(boost::size(v) == 20); // [0,19]
    bgr::resize(v, 18);
    BOOST_CHECK(boost::size(v) == 18); // [0,17]
    bgr::resize(bgt::make_proxy(v), 16);
    BOOST_CHECK(boost::size(v) == 16); // [0,15]
    BOOST_CHECK(bgr::back(v) == 15);

    bgr::pop_back(v);
    BOOST_CHECK(boost::size(v) == 15); // [0,14]
    bgr::pop_back(bgt::make_proxy(v));
    BOOST_CHECK(boost::size(v) == 14); // [0,13]
    BOOST_CHECK(bgr::back(v) == 13);
}

template <typename T, typename Begin, typename End>
void test_erase(std::vector<T> & v, Begin begin, End end)
{
    typename std::vector<T>::iterator
        it = bgr::erase(v, end(v) - 1);
    BOOST_CHECK(boost::size(v) == 13); // [0,12]
    BOOST_CHECK(bgr::back(v) == 12);
    BOOST_CHECK(it == end(v));

    it = bgr::erase(v, end(v) - 3, end(v));
    BOOST_CHECK(boost::size(v) == 10); // [0,9]
    BOOST_CHECK(bgr::back(v) == 9);
    BOOST_CHECK(it == end(v));

    it = bgr::erase(bgt::make_proxy(v), begin(v) + 2);
    BOOST_CHECK(boost::size(v) == 9); // {0,1,3..9}
    BOOST_CHECK(bgr::at(v, 1) == 1);
    BOOST_CHECK(bgr::at(v, 2) == 3);
    BOOST_CHECK(bgr::back(v) == 9);
    BOOST_CHECK(it == bgr::pos(v, 2));

    it = bgr::erase(bgt::make_proxy(v), begin(v) + 2, begin(v) + 2);
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

template <typename T, typename Begin, typename End>
void test_erase(std::vector<bgt::NonCopyable> & , Begin , End )
{}

template <typename T>
void test_all()
{
    std::vector<T> v;
    
    test_push_emplace(v);

    BOOST_CHECK(boost::size(v) == 20);

    test_at_front_back(v);
    test_at_front_back(const_cast<std::vector<T> const&>(v));

    test_clear(v);

    test_resize_pop(v);

    int n = (int)v.size();
    
    test_erase(v, [](auto & rng) { return boost::begin(rng); },
                  [](auto & rng) { return boost::end(rng); });

    bgr::clear(v);
    fill(v, n);

    test_erase(v, [](auto & rng) { return boost::const_begin(rng); },
                  [](auto & rng) { return boost::const_end(rng); });
}

void test_detail()
{
    int arr[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    std::move(arr + 1, arr + 10, arr);
    BOOST_CHECK(arr[0] == 1);

    std::vector<int> v(10, 0);
    std::move(v.begin() + 1, v.begin() + 10, v.begin());
    BOOST_CHECK(boost::size(v) == 10);
    bgr::erase(v, v.begin() + 1);
    BOOST_CHECK(boost::size(v) == 9);
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
    test_all<int>();
    test_all<bgt::CopyableAndMovable>();
    test_all<bgt::NonCopyable>();

    test_detail();

    test_pointers<int*>();
    test_pointers<int const*>();

    return 0;
}
