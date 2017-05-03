// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/bool.hpp>
#include <boost/hana/experimental/types.hpp>
#include <boost/hana/replace_if.hpp>
#include <boost/hana/type.hpp>
namespace hana = boost::hana;


struct predicate { hana::true_ operator()(...) const; };

int main() {
    auto types = hana::experimental::types<void, void>{};
    hana::replace_if(types, predicate{}, hana::type<struct anything>{});
}
