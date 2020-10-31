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

#include "CpuUsageFeature.h"

#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/MutexLocker.h"
#include "Basics/NumberUtils.h"
#include "Basics/debugging.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {
constexpr size_t bufferSize = 4096;
}

namespace arangodb {

CpuUsageFeature::CpuUsageFeature(application_features::ApplicationServer& server)
    : ApplicationFeature(server, "CpuUsage"),
      _statFile(nullptr),
      _updateInProgress(false) {
  setOptional(true);
  startsAfter<application_features::GreetingsFeaturePhase>();
}

CpuUsageFeature::~CpuUsageFeature() {
  if (_statFile != nullptr) {
    fclose(_statFile);
  }
}

void CpuUsageFeature::prepare() {
  TRI_ASSERT(_statFile == nullptr);
#ifdef __linux__
  // we are opening the /proc/stat file only once during the lifetime of the process,
  // in order to avoid frequent open/close calls
  _statFile = fopen("/proc/stat", "r");
#endif

  if (_statFile == nullptr) {
    // we will not be able to provide any stats, so let's disable ourselves
    disable();
  }
}

CpuUsageSnapshot CpuUsageFeature::snapshot() {
  CpuUsageSnapshot last;
  
  if (_statFile == nullptr) {
    // /proc/stat not there. will happen on non-Linux platforms
    return last;
  }
  
  // whether or not a concurrent thread is currently updating our
  // snapshot. if this is the case, we will simply return the old
  // snapshot
  bool updateInProgress;
  {
    // read last snapshot under the mutex
    MUTEX_LOCKER(guard, _snapshotMutex);
    last = _snapshot;
    updateInProgress = _updateInProgress;

    if (!updateInProgress) {
      // its our turn!
      _updateInProgress = true;
    }
  }

  // now we can go on with a copy of the last snapshot without
  // holding the mutex
  if (updateInProgress) {
    // in a multi-threaded environment, we need to serialize our access
    // to /proc/stat by multiple concurrent threads. this also helps
    // reducing the load if multiple threads concurrently request the
    // CPU statistics
    return last;
  }

  // none of the following methods will throw an exception
  rewind(_statFile);    
  fflush(_statFile); 

  char buffer[::bufferSize];
  buffer[0] = '\0';
 
  size_t nread = readStatFile(&buffer[0], ::bufferSize);
  // expect a minimum size
  if (nread < 32 || memcmp(&buffer[0], "cpu ", 4) != 0) {
    // invalid data read. return whatever we had before
    return last;
  }

  // 4 bytes because we skip the initial "cpu " intro
  CpuUsageSnapshot next = CpuUsageSnapshot::fromString(&buffer[4], bufferSize - 4);
 
  // snapshot must be updated and returned under mutex
  MUTEX_LOCKER(guard, _snapshotMutex);
  _snapshot = next;
  _updateInProgress = false;

  if (last.valid()) {
    next.subtract(last);
  }

  return next;
}

size_t CpuUsageFeature::readStatFile(char* buffer, size_t bufferSize) noexcept {
  size_t offset = 0;
  size_t remain = bufferSize - 1;
  while (remain > 0) {
    TRI_ASSERT(offset < bufferSize);

    size_t nread = fread(buffer + offset, 1, remain, _statFile);
    if (nread == 0) {
      break;
    }
    remain -= nread;
    offset += nread;
  }
  TRI_ASSERT(offset < bufferSize);
  buffer[offset] = '\0';
  return offset;
}

}
