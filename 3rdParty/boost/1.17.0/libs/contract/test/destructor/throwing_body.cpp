
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test throw from destructor body (in middle branch of inheritance tree).

#include "../detail/oteststream.hpp"
#include <boost/contract/destructor.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/check.hpp>
#include <boost/config.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct c {
    static void static_invariant() { out << "c::static_inv" << std::endl; }
    void invariant() const { out << "c::inv" << std::endl; }

    ~c() BOOST_NOEXCEPT_IF(false) {
        boost::contract::check c = boost::contract::destructor(this)
            .old([] { out << "c::dtor::old" << std::endl; })
            .postcondition([] { out << "c::dtor::post" << std::endl; })
            .except([] { out << "c::dtor::except" << std::endl; })
        ;
        out << "c::dtor::body" << std::endl;
        // Do not throw (from inheritance root).
    }
};

struct b_err {}; // Global decl so visible in MSVC10 lambdas.

struct b
    #define BASES public c
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    static void static_invariant() { out << "b::static_inv" << std::endl; }
    void invariant() const { out << "b::inv" << std::endl; }

    ~b() BOOST_NOEXCEPT_IF(false) {
        boost::contract::check c = boost::contract::destructor(this)
            .old([] { out << "b::dtor::old" << std::endl; })
            .postcondition([] { out << "b::dtor::post" << std::endl; })
            .except([] { out << "b::dtor::except" << std::endl; })
        ;
        out << "b::dtor::body" << std::endl;
        throw b_err(); // Test body throw (from inheritance mid branch).
    }
};

struct a
    #define BASES public b
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    static void static_invariant() { out << "a::static_inv" << std::endl; }
    void invariant() const { out << "a::inv" << std::endl; }

    ~a() BOOST_NOEXCEPT_IF(false) {
        boost::contract::check c = boost::contract::destructor(this)
            .old([] { out << "a::dtor::old" << std::endl; })
            .postcondition([] { out << "a::dtor::post" << std::endl; })
            .except([] { out << "a::dtor::except" << std::endl; })
        ;
        out << "a::dtor::body" << std::endl;
        // Do not throw (from inheritance leaf).
    }
};

int main() {
    std::ostringstream ok;

    try {
        {
            a aa;
            out.str("");
        }
        BOOST_TEST(false);
    } catch(b_err const&) {
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "a::static_inv" << std::endl
                << "a::inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "a::dtor::old" << std::endl
            #endif
            << "a::dtor::body" << std::endl
            // Test a destructed (so only static_inv and post, but no inv).
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "a::static_inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                << "a::dtor::post" << std::endl
            #endif

            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "b::static_inv" << std::endl
                << "b::inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "b::dtor::old" << std::endl
            #endif
            << "b::dtor::body" << std::endl // Test this threw.
            // Test b not destructed (so static_inv, inv, and except, no post).
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "b::static_inv" << std::endl
                << "b::inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_EXCEPTS
                << "b::dtor::except" << std::endl
            #endif
                
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "c::static_inv" << std::endl
                << "c::inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_OLDS
                << "c::dtor::old" << std::endl
            #endif
            << "c::dtor::body" << std::endl
            // Test c not destructed (so static_inv, inv, and except, no post).
            #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
                << "c::static_inv" << std::endl
                << "c::inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_EXCEPTS
                << "c::dtor::except" << std::endl
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    return boost::report_errors();
}

