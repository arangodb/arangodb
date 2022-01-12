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
# include <boost/preprocessor/logical.hpp>
# include <libs/preprocessor/test/test.h>

BEGIN BOOST_PP_NOT(0) == 1 END
BEGIN BOOST_PP_NOT(2) == 0 END

BEGIN BOOST_PP_AND(0, 0) == 0 END
BEGIN BOOST_PP_AND(0, 3) == 0 END
BEGIN BOOST_PP_AND(4, 0) == 0 END
BEGIN BOOST_PP_AND(5, 6) == 1 END

BEGIN BOOST_PP_OR(0, 0) == 0 END
BEGIN BOOST_PP_OR(0, 7) == 1 END
BEGIN BOOST_PP_OR(8, 0) == 1 END
BEGIN BOOST_PP_OR(9, 1) == 1 END

BEGIN BOOST_PP_XOR(0, 0) == 0 END
BEGIN BOOST_PP_XOR(0, 2) == 1 END
BEGIN BOOST_PP_XOR(3, 0) == 1 END
BEGIN BOOST_PP_XOR(4, 5) == 0 END

BEGIN BOOST_PP_NOR(0, 0) == 1 END
BEGIN BOOST_PP_NOR(0, 6) == 0 END
BEGIN BOOST_PP_NOR(7, 0) == 0 END
BEGIN BOOST_PP_NOR(8, 9) == 0 END

BEGIN BOOST_PP_BOOL(22) == 1 END
BEGIN BOOST_PP_BOOL(0) == 0 END

#if BOOST_PP_LIMIT_MAG == 512

BEGIN BOOST_PP_NOT(283) == 0 END

BEGIN BOOST_PP_AND(0, 505) == 0 END
BEGIN BOOST_PP_AND(376, 0) == 0 END
BEGIN BOOST_PP_AND(482, 139) == 1 END

BEGIN BOOST_PP_OR(0, 274) == 1 END
BEGIN BOOST_PP_OR(512, 0) == 1 END
BEGIN BOOST_PP_OR(23, 386) == 1 END

BEGIN BOOST_PP_XOR(0, 494) == 1 END
BEGIN BOOST_PP_XOR(391, 0) == 1 END
BEGIN BOOST_PP_XOR(260, 438) == 0 END

BEGIN BOOST_PP_NOR(0, 455) == 0 END
BEGIN BOOST_PP_NOR(338, 0) == 0 END
BEGIN BOOST_PP_NOR(267, 504) == 0 END

BEGIN BOOST_PP_BOOL(378) == 1 END

#endif

#if BOOST_PP_LIMIT_MAG == 1024

BEGIN BOOST_PP_NOT(946) == 0 END

BEGIN BOOST_PP_AND(0, 1002) == 0 END
BEGIN BOOST_PP_AND(628, 0) == 0 END
BEGIN BOOST_PP_AND(741, 110) == 1 END

BEGIN BOOST_PP_OR(0, 893) == 1 END
BEGIN BOOST_PP_OR(1024, 0) == 1 END
BEGIN BOOST_PP_OR(23, 730) == 1 END

BEGIN BOOST_PP_XOR(0, 942) == 1 END
BEGIN BOOST_PP_XOR(641, 0) == 1 END
BEGIN BOOST_PP_XOR(783, 869) == 0 END

BEGIN BOOST_PP_NOR(0, 515) == 0 END
BEGIN BOOST_PP_NOR(739, 0) == 0 END
BEGIN BOOST_PP_NOR(1024, 983) == 0 END

BEGIN BOOST_PP_BOOL(577) == 1 END

#endif

BEGIN BOOST_PP_BITAND(0, 0) == 0 END
BEGIN BOOST_PP_BITAND(0, 1) == 0 END
BEGIN BOOST_PP_BITAND(1, 0) == 0 END
BEGIN BOOST_PP_BITAND(1, 1) == 1 END

BEGIN BOOST_PP_BITNOR(0, 0) == 1 END
BEGIN BOOST_PP_BITNOR(0, 1) == 0 END
BEGIN BOOST_PP_BITNOR(1, 0) == 0 END
BEGIN BOOST_PP_BITNOR(1, 1) == 0 END

BEGIN BOOST_PP_BITOR(0, 0) == 0 END
BEGIN BOOST_PP_BITOR(0, 1) == 1 END
BEGIN BOOST_PP_BITOR(1, 0) == 1 END
BEGIN BOOST_PP_BITOR(1, 1) == 1 END

BEGIN BOOST_PP_BITXOR(0, 0) == 0 END
BEGIN BOOST_PP_BITXOR(0, 1) == 1 END
BEGIN BOOST_PP_BITXOR(1, 0) == 1 END
BEGIN BOOST_PP_BITXOR(1, 1) == 0 END

BEGIN BOOST_PP_COMPL(0) == 1 END
BEGIN BOOST_PP_COMPL(1) == 0 END
