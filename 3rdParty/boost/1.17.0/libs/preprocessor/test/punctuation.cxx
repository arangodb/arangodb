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
# if BOOST_PP_VARIADICS
# include <boost/preprocessor/punctuation.hpp>
# include <libs/preprocessor/test/test.h>

# define A_TUPLE (*,#,zz)
# define A_TUPLE2 (*,#,(zz,44,(e7)))
# define A_TUPLE_PLUS (mmf,34,^^,!) 456
# define PLUS_ATUPLE yyt (j,ii%)
# define JDATA ggh 
# define NOT_TUPLE y6()
# define NOT_TUPLE2 &(kkkgg,(e))
# define A_SEQ (r)($)(#)
# define AN_ARRAY (4,(5,7,f,x)) 
# define A_LIST (e,(g,(&,BOOST_PP_NIL)))
# define DATA (5 + 3) * 4
# define DATA2 4 * (5 + 3)
# define DATA3 4 * (5 + 3) * (2 + 1)
# define DATA4 (5 + 3) * (2 + 1) * 4
  
  
// is_begin_parens

BEGIN BOOST_PP_IS_BEGIN_PARENS() == 0 END
BEGIN BOOST_PP_IS_BEGIN_PARENS(A_TUPLE) == 1 END
BEGIN BOOST_PP_IS_BEGIN_PARENS(A_TUPLE2) == 1 END
BEGIN BOOST_PP_IS_BEGIN_PARENS(A_TUPLE_PLUS) == 1 END
BEGIN BOOST_PP_IS_BEGIN_PARENS(PLUS_ATUPLE) == 0 END
BEGIN BOOST_PP_IS_BEGIN_PARENS(JDATA) == 0 END
BEGIN BOOST_PP_IS_BEGIN_PARENS(NOT_TUPLE) == 0 END
BEGIN BOOST_PP_IS_BEGIN_PARENS(NOT_TUPLE2) == 0 END
BEGIN BOOST_PP_IS_BEGIN_PARENS(A_SEQ) == 1 END
BEGIN BOOST_PP_IS_BEGIN_PARENS(AN_ARRAY) == 1 END
BEGIN BOOST_PP_IS_BEGIN_PARENS(A_LIST) == 1 END
BEGIN BOOST_PP_IS_BEGIN_PARENS((y)2(x)) == 1 END

// remove_parens

BEGIN BOOST_PP_REMOVE_PARENS(DATA) == 17 END
BEGIN BOOST_PP_REMOVE_PARENS(DATA2)== 32 END
BEGIN BOOST_PP_REMOVE_PARENS(DATA3)== 96 END
BEGIN BOOST_PP_REMOVE_PARENS(DATA4)== 41 END

#endif
