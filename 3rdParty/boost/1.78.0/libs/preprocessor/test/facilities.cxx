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
# include <boost/preprocessor/cat.hpp>
# include <boost/preprocessor/facilities.hpp>
# include <boost/preprocessor/arithmetic/add.hpp>
# include <boost/preprocessor/arithmetic/mul.hpp>
# include <libs/preprocessor/test/test.h>

BEGIN BOOST_PP_APPLY(BOOST_PP_NIL) 0 == 0 END
BEGIN BOOST_PP_APPLY((0)) == 0 END

BEGIN BOOST_PP_APPLY((BOOST_PP_EMPTY))() 0 == 0 END

# define MACRO(x, y, z) 1
# define ARGS (1, 2, 3)

BEGIN BOOST_PP_EXPAND(MACRO ARGS) == 1 END

BEGIN BOOST_PP_IDENTITY(1)() == 1 END
BEGIN BOOST_PP_IDENTITY_N(36,10)(0,1,2,3,4,5,6,7,8,9) == 36 END

BEGIN BOOST_PP_CAT(BOOST_PP_INTERCEPT, 2) 1 == 1 END

#define OVMAC_1(x) BOOST_PP_ADD(x,5)
#define OVMAC_2(x,y) BOOST_PP_ADD(x,y)
#define OVMAC_3(x,y,z) BOOST_PP_ADD(BOOST_PP_MUL(x,y),z)
#define OVMAC_4(x,y,z,a) BOOST_PP_ADD(BOOST_PP_MUL(x,y),BOOST_PP_MUL(a,z))

#if BOOST_PP_VARIADICS_MSVC

#define OVTEST(...) BOOST_PP_CAT(BOOST_PP_OVERLOAD(OVMAC_,__VA_ARGS__)(__VA_ARGS__),BOOST_PP_EMPTY())

#else

#define OVTEST(...) BOOST_PP_OVERLOAD(OVMAC_,__VA_ARGS__)(__VA_ARGS__)

#endif

BEGIN OVTEST(3,4,5) == 17 END
BEGIN OVTEST(9,3,2,7) == 41 END
BEGIN OVTEST(8) == 13 END
BEGIN OVTEST(24,61) == 85 END
