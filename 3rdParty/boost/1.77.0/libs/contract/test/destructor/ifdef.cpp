
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test contract compilation on/off.

#include "../detail/oteststream.hpp"
#include <boost/contract/core/config.hpp>
#ifndef BOOST_CONTRACT_NO_DESTRUCTORS
    #include <boost/contract/destructor.hpp>
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

    virtual ~b() {
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            boost::contract::old_ptr<int> old_y = BOOST_CONTRACT_OLDOF(y);
        #endif
        #ifndef BOOST_CONTRACT_NO_DESTRUCTORS
            boost::contract::check c = boost::contract::destructor(this)
                #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                    .old([] { out << "b::dtor::old" << std::endl; })
                    .postcondition([] { out << "b::dtor::post" << std::endl; })
                #endif
            ;
        #endif
        out << "b::dtor::body" << std::endl;
    }
    
    static int y;
};
int b::y = 0;

struct a : public b {
    #ifndef BOOST_CONTRACT_NO_INVARIANTS
        static void static_invariant() { out << "a::static_inv" << std::endl; }
        void invariant() const { out << "a::inv" << std::endl; }
    #endif

    virtual ~a() {
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            boost::contract::old_ptr<int> old_x = BOOST_CONTRACT_OLDOF(x);
        #endif
        #ifndef BOOST_CONTRACT_NO_DESTRUCTORS
            boost::contract::check c = boost::contract::destructor(this)
                #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
                    .old([] { out << "a::dtor::old" << std::endl; })
                    .postcondition([] { out << "a::dtor::post" << std::endl; })
                #endif
            ;
        #endif
        out << "a::dtor::body" << std::endl;
    }
    
    static int x;
};
int a::x = 0;

int main() {
    std::ostringstream ok;
    {
        a aa;
        out.str("");
    }
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "a::dtor::old" << std::endl
        #endif
        << "a::dtor::body" << std::endl
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
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::dtor::old" << std::endl
        #endif
        << "b::dtor::body" << std::endl
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "b::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::dtor::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));
    return boost::report_errors();
}

