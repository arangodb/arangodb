# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Edward Diener 2018.
#  *     Distributed under the Boost Software License, Version 1.0. (See
#  *     accompanying file LICENSE_1_0.txt or copy at
#  *     http://www.boost.org/LICENSE_1_0.txt)
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# include <boost/preprocessor/stringize.hpp>
# include <boost/preprocessor/wstringize.hpp>
# include <boost/preprocessor/arithmetic/inc.hpp>
# if !defined __cplusplus
#include <wchar.h>
#endif
# include <libs/preprocessor/test/test.h>

#define VDATA 1,2,3,4
#define NDATA
#define DATA data
#define FDATA(x) BOOST_PP_INC(x)

# if ~BOOST_PP_CONFIG_FLAGS() & BOOST_PP_CONFIG_MSVC()
BEGIN sizeof(BOOST_PP_STRINGIZE(NDATA)) / sizeof(char) == 1 END
BEGIN sizeof(BOOST_PP_WSTRINGIZE(NDATA)) / sizeof(wchar_t) == 1 END
#endif
BEGIN sizeof(BOOST_PP_STRINGIZE(DATA)) / sizeof(char) == 5 END
BEGIN sizeof(BOOST_PP_STRINGIZE(FDATA(1))) / sizeof(char) == 2 END
BEGIN sizeof(BOOST_PP_STRINGIZE(FDATA(9))) / sizeof(char) == 3 END
BEGIN sizeof(BOOST_PP_WSTRINGIZE(DATA)) / sizeof(wchar_t) == 5 END
BEGIN sizeof(BOOST_PP_WSTRINGIZE(FDATA(1))) / sizeof(wchar_t) == 2 END
BEGIN sizeof(BOOST_PP_WSTRINGIZE(FDATA(9))) / sizeof(wchar_t) == 3 END
BEGIN sizeof(BOOST_PP_STRINGIZE(VDATA)) / sizeof(char) == 8 END
BEGIN sizeof(BOOST_PP_WSTRINGIZE(VDATA)) / sizeof(wchar_t) == 8 END
