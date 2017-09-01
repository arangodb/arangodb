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
#include "ApplicationFeatures/MaxMapCountFeature.h"
#include "Basics/process-utils.h"
#include "Basics/FileUtils.h"
#include "Basics/StringUtils.h"
#include "Logger/Logger.h"
#include "RocksDBEngine/RocksDBEngine.h"
#include "StorageEngine/EngineSelectorFeature.h"

#ifdef __linux__
#include <sys/sysinfo.h>
#endif

using namespace arangodb;
using namespace arangodb::basics;

EnvironmentFeature::EnvironmentFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "Environment") {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Greetings");
  startsAfter("Logger");
  startsAfter("MaxMapCount");
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
  // test local ipv4 port range
  try {
    std::string value =
        basics::FileUtils::slurp("/proc/sys/net/ipv4/ip_local_port_range");

    std::vector<std::string> parts = basics::StringUtils::split(value, '\t');
    if (parts.size() == 2) {
      uint64_t lower = basics::StringUtils::uint64(parts[0]);
      uint64_t upper = basics::StringUtils::uint64(parts[1]);

      if (lower > upper || (upper - lower) < 16384) {
        LOG_TOPIC(WARN, arangodb::Logger::COMMUNICATION)
            << "local port range for ipv4/ipv6 ports is " << lower << " - " << upper
            << ", which does not look right. it is recommended to make at least 16K ports available";
        LOG_TOPIC(WARN, Logger::MEMORY) << "execute 'sudo bash -c \"echo -e \\\"32768\\t60999\\\" > "
                                           "/proc/sys/net/ipv4/ip_local_port_range\"' or use an even bigger port range";
      }
    }
  } catch (...) {
    // file not found or values not convertible into integers
  }

  // test value tcp_tw_recycle
  // https://vincent.bernat.im/en/blog/2014-tcp-time-wait-state-linux
  // https://stackoverflow.com/questions/8893888/dropping-of-connections-with-tcp-tw-recycle
  try {
    std::string value =
        basics::FileUtils::slurp("/proc/sys/net/ipv4/tcp_tw_recycle");
    uint64_t v = basics::StringUtils::uint64(value);
    if (v != 0) {
      LOG_TOPIC(WARN, Logger::COMMUNICATION)
          << "/proc/sys/net/ipv4/tcp_tw_recycle is enabled (" << v << ")"
          << "'. This can lead to all sorts of \"random\" network problems. "
          << "It is advised to leave it disabled (should be kernel default)";
      LOG_TOPIC(WARN, Logger::COMMUNICATION) << "execute 'sudo bash -c \"echo 0 > "
                                         "/proc/sys/net/ipv4/tcp_tw_recycle\"'";
    }
  } catch (...) {
    // file not found or value not convertible into integer
  }

#ifdef __GLIBC__
  // test presence of environment variable GLIBCXX_FORCE_NEW
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
  
  // test max_map_count
  if (MaxMapCountFeature::needsChecking()) {
    uint64_t actual = MaxMapCountFeature::actualMaxMappings();
    uint64_t expected = MaxMapCountFeature::minimumExpectedMaxMappings();

    if (actual < expected) {
      LOG_TOPIC(WARN, arangodb::Logger::MEMORY)
          << "maximum number of memory mappings per process is " << actual
          << ", which seems too low. it is recommended to set it to at least " << expected;
      LOG_TOPIC(WARN, Logger::MEMORY) << "execute 'sudo sysctl -w \"vm.max_map_count=" << expected << "\"'";
    }
  }

  // test zone_reclaim_mode
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

void EnvironmentFeature::start() {
#ifdef __linux__
  bool usingRocksDB =
    (EngineSelectorFeature::engineName() == RocksDBEngine::EngineName);
  try {
    std::string value =
        basics::FileUtils::slurp("/proc/sys/vm/overcommit_memory");
    uint64_t v = basics::StringUtils::uint64(value);
    // from https://www.kernel.org/doc/Documentation/sysctl/vm.txt:
    //
    //   When this flag is 0, the kernel attempts to estimate the amount
    //   of free memory left when userspace requests more memory.
    //   When this flag is 1, the kernel pretends there is always enough
    //   memory until it actually runs out.
    //   When this flag is 2, the kernel uses a "never overcommit"
    //   policy that attempts to prevent any overcommit of memory.
    std::string ratio =
        basics::FileUtils::slurp("/proc/sys/vm/overcommit_ratio");
    uint64_t r = basics::StringUtils::uint64(ratio);
    // from https://www.kernel.org/doc/Documentation/sysctl/vm.txt:
    //
    //  When overcommit_memory is set to 2, the committed address
    //  space is not permitted to exceed swap plus this percentage
    //  of physical RAM.

    if (usingRocksDB) {
      if (v != 2) {
        LOG_TOPIC(WARN, Logger::MEMORY)
          << "/proc/sys/vm/overcommit_memory is set to '" << v
          << "'. It is recommended to set it to a value of 2";
        LOG_TOPIC(WARN, Logger::MEMORY) << "execute 'sudo bash -c \"echo 2 > "
                                        << "/proc/sys/vm/overcommit_memory\"'";
      }
    } else {
      if (v == 1) {
        LOG_TOPIC(WARN, Logger::MEMORY)
          << "/proc/sys/vm/overcommit_memory is set to '" << v
          << "'. It is recommended to set it to a value of 0 or 2";
        LOG_TOPIC(WARN, Logger::MEMORY) << "execute 'sudo bash -c \"echo 2 > "
                                        << "/proc/sys/vm/overcommit_memory\"'";
      }
    }
    if (v == 2) {
      struct sysinfo info;
      int res = sysinfo(&info);
      if (res == 0) {
        double swapSpace = static_cast<double>(info.totalswap);
        double ram = static_cast<double>(TRI_PhysicalMemory);
        double rr = (ram >= swapSpace)
            ? 100.0 * ((ram - swapSpace) / ram)
            : 0.0;
        if (static_cast<double>(r) < 0.99 * rr) {
          LOG_TOPIC(WARN, Logger::MEMORY)
            << "/proc/sys/vm/overcommit_ratio is set to '" << r
            << "'. It is recommended to set it to at least'" << std::llround(rr)
            << "' (100 * (max(0, (RAM - Swap Space)) / RAM)) to utilize all "
            << "available RAM. Setting it to this value will minimize swap "
            << "usage, but may result in more out-of-memory errors, while "
            << "setting it to 100 will allow the system to use both all "
            << "available RAM and swap space.";
          LOG_TOPIC(WARN, Logger::MEMORY) << "execute 'sudo bash -c \"echo "
                                          << std::llround(rr) << " > "
                                          << "/proc/sys/vm/overcommit_ratio\"'";
        }
      }
    }
  } catch (...) {
    // file not found or value not convertible into integer
  }
#endif
}
