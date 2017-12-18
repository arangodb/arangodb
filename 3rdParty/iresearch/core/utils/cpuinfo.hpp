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

#ifndef IRESEARCH_CPUID_ID
#define IRESEARCH_CPUID_ID

#if defined(_MSC_VER)

#include "shared.hpp"
#include <intrin.h>

NS_ROOT

class IRESEARCH_API cpuinfo {
 public:
  static bool support_popcnt();
 private:
  static const cpuinfo instance_;

  cpuinfo() {
    __cpuid( f1_cpuinfo_, 1 );
  }

  int f1_cpuinfo_[4];
};

NS_END

#endif

#endif
