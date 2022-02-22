// Copyright 2018 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef S2_BASE_COMMANDLINEFLAGS_H_
#define S2_BASE_COMMANDLINEFLAGS_H_

#ifdef S2_USE_GFLAGS

#include <gflags/gflags.h>

#else  // !defined(S2_USE_GFLAGS)

#include <string>

#include "s2/base/integral_types.h"

#define DEFINE_bool(name, default_value, description) \
  bool FLAGS_##name = default_value
#define DECLARE_bool(name) \
  extern bool FLAGS_##name

#define DEFINE_double(name, default_value, description) \
  double FLAGS_##name = default_value
#define DECLARE_double(name) \
  extern double FLAGS_##name

#define DEFINE_int32(name, default_value, description) \
  int32 FLAGS_##name = default_value
#define DECLARE_int32(name) \
  extern int32 FLAGS_##name

#define DEFINE_string(name, default_value, description) \
  std::string FLAGS_##name = default_value
#define DECLARE_string(name) \
  extern std::string FLAGS_##name

#endif  // !defined(S2_USE_GFLAGS)

#endif  // S2_BASE_COMMANDLINEFLAGS_H_
