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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "Basics/CGroupDetection.h"
#include "Basics/FileUtils.h"
#include "Basics/operating-system.h"

#include "Basics/PhysicalMemory.h"
#include "Basics/files.h"
#include "Basics/application-exit.h"
#include "ProgramOptions/Parameters.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <iostream>
#include <ostream>

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

namespace arangodb {

namespace {

// cgroup v1 file paths
static constexpr char const* kCgroupV1MemoryLimitPath =
    "/sys/fs/cgroup/memory/memory.limit_in_bytes";

// cgroup v2 file paths
static constexpr char const* kCgroupV2MemoryMaxPath =
    "/sys/fs/cgroup/memory.max";

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

#endif
#endif

uint64_t effectivePhysicalMemoryImpl() {
  auto const cgroup = cgroup::getVersion();

  switch (cgroup) {
    case cgroup::Version::NONE: {
      break;
    }
    case cgroup::Version::V1: {
      try {
        if (basics::FileUtils::exists(kCgroupV1MemoryLimitPath)) {
          if (auto const limit = basics::FileUtils::readCgroupFileValue(
                  kCgroupV1MemoryLimitPath);
              limit) {
            // Check if it's not the "unlimited" value (very large number)
            if (limit > 0 && *limit != std::numeric_limits<int64_t>::max()) {
              return *limit;
            }
          }
        }
      } catch (std::exception const& ex) {
        LOG_TOPIC("b4d32", INFO, Logger::FIXME)
            << "Failed to determine physical memory from cgroup v1 "
               "memory.limit_in_bytes file: "
            << ex.what();
      } catch (...) {
        LOG_TOPIC("b4d33", INFO, Logger::FIXME)
            << "Failed to determine physical memory from cgroup v1 "
               "memory.limit_in_bytes file: unknown error";
      }
      break;
    }
    case cgroup::Version::V2: {
      try {
        if (basics::FileUtils::exists(kCgroupV2MemoryMaxPath)) {
          if (auto const limit = basics::FileUtils::readCgroupFileValue(
                  kCgroupV2MemoryMaxPath);
              limit) {
            if (limit > 0 && *limit != std::numeric_limits<int64_t>::max()) {
              return *limit;
            }
          }
        }
      } catch (std::exception const& ex) {
        LOG_TOPIC("b4d34", INFO, Logger::FIXME)
            << "Failed to determine physical memory from cgroup v2 "
               "memory.max file: "
            << ex.what();
      } catch (...) {
        LOG_TOPIC("b4d35", INFO, Logger::FIXME)
            << "Failed to determine physical memory from cgroup v2 "
               "memory.max file: unknown error";
      }
      break;
    }
  }

  return physicalMemoryImpl();
}

struct PhysicalMemoryCache {
  PhysicalMemoryCache()
      : memory(physicalMemoryImpl()),
        effectiveMemory(effectivePhysicalMemoryImpl()),
        overridden(false) {
    std::string value;
    if (TRI_GETENV("ARANGODB_OVERRIDE_DETECTED_TOTAL_MEMORY", value)) {
      if (!value.empty()) {
        try {
          uint64_t v = options::fromString<uint64_t>(value);
          if (v != 0) {
            // value in environment variable must always be > 0
            memory = v;
            overridden = true;
          }
        } catch (...) {
          std::cerr
              << "failed to parse ARANGODB_OVERRIDE_DETECTED_TOTAL_MEMORY: "
              << "expected integer, got " << value << std::endl;
          FATAL_ERROR_EXIT();
        }
      }
    }
  }
  uint64_t memory;
  uint64_t effectiveMemory;
  bool overridden;
};

PhysicalMemoryCache const& getCache() {
  static PhysicalMemoryCache const cache;
  return cache;
}

}  // namespace

/// @brief return physical memory size from cache
uint64_t PhysicalMemory::getValue() { return getCache().memory; }

std::size_t PhysicalMemory::getEffectiveValue() {
  return getCache().effectiveMemory;
}

/// @brief return if physical memory size was overridden
bool PhysicalMemory::overridden() { return getCache().overridden; }
}  // namespace arangodb
