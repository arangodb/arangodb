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

#ifndef IRESEARCH_VERSION_UTILS_H
#define IRESEARCH_VERSION_UTILS_H

#include "shared.hpp"
#include "utils/string.hpp"

#include "utils/version_defines.hpp"

#ifdef IResearch_int_version
  #define IRESEARCH_VERSION IResearch_int_version
#else
  #define IRESEARCH_VERSION 0
#endif

namespace iresearch {
namespace version_utils {

const string_ref build_date();
const string_ref build_id();
const string_ref build_time();
const string_ref build_version();

}
}

#endif
