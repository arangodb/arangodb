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
# include <boost/preprocessor/comparison.hpp>
# include <libs/preprocessor/test/test.h>

/* equality */

BEGIN BOOST_PP_EQUAL(2, 0) == 0 END
BEGIN BOOST_PP_EQUAL(2, 2) == 1 END

#if BOOST_PP_LIMIT_MAG == 512

BEGIN BOOST_PP_EQUAL(135, 487) == 0 END
BEGIN BOOST_PP_EQUAL(367, 367) == 1 END

#endif

#if BOOST_PP_LIMIT_MAG == 1024

BEGIN BOOST_PP_EQUAL(749, 832) == 0 END
BEGIN BOOST_PP_EQUAL(955, 955) == 1 END

#endif

/* inequality */

BEGIN BOOST_PP_NOT_EQUAL(2, 0) == 1 END
BEGIN BOOST_PP_NOT_EQUAL(2, 2) == 0 END

#if BOOST_PP_LIMIT_MAG == 512

BEGIN BOOST_PP_NOT_EQUAL(375, 126) == 1 END
BEGIN BOOST_PP_NOT_EQUAL(439, 439) == 0 END

#endif

#if BOOST_PP_LIMIT_MAG == 1024

BEGIN BOOST_PP_NOT_EQUAL(684, 674) == 1 END
BEGIN BOOST_PP_NOT_EQUAL(1011, 1011) == 0 END

#endif

/* less */

BEGIN BOOST_PP_LESS(2, 1) == 0 END
BEGIN BOOST_PP_LESS(1, 2) == 1 END

#if BOOST_PP_LIMIT_MAG == 512

BEGIN BOOST_PP_LESS(265, 241) == 0 END
BEGIN BOOST_PP_LESS(510, 511) == 1 END

#endif

#if BOOST_PP_LIMIT_MAG == 1024

BEGIN BOOST_PP_LESS(753, 629) == 0 END
BEGIN BOOST_PP_LESS(1022, 1024) == 1 END

#endif

/* less_equal */

BEGIN BOOST_PP_LESS_EQUAL(2, 1) == 0 END
BEGIN BOOST_PP_LESS_EQUAL(1, 2) == 1 END
BEGIN BOOST_PP_LESS_EQUAL(2, 2) == 1 END

#if BOOST_PP_LIMIT_MAG == 512

BEGIN BOOST_PP_LESS_EQUAL(327, 211) == 0 END
BEGIN BOOST_PP_LESS_EQUAL(489, 502) == 1 END
BEGIN BOOST_PP_LESS_EQUAL(327, 327) == 1 END

#endif

#if BOOST_PP_LIMIT_MAG == 1024

BEGIN BOOST_PP_LESS_EQUAL(951, 632) == 0 END
BEGIN BOOST_PP_LESS_EQUAL(875, 1002) == 1 END
BEGIN BOOST_PP_LESS_EQUAL(727, 727) == 1 END

#endif

/* greater */

BEGIN BOOST_PP_GREATER(2, 1) == 1 END
BEGIN BOOST_PP_GREATER(1, 2) == 0 END

#if BOOST_PP_LIMIT_MAG == 512

BEGIN BOOST_PP_GREATER(348, 259) == 1 END
BEGIN BOOST_PP_GREATER(411, 411) == 0 END
BEGIN BOOST_PP_GREATER(327, 373) == 0 END

#endif

#if BOOST_PP_LIMIT_MAG == 1024

BEGIN BOOST_PP_GREATER(846, 523) == 1 END
BEGIN BOOST_PP_GREATER(574, 574) == 0 END
BEGIN BOOST_PP_GREATER(749, 811) == 0 END

#endif

/* greater_equal */

BEGIN BOOST_PP_GREATER_EQUAL(2, 1) == 1 END
BEGIN BOOST_PP_GREATER_EQUAL(1, 2) == 0 END
BEGIN BOOST_PP_GREATER_EQUAL(2, 2) == 1 END

#if BOOST_PP_LIMIT_MAG == 512

BEGIN BOOST_PP_GREATER_EQUAL(341, 27) == 1 END
BEGIN BOOST_PP_GREATER_EQUAL(289, 470) == 0 END
BEGIN BOOST_PP_GREATER_EQUAL(296, 296) == 1 END

#endif

#if BOOST_PP_LIMIT_MAG == 1024

BEGIN BOOST_PP_GREATER_EQUAL(946, 852) == 1 END
BEGIN BOOST_PP_GREATER_EQUAL(685, 717) == 0 END
BEGIN BOOST_PP_GREATER_EQUAL(1001, 1001) == 1 END

#endif
