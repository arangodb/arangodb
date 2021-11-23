// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/not.hpp>
#include <boost/hana/type.hpp>

#include <string>
#include <vector>
namespace hana = boost::hana;


int main() {
    // Checking for a member
    struct Person { std::string name; };
    auto has_name = hana::is_valid([](auto&& p) -> decltype((void)p.name) { });

    Person joe{"Joe"};
    static_assert(has_name(joe), "");
    static_assert(!has_name(1), "");


    // Checking for a nested type
    auto has_value_type = hana::is_valid([](auto t) -> hana::type<
        typename decltype(t)::type::value_type
    > { });

    static_assert(has_value_type(hana::type_c<std::vector<int>>), "");
    static_assert(!has_value_type(hana::type_c<Person>), "");
}
