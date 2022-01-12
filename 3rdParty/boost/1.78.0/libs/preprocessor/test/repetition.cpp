# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Paul Mensonides 2002.
#  *     Distributed under the Boost Software License, Version 1.0. (See
#  *     accompanying file LICENSE_1_0.txt or copy at
#  *     http://www.boost.org/LICENSE_1_0.txt)
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# include <boost/preprocessor/config/limits.hpp>
# include <boost/preprocessor/arithmetic/inc.hpp>
# include <boost/preprocessor/cat.hpp>
# include <boost/preprocessor/comparison/equal.hpp>
# include <boost/preprocessor/comparison/not_equal.hpp>
# include <boost/preprocessor/control/iif.hpp>
# include <boost/preprocessor/facilities/intercept.hpp>
# include <boost/preprocessor/logical/bitor.hpp>
# include <boost/preprocessor/repetition.hpp>
# include <libs/preprocessor/test/test.h>

# define MAX 10

# define NTH(z, n, data) data ## n

int add(BOOST_PP_ENUM_PARAMS(MAX, int x)) {
    return BOOST_PP_REPEAT(MAX, NTH, + x);
}

const int r = add(BOOST_PP_ENUM_PARAMS(MAX, 1 BOOST_PP_INTERCEPT));

# define CONSTANT(z, n, text) BOOST_PP_CAT(text, n) = n
const int BOOST_PP_ENUM(MAX, CONSTANT, default_param_);

# define TEST(n) \
    void BOOST_PP_CAT(test_enum_params, n)(BOOST_PP_ENUM_PARAMS(n, int x)); \
    void BOOST_PP_CAT(test_enum_params_with_a_default, n)(BOOST_PP_ENUM_PARAMS_WITH_A_DEFAULT(n, int x, 0)); \
    void BOOST_PP_CAT(test_enum_params_with_defaults, n)(BOOST_PP_ENUM_PARAMS_WITH_DEFAULTS(n, int x, default_param_));

TEST(0)
TEST(MAX)

template<BOOST_PP_ENUM_PARAMS(MAX, class T)> struct no_rescan;

# define F1(z, n, p) p n
BEGIN 1 + (4+5+6) BOOST_PP_REPEAT_FROM_TO(4, 7, F1, -) END

# if BOOST_PP_LIMIT_REPEAT == 512

# define RP512M(z,n,data)                      \
    struct BOOST_PP_CAT(data,BOOST_PP_INC(n)); \
/* */

BOOST_PP_REPEAT_FROM_TO(508,512,RP512M,r512_t)

#endif

# if BOOST_PP_LIMIT_REPEAT == 1024

# define RP1024M(z,n,data)                      \
    struct BOOST_PP_CAT(data,BOOST_PP_INC(n)); \
/* */

BOOST_PP_REPEAT_FROM_TO(1010,1024,RP1024M,r1024_t)

#endif

# define PRED(r, state) BOOST_PP_NOT_EQUAL(state, BOOST_PP_INC(MAX))
# define OP(r, state) BOOST_PP_INC(state)
# define MACRO(r, state) BOOST_PP_COMMA_IF(BOOST_PP_NOT_EQUAL(state, 1)) BOOST_PP_CAT(class T, state)

template<BOOST_PP_FOR(1, PRED, OP, MACRO)> struct for_test;

# if BOOST_PP_LIMIT_FOR == 512

# define PRED512(r, state) BOOST_PP_NOT_EQUAL(state, 512)

# define MACRO512_NUL(state)

# define MACRO512_OUT(state) struct BOOST_PP_CAT(f_512t,state);

# define MACRO512(r, state)                    \
    BOOST_PP_IIF                               \
        (                                      \
        BOOST_PP_BITOR                         \
            (                                  \
            BOOST_PP_BITOR                     \
                (                              \
                BOOST_PP_EQUAL(state,100),     \
                BOOST_PP_EQUAL(state,200)      \
                ),                             \
            BOOST_PP_BITOR                     \
                (                              \
                BOOST_PP_BITOR                 \
                    (                          \
                    BOOST_PP_EQUAL(state,300), \
                    BOOST_PP_EQUAL(state,400)  \
                    ),                         \
                BOOST_PP_EQUAL(state,500)      \
                )                              \
            ),                                 \
        MACRO512_OUT,                          \
        MACRO512_NUL                           \
        )                                      \
    (state)                                    \
/* */

BOOST_PP_FOR(0, PRED512, OP, MACRO512)

#endif

# if BOOST_PP_LIMIT_FOR == 1024

# define PRED1024(r, state) BOOST_PP_NOT_EQUAL(state, 1024)

# define MACRO1024_NUL(state)

# define MACRO1024_OUT(state) struct BOOST_PP_CAT(f_1024t,state);

# define MACRO1024(r, state)                       \
    BOOST_PP_IIF                                   \
        (                                          \
        BOOST_PP_BITOR                             \
            (                                      \
            BOOST_PP_BITOR                         \
                (                                  \
                BOOST_PP_BITOR                     \
                    (                              \
                    BOOST_PP_EQUAL(state,100),     \
                    BOOST_PP_EQUAL(state,200)      \
                    ),                             \
                BOOST_PP_BITOR                     \
                    (                              \
                    BOOST_PP_BITOR                 \
                        (                          \
                        BOOST_PP_EQUAL(state,300), \
                        BOOST_PP_EQUAL(state,400)  \
                        ),                         \
                    BOOST_PP_EQUAL(state,500)      \
                    )                              \
                ),                                 \
            BOOST_PP_BITOR                         \
                (                                  \
                BOOST_PP_BITOR                     \
                    (                              \
                    BOOST_PP_EQUAL(state,600),     \
                    BOOST_PP_EQUAL(state,700)      \
                    ),                             \
                BOOST_PP_BITOR                     \
                    (                              \
                    BOOST_PP_BITOR                 \
                        (                          \
                        BOOST_PP_EQUAL(state,800), \
                        BOOST_PP_EQUAL(state,900)  \
                        ),                         \
                    BOOST_PP_EQUAL(state,1000)     \
                    )                              \
                )                                  \
            ),                                     \
        MACRO1024_OUT,                             \
        MACRO1024_NUL                              \
        )                                          \
    (state)                                        \
/* */

BOOST_PP_FOR(0, PRED1024, OP, MACRO1024)

#endif
