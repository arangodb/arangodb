// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/adapt_adt.hpp>
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
        ct_eq<1> get_member1() const { return {}; }
    };
    struct Data2 {
        ct_eq<1> get_member1() const { return {}; }
        ct_eq<2> get_member2() const { return {}; }
    };
    struct Data3 {
        ct_eq<1> get_member1() const { return {}; }
        ct_eq<2> get_member2() const { return {}; }
        ct_eq<3> get_member3() const { return {}; }
    };
}

// Note: We use commas in the lambdas to make sure the macro can handle it.
BOOST_HANA_ADAPT_ADT(ns::Data0);
BOOST_HANA_ADAPT_ADT(ns::Data1,
    (member1, [](auto const& d) { return 0, d.get_member1(); })
);
BOOST_HANA_ADAPT_ADT(ns::Data2,
    (member1, [](auto const& d) { return 0, d.get_member1(); }),
    (member2, [](auto const& d) { return d.get_member2(); })
);
BOOST_HANA_ADAPT_ADT(ns::Data3,
    (member1, [](auto const& d) { return 0, d.get_member1(); }),
    (member2, [](auto const& d) { return d.get_member2(); }),
    (member3, [](auto const& d) { return d.get_member3(); })
);

static_assert(hana::Struct<ns::Data0>::value, "");
static_assert(hana::Struct<ns::Data1>::value, "");
static_assert(hana::Struct<ns::Data2>::value, "");
static_assert(hana::Struct<ns::Data3>::value, "");

int main() {
    BOOST_HANA_CONSTANT_CHECK(hana::contains(ns::Data1{}, BOOST_HANA_STRING("member1")));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(ns::Data2{}, BOOST_HANA_STRING("member1")));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(ns::Data2{}, BOOST_HANA_STRING("member2")));

    BOOST_HANA_CONSTANT_CHECK(hana::contains(ns::Data3{}, BOOST_HANA_STRING("member1")));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(ns::Data3{}, BOOST_HANA_STRING("member2")));
    BOOST_HANA_CONSTANT_CHECK(hana::contains(ns::Data3{}, BOOST_HANA_STRING("member3")));
}
