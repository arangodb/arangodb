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

#include "OpenFilesTracker.h"
#include "Logger/Logger.h"

using namespace arangodb;

namespace {
// single global instance which keeps track of all opened/closed files
OpenFilesTracker Instance;
}  // namespace

OpenFilesTracker::OpenFilesTracker() : _warnThreshold(0), _lastWarning(0.0) {}

OpenFilesTracker::~OpenFilesTracker() {}

int OpenFilesTracker::create(char const* path, int oflag, int mode) {
  // call underlying open() function from OS
  int res = TRI_CREATE(path, oflag, mode);

  if (res >= 0) {
    LOG_TOPIC(TRACE, Logger::SYSCALL)
        << "created file '" << path << "' was assigned file descriptor " << res;

    increase();
  }
  return res;
}

int OpenFilesTracker::open(char const* path, int oflag) {
  // call underlying open() function from OS
  int res = TRI_OPEN(path, oflag);

  if (res >= 0) {
    LOG_TOPIC(TRACE, Logger::SYSCALL)
        << "opened file '" << path << "' was assigned file descriptor " << res;

    increase();
  }
  return res;
}

int OpenFilesTracker::close(int fd) noexcept {
  // call underlying close() function from OS
  int res = TRI_CLOSE(fd);

  if (res == TRI_ERROR_NO_ERROR) {
    LOG_TOPIC(TRACE, Logger::SYSCALL) << "closed file with file descriptor " << fd;

    decrease();
  }

  return res;
}

void OpenFilesTracker::increase() {
  uint64_t nowOpen = ++_numOpenFiles;

  if (nowOpen > _warnThreshold && _warnThreshold > 0) {
    double now = TRI_microtime();

    if (_lastWarning <= 0.0 || now - _lastWarning >= 30.0) {
      // warn only every x seconds at most
      LOG_TOPIC(WARN, Logger::SYSCALL)
          << "number of currently open files is now " << nowOpen
          << " and exceeds the warning threshold value " << _warnThreshold;
      _lastWarning = now;
    }
  }
}

void OpenFilesTracker::decrease() noexcept { --_numOpenFiles; }

OpenFilesTracker* OpenFilesTracker::instance() { return &Instance; }
