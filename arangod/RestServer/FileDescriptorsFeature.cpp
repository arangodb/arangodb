////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "FileDescriptorsFeature.h"

#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Scheduler/SchedulerFeature.h"

#ifdef TRI_HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

namespace arangodb {

uint64_t const FileDescriptorsFeature::RECOMMENDED = 8192;

FileDescriptorsFeature::FileDescriptorsFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "FileDescriptors"), _descriptorsMinimum(0) {
  setOptional(false);
  startsAfter<GreetingsFeaturePhase>();
}

void FileDescriptorsFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
#ifdef TRI_HAVE_GETRLIMIT
  options->addSection("server", "Server features");

  options->addOption("--server.descriptors-minimum",
                     "minimum number of file descriptors needed to start",
                     new UInt64Parameter(&_descriptorsMinimum));
#endif
}

void FileDescriptorsFeature::prepare() { adjustFileDescriptors(); }

#ifdef TRI_HAVE_GETRLIMIT
template <typename T>
static bool isUnlimited(T value) {
  auto max = std::numeric_limits<decltype(value)>::max();
  if (value == max || value == max / 2) {
    return true;
  }
  return false;
}

template <typename T>
static std::string StringifyLimitValue(T value) {
  if (isUnlimited(value)) {
    return "unlimited";
  }
  return std::to_string(value);
}
#endif

void FileDescriptorsFeature::start() {
#ifdef TRI_HAVE_GETRLIMIT
  struct rlimit rlim;
  int res = getrlimit(RLIMIT_NOFILE, &rlim);

  if (res == 0) {
    LOG_TOPIC("a1c60", INFO, arangodb::Logger::SYSCALL)
        << "file-descriptors (nofiles) hard limit is "
        << StringifyLimitValue(rlim.rlim_max) << ", soft limit is "
        << StringifyLimitValue(rlim.rlim_cur);
  }

  if (rlim.rlim_cur < RECOMMENDED) {
    LOG_TOPIC("8c771", WARN, arangodb::Logger::SYSCALL)
        << "file-descriptors limit is too low, currently "
        << StringifyLimitValue(rlim.rlim_cur) << ", please raise to at least "
        << RECOMMENDED << " (e.g. ulimit -n " << RECOMMENDED << ")";
  }
#endif
}

void FileDescriptorsFeature::adjustFileDescriptors() {
#ifdef TRI_HAVE_GETRLIMIT
  struct rlimit rlim;
  int res = getrlimit(RLIMIT_NOFILE, &rlim);

  if (res != 0) {
    LOG_TOPIC("17d7b", FATAL, arangodb::Logger::SYSCALL)
        << "cannot get the file descriptor limit: " << strerror(errno);
    FATAL_ERROR_EXIT();
  }

  LOG_TOPIC("6762c", DEBUG, arangodb::Logger::SYSCALL)
      << "file-descriptors (nofiles) hard limit is " << StringifyLimitValue(rlim.rlim_max)
      << ", soft limit is " << StringifyLimitValue(rlim.rlim_cur);

  uint64_t recommended = RECOMMENDED;
  uint64_t minimum = _descriptorsMinimum;

  if (minimum < recommended && 0 < minimum) {
    recommended = minimum;
  }

  if (rlim.rlim_max < recommended) {
    LOG_TOPIC("0835c", DEBUG, arangodb::Logger::SYSCALL)
        << "hard limit " << rlim.rlim_max << " is too small, trying to raise";

    rlim.rlim_max = recommended;
    rlim.rlim_cur = recommended;

    res = setrlimit(RLIMIT_NOFILE, &rlim);

    if (0 < minimum && minimum < recommended && res < 0) {
      rlim.rlim_max = minimum;
      rlim.rlim_cur = minimum;

      res = setrlimit(RLIMIT_NOFILE, &rlim);
    }

    if (0 < minimum && res < 0) {
      LOG_TOPIC("ba733", FATAL, arangodb::Logger::SYSCALL)
          << "cannot raise the file descriptor limit to " << minimum << ": "
          << strerror(errno);
      FATAL_ERROR_EXIT();
    }
  } else if (rlim.rlim_cur < recommended) {
    LOG_TOPIC("2940e", DEBUG, arangodb::Logger::SYSCALL)
        << "soft limit " << rlim.rlim_cur << " is too small, trying to raise";

    if (!isUnlimited(rlim.rlim_max) && recommended < rlim.rlim_max) {
      recommended = rlim.rlim_max;
    }

    rlim.rlim_cur = recommended;

    res = setrlimit(RLIMIT_NOFILE, &rlim);

    if (0 < minimum && minimum < recommended && res < 0) {
      rlim.rlim_cur = minimum;

      res = setrlimit(RLIMIT_NOFILE, &rlim);
    }

    if (0 < minimum && res < 0) {
      LOG_TOPIC("dd521", FATAL, arangodb::Logger::SYSCALL)
          << "cannot raise the file descriptor limit to " << minimum << ": "
          << strerror(errno);
      FATAL_ERROR_EXIT();
    }
  }
#endif
}

}  // namespace arangodb
