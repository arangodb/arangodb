
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test making all contract extra declarations (base types, inv, etc.) private.

#include "../detail/oteststream.hpp"
#include <boost/contract/base_types.hpp>
#include <boost/contract/override.hpp>
#include <boost/contract/public_function.hpp>
#include <boost/contract/check.hpp>
#include <boost/contract/assert.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

class b {
    friend class boost::contract::access;

    static void static_invariant() { out << "b::static_inv" << std::endl; }
    void invariant() const { out << "b::inv" << std::endl; }

public:
    virtual void f(char ch, boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([&] {
                out << "b::f::pre" << std::endl;
                BOOST_CONTRACT_ASSERT(ch == 'b');
            })
            .old([] { out << "b::f::old" << std::endl; })
            .postcondition([] { out << "b::f::post" << std::endl; })
        ;
        out << "b::f::body" << std::endl;
    }
};

class a
    #define BASES public b
    : BASES
{
    friend class boost::contract::access;
    
    // Private base types.
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    // Private invariants.
    static void static_invariant() { out << "a::static_inv" << std::endl; }
    void invariant() const { out << "a::inv" << std::endl; }
    
    // Private override (always possible even when access is not friend).
    BOOST_CONTRACT_OVERRIDE(f)

public:
    virtual void f(char ch, boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<override_f>(
                v, &a::f, this, ch)
            .precondition([&] {
                out << "a::f::pre" << std::endl;
                BOOST_CONTRACT_ASSERT(ch == 'a');
            })
            .old([] { out << "a::f::old" << std::endl; })
            .postcondition([] { out << "a::f::post" << std::endl; })
        ;
        out << "a::f::body" << std::endl;
    }
};

int main() {
    std::ostringstream ok;

    a aa;
    out.str("");
    aa.f('a');
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "b::f::pre" << std::endl
            << "a::f::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::f::old" << std::endl
            << "a::f::old" << std::endl
        #endif
        << "a::f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::f::old" << std::endl
            << "b::f::post" << std::endl
            // No old call here because not a base object.
            << "a::f::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    b bb;
    out.str("");
    bb.f('b');
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "b::f::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::f::old" << std::endl
        #endif
        << "b::f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::f::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

