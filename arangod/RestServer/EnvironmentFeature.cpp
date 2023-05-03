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
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "EnvironmentFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/FileUtils.h"
#include "Basics/NumberOfCores.h"
#include "Basics/PhysicalMemory.h"
#include "Basics/Result.h"
#include "Basics/StringUtils.h"
#include "Basics/application-exit.h"
#include "Basics/operating-system.h"
#include "Basics/process-utils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "RestServer/MaxMapCountFeature.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <vector>

#ifdef __linux__
#include <sys/sysinfo.h>
#include <unistd.h>
#endif

#include <absl/strings/str_cat.h>

namespace {
#ifdef __linux__
std::string_view trimProcName(std::string_view content) {
  std::size_t pos = content.find(' ');
  if (pos != std::string_view::npos && pos + 1 < content.size()) {
    std::size_t pos2 = std::string_view::npos;
    if (content[++pos] == '(') {
      ++pos;
      if (pos + 1 < content.size()) {
        pos2 = content.find(')', pos);
      }
    } else {
      pos2 = content.find(' ', pos);
    }
    if (pos2 != std::string_view::npos) {
      return content.substr(pos, pos2 - pos);
    }
  }
  return {};
}
#endif
}  // namespace

using namespace arangodb::basics;

namespace arangodb {
class OptionsCheckFeature;
class SharedPRNGFeature;

EnvironmentFeature::EnvironmentFeature(Server& server)
    : ArangodFeature{server, *this} {
  setOptional(true);
  startsAfter<application_features::GreetingsFeaturePhase>();

  startsAfter<LogBufferFeature>();
  startsAfter<MaxMapCountFeature>();
  startsAfter<OptionsCheckFeature>();
  startsAfter<SharedPRNGFeature>();
}

void EnvironmentFeature::prepare() {
#ifdef __linux__
  _operatingSystem = "linux";
  try {
    std::string const versionFilename("/proc/version");

    if (basics::FileUtils::exists(versionFilename)) {
      _operatingSystem =
          basics::StringUtils::trim(basics::FileUtils::slurp(versionFilename));
    }
  } catch (...) {
    // ignore any errors as the log output is just informational
  }
#elif _WIN32
  // TODO: improve Windows version detection
  _operatingSystem = "windows";
#elif __APPLE__
  // TODO: improve MacOS version detection
  _operatingSystem = "macos";
#else
  _operatingSystem = "unknown";
#endif

  // find parent process id and name
  std::string parent;
#ifdef __linux__
  try {
    pid_t parentId = getppid();
    if (parentId) {
      parent = ", parent process: " + std::to_string(parentId);
      std::string const procFilename =
          absl::StrCat("/proc/", parentId, "/stat");

      if (basics::FileUtils::exists(procFilename)) {
        std::string content = basics::FileUtils::slurp(procFilename);
        std::string_view procName = ::trimProcName(content);
        if (!procName.empty()) {
          parent += absl::StrCat(" (", procName, ")");
        }
      }
    }
  } catch (...) {
  }
#endif

  LOG_TOPIC("75ddc", INFO, Logger::FIXME)
      << "detected operating system: " << _operatingSystem << parent;

  if (sizeof(void*) == 4) {
    // 32 bit build
    LOG_TOPIC("ae57c", WARN, arangodb::Logger::MEMORY)
        << "this is a 32 bit build of ArangoDB, which is unsupported. "
        << "it is recommended to run a 64 bit build instead because it can "
        << "address significantly bigger regions of memory";
  }

#ifdef __linux__
#if defined(__arm__) || defined(__arm64__) || defined(__aarch64__)
  // detect alignment settings for ARM
  {
    LOG_TOPIC("6aec3", TRACE, arangodb::Logger::MEMORY)
        << "running CPU alignment check";
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
      if (basics::FileUtils::exists(filename)) {
        std::string const cpuAlignment = basics::FileUtils::slurp(filename);
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
            if (cpuAlignment[end] < '0' || cpuAlignment[end] > '9') {
              ++end;
              break;
            }
            ++end;
          }

          int64_t alignment =
              std::stol(std::string(cpuAlignment.c_str() + start, end - start));
          if ((alignment & 2) == 0) {
            LOG_TOPIC("f1bb9", FATAL, arangodb::Logger::MEMORY)
                << "possibly incompatible CPU alignment settings found in '"
                << filename
                << "'. this may cause arangod to abort with "
                   "SIGBUS. please set the value in '"
                << filename << "' to 2";
            FATAL_ERROR_EXIT();
          }

          alignmentDetected = true;
        }
      } else {
        // if the file /proc/cpu/alignment does not exist, we should not
        // warn about it
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
          << "unable to detect CPU alignment settings. could not process file '"
          << filename
          << "'. this may cause arangod to abort with SIGBUS. it may be "
             "necessary to set the value in '"
          << filename << "' to 2";
    }

    std::string const cpuInfoFilename("/proc/cpuinfo");
    try {
      if (basics::FileUtils::exists(cpuInfoFilename)) {
        std::string const cpuInfo = basics::FileUtils::slurp(cpuInfoFilename);
        auto start = cpuInfo.find("ARMv6");

        if (start != std::string::npos) {
          LOG_TOPIC("0cfa9", FATAL, arangodb::Logger::MEMORY)
              << "possibly incompatible ARMv6 CPU detected.";
          FATAL_ERROR_EXIT();
        }
      }
    } catch (...) {
      // ignore that we cannot detect the alignment
      LOG_TOPIC("a8305", TRACE, arangodb::Logger::MEMORY)
          << "unable to detect CPU type '" << filename << "'";
    }
  }
#endif
#endif

#ifdef __linux__
#ifdef ARANGODB_HAVE_JEMALLOC
  {
    char const* v = getenv("LD_PRELOAD");
    if (v != nullptr && (strstr(v, "/valgrind/") != nullptr ||
                         strstr(v, "/vgpreload") != nullptr)) {
      // smells like Valgrind
      LOG_TOPIC("a2a1e", WARN, arangodb::Logger::MEMORY)
          << "found LD_PRELOAD env variable value that looks like we are "
             "running under Valgrind. "
          << "this is unsupported in combination with jemalloc and may cause "
             "undefined behavior at least with memcheck!";
    }
  }
#endif
#endif

  {
    char const* v = getenv("MALLOC_CONF");

    if (v != nullptr) {
      // report value of MALLOC_CONF environment variable
      LOG_TOPIC("d89f7", WARN, arangodb::Logger::MEMORY)
          << "found custom MALLOC_CONF environment value: " << v;
    }
  }

#ifdef __linux__
  // check overcommit_memory & overcommit_ratio
  try {
    std::string const memoryFilename("/proc/sys/vm/overcommit_memory");

    std::string content = basics::FileUtils::slurp(memoryFilename);
    uint64_t v = basics::StringUtils::uint64(content);

    if (v == 2) {
#ifdef ARANGODB_HAVE_JEMALLOC
      LOG_TOPIC("fadc5", WARN, arangodb::Logger::MEMORY)
          << memoryFilename
          << " is set to a value of 2. this "
             "setting has been found to be problematic";
      LOG_TOPIC("d08d6", WARN, Logger::MEMORY)
          << "execute 'sudo bash -c \"echo 0 > " << memoryFilename << "\"'";
#endif

      // from https://www.kernel.org/doc/Documentation/sysctl/vm.txt:
      //
      //   When this flag is 0, the kernel attempts to estimate the amount
      //   of free memory left when userspace requests more memory.
      //   When this flag is 1, the kernel pretends there is always enough
      //   memory until it actually runs out.
      //   When this flag is 2, the kernel uses a "never overcommit"
      //   policy that attempts to prevent any overcommit of memory.
      std::string const ratioFilename("/proc/sys/vm/overcommit_ratio");

      if (basics::FileUtils::exists(ratioFilename)) {
        content = basics::FileUtils::slurp(ratioFilename);
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
          double rr =
              (ram >= swapSpace) ? 100.0 * ((ram - swapSpace) / ram) : 0.0;
          if (static_cast<double>(r) < 0.99 * rr) {
            LOG_TOPIC("b0a75", WARN, Logger::MEMORY)
                << ratioFilename << " is set to '" << r
                << "'. It is recommended to set it to at least '"
                << std::llround(rr)
                << "' (100 * (max(0, (RAM - Swap Space)) / RAM)) to utilize "
                   "all "
                << "available RAM. Setting it to this value will minimize "
                   "swap "
                << "usage, but may result in more out-of-memory errors, "
                   "while "
                << "setting it to 100 will allow the system to use both all "
                << "available RAM and swap space.";
            LOG_TOPIC("1041e", WARN, Logger::MEMORY)
                << "execute 'sudo bash -c \"echo " << std::llround(rr) << " > "
                << ratioFilename << "\"'";
          }
        }
      }
    }
  } catch (...) {
    // file not found or value not convertible into integer
  }
#endif

  // Report memory and CPUs found:
  LOG_TOPIC("25362", INFO, Logger::MEMORY)
      << "Available physical memory: " << PhysicalMemory::getValue() << " bytes"
      << (PhysicalMemory::overridden() ? " (overriden by environment variable)"
                                       : "")
      << ", available cores: " << NumberOfCores::getValue()
      << (NumberOfCores::overridden() ? " (overriden by environment variable)"
                                      : "");

#ifdef __linux__
  // test local ipv6 support
  try {
    std::string const ipV6Filename("/proc/net/if_inet6");

    if (!basics::FileUtils::exists(ipV6Filename)) {
      LOG_TOPIC("0f48d", INFO, arangodb::Logger::COMMUNICATION)
          << "IPv6 support seems to be disabled";
    }
  } catch (...) {
    // file not found
  }

  // test local ipv4 port range
  try {
    std::string const portFilename("/proc/sys/net/ipv4/ip_local_port_range");

    if (basics::FileUtils::exists(portFilename)) {
      std::string content = basics::FileUtils::slurp(portFilename);
      auto parts = basics::StringUtils::split(content, '\t');
      if (parts.size() == 2) {
        uint64_t lower = basics::StringUtils::uint64(parts[0]);
        uint64_t upper = basics::StringUtils::uint64(parts[1]);

        if (lower > upper || (upper - lower) < 16384) {
          LOG_TOPIC("721da", WARN, arangodb::Logger::COMMUNICATION)
              << "local port range for ipv4/ipv6 ports is " << lower << " - "
              << upper
              << ", which does not look right. it is recommended to make at "
                 "least 16K ports available";
          LOG_TOPIC("eb911", WARN, Logger::MEMORY)
              << "execute 'sudo bash -c \"echo -e \\\"32768\\t60999\\\" "
                 "> "
              << portFilename
              << "\"' or use an even "
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
    std::string const recycleFilename("/proc/sys/net/ipv4/tcp_tw_recycle");

    if (basics::FileUtils::exists(recycleFilename)) {
      std::string content = basics::FileUtils::slurp(recycleFilename);
      uint64_t v = basics::StringUtils::uint64(content);
      if (v != 0) {
        LOG_TOPIC("c277c", WARN, Logger::COMMUNICATION)
            << recycleFilename << " is enabled(" << v << ") "
            << "'. This can lead to all sorts of \"random\" network problems. "
            << "It is advised to leave it disabled (should be kernel default)";
        LOG_TOPIC("29333", WARN, Logger::COMMUNICATION)
            << "execute 'sudo bash -c \"echo 0 > " << recycleFilename << "\"'";
      }
    }
  } catch (...) {
    // file not found or value not convertible into integer
  }

  // test max_map_count
  if (MaxMapCountFeature::needsChecking()) {
    uint64_t actual = MaxMapCountFeature::actualMaxMappings();
    uint64_t expected = MaxMapCountFeature::minimumExpectedMaxMappings();

    if (actual < expected) {
      LOG_TOPIC("118b0", WARN, arangodb::Logger::MEMORY)
          << "maximum number of memory mappings per process is " << actual
          << ", which seems too low. it is recommended to set it to at least "
          << expected;
      LOG_TOPIC("49528", WARN, Logger::MEMORY)
          << "execute 'sudo sysctl -w \"vm.max_map_count=" << expected << "\"'";
    }
  }

  // test zone_reclaim_mode
  try {
    std::string const reclaimFilename("/proc/sys/vm/zone_reclaim_mode");

    if (basics::FileUtils::exists(reclaimFilename)) {
      std::string content = basics::FileUtils::slurp(reclaimFilename);
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
        LOG_TOPIC("7a7af", WARN, Logger::MEMORY)
            << reclaimFilename << " is set to '" << v
            << "'. It is recommended to set it to a value of 0";
        LOG_TOPIC("11b2b", WARN, Logger::MEMORY)
            << "execute 'sudo bash -c \"echo 0 > " << reclaimFilename << "\"'";
      }
    }
  } catch (...) {
    // file not found or value not convertible into integer
  }

  std::array<std::string, 2> paths = {
      "/sys/kernel/mm/transparent_hugepage/enabled",
      "/sys/kernel/mm/transparent_hugepage/defrag"};

  for (auto const& file : paths) {
    try {
      if (basics::FileUtils::exists(file)) {
        std::string value = basics::FileUtils::slurp(file);
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
            LOG_TOPIC("f3108", WARN, Logger::MEMORY)
                << "execute 'sudo bash -c \"echo madvise > " << file << "\"'";
          }
        }
      }
    } catch (...) {
      // file not found
    }
  }

  bool numa = FileUtils::exists("/sys/devices/system/node/node1");

  if (numa) {
    try {
      std::string const mapsFilename("/proc/self/numa_maps");

      if (basics::FileUtils::exists(mapsFilename)) {
        std::string content = basics::FileUtils::slurp(mapsFilename);
        auto values = basics::StringUtils::split(content, '\n');

        if (!values.empty()) {
          auto first = values[0];
          auto where = first.find(' ');

          if (where != std::string::npos &&
              !first.substr(where).starts_with(" interleave")) {
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
    std::string const settingsFilename("/proc/sys/kernel/randomize_va_space");

    if (basics::FileUtils::exists(settingsFilename)) {
      std::string content = basics::FileUtils::slurp(settingsFilename);
      uint64_t v = basics::StringUtils::uint64(content);
      // from man proc:
      //
      // 0 – No randomization. Everything is static.
      // 1 – Conservative randomization. Shared libraries, stack, mmap(), VDSO
      // and heap are randomized. 2 – Full randomization. In addition to
      // elements listed in the previous point, memory managed through brk() is
      // also randomized.
      std::string_view s;
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
      if (!s.empty()) {
        LOG_TOPIC("63a7a", DEBUG, Logger::FIXME)
            << "host ASLR is in use for " << s;
      }
    }
  } catch (...) {
    // file not found or value not convertible into integer
  }

#endif
}

}  // namespace arangodb
