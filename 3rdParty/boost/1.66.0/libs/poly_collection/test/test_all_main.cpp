/* Copyright 2016 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/poly_collection for library home page.
 */

#include <boost/core/lightweight_test.hpp>
#include "test_algorithm.hpp"
#include "test_capacity.hpp"
#include "test_comparison.hpp"
#include "test_construction.hpp"
#include "test_emplacement.hpp"
#include "test_erasure.hpp"
#include "test_insertion.hpp"
#include "test_iterators.hpp"
#include "test_registration.hpp"

int main()
{
  test_algorithm();
  test_capacity();
  test_comparison();
  test_construction();
  test_emplacement();
  test_erasure();
  test_insertion();
  test_iterators();
  test_registration();

  return boost::report_errors();
}
