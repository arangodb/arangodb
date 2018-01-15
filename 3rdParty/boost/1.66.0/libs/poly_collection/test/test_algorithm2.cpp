/* Copyright 2016 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#include "test_algorithm2.hpp"

#include "base_types.hpp"
#include "test_algorithm_impl.hpp"

void test_algorithm2()
{
  test_algorithm<
    base_types::collection,jammed_auto_increment,base_types::to_int,
    base_types::t1,base_types::t2,base_types::t3,
    base_types::t4,base_types::t5>();
}
