// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/experimental/view.hpp>
#include <boost/hana/is_empty.hpp>
#include <boost/hana/not.hpp>

#include <support/seq.hpp>
namespace hana = boost::hana;


struct undefined { };

int main() {
    auto container = ::seq;

    {
        auto xs = container();
        auto tr = hana::experimental::transformed(xs, undefined{});
        BOOST_HANA_CONSTANT_CHECK(hana::is_empty(tr));
    }

    {
        auto xs = container(undefined{});
        auto tr = hana::experimental::transformed(xs, undefined{});
        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(tr)));
    }

    {
        auto xs = container(undefined{}, undefined{});
        auto tr = hana::experimental::transformed(xs, undefined{});
        BOOST_HANA_CONSTANT_CHECK(hana::not_(hana::is_empty(tr)));
    }
}
