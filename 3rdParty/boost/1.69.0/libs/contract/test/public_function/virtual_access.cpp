
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test overrides with mixed access level from (single) base.

#include "../detail/oteststream.hpp"
#include <boost/contract/public_function.hpp>
#include <boost/contract/function.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/override.hpp>
#include <boost/contract/check.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct b { // Test all access levels (public, protected, and private).
    friend void call(b& me) { // Test polymorphic calls (object by &).
        me.f();
        me.g();
        me.h();
    }

    static void statci_inv() { out << "b::static_inv" << std::endl; }
    void invariant() const { out << "b::inv" << std::endl; }

    virtual void f(boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([] { out << "b::f::pre" << std::endl; })
            .old([] { out << "b::f::old" << std::endl; })
            .postcondition([] { out << "b::f::post" << std::endl; })
        ;
        out << "b::f::body" << std::endl;
    }

    // NOTE: Both protected and private virtual members must declare
    // extra `virtual_* = 0` parameter (otherwise they cannot be overridden in
    // derived classes with contracts because C++ uses also default parameters
    // to match signature of overriding functions).

protected:
    virtual void g(boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::function()
            .precondition([] { out << "b::g::pre" << std::endl; })
            .old([] { out << "b::g::old" << std::endl; })
            .postcondition([] { out << "b::g::post" << std::endl; })
        ;
        out << "b::g::body" << std::endl;
    }

private:
    virtual void h(boost::contract::virtual_* v = 0) {
        boost::contract::check c = boost::contract::function()
            .precondition([] { out << "b::h::pre" << std::endl; })
            .old([] { out << "b::h::old" << std::endl; })
            .postcondition([] { out << "b::h::post" << std::endl; })
        ;
        out << "b::h::body" << std::endl;
    }
};

struct a // Test overrides with mixed access levels from base.
    #define BASES public b
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES
    
    static void statci_inv() { out << "a::static_inv" << std::endl; }
    void invariant() const { out << "a::inv" << std::endl; }

    virtual void f(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function<override_f>(
                v, &a::f, this)
            .precondition([] { out << "a::f::pre" << std::endl; })
            .old([] { out << "a::f::old" << std::endl; })
            .postcondition([] { out << "a::f::post" << std::endl; })
        ;
        out << "a::f::body" << std::endl;
    }
    BOOST_CONTRACT_OVERRIDES(f);

    // Following do not override public members so no `override_...` param and
    // they do not actually subcontract.

    virtual void g(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([] { out << "a::g::pre" << std::endl; })
            .old([] { out << "a::g::old" << std::endl; })
            .postcondition([] { out << "a::g::post" << std::endl; })
        ;
        out << "a::g::body" << std::endl;
    }
    
    virtual void h(boost::contract::virtual_* v = 0) /* override */ {
        boost::contract::check c = boost::contract::public_function(v, this)
            .precondition([] { out << "a::h::pre" << std::endl; })
            .old([] { out << "a::h::old" << std::endl; })
            .postcondition([] { out << "a::h::post" << std::endl; })
        ;
        out << "a::h::body" << std::endl;
    }
};

int main() {
    std::ostringstream ok;

    b bb;
    out.str("");
    call(bb);
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
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
            << "b::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::f::post" << std::endl
        #endif

        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "b::g::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::g::old" << std::endl
        #endif
        << "b::g::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::g::post" << std::endl
        #endif

        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "b::h::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::h::old" << std::endl
        #endif
        << "b::h::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::h::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));
    
    a aa;
    out.str("");
    call(aa);
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "b::f::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::f::old" << std::endl
            << "a::f::old" << std::endl
        #endif
        << "a::f::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "b::inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::f::old" << std::endl
            << "b::f::post" << std::endl
            << "a::f::post" << std::endl
        #endif

        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "a::g::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "a::g::old" << std::endl
        #endif
        << "a::g::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "a::g::post" << std::endl
        #endif

        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "a::h::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "a::h::old" << std::endl
        #endif
        << "a::h::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "a::h::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

