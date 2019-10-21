
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test public static member function contract compilation on/off (w/ macros).

#include "../detail/oteststream.hpp"
#include "../detail/unprotected_commas.hpp"
#include <boost/contract/core/config.hpp>
#include <boost/contract_macro.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct a {
    BOOST_CONTRACT_STATIC_INVARIANT({
        boost::contract::test::detail::unprotected_commas<void, void, void>::
                call();
        out << "a::static_inv" << std::endl;
    })
    
    BOOST_CONTRACT_INVARIANT({
        boost::contract::test::detail::unprotected_commas<void, void, void>::
                call();
        out << "a::inv" << std::endl;
    })

    static void f(int x) {
        BOOST_CONTRACT_OLD_PTR(
            boost::contract::test::detail::unprotected_commas<int, void, void>::
                    type1
        )(
            old_x,
            (boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(x))
        );
        BOOST_CONTRACT_STATIC_PUBLIC_FUNCTION(boost::contract::test::detail::
                unprotected_commas<a, void, void>::type1)
            BOOST_CONTRACT_PRECONDITION([] {
                boost::contract::test::detail::unprotected_commas<
                        void, void, void>::call();
                out << "a::f::pre" << std::endl;
            })
            BOOST_CONTRACT_OLD([] {
                boost::contract::test::detail::unprotected_commas<
                        void, void, void>::call();
                out << "a::f::old" << std::endl;
            })
            BOOST_CONTRACT_POSTCONDITION([] {
                boost::contract::test::detail::unprotected_commas<
                        void, void, void>::call();
                out << "a::f::post" << std::endl;
            })
        ;
        out << "a::f::body" << std::endl;
    }
};

int main() {
    std::ostringstream ok;

    out.str("");
    a::f(123);
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "a::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "a::f::pre" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "a::f::old" << std::endl
        #endif
        << "a::f::body" << std::endl
        // Test no post (but still static inv) because body threw.
        #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
            << "a::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "a::f::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

