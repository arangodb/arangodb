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

#include "s2/base/sysinfo.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/resource.h>   // for rusage, RUSAGE_SELF
#include <sys/time.h>
#endif

namespace base {

absl::Duration CPUUsage() {
#ifdef _WIN32
  FILETIME creation, exit, kernel, user;
  int res = GetProcessTimes(GetCurrentProcess(), &creation,
                            &exit, &kernel, &user);
  if (res != 0) return absl::ZeroDuration();
  // high and low combine to 64-bit int
  ULARGE_INTEGER conv;
  conv.LowPart = user.dwLowDateTime;
  conv.HighPart = user.dwHighDateTime;
  // time uses 100-nanosecond intervals
  return absl::Duration(conv.QuadPart / 1e7);
#else
  struct rusage ru;
  if (getrusage(RUSAGE_SELF, &ru) != 0) return absl::ZeroDuration();
  return absl::Duration(ru.ru_utime.tv_sec + ru.ru_utime.tv_usec / 1e6);
#endif
}

}  // namespace base
