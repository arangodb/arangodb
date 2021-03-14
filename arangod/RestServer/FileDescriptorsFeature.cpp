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

#include "FileDescriptorsFeature.h"

#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/exitcodes.h"
#include "Basics/application-exit.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"

#ifdef TRI_HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <cstdlib>
#include <cstring>
#include <string>
#include <sstream>

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

#ifdef TRI_HAVE_GETRLIMIT
namespace arangodb {

struct FileDescriptors {
  static constexpr rlim_t requiredMinimum = 1024;

  rlim_t hard;
  rlim_t soft;
  
  static FileDescriptors load() {
    struct rlimit rlim;
    int res = getrlimit(RLIMIT_NOFILE, &rlim);

    if (res != 0) {
      LOG_TOPIC("17d7b", FATAL, arangodb::Logger::SYSCALL)
          << "cannot get the file descriptors limit value: " << strerror(errno);
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_RESOURCES_TOO_LOW);
    }

    return {rlim.rlim_max, rlim.rlim_cur};
  }

  int store() {
    struct rlimit rlim;
    rlim.rlim_max = hard;
    rlim.rlim_cur = soft;

    return setrlimit(RLIMIT_NOFILE, &rlim);
  }

  static rlim_t recommendedMinimum() {
    // check if we are running under Valgrind...
    char const* v = getenv("LD_PRELOAD");
    if (v != nullptr &&
        (strstr(v, "/valgrind/") != nullptr || strstr(v, "/vgpreload") != nullptr)) {
      // valgrind will somehow reset the ulimit values to some very low values.
      return requiredMinimum;
    }

#ifdef __APPLE__
    // some MacOS versions disallow raising file descriptor limits higher than this 🙄
    return 8192;
#else
    // on Linux, we will also use 8192 for now. this should be low enough so that it
    // doesn't cause too much trouble when upgrading. however, this is not a high 
    // enough value to operate with larger amounts of data! it is a MINIMUM!
    return 8192;
#endif
  }

  static bool isUnlimited(rlim_t value) {
    auto max = std::numeric_limits<decltype(value)>::max();
    return (value == max || value == max / 2);
  }

  static std::string stringify(rlim_t value) {
    if (isUnlimited(value)) {
      return "unlimited";
    }
    return std::to_string(value);
  }
};

FileDescriptorsFeature::FileDescriptorsFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "FileDescriptors"), 
      _descriptorsMinimum(FileDescriptors::recommendedMinimum()) {
  setOptional(false);
  startsAfter<GreetingsFeaturePhase>();
}

void FileDescriptorsFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("server", "Server features");

  options->addOption("--server.descriptors-minimum",
                     "minimum number of file descriptors needed to start (0 = no minimum)",
                     new UInt64Parameter(&_descriptorsMinimum),
                     arangodb::options::makeFlags(arangodb::options::Flags::DefaultNoOs, arangodb::options::Flags::OsLinux, arangodb::options::Flags::OsMac));
}

void FileDescriptorsFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  if (_descriptorsMinimum > 0 && 
      (_descriptorsMinimum < FileDescriptors::requiredMinimum ||
       _descriptorsMinimum > std::numeric_limits<rlim_t>::max())) {
    LOG_TOPIC("7e15c", FATAL, Logger::STARTUP) 
      << "invalid value for --server.descriptors-minimum. must be between " 
      << FileDescriptors::requiredMinimum << " and "
      << std::numeric_limits<rlim_t>::max(); 
    FATAL_ERROR_EXIT();
  }
}

void FileDescriptorsFeature::prepare() { adjustFileDescriptors(); }

void FileDescriptorsFeature::start() {
  FileDescriptors current = FileDescriptors::load();

  LOG_TOPIC("a1c60", INFO, arangodb::Logger::SYSCALL)
      << "file-descriptors (nofiles) hard limit is "
      << FileDescriptors::stringify(current.hard) 
      << ", soft limit is "
      << FileDescriptors::stringify(current.soft);

  rlim_t const required = std::max<rlim_t>(
    static_cast<rlim_t>(_descriptorsMinimum),
    FileDescriptors::requiredMinimum
  );

  if (current.soft < required) {
    std::stringstream s;
    s << "file-descriptors (nofiles) soft limit is too low, currently "
      << FileDescriptors::stringify(current.soft) 
      << ". please raise to at least " << required 
      << " (e.g. via ulimit -n " << required << ") or" 
      << " adjust the value of the startup option --servers.descriptors-minimum";
    if (_descriptorsMinimum == 0) {
      LOG_TOPIC("a33ba", WARN, arangodb::Logger::SYSCALL) << s.str();
    } else {
      LOG_TOPIC("8c771", FATAL, arangodb::Logger::SYSCALL) << s.str();
      FATAL_ERROR_EXIT_CODE(TRI_EXIT_RESOURCES_TOO_LOW);
    }
  }
}

void FileDescriptorsFeature::adjustFileDescriptors() {
  auto doAdjust = [](rlim_t recommended) {
    FileDescriptors current = FileDescriptors::load();

    LOG_TOPIC("6762c", DEBUG, arangodb::Logger::SYSCALL)
        << "file-descriptors (nofiles) hard limit is "
        << FileDescriptors::stringify(current.hard) 
        << ", soft limit is "
        << FileDescriptors::stringify(current.soft);

    if (recommended > 0) {
      if (current.hard < recommended) {
        LOG_TOPIC("0835c", DEBUG, arangodb::Logger::SYSCALL)
          << "hard limit " << current.hard << " is too small, trying to raise";

        FileDescriptors copy = current;
        copy.hard = recommended;
        if (copy.store() == 0) {
          // value adjusted successfully
          current.hard = recommended;
        }
      } else {
        TRI_ASSERT(current.hard >= recommended);
        recommended = current.hard;
      }
    
      if (current.soft < recommended) {
        LOG_TOPIC("2940e", DEBUG, arangodb::Logger::SYSCALL)
          << "soft limit " << current.soft << " is too small, trying to raise";

        FileDescriptors copy = current;
        copy.soft = recommended;
        if (copy.store() != 0) {
          LOG_TOPIC("ba733", WARN, arangodb::Logger::SYSCALL)
            << "cannot raise the file descriptors limit to " << recommended << ": "
            << strerror(errno);
        }
      }
    }
  };

  // first try to raise file descriptors to at least the recommended minimum value.
  // as the recommended minimum value is pretty low, there is a high chance that this
  // actually succeeds and does not violate any hard limits
  doAdjust(std::max<rlim_t>(
    static_cast<rlim_t>(_descriptorsMinimum),
    FileDescriptors::recommendedMinimum()
  ));

  // still, we are not satisfied and will now try to raise the file descriptors limit
  // even further. if that fails, then it is at least likely that the small raise in
  // step 1 has worked.
  doAdjust(65535);
}

}  // namespace arangodb

#endif
