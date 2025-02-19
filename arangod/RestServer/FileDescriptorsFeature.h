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

#pragma once

#include "ApplicationFeatures/ApplicationFeature.h"
#include "Basics/operating-system.h"
#include "Metrics/Fwd.h"
#include "RestServer/arangod.h"

#include <chrono>
#include <mutex>

#ifdef TRI_HAVE_GETRLIMIT
namespace arangodb {

class FileDescriptorsFeature : public ArangodFeature {
 public:
  static constexpr std::string_view name() noexcept {
    return "FileDescriptors";
  }

  explicit FileDescriptorsFeature(Server& server);

  void collectOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void validateOptions(std::shared_ptr<options::ProgramOptions>) override final;
  void prepare() override final;

  uint64_t current() const noexcept;
  uint64_t limit() const noexcept;

  // count the number of open files, by scanning /proc/self/fd.
  // note: this can be expensive
  void countOpenFiles();

  // same as countOpenFiles(), but prevents multiple threads from counting
  // at the same time, and only recounts if at a last 30 seconds have
  // passed since the last counting
  void countOpenFilesIfNeeded();

 private:
  uint64_t _countDescriptorsInterval;

  metrics::Gauge<uint64_t>& _fileDescriptorsCurrent;
  metrics::Gauge<uint64_t>& _fileDescriptorsLimit;

  // mutex for counting open file in countOpenFilesIfNeeds.
  // this mutex prevents multiple callers from entering the function at
  // the same time, causing excessive IO for directory iteration.
  // additionally, it protects _lastCountStamp, so that only one thread
  // at a time wil do the counting and check/update _lastCountStamp.
  // this also prevents overly eager re-counting in case we have counted
  // only recented.
  std::mutex _lastCountMutex;
  std::chrono::steady_clock::time_point _lastCountStamp;
};

}  // namespace arangodb
#endif
