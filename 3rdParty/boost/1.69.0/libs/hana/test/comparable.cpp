// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/comparable.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/tuple.hpp>

#include <laws/base.hpp>
#include <laws/comparable.hpp>

#include <type_traits>
namespace hana = boost::hana;


// Minimal EqualityComparable types
struct eq1 { int value; };
struct eq2 {
    int value;
    constexpr operator eq1() const { return {value}; }
};

template <typename T, typename U, typename = std::enable_if_t<
    (std::is_same<T, eq1>{} || std::is_same<T, eq2>{}) &&
    (std::is_same<U, eq1>{} || std::is_same<U, eq2>{})
>>
constexpr bool operator==(T a, U b)
{ return a.value == b.value; }

template <typename T, typename U, typename = std::enable_if_t<
    (std::is_same<T, eq1>{} || std::is_same<T, eq2>{}) &&
    (std::is_same<U, eq1>{} || std::is_same<U, eq2>{})
>>
constexpr bool operator!=(T a, U b)
{ return !(a == b); }


int main() {
    // equal
    {
        // Two objects of different data types are unequal by default,
        // and no model is provided for two objects of the same data type.
        struct Random1 { }; struct Random2 { };
        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::equal(Random1{}, Random2{})));
        static_assert(!hana::Comparable<Random1>::value, "");
        static_assert(!hana::Comparable<Random2>::value, "");

        // Provided model for EqualityComparable types
        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(eq1{0}, eq1{0}));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(eq1{0}, eq1{1})));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(eq1{1}, eq1{0})));

        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(eq1{0}, eq2{0}));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(eq1{0}, eq2{1})));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(eq1{1}, eq2{0})));

        BOOST_HANA_CONSTEXPR_CHECK(hana::equal(eq2{0}, eq1{0}));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(eq2{0}, eq1{1})));
        BOOST_HANA_CONSTEXPR_CHECK(hana::not_(hana::equal(eq2{1}, eq1{0})));
    }

    // laws
    hana::test::TestComparable<int>{hana::make_tuple(0,1,2,3,4,5)};
    hana::test::TestComparable<unsigned int>{hana::make_tuple(0u,1u,2u,3u,4u,5u)};
    hana::test::TestComparable<long>{hana::make_tuple(0l,1l,2l,3l,4l,5l)};
    hana::test::TestComparable<unsigned long>{hana::make_tuple(0ul,1ul,2ul,3ul,4ul,5ul)};
    hana::test::TestComparable<eq1>{hana::make_tuple(eq1{0}, eq1{1}, eq1{2}, eq1{3}, eq1{4})};
}
