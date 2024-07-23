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

#include "CpuUsageFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/debugging.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace arangodb {

struct CpuUsageFeature::SnapshotProvider {
  SnapshotProvider();
  ~SnapshotProvider();
  SnapshotProvider(SnapshotProvider const&) = delete;
  SnapshotProvider& operator=(SnapshotProvider const&) = delete;

  bool canTakeSnapshot() const noexcept { return _statFile != nullptr; }

  bool tryTakeSnapshot(CpuUsageSnapshot& result) noexcept;

 private:
  size_t readStatFile(char* buffer, size_t bufferSize) noexcept;

  /// @brief handle for /proc/stat
  FILE* _statFile;
};

CpuUsageFeature::SnapshotProvider::SnapshotProvider() : _statFile(nullptr) {
  // we are opening the /proc/stat file only once during the lifetime of the
  // process, in order to avoid frequent open/close calls
  _statFile = fopen("/proc/stat", "r");
}

CpuUsageFeature::SnapshotProvider::~SnapshotProvider() {
  if (_statFile != nullptr) {
    fclose(_statFile);
  }
}

bool CpuUsageFeature::SnapshotProvider::tryTakeSnapshot(
    CpuUsageSnapshot& result) noexcept {
  constexpr size_t bufferSize = 4096;

  // none of the following methods will throw an exception
  rewind(_statFile);
  fflush(_statFile);

  char buffer[bufferSize];
  buffer[0] = '\0';

  size_t nread = readStatFile(&buffer[0], bufferSize);
  // expect a minimum size
  if (nread < 32 || memcmp(&buffer[0], "cpu ", 4) != 0) {
    // invalid data read.
    return false;
  }

  // 4 bytes because we skip the initial "cpu " intro
  result = CpuUsageSnapshot::fromString(&buffer[4], bufferSize - 4);
  return true;
}

size_t CpuUsageFeature::SnapshotProvider::readStatFile(
    char* buffer, size_t bufferSize) noexcept {
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

CpuUsageFeature::CpuUsageFeature(Server& server)
    : ArangodFeature{server, *this},
      _snapshotProvider(),
      _updateInProgress(false) {
  setOptional(true);

  startsAfter<application_features::GreetingsFeaturePhase>();
}

void CpuUsageFeature::prepare() {
  _snapshotProvider = std::make_unique<SnapshotProvider>();

  if (!_snapshotProvider->canTakeSnapshot()) {
    // we will not be able to provide any stats, so let's disable ourselves
    disable();
  }
}

CpuUsageFeature::~CpuUsageFeature() = default;

CpuUsageSnapshot CpuUsageFeature::snapshot() {
  CpuUsageSnapshot lastSnapshot, lastDelta;

  if (!isEnabled()) {
    return lastDelta;
  }

  // whether or not a concurrent thread is currently updating our
  // snapshot. if this is the case, we will simply return the old
  // snapshot
  bool updateInProgress;
  {
    // read last snapshot under the mutex
    std::lock_guard guard{_snapshotMutex};
    lastSnapshot = _snapshot;
    lastDelta = _snapshotDelta;
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
    return lastDelta;
  }

  CpuUsageSnapshot next;
  auto success = _snapshotProvider->tryTakeSnapshot(next);
  {
    // snapshot must be updated and returned under mutex
    std::lock_guard guard{_snapshotMutex};
    if (success) {
      // if we failed to obtain new snapshot, we simply return whatever we had
      // before
      _snapshot = next;
      if (lastSnapshot.valid()) {
        next.subtract(lastSnapshot);
      }
      _snapshotDelta = next;
    }
    _updateInProgress = false;
    return _snapshotDelta;
  }
}

}  // namespace arangodb
