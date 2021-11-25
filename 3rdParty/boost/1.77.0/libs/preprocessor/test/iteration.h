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
# if !BOOST_PP_IS_ITERATING
#
# include <boost/preprocessor/cat.hpp>
# include <boost/preprocessor/comparison/equal.hpp>
# include <boost/preprocessor/control/expr_iif.hpp>
# include <boost/preprocessor/iteration.hpp>
# include <boost/preprocessor/logical/bitor.hpp>
# include <libs/preprocessor/test/test.h>
#
# define NO_FLAGS
#
# define BOOST_PP_ITERATION_PARAMS_1 (3, (1, 10, <libs/preprocessor/test/iteration.h>))
# include BOOST_PP_ITERATE()
#
# undef NO_FLAGS
#
# define BOOST_PP_ITERATION_PARAMS_1 (4, (1, 5, <libs/preprocessor/test/iteration.h>, 0x0001))
# include BOOST_PP_ITERATE()
#
# define BOOST_PP_ITERATION_PARAMS_1 (4, (1, 5, <libs/preprocessor/test/iteration.h>, 0x0002))
# include BOOST_PP_ITERATE()
#
# if BOOST_PP_LIMIT_ITERATION == 512
#
# define ITER100S(n)                       \
         BOOST_PP_BITOR                    \
            (                              \
            BOOST_PP_BITOR                 \
                (                          \
                BOOST_PP_EQUAL(n,100),     \
                BOOST_PP_EQUAL(n,200)      \
                ),                         \
            BOOST_PP_BITOR                 \
                (                          \
                BOOST_PP_BITOR             \
                    (                      \
                    BOOST_PP_EQUAL(n,300), \
                    BOOST_PP_EQUAL(n,400)  \
                    ),                     \
                BOOST_PP_EQUAL(n,500)      \
                )                          \
            )                              \
/* */
#
# define BOOST_PP_ITERATION_PARAMS_1 (4, (0, 512, <libs/preprocessor/test/iteration.h>, 0x0003))
# include BOOST_PP_ITERATE()
#
# undef ITER100S
#
# define ITER50S(n)                        \
         BOOST_PP_BITOR                    \
            (                              \
            BOOST_PP_BITOR                 \
                (                          \
                BOOST_PP_EQUAL(n,450),     \
                BOOST_PP_EQUAL(n,350)      \
                ),                         \
            BOOST_PP_BITOR                 \
                (                          \
                BOOST_PP_BITOR             \
                    (                      \
                    BOOST_PP_EQUAL(n,250), \
                    BOOST_PP_EQUAL(n,150)  \
                    ),                     \
                BOOST_PP_EQUAL(n,50)       \
                )                          \
            )                              \
/* */
#
# define BOOST_PP_ITERATION_PARAMS_1 (4, (512, 0, <libs/preprocessor/test/iteration.h>, 0x0004))
# include BOOST_PP_ITERATE()
#
# undef ITER50S
#
# endif
#
# if BOOST_PP_LIMIT_ITERATION == 1024
#
# define ITER100SA(n)                          \
         BOOST_PP_BITOR                        \
             (                                 \
             BOOST_PP_BITOR                    \
                (                              \
                BOOST_PP_BITOR                 \
                    (                          \
                    BOOST_PP_EQUAL(n,100),     \
                    BOOST_PP_EQUAL(n,200)      \
                    ),                         \
                BOOST_PP_BITOR                 \
                    (                          \
                    BOOST_PP_BITOR             \
                        (                      \
                        BOOST_PP_EQUAL(n,300), \
                        BOOST_PP_EQUAL(n,400)  \
                        ),                     \
                    BOOST_PP_EQUAL(n,500)      \
                    )                          \
                ),                             \
             BOOST_PP_BITOR                    \
                (                              \
                BOOST_PP_BITOR                 \
                    (                          \
                    BOOST_PP_EQUAL(n,600),     \
                    BOOST_PP_EQUAL(n,700)      \
                    ),                         \
                BOOST_PP_BITOR                 \
                    (                          \
                    BOOST_PP_BITOR             \
                        (                      \
                        BOOST_PP_EQUAL(n,800), \
                        BOOST_PP_EQUAL(n,900)  \
                        ),                     \
                    BOOST_PP_EQUAL(n,1000)     \
                    )                          \
                )                              \
             )                                 \
/* */
#
# define BOOST_PP_ITERATION_PARAMS_1 (4, (0, 1024, <libs/preprocessor/test/iteration.h>, 0x0005))
# include BOOST_PP_ITERATE()
#
# undef ITER100SA
#
# define ITER50SA(n)                           \
         BOOST_PP_BITOR                        \
             (                                 \
             BOOST_PP_BITOR                    \
                (                              \
                BOOST_PP_BITOR                 \
                    (                          \
                    BOOST_PP_EQUAL(n,950),     \
                    BOOST_PP_EQUAL(n,850)      \
                    ),                         \
                BOOST_PP_BITOR                 \
                    (                          \
                    BOOST_PP_BITOR             \
                        (                      \
                        BOOST_PP_EQUAL(n,750), \
                        BOOST_PP_EQUAL(n,650)  \
                        ),                     \
                    BOOST_PP_EQUAL(n,550)       \
                    )                          \
                ),                             \
             BOOST_PP_BITOR                    \
                (                              \
                BOOST_PP_BITOR                 \
                    (                          \
                    BOOST_PP_EQUAL(n,450),     \
                    BOOST_PP_EQUAL(n,350)      \
                    ),                         \
                BOOST_PP_BITOR                 \
                    (                          \
                    BOOST_PP_BITOR             \
                        (                      \
                        BOOST_PP_EQUAL(n,250), \
                        BOOST_PP_EQUAL(n,150)  \
                        ),                     \
                    BOOST_PP_EQUAL(n,50)       \
                    )                          \
                )                              \
             )                                 \
/* */
#
# define BOOST_PP_ITERATION_PARAMS_1 (4, (1024, 0, <libs/preprocessor/test/iteration.h>, 0x0006))
# include BOOST_PP_ITERATE()
#
# undef ITER50SA
#
# endif
#
# else
# if defined NO_FLAGS

struct BOOST_PP_CAT(X, BOOST_PP_ITERATION()) {
    BEGIN
        BOOST_PP_ITERATION() >= BOOST_PP_ITERATION_START() &&
        BOOST_PP_ITERATION() <= BOOST_PP_ITERATION_FINISH()
    END
};

# elif BOOST_PP_ITERATION_DEPTH() == 1 \
    && BOOST_PP_ITERATION_FLAGS() == 0x0001

struct BOOST_PP_CAT(Y, BOOST_PP_ITERATION()) { };

# elif BOOST_PP_ITERATION_DEPTH() == 1 \
    && BOOST_PP_ITERATION_FLAGS() == 0x0002

# define BOOST_PP_ITERATION_PARAMS_2 (3, (1, BOOST_PP_ITERATION(), <libs/preprocessor/test/iteration.h>))
# include BOOST_PP_ITERATE()

# elif BOOST_PP_ITERATION_DEPTH() == 1 \
    && BOOST_PP_ITERATION_FLAGS() == 0x0003
    
#define TEST_ITER_F(n)                         \
    BOOST_PP_EXPR_IIF                          \
        (                                      \
        ITER100S(n),                           \
        struct BOOST_PP_CAT(iter_512_f,n) { }; \
        )                                      \
/* */

TEST_ITER_F(BOOST_PP_ITERATION())

# undef TEST_ITER_F

# elif BOOST_PP_ITERATION_DEPTH() == 1 \
    && BOOST_PP_ITERATION_FLAGS() == 0x0004

#define TEST_ITER_B(n)                         \
    BOOST_PP_EXPR_IIF                          \
        (                                      \
        ITER50S(n),                            \
        struct BOOST_PP_CAT(iter_512_b,n) { }; \
        )                                      \
/* */

TEST_ITER_B(BOOST_PP_ITERATION())

# undef TEST_ITER_B

# elif BOOST_PP_ITERATION_DEPTH() == 1 \
    && BOOST_PP_ITERATION_FLAGS() == 0x0005
    
#define TEST_ITER_FA(n)                        \
    BOOST_PP_EXPR_IIF                          \
        (                                      \
        ITER100SA(n),                          \
        struct BOOST_PP_CAT(iter_1024_f,n) {}; \
        )                                      \
/* */

TEST_ITER_FA(BOOST_PP_ITERATION())

# undef TEST_ITER_FA

# elif BOOST_PP_ITERATION_DEPTH() == 1 \
    && BOOST_PP_ITERATION_FLAGS() == 0x0006

#define TEST_ITER_BA(n)                        \
    BOOST_PP_EXPR_IIF                          \
        (                                      \
        ITER50SA(n),                           \
        struct BOOST_PP_CAT(iter_1024_b,n) {}; \
        )                                      \
/* */

TEST_ITER_BA(BOOST_PP_ITERATION())

# undef TEST_ITER_BA

# elif BOOST_PP_ITERATION_DEPTH() == 2 \
    && BOOST_PP_FRAME_FLAGS(1) == 0x0002

struct BOOST_PP_CAT(Z, BOOST_PP_CAT(BOOST_PP_ITERATION(), BOOST_PP_RELATIVE_ITERATION(1))) { };

# else
#
# error should not get here!
#
# endif
# endif
