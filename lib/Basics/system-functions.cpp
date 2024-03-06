////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "Basics/operating-system.h"

#include "system-functions.h"

#include <chrono>
#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

using namespace arangodb;
using namespace arangodb::utilities;

#ifdef ARANGODB_MISSING_MEMRCHR
void* memrchr(void const* block, int c, size_t size) {
  if (size) {
    unsigned char const* p = static_cast<unsigned char const*>(block);

    for (p += size - 1; size; p--, size--) {
      if (*p == c) {
        return (void*)p;
      }
    }
  }
  return nullptr;
}
#endif

void TRI_localtime(time_t tt, struct tm* tb) {
#ifdef ARANGODB_HAVE_LOCALTIME_R
  localtime_r(&tt, tb);
#else
#ifdef ARANGODB_HAVE_LOCALTIME_S
  localtime_s(tb, &tt);
#else
  struct tm* tp = localtime(&tt);

  if (tp != nullptr) {
    memcpy(tb, tp, sizeof(struct tm));
  }
#endif
#endif
}

void TRI_gmtime(time_t tt, struct tm* tb) {
#ifdef ARANGODB_HAVE_GMTIME_R
  gmtime_r(&tt, tb);
#else
#ifdef ARANGODB_HAVE_GMTIME_S
  gmtime_s(tb, &tt);
#else
  struct tm* tp = gmtime(&tt);

  if (tp != nullptr) {
    // cppcheck-suppress uninitvar
    memcpy(tb, tp, sizeof(struct tm));
  }
#endif
#endif
}

time_t TRI_timegm(struct tm* tm) {
  // Linux, OSX and BSD variants:
  return timegm(tm);
}

double TRI_microtime() noexcept {
  return std::chrono::duration<double>(  // time since epoch in seconds
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

std::string arangodb::utilities::timeString(char sep, char fin) {
  time_t tt = time(nullptr);
  // cppcheck-suppress uninitvar
  struct tm tb;
  TRI_gmtime(tt, &tb);
  char buffer[32];
  size_t len = strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &tb);
  buffer[10] = sep;
  buffer[19] = fin;

  if (fin == '\0') {
    --len;
  }

  return std::string(buffer, len);
}

std::string arangodb::utilities::hostname() {
  char buffer[1024];

  int res = gethostname(buffer, sizeof(buffer) - 1);

  if (res == 0) {
    return buffer;
  }

  return "localhost";
}
