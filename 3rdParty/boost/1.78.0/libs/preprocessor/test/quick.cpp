// Copyright 2002 Paul Mensonides
// Copyright 2019 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/preprocessor/comparison.hpp>
#include "test_macro.h"

/* equality */

BEGIN BOOST_PP_EQUAL(2, 0) == 0 END
BEGIN BOOST_PP_EQUAL(2, 2) == 1 END

/* inequality */

BEGIN BOOST_PP_NOT_EQUAL(2, 0) == 1 END
BEGIN BOOST_PP_NOT_EQUAL(2, 2) == 0 END

/* less */

BEGIN BOOST_PP_LESS(2, 1) == 0 END
BEGIN BOOST_PP_LESS(1, 2) == 1 END

/* less_equal */

BEGIN BOOST_PP_LESS_EQUAL(2, 1) == 0 END
BEGIN BOOST_PP_LESS_EQUAL(1, 2) == 1 END
BEGIN BOOST_PP_LESS_EQUAL(2, 2) == 1 END

/* greater */

BEGIN BOOST_PP_GREATER(2, 1) == 1 END
BEGIN BOOST_PP_GREATER(1, 2) == 0 END

/* greater_equal */

BEGIN BOOST_PP_GREATER_EQUAL(2, 1) == 1 END
BEGIN BOOST_PP_GREATER_EQUAL(1, 2) == 0 END
BEGIN BOOST_PP_GREATER_EQUAL(2, 2) == 1 END

int main()
{
}
