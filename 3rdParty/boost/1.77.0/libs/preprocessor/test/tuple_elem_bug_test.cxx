# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Edward Diener 2014.
#  *     Distributed under the Boost Software License, Version 1.0. (See
#  *     accompanying file LICENSE_1_0.txt or copy at
#  *     http://www.boost.org/LICENSE_1_0.txt)
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# include <boost/preprocessor/cat.hpp>
# include <boost/preprocessor/control/if.hpp>
# include <boost/preprocessor/tuple.hpp>
# include <libs/preprocessor/test/test.h>

#define TN_GEN_ONE(p) (1)
#define TN_GEN_ZERO(p) (0)
#define TN_TEST_ONE_MORE(parameter,ens) \
        BOOST_PP_IF \
            ( \
            BOOST_PP_CAT(BOOST_PP_TUPLE_ELEM(1,0,ens),0), \
            TN_GEN_ONE, \
            TN_GEN_ZERO \
            ) \
        (parameter) \
/**/
#define TN_TEST_ONE(parameter,ens) \
    BOOST_PP_TUPLE_ELEM \
        ( \
        1, \
        0, \
        TN_TEST_ONE_MORE(parameter,ens) \
        ) \
/**/

BEGIN TN_TEST_ONE(A,(1)) == 1 END
BEGIN TN_TEST_ONE(A,()) == 0 END
