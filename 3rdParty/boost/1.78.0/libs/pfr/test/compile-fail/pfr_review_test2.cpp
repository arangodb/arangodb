// Copyright (c) 2020-2021 Antony Polukhin
// Copyright (c) 2020 Richard Hodges
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

// Test case from https://github.com/madmongo1/pfr_review/blob/master/pre-cxx20/test-2.cpp

#include <boost/pfr/functions_for.hpp>

#include <boost/utility/string_view.hpp>

#include <sstream>
#include <string>

#include <boost/core/lightweight_test.hpp>

namespace the_wild {
    struct animal {
        std::string        name;
        boost::string_view temperament;
    };
    
    // Error: std::hash not specialized for type
    // OR in C++14:
    // Error: animal is not constexpr initializable
    BOOST_PFR_FUNCTIONS_FOR(animal)
} // namespace the_wild

const auto fido = the_wild::animal { "fido", "aloof" };

int main() {
    std::ostringstream oss;
    oss << fido;

    BOOST_TEST_EQ(oss.str(), "{\"fido\", \"aloof\"}");

    return boost::report_errors();
}
