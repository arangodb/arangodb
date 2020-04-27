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

#include <stdlib.h>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

#include "EnvironmentFeature.h"

#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "ApplicationFeatures/MaxMapCountFeature.h"
#include "Basics/FileUtils.h"
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/Result.h"
#include "Basics/StringUtils.h"
#include "Basics/operating-system.h"
#include "Basics/process-utils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#ifdef __linux__
#include <sys/sysinfo.h>
#endif

using namespace arangodb::basics;

namespace arangodb {

EnvironmentFeature::EnvironmentFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "Environment") {
  setOptional(true);
  startsAfter<application_features::GreetingsFeaturePhase>();

  startsAfter<MaxMapCountFeature>();
}

void EnvironmentFeature::prepare() {
#ifdef __linux__
  try {
    if (basics::FileUtils::exists("/proc/version")) {
      std::string content;
      auto rv = basics::FileUtils::slurp("/proc/version", content);
      if (rv.ok()) {
        std::string value = basics::StringUtils::trim(content);
        LOG_TOPIC("75ddc", INFO, Logger::FIXME) << "detected operating system: " << value;
      }
    }
  } catch (...) {
    // ignore any errors as the log output is just informational
  }
#endif

  if (sizeof(void*) == 4) {
    // 32 bit build
    LOG_TOPIC("ae57c", WARN, arangodb::Logger::MEMORY)
        << "this is a 32 bit build of ArangoDB, which is unsupported. "
        << "it is recommended to run a 64 bit build instead because it can "
        << "address significantly bigger regions of memory";
  }

#ifdef __arm__
  // detect alignment settings for ARM
  {
    LOG_TOPIC("6aec3", TRACE, arangodb::Logger::MEMORY) << "running CPU alignment check";
    // To change the alignment trap behavior, simply echo a number into
    // /proc/cpu/alignment.  The number is made up from various bits:
    //
    // bit             behavior when set
    // ---             -----------------
    //
    // 0               A user process performing an unaligned memory access
    //                 will cause the kernel to print a message indicating
    //                 process name, pid, pc, instruction, address, and the
    //                 fault code.
    //
    // 1               The kernel will attempt to fix up the user process
    //                 performing the unaligned access.  This is of course
    //                 slow (think about the floating point emulator) and
    //                 not recommended for production use.
    //
    // 2               The kernel will send a SIGBUS signal to the user process
    //                 performing the unaligned access.
    bool alignmentDetected = false;

    std::string const filename("/proc/cpu/alignment");
    try {
      std::string const cpuAlignment = arangodb::basics::FileUtils::slurp(filename);
      auto start = cpuAlignment.find("User faults:");

      if (start != std::string::npos) {
        start += strlen("User faults:");
        size_t end = start;
        while (end < cpuAlignment.size()) {
          if (cpuAlignment[end] == ' ' || cpuAlignment[end] == '\t') {
            ++end;
          } else {
            break;
          }
        }
        while (end < cpuAlignment.size()) {
          ++end;
          if (cpuAlignment[end] < '0' || cpuAlignment[end] > '9') {
            break;
          }
        }

        int64_t alignment =
            std::stol(std::string(cpuAlignment.c_str() + start, end - start));
        if ((alignment & 2) == 0) {
          LOG_TOPIC("f1bb9", FATAL, arangodb::Logger::MEMORY)
              << "possibly incompatible CPU alignment settings found in '" << filename
              << "'. this may cause arangod to abort with "
                 "SIGBUS. please set the value in '"
              << filename << "' to 2";
          FATAL_ERROR_EXIT();
        }

        alignmentDetected = true;
      }

    } catch (...) {
      // ignore that we cannot detect the alignment
      LOG_TOPIC("14b8a", TRACE, arangodb::Logger::MEMORY)
          << "unable to detect CPU alignment settings. could not process file '"
          << filename << "'";
    }

    if (!alignmentDetected) {
      LOG_TOPIC("b8a20", WARN, arangodb::Logger::MEMORY)
          << "unable to detect CPU alignment settings. could not process file '" << filename
          << "'. this may cause arangod to abort with SIGBUS. it may be "
             "necessary to set the value in '"
          << filename << "' to 2";
    }
    std::string const proc_cpuinfo_filename("/proc/cpuinfo");
    try {
      std::string const cpuInfo = arangodb::basics::FileUtils::slurp(proc_cpuinfo_filename);
      auto start = cpuInfo.find("ARMv6");

      if (start != std::string::npos) {
        LOG_TOPIC("0cfa9", FATAL, arangodb::Logger::MEMORY)
            << "possibly incompatible ARMv6 CPU detected.";
        FATAL_ERROR_EXIT();
      }
    } catch (...) {
      // ignore that we cannot detect the alignment
      LOG_TOPIC("a8305", TRACE, arangodb::Logger::MEMORY)
          << "unable to detect CPU type '" << filename << "'";
    }
  }
#endif

#ifdef __linux__
  {
    char const* v = getenv("MALLOC_CONF");

    if (v != nullptr) {
      // report value of MALLOC_CONF environment variable
      LOG_TOPIC("d89f7", WARN, arangodb::Logger::MEMORY)
          << "found custom MALLOC_CONF environment value '" << v << "'";
    }
  }

  // check overcommit_memory & overcommit_ratio
  try {
    std::string content;
    auto rv = basics::FileUtils::slurp("/proc/sys/vm/overcommit_memory", content);
    if (rv.ok()) {
      uint64_t v = basics::StringUtils::uint64(content);

      if (v == 2) {
#ifdef ARANGODB_HAVE_JEMALLOC
        LOG_TOPIC("fadc5", WARN, arangodb::Logger::MEMORY)
            << "/proc/sys/vm/overcommit_memory is set to a value of 2. this "
               "setting has been found to be problematic";
        LOG_TOPIC("d08d6", WARN, Logger::MEMORY)
            << "execute 'sudo bash -c \"echo 0 > "
            << "/proc/sys/vm/overcommit_memory\"'";
#endif

        // from https://www.kernel.org/doc/Documentation/sysctl/vm.txt:
        //
        //   When this flag is 0, the kernel attempts to estimate the amount
        //   of free memory left when userspace requests more memory.
        //   When this flag is 1, the kernel pretends there is always enough
        //   memory until it actually runs out.
        //   When this flag is 2, the kernel uses a "never overcommit"
        //   policy that attempts to prevent any overcommit of memory.
        rv = basics::FileUtils::slurp("/proc/sys/vm/overcommit_ratio", content);
        if (rv.ok()) {
          uint64_t r = basics::StringUtils::uint64(content);
          // from https://www.kernel.org/doc/Documentation/sysctl/vm.txt:
          //
          //  When overcommit_memory is set to 2, the committed address
          //  space is not permitted to exceed swap plus this percentage
          //  of physical RAM.

          struct sysinfo info;
          int res = sysinfo(&info);
          if (res == 0) {
            double swapSpace = static_cast<double>(info.totalswap);
            double ram = static_cast<double>(PhysicalMemory::getValue());
            double rr = (ram >= swapSpace) ? 100.0 * ((ram - swapSpace) / ram) : 0.0;
            if (static_cast<double>(r) < 0.99 * rr) {
              LOG_TOPIC("b0a75", WARN, Logger::MEMORY)
                  << "/proc/sys/vm/overcommit_ratio is set to '" << r
                  << "'. It is recommended to set it to at least '"
                  << std::llround(rr) << "' (100 * (max(0, (RAM - Swap Space)) / RAM)) to utilize all "
                  << "available RAM. Setting it to this value will minimize "
                     "swap "
                  << "usage, but may result in more out-of-memory errors, "
                     "while "
                  << "setting it to 100 will allow the system to use both all "
                  << "available RAM and swap space.";
              LOG_TOPIC("1041e", WARN, Logger::MEMORY)
                  << "execute 'sudo bash -c \"echo " << std::llround(rr) << " > "
                  << "/proc/sys/vm/overcommit_ratio\"'";
            }
          }
        }
      }
    }
  } catch (...) {
    // file not found or value not convertible into integer
  }

  // Report memory and CPUs found:
  LOG_TOPIC("25362", INFO, Logger::MEMORY)
    << "Available physical memory: " 
    << PhysicalMemory::getValue() << " bytes" 
    << (PhysicalMemory::overridden() ? " (overriden by environment variable)" : "")
    << ", available cores: " << NumberOfCores::getValue()
    << (NumberOfCores::overridden() ? " (overriden by environment variable)" : "");

  // test local ipv6 support
  try {
    if (!basics::FileUtils::exists("/proc/net/if_inet6")) {
      LOG_TOPIC("0f48d", INFO, arangodb::Logger::COMMUNICATION)
          << "IPv6 support seems to be disabled";
    }
  } catch (...) {
    // file not found
  }

  // test local ipv4 port range
  try {
    std::string content;
    auto rv = basics::FileUtils::slurp("/proc/sys/net/ipv4/ip_local_port_range", content);
    if (rv.ok()) {
      std::vector<std::string> parts = basics::StringUtils::split(content, '\t');
      if (parts.size() == 2) {
        uint64_t lower = basics::StringUtils::uint64(parts[0]);
        uint64_t upper = basics::StringUtils::uint64(parts[1]);

        if (lower > upper || (upper - lower) < 16384) {
          LOG_TOPIC("721da", WARN, arangodb::Logger::COMMUNICATION)
              << "local port range for ipv4/ipv6 ports is " << lower << " - " << upper
              << ", which does not look right. it is recommended to make at "
                 "least 16K ports available";
          LOG_TOPIC("eb911", WARN, Logger::MEMORY)
              << "execute 'sudo bash -c \"echo -e \\\"32768\\t60999\\\" > "
                 "/proc/sys/net/ipv4/ip_local_port_range\"' or use an even "
                 "bigger port range";
        }
      }
    }
  } catch (...) {
    // file not found or values not convertible into integers
  }

  // test value tcp_tw_recycle
  // https://vincent.bernat.im/en/blog/2014-tcp-time-wait-state-linux
  // https://stackoverflow.com/questions/8893888/dropping-of-connections-with-tcp-tw-recycle
  try {
    std::string content;
    auto rv = basics::FileUtils::slurp("/proc/sys/net/ipv4/tcp_tw_recycle", content);
    if (rv.ok()) {
      uint64_t v = basics::StringUtils::uint64(content);
      if (v != 0) {
        LOG_TOPIC("c277c", WARN, Logger::COMMUNICATION)
            << "/proc/sys/net/ipv4/tcp_tw_recycle is enabled (" << v << ")"
            << "'. This can lead to all sorts of \"random\" network problems. "
            << "It is advised to leave it disabled (should be kernel default)";
        LOG_TOPIC("29333", WARN, Logger::COMMUNICATION)
            << "execute 'sudo bash -c \"echo 0 > "
               "/proc/sys/net/ipv4/tcp_tw_recycle\"'";
      }
    }
  } catch (...) {
    // file not found or value not convertible into integer
  }

#ifdef __GLIBC__
  {
    // test presence of environment variable GLIBCXX_FORCE_NEW
    char const* v = getenv("GLIBCXX_FORCE_NEW");

    if (v == nullptr) {
      // environment variable not set
      LOG_TOPIC("3909f", WARN, arangodb::Logger::MEMORY)
          << "environment variable GLIBCXX_FORCE_NEW' is not set. "
          << "it is recommended to set it to some value to avoid unnecessary "
             "memory pooling in glibc++";
      LOG_TOPIC("56d59", WARN, arangodb::Logger::MEMORY)
          << "execute 'export GLIBCXX_FORCE_NEW=1'";
    }
  }
#endif

  // test max_map_count
  if (MaxMapCountFeature::needsChecking()) {
    uint64_t actual = MaxMapCountFeature::actualMaxMappings();
    uint64_t expected = MaxMapCountFeature::minimumExpectedMaxMappings();

    if (actual < expected) {
      LOG_TOPIC("118b0", WARN, arangodb::Logger::MEMORY)
          << "maximum number of memory mappings per process is " << actual
          << ", which seems too low. it is recommended to set it to at least " << expected;
      LOG_TOPIC("49528", WARN, Logger::MEMORY)
          << "execute 'sudo sysctl -w \"vm.max_map_count=" << expected << "\"'";
    }
  }

  // test zone_reclaim_mode
  try {
    std::string content;
    auto rv = basics::FileUtils::slurp("/proc/sys/vm/zone_reclaim_mode", content);
    if (rv.ok()) {
      uint64_t v = basics::StringUtils::uint64(content);
      if (v != 0) {
        // from https://www.kernel.org/doc/Documentation/sysctl/vm.txt:
        //
        //    This is value ORed together of
        //    1 = Zone reclaim on
        //    2 = Zone reclaim writes dirty pages out
        //    4 = Zone reclaim swaps pages
        //
        // https://www.poempelfox.de/blog/2010/03/19/
        LOG_TOPIC("7a7af", WARN, Logger::PERFORMANCE)
            << "/proc/sys/vm/zone_reclaim_mode is set to '" << v
            << "'. It is recommended to set it to a value of 0";
        LOG_TOPIC("11b2b", WARN, Logger::PERFORMANCE)
            << "execute 'sudo bash -c \"echo 0 > "
               "/proc/sys/vm/zone_reclaim_mode\"'";
      }
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
      std::string value;
      auto rv = basics::FileUtils::slurp(file, value);
      if (rv.ok()) {
        size_t start = value.find('[');
        size_t end = value.find(']');

        if (start != std::string::npos && end != std::string::npos &&
            start < end && end - start >= 4) {
          value = value.substr(start + 1, end - start - 1);
          if (value == "always") {
            LOG_TOPIC("e8b68", WARN, Logger::MEMORY)
                << file << " is set to '" << value
                << "'. It is recommended to set it to a value of 'never' "
                   "or 'madvise'";
            showHuge = true;
          }
        }
      }
    } catch (...) {
      // file not found
    }
  }

  if (showHuge) {
    for (auto file : paths) {
      LOG_TOPIC("f3108", WARN, Logger::MEMORY)
          << "execute 'sudo bash -c \"echo madvise > " << file << "\"'";
    }
  }

  bool numa = FileUtils::exists("/sys/devices/system/node/node1");

  if (numa) {
    try {
      std::string content;
      auto rv = basics::FileUtils::slurp("/proc/self/numa_maps", content);
      if (rv.ok()) {
        auto values = basics::StringUtils::split(content, '\n');

        if (!values.empty()) {
          auto first = values[0];
          auto where = first.find(' ');

          if (where != std::string::npos &&
              !StringUtils::isPrefix(first.substr(where), " interleave")) {
            LOG_TOPIC("3e451", WARN, Logger::MEMORY)
                << "It is recommended to set NUMA to interleaved.";
            LOG_TOPIC("b25a4", WARN, Logger::MEMORY)
                << "put 'numactl --interleave=all' in front of your command";
          }
        }
      }
    } catch (...) {
      // file not found
    }
  }

  // check kernel ASLR settings
  try {
    std::string content;
    auto rv = basics::FileUtils::slurp("/proc/sys/kernel/randomize_va_space", content);
    if (rv.ok()) {
      uint64_t v = basics::StringUtils::uint64(content);
      // from man proc:
      //
      // 0 – No randomization. Everything is static.
      // 1 – Conservative randomization. Shared libraries, stack, mmap(), VDSO
      // and heap are randomized. 2 – Full randomization. In addition to
      // elements listed in the previous point, memory managed through brk() is
      // also randomized.
      char const* s = nullptr;
      switch (v) {
        case 0:
          s = "nothing";
          break;
        case 1:
          s = "shared libraries, stack, mmap, VDSO and heap";
          break;
        case 2:
          s = "shared libraries, stack, mmap, VDSO, heap and memory managed "
              "through brk()";
          break;
      }
      if (s != nullptr) {
        LOG_TOPIC("63a7a", DEBUG, Logger::FIXME) << "host ASLR is in use for " << s;
      }
    }
  } catch (...) {
    // file not found or value not convertible into integer
  }

#endif
}

}  // namespace arangodb
