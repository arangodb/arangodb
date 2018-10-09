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

#include "filter_test_case_base.hpp"

NS_BEGIN(tests)
NS_BEGIN(sort)

DEFINE_SORT_TYPE(boost);
DEFINE_FACTORY_DEFAULT(boost);

DEFINE_SORT_TYPE(custom_sort);
DEFINE_FACTORY_DEFAULT(custom_sort);

DEFINE_SORT_TYPE(frequency_sort);
DEFINE_FACTORY_DEFAULT(frequency_sort);

DEFINE_ATTRIBUTE_TYPE(frequency_sort::prepared::count);
DEFINE_FACTORY_DEFAULT(frequency_sort::prepared::count);

NS_END // NS_BEGIN(sort)
NS_END // NS_BEGIN(tests)

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
