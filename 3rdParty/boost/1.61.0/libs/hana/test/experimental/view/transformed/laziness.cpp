// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/experimental/view.hpp>

#include <laws/base.hpp>
#include <support/seq.hpp>
namespace hana = boost::hana;
using hana::test::ct_eq;


struct undefined { };

int main() {
    // Make sure we do not evaluate the function unless required
    auto storage = ::seq(ct_eq<0>{}, ct_eq<1>{}, ct_eq<2>{});
    auto transformed = hana::experimental::transformed(storage, undefined{});
    (void)transformed;
}
