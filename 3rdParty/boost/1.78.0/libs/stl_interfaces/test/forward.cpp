// Copyright (C) 2019 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/stl_interfaces/iterator_interface.hpp>

#include "ill_formed.hpp"

#include <boost/core/lightweight_test.hpp>

#include <algorithm>
#include <array>
#include <numeric>
#include <type_traits>


template<typename T>
using decrementable_t = decltype(--std::declval<T &>());

struct basic_forward_iter
    : boost::stl_interfaces::
          iterator_interface<basic_forward_iter, std::forward_iterator_tag, int>
{
    basic_forward_iter() : it_(nullptr) {}
    basic_forward_iter(int * it) : it_(it) {}

    int & operator*() const { return *it_; }
    basic_forward_iter & operator++()
    {
        ++it_;
        return *this;
    }
    friend bool
    operator==(basic_forward_iter lhs, basic_forward_iter rhs) noexcept
    {
        return lhs.it_ == rhs.it_;
    }

    using base_type = boost::stl_interfaces::
        iterator_interface<basic_forward_iter, std::forward_iterator_tag, int>;
    using base_type::operator++;

private:
    int * it_;
};

BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(
    basic_forward_iter, std::forward_iterator)
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    basic_forward_iter,
    std::forward_iterator_tag,
    std::forward_iterator_tag,
    int,
    int &,
    int *,
    std::ptrdiff_t)

static_assert(ill_formed<decrementable_t, basic_forward_iter>::value, "");

template<typename ValueType>
struct forward_iter : boost::stl_interfaces::iterator_interface<
                          forward_iter<ValueType>,
                          std::forward_iterator_tag,
                          ValueType>
{
    forward_iter() : it_(nullptr) {}
    forward_iter(ValueType * it) : it_(it) {}
    template<
        typename ValueType2,
        typename E = std::enable_if_t<
            std::is_convertible<ValueType2 *, ValueType *>::value>>
    forward_iter(forward_iter<ValueType2> it) : it_(it.it_)
    {}

    ValueType & operator*() const { return *it_; }
    forward_iter & operator++()
    {
        ++it_;
        return *this;
    }
    friend bool operator==(forward_iter lhs, forward_iter rhs) noexcept
    {
        return lhs.it_ == rhs.it_;
    }

    using base_type = boost::stl_interfaces::iterator_interface<
        forward_iter<ValueType>,
        std::forward_iterator_tag,
        ValueType>;
    using base_type::operator++;

private:
    ValueType * it_;

    template<typename ValueType2>
    friend struct forward_iter;
};

using forward = forward_iter<int>;
using const_forward = forward_iter<int const>;

static_assert(ill_formed<decrementable_t, forward>::value, "");
static_assert(ill_formed<decrementable_t, const_forward>::value, "");

BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(forward, std::forward_iterator)
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    forward,
    std::forward_iterator_tag,
    std::forward_iterator_tag,
    int,
    int &,
    int *,
    std::ptrdiff_t)

BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(const_forward, std::forward_iterator)
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    const_forward,
    std::forward_iterator_tag,
    std::forward_iterator_tag,
    int,
    int const &,
    int const *,
    std::ptrdiff_t)


std::array<int, 10> ints = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};



////////////////////
// view_interface //
////////////////////
#include "view_tests.hpp"

template<typename T>
using data_t = decltype(std::declval<T>().data());

static_assert(
    ill_formed<
        data_t,
        subrange<
            basic_forward_iter,
            basic_forward_iter,
            boost::stl_interfaces::element_layout::discontiguous>>::value,
    "");
static_assert(
    ill_formed<
        data_t,
        subrange<
            basic_forward_iter,
            basic_forward_iter,
            boost::stl_interfaces::element_layout::discontiguous> const>::
        value,
    "");

template<typename T>
using size_t_ = decltype(std::declval<T>().size());

static_assert(
    ill_formed<
        size_t_,
        subrange<
            basic_forward_iter,
            basic_forward_iter,
            boost::stl_interfaces::element_layout::discontiguous>>::value,
    "");
static_assert(
    ill_formed<
        size_t_,
        subrange<
            basic_forward_iter,
            basic_forward_iter,
            boost::stl_interfaces::element_layout::discontiguous> const>::
        value,
    "");

template<typename T>
using back_t_ = decltype(std::declval<T>().back());

static_assert(
    ill_formed<
        back_t_,
        subrange<
            basic_forward_iter,
            basic_forward_iter,
            boost::stl_interfaces::element_layout::discontiguous>>::value,
    "");
static_assert(
    ill_formed<
        back_t_,
        subrange<
            basic_forward_iter,
            basic_forward_iter,
            boost::stl_interfaces::element_layout::discontiguous> const>::
        value,
    "");

template<typename T>
using index_operator_t = decltype(std::declval<T>()[0]);

static_assert(
    ill_formed<
        index_operator_t,
        subrange<
            basic_forward_iter,
            basic_forward_iter,
            boost::stl_interfaces::element_layout::discontiguous>>::value,
    "");
static_assert(
    ill_formed<
        index_operator_t,
        subrange<
            basic_forward_iter,
            basic_forward_iter,
            boost::stl_interfaces::element_layout::discontiguous> const>::
        value,
    "");


int main()
{

{
    basic_forward_iter first(ints.data());
    basic_forward_iter last(ints.data() + ints.size());

    {
        std::array<int, 10> ints_copy;
        std::copy(first, last, ints_copy.begin());
        BOOST_TEST(ints_copy == ints);
    }

    {
        std::array<int, 10> iota_ints;
        basic_forward_iter first(iota_ints.data());
        basic_forward_iter last(iota_ints.data() + iota_ints.size());
        std::iota(first, last, 0);
        BOOST_TEST(iota_ints == ints);
    }
}


{
    forward first(ints.data());
    forward last(ints.data() + ints.size());
    const_forward first_copy(first);
    const_forward last_copy(last);
    std::equal(first, last, first_copy, last_copy);
}


{
    forward first(ints.data());
    forward last(ints.data() + ints.size());
    while (first != last)
        first++;
}


{
    forward first(ints.data());
    forward last(ints.data() + ints.size());

    {
        std::array<int, 10> ints_copy;
        std::copy(first, last, ints_copy.begin());
        BOOST_TEST(ints_copy == ints);
    }

    {
        std::array<int, 10> iota_ints;
        forward first(iota_ints.data());
        forward last(iota_ints.data() + iota_ints.size());
        std::iota(first, last, 0);
        BOOST_TEST(iota_ints == ints);
    }
}


{
    const_forward first(ints.data());
    const_forward last(ints.data() + ints.size());

    {
        std::array<int, 10> ints_copy;
        std::copy(first, last, ints_copy.begin());
        BOOST_TEST(ints_copy == ints);
    }

    {
        BOOST_TEST(std::binary_search(first, last, 3));
    }
}

{
    basic_forward_iter first(ints.data());
    basic_forward_iter last(ints.data() + ints.size());

    auto r = range<boost::stl_interfaces::element_layout::discontiguous>(
        first, last);
    auto empty =
        range<boost::stl_interfaces::element_layout::discontiguous>(
            first, first);

    // range begin/end
    {
        std::array<int, 10> ints_copy;
        std::copy(r.begin(), r.end(), ints_copy.begin());
        BOOST_TEST(ints_copy == ints);

        BOOST_TEST(empty.begin() == empty.end());
    }

    // empty/op bool
    {
        BOOST_TEST(!r.empty());
        BOOST_TEST(r);

        BOOST_TEST(empty.empty());
        BOOST_TEST(!empty);

        auto const cr = r;
        BOOST_TEST(!cr.empty());
        BOOST_TEST(cr);

        auto const cempty = empty;
        BOOST_TEST(cempty.empty());
        BOOST_TEST(!cempty);
    }

    // front/back
    {
        BOOST_TEST(r.front() == 0);

        auto const cr = r;
        BOOST_TEST(cr.front() == 0);
    }
}

    return boost::report_errors();
}
