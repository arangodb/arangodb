// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/experimental/view.hpp>
#include <boost/hana/unpack.hpp>

#include <laws/base.hpp>
#include <support/seq.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    auto f = hana::test::_injection<0>{};
    auto g = hana::test::_injection<1>{};
    auto check = [=](auto ...x) {
        auto storage = ::seq(x...);
        auto transformed = hana::experimental::transformed(storage, f);

        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::unpack(transformed, g),
            g(f(x)...)
        ));
    };

    check();
    check(ct_eq<0>{});
    check(ct_eq<0>{}, ct_eq<1>{});
    check(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
    check(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{}, ct_eq<3>{});
}
