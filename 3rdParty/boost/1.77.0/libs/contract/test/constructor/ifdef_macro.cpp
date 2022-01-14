
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test contract compilation on/off (using macro interface).

#include "../detail/oteststream.hpp"
#include "../detail/unprotected_commas.hpp"
#include <boost/contract/core/config.hpp>
#include <boost/contract/constructor.hpp> // Outside #if below for ctor pre.
#include <boost/contract_macro.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;

struct b :
    private boost::contract::constructor_precondition<b> // OK, always in code.
{
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

    explicit b(int x) :
        BOOST_CONTRACT_CONSTRUCTOR_PRECONDITION(b)([] {
            boost::contract::test::detail::unprotected_commas<void, void, void>
                    ::call();
            out << "b::ctor::pre" << std::endl;
        })
    {
        BOOST_CONTRACT_OLD_PTR(
            boost::contract::test::detail::unprotected_commas<int, void,
                    void>::type1
        )(
            old_x,
            (boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(x))
        );
        BOOST_CONTRACT_CONSTRUCTOR(this)
            BOOST_CONTRACT_OLD([] {
                boost::contract::test::detail::unprotected_commas<
                        void, void, void>::call();
                out << "b::f::old" << std::endl;
            })
            BOOST_CONTRACT_POSTCONDITION([] {
                boost::contract::test::detail::unprotected_commas<
                        void, void, void>::call();
                out << "b::ctor::post" << std::endl;
            })
        ;
        out << "b::ctor::body" << std::endl;
    }
};

struct a:
    private boost::contract::constructor_precondition<a>, // OK, always in code.
    public b
{
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

    explicit a(int x) :
        BOOST_CONTRACT_CONSTRUCTOR_PRECONDITION(a)([] {
            boost::contract::test::detail::unprotected_commas<void, void, void>
                    ::call();
            out << "a::ctor::pre" << std::endl; }
        ),
        b(x)
    {
        BOOST_CONTRACT_OLD_PTR(
            boost::contract::test::detail::unprotected_commas<int, void,
                    void>::type1
        )(
            old_x,
            (boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(x))
        );
        BOOST_CONTRACT_CONSTRUCTOR(boost::contract::test::detail::
                unprotected_commas<void, void, void>::same(this))
            BOOST_CONTRACT_OLD([] {
                boost::contract::test::detail::unprotected_commas<
                        void, void, void>::call();
                out << "a::f::old" << std::endl;
            })
            BOOST_CONTRACT_POSTCONDITION([] {
                boost::contract::test::detail::unprotected_commas<
                        void, void, void>::call();
                out << "a::ctor::post" << std::endl;
            })
        ;
        out << "a::ctor::body" << std::endl;
    }
};

int main() {
    std::ostringstream ok;
    out.str("");
    a aa(123);
    ok.str(""); ok
        #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
            << "a::ctor::pre" << std::endl
            << "b::ctor::pre" << std::endl
        #endif
        
        #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
            << "b::static_inv" << std::endl
        #endif
        #ifndef BOOST_CONTRACT_NO_OLDS
            << "b::f::old" << std::endl
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
            << "a::f::old" << std::endl
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
    return boost::report_errors();
}

