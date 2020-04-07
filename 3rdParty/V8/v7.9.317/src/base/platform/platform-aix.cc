// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Platform specific code for AIX goes here. For the POSIX comaptible parts
// the implementation is in platform-posix.cc.

#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/ucontext.h>

#include <errno.h>
#include <fcntl.h>  // open
#include <limits.h>
#include <stdarg.h>
#include <strings.h>    // index
#include <sys/mman.h>   // mmap & munmap
#include <sys/stat.h>   // open
#include <sys/types.h>  // mmap & munmap
#include <unistd.h>     // getpagesize

#include <cmath>

#undef MAP_TYPE

#include "src/base/macros.h"
#include "src/base/platform/platform-posix.h"
#include "src/base/platform/platform.h"

namespace v8 {
namespace base {


int64_t get_gmt_offset(const tm& localtm) {
  // replacement for tm->tm_gmtoff field in glibc
  // returns seconds east of UTC, taking DST into account
  struct timeval tv;
  struct timezone tz;
  int ret_code = gettimeofday(&tv, &tz);
  // 0 = success, -1 = failure
  DCHECK_NE(ret_code, -1);
  if (ret_code == -1) {
    return 0;
  }
  return (-tz.tz_minuteswest * 60) + (localtm.tm_isdst > 0 ? 3600 : 0);
}

class AIXTimezoneCache : public PosixTimezoneCache {
  const char* LocalTimezone(double time) override;

  double LocalTimeOffset(double time_ms, bool is_utc) override;

  ~AIXTimezoneCache() override {}
};

const char* AIXTimezoneCache::LocalTimezone(double time_ms) {
  if (std::isnan(time_ms)) return "";
  time_t tv = static_cast<time_t>(floor(time_ms / msPerSecond));
  struct tm tm;
  struct tm* t = localtime_r(&tv, &tm);
  if (nullptr == t) return "";
  return tzname[0];  // The location of the timezone string on AIX.
}

double AIXTimezoneCache::LocalTimeOffset(double time_ms, bool is_utc) {
  // On AIX, struct tm does not contain a tm_gmtoff field, use get_gmt_offset
  // helper function
  time_t utc = time(nullptr);
  DCHECK_NE(utc, -1);
  struct tm tm;
  struct tm* loc = localtime_r(&utc, &tm);
  DCHECK_NOT_NULL(loc);
  return static_cast<double>(get_gmt_offset(*loc) * msPerSecond -
                             (loc->tm_isdst > 0 ? 3600 * msPerSecond : 0));
}

TimezoneCache* OS::CreateTimezoneCache() { return new AIXTimezoneCache(); }

static unsigned StringToLong(char* buffer) {
  return static_cast<unsigned>(strtol(buffer, nullptr, 16));  // NOLINT
}

std::vector<OS::SharedLibraryAddress> OS::GetSharedLibraryAddresses() {
  std::vector<SharedLibraryAddress> result;
  static const int MAP_LENGTH = 1024;
  int fd = open("/proc/self/maps", O_RDONLY);
  if (fd < 0) return result;
  while (true) {
    char addr_buffer[11];
    addr_buffer[0] = '0';
    addr_buffer[1] = 'x';
    addr_buffer[10] = 0;
    ssize_t rc = read(fd, addr_buffer + 2, 8);
    if (rc < 8) break;
    unsigned start = StringToLong(addr_buffer);
    rc = read(fd, addr_buffer + 2, 1);
    if (rc < 1) break;
    if (addr_buffer[2] != '-') break;
    rc = read(fd, addr_buffer + 2, 8);
    if (rc < 8) break;
    unsigned end = StringToLong(addr_buffer);
    char buffer[MAP_LENGTH];
    int bytes_read = -1;
    do {
      bytes_read++;
      if (bytes_read >= MAP_LENGTH - 1) break;
      rc = read(fd, buffer + bytes_read, 1);
      if (rc < 1) break;
    } while (buffer[bytes_read] != '\n');
    buffer[bytes_read] = 0;
    // Ignore mappings that are not executable.
    if (buffer[3] != 'x') continue;
    char* start_of_path = index(buffer, '/');
    // There may be no filename in this line.  Skip to next.
    if (start_of_path == nullptr) continue;
    buffer[bytes_read] = 0;
    result.push_back(SharedLibraryAddress(start_of_path, start, end));
  }
  close(fd);
  return result;
}

void OS::SignalCodeMovingGC() {}

void OS::AdjustSchedulingParams() {}

}  // namespace base
}  // namespace v8
