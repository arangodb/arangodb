
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test throw from public function (derived and bases) .pre().

#include "../detail/oteststream.hpp"
#include <boost/contract/public_function.hpp>
#include <boost/contract/assert.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/override.hpp>
#include <boost/contract/check.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct c_err {}; // Global decl so visible in MSVC10 lambdas.

struct c {
    static void static_invariant() { out << "c::static_inv" << std::endl; }
    void invariant() const { out << "c::inv" << std::endl; }
    
    virtual void f(boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([] {
                out << "c::f::pre" << std::endl;
                BOOST_CONTRACT_ASSERT(false); // To check derived pre.
            })
            .old([] { out << "c::f::old" << std::endl; })
            .postcondition([] { out << "c::f::post" << std::endl; })
            .except([] { out << "c::f::except" << std::endl; })
        ;
        out << "c::f::body" << std::endl;
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

    virtual void f(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<override_f>(
                v, &b::f, this)
            .precondition([] {
                out << "b::f::pre" << std::endl;
                BOOST_CONTRACT_ASSERT(false); // To check derived pre.
            })
            .old([] { out << "b::f::old" << std::endl; })
            .postcondition([] { out << "b::f::post" << std::endl; })
            .except([] { out << "b::f::except" << std::endl; })
        ;
        out << "b::f::body" << std::endl;
    }
    BOOST_CONTRACT_OVERRIDE(f)
};

struct a_err {}; // Global decl so visible in MSVC10 lambdas.

struct a
    #define BASES public b
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    static void static_invariant() { out << "a::static_inv" << std::endl; }
    void invariant() const { out << "a::inv" << std::endl; }

    void f(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<override_f>(
                v, &a::f, this)
            .precondition([] {
                out << "a::f::pre" << std::endl;
                throw a_err(); // Test this throws.
            })
            .old([] { out << "a::f::old" << std::endl; })
            .postcondition([] { out << "a::f::post" << std::endl; })
            .except([] { out << "a::f::except" << std::endl; })
        ;
        out << "a::f::body" << std::endl;
    }
    BOOST_CONTRACT_OVERRIDE(f)
};

int main() {
    std::ostringstream ok;

    boost::contract::set_precondition_failure(
            [] (boost::contract::from) { throw; });
    
    a aa;
    b& ba = aa; // Test as virtual call via polymorphism.
    try {
        out.str("");
        ba.f();
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                BOOST_TEST(false);
            } catch(a_err const&) {
        #endif
        ok.str(""); ok
            #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
                << "c::static_inv" << std::endl
                << "c::inv" << std::endl
                << "b::static_inv" << std::endl
                << "b::inv" << std::endl
                << "a::static_inv" << std::endl
                << "a::inv" << std::endl
            #endif
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                << "c::f::pre" << std::endl
                << "b::f::pre" << std::endl
                << "a::f::pre" << std::endl // Test this threw.
            #else
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
                    << "c::f::old" << std::endl
                    << "c::f::post" << std::endl
                    << "b::f::old" << std::endl
                    << "b::f::post" << std::endl
                    // No old call here because not a base object.
                    << "a::f::post" << std::endl
                #endif
            #endif
        ;
        BOOST_TEST(out.eq(ok.str()));
    } catch(...) { BOOST_TEST(false); }

    return boost::report_errors();
}

