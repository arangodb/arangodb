// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/core/is_a.hpp>
#include <boost/hana/integral_constant.hpp>
#include <boost/hana/value.hpp>
namespace hana = boost::hana;


void function() { }
void function_index(...) { }

int main() {
    // times member function
    {
        int counter = 0;
        hana::int_c<3>.times([&] { ++counter; });
        BOOST_HANA_RUNTIME_CHECK(counter == 3);

        // Call .times with a normal function used to fail.
        hana::int_c<3>.times(function);

        // make sure times can be accessed as a static member function too
        decltype(hana::int_c<5>)::times([]{ });

        // make sure xxx.times can be used as a function object
        auto z = hana::int_c<5>.times;
        (void)z;
    }

    // times.with_index
    {
        int index = 0;
        hana::int_c<3>.times.with_index([&](auto i) {
            static_assert(hana::is_an<hana::integral_constant_tag<int>>(i), "");
            BOOST_HANA_RUNTIME_CHECK(hana::value(i) == index);
            ++index;
        });

        // Calling .times.with_index with a normal function used to fail.
        hana::int_c<3>.times.with_index(function_index);

        // make sure times.with_index can be accessed as a static member
        // function too
        auto times = hana::int_c<5>.times;
        decltype(times)::with_index([](auto) { });

        // make sure xxx.times.with_index can be used as a function object
        auto z = hana::int_c<5>.times.with_index;
        (void)z;

        // make sure we're calling the function in the right order
        int current = 0;
        hana::int_c<5>.times.with_index([&](auto i) {
            BOOST_HANA_RUNTIME_CHECK(hana::value(i) == current);
            ++current;
        });
    }
}
