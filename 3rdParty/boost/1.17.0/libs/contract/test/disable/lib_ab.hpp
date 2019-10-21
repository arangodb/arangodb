
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include "lib_a.hpp"
#include "lib_b.hpp"
#include "../detail/oteststream.hpp"
#include <boost/contract/core/exception.hpp>
#include <boost/contract/core/config.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>
#include <string>

std::string ok_f() {
    std::ostringstream ok; ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "a::f::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "a::f::old" << std::endl
        #endif
        << "a::f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "a::f::post" << std::endl
        #endif
    ;
    return ok.str();
}

int main() {
    using boost::contract::test::detail::out;
    std::ostringstream ok;
    b bb;

    out("");
    bb.g();
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "b::g::pre" << std::endl
            #ifdef BOOST_CONTRACT_PRECONDITIONS_DISABLE_NO_ASSERTION
                // Test preconditions have disabled no contract.
                << ok_f()
            #else
                // Test call while checking executes body (but no contracts).
                << "a::f::body" << std::endl
            #endif
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::g::old" << std::endl
        #endif
        << "b::g::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS    
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
        #endif 
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::g::post" << std::endl
            // Test call while checking executes body (but no contracts).
            << "a::f::body" << std::endl
        #endif
    ;
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(), ok.str()));

    // Test old values not copied for disabled contracts.

    #if      defined(BOOST_CONTRACT_PRECONDITIONS_DISABLE_NO_ASSERTION) && \
            !defined(BOOST_CONTRACT_NO_PRECONDITIONS) && \
            !defined(BOOST_CONTRACT_NO_OLDS)
        #define BOOST_CONTRACT_TEST_old 1u
    #else
        #define BOOST_CONTRACT_TEST_old 0u
    #endif
    
    BOOST_TEST_EQ(a::x_type::copies(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(a::x_type::evals(), BOOST_CONTRACT_TEST_old);
    BOOST_TEST_EQ(a::x_type::ctors(), a::x_type::dtors());

    // Double check a call to f outside another contract checks f's contracts.
    
    out("");
    call_f();
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(), ok_f()));

    // Test setting failure handlers (from this program using a lib).

    a::disable_pre_failure();
    out("");
    boost::contract::precondition_failure(boost::contract::from());
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(),
            "a::pre_failure\n"));
    
    a::disable_post_failure();
    out("");
    boost::contract::postcondition_failure(boost::contract::from());
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(),
            "a::post_failure\n"));
    
    a::disable_entry_inv_failure();
    out("");
    boost::contract::entry_invariant_failure(boost::contract::from());
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(),
            "a::entry_inv_failure\n"));
    
    a::disable_exit_inv_failure();
    out("");
    boost::contract::exit_invariant_failure(boost::contract::from());
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(),
            "a::exit_inv_failure\n"));
    
    a::disable_inv_failure();
    out("");
    boost::contract::entry_invariant_failure(boost::contract::from());
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(),
            "a::inv_failure\n"));
    out("");
    boost::contract::exit_invariant_failure(boost::contract::from());
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(),
            "a::inv_failure\n"));
    
    a::disable_failure();
    out("");
    boost::contract::precondition_failure(boost::contract::from());
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(),
            "a::failure\n"));
    out("");
    boost::contract::postcondition_failure(boost::contract::from());
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(),
            "a::failure\n"));
    out("");
    boost::contract::entry_invariant_failure(boost::contract::from());
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(),
            "a::failure\n"));
    out("");
    boost::contract::exit_invariant_failure(boost::contract::from());
    BOOST_TEST(boost::contract::test::detail::oteststream::eq(out(),
            "a::failure\n"));

    // Test setting failure handlers (from a lib using another lib).

    BOOST_TEST(b::test_disable_pre_failure());
    BOOST_TEST(b::test_disable_post_failure());
    BOOST_TEST(b::test_disable_entry_inv_failure());
    BOOST_TEST(b::test_disable_exit_inv_failure());
    BOOST_TEST(b::test_disable_inv_failure());
    BOOST_TEST(b::test_disable_failure());
    
    #undef BOOST_CONTRACT_TEST_old
    return boost::report_errors();
}

