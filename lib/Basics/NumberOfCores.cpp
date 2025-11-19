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

#include "Basics/NumberOfCores.h"
#include "Basics/StringUtils.h"
#include "Basics/operating-system.h"
#include "Basics/files.h"
#include "Basics/FileUtils.h"
#include "Basics/CGroupDetection.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <cstdint>
#include <string>
#include <thread>
#include <sstream>

using namespace arangodb;

namespace {

std::size_t numberOfCoresImpl() {
#ifdef TRI_SC_NPROCESSORS_ONLN
  auto n = sysconf(_SC_NPROCESSORS_ONLN);

  if (n < 0) {
    n = 0;
  }

  if (n > 0) {
    return static_cast<std::size_t>(n);
  }
#endif

  return static_cast<std::size_t>(std::thread::hardware_concurrency());
}

std::size_t numberOfEffectiveCoresImpl() {
  auto const cgroup = cgroup::getVersion();
  int64_t quota = -1;
  int64_t period = -1;

  switch (cgroup) {
    case cgroup::Version::NONE: {
      break;
    }
    case cgroup::Version::V1: {
      try {
        auto quota = arangodb::basics::FileUtils::readCgroupFileValue(
            "/sys/fs/cgroup/cpu/cpu.cfs_quota_us");
        auto period = arangodb::basics::FileUtils::readCgroupFileValue(
            "/sys/fs/cgroup/cpu/cpu.cfs_period_us");
        if (quota && period) {
          if (*quota > 0 && *period > 0) {
            return *quota / *period;
          }
        }
      } catch (std::exception const& ex) {
        LOG_TOPIC("a3c21", INFO, arangodb::Logger::FIXME)
            << "Failed to determine the number of CPU cores from cgroup v1 "
               "cpu.cfs_quota_us/cpu.cfs_period_us files: "
            << ex.what();
      } catch (...) {
        LOG_TOPIC("a3c22", INFO, arangodb::Logger::FIXME)
            << "Failed to determine the number of CPU cores from cgroup v1 "
               "cpu.cfs_quota_us/cpu.cfs_period_us files: unknown error";
      }
      break;
    }
    case cgroup::Version::V2: {
      try {
        std::string content =
            arangodb::basics::FileUtils::slurp("/sys/fs/cgroup/cpu.max");
        std::istringstream stream(content);
        std::string quotaStr, periodStr;
        stream >> quotaStr >> periodStr;
        if (quotaStr != "max" && !quotaStr.empty() && !periodStr.empty()) {
          quota = std::stoll(quotaStr);
          period = std::stoll(periodStr);
          return quota / period;
        }
      } catch (std::exception const& ex) {
        LOG_TOPIC("a3c23", INFO, arangodb::Logger::FIXME)
            << "Failed to determine the number of CPU cores from cgroup v2 "
               "cpu.max file: "
            << ex.what();
      } catch (...) {
        LOG_TOPIC("a3c24", INFO, arangodb::Logger::FIXME)
            << "Failed to determine the number of CPU cores from cgroup v2 "
               "cpu.max file: unknown error";
      }
      break;
    }
  }

  return numberOfCoresImpl();
}

struct NumberOfCoresCache {
  NumberOfCoresCache()
      : cpuCores(numberOfCoresImpl()),
        effectiveCpuCores(numberOfEffectiveCoresImpl()),
        overridden(false) {
    std::string value;
    if (TRI_GETENV("ARANGODB_OVERRIDE_DETECTED_NUMBER_OF_CORES", value)) {
      if (!value.empty()) {
        uint64_t v = arangodb::basics::StringUtils::uint64(value);
        if (v != 0) {
          cpuCores = static_cast<std::size_t>(v);
          overridden = true;
        }
      }
    }
  }

  std::size_t cpuCores;
  std::size_t effectiveCpuCores;
  bool overridden;
};

NumberOfCoresCache const cache;

}  // namespace

/// @brief return number of cores from cache
std::size_t arangodb::NumberOfCores::getValue() { return ::cache.cpuCores; }

std::size_t arangodb::NumberOfCores::getEffectiveValue() {
  return ::cache.effectiveCpuCores;
}

/// @brief return if number of cores was overridden
bool arangodb::NumberOfCores::overridden() { return ::cache.overridden; }
