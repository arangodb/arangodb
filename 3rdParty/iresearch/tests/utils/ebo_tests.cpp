////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

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
