
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test old values of non-copyable types (using macro interface).

#include "if_copyable.hpp"
#include "../detail/unprotected_commas.hpp"
#include <boost/contract_macro.hpp>
#include <boost/noncopyable.hpp>
#include <boost/detail/lightweight_test.hpp>

unsigned old_checks;

template<typename T>
struct b {
    virtual void next(T& x, boost::contract::virtual_* v = 0) {
        BOOST_CONTRACT_OLD_PTR_IF_COPYABLE(
            typename boost::contract::test::detail::unprotected_commas<T, void,
                    void>::type1
        )(
            (boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(v)),
            old_x,
            (boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(x))
        );
        BOOST_CONTRACT_PUBLIC_FUNCTION(
            boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(v),
            boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(this)
        )
            BOOST_CONTRACT_POSTCONDITION([&] {
                boost::contract::test::detail::unprotected_commas<
                        void, void, void>::call();
                if(old_x) {
                    BOOST_CONTRACT_ASSERT(x == *old_x + T(1));
                    ++old_checks;
                }
            })
        ;
        ++x;
    }
    BOOST_CONTRACT_OVERRIDE(next)
};

template<typename T>
struct a
    #define BASES public b<T>
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES

    virtual void next(T& x, boost::contract::virtual_* v = 0) /* override */ {
        BOOST_CONTRACT_OLD_PTR_IF_COPYABLE(
            typename boost::contract::test::detail::unprotected_commas<T, void,
                    void>::type1
        )(
            (boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(v)),
            old_x,
            (boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(x))
        );
        BOOST_CONTRACT_PUBLIC_FUNCTION_OVERRIDE(
            typename boost::contract::test::detail::unprotected_commas<
                    override_next, void, void>::type1
        )(
            boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(v),
            &a::next,
            boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(this),
            boost::contract::test::detail::unprotected_commas<void, void,
                    void>::same(x)
        )
            BOOST_CONTRACT_POSTCONDITION([&] {
                boost::contract::test::detail::unprotected_commas<
                        void, void, void>::call();
                if(old_x) {
                    BOOST_CONTRACT_ASSERT(x == *old_x + T(1));
                    ++old_checks;
                }
            })
        ;
        ++x;
    }
    BOOST_CONTRACT_OVERRIDE(next)
};

template<typename T>
void next(T& x) {
    BOOST_CONTRACT_OLD_PTR_IF_COPYABLE(
        typename boost::contract::test::detail::unprotected_commas<T, void,
                void>::type1
    )(
        old_x,
        (boost::contract::test::detail::unprotected_commas<void, void,
                void>::same(x))
    );
    BOOST_CONTRACT_FUNCTION()
        BOOST_CONTRACT_POSTCONDITION([&] {
            boost::contract::test::detail::unprotected_commas<void, void, void>
                    ::call();
            if(old_x) {
                BOOST_CONTRACT_ASSERT(x == *old_x + T(1));
                ++old_checks;
            }
        })
    ;
    ++x;
}
    
int main() {
    int i = 1; // Test built-in copyable type.
    cp c(1); // Test custom copyable type.
    ncp n(1); // Test non-copyable type.

    // Test free functions (old values without `v`).

    unsigned cnt =
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            1
        #else
            0
        #endif
    ;

    old_checks = 0;
    next(i);
    BOOST_TEST_EQ(old_checks, cnt);
    
    old_checks = 0;
    next(c);
    BOOST_TEST_EQ(old_checks, cnt);

    old_checks = 0;
    next(n);
    BOOST_TEST_EQ(old_checks, 0u);

    // Test virtual functions (old values with `v`).
    
    cnt =
        #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
            2
        #else
            0
        #endif
    ;

    a<int> ai;
    old_checks = 0;
    ai.next(i);
    BOOST_TEST_EQ(old_checks, cnt);
    
    a<cp> ac;
    old_checks = 0;
    ac.next(c);
    BOOST_TEST_EQ(old_checks, cnt);

    a<ncp> an;
    old_checks = 0;
    an.next(n);
    BOOST_TEST_EQ(old_checks, 0u);

    return boost::report_errors();
}

