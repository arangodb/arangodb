
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test contract compilation on/off (using macro interface only).

#include "../detail/oteststream.hpp"
#include "../detail/unprotected_commas.hpp"
#include <boost/contract_macro.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

void f(bool check) {
    BOOST_CONTRACT_CHECK((
        [&] () -> bool {
            boost::contract::test::detail::unprotected_commas<void, void, void>
                    ::call();
            out << "f::check" << std::endl;
            return check;
        }()
    ));
    out << "f::body" << std::endl;
}

struct err {}; // Global decl so visible in MSVC10 lambdas.

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

