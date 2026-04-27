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

#include "FileDescriptorsFeature.h"

#include "RestServer/FileDescriptorsOptionsProvider.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "ApplicationFeatures/BumpFileDescriptorsFeature.h"
#include "ApplicationFeatures/GreetingsFeaturePhase.h"
#include "Basics/FileDescriptors.h"
#include "Basics/FileUtils.h"
#include "Logger/LogMacros.h"
#include "Logger/Logger.h"
#include "Logger/LoggerStream.h"
#include "Metrics/GaugeBuilder.h"
#include "Metrics/MetricsFeature.h"
#include "ProgramOptions/ProgramOptions.h"
#include "RestServer/EnvironmentFeature.h"

#ifdef TRI_HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <string>
#include <filesystem>

using namespace arangodb::application_features;
using namespace arangodb::basics;
using namespace arangodb::options;

#ifdef TRI_HAVE_GETRLIMIT
DECLARE_GAUGE(
    arangodb_file_descriptors_current, uint64_t,
    "Number of currently open file descriptors for the arangod process");
DECLARE_GAUGE(
    arangodb_file_descriptors_limit, uint64_t,
    "Limit for the number of open file descriptors for the arangod process");

namespace arangodb {

FileDescriptorsFeature::FileDescriptorsFeature(ApplicationServer& server,
                                               metrics::MetricsFeature& metrics)
    : ApplicationFeature{server, *this},
      _fileDescriptorsCurrent(metrics.add(arangodb_file_descriptors_current{})),
      _fileDescriptorsLimit(metrics.add(arangodb_file_descriptors_limit{})) {
  setOptional(false);
  startsAfter<BumpFileDescriptorsFeature>();
  startsAfter<GreetingsFeaturePhase>();
  startsAfter<EnvironmentFeature>();
}

void FileDescriptorsFeature::collectOptions(
    std::shared_ptr<ProgramOptions> options) {
  arangodb::file_descriptors::FileDescriptorsOptionsProvider provider;
  provider.declareOptions(options, _options);
}

void FileDescriptorsFeature::validateOptions(
    std::shared_ptr<ProgramOptions> options) {
  arangodb::file_descriptors::FileDescriptorsOptionsProvider provider;
  provider.validateOptions(options, _options);
}

void FileDescriptorsFeature::prepare() {
  FileDescriptors current;
  if (Result res = FileDescriptors::load(current); res.fail()) {
    THROW_ARANGO_EXCEPTION(res);
  }

  _fileDescriptorsLimit.store(current.soft, std::memory_order_relaxed);
}

uint64_t FileDescriptorsFeature::current() const noexcept {
  return _fileDescriptorsCurrent.load(std::memory_order_relaxed);
}
uint64_t FileDescriptorsFeature::limit() const noexcept {
  return _fileDescriptorsLimit.load(std::memory_order_relaxed);
}

void FileDescriptorsFeature::countOpenFiles() {
  try {
    std::filesystem::path fdPath{"/proc/self/fd"};
    size_t numFiles = 0;

    if (std::filesystem::exists(fdPath)) {
      for (auto const& entry : std::filesystem::directory_iterator(fdPath)) {
        // The intent is simply to increment numFiles for each entry in
        // fdPath, not to inspect the entries themselves. Hence Ignore
        // entry
        (void)entry;  // explicitly ignore
        ++numFiles;
      }
    }

    _fileDescriptorsCurrent.store(numFiles, std::memory_order_relaxed);
  } catch (std::exception const& ex) {
    LOG_TOPIC("bee41", DEBUG, Logger::SYSCALL)
        << "unable to count number of open files for arangod process: "
        << ex.what();
  } catch (...) {
    LOG_TOPIC("0a654", DEBUG, Logger::SYSCALL)
        << "unable to count number of open files for arangod process";
  }
}

void FileDescriptorsFeature::countOpenFilesIfNeeded() {
  if (_options.countDescriptorsInterval == 0) {
    return;
  }

  auto now = std::chrono::steady_clock::now();

  std::unique_lock guard{_lastCountMutex, std::try_to_lock};

  if (guard.owns_lock() &&
      (_lastCountStamp.time_since_epoch().count() == 0 ||
       now - _lastCountStamp >
           std::chrono::milliseconds(_options.countDescriptorsInterval))) {
    countOpenFiles();
    _lastCountStamp = now;
  }
}
}  // namespace arangodb

#endif
