// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/concept/struct.hpp>
#include <boost/hana/contains.hpp>
#include <boost/hana/define_struct.hpp>
#include <boost/hana/string.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


// This allows us to make sure we can enter template types
// containing commas in the macro.
template <typename T, typename ...>
using commas = T;

struct Data0 {
    BOOST_HANA_DEFINE_STRUCT(Data0);
};
struct Data1 {
    BOOST_HANA_DEFINE_STRUCT(Data1,
        (commas<ct_eq<1>, void>, member1)
    );
};
struct Data2 {
    BOOST_HANA_DEFINE_STRUCT(Data2,
        (commas<ct_eq<1>, void, void>, member1),
        (ct_eq<2>, member2)
    );
};
struct Data3 {
    BOOST_HANA_DEFINE_STRUCT(Data3,
        (ct_eq<1>, member1),
        (ct_eq<2>, member2),
        (commas<ct_eq<3>, void, void, void>, member3)
    );
};

static_assert(hana::Struct<Data0>::value, "");
static_assert(hana::Struct<Data1>::value, "");
static_assert(hana::Struct<Data2>::value, "");
static_assert(hana::Struct<Data3>::value, "");

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::contains(Data1{}, BOOST_HANA_STRING("member1")));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(Data2{}, BOOST_HANA_STRING("member1")));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(Data2{}, BOOST_HANA_STRING("member2")));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(Data3{}, BOOST_HANA_STRING("member1")));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(Data3{}, BOOST_HANA_STRING("member2")));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(Data3{}, BOOST_HANA_STRING("member3")));
}
