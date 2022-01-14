# /* Copyright (C) 2001
#  * Housemarque Oy
#  * http://www.housemarque.com
#  *
#  * Distributed under the Boost Software License, Version 1.0. (See
#  * accompanying file LICENSE_1_0.txt or copy at
#  * http://www.boost.org/LICENSE_1_0.txt)
#  */
#
# /* Revised by Paul Mensonides (2002) */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef BOOST_LIBS_PREPROCESSOR_REGRESSION_TEST_MACRO_H
# define BOOST_LIBS_PREPROCESSOR_REGRESSION_TEST_MACRO_H
#
# include <boost/preprocessor/cat.hpp>
#
# define BEGIN typedef int BOOST_PP_CAT(test_, __LINE__)[((
# define END )==1) ? 1 : -1];
#
# endif
