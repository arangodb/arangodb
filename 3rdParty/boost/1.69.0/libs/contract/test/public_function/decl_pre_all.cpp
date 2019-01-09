
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test all derived and base classes with preconditions.

#undef BOOST_CONTRACT_TEST_NO_A_PRE
#undef BOOST_CONTRACT_TEST_NO_B_PRE
#undef BOOST_CONTRACT_TEST_NO_C_PRE
#include "decl.hpp"

#include <boost/detail/lightweight_test.hpp>
#include <sstream>
#include <string>

std::string ok_begin() {
    std::ostringstream ok; ok << "" // Suppress a warning.
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "c::static_inv" << std::endl
            << "c::inv" << std::endl
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
    ;
    return ok.str();
}
        
std::string ok_end() {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "c::f::old" << std::endl
            << "b::f::old" << std::endl
            << "a::f::old" << std::endl
        #endif
        << "a::f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "c::static_inv" << std::endl
            << "c::inv" << std::endl
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "c::f::old" << std::endl // Old only if post (or except) run.
            << "c::f::post" << std::endl
            << "b::f::old" << std::endl
            << "b::f::post" << std::endl
            << "a::f::post" << std::endl
        #endif
    ;
    return ok.str();
}

struct err {}; // Global decl so visible in MSVC10 lambdas.

int main() {
    std::ostringstream ok;

    a aa;
    
    a_pre = true;
    b_pre = true;
    c_pre = true;
    out.str("");
    aa.f();
    ok.str(""); ok
        << ok_begin()
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "c::f::pre" << std::endl // Test only c::f::pre checked.
        #endif
        << ok_end()
    ;
    BOOST_TEST(out.eq(ok.str()));

    a_pre = true;
    b_pre = false;
    c_pre = false;
    out.str("");
    aa.f();
    ok.str(""); ok
        << ok_begin()
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "c::f::pre" << std::endl
            << "b::f::pre" << std::endl
            << "a::f::pre" << std::endl // Test all pre checked.
        #endif
        << ok_end()
    ;
    BOOST_TEST(out.eq(ok.str()));
    
    a_pre = false;
    b_pre = true;
    c_pre = false;
    out.str("");
    aa.f();
    ok.str(""); ok
        << ok_begin()
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "c::f::pre" << std::endl
            << "b::f::pre" << std::endl
            // Test only a::f::pre not checked.
        #endif
        << ok_end()
    ;
    BOOST_TEST(out.eq(ok.str()));
    
    a_pre = false;
    b_pre = false;
    c_pre = true;
    out.str("");
    aa.f();
    ok.str(""); ok
        << ok_begin()
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "c::f::pre" << std::endl // Test only c::f::pre checked.
        #endif
        << ok_end()
    ;
    BOOST_TEST(out.eq(ok.str()));
    
    boost::contract::set_precondition_failure(
            [] (boost::contract::from) { throw err(); });

    a_pre = false;
    b_pre = false;
    c_pre = false;
    out.str("");
    try {
        aa.f();
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                    BOOST_TEST(false);
            } catch(err const&) {
        #endif
        ok.str(""); ok
            << ok_begin()
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "c::f::pre" << std::endl
                << "b::f::pre" << std::endl
                << "a::f::pre" << std::endl // Test all pre checked and failed.
            #else
                << ok_end()
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    return boost::report_errors();
}

