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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_SO_UTILS_H
#define IRESEARCH_SO_UTILS_H

#include <functional>

namespace iresearch {

// #define RTLD_LAZY   1
// #define RTLD_NOW    2
// #define RTLD_GLOBAL 4

void* load_library(const char* soname, int mode = 2);
void* get_function(void* handle, const char* fname);
bool free_library(void* handle);
void load_libraries(
  const std::string& path,
  const std::string& prefix,
  const std::string& suffix
);

}

#endif // IRESEARCH_SO_UTILS_H
