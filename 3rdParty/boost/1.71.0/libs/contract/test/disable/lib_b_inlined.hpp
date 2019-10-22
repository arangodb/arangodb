
#ifndef BOOST_CONTRACT_TEST_LIB_B_INLINED_HPP_
#define BOOST_CONTRACT_TEST_LIB_B_INLINED_HPP_

// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

#include "lib_b.hpp"
#include "lib_a.hpp"
#include "../detail/oteststream.hpp"
#include <boost/contract.hpp> // All headers to test ODR for entire lib.

bool call_f() {
    a aa;
    a::x_type x; x.value = -123;
    return aa.f(x) == -123;
}

void b::static_invariant() {
    using boost::contract::test::detail::out;
    out("b::static_inv\n");
}

void b::invariant() const {
    using boost::contract::test::detail::out;
    out("b::inv\n");
}
    
void b::g() {
    using boost::contract::test::detail::out;
    boost::contract::check c = boost::contract::public_function(this)
        .precondition([&] {
            out("b::g::pre\n");
            BOOST_CONTRACT_ASSERT(call_f());
        })
        .old([&] { out("b::g::old\n"); })
        .postcondition([&] {
            out("b::g::post\n");
            BOOST_CONTRACT_ASSERT(call_f());
        })
    ;
    out("b::g::body\n");
}

bool b::test_disable_pre_failure() {
    using boost::contract::test::detail::out;
    a::disable_pre_failure();
    out("");
    boost::contract::precondition_failure(boost::contract::from());
    return boost::contract::test::detail::oteststream::eq(out(),
            "a::pre_failure\n");
}

bool b::test_disable_post_failure() {
    using boost::contract::test::detail::out;
    a::disable_post_failure();
    out("");
    boost::contract::postcondition_failure(boost::contract::from());
    return boost::contract::test::detail::oteststream::eq(out(),
            "a::post_failure\n");
}
    
bool b::test_disable_entry_inv_failure() {
    using boost::contract::test::detail::out;
    a::disable_entry_inv_failure();
    out("");
    boost::contract::entry_invariant_failure(boost::contract::from());
    return boost::contract::test::detail::oteststream::eq(out(),
            "a::entry_inv_failure\n");
}
    
bool b::test_disable_exit_inv_failure() {
    using boost::contract::test::detail::out;
    a::disable_exit_inv_failure();
    out("");
    boost::contract::exit_invariant_failure(boost::contract::from());
    return boost::contract::test::detail::oteststream::eq(out(),
            "a::exit_inv_failure\n");
}
    
bool b::test_disable_inv_failure() {
    using boost::contract::test::detail::out;
    
    a::disable_inv_failure();
    out("");
    boost::contract::entry_invariant_failure(boost::contract::from());
    bool entry_inv = boost::contract::test::detail::oteststream::eq(out(),
            "a::inv_failure\n");
    out("");
    boost::contract::exit_invariant_failure(boost::contract::from());
    bool exit_inv = boost::contract::test::detail::oteststream::eq(out(),
            "a::inv_failure\n");
    
    return entry_inv && exit_inv;
}
    
bool b::test_disable_failure() {
    using boost::contract::test::detail::out;

    a::disable_failure();
    out("");
    boost::contract::precondition_failure(boost::contract::from());
    bool pre = boost::contract::test::detail::oteststream::eq(out(),
            "a::failure\n");
    out("");
    boost::contract::postcondition_failure(boost::contract::from());
    bool post = boost::contract::test::detail::oteststream::eq(out(),
            "a::failure\n");
    out("");
    boost::contract::entry_invariant_failure(boost::contract::from());
    bool entry_inv = boost::contract::test::detail::oteststream::eq(out(),
            "a::failure\n");
    out("");
    boost::contract::exit_invariant_failure(boost::contract::from());
    bool exit_inv = boost::contract::test::detail::oteststream::eq(out(),
            "a::failure\n");

    return pre && post && entry_inv && exit_inv;
}

#endif // #include guard

