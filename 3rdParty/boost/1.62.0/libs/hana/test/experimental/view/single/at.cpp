// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/at.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/experimental/view.hpp>
#include <boost/hana/integral_constant.hpp>

#include <laws/base.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


int main() {
    {
        auto single = hana::experimental::single_view(ct_eq<0>{});
        BOOST_HANA_CONSTANT_CHECK(hana::equal(
            hana::at(single, hana::size_c<0>),
            ct_eq<0>{}
        ));
    }
}
