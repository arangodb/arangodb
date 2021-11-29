// Copyright (C) 2019 T. Zachary Laine
//
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
#include <boost/stl_interfaces/iterator_interface.hpp>

#include "ill_formed.hpp"

#include <boost/core/lightweight_test.hpp>

#include <array>
#include <numeric>
#include <type_traits>


struct basic_input_iter
    : boost::stl_interfaces::
          iterator_interface<basic_input_iter, std::input_iterator_tag, int>
{
    basic_input_iter() : it_(nullptr) {}
    basic_input_iter(int * it) : it_(it) {}

    int & operator*() const noexcept { return *it_; }
    basic_input_iter & operator++() noexcept
    {
        ++it_;
        return *this;
    }
    friend bool operator==(basic_input_iter lhs, basic_input_iter rhs) noexcept
    {
        return lhs.it_ == rhs.it_;
    }

    using base_type = boost::stl_interfaces::
        iterator_interface<basic_input_iter, std::input_iterator_tag, int>;
    using base_type::operator++;

private:
    int * it_;
};

BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(
    basic_input_iter, std::input_iterator)
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    basic_input_iter,
    std::input_iterator_tag,
    std::input_iterator_tag,
    int,
    int &,
    int *,
    std::ptrdiff_t)

template<typename ValueType>
struct input_iter : boost::stl_interfaces::iterator_interface<
                        input_iter<ValueType>,
                        std::input_iterator_tag,
                        ValueType>
{
    input_iter() : it_(nullptr) {}
    input_iter(ValueType * it) : it_(it) {}
    template<
        typename ValueType2,
        typename E = std::enable_if_t<
            std::is_convertible<ValueType2 *, ValueType *>::value>>
    input_iter(input_iter<ValueType2> it) : it_(it.it_)
    {}

    ValueType & operator*() const noexcept { return *it_; }
    input_iter & operator++() noexcept
    {
        ++it_;
        return *this;
    }
    friend bool operator==(input_iter lhs, input_iter rhs) noexcept
    {
        return lhs.it_ == rhs.it_;
    }

    using base_type = boost::stl_interfaces::iterator_interface<
        input_iter<ValueType>,
        std::input_iterator_tag,
        ValueType>;
    using base_type::operator++;

private:
    ValueType * it_;

    template<typename ValueType2>
    friend struct input_iter;
};

BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(input_iter<int>, std::input_iterator)
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    input_iter<int>,
    std::input_iterator_tag,
    std::input_iterator_tag,
    int,
    int &,
    int *,
    std::ptrdiff_t)

using int_input = input_iter<int>;
using const_int_input = input_iter<int const>;
using pair_input = input_iter<std::pair<int, int>>;
using const_pair_input = input_iter<std::pair<int, int> const>;

template<typename ValueType>
struct proxy_input_iter : boost::stl_interfaces::proxy_iterator_interface<
                              proxy_input_iter<ValueType>,
                              std::input_iterator_tag,
                              ValueType>
{
    proxy_input_iter() : it_(nullptr) {}
    proxy_input_iter(ValueType * it) : it_(it) {}
    template<
        typename ValueType2,
        typename E = std::enable_if_t<
            std::is_convertible<ValueType2 *, ValueType *>::value>>
    proxy_input_iter(proxy_input_iter<ValueType2> it) : it_(it.it_)
    {}

    ValueType operator*() const noexcept { return *it_; }
    proxy_input_iter & operator++() noexcept
    {
        ++it_;
        return *this;
    }
    friend bool operator==(proxy_input_iter lhs, proxy_input_iter rhs) noexcept
    {
        return lhs.it_ == rhs.it_;
    }

    using base_type = boost::stl_interfaces::proxy_iterator_interface<
        proxy_input_iter<ValueType>,
        std::input_iterator_tag,
        ValueType>;
    using base_type::operator++;

private:
    ValueType * it_;

    template<typename ValueType2>
    friend struct proxy_input_iter;
};

using int_pair = std::pair<int, int>;
BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(
    proxy_input_iter<int_pair>, std::input_iterator)
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    proxy_input_iter<int_pair>,
    std::input_iterator_tag,
    std::input_iterator_tag,
    int_pair,
    int_pair,
    boost::stl_interfaces::proxy_arrow_result<int_pair>,
    std::ptrdiff_t)

std::array<int, 10> ints = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
std::array<std::pair<int, int>, 10> pairs = {{
    {0, 1},
    {1, 1},
    {2, 1},
    {3, 1},
    {4, 1},
    {5, 1},
    {6, 1},
    {7, 1},
    {8, 1},
    {9, 1},
}};


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
            basic_input_iter,
            basic_input_iter,
            boost::stl_interfaces::element_layout::discontiguous>>::value,
    "");
static_assert(
    ill_formed<
        data_t,
        subrange<
            basic_input_iter,
            basic_input_iter,
            boost::stl_interfaces::element_layout::discontiguous> const>::
        value,
    "");

template<typename T>
using size_t_ = decltype(std::declval<T>().size());

static_assert(
    ill_formed<
        size_t_,
        subrange<
            basic_input_iter,
            basic_input_iter,
            boost::stl_interfaces::element_layout::discontiguous>>::value,
    "");
static_assert(
    ill_formed<
        size_t_,
        subrange<
            basic_input_iter,
            basic_input_iter,
            boost::stl_interfaces::element_layout::discontiguous> const>::
        value,
    "");

template<typename T>
using back_t_ = decltype(std::declval<T>().back());

static_assert(
    ill_formed<
        back_t_,
        subrange<
            basic_input_iter,
            basic_input_iter,
            boost::stl_interfaces::element_layout::discontiguous>>::value,
    "");
static_assert(
    ill_formed<
        back_t_,
        subrange<
            basic_input_iter,
            basic_input_iter,
            boost::stl_interfaces::element_layout::discontiguous> const>::
        value,
    "");

template<typename T>
using index_operator_t = decltype(std::declval<T>()[0]);

static_assert(
    ill_formed<
        index_operator_t,
        subrange<
            basic_input_iter,
            basic_input_iter,
            boost::stl_interfaces::element_layout::discontiguous>>::value,
    "");
static_assert(
    ill_formed<
        index_operator_t,
        subrange<
            basic_input_iter,
            basic_input_iter,
            boost::stl_interfaces::element_layout::discontiguous> const>::
        value,
    "");


int main()
{

{
    basic_input_iter first(ints.data());
    basic_input_iter last(ints.data() + ints.size());

    {
        std::array<int, 10> ints_copy;
        std::copy(first, last, ints_copy.begin());
        BOOST_TEST(ints_copy == ints);
    }
}


{
    int_input first(ints.data());
    int_input last(ints.data() + ints.size());
    const_int_input first_copy(first);
    const_int_input last_copy(last);
    std::equal(first, last, first_copy, last_copy);
}


{
    int_input first(ints.data());
    int_input last(ints.data() + ints.size());
    while (first != last)
        first++;
}


{
    {
        std::array<int, 10> ints_copy;
        int_input first(ints.data());
        int_input last(ints.data() + ints.size());
        std::copy(first, last, ints_copy.begin());
        BOOST_TEST(ints_copy == ints);
    }

    {
        std::array<std::pair<int, int>, 10> pairs_copy;
        pair_input first(pairs.data());
        pair_input last(pairs.data() + pairs.size());
        std::copy(first, last, pairs_copy.begin());
        BOOST_TEST(pairs_copy == pairs);
    }

    {
        std::array<int, 10> firsts_copy;
        pair_input first(pairs.data());
        pair_input last(pairs.data() + pairs.size());
        for (auto out = firsts_copy.begin(); first != last; ++first) {
            *out++ = first->first;
        }
        BOOST_TEST(firsts_copy == ints);
    }

    {
        std::array<int, 10> firsts_copy;
        proxy_input_iter<std::pair<int, int>> first(pairs.data());
        proxy_input_iter<std::pair<int, int>> last(pairs.data() + pairs.size());
        for (auto out = firsts_copy.begin(); first != last; ++first) {
            *out++ = first->first;
        }
        BOOST_TEST(firsts_copy == ints);
    }
}


{
    {
        std::array<int, 10> ints_copy;
        const_int_input first(ints.data());
        const_int_input last(ints.data() + ints.size());
        std::copy(first, last, ints_copy.begin());
        BOOST_TEST(ints_copy == ints);
    }

    {
        std::array<std::pair<int, int>, 10> pairs_copy;
        const_pair_input first(pairs.data());
        const_pair_input last(pairs.data() + pairs.size());
        std::copy(first, last, pairs_copy.begin());
        BOOST_TEST(pairs_copy == pairs);
    }

    {
        std::array<int, 10> firsts_copy;
        const_pair_input first(pairs.data());
        const_pair_input last(pairs.data() + pairs.size());
        for (auto out = firsts_copy.begin(); first != last; ++first) {
            *out++ = first->first;
        }
        BOOST_TEST(firsts_copy == ints);
    }

    {
        std::array<int, 10> firsts_copy;
        proxy_input_iter<std::pair<int, int> const> first(pairs.data());
        proxy_input_iter<std::pair<int, int> const> last(
            pairs.data() + pairs.size());
        for (auto out = firsts_copy.begin(); first != last; ++first) {
            *out++ = first->first;
        }
        BOOST_TEST(firsts_copy == ints);
    }
}

{
    basic_input_iter first(ints.data());
    basic_input_iter last(ints.data() + ints.size());

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
}

    return boost::report_errors();
}
