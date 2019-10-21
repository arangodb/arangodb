// Copyright Louis Dionne 2013-2017
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)

#include <boost/hana/assert.hpp>
#include <boost/hana/equal.hpp>
#include <boost/hana/experimental/type_name.hpp>
#include <boost/hana/string.hpp>

#include <cstdlib>
#include <iostream>
#include <regex>
#include <string>
namespace hana = boost::hana;


template <typename ...T>
struct Template { };

template <typename T>
void check_matches(std::string const& re) {
    std::string name = hana::to<char const*>(hana::experimental::type_name<T>());
    std::regex regex{re};
    if (!std::regex_match(name, regex)) {
        std::cerr << "type name '" << name << "' does not match regex '" << re << "'" << std::endl;
        std::abort();
    }
}

int main() {
    // Make sure this can be obtained at compile-time
    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::experimental::type_name<void>(),
        BOOST_HANA_STRING("void")
    ));

    BOOST_HANA_CONSTANT_CHECK(hana::equal(
        hana::experimental::type_name<int>(),
        BOOST_HANA_STRING("int")
    ));

    // Make sure we get something reasonable
    check_matches<int const>("int const|const int");
    check_matches<int&>(R"(int\s*&)");
    check_matches<int const&>(R"(const\s+int\s*&|int\s+const\s*&)");
    check_matches<int(&)[]>(R"(int\s*\(\s*&\s*\)\s*\[\s*\])");
    check_matches<int(&)[10]>(R"(int\s*\(\s*&\s*\)\s*\[\s*10\s*\])");
    check_matches<Template<void, char const*>>(R"(Template<\s*void\s*,\s*(char const|const char)\s*\*\s*>)");
    check_matches<void(*)(int)>(R"(void\s*\(\s*\*\s*\)\s*\(\s*int\s*\))");
}
