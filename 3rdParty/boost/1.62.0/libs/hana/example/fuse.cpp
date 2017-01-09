// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/fuse.hpp>
#include <boost/hana/tuple.hpp>
namespace hana = boost::hana;


auto tie = [](auto& ...vars) {
    return hana::fuse([&vars...](auto ...values) {
        // Using an initializer list to sequence the assignments.
        int dummy[] = {0, ((void)(vars = values), 0)...};
        (void)dummy;
    });
};

int main() {
    int a = 0;
    char b = '\0';
    double c = 0;

    tie(a, b, c)(hana::make_tuple(1, '2', 3.3));
    BOOST_HANA_RUNTIME_CHECK(a == 1 && b == '2' && c == 3.3);
}
