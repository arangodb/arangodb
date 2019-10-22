// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/experimental/printable.hpp>
#include <boost/hana/type.hpp>

#include <iostream>
namespace hana = boost::hana;


template <typename ...T>
struct Template { };

int main() {
    // Since demangling may not always be available, and since that's not
    // our job to test this (but Boost.Core's job), we don't test the
    // actual demangling of types. We merely print a type to make sure
    // things don't blow up stupidly, but we can't really test more than
    // that.

    std::cout << hana::experimental::print(hana::type_c<void>) << std::endl;

    std::cout << hana::experimental::print(hana::type_c<int>) << std::endl;

    std::cout << hana::experimental::print(hana::type_c<int const>) << std::endl;

    std::cout << hana::experimental::print(hana::type_c<int&>) << std::endl;

    std::cout << hana::experimental::print(hana::type_c<int const&>) << std::endl;

    std::cout << hana::experimental::print(hana::type_c<int(&)[]>) << std::endl;

    std::cout << hana::experimental::print(hana::type_c<Template<void, char const*>>) << std::endl;
}
