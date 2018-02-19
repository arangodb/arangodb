// Copyright (C) 2014 Andrzej Krzemienski.
//
// Use, modification, and distribution is subject to the Boost Software
// License, Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//
// See http://www.boost.org/lib/optional for documentation.
//
// You are welcome to contact the author at: akrzemi1@gmail.com


#include "boost/optional/optional.hpp"

#ifdef __BORLANDC__
#pragma hdrstop
#endif

#include "boost/core/lightweight_test.hpp"
#include "testable_classes.hpp"
#include "optional_ref_assign_test_defs.hpp"

using boost::optional;
using boost::none;


template <typename T>
void test_optional_ref_assignment()
{
  test_copy_assignment_for<T>();
  test_rebinding_assignment_semantics<T>();
  
  test_copy_assignment_for_const<T>();
  test_copy_assignment_for_noconst_const<T>();
  test_rebinding_assignment_semantics_const<T>();
  test_rebinding_assignment_semantics_noconst_const<T>();
}

int main()
{
  test_optional_ref_assignment<ScopeGuard>();
  test_optional_ref_assignment<Abstract>();
  test_optional_ref_assignment< optional<int> >();
  
  return boost::report_errors();
}
