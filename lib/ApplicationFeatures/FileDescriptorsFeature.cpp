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

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::options;

FileDescriptorsFeature::FileDescriptorsFeature(
    application_features::ApplicationServer* server)
    : ApplicationFeature(server, "FileDescriptors") {
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("Logger");
}

void FileDescriptorsFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::collectOptions";

#ifdef TRI_HAVE_GETRLIMIT
  options->addSection("server", "Server features");

  options->addOption("--server.descriptors-minimum",
                     "minimum number of file descriptors needed to start",
                     new UInt64Parameter(&_descriptorMinimum));
#endif
}

void FileDescriptorsFeature::prepare() {
  LOG_TOPIC(TRACE, Logger::STARTUP) << name() << "::prepare";

  adjustFileDescriptors();
}

void FileDescriptors::adjustFileDescriptors() {
#ifdef TRI_HAVE_GETRLIMIT

  if (0 < _descriptorMinimum) {
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

    if (rlim.rlim_max < _descriptorMinimum) {
      LOG(DEBUG) << "hard limit " << rlim.rlim_max
                 << " is too small, trying to raise";

      rlim.rlim_max = _descriptorMinimum;
      rlim.rlim_cur = _descriptorMinimum;

      res = setrlimit(RLIMIT_NOFILE, &rlim);

      if (res < 0) {
        LOG(FATAL) << "cannot raise the file descriptor limit to "
                   << _descriptorMinimum << ": " << strerror(errno);
        FATAL_ERROR_EXIT();
      }

      changed = true;
    } else if (rlim.rlim_cur < _descriptorMinimum) {
      LOG(DEBUG) << "soft limit " << rlim.rlim_cur
                 << " is too small, trying to raise";

      rlim.rlim_cur = _descriptorMinimum;

      res = setrlimit(RLIMIT_NOFILE, &rlim);

      if (res < 0) {
        LOG(FATAL) << "cannot raise the file descriptor limit to "
                   << _descriptorMinimum << ": " << strerror(errno);
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
                << ", soft limit is " << StringifyLimitValue(rlim.rlim_cur);
    }

    // the select backend has more restrictions
    if (_backend == 1) {
      if (FD_SETSIZE < _descriptorMinimum) {
        LOG(FATAL)
            << "i/o backend 'select' has been selected, which supports only "
            << FD_SETSIZE << " descriptors, but " << _descriptorMinimum
            << " are required";
        FATAL_ERROR_EXIT();
      }
    }
  }

#endif
}
