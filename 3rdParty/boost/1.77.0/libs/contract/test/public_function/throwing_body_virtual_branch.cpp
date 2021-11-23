
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test virt pub func body throws with subcontr from middle of inheritance tree.

#include "smoke.hpp"
#include <boost/optional.hpp>
#include <boost/preprocessor/control/iif.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

int main() {
    std::ostringstream ok;
    
    c cc; // Test call to class at mid- inheritance tree (as base with bases).
    s_type s; s.value = "X"; // So body will throw.
    out.str("");
    boost::optional<result_type&> r;
    try {
        r = cc.f(s);
        BOOST_TEST(false);
    } catch(except_error const&) {
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "d::static_inv" << std::endl
                << "d::inv" << std::endl
                << "e::static_inv" << std::endl
                << "e::inv" << std::endl
                << "c::static_inv" << std::endl
                << "c::inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "d::f::pre" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "d::f::old" << std::endl
                << "e::f::old" << std::endl
                << "c::f::old" << std::endl
            #endif
            << "c::f::body" << std::endl
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "d::static_inv" << std::endl
                << "d::inv" << std::endl
                << "e::static_inv" << std::endl
                << "e::inv" << std::endl
                << "c::static_inv" << std::endl
                << "c::inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_EXCEPTS
                << "d::f::old" << std::endl
                << "d::f::except" << std::endl
                << "e::f::old" << std::endl
                << "e::f::except" << std::endl
                // No old call here because not a base object.
                << "c::f::except" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));

        #ifndef BOOST_CONTRACT_NO_OLDS
            #define BOOST_CONTRACT_TEST_old 1u
        #else
            #define BOOST_CONTRACT_TEST_old 0u
        #endif
        
        BOOST_TEST(!r); // Boost.Optional result not init (as body threw).
        BOOST_TEST_EQ(s.value, "X");
        BOOST_TEST_EQ(s.copies(), BOOST_CONTRACT_TEST_old * 3);
        BOOST_TEST_EQ(s.evals(), BOOST_CONTRACT_TEST_old * 3);
        BOOST_TEST_EQ(s.ctors(), s.dtors() + 1); // 1 for local var.

        BOOST_TEST_EQ(cc.y.value, "c");
        BOOST_TEST_EQ(cc.y.copies(), BOOST_CONTRACT_TEST_old);
        BOOST_TEST_EQ(cc.y.evals(), BOOST_CONTRACT_TEST_old);
        BOOST_TEST_EQ(cc.y.ctors(), cc.y.dtors() + 1); // 1 for member var.
        
        BOOST_TEST_EQ(cc.t<'d'>::z.value, "d");
        BOOST_TEST_EQ(cc.t<'d'>::z.copies(), BOOST_CONTRACT_TEST_old);
        BOOST_TEST_EQ(cc.t<'d'>::z.evals(), BOOST_CONTRACT_TEST_old);
        BOOST_TEST_EQ(cc.t<'d'>::z.ctors(), cc.t<'d'>::z.dtors() + 1); // 1 mem.

        BOOST_TEST_EQ(cc.t<'e'>::z.value, "e");
        BOOST_TEST_EQ(cc.t<'e'>::z.copies(), BOOST_CONTRACT_TEST_old);
        BOOST_TEST_EQ(cc.t<'e'>::z.evals(), BOOST_CONTRACT_TEST_old);
        BOOST_TEST_EQ(cc.t<'e'>::z.ctors(), cc.t<'e'>::z.dtors() + 1); // 1 mem.

        #undef BOOST_CONTRACT_TEST_old
    }
    return boost::report_errors();
}

