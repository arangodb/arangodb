// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/eval.hpp>
#include <boost/hana/extend.hpp>
#include <boost/hana/extract.hpp>
#include <boost/hana/lazy.hpp>

#include <sstream>
namespace hana = boost::hana;


int main() {
    std::stringstream s("1 2 3");
    auto i = hana::make_lazy([&] {
        int i;
        s >> i;
        return i;
    })();

    auto i_plus_one = hana::extend(i, [](auto lazy_int) {
        return hana::eval(lazy_int) + 1;
    });

    BOOST_HANA_RUNTIME_CHECK(hana::extract(i_plus_one) == 2);
    BOOST_HANA_RUNTIME_CHECK(hana::extract(i_plus_one) == 3);
    BOOST_HANA_RUNTIME_CHECK(hana::extract(i_plus_one) == 4);
}
