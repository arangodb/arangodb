
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test contract compilation on/off.

#include "../detail/oteststream.hpp"
#include <boost/contract/core/config.hpp>
#include <boost/contract/core/virtual.hpp>
#ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
    #include <boost/contract/public_function.hpp>
    #include <boost/contract/base_types.hpp>
    #include <boost/contract/override.hpp>
    #include <boost/contract/check.hpp>
    #include <boost/contract/old.hpp>
#endif
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct b {
    #ifndef BOOST_CONTRACT_NO_INVARIANTS
        static void static_invariant() { out << "b::static_inv" << std::endl; }
        void invariant() const { out << "b::inv" << std::endl; }
    #endif

    virtual void f(int x, boost::contract::virtual_* v = 0) = 0;
};

void b::f(int x, boost::contract::virtual_* v) {
    #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
        boost::contract::old_ptr<int> old_x = BOOST_CONTRACT_OLDOF(v, x);
    #endif
    #ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
        boost::contract::check c = boost::contract::public_function(v, this)
            #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                .precondition([] { out << "b::f::pre" << std::endl; })
            #endif
            #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                .old([] { out << "b::f::old" << std::endl; })
                .postcondition([] { out << "b::f::post" << std::endl; })
            #endif
        ;
    #endif
    out << "b::f::body" << std::endl;
}

struct a
    #define BASES public b
    : BASES
{
    #ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
        typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
        BOOST_CONTRACT_OVERRIDE(f)
    #endif

    #ifndef BOOST_CONTRACT_NO_INVARIANTS
        static void static_invariant() { out << "a::static_inv" << std::endl; }
        void invariant() const { out << "a::inv" << std::endl; }
    #endif

    virtual void f(int x, boost::contract::virtual_* v = 0) {
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            boost::contract::old_ptr<int> old_x = BOOST_CONTRACT_OLDOF(v, x);
        #endif
        #ifndef BOOST_CONTRACT_NO_PUBLIC_FUNCTIONS
            boost::contract::check c = boost::contract::public_function<
                    override_f>(v, &a::f, this, x)
                #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
                    .precondition([] { out << "a::f::pre" << std::endl; })
                #endif
                #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                    .old([] { out << "a::f::old" << std::endl; })
                    .postcondition([] { out << "a::f::post" << std::endl; })
                #endif
            ;
        #endif
        out << "a::f::body" << std::endl;
    }
};

int main() {
    std::ostringstream ok;
    
    a aa;
    out.str("");
    aa.f(123);
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "b::f::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
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
            << "a::f::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

