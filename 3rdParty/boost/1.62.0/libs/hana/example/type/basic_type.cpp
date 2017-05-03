// Copyright Louis Dionne 2013-2016
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/for_each.hpp>
#include <boost/hana/tuple.hpp>
#include <boost/hana/type.hpp>

#include <boost/core/demangle.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
namespace hana = boost::hana;


// Using hana::type<T> as a parameter type wouldn't work, since it may be
// a dependent type, in which case template argument deduction will fail.
// Using hana::basic_type<T> is fine, since this one is guaranteed to be
// a non dependent base of hana::type<T>.
template <typename T>
std::string const& name_of(hana::basic_type<T>) {
    static std::string name = boost::core::demangle(typeid(T).name());
    return name;
}

int main() {
    hana::for_each(hana::tuple_t<int, char, void, std::string>, [](auto type) {
        std::cout << name_of(type) << std::endl;
    });
}
