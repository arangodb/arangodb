
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test contract compilation on/off (using macro interface).

#include "../detail/oteststream.hpp"
#include "../detail/unprotected_commas.hpp"
#include <boost/contract/core/config.hpp>
#include <boost/contract/core/virtual.hpp>
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

    virtual void f(int x, boost::contract::virtual_* v = 0) = 0;
};

void b::f(int x, boost::contract::virtual_* v) {
    BOOST_CONTRACT_OLD_PTR(
        boost::contract::test::detail::unprotected_commas<int, void, void>::
                type1
    )(
        (boost::contract::test::detail::unprotected_commas<void, void, void>::
                same(v)),
        old_x,
        (boost::contract::test::detail::unprotected_commas<void, void, void>::
                same(x))
    );
    BOOST_CONTRACT_PUBLIC_FUNCTION(
        boost::contract::test::detail::unprotected_commas<void, void, void>::
                same(v),
        boost::contract::test::detail::unprotected_commas<void, void, void>::
                same(this)
    )
        BOOST_CONTRACT_PRECONDITION([] {
            boost::contract::test::detail::unprotected_commas<
                    void, void, void>::call();
            out << "b::f::pre" << std::endl;
        })
        BOOST_CONTRACT_OLD([] {
            boost::contract::test::detail::unprotected_commas<
                    void, void, void>::call();
            out << "b::f::old" << std::endl;
        })
        BOOST_CONTRACT_POSTCONDITION([] {
            boost::contract::test::detail::unprotected_commas<
                    void, void, void>::call();
            out << "b::f::post" << std::endl;
        })
    ;
    out << "b::f::body" << std::endl;
}

struct a
    #define BASES public boost::contract::test::detail::unprotected_commas< \
            b, void, void>::type1
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    BOOST_CONTRACT_OVERRIDE(f)

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

    virtual void f(int x, boost::contract::virtual_* v = 0) {
        BOOST_CONTRACT_OLD_PTR(
            boost::contract::test::detail::unprotected_commas<int, void, void>::
                    type1
        )(
            (boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(v)),
            old_x,
            (boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(x))
        );
        BOOST_CONTRACT_PUBLIC_FUNCTION_OVERRIDE(
            boost::contract::test::detail::unprotected_commas<override_f, void,
                    void>::type1
        )(
            boost::contract::test::detail::unprotected_commas<void, void, void>
                    ::same(v),
            &a::f,
            boost::contract::test::detail::unprotected_commas<void, void, void>
                    ::same(this),
            boost::contract::test::detail::unprotected_commas<void, void, void>
                    ::same(x)
        )
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
    
    a aa;
    out.str("");
    aa.f(123);
    ok.str(); ok
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
            << "a::static_inv" << std::endl
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
            << "b::static_inv" << std::endl
            << "b::inv" << std::endl
            << "a::static_inv" << std::endl
            << "a::inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            << "b::f::old" << std::endl // Called by post (so under NO_POST).
            << "b::f::post" << std::endl
            << "a::f::post" << std::endl
        #endif
    ;
    BOOST_TEST(out.eq(ok.str()));

    return boost::report_errors();
}

