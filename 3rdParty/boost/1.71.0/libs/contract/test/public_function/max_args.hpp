
// Copyright (C) 2008-2018 Lorenzo Caminiti
// Distributed under the Boost Software License, Version 1.0 (see accompanying
// file LICENSE_1_0.txt or a copy at http://www.boost.org/LICENSE_1_0.txt).
// See: http://www.boost.org/doc/libs/release/libs/contract/doc/html/index.html

// Test max argument number for public function (with and without result).

#include "../detail/oteststream.hpp"
#include <boost/contract/public_function.hpp>
#include <boost/contract/check.hpp>
#include <boost/contract/base_types.hpp>
#include <boost/contract/override.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/preprocessor/arithmetic/inc.hpp>
#include <boost/preprocessor/control/expr_iif.hpp>
#include <boost/preprocessor/cat.hpp>
#include <boost/preprocessor/stringize.hpp>
#include <boost/detail/lightweight_test.hpp>
#include <sstream>

boost::contract::test::detail::oteststream out;
        
#define BOOST_CONTRACT_TEST_MAX_ARGS_PARAM_COMMA_(z, n, unused) \
    int BOOST_PP_CAT(a, n) ,

#define BOOST_CONTRACT_TEST_MAX_ARGS_COMMA_ARG_(z, n, unused) \
    , BOOST_PP_CAT(a, n)
    
#define BOOST_CONTRACT_TEST_MAX_ARGS_N_(z, n, unused) \
    n

struct b {
    static void static_invariant() { out << "b::static_inv" << std::endl; }
    void invariant() const { out << "b::inv" << std::endl; }

    #define BOOST_CONTRACT_TEST_MAX_ARGS_B_F_(z, n, unused) \
        virtual int BOOST_PP_CAT(f, n)( \
            BOOST_PP_REPEAT_ ## z( \
                    n, BOOST_CONTRACT_TEST_MAX_ARGS_PARAM_COMMA_, ~) \
            boost::contract::virtual_* v = 0 \
        ) { \
            int result = 0; \
            boost::contract::check c = boost::contract::public_function( \
                    v, result, this) \
                .precondition([] { \
                    out << "b::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << \
                            "::pre" << std::endl; \
                }) \
                .old([] { \
                    out << "b::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << \
                            "::old" << std::endl; \
                }) \
                .postcondition([] (int result) { \
                    out << "b::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << \
                            "::post" << std::endl; \
                }) \
            ; \
            out << "b::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << \
                    "::body" << std::endl; \
            return result; \
        }

    BOOST_PP_REPEAT(BOOST_PP_INC(BOOST_CONTRACT_MAX_ARGS),
            BOOST_CONTRACT_TEST_MAX_ARGS_B_F_, ~)
};

struct a
    #define BASES public b
    : BASES
{
    typedef BOOST_CONTRACT_BASE_TYPES(BASES) base_types;
    #undef BASES
    
    static void static_invariant() { out << "a::static_inv" << std::endl; }
    void invariant() const { out << "a::inv" << std::endl; }

    #define BOOST_CONTRACT_TEST_MAX_ARGS_A_F_(z, n, unused) \
        int BOOST_PP_CAT(f, n)( \
            BOOST_PP_REPEAT_ ## z( \
                    n, BOOST_CONTRACT_TEST_MAX_ARGS_PARAM_COMMA_, ~) \
            boost::contract::virtual_* v = 0 \
        ) /* override */ { \
            int result = 0; \
            boost::contract::check c = boost::contract::public_function< \
                BOOST_PP_CAT(override_, BOOST_PP_CAT(f, n)) \
            >( \
                v, result, &a::BOOST_PP_CAT(f, n), this \
                BOOST_PP_REPEAT_ ## z( \
                        n, BOOST_CONTRACT_TEST_MAX_ARGS_COMMA_ARG_, ~) \
            ) \
                .precondition([] { \
                    out << "a::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << \
                            "::pre" << std::endl; \
                }) \
                .old([] { \
                    out << "a::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << \
                            "::old" << std::endl; \
                }) \
                .postcondition([] (int result) { \
                    out << "a::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << \
                            "::post" << std::endl; \
                }) \
            ; \
            out << "a::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << \
                    "::body" << std::endl; \
            return result; \
        } \
        BOOST_CONTRACT_OVERRIDE(BOOST_PP_CAT(f, n))

    BOOST_PP_REPEAT(BOOST_PP_INC(BOOST_CONTRACT_MAX_ARGS),
            BOOST_CONTRACT_TEST_MAX_ARGS_A_F_, ~)
};

int main() {
    std::ostringstream ok;
    a aa;

    #ifndef BOOST_CONTRACT_NO_ENTRY_INVARIANTS
        #define BOOST_CONTRACT_TEST_entry_inv 1
    #else
        #define BOOST_CONTRACT_TEST_entry_inv 0
    #endif
    #ifndef BOOST_CONTRACT_NO_PRECONDITIONS
        #define BOOST_CONTRACT_TEST_pre 1
    #else
        #define BOOST_CONTRACT_TEST_pre 0
    #endif
    #ifndef BOOST_CONTRACT_NO_EXIT_INVARIANTS
        #define BOOST_CONTRACT_TEST_exit_inv 1
    #else
        #define BOOST_CONTRACT_TEST_exit_inv 0
    #endif
    #ifndef BOOST_CONTRACT_NO_POSTCONDITIONS
        #define BOOST_CONTRACT_TEST_post 1
    #else
        #define BOOST_CONTRACT_TEST_post 0
    #endif
    #ifndef BOOST_CONTRACT_NO_OLDS
        #define BOOST_CONTRACT_TEST_old 1
    #else
        #define BOOST_CONTRACT_TEST_old 0
    #endif
    
    #define BOOST_CONTRACT_TEST_MAX_ARGS_TEST_(z, n, unused) \
        out.str(""); \
        aa.BOOST_PP_CAT(f, n)(BOOST_PP_ENUM_ ## z( \
                n, BOOST_CONTRACT_TEST_MAX_ARGS_N_, ~)); \
        ok.str(""); ok \
            BOOST_PP_EXPR_IIF(BOOST_CONTRACT_TEST_entry_inv, \
                << "b::static_inv\n" \
                << "b::inv\n"\
                << "a::static_inv\n" \
                << "a::inv\n" \
            ) \
            BOOST_PP_EXPR_IIF(BOOST_CONTRACT_TEST_pre, \
                << "b::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << \
                        "::pre\n" \
            ) \
            BOOST_PP_EXPR_IIF(BOOST_CONTRACT_TEST_old, \
                << "b::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << \
                        "::old\n" \
                << "a::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << \
                        "::old\n" \
            ) \
            << "a::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << "::body\n" \
            BOOST_PP_EXPR_IIF(BOOST_CONTRACT_TEST_exit_inv, \
                << "b::static_inv\n" \
                << "b::inv\n"\
                << "a::static_inv\n" \
                << "a::inv\n" \
            ) \
            BOOST_PP_EXPR_IIF(BOOST_CONTRACT_TEST_post, \
                << "b::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << \
                        "::old\n" \
                << "b::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << \
                        "::post\n" \
                << "a::" << BOOST_PP_STRINGIZE(BOOST_PP_CAT(f, n)) << \
                        "::post\n" \
            ) \
        ; \
        BOOST_TEST(out.eq(ok.str()));
    
    BOOST_PP_REPEAT(BOOST_PP_INC(BOOST_CONTRACT_MAX_ARGS),
            BOOST_CONTRACT_TEST_MAX_ARGS_TEST_, ~)
    
    #undef BOOST_CONTRACT_TEST_entry_inv
    #undef BOOST_CONTRACT_TEST_pre
    #undef BOOST_CONTRACT_TEST_exit_inv
    #undef BOOST_CONTRACT_TEST_post
    #undef BOOST_CONTRACT_TEST_old
    return boost::report_errors();
}

