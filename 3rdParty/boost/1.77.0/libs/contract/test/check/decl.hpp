
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test check (class and macro).

#include "../detail/oteststream.hpp"
#include <boost/contract/check.hpp>
#include <boost/contract/assert.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct err {}; // Global decl so visible in MSVC10 lambdas.

void f(bool check) {
    #ifdef BOOST_CONTRACT_TEST_CHECK_MACRO
        BOOST_CONTRACT_CHECK([&] () -> bool {
            out << "f::check" << std::endl;
            return check;
        }());
    #else
        boost::contract::check c = [&] {
            out << "f::check" << std::endl;
            BOOST_CONTRACT_ASSERT(check);
        };
    #endif
    out << "f::body" << std::endl;
}

int main() {
    std::ostringstream ok;

    out.str("");
    f(true);
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_CHECKS
            << "f::check" << std::endl
        #endif
        << "f::body" << std::endl
    ;
    BOOST_TEST(out.eq(ok.str()));

    boost::contract::set_check_failure([] { throw err(); });

    out.str("");
    try {
        f(false);
        #ifndef BOOST_CONTRACT_NO_CHECKS
            BOOST_TEST(false);
            } catch(err const&) {
                ok.str("");
                ok << "f::check" << std::endl;
        #else
            ok.str("");
            ok << "f::body" << std::endl;
        #endif
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    return boost::report_errors();
}

