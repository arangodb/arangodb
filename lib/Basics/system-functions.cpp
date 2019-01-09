////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include <chrono>
#include <thread>

using namespace arangodb;
using namespace arangodb::utilities;

#ifdef TRI_MISSING_MEMRCHR
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

#ifdef _WIN32
void* memmem(void const* haystack, size_t haystackLength, void const* needle,
             size_t needleLength) {
  if (haystackLength == 0 || needleLength == 0 || haystackLength < needleLength) {
    return nullptr;
  }

  char const* n = static_cast<char const*>(needle);

  if (needleLength == 1) {
    return memchr(const_cast<void*>(haystack), static_cast<int>(*n), haystackLength);
  }

  char const* current = static_cast<char const*>(haystack);
  char const* end = static_cast<char const*>(haystack) + haystackLength - needleLength;

  for (; current <= end; ++current) {
    if (*current == *n && memcmp(needle, current, needleLength) == 0) {
      return const_cast<void*>(static_cast<void const*>(current));
    }
  }

  return nullptr;
}
#endif

#ifdef TRI_HAVE_WIN32_GETTIMEOFDAY
int gettimeofday(struct timeval* tv, void* tz) {
  union {
    int64_t ns100;  // since 1.1.1601 in 100ns units
    FILETIME ft;
  } now;

  GetSystemTimeAsFileTime(&now.ft);

  tv->tv_usec = (long)((now.ns100 / 10LL) % 1000000LL);
  tv->tv_sec = (long)((now.ns100 - 116444736000000000LL) / 10000000LL);

  return 0;
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
    memcpy(tb, tp, sizeof(struct tm));
  }
#endif
#endif
}

time_t TRI_timegm(struct tm* tm) {
#ifdef _WIN32
  return _mkgmtime(tm);
#else
#ifdef __sun
  // This should work, but who knows? ti is any valid time_t value, since
  // it is produced by mktime. Therefore gmtime_r and localtime_r compute
  // two different split up time values gm and lo, which differ by the
  // currently configured offset of local time and UTC. Note that DST
  // will be factored into the lo result but not into gm, including the
  // lo.tm_isdst field! Then we apply mktime to both gm and lo, and,
  // whatever the currently configured offset of local time and UTC is,
  // it will be expressed in time_t units by (mktime(&gm) - mktime(&lo)).
  // Therefore, this is the correct fix for the result of mktime(tm)
  // to actually interpret tm as universal time. Note that we set tm.isdst
  // to 0 to make mktime behave exactly as for gm.
  tm.isdst = 0;
  time_t ti = mktime(tm);
  struct tm gm;
  gmtime_r(&ti, &gm);
  struct tm lo;
  localtime_r(&ti, &lo);
  return ti - (mktime(&gm) - mktime(&lo));
#else
  // Linux, OSX and BSD variants:
  return timegm(tm);
#endif
#endif
}

double TRI_microtime() {
  return std::chrono::duration<double>(  // time since epoch in seconds
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

size_t TRI_numberProcessors() {
#ifdef TRI_SC_NPROCESSORS_ONLN
  auto n = sysconf(_SC_NPROCESSORS_ONLN);

  if (n < 0) {
    n = 0;
  }

  if (n > 0) {
    return n;
  }
#endif

  return static_cast<size_t>(std::thread::hardware_concurrency());
}

std::string arangodb::utilities::timeString(char sep, char fin) {
  time_t tt = time(nullptr);
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
