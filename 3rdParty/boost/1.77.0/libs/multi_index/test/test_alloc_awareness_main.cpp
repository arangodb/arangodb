/* Boost.MultiIndex test for allocator awareness.
 *
 * Copyright 2003-2020 Joaquin M Lopez Munoz.
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 *
 * See http://www.boost.org/libs/multi_index for library home page.
 */

#include <boost/detail/lightweight_test.hpp>
#include "test_alloc_awareness.hpp"

int main()
{
  test_allocator_awareness();
  return boost::report_errors();
}
