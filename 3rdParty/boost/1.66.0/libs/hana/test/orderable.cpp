// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/orderable.hpp>
#include <boost/hana/greater.hpp>
#include <boost/hana/greater_equal.hpp>
#include <boost/hana/less.hpp>
#include <boost/hana/less_equal.hpp>
#include <boost/hana/max.hpp>
#include <boost/hana/min.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/comparable.hpp>
#include <laws/orderable.hpp>

#include <string>
#include <type_traits>
namespace hana = boost::hana;
using namespace std::literals;


// Minimal LessThanComparable types
struct ord1 { int value; };
struct ord2 {
    int value;
    constexpr operator ord1() const { return {value}; }
};

template <typename T, typename U, typename = std::enable_if_t<
    (std::is_same<T, ord1>{} || std::is_same<T, ord2>{}) &&
    (std::is_same<U, ord1>{} || std::is_same<U, ord2>{})
>>
constexpr bool operator<(T a, U b)
{ return a.value < b.value; }

namespace boost { namespace hana {
    template <typename T, typename U>
    struct equal_impl<T, U, when<
        (std::is_same<T, ord1>{} || std::is_same<T, ord2>{}) &&
        (std::is_same<U, ord1>{} || std::is_same<U, ord2>{})
    >> {
        static constexpr bool apply(T a, U b)
        { return a.value == b.value; }
    };
}}

int main() {
    // laws
    hana::test::TestOrderable<int>{hana::make_tuple(0,1,2,3,4,5)};
    hana::test::TestOrderable<unsigned int>{hana::make_tuple(0u,1u,2u,3u,4u,5u)};
    hana::test::TestOrderable<long>{hana::make_tuple(0l,1l,2l,3l,4l,5l)};
    hana::test::TestOrderable<unsigned long>{hana::make_tuple(0ul,1ul,2ul,3ul,4ul,5ul)};
    hana::test::TestOrderable<ord1>{hana::make_tuple(ord1{0}, ord1{1}, ord1{2}, ord1{3}, ord1{4})};

    // Orderable => Comparable
    hana::test::TestComparable<ord1>{hana::make_tuple(ord1{0}, ord1{1}, ord1{2}, ord1{3}, ord1{4})};

    // less
    {
        BOOST_HANA_CONSTEXPR_CHECK(hana::less(5, 6));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::less(6, 6)));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::less(7, 6)));

        // Provided model for LessThanComparable types
        BOOST_HANA_CONSTEXPR_CHECK(hana::less(ord1{0}, ord1{1}));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::less(ord1{0}, ord1{0})));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::less(ord1{1}, ord1{0})));

        BOOST_HANA_CONSTEXPR_CHECK(hana::less(ord1{0}, ord2{1}));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::less(ord1{0}, ord2{0})));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::less(ord1{1}, ord2{0})));

        BOOST_HANA_CONSTEXPR_CHECK(hana::less(ord2{0}, ord1{1}));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::less(ord2{0}, ord1{0})));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::less(ord2{1}, ord1{0})));

        BOOST_HANA_RUNTIME_CHECK(hana::less("ab", "abc"s));
        BOOST_HANA_RUNTIME_CHECK(hana::less("abc"s, "abcde"));
    }

    // greater
    {
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::greater(5, 6)));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::greater(6, 6)));
        BOOST_HANA_CONSTEXPR_CHECK(hana::greater(7, 6));

        BOOST_HANA_RUNTIME_CHECK(hana::greater("abcd", "ab"s));
        BOOST_HANA_RUNTIME_CHECK(hana::greater("abc"s, "abb"));
    }

    // less_equal
    {
        BOOST_HANA_CONSTEXPR_CHECK(hana::less_equal(5, 6));
        BOOST_HANA_CONSTEXPR_CHECK(hana::less_equal(6, 6));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::less_equal(7, 6)));

        BOOST_HANA_RUNTIME_CHECK(hana::less_equal("ab", "abcd"s));
        BOOST_HANA_RUNTIME_CHECK(hana::less_equal("abc"s, "abc"));
    }

    // greater_equal
    {
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::greater_equal(5, 6)));
        BOOST_HANA_CONSTEXPR_CHECK(hana::greater_equal(6, 6));
        BOOST_HANA_CONSTEXPR_CHECK(hana::greater_equal(7, 6));

        BOOST_HANA_RUNTIME_CHECK(hana::greater_equal("abcd", "ab"s));
        BOOST_HANA_RUNTIME_CHECK(hana::greater_equal("abc"s, "abc"));
    }

    // min
    {
        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(hana::min(5, 6), 5));
        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(hana::min(6, 5), 5));
    }

    // max
    {
        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(hana::max(5, 6), 6));
        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(hana::max(6, 5), 6));
    }
}
