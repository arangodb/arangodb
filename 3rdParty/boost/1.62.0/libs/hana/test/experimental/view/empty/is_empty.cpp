// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/experimental/view.hpp>
#include <boost/hana/is_empty.hpp>
namespace hana = boost::hana;


int main() {
    {
        auto empty = hana::experimental::empty_view();
        BOOST_HANA_CONSTANT_CHECK(hana::is_empty(empty));
    }
}
