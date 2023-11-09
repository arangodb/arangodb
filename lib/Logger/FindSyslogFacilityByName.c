////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include "Basics/operating-system.h"

#if defined(ARANGODB_ENABLE_SYSLOG)
//
// This is C code to prevent clang from complaining about the
// expansion of the facilitynames macro on musl libc yielding char *
// to character constants (and hence failing to compile) when
// compiling C++
//

#include <stddef.h>
// we need to define SYSLOG_NAMES for linux to get a list of names
#define SYSLOG_NAMES
#include <syslog.h>
#include <string.h>

int find_syslog_facility_by_name(const char* facility) {
  CODE* ptr = facilitynames;
  while (ptr->c_name != NULL) {
    if (strcmp(ptr->c_name, facility) == 0) {
      return ptr->c_val;
    }
  }
  return LOG_LOCAL0;
}
#endif
