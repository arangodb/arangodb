// Copyright Google Inc. All Rights Reserved.
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

#include <string>

#include "s2/base/integral_types.h"
#include "absl/strings/string_view.h"

#ifdef S2_USE_GFLAGS

#include <gflags/gflags.h>

// If the GFlags library is available, map the local macro names to
// GFlags macros.
#define S2_DEFINE_bool  DEFINE_bool
#define S2_DECLARE_bool  DECLARE_bool

#define S2_DEFINE_double  DEFINE_double
#define S2_DECLARE_double  DECLARE_double

#define S2_DEFINE_int32  DEFINE_int32
#define S2_DECLARE_int32  DECLARE_int32

#define S2_DEFINE_int64  DEFINE_int64
#define S2_DECLARE_int64  DECLARE_int64

#define S2_DEFINE_string  DEFINE_string
#define S2_DECLARE_string  DECLARE_string

// gflags has its own int32 definitions.
namespace absl {
inline int32 GetFlag(google::int32 flag) { return flag; }
inline int64 GetFlag(google::int64 flag) { return flag; }
inline void SetFlag(google::int32* f, google::int32 v) { *f = v; }
inline void SetFlag(google::int64* f, google::int64 v) { *f = v; }
}  // namespace absl

#else  // !defined(S2_USE_GFLAGS)

#include <string>

#include "s2/base/integral_types.h"

// Create a set of gflags-like macros for declaring/defining flags. Use
// a library-specific name to potential minimize clashes with GFlags.

#define S2_DEFINE_bool(name, default_value, description) \
  bool FLAGS_##name = default_value
#define S2_DECLARE_bool(name) \
  extern bool FLAGS_##name
#define S2_DEFINE_double(name, default_value, description) \
  double FLAGS_##name = default_value
#define S2_DECLARE_double(name) \
  extern double FLAGS_##name

#define S2_DEFINE_int32(name, default_value, description) \
  int32 FLAGS_##name = default_value
#define S2_DECLARE_int32(name) \
  extern int32 FLAGS_##name

#define S2_DEFINE_int64(name, default_value, description) \
  int64 FLAGS_##name = default_value
#define S2_DECLARE_int64(name) extern int64 FLAGS_##name

#define S2_DEFINE_string(name, default_value, description) \
  std::string FLAGS_##name = default_value
#define S2_DECLARE_string(name) \
  extern std::string FLAGS_##name

namespace absl {
inline int32 GetFlag(int32 flag) { return flag; }
inline int64 GetFlag(int64 flag) { return flag; }
inline void SetFlag(int32* f, int32 v) { *f = v; }
inline void SetFlag(int64* f, int64 v) { *f = v; }
}  // namespace absl

#endif  // !defined(S2_USE_GFLAGS)

// Flags are declared with v1 flags, but accessed via absl::GetFlag/SetFlag.
// TODO(user,b/205817426): Convert everything to absl flags and remove gflags.
namespace absl {

inline bool GetFlag(bool flag) { return flag; }
inline double GetFlag(double flag) { return flag; }
inline std::string GetFlag(const std::string& flag) { return flag; }

inline void SetFlag(bool* f, bool v) { *f = v; }
inline void SetFlag(double* f, double v) { *f = v; }
inline void SetFlag(std::string* f, absl::string_view v) {
  *f = std::string(v);
}

}  // namespace absl

#endif  // S2_BASE_COMMANDLINEFLAGS_H_
