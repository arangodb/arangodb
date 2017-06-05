//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "tests_shared.hpp"
#include "utils/ebo.hpp"

TEST(compact_pair_tests, check_size) {
  struct empty { };
  struct non_empty { int i; };

  static_assert(
    sizeof(irs::compact_pair<empty, non_empty>) == sizeof(non_empty),
    "Invalid size"
  );

  static_assert(
    sizeof(irs::compact_pair<non_empty, empty>) == sizeof(non_empty),
    "Invalid size"
  );

  static_assert(
    sizeof(irs::compact_pair<non_empty, non_empty>) == 2*sizeof(non_empty),
    "Invalid size"
  );
}

TEST(ebo_tests, check_size) {
  struct empty { };
  struct non_empty { int i; };

  struct empty_inherited : irs::compact<0, empty> { int i; };
  static_assert(
    sizeof(empty_inherited) == sizeof(int),
    "Invalid size"
  );

  struct non_empty_inherited : irs::compact<0, non_empty> { int i; };
  static_assert(
    sizeof(non_empty_inherited) == 2*sizeof(int),
    "Invalid size"
  );

  struct empty_ref_inherited : irs::compact_ref<0, empty> { int i; };
  static_assert(
    sizeof(empty_ref_inherited) == sizeof(int),
    "Invalid size"
  );

  struct non_empty_ref_inherited : irs::compact_ref<0, non_empty> { int i; };
  static_assert(
    sizeof(non_empty_ref_inherited) == 2*sizeof(non_empty*), // because of non_empty* alignment
    "Invalid size"
  );
}
