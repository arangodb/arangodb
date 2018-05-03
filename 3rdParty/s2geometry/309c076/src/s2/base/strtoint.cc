// Copyright 2008 Google Inc. All Rights Reserved.
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

//
// Architecture-neutral plug compatible replacements for strtol() friends.
// See strtoint.h for details on how to use this component.
//

#include <cerrno>
#include <climits>
#include "s2/base/port.h"
#include "s2/base/strtoint.h"

// Replacement strto[u]l functions that have identical overflow and underflow
// characteristics for both ILP-32 and LP-64 platforms, including errno
// preservation for error-free calls.
int32 strto32_adapter(const char *nptr, char **endptr, int base) {
  const int saved_errno = errno;
  errno = 0;
  const long result = strtol(nptr, endptr, base);
  if (errno == ERANGE && result == LONG_MIN) {
    return kint32min;
  } else if (errno == ERANGE && result == LONG_MAX) {
    return kint32max;
  } else if (errno == 0 && result < kint32min) {
    errno = ERANGE;
    return kint32min;
  } else if (errno == 0 && result > kint32max) {
    errno = ERANGE;
    return kint32max;
  }
  if (errno == 0)
    errno = saved_errno;
  return static_cast<int32>(result);
}

uint32 strtou32_adapter(const char *nptr, char **endptr, int base) {
  const int saved_errno = errno;
  errno = 0;
  const unsigned long result = strtoul(nptr, endptr, base);
  if (errno == ERANGE && result == ULONG_MAX) {
    return kuint32max;
  } else if (errno == 0 && result > kuint32max) {
    errno = ERANGE;
    return kuint32max;
  }
  if (errno == 0)
    errno = saved_errno;
  return static_cast<uint32>(result);
}
