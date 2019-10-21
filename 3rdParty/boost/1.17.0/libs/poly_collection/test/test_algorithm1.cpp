/* Copyright 2016 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#include "test_algorithm1.hpp"

#include "any_types.hpp"
#include "test_algorithm_impl.hpp"

void test_algorithm1()
{
  test_algorithm<
    any_types::collection,jammed_auto_increment,any_types::to_int,
    any_types::t1,any_types::t2,any_types::t3,
    any_types::t4,any_types::t5>();
}
