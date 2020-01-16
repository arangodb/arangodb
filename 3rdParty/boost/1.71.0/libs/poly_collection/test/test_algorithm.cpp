/* Copyright 2016 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#include "test_algorithm.hpp"
#include "test_algorithm1.hpp"
#include "test_algorithm2.hpp"
#include "test_algorithm3.hpp"

/* test split in chunks to avoid problems with compilation object sizes */

void test_algorithm()
{
  test_algorithm1();
  test_algorithm2();
  test_algorithm3();
}
