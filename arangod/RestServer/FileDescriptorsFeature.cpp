////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#include "Logger/Logger.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "Scheduler/SchedulerFeature.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

FileDescriptorsFeature::FileDescriptorsFeature(
    application_features::ApplicationServer* server)
  : ApplicationFeature(server, "FileDescriptors"),
    _descriptorsMinimum(1024) {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

void FileDescriptorsFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
#ifdef TRI_HAVE_GETRLIMIT
  options->addSection("server", "Server features");

  options->addOption("--server.descriptors-minimum",
                     "minimum number of file descriptors needed to start",
                     new UInt64Parameter(&_descriptorsMinimum));
#endif
}

void FileDescriptorsFeature::prepare() {
  adjustFileDescriptors();
}

#ifdef TRI_HAVE_GETRLIMIT
template <typename T>
static std::string StringifyLimitValue(T value) {
  auto max = std::numeric_limits<decltype(value)>::max();
  if (value == max || value == max / 2) {
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
    LOG(INFO) << "file-descriptors (nofiles) hard limit is "
              << StringifyLimitValue(rlim.rlim_max) << ", soft limit is "
              << StringifyLimitValue(rlim.rlim_cur);
  }
#endif
}

void FileDescriptorsFeature::adjustFileDescriptors() {
#ifdef TRI_HAVE_GETRLIMIT
  if (0 < _descriptorsMinimum) {
    struct rlimit rlim;
    int res = getrlimit(RLIMIT_NOFILE, &rlim);

    if (res != 0) {
      LOG(FATAL) << "cannot get the file descriptor limit: " << strerror(errno);
      FATAL_ERROR_EXIT();
    }

    LOG(DEBUG) << "file-descriptors (nofiles) hard limit is "
               << StringifyLimitValue(rlim.rlim_max) << ", soft limit is "
               << StringifyLimitValue(rlim.rlim_cur);

    bool changed = false;

    if (rlim.rlim_max < _descriptorsMinimum) {
      LOG(DEBUG) << "hard limit " << rlim.rlim_max
                 << " is too small, trying to raise";

      rlim.rlim_max = _descriptorsMinimum;
      rlim.rlim_cur = _descriptorsMinimum;

      res = setrlimit(RLIMIT_NOFILE, &rlim);

      if (res < 0) {
        LOG(FATAL) << "cannot raise the file descriptor limit to "
                   << _descriptorsMinimum << ": " << strerror(errno);
        FATAL_ERROR_EXIT();
      }

      changed = true;
    } else if (rlim.rlim_cur < _descriptorsMinimum) {
      LOG(DEBUG) << "soft limit " << rlim.rlim_cur
                 << " is too small, trying to raise";

      rlim.rlim_cur = _descriptorsMinimum;

      res = setrlimit(RLIMIT_NOFILE, &rlim);

      if (res < 0) {
        LOG(FATAL) << "cannot raise the file descriptor limit to "
                   << _descriptorsMinimum << ": " << strerror(errno);
        FATAL_ERROR_EXIT();
      }

      changed = true;
    }

    if (changed) {
      res = getrlimit(RLIMIT_NOFILE, &rlim);

      if (res != 0) {
        LOG(FATAL) << "cannot get the file descriptor limit: "
                   << strerror(errno);
        FATAL_ERROR_EXIT();
      }

      LOG(INFO) << "file-descriptors (nofiles) new hard limit is "
                << StringifyLimitValue(rlim.rlim_max) << ", new soft limit is "
                << StringifyLimitValue(rlim.rlim_cur);
    }

    // the select backend has more restrictions
    try {
      SchedulerFeature* scheduler = 
          ApplicationServer::getFeature<SchedulerFeature>("Scheduler");

      if (scheduler->backend() == 1) {
        if (FD_SETSIZE < _descriptorsMinimum) {
          LOG(FATAL)
              << "i/o backend 'select' has been selected, which supports only "
              << FD_SETSIZE << " descriptors, but " << _descriptorsMinimum
              << " are required";
          FATAL_ERROR_EXIT();
        }
      }
    } catch (...) {
      // Scheduler feature not present... simply ignore this
    }
  }
#endif
}
