
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test specifying except, no pre, old, or post (same if not free func).

#include "../detail/oteststream.hpp"
#include <boost/contract/function.hpp>
#include <boost/contract/check.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

void f() {
    boost::contract::check c = boost::contract::function()
        .except([] { out << "f::except" << std::endl; })
    ;
    out << "f::body" << std::endl;
}

int main() {
    std::ostringstream ok;

    out.str("");
    f();
    ok.str("");
    ok
        << "f::body" << std::endl
    ;
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

