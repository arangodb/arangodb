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
# include <boost/preprocessor/facilities/is_empty.hpp>
# include <libs/preprocessor/test/test.h>

#if BOOST_PP_VARIADICS && (BOOST_PP_CONFIG_FLAGS() & BOOST_PP_CONFIG_STRICT()) && !BOOST_PP_VARIADICS_MSVC

#define FUNC_GEN8(x,y) (1,2,3)
  
BEGIN BOOST_PP_IS_EMPTY(FUNC_GEN8) == 0 END

#else
  
BEGIN 1 == 0 END

#endif
