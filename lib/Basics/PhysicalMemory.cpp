////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Basics/operating-system.h"

#include "Basics/PhysicalMemory.h"
#include "Basics/StringUtils.h"
#include "Basics/files.h"
#include "ProgramOptions/Parameters.h"

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef TRI_HAVE_MACH
#include <mach/mach_host.h>
#include <mach/mach_port.h>
#include <mach/mach_traps.h>
#include <mach/task.h>
#include <mach/thread_act.h>
#include <mach/vm_map.h>
#endif

#if defined(TRI_HAVE_MACOS_MEM_STATS)
#include <sys/sysctl.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

using namespace arangodb;

namespace {

/// @brief gets the physical memory size
#if defined(TRI_HAVE_MACOS_MEM_STATS)
uint64_t physicalMemoryImpl() {
  int mib[2];

  // Get the Physical memory size
  mib[0] = CTL_HW;
#ifdef TRI_HAVE_MACOS_MEM_STATS
  mib[1] = HW_MEMSIZE;
#else
  mib[1] = HW_PHYSMEM;  // The bytes of physical memory. (kenel + user space)
#endif
  size_t length = sizeof(int64_t);
  int64_t physicalMemory;
  sysctl(mib, 2, &physicalMemory, &length, nullptr, 0);

  return static_cast<uint64_t>(physicalMemory);
}

#else
#ifdef TRI_HAVE_SC_PHYS_PAGES
uint64_t physicalMemoryImpl() {
  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);

  return static_cast<uint64_t>(pages * page_size);
}

#else
#ifdef TRI_HAVE_WIN32_GLOBAL_MEMORY_STATUS
uint64_t physicalMemoryImpl() {
  MEMORYSTATUSEX status;
  status.dwLength = sizeof(status);
  GlobalMemoryStatusEx(&status);

  return static_cast<uint64_t>(status.ullTotalPhys);
}
#endif  // TRI_HAVE_WIN32_GLOBAL_MEMORY_STATUS
#endif
#endif

struct PhysicalMemoryCache {
  PhysicalMemoryCache() : cachedValue(physicalMemoryImpl()), overridden(false) {
    std::string value;
    if (TRI_GETENV("ARANGODB_OVERRIDE_DETECTED_TOTAL_MEMORY", value)) {
      if (!value.empty()) {
        uint64_t v = arangodb::options::fromString<uint64_t>(value);
        if (v != 0) {
          // value in environment variable must always be > 0
          cachedValue = v;
          overridden = true;
        }
      }
    }
  }
  uint64_t cachedValue;
  bool overridden;
};

PhysicalMemoryCache const cache;

} // namespace

/// @brief return physical memory size from cache
uint64_t arangodb::PhysicalMemory::getValue() {
  return ::cache.cachedValue;
}

/// @brief return if physical memory size was overridden
bool arangodb::PhysicalMemory::overridden() {
  return ::cache.overridden;
}
