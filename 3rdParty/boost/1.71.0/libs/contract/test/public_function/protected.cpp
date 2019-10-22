
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test overriding function never overrides protected function contract.

#include "../detail/oteststream.hpp"
#include <boost/contract/function.hpp>
#include <boost/contract/public_function.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/check.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct b {
    static void static_invariant() { out << "b::static_inv" << std::endl; }
    void invariant() const { out << "b::inv" << std::endl; }

protected:
    // NOTE: This is the correct way of programming contracts for overridden
    // protected and overriding public functions: Both must use virtual_
    // (otherwise C++ won't override because mismatching parameters), but
    // overridden protected does not use public_function.
    virtual void f(boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::function()
            .precondition([] { out << "b::f::pre" << std::endl; })
            .old([] { out << "b::f::old" << std::endl; })
            .postcondition([] { out << "b::f::post" << std::endl; })
        ;
        out << "b::f::body" << std::endl;
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

    void f(boost::contract::virtual_* v = 0) /* not override */ {
        // C++ func a::f overrides b::f, but contracts don't (so no override_f).
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([] { out << "a::f::pre" << std::endl; })
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
    aa.f();
    ok.str(""); ok
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
    BOOST_TEST(out.eq(ok.str()));
    
    return boost::report_errors();
}

