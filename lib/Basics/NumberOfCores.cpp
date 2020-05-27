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

#include "Basics/NumberOfCores.h"
#include "Basics/StringUtils.h"
#include "Basics/FileUtils.h"
#include "Basics/operating-system.h"
#include "Basics/files.h"

#ifdef TRI_HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <cmath>
#include <cstdint>
#include <string>
#include <thread>

using namespace arangodb;

namespace {

std::size_t numberOfCoresImpl() {
  int64_t n;
#ifdef TRI_SC_NPROCESSORS_ONLN
  n = sysconf(_SC_NPROCESSORS_ONLN);

  if (n < 0) {
    n = 0;
  }

  if (n <= 0)
#endif
    {
      n = std::thread::hardware_concurrency();
    }
#ifdef TRI_SC_NPROCESSORS_ONLN
  try {
    int64_t cfsPeriodUs = std::stoll(basics::FileUtils::slurp("/sys/fs/cgroup/cpu/cpu.cfs_period_us"));
    int64_t cfsQuotaUs = std::stoll(basics::FileUtils::slurp("/sys/fs/cgroup/cpu/cpu.cfs_quota_us"));
    if (cfsQuotaUs > 0 && cfsPeriodUs > 0) {
      n = std::lround(double(cfsQuotaUs) / double(cfsPeriodUs));
    }
  } catch (...) {
    // ignore errors due to wrong file access permissions, 
    // malformed input etc.
    // in this case we simply fall back to using the available
    // number of cores. this should not be an issue as the
    // count of cores detected is always logged at startup.
  }
#endif
  return static_cast<size_t>(n);
}

struct NumberOfCoresCache {
  NumberOfCoresCache() : cachedValue(numberOfCoresImpl()), overridden(false) {
    std::string value;
    if (TRI_GETENV("ARANGODB_OVERRIDE_DETECTED_NUMBER_OF_CORES", value)) {
      if (!value.empty()) {
        uint64_t v = arangodb::basics::StringUtils::uint64(value);
        if (v != 0) {
          cachedValue = static_cast<std::size_t>(v);
          overridden = true;
        }
      }
    }
    if (cachedValue == 0) {
      cachedValue = 1;
    }
  }

  std::size_t cachedValue;
  bool overridden;
};

NumberOfCoresCache const cache;

} // namespace

/// @brief return number of cores from cache
std::size_t arangodb::NumberOfCores::getValue() {
  return ::cache.cachedValue;
}

/// @brief return if number of cores was overridden
bool arangodb::NumberOfCores::overridden() {
  return ::cache.overridden;
}
