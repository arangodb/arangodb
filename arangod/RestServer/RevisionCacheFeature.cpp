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

#include "RevisionCacheFeature.h"

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/process-utils.h"
#include "ProgramOptions/ProgramOptions.h"
#include "ProgramOptions/Section.h"
#include "VocBase/RevisionCacheChunkAllocator.h"

using namespace arangodb;
using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

// smaller default cache value for ARM-based platforms
#ifdef __arm__
static constexpr uint64_t DefaultTargetSize = 64 * 1024 * 1024;
#else
static constexpr uint64_t DefaultTargetSize = 512 * 1024 * 1024;
#endif

RevisionCacheChunkAllocator* RevisionCacheFeature::ALLOCATOR = nullptr;

RevisionCacheFeature::RevisionCacheFeature(ApplicationServer* server)
    : ApplicationFeature(server, "RevisionCache"),
      _chunkSize(4 * 1024 * 1024),
      _targetSize(DefaultTargetSize) { 
  setOptional(false);
  requiresElevatedPrivileges(false);
  startsAfter("WorkMonitor");

  if (TRI_PhysicalMemory >= 2147483648ULL) { // 2 GB
    // reset target size to a fraction of the available memory
    _targetSize = TRI_PhysicalMemory - 1073741824ULL; // 1 GB
    _targetSize *= 0.4; // 40 % or RAM
  }
}

void RevisionCacheFeature::collectOptions(std::shared_ptr<ProgramOptions> options) {
  options->addSection("database", "Configure the database");
  
  options->addOption("--database.revision-cache-chunk-size", "chunk size (in bytes) for the document revisions cache",
                     new UInt32Parameter(&_chunkSize));
  options->addOption("--database.revision-cache-target-size", "total target size (in bytes) for the document revisions cache",
                     new UInt64Parameter(&_targetSize));
}

void RevisionCacheFeature::validateOptions(std::shared_ptr<ProgramOptions> options) {
  // note: all the following are arbitrary limits
  if (_chunkSize < 8 * 1024) {
    LOG(FATAL) << "value for '--database.revision-cache-chunk-size' is too low";
    FATAL_ERROR_EXIT();
  }
  if (_chunkSize > 256 * 1024 * 1024) {
    LOG(FATAL) << "value for '--database.revision-cache-chunk-size' is too high";
    FATAL_ERROR_EXIT();
  }

  if (_targetSize < 512 * 1024) {
    LOG(FATAL) << "value for '--database.revision-cache-target-size' is too low";
    FATAL_ERROR_EXIT();
  } else if (_targetSize < 32 * 1024 * 1024 || _targetSize / _chunkSize < 16) {
    LOG(WARN) << "value for '--database.revision-cache-target-size' is very low";
  }
  
  if (_chunkSize >= _targetSize) {
    LOG(WARN) << "value for '--database.revision-cache-target-size' should be higher than value of '--database.revision-cache-chunk-size'";
    _chunkSize = static_cast<uint32_t>(_targetSize);
  }
}

void RevisionCacheFeature::prepare() {
  _allocator.reset(new RevisionCacheChunkAllocator(_chunkSize, _targetSize));
  ALLOCATOR = _allocator.get();
}

void RevisionCacheFeature::start() {
  _allocator->startGcThread();
}

void RevisionCacheFeature::beginShutdown() {
  _allocator->beginShutdown();
}

void RevisionCacheFeature::stop() {
  _allocator->stopGcThread();
}

void RevisionCacheFeature::unprepare() {
  ALLOCATOR = nullptr;
  _allocator.reset();
}
