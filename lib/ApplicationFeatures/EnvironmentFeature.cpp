////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#include "EnvironmentFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"

using namespace arangodb;
using namespace arangodb::basics;

EnvironmentFeature::EnvironmentFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Environment") {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Greetings");
  startsAfter("Logger");
}

void EnvironmentFeature::prepare() {
  if (sizeof(void*) == 4) {
    // 32 bit build
    LOG_TOPIC(WARN, arangodb::Logger::MEMORY)
        << "this is a 32 bit build of ArangoDB. "
        << "it is recommended to run a 64 bit build instead because it can "
        << "address significantly bigger regions of memory";
  }

#ifdef __linux__

#ifdef __GLIBC__
  char const* v = getenv("GLIBCXX_FORCE_NEW");

  if (v == nullptr) {
    // environment variable not set
    LOG_TOPIC(WARN, arangodb::Logger::MEMORY)
        << "environment variable GLIBCXX_FORCE_NEW' is not set. "
        << "it is recommended to set it to some value to avoid unnecessary memory pooling in glibc++";
    LOG_TOPIC(WARN, arangodb::Logger::MEMORY)
        << "execute 'export GLIBCXX_FORCE_NEW=1'";
  }
#endif

  try {
    std::string value =
        basics::FileUtils::slurp("/proc/sys/vm/overcommit_memory");
    uint64_t v = basics::StringUtils::uint64(value);
    if (v != 0 && v != 1) {
      // from https://www.kernel.org/doc/Documentation/sysctl/vm.txt:
      //
      //   When this flag is 0, the kernel attempts to estimate the amount
      //   of free memory left when userspace requests more memory.
      //   When this flag is 1, the kernel pretends there is always enough
      //   memory until it actually runs out.
      //   When this flag is 2, the kernel uses a "never overcommit"
      //   policy that attempts to prevent any overcommit of memory.
      LOG_TOPIC(WARN, Logger::MEMORY)
          << "/proc/sys/vm/overcommit_memory is set to '" << v
          << "'. It is recommended to set it to a value of 0 or 1";
      LOG_TOPIC(WARN, Logger::MEMORY) << "execute 'sudo bash -c \"echo 0 > "
                                         "/proc/sys/vm/overcommit_memory\"'";
    }
  } catch (...) {
    // file not found or value not convertible into integer
  }

  try {
    std::string value =
        basics::FileUtils::slurp("/proc/sys/vm/zone_reclaim_mode");
    uint64_t v = basics::StringUtils::uint64(value);
    if (v != 0) {
      // from https://www.kernel.org/doc/Documentation/sysctl/vm.txt:
      //
      //    This is value ORed together of
      //    1 = Zone reclaim on
      //    2 = Zone reclaim writes dirty pages out
      //    4 = Zone reclaim swaps pages
      //
      // https://www.poempelfox.de/blog/2010/03/19/
      LOG_TOPIC(WARN, Logger::PERFORMANCE)
          << "/proc/sys/vm/zone_reclaim_mode is set to '" << v
          << "'. It is recommended to set it to a value of 0";
      LOG_TOPIC(WARN, Logger::PERFORMANCE)
          << "execute 'sudo bash -c \"echo 0 > "
             "/proc/sys/vm/zone_reclaim_mode\"'";
    }
  } catch (...) {
    // file not found or value not convertible into integer
  }

  bool showHuge = false;
  std::vector<std::string> paths = {
      "/sys/kernel/mm/transparent_hugepage/enabled",
      "/sys/kernel/mm/transparent_hugepage/defrag"};

  for (auto file : paths) {
    try {
      std::string value = basics::FileUtils::slurp(file);
      size_t start = value.find('[');
      size_t end = value.find(']');

      if (start != std::string::npos && end != std::string::npos &&
          start < end && end - start >= 4) {
        value = value.substr(start + 1, end - start - 1);
        if (value == "always") {
          LOG_TOPIC(WARN, Logger::MEMORY)
              << file << " is set to '" << value
              << "'. It is recommended to set it to a value of 'never' "
                 "or 'madvise'";
          showHuge = true;
        }
      }
    } catch (...) {
      // file not found
    }
  }

  if (showHuge) {
    for (auto file : paths) {
      LOG_TOPIC(WARN, Logger::MEMORY)
          << "execute 'sudo bash -c \"echo madvise > " << file << "\"'";
    }
  }

  bool numa = FileUtils::exists("/sys/devices/system/node/node1");

  if (numa) {
    try {
      std::string value = basics::FileUtils::slurp("/proc/self/numa_maps");
      auto values = basics::StringUtils::split(value, '\n', '\0');

      if (!values.empty()) {
        auto first = values[0];
        auto where = first.find(' ');

        if (where != std::string::npos &&
            !StringUtils::isPrefix(first.substr(where), " interleave")) {
          LOG_TOPIC(WARN, Logger::MEMORY)
              << "It is recommended to set NUMA to interleaved.";
          LOG_TOPIC(WARN, Logger::MEMORY)
              << "put 'numactl --interleave=all' in front of your command";
        }
      }
    } catch (...) {
      // file not found
    }
  }

#endif
}
