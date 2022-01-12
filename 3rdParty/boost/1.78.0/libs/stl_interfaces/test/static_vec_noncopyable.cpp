// Copyright (C) 2019 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include "../example/static_vector.hpp"

#include "ill_formed.hpp"

#include <boost/core/lightweight_test.hpp>

#include <array>


struct noncopyable_int
{
    noncopyable_int() : value_(0) {}

    noncopyable_int(noncopyable_int const &) = delete;
    noncopyable_int(noncopyable_int && other) : value_(other.value_) {}
    noncopyable_int & operator=(noncopyable_int const &) = delete;
    noncopyable_int & operator=(noncopyable_int && rhs)
    {
        value_ = rhs.value_;
        return *this;
    }

    noncopyable_int(int i) : value_(i) {}
    noncopyable_int & operator=(int i)
    {
        value_ = i;
        return *this;
    }

    operator int() const { return value_; }

    int value_;
};

void swap(noncopyable_int & x, noncopyable_int & y)
{
    std::swap(x.value_, y.value_);
}

#define USE_STD_VECTOR 0

#if USE_STD_VECTOR
using vec_type = std::vector<noncopyable_int>;
#else
using vec_type = static_vector<noncopyable_int, 10>;
#endif


void test_default_ctor()
{
    vec_type v;
    BOOST_TEST(v.empty());
    BOOST_TEST(v.size() == 0u);

#if !USE_STD_VECTOR
    BOOST_TEST(v.max_size() == 10u);
    BOOST_TEST(v.capacity() == 10u);
#endif

    BOOST_TEST(v == v);
    BOOST_TEST(v <= v);
    BOOST_TEST(v >= v);

    BOOST_TEST_THROWS(v.at(0), std::out_of_range);

    vec_type const & cv = v;
    BOOST_TEST(cv.empty());
    BOOST_TEST(cv.size() == 0u);

#if !USE_STD_VECTOR
    BOOST_TEST(cv.max_size() == 10u);
    BOOST_TEST(cv.capacity() == 10u);
#endif

    BOOST_TEST(cv == cv);
    BOOST_TEST(cv <= cv);
    BOOST_TEST(cv >= cv);

    BOOST_TEST_THROWS(cv.at(0), std::out_of_range);
}


void test_other_ctors_assign_ctor()
{
    {
        vec_type v(3);
        BOOST_TEST(!v.empty());
        BOOST_TEST(v.size() == 3u);
    }

#if 0 // initializer_list construction is not supported for move-only value_type.
        vec_type v2(std::initializer_list<noncopyable_int>{0, 0, 0});
        BOOST_TEST(v == v2);

    {
        std::initializer_list<noncopyable_int> il{3, 2, 1};
        vec_type v(il);
        BOOST_TEST(!v.empty());
        BOOST_TEST(v.size() == 3u);

        static_assert(std::is_same<decltype(v = il), vec_type &>::value, "");

        vec_type v2;
        v2 = il;
        BOOST_TEST(v == v2);
    }

    {
        std::initializer_list<noncopyable_int> il{3, 2, 1};
        vec_type v;
        v.assign(il);
        BOOST_TEST(!v.empty());
        BOOST_TEST(v.size() == 3u);

        static_assert(std::is_same<decltype(v.assign(il)), void>::value, "");

        vec_type v2;
        v2 = il;
        BOOST_TEST(v == v2);
    }
#endif

#if 0 // Code that boils down to fill-insert is not supported for move-only value_type.
    {
        vec_type v(3, 4);
        BOOST_TEST(!v.empty());
        BOOST_TEST(v.size() == 3u);

        vec_type v2 = {4, 4, 4};
        BOOST_TEST(v == v2);
    }

    {
        vec_type v;
        v.assign(3, 4);
        BOOST_TEST(!v.empty());
        BOOST_TEST(v.size() == 3u);

        static_assert(std::is_same<decltype(v.assign(3, 4)), void>::value, "");

        vec_type v2 = {4, 4, 4};
        BOOST_TEST(v == v2);
    }
#endif

    {
        std::array<int, 3> a = {{1, 2, 3}};

        vec_type v(a.begin(), a.end());
        BOOST_TEST(!v.empty());
        BOOST_TEST(v.size() == 3u);

        vec_type v2;
        v2.push_back(1);
        v2.push_back(2);
        v2.push_back(3);
        BOOST_TEST(v == v2);
    }

    {
        std::array<int, 3> a = {{1, 2, 3}};

        vec_type v;
        v.assign(a.begin(), a.end());
        BOOST_TEST(!v.empty());
        BOOST_TEST(v.size() == 3u);

        static_assert(
            std::is_same<decltype(v.assign(a.begin(), a.end())), void>::value,
            "");

        vec_type v2;
        v2.push_back(1);
        v2.push_back(2);
        v2.push_back(3);
        BOOST_TEST(v == v2);
    }
}


void test_resize()
{
    {
        vec_type v;

        static_assert(std::is_same<decltype(v.resize(1)), void>::value, "");

        v.resize(3);
        BOOST_TEST(v == vec_type(3));

        v.resize(6);
        BOOST_TEST(v == vec_type(6));
    }

    {
        vec_type v(6);

        v.resize(3);
        BOOST_TEST(v == vec_type(3));

        v.resize(0);
        BOOST_TEST(v == vec_type{});
    }
}


void test_assignment_copy_move_equality()
{
    {
        vec_type v2;
        v2.push_back(4);
        v2.push_back(4);
        v2.push_back(4);

        vec_type v(std::move(v2));
        BOOST_TEST(v[0] == 4);
        BOOST_TEST(v[1] == 4);
        BOOST_TEST(v[2] == 4);
        BOOST_TEST(v2.empty());
    }

    {
        vec_type v;
        vec_type v2;
        v2.push_back(4);
        v2.push_back(4);
        v2.push_back(4);

        v = std::move(v2);
        BOOST_TEST(v[0] == 4);
        BOOST_TEST(v[1] == 4);
        BOOST_TEST(v[2] == 4);
        BOOST_TEST(v2.empty());
    }
}


void test_comparisons()
{
    vec_type sm;
    sm.push_back(1);
    sm.push_back(2);
    sm.push_back(3);
    vec_type md;
    md.push_back(1);
    md.push_back(2);
    md.push_back(3);
    md.push_back(4);
    vec_type lg;
    lg.push_back(1);
    lg.push_back(2);
    lg.push_back(3);
    lg.push_back(4);
    lg.push_back(5);

    BOOST_TEST(sm == sm);
    BOOST_TEST(!(sm == md));
    BOOST_TEST(!(sm == lg));

    BOOST_TEST(!(sm != sm));
    BOOST_TEST(sm != md);
    BOOST_TEST(sm != lg);

    BOOST_TEST(!(sm < sm));
    BOOST_TEST(sm < md);
    BOOST_TEST(sm < lg);

    BOOST_TEST(sm <= sm);
    BOOST_TEST(sm <= md);
    BOOST_TEST(sm <= lg);

    BOOST_TEST(!(sm > sm));
    BOOST_TEST(!(sm > md));
    BOOST_TEST(!(sm > lg));

    BOOST_TEST(sm >= sm);
    BOOST_TEST(!(sm >= md));
    BOOST_TEST(!(sm >= lg));


    BOOST_TEST(!(md == sm));
    BOOST_TEST(md == md);
    BOOST_TEST(!(md == lg));

    BOOST_TEST(!(md < sm));
    BOOST_TEST(!(md < md));
    BOOST_TEST(md < lg);

    BOOST_TEST(!(md <= sm));
    BOOST_TEST(md <= md);
    BOOST_TEST(md <= lg);

    BOOST_TEST(md > sm);
    BOOST_TEST(!(md > md));
    BOOST_TEST(!(md > lg));

    BOOST_TEST(md >= sm);
    BOOST_TEST(md >= md);
    BOOST_TEST(!(md >= lg));


    BOOST_TEST(!(lg == sm));
    BOOST_TEST(!(lg == md));
    BOOST_TEST(lg == lg);

    BOOST_TEST(!(lg < sm));
    BOOST_TEST(!(lg < md));
    BOOST_TEST(!(lg < lg));

    BOOST_TEST(!(lg <= sm));
    BOOST_TEST(!(lg <= md));
    BOOST_TEST(lg <= lg);

    BOOST_TEST(lg > sm);
    BOOST_TEST(lg > md);
    BOOST_TEST(!(lg > lg));

    BOOST_TEST(lg >= sm);
    BOOST_TEST(lg >= md);
    BOOST_TEST(lg >= lg);
}


void test_swap()
{
    {
        vec_type v1;
        v1.push_back(4);
        v1.push_back(4);
        v1.push_back(4);
        vec_type v2;
        v2.push_back(3);
        v2.push_back(3);
        v2.push_back(3);
        v2.push_back(3);

        static_assert(std::is_same<decltype(v1.swap(v2)), void>::value, "");
        static_assert(std::is_same<decltype(swap(v1, v2)), void>::value, "");

        v1.swap(v2);

        BOOST_TEST(v1.size() == 4u);
        BOOST_TEST(v2.size() == 3u);

        BOOST_TEST(v1[0] == 3);
        BOOST_TEST(v2[0] == 4);
    }

    {
        vec_type v1;
        v1.push_back(4);
        v1.push_back(4);
        v1.push_back(4);
        vec_type v2;
        v2.push_back(3);
        v2.push_back(3);
        v2.push_back(3);
        v2.push_back(3);

        swap(v1, v2);

        BOOST_TEST(v1.size() == 4u);
        BOOST_TEST(v2.size() == 3u);

        BOOST_TEST(v1[0] == 3);
        BOOST_TEST(v2[0] == 4);
    }
}

template<typename Iter>
using writable_iter_t = decltype(
    *std::declval<Iter>() =
        std::declval<typename std::iterator_traits<Iter>::value_type>());

static_assert(
    ill_formed<
        writable_iter_t,
        decltype(std::declval<vec_type const &>().begin())>::value,
    "");
static_assert(
    ill_formed<
        writable_iter_t,
        decltype(std::declval<vec_type const &>().end())>::value,
    "");
static_assert(
    ill_formed<writable_iter_t, decltype(std::declval<vec_type &>().cbegin())>::
        value,
    "");
static_assert(
    ill_formed<writable_iter_t, decltype(std::declval<vec_type &>().cend())>::
        value,
    "");

static_assert(
    ill_formed<
        writable_iter_t,
        decltype(std::declval<vec_type const &>().rbegin())>::value,
    "");
static_assert(
    ill_formed<
        writable_iter_t,
        decltype(std::declval<vec_type const &>().rend())>::value,
    "");
static_assert(
    ill_formed<
        writable_iter_t,
        decltype(std::declval<vec_type &>().crbegin())>::value,
    "");
static_assert(
    ill_formed<writable_iter_t, decltype(std::declval<vec_type &>().crend())>::
        value,
    "");

void test_iterators()
{
    {
        vec_type v;
        v.push_back(3);
        v.push_back(2);
        v.push_back(1);

        static_assert(
            std::is_same<decltype(v.begin()), vec_type::iterator>::value, "");
        static_assert(
            std::is_same<decltype(v.end()), vec_type::iterator>::value, "");
        static_assert(
            std::is_same<decltype(v.cbegin()), vec_type::const_iterator>::value,
            "");
        static_assert(
            std::is_same<decltype(v.cend()), vec_type::const_iterator>::value,
            "");
        static_assert(
            std::is_same<decltype(v.rbegin()), vec_type::reverse_iterator>::
                value,
            "");
        static_assert(
            std::is_same<decltype(v.rbegin()), vec_type::reverse_iterator>::
                value,
            "");
        static_assert(
            std::is_same<
                decltype(v.crbegin()),
                vec_type::const_reverse_iterator>::value,
            "");
        static_assert(
            std::is_same<
                decltype(v.crbegin()),
                vec_type::const_reverse_iterator>::value,
            "");

        std::array<int, 3> const a = {{3, 2, 1}};
	std::array<int, 3> const ra = {{1, 2, 3}};

        BOOST_TEST(std::equal(v.begin(), v.end(), a.begin(), a.end()));
        BOOST_TEST(std::equal(v.cbegin(), v.cend(), a.begin(), a.end()));

        BOOST_TEST(std::equal(v.rbegin(), v.rend(), ra.begin(), ra.end()));
        BOOST_TEST(std::equal(v.crbegin(), v.crend(), ra.begin(), ra.end()));

        *v.begin() = 8;
        *v.rbegin() = 9;
        BOOST_TEST(v[0] == 8);
        BOOST_TEST(v[2] == 9);
    }

    {
        vec_type v_;
        v_.push_back(3);
        v_.push_back(2);
        v_.push_back(1);
        vec_type const & v = v_;

        static_assert(
            std::is_same<decltype(v.begin()), vec_type::const_iterator>::value,
            "");
        static_assert(
            std::is_same<decltype(v.end()), vec_type::const_iterator>::value,
            "");
        static_assert(
            std::is_same<decltype(v.cbegin()), vec_type::const_iterator>::value,
            "");
        static_assert(
            std::is_same<decltype(v.cend()), vec_type::const_iterator>::value,
            "");
        static_assert(
            std::is_same<
                decltype(v.rbegin()),
                vec_type::const_reverse_iterator>::value,
            "");
        static_assert(
            std::is_same<
                decltype(v.rbegin()),
                vec_type::const_reverse_iterator>::value,
            "");
        static_assert(
            std::is_same<
                decltype(v.crbegin()),
                vec_type::const_reverse_iterator>::value,
            "");
        static_assert(
            std::is_same<
                decltype(v.crbegin()),
                vec_type::const_reverse_iterator>::value,
            "");

        std::array<int, 3> const a = {{3, 2, 1}};
        std::array<int, 3> const ra = {{1, 2, 3}};

        BOOST_TEST(std::equal(v.begin(), v.end(), a.begin(), a.end()));
        BOOST_TEST(std::equal(v.cbegin(), v.cend(), a.begin(), a.end()));

        BOOST_TEST(std::equal(v.rbegin(), v.rend(), ra.begin(), ra.end()));
        BOOST_TEST(std::equal(v.crbegin(), v.crend(), ra.begin(), ra.end()));
    }
}


void test_emplace_insert()
{
    {
        vec_type v;

        int const i = 0;
        static_assert(
            std::is_same<decltype(v.emplace_back(0)), vec_type::reference>::
                value,
            "");
        static_assert(
            std::is_same<decltype(v.emplace_back(i)), vec_type::reference>::
                value,
            "");

        v.emplace_back(i);
        BOOST_TEST(v.front() == i);
        BOOST_TEST(v.back() == i);

        v.emplace_back(1);
        BOOST_TEST(v.front() == i);
        BOOST_TEST(v.back() == 1);

        v.emplace_back(2);
        BOOST_TEST(v.front() == i);
        BOOST_TEST(v.back() == 2);

        BOOST_TEST(v[0] == 0);
        BOOST_TEST(v[1] == 1);
        BOOST_TEST(v[2] == 2);
    }

    {
        vec_type v;
        v.push_back(1);
        v.push_back(2);

        int const i = 0;
        static_assert(
            std::is_same<
                decltype(v.emplace(v.begin(), 0)),
                vec_type::iterator>::value,
            "");
        static_assert(
            std::is_same<
                decltype(v.emplace(v.begin(), i)),
                vec_type::iterator>::value,
            "");

        v.emplace(v.begin(), i);
        BOOST_TEST(v[0] == 0);
        BOOST_TEST(v[1] == 1);
        BOOST_TEST(v[2] == 2);

        v.emplace(v.end(), 3);
        BOOST_TEST(v[0] == 0);
        BOOST_TEST(v[1] == 1);
        BOOST_TEST(v[2] == 2);
        BOOST_TEST(v[3] == 3);

        v.emplace(v.begin() + 2, 9);
        BOOST_TEST(v[0] == 0);
        BOOST_TEST(v[1] == 1);
        BOOST_TEST(v[2] == 9);
        BOOST_TEST(v[3] == 2);
        BOOST_TEST(v[4] == 3);
    }

    {
        vec_type v;
        v.push_back(1);
        v.push_back(2);

        std::array<int, 2> a1 = {{0, 0}};
        std::array<int, 1> a2 = {{3}};
        std::array<int, 3> a3 = {{9, 9, 9}};

        static_assert(
            std::is_same<
                decltype(v.insert(v.begin(), a1.begin(), a1.end())),
                vec_type::iterator>::value,
            "");

        auto const it0 = v.insert(v.begin(), a1.begin(), a1.end());
        BOOST_TEST(v[0] == 0);
        BOOST_TEST(v[1] == 0);
        BOOST_TEST(v[2] == 1);
        BOOST_TEST(v[3] == 2);
        BOOST_TEST(it0 == v.begin());

        auto const it1 = v.insert(v.end(), a2.begin(), a2.end());
        BOOST_TEST(v[0] == 0);
        BOOST_TEST(v[1] == 0);
        BOOST_TEST(v[2] == 1);
        BOOST_TEST(v[3] == 2);
        BOOST_TEST(v[4] == 3);
        BOOST_TEST(it1 == v.begin() + 4);

        auto const it2 = v.insert(v.begin() + 2, a3.begin(), a3.end());
        BOOST_TEST(v[0] == 0);
        BOOST_TEST(v[1] == 0);
        BOOST_TEST(v[2] == 9);
        BOOST_TEST(v[3] == 9);
        BOOST_TEST(v[4] == 9);
        BOOST_TEST(v[5] == 1);
        BOOST_TEST(v[6] == 2);
        BOOST_TEST(v[7] == 3);
        BOOST_TEST(it2 == v.begin() + 2);
    }

    {
        vec_type v;
        v.push_back(1);
        v.push_back(2);

        int const i = 0;
        static_assert(
            std::is_same<decltype(v.insert(v.begin(), 0)), vec_type::iterator>::
                value,
            "");
        static_assert(
            std::is_same<decltype(v.insert(v.begin(), i)), vec_type::iterator>::
                value,
            "");

        v.emplace(v.begin(), i);
        BOOST_TEST(v[0] == 0);
        BOOST_TEST(v[1] == 1);
        BOOST_TEST(v[2] == 2);

        v.emplace(v.end(), 3);
        BOOST_TEST(v[0] == 0);
        BOOST_TEST(v[1] == 1);
        BOOST_TEST(v[2] == 2);
        BOOST_TEST(v[3] == 3);

        v.emplace(v.begin() + 2, 9);
        BOOST_TEST(v[0] == 0);
        BOOST_TEST(v[1] == 1);
        BOOST_TEST(v[2] == 9);
        BOOST_TEST(v[3] == 2);
        BOOST_TEST(v[4] == 3);
    }

#if 0 // Fill-insert is not supported for move-only value_type.
    {
        vec_type v;
        v.push_back(1);
        v.push_back(2);

        static_assert(
            std::is_same<
                decltype(v.insert(v.begin(), 3, 0)),
                vec_type::iterator>::value,
            "");

        v.insert(v.begin(), 2, 0);
        v.insert(v.end(), 1, 3);
        v.insert(v.begin() + 2, 3, 9);
    }
#endif

#if 0 // initializer_list-insert is not supported for move-only value_type.
    {
        vec_type v;
        v.push_back(1);
        v.push_back(2);

        static_assert(
            std::is_same<
                decltype(v.insert(v.begin(), std::initializer_list<noncopyable_int>{0, 0})),
                vec_type::iterator>::value,
            "");

        v.insert(v.begin(), std::initializer_list<noncopyable_int>{0, 0});
        v.insert(v.end(), std::initializer_list<noncopyable_int>{3});
        v.insert(v.begin() + 2, std::initializer_list<noncopyable_int>{9, 9, 9});
    }
#endif
}


void test_erase()
{
    {
        vec_type v;
        v.push_back(3);
        v.push_back(2);
        v.push_back(1);

        static_assert(
            std::is_same<
                decltype(v.erase(v.begin(), v.end())),
                vec_type::iterator>::value,
            "");
        static_assert(
            std::is_same<decltype(v.erase(v.begin())), vec_type::iterator>::
                value,
            "");

        v.erase(v.begin(), v.end());
        BOOST_TEST(v.empty());
        BOOST_TEST(v.size() == 0u);
    }

    {
        vec_type v;
        v.push_back(3);
        v.push_back(2);
        v.push_back(1);
        v.erase(v.begin() + 1, v.end());
        BOOST_TEST(!v.empty());
        BOOST_TEST(v.size() == 1u);
        BOOST_TEST(v[0] == 3);
    }

    {
        vec_type v;
        v.push_back(3);
        v.push_back(2);
        v.push_back(1);
        v.erase(v.begin(), v.end() - 1);
        BOOST_TEST(!v.empty());
        BOOST_TEST(v.size() == 1u);
        BOOST_TEST(v[0] == 1);
    }

    {
        vec_type v;
        v.push_back(3);
        v.push_back(2);
        v.push_back(1);
        v.erase(v.begin());
        BOOST_TEST(!v.empty());
        BOOST_TEST(v.size() == 2u);
        BOOST_TEST(v[0] == 2);
        BOOST_TEST(v[1] == 1);
    }

    {
        vec_type v;
        v.push_back(3);
        v.push_back(2);
        v.push_back(1);
        v.erase(v.begin() + 1);
        BOOST_TEST(!v.empty());
        BOOST_TEST(v.size() == 2u);
        BOOST_TEST(v[0] == 3);
        BOOST_TEST(v[1] == 1);
    }

    {
        vec_type v;
        v.push_back(3);
        v.push_back(2);
        v.push_back(1);
        v.erase(v.begin() + 2);
        BOOST_TEST(!v.empty());
        BOOST_TEST(v.size() == 2u);
        BOOST_TEST(v[0] == 3);
        BOOST_TEST(v[1] == 2);
    }
}

template<
    typename Container,
    typename ValueType = typename Container::value_type>
using lvalue_push_front_t = decltype(
    std::declval<Container &>().push_front(std::declval<ValueType const &>()));
template<
    typename Container,
    typename ValueType = typename Container::value_type>
using rvalue_push_front_t = decltype(std::declval<Container &>().push_front(0));
template<typename Container>
using pop_front_t = decltype(std::declval<Container &>().pop_front());

static_assert(ill_formed<lvalue_push_front_t, vec_type>::value, "");
static_assert(ill_formed<rvalue_push_front_t, vec_type>::value, "");
static_assert(ill_formed<pop_front_t, vec_type>::value, "");

void test_front_back()
{
    {
        vec_type v;

        int const i = 0;
        static_assert(std::is_same<decltype(v.push_back(0)), void>::value, "");
        static_assert(std::is_same<decltype(v.push_back(i)), void>::value, "");
        static_assert(std::is_same<decltype(v.pop_back()), void>::value, "");

        v.emplace_back(i);
        BOOST_TEST(v.front() == i);
        BOOST_TEST(v.back() == i);

        v.emplace_back(1);
        BOOST_TEST(v.front() == i);
        BOOST_TEST(v.back() == 1);

        v.emplace_back(2);
        BOOST_TEST(v.front() == i);
        BOOST_TEST(v.back() == 2);

        static_assert(std::is_same<decltype(v.front()), noncopyable_int &>::value, "");
        static_assert(std::is_same<decltype(v.back()), noncopyable_int &>::value, "");

        v.front() = 9;
        v.back() = 8;
        BOOST_TEST(v[0] == 9);
        BOOST_TEST(v[1] == 1);
        BOOST_TEST(v[2] == 8);

        v.pop_back();
        BOOST_TEST(v[0] == 9);
        BOOST_TEST(v[1] == 1);
    }

    {
        vec_type v_;
        v_.push_back(3);
        v_.push_back(2);
        v_.push_back(1);
        vec_type const & v = v_;
        BOOST_TEST(v.front() == 3);
        BOOST_TEST(v.back() == 1);

        static_assert(
            std::is_same<decltype(v.front()), noncopyable_int const &>::value, "");
        static_assert(std::is_same<decltype(v.back()), noncopyable_int const &>::value, "");
    }
}


void test_data_index_at()
{
    {
        vec_type v;
        v.push_back(3);
        v.push_back(2);
        v.push_back(1);
        BOOST_TEST(v.data()[0] == 3);
        BOOST_TEST(v.data()[1] == 2);
        BOOST_TEST(v.data()[2] == 1);
        BOOST_TEST(v[0] == 3);
        BOOST_TEST(v[1] == 2);
        BOOST_TEST(v[2] == 1);
        BOOST_TEST_NO_THROW(v.at(0));
        BOOST_TEST_NO_THROW(v.at(1));
        BOOST_TEST_NO_THROW(v.at(2));
        BOOST_TEST_THROWS(v.at(3), std::out_of_range);

        static_assert(std::is_same<decltype(v.data()), noncopyable_int *>::value, "");
        static_assert(std::is_same<decltype(v[0]), noncopyable_int &>::value, "");
        static_assert(std::is_same<decltype(v.at(0)), noncopyable_int &>::value, "");

        v[0] = 8;
        v.at(1) = 9;
        BOOST_TEST(v[0] == 8);
        BOOST_TEST(v[1] == 9);
        BOOST_TEST(v[2] == 1);
    }

    {
        vec_type v_;
        v_.push_back(3);
        v_.push_back(2);
        v_.push_back(1);
        vec_type const & v = v_;
        BOOST_TEST(v.data()[0] == 3);
        BOOST_TEST(v.data()[1] == 2);
        BOOST_TEST(v.data()[2] == 1);
        BOOST_TEST(v[0] == 3);
        BOOST_TEST(v[1] == 2);
        BOOST_TEST(v[2] == 1);
        BOOST_TEST_NO_THROW(v.at(0));
        BOOST_TEST_NO_THROW(v.at(1));
        BOOST_TEST_NO_THROW(v.at(2));
        BOOST_TEST_THROWS(v.at(3), std::out_of_range);

        static_assert(std::is_same<decltype(v.data()), noncopyable_int const *>::value, "");
        static_assert(std::is_same<decltype(v[0]), noncopyable_int const &>::value, "");
        static_assert(std::is_same<decltype(v.at(0)), noncopyable_int const &>::value, "");
    }
}

int main()
{
    test_default_ctor();
    test_other_ctors_assign_ctor();
    test_resize();
    test_assignment_copy_move_equality();
    test_comparisons();
    test_swap();
    test_iterators();
    test_emplace_insert();
    test_erase();
    test_front_back();
    test_data_index_at();
    return boost::report_errors();
}
