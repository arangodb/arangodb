// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/adapt_struct.hpp>
#include <boost/hana/assert.hpp>
#include <boost/hana/concept/struct.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/string.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


namespace ns {
    struct Data0 { };
    struct Data1 {
        ct_eq<1> member1;
    };
    struct Data2 {
        ct_eq<1> member1;
        ct_eq<2> member2;
    };
    struct Data3 {
        ct_eq<1> member1;
        ct_eq<2> member2;
        ct_eq<3> member3;
    };
    struct MemberArray {
        int array[10];
    };
}

BOOST_HANA_ADAPT_STRUCT(ns::Data0);
BOOST_HANA_ADAPT_STRUCT(ns::Data1, member1);
BOOST_HANA_ADAPT_STRUCT(ns::Data2, member1, member2);
BOOST_HANA_ADAPT_STRUCT(ns::Data3, member1, member2, member3);
BOOST_HANA_ADAPT_STRUCT(ns::MemberArray, array);

static_assert(hana::Struct<ns::Data0>::value, "");
static_assert(hana::Struct<ns::Data1>::value, "");
static_assert(hana::Struct<ns::Data2>::value, "");
static_assert(hana::Struct<ns::Data3>::value, "");
static_assert(hana::Struct<ns::MemberArray>::value, "");

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::contains(ns::Data1{}, BOOST_HANA_STRING("member1")));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(ns::Data2{}, BOOST_HANA_STRING("member1")));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(ns::Data2{}, BOOST_HANA_STRING("member2")));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(ns::Data3{}, BOOST_HANA_STRING("member1")));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(ns::Data3{}, BOOST_HANA_STRING("member2")));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(ns::Data3{}, BOOST_HANA_STRING("member3")));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(ns::MemberArray{}, BOOST_HANA_STRING("array")));
}
