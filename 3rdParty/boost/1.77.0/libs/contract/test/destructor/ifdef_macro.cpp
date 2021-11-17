
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test contract compilation on/off (using macro interface).

#include "../detail/oteststream.hpp"
#include "../detail/unprotected_commas.hpp"
#include <boost/contract/core/config.hpp>
#include <boost/contract_macro.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct b {
    BOOST_CONTRACT_STATIC_INVARIANT({
        boost::contract::test::detail::unprotected_commas<void, void, void>::
                call();
        out << "b::static_inv" << std::endl;
    })
    
    BOOST_CONTRACT_INVARIANT({
        boost::contract::test::detail::unprotected_commas<void, void, void>::
                call();
        out << "b::inv" << std::endl;
    })

    virtual ~b() {
        BOOST_CONTRACT_OLD_PTR(
            boost::contract::test::detail::unprotected_commas<int, void,
                    void>::type1
        )(
            old_y,
            (boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(y))
        );
        BOOST_CONTRACT_DESTRUCTOR(boost::contract::test::detail::
                unprotected_commas<void, void, void>::same(this))
            BOOST_CONTRACT_OLD([] {
                boost::contract::test::detail::unprotected_commas<
                        void, void, void>::call();
                out << "b::dtor::old" << std::endl;
            })
            BOOST_CONTRACT_POSTCONDITION([] {
                boost::contract::test::detail::unprotected_commas<
                        void, void, void>::call();
                out << "b::dtor::post" << std::endl;
            })
        ;
        out << "b::dtor::body" << std::endl;
    }
    
    static int y;
};
int b::y = 0;

struct a : public b {
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

    virtual ~a() {
        BOOST_CONTRACT_OLD_PTR(
            boost::contract::test::detail::unprotected_commas<int, void, void>::
                    type1
        )(
            old_x,
            (boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(x))
        );
        BOOST_CONTRACT_DESTRUCTOR(boost::contract::test::detail::
                unprotected_commas<void, void, void>::same(this))
            BOOST_CONTRACT_OLD([] {
                boost::contract::test::detail::unprotected_commas<
                        void, void, void>::call();
                out << "a::dtor::old" << std::endl;
            })
            BOOST_CONTRACT_POSTCONDITION([] {
                boost::contract::test::detail::unprotected_commas<
                        void, void, void>::call();
                out << "a::dtor::post" << std::endl;
            })
        ;
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
        #ifndef BOOST_CONTRACT_NO_OLDS
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
        #ifndef BOOST_CONTRACT_NO_OLDS
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

