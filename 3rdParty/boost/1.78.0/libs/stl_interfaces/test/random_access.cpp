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
#include <functional>
#include <numeric>
#include <tuple>
#include <type_traits>


struct basic_random_access_iter : boost::stl_interfaces::iterator_interface<
                                      basic_random_access_iter,
                                      std::random_access_iterator_tag,
                                      int>
{
    basic_random_access_iter() {}
    basic_random_access_iter(int * it) : it_(it) {}

    int & operator*() const { return *it_; }
    basic_random_access_iter & operator+=(std::ptrdiff_t i)
    {
        it_ += i;
        return *this;
    }
    friend std::ptrdiff_t operator-(
        basic_random_access_iter lhs, basic_random_access_iter rhs) noexcept
    {
        return lhs.it_ - rhs.it_;
    }

private:
    int * it_;
};

BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(
    basic_random_access_iter, std::random_access_iterator)
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    basic_random_access_iter,
    std::random_access_iterator_tag,
    std::random_access_iterator_tag,
    int,
    int &,
    int *,
    std::ptrdiff_t)

static_assert(
    boost::stl_interfaces::v1::v1_dtl::
        plus_eq<basic_random_access_iter, std::ptrdiff_t>::value,
    "");

struct basic_adapted_random_access_iter
    : boost::stl_interfaces::iterator_interface<
          basic_adapted_random_access_iter,
          std::random_access_iterator_tag,
          int>
{
    basic_adapted_random_access_iter() {}
    basic_adapted_random_access_iter(int * it) : it_(it) {}

private:
    friend boost::stl_interfaces::access;
    int *& base_reference() noexcept { return it_; }
    int * base_reference() const noexcept { return it_; }

    int * it_;
};

BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(
    basic_adapted_random_access_iter, std::random_access_iterator)
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    basic_adapted_random_access_iter,
    std::random_access_iterator_tag,
    std::random_access_iterator_tag,
    int,
    int &,
    int *,
    std::ptrdiff_t)

template<typename ValueType>
struct adapted_random_access_iter : boost::stl_interfaces::iterator_interface<
                                        adapted_random_access_iter<ValueType>,
                                        std::random_access_iterator_tag,
                                        ValueType>
{
    adapted_random_access_iter() {}
    adapted_random_access_iter(ValueType * it) : it_(it) {}

    template<
        typename ValueType2,
        typename Enable = std::enable_if_t<
            std::is_convertible<ValueType2 *, ValueType *>::value>>
    adapted_random_access_iter(adapted_random_access_iter<ValueType2> other) :
        it_(other.it_)
    {}

    template<typename ValueType2>
    friend struct adapted_random_access_iter;

private:
    friend boost::stl_interfaces::access;
    ValueType *& base_reference() noexcept { return it_; }
    ValueType * base_reference() const noexcept { return it_; }

    ValueType * it_;
};

BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(
    adapted_random_access_iter<int>, std::random_access_iterator)
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    adapted_random_access_iter<int>,
    std::random_access_iterator_tag,
    std::random_access_iterator_tag,
    int,
    int &,
    int *,
    std::ptrdiff_t)
BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(
    adapted_random_access_iter<int const>, std::random_access_iterator)
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    adapted_random_access_iter<int const>,
    std::random_access_iterator_tag,
    std::random_access_iterator_tag,
    int,
    int const &,
    int const *,
    std::ptrdiff_t)

template<typename ValueType>
struct random_access_iter : boost::stl_interfaces::iterator_interface<
                                random_access_iter<ValueType>,
                                std::random_access_iterator_tag,
                                ValueType>
{
    random_access_iter() {}
    random_access_iter(ValueType * it) : it_(it) {}
    template<
        typename ValueType2,
        typename E = std::enable_if_t<
            std::is_convertible<ValueType2 *, ValueType *>::value>>
    random_access_iter(random_access_iter<ValueType2> it) : it_(it.it_)
    {}

    ValueType & operator*() const { return *it_; }
    random_access_iter & operator+=(std::ptrdiff_t i)
    {
        it_ += i;
        return *this;
    }
    friend std::ptrdiff_t
    operator-(random_access_iter lhs, random_access_iter rhs) noexcept
    {
        return lhs.it_ - rhs.it_;
    }

private:
    ValueType * it_;

    template<typename ValueType2>
    friend struct random_access_iter;
};

using random_access = random_access_iter<int>;
using const_random_access = random_access_iter<int const>;

BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(
    random_access, std::random_access_iterator)
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    random_access,
    std::random_access_iterator_tag,
    std::random_access_iterator_tag,
    int,
    int &,
    int *,
    std::ptrdiff_t)

BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(
    const_random_access, std::random_access_iterator)
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    const_random_access,
    std::random_access_iterator_tag,
    std::random_access_iterator_tag,
    int,
    int const &,
    int const *,
    std::ptrdiff_t)

struct zip_iter : boost::stl_interfaces::proxy_iterator_interface<
                      zip_iter,
                      std::random_access_iterator_tag,
                      std::tuple<int, int>,
                      std::tuple<int &, int &>>
{
    zip_iter() : it1_(nullptr), it2_(nullptr) {}
    zip_iter(int * it1, int * it2) : it1_(it1), it2_(it2) {}

    std::tuple<int &, int &> operator*() const
    {
        return std::tuple<int &, int &>{*it1_, *it2_};
    }
    zip_iter & operator+=(std::ptrdiff_t i)
    {
        it1_ += i;
        it2_ += i;
        return *this;
    }
    friend std::ptrdiff_t operator-(zip_iter lhs, zip_iter rhs) noexcept
    {
        return lhs.it1_ - rhs.it1_;
    }

private:
    int * it1_;
    int * it2_;
};

using int_pair = std::tuple<int, int>;
using int_refs_pair = std::tuple<int &, int &>;

BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(
    zip_iter, std::random_access_iterator)
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    zip_iter,
    std::random_access_iterator_tag,
    std::random_access_iterator_tag,
    int_pair,
    int_refs_pair,
    boost::stl_interfaces::proxy_arrow_result<int_refs_pair>,
    std::ptrdiff_t)

struct int_t
{
    int value_;

    bool operator==(int_t other) const { return value_ == other.value_; }
    bool operator!=(int_t other) const { return value_ != other.value_; }
    bool operator<(int_t other) const { return value_ < other.value_; }

    bool operator==(int other) const { return value_ == other; }
    bool operator!=(int other) const { return value_ != other; }
    bool operator<(int other) const { return value_ < other; }

    friend bool operator==(int lhs, int_t rhs) { return lhs == rhs.value_; }
    friend bool operator!=(int lhs, int_t rhs) { return lhs != rhs.value_; }
    friend bool operator<(int lhs, int_t rhs) { return lhs < rhs.value_; }
};

struct udt_zip_iter : boost::stl_interfaces::proxy_iterator_interface<
                          udt_zip_iter,
                          std::random_access_iterator_tag,
                          std::tuple<int_t, int>,
                          std::tuple<int_t &, int &>>
{
    udt_zip_iter() : it1_(nullptr), it2_(nullptr) {}
    udt_zip_iter(int_t * it1, int * it2) : it1_(it1), it2_(it2) {}

    std::tuple<int_t &, int &> operator*() const
    {
        return std::tuple<int_t &, int &>{*it1_, *it2_};
    }
    udt_zip_iter & operator+=(std::ptrdiff_t i)
    {
        it1_ += i;
        it2_ += i;
        return *this;
    }
    friend std::ptrdiff_t operator-(udt_zip_iter lhs, udt_zip_iter rhs) noexcept
    {
        return lhs.it1_ - rhs.it1_;
    }

private:
    int_t * it1_;
    int * it2_;
};

using int_t_int_pair = std::tuple<int_t, int>;
using int_t_int_refs_pair = std::tuple<int_t &, int &>;

BOOST_STL_INTERFACES_STATIC_ASSERT_CONCEPT(
    udt_zip_iter, std::random_access_iterator)
BOOST_STL_INTERFACES_STATIC_ASSERT_ITERATOR_TRAITS(
    udt_zip_iter,
    std::random_access_iterator_tag,
    std::random_access_iterator_tag,
    int_t_int_pair,
    int_t_int_refs_pair,
    boost::stl_interfaces::proxy_arrow_result<int_t_int_refs_pair>,
    std::ptrdiff_t)

namespace std {
    // Required for std::sort to work with zip_iter.  If zip_iter::reference
    // were not a std::tuple with builtin types as its template parameters, we
    // could have put this in another namespace.
    void swap(zip_iter::reference && lhs, zip_iter::reference && rhs)
    {
        using std::swap;
        swap(std::get<0>(lhs), std::get<0>(rhs));
        swap(std::get<1>(lhs), std::get<1>(rhs));
    }
}

void swap(udt_zip_iter::reference && lhs, udt_zip_iter::reference && rhs)
{
    using std::swap;
    swap(std::get<0>(lhs), std::get<0>(rhs));
    swap(std::get<1>(lhs), std::get<1>(rhs));
}

std::array<int, 10> ints = {{0, 1, 2, 3, 4, 5, 6, 7, 8, 9}};
std::array<int, 10> ones = {{1, 1, 1, 1, 1, 1, 1, 1, 1, 1}};
std::array<std::tuple<int, int>, 10> tuples = {{
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

std::array<int_t, 10> udts = {
    {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}}};
std::array<std::tuple<int_t, int>, 10> udt_tuples = {{
    std::tuple<int_t, int>{{0}, 1},
    std::tuple<int_t, int>{{1}, 1},
    std::tuple<int_t, int>{{2}, 1},
    std::tuple<int_t, int>{{3}, 1},
    std::tuple<int_t, int>{{4}, 1},
    std::tuple<int_t, int>{{5}, 1},
    std::tuple<int_t, int>{{6}, 1},
    std::tuple<int_t, int>{{7}, 1},
    std::tuple<int_t, int>{{8}, 1},
    std::tuple<int_t, int>{{9}, 1},
}};


////////////////////
// view_interface //
////////////////////
#include "view_tests.hpp"

#if !defined(__cpp_lib_concepts)

template<typename T>
using data_t = decltype(std::declval<T>().data());

static_assert(
    ill_formed<
        data_t,
        subrange<
            basic_random_access_iter,
            basic_random_access_iter,
            boost::stl_interfaces::element_layout::discontiguous>>::value,
    "");
static_assert(
    ill_formed<
        data_t,
        subrange<
            basic_random_access_iter,
            basic_random_access_iter,
            boost::stl_interfaces::element_layout::discontiguous> const>::
        value,
    "");

template<typename T>
using back_t = decltype(std::declval<T>().back());

static_assert(
    ill_formed<
        back_t,
        subrange<
            int *,
            int const *,
            boost::stl_interfaces::element_layout::discontiguous>>::value,
    "");
static_assert(
    ill_formed<
        back_t,
        subrange<
            int *,
            int const *,
            boost::stl_interfaces::element_layout::discontiguous> const>::
        value,
    "");

#endif


int main()
{

{
    basic_random_access_iter first(ints.data());
    basic_random_access_iter last(ints.data() + ints.size());

    BOOST_TEST(*first == 0);
    BOOST_TEST(*(first + 1) == 1);
    BOOST_TEST(*(first + 2) == 2);
    BOOST_TEST(*(1 + first) == 1);
    BOOST_TEST(*(2 + first) == 2);

    BOOST_TEST(first[0] == 0);
    BOOST_TEST(first[1] == 1);
    BOOST_TEST(first[2] == 2);

    BOOST_TEST(*(last - 1) == 9);
    BOOST_TEST(*(last - 2) == 8);
    BOOST_TEST(*(last - 3) == 7);

    BOOST_TEST(last[-1] == 9);
    BOOST_TEST(last[-2] == 8);
    BOOST_TEST(last[-3] == 7);

    BOOST_TEST(last - first == 10);
    BOOST_TEST(first == first);
    BOOST_TEST(first != last);
    BOOST_TEST(first < last);
    BOOST_TEST(first <= last);
    BOOST_TEST(first <= first);
    BOOST_TEST(last > first);
    BOOST_TEST(last >= first);
    BOOST_TEST(last >= last);

    {
        auto first_copy = first;
        first_copy += 10;
        BOOST_TEST(first_copy == last);
    }

    {
        auto last_copy = last;
        last_copy -= 10;
        BOOST_TEST(last_copy == first);
    }
}


{
    {
        std::array<int, 10> ints_copy;
        basic_random_access_iter first(ints.data());
        basic_random_access_iter last(ints.data() + ints.size());
        std::copy(first, last, ints_copy.begin());
        BOOST_TEST(ints_copy == ints);
    }

    {
        std::array<int, 10> ints_copy;
        basic_random_access_iter first(ints.data());
        basic_random_access_iter last(ints.data() + ints.size());
        std::copy(
            std::make_reverse_iterator(last),
            std::make_reverse_iterator(first),
            ints_copy.begin());
        std::reverse(ints_copy.begin(), ints_copy.end());
        BOOST_TEST(ints_copy == ints);
    }

    {
        std::array<int, 10> iota_ints;
        basic_random_access_iter first(iota_ints.data());
        basic_random_access_iter last(iota_ints.data() + iota_ints.size());
        std::iota(first, last, 0);
        BOOST_TEST(iota_ints == ints);
    }

    {
        std::array<int, 10> iota_ints;
        basic_random_access_iter first(iota_ints.data());
        basic_random_access_iter last(iota_ints.data() + iota_ints.size());
        std::iota(
            std::make_reverse_iterator(last),
            std::make_reverse_iterator(first),
            0);
        std::reverse(iota_ints.begin(), iota_ints.end());
        BOOST_TEST(iota_ints == ints);
    }

    {
        std::array<int, 10> iota_ints;
        basic_random_access_iter first(iota_ints.data());
        basic_random_access_iter last(iota_ints.data() + iota_ints.size());
        std::iota(
            std::make_reverse_iterator(last),
            std::make_reverse_iterator(first),
            0);
        std::sort(first, last);
        BOOST_TEST(iota_ints == ints);
    }
}


{
    basic_adapted_random_access_iter first(ints.data());
    basic_adapted_random_access_iter last(ints.data() + ints.size());

    BOOST_TEST(*first == 0);
    BOOST_TEST(*(first + 1) == 1);
    BOOST_TEST(*(first + 2) == 2);
    BOOST_TEST(*(1 + first) == 1);
    BOOST_TEST(*(2 + first) == 2);

    BOOST_TEST(first[0] == 0);
    BOOST_TEST(first[1] == 1);
    BOOST_TEST(first[2] == 2);

    BOOST_TEST(*(last - 1) == 9);
    BOOST_TEST(*(last - 2) == 8);
    BOOST_TEST(*(last - 3) == 7);

    BOOST_TEST(last[-1] == 9);
    BOOST_TEST(last[-2] == 8);
    BOOST_TEST(last[-3] == 7);

    BOOST_TEST(last - first == 10);
    BOOST_TEST(first == first);
    BOOST_TEST(first != last);
    BOOST_TEST(first < last);
    BOOST_TEST(first <= last);
    BOOST_TEST(first <= first);
    BOOST_TEST(last > first);
    BOOST_TEST(last >= first);
    BOOST_TEST(last >= last);

    {
        auto first_copy = first;
        first_copy += 10;
        BOOST_TEST(first_copy == last);
    }

    {
        auto last_copy = last;
        last_copy -= 10;
        BOOST_TEST(last_copy == first);
    }
}


{
    {
        std::array<int, 10> ints_copy;
        basic_adapted_random_access_iter first(ints.data());
        basic_adapted_random_access_iter last(ints.data() + ints.size());
        std::copy(first, last, ints_copy.begin());
        BOOST_TEST(ints_copy == ints);
    }

    {
        std::array<int, 10> ints_copy;
        basic_adapted_random_access_iter first(ints.data());
        basic_adapted_random_access_iter last(ints.data() + ints.size());
        std::copy(
            std::make_reverse_iterator(last),
            std::make_reverse_iterator(first),
            ints_copy.begin());
        std::reverse(ints_copy.begin(), ints_copy.end());
        BOOST_TEST(ints_copy == ints);
    }

    {
        std::array<int, 10> iota_ints;
        basic_adapted_random_access_iter first(iota_ints.data());
        basic_adapted_random_access_iter last(
            iota_ints.data() + iota_ints.size());
        std::iota(first, last, 0);
        BOOST_TEST(iota_ints == ints);
    }

    {
        std::array<int, 10> iota_ints;
        basic_adapted_random_access_iter first(iota_ints.data());
        basic_adapted_random_access_iter last(
            iota_ints.data() + iota_ints.size());
        std::iota(
            std::make_reverse_iterator(last),
            std::make_reverse_iterator(first),
            0);
        std::reverse(iota_ints.begin(), iota_ints.end());
        BOOST_TEST(iota_ints == ints);
    }

    {
        std::array<int, 10> iota_ints;
        basic_adapted_random_access_iter first(iota_ints.data());
        basic_adapted_random_access_iter last(
            iota_ints.data() + iota_ints.size());
        std::iota(
            std::make_reverse_iterator(last),
            std::make_reverse_iterator(first),
            0);
        std::sort(first, last);
        BOOST_TEST(iota_ints == ints);
    }
}


{
    {
        random_access first(ints.data());
        random_access last(ints.data() + ints.size());
        const_random_access first_copy(first);
        const_random_access last_copy(last);
        std::equal(first, last, first_copy, last_copy);
    }

    {
        adapted_random_access_iter<int> first(ints.data());
        adapted_random_access_iter<int> last(ints.data() + ints.size());
        adapted_random_access_iter<int const> first_copy(
            (int const *)ints.data());
        adapted_random_access_iter<int const> last_copy(
            (int const *)ints.data() + ints.size());
        std::equal(first, last, first_copy, last_copy);
    }
}


{
    {
        random_access first(ints.data());
        random_access last(ints.data() + ints.size());
        const_random_access first_const(first);
        const_random_access last_const(last);

        BOOST_TEST(first == first_const);
        BOOST_TEST(first_const == first);
        BOOST_TEST(first != last_const);
        BOOST_TEST(last_const != first);
        BOOST_TEST(first <= first_const);
        BOOST_TEST(first_const <= first);
        BOOST_TEST(first >= first_const);
        BOOST_TEST(first_const >= first);
        BOOST_TEST(last_const > first);
        BOOST_TEST(last > first_const);
        BOOST_TEST(first_const < last);
        BOOST_TEST(first < last_const);
    }

    {
        adapted_random_access_iter<int> first(ints.data());
        adapted_random_access_iter<int> last(ints.data() + ints.size());
        adapted_random_access_iter<int const> first_const(first);
        adapted_random_access_iter<int const> last_const(last);

        BOOST_TEST(first == first_const);
        BOOST_TEST(first_const == first);
        BOOST_TEST(first != last_const);
        BOOST_TEST(last_const != first);
        BOOST_TEST(first <= first_const);
        BOOST_TEST(first_const <= first);
        BOOST_TEST(first >= first_const);
        BOOST_TEST(first_const >= first);
        BOOST_TEST(last_const > first);
        BOOST_TEST(last > first_const);
        BOOST_TEST(first_const < last);
        BOOST_TEST(first < last_const);
    }
}


{
    {
        random_access first(ints.data());
        random_access last(ints.data() + ints.size());
        while (first != last)
            first++;
    }

    {
        random_access first(ints.data());
        random_access last(ints.data() + ints.size());
        while (first != last)
            last--;
    }

    {
        basic_random_access_iter first(ints.data());
        basic_random_access_iter last(ints.data() + ints.size());
        while (first != last)
            first++;
    }

    {
        basic_random_access_iter first(ints.data());
        basic_random_access_iter last(ints.data() + ints.size());
        while (first != last)
            last--;
    }

    {
        basic_adapted_random_access_iter first(ints.data());
        basic_adapted_random_access_iter last(ints.data() + ints.size());
        while (first != last)
            first++;
    }

    {
        basic_adapted_random_access_iter first(ints.data());
        basic_adapted_random_access_iter last(ints.data() + ints.size());
        while (first != last)
            last--;
    }
}


{
    random_access first(ints.data());
    random_access last(ints.data() + ints.size());

    BOOST_TEST(*first == 0);
    BOOST_TEST(*(first + 1) == 1);
    BOOST_TEST(*(first + 2) == 2);
    BOOST_TEST(*(1 + first) == 1);
    BOOST_TEST(*(2 + first) == 2);

    BOOST_TEST(first[0] == 0);
    BOOST_TEST(first[1] == 1);
    BOOST_TEST(first[2] == 2);

    BOOST_TEST(*(last - 1) == 9);
    BOOST_TEST(*(last - 2) == 8);
    BOOST_TEST(*(last - 3) == 7);

    BOOST_TEST(last[-1] == 9);
    BOOST_TEST(last[-2] == 8);
    BOOST_TEST(last[-3] == 7);

    BOOST_TEST(last - first == 10);
    BOOST_TEST(first == first);
    BOOST_TEST(first != last);
    BOOST_TEST(first < last);
    BOOST_TEST(first <= last);
    BOOST_TEST(first <= first);
    BOOST_TEST(last > first);
    BOOST_TEST(last >= first);
    BOOST_TEST(last >= last);

    {
        auto first_copy = first;
        first_copy += 10;
        BOOST_TEST(first_copy == last);
    }

    {
        auto last_copy = last;
        last_copy -= 10;
        BOOST_TEST(last_copy == first);
    }
}


{
    random_access first(ints.data());
    random_access last(ints.data() + ints.size());

    {
        std::array<int, 10> ints_copy;
        std::copy(first, last, ints_copy.begin());
        BOOST_TEST(ints_copy == ints);
    }

    {
        std::array<int, 10> ints_copy;
        std::copy(
            std::make_reverse_iterator(last),
            std::make_reverse_iterator(first),
            ints_copy.begin());
        std::reverse(ints_copy.begin(), ints_copy.end());
        BOOST_TEST(ints_copy == ints);
    }

    {
        std::array<int, 10> iota_ints;
        random_access first(iota_ints.data());
        random_access last(iota_ints.data() + iota_ints.size());
        std::iota(first, last, 0);
        BOOST_TEST(iota_ints == ints);
    }

    {
        std::array<int, 10> iota_ints;
        random_access first(iota_ints.data());
        random_access last(iota_ints.data() + iota_ints.size());
        std::iota(
            std::make_reverse_iterator(last),
            std::make_reverse_iterator(first),
            0);
        std::reverse(iota_ints.begin(), iota_ints.end());
        BOOST_TEST(iota_ints == ints);
    }

    {
        std::array<int, 10> iota_ints;
        random_access first(iota_ints.data());
        random_access last(iota_ints.data() + iota_ints.size());
        std::iota(
            std::make_reverse_iterator(last),
            std::make_reverse_iterator(first),
            0);
        std::sort(first, last);
        BOOST_TEST(iota_ints == ints);
    }
}


{
    const_random_access first(ints.data());
    const_random_access last(ints.data() + ints.size());

    {
        std::array<int, 10> ints_copy;
        std::copy(first, last, ints_copy.begin());
        BOOST_TEST(ints_copy == ints);
    }

    {
        BOOST_TEST(std::binary_search(first, last, 3));
        BOOST_TEST(std::binary_search(
            std::make_reverse_iterator(last),
            std::make_reverse_iterator(first),
            3,
            std::greater<>{}));
    }
}


{
    {
        zip_iter first(ints.data(), ones.data());
        zip_iter last(ints.data() + ints.size(), ones.data() + ones.size());
        BOOST_TEST(std::equal(first, last, tuples.begin(), tuples.end()));
    }

    {
        auto ints_copy = ints;
        std::reverse(ints_copy.begin(), ints_copy.end());
        auto ones_copy = ones;
        zip_iter first(ints_copy.data(), ones_copy.data());
        zip_iter last(
            ints_copy.data() + ints_copy.size(),
            ones_copy.data() + ones_copy.size());
        BOOST_TEST(!std::equal(first, last, tuples.begin(), tuples.end()));
        std::sort(first, last);
        BOOST_TEST(std::equal(first, last, tuples.begin(), tuples.end()));
    }

    {
        udt_zip_iter first(udts.data(), ones.data());
        udt_zip_iter last(udts.data() + udts.size(), ones.data() + ones.size());
        BOOST_TEST(
            std::equal(first, last, udt_tuples.begin(), udt_tuples.end()));
    }

    {
        auto udts_copy = udts;
        std::reverse(udts_copy.begin(), udts_copy.end());
        auto ones_copy = ones;
        udt_zip_iter first(udts_copy.data(), ones_copy.data());
        udt_zip_iter last(
            udts_copy.data() + udts_copy.size(),
            ones_copy.data() + ones_copy.size());
        BOOST_TEST(
            !std::equal(first, last, udt_tuples.begin(), udt_tuples.end()));
        std::sort(first, last);
        BOOST_TEST(
            std::equal(first, last, udt_tuples.begin(), udt_tuples.end()));
    }
}

{
    basic_random_access_iter first(ints.data());
    basic_random_access_iter last(ints.data() + ints.size());

    auto r = range<boost::stl_interfaces::element_layout::contiguous>(
        first, last);
    auto empty = range<boost::stl_interfaces::element_layout::contiguous>(
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

    // TODO: Disabled for now, because std::to_address() appears to be broken
    // in GCC10, which breaks the contiguous_iterator concept.
    // https://gcc.gnu.org/bugzilla/show_bug.cgi?id=96416
#if !defined(__cpp_lib_concepts)
    // data
    {
        BOOST_TEST(r.data() != nullptr);
        BOOST_TEST(r.data()[2] == 2);

        BOOST_TEST(empty.data() != nullptr);

        auto const cr = r;
        BOOST_TEST(cr.data() != nullptr);
        BOOST_TEST(cr.data()[2] == 2);

        auto const cempty = empty;
        BOOST_TEST(cempty.data() != nullptr);
    }
#endif

    // size
    {
        BOOST_TEST(r.size() == 10u);

        BOOST_TEST(empty.size() == 0u);

        auto const cr = r;
        BOOST_TEST(cr.size() == 10u);

        auto const cempty = empty;
        BOOST_TEST(cempty.size() == 0u);
    }

    // front/back
    {
        BOOST_TEST(r.front() == 0);
        BOOST_TEST(r.back() == 9);

        auto const cr = r;
        BOOST_TEST(cr.front() == 0);
        BOOST_TEST(cr.back() == 9);
    }

    // op[]
    {
        BOOST_TEST(r[2] == 2);

        auto const cr = r;
        BOOST_TEST(cr[2] == 2);
    }
}


{
    zip_iter first(ints.data(), ones.data());
    zip_iter last(ints.data() + ints.size(), ones.data() + ones.size());

    auto r = range<boost::stl_interfaces::element_layout::discontiguous>(
        first, last);
    auto empty =
        range<boost::stl_interfaces::element_layout::discontiguous>(
            first, first);

    // range begin/end
    {
        BOOST_TEST(std::equal(first, last, tuples.begin(), tuples.end()));
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

    // size
    {
        BOOST_TEST(r.size() == 10u);

        BOOST_TEST(empty.size() == 0u);

        auto const cr = r;
        BOOST_TEST(cr.size() == 10u);

        auto const cempty = empty;
        BOOST_TEST(cempty.size() == 0u);
    }

    // front/back
    {
        BOOST_TEST(r.front() == (std::tuple<int, int>(0, 1)));
        BOOST_TEST(r.back() == (std::tuple<int, int>(9, 1)));

        auto const cr = r;
        BOOST_TEST(cr.front() == (std::tuple<int, int>(0, 1)));
        BOOST_TEST(cr.back() == (std::tuple<int, int>(9, 1)));
    }

    // op[]
    {
        BOOST_TEST(r[2] == (std::tuple<int, int>(2, 1)));

        auto const cr = r;
        BOOST_TEST(cr[2] == (std::tuple<int, int>(2, 1)));
    }
}

    return boost::report_errors();
}
