
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test making all contract extra declarations (base types, inv, etc.) private.

#include "../detail/oteststream.hpp"
#include <boost/contract/constructor.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/check.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

class b
    #define BASES private boost::contract::constructor_precondition<b>
    : BASES
{
    friend class boost::contract::access;

    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    static void static_invariant() { out << "b::static_inv" << std::endl; }
    void invariant() const { out << "b::inv" << std::endl; }

public:
    b() : boost::contract::constructor_precondition<b>([] {
        out << "b::ctor::pre" << std::endl;
    }) {
        boost::contract::check c = boost::contract::constructor(this)
            .old([] { out << "b::ctor::old" << std::endl; })
            .postcondition([] { out << "b::ctor::post" << std::endl; })
        ;
        out << "b::ctor::body" << std::endl;
    }
};

class a
    #define BASES private boost::contract::constructor_precondition<a>, public b
    : BASES
{
    friend class boost::contract::access;
    
    // Private base types (always OK because never used by ctors).
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    // Private invariants.
    static void static_invariant() { out << "a::static_inv" << std::endl; }
    void invariant() const { out << "a::inv" << std::endl; }
    
public:
    a() : boost::contract::constructor_precondition<a>([] {
        out << "a::ctor::pre" << std::endl;
    }) {
        boost::contract::check c = boost::contract::constructor(this)
            .old([] { out << "a::ctor::old" << std::endl; })
            .postcondition([] { out << "a::ctor::post" << std::endl; })
        ;
        out << "a::ctor::body" << std::endl;
    }
};

int main() {
    std::ostringstream ok;

    out.str("");
    a aa;
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "a::ctor::pre" << std::endl
            << "b::ctor::pre" << std::endl
        #endif

        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::ctor::old" << std::endl
        #endif
        << "b::ctor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::ctor::post" << std::endl
        #endif

        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "a::ctor::old" << std::endl
        #endif
        << "a::ctor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "a::ctor::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    out.str("");
    b bb;
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "b::ctor::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::ctor::old" << std::endl
        #endif
        << "b::ctor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::ctor::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

